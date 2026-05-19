// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/transform.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

struct GeometryPoint {
  Vec3 position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float radius = 1.0f;
  std::uint32_t seed = 0u;
};

struct GeometryMeshPart {
  std::string id;
  CpuMesh mesh;
  Transform transform{};
  std::string material_tag;
  std::uint32_t source_index = 0u;
};

struct GeometrySet {
  std::vector<GeometryMeshPart> meshes;
  std::vector<GeometryPoint> points;
  std::vector<std::string> diagnostics;
};

struct GeometryJoinOptions {
  bool apply_transforms = true;
  bool rebuild_flat_normals = false;
};

struct GeometryScatterOptions {
  std::uint32_t count = 0u;
  std::uint32_t seed = 1u;
  float radius = 1.0f;
};

[[nodiscard]] CpuMesh transformCpuMesh(const CpuMesh &mesh, const Transform &transform);
[[nodiscard]] GeometrySet makeGeometrySet(CpuMesh mesh, std::string id = {});
[[nodiscard]] CpuMesh joinGeometryMeshes(const GeometrySet &set,
                                         GeometryJoinOptions options = {});
[[nodiscard]] std::vector<CpuMesh> separateDisconnectedMeshIslands(const CpuMesh &mesh);
[[nodiscard]] std::vector<GeometryPoint> meshToPoints(const CpuMesh &mesh);
[[nodiscard]] std::vector<GeometryPoint> scatterPointsOnMesh(const CpuMesh &mesh,
                                                             GeometryScatterOptions options);
[[nodiscard]] GeometrySet instanceMeshOnPoints(const CpuMesh &prototype,
                                               const std::vector<GeometryPoint> &points,
                                               std::string id_prefix = "instance");

} // namespace aster
