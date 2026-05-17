// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_scene.hpp"

#include "aster/core/profiler.hpp"
#include "aster/render/render_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace aster {
namespace {

constexpr std::uint32_t kRuntimeFlagFadeEligible = 1u << 0u;

struct LocalBounds {
  Vec3 min{};
  Vec3 max{};
};

struct RuntimeVec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct RuntimeRenderObject {
  std::uint64_t entity_id = 0;
  std::uint32_t object_index = 0;
  std::uint64_t mesh_key = 0;
  std::uint64_t material_key = 0;
  std::uint32_t render_queue = 0;
  std::uint32_t flags = 0;
  std::uint32_t visibility_class = 0;
  RuntimeVec3 position{};
  RuntimeVec3 visibility_cell{};
  RuntimeVec3 bounds_center{};
  float bounds_radius = 0.0f;
  float opacity = 1.0f;
  float lod_max_distance = 0.0f;
  float lod_min_projected_radius = 0.0f;
  float portal_depth = 0.0f;
  std::uint64_t dynamic_mesh_generation = 0;
};

struct RuntimeCamera {
  RuntimeVec3 position{};
  RuntimeVec3 forward{};
  RuntimeVec3 right{};
  RuntimeVec3 up{};
  float vertical_fov = 0.0f;
  float aspect_ratio = 1.0f;
  float near_plane = 0.0f;
  float far_plane = 0.0f;
};

struct RuntimeLineOfSightFade {
  std::uint32_t enabled = 0;
  RuntimeVec3 camera_position{};
  RuntimeVec3 target_position{};
  float radius = 0.0f;
  float min_opacity = 1.0f;
  float camera_clearance = 0.0f;
  float target_clearance = 0.0f;
  float max_object_radius = 0.0f;
};

struct RuntimePlanOptions {
  RuntimeLineOfSightFade line_of_sight_fade{};
};

struct RuntimeDrawInstance {
  std::uint32_t object_index = 0;
  float opacity = 1.0f;
  float sort_distance_sq = 0.0f;
  std::uint32_t flags = 0;
};

struct RuntimeDrawGroup {
  std::uint64_t mesh_key = 0;
  std::uint64_t material_key = 0;
  std::uint32_t render_queue = 0;
  std::uint32_t pass = 0;
  std::size_t first_instance = 0;
  std::size_t instance_count = 0;
};

struct RuntimeDiagnostics {
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

struct RuntimeFramePlan {
  RuntimeDrawInstance *instances = nullptr;
  std::size_t instance_count = 0;
  RuntimeDrawGroup *groups = nullptr;
  std::size_t group_count = 0;
  RuntimeDiagnostics diagnostics{};
};

extern "C" std::uint32_t
aster_runtime_build_frame_plan(const RuntimeRenderObject *objects, std::size_t object_count,
                               RuntimeCamera camera, RuntimePlanOptions options,
                               RuntimeFramePlan *out_plan);
extern "C" void aster_runtime_free_frame_plan(RuntimeFramePlan plan);

RuntimeVec3 runtimeVec(const Vec3 value) {
  return {value.x, value.y, value.z};
}

float primitiveLocalRadius(const MeshPrimitive primitive) {
  switch (primitive) {
  case MeshPrimitive::Box:
    return 0.8661f;
  case MeshPrimitive::Sphere:
  case MeshPrimitive::Rock:
  case MeshPrimitive::Pillar:
    return 1.0f;
  case MeshPrimitive::Crystal:
    return 1.8f;
  case MeshPrimitive::RuinBlock:
    return 0.95f;
  case MeshPrimitive::Plane:
    return 8.5f;
  }
  return 1.0f;
}

LocalBounds primitiveLocalBounds(const MeshPrimitive primitive) {
  switch (primitive) {
  case MeshPrimitive::Box:
    return {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  case MeshPrimitive::Sphere:
  case MeshPrimitive::Rock:
    return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
  case MeshPrimitive::Crystal:
    return {{-1.0f, -0.9f, -0.9f}, {1.0f, 0.9f, 0.9f}};
  case MeshPrimitive::RuinBlock:
    return {{-0.56f, -0.54f, -0.55f}, {0.56f, 0.54f, 0.55f}};
  case MeshPrimitive::Pillar:
    return {{-1.18f, -0.5f, -1.18f}, {1.18f, 0.5f, 1.18f}};
  case MeshPrimitive::Plane:
    return {{-6.0f, 0.0f, -6.0f}, {6.0f, 0.0f, 6.0f}};
  }
  return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
}

LocalBounds customMeshLocalBounds(const CpuMesh &mesh) {
  if (mesh.vertices.empty()) {
    return {};
  }
  LocalBounds bounds{mesh.vertices.front().position, mesh.vertices.front().position};
  for (const Vertex &vertex : mesh.vertices) {
    bounds.min.x = std::min(bounds.min.x, vertex.position.x);
    bounds.min.y = std::min(bounds.min.y, vertex.position.y);
    bounds.min.z = std::min(bounds.min.z, vertex.position.z);
    bounds.max.x = std::max(bounds.max.x, vertex.position.x);
    bounds.max.y = std::max(bounds.max.y, vertex.position.y);
    bounds.max.z = std::max(bounds.max.z, vertex.position.z);
  }
  return bounds;
}

float customMeshLocalRadius(const CpuMesh &mesh, const Vec3 center) {
  float radius = 0.0f;
  for (const Vertex &vertex : mesh.vertices) {
    radius = std::max(radius, length(vertex.position - center));
  }
  return radius;
}

float maxAbsComponent(const Vec3 value) {
  return std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
}

std::uint64_t fnvAppend(std::uint64_t hash, const void *data, const std::size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

template <typename T> void appendKey(std::uint64_t &hash, const T &value) {
  hash = fnvAppend(hash, &value, sizeof(T));
}

Vec3 cameraForward(const OrbitCamera &camera) {
  Vec3 forward = normalize(camera.target - camera.position());
  if (length(forward) <= 0.0001f) {
    return {0.0f, 0.0f, -1.0f};
  }
  return forward;
}

Vec3 cameraRight(const Vec3 forward) {
  Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  if (length(right) <= 0.0001f) {
    return {1.0f, 0.0f, 0.0f};
  }
  return right;
}

std::uint32_t queueValue(const MaterialRenderQueue queue) {
  return static_cast<std::uint32_t>(queue);
}

MaterialRenderQueue materialQueueFromValue(const std::uint32_t value) {
  switch (value) {
  case 1u:
    return MaterialRenderQueue::Masked;
  case 2u:
    return MaterialRenderQueue::Translucent;
  default:
    return MaterialRenderQueue::Opaque;
  }
}

std::uint32_t visibilityClassValue(const RenderVisibilityClass value) {
  return static_cast<std::uint32_t>(value);
}

std::uint64_t dynamicMeshValue(const DynamicMeshResourceKey key) {
  std::uint64_t hash = key.id ^ 0xcbf29ce484222325ull;
  appendKey(hash, key.generation);
  return 0x8000000000000000ull | (hash & 0x7fffffffffffffffull);
}

FrameRenderPass passFromValue(const std::uint32_t value) {
  return value == 1u ? FrameRenderPass::Transparent : FrameRenderPass::Opaque;
}

class RuntimePlanHandle {
public:
  explicit RuntimePlanHandle(RuntimeFramePlan plan) : plan_(plan) {}
  ~RuntimePlanHandle() {
    aster_runtime_free_frame_plan(plan_);
  }

  RuntimePlanHandle(const RuntimePlanHandle &) = delete;
  RuntimePlanHandle &operator=(const RuntimePlanHandle &) = delete;

  [[nodiscard]] const RuntimeFramePlan &get() const {
    return plan_;
  }

private:
  RuntimeFramePlan plan_{};
};

} // namespace

RenderMeshId renderMeshIdForObject(const RenderObject &object) {
  if (object.custom_mesh != nullptr) {
    if (object.dynamic_mesh.valid()) {
      return {dynamicMeshValue(object.dynamic_mesh)};
    }
    const auto address = reinterpret_cast<std::uintptr_t>(object.custom_mesh.get());
    return {0x8000000000000000ull | static_cast<std::uint64_t>(address)};
  }
  return {static_cast<std::uint64_t>(static_cast<std::uint32_t>(object.primitive)) + 1u};
}

RenderMaterialKey renderMaterialKeyForObject(const RenderObject &object) {
  std::uint64_t hash = 1469598103934665603ull;
  const MaterialRenderQueue queue = classifyMaterialRenderQueue(object.material);
  const bool writes_depth = materialWritesDepth(object.material);
  const bool double_sided = isDoubleSidedMaterial(object.material);
  const bool contact_shadow = object.material.surface_pattern == SurfacePattern::ContactShadow;
  appendKey(hash, queue);
  appendKey(hash, writes_depth);
  appendKey(hash, double_sided);
  appendKey(hash, object.material.cull_mode);
  appendKey(hash, contact_shadow);
  return {hash};
}

RenderBounds renderBoundsForObject(const RenderObject &object) {
  if (object.custom_mesh != nullptr && object.custom_mesh->vertices.empty()) {
    return {object.transform.position, 0.0f};
  }

  const LocalBounds bounds = object.custom_mesh != nullptr
                                 ? customMeshLocalBounds(*object.custom_mesh)
                                 : primitiveLocalBounds(object.primitive);
  const Vec3 local_center = (bounds.min + bounds.max) * 0.5f;
  const float local_radius = object.custom_mesh != nullptr
                                 ? customMeshLocalRadius(*object.custom_mesh, local_center)
                                 : primitiveLocalRadius(object.primitive);
  return {transformPoint(object.transform, local_center),
          local_radius * std::max(maxAbsComponent(object.transform.scale), 0.001f)};
}

void RenderScene::rebuild(const Scene &scene) {
  std::unordered_set<std::uintptr_t> live_custom_meshes;
  live_custom_meshes.reserve(scene.objects().size());
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh != nullptr) {
      live_custom_meshes.insert(reinterpret_cast<std::uintptr_t>(object.custom_mesh.get()));
    }
  }
  for (auto it = custom_bounds_cache_.begin(); it != custom_bounds_cache_.end();) {
    if (live_custom_meshes.contains(it->first)) {
      ++it;
    } else {
      it = custom_bounds_cache_.erase(it);
    }
  }

  const auto cached_bounds = [this](const RenderObject &object) {
    if (object.custom_mesh == nullptr) {
      const LocalBounds bounds = primitiveLocalBounds(object.primitive);
      return CachedLocalBounds{bounds.min, bounds.max, primitiveLocalRadius(object.primitive)};
    }
    if (object.custom_mesh->vertices.empty()) {
      return CachedLocalBounds{{}, {}, 0.0f};
    }
    const auto key = reinterpret_cast<std::uintptr_t>(object.custom_mesh.get());
    if (const auto found = custom_bounds_cache_.find(key); found != custom_bounds_cache_.end()) {
      return found->second;
    }
    const LocalBounds bounds = customMeshLocalBounds(*object.custom_mesh);
    const Vec3 local_center = (bounds.min + bounds.max) * 0.5f;
    const CachedLocalBounds cached{bounds.min, bounds.max,
                                   customMeshLocalRadius(*object.custom_mesh, local_center)};
    custom_bounds_cache_.emplace(key, cached);
    return cached;
  };
  const auto world_bounds = [&cached_bounds](const RenderObject &object) {
    const CachedLocalBounds bounds = cached_bounds(object);
    if (bounds.radius <= 0.0f && object.custom_mesh != nullptr) {
      return RenderBounds{object.transform.position, 0.0f};
    }
    const Vec3 local_center = (bounds.min + bounds.max) * 0.5f;
    return RenderBounds{
        transformPoint(object.transform, local_center),
        bounds.radius * std::max(maxAbsComponent(object.transform.scale), 0.001f)};
  };

  objects_.clear();
  objects_.reserve(scene.objects().size());
  for (std::size_t index = 0; index < scene.objects().size(); ++index) {
    const RenderObject &object = scene.objects()[index];
    const bool plane_primitive = object.custom_mesh == nullptr && object.primitive == MeshPrimitive::Plane;
    const bool fade_eligible =
        object.camera_occlusion_fade && !plane_primitive && allowsCameraOcclusionFade(object.material);
    objects_.push_back({.entity = {static_cast<std::uint64_t>(index) + 1u},
                        .object_index = index,
                        .mesh = renderMeshIdForObject(object),
                        .material = renderMaterialKeyForObject(object),
                        .render_queue = classifyMaterialRenderQueue(object.material),
                        .flags = fade_eligible ? kRuntimeFlagFadeEligible : 0u,
                        .visibility_class =
                            object.visibility_hint.visibility_class,
                        .position = object.transform.position,
                        .visibility_cell = object.visibility_hint.cell,
                        .bounds = world_bounds(object),
                        .opacity = object.material.opacity,
                        .lod_max_distance = std::max(object.lod.max_distance, 0.0f),
                        .lod_min_projected_radius =
                            std::max(object.lod.min_projected_radius, 0.0f),
                        .portal_depth = object.visibility_hint.portal_depth,
                        .dynamic_mesh_generation =
                            object.dynamic_mesh.valid() ? object.dynamic_mesh.generation : 0u});
  }
}

FrameRenderPlan buildFrameRenderPlan(const RenderScene &scene, const OrbitCamera &camera,
                                     const LineOfSightFadeSettings &fade,
                                     const int framebuffer_width,
                                     const int framebuffer_height) {
  ASTER_PROFILE_SCOPE("RenderScene::buildFrameRenderPlan");
  const float aspect = static_cast<float>(std::max(framebuffer_width, 1)) /
                       static_cast<float>(std::max(framebuffer_height, 1));
  const Vec3 position = camera.position();
  const Vec3 forward = cameraForward(camera);
  const Vec3 right = cameraRight(forward);
  const Vec3 up = normalize(cross(right, forward));

  thread_local std::vector<RuntimeRenderObject> runtime_objects;
  runtime_objects.clear();
  runtime_objects.reserve(scene.objects().size());
  for (const RenderObjectPacket &object : scene.objects()) {
    if (object.object_index > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Render object index exceeds the Rust planner ABI.");
    }
    runtime_objects.push_back({.entity_id = object.entity.value,
                               .object_index = static_cast<std::uint32_t>(object.object_index),
                               .mesh_key = object.mesh.value,
                               .material_key = object.material.value,
                               .render_queue = queueValue(object.render_queue),
                               .flags = object.flags,
                               .visibility_class =
                                   visibilityClassValue(object.visibility_class),
                               .position = runtimeVec(object.position),
                               .visibility_cell = runtimeVec(object.visibility_cell),
                               .bounds_center = runtimeVec(object.bounds.center),
                               .bounds_radius = object.bounds.radius,
                               .opacity = object.opacity,
                               .lod_max_distance = object.lod_max_distance,
                               .lod_min_projected_radius =
                                   object.lod_min_projected_radius,
                               .portal_depth = object.portal_depth,
                               .dynamic_mesh_generation =
                                   object.dynamic_mesh_generation});
  }

  RuntimePlanOptions options;
  options.line_of_sight_fade = {.enabled = fade.enabled ? 1u : 0u,
                                .camera_position = runtimeVec(fade.camera_position),
                                .target_position = runtimeVec(fade.target_position),
                                .radius = fade.radius,
                                .min_opacity = fade.min_opacity,
                                .camera_clearance = fade.camera_clearance,
                                .target_clearance = fade.target_clearance,
                                .max_object_radius = fade.max_object_radius};
  const RuntimeCamera runtime_camera{.position = runtimeVec(position),
                                     .forward = runtimeVec(forward),
                                     .right = runtimeVec(right),
                                     .up = runtimeVec(up),
                                     .vertical_fov = camera.vertical_fov,
                                     .aspect_ratio = aspect,
                                     .near_plane = camera.near_plane,
                                     .far_plane = camera.far_plane};
  RuntimeFramePlan raw_plan;
  const std::uint32_t plan_ok = aster_runtime_build_frame_plan(
      runtime_objects.empty() ? nullptr : runtime_objects.data(), runtime_objects.size(),
      runtime_camera, options, &raw_plan);
  if (plan_ok == 0u) {
    throw std::runtime_error("Rust render planner failed.");
  }
  RuntimePlanHandle handle(raw_plan);
  const RuntimeFramePlan &runtime_plan = handle.get();
  if ((runtime_plan.instance_count > 0 && runtime_plan.instances == nullptr) ||
      (runtime_plan.group_count > 0 && runtime_plan.groups == nullptr)) {
    throw std::runtime_error("Rust render planner returned invalid buffers.");
  }

  FrameRenderPlan plan;
  plan.instances.reserve(runtime_plan.instance_count);
  for (std::size_t i = 0; i < runtime_plan.instance_count; ++i) {
    const RuntimeDrawInstance &instance = runtime_plan.instances[i];
    plan.instances.push_back({.object_index = instance.object_index,
                              .opacity = instance.opacity,
                              .sort_distance_sq = instance.sort_distance_sq,
                              .flags = instance.flags});
  }
  plan.groups.reserve(runtime_plan.group_count);
  for (std::size_t i = 0; i < runtime_plan.group_count; ++i) {
    const RuntimeDrawGroup &group = runtime_plan.groups[i];
    plan.groups.push_back({.mesh = {group.mesh_key},
                           .material = {group.material_key},
                           .render_queue = materialQueueFromValue(group.render_queue),
                           .pass = passFromValue(group.pass),
                           .first_instance = group.first_instance,
                           .instance_count = group.instance_count});
  }
  plan.diagnostics = {.object_count = runtime_plan.diagnostics.object_count,
                      .visible_objects = runtime_plan.diagnostics.visible_objects,
                      .culled_objects = runtime_plan.diagnostics.culled_objects,
                      .opaque_groups = runtime_plan.diagnostics.opaque_groups,
                      .transparent_groups = runtime_plan.diagnostics.transparent_groups,
                      .instance_groups = runtime_plan.diagnostics.instance_groups,
                      .planned_instances = runtime_plan.diagnostics.planned_instances,
                      .lod_culled_objects = runtime_plan.diagnostics.lod_culled_objects,
                      .visibility_hint_objects =
                          runtime_plan.diagnostics.visibility_hint_objects,
                      .dynamic_mesh_objects = runtime_plan.diagnostics.dynamic_mesh_objects,
                      .rust_plan_seconds = runtime_plan.diagnostics.rust_plan_seconds};
  return plan;
}

} // namespace aster
