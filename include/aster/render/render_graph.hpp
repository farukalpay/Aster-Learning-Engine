// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace aster {

enum class RenderGraphPass : std::uint32_t {
  SceneColorDepth,
  Opaque,
  ContactShadow,
  Transparent,
  UiComposite,
  Capture,
};

enum class RenderGraphResource : std::uint32_t {
  SceneColor,
  SceneDepth,
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

struct FixedRenderGraph {
  std::vector<RenderGraphResourceDesc> resources;
  std::vector<RenderGraphPassDesc> passes;
};

[[nodiscard]] constexpr std::uint32_t renderGraphResourceBit(const RenderGraphResource resource) {
  return 1u << static_cast<std::uint32_t>(resource);
}

[[nodiscard]] std::string_view renderGraphPassName(RenderGraphPass pass);
[[nodiscard]] std::string_view renderGraphResourceName(RenderGraphResource resource);
[[nodiscard]] std::string_view renderGraphResourceLifetimeName(RenderGraphResourceLifetime lifetime);
[[nodiscard]] FixedRenderGraph makeFixedRenderGraph(bool ui_overlay_enabled = true,
                                                    bool capture_enabled = true);

} // namespace aster
