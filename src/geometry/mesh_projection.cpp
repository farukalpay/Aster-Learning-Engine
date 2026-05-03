// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/mesh_projection.hpp"

#include <algorithm>
#include <stdexcept>

namespace aster {

CpuMesh drapeMeshToSurface(CpuMesh mesh, const MeshSurfaceSampler &sampler,
                           const MeshDrapeSettings settings) {
  if (!sampler) {
    throw std::invalid_argument("Mesh draping requires a surface sampler.");
  }
  const float surface_offset = std::max(settings.surface_offset, 0.0f);
  for (Vertex &vertex : mesh.vertices) {
    const TerrainSurfaceSample sample = sampler({vertex.position.x, vertex.position.z});
    if (!sample.valid) {
      continue;
    }
    const float local_offset =
        settings.preserve_vertical_offset ? vertex.position.y - settings.reference_y : 0.0f;
    const float projected_y = sample.height + surface_offset + local_offset;
    vertex.position.y =
        settings.raise_only ? std::max(vertex.position.y, projected_y) : projected_y;
  }
  return mesh;
}

} // namespace aster
