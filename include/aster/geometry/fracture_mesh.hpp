// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct ConvexFractureVolume {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  Vec3 axis_x{1.0f, 0.0f, 0.0f};
  Vec3 axis_y{0.0f, 1.0f, 0.0f};
  Vec3 axis_z{0.0f, 0.0f, 1.0f};
};

struct VoronoiFractureSpec {
  ConvexFractureVolume volume{};
  Vec3 impact_point{};
  Vec3 impact_normal{0.0f, 1.0f, 0.0f};
  std::uint32_t seed = 1u;
  int shard_count = 9;
  float impact_seed_fraction = 0.55f;
  float impact_seed_spread = 0.42f;
  float random_seed_spread = 0.82f;
  std::vector<Vec3> seed_points;
};

struct VoronoiFractureShard {
  CpuMesh mesh{};
  Vec3 centroid{};
  Vec3 impulse_direction{0.0f, 1.0f, 0.0f};
  float relative_volume = 0.0f;
};

[[nodiscard]] CpuMesh makeConvexFractureBox(const ConvexFractureVolume &volume);

[[nodiscard]] std::vector<VoronoiFractureShard>
buildVoronoiFracture(const CpuMesh &source_mesh, const VoronoiFractureSpec &spec);

[[nodiscard]] std::vector<VoronoiFractureShard>
buildImpactVoronoiFracture(const VoronoiFractureSpec &spec);

} // namespace aster
