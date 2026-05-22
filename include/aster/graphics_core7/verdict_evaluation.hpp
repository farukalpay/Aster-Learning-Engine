// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/graphics_core7/graphics_core7.hpp"

namespace aster {
struct FrameForensics;
struct FrameStats;
struct RenderBackendCapabilities;
struct RendererSettings;
}

namespace aster::graphics_core7 {

struct EvaluationResult {
  PlayerReadableFrameVerdict verdict{};
  std::vector<SignalEvidence> signals;
};

[[nodiscard]] EvaluationResult evaluateFrameTruth(const FrameForensics &forensics,
                                                  const RenderBackendCapabilities &capabilities,
                                                  const RendererSettings &settings,
                                                  const FrameStats &stats);

void applyEvaluation(FrameForensics &forensics, EvaluationResult result);

} // namespace aster::graphics_core7
