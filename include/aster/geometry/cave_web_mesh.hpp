// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>

namespace aster {

struct CaveWebMeshSpec {
  Vec3 center{};
  Vec3 normal{0.0f, 0.0f, -1.0f};
  Vec3 side{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float radius_x = 1.0f;
  float radius_y = 1.0f;
  int radial_strands = 18;
  int ring_strands = 6;
  int ring_segments = 96;
  float strand_width = 0.018f;
  float anchor_width_scale = 1.24f;
  float sag = 0.10f;
  float irregularity = 0.07f;
  std::uint32_t seed = 1u;
  bool double_sided = true;
};

[[nodiscard]] CpuMesh makeCaveWebMesh(CaveWebMeshSpec spec = {});

} // namespace aster
