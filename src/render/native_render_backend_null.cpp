// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#include "aster/core/profiler.hpp"
#include "aster/render/software_framebuffer.hpp"

#include <chrono>
#include <memory>

namespace {

aster::rhi::DeviceCapabilities nullCapabilityTable() {
  aster::rhi::DeviceCapabilities table;
  table.backend = aster::rhi::BackendKind::Null;
  table.shader_materials = false;
  table.texture_sampling = false;
  table.instancing = false;
  table.capture = false;
  table.ui_composite = false;
  table.gpu_timestamps = false;
  table.presentation = aster::rhi::PresentationMode::None;
  return table;
}

aster::RenderBackendCapabilities nullCapabilities() {
  const std::uint32_t graph_resources =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::UiOverlay) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  return {.kind = aster::RenderBackendKind::Null,
          .name = "Aster Null Renderer",
          .gpu = false,
          .supports_shader_materials = false,
          .supports_texture_sampling = false,
          .supports_instancing = false,
          .supports_capture = false,
          .supports_ui_composite = false,
          .supports_gpu_timestamps = false,
          .graph_resource_mask = graph_resources,
          .capability_table = nullCapabilityTable()};
}

class NullNativeRenderBackend final : public aster::NativeRenderBackend {
public:
  bool initialize() override {
    return true;
  }

  aster::FrameStats render(const aster::FrameExecutionContext &context) override {
    (void)context.scene;
    (void)context.plan;
    (void)context.camera;
    (void)context.settings;
    (void)context.meshes;
    (void)context.material_library;
    ASTER_PROFILE_SCOPE("NullNativeRenderBackend::render");
    const auto encode_start = std::chrono::steady_clock::now();
    aster::FrameStats stats;
    stats.frame_seconds = context.frame_seconds;
    stats.framebuffer_width = context.framebuffer_width;
    stats.framebuffer_height = context.framebuffer_height;
    stats.graph_passes = context.graph.passes.size();
    stats.graph_resources = context.graph.resources.size();
    stats.graph_barriers = context.graph.barriers.size();
    stats.graph_transient_resources = context.graph.transient_resource_count;
    stats.backend_feature_mask = capabilities().graph_resource_mask;
    stats.backend_kind_value = static_cast<std::uint32_t>(capabilities().kind);
    aster::activeFrameBuffer().resize(context.framebuffer_width, context.framebuffer_height);
    aster::activeFrameBuffer().clearTransparent();
    const auto encode_end = std::chrono::steady_clock::now();
    stats.render_encode_seconds =
        std::chrono::duration<double>(encode_end - encode_start).count();
    if (context.forensics != nullptr) {
      context.forensics->passes.push_back({.pass = aster::RenderGraphPass::SceneColorDepth,
                                           .name = "null-clear",
                                           .encode_seconds = stats.render_encode_seconds});
      context.forensics->events.push_back(
          {.kind = aster::FrameDiagnosticKind::BackendFallback,
           .severity = aster::FrameDiagnosticSeverity::Warning,
           .label = "null-renderer",
           .message = "Null renderer produced a transparent frame without scene drawing."});
    }
    return stats;
  }

  const char *backendName() const override {
    return "Aster Null Renderer";
  }

  aster::RenderBackendCapabilities capabilities() const override {
    return nullCapabilities();
  }
};

} // namespace

namespace aster {

std::unique_ptr<NativeRenderBackend> createNullRenderBackend() {
  return std::make_unique<NullNativeRenderBackend>();
}

} // namespace aster
