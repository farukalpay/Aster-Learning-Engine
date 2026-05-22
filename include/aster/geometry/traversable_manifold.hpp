// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/geometry/mesh_modeling.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aster {

enum class TraversableManifoldVertexRole : std::uint32_t {
  Floor,
  Shoulder,
  Wall,
  Ceiling,
};

struct TraversableManifoldStation {
  float t = 0.0f;
  Vec3 floor_center{};
  Vec3 tangent{0.0f, 0.0f, -1.0f};
  Vec3 side{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float floor_half_width = 0.65f;
  float wall_half_width = 1.0f;
  float height = 1.6f;
  float floor_crown = 0.025f;
  float floor_edge_raise = 0.055f;
};

struct TraversableManifoldBuildSettings {
  int floor_columns = 9;
  int wall_columns = 18;
  int chunk_segments = 14;
  float shoulder_width = 0.22f;
  float lower_wall_lift = 0.035f;
  float collision_floor_clearance = 0.18f;
  float collision_max_walkable_normal_y = 0.58f;
  float support_edge_inset = 0.03f;
  float uv_scale_u = 2.0f;
  float uv_scale_v = 5.0f;
  bool emit_end_caps = false;
};

struct TraversableManifoldDiagnostics {
  std::size_t station_count = 0u;
  std::size_t structural_vertices = 0u;
  std::size_t structural_triangles = 0u;
  std::size_t support_vertices = 0u;
  std::size_t support_triangles = 0u;
  std::size_t collision_vertices = 0u;
  std::size_t collision_triangles = 0u;
  std::size_t structural_open_edges = 0u;
  std::size_t support_open_edges = 0u;
  std::size_t collision_open_edges = 0u;
  std::size_t near_coplanar_pairs = 0u;
  std::size_t seam_gap_samples = 0u;
  float max_support_deviation = 0.0f;
  float max_floor_wall_gap = 0.0f;
};

struct TraversableManifold {
  std::vector<TraversableManifoldStation> stations;
  std::vector<CpuMesh> structural_chunks;
  CpuMesh structural_mesh;
  CpuMesh support_mesh;
  CpuMesh collision_mesh;
  TraversableManifoldDiagnostics diagnostics{};
};

struct TraversableManifoldSupportQuery {
  Vec3 position{};
  float actor_radius = 0.28f;
  float max_above = 0.42f;
  float max_below = 1.35f;
};

struct TraversableManifoldSupportSample {
  bool valid = false;
  bool inside_envelope = false;
  bool walkable = false;
  float tunnel_t = 0.0f;
  float height = 0.0f;
  float ground_distance = 0.0f;
  float lateral = 0.0f;
  float half_width = 0.0f;
  float lateral_clearance = 0.0f;
  float lateral_penetration = 0.0f;
  Vec3 floor_position{};
  Vec3 tangent{0.0f, 0.0f, -1.0f};
  Vec3 side{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  Vec3 obstacle_normal{};
  Vec3 depenetration{};
};

struct TraversableVelocityProjectionSettings {
  float edge_slow_start = 0.68f;
  float edge_stop = 0.96f;
  float minimum_lateral_scale = 0.08f;
  float maximum_lateral_fraction_of_forward = 0.80f;
};

[[nodiscard]] TraversableManifold
buildTraversableManifold(const std::vector<TraversableManifoldStation> &stations,
                         TraversableManifoldBuildSettings settings = {});

[[nodiscard]] TraversableManifoldSupportSample
sampleTraversableManifoldSupport(const TraversableManifold &manifold,
                                 TraversableManifoldSupportQuery query);

[[nodiscard]] Vec3
projectVelocityOnTraversableManifold(const TraversableManifoldSupportSample &sample,
                                     Vec3 desired_velocity,
                                     TraversableVelocityProjectionSettings settings = {});

} // namespace aster
