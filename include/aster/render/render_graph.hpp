// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"
#include "aster/rhi/framebuffer.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

enum class RenderGraphViewportPolicy : std::uint32_t {
  FullFrame,
  ResourceExtent,
};

enum class RenderGraphScissorPolicy : std::uint32_t {
  FullFrame,
  Viewport,
};

enum class RenderGraphDebugCapturePolicy : std::uint32_t {
  Disabled,
  OnRequest,
  Always,
};

enum class RenderGraphExecutorKey : std::uint32_t {
  None,
  ClearScene,
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

struct RenderGraphResourceBindingDesc {
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  rhi::ResourceState state = rhi::ResourceState::ShaderRead;
  rhi::AttachmentLoadOp load_op = rhi::AttachmentLoadOp::Load;
  rhi::AttachmentStoreOp store_op = rhi::AttachmentStoreOp::Store;
  rhi::AttachmentClearValue clear_value{};
  rhi::QueueKind queue = rhi::QueueKind::Graphics;
  bool required = true;
};

struct RenderGraphPassDeclaration {
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  std::string name;
  std::vector<RenderGraphResourceBindingDesc> inputs;
  std::vector<RenderGraphResourceBindingDesc> outputs;
  RenderGraphViewportPolicy viewport = RenderGraphViewportPolicy::FullFrame;
  RenderGraphScissorPolicy scissor = RenderGraphScissorPolicy::Viewport;
  RenderGraphDebugCapturePolicy debug_capture = RenderGraphDebugCapturePolicy::OnRequest;
  RenderGraphExecutorKey executor = RenderGraphExecutorKey::None;
  rhi::QueueKind queue = rhi::QueueKind::Graphics;
  bool produces_backend_work = false;
};

class RenderPassRegistry {
public:
  RenderPassRegistry &add(RenderGraphPassDeclaration declaration);

  [[nodiscard]] const RenderGraphPassDeclaration *find(RenderGraphPass pass) const;
  [[nodiscard]] const RenderGraphPassDeclaration *find(std::string_view name) const;
  [[nodiscard]] const std::vector<RenderGraphPassDeclaration> &passes() const noexcept {
    return passes_;
  }
  [[nodiscard]] std::vector<std::string> validate() const;

private:
  std::vector<RenderGraphPassDeclaration> passes_;
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
[[nodiscard]] std::string_view renderGraphExecutorKeyName(RenderGraphExecutorKey key);
[[nodiscard]] std::string_view renderGraphDebugCapturePolicyName(RenderGraphDebugCapturePolicy policy);
[[nodiscard]] framegraph::ResourceDesc defaultRenderGraphResourceDesc(RenderGraphResource resource);
[[nodiscard]] RenderPassRegistry makeDefaultRenderPassRegistry(bool ui_overlay_enabled = true,
                                                               bool capture_enabled = true);
[[nodiscard]] const RenderGraphPassDeclaration *defaultRenderPassDeclaration(RenderGraphPass pass);
[[nodiscard]] framegraph::FrameGraph makeDefaultFrameGraph(bool ui_overlay_enabled = true,
                                                          bool capture_enabled = true);
[[nodiscard]] FixedRenderGraph makeFixedRenderGraph(bool ui_overlay_enabled = true,
                                                    bool capture_enabled = true);

} // namespace aster
