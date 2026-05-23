// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/work_budget.hpp"

#include <cstdint>

namespace aster {

struct FrameControlInput {
  double target_frame_seconds = 1.0 / 60.0;
  double frame_seconds = 0.0;
  double update_seconds = 0.0;
  double render_seconds = 0.0;
  double hud_seconds = 0.0;
  double swap_seconds = 0.0;
  double perception_seconds = 0.0;
  double physics_seconds = 0.0;
  double streaming_seconds = 0.0;
  double frame_seconds_p95 = 0.0;
  double frame_seconds_p99 = 0.0;
  double frame_jitter_seconds = 0.0;
  std::uint32_t streaming_backlog_items = 0u;
  std::uint32_t perceptual_backlog_items = 0u;
  std::uint32_t active_dynamic_bodies = 0u;
  std::uint32_t active_contacts = 0u;
  std::uint32_t contact_islands = 0u;
  std::uint32_t warm_started_contacts = 0u;
  std::uint32_t mesh_triangle_candidates = 0u;
  std::uint32_t active_lights = 0u;
  std::uint32_t visible_objects = 0u;
  double player_speed = 0.0;
  double cave_pressure = 0.0;
  double region_pressure = 0.0;
  double visibility_pressure = 0.0;
  double light_pressure = 0.0;
};

struct FrameControlOutput {
  FrameWorkBudget streaming_budget{};
  FrameWorkBudget perceptual_budget{};
  std::uint32_t physics_max_substeps = 1u;
  std::uint32_t physics_solver_iterations = 6u;
  std::uint32_t perceptual_proof_interval_frames = 1u;
  std::uint32_t lighting_update_interval_frames = 1u;
  std::uint32_t visibility_hint_budget = 0u;
  std::uint32_t active_light_budget = 0u;
  std::uint32_t mesh_triangle_candidate_budget = 0u;
  float semantic_lod_bias = 0.0f;
  double pressure = 0.0;
  double optional_work_seconds = 0.0;
  double physics_budget_seconds = 0.0;
  double render_budget_seconds = 0.0;
  std::uint32_t degraded = 0u;
  std::uint32_t quality_tier = 0u;
};

class FrameControlPolicy {
public:
  FrameControlPolicy();

  void reset();
  [[nodiscard]] FrameControlOutput evaluate(const FrameControlInput &input);

private:
  FrameBudgetController streaming_controller_;
  FrameBudgetController perceptual_controller_;
};

[[nodiscard]] FrameControlOutput evaluateFrameControl(const FrameControlInput &input);

} // namespace aster
