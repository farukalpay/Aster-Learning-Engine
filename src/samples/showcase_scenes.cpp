// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/showcase_scenes.hpp"

#include "aster/asset/pipe_runtime_asset.hpp"
#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/cave_web_mesh.hpp"
#include "aster/geometry/fracture_mesh.hpp"
#include "aster/geometry/primate_anatomy.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/material/procedural_surface.hpp"

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
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
  return makeMaterial({.base_color = LinearRgb{base},
                       .emission_color = EmissionColor{emission},
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

float labSoilHeightAt(const float px, const float pz) {
  const Vec3 up{0.0f, 1.0f, 0.0f};
  const float macro =
      (asterSurfaceProjectedFbm({px + 4.7f, 0.0f, pz - 2.1f}, up, 0.075f, 71.0f, 5) - 0.5f) *
      0.34f;
  const float packed =
      (asterSurfaceProjectedFbm({px - 1.3f, 0.0f, pz + 6.2f}, up, 0.31f, 83.0f, 4) - 0.5f) *
      0.11f;
  const float crust =
      (asterSurfaceRidgedFbm({px * 0.92f, 0.0f, pz * 0.92f}, up, 0.82f, 97.0f, 4) - 0.5f) *
      0.040f;
  const float gritty =
      (asterSurfaceValueNoise({px * 4.4f, 0.0f, pz * 4.4f}) - 0.5f) * 0.009f;
  const float settled = std::sin(px * 0.18f - pz * 0.10f) * 0.020f;
  return macro + packed + crust + gritty + settled;
}

Vec3 labSoilNormalAt(const float px, const float pz) {
  constexpr float step = 0.055f;
  const float left = labSoilHeightAt(px - step, pz);
  const float right = labSoilHeightAt(px + step, pz);
  const float back = labSoilHeightAt(px, pz - step);
  const float front = labSoilHeightAt(px, pz + step);
  return normalize(Vec3{left - right, step * 2.0f, back - front});
}

std::shared_ptr<const CpuMesh> labSoilPatchMesh() {
  static const std::shared_ptr<const CpuMesh> mesh = [] {
    CpuMesh soil;
    constexpr int columns = 60;
    constexpr int rows = 44;
    constexpr float width = 20000.0f;
    constexpr float depth = 24000.0f;
    constexpr float side_drop = 0.42f;
    soil.vertices.reserve(static_cast<std::size_t>(columns * rows + columns * 4 + rows * 4));
    for (int z = 0; z < rows; ++z) {
      const float v = static_cast<float>(z) / static_cast<float>(rows - 1);
      for (int x = 0; x < columns; ++x) {
        const float u = static_cast<float>(x) / static_cast<float>(columns - 1);
        const float px = (u - 0.5f) * width;
        const float pz = (v - 0.5f) * depth;
        Vertex vertex;
        vertex.position = {px, labSoilHeightAt(px, pz), pz};
        vertex.normal = labSoilNormalAt(px, pz);
        vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        vertex.uv = {u * 8.0f, v * 6.0f};
        vertex.ambient_occlusion = 0.90f + vertex.normal.y * 0.06f;
        soil.vertices.push_back(vertex);
      }
    }
    for (int z = 0; z + 1 < rows; ++z) {
      for (int x = 0; x + 1 < columns; ++x) {
        const std::uint32_t a = static_cast<std::uint32_t>(z * columns + x);
        const std::uint32_t b = static_cast<std::uint32_t>(z * columns + x + 1);
        const std::uint32_t c = static_cast<std::uint32_t>((z + 1) * columns + x);
        const std::uint32_t d = static_cast<std::uint32_t>((z + 1) * columns + x + 1);
        soil.indices.insert(soil.indices.end(), {a, c, b, b, c, d});
      }
    }
    const auto append_side = [&](const Vec3 top_a, const Vec3 top_b, const Vec3 bottom_a,
                                 const Vec3 bottom_b, const Vec3 normal, const Vec3 tangent,
                                 const float u0, const float u1) {
      const std::uint32_t base = static_cast<std::uint32_t>(soil.vertices.size());
      const Vec4 tangent4{tangent.x, tangent.y, tangent.z, 1.0f};
      soil.vertices.push_back({top_a, normal, {u0, 0.0f}, tangent4, 0.78f});
      soil.vertices.push_back({bottom_a, normal, {u0, 1.0f}, tangent4, 0.68f});
      soil.vertices.push_back({top_b, normal, {u1, 0.0f}, tangent4, 0.78f});
      soil.vertices.push_back({bottom_b, normal, {u1, 1.0f}, tangent4, 0.68f});
      soil.indices.insert(soil.indices.end(), {base, base + 1u, base + 2u, base + 2u,
                                               base + 1u, base + 3u});
    };
    const float bottom_y = -side_drop;
    for (int x = 0; x + 1 < columns; ++x) {
      const float u0 = static_cast<float>(x) / static_cast<float>(columns - 1);
      const float u1 = static_cast<float>(x + 1) / static_cast<float>(columns - 1);
      const float x0 = (u0 - 0.5f) * width;
      const float x1 = (u1 - 0.5f) * width;
      const float z_front = depth * 0.5f;
      const float z_back = -depth * 0.5f;
      append_side({x0, labSoilHeightAt(x0, z_front), z_front},
                  {x1, labSoilHeightAt(x1, z_front), z_front},
                  {x0, bottom_y + labSoilHeightAt(x0, z_front) * 0.22f, z_front},
                  {x1, bottom_y + labSoilHeightAt(x1, z_front) * 0.22f, z_front},
                  {0.0f, 0.10f, 0.995f}, {1.0f, 0.0f, 0.0f}, u0, u1);
      append_side({x1, labSoilHeightAt(x1, z_back), z_back},
                  {x0, labSoilHeightAt(x0, z_back), z_back},
                  {x1, bottom_y + labSoilHeightAt(x1, z_back) * 0.22f, z_back},
                  {x0, bottom_y + labSoilHeightAt(x0, z_back) * 0.22f, z_back},
                  {0.0f, 0.10f, -0.995f}, {-1.0f, 0.0f, 0.0f}, u0, u1);
    }
    for (int z = 0; z + 1 < rows; ++z) {
      const float v0 = static_cast<float>(z) / static_cast<float>(rows - 1);
      const float v1 = static_cast<float>(z + 1) / static_cast<float>(rows - 1);
      const float z0 = (v0 - 0.5f) * depth;
      const float z1 = (v1 - 0.5f) * depth;
      const float x_left = -width * 0.5f;
      const float x_right = width * 0.5f;
      append_side({x_left, labSoilHeightAt(x_left, z1), z1},
                  {x_left, labSoilHeightAt(x_left, z0), z0},
                  {x_left, bottom_y + labSoilHeightAt(x_left, z1) * 0.22f, z1},
                  {x_left, bottom_y + labSoilHeightAt(x_left, z0) * 0.22f, z0},
                  {-0.995f, 0.10f, 0.0f}, {0.0f, 0.0f, -1.0f}, v0, v1);
      append_side({x_right, labSoilHeightAt(x_right, z0), z0},
                  {x_right, labSoilHeightAt(x_right, z1), z1},
                  {x_right, bottom_y + labSoilHeightAt(x_right, z0) * 0.22f, z0},
                  {x_right, bottom_y + labSoilHeightAt(x_right, z1) * 0.22f, z1},
                  {0.995f, 0.10f, 0.0f}, {0.0f, 0.0f, 1.0f}, v0, v1);
    }
    return std::make_shared<const CpuMesh>(std::move(soil));
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

Material cercopithecidaeMaterialFor(const AnatomicalTissue tissue) {
  switch (tissue) {
  case AnatomicalTissue::Bone:
    return material({0.72f, 0.66f, 0.55f}, {}, 0.74f, 0.0f, 0.0f, 0.34f, 5.0f, 0.08f,
                    0.88f, SurfacePattern::None);
  case AnatomicalTissue::Enamel:
    return material({0.92f, 0.88f, 0.76f}, {0.015f, 0.012f, 0.008f}, 0.34f, 0.0f, 0.02f,
                    0.12f, 2.0f, 0.0f, 0.96f, SurfacePattern::None);
  case AnatomicalTissue::Muscle:
    return material({0.48f, 0.15f, 0.13f}, {}, 0.62f, 0.0f, 0.0f, 0.48f, 6.0f, 0.10f,
                    0.76f, SurfacePattern::FiberStrands, {7.0f, 2.0f}, 0.08f, 0.38f);
  case AnatomicalTissue::Tendon:
    return material({0.78f, 0.68f, 0.48f}, {}, 0.66f, 0.0f, 0.0f, 0.42f, 9.0f, 0.06f,
                    0.84f, SurfacePattern::FiberStrands, {9.0f, 1.4f}, 0.06f, 0.32f);
  case AnatomicalTissue::SoftTissue:
    return material({0.55f, 0.32f, 0.25f}, {}, 0.70f, 0.0f, 0.0f, 0.24f, 3.0f, 0.04f,
                    0.82f, SurfacePattern::FiberStrands, {2.0f, 2.0f}, 0.025f, 0.16f);
  case AnatomicalTissue::PlantarPad:
    return material({0.20f, 0.17f, 0.14f}, {}, 0.84f, 0.0f, 0.0f, 0.30f, 3.4f, 0.06f,
                    0.70f, SurfacePattern::FiberStrands, {2.4f, 1.6f}, 0.035f, 0.20f);
  case AnatomicalTissue::FurSkin:
    return material({0.34f, 0.29f, 0.22f}, {}, 0.82f, 0.0f, 0.0f, 0.58f, 9.0f, 0.12f,
                    0.70f, SurfacePattern::FurFibers, {8.0f, 4.0f}, 0.10f, 0.34f,
                    0.08f, {.macro_variation = 0.28f,
                            .micro_normal_strength = 0.26f,
                            .roughness_variation = 0.20f,
                            .height_shading = 0.08f});
  }
  return material({0.65f, 0.62f, 0.56f}, {}, 0.70f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
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

  const Material floor_material =
      material({0.075f, 0.080f, 0.082f}, {0.0f, 0.0f, 0.0f}, 0.86f, 0.0f, 0.0f, 0.25f, 3.0f, 0.10f,
               0.90f);

  RenderObject floor;
  floor.name = "neutral inspection plane";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.0f, 1.0f, 1.0f};
  floor.material = floor_material;
  scene.objects().push_back(floor);

  const AsterPipeAsset pipe_asset = makeAsterPipeAsset({.asset_id = "asset_graph.pipe_lab.rusted_pipe",
                                                        .length = 5.2f,
                                                        .outer_radius = 0.54f,
                                                        .wall_thickness = 0.090f,
                                                        .radial_segments = 96,
                                                        .length_segments = 24,
                                                        .include_longitudinal_seam = false,
                                                        .include_flanges = false,
                                                        .include_bolts = false,
                                                        .bolt_count_per_flange = 10,
                                                        .rust_strength = 0.98f,
                                                        .wetness_strength = 0.18f,
                                                        .pitting_density = 1.32f,
                                                        .pitting_depth = 0.0048f,
                                                        .oxide_layering = 0.92f,
                                                        .cavity_grime_strength = 0.84f,
                                                        .edge_polish_strength = 0.36f,
                                                        .weld_heat_tint_strength = 0.48f,
                                                        .axial_scratch_strength = 0.66f,
                                                        .rust_bloom_strength = 0.92f,
                                                        .black_scab_strength = 0.78f,
                                                        .paint_remnant_strength = 0.06f,
                                                        .weld_slag_strength = 0.88f,
                                                        .rim_soot_strength = 0.92f,
                                                        .attachment_clearance = 0.0065f,
                                                        .weld_contact_skirt_width = 0.040f,
                                                        .seam_inset_depth = 0.004f,
                                                        .rim_normal_feather = 0.78f,
                                                        .wet_streak_count = 7});
  for (const AsterPipeAssetPart &part : pipe_asset.parts) {
    RenderObject object;
    object.name = "runtime rusted pipe " + part.name;
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = std::make_shared<const CpuMesh>(part.mesh);
    object.transform.position = {0.0f, 0.57f, 0.0f};
    object.transform.rotation = quatFromEulerXyz({0.0f, 0.0f, -0.045f});
    object.material = makeAsterPipeMaterial(part.material_slot);
    object.material_asset_id = "asset_graph.pipe_lab.rusted_pipe/" + part.material_slot;
    object.casts_contact_shadow = true;
    object.contact_shadow_strength = part.material_slot == "pipe.weld" ? 0.52f : 0.64f;
    object.contact_shadow_radius_scale = part.material_slot == "pipe.body" ? 1.55f : 1.08f;
    scene.objects().push_back(std::move(object));
  }

  return scene;
}

Scene makeMaterialLabShowcaseScene() {
  Scene scene;
  scene.reflectionProbes().push_back({"material lab soft sky probe",
                                      {0.0f, 1.60f, -1.4f},
                                      11.0f,
                                      {0.56f, 0.70f, 0.95f},
                                      {0.18f, 0.125f, 0.072f},
                                      {1.12f, 1.08f, 0.98f},
                                      1.38f,
                                      {}});
  scene.reflectionProbes().push_back({"material lab contact irradiance probe",
                                      {0.0f, 0.38f, 1.25f},
                                      4.20f,
                                      {0.30f, 0.38f, 0.52f},
                                      {0.34f, 0.22f, 0.12f},
                                      {1.05f, 0.94f, 0.78f},
                                      0.72f,
                                      {}});
  scene.reflectionProbes().push_back({"material lab grazing rim probe",
                                      {-2.30f, 2.10f, -2.0f},
                                      6.50f,
                                      {0.52f, 0.70f, 1.0f},
                                      {0.12f, 0.10f, 0.085f},
                                      {0.86f, 0.96f, 1.18f},
                                      0.62f,
                                      {}});

  Material soil_surface =
      material({0.135f, 0.104f, 0.070f}, {}, 0.98f, 0.0f, 0.0f, 0.88f, 3.2f, 0.10f,
               0.97f, SurfacePattern::TerrainBlend, {5.6f, 6.8f}, 0.34f, 0.78f, 0.052f,
               {.macro_variation = 0.78f,
                .micro_normal_strength = 0.50f,
                .roughness_variation = 0.38f,
                .physical_texel_density = 940.0f,
                .height_normal_coupling = 1.12f,
                .roughness_height_coupling = 0.96f,
                .macro_frequency_breakup = 0.70f,
                .micro_frequency_breakup = 0.72f,
                .wetness = 0.025f,
                .height_shading = 0.58f,
                .pitting_density = 0.22f,
                .pitting_depth = 0.18f,
                .cavity_grime = 0.18f});
  soil_surface.edge_sheen_color = {0.026f, 0.022f, 0.017f};
  soil_surface.edge_sheen_roughness = 0.72f;
  const Material floor_material = makeSupportSurfaceMaterial(soil_surface);
  RenderObject floor;
  floor.name = "material lab granular soil floor";
  floor.primitive = MeshPrimitive::Box;
  floor.custom_mesh = labSoilPatchMesh();
  floor.transform.position = {0.0f, 0.0f, 0.02f};
  floor.transform.scale = {1.0f, 1.0f, 1.0f};
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  Material brushed_aluminium =
      material({0.60f, 0.62f, 0.61f}, {0.002f, 0.003f, 0.003f}, 0.22f, 0.98f, 0.0f,
               0.62f, 36.0f, 0.10f, 0.90f, SurfacePattern::FiberStrands,
               {38.0f, 1.08f}, 0.09f, 0.42f, 0.04f,
               {.macro_variation = 0.26f,
                .micro_normal_strength = 0.16f,
                .roughness_variation = 0.22f,
                .physical_texel_density = 1152.0f,
                .height_normal_coupling = 0.70f,
                .roughness_height_coupling = 0.54f,
                .macro_frequency_breakup = 0.36f,
                .micro_frequency_breakup = 1.02f,
                .wetness = 0.01f,
                .height_shading = 0.12f,
                .edge_polish = 0.40f,
                .axial_scratches = 0.55f});
  brushed_aluminium.dielectric_reflectance = 0.64f;
  brushed_aluminium.coat_strength = 0.08f;
  brushed_aluminium.coat_roughness = 0.18f;
  brushed_aluminium.tangent_anisotropy = 0.92f;

  Material honed_slate =
      material({0.210f, 0.220f, 0.218f}, {}, 0.72f, 0.01f, 0.0f, 0.68f, 6.2f, 0.18f,
               0.92f, SurfacePattern::CaveRock, {3.2f, 4.2f}, 0.22f, 0.38f, 0.052f,
               {.macro_variation = 0.72f,
                .micro_normal_strength = 0.64f,
                .roughness_variation = 0.36f,
                .physical_texel_density = 960.0f,
                .height_normal_coupling = 0.92f,
                .roughness_height_coupling = 0.78f,
                .macro_frequency_breakup = 0.54f,
                .micro_frequency_breakup = 0.66f,
                .wetness = 0.08f,
                .height_shading = 0.44f});
  honed_slate.dielectric_reflectance = 0.42f;
  honed_slate.coat_strength = 0.08f;
  honed_slate.coat_roughness = 0.26f;
  honed_slate.edge_sheen_color = {0.050f, 0.062f, 0.060f};
  honed_slate.edge_sheen_roughness = 0.64f;

  Material green_marble =
      material({0.055f, 0.255f, 0.190f}, {0.004f, 0.008f, 0.005f}, 0.24f, 0.0f, 0.0f,
               0.90f, 10.8f, 0.05f, 0.92f, SurfacePattern::CoalVein, {5.8f, 7.6f},
               0.24f, 0.82f, 0.045f,
               {.macro_variation = 0.50f,
                .micro_normal_strength = 0.16f,
                .roughness_variation = 0.28f,
                .physical_texel_density = 864.0f,
                .height_normal_coupling = 0.42f,
                .roughness_height_coupling = 0.52f,
                .macro_frequency_breakup = 0.60f,
                .micro_frequency_breakup = 0.54f,
                .wetness = 0.04f,
                .height_shading = 0.16f});
  green_marble.dielectric_reflectance = 0.80f;
  green_marble.coat_strength = 0.52f;
  green_marble.coat_roughness = 0.16f;
  green_marble.edge_sheen_color = {0.10f, 0.22f, 0.16f};
  green_marble.edge_sheen_roughness = 0.28f;

  Material crackle_ceramic =
      material({0.78f, 0.72f, 0.62f}, {0.005f, 0.004f, 0.002f}, 0.42f, 0.0f, 0.0f,
               0.82f, 7.2f, 0.04f, 0.90f, SurfacePattern::AmberResin,
               {7.4f, 5.8f}, 0.18f, 0.78f, 0.04f,
               {.macro_variation = 0.40f,
                .micro_normal_strength = 0.24f,
                .roughness_variation = 0.38f,
                .physical_texel_density = 980.0f,
                .height_normal_coupling = 0.58f,
                .roughness_height_coupling = 0.62f,
                .macro_frequency_breakup = 0.52f,
                .micro_frequency_breakup = 0.64f,
                .wetness = 0.02f,
                .height_shading = 0.24f,
                .cavity_grime = 0.10f});
  crackle_ceramic.dielectric_reflectance = 0.58f;
  crackle_ceramic.coat_strength = 0.30f;
  crackle_ceramic.coat_roughness = 0.24f;
  crackle_ceramic.edge_sheen_color = {0.090f, 0.078f, 0.055f};
  crackle_ceramic.edge_sheen_roughness = 0.52f;

  const Material materials[] = {brushed_aluminium, honed_slate, green_marble, crackle_ceramic};
  const char *names[] = {"brushed aluminium material sphere", "honed slate material sphere",
                         "polished jade material sphere", "crackle ceramic material sphere"};
  const float sphere_x[] = {-2.70f, -0.88f, 0.92f, 2.70f};
  const float sphere_z[] = {-0.20f, 0.12f, -0.08f, 0.08f};
  constexpr float sphere_radius = 0.55f;
  for (std::size_t i = 0; i < 4u; ++i) {
    RenderObject object;
    object.name = names[i];
    object.primitive = MeshPrimitive::Sphere;
    object.transform.position = {sphere_x[i],
                                 labSoilHeightAt(sphere_x[i], sphere_z[i]) + sphere_radius - 0.070f,
                                 sphere_z[i]};
    object.transform.scale = {sphere_radius, sphere_radius, sphere_radius};
    object.material = materials[i];
    object.casts_contact_shadow = true;
    object.contact_shadow_strength = i == 2u ? 0.88f : 1.00f;
    object.contact_shadow_radius_scale = i == 3u ? 1.26f : 1.18f;
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

Scene makeCleanCaveShowcaseScene() {
  Scene scene;

  const Material wet_rock =
      material({0.18f, 0.17f, 0.155f}, {}, 0.88f, 0.0f, 0.0f, 0.84f, 6.8f, 0.22f, 0.78f,
               SurfacePattern::CaveRock, {3.0f, 4.2f}, 0.28f, 0.62f, 0.055f,
               {.macro_variation = 0.60f,
                .micro_normal_strength = 0.48f,
                .roughness_variation = 0.32f,
                .wetness = 0.42f,
                .height_shading = 0.25f});
  const Material floor_material = makeSupportSurfaceMaterial(wet_rock);
  const Material warm_lamp =
      material({0.68f, 0.36f, 0.14f}, {1.0f, 0.46f, 0.14f}, 0.36f, 0.0f, 0.55f, 0.20f,
               3.0f, 0.04f, 1.0f, SurfacePattern::AmberResin, {2.0f, 2.0f}, 0.10f, 0.34f);
  const Material mineral =
      material({0.30f, 0.30f, 0.27f}, {0.02f, 0.025f, 0.030f}, 0.74f, 0.05f, 0.04f, 0.46f,
               5.0f, 0.18f, 0.82f, SurfacePattern::CoalVein, {2.2f, 3.4f}, 0.16f, 0.46f);

  RenderObject floor;
  floor.name = "clean cave wet floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.20f, 1.0f, 1.12f};
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  struct WallSpec {
    const char *name;
    Vec3 position;
    Vec3 scale;
    Vec3 rotation;
    Material material;
  };
  const WallSpec walls[] = {
      {"clean cave back wall", {0.0f, 1.14f, -2.22f}, {5.80f, 1.84f, 0.22f}, {}, wet_rock},
      {"clean cave left wall", {-2.86f, 1.06f, -0.35f}, {0.22f, 1.70f, 2.58f},
       {0.0f, radians(7.0f), 0.0f}, wet_rock},
      {"clean cave right wall", {2.74f, 1.08f, -0.25f}, {0.22f, 1.72f, 2.70f},
       {0.0f, radians(-9.0f), 0.0f}, wet_rock},
      {"clean cave low ceiling", {0.0f, 1.92f, -0.76f}, {5.60f, 0.22f, 2.86f},
       {radians(3.0f), 0.0f, radians(-1.5f)}, wet_rock},
      {"clean cave mineral shelf", {-0.82f, 0.58f, -1.96f}, {0.95f, 0.16f, 0.22f},
       {0.0f, radians(-3.0f), radians(2.0f)}, mineral},
  };
  for (const WallSpec &spec : walls) {
    RenderObject wall;
    wall.name = spec.name;
    wall.primitive = MeshPrimitive::Box;
    wall.transform.position = spec.position;
    wall.transform.scale = spec.scale;
    wall.transform.rotation = quatFromEulerXyz(spec.rotation);
    wall.material = spec.material;
    wall.casts_shadows = true;
    scene.objects().push_back(wall);
  }

  for (const Vec3 position : {Vec3{-1.30f, 1.22f, -1.66f}, Vec3{1.22f, 1.06f, -1.90f},
                              Vec3{0.0f, 0.82f, -2.05f}}) {
    RenderObject lamp;
    lamp.name = "clean cave warm crystal";
    lamp.primitive = MeshPrimitive::Crystal;
    lamp.transform.position = position;
    lamp.transform.scale = {0.13f, 0.26f, 0.13f};
    lamp.material = warm_lamp;
    lamp.casts_contact_shadow = false;
    scene.objects().push_back(lamp);
  }

  scene.reflectionProbes().push_back({.name = "clean cave reflection probe",
                                      .position = {0.0f, 0.82f, -0.92f},
                                      .influence_radius = 4.6f,
                                      .sky_irradiance = {0.18f, 0.22f, 0.28f},
                                      .ground_irradiance = {0.085f, 0.070f, 0.052f},
                                      .specular_tint = {1.0f, 0.90f, 0.78f},
                                      .intensity = 1.15f});
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

Scene makeCercopithecidaeShowcaseScene() {
  Scene scene;

  const Material floor_material =
      makeSupportSurfaceMaterial(material({0.10f, 0.115f, 0.105f}, {}, 0.92f, 0.0f, 0.0f,
                                          0.20f, 3.0f, 0.08f, 0.86f,
                                          SurfacePattern::CourseCells, {2.0f, 2.0f},
                                          0.035f, 0.22f));
  RenderObject floor;
  floor.name = "cercopithecidae anatomical inspection floor";
  floor.primitive = MeshPrimitive::Plane;
  floor.transform.scale = {1.35f, 1.0f, 1.35f};
  floor.material = floor_material;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  const AnatomicalModel model = makeCercopithecidaeModel({.surface_segments = 36,
                                                          .surface_rings = 18,
                                                          .include_soft_tissue = true,
                                                          .include_muscle_insertions = true,
                                                          .include_surface_pads = true,
                                                          .include_surface_detail = true,
                                                          .fur_strand_guides = 96,
                                                          .surface_detail_strength = 1.0f});
  for (const AnatomicalModelPart &part : model.parts) {
    RenderObject object;
    object.name = "Cercopithecidae " + part.name;
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = std::make_shared<const CpuMesh>(part.mesh);
    const bool envelope = part.tissue == AnatomicalTissue::FurSkin;
    object.transform.position = {envelope ? 0.58f : -0.36f, 0.04f, envelope ? 0.18f : 0.26f};
    object.transform.rotation = quatFromEulerXyz({0.0f, radians(-17.0f), 0.0f});
    object.transform.scale = envelope ? Vec3{0.96f, 0.96f, 0.96f} : Vec3{0.98f, 0.98f, 0.98f};
    object.material = cercopithecidaeMaterialFor(part.tissue);
    if (envelope) {
      object.material.opacity = 0.58f;
      object.material.alpha_mode = MaterialAlphaMode::Blend;
      object.material.depth_write = MaterialDepthWrite::Disabled;
    }
    object.material_asset_id = std::string("procedural.cercopithecidae.") +
                               anatomicalTissueName(part.tissue);
    object.casts_contact_shadow = true;
    object.contact_shadow_strength = part.tissue == AnatomicalTissue::FurSkin ? 0.28f : 0.42f;
    object.contact_shadow_radius_scale = 0.70f;
    scene.objects().push_back(std::move(object));
  }

  return scene;
}

} // namespace aster
