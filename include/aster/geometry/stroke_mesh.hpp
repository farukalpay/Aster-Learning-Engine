// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <vector>

namespace aster {

struct StrokePoint {
  Vec2 position{};
  float width = 0.035f;
};

struct StrokePath {
  std::vector<StrokePoint> points;
};

struct StrokeMeshSpec {
  std::vector<StrokePath> paths;
  float plane_z = 0.0f;
};

[[nodiscard]] CpuMesh makeStrokeRibbonMesh(const StrokeMeshSpec &spec);

} // namespace aster
