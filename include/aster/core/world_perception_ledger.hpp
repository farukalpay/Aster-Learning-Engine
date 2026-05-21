// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class WorldPerceptionLedgerChannel : std::uint32_t {
  MaterialMemory = 1u << 0u,
  ContactHistory = 1u << 1u,
  LightingExposure = 1u << 2u,
  AtmosphereCell = 1u << 3u,
  OcclusionRole = 1u << 4u,
  GameplayAffordance = 1u << 5u,
  WearContinuity = 1u << 6u,
  StreamingSemanticLod = 1u << 7u,
  AudioVisualCueBudget = 1u << 8u,
};

[[nodiscard]] std::uint32_t worldPerceptionLedgerChannelBit(std::string_view channel);
[[nodiscard]] std::string_view worldPerceptionLedgerChannelName(
    WorldPerceptionLedgerChannel channel);
[[nodiscard]] float worldPerceptionLedgerScore(std::uint32_t required_channel_mask,
                                               std::uint32_t observed_channel_mask);

struct WorldPerceptionChannelEvidence {
  WorldPerceptionLedgerChannel channel = WorldPerceptionLedgerChannel::MaterialMemory;
  std::uint64_t hash = 0u;
  float strength = 1.0f;
};

struct WorldPerceptionLedgerCellDesc {
  std::uint64_t region_id = 0u;
  std::string cell_id;
  std::uint32_t required_channel_mask = 0u;
  float minimum_score = 0.0f;
  std::vector<WorldPerceptionChannelEvidence> evidence;
  std::string diagnostic;
};

struct WorldPerceptionLedgerCellReport {
  std::uint64_t region_id = 0u;
  std::string cell_id;
  std::uint32_t required_channel_mask = 0u;
  std::uint32_t observed_channel_mask = 0u;
  std::uint32_t missing_channel_mask = 0u;
  float score = 0.0f;
  float minimum_score = 0.0f;
  bool accepted = false;
  std::uint64_t ledger_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t contact_history_hash = 0u;
  std::uint64_t lighting_exposure_hash = 0u;
  std::uint64_t atmosphere_cell_hash = 0u;
  std::uint64_t occlusion_role_hash = 0u;
  std::uint64_t gameplay_affordance_hash = 0u;
  std::uint64_t wear_continuity_hash = 0u;
  std::uint64_t streaming_semantic_lod_hash = 0u;
  std::uint64_t audio_visual_cue_budget_hash = 0u;
  std::string diagnostic;
};

struct WorldPerceptionLedgerReport {
  std::uint64_t region_id = 0u;
  std::uint32_t required_channel_mask = 0u;
  std::uint32_t observed_channel_mask = 0u;
  std::uint32_t missing_channel_mask = 0u;
  float score = 0.0f;
  float minimum_score = 0.0f;
  bool accepted = false;
  std::uint64_t ledger_hash = 0u;
  std::size_t cell_count = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t contact_history_hash = 0u;
  std::uint64_t lighting_exposure_hash = 0u;
  std::uint64_t atmosphere_cell_hash = 0u;
  std::uint64_t occlusion_role_hash = 0u;
  std::uint64_t gameplay_affordance_hash = 0u;
  std::uint64_t wear_continuity_hash = 0u;
  std::uint64_t streaming_semantic_lod_hash = 0u;
  std::uint64_t audio_visual_cue_budget_hash = 0u;
  std::string diagnostic;
  std::vector<WorldPerceptionLedgerCellReport> cells;
};

struct WorldPerceptionObjectTrace {
  std::string object_name;
  std::string cell_id;
  std::uint32_t observed_channel_mask = 0u;
  std::uint64_t object_hash = 0u;
  std::uint64_t ledger_hash = 0u;
};

[[nodiscard]] WorldPerceptionLedgerCellReport
evaluateWorldPerceptionLedgerCell(const WorldPerceptionLedgerCellDesc &desc);

[[nodiscard]] WorldPerceptionLedgerReport summarizeWorldPerceptionLedger(
    std::uint64_t region_id, std::uint32_t required_channel_mask, float minimum_score,
    std::vector<WorldPerceptionLedgerCellReport> cells);

} // namespace aster
