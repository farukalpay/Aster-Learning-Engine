// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aster {

struct MeshBounds {
  bool valid = false;
  Vec3 min{};
  Vec3 max{};
};

enum class QuadTriangulationMode {
  FixedDiagonal02,
  FixedDiagonal13,
  ShortestDiagonal,
  LongestDiagonal,
  Beauty,
};

struct MeshTopologyOptions {
  float degenerate_area_epsilon = 0.00000001f;
};

struct MeshTopologyReport {
  std::size_t vertices = 0;
  std::size_t indices = 0;
  std::size_t triangles = 0;
  std::size_t invalid_indices = 0;
  std::size_t degenerate_triangles = 0;
  std::size_t open_edges = 0;
  std::size_t non_manifold_edges = 0;
  std::size_t inconsistent_winding_edges = 0;
  std::size_t isolated_vertices = 0;
  MeshBounds bounds{};

  [[nodiscard]] bool indexable() const;
  [[nodiscard]] bool renderable() const;
  [[nodiscard]] bool closedManifold() const;
};

[[nodiscard]] MeshBounds calculateMeshBounds(const CpuMesh &mesh);
[[nodiscard]] Vec3 meshBoundsCenter(const MeshBounds &bounds, Vec3 fallback = {});
[[nodiscard]] float meshBoundsRadius(const MeshBounds &bounds, Vec3 center);

[[nodiscard]] float triangleArea(Vec3 a, Vec3 b, Vec3 c);
[[nodiscard]] float triangleAreaSquared(Vec3 a, Vec3 b, Vec3 c);
[[nodiscard]] Vec3 triangleNormal(Vec3 a, Vec3 b, Vec3 c, Vec3 fallback = {0.0f, 1.0f, 0.0f});
[[nodiscard]] bool degenerateTriangle(Vec3 a, Vec3 b, Vec3 c, float area_epsilon = 0.00000001f);

[[nodiscard]] std::array<std::array<std::uint32_t, 3>, 2>
triangulateStableQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d,
                      Vec3 preferred_normal = {0.0f, 1.0f, 0.0f},
                      QuadTriangulationMode mode = QuadTriangulationMode::Beauty);

[[nodiscard]] std::uint32_t appendMeshVertex(CpuMesh &mesh, const Vertex &vertex);
void appendOrientedTriangle(CpuMesh &mesh, Vertex a, Vertex b, Vertex c, Vec3 preferred_normal);
void appendOrientedQuad(CpuMesh &mesh, Vertex a, Vertex b, Vertex c, Vertex d,
                        Vec3 preferred_normal,
                        QuadTriangulationMode mode = QuadTriangulationMode::Beauty);
void appendOrientedTriangleIndices(CpuMesh &mesh, std::uint32_t a, std::uint32_t b,
                                   std::uint32_t c, Vec3 preferred_normal);
void appendOrientedQuadIndices(CpuMesh &mesh, std::uint32_t a, std::uint32_t b,
                               std::uint32_t c, std::uint32_t d, Vec3 preferred_normal,
                               QuadTriangulationMode mode = QuadTriangulationMode::Beauty);

void rebuildAngleWeightedNormals(CpuMesh &mesh, Vec3 fallback = {0.0f, 1.0f, 0.0f});
[[nodiscard]] MeshTopologyReport
validateMeshTopology(const CpuMesh &mesh, MeshTopologyOptions options = {});

} // namespace aster
