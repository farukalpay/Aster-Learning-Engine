// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/terrain_mesh.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace aster {

struct MoundMeshSpec {
  int radial_segments = 96;
  int rings = 18;
  float radius_x = 1.4f;
  float radius_z = 0.95f;
  float height = 0.34f;
  float pond_radius_x = 0.72f;
  float pond_radius_z = 0.48f;
  float pond_depth = 0.16f;
  float bank_width = 0.20f;
  float bank_height = 0.14f;
  float basin_floor_radius = 0.55f;
  float shore_depth_fraction = 0.18f;
  float surface_noise = 0.035f;
  float edge_irregularity = 0.0f;
  float edge_skirt_depth = 0.12f;
};

struct PondWaterMeshSpec {
  int radial_segments = 96;
  int rings = 8;
  float radius_x = 0.72f;
  float radius_z = 0.48f;
  float edge_softness = 0.04f;
  float edge_irregularity = 0.0f;
  float shoreline_inset = 0.0f;
  float wave_amplitude = 0.018f;
  float wave_choppiness = 0.12f;
  float wind_speed = 7.0f;
  Vec2 wind_direction{1.0f, 0.35f};
  int wave_components = 18;
};

struct SubmergedBasinMeshSpec {
  int radial_segments = 96;
  int rings = 12;
  float radius_x = 0.72f;
  float radius_z = 0.48f;
  float floor_depth = 0.58f;
  float shore_depth = 0.10f;
  float basin_floor_radius = 0.42f;
  float edge_irregularity = 0.0f;
  float bottom_noise = 0.018f;
};

struct TerrainTransitionMeshSpec {
  int radial_segments = 96;
  int rings = 8;
  float inner_radius_x = 1.8f;
  float inner_radius_z = 1.2f;
  float outer_radius_x = 2.4f;
  float outer_radius_z = 1.7f;
  float outer_radius_z_negative = 0.0f;
  float outer_radius_z_positive = 0.0f;
  float outer_radius_x_negative = 0.0f;
  float outer_radius_x_positive = 0.0f;
  float inner_height = 0.055f;
  float foundation_lift = 0.012f;
  float surface_noise = 0.010f;
  float edge_irregularity = 0.0f;
  float edge_skirt_depth = 0.08f;
};

struct GrassTuftMeshSpec {
  int blades = 14;
  float radius = 0.18f;
  float min_height = 0.16f;
  float max_height = 0.34f;
  float min_width = 0.018f;
  float max_width = 0.034f;
  float bend = 0.060f;
};

using GrassSurfaceSampler = std::function<TerrainSurfaceSample(Vec2)>;
using GrassPlacementPredicate = std::function<bool(Vec2)>;

struct GrassBladeAnchor {
  Vec3 root{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float height = 0.28f;
  float width = 0.024f;
  float bend = 0.060f;
  float lean = 0.0f;
  float azimuth = 0.0f;
  float phase = 0.0f;
  float ambient_occlusion = 0.82f;
};

struct GrassFieldScatterSpec {
  Vec2 min{-1.0f, -1.0f};
  Vec2 max{1.0f, 1.0f};
  std::uint32_t seed = 1u;
  int target_blades = 0;
  float blades_per_square_meter = 18.0f;
  int candidate_multiplier = 4;
  int max_blades = 0;
  float min_spacing = 0.035f;
  float surface_offset = 0.012f;
  float min_height = 0.16f;
  float max_height = 0.42f;
  float min_width = 0.012f;
  float max_width = 0.030f;
  float max_bend = 0.105f;
  float max_lean = 0.040f;
  float density_noise_scale = 0.42f;
  float density_noise_contrast = 0.26f;
  float min_surface_normal_y = 0.58f;
  float preferred_surface_normal_y = 0.86f;
  GrassPlacementPredicate accepts_position{};
};

struct GrassFieldMeshSpec {
  int blade_segments = 4;
  bool double_sided = true;
  float root_ao = 0.62f;
  float mid_ao = 0.86f;
  float tip_ao = 1.0f;
  float width_taper = 1.18f;
  float lateral_sway = 0.15f;
};

enum class GroundDetailKind {
  Pebble,
  Leaf,
  Twig,
};

struct GroundDetailAnchor {
  Vec3 position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  GroundDetailKind kind = GroundDetailKind::Pebble;
  float radius = 0.06f;
  float length = 0.12f;
  float width = 0.04f;
  float yaw = 0.0f;
  float lift = 0.0f;
  float ambient_occlusion = 0.84f;
};

struct GroundDetailScatterSpec {
  Vec2 min{-1.0f, -1.0f};
  Vec2 max{1.0f, 1.0f};
  std::uint32_t seed = 1u;
  int target_details = 0;
  float details_per_square_meter = 4.0f;
  int candidate_multiplier = 4;
  int max_details = 0;
  float min_spacing = 0.12f;
  float surface_offset = 0.018f;
  float min_radius = 0.035f;
  float max_radius = 0.115f;
  float twig_fraction = 0.20f;
  float leaf_fraction = 0.34f;
  float density_noise_scale = 0.32f;
  float density_noise_contrast = 0.22f;
  float min_surface_normal_y = 0.46f;
  float preferred_surface_normal_y = 0.82f;
  GrassPlacementPredicate accepts_position{};
};

struct GroundDetailMeshSpec {
  int pebble_segments = 7;
  bool double_sided_litter = true;
  float pebble_height = 0.42f;
  float leaf_curl = 0.020f;
  float twig_height = 0.020f;
};

struct PathRibbonMeshSpec {
  int segments = 24;
  float width = 0.58f;
  float width_variation = 0.0f;
  float crown_height = 0.035f;
  float surface_noise = 0.010f;
  float endpoint_taper = 0.0f;
  float endpoint_width_floor = 0.68f;
  Vec3 start{0.0f, 0.02f, 0.0f};
  Vec3 control{1.0f, 0.02f, 0.0f};
  Vec3 control_b{1.5f, 0.02f, 0.0f};
  Vec3 end{2.0f, 0.02f, 0.0f};
};

struct PathShoulderMeshSpec {
  PathRibbonMeshSpec path{};
  int side_segments = 3;
  float shoulder_width = 0.42f;
  float shoulder_height = 0.10f;
  float outer_drop = 0.025f;
  float surface_noise = 0.014f;
};

struct PathRibbonRouteMeshSpec {
  std::vector<PathRibbonMeshSpec> segments{};
};

struct PathShoulderRouteMeshSpec {
  std::vector<PathShoulderMeshSpec> segments{};
};

struct PathJunctionMeshSpec {
  int radial_segments = 48;
  int rings = 5;
  float radius_x = 0.72f;
  float radius_z = 0.48f;
  float crown_height = 0.028f;
  float surface_noise = 0.010f;
  float edge_irregularity = 0.08f;
  float radius_x_negative = 0.0f;
  float radius_x_positive = 0.0f;
  float radius_z_negative = 0.0f;
  float radius_z_positive = 0.0f;
  float edge_skirt_depth = 0.0f;
};

struct FishMeshSpec {
  int body_segments = 18;
  int body_rings = 8;
  float body_length = 0.44f;
  float body_height = 0.16f;
  float body_width = 0.12f;
  float tail_length = 0.18f;
  float fin_span = 0.14f;
};

struct BroadLeafPlantMeshSpec {
  int leaf_count = 9;
  float radius = 0.26f;
  float min_height = 0.22f;
  float max_height = 0.56f;
  float leaf_width = 0.095f;
  float leaf_length = 0.34f;
};

struct ClimbableTreeTrunkMeshSpec {
  int radial_segments = 18;
  int height_segments = 8;
  float radius = 0.34f;
  float height = 3.6f;
  float root_radius = 0.72f;
  float bark_ridge = 0.035f;
};

struct TreeCanopyMeshSpec {
  int leaf_clusters = 42;
  float radius_x = 1.38f;
  float radius_y = 0.92f;
  float radius_z = 1.24f;
};

struct TreasureChestMeshSpec {
  float width = 0.74f;
  float height = 0.46f;
  float depth = 0.54f;
  float lid_rounding = 0.18f;
};

struct SignalSentinelMeshSpec {
  int radial_segments = 8;
  int fin_count = 4;
  float height = 1.82f;
  float body_radius = 0.36f;
  float fin_span = 0.52f;
  float fin_height = 0.78f;
};

struct BirdMeshSpec {
  int body_segments = 18;
  int body_rings = 8;
  float body_length = 0.54f;
  float body_height = 0.20f;
  float body_width = 0.16f;
  float head_radius = 0.12f;
  float beak_length = 0.11f;
  float wing_span = 0.70f;
  float wing_chord = 0.24f;
  float tail_length = 0.24f;
  float crest_height = 0.0f;
  float leg_length = 0.10f;
};

struct BirdNestMeshSpec {
  int twig_count = 46;
  int radial_segments = 7;
  float outer_radius = 0.54f;
  float inner_radius = 0.26f;
  float depth = 0.18f;
  float twig_radius = 0.018f;
  float height = 0.24f;
  int seed = 41;
};

struct AmphibiousPredatorMeshSpec {
  int body_segments = 28;
  int body_rings = 10;
  int radial_segments = 10;
  int scute_count = 11;
  float body_length = 0.96f;
  float body_width = 0.22f;
  float body_height = 0.105f;
  float head_length = 0.30f;
  float head_width = 0.18f;
  float head_height = 0.10f;
  float snout_length = 0.42f;
  float snout_width = 0.13f;
  float snout_height = 0.052f;
  float tail_length = 0.86f;
  float tail_tip_width = 0.040f;
  float leg_length = 0.27f;
};

struct CaveSkitterMeshSpec {
  int body_segments = 24;
  int body_rings = 8;
  int leg_segments = 8;
  float body_length = 0.30f;
  float body_width = 0.18f;
  float body_height = 0.10f;
  float abdomen_length = 0.34f;
  float abdomen_width = 0.22f;
  float abdomen_height = 0.14f;
  float leg_span = 0.42f;
  float leg_lift = 0.045f;
  float fang_length = 0.060f;
  float eye_radius = 0.020f;
};

[[nodiscard]] CpuMesh makeMoundMesh(MoundMeshSpec spec = {});
[[nodiscard]] CpuMesh makePondWaterMesh(PondWaterMeshSpec spec = {});
[[nodiscard]] CpuMesh makeSubmergedBasinMesh(SubmergedBasinMeshSpec spec = {});
[[nodiscard]] CpuMesh makeTerrainTransitionMesh(TerrainTransitionMeshSpec spec = {});
[[nodiscard]] CpuMesh makeGrassTuftMesh(GrassTuftMeshSpec spec = {});
[[nodiscard]] std::vector<GrassBladeAnchor>
scatterGrassFieldAnchors(const GrassFieldScatterSpec &spec, const GrassSurfaceSampler &sampler);
[[nodiscard]] CpuMesh makeGrassFieldMesh(const std::vector<GrassBladeAnchor> &anchors,
                                         GrassFieldMeshSpec spec = {});
[[nodiscard]] std::vector<GroundDetailAnchor>
scatterGroundDetailAnchors(const GroundDetailScatterSpec &spec,
                           const GrassSurfaceSampler &sampler);
[[nodiscard]] CpuMesh makeGroundDetailMesh(const std::vector<GroundDetailAnchor> &anchors,
                                           GroundDetailMeshSpec spec = {});
[[nodiscard]] CpuMesh makeFishMesh(FishMeshSpec spec = {});
[[nodiscard]] CpuMesh makeBroadLeafPlantMesh(BroadLeafPlantMeshSpec spec = {});
[[nodiscard]] CpuMesh makeClimbableTreeTrunkMesh(ClimbableTreeTrunkMeshSpec spec = {});
[[nodiscard]] CpuMesh makeTreeCanopyMesh(TreeCanopyMeshSpec spec = {});
[[nodiscard]] CpuMesh makeTreasureChestMesh(TreasureChestMeshSpec spec = {});
[[nodiscard]] CpuMesh makeSignalSentinelMesh(SignalSentinelMeshSpec spec = {});
[[nodiscard]] CpuMesh makeBirdMesh(BirdMeshSpec spec = {});
[[nodiscard]] CpuMesh makeBirdNestMesh(BirdNestMeshSpec spec = {});
[[nodiscard]] CpuMesh makeAmphibiousPredatorMesh(AmphibiousPredatorMeshSpec spec = {});
[[nodiscard]] CpuMesh makeCaveSkitterMesh(CaveSkitterMeshSpec spec = {});
[[nodiscard]] Vec3 evaluatePathRibbonCenter(const PathRibbonMeshSpec &spec, float t);
[[nodiscard]] Vec3 evaluatePathRibbonTangent(const PathRibbonMeshSpec &spec, float t);
[[nodiscard]] CpuMesh makePathRibbonMesh(PathRibbonMeshSpec spec = {});
[[nodiscard]] CpuMesh makePathShoulderMesh(PathShoulderMeshSpec spec = {});
[[nodiscard]] CpuMesh makePathRouteRibbonMesh(PathRibbonRouteMeshSpec spec = {});
[[nodiscard]] CpuMesh makePathRouteShoulderMesh(PathShoulderRouteMeshSpec spec = {});
[[nodiscard]] CpuMesh makePathJunctionMesh(PathJunctionMeshSpec spec = {});

} // namespace aster
