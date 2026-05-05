// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/scene/scene.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace aster {

struct GeneratedSceneryCollisionProxy {
  std::string name;
  Vec3 center{};
  Vec3 half_extents{};
};

struct GeneratedScenerySocket {
  std::string name;
  Transform transform{};
  float radius = 0.0f;
};

struct GeneratedSceneryPrimitivePart {
  std::string name;
  MeshPrimitive primitive = MeshPrimitive::Box;
  Transform transform{};
  Material material{};
  bool camera_occlusion_fade = true;
  bool auto_contact_shadow = true;
  bool casts_contact_shadow = false;
  float contact_shadow_strength = 1.0f;
  float contact_shadow_radius_scale = 1.0f;
  ViewerCullVolume viewer_cull_volume{};
};

struct GeneratedSceneryMeshPart {
  std::string name;
  std::shared_ptr<const CpuMesh> mesh{};
  Transform transform{};
  Material material{};
  bool camera_occlusion_fade = true;
  bool auto_contact_shadow = true;
  bool casts_contact_shadow = false;
  float contact_shadow_strength = 1.0f;
  float contact_shadow_radius_scale = 1.0f;
  ViewerCullVolume viewer_cull_volume{};
};

struct GeneratedSceneryAssemblySpec {
  std::string name;
  Transform root{};
  std::vector<GeneratedSceneryPrimitivePart> primitives;
  std::vector<GeneratedSceneryMeshPart> meshes;
  std::vector<GeneratedSceneryCollisionProxy> collision_proxies;
  std::vector<GeneratedScenerySocket> sockets;
};

struct GeneratedSceneryBundle {
  std::vector<RenderObject> render_objects;
  std::vector<GeneratedSceneryCollisionProxy> collision_proxies;
  std::vector<GeneratedScenerySocket> sockets;
};

[[nodiscard]] GeneratedSceneryBundle assembleGeneratedScenery(
    const GeneratedSceneryAssemblySpec &spec);
void appendGeneratedScenery(Scene &scene, const GeneratedSceneryBundle &bundle,
                            std::vector<std::size_t> *object_indices = nullptr);
[[nodiscard]] const GeneratedScenerySocket *findGeneratedScenerySocket(
    const GeneratedSceneryBundle &bundle, const std::string &name);

} // namespace aster
