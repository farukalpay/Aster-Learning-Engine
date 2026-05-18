// Author: Faruk Alpay
// Do not remove this notice.

#include "render_backend_common.hpp"

#include <algorithm>
#include <cmath>

namespace aster::render_backend {
namespace {

float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

float localMaxAbs(const float min_value, const float max_value) {
  return std::max(std::abs(min_value), std::abs(max_value));
}

float objectFootY(const RenderObject &object, const LocalBounds &bounds) {
  return object.transform.position.y + bounds.min.y * object.transform.scale.y;
}

Vec2 objectContactHalfExtents(const RenderObject &object, const LocalBounds &bounds) {
  return {localMaxAbs(bounds.min.x, bounds.max.x) * std::abs(object.transform.scale.x),
          localMaxAbs(bounds.min.z, bounds.max.z) * std::abs(object.transform.scale.z)};
}

} // namespace

bool containsViewerCullVolume(const ViewerCullVolume &volume, const Vec3 point) {
  if (!volume.enabled || volume.half_extents.x <= 0.0f || volume.half_extents.y <= 0.0f ||
      volume.half_extents.z <= 0.0f) {
    return false;
  }
  const Vec3 delta = point - volume.center;
  return std::abs(delta.x) <= volume.half_extents.x &&
         std::abs(delta.y) <= volume.half_extents.y &&
         std::abs(delta.z) <= volume.half_extents.z;
}

FaceCullMode objectCullMode(const RenderObject &object, const Vec3 camera_position,
                            const bool pipeline_back_face_culling) {
  if (!pipeline_back_face_culling || isDoubleSidedMaterial(object.material)) {
    return FaceCullMode::None;
  }
  if (containsViewerCullVolume(object.viewer_cull_volume, camera_position)) {
    return object.viewer_cull_volume.inside;
  }
  if (object.viewer_cull_volume.enabled) {
    return object.viewer_cull_volume.outside;
  }
  return object.material.cull_mode;
}

bool isContactShadowUtility(const RenderObject &object) {
  return resolveMaterialSurfaceProfile(object.material) == MaterialSurfaceProfile::ContactShadow;
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

LocalBounds objectLocalBounds(const RenderObject &object) {
  return object.custom_mesh != nullptr ? customMeshLocalBounds(*object.custom_mesh)
                                       : primitiveLocalBounds(object.primitive);
}

bool canCastContactShadow(const RenderObject &object, const GroundingSettings &grounding) {
  const bool requested =
      object.casts_contact_shadow || (grounding.auto_contact_shadows && object.auto_contact_shadow);
  if (!grounding.enabled || !grounding.contact_shadows || !requested) {
    return false;
  }
  if (object.material.render_role == MaterialRenderRole::SupportSurface ||
      classifyMaterialRenderQueue(object.material) != MaterialRenderQueue::Opaque ||
      isContactShadowUtility(object) ||
      (object.custom_mesh == nullptr && object.primitive == MeshPrimitive::Plane)) {
    return false;
  }

  const LocalBounds bounds = objectLocalBounds(object);
  const Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  if (raw_radius < min_radius || raw_radius > max_radius) {
    return false;
  }
  return std::abs(objectFootY(object, bounds) - grounding.reference_y) <=
         std::max(grounding.contact_shadow_receiver_height, 0.0f);
}

RenderObject contactShadowObjectFor(const RenderObject &object,
                                    const GroundingSettings &grounding) {
  const LocalBounds bounds = objectLocalBounds(object);
  const Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  const float radius = std::clamp(raw_radius, min_radius, max_radius);
  const float footprint_x = std::clamp(half_extents.x * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float footprint_z = std::clamp(half_extents.y * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float foot_y = objectFootY(object, bounds);
  const float receiver_delta = std::abs(foot_y - grounding.reference_y);
  const float fade = 1.0f - smoothstep(grounding.contact_shadow_receiver_height * 0.72f,
                                       grounding.contact_shadow_receiver_height, receiver_delta);

  RenderObject shadow;
  shadow.name = "Procedural contact shadow";
  shadow.primitive = MeshPrimitive::Plane;
  shadow.transform.position = {object.transform.position.x,
                               foot_y + grounding.contact_shadow_receiver_bias,
                               object.transform.position.z};
  shadow.transform.rotation =
      quatFromEulerXyz({0.0f, eulerXyz(object.transform.rotation).y, 0.0f});
  shadow.transform.scale = {footprint_x, 1.0f, footprint_z};
  shadow.material.base_color = {0.0f, 0.0f, 0.0f};
  shadow.material.roughness = 1.0f;
  shadow.material.opacity = std::clamp(
      grounding.contact_shadow_strength * object.contact_shadow_strength * fade, 0.0f, 0.42f);
  shadow.material.surface_profile = MaterialSurfaceProfile::ContactShadow;
  shadow.material.surface_pattern = SurfacePattern::ContactShadow;
  shadow.material.alpha_mode = MaterialAlphaMode::Blend;
  shadow.material.depth_write = MaterialDepthWrite::Disabled;
  shadow.material.camera_occlusion = CameraOcclusionPolicy::Solid;
  shadow.material.double_sided = true;
  shadow.casts_contact_shadow = false;
  shadow.camera_occlusion_fade = false;
  return shadow;
}

const CpuMesh *meshForObject(const RenderObject &object, const PreparedRenderMeshes &meshes) {
  if (isContactShadowUtility(object) && object.custom_mesh == nullptr) {
    return meshes.contact_shadow_plane;
  }
  if (object.custom_mesh != nullptr) {
    if (object.dynamic_mesh.valid()) {
      if (meshes.custom_mesh_resources == nullptr) {
        return nullptr;
      }
      const auto it = meshes.custom_mesh_resources->find(object.dynamic_mesh);
      return it == meshes.custom_mesh_resources->end() ? nullptr : &it->second;
    }
    if (meshes.custom_meshes == nullptr) {
      return nullptr;
    }
    const auto it = meshes.custom_meshes->find(object.custom_mesh.get());
    return it == meshes.custom_meshes->end() ? nullptr : &it->second;
  }
  switch (object.primitive) {
  case MeshPrimitive::Box:
    return meshes.box;
  case MeshPrimitive::Sphere:
    return meshes.sphere;
  case MeshPrimitive::Plane:
    return meshes.plane;
  case MeshPrimitive::Rock:
    return meshes.rock;
  case MeshPrimitive::Crystal:
    return meshes.crystal;
  case MeshPrimitive::RuinBlock:
    return meshes.ruin_block;
  case MeshPrimitive::Pillar:
    return meshes.pillar;
  }
  return meshes.sphere;
}

} // namespace aster::render_backend
