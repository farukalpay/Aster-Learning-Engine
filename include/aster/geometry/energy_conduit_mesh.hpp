// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <vector>

namespace aster {

struct EnergyConduitRibbonSpec {
  std::vector<Vec3> points;
  float width = 0.16f;
  float crest_height = 0.20f;
  int subdivisions_per_segment = 10;
  bool double_sided = true;
};

struct EnergyConduitRingSpec {
  float radius = 1.0f;
  float band_width = 0.08f;
  int segments = 64;
  bool double_sided = true;
};

[[nodiscard]] CpuMesh makeEnergyConduitRibbonMesh(const EnergyConduitRibbonSpec &spec);
[[nodiscard]] CpuMesh
makeEnergyConduitRibbonNetworkMesh(const std::vector<EnergyConduitRibbonSpec> &ribbons);
[[nodiscard]] CpuMesh makeEnergyConduitRingMesh(EnergyConduitRingSpec spec = {});

} // namespace aster
