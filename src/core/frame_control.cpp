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
  const double frame_pressure = std::max(0.0, measured / target - 1.0);
  const double update_pressure = std::max(0.0, input.update_seconds / (target * 0.58) - 1.0);
  const double render_pressure = std::max(0.0, input.render_seconds / (target * 0.42) - 1.0);
  const double cave_pressure = std::clamp(input.cave_pressure, 0.0, 2.0);
  const double backlog_pressure =
      std::clamp((input.streaming_backlog_items + input.perceptual_backlog_items) / 96.0, 0.0, 1.0);
  return std::clamp(frame_pressure * 0.45 + update_pressure * 0.25 + render_pressure * 0.12 +
                        cave_pressure * 0.10 + backlog_pressure * 0.08,
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
  output.physics_solver_iterations =
      pressure > 0.90 ? 4u : (pressure > 0.45 ? 6u : 8u + (input.active_contacts > 16u ? 1u : 0u));
  output.perceptual_proof_interval_frames = pressure > 0.90 ? 4u : (pressure > 0.45 ? 2u : 1u);
  output.lighting_update_interval_frames = pressure > 0.90 ? 5u : (pressure > 0.45 ? 3u : 1u);
  output.visibility_hint_budget =
      pressure > 0.90 ? 64u : (pressure > 0.45 ? 160u : 320u);
  output.semantic_lod_bias = static_cast<float>(std::clamp(pressure * 0.65, 0.0, 1.0));
  return output;
}

FrameControlOutput evaluateFrameControl(const FrameControlInput &input) {
  FrameControlPolicy policy;
  return policy.evaluate(input);
}

} // namespace aster
