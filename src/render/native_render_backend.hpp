// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/platform/window.hpp"
#include "aster/render/render_device.hpp"

namespace aster {

struct PreparedRenderMeshes {
  const CpuMesh *box = nullptr;
  const CpuMesh *sphere = nullptr;
  const CpuMesh *plane = nullptr;
  const CpuMesh *contact_shadow_plane = nullptr;
  const CpuMesh *rock = nullptr;
  const CpuMesh *crystal = nullptr;
  const CpuMesh *ruin_block = nullptr;
  const CpuMesh *pillar = nullptr;
  const std::unordered_map<const CpuMesh *, CpuMesh> *custom_meshes = nullptr;
  const std::unordered_map<DynamicMeshResourceKey, CpuMesh, DynamicMeshResourceKeyHash>
      *custom_mesh_resources = nullptr;
};

struct FrameExecutionContext {
  const Scene &scene;
  const FrameRenderPlan &plan;
  const OrbitCamera &camera;
  const RendererSettings &settings;
  const FixedRenderGraph &graph;
  const PreparedRenderMeshes &meshes;
  const ClusteredLightFrameData *clustered_lights = nullptr;
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  double frame_seconds = 0.0;
  const MaterialResourceLibrary *material_library = nullptr;
  FrameForensics *forensics = nullptr;
};

class NativeRenderBackend {
public:
  virtual ~NativeRenderBackend() = default;

  virtual bool initialize() = 0;
  virtual bool bindWindow(const NativeWindowSurface &surface) {
    (void)surface;
    return false;
  }
  virtual bool resizeDrawable(const NativeWindowSurface &surface) {
    (void)surface;
    return false;
  }
  virtual FrameStats render(const FrameExecutionContext &context) = 0;
  virtual RendererPresentResult present(const NativeWindowSurface &surface,
                                        const RendererPresentDesc &desc) {
    (void)surface;
    (void)desc;
    RendererPresentResult result;
    const RenderBackendCapabilities caps = capabilities();
    result.backend = caps.kind;
    result.presentation = caps.capability_table.presentation;
    return result;
  }
  [[nodiscard]] virtual RendererPresentationStatus presentationStatus() const {
    RendererPresentationStatus status;
    const RenderBackendCapabilities caps = capabilities();
    status.backend = caps.kind;
    status.presentation = caps.capability_table.presentation;
    return status;
  }
  [[nodiscard]] virtual const char *backendName() const = 0;
  [[nodiscard]] virtual RenderBackendCapabilities capabilities() const = 0;
};

[[nodiscard]] std::unique_ptr<NativeRenderBackend> createNativeRenderBackend();
[[nodiscard]] std::unique_ptr<NativeRenderBackend> createNullRenderBackend();
[[nodiscard]] bool captureNativeFrameToActiveFramebuffer();
void clearNativeFrame();

} // namespace aster
