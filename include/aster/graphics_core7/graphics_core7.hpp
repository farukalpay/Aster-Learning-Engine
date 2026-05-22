// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/render/render_graph.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster::graphics_core7 {

enum class Signal : std::uint32_t {
  SurfaceTruthBuffer,
  LightTransportEvidence,
  ShadowContinuityField,
  ReflectionProbeResidency,
  MaterialFrequencyAudit,
  TemporalStabilityAudit,
  BackendVisualDelta,
  PlayerReadableFrameVerdict,
};

enum class SignalStatus : std::uint32_t {
  NotRequired,
  Proven,
  Degraded,
  MissingProof,
  Unsupported,
};

enum class VerdictStatus : std::uint32_t {
  Accepted,
  Degraded,
  Rejected,
};

enum Flags : std::uint32_t {
  Strict = 1u << 0u,
  RequireNativeBackend = 1u << 1u,
};

struct Settings {
  std::uint32_t flags = 0u;
  std::uint32_t required_signal_mask = 0u;
  float minimum_score = 0.74f;

  [[nodiscard]] bool strict() const noexcept {
    return (flags & Strict) != 0u;
  }
};

struct SignalEvidence {
  Signal signal = Signal::SurfaceTruthBuffer;
  SignalStatus status = SignalStatus::NotRequired;
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  bool required = false;
  float score = 1.0f;
  float threshold = 1.0f;
  std::string label;
  std::string evidence;
  std::string message;
  std::uint64_t evidence_hash = 0u;
};

struct PlayerReadableFrameVerdict {
  VerdictStatus status = VerdictStatus::Accepted;
  bool accepted = true;
  bool strict = false;
  float score = 1.0f;
  float minimum_score = 0.74f;
  std::uint32_t required_signal_mask = 0u;
  std::uint32_t proven_signal_mask = 0u;
  std::uint32_t degraded_signal_mask = 0u;
  std::uint32_t missing_signal_mask = 0u;
  std::uint32_t unsupported_signal_mask = 0u;
  std::size_t signal_count = 0u;
  std::size_t rejected_signal_count = 0u;
  std::uint64_t evidence_hash = 0u;
  std::string diagnostic;
};

[[nodiscard]] constexpr std::uint32_t signalBit(const Signal signal) {
  return 1u << static_cast<std::uint32_t>(signal);
}

[[nodiscard]] std::string_view signalName(Signal signal) noexcept;
[[nodiscard]] std::string_view signalStatusName(SignalStatus status) noexcept;
[[nodiscard]] std::string_view verdictStatusName(VerdictStatus status) noexcept;
[[nodiscard]] std::uint64_t signalEvidenceHash(const SignalEvidence &evidence) noexcept;
[[nodiscard]] std::uint64_t verdictHash(const PlayerReadableFrameVerdict &verdict,
                                        const std::vector<SignalEvidence> &signals) noexcept;

} // namespace aster::graphics_core7
