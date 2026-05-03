// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace aster {

struct CaveOreNodePlacement {
  Vec3 position{};
  Vec3 normal{0.0f, 0.0f, 1.0f};
  Vec3 scale{0.18f, 0.14f, 0.08f};
  float radius = 0.22f;
};

struct CaveTunnelProfile {
  std::uint32_t seed = 1u;
  Vec3 start{};
  Vec3 control{};
  Vec3 control_b{};
  Vec3 end{};
  int length_segments = 64;
  int radial_segments = 18;
  float half_width = 1.05f;
  float wall_height = 1.65f;
  float floor_width = 1.22f;
  float floor_crown = 0.025f;
  float floor_edge_raise = 0.055f;
  float wall_noise = 0.18f;
  float visible_wall_start_t = 0.12f;
  float collision_start_t = 0.10f;
  float ore_start_t = 0.24f;
  float chest_t = 0.56f;
  float chamber_t = 0.62f;
  float chamber_falloff = 0.22f;
  float chamber_width_scale = 1.55f;
  float chamber_height_scale = 1.22f;
};

struct CavePortalProfile {
  int arch_segments = 18;
  float inner_half_width = 0.92f;
  float inner_height = 1.30f;
  float outer_half_width = 1.78f;
  float outer_height = 2.20f;
  float depth = 0.86f;
  float ground_lip = 0.16f;
  float inner_lining_depth = 1.25f;
  float lining_breakup = 0.045f;
};

struct CaveOreVeinProfile {
  std::uint32_t seed = 1u;
  int candidates = 72;
  int max_nodes = 14;
  float field_frequency_a = 0.48f;
  float field_frequency_b = 0.91f;
  float intersection_threshold_a = 0.26f;
  float intersection_threshold_b = 0.22f;
  float wall_inset = 0.075f;
  float min_spacing = 1.08f;
};

struct CaveComplexSpec {
  CaveTunnelProfile tunnel{};
  CavePortalProfile portal{};
  CaveOreVeinProfile ore{};
};

struct CaveComplex {
  CpuMesh portal_mesh{};
  CpuMesh portal_floor_mesh{};
  CpuMesh floor_mesh{};
  CpuMesh collision_mesh{};
  std::vector<CpuMesh> tunnel_chunks;
  std::vector<CaveOreNodePlacement> ore_nodes;
  Vec3 chest_position{};
  float chest_yaw = 0.0f;
};

struct CaveTerrainPortalCut {
  Vec2 center{};
  Vec2 forward{0.0f, 1.0f};
  Vec2 radius{1.0f, 1.0f};
  float radius_side_negative = 0.0f;
  float radius_side_positive = 0.0f;
  float radius_forward_negative = 0.0f;
  float radius_forward_positive = 0.0f;
};

struct CaveTerrainCoverSample {
  bool valid = false;
  float height = 0.0f;
};

struct CaveTerrainCoverProbe {
  int samples = 72;
  int required_consecutive_samples = 3;
  float min_t = 0.0f;
  float max_t = 0.96f;
  float roof_clearance = 0.28f;
};

struct CaveTerrainCoverFit {
  CaveTunnelProfile tunnel{};
  bool cover_found = false;
  float first_covered_t = 0.0f;
};

struct CaveInteriorSample {
  float interior = 0.0f;
  float entrance_light = 0.0f;
  float tunnel_t = 0.0f;
  float lateral = 0.0f;
  float vertical = 0.0f;
  float depth = 0.0f;
  float chamber = 0.0f;
  float half_width = 0.0f;
  float height = 0.0f;
};

struct CaveTraversalConstraint {
  bool active = false;
  Vec3 corrected_position{};
  Vec3 correction{};
  float tunnel_t = 0.0f;
  float side_limit = 0.0f;
  float lateral = 0.0f;
};

struct CaveViewConstraintProbe {
  int samples = 30;
  float minimum_radius = 0.82f;
  float interior_threshold = 0.045f;
  float backtrack_tolerance_t = 0.24f;
};

struct CaveViewConstraint {
  bool active = false;
  float radius = 0.0f;
  Vec3 position{};
};

struct CaveWallFixtureProfile {
  float start_t = 0.24f;
  float end_t = 0.58f;
  float target_spacing = 5.5f;
  int max_count = 4;
  float wall_side = 1.0f;
  float mount_height = 1.08f;
  float wall_inset = 0.14f;
};

struct CaveWallFixturePlacement {
  Vec3 position{};
  Vec3 normal{0.0f, 0.0f, 1.0f};
  Vec3 tangent{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float t = 0.0f;
};

[[nodiscard]] Vec3 evaluateCaveTunnelCenter(const CaveTunnelProfile &profile, float t);
[[nodiscard]] Vec3 evaluateCaveTunnelTangent(const CaveTunnelProfile &profile, float t);
[[nodiscard]] CaveInteriorSample sampleCaveInteriorVolume(const CaveTunnelProfile &profile,
                                                          Vec3 position);
[[nodiscard]] CaveTraversalConstraint
constrainCaveTraversalPosition(const CaveTunnelProfile &profile, Vec3 position, float actor_radius);
[[nodiscard]] CaveViewConstraint constrainCaveViewSegment(const CaveTunnelProfile &profile,
                                                          Vec3 target, Vec3 desired_camera,
                                                          CaveViewConstraintProbe probe = {});
[[nodiscard]] std::vector<CaveWallFixturePlacement>
placeCaveWallFixtures(const CaveTunnelProfile &profile, CaveWallFixtureProfile fixture = {});
[[nodiscard]] CaveTerrainCoverFit
fitCaveTunnelToTerrainCover(const CaveTunnelProfile &profile,
                            const std::function<CaveTerrainCoverSample(Vec2)> &height_sampler,
                            CaveTerrainCoverProbe probe = {});
[[nodiscard]] CaveTerrainPortalCut makeCaveTerrainPortalCut(const CaveTunnelProfile &tunnel,
                                                            const CavePortalProfile &portal,
                                                            float padding = 0.24f);
[[nodiscard]] CaveComplex buildCaveComplex(const CaveComplexSpec &spec);

} // namespace aster
