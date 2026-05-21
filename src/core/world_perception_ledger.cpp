// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/world_perception_ledger.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <utility>

namespace aster {
namespace {

constexpr std::uint64_t kLedgerSeed = 0xA57E9E9CE91ED9E5ull;

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kLedgerSeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint32_t value) {
  return mix(hash, static_cast<std::uint64_t>(value));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::uint64_t mixString(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return hash;
}

void mergeChannelHash(std::uint64_t &target, const std::uint64_t value) {
  if (value == 0u) {
    return;
  }
  target = mix(target == 0u ? kLedgerSeed : target, value);
}

void storeChannelHash(WorldPerceptionLedgerCellReport &report,
                      const WorldPerceptionLedgerChannel channel, const std::uint64_t hash) {
  switch (channel) {
  case WorldPerceptionLedgerChannel::MaterialMemory:
    mergeChannelHash(report.material_memory_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::ContactHistory:
    mergeChannelHash(report.contact_history_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::LightingExposure:
    mergeChannelHash(report.lighting_exposure_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::AtmosphereCell:
    mergeChannelHash(report.atmosphere_cell_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::OcclusionRole:
    mergeChannelHash(report.occlusion_role_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::GameplayAffordance:
    mergeChannelHash(report.gameplay_affordance_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::WearContinuity:
    mergeChannelHash(report.wear_continuity_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::StreamingSemanticLod:
    mergeChannelHash(report.streaming_semantic_lod_hash, hash);
    break;
  case WorldPerceptionLedgerChannel::AudioVisualCueBudget:
    mergeChannelHash(report.audio_visual_cue_budget_hash, hash);
    break;
  }
}

void mergeCellChannels(WorldPerceptionLedgerReport &report,
                       const WorldPerceptionLedgerCellReport &cell) {
  mergeChannelHash(report.material_memory_hash, cell.material_memory_hash);
  mergeChannelHash(report.contact_history_hash, cell.contact_history_hash);
  mergeChannelHash(report.lighting_exposure_hash, cell.lighting_exposure_hash);
  mergeChannelHash(report.atmosphere_cell_hash, cell.atmosphere_cell_hash);
  mergeChannelHash(report.occlusion_role_hash, cell.occlusion_role_hash);
  mergeChannelHash(report.gameplay_affordance_hash, cell.gameplay_affordance_hash);
  mergeChannelHash(report.wear_continuity_hash, cell.wear_continuity_hash);
  mergeChannelHash(report.streaming_semantic_lod_hash, cell.streaming_semantic_lod_hash);
  mergeChannelHash(report.audio_visual_cue_budget_hash, cell.audio_visual_cue_budget_hash);
}

} // namespace

std::uint32_t worldPerceptionLedgerChannelBit(const std::string_view channel) {
  if (channel == "material_memory") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::MaterialMemory);
  }
  if (channel == "contact_history") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::ContactHistory);
  }
  if (channel == "lighting_exposure") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::LightingExposure);
  }
  if (channel == "atmosphere_cell") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::AtmosphereCell);
  }
  if (channel == "occlusion_role") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::OcclusionRole);
  }
  if (channel == "gameplay_affordance") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::GameplayAffordance);
  }
  if (channel == "wear_continuity") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::WearContinuity);
  }
  if (channel == "streaming_semantic_lod") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::StreamingSemanticLod);
  }
  if (channel == "audio_visual_cue_budget") {
    return static_cast<std::uint32_t>(WorldPerceptionLedgerChannel::AudioVisualCueBudget);
  }
  return 0u;
}

std::string_view worldPerceptionLedgerChannelName(const WorldPerceptionLedgerChannel channel) {
  switch (channel) {
  case WorldPerceptionLedgerChannel::MaterialMemory:
    return "material_memory";
  case WorldPerceptionLedgerChannel::ContactHistory:
    return "contact_history";
  case WorldPerceptionLedgerChannel::LightingExposure:
    return "lighting_exposure";
  case WorldPerceptionLedgerChannel::AtmosphereCell:
    return "atmosphere_cell";
  case WorldPerceptionLedgerChannel::OcclusionRole:
    return "occlusion_role";
  case WorldPerceptionLedgerChannel::GameplayAffordance:
    return "gameplay_affordance";
  case WorldPerceptionLedgerChannel::WearContinuity:
    return "wear_continuity";
  case WorldPerceptionLedgerChannel::StreamingSemanticLod:
    return "streaming_semantic_lod";
  case WorldPerceptionLedgerChannel::AudioVisualCueBudget:
    return "audio_visual_cue_budget";
  }
  return "unknown";
}

float worldPerceptionLedgerScore(const std::uint32_t required_channel_mask,
                                 const std::uint32_t observed_channel_mask) {
  if (required_channel_mask == 0u) {
    return 1.0f;
  }
  const std::uint32_t covered = required_channel_mask & observed_channel_mask;
  return static_cast<float>(std::popcount(covered)) /
         static_cast<float>(std::popcount(required_channel_mask));
}

WorldPerceptionLedgerCellReport
evaluateWorldPerceptionLedgerCell(const WorldPerceptionLedgerCellDesc &desc) {
  WorldPerceptionLedgerCellReport report;
  report.region_id = desc.region_id;
  report.cell_id = desc.cell_id;
  report.required_channel_mask = desc.required_channel_mask;
  report.minimum_score = std::clamp(desc.minimum_score, 0.0f, 1.0f);
  report.diagnostic = desc.diagnostic;

  std::uint64_t hash = mixString(kLedgerSeed, "aster.world-perception.cell.v1");
  hash = mix(hash, report.region_id);
  hash = mixString(hash, report.cell_id);
  hash = mix(hash, report.required_channel_mask);
  hash = mix(hash, report.minimum_score);
  for (const WorldPerceptionChannelEvidence &evidence : desc.evidence) {
    if (evidence.hash == 0u || evidence.strength <= 0.0f) {
      continue;
    }
    const std::uint32_t bit = static_cast<std::uint32_t>(evidence.channel);
    report.observed_channel_mask |= bit;
    storeChannelHash(report, evidence.channel, evidence.hash);
    hash = mix(hash, bit);
    hash = mix(hash, evidence.hash);
    hash = mix(hash, std::clamp(evidence.strength, 0.0f, 1.0f));
  }

  report.missing_channel_mask = report.required_channel_mask & ~report.observed_channel_mask;
  report.score =
      worldPerceptionLedgerScore(report.required_channel_mask, report.observed_channel_mask);
  report.accepted = report.missing_channel_mask == 0u &&
                    report.score + 0.0001f >= report.minimum_score;
  hash = mix(hash, report.observed_channel_mask);
  hash = mix(hash, report.missing_channel_mask);
  hash = mix(hash, report.score);
  hash = mix(hash, static_cast<std::uint64_t>(report.accepted ? 1u : 0u));
  report.ledger_hash = hash;
  if (report.diagnostic.empty()) {
    report.diagnostic =
        report.accepted ? "perception ledger cell accepted"
                        : "perception ledger cell missing required channels";
  }
  return report;
}

WorldPerceptionLedgerReport summarizeWorldPerceptionLedger(
    const std::uint64_t region_id, const std::uint32_t required_channel_mask,
    const float minimum_score, std::vector<WorldPerceptionLedgerCellReport> cells) {
  WorldPerceptionLedgerReport report;
  report.region_id = region_id;
  report.required_channel_mask = required_channel_mask;
  report.minimum_score = std::clamp(minimum_score, 0.0f, 1.0f);
  report.cell_count = cells.size();
  report.cells = std::move(cells);

  std::uint64_t hash = mixString(kLedgerSeed, "aster.world-perception.ledger.v1");
  hash = mix(hash, report.region_id);
  hash = mix(hash, report.required_channel_mask);
  hash = mix(hash, report.minimum_score);
  bool all_cells_accepted = !report.cells.empty();
  for (const WorldPerceptionLedgerCellReport &cell : report.cells) {
    report.observed_channel_mask |= cell.observed_channel_mask;
    hash = mix(hash, cell.ledger_hash);
    mergeCellChannels(report, cell);
    all_cells_accepted = all_cells_accepted && cell.accepted;
  }
  report.missing_channel_mask = report.required_channel_mask & ~report.observed_channel_mask;
  report.score =
      worldPerceptionLedgerScore(report.required_channel_mask, report.observed_channel_mask);
  report.accepted = all_cells_accepted && report.missing_channel_mask == 0u &&
                    report.score + 0.0001f >= report.minimum_score;
  hash = mix(hash, report.observed_channel_mask);
  hash = mix(hash, report.missing_channel_mask);
  hash = mix(hash, report.score);
  hash = mix(hash, static_cast<std::uint64_t>(report.accepted ? 1u : 0u));
  report.ledger_hash = hash;
  report.diagnostic = report.accepted ? "world perception ledger accepted"
                                      : "world perception ledger missing required channels";
  return report;
}

} // namespace aster
