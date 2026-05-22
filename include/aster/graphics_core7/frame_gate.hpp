// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/graphics_core7/graphics_core7.hpp"
#include "aster/render/render_graph.hpp"

namespace aster {
struct FrameForensics;
struct FrameStats;
struct RenderBackendCapabilities;
struct RendererSettings;
} // namespace aster

namespace aster::graphics_core7 {

struct FrameGateReport {
  PlayerReadableFrameVerdict verdict{};
  std::vector<SignalEvidence> rejected_signals;
};

[[nodiscard]] FrameGateReport preflightFrameTruth(const FixedRenderGraph &graph,
                                                  const RenderBackendCapabilities &capabilities,
                                                  const RendererSettings &settings,
                                                  const FrameStats &stats,
                                                  const FrameForensics &forensics);

void applyFrameGateReport(FrameForensics &forensics, FrameGateReport report);

} // namespace aster::graphics_core7
