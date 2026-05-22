// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/render_scene.hpp"

#include "aster/core/profiler.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/render/render_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

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
  std::uint64_t perceptual_primitive_hash = 0;
  std::uint64_t perceptual_sound_surface_class_hash = 0;
  std::uint64_t perceptual_neural_irradiance_hash = 0;
  RuntimeVec3 perceptual_neural_irradiance{};
  float perceptual_material_memory = 0.0f;
  float perceptual_interaction_residue = 0.0f;
  float perceptual_contact_field = 0.0f;
  float perceptual_light_history = 0.0f;
  float perceptual_acoustic_occlusion = 0.0f;
  float perceptual_ecology_pressure = 0.0f;
  float perceptual_threat_gradient = 0.0f;
  float perceptual_traversal_pressure = 0.0f;
  float perceptual_semantic_lod = 0.0f;
  float perceptual_decision_impact = 0.0f;
  float perceptual_player_readable_cause = 0.0f;
  float perceptual_ai_cover_value = 0.0f;
  float perceptual_neural_irradiance_confidence = 0.0f;
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
  std::uint32_t graph_pass_id = 0;
  std::uint32_t resource_usage_flags = 0;
  std::uint32_t upload_range_index = 0;
  std::uint32_t diagnostic_id = 0;
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

struct RuntimeFramePlanBuffers {
  RuntimeDrawInstance *instances = nullptr;
  std::size_t instance_capacity = 0;
  std::size_t instance_count = 0;
  RuntimeDrawGroup *groups = nullptr;
  std::size_t group_capacity = 0;
  std::size_t group_count = 0;
  RuntimeDiagnostics *diagnostics = nullptr;
};

static_assert(std::is_standard_layout_v<RuntimeVec3>);
static_assert(std::is_standard_layout_v<RuntimeRenderObject>);
static_assert(std::is_standard_layout_v<RuntimeCamera>);
static_assert(std::is_standard_layout_v<RuntimeLineOfSightFade>);
static_assert(std::is_standard_layout_v<RuntimePlanOptions>);
static_assert(std::is_standard_layout_v<RuntimeDrawInstance>);
static_assert(std::is_standard_layout_v<RuntimeDrawGroup>);
static_assert(std::is_standard_layout_v<RuntimeDiagnostics>);
static_assert(std::is_standard_layout_v<RuntimeFramePlanBuffers>);

static_assert(sizeof(RuntimeVec3) == 12u);
static_assert(alignof(RuntimeVec3) == 4u);
static_assert(sizeof(RuntimeCamera) == 64u);
static_assert(sizeof(RuntimeLineOfSightFade) == 48u);
static_assert(sizeof(RuntimePlanOptions) == 48u);
static_assert(sizeof(RuntimeDrawInstance) == 16u);

static_assert(offsetof(RuntimeRenderObject, entity_id) == 0u);
static_assert(offsetof(RuntimeRenderObject, object_index) == 8u);
static_assert(offsetof(RuntimeRenderObject, mesh_key) == 16u);
static_assert(offsetof(RuntimeRenderObject, material_key) == 24u);
static_assert(offsetof(RuntimeRenderObject, render_queue) == 32u);
static_assert(offsetof(RuntimeRenderObject, position) == 44u);
static_assert(offsetof(RuntimeRenderObject, bounds_center) == 68u);
static_assert(offsetof(RuntimeRenderObject, dynamic_mesh_generation) == 104u);
static_assert(offsetof(RuntimeRenderObject, perceptual_primitive_hash) == 112u);
static_assert(offsetof(RuntimeRenderObject, perceptual_sound_surface_class_hash) == 120u);
static_assert(offsetof(RuntimeRenderObject, perceptual_neural_irradiance_hash) == 128u);
static_assert(offsetof(RuntimeRenderObject, perceptual_neural_irradiance) == 136u);
static_assert(offsetof(RuntimeRenderObject, perceptual_material_memory) == 148u);
static_assert(sizeof(RuntimeRenderObject) == 200u);

static_assert(sizeof(std::size_t) == 8u,
              "The Rust render planner ABI currently uses usize and is validated for 64-bit "
              "desktop targets.");
static_assert(sizeof(RuntimeDrawGroup) == 56u);
static_assert(sizeof(RuntimeDiagnostics) == 88u);
static_assert(sizeof(RuntimeFramePlanBuffers) == 56u);

extern "C" std::uint32_t
aster_runtime_build_frame_plan_v3(const RuntimeRenderObject *objects, std::size_t object_count,
                                  const RuntimeCamera *camera,
                                  const RuntimePlanOptions *options,
                                  RuntimeFramePlanBuffers *buffers);

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

void appendKeyString(std::uint64_t &hash, const std::string &value) {
  hash = fnvAppend(hash, value.data(), value.size());
}

void appendDepthPolicy(std::uint64_t &hash, const RenderDepthPolicy policy) {
  appendKey(hash, policy.layer);
  appendKey(hash, policy.constant_bias);
  appendKey(hash, policy.slope_bias);
  appendKey(hash, policy.normal_offset);
}

Vec3 cameraForward(const OrbitCamera &camera) {
  return normalizeOr(camera.target - camera.position(), {0.0f, 0.0f, -1.0f});
}

Vec3 cameraRight(const Vec3 forward) {
  return normalizeOr(cross(forward, {0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
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

struct WorldPerceptualPrimitiveProjection {
  std::uint64_t primitive_hash = 0u;
  std::uint64_t sound_surface_class_hash = 0u;
  std::uint64_t neural_irradiance_hash = 0u;
  Vec3 neural_irradiance{};
  float material_memory = 0.0f;
  float interaction_residue = 0.0f;
  float contact_field = 0.0f;
  float light_history = 0.0f;
  float acoustic_occlusion = 0.0f;
  float ecology_pressure = 0.0f;
  float threat_gradient = 0.0f;
  float traversal_pressure = 0.0f;
  float semantic_lod = 0.0f;
  float decision_impact = 0.0f;
  float player_readable_cause = 0.0f;
  float ai_cover_value = 0.0f;
  float neural_irradiance_confidence = 0.0f;
};

WorldPerceptualPrimitiveProjection
projectWorldPerceptualPrimitive(const WorldPerceptualPrimitive &primitive) {
  const WorldPerceptualSignals &signals = primitive.signals;
  return {.primitive_hash = primitive.truth_hash,
          .sound_surface_class_hash = primitive.sound_surface_class_hash,
          .neural_irradiance_hash = primitive.neural_irradiance_hash,
          .neural_irradiance = primitive.neural_irradiance,
          .material_memory = std::clamp(signals.material_memory, 0.0f, 1.0f),
          .interaction_residue = std::clamp(signals.interaction_residue, 0.0f, 1.0f),
          .contact_field = std::clamp(signals.contact_field, 0.0f, 1.0f),
          .light_history = std::clamp(signals.light_history, 0.0f, 1.0f),
          .acoustic_occlusion = std::clamp(signals.acoustic_occlusion, 0.0f, 1.0f),
          .ecology_pressure = std::clamp(signals.ecology_pressure, 0.0f, 1.0f),
          .threat_gradient = std::clamp(signals.threat_gradient, 0.0f, 1.0f),
          .traversal_pressure = std::clamp(signals.traversal_pressure, 0.0f, 1.0f),
          .semantic_lod = std::clamp(signals.semantic_lod, 0.0f, 1.0f),
          .decision_impact = std::clamp(signals.decision_impact, 0.0f, 1.0f),
          .player_readable_cause = std::clamp(signals.player_readable_cause, 0.0f, 1.0f),
          .ai_cover_value = std::clamp(primitive.ai_cover_value, 0.0f, 1.0f),
          .neural_irradiance_confidence =
              std::clamp(primitive.neural_irradiance_confidence, 0.0f, 1.0f)};
}

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
  std::uint64_t hash = object.material.compiled_permutation_key != 0u
                           ? object.material.compiled_permutation_key
                           : 1469598103934665603ull;
  const MaterialRenderQueue queue = classifyMaterialRenderQueue(object.material);
  const bool writes_depth = materialWritesDepth(object.material);
  const bool double_sided = isDoubleSidedMaterial(object.material);
  const bool contact_shadow =
      resolveMaterialSurfaceProfile(object.material) == MaterialSurfaceProfile::ContactShadow;
  if (object.material.compiled_permutation_key == 0u) {
    appendKey(hash, materialPermutationKey(object.material));
  }
  appendKey(hash, queue);
  appendKey(hash, writes_depth);
  appendKey(hash, double_sided);
  appendDepthPolicy(hash, object.material.depth_policy);
  appendKey(hash, object.material.cull_mode);
  appendKey(hash, contact_shadow);
  appendKey(hash, object.material.shader_variant_key);
  appendKeyString(hash, object.material.asset_id);
  appendKeyString(hash, object.material_asset_id);
  return {hash};
}

RenderSignatureKey renderSignatureKeyFor(const RenderMeshId mesh, const RenderMaterialKey material,
                                         const MaterialRenderQueue queue,
                                         const RenderDepthPolicy depth_policy,
                                         const FrameRenderPass pass) {
  std::uint64_t hash = 1469598103934665603ull;
  appendKey(hash, mesh.value);
  appendKey(hash, material.value);
  appendKey(hash, queue);
  appendDepthPolicy(hash, depth_policy);
  appendKey(hash, pass);
  return {hash};
}

RenderBounds renderBoundsForObject(const RenderObject &object) {
  if (object.custom_mesh != nullptr &&
      (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty())) {
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
  custom_bounds_cache_.clear();

  const auto cached_bounds = [this](const RenderObject &object) {
    if (object.custom_mesh == nullptr) {
      const LocalBounds bounds = primitiveLocalBounds(object.primitive);
      return CachedLocalBounds{bounds.min, bounds.max, primitiveLocalRadius(object.primitive)};
    }
    if (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty()) {
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

  ir_.objects.clear();
  ir_.objects.reserve(scene.objects().size());
  std::uint64_t ir_hash = 1469598103934665603ull;
  for (std::size_t index = 0; index < scene.objects().size(); ++index) {
    const RenderObject &object = scene.objects()[index];
    const bool plane_primitive = object.custom_mesh == nullptr && object.primitive == MeshPrimitive::Plane;
    const bool fade_eligible =
        object.camera_occlusion_fade && !plane_primitive && allowsCameraOcclusionFade(object.material);
    const RenderMeshId mesh = renderMeshIdForObject(object);
    const RenderMaterialKey material = renderMaterialKeyForObject(object);
    const MaterialRenderQueue queue = classifyMaterialRenderQueue(object.material);
    const RenderDepthPolicy depth_policy = object.material.depth_policy;
    const WorldPerceptualPrimitiveProjection perceptual =
        projectWorldPerceptualPrimitive(object.perceptual_primitive);
    const RenderObjectPacket packet{
        .entity = {static_cast<std::uint64_t>(index) + 1u},
        .object_index = index,
        .mesh = mesh,
        .material = material,
        .draw_signature =
            renderSignatureKeyFor(mesh, material, queue, depth_policy, FrameRenderPass::Opaque),
        .render_queue = queue,
        .depth_policy = depth_policy,
        .flags = fade_eligible ? kRuntimeFlagFadeEligible : 0u,
        .visibility_class = object.visibility_hint.visibility_class,
        .position = object.transform.position,
        .visibility_cell = object.visibility_hint.cell,
        .bounds = world_bounds(object),
        .opacity = object.material.opacity,
        .lod_max_distance = std::max(object.lod.max_distance, 0.0f),
        .lod_min_projected_radius = std::max(object.lod.min_projected_radius, 0.0f),
        .portal_depth = object.visibility_hint.portal_depth,
        .dynamic_mesh_generation = object.dynamic_mesh.valid() ? object.dynamic_mesh.generation
                                                               : 0u,
        .perceptual_primitive_hash = perceptual.primitive_hash,
        .perceptual_sound_surface_class_hash = perceptual.sound_surface_class_hash,
        .perceptual_neural_irradiance_hash = perceptual.neural_irradiance_hash,
        .perceptual_neural_irradiance = perceptual.neural_irradiance,
        .perceptual_material_memory = perceptual.material_memory,
        .perceptual_interaction_residue = perceptual.interaction_residue,
        .perceptual_contact_field = perceptual.contact_field,
        .perceptual_light_history = perceptual.light_history,
        .perceptual_acoustic_occlusion = perceptual.acoustic_occlusion,
        .perceptual_ecology_pressure = perceptual.ecology_pressure,
        .perceptual_threat_gradient = perceptual.threat_gradient,
        .perceptual_traversal_pressure = perceptual.traversal_pressure,
        .perceptual_semantic_lod = perceptual.semantic_lod,
        .perceptual_decision_impact = perceptual.decision_impact,
        .perceptual_player_readable_cause = perceptual.player_readable_cause,
        .perceptual_ai_cover_value = perceptual.ai_cover_value,
        .perceptual_neural_irradiance_confidence =
            perceptual.neural_irradiance_confidence};
    appendKey(ir_hash, packet.entity.value);
    appendKey(ir_hash, packet.object_index);
    appendKey(ir_hash, packet.mesh.value);
    appendKey(ir_hash, packet.material.value);
    appendKey(ir_hash, packet.render_queue);
    appendDepthPolicy(ir_hash, packet.depth_policy);
    appendKey(ir_hash, packet.flags);
    appendKey(ir_hash, packet.visibility_class);
    appendKey(ir_hash, packet.position.x);
    appendKey(ir_hash, packet.position.y);
    appendKey(ir_hash, packet.position.z);
    appendKey(ir_hash, packet.bounds.center.x);
    appendKey(ir_hash, packet.bounds.center.y);
    appendKey(ir_hash, packet.bounds.center.z);
    appendKey(ir_hash, packet.bounds.radius);
    appendKey(ir_hash, packet.opacity);
    appendKey(ir_hash, packet.dynamic_mesh_generation);
    appendKey(ir_hash, packet.perceptual_primitive_hash);
    appendKey(ir_hash, packet.perceptual_sound_surface_class_hash);
    appendKey(ir_hash, packet.perceptual_neural_irradiance_hash);
    appendKey(ir_hash, packet.perceptual_neural_irradiance.x);
    appendKey(ir_hash, packet.perceptual_neural_irradiance.y);
    appendKey(ir_hash, packet.perceptual_neural_irradiance.z);
    appendKey(ir_hash, packet.perceptual_material_memory);
    appendKey(ir_hash, packet.perceptual_interaction_residue);
    appendKey(ir_hash, packet.perceptual_contact_field);
    appendKey(ir_hash, packet.perceptual_light_history);
    appendKey(ir_hash, packet.perceptual_acoustic_occlusion);
    appendKey(ir_hash, packet.perceptual_ecology_pressure);
    appendKey(ir_hash, packet.perceptual_threat_gradient);
    appendKey(ir_hash, packet.perceptual_traversal_pressure);
    appendKey(ir_hash, packet.perceptual_semantic_lod);
    appendKey(ir_hash, packet.perceptual_decision_impact);
    appendKey(ir_hash, packet.perceptual_player_readable_cause);
    appendKey(ir_hash, packet.perceptual_ai_cover_value);
    appendKey(ir_hash, packet.perceptual_neural_irradiance_confidence);
    ir_.objects.push_back(packet);
  }
  ir_.content_hash = ir_hash;
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
                                   object.dynamic_mesh_generation,
                               .perceptual_primitive_hash =
                                   object.perceptual_primitive_hash,
                               .perceptual_sound_surface_class_hash =
                                   object.perceptual_sound_surface_class_hash,
                               .perceptual_neural_irradiance_hash =
                                   object.perceptual_neural_irradiance_hash,
                               .perceptual_neural_irradiance =
                                   runtimeVec(object.perceptual_neural_irradiance),
                               .perceptual_material_memory =
                                   object.perceptual_material_memory,
                               .perceptual_interaction_residue =
                                   object.perceptual_interaction_residue,
                               .perceptual_contact_field =
                                   object.perceptual_contact_field,
                               .perceptual_light_history =
                                   object.perceptual_light_history,
                               .perceptual_acoustic_occlusion =
                                   object.perceptual_acoustic_occlusion,
                               .perceptual_ecology_pressure =
                                   object.perceptual_ecology_pressure,
                               .perceptual_threat_gradient =
                                   object.perceptual_threat_gradient,
                               .perceptual_traversal_pressure =
                                   object.perceptual_traversal_pressure,
                               .perceptual_semantic_lod =
                                   object.perceptual_semantic_lod,
                               .perceptual_decision_impact =
                                   object.perceptual_decision_impact,
                               .perceptual_player_readable_cause =
                                   object.perceptual_player_readable_cause,
                               .perceptual_ai_cover_value =
                                   object.perceptual_ai_cover_value,
                               .perceptual_neural_irradiance_confidence =
                                   object.perceptual_neural_irradiance_confidence});
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
  FrameRenderPlan plan;
  std::vector<RuntimeDrawInstance> runtime_instances(runtime_objects.size());
  std::vector<RuntimeDrawGroup> runtime_groups(runtime_objects.size());
  RuntimeDiagnostics diagnostics;
  RuntimeFramePlanBuffers buffers{
      .instances = runtime_instances.empty() ? nullptr : runtime_instances.data(),
      .instance_capacity = runtime_instances.size(),
      .instance_count = 0,
      .groups = runtime_groups.empty() ? nullptr : runtime_groups.data(),
      .group_capacity = runtime_groups.size(),
      .group_count = 0,
      .diagnostics = &diagnostics};
  const std::uint32_t plan_ok = aster_runtime_build_frame_plan_v3(
      runtime_objects.empty() ? nullptr : runtime_objects.data(), runtime_objects.size(),
      &runtime_camera, &options, &buffers);
  if (plan_ok == 0u) {
    throw std::runtime_error("Rust render planner failed.");
  }

  plan.instances.reserve(buffers.instance_count);
  for (std::size_t i = 0; i < buffers.instance_count; ++i) {
    const RuntimeDrawInstance &instance = runtime_instances[i];
    plan.instances.push_back({.object_index = instance.object_index,
                              .opacity = instance.opacity,
                              .sort_distance_sq = instance.sort_distance_sq,
                              .flags = instance.flags});
  }
  plan.groups.reserve(buffers.group_count);
  for (std::size_t i = 0; i < buffers.group_count; ++i) {
    const RuntimeDrawGroup &group = runtime_groups[i];
    const FrameRenderPass pass = passFromValue(group.pass);
    RenderDepthPolicy depth_policy{};
    if (group.first_instance < plan.instances.size()) {
      const FrameRenderInstance &first_instance = plan.instances[group.first_instance];
      if (first_instance.object_index < scene.objects().size()) {
        depth_policy = scene.objects()[first_instance.object_index].depth_policy;
      }
    }
    const RenderMeshId mesh{group.mesh_key};
    const RenderMaterialKey material{group.material_key};
    const MaterialRenderQueue queue = materialQueueFromValue(group.render_queue);
    const CanonicalDrawSignature signature{
        .key = renderSignatureKeyFor(mesh, material, queue, depth_policy, pass),
        .mesh = mesh,
        .material = material,
        .render_queue = queue,
        .depth_policy = depth_policy,
        .pass = pass};
    plan.groups.push_back({.signature = signature,
                           .mesh = mesh,
                           .material = material,
                           .render_queue = queue,
                           .pass = pass,
                           .graph_pass_id =
                               static_cast<std::uint32_t>(pass == FrameRenderPass::Opaque
                                                              ? RenderGraphPass::Opaque
                                                              : RenderGraphPass::Transparent),
                           .resource_usage_flags = group.resource_usage_flags,
                           .upload_range_index = group.upload_range_index,
                           .diagnostic_id = group.diagnostic_id,
                           .first_instance = group.first_instance,
                           .instance_count = group.instance_count});
  }
  plan.diagnostics = {.object_count = diagnostics.object_count,
                      .visible_objects = diagnostics.visible_objects,
                      .culled_objects = diagnostics.culled_objects,
                      .opaque_groups = diagnostics.opaque_groups,
                      .transparent_groups = diagnostics.transparent_groups,
                      .instance_groups = diagnostics.instance_groups,
                      .planned_instances = diagnostics.planned_instances,
                      .lod_culled_objects = diagnostics.lod_culled_objects,
                      .visibility_hint_objects = diagnostics.visibility_hint_objects,
                      .dynamic_mesh_objects = diagnostics.dynamic_mesh_objects,
                      .rust_plan_seconds = diagnostics.rust_plan_seconds};
  plan.source_ir_hash = scene.ir().content_hash;
  return plan;
}

} // namespace aster
