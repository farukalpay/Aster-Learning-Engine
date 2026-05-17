// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

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

class NativeRenderBackend {
public:
  virtual ~NativeRenderBackend() = default;

  virtual bool initialize() = 0;
  virtual FrameStats render(const Scene &scene, const FrameRenderPlan &plan,
                            const OrbitCamera &camera, const RendererSettings &settings,
                            const PreparedRenderMeshes &meshes, int framebuffer_width,
                            int framebuffer_height, double frame_seconds) = 0;
  [[nodiscard]] virtual const char *backendName() const = 0;
};

[[nodiscard]] std::unique_ptr<NativeRenderBackend> createNativeRenderBackend();
[[nodiscard]] bool captureNativeFrameToActiveFramebuffer();
void clearNativeFrame();

} // namespace aster
