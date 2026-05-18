// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/authoring.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>

namespace aster {

struct MeshProcessOptions {
  bool drop_degenerate_triangles = true;
  bool rebuild_invalid_normals = true;
  bool generate_tangents = true;
  bool optimize_vertex_cache = true;
  bool optimize_vertex_fetch = true;
};

struct MeshDiagnostics {
  std::size_t input_vertices = 0;
  std::size_t input_indices = 0;
  std::size_t output_vertices = 0;
  std::size_t output_indices = 0;
  std::size_t degenerate_triangles = 0;
  std::size_t invalid_normals = 0;
  std::size_t generated_tangents = 0;
  std::size_t remapped_vertices = 0;
};

[[nodiscard]] CpuMesh prepareMeshForRendering(CpuMesh mesh, MeshProcessOptions options = {},
                                              MeshDiagnostics *diagnostics = nullptr);
[[nodiscard]] MeshMeasure measureMeshForAuthoring(const CpuMesh &mesh);

} // namespace aster
