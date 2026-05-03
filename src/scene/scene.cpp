// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/scene.hpp"

#include "aster/geometry/architectural_mesh.hpp"

#include <memory>

namespace aster {
namespace {

Material material(const Vec3 base, const Vec3 emission, const float roughness, const float metallic,
                  const float glow, const float detail, const float detail_scale,
                  const float edge_wear, const float ambient_occlusion = 1.0f,
                  const SurfacePattern pattern = SurfacePattern::None,
                  const Vec2 pattern_scale = {1.0f, 1.0f}, const float pattern_depth = 0.0f,
                  const float pattern_contrast = 0.0f, const float pattern_mortar = 0.08f) {
  return {base,          emission,      roughness,        metallic,          glow,
          detail,        detail_scale,  edge_wear,        ambient_occlusion, pattern,
          pattern_scale, pattern_depth, pattern_contrast, pattern_mortar};
}

std::shared_ptr<const CpuMesh> showcaseGatehouseMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeGatehouseMesh({.wall_half_width = 1.68f,
                                                         .wall_height = 2.24f,
                                                         .tower_half_width = 0.44f,
                                                         .tower_height = 2.92f,
                                                         .depth = 0.72f,
                                                         .door_half_width = 0.48f,
                                                         .door_height = 1.45f,
                                                         .parapet_blocks = 5}));
  return mesh;
}

std::shared_ptr<const CpuMesh> showcaseHouseMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeCourtyardHouseMesh(
          {.half_width = 0.84f, .body_height = 1.48f, .depth = 0.82f, .roof_pitch = 0.40f}));
  return mesh;
}

} // namespace

Material makeSupportSurfaceMaterial(Material material) {
  material.opacity = 1.0f;
  material.double_sided = true;
  material.render_role = MaterialRenderRole::SupportSurface;
  return material;
}

bool isMaterialTranslucent(const Material &material) {
  if (material.render_role == MaterialRenderRole::SupportSurface) {
    return false;
  }
  return material.opacity < 0.999f;
}

bool isDoubleSidedMaterial(const Material &material) {
  return material.double_sided || material.render_role == MaterialRenderRole::SupportSurface;
}

Scene Scene::makeShowcase() {
  Scene scene;

  const Material plaza_material =
      material({0.36f, 0.34f, 0.30f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.86f, 4.5f, 0.26f,
               0.74f, SurfacePattern::CourseCells, {1.38f, 1.38f}, 0.052f, 0.58f, 0.052f);
  const Material brick_material =
      material({0.48f, 0.31f, 0.24f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.90f, 7.4f, 0.40f,
               0.62f, SurfacePattern::CourseCells, {5.4f, 8.2f}, 0.098f, 0.84f, 0.055f);
  const Material dark_stone =
      material({0.24f, 0.24f, 0.24f}, {0.0f, 0.0f, 0.0f}, 0.92f, 0.0f, 0.0f, 0.68f, 6.0f, 0.24f,
               0.66f, SurfacePattern::CourseCells, {4.0f, 6.2f}, 0.060f, 0.54f, 0.070f);
  const Material warm_light = material({0.80f, 0.58f, 0.34f}, {1.0f, 0.66f, 0.30f}, 0.42f, 0.0f,
                                       0.34f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material iron_material = material({0.21f, 0.21f, 0.20f}, {0.0f, 0.0f, 0.0f}, 0.58f, 0.62f,
                                          0.0f, 0.28f, 8.0f, 0.14f, 0.88f);

  RenderObject floor;
  floor.name = "Masonry plaza";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.0f, 1.0f, 1.0f};
  floor.material = plaza_material;
  scene.objects_.push_back(floor);

  RenderObject gate;
  gate.name = "Brick gatehouse mesh";
  gate.primitive = MeshPrimitive::Box;
  gate.custom_mesh = showcaseGatehouseMesh();
  gate.transform.position = {0.0f, 0.0f, -0.70f};
  gate.transform.scale = {1.04f, 1.04f, 1.0f};
  gate.material = brick_material;
  scene.objects_.push_back(gate);

  RenderObject door;
  door.name = "Recessed iron gate";
  door.primitive = MeshPrimitive::Box;
  door.transform.position = {0.0f, 0.66f, -0.32f};
  door.transform.scale = {0.72f, 1.10f, 0.07f};
  door.material = iron_material;
  scene.objects_.push_back(door);

  for (const float x : {-1.86f, 1.86f}) {
    RenderObject window;
    window.name = "Warm tower window";
    window.primitive = MeshPrimitive::Box;
    window.transform.position = {x, 1.48f, -0.31f};
    window.transform.scale = {0.14f, 0.32f, 0.06f};
    window.material = warm_light;
    scene.objects_.push_back(window);
  }

  for (const float side : {-1.0f, 1.0f}) {
    RenderObject house;
    house.name = "Courtyard house mesh";
    house.primitive = MeshPrimitive::Box;
    house.custom_mesh = showcaseHouseMesh();
    house.transform.position = {side * 2.45f, 0.0f, 0.20f};
    house.transform.rotation = {0.0f, -side * 0.20f, 0.0f};
    house.transform.scale = {0.92f, 0.92f, 0.92f};
    house.material = side < 0.0f ? dark_stone : brick_material;
    scene.objects_.push_back(house);
  }

  RenderObject lamp_post;
  lamp_post.name = "Rectangular lamp post";
  lamp_post.primitive = MeshPrimitive::Box;
  lamp_post.transform.position = {-1.45f, 0.64f, 1.18f};
  lamp_post.transform.scale = {0.09f, 1.28f, 0.09f};
  lamp_post.material = iron_material;
  scene.objects_.push_back(lamp_post);

  RenderObject lamp_core;
  lamp_core.name = "Amber lamp core";
  lamp_core.primitive = MeshPrimitive::Crystal;
  lamp_core.transform.position = {-1.45f, 1.38f, 1.18f};
  lamp_core.transform.scale = {0.13f, 0.30f, 0.13f};
  lamp_core.material = warm_light;
  lamp_core.spin_rate = 0.24f;
  scene.objects_.push_back(lamp_core);

  return scene;
}

} // namespace aster
