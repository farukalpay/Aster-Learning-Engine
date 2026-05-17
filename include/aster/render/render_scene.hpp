// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/camera.hpp"
#include "aster/scene/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
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
  RenderVisibilityClass visibility_class = RenderVisibilityClass::General;
  Vec3 position{};
  Vec3 visibility_cell{};
  RenderBounds bounds{};
  float opacity = 1.0f;
  float lod_max_distance = 0.0f;
  float lod_min_projected_radius = 0.0f;
  float portal_depth = 0.0f;
  std::uint64_t dynamic_mesh_generation = 0u;
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
  std::size_t lod_culled_objects = 0;
  std::size_t visibility_hint_objects = 0;
  std::size_t dynamic_mesh_objects = 0;
  double rust_plan_seconds = 0.0;
};

class RenderScene {
public:
  void rebuild(const Scene &scene);

  [[nodiscard]] const std::vector<RenderObjectPacket> &objects() const {
    return objects_;
  }

private:
  struct CachedLocalBounds {
    Vec3 min{};
    Vec3 max{};
    float radius = 0.0f;
  };

  std::vector<RenderObjectPacket> objects_;
  std::unordered_map<std::uintptr_t, CachedLocalBounds> custom_bounds_cache_;
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
