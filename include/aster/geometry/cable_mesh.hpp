// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/mesh.hpp"

namespace aster {

enum class CableConstruction {
  BraidedSleeve,
  TwistedStrands,
};

struct CableMeshSpec {
  CableConstruction construction = CableConstruction::BraidedSleeve;
  int radial_segments = 18;
  int length_segments = 18;
  int strand_count = 8;
  float radius = 0.5f;
  float length = 1.0f;
  float strand_depth = 0.045f;
  float twist_turns = 2.25f;
  float strand_radius = 0.34f;
  float strand_center_radius = 0.56f;
  float fiber_twist_turns = 7.0f;
};

[[nodiscard]] CpuMesh makeCableMesh(CableMeshSpec spec = {});

} // namespace aster
