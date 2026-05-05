// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/generated_scenery.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace aster {
namespace {

Vec3 scaleVec(const Vec3 a, const Vec3 b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 absVec(const Vec3 value) {
  return {std::abs(value.x), std::abs(value.y), std::abs(value.z)};
}

Vec3 rotateX(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

Vec3 rotateY(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

Vec3 rotateZ(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

Vec3 rotateEuler(const Vec3 value, const Vec3 rotation) {
  return rotateZ(rotateY(rotateX(value, rotation.x), rotation.y), rotation.z);
}

Transform composeTransform(const Transform &root, const Transform &local) {
  Transform out;
  out.position = root.position + rotateEuler(scaleVec(local.position, root.scale), root.rotation);
  out.rotation = root.rotation + local.rotation;
  out.scale = scaleVec(root.scale, local.scale);
  return out;
}

std::string scopedName(const std::string &root, const std::string &part) {
  if (root.empty()) {
    return part;
  }
  if (part.empty()) {
    return root;
  }
  return root + "/" + part;
}

void applyCommonPartState(RenderObject &object, const bool camera_occlusion_fade,
                          const bool auto_contact_shadow, const bool casts_contact_shadow,
                          const float contact_shadow_strength,
                          const float contact_shadow_radius_scale,
                          const ViewerCullVolume &viewer_cull_volume) {
  object.camera_occlusion_fade =
      camera_occlusion_fade && allowsCameraOcclusionFade(object.material);
  object.auto_contact_shadow = auto_contact_shadow;
  object.casts_contact_shadow = casts_contact_shadow;
  object.contact_shadow_strength = contact_shadow_strength;
  object.contact_shadow_radius_scale = contact_shadow_radius_scale;
  object.viewer_cull_volume = viewer_cull_volume;
}

GeneratedSceneryCollisionProxy composeCollisionProxy(
    const std::string &root_name, const Transform &root,
    const GeneratedSceneryCollisionProxy &proxy) {
  GeneratedSceneryCollisionProxy out;
  out.name = scopedName(root_name, proxy.name);
  out.center = root.position + rotateEuler(scaleVec(proxy.center, root.scale), root.rotation);
  out.half_extents = scaleVec(proxy.half_extents, absVec(root.scale));
  return out;
}

GeneratedScenerySocket composeSocket(const std::string &root_name, const Transform &root,
                                     const GeneratedScenerySocket &socket) {
  GeneratedScenerySocket out;
  out.name = scopedName(root_name, socket.name);
  out.transform = composeTransform(root, socket.transform);
  out.radius = socket.radius * std::max({std::abs(root.scale.x), std::abs(root.scale.y),
                                         std::abs(root.scale.z)});
  return out;
}

} // namespace

GeneratedSceneryBundle assembleGeneratedScenery(const GeneratedSceneryAssemblySpec &spec) {
  GeneratedSceneryBundle bundle;
  bundle.render_objects.reserve(spec.primitives.size() + spec.meshes.size());
  bundle.collision_proxies.reserve(spec.collision_proxies.size());
  bundle.sockets.reserve(spec.sockets.size());

  for (const GeneratedSceneryPrimitivePart &part : spec.primitives) {
    RenderObject object;
    object.name = scopedName(spec.name, part.name);
    object.primitive = part.primitive;
    object.transform = composeTransform(spec.root, part.transform);
    object.material = part.material;
    applyCommonPartState(object, part.camera_occlusion_fade, part.auto_contact_shadow,
                         part.casts_contact_shadow, part.contact_shadow_strength,
                         part.contact_shadow_radius_scale, part.viewer_cull_volume);
    bundle.render_objects.push_back(std::move(object));
  }

  for (const GeneratedSceneryMeshPart &part : spec.meshes) {
    RenderObject object;
    object.name = scopedName(spec.name, part.name);
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = part.mesh;
    object.transform = composeTransform(spec.root, part.transform);
    object.material = part.material;
    applyCommonPartState(object, part.camera_occlusion_fade, part.auto_contact_shadow,
                         part.casts_contact_shadow, part.contact_shadow_strength,
                         part.contact_shadow_radius_scale, part.viewer_cull_volume);
    bundle.render_objects.push_back(std::move(object));
  }

  for (const GeneratedSceneryCollisionProxy &proxy : spec.collision_proxies) {
    bundle.collision_proxies.push_back(composeCollisionProxy(spec.name, spec.root, proxy));
  }
  for (const GeneratedScenerySocket &socket : spec.sockets) {
    bundle.sockets.push_back(composeSocket(spec.name, spec.root, socket));
  }
  return bundle;
}

void appendGeneratedScenery(Scene &scene, const GeneratedSceneryBundle &bundle,
                            std::vector<std::size_t> *object_indices) {
  std::vector<RenderObject> &objects = scene.objects();
  if (object_indices != nullptr) {
    object_indices->reserve(object_indices->size() + bundle.render_objects.size());
  }
  for (const RenderObject &object : bundle.render_objects) {
    objects.push_back(object);
    if (object_indices != nullptr) {
      object_indices->push_back(objects.size() - 1u);
    }
  }
}

const GeneratedScenerySocket *findGeneratedScenerySocket(const GeneratedSceneryBundle &bundle,
                                                         const std::string &name) {
  for (const GeneratedScenerySocket &socket : bundle.sockets) {
    if (socket.name == name) {
      return &socket;
    }
  }
  return nullptr;
}

} // namespace aster
