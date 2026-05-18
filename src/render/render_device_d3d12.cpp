// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#ifdef _WIN32

#include "aster/core/profiler.hpp"
#include "aster/render/software_framebuffer.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <chrono>
#include <memory>

namespace {

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

aster::RenderBackendCapabilities d3d12Capabilities() {
  const std::uint32_t graph_resources =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::UiOverlay) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  return {.kind = aster::RenderBackendKind::D3D12,
          .name = "Aster Native D3D12 Bootstrap",
          .gpu = true,
          .supports_shader_materials = false,
          .supports_texture_sampling = false,
          .supports_instancing = false,
          .supports_capture = true,
          .supports_ui_composite = false,
          .supports_gpu_timestamps = false,
          .graph_resource_mask = graph_resources};
}

class D3D12NativeRenderBackend final : public aster::NativeRenderBackend {
public:
  bool initialize() override {
    UINT factory_flags = 0u;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
      debug->EnableDebugLayer();
      factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_)))) {
      return false;
    }
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapter_index = 0u;
         factory_->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapter_index) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u) {
        adapter.Reset();
        continue;
      }
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                      IID_PPV_ARGS(&device_)))) {
        break;
      }
      adapter.Reset();
    }
    if (device_ == nullptr &&
        FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
      return false;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    return SUCCEEDED(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_)));
  }

  aster::FrameStats render(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                           const aster::OrbitCamera &camera,
                           const aster::RendererSettings &settings,
                           const aster::FixedRenderGraph &graph,
                           const aster::PreparedRenderMeshes &meshes,
                           const int framebuffer_width, const int framebuffer_height,
                           const double frame_seconds) override {
    (void)scene;
    (void)plan;
    (void)camera;
    (void)settings;
    (void)meshes;
    ASTER_PROFILE_SCOPE("D3D12NativeRenderBackend::render");
    aster::FrameStats stats;
    stats.frame_seconds = frame_seconds;
    stats.framebuffer_width = framebuffer_width;
    stats.framebuffer_height = framebuffer_height;
    stats.graph_passes = graph.passes.size();
    stats.backend_feature_mask = capabilities().graph_resource_mask;
    const auto encode_start = std::chrono::steady_clock::now();

    aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
    framebuffer.resize(framebuffer_width, framebuffer_height);
    framebuffer.clear(settings.pipeline.clear_color);

    const auto encode_end = std::chrono::steady_clock::now();
    stats.render_encode_seconds =
        std::chrono::duration<double>(encode_end - encode_start).count();
    return stats;
  }

  const char *backendName() const override {
    return "Aster Native D3D12 Bootstrap";
  }

  aster::RenderBackendCapabilities capabilities() const override {
    return d3d12Capabilities();
  }

private:
  ComPtr<IDXGIFactory6> factory_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
};

} // namespace

namespace aster {

std::unique_ptr<NativeRenderBackend> createNativeRenderBackend() {
  return std::make_unique<D3D12NativeRenderBackend>();
}

bool captureNativeFrameToActiveFramebuffer() {
  return false;
}

void clearNativeFrame() {}

} // namespace aster

#endif
