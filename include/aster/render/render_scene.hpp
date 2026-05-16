// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/camera.hpp"
#include "aster/scene/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aster {

struct LineOfSightFadeSettings;

struct RenderEntityId {
  std::uint64_t value = 0;
};

struct RenderMeshId {
  std::uint64_t value = 0;
};

struct RenderMaterialKey {
  std::uint64_t value = 0;
};

struct RenderBounds {
  Vec3 center{};
  float radius = 0.0f;
};

enum class FrameRenderPass : std::uint32_t {
  Opaque = 0,
  Transparent = 1,
};

enum class FrameRenderInstanceFlags : std::uint32_t {
  None = 0,
  LineOfSightFade = 1u << 0u,
};

struct RenderObjectPacket {
  RenderEntityId entity{};
  std::size_t object_index = 0;
  RenderMeshId mesh{};
  RenderMaterialKey material{};
  MaterialRenderQueue render_queue = MaterialRenderQueue::Opaque;
  std::uint32_t flags = 0;
  Vec3 position{};
  RenderBounds bounds{};
  float opacity = 1.0f;
};

struct FrameRenderInstance {
  std::size_t object_index = 0;
  float opacity = 1.0f;
  float sort_distance_sq = 0.0f;
  std::uint32_t flags = 0;
};

struct FrameRenderDrawGroup {
  RenderMeshId mesh{};
  RenderMaterialKey material{};
  MaterialRenderQueue render_queue = MaterialRenderQueue::Opaque;
  FrameRenderPass pass = FrameRenderPass::Opaque;
  std::size_t first_instance = 0;
  std::size_t instance_count = 0;
};

struct FrameRenderDiagnostics {
  std::size_t object_count = 0;
  std::size_t visible_objects = 0;
  std::size_t culled_objects = 0;
  std::size_t opaque_groups = 0;
  std::size_t transparent_groups = 0;
  std::size_t instance_groups = 0;
  std::size_t planned_instances = 0;
  double rust_plan_seconds = 0.0;
};

class RenderScene {
public:
  void rebuild(const Scene &scene);

  [[nodiscard]] const std::vector<RenderObjectPacket> &objects() const {
    return objects_;
  }

private:
  std::vector<RenderObjectPacket> objects_;
};

struct FrameRenderPlan {
  std::vector<FrameRenderInstance> instances;
  std::vector<FrameRenderDrawGroup> groups;
  FrameRenderDiagnostics diagnostics{};
};

[[nodiscard]] FrameRenderPlan buildFrameRenderPlan(const RenderScene &scene,
                                                   const OrbitCamera &camera,
                                                   const LineOfSightFadeSettings &fade,
                                                   int framebuffer_width,
                                                   int framebuffer_height);

[[nodiscard]] RenderMeshId renderMeshIdForObject(const RenderObject &object);
[[nodiscard]] RenderMaterialKey renderMaterialKeyForObject(const RenderObject &object);
[[nodiscard]] RenderBounds renderBoundsForObject(const RenderObject &object);

} // namespace aster
