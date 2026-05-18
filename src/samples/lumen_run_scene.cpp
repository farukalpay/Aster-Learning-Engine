// Author: Faruk Alpay
// Do not remove this notice.

#include "lumen_run_detail.hpp"

namespace aster {
namespace {

[[nodiscard]] Vec3 toRuntimeVec(const sdk::Vec3 &value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] CaveTunnelProfile toRuntimeTunnelProfile(const sdk::CaveTunnelProfileDocument &source,
                                                       const float floor_y,
                                                       const CaveTunnelProfile &fallback) {
  CaveTunnelProfile profile = fallback;
  profile.seed = source.seed;
  profile.start = toRuntimeVec(source.start);
  profile.control = toRuntimeVec(source.control);
  profile.control_b = toRuntimeVec(source.control_b);
  profile.end = toRuntimeVec(source.end);
  if (source.floor_relative) {
    profile.start.y += floor_y;
    profile.control.y += floor_y;
    profile.control_b.y += floor_y;
    profile.end.y += floor_y;
  }
  profile.length_segments = source.length_segments;
  profile.radial_segments = source.radial_segments;
  profile.half_width = source.half_width;
  profile.wall_height = source.wall_height;
  profile.floor_width = source.floor_width;
  profile.floor_crown = source.floor_crown;
  profile.floor_edge_raise = source.floor_edge_raise;
  profile.wall_noise = source.wall_noise;
  profile.visible_wall_start_t = source.visible_wall_start_t;
  profile.collision_start_t = source.collision_start_t;
  profile.collision_end_t = source.collision_end_t;
  profile.ore_start_t = source.ore_start_t;
  profile.chest_t = source.chest_t;
  profile.chamber_t = source.chamber_t;
  profile.chamber_falloff = source.chamber_falloff;
  profile.chamber_width_scale = source.chamber_width_scale;
  profile.chamber_height_scale = source.chamber_height_scale;
  profile.end_constraint_enabled = source.end_constraint_enabled;
  return profile;
}

[[nodiscard]] CavePortalProfile
toRuntimePortalProfile(const sdk::CavePortalProfileDocument &source,
                       const CavePortalProfile &fallback) {
  CavePortalProfile profile = fallback;
  profile.arch_segments = source.arch_segments;
  profile.inner_half_width = source.inner_half_width;
  profile.inner_height = source.inner_height;
  profile.outer_half_width = source.outer_half_width;
  profile.outer_height = source.outer_height;
  profile.depth = source.depth;
  profile.ground_lip = source.ground_lip;
  profile.inner_lining_depth = source.inner_lining_depth;
  profile.lining_breakup = source.lining_breakup;
  return profile;
}

[[nodiscard]] CaveOreVeinProfile
toRuntimeOreProfile(const sdk::CaveOreVeinProfileDocument &source,
                    const CaveOreVeinProfile &fallback) {
  CaveOreVeinProfile profile = fallback;
  profile.seed = source.seed;
  profile.candidates = source.candidates;
  profile.max_nodes = source.max_nodes;
  profile.field_frequency_a = source.field_frequency_a;
  profile.field_frequency_b = source.field_frequency_b;
  profile.intersection_threshold_a = source.intersection_threshold_a;
  profile.intersection_threshold_b = source.intersection_threshold_b;
  profile.wall_inset = source.wall_inset;
  profile.min_spacing = source.min_spacing;
  return profile;
}

[[nodiscard]] CaveFeatureProfile
toRuntimeFeatureProfile(const sdk::CaveFeatureProfileDocument &source,
                        const CaveFeatureProfile &fallback) {
  CaveFeatureProfile profile = fallback;
  profile.seed = source.seed;
  profile.candidates = source.candidates;
  profile.max_features = source.max_features;
  profile.start_t = source.start_t;
  profile.end_t = source.end_t;
  profile.min_spacing = source.min_spacing;
  profile.wall_inset = source.wall_inset;
  profile.ceiling_fraction = source.ceiling_fraction;
  profile.column_fraction = source.column_fraction;
  profile.shelf_fraction = source.shelf_fraction;
  profile.mineral_fraction = source.mineral_fraction;
  return profile;
}

[[nodiscard]] CaveWallFixtureProfile
toRuntimeFixtureProfile(const sdk::CaveWallFixtureProfileDocument &source) {
  CaveWallFixtureProfile profile;
  profile.start_t = source.start_t;
  profile.end_t = source.end_t;
  profile.target_spacing = source.target_spacing;
  profile.max_count = source.max_count;
  profile.wall_side = source.wall_side;
  profile.mount_height = source.mount_height;
  profile.wall_inset = source.wall_inset;
  profile.normal_up_bias = source.normal_up_bias;
  profile.lens_offset = source.lens_offset;
  profile.light_offset = source.light_offset;
  return profile;
}

[[nodiscard]] const sdk::CaveSectionDocument *findCaveSection(const sdk::CaveDocument &cave,
                                                              const std::string_view id) {
  const auto found = std::find_if(cave.sections.begin(), cave.sections.end(),
                                  [id](const sdk::CaveSectionDocument &section) {
                                    return section.id == id;
                                  });
  return found == cave.sections.end() ? nullptr : &*found;
}

[[nodiscard]] const sdk::CavePlacementDocument *findCavePlacement(const sdk::CaveDocument &cave,
                                                                  const std::string_view id) {
  const auto found = std::find_if(cave.placements.begin(), cave.placements.end(),
                                  [id](const sdk::CavePlacementDocument &placement) {
                                    return placement.id == id;
                                  });
  return found == cave.placements.end() ? nullptr : &*found;
}

[[nodiscard]] CaveComplexSpec authoredEntryCaveSpec(const LumenAuthoringData &authoring,
                                                    const TerrainHeightField &terrain,
                                                    const float floor_y) {
  CaveComplexSpec spec = lumenTerrainCoveredCaveComplexSpec(terrain, floor_y);
  if (!authoring.valid) {
    return spec;
  }
  const sdk::CaveSectionDocument *section = findCaveSection(authoring.cave, "entry");
  if (section == nullptr) {
    return spec;
  }
  spec.tunnel = toRuntimeTunnelProfile(section->tunnel, floor_y, spec.tunnel);
  if (section->terrain_cover_fit) {
    const CaveTerrainCoverFit cover_fit = fitCaveTunnelToTerrainCover(
        spec.tunnel,
        [&terrain](const Vec2 position) {
          const TerrainSurfaceSample sample = sampleTerrain(terrain, position);
          return CaveTerrainCoverSample{sample.valid, sample.valid ? sample.height : 0.0f};
        },
        {.samples = 96,
         .required_consecutive_samples = 3,
         .min_t = 0.0f,
         .max_t = 0.52f,
         .roof_clearance = 0.24f});
    if (cover_fit.cover_found) {
      spec.tunnel = cover_fit.tunnel;
      spec.tunnel.visible_wall_start_t =
          clamp(cover_fit.first_covered_t - 0.018f, 0.0f, 0.40f);
    }
    spec.tunnel.visible_wall_start_t = std::min(spec.tunnel.visible_wall_start_t, 0.035f);
  }
  spec.portal = toRuntimePortalProfile(section->portal, spec.portal);
  spec.ore = toRuntimeOreProfile(section->ore, spec.ore);
  spec.features = toRuntimeFeatureProfile(section->features, spec.features);
  spec.features.start_t =
      std::max(spec.features.start_t,
               clamp(spec.tunnel.visible_wall_start_t + 0.18f, 0.0f, spec.features.end_t));
  return spec;
}

[[nodiscard]] CaveComplexSpec authoredDeepCaveSpec(const LumenAuthoringData &authoring,
                                                   const CaveTunnelProfile &entry_tunnel) {
  CaveComplexSpec spec = lumenDeepCaveComplexSpec(entry_tunnel);
  if (!authoring.valid) {
    return spec;
  }
  const sdk::CaveSectionDocument *section = findCaveSection(authoring.cave, "deep");
  if (section == nullptr) {
    return spec;
  }
  if (section->derive_from_previous) {
    CaveTunnelProfile tunnel = lumenDeepCaveTunnelProfile(entry_tunnel);
    const sdk::CaveTunnelProfileDocument &source = section->tunnel;
    tunnel.seed = source.seed;
    tunnel.length_segments = source.length_segments;
    tunnel.radial_segments = source.radial_segments;
    tunnel.half_width = source.half_width;
    tunnel.wall_height = source.wall_height;
    tunnel.floor_width = source.floor_width;
    tunnel.floor_crown = source.floor_crown;
    tunnel.floor_edge_raise = source.floor_edge_raise;
    tunnel.wall_noise = source.wall_noise;
    tunnel.visible_wall_start_t = source.visible_wall_start_t;
    tunnel.collision_start_t = source.collision_start_t;
    tunnel.collision_end_t = source.collision_end_t;
    tunnel.ore_start_t = source.ore_start_t;
    tunnel.chest_t = source.chest_t;
    tunnel.chamber_t = source.chamber_t;
    tunnel.chamber_falloff = source.chamber_falloff;
    tunnel.chamber_width_scale = source.chamber_width_scale;
    tunnel.chamber_height_scale = source.chamber_height_scale;
    tunnel.end_constraint_enabled = source.end_constraint_enabled;
    spec.tunnel = tunnel;
  } else {
    spec.tunnel = toRuntimeTunnelProfile(section->tunnel, 0.0f, spec.tunnel);
  }
  spec.ore = toRuntimeOreProfile(section->ore, spec.ore);
  spec.features = toRuntimeFeatureProfile(section->features, spec.features);
  return spec;
}

[[nodiscard]] std::vector<CaveWallFixturePlacement>
authoredFixturePlacements(const LumenAuthoringData &authoring, const std::string_view section_id,
                          const CaveTunnelProfile &tunnel,
                          const std::vector<CaveWallFixtureProfile> &fallback_profiles) {
  std::vector<CaveWallFixturePlacement> placements;
  const sdk::CaveSectionDocument *section =
      authoring.valid ? findCaveSection(authoring.cave, section_id) : nullptr;
  if (section != nullptr && !section->fixtures.empty()) {
    for (const sdk::CaveWallFixtureProfileDocument &fixture : section->fixtures) {
      std::vector<CaveWallFixturePlacement> generated =
          placeCaveWallFixtures(tunnel, toRuntimeFixtureProfile(fixture));
      placements.insert(placements.end(), generated.begin(), generated.end());
    }
    return placements;
  }
  for (const CaveWallFixtureProfile &profile : fallback_profiles) {
    std::vector<CaveWallFixturePlacement> generated = placeCaveWallFixtures(tunnel, profile);
    placements.insert(placements.end(), generated.begin(), generated.end());
  }
  return placements;
}

[[nodiscard]] Material caveOverlayMaterial(Vec3 color) {
  Material overlay =
      material(color, color, 0.42f, 0.0f, 0.28f, 0.12f, 1.0f, 0.0f, 1.0f);
  overlay.opacity = 0.26f;
  overlay.alpha_mode = MaterialAlphaMode::Blend;
  overlay.depth_write = MaterialDepthWrite::Disabled;
  overlay.double_sided = true;
  overlay.cull_mode = FaceCullMode::None;
  overlay.camera_occlusion = CameraOcclusionPolicy::Solid;
  return overlay;
}

} // namespace

// Scene construction and physics world rebuilds.
std::size_t LumenRun::appendObject(RenderObject object) {
  scene_.objects().push_back(std::move(object));
  return scene_.objects().size() - 1;
}

void LumenRun::rebuildScene() {
  ASTER_PROFILE_SCOPE("LumenRun::rebuildScene");
  scene_.objects().clear();
  pond_accent_light_valid_ = false;
  cave_sections_.clear();
  cave_entrance_light_position_ = {};
  cave_collision_meshes_.clear();
  cave_webs_.clear();
  scenery_collision_boxes_.clear();
  mining_fracture_shards_.clear();
  prism_relay_core_object_ = 0;
  prism_relay_core_valid_ = false;
  prism_relay_ring_objects_.clear();
  prism_relay_conduit_objects_.clear();
  prism_relay_node_objects_.clear();
  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  const TerrainMeshClipEllipse bird_grove_clip = birdGroveClip(castle_bird_nest_position_);
  const std::vector<TerrainMeshClipEllipse> terrain_holes =
      terrainSurfaceHoles(pond_center_, inner_pond_center_, inner_pond_radius_, bird_grove_clip);
  const std::vector<TerrainMeshClipOrientedEllipse> terrain_portal_clips = {
      cavePortalTerrainClip()};
  const std::vector<TerrainMeshClipEllipse> wetland_clips =
      plazaWetlandClips(pond_center_, inner_pond_center_, inner_pond_radius_);
  const std::vector<TerrainMeshClipEllipse> stone_floor_holes =
      hardscapeOcclusionClips(pond_center_, inner_pond_center_, inner_pond_radius_);
  std::vector<PlacementFootprint> stone_floor_footprints;
  stone_floor_footprints.reserve(course.ground_zones.size());
  for (const CastleCourseGroundZone &zone : course.ground_zones) {
    stone_floor_footprints.push_back(translatedGroundZoneFootprint(zone, castle_origin));
  }
  const PlacementValidator terrain_placement =
      terrainPlacementValidator(stone_floor_footprints, terrain_holes, terrain_portal_clips);
  support_surfaces_.clear();
  support_surfaces_.setTerrain(&terrain_);
  support_surfaces_.setTerrainPlacementValidator(terrain_placement);
  SupportSurfaceSet decorative_ground_surfaces;
  decorative_ground_surfaces.setTerrain(&terrain_);
  decorative_ground_surfaces.setTerrainPlacementValidator(terrain_placement);

  const Material stone_plaza = makeSupportSurfaceMaterial(
      material({0.40f, 0.39f, 0.34f}, {0.0f, 0.0f, 0.0f}, 0.9f, 0.0f, 0.0f, 0.92f, 4.8f, 0.28f,
               0.74f, SurfacePattern::CourseCells, {1.45f, 1.45f}, 0.050f, 0.56f, 0.050f));
  const Material brick_facade =
      material({0.50f, 0.34f, 0.25f}, {0.0f, 0.0f, 0.0f}, 0.91f, 0.0f, 0.0f, 0.92f, 7.2f, 0.46f,
               0.58f, SurfacePattern::WeatheredStone, {5.4f, 8.2f}, 0.100f, 0.86f, 0.055f);
  const Material dark_masonry =
      material({0.31f, 0.325f, 0.305f}, {0.0f, 0.0f, 0.0f}, 0.94f, 0.0f, 0.0f, 0.76f, 6.2f, 0.40f,
               0.62f, SurfacePattern::WeatheredStone, {4.6f, 6.6f}, 0.070f, 0.74f, 0.070f);
  const Material warm_window = material({0.74f, 0.58f, 0.36f}, {1.0f, 0.66f, 0.32f}, 0.5f, 0.0f,
                                        0.18f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material weathered_iron = material({0.24f, 0.23f, 0.22f}, {0.0f, 0.0f, 0.0f}, 0.56f, 0.62f,
                                           0.0f, 0.34f, 8.6f, 0.18f, 0.88f);
  const Material amber_lamp = material({0.92f, 0.68f, 0.38f}, {1.0f, 0.68f, 0.30f}, 0.16f, 0.02f,
                                       0.44f, 0.08f, 5.0f, 0.04f, 1.0f);
  const Material amber_shard =
      material({0.98f, 0.55f, 0.18f}, {1.0f, 0.46f, 0.12f}, 0.13f, 0.0f, 0.36f, 0.24f, 8.2f, 0.05f,
               0.96f, SurfacePattern::AmberResin, {6.8f, 10.5f}, 0.035f, 0.92f, 0.04f);
  const Material sentinel_material =
      material({0.13f, 0.145f, 0.13f}, {0.82f, 0.24f, 0.08f}, 0.52f, 0.36f, 0.18f, 0.46f, 10.0f,
               0.24f, 0.84f, SurfacePattern::FiberStrands, {4.0f, 9.0f}, 0.018f, 0.42f, 0.05f);
  Material grass_soil = makeSupportSurfaceMaterial(
      material({0.30f, 0.44f, 0.20f}, {0.0f, 0.0f, 0.0f}, 0.90f, 0.0f, 0.0f, 0.92f, 12.0f, 0.16f,
               0.96f, SurfacePattern::GrassSoil, {6.2f, 7.4f}, 0.095f, 0.88f, 0.08f));
  grass_soil.procedural = {.macro_variation = 0.62f,
                           .micro_normal_strength = 0.36f,
                           .roughness_variation = 0.18f,
                           .height_shading = 0.18f};
  Material terrain_transition = makeSupportSurfaceMaterial(
      material({0.42f, 0.39f, 0.30f}, {0.0f, 0.0f, 0.0f}, 0.94f, 0.0f, 0.0f, 0.72f, 8.6f, 0.08f,
               0.94f, SurfacePattern::TerrainBlend, {1.65f, 1.65f}, 0.030f, 0.66f, 0.08f));
  Material hardscape_dust = makeSupportSurfaceMaterial(
      material({0.46f, 0.45f, 0.36f}, {0.0f, 0.0f, 0.0f}, 0.96f, 0.0f, 0.0f, 0.56f, 8.4f, 0.06f,
               0.96f, SurfacePattern::TerrainBlend, {3.40f, 4.10f}, 0.024f, 0.62f, 0.08f));
  Material hardscape_substrate = hardscape_dust;
  hardscape_substrate.base_color = {0.49f, 0.51f, 0.40f};
  hardscape_substrate.opacity = 1.0f;
  hardscape_substrate.pattern_contrast = 0.30f;
  Material grass_blades =
      material({0.19f, 0.39f, 0.13f}, {0.0f, 0.0f, 0.0f}, 0.92f, 0.0f, 0.0f, 0.52f, 9.8f, 0.05f,
               0.95f, SurfacePattern::Foliage, {8.8f, 12.6f}, 0.026f, 0.62f, 0.08f);
  grass_blades.double_sided = true;
  grass_blades.alpha_mode = MaterialAlphaMode::Masked;
  grass_blades.camera_occlusion = CameraOcclusionPolicy::Solid;
  grass_blades.procedural = {.macro_variation = 0.42f,
                             .micro_normal_strength = 0.28f,
                             .roughness_variation = 0.12f,
                             .height_shading = 0.14f};
  Material exotic_leaf =
      material({0.15f, 0.50f, 0.28f}, {0.015f, 0.040f, 0.016f}, 0.68f, 0.0f, 0.025f, 0.36f, 8.8f,
               0.02f, 0.96f, SurfacePattern::Foliage, {8.6f, 12.2f}, 0.026f, 0.56f, 0.06f);
  exotic_leaf.double_sided = true;
  exotic_leaf.alpha_mode = MaterialAlphaMode::Masked;
  exotic_leaf.camera_occlusion = CameraOcclusionPolicy::Solid;
  exotic_leaf.procedural = {.macro_variation = 0.26f,
                            .micro_normal_strength = 0.20f,
                            .roughness_variation = 0.10f,
                            .height_shading = 0.08f};
  const Material exotic_flower =
      material({0.72f, 0.26f, 0.52f}, {0.16f, 0.035f, 0.10f}, 0.58f, 0.0f, 0.08f, 0.18f, 6.2f,
               0.02f, 0.92f, SurfacePattern::FiberStrands, {5.0f, 7.2f}, 0.012f, 0.36f, 0.05f);
  const Material fish_scale_a = material(
      {0.14f, 0.54f, 0.60f}, {0.015f, 0.055f, 0.055f}, 0.42f, 0.0f, 0.020f, 0.20f, 9.2f, 0.03f,
      0.94f, SurfacePattern::IridescentScales, {13.5f, 17.0f}, 0.014f, 0.30f, 0.04f);
  const Material fish_scale_b = material(
      {0.78f, 0.36f, 0.16f}, {0.06f, 0.025f, 0.010f}, 0.44f, 0.0f, 0.018f, 0.18f, 8.0f, 0.03f,
      0.94f, SurfacePattern::IridescentScales, {12.0f, 15.5f}, 0.013f, 0.28f, 0.04f);
  Material pond_water =
      material({0.055f, 0.54f, 0.68f}, {0.025f, 0.12f, 0.13f}, 0.10f, 0.0f, 0.050f, 0.34f, 7.0f,
               0.0f, 0.98f, SurfacePattern::WaterSurface, {5.8f, 6.6f}, 0.045f, 0.86f, 0.04f);
  pond_water.opacity = 0.78f;
  pond_water.double_sided = true;
  pond_water.alpha_mode = MaterialAlphaMode::Blend;
  pond_water.depth_write = MaterialDepthWrite::Disabled;
  pond_water.camera_occlusion = CameraOcclusionPolicy::Solid;
  pond_water.procedural = {.macro_variation = 0.20f,
                           .micro_normal_strength = 0.0f,
                           .roughness_variation = 0.16f,
                           .wetness = 0.42f};
  Material pond_stone =
      material({0.54f, 0.49f, 0.40f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.58f, 6.4f, 0.16f,
               0.92f, SurfacePattern::WeatheredStone, {3.4f, 4.8f}, 0.040f, 0.54f, 0.07f);
  pond_stone.procedural = {.macro_variation = 0.22f,
                           .micro_normal_strength = 0.22f,
                           .roughness_variation = 0.12f,
                           .wetness = 0.06f,
                           .height_shading = 0.10f};
  Material soil_path = makeSupportSurfaceMaterial(
      material({0.60f, 0.43f, 0.24f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.86f, 7.4f, 0.10f,
               0.95f, SurfacePattern::SoilPath, {6.4f, 9.0f}, 0.040f, 0.88f, 0.05f));
  soil_path.procedural = {.macro_variation = 0.36f,
                          .micro_normal_strength = 0.24f,
                          .roughness_variation = 0.14f,
                          .height_shading = 0.12f};
  const Material fishing_line_material = material({0.50f, 0.52f, 0.46f}, {0.02f, 0.02f, 0.015f},
                                                  0.56f, 0.0f, 0.02f, 0.08f, 10.0f, 0.02f, 0.96f);
  const Material fishing_rod_material = material({0.16f, 0.105f, 0.055f}, {0.0f, 0.0f, 0.0f}, 0.54f,
                                                 0.04f, 0.0f, 0.30f, 9.0f, 0.12f, 0.92f);
  const Material tree_bark =
      material({0.30f, 0.18f, 0.095f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.02f, 0.0f, 0.66f, 10.5f, 0.18f,
               0.88f, SurfacePattern::PaintedWood, {3.2f, 12.0f}, 0.042f, 0.72f, 0.06f);
  Material layered_terrain = makeSupportSurfaceMaterial(
      material({0.185f, 0.268f, 0.160f}, {0.0f, 0.0f, 0.0f}, 0.95f, 0.0f, 0.0f, 0.84f, 4.8f, 0.09f,
               0.91f, SurfacePattern::LayeredTerrain, {0.54f, 0.62f}, 0.048f, 1.0f, 0.08f));
  layered_terrain.procedural = {.macro_variation = 0.60f,
                                .micro_normal_strength = 0.36f,
                                .roughness_variation = 0.18f,
                                .height_shading = 0.24f};
  Material mountain_stone =
      material({0.245f, 0.255f, 0.235f}, {0.0f, 0.0f, 0.0f}, 0.96f, 0.0f, 0.0f, 0.90f, 7.6f, 0.30f,
               0.70f, SurfacePattern::WeatheredStone, {5.8f, 9.2f}, 0.092f, 0.88f, 0.070f);
  mountain_stone.procedural = {.macro_variation = 0.42f,
                               .micro_normal_strength = 0.34f,
                               .roughness_variation = 0.14f,
                               .height_shading = 0.16f};
  Material cave_mouth_stone = mountain_stone;
  cave_mouth_stone.base_color = {0.315f, 0.318f, 0.286f};
  cave_mouth_stone.surface_pattern = SurfacePattern::CaveRock;
  cave_mouth_stone.pattern_scale = {3.6f, 6.2f};
  cave_mouth_stone.pattern_depth = 0.132f;
  cave_mouth_stone.pattern_contrast = 1.06f;
  cave_mouth_stone.detail_strength = 0.94f;
  cave_mouth_stone.detail_scale = 11.4f;
  cave_mouth_stone.edge_wear = 0.32f;
  cave_mouth_stone.ambient_occlusion = 0.62f;
  cave_mouth_stone.double_sided = true;
  cave_mouth_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_mouth_stone.procedural = {.macro_variation = 0.58f,
                                 .micro_normal_strength = 0.48f,
                                 .roughness_variation = 0.18f,
                                 .wetness = 0.06f,
                                 .height_shading = 0.22f};
  Material cave_wall =
      material({0.108f, 0.112f, 0.104f}, {0.004f, 0.006f, 0.004f}, 0.97f, 0.0f, 0.006f, 1.18f, 12.4f,
               0.34f, 0.54f, SurfacePattern::CaveRock, {2.9f, 5.6f}, 0.155f, 1.08f, 0.050f);
  cave_wall.cull_mode = FaceCullMode::Back;
  cave_wall.double_sided = true;
  cave_wall.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_wall.asset_id = "material.cave_rock";
  cave_wall.procedural = {.macro_variation = 0.78f,
                          .micro_normal_strength = 0.62f,
                          .roughness_variation = 0.24f,
                          .wetness = 0.18f,
                          .height_shading = 0.30f};
  Material cave_entrance_wall = cave_wall;
  cave_entrance_wall.base_color = {0.132f, 0.124f, 0.104f};
  cave_entrance_wall.emission_color = {0.026f, 0.020f, 0.012f};
  cave_entrance_wall.emission_strength = 0.026f;
  cave_entrance_wall.ambient_occlusion = 0.54f;
  cave_entrance_wall.procedural.wetness = 0.12f;
  Material cave_floor = makeSupportSurfaceMaterial(cave_wall);
  cave_floor.base_color = {0.086f, 0.083f, 0.070f};
  cave_floor.pattern_scale = {4.0f, 5.8f};
  cave_floor.pattern_depth = 0.118f;
  cave_floor.ambient_occlusion = 0.42f;
  cave_floor.procedural.wetness = 0.28f;
  cave_floor.asset_id = "material.cave_rock";
  Material cave_wet_streak =
      material({0.044f, 0.046f, 0.041f}, {0.014f, 0.016f, 0.012f}, 0.42f, 0.0f, 0.018f, 1.10f,
               18.0f, 0.02f, 0.38f, SurfacePattern::CaveRock, {1.3f, 9.2f}, 0.038f, 0.62f,
               0.035f,
               {.macro_variation = 0.42f,
                .micro_normal_strength = 0.18f,
                .roughness_variation = 0.34f,
                .wetness = 0.82f,
                .height_shading = 0.08f});
  cave_wet_streak.opacity = 0.78f;
  cave_wet_streak.alpha_mode = MaterialAlphaMode::Blend;
  cave_wet_streak.depth_write = MaterialDepthWrite::Disabled;
  cave_wet_streak.double_sided = true;
  cave_wet_streak.cull_mode = FaceCullMode::None;
  cave_wet_streak.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_moss_patch =
      material({0.065f, 0.115f, 0.054f}, {0.003f, 0.010f, 0.002f}, 0.92f, 0.0f, 0.004f, 0.72f,
               14.0f, 0.04f, 0.70f, SurfacePattern::GrassSoil, {5.0f, 7.0f}, 0.020f, 0.48f,
               0.065f,
               {.macro_variation = 0.38f,
                .micro_normal_strength = 0.26f,
                .roughness_variation = 0.20f,
                .wetness = 0.46f,
                .height_shading = 0.06f});
  cave_moss_patch.opacity = 0.82f;
  cave_moss_patch.alpha_mode = MaterialAlphaMode::Blend;
  cave_moss_patch.depth_write = MaterialDepthWrite::Disabled;
  cave_moss_patch.double_sided = true;
  cave_moss_patch.cull_mode = FaceCullMode::None;
  cave_moss_patch.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_scratch_mark =
      material({0.235f, 0.205f, 0.160f}, {0.020f, 0.012f, 0.006f}, 0.74f, 0.0f, 0.010f, 0.46f,
               22.0f, 0.18f, 0.66f, SurfacePattern::WeatheredStone, {18.0f, 3.0f}, 0.012f,
               0.72f, 0.040f);
  cave_scratch_mark.opacity = 0.72f;
  cave_scratch_mark.alpha_mode = MaterialAlphaMode::Blend;
  cave_scratch_mark.depth_write = MaterialDepthWrite::Disabled;
  cave_scratch_mark.double_sided = true;
  cave_scratch_mark.cull_mode = FaceCullMode::None;
  cave_scratch_mark.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_soot_rust =
      material({0.034f, 0.033f, 0.030f}, {0.001f, 0.001f, 0.001f}, 0.97f, 0.02f, 0.0f, 0.58f,
               10.0f, 0.34f, 0.46f, SurfacePattern::WeatheredMetal, {4.0f, 9.0f}, 0.016f, 0.68f,
               0.050f);
  cave_soot_rust.opacity = 0.34f;
  cave_soot_rust.alpha_mode = MaterialAlphaMode::Blend;
  cave_soot_rust.depth_write = MaterialDepthWrite::Disabled;
  cave_soot_rust.double_sided = true;
  cave_soot_rust.cull_mode = FaceCullMode::None;
  cave_soot_rust.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_dust_accumulation =
      material({0.205f, 0.178f, 0.130f}, {0.004f, 0.003f, 0.002f}, 0.98f, 0.0f, 0.0f, 0.52f,
               8.0f, 0.06f, 0.72f, SurfacePattern::SoilPath, {3.4f, 5.0f}, 0.014f, 0.36f,
               0.070f,
               {.macro_variation = 0.36f,
                .micro_normal_strength = 0.10f,
                .roughness_variation = 0.12f,
                .height_shading = 0.035f});
  cave_dust_accumulation.opacity = 0.64f;
  cave_dust_accumulation.alpha_mode = MaterialAlphaMode::Blend;
  cave_dust_accumulation.depth_write = MaterialDepthWrite::Disabled;
  cave_dust_accumulation.double_sided = true;
  cave_dust_accumulation.cull_mode = FaceCullMode::None;
  cave_dust_accumulation.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_talus = makeSupportSurfaceMaterial(
      material({0.155f, 0.150f, 0.128f}, {0.010f, 0.008f, 0.005f}, 0.94f, 0.0f, 0.008f, 0.82f, 8.6f,
               0.30f, 0.62f, SurfacePattern::CaveRock, {2.8f, 5.4f}, 0.052f, 0.72f, 0.055f));
  cave_talus.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_talus.procedural = {.macro_variation = 0.34f,
                           .micro_normal_strength = 0.28f,
                           .roughness_variation = 0.14f,
                           .wetness = 0.08f,
                           .height_shading = 0.14f};
  Material cave_calcite =
      material({0.215f, 0.208f, 0.184f}, {0.010f, 0.009f, 0.007f}, 0.91f, 0.0f, 0.004f, 0.92f,
               12.0f, 0.30f, 0.62f, SurfacePattern::CaveRock, {3.0f, 9.0f}, 0.082f, 0.80f,
               0.055f);
  cave_calcite.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_mineral_glow =
      material({0.72f, 0.46f, 0.24f}, {0.86f, 0.44f, 0.14f}, 0.28f, 0.0f, 0.30f, 0.36f, 12.0f,
               0.04f, 0.92f, SurfacePattern::AmberResin, {7.0f, 11.0f}, 0.020f, 0.64f, 0.04f);
  cave_mineral_glow.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material coal_ore_material =
      material({0.090f, 0.080f, 0.066f}, {0.170f, 0.105f, 0.040f}, 0.52f, 0.12f, 0.115f, 0.74f,
               18.0f, 0.22f, 0.94f, SurfacePattern::CoalVein, {7.4f, 12.0f}, 0.135f, 0.98f, 0.045f);
  coal_ore_material.opacity = 1.0f;
  coal_ore_material.emission_strength = 0.16f;
  coal_ore_material.edge_wear = 0.30f;
  coal_ore_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material ironstone_ore_material =
      material({0.245f, 0.155f, 0.105f}, {0.42f, 0.19f, 0.075f}, 0.78f, 0.08f, 0.040f, 0.82f,
               14.0f, 0.28f, 0.86f, SurfacePattern::CaveRock, {3.8f, 6.8f}, 0.180f, 0.94f,
               0.055f);
  ironstone_ore_material.opacity = 1.0f;
  ironstone_ore_material.edge_wear = 0.38f;
  ironstone_ore_material.emission_strength = 0.075f;
  ironstone_ore_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  ironstone_ore_material.cull_mode = FaceCullMode::Back;
  Material cave_web_material =
      material({0.78f, 0.80f, 0.76f}, {0.070f, 0.078f, 0.070f}, 0.34f, 0.0f, 0.060f, 0.76f,
               15.0f, 0.02f, 0.82f, SurfacePattern::CaveWeb, {4.0f, 18.0f}, 0.018f, 0.72f,
               0.035f,
               {.macro_variation = 0.18f,
                .micro_normal_strength = 0.16f,
                .roughness_variation = 0.18f,
                .height_shading = 0.04f});
  cave_web_material.opacity = 0.86f;
  cave_web_material.double_sided = true;
  cave_web_material.cull_mode = FaceCullMode::None;
  cave_web_material.alpha_mode = MaterialAlphaMode::Blend;
  cave_web_material.depth_write = MaterialDepthWrite::Enabled;
  cave_web_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_skitter_material =
      material({0.115f, 0.058f, 0.042f}, {0.105f, 0.020f, 0.010f}, 0.34f, 0.06f, 0.070f, 0.82f,
               24.0f, 0.12f, 0.84f, SurfacePattern::CaveSkitterChitin, {28.0f, 34.0f}, 0.070f,
               0.92f, 0.035f,
               {.macro_variation = 0.24f,
                .micro_normal_strength = 0.38f,
                .roughness_variation = 0.24f,
                .height_shading = 0.16f});
  cave_skitter_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_skitter_material.opacity = 1.0f;
  cave_skitter_material.alpha_mode = MaterialAlphaMode::Opaque;
  cave_skitter_material.depth_write = MaterialDepthWrite::Enabled;
  cave_skitter_material.double_sided = false;
  cave_skitter_material.cull_mode = FaceCullMode::Back;
  Material industrial_light_metal = material(
      {0.070f, 0.064f, 0.055f}, {0.004f, 0.003f, 0.002f}, 0.52f, 0.68f, 0.004f, 0.54f, 12.0f, 0.16f,
      0.78f, SurfacePattern::WeatheredStone, {4.2f, 6.4f}, 0.024f, 0.56f, 0.05f);
  industrial_light_metal.cull_mode = FaceCullMode::Back;
  Material industrial_red_lens =
      material({0.92f, 0.56f, 0.30f}, {1.0f, 0.42f, 0.18f}, 0.34f, 0.0f, 0.34f, 0.26f, 6.0f, 0.02f,
               0.88f, SurfacePattern::AmberResin, {4.4f, 7.2f}, 0.010f, 0.50f, 0.04f);
  industrial_red_lens.double_sided = true;
  industrial_red_lens.alpha_mode = MaterialAlphaMode::Blend;
  industrial_red_lens.depth_write = MaterialDepthWrite::Disabled;
  applyIndustrialLensColor(industrial_red_lens, kCaveIndustrialWarmLight);
  Material teddy_fur =
      material({0.39f, 0.255f, 0.155f}, {0.0f, 0.0f, 0.0f}, 0.99f, 0.0f, 0.0f, 1.08f, 18.0f, 0.035f,
               0.86f, SurfacePattern::FurFibers, {9.5f, 18.0f}, 0.060f, 0.92f, 0.08f);
  teddy_fur.procedural = {.macro_variation = 0.24f,
                          .micro_normal_strength = 0.30f,
                          .roughness_variation = 0.16f,
                          .height_shading = 0.08f};
  Material teddy_muzzle =
      material({0.70f, 0.55f, 0.37f}, {0.0f, 0.0f, 0.0f}, 0.97f, 0.0f, 0.0f, 0.70f, 12.0f, 0.02f,
               0.86f, SurfacePattern::FurFibers, {6.2f, 10.5f}, 0.026f, 0.58f, 0.08f);
  teddy_muzzle.procedural = {.macro_variation = 0.16f,
                             .micro_normal_strength = 0.20f,
                             .roughness_variation = 0.12f,
                             .height_shading = 0.06f};
  const Material teddy_eye = material({0.018f, 0.014f, 0.012f}, {0.030f, 0.022f, 0.014f}, 0.18f,
                                      0.0f, 0.03f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material teddy_nose = material({0.050f, 0.030f, 0.020f}, {0.018f, 0.010f, 0.006f}, 0.34f,
                                       0.0f, 0.02f, 0.04f, 3.5f, 0.0f, 1.0f);
  Material sign_wood =
      material({0.48f, 0.31f, 0.16f}, {0.0f, 0.0f, 0.0f}, 0.86f, 0.0f, 0.0f, 0.72f, 8.2f, 0.62f,
               0.72f, SurfacePattern::PaintedWood, {2.2f, 7.6f}, 0.036f, 0.86f, 0.08f);
  sign_wood.double_sided = true;
  Material sign_ink =
      material({0.055f, 0.038f, 0.020f}, {0.0f, 0.0f, 0.0f}, 0.78f, 0.0f, 0.0f, 0.18f, 5.0f, 0.12f,
               0.98f, SurfacePattern::FiberStrands, {3.4f, 8.2f}, 0.004f, 0.34f, 0.08f);
  sign_ink.double_sided = true;
  const Material marsh_soil = makeSupportSurfaceMaterial(
      material({0.22f, 0.25f, 0.16f}, {0.0f, 0.0f, 0.0f}, 0.97f, 0.0f, 0.0f, 0.86f, 9.2f, 0.18f,
               0.94f, SurfacePattern::GrassSoil, {6.8f, 8.6f}, 0.070f, 0.78f, 0.08f));
  Material wetland_blades =
      material({0.20f, 0.31f, 0.18f}, {0.0f, 0.0f, 0.0f}, 0.92f, 0.0f, 0.0f, 0.58f, 9.5f, 0.08f,
               0.95f, SurfacePattern::Foliage, {8.5f, 11.0f}, 0.024f, 0.58f, 0.08f);
  wetland_blades.double_sided = true;
  wetland_blades.alpha_mode = MaterialAlphaMode::Masked;
  wetland_blades.camera_occlusion = CameraOcclusionPolicy::Solid;
  const Material crocodile_hide =
      material({0.11f, 0.23f, 0.115f}, {0.010f, 0.020f, 0.006f}, 0.72f, 0.0f, 0.018f, 0.68f, 15.0f,
               0.18f, 0.86f, SurfacePattern::ReptileScales, {22.0f, 30.0f}, 0.048f, 0.92f, 0.030f);
  const Material crocodile_belly =
      material({0.42f, 0.39f, 0.23f}, {0.010f, 0.008f, 0.003f}, 0.78f, 0.0f, 0.0f, 0.38f, 8.0f,
               0.04f, 0.92f, SurfacePattern::FiberStrands, {4.0f, 9.0f}, 0.014f, 0.36f, 0.06f);
  const Material crocodile_eye = material({0.70f, 0.54f, 0.12f}, {0.32f, 0.20f, 0.04f}, 0.32f, 0.0f,
                                          0.10f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material blood_material =
      material({0.48f, 0.018f, 0.010f}, {0.10f, 0.0f, 0.0f}, 0.44f, 0.0f, 0.10f, 0.20f, 9.0f, 0.03f,
               0.96f, SurfacePattern::WaterSurface, {2.5f, 3.0f}, 0.012f, 0.26f, 0.05f);
  Material x_eye_material =
      material({0.010f, 0.006f, 0.004f}, {0.12f, 0.010f, 0.004f}, 0.62f, 0.0f, 0.08f);
  x_eye_material.double_sided = true;
  Material emerald_feathers =
      material({0.10f, 0.48f, 0.36f}, {0.018f, 0.060f, 0.046f}, 0.46f, 0.0f, 0.025f, 0.44f, 12.0f,
               0.04f, 0.92f, SurfacePattern::FeatherVanes, {5.6f, 13.0f}, 0.030f, 0.62f, 0.05f);
  emerald_feathers.double_sided = true;
  emerald_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material crimson_feathers =
      material({0.54f, 0.12f, 0.18f}, {0.045f, 0.010f, 0.018f}, 0.50f, 0.0f, 0.030f, 0.40f, 11.5f,
               0.03f, 0.93f, SurfacePattern::FeatherVanes, {5.2f, 12.4f}, 0.028f, 0.58f, 0.05f);
  crimson_feathers.double_sided = true;
  crimson_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material ink_white_feathers =
      material({0.12f, 0.11f, 0.105f}, {0.020f, 0.018f, 0.016f}, 0.54f, 0.0f, 0.018f, 0.46f, 13.5f,
               0.02f, 0.94f, SurfacePattern::FeatherVanes, {6.6f, 15.5f}, 0.026f, 0.66f, 0.05f);
  ink_white_feathers.double_sided = true;
  ink_white_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material gold_blue_feathers =
      material({0.78f, 0.48f, 0.13f}, {0.058f, 0.032f, 0.012f}, 0.44f, 0.0f, 0.026f, 0.38f, 10.8f,
               0.03f, 0.92f, SurfacePattern::FeatherVanes, {4.8f, 11.2f}, 0.024f, 0.56f, 0.05f);
  gold_blue_feathers.double_sided = true;
  gold_blue_feathers.alpha_mode = MaterialAlphaMode::Masked;
  const Material nest_twig_material =
      material({0.35f, 0.22f, 0.12f}, {0.0f, 0.0f, 0.0f}, 0.84f, 0.0f, 0.0f, 0.66f, 14.0f, 0.18f,
               0.86f, SurfacePattern::TwigNest, {8.0f, 14.0f}, 0.040f, 0.82f, 0.06f);
  auto appendScenery = [&](const char *name, const MeshPrimitive primitive, const Vec3 position,
                           const Vec3 scale, const Vec3 rotation, const Material &scenery_material,
                           const float spin_rate = 0.0f) {
    RenderObject object;
    object.name = name;
    object.primitive = primitive;
    object.transform.position = position;
    object.transform.scale = scale;
    object.transform.rotation = quatFromEulerXyz(rotation);
    object.material = scenery_material;
    object.camera_occlusion_fade = allowsCameraOcclusionFade(scenery_material);
    object.spin_rate = spin_rate;
    const std::size_t index = appendObject(object);
    scenery_objects_.push_back(index);
    return index;
  };

  auto appendGeneratedScenery = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                                    const Vec3 position, const Vec3 scale, const Vec3 rotation,
                                    const Material &scenery_material,
                                    const float spin_rate = 0.0f) {
    RenderObject object;
    object.name = name;
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = mesh;
    object.transform.position = position;
    object.transform.scale = scale;
    object.transform.rotation = quatFromEulerXyz(rotation);
    object.material = scenery_material;
    object.camera_occlusion_fade = allowsCameraOcclusionFade(scenery_material);
    object.spin_rate = spin_rate;
    const std::size_t index = appendObject(object);
    scenery_objects_.push_back(index);
    return index;
  };

  auto appendCaveDebugOverlay = [&](const char *name, const MeshPrimitive primitive,
                                    const Vec3 position, const Vec3 scale, const Vec3 rotation,
                                    const Vec3 color, const std::uint32_t layer) {
    Material overlay_material = caveOverlayMaterial(color);
    const float visible_opacity = overlay_material.opacity;
    const float visible_emission = overlay_material.emission_strength;
    const std::size_t index =
        appendScenery(name, primitive, position, scale, rotation, overlay_material);
    if (index < scene_.objects().size()) {
      RenderObject &object = scene_.objects()[index];
      object.auto_contact_shadow = false;
      object.casts_contact_shadow = false;
      object.camera_occlusion_fade = false;
    }
    cave_debug_overlay_objects_.push_back(
        {index, layer, scale, visible_opacity, visible_emission});
    return index;
  };

  const auto keepCameraSolid = [&](const std::size_t object_index) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].camera_occlusion_fade = false;
    }
    return object_index;
  };

  const auto enableContactShadow = [&](const std::size_t object_index, const float strength,
                                       const float radius_scale = 1.0f) {
    if (object_index < scene_.objects().size()) {
      RenderObject &object = scene_.objects()[object_index];
      object.auto_contact_shadow = true;
      object.casts_contact_shadow = true;
      object.contact_shadow_strength = strength;
      object.contact_shadow_radius_scale = radius_scale;
    }
    return object_index;
  };

  const auto appendStructuralScenery = [&](const char *name, const MeshPrimitive primitive,
                                           const Vec3 position, const Vec3 scale,
                                           const Vec3 rotation, const Material &scenery_material) {
    return keepCameraSolid(
        appendScenery(name, primitive, position, scale, rotation, scenery_material));
  };

  const auto appendGeneratedStructuralScenery =
      [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh, const Vec3 position,
          const Vec3 scale, const Vec3 rotation, const Material &scenery_material) {
        return keepCameraSolid(
            appendGeneratedScenery(name, mesh, position, scale, rotation, scenery_material));
      };

  auto appendBeam = [&](const char *name, const Vec3 from, const Vec3 to, const float radius,
                        const Material &beam_material) {
    const float beam_length = std::max(length(to - from), 0.05f);
    return appendScenery(name, MeshPrimitive::Box, (from + to) * 0.5f,
                         {radius, beam_length, radius}, segmentRotation(from, to), beam_material);
  };

  Material prism_statue_stone = dark_masonry;
  prism_statue_stone.base_color = mixColor(prism_statue_stone.base_color, {0.18f, 0.205f, 0.20f},
                                           0.34f);
  prism_statue_stone.roughness = 0.88f;
  prism_statue_stone.metallic = 0.0f;
  prism_statue_stone.edge_wear = 0.38f;
  prism_statue_stone.ambient_occlusion = 0.78f;
  prism_statue_stone.procedural = {.macro_variation = 0.30f,
                                   .micro_normal_strength = 0.34f,
                                   .roughness_variation = 0.16f,
                                   .height_shading = 0.14f};
  prism_statue_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_statue_trim = pond_stone;
  prism_statue_trim.base_color = mixColor(prism_statue_trim.base_color, {0.26f, 0.30f, 0.30f},
                                          0.36f);
  prism_statue_trim.roughness = 0.82f;
  prism_statue_trim.edge_wear = 0.44f;
  prism_statue_trim.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_inlay = weathered_iron;
  prism_inlay.base_color = mixColor(prism_inlay.base_color, {0.08f, 0.12f, 0.13f}, 0.46f);
  prism_inlay.emission_color = {0.10f, 0.36f, 0.42f};
  prism_inlay.emission_strength = 0.08f;
  prism_inlay.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_core_material = amber_shard;
  prism_core_material.base_color = {0.34f, 0.78f, 1.0f};
  prism_core_material.emission_color = {0.30f, 0.78f, 1.0f};
  prism_core_material.emission_strength = 0.30f;
  prism_core_material.surface_pattern = SurfacePattern::AmberResin;
  prism_core_material.pattern_scale = {8.0f, 14.0f};
  prism_core_material.pattern_depth = 0.060f;
  prism_core_material.pattern_contrast = 0.95f;
  prism_core_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_conduit_material = prism_core_material;
  prism_conduit_material.base_color = {0.16f, 0.56f, 0.92f};
  prism_conduit_material.emission_color = {0.26f, 0.82f, 1.0f};
  prism_conduit_material.emission_strength = 0.46f;
  prism_conduit_material.opacity = 0.18f;
  prism_conduit_material.double_sided = true;
  prism_conduit_material.alpha_mode = MaterialAlphaMode::Blend;
  prism_conduit_material.depth_write = MaterialDepthWrite::Disabled;
  prism_conduit_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  prism_conduit_material.surface_pattern = SurfacePattern::FiberStrands;
  prism_conduit_material.pattern_scale = {10.0f, 18.0f};
  prism_conduit_material.pattern_depth = 0.030f;
  prism_conduit_material.procedural = {.macro_variation = 0.38f,
                                       .micro_normal_strength = 0.18f,
                                       .roughness_variation = 0.12f,
                                       .height_shading = 0.08f};

  const auto appendRelayStatuePart = [&](const char *name, const MeshPrimitive primitive,
                                         const Vec3 position, const Vec3 scale,
                                         const Vec3 rotation, const Material &part_material,
                                         const bool collision = false,
                                         const float shadow = 0.22f) {
    const std::size_t index =
        enableContactShadow(appendStructuralScenery(name, primitive, position, scale, rotation,
                                                    part_material),
                            shadow, 0.86f);
    if (collision) {
      scenery_collision_boxes_.push_back({position, scale * 0.50f});
      support_surfaces_.addBox({position, scale * 0.50f});
    }
    return index;
  };
  const Vec3 relay_core = prismRelayFocusPosition();
  appendRelayStatuePart("Prism relay statue square plinth", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 0.11f, 0.0f},
                        {1.45f, 0.22f, 1.45f}, {}, prism_statue_trim, true, 0.32f);
  appendRelayStatuePart("Prism relay statue octagonal riser", MeshPrimitive::Pillar,
                        prism_relay_base_ + Vec3{0.0f, 0.44f, 0.0f},
                        {0.78f, 0.58f, 0.78f}, {}, prism_statue_stone, true, 0.30f);
  appendRelayStatuePart("Prism relay statue engraved inlay", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 0.75f, 0.0f},
                        {0.92f, 0.045f, 0.92f}, {}, prism_inlay, false, 0.18f);
  appendRelayStatuePart("Prism relay statue torso", MeshPrimitive::Pillar,
                        prism_relay_base_ + Vec3{0.0f, 1.18f, 0.0f},
                        {0.34f, 0.82f, 0.28f}, {}, prism_statue_stone, false, 0.24f);
  appendRelayStatuePart("Prism relay statue shoulders", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 1.54f, 0.02f},
                        {0.82f, 0.16f, 0.28f}, {}, prism_statue_trim, false, 0.22f);
  appendRelayStatuePart("Prism relay statue head", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{0.0f, 1.83f, 0.0f},
                        {0.18f, 0.22f, 0.18f}, {}, prism_statue_stone, false, 0.20f);
  enableContactShadow(appendBeam("Prism relay statue left raised arm",
                                 prism_relay_base_ + Vec3{-0.34f, 1.50f, 0.02f},
                                 prism_relay_base_ + Vec3{-0.18f, 2.06f, 0.06f}, 0.060f,
                                 prism_statue_trim),
                      0.18f, 0.70f);
  enableContactShadow(appendBeam("Prism relay statue right raised arm",
                                 prism_relay_base_ + Vec3{0.34f, 1.50f, 0.02f},
                                 prism_relay_base_ + Vec3{0.18f, 2.06f, 0.06f}, 0.060f,
                                 prism_statue_trim),
                      0.18f, 0.70f);
  appendRelayStatuePart("Prism relay statue left crystal hand", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{-0.18f, 2.07f, 0.06f},
                        {0.075f, 0.075f, 0.075f}, {}, prism_inlay, false, 0.12f);
  appendRelayStatuePart("Prism relay statue right crystal hand", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{0.18f, 2.07f, 0.06f},
                        {0.075f, 0.075f, 0.075f}, {}, prism_inlay, false, 0.12f);
  prism_relay_core_object_ =
      appendScenery("Aster prism relay refractor core", MeshPrimitive::Crystal, relay_core,
                    {0.24f, 0.52f, 0.24f}, {0.0f, status_.elapsed_seconds, 0.0f},
                    prism_core_material, 0.36f);
  prism_relay_core_valid_ = true;
  prism_relay_ring_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay equatorial field ring",
      makeSharedMesh(
          makeEnergyConduitRingMesh({.radius = 0.56f, .band_width = 0.030f, .segments = 72})),
      relay_core, {1.0f, 1.0f, 1.0f}, {}, prism_conduit_material, 0.18f));
  prism_relay_ring_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay vertical field ring",
      makeSharedMesh(
          makeEnergyConduitRingMesh({.radius = 0.68f, .band_width = 0.026f, .segments = 72})),
      relay_core, {1.0f, 1.0f, 1.0f}, {radians(90.0f), 0.0f, radians(12.0f)},
      prism_conduit_material, -0.14f));

  std::vector<EnergyConduitRibbonSpec> relay_conduits;
  relay_conduits.reserve(3u);
  const auto addRelayConduit = [&](const Vec3 anchor, const float crest, const float width) {
    Vec3 planar_direction{anchor.x - relay_core.x, 0.0f, anchor.z - relay_core.z};
    if (length(planar_direction) <= 0.0001f) {
      planar_direction = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 start =
        relay_core + normalize(planar_direction) * 0.76f + Vec3{0.0f, -0.08f, 0.0f};
    const Vec3 midpoint = (start + anchor) * 0.5f + Vec3{0.0f, crest, 0.0f};
    relay_conduits.push_back({.points = {start, midpoint, anchor},
                              .width = width,
                              .crest_height = crest * 0.20f,
                              .subdivisions_per_segment = 24});
  };
  const Vec3 relay_left_anchor = prism_relay_base_ + Vec3{-0.92f, 0.78f, 0.26f};
  const Vec3 relay_right_anchor = prism_relay_base_ + Vec3{0.92f, 0.78f, 0.26f};
  const Vec3 relay_rear_anchor = prism_relay_base_ + Vec3{0.0f, 0.88f, -1.02f};
  addRelayConduit(relay_left_anchor, 0.36f, 0.070f);
  addRelayConduit(relay_right_anchor, 0.36f, 0.070f);
  addRelayConduit(relay_rear_anchor, 0.44f, 0.064f);
  prism_relay_conduit_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay conduit network",
      makeSharedMesh(makeEnergyConduitRibbonNetworkMesh(relay_conduits)),
      {}, {1.0f, 1.0f, 1.0f}, {}, prism_conduit_material));
  for (const Vec3 node : {relay_left_anchor, relay_right_anchor, relay_rear_anchor}) {
    prism_relay_node_objects_.push_back(appendScenery("Aster prism relay endpoint node",
                                                     MeshPrimitive::Sphere, node,
                                                     {0.095f, 0.095f, 0.095f}, {},
                                                     prism_core_material,
                                                     0.22f));
  }

  for (PlacedResourceRock &rock : placed_rocks_) {
    const ItemDefinition *definition = item_registry_.find(rock.item_id);
    if (rock.id.empty()) {
      rock.id = placedResourceId(placed_resource_serial_++);
    }
    if (definition != nullptr) {
      rock.placement_shape = definition->placement_shape;
    }
    const std::size_t index =
        keepCameraSolid(appendScenery("Placed cave resource rock",
                                      placementPrimitiveFor(rock.placement_shape), rock.position,
                                      rock.scale, rock.rotation,
                                      lumenPlacedResourceMaterial(definition)));
    rock.object_index = index;
    rock.body = {};
    if (index < scene_.objects().size()) {
      RenderObject &object = scene_.objects()[index];
      object.casts_contact_shadow = true;
      object.contact_shadow_strength = 0.18f;
      object.contact_shadow_radius_scale = 0.78f;
    }
    support_surfaces_.addBox({rock.position, rock.collision_half_extents});
  }

  const auto appendIndustrialWallLight = [&](const CaveWallFixturePlacement &placement,
                                             const Material &metal,
                                             const Material &lens) -> CaveWallFixtureVisual {
    CaveWallFixtureVisual visual;
    const Vec3 normal =
        length(placement.normal) > 0.0001f ? normalize(placement.normal) : Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 up =
        length(placement.up) > 0.0001f ? normalize(placement.up) : Vec3{0.0f, 1.0f, 0.0f};
    Vec3 right = normalize(cross(up, normal));
    if (length(right) <= 0.0001f) {
      right = {1.0f, 0.0f, 0.0f};
    }
    const float yaw = std::atan2(normal.x, normal.z);
    const Vec3 rotation{0.0f, yaw, 0.0f};
    const Vec3 base =
        length(placement.mount_position) > 0.0001f ? placement.mount_position : placement.position;
    visual.backplate = keepCameraSolid(appendScenery("Industrial red cave wall light backplate",
                                                     MeshPrimitive::Box, base + normal * 0.018f,
                                                     {0.48f, 0.30f, 0.036f}, rotation, metal));
    visual.lens = keepCameraSolid(appendScenery("Industrial red cave wall light glowing lens",
                                                MeshPrimitive::Box, placement.lens_position,
                                                {0.36f, 0.19f, 0.032f}, rotation, lens));
    if (visual.lens < scene_.objects().size()) {
      applyIndustrialLensColor(scene_.objects()[visual.lens].material, placement.light_color);
    }

    constexpr std::array<float, 5> kVerticalOffsets{-0.30f, -0.15f, 0.0f, 0.15f, 0.30f};
    for (std::size_t i = 0; i < visual.vertical_guards.size(); ++i) {
      const Vec3 rib_center = base + normal * 0.105f + right * kVerticalOffsets[i];
      visual.vertical_guards[i] = keepCameraSolid(
          appendBeam("Industrial red cave wall light vertical guard", rib_center - up * 0.235f,
                     rib_center + up * 0.235f, 0.018f, metal));
    }
    constexpr std::array<float, 3> kHorizontalOffsets{-0.17f, 0.0f, 0.17f};
    for (std::size_t i = 0; i < visual.horizontal_guards.size(); ++i) {
      const Vec3 rib_center = base + normal * 0.125f + up * kHorizontalOffsets[i];
      visual.horizontal_guards[i] = keepCameraSolid(
          appendBeam("Industrial red cave wall light horizontal guard", rib_center - right * 0.385f,
                     rib_center + right * 0.385f, 0.016f, metal));
    }
    return visual;
  };

  const auto appendWallDecal = [&](const char *name, const Vec3 surface, const Vec3 normal,
                                   const Vec3 up_hint, const Vec2 size, const Material &decal,
                                   const float lift = 0.018f) {
    const Vec3 n = length(normal) > 0.0001f ? normalize(normal) : Vec3{0.0f, 0.0f, 1.0f};
    (void)up_hint;
    const float yaw = std::atan2(n.x, n.z);
    const float pitch = std::atan2(-n.y, std::max(std::sqrt(n.x * n.x + n.z * n.z), 0.0001f));
    const Vec3 center = surface + n * lift;
    const Vec3 scale{std::max(size.x, 0.02f), std::max(size.y, 0.02f), 0.010f};
    keepCameraSolid(appendScenery(name, MeshPrimitive::Box, center, scale, {pitch, yaw, 0.0f},
                                  decal));
  };

  const auto appendFloorDecal = [&](const char *name, const Vec3 center, const Vec3 tangent,
                                    const Vec2 size, const Material &decal,
                                    const float lift = 0.020f) {
    const Vec3 t = length(tangent) > 0.0001f ? normalize(tangent) : Vec3{0.0f, 0.0f, -1.0f};
    const float yaw = std::atan2(t.x, t.z);
    const Vec3 scale{std::max(size.x, 0.04f), 0.010f, std::max(size.y, 0.04f)};
    keepCameraSolid(appendScenery(name, MeshPrimitive::Box, center + Vec3{0.0f, lift, 0.0f}, scale,
                                  {0.0f, yaw, 0.0f}, decal));
  };

  const auto appendCaveAgingPass = [&](const AuthoredCaveSection &section,
                                       const bool deep_section) {
    constexpr std::array<float, 7> kStations{0.12f, 0.22f, 0.34f, 0.48f, 0.60f, 0.74f, 0.88f};
    for (std::size_t i = 0; i < kStations.size(); ++i) {
      const CaveTunnelFrame frame = sampleCaveTunnelFrame(section.tunnel, kStations[i]);
      const float side_sign = (i % 2u == 0u) ? -1.0f : 1.0f;
      const Vec3 wall_normal = normalize(frame.side * side_sign + frame.up * 0.10f);
      const Vec3 wall_base =
          frame.floor_center + frame.side * (side_sign * frame.half_width * 0.86f) +
          frame.up * (0.58f + 0.18f * static_cast<float>(i % 3u));
      appendWallDecal(deep_section ? "Deep cave black wet wall streak"
                                   : "Entry cave black wet wall streak",
                      wall_base, wall_normal, frame.up,
                      {0.24f + 0.05f * static_cast<float>(i % 2u),
                       0.88f + 0.18f * static_cast<float>(i % 3u)},
                      cave_wet_streak, 0.020f);
      if (i % 2u == 1u) {
        appendWallDecal(deep_section ? "Deep cave green fungus patch"
                                     : "Entry cave green fungus patch",
                        wall_base - frame.up * 0.28f + frame.tangent * 0.12f, wall_normal,
                        frame.up, {0.46f, 0.28f}, cave_moss_patch, 0.022f);
      }
      if (i % 3u == 0u) {
        appendWallDecal("Old mining pick scratches in cave wall",
                        wall_base + frame.tangent * 0.20f + frame.up * 0.18f, wall_normal,
                        frame.up, {0.56f, 0.12f}, cave_scratch_mark, 0.024f);
      }

      const Vec3 floor_center =
          frame.floor_center + frame.tangent * (0.12f * static_cast<float>((i % 3u) - 1u));
      appendFloorDecal(deep_section ? "Deep cave wet floor patch" : "Entry cave wet floor patch",
                       floor_center, frame.tangent,
                       {frame.floor_half_width * 0.78f, 0.52f + 0.08f * static_cast<float>(i % 2u)},
                       (i % 2u == 0u) ? cave_wet_streak : cave_dust_accumulation, 0.018f);
    }
  };

  auto appendGeneratedBeam = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                                 const Vec3 from, const Vec3 to, const float radius,
                                 const Material &beam_material) {
    const float beam_length = std::max(length(to - from), 0.05f);
    return appendGeneratedScenery(name, mesh, (from + to) * 0.5f, {radius, beam_length, radius},
                                  segmentRotation(from, to), beam_material);
  };

  const auto appendGroundUnderlay = [&](const char *name,
                                        const std::shared_ptr<const CpuMesh> &mesh,
                                        const Vec3 position, const Material &underlay_material) {
    appendGeneratedScenery(name, mesh, position, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                           underlay_material);
    const Transform transform = Transform::fromEuler(position, {0.0f, 0.0f, 0.0f});
    support_surfaces_.addMesh({mesh, transform, 0.12f});
    decorative_ground_surfaces.addMesh({mesh, transform, 0.12f});
  };

  const std::size_t terrain_index = appendGeneratedScenery(
      "Surrounding heightfield terrain",
      terrainMesh(terrain_, terrain_holes, terrainPlacementClips(terrain_placement),
                  terrain_portal_clips),
      {0.0f, -0.012f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, layered_terrain);
  scene_.objects()[terrain_index].camera_occlusion_fade = false;

  for (const CastleCourseGroundZone &zone : course.ground_zones) {
    RenderObject floor;
    floor.name = "Castle interior stone floor";
    floor.primitive = MeshPrimitive::Plane;
    floor.transform.position = {castle_origin.x + zone.center.x, 0.010f,
                                castle_origin.z + zone.center.y};
    floor.custom_mesh =
        plazaDeckMesh(zone.half_extents * 2.0f, floor.transform.position, stone_floor_holes);
    floor.camera_occlusion_fade = false;
    floor.material = stone_plaza;
    appendObject(floor);
    support_surfaces_.addMesh({floor.custom_mesh, floor.transform, 0.50f});
    decorative_ground_surfaces.addMesh({floor.custom_mesh, floor.transform, 0.50f});
  }

  player_avatar_ = makePlushHumanoidAvatar({.height = tuning_.player_height * 1.24f,
                                            .fur_material = teddy_fur,
                                            .muzzle_material = teddy_muzzle,
                                            .eye_material = teddy_eye,
                                            .nose_material = teddy_nose});
  player_avatar_support_extent_ = avatarGroundSupportExtent(player_avatar_);
  player_avatar_instance_ = appendAvatar(
      scene_, player_avatar_, {.position = avatarPosePosition(), .facing_yaw = player_facing_yaw_});
  bool found_left_eye = false;
  bool found_right_eye = false;
  for (std::size_t i = 0;
       i < player_avatar_.parts.size() && i < player_avatar_instance_.object_indices.size(); ++i) {
    if (player_avatar_.parts[i].name == "left bead eye") {
      left_eye_object_ = player_avatar_instance_.object_indices[i];
      found_left_eye = true;
    } else if (player_avatar_.parts[i].name == "right bead eye") {
      right_eye_object_ = player_avatar_instance_.object_indices[i];
      found_right_eye = true;
    }
  }
  eye_objects_valid_ = found_left_eye && found_right_eye;
  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].camera_occlusion_fade = false;
      enableContactShadow(object_index, 0.42f, 0.92f);
    }
  }

  for (Shard &shard : shards_) {
    RenderObject object;
    object.name = "Veined amber shard";
    object.primitive = MeshPrimitive::Crystal;
    object.transform.scale = {tuning_.shard_radius * 0.82f, tuning_.shard_radius * 1.55f,
                              tuning_.shard_radius * 0.82f};
    object.material = amber_shard;
    shard.object_index = appendObject(object);
    enableContactShadow(shard.object_index, 0.38f, 0.72f);
  }

  for (Sentinel &sentinel : sentinels_) {
    RenderObject object;
    object.name = "Floodlight ward sentinel";
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = signalSentinelMesh();
    object.transform.scale = {tuning_.sentinel_radius * 1.35f, tuning_.sentinel_radius * 1.35f,
                              tuning_.sentinel_radius * 1.35f};
    object.material = sentinel_material;
    sentinel.object_index = appendObject(object);
    enableContactShadow(sentinel.object_index, 0.34f, 0.82f);
  }

  const auto castleMaterial = [&](const CastleCourseChannel channel) -> const Material & {
    switch (channel) {
    case CastleCourseChannel::Foundation:
      return stone_plaza;
    case CastleCourseChannel::WarmStone:
      return brick_facade;
    case CastleCourseChannel::CoolStone:
      return dark_masonry;
    case CastleCourseChannel::TrimStone:
      return pond_stone;
    case CastleCourseChannel::Wood:
      return sign_wood;
    case CastleCourseChannel::Iron:
      return weathered_iron;
    case CastleCourseChannel::WarmLight:
      return warm_window;
    }
    return brick_facade;
  };

  for (const VoxelMeshBatch &batch : course.structure.mesh_batches) {
    const CastleCourseChannel channel = static_cast<CastleCourseChannel>(batch.channel);
    const std::shared_ptr<const CpuMesh> mesh = makeSharedMesh(batch.mesh);
    appendGeneratedStructuralScenery(castleChannelName(channel), mesh, castle_origin,
                                     {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                     castleMaterial(channel));
    support_surfaces_.addMesh({mesh, Transform::fromEuler(castle_origin, {}), 0.55f});
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    appendStructuralScenery(castleChannelName(element.channel), MeshPrimitive::Box,
                            castle_origin + element.center, element.half_extents * 2.0f,
                            {0.0f, 0.0f, 0.0f}, castleMaterial(element.channel));
    if (element.support || element.collidable) {
      support_surfaces_.addBox({castle_origin + element.center, element.half_extents});
    }
  }
  appendGeneratedScenery("Jump sign text", parkourSignStrokeMesh(),
                         castle_origin + Vec3{0.0f, 3.34f, 3.43f}, {0.84f, 0.84f, 0.84f},
                         {0.0f, 0.0f, 0.0f}, sign_ink);

  const Vec3 cave_entrance = caveEntrancePlanar();
  const TerrainSurfaceSample cave_ground =
      sampleTerrain(terrain_, {cave_entrance.x, cave_entrance.z});
  const float cave_floor_y = (cave_ground.valid ? cave_ground.height : 0.0f) + 0.030f;
  const CaveComplexSpec cave_spec = authoredEntryCaveSpec(authoring_, terrain_, cave_floor_y);
  CaveComplex cave_complex = buildCaveComplex(cave_spec);
  const CaveComplexSpec deep_cave_spec = authoredDeepCaveSpec(authoring_, cave_spec.tunnel);
  CaveComplex deep_cave_complex = buildCaveComplex(deep_cave_spec);
  const LumenCaveWebPlacement web_placement = lumenCaveWebPlacement(cave_floor_y);
  CaveWebObstacle cave_web;
  cave_web.id = "lumen.cave_web.0";
  cave_web.center = web_placement.center;
  cave_web.normal =
      length(web_placement.normal) > 0.0001f ? normalize(web_placement.normal)
                                             : Vec3{0.0f, 0.0f, -1.0f};
  cave_web.side = length(web_placement.side) > 0.0001f ? normalize(web_placement.side)
                                                       : Vec3{1.0f, 0.0f, 0.0f};
  cave_web.up =
      length(web_placement.up) > 0.0001f ? normalize(web_placement.up) : Vec3{0.0f, 1.0f, 0.0f};
  cave_web.radius_x = web_placement.radius_x;
  cave_web.radius_y = web_placement.radius_y;
  cave_web.thickness = web_placement.thickness;
  cave_web.slow_scale = kCaveWebSlowScale;
  cave_web.hardness = 2.0f;
  cave_web.object_index = keepCameraSolid(appendGeneratedScenery(
      "Oval cave spider web span",
      makeSharedMesh(makeCaveWebMesh({.center = cave_web.center,
                                      .normal = cave_web.normal,
                                      .side = cave_web.side,
                                      .up = cave_web.up,
                                      .radius_x = cave_web.radius_x,
                                      .radius_y = cave_web.radius_y,
                                      .radial_strands = 26,
                                      .ring_strands = 8,
                                      .ring_segments = 112,
                                      .strand_width =
                                          std::max(0.010f, std::min(cave_web.radius_x,
                                                                    cave_web.radius_y) *
                                                            0.010f),
                                      .anchor_width_scale = 1.30f,
                                      .sag = cave_web.radius_y * 0.065f,
                                      .irregularity = 0.055f,
                                      .seed = kLumenCaveSeed + 7331u,
                                      .double_sided = true})),
      {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_web_material));
  if (cave_web.object_index < scene_.objects().size()) {
    scene_.objects()[cave_web.object_index].auto_contact_shadow = false;
  }
  cave_webs_.push_back(cave_web);
  appendCaveDebugOverlay(
      "Cave debug web interaction volume", MeshPrimitive::Box, cave_web.center,
      {cave_web.radius_x * 2.0f, cave_web.radius_y * 2.0f, std::max(cave_web.thickness, 0.04f)},
      {0.0f, std::atan2(cave_web.normal.x, cave_web.normal.z), 0.0f},
      {0.74f, 0.82f, 0.76f},
      static_cast<std::uint32_t>(CaveDebugOverlayLayer::Collision) |
          static_cast<std::uint32_t>(CaveDebugOverlayLayer::Interactable) |
          static_cast<std::uint32_t>(CaveDebugOverlayLayer::MiningTarget));
  const std::shared_ptr<const CpuMesh> cave_skitter_mesh =
      makeSharedMesh(makeCaveSkitterMesh({.body_segments = 34,
                                          .body_rings = 11,
                                          .leg_segments = 10,
                                          .body_length = 0.30f,
                                          .body_width = 0.17f,
                                          .body_height = 0.095f,
                                          .abdomen_length = 0.36f,
                                          .abdomen_width = 0.22f,
                                          .abdomen_height = 0.14f,
                                          .leg_span = 0.42f,
                                          .leg_lift = 0.050f,
                                          .fang_length = 0.070f,
                                          .eye_radius = 0.020f}));
  for (int skitter_index = 0; skitter_index < kCaveSkitterEncounterCount; ++skitter_index) {
    const float angle =
        (static_cast<float>(skitter_index) / static_cast<float>(kCaveSkitterEncounterCount)) *
            kPi * 2.0f +
        kPi * 0.18f;
    CaveSkitter skitter;
    skitter.id = "lumen.cave_skitter." + std::to_string(skitter_index);
    skitter.max_health = kCaveSkitterMaxHealth;
    skitter.health = skitter.max_health;
    skitter.state.home_offset = {std::cos(angle) * cave_web.radius_x * 0.38f,
                                 std::sin(angle) * cave_web.radius_y * 0.30f};
    skitter.state.position = cave_web.center + cave_web.side * skitter.state.home_offset.x +
                             cave_web.up * skitter.state.home_offset.y -
                             cave_web.normal * std::max(cave_web.thickness * 0.64f, 0.12f);
    skitter.state.facing_yaw = std::atan2(cave_web.normal.x, cave_web.normal.z);
    skitter.state.temperament =
        0.22f + 0.26f * static_cast<float>(skitter_index) +
        0.08f * std::sin(static_cast<float>(skitter_index) * 1.73f);
    skitter.object_index = appendGeneratedScenery("Cave skitter arachnid", cave_skitter_mesh,
                                                  skitter.state.position, {1.0f, 1.0f, 1.0f},
                                                  {0.0f, skitter.state.facing_yaw, 0.0f},
                                                  cave_skitter_material);
    enableContactShadow(skitter.object_index, 0.42f, 0.82f);
    appendCaveDebugOverlay("Cave debug skitter spawn volume", MeshPrimitive::Sphere,
                           skitter.state.position, {0.34f, 0.20f, 0.34f}, {},
                           {1.0f, 0.22f, 0.16f},
                           static_cast<std::uint32_t>(CaveDebugOverlayLayer::SpawnVolume));
  cave_skitters_.push_back(skitter);
  }
  const std::vector<CaveWallFixturePlacement> entry_fixtures =
      authoredFixturePlacements(authoring_, "entry", cave_spec.tunnel,
                                {lumenCaveWallFixtureProfile(),
                                 lumenCaveSecondaryWallFixtureProfile()});
  const std::vector<CaveWallFixturePlacement> deep_fixtures =
      authoredFixturePlacements(authoring_, "deep", deep_cave_spec.tunnel,
                                {lumenDeepCaveWallFixtureProfile(),
                                 lumenDeepCaveSecondaryWallFixtureProfile()});
  cave_sections_.push_back({.tunnel = cave_spec.tunnel,
                            .wall_fixtures = entry_fixtures,
                            .contributes_entrance_light = true});
  cave_sections_.push_back({.tunnel = deep_cave_spec.tunnel,
                            .wall_fixtures = deep_fixtures,
                            .contributes_entrance_light = false});
  cave_entrance_light_position_ = caveEntranceLightPosition(cave_floor_y);
  cave_viewer_cull_volume_ = viewerCullVolumeForMesh(cave_complex.collision_mesh, 0.20f,
                                                     FaceCullMode::Back, FaceCullMode::Back);
  cave_collision_meshes_.push_back(makeSharedMesh(std::move(cave_complex.collision_mesh)));
  cave_collision_meshes_.push_back(makeSharedMesh(std::move(deep_cave_complex.collision_mesh)));
  for (const AuthoredCaveSection &section : cave_sections_) {
    const CaveTunnelProfile &tunnel = section.tunnel;
    const Vec3 center = evaluateCaveTunnelCenter(tunnel, 0.5f);
    const float estimated_length = std::max(estimateCaveTunnelLength(tunnel), 0.50f);
    appendCaveDebugOverlay("Cave debug walkable route", MeshPrimitive::Box, center,
                           {std::max(tunnel.floor_width, 0.20f), 0.040f, estimated_length * 0.50f},
                           {0.0f, std::atan2(tunnel.end.x - tunnel.start.x,
                                             tunnel.end.z - tunnel.start.z),
                            0.0f},
                           {0.25f, 0.70f, 1.0f},
                           static_cast<std::uint32_t>(CaveDebugOverlayLayer::Walkable));
    appendCaveDebugOverlay(
        "Cave debug camera obstruction volume", MeshPrimitive::Box,
        center + Vec3{0.0f, std::max(tunnel.wall_height * 0.45f, 0.40f), 0.0f},
        {tunnel.half_width * 2.0f, std::max(tunnel.wall_height, 0.40f), estimated_length * 0.50f},
        {0.0f, std::atan2(tunnel.end.x - tunnel.start.x, tunnel.end.z - tunnel.start.z), 0.0f},
        {1.0f, 0.20f, 0.36f},
        static_cast<std::uint32_t>(CaveDebugOverlayLayer::CameraObstruction));
  }

  chest_base_ = cave_complex.chest_position;
  chest_yaw_ = cave_complex.chest_yaw;
  const auto chestPartPosition = [&](const Vec3 local_offset) {
    return chest_base_ + rotateYaw(local_offset, chest_yaw_);
  };
  const Material item_iron = weathered_iron;
  Material torch_flame_material = amber_lamp;
  torch_flame_material.base_color = {1.0f, 0.42f, 0.10f};
  torch_flame_material.emission_color = {1.0f, 0.34f, 0.08f};
  torch_flame_material.emission_strength = 0.78f;
  torch_flame_material.opacity = 0.94f;
  torch_flame_material.alpha_mode = MaterialAlphaMode::Blend;
  torch_flame_material.depth_write = MaterialDepthWrite::Disabled;
  torch_flame_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material dark_chest_interior =
      material({0.030f, 0.020f, 0.014f}, {0.020f, 0.010f, 0.004f}, 0.72f, 0.0f, 0.02f);

  const std::size_t parkour_chest_index = appendScenery(
      "Parkour starter chest base", MeshPrimitive::Box, chestPartPosition({0.0f, 0.19f, 0.0f}),
      {0.86f, 0.34f, 0.58f}, {0.0f, chest_yaw_, 0.0f}, sign_wood);
  enableContactShadow(parkour_chest_index, 0.58f, 0.88f);
  appendScenery("Chest dark open interior", MeshPrimitive::Box,
                chestPartPosition({0.0f, 0.39f, 0.0f}), {0.76f, 0.065f, 0.48f},
                {0.0f, chest_yaw_, 0.0f}, dark_chest_interior);
  chest_lid_object_ = appendScenery("Animated chest lid", MeshPrimitive::Box,
                                    chestPartPosition({0.0f, 0.49f, 0.0f}), {0.88f, 0.17f, 0.62f},
                                    {0.0f, chest_yaw_, 0.0f}, sign_wood);
  chest_lid_band_object_ = appendScenery(
      "Chest moving iron band front", MeshPrimitive::Box, chestPartPosition({0.0f, 0.44f, 0.326f}),
      {0.72f, 0.050f, 0.028f}, {0.0f, chest_yaw_, 0.0f}, weathered_iron);
  chest_lock_object_ =
      appendScenery("Chest moving blackened iron lock", MeshPrimitive::Box,
                    chestPartPosition({0.0f, 0.36f, 0.354f}), {0.105f, 0.14f, 0.030f},
                    {0.0f, chest_yaw_, 0.0f}, weathered_iron);
  appendScenery("Chest blackened iron band front", MeshPrimitive::Box,
                chestPartPosition({0.0f, 0.25f, 0.294f}), {0.72f, 0.052f, 0.030f},
                {0.0f, chest_yaw_, 0.0f}, weathered_iron);
  appendCaveDebugOverlay("Cave debug chest collision", MeshPrimitive::Box,
                         chest_base_ + Vec3{0.0f, 0.25f, 0.0f},
                         {0.80f, 0.50f, 0.60f}, {0.0f, chest_yaw_, 0.0f}, {1.0f, 0.78f, 0.22f},
                         static_cast<std::uint32_t>(CaveDebugOverlayLayer::Collision) |
                             static_cast<std::uint32_t>(CaveDebugOverlayLayer::Interactable));

  const auto appendChestItemPart = [&](ChestItemVisual &item, const char *name,
                                       const MeshPrimitive primitive, const Vec3 local_position,
                                       const Vec3 scale, const Vec3 local_rotation,
                                       const Material &part_material) {
    const std::size_t index = appendScenery(
        name, primitive, chestPartPosition(local_position), scale,
        {local_rotation.x, chest_yaw_ + local_rotation.y, local_rotation.z}, part_material);
    item.parts.push_back(
        {index, local_position, local_rotation, scale, part_material.emission_strength});
    return index;
  };
  chest_items_.push_back({"pickaxe", {-0.23f, 0.58f, 0.02f}, 0.34f, true, {}});
  ChestItemVisual &pickaxe_item = chest_items_.back();
  appendChestItemPart(pickaxe_item, "Chest pickaxe handle", MeshPrimitive::Box,
                      {-0.27f, 0.57f, -0.01f}, {0.040f, 0.46f, 0.040f},
                      segmentRotation({-0.45f, 0.47f, -0.14f}, {-0.09f, 0.67f, 0.14f}), sign_wood);
  appendChestItemPart(pickaxe_item, "Chest pickaxe head", MeshPrimitive::Box,
                      {-0.07f, 0.69f, 0.17f}, {0.38f, 0.060f, 0.065f},
                      {radians(4.0f), radians(54.0f), radians(-7.0f)}, item_iron);

  chest_items_.push_back({"torch", {0.08f, 0.61f, 0.02f}, 0.34f, true, {}});
  ChestItemVisual &torch_item = chest_items_.back();
  appendChestItemPart(torch_item, "Chest torch wrapped handle", MeshPrimitive::Box,
                      {0.05f, 0.57f, -0.04f}, {0.048f, 0.43f, 0.048f},
                      segmentRotation({-0.02f, 0.46f, -0.17f}, {0.14f, 0.68f, 0.13f}), sign_wood);
  appendChestItemPart(torch_item, "Chest torch ember head", MeshPrimitive::Sphere,
                      {0.16f, 0.71f, 0.16f}, {0.075f, 0.095f, 0.075f}, {0.0f, 0.0f, 0.0f},
                      torch_flame_material);

  chest_items_.push_back({"fishing_rod", {0.34f, 0.58f, 0.00f}, 0.36f, true, {}});
  ChestItemVisual &rod_item = chest_items_.back();
  appendChestItemPart(rod_item, "Chest fishing rod blank", MeshPrimitive::Box,
                      {0.31f, 0.58f, -0.01f}, {0.024f, 0.72f, 0.024f},
                      segmentRotation({0.05f, 0.47f, -0.21f}, {0.58f, 0.70f, 0.20f}),
                      fishing_rod_material);
  appendChestItemPart(rod_item, "Chest fishing reel", MeshPrimitive::Sphere, {0.18f, 0.55f, -0.11f},
                      {0.100f, 0.100f, 0.055f}, {0.0f, 0.0f, 0.0f}, item_iron);

  const auto appendEquippedPart = [&](const std::string &item_id, const char *name,
                                      const MeshPrimitive primitive, const Vec3 local_position,
                                      const Vec3 scale, const Vec3 local_rotation,
                                      const Material &part_material) {
    const std::size_t index =
        appendScenery(name, primitive, {0.0f, -20.0f, 0.0f}, {0.001f, 0.001f, 0.001f},
                      local_rotation, part_material);
    equipped_item_parts_.push_back({item_id, index, local_position, local_rotation, scale});
  };
  appendEquippedPart("pickaxe", "Equipped pickaxe handle", MeshPrimitive::Box, {0.0f, 0.17f, 0.0f},
                     {0.032f, 0.38f, 0.032f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendEquippedPart("pickaxe", "Equipped pickaxe head", MeshPrimitive::Box, {0.0f, 0.36f, 0.0f},
                     {0.31f, 0.052f, 0.056f}, {0.0f, radians(90.0f), 0.0f}, item_iron);
  appendEquippedPart("torch", "Equipped torch wrapped handle", MeshPrimitive::Box,
                     {0.0f, 0.16f, 0.0f}, {0.040f, 0.34f, 0.040f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendEquippedPart("torch", "Equipped torch ember core", MeshPrimitive::Crystal,
                     {0.0f, 0.34f, 0.0f}, {0.086f, 0.126f, 0.086f},
                     {radians(0.0f), radians(0.0f), radians(0.0f)}, torch_flame_material);
  appendEquippedPart("fishing_rod", "Equipped fishing rod blank", MeshPrimitive::Box,
                     {0.0f, 0.30f, 0.0f}, {0.016f, 0.68f, 0.016f},
                     {radians(-5.0f), 0.0f, radians(7.0f)}, fishing_rod_material);
  appendEquippedPart("fishing_rod", "Equipped fishing reel", MeshPrimitive::Sphere,
                     {0.048f, 0.12f, 0.0f}, {0.070f, 0.070f, 0.038f}, {0.0f, 0.0f, 0.0f},
                     item_iron);
  for (std::size_t i = 0; i < 8u; ++i) {
    const std::size_t index =
        appendScenery("Equipped torch fire particle", MeshPrimitive::Crystal, {0.0f, -20.0f, 0.0f},
                      {0.001f, 0.001f, 0.001f}, {}, torch_flame_material);
    torch_particle_visuals_.push_back({index});
  }
  for (std::size_t i = 0; i < kMiningFractureShardVisualPoolSize; ++i) {
    const std::size_t index =
        appendScenery("Mesh-cut mining fragment", MeshPrimitive::Rock, {0.0f, -24.0f, 0.0f},
                      {0.001f, 0.001f, 0.001f}, {}, cave_talus);
    scene_.objects()[index].camera_occlusion_fade = false;
    mining_fracture_shards_.push_back({.object_index = index});
  }

  support_surfaces_.addBox({chest_base_ + Vec3{0.0f, 0.25f, 0.0f}, {0.40f, 0.25f, 0.30f}});
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    appendGeneratedStructuralScenery(portal.name, gothicUnderpassMesh(), portal.position,
                                     portal.scale, {0.0f, 0.0f, 0.0f}, dark_masonry);
  }

  support_surfaces_.addMesh(
      {terrainTransitionMesh(), Transform::fromEuler(pond_center_, {}), 0.35f});
  decorative_ground_surfaces.addMesh(
      {terrainTransitionMesh(), Transform::fromEuler(pond_center_, {}), 0.35f});
  appendGroundUnderlay("Courtyard pond hardscape underlay", pondHardscapeUnderlayMesh(),
                       {pond_center_.x, -0.042f, pond_center_.z}, hardscape_substrate);
  support_surfaces_.addMesh({moundMesh(), Transform::fromEuler(pond_center_, {}), 0.25f});
  decorative_ground_surfaces.addMesh(
      {moundMesh(), Transform::fromEuler(pond_center_, {}), 0.25f});
  support_surfaces_.addMesh({pondHardscapeSubstrateMesh(),
                             Transform::fromEuler(pond_center_, {}),
                             0.18f});
  decorative_ground_surfaces.addMesh({pondHardscapeSubstrateMesh(),
                                      Transform::fromEuler(pond_center_, {}),
                                      0.18f});
  support_surfaces_.addMesh({innerPondTransitionMesh(),
                             Transform::fromEuler(inner_pond_center_, {}),
                             0.35f});
  decorative_ground_surfaces.addMesh({innerPondTransitionMesh(),
                                      Transform::fromEuler(inner_pond_center_, {}),
                                      0.35f});
  appendGroundUnderlay("Inner pond hardscape underlay", innerPondHardscapeUnderlayMesh(),
                       {inner_pond_center_.x, -0.044f, inner_pond_center_.z}, hardscape_substrate);
  support_surfaces_.addMesh(
      {innerPondMoundMesh(), Transform::fromEuler(inner_pond_center_, {}), 0.25f});
  decorative_ground_surfaces.addMesh(
      {innerPondMoundMesh(), Transform::fromEuler(inner_pond_center_, {}), 0.25f});
  support_surfaces_.addMesh({innerPondHardscapeSubstrateMesh(),
                             Transform::fromEuler(inner_pond_center_, {}),
                             0.18f});
  decorative_ground_surfaces.addMesh({innerPondHardscapeSubstrateMesh(),
                                      Transform::fromEuler(inner_pond_center_, {}),
                                      0.18f});

  const Vec3 bird_grove_center{bird_grove_clip.center.x, 0.0f, bird_grove_clip.center.y};
  appendGeneratedScenery("Bird grove feathered transition", birdGroveTransitionMesh(),
                         bird_grove_center, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         terrain_transition);
  support_surfaces_.addMesh({birdGroveTransitionMesh(),
                             Transform::fromEuler(bird_grove_center, {}),
                             0.32f});
  decorative_ground_surfaces.addMesh({birdGroveTransitionMesh(),
                                      Transform::fromEuler(bird_grove_center, {}),
                                      0.32f});
  appendGeneratedScenery("Bird grove grass island", birdGroveGroundMesh(), bird_grove_center,
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, grass_soil);
  support_surfaces_.addMesh(
      {birdGroveGroundMesh(), Transform::fromEuler(bird_grove_center, {}), 0.25f});
  decorative_ground_surfaces.addMesh(
      {birdGroveGroundMesh(), Transform::fromEuler(bird_grove_center, {}), 0.25f});
  const TerrainSurfaceSample bird_tree_ground =
      decorative_ground_surfaces.sample(Vec2{bird_grove_center.x, bird_grove_center.z});
  const Vec3 bird_tree_base{bird_grove_center.x,
                            (bird_tree_ground.valid ? bird_tree_ground.height : 0.0f) + 0.010f,
                            bird_grove_center.z};
  castle_bird_nest_position_ = bird_tree_base + Vec3{0.0f, 3.56f, 0.0f};
  castle_bird_flight_center_ = {0.0f, castle_bird_nest_position_.y + 0.32f, -15.80f};

  const auto groundDrapeSurface = [&decorative_ground_surfaces](const Vec2 position) {
    return decorative_ground_surfaces.sample(
        {.world_position = position, .reference_y = 0.08f, .max_above = 0.54f, .max_below = 1.20f});
  };
  const MeshSurfaceSampler terrainOnlyDrapeSurface = [this](const Vec2 position) {
    return sampleTerrain(terrain_, position);
  };
  const GrassSurfaceSampler decorativeGrassSurface = groundDrapeSurface;
  const GrassSurfaceSampler terrainGrassSurface = terrainOnlyDrapeSurface;
  const auto appendPathShouldersOn = [&](const char *name, const PathRibbonMeshSpec &path_spec,
                                         const float width, const float height,
                                         const MeshSurfaceSampler &surface_sampler,
                                         const float surface_offset = 0.006f) {
    const std::shared_ptr<const CpuMesh> shoulder_mesh = makeSharedMesh(drapeMeshToSurface(
        makePathShoulderMesh(pathShoulderSpec(path_spec, width, height)), surface_sampler,
        {.surface_offset = surface_offset,
         .raise_only = false,
         .preserve_vertical_offset = true,
         .reference_y = 0.0f}));
    appendGeneratedScenery(name, shoulder_mesh, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f}, grass_soil);
    support_surfaces_.addMesh({shoulder_mesh, {}, 0.32f});
  };
  const auto appendPathShoulders = [&](const char *name, const PathRibbonMeshSpec &path_spec,
                                       const float width, const float height) {
    appendPathShouldersOn(name, path_spec, width, height, groundDrapeSurface);
  };
  const auto appendPathRouteShouldersOn =
      [&](const char *name, const std::vector<PathRibbonMeshSpec> &path_specs, const float width,
          const float height, const MeshSurfaceSampler &surface_sampler,
          const float surface_offset = 0.006f) {
        PathShoulderRouteMeshSpec route_spec;
        route_spec.segments.reserve(path_specs.size());
        for (const PathRibbonMeshSpec &path_spec : path_specs) {
          route_spec.segments.push_back(pathShoulderSpec(path_spec, width, height));
        }
        const std::shared_ptr<const CpuMesh> shoulder_mesh = makeSharedMesh(
            drapeMeshToSurface(makePathRouteShoulderMesh(std::move(route_spec)), surface_sampler,
                               {.surface_offset = surface_offset,
                                .raise_only = false,
                                .preserve_vertical_offset = true,
                                .reference_y = 0.0f}));
        appendGeneratedScenery(name, shoulder_mesh, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                               {0.0f, 0.0f, 0.0f}, grass_soil);
        support_surfaces_.addMesh({shoulder_mesh, {}, 0.32f});
      };
  const auto appendSoilPathOn = [&](const char *name, CpuMesh path_mesh,
                                    const MeshSurfaceSampler &surface_sampler,
                                    const float surface_offset = 0.012f) {
    const std::shared_ptr<const CpuMesh> draped_path =
        makeSharedMesh(drapeMeshToSurface(std::move(path_mesh), surface_sampler,
                                          {.surface_offset = surface_offset, .raise_only = false}));
    appendGeneratedScenery(name, draped_path, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f}, soil_path);
    support_surfaces_.addMesh({draped_path, {}, 0.40f});
    return draped_path;
  };
  const auto appendSoilPath = [&](const char *name, CpuMesh path_mesh) {
    return appendSoilPathOn(name, std::move(path_mesh), groundDrapeSurface);
  };
  const auto appendGrassFieldOn = [&](const char *name, GrassFieldScatterSpec scatter_spec,
                                      const Material &field_material,
                                      const GrassSurfaceSampler &surface_sampler) {
    const std::vector<GrassBladeAnchor> anchors =
        scatterGrassFieldAnchors(scatter_spec, surface_sampler);
    CpuMesh field_mesh = makeGrassFieldMesh(
        anchors, {.blade_segments = 4,
                  .double_sided = true,
                  .root_ao = 0.58f,
                  .mid_ao = 0.84f,
                  .tip_ao = 1.0f,
                  .width_taper = 1.22f,
                  .lateral_sway = 0.17f});
    if (field_mesh.vertices.empty() || field_mesh.indices.empty()) {
      return scene_.objects().size();
    }
    return appendGeneratedScenery(name, makeSharedMesh(std::move(field_mesh)), {0.0f, 0.0f, 0.0f},
                                  {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, field_material);
  };
  const auto appendGrassField = [&](const char *name, GrassFieldScatterSpec scatter_spec,
                                    const Material &field_material) {
    return appendGrassFieldOn(name, std::move(scatter_spec), field_material, decorativeGrassSurface);
  };

  const std::vector<PathRibbonMeshSpec> cave_path_specs = caveApproachPathSpecs();
  const std::vector<PathRibbonMeshSpec> abandoned_house_path_specs =
      abandonedGothicHousePathSpecs();
  appendPathRouteShouldersOn("Mounded exterior verge to cave", cave_path_specs, 0.56f, 0.088f,
                             terrainOnlyDrapeSurface, 0.034f);
  appendSoilPathOn("Curving exterior soil path to cave",
                   makePathRouteRibbonMesh({.segments = cave_path_specs}), terrainOnlyDrapeSurface,
                   0.074f);
  appendPathRouteShouldersOn("Mounded exterior verge to gothic stone house",
                             abandoned_house_path_specs, 0.46f, 0.076f, terrainOnlyDrapeSurface,
                             0.030f);
  appendSoilPathOn("Right branch soil path to gothic stone house",
                   makePathRouteRibbonMesh({.segments = abandoned_house_path_specs}),
                   terrainOnlyDrapeSurface, 0.072f);
  std::vector<PathRibbonMeshSpec> central_grass_paths = cave_path_specs;
  central_grass_paths.insert(central_grass_paths.end(), abandoned_house_path_specs.begin(),
                             abandoned_house_path_specs.end());
  central_grass_paths.push_back(castleUnderpassPathSpec());
  central_grass_paths.push_back(castleNestPathSpec());
  central_grass_paths.push_back(naturePathSpec(wetland_clips.front()));
  appendGrassFieldOn(
      "Engine central terrain grass field",
      {.min = {-14.5f, -12.5f},
       .max = {15.5f, 14.5f},
       .seed = kLumenCaveSeed + 2401u,
       .target_blades = 3400,
       .max_blades = 3400,
       .min_spacing = 0.058f,
       .surface_offset = 0.018f,
       .min_height = 0.14f,
       .max_height = 0.38f,
       .min_width = 0.009f,
       .max_width = 0.024f,
       .max_bend = 0.104f,
       .max_lean = 0.046f,
       .density_noise_scale = 0.66f,
       .density_noise_contrast = 0.36f,
       .min_surface_normal_y = 0.54f,
       .preferred_surface_normal_y = 0.86f,
       .accepts_position =
           [central_grass_paths, terrain_placement, this](const Vec2 position) {
             const TerrainSurfaceSample sample = sampleTerrain(terrain_, position);
             if (!sample.valid ||
                 terrain_placement.rejectsPoint({position.x, sample.height, position.y})) {
               return false;
             }
             return length(position) < 18.0f &&
                    distanceToPathRoute(central_grass_paths, position) > 0.62f;
           }},
      grass_blades, terrainGrassSurface);

  const auto appendGothicStoneHouse = [&]() {
    if (abandoned_house_path_specs.empty()) {
      return;
    }

    const PathRibbonMeshSpec &approach = abandoned_house_path_specs.back();
    Vec3 forward = evaluatePathRibbonTangent(approach, 1.0f);
    forward.y = 0.0f;
    if (length(forward) <= 0.0001f) {
      forward = {0.0f, 0.0f, 1.0f};
    }
    forward = normalize(forward);
    Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, forward));
    if (length(side) <= 0.0001f) {
      side = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 doorstep = evaluatePathRibbonCenter(approach, 1.0f);
    Vec3 base = doorstep + forward * 3.05f;
    const TerrainSurfaceSample ground = terrainOnlyDrapeSurface({base.x, base.z});
    base.y = (ground.valid ? ground.height : doorstep.y) + 0.035f;
    const float yaw = std::atan2(forward.x, forward.z);
    const Vec3 house_rotation{0.0f, yaw, 0.0f};

    Material gothic_stone = cave_mouth_stone;
    gothic_stone.base_color = mixColor(gothic_stone.base_color, {0.22f, 0.23f, 0.21f}, 0.46f);
    gothic_stone.edge_wear = 0.34f;
    gothic_stone.ambient_occlusion = 0.86f;
    gothic_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
    Material gothic_dark_stone = gothic_stone;
    gothic_dark_stone.base_color = mixColor(gothic_dark_stone.base_color, {0.10f, 0.105f, 0.10f},
                                            0.36f);
    gothic_dark_stone.pattern_depth = 0.12f;
    Material gothic_timber = sign_wood;
    gothic_timber.base_color = mixColor(gothic_timber.base_color, {0.18f, 0.10f, 0.055f}, 0.42f);
    gothic_timber.surface_pattern = SurfacePattern::PaintedWood;
    gothic_timber.pattern_scale = {5.4f, 13.0f};
    gothic_timber.pattern_depth = 0.050f;
    gothic_timber.detail_strength = 0.58f;
    gothic_timber.ambient_occlusion = 0.84f;
    gothic_timber.camera_occlusion = CameraOcclusionPolicy::Solid;
    Material warm_glass = warm_window;
    warm_glass.emission_strength = 0.42f;
    warm_glass.opacity = 0.72f;
    warm_glass.alpha_mode = MaterialAlphaMode::Blend;
    warm_glass.depth_write = MaterialDepthWrite::Disabled;
    warm_glass.camera_occlusion = CameraOcclusionPolicy::Solid;

    const auto local = [&](const float x, const float y, const float z) {
      return base + side * x + Vec3{0.0f, y, 0.0f} + forward * z;
    };
    const auto worldHalfExtents = [&](const Vec3 scale) {
      const Vec3 half = scale * 0.5f;
      return Vec3{std::abs(side.x) * half.x + std::abs(forward.x) * half.z,
                  half.y,
                  std::abs(side.z) * half.x + std::abs(forward.z) * half.z};
    };
    const auto addBoxSupport = [&](const Vec3 center, const Vec3 scale) {
      support_surfaces_.addBox({center, worldHalfExtents(scale)});
    };
    const auto addBoxCollision = [&](const Vec3 center, const Vec3 scale) {
      scenery_collision_boxes_.push_back({center, worldHalfExtents(scale)});
    };
    const auto appendHouseBox = [&](const char *name, const Vec3 center, const Vec3 scale,
                                    const Vec3 rotation, const Material &house_material,
                                    const bool support, const bool collision) {
      const std::size_t index =
          enableContactShadow(appendStructuralScenery(name, MeshPrimitive::Box, center, scale,
                                                      rotation, house_material),
                              0.22f, 0.86f);
      if (support) {
        addBoxSupport(center, scale);
      }
      if (collision) {
        addBoxCollision(center, scale);
      }
      return index;
    };
    const auto appendHouseStone = [&](const char *name, const MeshPrimitive primitive,
                                      const Vec3 center, const Vec3 scale, const Vec3 rotation,
                                      const bool collision) {
      const std::size_t index =
          enableContactShadow(appendStructuralScenery(name, primitive, center, scale, rotation,
                                                      gothic_stone),
                              0.20f, 0.82f);
      if (collision) {
        addBoxCollision(center, scale);
      }
      return index;
    };
    const auto appendHouseBeam = [&](const char *name, const Vec3 from, const Vec3 to,
                                     const float radius) {
      const std::size_t index = enableContactShadow(appendBeam(name, from, to, radius, gothic_timber),
                                                    0.18f, 0.74f);
      return index;
    };

    appendHouseBox("Gothic stone house ground floor slabs", local(0.0f, 0.06f, 0.0f),
                   {5.30f, 0.12f, 5.80f}, house_rotation, gothic_dark_stone, true, true);
    appendHouseBox("Gothic stone house second floor slabs", local(0.0f, 1.48f, 0.18f),
                   {4.86f, 0.12f, 5.12f}, house_rotation, gothic_dark_stone, true, true);

    appendHouseBox("Gothic house rear wall lower", local(0.0f, 0.82f, 2.74f),
                   {5.34f, 1.52f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house left wall lower", local(-2.66f, 0.78f, 0.10f),
                   {0.28f, 1.46f, 5.20f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house right wall lower", local(2.66f, 0.78f, 0.10f),
                   {0.28f, 1.46f, 5.20f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house front wall lower left", local(-1.82f, 0.86f, -2.76f),
                   {1.42f, 1.56f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house front wall lower right", local(1.82f, 0.86f, -2.76f),
                   {1.42f, 1.56f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseStone("Gothic pointed doorway left jamb", MeshPrimitive::Pillar,
                     local(-0.66f, 0.86f, -2.86f), {0.22f, 1.55f, 0.22f}, house_rotation, true);
    appendHouseStone("Gothic pointed doorway right jamb", MeshPrimitive::Pillar,
                     local(0.66f, 0.86f, -2.86f), {0.22f, 1.55f, 0.22f}, house_rotation, true);
    appendHouseStone("Gothic pointed doorway keystone", MeshPrimitive::RuinBlock,
                     local(0.0f, 1.64f, -2.86f), {0.84f, 0.30f, 0.28f},
                     {0.0f, yaw, radians(45.0f)}, false);

    appendHouseBox("Gothic house rear wall upper", local(0.0f, 2.14f, 2.62f),
                   {5.04f, 1.20f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house left wall upper", local(-2.48f, 2.12f, 0.12f),
                   {0.24f, 1.16f, 4.82f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house right wall upper", local(2.48f, 2.12f, 0.12f),
                   {0.24f, 1.16f, 4.82f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house upper front left", local(-1.54f, 2.14f, -2.56f),
                   {1.38f, 1.18f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house upper front right", local(1.54f, 2.14f, -2.56f),
                   {1.38f, 1.18f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseStone("Gothic upper pointed window stone peak", MeshPrimitive::RuinBlock,
                     local(0.0f, 2.48f, -2.66f), {0.72f, 0.24f, 0.20f},
                     {0.0f, yaw, radians(45.0f)}, false);

    for (const float x : {-1.95f, 1.95f}) {
      appendHouseStone("Gothic stone side buttress", MeshPrimitive::RuinBlock,
                       local(x, 0.78f, -2.18f), {0.34f, 1.35f, 0.40f}, house_rotation, true);
      appendHouseStone("Gothic stone rear buttress", MeshPrimitive::RuinBlock,
                       local(x, 0.82f, 2.96f), {0.36f, 1.42f, 0.38f}, house_rotation, true);
      appendHouseBeam("Gothic vertical timber brace", local(x, 0.30f, -2.58f),
                      local(x, 2.68f, -2.58f), 0.060f);
    }

    appendHouseBeam("Gothic interior stair rail", local(1.86f, 0.42f, -1.82f),
                    local(0.76f, 1.68f, 0.80f), 0.044f);
    for (int i = 0; i < 7; ++i) {
      const float t = static_cast<float>(i) / 6.0f;
      const Vec3 step_center =
          local(1.88f - t * 1.12f, 0.22f + t * 1.08f, -1.82f + t * 2.62f);
      const Vec3 step_scale{0.78f, 0.12f, 0.42f};
      appendHouseBox("Gothic stone stair tread", step_center, step_scale, house_rotation,
                     gothic_dark_stone, true, true);
    }

    appendHouseBox("Gothic left slate roof plane", local(-1.12f, 3.02f, 0.0f),
                   {3.16f, 0.16f, 5.94f}, {0.0f, yaw, radians(-24.0f)}, gothic_dark_stone,
                   false, false);
    appendHouseBox("Gothic right slate roof plane", local(1.12f, 3.02f, 0.0f),
                   {3.16f, 0.16f, 5.94f}, {0.0f, yaw, radians(24.0f)}, gothic_dark_stone,
                   false, false);
    appendHouseBeam("Gothic roof ridge beam", local(0.0f, 3.38f, -2.82f),
                    local(0.0f, 3.30f, 2.88f), 0.070f);
    for (const float z : {-2.10f, -0.70f, 0.70f, 2.10f}) {
      appendHouseBeam("Gothic left roof spar", local(-2.50f, 2.58f, z),
                      local(0.0f, 3.36f, z), 0.052f);
      appendHouseBeam("Gothic right roof spar", local(2.50f, 2.58f, z),
                      local(0.0f, 3.36f, z), 0.052f);
    }

    for (const Vec3 window : {Vec3{-2.72f, 1.05f, -0.92f}, Vec3{2.72f, 1.02f, 1.02f},
                             Vec3{0.0f, 2.16f, -2.72f}, Vec3{0.0f, 2.20f, 2.76f}}) {
      appendHouseBox("Gothic narrow amber window", local(window.x, window.y, window.z),
                     {0.08f, 0.62f, 0.42f}, house_rotation, warm_glass, false, false);
    }
  };
  appendGothicStoneHouse();

  const Vec3 cave_inward = caveInwardDirection();
  const float cave_yaw = std::atan2(-cave_inward.x, -cave_inward.z);
  supply_crate_base_ = cave_entrance + Vec3{-1.34f, cave_floor_y + 0.28f, 1.18f};
  if (authoring_.valid) {
    if (const sdk::CavePlacementDocument *placement = findCavePlacement(authoring_.cave, "supply_chest");
        placement != nullptr) {
      supply_crate_base_ = toRuntimeVec(placement->position);
      if (placement->floor_relative) {
        supply_crate_base_.y += cave_floor_y;
      }
    }
  }
  if (const TerrainSurfaceSample supply_ground =
          groundDrapeSurface({supply_crate_base_.x, supply_crate_base_.z});
      supply_ground.valid) {
    supply_crate_base_.y = supply_ground.height + 0.28f;
  }
  supply_crate_yaw_ = cave_yaw + radians(8.0f);
  appendGroundUnderlay("Cave mouth fractured talus apron",
                       makeSharedMesh(makePathJunctionMesh({.radial_segments = 128,
                                                            .rings = 7,
                                                            .radius_x = 2.25f,
                                                            .radius_z = 2.10f,
                                                            .crown_height = 0.038f,
                                                            .surface_noise = 0.026f,
                                                            .edge_irregularity = 0.18f,
                                                            .radius_x_negative = 2.60f,
                                                            .radius_x_positive = 2.85f,
                                                            .radius_z_negative = 1.58f,
                                                            .radius_z_positive = 3.85f,
                                                            .edge_skirt_depth = 0.045f})),
                       cave_entrance + Vec3{0.08f, cave_floor_y + 0.060f, 0.36f}, cave_talus);
  const std::shared_ptr<const CpuMesh> cave_portal_seal_mesh =
      makeSharedMesh(std::move(cave_complex.portal_seal_mesh));
  appendGeneratedStructuralScenery("Opaque cave portal terrain seal", cave_portal_seal_mesh,
                                   {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                   cave_talus);
  support_surfaces_.addMesh({cave_portal_seal_mesh, {}, 0.42f});
  decorative_ground_surfaces.addMesh({cave_portal_seal_mesh, {}, 0.42f});
  const std::shared_ptr<const CpuMesh> cave_portal_mesh =
      makeSharedMesh(std::move(cave_complex.portal_mesh));
  appendGeneratedStructuralScenery(
      "Natural embedded cave mouth", cave_portal_mesh,
      {cave_entrance.x, cave_floor_y + 0.015f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_portal_blend_mesh =
      makeSharedMesh(std::move(cave_complex.portal_blend_mesh));
  appendGeneratedStructuralScenery(
      "Smooth terrain blended cave overhang", cave_portal_blend_mesh,
      {cave_entrance.x, cave_floor_y - 0.010f, cave_entrance.z + 0.10f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_portal_shell_mesh =
      makeSharedMesh(std::move(cave_complex.portal_shell_mesh));
  appendGeneratedStructuralScenery(
      "Opaque recessed cave mouth liner", cave_portal_shell_mesh,
      {cave_entrance.x, cave_floor_y + 0.015f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_entrance_wall);
  const std::shared_ptr<const CpuMesh> cave_portal_formation_mesh =
      makeSharedMesh(std::move(cave_complex.portal_formation_mesh));
  appendGeneratedStructuralScenery(
      "Continuous procedural cave mouth formation", cave_portal_formation_mesh,
      {cave_entrance.x, cave_floor_y + 0.010f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_entrance_throat_mesh =
      makeSharedMesh(std::move(cave_complex.entrance_throat_mesh));
  appendGeneratedStructuralScenery("Sealed cave entrance throat", cave_entrance_throat_mesh,
                                   {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                   cave_entrance_wall);
  support_surfaces_.addMesh({cave_entrance_throat_mesh, {}, 0.20f});
  const std::shared_ptr<const CpuMesh> cave_portal_floor_mesh =
      makeSharedMesh(std::move(cave_complex.portal_floor_mesh));
  appendGeneratedScenery("Walkable cave entrance threshold", cave_portal_floor_mesh,
                         {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({cave_portal_floor_mesh, {}, 0.46f});
  decorative_ground_surfaces.addMesh({cave_portal_floor_mesh, {}, 0.46f});
  const std::shared_ptr<const CpuMesh> cave_floor_mesh =
      makeSharedMesh(std::move(cave_complex.floor_mesh));
  appendGeneratedScenery("Walkable packed cave floor", cave_floor_mesh, {0.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({cave_floor_mesh, {}, 0.36f});
  decorative_ground_surfaces.addMesh({cave_floor_mesh, {}, 0.36f});
  const std::shared_ptr<const CpuMesh> deep_cave_floor_mesh =
      makeSharedMesh(std::move(deep_cave_complex.floor_mesh));
  appendGeneratedScenery("Walkable deep cave floor", deep_cave_floor_mesh, {0.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({deep_cave_floor_mesh, {}, 0.36f});
  decorative_ground_surfaces.addMesh({deep_cave_floor_mesh, {}, 0.36f});
  std::size_t cave_chunk_index = 0;
  for (CpuMesh &chunk : cave_complex.tunnel_chunks) {
    const Material &chunk_material = cave_chunk_index < 2u ? cave_entrance_wall : cave_wall;
    const std::size_t chunk_index = appendGeneratedStructuralScenery(
        "Authored cave interior", makeSharedMesh(std::move(chunk)), {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, chunk_material);
    scene_.objects()[chunk_index].viewer_cull_volume = cave_viewer_cull_volume_;
    ++cave_chunk_index;
  }
  for (CpuMesh &chunk : deep_cave_complex.tunnel_chunks) {
    appendGeneratedStructuralScenery("Authored deep cave interior", makeSharedMesh(std::move(chunk)),
                                     {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                     {0.0f, 0.0f, 0.0f}, cave_wall);
  }
  for (std::size_t section_index = 0u; section_index < cave_sections_.size(); ++section_index) {
    appendCaveAgingPass(cave_sections_[section_index], section_index > 0u);
  }
  for (const AuthoredCaveSection &section : cave_sections_) {
    for (const CaveWallFixturePlacement &fixture : section.wall_fixtures) {
      appendIndustrialWallLight(fixture, industrial_light_metal, industrial_red_lens);
      appendWallDecal("Rust-stained cave wall around lamp", fixture.mount_position,
                      fixture.normal, fixture.up, {0.72f, 0.52f}, cave_soot_rust, 0.014f);
      appendWallDecal("Wet drip trail under cave lamp",
                      fixture.mount_position - fixture.up * 0.52f + fixture.normal * 0.010f,
                      fixture.normal, fixture.up, {0.18f, 0.82f}, cave_wet_streak, 0.018f);
    }
    for (const CaveWallFixturePlacement &fixture : section.secondary_wall_fixtures) {
      appendIndustrialWallLight(fixture, industrial_light_metal, industrial_red_lens);
      appendWallDecal("Rust-stained cave wall around lamp", fixture.mount_position,
                      fixture.normal, fixture.up, {0.60f, 0.44f}, cave_soot_rust, 0.014f);
      appendWallDecal("Thin wet drip trail under cave lamp",
                      fixture.mount_position - fixture.up * 0.48f + fixture.normal * 0.010f,
                      fixture.normal, fixture.up, {0.12f, 0.68f}, cave_wet_streak, 0.018f);
    }
  }

  const auto appendCaveFeatures = [&](const std::vector<CaveFeaturePlacement> &features,
                                      const CaveFeatureProfile &profile) {
    const float mineral_cutoff = 1.0f - std::clamp(profile.mineral_fraction, 0.0f, 1.0f);
    for (const CaveFeaturePlacement &feature : features) {
      const bool mineral_accent = feature.mineral >= mineral_cutoff;
      const Material &feature_material = mineral_accent ? cave_mouth_stone : cave_calcite;
      constexpr MeshPrimitive primitive = MeshPrimitive::Rock;
      Vec3 position = feature.position;
      Vec3 rotation{};
      Vec3 scale = feature.scale;
      const char *name = "Cave calcite formation";
      switch (feature.kind) {
      case CaveFeatureKind::Stalactite:
        name = mineral_accent ? "Cave iron-stained stalactite" : "Cave calcite stalactite";
        scale.x *= 0.42f;
        scale.y *= 0.48f;
        scale.z *= 0.42f;
        position = feature.position + feature.normal * (scale.y * 0.88f);
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        break;
      case CaveFeatureKind::Stalagmite:
        name = mineral_accent ? "Cave iron-stained stalagmite" : "Cave calcite stalagmite";
        scale.x *= 0.40f;
        scale.y *= 0.46f;
        scale.z *= 0.40f;
        position = feature.position + feature.normal * (scale.y * 0.88f);
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        break;
      case CaveFeatureKind::Column:
        name = mineral_accent ? "Cave iron-stained column" : "Cave calcite column";
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        scale.x *= 0.52f;
        scale.z *= 0.52f;
        scale.y *= 0.64f;
        break;
      case CaveFeatureKind::WallShelf:
        name = mineral_accent ? "Cave iron-stained wall shelf" : "Cave flowstone wall shelf";
        rotation = {0.0f, std::atan2(feature.normal.x, feature.normal.z), 0.0f};
        break;
      }
      keepCameraSolid(appendScenery(name, primitive, position, scale, rotation, feature_material));
      if (feature.kind == CaveFeatureKind::WallShelf && !mineral_accent) {
        appendWallDecal("Damp moss layer on cave shelf", feature.position + feature.normal * 0.035f,
                        feature.normal, {0.0f, 1.0f, 0.0f},
                        {std::max(scale.x * 1.30f, 0.18f), std::max(scale.z * 0.72f, 0.10f)},
                        cave_moss_patch, 0.016f);
      }
      if (mineral_accent) {
        appendWallDecal("Fresh broken mineral interior",
                        feature.position + feature.normal * std::max(scale.y * 0.22f, 0.04f),
                        feature.normal, {0.0f, 1.0f, 0.0f},
                        {std::max(scale.x * 0.62f, 0.12f), std::max(scale.z * 0.50f, 0.08f)},
                        cave_scratch_mark, 0.018f);
      }
    }
  };
  appendCaveFeatures(cave_complex.features, cave_spec.features);
  appendCaveFeatures(deep_cave_complex.features, deep_cave_spec.features);

  Vec3 sign_base = {cave_entrance.x - 1.78f, cave_floor_y + 0.72f, cave_entrance.z + 1.32f};
  if (const TerrainSurfaceSample sign_ground = groundDrapeSurface({sign_base.x, sign_base.z});
      sign_ground.valid) {
    sign_base.y = sign_ground.height + 0.76f;
  }
  appendScenery("Cave sign left post", MeshPrimitive::Box, sign_base + Vec3{-0.40f, -0.25f, 0.0f},
                {0.060f, 0.74f, 0.060f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendScenery("Cave sign right post", MeshPrimitive::Box, sign_base + Vec3{0.40f, -0.25f, 0.0f},
                {0.060f, 0.74f, 0.060f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendScenery("Weathered Cave sign board", MeshPrimitive::Box, sign_base, {1.08f, 0.34f, 0.060f},
                {0.0f, 0.0f, 0.0f}, sign_wood);
  appendGeneratedScenery("Cave sign handwritten text", caveSignStrokeMesh(),
                         sign_base + Vec3{0.0f, -0.004f, 0.037f}, {0.82f, 0.82f, 0.82f},
                         {0.0f, 0.0f, 0.0f}, sign_ink);

  for (int i = 0; i < 7; ++i) {
    const float side = (i % 2 == 0) ? -1.0f : 1.0f;
    const float depth = 0.24f + static_cast<float>(i / 2) * 0.46f;
    const float spread = 1.78f + 0.22f * std::sin(static_cast<float>(i) * 1.7f);
    Vec3 rock_position = cave_entrance + Vec3{side * spread, cave_floor_y + 0.16f, 0.74f - depth};
    const TerrainSurfaceSample rock_ground =
        groundDrapeSurface(Vec2{rock_position.x, rock_position.z});
    if (rock_ground.valid) {
      rock_position.y = rock_ground.height + 0.17f;
    }
    appendScenery("Cave mouth fallen stone", MeshPrimitive::Rock, rock_position,
                  {0.19f + 0.045f * std::sin(static_cast<float>(i) * 0.8f),
                   0.12f + 0.030f * std::cos(static_cast<float>(i) * 1.1f),
                   0.18f + 0.040f * std::sin(static_cast<float>(i) * 1.3f)},
                  {0.0f, static_cast<float>(i) * 0.61f, 0.0f}, cave_mouth_stone);
  }

  const Vec2 cave_route_min = pathRouteMin(cave_path_specs) - Vec2{1.58f, 1.58f};
  const Vec2 cave_route_max = pathRouteMax(cave_path_specs) + Vec2{1.58f, 1.58f};
  const Vec2 cave_entrance_planar{cave_entrance.x, cave_entrance.z};
  const auto outsideCavePortal = [cave_entrance_planar](const Vec2 position) {
    const float dx = (position.x - cave_entrance_planar.x) / 2.18f;
    const float dz = (position.y - cave_entrance_planar.y) / 1.92f;
    return dx * dx + dz * dz > 1.0f && position.y > cave_entrance_planar.y - 0.82f;
  };
  appendGrassFieldOn(
      "Engine cave approach grass field",
      {.min = cave_route_min,
       .max = cave_route_max,
       .seed = kLumenCaveSeed + 2701u,
       .target_blades = 1780,
       .max_blades = 1780,
       .min_spacing = 0.066f,
       .surface_offset = 0.020f,
       .min_height = 0.18f,
       .max_height = 0.54f,
       .min_width = 0.010f,
       .max_width = 0.032f,
       .max_bend = 0.140f,
       .max_lean = 0.066f,
       .density_noise_scale = 0.66f,
       .density_noise_contrast = 0.42f,
       .min_surface_normal_y = 0.50f,
       .preferred_surface_normal_y = 0.83f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             const float route_distance = distanceToPathRoute(cave_path_specs, position);
             return outsideCavePortal(position) &&
                    route_distance > kCaveApproachPathWidth * 0.70f &&
                    route_distance < kCaveApproachPathWidth * 2.28f;
           }},
      grass_blades, terrainGrassSurface);
  appendGrassFieldOn(
      "Engine cave mouth grass field",
      {.min = {cave_entrance.x - 5.10f, cave_entrance.z - 1.05f},
       .max = {cave_entrance.x + 5.10f, cave_entrance.z + 5.20f},
       .seed = kLumenCaveSeed + 2809u,
       .target_blades = 1180,
       .max_blades = 1180,
       .min_spacing = 0.058f,
       .surface_offset = 0.022f,
       .min_height = 0.20f,
       .max_height = 0.66f,
       .min_width = 0.011f,
       .max_width = 0.034f,
       .max_bend = 0.160f,
       .max_lean = 0.072f,
       .density_noise_scale = 0.86f,
       .density_noise_contrast = 0.46f,
       .min_surface_normal_y = 0.48f,
       .preferred_surface_normal_y = 0.82f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             return outsideCavePortal(position) &&
                    distanceToPathRoute(cave_path_specs, position) >
                        kCaveApproachPathWidth * 0.76f;
           }},
      grass_blades, terrainGrassSurface);
  const std::vector<GroundDetailAnchor> cave_ground_details = scatterGroundDetailAnchors(
      {.min = {cave_entrance.x - 5.35f, cave_entrance.z - 1.20f},
       .max = {cave_entrance.x + 5.35f, cave_entrance.z + 5.45f},
       .seed = kLumenCaveSeed + 2927u,
       .target_details = 260,
       .max_details = 260,
       .min_spacing = 0.165f,
       .surface_offset = 0.026f,
       .min_radius = 0.035f,
       .max_radius = 0.135f,
       .twig_fraction = 0.22f,
       .leaf_fraction = 0.32f,
       .density_noise_scale = 0.50f,
       .density_noise_contrast = 0.34f,
       .min_surface_normal_y = 0.42f,
       .preferred_surface_normal_y = 0.80f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             return outsideCavePortal(position) &&
                    distanceToPathRoute(cave_path_specs, position) >
                        kCaveApproachPathWidth * 0.62f;
           }},
      terrainGrassSurface);
  appendGeneratedScenery("Engine cave mouth procedural ground detail",
                         makeSharedMesh(makeGroundDetailMesh(
                             cave_ground_details,
                             {.pebble_segments = 8,
                              .double_sided_litter = true,
                              .pebble_height = 0.46f,
                              .leaf_curl = 0.024f,
                              .twig_height = 0.022f})),
                         {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, cave_talus);

  coal_ores_.clear();
  coal_ores_.reserve(cave_complex.ore_nodes.size() + deep_cave_complex.ore_nodes.size());
  const auto appendCaveOreNodes = [&](const std::vector<CaveOreNodePlacement> &ore_nodes) {
    for (const CaveOreNodePlacement &ore : ore_nodes) {
      const float yaw = std::atan2(ore.normal.x, ore.normal.z);
      const Vec3 ore_position = ore.position + ore.normal * std::max(ore.radius * 0.16f, 0.035f);
      const Vec3 ore_scale = ore.scale * 0.92f;
      const std::size_t index =
          keepCameraSolid(appendScenery("Coal ore vein node", MeshPrimitive::Rock, ore_position,
                                        ore_scale, {0.0f, yaw, 0.0f}, coal_ore_material));
      coal_ores_.push_back({ore_position, ore.normal, ore_scale, ore.radius, kCoalOreMaxHealth,
                            kCoalOreMaxHealth, 2, index, false, 0.0f});
      appendWallDecal("Mining scratch decals around coal seam",
                      ore.position + ore.normal * std::max(ore.radius * 0.08f, 0.024f),
                      ore.normal, {0.0f, 1.0f, 0.0f}, {ore.radius * 2.4f, ore.radius * 0.52f},
                      cave_scratch_mark, 0.016f);
      appendWallDecal("Wet mineral stain around coal seam",
                      ore.position - Vec3{0.0f, ore.radius * 0.38f, 0.0f} +
                          ore.normal * std::max(ore.radius * 0.05f, 0.018f),
                      ore.normal, {0.0f, 1.0f, 0.0f}, {ore.radius * 1.4f, ore.radius * 1.8f},
                      cave_wet_streak, 0.014f);
    }
  };
  appendCaveOreNodes(cave_complex.ore_nodes);
  appendCaveOreNodes(deep_cave_complex.ore_nodes);
  for (const CoalOreNode &ore : coal_ores_) {
    appendCaveDebugOverlay("Cave debug ore mining target", MeshPrimitive::Sphere, ore.position,
                           {ore.radius, ore.radius, ore.radius}, {}, {0.86f, 0.45f, 0.16f},
                           static_cast<std::uint32_t>(CaveDebugOverlayLayer::MiningTarget) |
                               static_cast<std::uint32_t>(CaveDebugOverlayLayer::Collision));
  }

  const PathRibbonMeshSpec underpass_path_spec = castleUnderpassPathSpec();
  appendPathShoulders("Mounded verge through underpass", underpass_path_spec, 0.34f, 0.052f);
  appendSoilPath("Packed soil path through underpass", *castleUnderpassPathMesh());
  {
    CpuMesh portal_blend = *castlePathPortalBlendMesh();
    const Vec3 portal = castlePathPortalPoint();
    for (Vertex &vertex : portal_blend.vertices) {
      vertex.position = vertex.position + portal;
    }
    const std::shared_ptr<const CpuMesh> draped_portal_blend =
        makeSharedMesh(drapeMeshToSurface(std::move(portal_blend), groundDrapeSurface,
                                          {.surface_offset = 0.024f, .raise_only = false}));
    appendGeneratedScenery("Packed soil path portal blend", draped_portal_blend, {0.0f, 0.0f, 0.0f},
                           {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, soil_path);
    support_surfaces_.addMesh({draped_portal_blend, {}, 0.42f});
  }

  const PathRibbonMeshSpec nest_path_spec = castleNestPathSpec();
  appendPathShoulders("Mounded verge to bird nest", nest_path_spec, 0.42f, 0.086f);
  appendSoilPath("Packed soil path to bird nest", *castleNestPathMesh());
  const GeneratedSceneryBundle bird_tree_bundle = assembleGeneratedScenery(
      {.root = {.position = bird_tree_base},
       .meshes = {GeneratedSceneryMeshPart{.name = "Bird nest tree trunk",
                                           .mesh = climbableTreeTrunkMesh(),
                                           .transform = {.rotation = quatFromEulerXyz(
                                                             {0.0f, radians(-10.0f), 0.0f}),
                                                         .scale = {0.70f, 0.92f, 0.70f}},
                                           .material = tree_bark},
                  GeneratedSceneryMeshPart{.name = "Bird nest tree crown",
                                           .mesh = treeCanopyMesh(),
                                           .transform = {.position = {0.0f, 3.12f, 0.0f},
                                                         .rotation = quatFromEulerXyz(
                                                             {0.0f, radians(16.0f), 0.0f}),
                                                         .scale = {0.92f, 0.78f, 0.90f}},
                                           .material = exotic_leaf}},
       .sockets = {GeneratedScenerySocket{
           .name = "nest", .transform = {.position = {0.0f, 3.56f, 0.0f}}, .radius = 0.42f}}});
  aster::appendGeneratedScenery(scene_, bird_tree_bundle, &scenery_objects_);
  appendGeneratedBeam("Bird nest support branch", fishingRodMesh(),
                      bird_tree_base + Vec3{-0.48f, 2.72f, 0.18f},
                      castle_bird_nest_position_ + Vec3{-0.08f, -0.08f, 0.02f}, 0.035f, tree_bark);
  appendGeneratedBeam("Bird nest crossing branch", fishingRodMesh(),
                      bird_tree_base + Vec3{0.52f, 2.78f, -0.14f},
                      castle_bird_nest_position_ + Vec3{0.10f, -0.09f, -0.04f}, 0.030f, tree_bark);
  appendGeneratedScenery("Interwoven branch bird nest", castleBirdNestMesh(),
                         castle_bird_nest_position_, {1.0f, 1.0f, 1.0f},
                         {0.0f, radians(-18.0f), 0.0f}, nest_twig_material);

  castle_birds_.clear();
  const auto appendBird = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                              const Material &bird_material, const Vec3 home_offset,
                              const Vec3 start_offset, const Vec3 scale, const float temperament,
                              const float fear_radius, const float wing_phase_offset) {
    CastleBird bird;
    bird.state.home_position = castle_bird_nest_position_ + home_offset;
    bird.state.position = castle_bird_flight_center_ + start_offset;
    bird.state.velocity =
        normalize(Vec3{-start_offset.z, 0.24f + temperament * 0.18f, start_offset.x}) *
        (1.1f + temperament * 0.55f);
    bird.state.temperament = temperament;
    bird.scale = scale;
    bird.wing_phase_offset = wing_phase_offset;
    bird.fear_radius = fear_radius;
    bird.body_length = scale.z * 0.52f;
    bird.wing_span = scale.x * 0.72f;
    bird.object_index =
        appendGeneratedScenery(name, mesh, bird.state.position, scale, {}, bird_material);
    if (bird.object_index < scene_.objects().size()) {
      scene_.objects()[bird.object_index].casts_contact_shadow = false;
      scene_.objects()[bird.object_index].auto_contact_shadow = false;
    }
    castle_birds_.push_back(bird);
  };
  appendBird("Schalow's turaco", crestedBirdMesh(), emerald_feathers, {-0.34f, 0.24f, 0.08f},
             {-2.8f, 0.30f, 2.4f}, {0.62f, 0.62f, 0.62f}, 0.24f, 3.2f, 0.4f);
  appendBird("Ruspoli's turaco", crestedBirdMesh(), crimson_feathers, {0.22f, 0.32f, -0.10f},
             {1.7f, 0.52f, -2.8f}, {0.60f, 0.62f, 0.60f}, 0.58f, 3.7f, 1.9f);
  appendBird("Shaft-tailed whydah", longTailBirdMesh(), ink_white_feathers, {0.06f, 0.42f, 0.38f},
             {3.5f, 0.78f, 1.0f}, {0.48f, 0.50f, 0.60f}, 0.82f, 4.1f, 3.2f);
  appendBird("Curl-crested aracari", compactBirdMesh(), gold_blue_feathers, {-0.12f, 0.18f, -0.36f},
             {-0.8f, 0.18f, -3.3f}, {0.56f, 0.58f, 0.56f}, 0.43f, 3.5f, 4.6f);

  for (int side = -1; side <= 1; side += 2) {
    const float x = static_cast<float>(side) * 2.45f;
    appendScenery("Amber lamp post", MeshPrimitive::Box, {x, 0.72f, -4.15f}, {0.11f, 1.26f, 0.11f},
                  {0.0f, 0.0f, 0.0f}, weathered_iron);
    appendScenery("Amber lamp core", MeshPrimitive::Crystal, {x, 1.52f, -4.15f},
                  {0.10f, 0.22f, 0.10f}, {0.0f, 0.0f, 0.0f}, amber_lamp, 0.35f);
  }

  const PathRibbonMeshSpec pond_path_spec = naturePathSpec(wetland_clips.front());
  const std::vector<PathRibbonMeshSpec> pond_path_route{pond_path_spec};
  appendPathShoulders("Mounded verge to pond", pond_path_spec, 0.48f, 0.078f);
  appendSoilPath("Packed soil path to pond", makePathRibbonMesh(pond_path_spec));

  for (int i = 0; i < 30; ++i) {
    const float t = (static_cast<float>(i) + 0.30f) / 30.0f;
    if (t < 0.13f) {
      continue;
    }
    const float side_sign = i % 2 == 0 ? -1.0f : 1.0f;
    const float stagger = 0.34f + 0.13f * std::sin(static_cast<float>(i) * 1.77f);
    Vec3 path_position = naturePathPoint(pond_path_spec, t) +
                         naturePathSide(pond_path_spec, t) * (side_sign * stagger);
    const TerrainSurfaceSample path_ground =
        groundDrapeSurface(Vec2{path_position.x, path_position.z});
    if (path_ground.valid) {
      path_position.y = path_ground.height + 0.014f;
    }
    appendGeneratedScenery("Path edge grass tuft", grassTuftMesh(),
                           path_position + Vec3{0.0f, 0.010f, 0.0f},
                           {0.58f, 0.70f + 0.20f * std::sin(static_cast<float>(i) * 0.91f), 0.58f},
                           {0.0f, static_cast<float>(i) * 0.83f, 0.0f}, grass_blades);
  }

  appendGeneratedScenery("Feathered terrain transition", terrainTransitionMesh(),
                         pond_center_ + Vec3{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, terrain_transition);
  appendGeneratedScenery("Moss and soil pond mound", moundMesh(), pond_center_, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, grass_soil);
  appendGeneratedScenery("Courtyard pond hardscape substrate", pondHardscapeSubstrateMesh(),
                         pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, hardscape_substrate);
  appendGeneratedScenery("Courtyard pond hardscape soil apron", pondHardscapeApronMesh(),
                         pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, hardscape_dust);
  appendGeneratedScenery("Submerged courtyard pond silt bed", pondBasinMesh(),
                         {pond_center_.x, kCourtyardPondSurfaceY, pond_center_.z},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  pond_water_object_ = appendGeneratedScenery("Wind-rippled courtyard pond", pondWaterMesh(),
                                              {pond_center_.x, 0.405f, pond_center_.z},
                                              {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, pond_water);
  if (pond_water_object_ < scene_.objects().size()) {
    scene_.objects()[pond_water_object_].casts_contact_shadow = false;
    scene_.objects()[pond_water_object_].auto_contact_shadow = false;
  }
  appendGrassField(
      "Engine pond bank grass field",
      {.min = {pond_center_.x - 2.90f, pond_center_.z - 2.12f},
       .max = {pond_center_.x + 2.90f, pond_center_.z + 2.12f},
       .seed = kLumenCaveSeed + 2303u,
       .target_blades = 620,
       .max_blades = 620,
       .min_spacing = 0.060f,
       .surface_offset = 0.020f,
       .min_height = 0.18f,
       .max_height = 0.52f,
       .min_width = 0.010f,
       .max_width = 0.028f,
       .max_bend = 0.115f,
       .max_lean = 0.052f,
       .density_noise_scale = 0.84f,
       .density_noise_contrast = 0.34f,
       .min_surface_normal_y = 0.52f,
       .preferred_surface_normal_y = 0.84f,
       .accepts_position =
           [pond_path_spec, pond_path_route, this](const Vec2 position) {
             const float dx = (position.x - pond_center_.x) / std::max(kCourtyardPondRadius.x, 0.001f);
             const float dz = (position.y - pond_center_.z) / std::max(kCourtyardPondRadius.y, 0.001f);
             const float normalized = std::sqrt(dx * dx + dz * dz);
             return normalized > 0.93f && normalized < 1.34f &&
                    distanceToPathRoute(pond_path_route, position) > pond_path_spec.width * 0.62f;
           }},
      grass_blades);

  for (int i = 0; i < 26; ++i) {
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const float fill = (static_cast<float>(i) + 0.5f) / 26.0f;
    const float ring = 0.62f + std::sqrt(fill) * 0.34f;
    Vec3 tuft_position{pond_center_.x + std::cos(angle) * 2.20f * ring, 0.0f,
                       pond_center_.z + std::sin(angle) * 1.46f * ring};
    const TerrainSurfaceSample tuft_ground =
        groundDrapeSurface(Vec2{tuft_position.x, tuft_position.z});
    if (!tuft_ground.valid) {
      continue;
    }
    tuft_position.y = tuft_ground.height + 0.018f;
    appendGeneratedScenery("Pond bank grass tuft", grassTuftMesh(), tuft_position,
                           {0.78f + fill * 0.38f, 0.86f + fill * 0.28f, 0.78f + fill * 0.38f},
                           {0.0f, angle, 0.0f}, grass_blades);
  }

  for (int i = 0; i < 14; ++i) {
    const float angle = (static_cast<float>(i) / 14.0f) * kPi * 2.0f;
    const float radius_x = 1.72f + 0.18f * std::sin(static_cast<float>(i) * 1.7f);
    const float radius_z = 1.25f + 0.12f * std::cos(static_cast<float>(i) * 1.1f);
    appendScenery("Pond edge stone", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * radius_x, 0.23f,
                   pond_center_.z + std::sin(angle) * radius_z},
                  {0.13f + 0.035f * std::sin(static_cast<float>(i) * 0.9f),
                   0.070f + 0.025f * std::cos(static_cast<float>(i) * 1.4f),
                   0.10f + 0.025f * std::sin(static_cast<float>(i) * 1.2f)},
                  {0.0f, angle, 0.0f}, pond_stone);
  }

  for (int i = 0; i < 9; ++i) {
    const float angle = static_cast<float>(i) * 0.73f + 0.35f;
    const float radius = 2.02f + 0.25f * std::sin(static_cast<float>(i) * 1.21f);
    appendScenery("Raised bank soil clump", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * radius, 0.30f,
                   pond_center_.z + std::sin(angle) * radius * 0.64f},
                  {0.18f, 0.080f, 0.14f}, {0.0f, angle, 0.0f}, soil_path);
  }

  for (int i = 0; i < 6; ++i) {
    const float angle = -2.55f + static_cast<float>(i) * 0.42f;
    appendScenery("Visible shoreline stone", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * (1.62f + 0.08f * static_cast<float>(i)),
                   0.34f,
                   pond_center_.z + std::sin(angle) * (1.14f + 0.05f * static_cast<float>(i))},
                  {0.18f, 0.075f, 0.14f}, {0.0f, angle, 0.0f}, pond_stone);
  }

  for (int i = 0; i < 7; ++i) {
    const float angle = -0.92f + static_cast<float>(i) * 0.58f;
    const Vec3 plant_position{pond_center_.x + std::cos(angle) * (2.42f + 0.18f * (i % 2)),
                              0.24f + 0.025f * std::sin(static_cast<float>(i) * 1.3f),
                              pond_center_.z + std::sin(angle) * (1.78f + 0.12f * (i % 3))};
    if (insideTerrainClipEllipse({plant_position.x, plant_position.z}, wetland_clips.front())) {
      continue;
    }
    Vec3 grounded_plant_position = plant_position;
    const TerrainSurfaceSample plant_ground =
        groundDrapeSurface(Vec2{plant_position.x, plant_position.z});
    if (plant_ground.valid) {
      grounded_plant_position.y = plant_ground.height + 0.022f;
    }
    appendGeneratedScenery(
        "Exotic fishing-bank broadleaf", broadLeafPlantMesh(), grounded_plant_position,
        {0.82f + 0.08f * static_cast<float>(i % 3), 0.92f + 0.10f * static_cast<float>(i % 2),
         0.82f + 0.08f * static_cast<float>(i % 3)},
        {0.0f, angle + 0.35f, 0.0f}, exotic_leaf);
    if ((i % 2) == 0) {
      appendScenery("Exotic fishing-bank flower", MeshPrimitive::Sphere,
                    grounded_plant_position + Vec3{0.0f, 0.58f, 0.0f}, {0.055f, 0.046f, 0.055f},
                    {0.0f, angle, 0.0f}, exotic_flower);
    }
  }

  const Vec3 fishing_tree_base = pond_center_ + Vec3{-3.35f, 0.08f, 1.88f};
  appendGeneratedScenery("Small wetland biome tree trunk", climbableTreeTrunkMesh(),
                         fishing_tree_base, {0.36f, 0.54f, 0.36f}, {0.0f, radians(18.0f), 0.0f},
                         fishing_rod_material);
  appendGeneratedScenery("Small wetland biome tree canopy", treeCanopyMesh(),
                         fishing_tree_base + Vec3{0.0f, 2.05f, 0.0f}, {0.62f, 0.56f, 0.62f},
                         {0.0f, radians(18.0f), 0.0f}, exotic_leaf);

  appendGeneratedScenery("Giant climbable exotic tree trunk", climbableTreeTrunkMesh(),
                         climbable_tree_.base, {1.0f, 1.0f, 1.0f}, {0.0f, radians(-14.0f), 0.0f},
                         tree_bark);
  appendGeneratedScenery("Giant climbable exotic tree crown", treeCanopyMesh(),
                         climbable_tree_.base + Vec3{0.0f, 3.48f, 0.0f}, {1.30f, 1.10f, 1.22f},
                         {0.0f, radians(22.0f), 0.0f}, exotic_leaf);
  appendGeneratedScenery("Giant climbable exotic upper crown", treeCanopyMesh(),
                         climbable_tree_.base + Vec3{0.28f, 4.20f, -0.18f}, {0.82f, 0.74f, 0.82f},
                         {0.0f, radians(-28.0f), 0.0f}, exotic_leaf);
  for (int i = 0; i < 5; ++i) {
    const float angle = static_cast<float>(i) / 5.0f * kPi * 2.0f;
    Vec3 root_leaf_position =
        climbable_tree_.base + Vec3{std::cos(angle) * 1.05f, 0.0f, std::sin(angle) * 0.94f};
    const TerrainSurfaceSample root_leaf_ground =
        groundDrapeSurface(Vec2{root_leaf_position.x, root_leaf_position.z});
    if (root_leaf_ground.valid) {
      root_leaf_position.y = root_leaf_ground.height + 0.020f;
    } else {
      root_leaf_position.y += 0.06f;
    }
    appendGeneratedScenery("Climb tree root broadleaf", broadLeafPlantMesh(), root_leaf_position,
                           {0.74f, 0.82f, 0.74f}, {0.0f, angle, 0.0f}, exotic_leaf);
  }

  fish_.clear();
  const auto appendFish = [&](const char *name, const Vec3 center, const Vec2 swim_radius,
                              const Vec3 scale, const float phase, const float speed,
                              const Material &fish_material) {
    AquaticCreature fish;
    fish.center = center;
    fish.swim_radius = swim_radius;
    fish.scale = scale;
    fish.phase = phase;
    fish.speed = speed;
    fish.object_index = appendGeneratedScenery(name, fishMesh(), center, scale, {}, fish_material);
    if (fish.object_index < scene_.objects().size()) {
      scene_.objects()[fish.object_index].casts_contact_shadow = false;
      scene_.objects()[fish.object_index].auto_contact_shadow = false;
    }
    fish_.push_back(fish);
  };
  appendFish("Exotic teal pond fish", {pond_center_.x - 0.34f, 0.345f, pond_center_.z - 0.08f},
             {1.36f, 0.72f}, {0.92f, 0.92f, 0.92f}, 0.20f, 0.92f, fish_scale_a);
  appendFish("Exotic ember pond fish", {pond_center_.x + 0.48f, 0.330f, pond_center_.z + 0.18f},
             {1.12f, 0.58f}, {0.78f, 0.78f, 0.78f}, 2.24f, 1.18f, fish_scale_b);

  appendGeneratedScenery("Inner castle wetland transition", innerPondTransitionMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         terrain_transition);
  appendGeneratedScenery("Deep inner castle pond basin", innerPondMoundMesh(), inner_pond_center_,
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  appendGeneratedScenery("Inner pond hardscape substrate", innerPondHardscapeSubstrateMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         hardscape_substrate);
  appendGeneratedScenery("Inner pond hardscape soil apron", innerPondHardscapeApronMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         hardscape_dust);
  appendGeneratedScenery("Submerged inner pond silt bed", innerPondBasinMesh(),
                         {inner_pond_center_.x, kInnerPondSurfaceY, inner_pond_center_.z},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  inner_pond_water_object_ =
      appendGeneratedScenery("Deep inner castle pond water", innerPondWaterMesh(),
                             {inner_pond_center_.x, 0.430f, inner_pond_center_.z},
                             {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, pond_water);
  if (inner_pond_water_object_ < scene_.objects().size()) {
    scene_.objects()[inner_pond_water_object_].casts_contact_shadow = false;
    scene_.objects()[inner_pond_water_object_].auto_contact_shadow = false;
  }
  const auto findWallMountedFixture =
      [&](const Vec3 anchor, const float mount_height) -> std::optional<CaveWallFixturePlacement> {
    struct Candidate {
      float score = std::numeric_limits<float>::max();
      CaveWallFixturePlacement placement{};
    };
    std::optional<Candidate> best;
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const auto considerFace = [&](const Vec3 normal, const float span, const Vec3 center,
                                  const Vec3 half) {
      if (span < 1.20f) {
        return;
      }

      Vec3 point{};
      if (std::abs(normal.x) > 0.5f) {
        point.x = center.x + normal.x * half.x;
        point.y = std::clamp(mount_height, center.y - half.y + 0.42f, center.y + half.y - 0.42f);
        point.z = std::clamp(anchor.z, center.z - half.z + 0.24f, center.z + half.z - 0.24f);
      } else {
        point.x = std::clamp(anchor.x, center.x - half.x + 0.24f, center.x + half.x - 0.24f);
        point.y = std::clamp(mount_height, center.y - half.y + 0.42f, center.y + half.y - 0.42f);
        point.z = center.z + normal.z * half.z;
      }

      if (dot(anchor - point, normal) <= 0.0f) {
        return;
      }

      const Vec2 planar_delta{anchor.x - point.x, anchor.z - point.z};
      const float score = length(planar_delta) + std::abs(anchor.y - point.y) * 0.18f;
      if (best.has_value() && best->score <= score) {
        return;
      }

      Vec3 tangent = normalize(cross(up, normal));
      if (length(tangent) <= 0.0001f) {
        tangent = {1.0f, 0.0f, 0.0f};
      }
      const Vec3 mount_position = point;
      const Vec3 lens_position = mount_position + normal * 0.075f;
      const Vec3 light_position = lens_position + normal * 0.18f;
      best = Candidate{score,
                       {.position = mount_position,
                        .mount_position = mount_position,
                        .lens_position = lens_position,
                        .light_position = light_position,
                        .light_color = kCaveIndustrialWarmLight,
                        .normal = normal,
                        .tangent = tangent,
                        .up = up,
                        .t = 0.0f}};
    };

    for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
      if (box.half_extents.y < 0.90f) {
        continue;
      }
      const Vec3 center = castle_origin + box.center;
      const Vec3 half = box.half_extents;
      if (center.y + half.y < mount_height - 0.35f) {
        continue;
      }
      considerFace({1.0f, 0.0f, 0.0f}, half.z * 2.0f, center, half);
      considerFace({-1.0f, 0.0f, 0.0f}, half.z * 2.0f, center, half);
      considerFace({0.0f, 0.0f, 1.0f}, half.x * 2.0f, center, half);
      considerFace({0.0f, 0.0f, -1.0f}, half.x * 2.0f, center, half);
    }
    return best.has_value() ? std::optional<CaveWallFixturePlacement>{best->placement}
                            : std::nullopt;
  };

  const Vec3 pond_wall_anchor =
      inner_pond_center_ +
      Vec3{inner_pond_radius_.x * 0.12f, kInnerPondSurfaceY + 1.34f, -inner_pond_radius_.y - 2.0f};
  if (const std::optional<CaveWallFixturePlacement> pond_fixture =
          findWallMountedFixture(pond_wall_anchor, pond_wall_anchor.y)) {
    appendIndustrialWallLight(*pond_fixture, industrial_light_metal, industrial_red_lens);
    pond_accent_light_position_ = pond_fixture->light_position + pond_fixture->up * 0.20f;
    pond_accent_light_valid_ = true;
  }

  const PlacementEllipseBand inner_pond_reed_band{
      {{inner_pond_center_.x, inner_pond_center_.z}, inner_pond_radius_}, 1.015f, 1.125f};
  for (int i = 0; i < 38; ++i) {
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f))) + 0.18f;
    if (std::sin(angle) < -0.72f) {
      continue;
    }
    const float fill = (static_cast<float>(i) + 0.5f) / 38.0f;
    const float radius = 1.018f + std::sqrt(fill) * 0.095f;
    Vec3 reed_position{inner_pond_center_.x + std::cos(angle) * inner_pond_radius_.x * radius, 0.0f,
                       inner_pond_center_.z + std::sin(angle) * inner_pond_radius_.y * radius};
    if (!contains(inner_pond_reed_band, Vec2{reed_position.x, reed_position.z})) {
      continue;
    }
    const TerrainSurfaceSample reed_ground =
        groundDrapeSurface(Vec2{reed_position.x, reed_position.z});
    if (!reed_ground.valid) {
      continue;
    }
    reed_position.y = reed_ground.height + 0.018f;
    appendGeneratedScenery("Inner pond sawgrass clump", grassTuftMesh(), reed_position,
                           {0.58f + fill * 0.22f, 0.70f + fill * 0.20f, 0.58f + fill * 0.22f},
                           {0.0f, angle, 0.0f}, wetland_blades);
  }

  crocodile_objects_.clear();
  const auto appendCrocodilePart = [&](const char *name, const MeshPrimitive primitive,
                                       const Vec3 scale, const Material &part_material) {
    const std::size_t index =
        appendScenery(name, primitive, crocodile_state_.position, scale, {}, part_material);
    if (index < scene_.objects().size()) {
      scene_.objects()[index].auto_contact_shadow = false;
    }
    crocodile_objects_.push_back(index);
  };
  const auto appendGeneratedCrocodilePart = [&](const char *name,
                                                const std::shared_ptr<const CpuMesh> &mesh,
                                                const Vec3 scale, const Material &part_material) {
    const std::size_t index =
        appendGeneratedScenery(name, mesh, crocodile_state_.position, scale, {}, part_material);
    if (index < scene_.objects().size()) {
      scene_.objects()[index].auto_contact_shadow = false;
    }
    crocodile_objects_.push_back(index);
  };
  appendGeneratedCrocodilePart("Crocodile armored body", crocodileMesh(), {1.0f, 1.0f, 1.0f},
                               crocodile_hide);
  appendCrocodilePart("Crocodile jaw", MeshPrimitive::Box, {0.17f, 0.030f, 0.30f}, crocodile_belly);
  appendCrocodilePart("Crocodile left eye", MeshPrimitive::Sphere, {0.035f, 0.032f, 0.035f},
                      crocodile_eye);
  appendCrocodilePart("Crocodile right eye", MeshPrimitive::Sphere, {0.035f, 0.032f, 0.035f},
                      crocodile_eye);

  x_eye_objects_.clear();
  for (int i = 0; i < 2; ++i) {
    x_eye_objects_.push_back(appendGeneratedScenery("Player X eye overlay", eyeCrossMesh(),
                                                    {0.0f, -20.0f, 0.0f}, {0.001f, 0.001f, 0.001f},
                                                    {}, x_eye_material));
  }

  blood_particles_.clear();
  blood_particle_cursor_ = 0;
  for (int i = 0; i < 24; ++i) {
    blood_particles_.push_back(
        {{},
         {},
         0.0f,
         0.0f,
         0.030f,
         appendScenery("Blood particle", MeshPrimitive::Sphere, {0.0f, -20.0f, 0.0f},
                       {0.001f, 0.001f, 0.001f}, {}, blood_material)});
  }

  appendGeneratedBeam("Fishing rod blank", fishingRodMesh(), fishing_rod_base_, fishing_rod_tip_,
                      0.024f, fishing_rod_material);
  appendGeneratedBeam("Fishing rod fork support left", fishingRodMesh(),
                      fishing_rod_base_ + Vec3{0.16f, -0.18f, 0.03f},
                      fishing_rod_base_ + Vec3{0.30f, 0.30f, 0.08f}, 0.018f, fishing_rod_material);
  appendGeneratedBeam("Fishing rod fork support right", fishingRodMesh(),
                      fishing_rod_base_ + Vec3{0.34f, -0.18f, 0.14f},
                      fishing_rod_base_ + Vec3{0.30f, 0.30f, 0.08f}, 0.018f, fishing_rod_material);
  appendScenery("Fishing rod ground rest", MeshPrimitive::Rock,
                fishing_rod_base_ + Vec3{0.06f, -0.15f, 0.02f}, {0.16f, 0.055f, 0.12f},
                {0.0f, 0.65f, 0.0f}, pond_stone);
  appendBeam("Fishing cork handle", fishing_rod_base_ + Vec3{-0.20f, -0.08f, 0.08f},
             fishing_rod_base_ + Vec3{0.12f, 0.06f, -0.04f}, 0.052f,
             material({0.48f, 0.30f, 0.17f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.0f, 0.0f, 0.22f, 8.0f,
                      0.10f, 0.90f));
  appendScenery("Fishing reel body", MeshPrimitive::Sphere,
                fishing_rod_base_ + Vec3{0.16f, 0.02f, -0.07f}, {0.095f, 0.095f, 0.045f},
                {0.0f, 0.0f, 0.0f}, weathered_iron);

  fishing_line_objects_.clear();
  for (int i = 0; i < kFishingLineSegmentCount; ++i) {
    const std::size_t index =
        appendGeneratedScenery("Fine fishing line segment", fishingLineMesh(), fishing_rod_tip_,
                               {fishing_line_radius_, 0.20f, fishing_line_radius_},
                               {0.0f, 0.0f, 0.0f}, fishing_line_material);
    fishing_line_objects_.push_back(index);
  }
  fishing_float_object_ =
      appendScenery("Small fishing float", MeshPrimitive::Sphere, fishing_float_rest_,
                    {0.055f, 0.075f, 0.055f}, {0.0f, 0.0f, 0.0f},
                    material({0.86f, 0.78f, 0.52f}, {0.18f, 0.06f, 0.02f}, 0.34f, 0.0f, 0.04f,
                             0.08f, 6.0f, 0.04f, 1.0f));

  rebuildPhysicsWorld();
  updateCaveDebugOverlayVisibility();
  updateSceneObjects(1.0f / 60.0f);
  invalidateSceneReports();
}

void LumenRun::rebuildPhysicsWorld() {
  physics_.clear();
  physics_.setSettings({{0.0f, -11.2f, 0.0f}, 8, 1.0f / 120.0f});

  PhysicsBodyDesc player_body;
  player_body.type = PhysicsBodyType::Dynamic;
  player_body.shape = PhysicsShapeType::Capsule;
  player_body.position = player_position_;
  player_body.half_extents = {tuning_.player_radius,
                              tuning_.player_height * 0.5f - tuning_.player_radius * 0.92f,
                              tuning_.player_radius};
  player_body.radius = tuning_.player_radius * 0.92f;
  player_body.mass = 1.0f;
  player_body.material = {0.34f, 0.02f};
  player_body.linear_damping = 0.045f;
  player_body.filter = {kPhysicsLayerPlayer, kPhysicsLayerWorld};
  player_body.allow_sleep = false;
  player_body_ = physics_.addBody(player_body);

  const float arena_extent = std::max(tuning_.playable_radius, tuning_.arena_radius * 5.8f);
  PhysicsBodyDesc floor_body;
  floor_body.type = PhysicsBodyType::Static;
  floor_body.shape = PhysicsShapeType::Box;
  floor_body.position = {0.0f, -0.66f, 0.0f};
  floor_body.half_extents = {arena_extent, 0.20f, arena_extent};
  floor_body.material = {0.82f, 0.01f};
  floor_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
  [[maybe_unused]] const PhysicsBodyHandle floor_handle = physics_.addBody(floor_body);
  pond_fluid_ = physics_.addFluidVolume(
      {pond_center_ + Vec3{0.0f, kCourtyardPondCenterOffsetY, 0.0f},
       {kCourtyardPondRadius.x, kCourtyardPondHalfDepth, kCourtyardPondRadius.y},
       kCourtyardPondSurfaceY,
       1.18f,
       6.0f,
       {0.06f, 0.0f, 0.02f},
       kPhysicsLayerPlayer});
  inner_pond_fluid_ =
      physics_.addFluidVolume({inner_pond_center_ + Vec3{0.0f, kInnerPondCenterOffsetY, 0.0f},
                               {inner_pond_radius_.x, kInnerPondHalfDepth, inner_pond_radius_.y},
                               kInnerPondSurfaceY,
                               1.22f,
                               6.8f,
                               {0.045f, 0.0f, -0.025f},
                               kPhysicsLayerPlayer});

  const auto addStaticBox = [&](const Vec3 position, const Vec3 half_extents) {
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
      return;
    }
    PhysicsBodyDesc obstacle_body;
    obstacle_body.type = PhysicsBodyType::Static;
    obstacle_body.shape = PhysicsShapeType::Box;
    obstacle_body.position = position;
    obstacle_body.half_extents = half_extents;
    obstacle_body.material = {0.68f, 0.0f};
    obstacle_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
    [[maybe_unused]] const PhysicsBodyHandle obstacle_handle = physics_.addBody(obstacle_body);
  };

  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
    addStaticBox(castle_origin + box.center, box.half_extents);
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    if (element.collidable) {
      addStaticBox(castle_origin + element.center, element.half_extents);
    }
  }
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    for (const ArchitecturalCollisionBox &box : makeGothicUnderpassCollisionBoxes()) {
      addStaticBox({portal.position.x + box.center.x * portal.scale.x,
                    portal.position.y + box.center.y * portal.scale.y,
                    portal.position.z + box.center.z * portal.scale.z},
                   {box.half_extents.x * portal.scale.x, box.half_extents.y * portal.scale.y,
                    box.half_extents.z * portal.scale.z});
    }
  }
  const float boundary = std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  addStaticBox({0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f});
  addStaticBox({0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f});
  addStaticBox({boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addStaticBox({-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addStaticBox(chest_base_ + Vec3{0.0f, 0.23f, 0.0f}, {0.30f, 0.23f, 0.22f});
  addStaticBox(climbable_tree_.base + Vec3{0.0f, climbable_tree_.height * 0.5f, 0.0f},
               {climbable_tree_.radius * 0.82f, climbable_tree_.height * 0.5f,
                climbable_tree_.radius * 0.82f});
  for (const StaticSceneryBox &box : scenery_collision_boxes_) {
    addStaticBox(box.center, box.half_extents);
  }
  for (PlacedResourceRock &rock : placed_rocks_) {
    rock.body = addPlacedRockPhysics(rock);
  }
  for (const std::shared_ptr<const CpuMesh> &cave_collision_mesh : cave_collision_meshes_) {
    if (cave_collision_mesh == nullptr) {
      continue;
    }
    PhysicsBodyDesc cave_body;
    cave_body.type = PhysicsBodyType::Static;
    cave_body.shape = PhysicsShapeType::TriangleMesh;
    cave_body.mesh = cave_collision_mesh;
    cave_body.mesh_transform = {};
    cave_body.material = {0.74f, 0.0f};
    cave_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
    [[maybe_unused]] const PhysicsBodyHandle cave_handle = physics_.addBody(cave_body);
  }
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      addStaticBox(ore.position, ore.scale * 0.54f);
    }
  }
}

} // namespace aster
