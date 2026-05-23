// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/frame_control.hpp"

#include <algorithm>
#include <cmath>

namespace aster {
namespace {

double stableTargetSeconds(const double value) {
  return std::clamp(value, 1.0 / 240.0, 1.0 / 20.0);
}

double normalizedPressure(const FrameControlInput &input) {
  const double target = stableTargetSeconds(input.target_frame_seconds);
  const double measured = input.frame_seconds > 0.0
                              ? input.frame_seconds
                              : input.update_seconds + input.render_seconds + input.hud_seconds +
                                    input.swap_seconds;
  const double measured_tail =
      std::max({measured, input.frame_seconds_p95, input.frame_seconds_p99 * 0.88});
  const double frame_pressure = std::max(0.0, measured / target - 1.0);
  const double tail_pressure = std::max(0.0, measured_tail / target - 1.0);
  const double jitter_pressure =
      std::max(0.0, input.frame_jitter_seconds / (target * 0.18) - 1.0);
  const double update_pressure = std::max(0.0, input.update_seconds / (target * 0.58) - 1.0);
  const double render_pressure = std::max(0.0, input.render_seconds / (target * 0.42) - 1.0);
  const double region_pressure =
      std::clamp(std::max(input.cave_pressure, input.region_pressure), 0.0, 2.0);
  const double visibility_pressure = std::clamp(input.visibility_pressure, 0.0, 2.0);
  const double light_pressure = std::clamp(input.light_pressure, 0.0, 2.0);
  const double physics_pressure = std::clamp(
      input.physics_seconds / std::max(target * 0.24, 0.0001) +
          static_cast<double>(input.mesh_triangle_candidates) / 4096.0 +
          static_cast<double>(input.active_contacts + input.contact_islands * 4u) / 128.0,
      0.0, 2.0);
  const double backlog_pressure =
      std::clamp((input.streaming_backlog_items + input.perceptual_backlog_items) / 96.0, 0.0, 1.0);
  return std::clamp(frame_pressure * 0.25 + tail_pressure * 0.20 +
                        update_pressure * 0.18 + render_pressure * 0.10 +
                        physics_pressure * 0.12 + region_pressure * 0.06 +
                        visibility_pressure * 0.03 + light_pressure * 0.03 +
                        jitter_pressure * 0.02 + backlog_pressure * 0.01,
                    0.0, 2.0);
}

FrameBudgetControllerInput controllerInput(const FrameControlInput &input,
                                           const std::uint32_t backlog) {
  return {.frame_seconds = input.frame_seconds,
          .update_seconds = input.update_seconds,
          .render_seconds = input.render_seconds,
          .upload_seconds = input.streaming_seconds,
          .backlog_items = backlog,
          .viewer_speed = input.player_speed};
}

} // namespace

FrameControlPolicy::FrameControlPolicy()
    : streaming_controller_({.target_frame_seconds = 1.0 / 60.0,
                             .min_work_seconds = 0.00020,
                             .max_work_seconds = 0.0035,
                             .min_items = 1u,
                             .max_items = 5u,
                             .backlog_full_scale = 64.0,
                             .recovery_rate = 0.18,
                             .pressure_rate = 0.40,
                             .starvation_priority_per_frame = 0.016,
                             .starvation_frame_limit = 16u}),
      perceptual_controller_({.target_frame_seconds = 1.0 / 60.0,
                              .min_work_seconds = 0.00010,
                              .max_work_seconds = 0.0022,
                              .min_items = 1u,
                              .max_items = 3u,
                              .backlog_full_scale = 48.0,
                              .recovery_rate = 0.16,
                              .pressure_rate = 0.44,
                              .starvation_priority_per_frame = 0.018,
                              .starvation_frame_limit = 20u}) {}

void FrameControlPolicy::reset() {
  streaming_controller_.reset();
  perceptual_controller_.reset();
}

FrameControlOutput FrameControlPolicy::evaluate(const FrameControlInput &input) {
  const double pressure = normalizedPressure(input);
  const double target = stableTargetSeconds(input.target_frame_seconds);
  const double optional_seconds =
      std::clamp(target - input.update_seconds - input.render_seconds - input.hud_seconds -
                     input.swap_seconds,
                 0.00005, 0.0060);

  FrameWorkBudget base_streaming;
  base_streaming.max_items = pressure > 0.85 ? 1u : (pressure > 0.35 ? 2u : 4u);
  base_streaming.max_seconds = optional_seconds * (pressure > 0.85 ? 0.25 : 0.45);
  base_streaming.starvation_frame_limit = 16u;
  base_streaming.starvation_priority_per_frame = 0.016;

  FrameWorkBudget base_perceptual;
  base_perceptual.max_items = pressure > 0.85 ? 1u : (pressure > 0.35 ? 2u : 3u);
  base_perceptual.max_seconds = optional_seconds * (pressure > 0.85 ? 0.16 : 0.30);
  base_perceptual.starvation_frame_limit = 20u;
  base_perceptual.starvation_priority_per_frame = 0.018;

  FrameControlOutput output;
  output.streaming_budget =
      streaming_controller_.nextBudget(controllerInput(input, input.streaming_backlog_items),
                                       base_streaming);
  output.perceptual_budget =
      perceptual_controller_.nextBudget(controllerInput(input, input.perceptual_backlog_items),
                                        base_perceptual);
  output.pressure = pressure;
  output.optional_work_seconds = optional_seconds;
  output.degraded = pressure > 0.35 ? 1u : 0u;
  output.physics_max_substeps = pressure > 0.90 ? 1u : (pressure > 0.45 ? 2u : 4u);
  const bool contact_graph_busy =
      input.contact_islands > 3u || input.mesh_triangle_candidates > 2048u ||
      input.active_contacts > 32u;
  output.physics_solver_iterations =
      pressure > 0.90 ? 4u
                      : (pressure > 0.45 ? 6u : 8u + (contact_graph_busy ? 1u : 0u));
  output.perceptual_proof_interval_frames = pressure > 0.90 ? 4u : (pressure > 0.45 ? 2u : 1u);
  output.lighting_update_interval_frames =
      pressure > 0.90 || input.active_lights > 48u ? 5u
                                                   : (pressure > 0.45 ? 3u : 1u);
  output.visibility_hint_budget =
      pressure > 0.90 ? 64u : (pressure > 0.45 ? 160u : 320u);
  output.active_light_budget = pressure > 0.90 ? 12u : (pressure > 0.45 ? 24u : 48u);
  output.mesh_triangle_candidate_budget =
      pressure > 0.90 ? 512u : (pressure > 0.45 ? 1536u : 4096u);
  output.semantic_lod_bias = static_cast<float>(std::clamp(pressure * 0.65, 0.0, 1.0));
  output.physics_budget_seconds = target * (pressure > 0.90 ? 0.18 : 0.24);
  output.render_budget_seconds = target * (pressure > 0.90 ? 0.36 : 0.44);
  output.quality_tier = pressure > 0.90 ? 2u : (pressure > 0.45 ? 1u : 0u);
  return output;
}

FrameControlOutput evaluateFrameControl(const FrameControlInput &input) {
  FrameControlPolicy policy;
  return policy.evaluate(input);
}

} // namespace aster
