// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/samples/showcase_scenes.hpp"

#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/cave_web_mesh.hpp"
#include "aster/geometry/fracture_mesh.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/geometry/tube_mesh.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <utility>

namespace aster {
namespace {

Material material(const Vec3 base, const Vec3 emission, const float roughness, const float metallic,
                  const float glow, const float detail, const float detail_scale,
                  const float edge_wear, const float ambient_occlusion = 1.0f,
                  const SurfacePattern pattern = SurfacePattern::None,
                  const Vec2 pattern_scale = {1.0f, 1.0f}, const float pattern_depth = 0.0f,
                  const float pattern_contrast = 0.0f, const float pattern_mortar = 0.08f,
                  const ProceduralSurfaceLayer procedural = {}) {
  return makeMaterial({.base_color = base,
                       .emission_color = emission,
                       .roughness = roughness,
                       .metallic = metallic,
                       .emission_strength = glow,
                       .detail_strength = detail,
                       .detail_scale = detail_scale,
                       .edge_wear = edge_wear,
                       .ambient_occlusion = ambient_occlusion,
                       .surface_pattern = pattern,
                       .pattern_scale = pattern_scale,
                       .pattern_depth = pattern_depth,
                       .pattern_contrast = pattern_contrast,
                       .pattern_mortar = pattern_mortar,
                       .procedural = procedural});
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

std::shared_ptr<const CpuMesh> pipeSectionMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeTubeMesh({.length = 5.2f,
                                                    .outer_radius = 0.54f,
                                                    .wall_thickness = 0.075f,
                                                    .radial_segments = 96,
                                                    .length_segments = 24}));
  return mesh;
}

std::shared_ptr<const CpuMesh> weldBeadMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeCircumferentialBeadMesh({.pipe_radius = 0.54f,
                                                                   .bead_radius = 0.045f,
                                                                   .axial_width = 0.18f,
                                                                   .radial_segments = 96,
                                                                   .bead_segments = 14}));
  return mesh;
}

std::shared_ptr<const CpuMesh> labCableMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeCableMesh({.construction = CableConstruction::TwistedStrands,
                                                     .radial_segments = 24,
                                                     .length_segments = 28,
                                                     .strand_count = 7,
                                                     .radius = 0.22f,
                                                     .length = 2.6f,
                                                     .twist_turns = 3.0f}));
  return mesh;
}

std::shared_ptr<const CpuMesh> labTerrainMesh() {
  static const std::shared_ptr<const CpuMesh> mesh = [] {
    TerrainHeightField terrain = makeProceduralTerrain(
        {.grid_size = 25,
         .square_size = 0.18f,
         .central_flat_radius = 0.72f,
         .transition_width = 1.8f,
         .hill_height = 0.18f,
         .mountain_height = 0.46f});
    return std::make_shared<const CpuMesh>(
        makeTerrainMesh(terrain, {.alternating_diagonal_split = true,
                                  .clamp_edge_samples = true,
                                  .subdivisions_per_square = 1,
                                  .smooth_visual_surface = true}));
  }();
  return mesh;
}

std::shared_ptr<const CpuMesh> labCaveWebMesh() {
  static const std::shared_ptr<const CpuMesh> mesh =
      std::make_shared<const CpuMesh>(makeCaveWebMesh({.radius_x = 0.82f,
                                                       .radius_y = 0.60f,
                                                       .radial_strands = 16,
                                                       .ring_strands = 5,
                                                       .ring_segments = 80,
                                                       .strand_width = 0.020f,
                                                       .sag = 0.12f,
                                                       .irregularity = 0.08f,
                                                       .seed = 11u}));
  return mesh;
}

std::shared_ptr<const CpuMesh> labFracturedMesh() {
  static const std::shared_ptr<const CpuMesh> mesh = [] {
    const std::vector<VoronoiFractureShard> shards = buildImpactVoronoiFracture(
        {.volume = {.center = {}, .half_extents = {0.62f, 0.44f, 0.50f}},
         .impact_point = {-0.28f, 0.12f, 0.32f},
         .impact_normal = {0.22f, 0.80f, -0.18f},
         .seed = 19u,
         .shard_count = 7,
         .impact_seed_fraction = 0.60f});
    CpuMesh merged;
    for (const VoronoiFractureShard &shard : shards) {
      const std::uint32_t base = static_cast<std::uint32_t>(merged.vertices.size());
      merged.vertices.insert(merged.vertices.end(), shard.mesh.vertices.begin(),
                             shard.mesh.vertices.end());
      for (const std::uint32_t index : shard.mesh.indices) {
        merged.indices.push_back(base + index);
      }
    }
    return std::make_shared<const CpuMesh>(std::move(merged));
  }();
  return mesh;
}

} // namespace

Scene makeArchitectureShowcaseScene() {
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
  floor.material = plaza_material;
  scene.objects().push_back(floor);

  RenderObject gate;
  gate.name = "Brick gatehouse mesh";
  gate.primitive = MeshPrimitive::Box;
  gate.custom_mesh = showcaseGatehouseMesh();
  gate.transform.position = {0.0f, 0.0f, -0.70f};
  gate.transform.scale = {1.04f, 1.04f, 1.0f};
  gate.material = brick_material;
  scene.objects().push_back(gate);

  RenderObject door;
  door.name = "Recessed iron gate";
  door.primitive = MeshPrimitive::Box;
  door.transform.position = {0.0f, 0.66f, -0.32f};
  door.transform.scale = {0.72f, 1.10f, 0.07f};
  door.material = iron_material;
  scene.objects().push_back(door);

  for (const float x : {-1.86f, 1.86f}) {
    RenderObject window;
    window.name = "Warm tower window";
    window.primitive = MeshPrimitive::Box;
    window.transform.position = {x, 1.48f, -0.31f};
    window.transform.scale = {0.14f, 0.32f, 0.06f};
    window.material = warm_light;
    scene.objects().push_back(window);
  }

  for (const float side : {-1.0f, 1.0f}) {
    RenderObject house;
    house.name = "Courtyard house mesh";
    house.primitive = MeshPrimitive::Box;
    house.custom_mesh = showcaseHouseMesh();
    house.transform.position = {side * 2.45f, 0.0f, 0.20f};
    house.transform.rotation = quatFromEulerXyz({0.0f, -side * 0.20f, 0.0f});
    house.transform.scale = {0.92f, 0.92f, 0.92f};
    house.material = side < 0.0f ? dark_stone : brick_material;
    scene.objects().push_back(house);
  }

  RenderObject lamp_post;
  lamp_post.name = "Rectangular lamp post";
  lamp_post.primitive = MeshPrimitive::Box;
  lamp_post.transform.position = {-1.45f, 0.64f, 1.18f};
  lamp_post.transform.scale = {0.09f, 1.28f, 0.09f};
  lamp_post.material = iron_material;
  scene.objects().push_back(lamp_post);

  RenderObject lamp_core;
  lamp_core.name = "Amber lamp core";
  lamp_core.primitive = MeshPrimitive::Crystal;
  lamp_core.transform.position = {-1.45f, 1.38f, 1.18f};
  lamp_core.transform.scale = {0.13f, 0.30f, 0.13f};
  lamp_core.material = warm_light;
  lamp_core.spin_rate = 0.24f;
  scene.objects().push_back(lamp_core);

  return scene;
}

Scene makeIndustrialPipeScene() {
  Scene scene;

  const Material pipe_metal =
      material({0.18f, 0.17f, 0.16f}, {0.0f, 0.0f, 0.0f}, 0.78f, 0.82f, 0.0f, 0.92f, 9.6f, 0.28f,
               0.70f, SurfacePattern::WeatheredMetal, {3.6f, 11.5f}, 0.42f, 0.92f, 0.05f,
               {.macro_variation = 0.72f,
                .micro_normal_strength = 0.54f,
                .roughness_variation = 0.72f,
                .wetness = 0.04f,
                .height_shading = 0.40f});
  const Material weld_metal =
      material({0.36f, 0.32f, 0.27f}, {0.06f, 0.025f, 0.008f}, 0.62f, 0.88f, 0.0f, 0.96f, 18.0f,
               0.52f, 0.72f, SurfacePattern::WeldBead, {24.0f, 18.0f}, 0.36f, 0.95f, 0.035f,
               {.macro_variation = 0.42f,
                .micro_normal_strength = 0.48f,
                .roughness_variation = 0.48f,
                .wetness = 0.08f,
                .height_shading = 0.32f});
  const Material floor_material =
      material({0.075f, 0.080f, 0.082f}, {0.0f, 0.0f, 0.0f}, 0.86f, 0.0f, 0.0f, 0.25f, 3.0f, 0.10f,
               0.90f);

  RenderObject floor;
  floor.name = "neutral inspection plane";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.0f, 1.0f, 1.0f};
  floor.material = floor_material;
  scene.objects().push_back(floor);

  RenderObject pipe;
  pipe.name = "weathered hollow pipe section";
  pipe.primitive = MeshPrimitive::Box;
  pipe.custom_mesh = pipeSectionMesh();
  pipe.transform.position = {0.0f, 0.62f, 0.0f};
  pipe.transform.rotation = quatFromEulerXyz({0.0f, 0.0f, -0.045f});
  pipe.material = pipe_metal;
  pipe.casts_contact_shadow = true;
  pipe.contact_shadow_strength = 0.44f;
  pipe.contact_shadow_radius_scale = 1.35f;
  scene.objects().push_back(pipe);

  for (const float x : {-1.55f, 1.42f}) {
    RenderObject bead;
    bead.name = "circumferential weld bead";
    bead.primitive = MeshPrimitive::Box;
    bead.custom_mesh = weldBeadMesh();
    bead.transform.position = {x, 0.62f + x * -0.045f, 0.0f};
    bead.transform.rotation = pipe.transform.rotation;
    bead.material = weld_metal;
    bead.casts_contact_shadow = true;
    bead.contact_shadow_strength = 0.36f;
    bead.contact_shadow_radius_scale = 0.85f;
    scene.objects().push_back(bead);
  }

  return scene;
}

Scene makeMaterialLabShowcaseScene() {
  Scene scene;

  const Material floor_material =
      makeSupportSurfaceMaterial(material({0.16f, 0.17f, 0.16f}, {}, 0.92f, 0.0f, 0.0f, 0.28f,
                                          3.0f, 0.08f, 0.86f, SurfacePattern::CourseCells,
                                          {2.2f, 2.2f}, 0.05f, 0.30f));
  RenderObject floor;
  floor.name = "material lab inspection floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.05f, 1.0f, 1.05f};
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  const Material wet_rock =
      material({0.20f, 0.22f, 0.21f}, {}, 0.78f, 0.0f, 0.0f, 0.88f, 5.0f, 0.22f, 0.78f,
               SurfacePattern::CaveRock, {2.6f, 3.2f}, 0.26f, 0.60f, 0.06f,
               {.macro_variation = 0.56f,
                .micro_normal_strength = 0.50f,
                .roughness_variation = 0.36f,
                .wetness = 0.52f,
                .height_shading = 0.28f});
  const Material rusty_pipe =
      material({0.24f, 0.17f, 0.12f}, {0.02f, 0.008f, 0.0f}, 0.74f, 0.78f, 0.0f, 0.92f,
               10.0f, 0.36f, 0.70f, SurfacePattern::WeatheredMetal, {3.6f, 11.5f}, 0.42f,
               0.92f, 0.05f, {.macro_variation = 0.70f,
                               .micro_normal_strength = 0.54f,
                               .roughness_variation = 0.68f,
                               .wetness = 0.08f,
                               .height_shading = 0.36f});
  const Material brushed_metal =
      material({0.55f, 0.53f, 0.48f}, {}, 0.34f, 0.92f, 0.0f, 0.48f, 18.0f, 0.10f, 0.84f,
               SurfacePattern::FiberStrands, {14.0f, 2.0f}, 0.08f, 0.38f);
  Material glass =
      material({0.34f, 0.62f, 0.78f}, {0.02f, 0.04f, 0.06f}, 0.08f, 0.0f, 0.08f, 0.12f,
               2.0f, 0.0f, 1.0f);
  glass.opacity = 0.42f;
  glass.alpha_mode = MaterialAlphaMode::Blend;
  glass.depth_write = MaterialDepthWrite::Disabled;
  glass.double_sided = true;

  const Material materials[] = {wet_rock, rusty_pipe, brushed_metal, glass};
  const char *names[] = {"wet rock shader ball", "weathered pipe shader ball",
                         "brushed metal shader ball", "translucent glass shader ball"};
  for (std::size_t i = 0; i < 4u; ++i) {
    RenderObject object;
    object.name = names[i];
    object.primitive = i == 1u ? MeshPrimitive::Box : MeshPrimitive::Sphere;
    object.custom_mesh = i == 1u ? pipeSectionMesh() : nullptr;
    object.transform.position = {-2.55f + static_cast<float>(i) * 1.70f, 0.70f, 0.0f};
    object.transform.scale = {0.58f, 0.58f, 0.58f};
    object.material = materials[i];
    object.casts_contact_shadow = true;
    object.contact_shadow_strength = 0.56f;
    scene.objects().push_back(object);
  }

  return scene;
}

Scene makeMeshLabShowcaseScene() {
  Scene scene;
  const Material floor_material =
      makeSupportSurfaceMaterial(material({0.13f, 0.14f, 0.15f}, {}, 0.90f, 0.0f, 0.0f, 0.24f,
                                          2.8f, 0.08f, 0.88f));
  RenderObject floor;
  floor.name = "mesh lab floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  const Material terrain =
      material({0.28f, 0.24f, 0.18f}, {}, 0.88f, 0.0f, 0.0f, 0.62f, 5.4f, 0.18f, 0.82f,
               SurfacePattern::TerrainBlend, {2.2f, 2.8f}, 0.18f, 0.48f);
  const Material cable =
      material({0.045f, 0.050f, 0.052f}, {}, 0.66f, 0.36f, 0.0f, 0.84f, 12.0f, 0.08f, 0.88f,
               SurfacePattern::FiberStrands, {16.0f, 3.0f}, 0.08f, 0.52f);
  const Material fracture =
      material({0.38f, 0.33f, 0.28f}, {}, 0.82f, 0.0f, 0.0f, 0.68f, 4.5f, 0.34f, 0.78f,
               SurfacePattern::WeatheredStone, {3.2f, 3.8f}, 0.16f, 0.56f);
  const Material web =
      material({0.68f, 0.70f, 0.66f}, {0.02f, 0.025f, 0.028f}, 0.56f, 0.0f, 0.08f, 0.46f,
               7.0f, 0.0f, 0.90f, SurfacePattern::CaveWeb, {5.0f, 5.0f}, 0.05f, 0.32f);

  RenderObject terrain_object;
  terrain_object.name = "terrain patch mesh";
  terrain_object.primitive = MeshPrimitive::Box;
  terrain_object.custom_mesh = labTerrainMesh();
  terrain_object.transform.position = {-2.15f, 0.02f, 0.05f};
  terrain_object.transform.scale = {0.76f, 0.76f, 0.76f};
  terrain_object.material = terrain;
  terrain_object.casts_contact_shadow = true;
  scene.objects().push_back(terrain_object);

  RenderObject cable_object;
  cable_object.name = "twisted cable mesh";
  cable_object.primitive = MeshPrimitive::Box;
  cable_object.custom_mesh = labCableMesh();
  cable_object.transform.position = {-0.68f, 0.52f, 0.0f};
  cable_object.transform.rotation = quatFromEulerXyz({0.0f, 0.0f, radians(88.0f)});
  cable_object.material = cable;
  cable_object.casts_contact_shadow = true;
  scene.objects().push_back(cable_object);

  RenderObject fracture_object;
  fracture_object.name = "fractured rock proxy mesh";
  fracture_object.primitive = MeshPrimitive::Box;
  fracture_object.custom_mesh = labFracturedMesh();
  fracture_object.transform.position = {0.92f, 0.62f, -0.05f};
  fracture_object.material = fracture;
  fracture_object.casts_contact_shadow = true;
  scene.objects().push_back(fracture_object);

  RenderObject web_object;
  web_object.name = "projected cave web mesh";
  web_object.primitive = MeshPrimitive::Box;
  web_object.custom_mesh = labCaveWebMesh();
  web_object.transform.position = {2.30f, 1.02f, -0.10f};
  web_object.transform.rotation = quatFromEulerXyz({0.0f, radians(-8.0f), 0.0f});
  web_object.material = web;
  web_object.material.double_sided = true;
  web_object.casts_contact_shadow = false;
  scene.objects().push_back(web_object);

  return scene;
}

Scene makeLightingLabShowcaseScene() {
  Scene scene;
  const Material floor_material =
      makeSupportSurfaceMaterial(material({0.19f, 0.18f, 0.16f}, {}, 0.88f, 0.0f, 0.0f, 0.38f,
                                          3.4f, 0.12f, 0.82f, SurfacePattern::CourseCells,
                                          {1.9f, 1.9f}, 0.05f, 0.34f));
  RenderObject floor;
  floor.name = "lighting lab receiver";
  floor.primitive = MeshPrimitive::Plane;
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  for (std::size_t i = 0; i < 5u; ++i) {
    RenderObject object;
    object.name = "lighting probe object";
    object.primitive = i % 2u == 0u ? MeshPrimitive::Sphere : MeshPrimitive::Rock;
    object.transform.position = {-2.0f + static_cast<float>(i) * 1.0f, 0.54f,
                                 i % 2u == 0u ? -0.18f : 0.22f};
    object.transform.scale = {0.38f, 0.38f, 0.38f};
    object.material =
        material({0.28f + static_cast<float>(i) * 0.08f, 0.28f, 0.24f}, {}, 0.48f + 0.08f * i,
                 i == 3u ? 0.70f : 0.0f, 0.0f, 0.38f, 4.0f, 0.14f, 0.86f,
                 i == 1u ? SurfacePattern::CaveRock : SurfacePattern::None);
    object.casts_contact_shadow = true;
    object.contact_shadow_strength = 0.78f;
    scene.objects().push_back(object);
  }

  RenderObject emissive;
  emissive.name = "warm emissive crystal";
  emissive.primitive = MeshPrimitive::Crystal;
  emissive.transform.position = {2.52f, 0.82f, 0.38f};
  emissive.transform.scale = {0.28f, 0.48f, 0.28f};
  emissive.material =
      material({0.74f, 0.42f, 0.18f}, {1.0f, 0.40f, 0.12f}, 0.24f, 0.0f, 0.52f, 0.0f, 1.0f,
               0.0f, 1.0f, SurfacePattern::AmberResin, {2.2f, 2.2f}, 0.12f, 0.42f);
  emissive.casts_contact_shadow = true;
  scene.objects().push_back(emissive);

  return scene;
}

Scene makeSceneLabShowcaseScene() {
  Scene scene;
  const Material floor_material =
      makeSupportSurfaceMaterial(material({0.12f, 0.135f, 0.14f}, {}, 0.91f, 0.0f, 0.0f, 0.18f,
                                          2.0f, 0.08f, 0.88f));
  RenderObject floor;
  floor.name = "scene lab floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  const Material crate_material =
      material({0.42f, 0.28f, 0.18f}, {}, 0.72f, 0.0f, 0.0f, 0.66f, 6.0f, 0.22f, 0.78f,
               SurfacePattern::PaintedWood, {4.0f, 3.0f}, 0.10f, 0.46f);
  const Material marker_material =
      material({0.16f, 0.42f, 0.68f}, {0.02f, 0.08f, 0.14f}, 0.36f, 0.1f, 0.16f, 0.12f,
               2.0f, 0.0f, 0.92f);

  for (int z = 0; z < 3; ++z) {
    for (int x = 0; x < 5; ++x) {
      RenderObject crate;
      crate.name = "instancing grid crate";
      crate.primitive = MeshPrimitive::Box;
      crate.transform.position = {-2.2f + static_cast<float>(x) * 1.1f, 0.35f,
                                  -0.95f + static_cast<float>(z) * 0.78f};
      crate.transform.scale = {0.38f, 0.38f, 0.38f};
      crate.material = crate_material;
      crate.casts_contact_shadow = true;
      crate.visibility_hint = {.visibility_class = RenderVisibilityClass::CaveCell,
                               .cell = {static_cast<float>(x), 0.0f, static_cast<float>(z)},
                               .portal_depth = static_cast<float>(z)};
      scene.objects().push_back(crate);
    }
  }

  for (int i = 0; i < 4; ++i) {
    RenderObject marker;
    marker.name = "scene debug marker";
    marker.primitive = MeshPrimitive::Crystal;
    marker.transform.position = {-1.65f + static_cast<float>(i) * 1.1f, 0.92f, 1.52f};
    marker.transform.scale = {0.18f, 0.32f, 0.18f};
    marker.material = marker_material;
    marker.spin_rate = 0.16f + 0.04f * static_cast<float>(i);
    marker.casts_contact_shadow = false;
    scene.objects().push_back(marker);
  }

  return scene;
}

Scene makeCaveConformanceShowcaseScene() {
  Scene scene;

  Material wet_rock =
      material({0.19f, 0.17f, 0.145f}, {}, 0.86f, 0.02f, 0.0f, 0.92f, 7.4f, 0.28f, 0.74f,
               SurfacePattern::CaveRock, {3.2f, 4.8f}, 0.32f, 0.74f, 0.052f,
               {.macro_variation = 0.68f,
                .micro_normal_strength = 0.56f,
                .roughness_variation = 0.38f,
                .wetness = 0.64f,
                .height_shading = 0.34f});
  wet_rock.asset_id = "CaveConformanceWetRock";

  Material floor_material = makeSupportSurfaceMaterial(wet_rock);
  floor_material.asset_id = "CaveConformanceWetRock";

  Material emissive_lamp =
      material({0.74f, 0.34f, 0.12f}, {1.0f, 0.38f, 0.08f}, 0.30f, 0.0f, 0.92f, 0.30f, 4.0f,
               0.04f, 1.0f, SurfacePattern::AmberResin, {2.0f, 2.0f}, 0.10f, 0.42f);
  emissive_lamp.asset_id = "CaveConformanceEmissive";

  Material wet_metal =
      material({0.20f, 0.18f, 0.16f}, {0.02f, 0.012f, 0.004f}, 0.58f, 0.76f, 0.0f, 0.64f,
               9.5f, 0.26f, 0.82f, SurfacePattern::WeatheredMetal, {4.2f, 8.0f}, 0.32f,
               0.78f, 0.040f, {.macro_variation = 0.44f,
                                .micro_normal_strength = 0.34f,
                                .roughness_variation = 0.46f,
                                .wetness = 0.24f,
                                .height_shading = 0.18f});
  wet_metal.asset_id = "CaveConformanceWetMetal";

  RenderObject floor;
  floor.name = "cave conformance wet floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.15f, 1.0f, 1.10f};
  floor.material_asset_id = wet_rock.asset_id;
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  floor.casts_shadows = false;
  scene.objects().push_back(floor);

  struct WallSpec {
    const char *name;
    Vec3 position;
    Vec3 scale;
    Vec3 rotation;
  };
  const WallSpec walls[] = {
      {"cave conformance back wall", {0.0f, 1.08f, -2.20f}, {2.85f, 1.55f, 0.18f}, {}},
      {"cave conformance left wall", {-2.50f, 1.02f, -0.18f}, {0.18f, 1.45f, 2.05f},
       {0.0f, radians(8.0f), 0.0f}},
      {"cave conformance right wall", {2.34f, 1.08f, -0.08f}, {0.18f, 1.55f, 2.25f},
       {0.0f, radians(-10.0f), 0.0f}},
      {"cave conformance low ceiling", {0.0f, 2.18f, -0.68f}, {2.65f, 0.16f, 2.10f},
       {radians(3.0f), 0.0f, radians(-2.0f)}},
  };
  for (const WallSpec &spec : walls) {
    RenderObject wall;
    wall.name = spec.name;
    wall.primitive = MeshPrimitive::Box;
    wall.transform.position = spec.position;
    wall.transform.scale = spec.scale;
    wall.transform.rotation = quatFromEulerXyz(spec.rotation);
    wall.material_asset_id = wet_rock.asset_id;
    wall.material = wet_rock;
    wall.casts_contact_shadow = false;
    wall.casts_shadows = true;
    scene.objects().push_back(wall);
  }

  for (std::size_t i = 0; i < 3u; ++i) {
    RenderObject rock;
    rock.name = "cave conformance caster rock";
    rock.primitive = MeshPrimitive::Rock;
    rock.transform.position = {-0.95f + static_cast<float>(i) * 0.88f, 0.42f,
                               -0.92f + static_cast<float>(i % 2u) * 0.58f};
    rock.transform.scale = {0.36f, 0.34f + static_cast<float>(i) * 0.07f, 0.34f};
    rock.transform.rotation = quatFromEulerXyz({0.0f, radians(20.0f * static_cast<float>(i)),
                                                radians(5.0f)});
    rock.material_asset_id = wet_rock.asset_id;
    rock.material = wet_rock;
    rock.casts_contact_shadow = true;
    rock.contact_shadow_strength = 0.62f;
    scene.objects().push_back(rock);
  }

  RenderObject metal_fixture;
  metal_fixture.name = "cave conformance wet metal fixture";
  metal_fixture.primitive = MeshPrimitive::Box;
  metal_fixture.transform.position = {-1.62f, 1.06f, -1.66f};
  metal_fixture.transform.scale = {0.16f, 0.48f, 0.08f};
  metal_fixture.material_asset_id = wet_metal.asset_id;
  metal_fixture.material = wet_metal;
  metal_fixture.casts_shadows = true;
  scene.objects().push_back(metal_fixture);

  for (const Vec3 position : {Vec3{-1.58f, 1.20f, -1.52f}, Vec3{1.35f, 1.08f, -1.82f}}) {
    RenderObject lamp;
    lamp.name = "cave conformance emissive lamp";
    lamp.primitive = MeshPrimitive::Crystal;
    lamp.transform.position = position;
    lamp.transform.scale = {0.16f, 0.30f, 0.16f};
    lamp.material_asset_id = emissive_lamp.asset_id;
    lamp.material = emissive_lamp;
    lamp.casts_contact_shadow = true;
    scene.objects().push_back(lamp);
  }

  scene.reflectionProbes().push_back({.name = "cave conformance local reflection probe",
                                      .position = {0.0f, 0.86f, -0.85f},
                                      .influence_radius = 4.8f,
                                      .sky_irradiance = {0.20f, 0.24f, 0.30f},
                                      .ground_irradiance = {0.10f, 0.075f, 0.052f},
                                      .specular_tint = {1.0f, 0.88f, 0.76f},
                                      .intensity = 1.35f});
  return scene;
}

} // namespace aster
