// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_graph.hpp"

namespace aster {

std::string_view renderGraphPassName(const RenderGraphPass pass) {
  switch (pass) {
  case RenderGraphPass::SceneColorDepth:
    return "scene-color-depth";
  case RenderGraphPass::Opaque:
    return "opaque";
  case RenderGraphPass::ContactShadow:
    return "contact-shadow";
  case RenderGraphPass::Transparent:
    return "transparent";
  case RenderGraphPass::UiComposite:
    return "ui-composite";
  case RenderGraphPass::Capture:
    return "capture";
  }
  return "unknown";
}

std::string_view renderGraphResourceName(const RenderGraphResource resource) {
  switch (resource) {
  case RenderGraphResource::SceneColor:
    return "scene-color";
  case RenderGraphResource::SceneDepth:
    return "scene-depth";
  case RenderGraphResource::UiOverlay:
    return "ui-overlay";
  case RenderGraphResource::CaptureReadback:
    return "capture-readback";
  }
  return "unknown";
}

std::string_view renderGraphResourceLifetimeName(const RenderGraphResourceLifetime lifetime) {
  switch (lifetime) {
  case RenderGraphResourceLifetime::Frame:
    return "frame";
  case RenderGraphResourceLifetime::Imported:
    return "imported";
  case RenderGraphResourceLifetime::Readback:
    return "readback";
  }
  return "unknown";
}

FixedRenderGraph makeFixedRenderGraph(const bool ui_overlay_enabled, const bool capture_enabled) {
  const std::uint32_t color = renderGraphResourceBit(RenderGraphResource::SceneColor);
  const std::uint32_t depth = renderGraphResourceBit(RenderGraphResource::SceneDepth);
  const std::uint32_t ui = renderGraphResourceBit(RenderGraphResource::UiOverlay);
  const std::uint32_t capture = renderGraphResourceBit(RenderGraphResource::CaptureReadback);

  FixedRenderGraph graph;
  graph.resources = {
      {RenderGraphResource::SceneColor, RenderGraphResourceLifetime::Frame},
      {RenderGraphResource::SceneDepth, RenderGraphResourceLifetime::Frame},
      {RenderGraphResource::UiOverlay, RenderGraphResourceLifetime::Frame},
      {RenderGraphResource::CaptureReadback, RenderGraphResourceLifetime::Readback},
  };
  graph.passes = {
      {RenderGraphPass::SceneColorDepth, 0u, color | depth},
      {RenderGraphPass::Opaque, color | depth, color | depth},
      {RenderGraphPass::ContactShadow, color | depth, color | depth},
      {RenderGraphPass::Transparent, color | depth, color},
  };
  if (ui_overlay_enabled) {
    graph.passes.push_back({RenderGraphPass::UiComposite, color | ui, color});
  }
  if (capture_enabled) {
    graph.passes.push_back({RenderGraphPass::Capture, color, capture});
  }
  return graph;
}

} // namespace aster
