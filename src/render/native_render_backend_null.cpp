// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#include "aster/core/profiler.hpp"
#include "aster/render/software_framebuffer.hpp"

#include <chrono>
#include <memory>

namespace {

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
          .graph_resource_mask = graph_resources};
}

class NullNativeRenderBackend final : public aster::NativeRenderBackend {
public:
  bool initialize() override {
    return true;
  }

  aster::FrameStats render(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                           const aster::OrbitCamera &camera,
                           const aster::RendererSettings &settings,
                           const aster::FixedRenderGraph &graph,
                           const aster::PreparedRenderMeshes &meshes, const int framebuffer_width,
                           const int framebuffer_height, const double frame_seconds,
                           aster::FrameForensics *forensics) override {
    (void)scene;
    (void)plan;
    (void)camera;
    (void)settings;
    (void)meshes;
    ASTER_PROFILE_SCOPE("NullNativeRenderBackend::render");
    const auto encode_start = std::chrono::steady_clock::now();
    aster::FrameStats stats;
    stats.frame_seconds = frame_seconds;
    stats.framebuffer_width = framebuffer_width;
    stats.framebuffer_height = framebuffer_height;
    stats.graph_passes = graph.passes.size();
    stats.graph_resources = graph.resources.size();
    stats.graph_barriers = graph.barriers.size();
    stats.graph_transient_resources = graph.transient_resource_count;
    stats.backend_feature_mask = capabilities().graph_resource_mask;
    stats.backend_kind_value = static_cast<std::uint32_t>(capabilities().kind);
    aster::activeFrameBuffer().resize(framebuffer_width, framebuffer_height);
    aster::activeFrameBuffer().clearTransparent();
    const auto encode_end = std::chrono::steady_clock::now();
    stats.render_encode_seconds =
        std::chrono::duration<double>(encode_end - encode_start).count();
    if (forensics != nullptr) {
      forensics->passes.push_back({.pass = aster::RenderGraphPass::SceneColorDepth,
                                   .name = "null-clear",
                                   .encode_seconds = stats.render_encode_seconds});
      forensics->events.push_back(
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
