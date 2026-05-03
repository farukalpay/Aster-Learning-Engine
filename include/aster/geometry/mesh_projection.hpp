// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/terrain_mesh.hpp"
#include "aster/render/mesh.hpp"

#include <functional>

namespace aster {

using MeshSurfaceSampler = std::function<TerrainSurfaceSample(Vec2)>;

struct MeshDrapeSettings {
  float surface_offset = 0.025f;
  bool raise_only = true;
  bool preserve_vertical_offset = false;
  float reference_y = 0.0f;
};

[[nodiscard]] CpuMesh drapeMeshToSurface(CpuMesh mesh, const MeshSurfaceSampler &sampler,
                                         MeshDrapeSettings settings = {});

} // namespace aster
