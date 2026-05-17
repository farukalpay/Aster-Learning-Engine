// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/mesh.hpp"

namespace aster {

struct TubeMeshSpec {
  float length = 1.0f;
  float outer_radius = 0.5f;
  float wall_thickness = 0.0f;
  int radial_segments = 32;
  int length_segments = 1;
};

struct CircumferentialBeadMeshSpec {
  float pipe_radius = 0.5f;
  float bead_radius = 0.035f;
  float axial_width = 0.10f;
  int radial_segments = 48;
  int bead_segments = 10;
};

[[nodiscard]] CpuMesh makeTubeMesh(const TubeMeshSpec &spec);
[[nodiscard]] CpuMesh makeCircumferentialBeadMesh(const CircumferentialBeadMeshSpec &spec);

} // namespace aster
