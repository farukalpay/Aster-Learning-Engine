// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct ConvexCutVolume {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  Vec3 axis_x{1.0f, 0.0f, 0.0f};
  Vec3 axis_y{0.0f, 1.0f, 0.0f};
  Vec3 axis_z{0.0f, 0.0f, 1.0f};
};

struct MeshCutPlane {
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
};

enum class MeshCutOperation {
  Partition,
  KeepNegativeHalfspaces,
  KeepPositiveHalfspaces,
};

enum class MeshCutFragmentSide {
  Boundary,
  Negative,
  Positive,
};

struct MeshCutSpec {
  std::vector<MeshCutPlane> planes;
  MeshCutOperation operation = MeshCutOperation::Partition;
  float plane_epsilon = 0.00001f;
  float point_merge_epsilon = 0.0005f;
  float minimum_fragment_area = 0.0001f;
  bool seal_cuts = true;
};

struct MeshCutImpactSpec {
  ConvexCutVolume volume{};
  Vec3 impact_point{};
  Vec3 impact_normal{0.0f, 1.0f, 0.0f};
  std::uint32_t seed = 1u;
  int plane_count = 5;
  float angular_spread = 0.46f;
  float offset_spread = 0.22f;
  float minimum_fragment_area = 0.0001f;
};

struct MeshCutFragment {
  CpuMesh mesh{};
  Vec3 centroid{};
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  Vec3 impulse_direction{0.0f, 1.0f, 0.0f};
  float surface_area = 0.0f;
  float signed_volume = 0.0f;
  MeshCutFragmentSide side = MeshCutFragmentSide::Boundary;
  std::uint32_t cut_plane_mask = 0u;
};

struct MeshCutResult {
  std::vector<MeshCutFragment> fragments;
  std::vector<Vec3> seam_points;
};

[[nodiscard]] MeshCutPlane makeMeshCutPlane(Vec3 point, Vec3 normal);

[[nodiscard]] CpuMesh makeConvexCutBox(const ConvexCutVolume &volume);

[[nodiscard]] std::vector<MeshCutPlane> buildImpactCutPlanes(const MeshCutImpactSpec &spec);

[[nodiscard]] MeshCutResult partitionMeshByCutPlanes(const CpuMesh &source_mesh,
                                                     const MeshCutSpec &spec);

[[nodiscard]] MeshCutResult buildImpactMeshCut(const MeshCutImpactSpec &spec);

} // namespace aster
