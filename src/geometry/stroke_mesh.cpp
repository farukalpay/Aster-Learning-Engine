// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/stroke_mesh.hpp"

#include <algorithm>
#include <cstdint>

namespace aster {
namespace {

constexpr float kMinSegmentLength = 0.0001f;
constexpr float kMinStrokeWidth = 0.001f;

Vec2 perpendicular(const Vec2 value) {
  return {-value.y, value.x};
}

void appendRibbonSegment(CpuMesh &mesh, const StrokePoint &a, const StrokePoint &b, const float z) {
  const Vec2 delta = b.position - a.position;
  const float segment_length = length(delta);
  if (segment_length <= kMinSegmentLength) {
    return;
  }

  const Vec2 side = perpendicular(delta / segment_length);
  const float half_a = std::max(a.width, kMinStrokeWidth) * 0.5f;
  const float half_b = std::max(b.width, kMinStrokeWidth) * 0.5f;
  const Vec2 a0 = a.position - side * half_a;
  const Vec2 a1 = a.position + side * half_a;
  const Vec2 b0 = b.position - side * half_b;
  const Vec2 b1 = b.position + side * half_b;

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const Vec3 normal{0.0f, 0.0f, 1.0f};
  mesh.vertices.push_back({{a0.x, a0.y, z}, normal, {0.0f, 0.0f}});
  mesh.vertices.push_back({{b0.x, b0.y, z}, normal, {1.0f, 0.0f}});
  mesh.vertices.push_back({{b1.x, b1.y, z}, normal, {1.0f, 1.0f}});
  mesh.vertices.push_back({{a1.x, a1.y, z}, normal, {0.0f, 1.0f}});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
}

} // namespace

CpuMesh makeStrokeRibbonMesh(const StrokeMeshSpec &spec) {
  CpuMesh mesh;
  std::size_t segment_count = 0;
  for (const StrokePath &path : spec.paths) {
    if (path.points.size() >= 2u) {
      segment_count += path.points.size() - 1u;
    }
  }
  mesh.vertices.reserve(segment_count * 4u);
  mesh.indices.reserve(segment_count * 6u);

  for (const StrokePath &path : spec.paths) {
    for (std::size_t i = 1; i < path.points.size(); ++i) {
      appendRibbonSegment(mesh, path.points[i - 1u], path.points[i], spec.plane_z);
    }
  }

  return mesh;
}

} // namespace aster
