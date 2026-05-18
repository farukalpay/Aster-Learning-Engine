// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"

#include <cstdint>
#include <string_view>

namespace aster {

enum class RenderGraphPass : std::uint32_t {
  SceneColorDepth,
  LightCull,
  ShadowAtlas,
  Opaque,
  ContactShadow,
  SceneLighting,
  VolumetricFog,
  ReflectionProbe,
  Transparent,
  UiComposite,
  Capture,
};

enum class RenderGraphResource : std::uint32_t {
  SceneColor,
  SceneDepth,
  LightClusters,
  ShadowAtlas,
  VolumetricFog,
  ReflectionProbes,
  UiOverlay,
  CaptureReadback,
};

enum class RenderGraphResourceLifetime : std::uint32_t {
  Frame,
  Imported,
  Readback,
};

struct RenderGraphResourceDesc {
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Frame;
};

struct RenderGraphPassDesc {
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  std::uint32_t reads = 0u;
  std::uint32_t writes = 0u;
};

using FixedRenderGraph = framegraph::CompiledFrameGraph;

[[nodiscard]] constexpr std::uint32_t renderGraphResourceBit(const RenderGraphResource resource) {
  return 1u << static_cast<std::uint32_t>(resource);
}

[[nodiscard]] std::string_view renderGraphPassName(RenderGraphPass pass);
[[nodiscard]] std::string_view renderGraphResourceName(RenderGraphResource resource);
[[nodiscard]] std::string_view renderGraphResourceLifetimeName(RenderGraphResourceLifetime lifetime);
[[nodiscard]] RenderGraphPass renderGraphPassFromName(std::string_view name);
[[nodiscard]] RenderGraphResource renderGraphResourceFromName(std::string_view name);
[[nodiscard]] framegraph::FrameGraph makeDefaultFrameGraph(bool ui_overlay_enabled = true,
                                                          bool capture_enabled = true);
[[nodiscard]] FixedRenderGraph makeFixedRenderGraph(bool ui_overlay_enabled = true,
                                                    bool capture_enabled = true);

} // namespace aster
