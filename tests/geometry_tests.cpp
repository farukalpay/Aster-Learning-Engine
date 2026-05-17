// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

namespace {

void testCaveWebMeshFitsOvalWithoutRectFrame() {
  const aster::Vec3 center{2.0f, 3.0f, -4.0f};
  const aster::Vec3 side{1.0f, 0.0f, 0.0f};
  const aster::Vec3 up{0.0f, 1.0f, 0.0f};
  constexpr float radius_x = 1.35f;
  constexpr float radius_y = 0.92f;
  const aster::CpuMesh mesh =
      aster::makeCaveWebMesh({.center = center,
                              .normal = {0.0f, 0.0f, -1.0f},
                              .side = side,
                              .up = up,
                              .radius_x = radius_x,
                              .radius_y = radius_y,
                              .radial_strands = 20,
                              .ring_strands = 6,
                              .ring_segments = 96,
                              .strand_width = 0.012f,
                              .sag = 0.08f,
                              .irregularity = 0.05f,
                              .seed = 117u,
                              .double_sided = true});
  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() % 3u == 0u);

  bool saw_corner_like_vertex = false;
  for (const aster::Vertex &vertex : mesh.vertices) {
    const aster::Vec3 offset = vertex.position - center;
    const float x = aster::dot(offset, side) / radius_x;
    const float y = aster::dot(offset, up) / radius_y;
    assert(x * x + y * y <= 1.10f);
    saw_corner_like_vertex =
        saw_corner_like_vertex || (std::abs(x) > 0.88f && std::abs(y) > 0.88f);
    assert(aster::length(vertex.normal) > 0.50f);
  }
  assert(!saw_corner_like_vertex);
}

void testMeshGeneration() {
  const aster::CpuMesh sphere = aster::makeUvSphere(16, 8, 1.0f);
  assert(!sphere.vertices.empty());
  assert(!sphere.indices.empty());
  assert(sphere.indices.size() == 16u * 8u * 6u);

  const aster::CpuMesh plane = aster::makePlane(2.0f);
  assert(plane.vertices.size() == 4u);
  assert(plane.indices.size() == 6u);

  const aster::CpuMesh box = aster::makeBox();
  assert(box.vertices.size() == 36u);
  assert(box.indices.size() == 36u);

  const aster::CpuMesh rock = aster::makeRock(8, 5, 1.0f);
  assert(!rock.vertices.empty());
  assert(!rock.indices.empty());

  const aster::CpuMesh crystal = aster::makeCrystal(6, 1.0f, 1.8f);
  assert(crystal.indices.size() == 6u * 12u);

  const aster::CpuMesh block = aster::makeRuinBlock();
  assert(block.indices.size() == 36u);
  for (const aster::Vertex &vertex : block.vertices) {
    assert(aster::dot(vertex.position, vertex.normal) > 0.0f);
  }

  const aster::CpuMesh pillar = aster::makePillar(8, 1.0f, 1.0f);
  assert(!pillar.vertices.empty());
  assert(!pillar.indices.empty());

  const aster::CpuMesh cable = aster::makeCableMesh();
  assert(!cable.vertices.empty());
  assert(!cable.indices.empty());

  const aster::CpuMesh conduit =
      aster::makeEnergyConduitRibbonMesh({.points = {{0.0f, 0.0f, 0.0f},
                                                     {1.0f, 0.35f, 0.30f},
                                                     {2.0f, 0.0f, 0.0f}},
                                          .width = 0.12f,
                                          .crest_height = 0.20f,
                                          .subdivisions_per_segment = 4});
  assert(!conduit.vertices.empty());
  assert(!conduit.indices.empty());
  assert(conduit.indices.size() % 3u == 0u);
  for (const aster::Vertex &vertex : conduit.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.normal.y));
    assert(aster::length(vertex.normal) > 0.50f);
  }
  const aster::CpuMesh conduit_ring =
      aster::makeEnergyConduitRingMesh({.radius = 0.8f, .band_width = 0.06f, .segments = 24});
  assert(!conduit_ring.vertices.empty());
  assert(!conduit_ring.indices.empty());
  assert(conduit_ring.indices.size() % 3u == 0u);
  const aster::CpuMesh conduit_network = aster::makeEnergyConduitRibbonNetworkMesh(
      {{.points = {{0.0f, 0.0f, 0.0f}, {0.8f, 0.4f, 0.2f}, {1.2f, 0.0f, 0.0f}},
        .width = 0.08f,
        .subdivisions_per_segment = 3},
       {.points = {{0.0f, 0.0f, 0.0f}, {-0.6f, 0.3f, 0.4f}, {-1.0f, 0.0f, 0.7f}},
        .width = 0.10f,
        .subdivisions_per_segment = 3}});
  assert(conduit_network.vertices.size() > conduit.vertices.size());
  assert(conduit_network.indices.size() > conduit.indices.size());

  const aster::VoronoiFractureSpec fracture_spec{
      .volume = {.center = {0.0f, 0.0f, 0.0f},
                 .half_extents = {0.80f, 0.42f, 0.56f},
                 .axis_x = {1.0f, 0.0f, 0.0f},
                 .axis_y = {0.0f, 1.0f, 0.0f},
                 .axis_z = {0.0f, 0.0f, 1.0f}},
      .impact_point = {0.18f, 0.06f, 0.52f},
      .impact_normal = {0.0f, 0.0f, 1.0f},
      .seed = 19u,
      .shard_count = 8};
  const std::vector<aster::VoronoiFractureShard> fracture_shards =
      aster::buildImpactVoronoiFracture(fracture_spec);
  assert(fracture_shards.size() >= 4u);
  float relative_volume = 0.0f;
  for (const aster::VoronoiFractureShard &shard : fracture_shards) {
    assert(!shard.mesh.vertices.empty());
    assert(!shard.mesh.indices.empty());
    assert(shard.mesh.indices.size() % 3u == 0u);
    assert(aster::length(shard.impulse_direction) > 0.90f);
    assert(std::abs(shard.centroid.x) <= 0.82f);
    assert(std::abs(shard.centroid.y) <= 0.44f);
    assert(std::abs(shard.centroid.z) <= 0.58f);
    relative_volume += shard.relative_volume;
  }
  expectNear(relative_volume, 1.0f, 0.001f);

  const aster::ConvexCutVolume cut_volume{.center = {0.0f, 0.0f, 0.0f},
                                          .half_extents = {0.8f, 0.5f, 0.6f},
                                          .axis_x = {1.0f, 0.0f, 0.0f},
                                          .axis_y = {0.0f, 1.0f, 0.0f},
                                          .axis_z = {0.0f, 0.0f, 1.0f}};
  const aster::CpuMesh cut_box = aster::makeConvexCutBox(cut_volume);
  aster::MeshCutSpec cut_spec;
  cut_spec.planes.push_back(aster::makeMeshCutPlane({0.0f, 0.0f, 0.0f},
                                                    {1.0f, 0.0f, 0.0f}));
  const aster::MeshCutResult cut_result = aster::partitionMeshByCutPlanes(cut_box, cut_spec);
  assert(cut_result.fragments.size() == 2u);
  assert(!cut_result.seam_points.empty());
  bool has_negative_cut = false;
  bool has_positive_cut = false;
  for (const aster::MeshCutFragment &fragment : cut_result.fragments) {
    assert(!fragment.mesh.vertices.empty());
    assert(!fragment.mesh.indices.empty());
    assert(fragment.mesh.indices.size() % 3u == 0u);
    assert(fragment.surface_area > 0.0f);
    assert(fragment.cut_plane_mask == 1u);
    has_negative_cut =
        has_negative_cut || fragment.side == aster::MeshCutFragmentSide::Negative;
    has_positive_cut =
        has_positive_cut || fragment.side == aster::MeshCutFragmentSide::Positive;
    assert(fragment.bounds_min.x >= -0.81f);
    assert(fragment.bounds_max.x <= 0.81f);
  }
  assert(has_negative_cut);
  assert(has_positive_cut);

  const aster::MeshCutImpactSpec impact_cut_spec{.volume = cut_volume,
                                                 .impact_point = {0.14f, 0.04f, 0.58f},
                                                 .impact_normal = {0.0f, 0.0f, 1.0f},
                                                 .seed = 23u,
                                                 .plane_count = 4};
  const aster::MeshCutResult impact_cut = aster::buildImpactMeshCut(impact_cut_spec);
  assert(impact_cut.fragments.size() >= 3u);
  assert(!impact_cut.seam_points.empty());
  for (const aster::MeshCutFragment &fragment : impact_cut.fragments) {
    assert(!fragment.mesh.vertices.empty());
    assert(!fragment.mesh.indices.empty());
    assert(fragment.mesh.indices.size() % 3u == 0u);
    assert(fragment.surface_area > 0.0f);
    assert(aster::length(fragment.impulse_direction) > 0.90f);
    assert(fragment.centroid.x >= -0.82f && fragment.centroid.x <= 0.82f);
    assert(fragment.centroid.y >= -0.52f && fragment.centroid.y <= 0.52f);
    assert(fragment.centroid.z >= -0.62f && fragment.centroid.z <= 0.62f);
  }

  const aster::CpuMesh mound = aster::makeMoundMesh();
  assert(!mound.vertices.empty());
  assert(!mound.indices.empty());
  const aster::MoundMeshSpec basin_spec{.radial_segments = 48,
                                        .rings = 12,
                                        .radius_x = 3.2f,
                                        .radius_z = 2.1f,
                                        .height = 0.42f,
                                        .pond_radius_x = 1.8f,
                                        .pond_radius_z = 1.0f,
                                        .pond_depth = 0.72f,
                                        .bank_width = 0.42f,
                                        .bank_height = 0.24f,
                                        .basin_floor_radius = 0.48f,
                                        .shore_depth_fraction = 0.12f};
  const aster::CpuMesh basin = aster::makeMoundMesh(basin_spec);
  float basin_floor_y = std::numeric_limits<float>::max();
  float shore_lip_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : basin.vertices) {
    const float dx = vertex.position.x / basin_spec.pond_radius_x;
    const float dz = vertex.position.z / basin_spec.pond_radius_z;
    const float pond_distance = std::sqrt(dx * dx + dz * dz);
    if (pond_distance < basin_spec.basin_floor_radius * 0.55f) {
      basin_floor_y = std::min(basin_floor_y, vertex.position.y);
    }
    if (pond_distance > 0.92f && pond_distance < 1.12f) {
      shore_lip_y = std::max(shore_lip_y, vertex.position.y);
    }
  }
  assert(shore_lip_y > basin_floor_y + basin_spec.pond_depth * 0.35f);

  const aster::CpuMesh pond = aster::makePondWaterMesh();
  assert(!pond.vertices.empty());
  assert(!pond.indices.empty());
  const aster::SubmergedBasinMeshSpec submerged_spec{.floor_depth = 0.42f,
                                                     .shore_depth = 0.09f,
                                                     .basin_floor_radius = 0.40f,
                                                     .bottom_noise = 0.0f};
  const aster::CpuMesh submerged_basin = aster::makeSubmergedBasinMesh(submerged_spec);
  assert(!submerged_basin.vertices.empty());
  assert(!submerged_basin.indices.empty());
  float submerged_floor_y = std::numeric_limits<float>::max();
  float submerged_shore_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : submerged_basin.vertices) {
    const float dx = vertex.position.x / submerged_spec.radius_x;
    const float dz = vertex.position.z / submerged_spec.radius_z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance < submerged_spec.basin_floor_radius * 0.50f) {
      submerged_floor_y = std::min(submerged_floor_y, vertex.position.y);
    }
    if (distance > 0.94f) {
      submerged_shore_y = std::max(submerged_shore_y, vertex.position.y);
    }
  }
  assert(submerged_shore_y > submerged_floor_y + 0.25f);
  const aster::CpuMesh spectral_water =
      aster::makeEllipticalWaterMesh({.radial_segments = 32,
                                      .rings = 6,
                                      .radius_x = 2.0f,
                                      .radius_z = 1.0f,
                                      .shoreline_inset = 0.08f,
                                      .spectrum = {.amplitude = 0.024f, .component_count = 10}});
  assert(!spectral_water.vertices.empty());
  assert(!spectral_water.indices.empty());
  const aster::Vec3 water_normal = aster::spectralWaterNormal(
      {0.25f, -0.15f}, {.amplitude = 0.020f, .dominant_wavelength = 0.8f, .component_count = 8});
  assert(aster::length(water_normal) > 0.99f);

  const aster::CpuMesh terrain_transition = aster::makeTerrainTransitionMesh();
  assert(!terrain_transition.vertices.empty());
  assert(!terrain_transition.indices.empty());
  bool transition_has_edge_skirt = false;
  for (const aster::Vertex &vertex : terrain_transition.vertices) {
    transition_has_edge_skirt = transition_has_edge_skirt || vertex.position.y < 0.0f;
  }
  assert(transition_has_edge_skirt);
  const aster::TerrainTransitionMeshSpec directional_transition_spec{
      .radial_segments = 32,
      .rings = 4,
      .inner_radius_x = 1.4f,
      .inner_radius_z = 1.0f,
      .outer_radius_x = 2.2f,
      .outer_radius_z = 2.1f,
      .outer_radius_z_negative = 1.42f,
      .outer_radius_z_positive = 2.1f,
      .outer_radius_x_negative = 2.75f,
      .outer_radius_x_positive = 2.2f,
      .edge_irregularity = 0.0f,
      .edge_skirt_depth = 0.0f};
  const aster::CpuMesh directional_transition =
      aster::makeTerrainTransitionMesh(directional_transition_spec);
  float directional_min_x = std::numeric_limits<float>::max();
  float directional_max_x = std::numeric_limits<float>::lowest();
  float directional_min_z = std::numeric_limits<float>::max();
  float directional_max_z = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : directional_transition.vertices) {
    directional_min_x = std::min(directional_min_x, vertex.position.x);
    directional_max_x = std::max(directional_max_x, vertex.position.x);
    directional_min_z = std::min(directional_min_z, vertex.position.z);
    directional_max_z = std::max(directional_max_z, vertex.position.z);
  }
  assert(-directional_min_x > directional_max_x + 0.45f);
  assert(-directional_min_x > 2.70f && -directional_min_x < 2.80f);
  assert(directional_max_x > 2.15f && directional_max_x < 2.25f);
  assert(directional_max_z > -directional_min_z + 0.55f);
  assert(directional_max_z > 2.05f && directional_max_z < 2.15f);
  assert(-directional_min_z > 1.37f && -directional_min_z < 1.47f);

  const aster::CpuMesh grass = aster::makeGrassTuftMesh();
  assert(!grass.vertices.empty());
  assert(!grass.indices.empty());
  const auto flat_grass_sampler = [](const aster::Vec2 position) {
    return aster::TerrainSurfaceSample{.valid = true,
                                       .height = 0.25f + position.x * 0.02f - position.y * 0.01f,
                                       .normal = {0.0f, 1.0f, 0.0f}};
  };
  aster::GrassFieldScatterSpec grass_field_spec{.min = {-1.0f, -0.75f},
                                                .max = {1.0f, 0.75f},
                                                .seed = 77u,
                                                .target_blades = 32,
                                                .candidate_multiplier = 1,
                                                .min_spacing = 0.0f,
                                                .surface_offset = 0.015f,
                                                .min_height = 0.20f,
                                                .max_height = 0.48f,
                                                .min_width = 0.012f,
                                                .max_width = 0.026f,
                                                .max_bend = 0.10f,
                                                .max_lean = 0.035f,
                                                .density_noise_contrast = 0.0f,
                                                .min_surface_normal_y = 0.0f,
                                                .preferred_surface_normal_y = 0.10f};
  const std::vector<aster::GrassBladeAnchor> anchors_a =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  const std::vector<aster::GrassBladeAnchor> anchors_b =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(anchors_a.size() == 32u);
  assert(anchors_a.size() == anchors_b.size());
  for (std::size_t i = 0; i < anchors_a.size(); ++i) {
    expectNear(anchors_a[i].root.x, anchors_b[i].root.x, 0.000001f);
    expectNear(anchors_a[i].root.y, anchors_b[i].root.y, 0.000001f);
    expectNear(anchors_a[i].root.z, anchors_b[i].root.z, 0.000001f);
  }
  grass_field_spec.seed = 78u;
  const std::vector<aster::GrassBladeAnchor> anchors_c =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(anchors_c.size() == anchors_a.size());
  bool saw_seed_variation = false;
  for (std::size_t i = 0; i < anchors_a.size(); ++i) {
    saw_seed_variation =
        saw_seed_variation || std::abs(anchors_a[i].root.x - anchors_c[i].root.x) > 0.0001f ||
        std::abs(anchors_a[i].root.z - anchors_c[i].root.z) > 0.0001f;
  }
  assert(saw_seed_variation);
  const aster::CpuMesh grass_field =
      aster::makeGrassFieldMesh(anchors_a, {.blade_segments = 3, .double_sided = true});
  assert(!grass_field.vertices.empty());
  assert(!grass_field.indices.empty());
  bool saw_root_ao = false;
  bool saw_tip_ao = false;
  for (const aster::Vertex &vertex : grass_field.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
    assert(aster::length(vertex.normal) > 0.50f);
    saw_root_ao = saw_root_ao || (vertex.uv.y <= 0.001f && vertex.ambient_occlusion < 0.82f);
    saw_tip_ao = saw_tip_ao || (vertex.uv.y >= 0.999f && vertex.ambient_occlusion > 0.88f);
  }
  assert(saw_root_ao);
  assert(saw_tip_ao);
  grass_field_spec.seed = 77u;
  grass_field_spec.target_blades = 12;
  grass_field_spec.candidate_multiplier = 4;
  grass_field_spec.accepts_position = [](const aster::Vec2 position) { return position.x < 0.0f; };
  const std::vector<aster::GrassBladeAnchor> accepted_side =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(!accepted_side.empty());
  for (const aster::GrassBladeAnchor &anchor : accepted_side) {
    assert(anchor.root.x < 0.0f);
  }
  grass_field_spec.accepts_position = {};
  grass_field_spec.min_surface_normal_y = 0.80f;
  grass_field_spec.preferred_surface_normal_y = 0.90f;
  const auto steep_sampler = [](const aster::Vec2 position) {
    (void)position;
    return aster::TerrainSurfaceSample{.valid = true, .height = 0.0f, .normal = {0.86f, 0.22f, 0.0f}};
  };
  assert(aster::scatterGrassFieldAnchors(grass_field_spec, steep_sampler).empty());
  const aster::GroundDetailScatterSpec detail_spec{.min = {-1.0f, -1.0f},
                                                   .max = {1.0f, 1.0f},
                                                   .seed = 91u,
                                                   .target_details = 24,
                                                   .candidate_multiplier = 2,
                                                   .min_spacing = 0.0f,
                                                   .surface_offset = 0.018f,
                                                   .min_radius = 0.035f,
                                                   .max_radius = 0.11f,
                                                   .twig_fraction = 0.26f,
                                                   .leaf_fraction = 0.34f,
                                                   .density_noise_contrast = 0.0f,
                                                   .min_surface_normal_y = 0.0f,
                                                   .preferred_surface_normal_y = 0.10f};
  const std::vector<aster::GroundDetailAnchor> details_a =
      aster::scatterGroundDetailAnchors(detail_spec, flat_grass_sampler);
  const std::vector<aster::GroundDetailAnchor> details_b =
      aster::scatterGroundDetailAnchors(detail_spec, flat_grass_sampler);
  assert(details_a.size() == 24u);
  assert(details_a.size() == details_b.size());
  bool saw_pebble = false;
  bool saw_litter = false;
  for (std::size_t i = 0; i < details_a.size(); ++i) {
    expectNear(details_a[i].position.x, details_b[i].position.x, 0.000001f);
    expectNear(details_a[i].position.y, details_b[i].position.y, 0.000001f);
    expectNear(details_a[i].position.z, details_b[i].position.z, 0.000001f);
    assert(details_a[i].radius >= detail_spec.min_radius);
    assert(details_a[i].radius <= detail_spec.max_radius);
    saw_pebble = saw_pebble || details_a[i].kind == aster::GroundDetailKind::Pebble;
    saw_litter = saw_litter || details_a[i].kind != aster::GroundDetailKind::Pebble;
  }
  assert(saw_pebble);
  assert(saw_litter);
  const aster::CpuMesh ground_detail_mesh = aster::makeGroundDetailMesh(details_a);
  assert(!ground_detail_mesh.vertices.empty());
  assert(!ground_detail_mesh.indices.empty());
  for (const aster::Vertex &vertex : ground_detail_mesh.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(aster::length(vertex.normal) > 0.50f);
  }
  const aster::CpuMesh fish = aster::makeFishMesh();
  assert(!fish.vertices.empty());
  assert(!fish.indices.empty());
  const aster::CpuMesh predator = aster::makeAmphibiousPredatorMesh();
  assert(!predator.vertices.empty());
  assert(!predator.indices.empty());
  const aster::CpuMesh skitter = aster::makeCaveSkitterMesh();
  assert(!skitter.vertices.empty());
  assert(!skitter.indices.empty());
  assert(skitter.indices.size() % 3u == 0u);
  bool saw_eye_uv = false;
  float min_x = std::numeric_limits<float>::infinity();
  float max_x = -std::numeric_limits<float>::infinity();
  float min_z = std::numeric_limits<float>::infinity();
  float max_z = -std::numeric_limits<float>::infinity();
  for (const aster::Vertex &vertex : skitter.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(aster::length(vertex.normal) > 0.50f);
    saw_eye_uv = saw_eye_uv || (vertex.uv.x > 0.90f && vertex.uv.y < 0.28f);
    min_x = std::min(min_x, vertex.position.x);
    max_x = std::max(max_x, vertex.position.x);
    min_z = std::min(min_z, vertex.position.z);
    max_z = std::max(max_z, vertex.position.z);
  }
  assert(saw_eye_uv);
  assert(max_x - min_x > 0.9f);
  assert(max_z - min_z > 0.5f);
  const aster::CpuMesh broad_leaf = aster::makeBroadLeafPlantMesh();
  assert(!broad_leaf.vertices.empty());
  assert(!broad_leaf.indices.empty());
  const aster::CpuMesh tree_trunk = aster::makeClimbableTreeTrunkMesh();
  assert(!tree_trunk.vertices.empty());
  assert(!tree_trunk.indices.empty());
  const aster::CpuMesh tree_canopy = aster::makeTreeCanopyMesh();
  assert(!tree_canopy.vertices.empty());
  assert(!tree_canopy.indices.empty());
  const aster::CpuMesh chest = aster::makeTreasureChestMesh();
  assert(!chest.vertices.empty());
  assert(!chest.indices.empty());
  const aster::CpuMesh sentinel = aster::makeSignalSentinelMesh();
  assert(!sentinel.vertices.empty());
  assert(!sentinel.indices.empty());
  const aster::CpuMesh underpass = aster::makeGothicUnderpassMesh();
  assert(!underpass.vertices.empty());
  assert(!underpass.indices.empty());
  const aster::GothicUnderpassMeshSpec underpass_spec;
  const std::vector<aster::ArchitecturalCollisionBox> underpass_collision =
      aster::makeGothicUnderpassCollisionBoxes(underpass_spec);
  assert(underpass_collision.size() == 3u);
  const auto underpassContains = [&](const aster::Vec3 point) {
    for (const aster::ArchitecturalCollisionBox &box : underpass_collision) {
      if (std::abs(point.x - box.center.x) <= box.half_extents.x &&
          std::abs(point.y - box.center.y) <= box.half_extents.y &&
          std::abs(point.z - box.center.z) <= box.half_extents.z) {
        return true;
      }
    }
    return false;
  };
  assert(!underpassContains({0.0f, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(
      underpassContains({underpass_spec.half_width, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(underpassContains({0.0f, underpass_spec.passage_height + 0.20f, 0.0f}));

  const aster::CpuMesh path = aster::makePathRibbonMesh();
  assert(!path.vertices.empty());
  assert(!path.indices.empty());
  const aster::CpuMesh tapered_path = aster::makePathRibbonMesh({.segments = 8,
                                                                 .width = 0.8f,
                                                                 .width_variation = 0.0f,
                                                                 .crown_height = 0.0f,
                                                                 .surface_noise = 0.0f,
                                                                 .endpoint_taper = 0.25f,
                                                                 .start = {-1.0f, 0.0f, 0.0f},
                                                                 .control = {-0.4f, 0.0f, 0.0f},
                                                                 .control_b = {0.4f, 0.0f, 0.0f},
                                                                 .end = {1.0f, 0.0f, 0.0f}});
  float tapered_start_min = std::numeric_limits<float>::max();
  float tapered_start_max = std::numeric_limits<float>::lowest();
  float tapered_mid_min = std::numeric_limits<float>::max();
  float tapered_mid_max = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : tapered_path.vertices) {
    if (std::abs(vertex.uv.y) < 0.0001f) {
      tapered_start_min = std::min(tapered_start_min, vertex.position.z);
      tapered_start_max = std::max(tapered_start_max, vertex.position.z);
    }
    if (std::abs(vertex.uv.y - 0.5f) < 0.0001f) {
      tapered_mid_min = std::min(tapered_mid_min, vertex.position.z);
      tapered_mid_max = std::max(tapered_mid_max, vertex.position.z);
    }
  }
  const float tapered_start_span = tapered_start_max - tapered_start_min;
  const float tapered_mid_span = tapered_mid_max - tapered_mid_min;
  assert(tapered_start_span > tapered_mid_span * 0.55f);
  assert(tapered_start_span < tapered_mid_span);
  const aster::CpuMesh path_shoulders =
      aster::makePathShoulderMesh({.path = {.segments = 8,
                                            .width = 0.8f,
                                            .endpoint_taper = 0.25f,
                                            .start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 0.0f},
                                            .control_b = {0.4f, 0.0f, 0.0f},
                                            .end = {1.0f, 0.0f, 0.0f}},
                                   .side_segments = 3,
                                   .shoulder_width = 0.35f,
                                   .shoulder_height = 0.12f});
  assert(!path_shoulders.vertices.empty());
  assert(!path_shoulders.indices.empty());
  float shoulder_min_y = std::numeric_limits<float>::max();
  float shoulder_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : path_shoulders.vertices) {
    shoulder_min_y = std::min(shoulder_min_y, vertex.position.y);
    shoulder_max_y = std::max(shoulder_max_y, vertex.position.y);
  }
  assert(shoulder_max_y > shoulder_min_y + 0.05f);
  aster::StrokePath stroke_path;
  stroke_path.points = {{{-0.5f, 0.0f}, 0.05f}, {{0.0f, 0.35f}, 0.04f}, {{0.5f, 0.0f}, 0.05f}};
  const aster::CpuMesh stroke = aster::makeStrokeRibbonMesh({{stroke_path}, 0.02f});
  assert(stroke.vertices.size() == 8u);
  assert(stroke.indices.size() == 12u);
  const aster::PathRibbonMeshSpec path_spec{.start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 1.0f},
                                            .control_b = {0.8f, 0.0f, -0.6f},
                                            .end = {1.0f, 0.0f, 0.0f}};
  const aster::Vec3 path_start = aster::evaluatePathRibbonCenter(path_spec, 0.0f);
  const aster::Vec3 path_end = aster::evaluatePathRibbonCenter(path_spec, 1.0f);
  assert(std::abs(path_start.x + 1.0f) < 0.001f);
  assert(std::abs(path_end.x - 1.0f) < 0.001f);
  assert(aster::length(aster::evaluatePathRibbonTangent(path_spec, 0.5f)) > 0.9f);
  const std::vector<aster::PathRibbonMeshSpec> route_segments = {{.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {-2.0f, 0.0f, 0.0f},
                                                                  .control = {-1.5f, 0.0f, 1.0f},
                                                                  .control_b = {-0.8f, 0.0f, 1.0f},
                                                                  .end = {0.0f, 0.0f, 0.6f}},
                                                                 {.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {0.0f, 0.0f, 0.6f},
                                                                  .control = {0.9f, 0.0f, 0.2f},
                                                                  .control_b = {1.4f, 0.0f, -0.5f},
                                                                  .end = {2.0f, 0.0f, 0.0f}}};
  const aster::CpuMesh route_path = aster::makePathRouteRibbonMesh({.segments = route_segments});
  assert(route_path.vertices.size() > tapered_path.vertices.size());
  assert(route_path.indices.size() > tapered_path.indices.size());
  aster::PathShoulderRouteMeshSpec route_shoulder_spec;
  for (const aster::PathRibbonMeshSpec &segment : route_segments) {
    route_shoulder_spec.segments.push_back({.path = segment, .side_segments = 2});
  }
  const aster::CpuMesh route_shoulders = aster::makePathRouteShoulderMesh(route_shoulder_spec);
  assert(!route_shoulders.vertices.empty());
  assert(!route_shoulders.indices.empty());

  const aster::TerrainHeightField terrain =
      aster::makeProceduralTerrain({.grid_size = 17, .square_size = 1.0f});
  const aster::TerrainSurfaceSample terrain_sample = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(terrain_sample.valid);
  assert(aster::length(terrain_sample.normal) > 0.99f);
  const aster::CpuMesh terrain_mesh = aster::makeTerrainMesh(terrain);
  assert(terrain_mesh.vertices.size() == 17u * 17u);
  assert(terrain_mesh.indices.size() == 16u * 16u * 6u);
  const auto assert_upward_winding = [](const aster::CpuMesh &mesh) {
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
      const aster::Vertex &a = mesh.vertices[mesh.indices[i]];
      const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
      const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
      assert(aster::cross(b.position - a.position, c.position - a.position).y > 0.0f);
    }
  };
  assert_upward_winding(terrain_mesh);
  aster::TerrainMeshBuildOptions smooth_terrain_options;
  smooth_terrain_options.subdivisions_per_square = 2;
  smooth_terrain_options.smooth_visual_surface = true;
  const aster::CpuMesh smooth_terrain_mesh =
      aster::makeTerrainMesh(terrain, smooth_terrain_options);
  assert(smooth_terrain_mesh.vertices.size() == 33u * 33u);
  assert(smooth_terrain_mesh.indices.size() == 32u * 32u * 6u);
  assert_upward_winding(smooth_terrain_mesh);

  aster::TerrainMeshBuildOptions clipped_terrain_options;
  clipped_terrain_options.clip_boxes.push_back({{-0.55f, -0.55f}, {0.55f, 0.55f}});
  const aster::CpuMesh clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, clipped_terrain_options);
  assert(clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
  aster::TerrainMeshBuildOptions oriented_clip_options;
  oriented_clip_options.clip_oriented_ellipses.push_back({.center = {0.0f, 0.0f},
                                                          .forward = {1.0f, 0.0f},
                                                          .radius = {1.2f, 1.8f},
                                                          .radius_forward_negative = 0.8f,
                                                          .radius_forward_positive = 2.4f});
  const aster::CpuMesh oriented_clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, oriented_clip_options);
  assert(oriented_clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
}

void testTerrainSculpting() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 25;
  terrain.square_size = 1.0f;
  terrain.origin = {-12.0f, -12.0f};
  terrain.heights.assign(static_cast<std::size_t>(terrain.grid_size * terrain.grid_size), 0.0f);

  aster::applyTerrainMountainBrush(terrain, {.seed = 11u,
                                             .center = {0.0f, 0.0f},
                                             .radius = {7.0f, 7.0f},
                                             .height = 3.0f,
                                             .shoulder_height = 0.4f,
                                             .surface_noise = 0.0f,
                                             .ridge_frequency = 0.12f,
                                             .inner_plateau = 0.12f});
  assert(aster::sampleTerrain(terrain, {0.0f, 0.0f}).height > 2.3f);
  assert(std::abs(aster::sampleTerrain(terrain, {11.0f, 11.0f}).height) < 0.001f);

  aster::deformTerrainAlongPath(terrain, {.path = {.samples = 24,
                                                   .start = {-6.0f, 0.5f, -4.0f},
                                                   .control = {-2.0f, 0.9f, -1.5f},
                                                   .control_b = {2.0f, 1.4f, 1.5f},
                                                   .end = {6.0f, 1.8f, 4.0f}},
                                          .half_width = 0.55f,
                                          .shoulder_width = 0.75f,
                                          .crown_height = 0.0f,
                                          .surface_noise = 0.0f,
                                          .seed = 17u});
  const aster::TerrainSurfaceSample path_mid = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(path_mid.valid);
  assert(path_mid.height > 0.9f);
  assert(path_mid.height < 1.7f);

  aster::TerrainHeightField route_terrain;
  route_terrain.grid_size = 25;
  route_terrain.square_size = 1.0f;
  route_terrain.origin = {-12.0f, -12.0f};
  route_terrain.heights.assign(
      static_cast<std::size_t>(route_terrain.grid_size * route_terrain.grid_size), 0.0f);
  aster::deformTerrainAlongRoute(route_terrain,
                                 {.route = {.segments = {{.samples = 16,
                                                          .start = {-8.0f, 0.4f, 5.0f},
                                                          .control = {-5.2f, 0.5f, 7.2f},
                                                          .control_b = {-2.7f, 0.6f, 6.4f},
                                                          .end = {0.0f, 0.7f, 5.0f}},
                                                         {.samples = 16,
                                                          .start = {0.0f, 0.7f, 5.0f},
                                                          .control = {2.8f, 0.8f, 3.1f},
                                                          .control_b = {5.2f, 0.9f, 3.4f},
                                                          .end = {8.0f, 1.0f, 5.0f}}}},
                                  .half_width = 0.5f,
                                  .shoulder_width = 0.6f,
                                  .crown_height = 0.0f,
                                  .surface_noise = 0.0f,
                                  .seed = 31u});
  const aster::TerrainSurfaceSample route_join = aster::sampleTerrain(route_terrain, {0.0f, 5.0f});
  assert(route_join.valid);
  assert(route_join.height > 0.62f);
  assert(std::abs(aster::sampleTerrain(route_terrain, {11.0f, -11.0f}).height) < 0.001f);

  aster::sculptTerrainPortalShelf(terrain, {.seed = 23u,
                                            .entrance = {2.0f, 1.25f, 2.0f},
                                            .inward = {0.0f, 0.0f, -1.0f},
                                            .floor_height = 1.25f,
                                            .half_width = 1.2f,
                                            .front_depth = 1.0f,
                                            .back_depth = 1.5f,
                                            .feather = 0.6f,
                                            .side_berm_height = 0.3f,
                                            .rear_cover_height = 0.4f,
                                            .surface_noise = 0.0f});
  const aster::TerrainSurfaceSample portal_floor = aster::sampleTerrain(terrain, {2.0f, 2.0f});
  assert(portal_floor.valid);
  assert(std::abs(portal_floor.height - 1.25f) < 0.08f);
}

void testCaveInteriorVolume() {
  const aster::CaveTunnelProfile tunnel{.seed = 7u,
                                        .start = {0.0f, 0.0f, 0.0f},
                                        .control = {0.2f, -0.5f, -3.0f},
                                        .control_b = {1.1f, -1.2f, -6.0f},
                                        .end = {0.4f, -1.6f, -9.0f},
                                        .length_segments = 32,
                                        .radial_segments = 12,
                                        .half_width = 1.0f,
                                        .wall_height = 1.5f,
                                        .floor_width = 1.1f,
                                        .floor_crown = 0.02f,
                                        .wall_noise = 0.0f,
                                        .visible_wall_start_t = 0.20f,
                                        .collision_start_t = 0.18f,
                                        .ore_start_t = 0.32f,
                                        .chest_t = 0.62f,
                                        .chamber_t = 0.64f,
                                        .chamber_falloff = 0.20f,
                                        .chamber_width_scale = 1.7f,
                                        .chamber_height_scale = 1.25f};
  const aster::CaveInteriorSample outside_under =
      aster::sampleCaveInteriorVolume(tunnel, {0.0f, -1.0f, -1.0f});
  assert(outside_under.interior < 0.001f);

  const aster::Vec3 inside_floor = aster::evaluateCaveTunnelCenter(tunnel, 0.58f);
  const aster::CaveInteriorSample inside =
      aster::sampleCaveInteriorVolume(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f});
  assert(inside.interior > 0.5f);
  assert(inside.depth > 0.0f);
  assert(inside.half_width > tunnel.half_width);

  const aster::CaveComplex complex =
      aster::buildCaveComplex({.tunnel = tunnel,
                               .portal = {.arch_segments = 8},
                               .ore = {.seed = 13u, .candidates = 16, .max_nodes = 4}});
  assert(!complex.tunnel_chunks.empty());
  assert(!complex.collision_mesh.vertices.empty());
  assert(!complex.collision_mesh.indices.empty());
  assert(!complex.features.empty());
  assert(complex.portal_mesh.vertices.size() > 18u);
  assert(countOpenIndexedEdges(complex.portal_mesh) == 0u);
  assert(countFacesOpposingVertexNormals(complex.portal_mesh) == 0u);
  assert(!complex.portal_blend_mesh.vertices.empty());
  assert(!complex.portal_blend_mesh.indices.empty());
  assert(!complex.portal_formation_mesh.vertices.empty());
  assert(!complex.portal_formation_mesh.indices.empty());
  assert(complex.portal_formation_mesh.vertices.size() > complex.portal_mesh.vertices.size());
  assert(!complex.portal_seal_mesh.vertices.empty());
  assert(!complex.portal_seal_mesh.indices.empty());
  assert(countFacesOpposingVertexNormals(complex.portal_seal_mesh) == 0u);
  assert(!complex.entrance_throat_mesh.vertices.empty());
  assert(!complex.entrance_throat_mesh.indices.empty());
  assert(countFacesOpposingVertexNormals(complex.entrance_throat_mesh) == 0u);
  for (const aster::CpuMesh &chunk : complex.tunnel_chunks) {
    assert(!chunk.vertices.empty());
    assert(!chunk.indices.empty());
    assert(countFacesOpposingVertexNormals(chunk) == 0u);
  }
  assert(countFacesOpposingVertexNormals(complex.collision_mesh) == 0u);
  assert(!complex.portal_floor_mesh.vertices.empty());
  assert(!complex.portal_floor_mesh.indices.empty());
  aster::Vec3 portal_floor_center{};
  for (const aster::Vertex &vertex : complex.portal_floor_mesh.vertices) {
    portal_floor_center = portal_floor_center + vertex.position;
    assert(vertex.normal.y > 0.20f);
  }
  portal_floor_center =
      portal_floor_center / static_cast<float>(complex.portal_floor_mesh.vertices.size());
  const aster::TerrainSurfaceSample portal_floor_support = aster::sampleMeshSupport(
      complex.portal_floor_mesh, {},
      aster::SurfaceSupportQuery{{portal_floor_center.x, portal_floor_center.z}}, 0.36f);
  assert(portal_floor_support.valid);
  assert(complex.chest_position.y < -0.4f);
  const aster::Vec3 chamber_center = aster::evaluateCaveTunnelCenter(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_tangent = aster::evaluateCaveTunnelTangent(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_side =
      aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, chamber_tangent));
  const aster::CaveTraversalConstraint side_constraint = aster::constrainCaveTraversalPosition(
      tunnel, chamber_center + chamber_side * 2.4f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(side_constraint.active);
  assert(aster::length(side_constraint.correction) > 0.0f);
  const aster::Vec3 end = aster::evaluateCaveTunnelCenter(tunnel, 1.0f);
  const aster::Vec3 end_tangent = aster::evaluateCaveTunnelTangent(tunnel, 1.0f);
  const aster::CaveTraversalConstraint end_constraint = aster::constrainCaveTraversalPosition(
      tunnel, end + end_tangent * 0.45f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(end_constraint.active);
  assert(aster::dot(end_constraint.corrected_position - end, end_tangent) < 0.0f);
  aster::CaveTunnelProfile streaming_tunnel = tunnel;
  streaming_tunnel.end_constraint_enabled = false;
  const aster::CaveTraversalConstraint streaming_end_constraint =
      aster::constrainCaveTraversalPosition(
          streaming_tunnel, end + end_tangent * 0.45f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(!streaming_end_constraint.active);
  const aster::CaveViewConstraint view_constraint =
      aster::constrainCaveViewSegment(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f},
                                      tunnel.start + aster::Vec3{0.0f, 1.0f, 1.2f},
                                      {.samples = 18,
                                       .minimum_radius = 0.65f,
                                       .interior_threshold = 0.045f,
                                       .backtrack_tolerance_t = 0.12f});
  assert(view_constraint.active);
  assert(view_constraint.radius > 0.0f);
  const std::vector<aster::CaveWallFixturePlacement> fixtures =
      aster::placeCaveWallFixtures(tunnel, {.start_t = 0.28f,
                                            .end_t = 0.70f,
                                            .target_spacing = 2.5f,
                                            .max_count = 3,
                                            .wall_side = -1.0f,
                                            .mount_height = 1.0f,
                                            .wall_inset = 0.12f});
  assert(!fixtures.empty());
  for (const aster::CaveWallFixturePlacement &fixture : fixtures) {
    const aster::CaveInteriorSample fixture_sample =
        aster::sampleCaveInteriorVolume(tunnel, fixture.position);
    assert(fixture_sample.tunnel_t >= 0.24f);
    assert(aster::length(fixture.normal) > 0.90f);
    const float lens_gap =
        aster::dot(fixture.lens_position - fixture.mount_position, fixture.normal);
    const float light_gap =
        aster::dot(fixture.light_position - fixture.lens_position, fixture.normal);
    assert(lens_gap > 0.050f);
    assert(light_gap > 0.120f);
    assert(aster::dot(fixture.light_position - fixture.mount_position, fixture.normal) > lens_gap);
  }
  const aster::CaveTerrainPortalCut portal_cut =
      aster::makeCaveTerrainPortalCut(tunnel, {.arch_segments = 8});
  assert(portal_cut.radius_forward_positive > portal_cut.radius_forward_negative);
  const aster::CaveTerrainCoverFit uncovered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel, [](const aster::Vec2) { return aster::CaveTerrainCoverSample{true, -10.0f}; },
      {.samples = 24,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(!uncovered_fit.cover_found);
  assert(uncovered_fit.tunnel.visible_wall_start_t == tunnel.visible_wall_start_t);
  const aster::CaveTerrainCoverFit covered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel,
      [](const aster::Vec2 position) {
        return aster::CaveTerrainCoverSample{true, position.y < -4.0f ? 10.0f : -10.0f};
      },
      {.samples = 48,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(covered_fit.cover_found);
  assert(covered_fit.tunnel.visible_wall_start_t > tunnel.visible_wall_start_t);
  assert(covered_fit.tunnel.collision_start_t == tunnel.collision_start_t);
  for (const aster::CaveOreNodePlacement &ore : complex.ore_nodes) {
    const aster::CaveInteriorSample ore_sample =
        aster::sampleCaveInteriorVolume(tunnel, ore.position);
    assert(ore_sample.tunnel_t >= tunnel.ore_start_t - 0.08f);
  }
  bool saw_ceiling_feature = false;
  bool saw_floor_feature = false;
  for (const aster::CaveFeaturePlacement &feature : complex.features) {
    const aster::CaveInteriorSample feature_sample =
        aster::sampleCaveInteriorVolume(tunnel, feature.position);
    assert(feature_sample.tunnel_t >= tunnel.visible_wall_start_t - 0.16f);
    assert(aster::length(feature.normal) > 0.90f);
    saw_ceiling_feature = saw_ceiling_feature || feature.kind == aster::CaveFeatureKind::Stalactite;
    saw_floor_feature = saw_floor_feature || feature.kind == aster::CaveFeatureKind::Stalagmite ||
                        feature.kind == aster::CaveFeatureKind::Column;
  }
  assert(saw_ceiling_feature);
  assert(saw_floor_feature);
}

void testVoxelCaveStreamingAndFixtureContracts() {
  aster::VoxelCaveSpec spec;
  spec.seed = 77u;
  spec.origin = {-8.0f, -8.0f, -48.0f};
  spec.cell_size = 0.40f;
  spec.chunk_cells = 8;
  spec.stream_radius = 1;
  spec.unload_radius = 2;
  spec.max_chunk_rebuilds_per_update = 0;
  spec.forced_rebuild_radius = 0;
  spec.structural_surface_mode = aster::VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  spec.authored_fixture_light_color = {1.0f, 0.14f, 0.06f};
  spec.procedural_fixture_light_color = {1.0f, 0.10f, 0.04f};
  spec.tunnels.push_back({{0.0f, 0.0f, -48.0f}, {0.0f, 0.0f, -58.0f}, 1.42f});
  spec.chambers.push_back({{0.0f, 0.1f, -54.0f}, {2.2f, 1.35f, 2.4f}});
  spec.procedural_fields.push_back({.enabled = true,
                                    .seed = 91u,
                                    .start = {0.0f, 0.0f, -58.0f},
                                    .forward = {0.0f, 0.0f, -1.0f},
                                    .up = {0.0f, 1.0f, 0.0f},
                                    .backtrack_distance = 1.5f,
                                    .tunnel_radius = 1.35f,
                                    .vertical_radius = 1.06f,
                                    .radius_variation = 0.08f,
                                    .wander_frequency = 0.05f,
                                    .side_wander = 0.55f,
                                    .vertical_wander = 0.18f,
                                    .chamber_spacing = 14.0f,
                                    .chamber_radius = 2.2f,
                                    .chamber_vertical_radius = 1.35f,
                                    .chamber_radius_variation = 0.18f,
                                    .chamber_jitter = 0.18f});
  spec.material_profiles = {{.material = aster::VoxelCaveMaterial::Rock,
                             .seed = 93u,
                             .display_name = "Rock",
                             .visual_material_id = "rock"}};

  aster::VoxelCaveState state;
  state.configure(spec);
  state.updateStreaming({0.0f, 0.0f, -52.0f}, 0.0f);
  const aster::VoxelChunkCoord first_center = state.chunkCoordFor({0.0f, 0.0f, -52.0f});
  bool saw_first_chunk = false;
  bool saw_collision_mesh = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    saw_first_chunk = saw_first_chunk || chunk.coord == first_center;
    saw_collision_mesh =
        saw_collision_mesh ||
        (chunk.collision_mesh != nullptr && !chunk.collision_mesh->vertices.empty() &&
         !chunk.collision_mesh->indices.empty() &&
         countFacesOpposingVertexNormals(*chunk.collision_mesh) == 0u);
    if (chunk.collision_mesh != nullptr && !chunk.collision_mesh->vertices.empty()) {
      assert(chunk.surface_stats.topology_invalid_indices == 0u);
      assert(chunk.surface_stats.topology_degenerate_triangles == 0u);
    }
    const float chunk_world_size = spec.cell_size * static_cast<float>(spec.chunk_cells);
    for (const aster::VoxelChunkRenderBatch &batch : chunk.batches) {
      if (batch.mesh == nullptr || batch.mesh->vertices.empty()) {
        continue;
      }
      aster::Vec3 min = batch.mesh->vertices.front().position;
      aster::Vec3 max = min;
      for (const aster::Vertex &vertex : batch.mesh->vertices) {
        min.x = std::min(min.x, vertex.position.x);
        min.y = std::min(min.y, vertex.position.y);
        min.z = std::min(min.z, vertex.position.z);
        max.x = std::max(max.x, vertex.position.x);
        max.y = std::max(max.y, vertex.position.y);
        max.z = std::max(max.z, vertex.position.z);
      }
      const aster::Vec3 mesh_center = (min + max) * 0.5f;
      assert(aster::length(mesh_center - chunk.center) <= chunk_world_size * 1.85f);
      assert(aster::length(mesh_center) > 8.0f);
    }
  }
  assert(saw_first_chunk);
  assert(saw_collision_mesh);

  const aster::VoxelEditResult immediate_edit =
      state.applyEdit({.center = {0.0f, 0.0f, -54.0f}, .radius = 0.72f},
                      aster::VoxelEditRebuildMode::ImmediateAffected);
  assert(immediate_edit.accepted);
  assert(!immediate_edit.affected_chunks.empty());
  assert(immediate_edit.rebuilt_chunks == static_cast<int>(immediate_edit.affected_chunks.size()));
  state.updateStreaming({0.0f, 0.0f, -52.0f}, 0.0f);
  bool edited_chunk_pending = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    for (const aster::VoxelChunkCoord affected : immediate_edit.affected_chunks) {
      if (chunk.coord == affected) {
        edited_chunk_pending = edited_chunk_pending || chunk.rebuild_pending;
      }
    }
  }
  assert(!edited_chunk_pending);

  const aster::VoxelCaveInteriorSample authored = state.sampleInterior({0.0f, 0.0f, -54.0f});
  assert(authored.valid);
  assert(authored.inside);
  assert(authored.interior > 0.65f);
  const aster::VoxelCaveProceduralFrame procedural_frame =
      aster::sampleVoxelCaveProceduralFrameAt(spec.procedural_fields.front(), 8.0f);
  assert(procedural_frame.valid);
  const aster::Vec3 procedural_probe = procedural_frame.center;
  const aster::VoxelCaveInteriorSample procedural = state.sampleInterior(procedural_probe);
  assert(procedural.valid);
  assert(procedural.procedural);

  const std::vector<aster::VoxelCaveFixturePlacement> path_fixtures =
      aster::placeVoxelCavePathFixtures(
          spec, {.start_distance = 2.0f,
                 .target_spacing = 3.0f,
                 .max_count = 4,
                 .side_mode = aster::VoxelCaveFixtureSideMode::FixedNegative,
                 .wall_inset = 0.12f,
                 .mount_height = 0.08f,
                 .normal_up_bias = -0.10f,
                 .lens_offset = 0.075f,
                 .light_offset = 0.18f});
  assert(!path_fixtures.empty());
  for (const aster::VoxelCaveFixturePlacement &fixture : path_fixtures) {
    assert(aster::dot(fixture.lens_position - fixture.mount_position, fixture.normal) > 0.050f);
    assert(aster::dot(fixture.light_position - fixture.lens_position, fixture.normal) > 0.120f);
    assert(state.densityAt(fixture.mount_position) <= 0.0f);
    assert(state.densityAt(fixture.lens_position) <= 0.0f);
  }

  const std::vector<aster::VoxelCaveFixturePlacement> procedural_fixtures =
      aster::placeVoxelCaveProceduralFixturesNear(
          spec, procedural_probe,
          {.target_spacing = 5.8f,
           .max_count = 4,
           .procedural_behind = 1,
           .procedural_ahead = 2,
           .side_mode = aster::VoxelCaveFixtureSideMode::Alternating,
           .wall_inset = 0.12f,
           .mount_height = 0.08f,
           .normal_up_bias = -0.10f,
           .lens_offset = 0.075f,
           .light_offset = 0.18f});
  assert(procedural_fixtures.size() >= 2u);
  const std::vector<aster::VoxelCaveFixtureLightSample> ranked =
      aster::rankVoxelCaveFixtureLights(procedural_probe, procedural, procedural_fixtures,
                                        {.progress_window = 5.8f,
                                         .minimum_progress_gain = 0.70f,
                                         .distance_score_weight = 0.55f,
                                         .progress_score_weight = 1.30f});
  assert(!ranked.empty());
  assert(ranked.front().weight > 0.0f);
  assert(aster::length(ranked.front().placement.light_position -
                       ranked.front().placement.lens_position) > 0.12f);
  assert(state.densityAt(ranked.front().placement.mount_position) <= 0.0f);
  assert(state.densityAt(ranked.front().placement.lens_position) <= 0.0f);

  state.updateStreaming({0.0f, 0.0f, -76.0f}, 0.1f);
  const aster::VoxelChunkCoord advanced_center = state.chunkCoordFor({0.0f, 0.0f, -76.0f});
  bool saw_advanced_chunk = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    saw_advanced_chunk = saw_advanced_chunk || chunk.coord == advanced_center;
  }
  assert(!(advanced_center == first_center));
  assert(saw_advanced_chunk);

  aster::VoxelCaveSpec plug_spec;
  plug_spec.seed = 171u;
  plug_spec.origin = {-4.0f, -4.0f, -18.0f};
  plug_spec.cell_size = 0.40f;
  plug_spec.chunk_cells = 8;
  plug_spec.stream_radius = 1;
  plug_spec.unload_radius = 2;
  plug_spec.max_chunk_rebuilds_per_update = 0;
  plug_spec.structural_surface_mode =
      aster::VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  plug_spec.tunnels.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -12.0f}, 1.10f});
  plug_spec.procedural_fields.push_back({.enabled = true,
                                         .seed = 173u,
                                         .start = {0.0f, 0.0f, -12.0f},
                                         .forward = {0.0f, 0.0f, -1.0f},
                                         .up = {0.0f, 1.0f, 0.0f},
                                         .tunnel_radius = 1.08f,
                                         .vertical_radius = 0.92f,
                                         .radius_variation = 0.0f,
                                         .side_wander = 0.0f,
                                         .vertical_wander = 0.0f,
                                         .chamber_spacing = 12.0f,
                                         .chamber_radius = 1.8f,
                                         .chamber_vertical_radius = 1.2f,
                                         .chamber_radius_variation = 0.0f,
                                         .chamber_jitter = 0.0f});
  plug_spec.solid_plugs.push_back({.enabled = true,
                                   .seed = 179u,
                                   .center = {0.0f, 0.0f, -5.0f},
                                   .forward = {0.0f, 0.0f, -1.0f},
                                   .up = {0.0f, 1.0f, 0.0f},
                                   .material = aster::VoxelCaveMaterial::Iron,
                                   .radius = 1.22f,
                                   .vertical_radius = 1.04f,
                                   .half_length = 1.35f,
                                   .edge_feather = 0.14f,
                                   .surface_noise = 0.0f});
  plug_spec.procedural_solid_plug_fields.push_back(
      {.enabled = true,
       .seed = 181u,
       .path_field_index = 0,
       .start = {0.0f, 0.0f, -12.0f},
       .forward = {0.0f, 0.0f, -1.0f},
       .up = {0.0f, 1.0f, 0.0f},
       .material = aster::VoxelCaveMaterial::Iron,
       .first_distance = 4.0f,
       .spacing = 8.0f,
       .radius_scale = 1.16f,
       .vertical_radius_scale = 1.10f,
       .half_length = 1.20f,
       .edge_feather = 0.12f,
       .surface_noise = 0.0f,
       .radius_variation = 0.0f,
       .length_variation = 0.0f,
       .lateral_jitter = 0.0f,
       .vertical_jitter = 0.0f});
  plug_spec.material_profiles = {{.material = aster::VoxelCaveMaterial::Rock,
                                  .seed = 183u,
                                  .display_name = "Rock",
                                  .visual_material_id = "rock",
                                  .resource_item_id = "stone"},
                                 {.material = aster::VoxelCaveMaterial::Iron,
                                  .seed = 191u,
                                  .display_name = "Ironstone",
                                  .visual_material_id = "ironstone",
                                  .hardness = 6.0f,
                                  .resource_item_id = "iron_ore",
                                  .vein_frequency = 0.0f,
                                  .vein_radius = 0.0f,
                                  .surface_relief = 0.20f,
                                  .surface_coverage = 0.84f}};
  aster::VoxelCaveState plug_state;
  plug_state.configure(plug_spec);
  assert(plug_state.densityAt({0.0f, 0.0f, -5.0f}) > 0.0f);
  assert(plug_state.materialAt({0.0f, 0.0f, -5.0f}) == aster::VoxelCaveMaterial::Iron);
  const aster::VoxelCaveHit plug_hit =
      plug_state.raycast({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, -1.0f}, 6.0f);
  assert(plug_hit.hit);
  assert(plug_hit.material == aster::VoxelCaveMaterial::Iron);
  const aster::VoxelCaveProceduralFrame plug_frame =
      aster::sampleVoxelCaveProceduralFrameAt(plug_spec.procedural_fields.front(), 4.0f);
  assert(plug_frame.valid);
  assert(plug_state.densityAt(plug_frame.center) > 0.0f);
  assert(plug_state.materialAt(plug_frame.center) == aster::VoxelCaveMaterial::Iron);
  plug_state.updateStreaming({0.0f, 0.0f, -5.0f}, 0.0f);
  bool saw_ironstone_batch = false;
  for (const aster::VoxelChunkSnapshot &chunk : plug_state.activeChunks()) {
    for (const aster::VoxelMaterialSurfaceStats &material : chunk.surface_stats.materials) {
      saw_ironstone_batch =
          saw_ironstone_batch || material.material == aster::VoxelCaveMaterial::Iron;
    }
  }
  assert(saw_ironstone_batch);
  const aster::VoxelEditResult plug_edit =
      plug_state.applyEdit({.center = {0.0f, 0.0f, -5.0f}, .radius = 1.45f},
                           aster::VoxelEditRebuildMode::ImmediateAffected);
  assert(plug_edit.accepted);
  assert(plug_state.densityAt({0.0f, 0.0f, -5.0f}) <= 0.0f);
}

void testVoxelCaveStreamingPublishesReadySnapshotsOnly() {
  aster::VoxelCaveSpec spec;
  spec.seed = 251u;
  spec.origin = {-4.0f, -4.0f, -12.0f};
  spec.cell_size = 0.40f;
  spec.chunk_cells = 8;
  spec.stream_radius = 1;
  spec.unload_radius = 2;
  spec.max_chunk_rebuilds_per_update = 1;
  spec.forced_rebuild_radius = 0;
  spec.structural_surface_mode = aster::VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  spec.tunnels.push_back({{0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, -14.0f}, 1.08f});
  spec.procedural_fields.push_back({.enabled = true,
                                    .seed = 257u,
                                    .start = {0.0f, 0.0f, -14.0f},
                                    .forward = {0.0f, 0.0f, -1.0f},
                                    .up = {0.0f, 1.0f, 0.0f},
                                    .tunnel_radius = 1.04f,
                                    .vertical_radius = 0.90f,
                                    .radius_variation = 0.0f,
                                    .side_wander = 0.0f,
                                    .vertical_wander = 0.0f,
                                    .chamber_spacing = 12.0f,
                                    .chamber_radius = 1.8f,
                                    .chamber_vertical_radius = 1.2f,
                                    .chamber_radius_variation = 0.0f,
                                    .chamber_jitter = 0.0f});
  spec.material_profiles = {{.material = aster::VoxelCaveMaterial::Rock,
                             .seed = 263u,
                             .display_name = "Rock",
                             .visual_material_id = "rock"}};

  aster::VoxelCaveState state;
  state.configure(spec);
  state.updateStreaming({0.0f, 0.0f, -6.0f}, 0.0f);
  assert(!state.activeChunks().empty());
  assert(state.activeChunks().size() < 27u);
  bool saw_coarse_proxy = false;
  bool saw_full_chunk = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    assert(chunk.mesh_generation > 0u);
    if (chunk.coarse_proxy) {
      saw_coarse_proxy = true;
      assert(chunk.lifecycle == aster::VoxelChunkLifecycle::CoarseVisible);
      assert(chunk.rebuild_pending);
      assert(chunk.collision_mesh == nullptr);
    } else {
      saw_full_chunk = true;
      assert(chunk.lifecycle == aster::VoxelChunkLifecycle::FullPublished);
      assert(!chunk.rebuild_pending);
    }
  }
  assert(saw_coarse_proxy);
  assert(saw_full_chunk);

  const aster::Vec3 probe_position{7.4f, 0.0f, -6.0f};
  const aster::VoxelChunkCoord probe_coord = state.chunkCoordFor(probe_position);
  aster::VoxelCaveUpdateOptions probe_options;
  probe_options.visibility_probes.push_back(probe_position);
  probe_options.viewer_velocity = {5.0f, 0.0f, 0.0f};
  state.updateStreaming({0.0f, 0.0f, -6.0f}, 1.0f / 60.0f, probe_options);
  bool saw_probe_chunk = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    saw_probe_chunk = saw_probe_chunk || chunk.coord == probe_coord;
  }
  assert(saw_probe_chunk);
  assert(state.lastUpdateStats().visibility_probe_chunks > 0u);
}

void testVoxelCaveDeferredEditRetainsPublishedCollision() {
  aster::VoxelCaveSpec spec;
  spec.seed = 271u;
  spec.origin = {-4.0f, -4.0f, -12.0f};
  spec.cell_size = 0.40f;
  spec.chunk_cells = 8;
  spec.stream_radius = 1;
  spec.unload_radius = 2;
  spec.max_chunk_rebuilds_per_update = 0;
  spec.forced_rebuild_radius = 0;
  spec.structural_surface_mode = aster::VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  spec.tunnels.push_back({{0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, -14.0f}, 1.08f});
  spec.material_profiles = {{.material = aster::VoxelCaveMaterial::Rock,
                             .seed = 277u,
                             .display_name = "Rock",
                             .visual_material_id = "rock"}};

  aster::VoxelCaveState state;
  state.configure(spec);
  const aster::Vec3 viewer{0.0f, 0.0f, -6.0f};
  state.updateStreaming(viewer, 0.0f);
  aster::VoxelCaveUpdateOptions warmup;
  warmup.override_rebuild_budget = true;
  warmup.rebuild_budget = aster::voxelCaveRebuildItemBudget(8u);
  for (int i = 0; i < 3; ++i) {
    state.updateStreaming(viewer, 0.0f, warmup);
  }
  const aster::VoxelChunkCoord viewer_coord = state.chunkCoordFor(viewer);
  const aster::VoxelChunkSnapshot *target = nullptr;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    if (!(chunk.coord == viewer_coord) && !chunk.coarse_proxy && !chunk.rebuild_pending &&
        chunk.collision_mesh != nullptr &&
        !chunk.collision_mesh->vertices.empty() && !chunk.collision_mesh->indices.empty()) {
      target = &chunk;
      break;
    }
  }
  if (target == nullptr) {
    throw std::runtime_error("Voxel cave test requires a full published neighbor chunk.");
  }
  const aster::VoxelChunkCoord target_coord = target->coord;
  const std::uint64_t before_generation = target->mesh_generation;
  const std::shared_ptr<const aster::CpuMesh> before_collision = target->collision_mesh;

  const aster::VoxelEditResult deferred =
      state.applyEdit({.center = target->center, .radius = 0.72f},
                      aster::VoxelEditRebuildMode::Deferred);
  assert(deferred.accepted);
  assert(deferred.rebuilt_chunks == 0);

  aster::VoxelCaveUpdateOptions no_work;
  no_work.override_rebuild_budget = true;
  no_work.rebuild_budget.max_seconds = 0.5;
  state.updateStreaming(viewer, 1.0f / 60.0f, no_work);
  assert(state.lastUpdateStats().rebuilt_chunks == 0u);
  bool retained_dirty_snapshot = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    if (chunk.coord == target_coord) {
      retained_dirty_snapshot = chunk.rebuild_pending && !chunk.collision_dirty &&
                                chunk.mesh_generation == before_generation &&
                                chunk.collision_mesh == before_collision;
    }
  }
  assert(retained_dirty_snapshot);

  aster::VoxelCaveUpdateOptions rebuild_one;
  rebuild_one.override_rebuild_budget = true;
  rebuild_one.rebuild_budget = aster::defaultVoxelCaveInteractiveRebuildBudget();
  state.updateStreaming(viewer, 1.0f / 60.0f, rebuild_one);
  assert(state.lastUpdateStats().rebuilt_chunks >= 1u);
  bool published_replacement = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.changedChunks()) {
    published_replacement =
        published_replacement || (chunk.coord == target_coord && !chunk.rebuild_pending &&
                                  chunk.mesh_generation > before_generation &&
                                  chunk.collision_dirty);
  }
  assert(published_replacement);
}

void testMeshModelingTopologyValidation() {
  aster::CpuMesh box = aster::makeBox();
  const aster::MeshTopologyReport box_report = aster::validateMeshTopology(box);
  assert(box_report.indexable());
  assert(box_report.renderable());
  assert(box_report.invalid_indices == 0u);
  assert(box_report.degenerate_triangles == 0u);
  assert(box_report.bounds.valid);

  aster::rebuildAngleWeightedNormals(box);
  for (const aster::Vertex &vertex : box.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
  }

  aster::CpuMesh invalid = aster::makePlane(2.0f);
  invalid.indices.front() = 99u;
  const aster::MeshTopologyReport invalid_report = aster::validateMeshTopology(invalid);
  assert(!invalid_report.indexable());
  assert(invalid_report.invalid_indices == 1u);

  aster::CpuMesh degenerate;
  degenerate.vertices = {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                         {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                         {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}};
  degenerate.indices = {0u, 1u, 2u};
  const aster::MeshTopologyReport degenerate_report = aster::validateMeshTopology(degenerate);
  assert(degenerate_report.degenerate_triangles == 1u);

  aster::CpuMesh quad_mesh;
  const aster::Vertex a{{-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
  const aster::Vertex b{{1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}};
  const aster::Vertex c{{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}};
  const aster::Vertex d{{-1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}};
  aster::appendOrientedQuad(quad_mesh, a, d, c, b, {0.0f, 1.0f, 0.0f});
  assert(quad_mesh.indices.size() == 6u);
  assert(countFacesOpposingVertexNormals(quad_mesh) == 0u);
}

void testImplicitSurfaceExtractionBuildsChamber() {
  const aster::ImplicitSurfaceField chamber =
      aster::makeEllipsoidField({0.0f, 0.0f, 0.0f}, {1.05f, 0.74f, 0.92f});
  const aster::SurfaceExtractionResult surface = aster::extractImplicitSurface(
      {.origin = {-1.6f, -1.6f, -1.6f},
       .cell_size = 0.20f,
       .cell_count = {16, 16, 16},
       .settings = {.emit_collision_mesh = true,
                    .emit_channel_meshes = true,
                    .capture_faces = true},
       .density = [chamber](const aster::Vec3 point) { return chamber.sample(point); },
       .channel = [](const aster::SurfaceFace &face, const float) {
         return aster::SurfaceChannelSample{face.centroid.x < 0.0f ? 3u : 7u, 1.0f};
       }});

  assert(surface.stats.active_cells > 0u);
  assert(surface.stats.faces > 0u);
  assert(!surface.collision_mesh.vertices.empty());
  assert(!surface.collision_mesh.indices.empty());
  assert(surface.collision_mesh.indices.size() % 3u == 0u);
  assert(surface.faces.size() == surface.stats.faces);
  assert(surface.mesh_batches.size() == 2u);
  assert(surface.stats.material_batches == 2u);
  assert(surface.stats.topology.invalid_indices == 0u);
  assert(surface.stats.topology.degenerate_triangles == 0u);
  assert(surface.stats.topology.open_edges == 0u);
  assert(surface.stats.topology.non_manifold_edges == 0u);
  assert(countFacesOpposingVertexNormals(surface.collision_mesh) == 0u);
  std::size_t batch_triangles = 0u;
  for (const aster::SurfaceMeshBatch &batch : surface.mesh_batches) {
    assert(!batch.mesh.vertices.empty());
    assert(!batch.mesh.indices.empty());
    batch_triangles += batch.mesh.indices.size() / 3u;
  }
  assert(batch_triangles == surface.collision_mesh.indices.size() / 3u);
}

void testImplicitSurfaceAdjacentChunksShareBoundary() {
  const aster::ImplicitSurfaceField tunnel =
      aster::makeCapsuleField({-1.4f, 0.0f, 0.0f}, {1.4f, 0.0f, 0.0f}, 0.62f);
  const auto extract_chunk = [&](const aster::Vec3 origin) {
    return aster::extractImplicitSurface(
        {.origin = origin,
         .cell_size = 0.20f,
         .cell_count = {8, 10, 10},
         .settings = {.emit_collision_mesh = true, .emit_channel_meshes = false},
         .density = [tunnel](const aster::Vec3 point) { return tunnel.sample(point); }});
  };

  const aster::SurfaceExtractionResult left = extract_chunk({-1.6f, -1.0f, -1.0f});
  const aster::SurfaceExtractionResult right = extract_chunk({0.0f, -1.0f, -1.0f});
  assert(!left.collision_mesh.vertices.empty());
  assert(!right.collision_mesh.vertices.empty());

  const aster::SurfaceBoundarySignature left_boundary =
      aster::collectSurfaceBoundary(left.collision_mesh, aster::SurfaceBoundaryAxis::X, 0.0f,
                                    0.101f);
  const aster::SurfaceBoundarySignature right_boundary =
      aster::collectSurfaceBoundary(right.collision_mesh, aster::SurfaceBoundaryAxis::X, 0.0f,
                                    0.101f);
  const aster::SurfaceBoundaryCompatibilityReport boundary_report =
      aster::compareSurfaceBoundaries(left_boundary, right_boundary, 0.001f);
  assert(left_boundary.triangles_touching > 0u);
  assert(right_boundary.triangles_touching > 0u);
  assert(boundary_report.compatible());
}

void testMeshProcessingPipeline() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  for (aster::Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());

  aster::MeshDiagnostics diagnostics;
  const aster::CpuMesh prepared = aster::prepareMeshForRendering(std::move(mesh), {}, &diagnostics);
  assert(prepared.indices.size() == 6u);
  assert(diagnostics.degenerate_triangles == 1u);
  assert(diagnostics.invalid_normals == 4u);
  assert(diagnostics.generated_tangents == 6u);
  for (const aster::Vertex &vertex : prepared.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    assert(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
  }
}

void testMeshProcessingRejectsInvalidIndices() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  mesh.indices.front() = 99u;

  bool rejected = false;
  try {
    (void)aster::prepareMeshForRendering(std::move(mesh));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
}

void testMeshDrapingRaisesEmbeddedVertices() {
  aster::CpuMesh path = aster::makePathRibbonMesh({.segments = 3,
                                                   .width = 0.3f,
                                                   .crown_height = 0.0f,
                                                   .surface_noise = 0.0f,
                                                   .start = {-1.0f, -0.2f, 0.0f},
                                                   .control = {-0.4f, -0.2f, 0.0f},
                                                   .control_b = {0.4f, -0.2f, 0.0f},
                                                   .end = {1.0f, -0.2f, 0.0f}});
  const aster::CpuMesh draped = aster::drapeMeshToSurface(
      std::move(path), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.35f}; },
      {.surface_offset = 0.02f, .raise_only = true});
  for (const aster::Vertex &vertex : draped.vertices) {
    assert(vertex.position.y >= 0.369f);
  }

  aster::CpuMesh raised_path = aster::makePathRibbonMesh({.segments = 3,
                                                          .width = 0.3f,
                                                          .crown_height = 0.0f,
                                                          .surface_noise = 0.0f,
                                                          .start = {-1.0f, 0.8f, 0.0f},
                                                          .control = {-0.4f, 0.8f, 0.0f},
                                                          .control_b = {0.4f, 0.8f, 0.0f},
                                                          .end = {1.0f, 0.8f, 0.0f}});
  const aster::CpuMesh projected = aster::drapeMeshToSurface(
      std::move(raised_path),
      [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.20f}; },
      {.surface_offset = 0.01f, .raise_only = false});
  for (const aster::Vertex &vertex : projected.vertices) {
    expectNear(vertex.position.y, 0.21f, 0.0001f);
  }

  aster::CpuMesh relief = aster::makePathShoulderMesh({.path = {.segments = 4,
                                                                .width = 0.4f,
                                                                .start = {-1.0f, 0.0f, 0.0f},
                                                                .control = {-0.4f, 0.0f, 0.0f},
                                                                .control_b = {0.4f, 0.0f, 0.0f},
                                                                .end = {1.0f, 0.0f, 0.0f}},
                                                       .shoulder_height = 0.16f});
  const aster::CpuMesh preserved = aster::drapeMeshToSurface(
      std::move(relief), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.45f}; },
      {.surface_offset = 0.02f,
       .raise_only = false,
       .preserve_vertical_offset = true,
       .reference_y = 0.0f});
  float preserved_min_y = std::numeric_limits<float>::max();
  float preserved_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : preserved.vertices) {
    preserved_min_y = std::min(preserved_min_y, vertex.position.y);
    preserved_max_y = std::max(preserved_max_y, vertex.position.y);
  }
  assert(preserved_max_y > preserved_min_y + 0.05f);
}

void testSceneAssetNormalTangentImport() {
  const std::filesystem::path sample = writeGeneratedNormalTangentFixture();
  assert(std::filesystem::exists(sample));

  const aster::SceneAsset asset =
      aster::importSceneAsset(sample, {{}, true, 1.0f, aster::AssetOriginPolicy::CenterOnGround});
  assert(asset.material_slots.size() == 2u);
  assert(asset.mesh_chunks.size() == 1u);
  assert(asset.mesh_chunks.front().material_slot == 1u);
  assert(asset.material_slots[1].has_base_color_texture);
  assert(asset.material_slots[1].has_metallic_roughness_texture);
  assert(asset.material_slots[1].has_normal_texture);
  assert(asset.material_slots[1].has_occlusion_texture);
  assert(asset.material_slots[1].material.double_sided);
  assert(asset.material_slots[1].material.alpha_mode == aster::MaterialAlphaMode::Masked);
  assert(aster::classifyMaterialRenderQueue(asset.material_slots[1].material) ==
         aster::MaterialRenderQueue::Masked);
  assert(!asset.mesh_chunks.front().mesh.vertices.empty());
  assert(asset.mesh_chunks.front().mesh.indices.size() == 6u);
  assert(asset.mesh_chunks.front().diagnostics.generated_tangents == 6u);

  float min_y = std::numeric_limits<float>::max();
  float max_radius = 0.0f;
  for (const aster::Vertex &vertex : asset.mesh_chunks.front().mesh.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    min_y = std::min(min_y, vertex.position.y);
    max_radius = std::max(max_radius, aster::length({vertex.position.x, 0.0f, vertex.position.z}));
  }
  expectNear(min_y, 0.0f, 0.001f);
  assert(max_radius > 0.1f);
  std::filesystem::remove_all(sample.parent_path());
}

void testBrushLevelMeshBuild() {
  const aster::CpuMesh mesh = aster::buildBrushLevelMesh(
      {
          {{0.0f, 1.0f, 0.0f}, {1.2f, 1.0f, 0.2f}, {}, aster::BrushVolume::Solid, 0, 0.08f},
          {{0.0f, 0.6f, 0.0f}, {0.3f, 0.6f, 0.4f}, {}, aster::BrushVolume::Air, 1, 0.0f},
      },
      {.uv_scale = 0.5f,
       .edge_softening = 0.04f,
       .noise_frequency = 0.5f,
       .noise_strength = 0.01f});

  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() > 36u);

  bool found_lower_door_triangle = false;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const aster::Vertex &a = mesh.vertices[mesh.indices[i + 0u]];
    const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.22f && centroid.y < 0.48f && std::abs(centroid.z) < 0.18f) {
      found_lower_door_triangle = true;
      break;
    }
  }
  assert(!found_lower_door_triangle);
}

void testArchitecturalMeshBuild() {
  const aster::CpuMesh gatehouse = aster::makeGatehouseMesh({.wall_half_width = 1.2f,
                                                             .wall_height = 1.8f,
                                                             .tower_half_width = 0.34f,
                                                             .tower_height = 2.2f,
                                                             .depth = 0.55f,
                                                             .door_half_width = 0.32f,
                                                             .door_height = 1.0f,
                                                             .parapet_blocks = 4});
  assert(!gatehouse.vertices.empty());
  assert(!gatehouse.indices.empty());

  bool found_open_gate = true;
  for (std::size_t i = 0; i < gatehouse.indices.size(); i += 3u) {
    const aster::Vertex &a = gatehouse.vertices[gatehouse.indices[i + 0u]];
    const aster::Vertex &b = gatehouse.vertices[gatehouse.indices[i + 1u]];
    const aster::Vertex &c = gatehouse.vertices[gatehouse.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.20f && centroid.y < 0.82f && std::abs(centroid.z) < 0.20f) {
      found_open_gate = false;
      break;
    }
  }
  assert(found_open_gate);

  const aster::CpuMesh ruined_castle = aster::makeRuinedCastleCourseMesh(
      {.half_width = 2.4f, .wall_height = 1.7f, .tower_height = 2.4f, .parapet_blocks = 7});
  assert(!ruined_castle.vertices.empty());
  assert(!ruined_castle.indices.empty());
}

void testVoxelStructureBuild() {
  std::vector<aster::VoxelCell> cells;
  aster::appendVoxelBox(cells, {0, 0, 0}, {2, 1, 1}, 3);
  const aster::VoxelStructure structure = aster::buildVoxelStructure(cells);
  assert(structure.mesh_batches.size() == 1u);
  assert(structure.mesh_batches.front().channel == 3u);
  assert(structure.mesh_batches.front().mesh.indices.size() == 10u * 6u);
  assert(structure.collision_boxes.size() == 1u);
  expectNear(structure.collision_boxes.front().center.x, 1.0f, 0.0001f);
  expectNear(structure.collision_boxes.front().half_extents.x, 1.0f, 0.0001f);

  std::vector<aster::VoxelCell> corner_cells;
  corner_cells.push_back({{0, 0, 0}, 1});
  corner_cells.push_back({{1, 0, 1}, 1});
  corner_cells.push_back({{0, 1, 1}, 1});
  const aster::VoxelStructure corner_structure = aster::buildVoxelStructure(corner_cells);
  bool found_occluded_vertex = false;
  for (const aster::VoxelMeshBatch &batch : corner_structure.mesh_batches) {
    for (const aster::Vertex &vertex : batch.mesh.vertices) {
      assert(vertex.ambient_occlusion >= 0.0f && vertex.ambient_occlusion <= 1.0f);
      found_occluded_vertex = found_occluded_vertex || vertex.ambient_occlusion < 0.70f;
    }
  }
  assert(found_occluded_vertex);
}

void testCastleCourseBuild() {
  const aster::CastleCourse course = aster::buildCastleCourse();
  assert(!course.structure.mesh_batches.empty());
  assert(!course.structure.collision_boxes.empty());
  assert(!course.box_elements.empty());
  assert(!course.ground_zones.empty());
  bool found_foundation = false;
  for (const aster::VoxelMeshBatch &batch : course.structure.mesh_batches) {
    found_foundation =
        found_foundation ||
        batch.channel == static_cast<std::uint32_t>(aster::CastleCourseChannel::Foundation);
  }
  assert(found_foundation);
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    min_x = std::min(min_x, box.center.x - box.half_extents.x);
    max_x = std::max(max_x, box.center.x + box.half_extents.x);
    min_z = std::min(min_z, box.center.z - box.half_extents.z);
    max_z = std::max(max_z, box.center.z + box.half_extents.z);
  }
  assert(max_x - min_x > 42.0f);
  assert(max_z - min_z > 34.0f);
  const aster::CastleCourseGroundZone &ground = course.ground_zones.front();
  assert(ground.half_extents.x > 20.0f);
  assert(ground.half_extents.y > 17.0f);
  assert(std::abs(ground.center.x) < 0.001f);
  const aster::Vec3 underpass_clearance{0.0f, 0.60f, -0.18f};
  bool underpass_blocked = false;
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    underpass_blocked =
        underpass_blocked || (std::abs(underpass_clearance.x - box.center.x) < box.half_extents.x &&
                              std::abs(underpass_clearance.y - box.center.y) < box.half_extents.y &&
                              std::abs(underpass_clearance.z - box.center.z) < box.half_extents.z);
  }
  assert(!underpass_blocked);
}

void testGeneratedSceneryAssembly() {
  auto mesh = std::make_shared<const aster::CpuMesh>(aster::makeBox());
  const aster::Material material =
      aster::makeMaterial({.base_color = {0.25f, 0.40f, 0.20f},
                           .camera_occlusion = aster::CameraOcclusionPolicy::Solid});
  const aster::GeneratedSceneryBundle bundle = aster::assembleGeneratedScenery(
      {.name = "grove",
       .root = {.position = {2.0f, 0.0f, 1.0f}, .scale = {2.0f, 1.0f, 2.0f}},
       .primitives = {aster::GeneratedSceneryPrimitivePart{
           .name = "stone",
           .primitive = aster::MeshPrimitive::Rock,
           .transform = {.position = {1.0f, 0.0f, 0.0f}, .scale = {0.5f, 0.5f, 0.5f}},
           .material = material}},
       .meshes = {aster::GeneratedSceneryMeshPart{.name = "trunk",
                                                  .mesh = mesh,
                                                  .transform = {.position = {0.0f, 1.0f, 0.0f}},
                                                  .material = material}},
       .collision_proxies = {{"bounds", {0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}}},
       .sockets = {aster::GeneratedScenerySocket{
           .name = "perch", .transform = {.position = {0.0f, 2.0f, 0.0f}}, .radius = 0.25f}}});
  assert(bundle.render_objects.size() == 2u);
  assert(bundle.render_objects[0].name == "grove/stone");
  expectNear(bundle.render_objects[0].transform.position.x, 4.0f, 0.0001f);
  assert(!bundle.render_objects[0].camera_occlusion_fade);
  assert(bundle.render_objects[1].custom_mesh == mesh);
  assert(bundle.collision_proxies.size() == 1u);
  expectNear(bundle.collision_proxies.front().half_extents.x, 1.0f, 0.0001f);
  const aster::GeneratedScenerySocket *socket =
      aster::findGeneratedScenerySocket(bundle, "grove/perch");
  assert(socket != nullptr);
  expectNear(socket->transform.position.y, 2.0f, 0.0001f);

  aster::Scene scene;
  std::vector<std::size_t> indices;
  aster::appendGeneratedScenery(scene, bundle, &indices);
  assert(scene.objects().size() == 2u);
  assert(indices.size() == 2u);
  assert(indices[0] == 0u && indices[1] == 1u);
}

} // namespace

int main() {
  testCaveWebMeshFitsOvalWithoutRectFrame();
  testMeshGeneration();
  testTerrainSculpting();
  testCaveInteriorVolume();
  testVoxelCaveStreamingAndFixtureContracts();
  testVoxelCaveStreamingPublishesReadySnapshotsOnly();
  testVoxelCaveDeferredEditRetainsPublishedCollision();
  testMeshModelingTopologyValidation();
  testImplicitSurfaceExtractionBuildsChamber();
  testImplicitSurfaceAdjacentChunksShareBoundary();
  testMeshProcessingPipeline();
  testMeshProcessingRejectsInvalidIndices();
  testMeshDrapingRaisesEmbeddedVertices();
  testSceneAssetNormalTangentImport();
  testBrushLevelMeshBuild();
  testArchitecturalMeshBuild();
  testVoxelStructureBuild();
  testCastleCourseBuild();
  testGeneratedSceneryAssembly();
  std::cout << "geometry_tests passed.\n";
  return 0;
}
