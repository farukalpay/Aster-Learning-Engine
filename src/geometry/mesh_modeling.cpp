// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/mesh_modeling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace aster {
namespace {

constexpr float kModelingEpsilon = 0.000001f;

struct EdgeKey {
  std::uint32_t a = 0u;
  std::uint32_t b = 0u;

  [[nodiscard]] friend bool operator==(const EdgeKey lhs, const EdgeKey rhs) {
    return lhs.a == rhs.a && lhs.b == rhs.b;
  }
};

struct EdgeKeyHash {
  [[nodiscard]] std::size_t operator()(const EdgeKey edge) const noexcept {
    std::size_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint32_t value) {
      hash ^= static_cast<std::size_t>(value);
      hash *= 1099511628211ull;
    };
    mix(edge.a);
    mix(edge.b);
    return hash;
  }
};

struct EdgeUse {
  std::size_t count = 0u;
  int winding_balance = 0;
};

[[nodiscard]] float lengthSquared(const Vec3 value) {
  return dot(value, value);
}

[[nodiscard]] Vec3 normalizedOr(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > kModelingEpsilon ? normalized : fallback;
}

[[nodiscard]] float safeAngleBetween(Vec3 lhs, Vec3 rhs) {
  lhs = normalizedOr(lhs, {});
  rhs = normalizedOr(rhs, {});
  if (length(lhs) <= kModelingEpsilon || length(rhs) <= kModelingEpsilon) {
    return 0.0f;
  }
  return std::acos(clamp(dot(lhs, rhs), -1.0f, 1.0f));
}

[[nodiscard]] float quadAreaBalanceScore(const Vec3 a, const Vec3 b, const Vec3 c,
                                         const Vec3 d, const bool diagonal_02) {
  const float area_a = diagonal_02 ? triangleArea(a, b, c) : triangleArea(b, c, d);
  const float area_b = diagonal_02 ? triangleArea(a, c, d) : triangleArea(b, d, a);
  const float total = std::max(area_a + area_b, kModelingEpsilon);
  return std::abs(area_a - area_b) / total;
}

[[nodiscard]] float quadNormalScore(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                                    const Vec3 preferred_normal, const bool diagonal_02) {
  const Vec3 normal_a = diagonal_02 ? triangleNormal(a, b, c, preferred_normal)
                                    : triangleNormal(b, c, d, preferred_normal);
  const Vec3 normal_b = diagonal_02 ? triangleNormal(a, c, d, preferred_normal)
                                    : triangleNormal(b, d, a, preferred_normal);
  const Vec3 preferred = normalizedOr(preferred_normal, normalizedOr(normal_a + normal_b,
                                                                     {0.0f, 1.0f, 0.0f}));
  const float fold = 1.0f - clamp(dot(normal_a, normal_b), -1.0f, 1.0f);
  const float facing = (dot(normal_a, preferred) < 0.0f ? 1.0f : 0.0f) +
                       (dot(normal_b, preferred) < 0.0f ? 1.0f : 0.0f);
  return fold + facing * 4.0f;
}

[[nodiscard]] bool chooseDiagonal02(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                                    const Vec3 preferred_normal,
                                    const QuadTriangulationMode mode) {
  const float diag_02 = lengthSquared(c - a);
  const float diag_13 = lengthSquared(d - b);
  switch (mode) {
  case QuadTriangulationMode::FixedDiagonal02:
    return true;
  case QuadTriangulationMode::FixedDiagonal13:
    return false;
  case QuadTriangulationMode::ShortestDiagonal:
    return diag_02 <= diag_13;
  case QuadTriangulationMode::LongestDiagonal:
    return diag_02 >= diag_13;
  case QuadTriangulationMode::Beauty: {
    const float score_02 = quadNormalScore(a, b, c, d, preferred_normal, true) +
                           quadAreaBalanceScore(a, b, c, d, true) + diag_02 * 0.0001f;
    const float score_13 = quadNormalScore(a, b, c, d, preferred_normal, false) +
                           quadAreaBalanceScore(a, b, c, d, false) + diag_13 * 0.0001f;
    return score_02 <= score_13;
  }
  }
  return true;
}

[[nodiscard]] bool validIndex(const CpuMesh &mesh, const std::uint32_t index) {
  return index < mesh.vertices.size();
}

void accumulateEdge(std::unordered_map<EdgeKey, EdgeUse, EdgeKeyHash> &edges,
                    const std::uint32_t from, const std::uint32_t to) {
  const EdgeKey key{std::min(from, to), std::max(from, to)};
  EdgeUse &use = edges[key];
  ++use.count;
  use.winding_balance += from < to ? 1 : -1;
}

} // namespace

bool MeshTopologyReport::indexable() const {
  return invalid_indices == 0u && indices % 3u == 0u;
}

bool MeshTopologyReport::renderable() const {
  return indexable() && triangles > 0u && degenerate_triangles == 0u;
}

bool MeshTopologyReport::closedManifold() const {
  return renderable() && open_edges == 0u && non_manifold_edges == 0u &&
         inconsistent_winding_edges == 0u;
}

MeshBounds calculateMeshBounds(const CpuMesh &mesh) {
  MeshBounds bounds;
  if (mesh.vertices.empty()) {
    return bounds;
  }
  bounds.valid = true;
  bounds.min = mesh.vertices.front().position;
  bounds.max = mesh.vertices.front().position;
  for (const Vertex &vertex : mesh.vertices) {
    bounds.min.x = std::min(bounds.min.x, vertex.position.x);
    bounds.min.y = std::min(bounds.min.y, vertex.position.y);
    bounds.min.z = std::min(bounds.min.z, vertex.position.z);
    bounds.max.x = std::max(bounds.max.x, vertex.position.x);
    bounds.max.y = std::max(bounds.max.y, vertex.position.y);
    bounds.max.z = std::max(bounds.max.z, vertex.position.z);
  }
  return bounds;
}

Vec3 meshBoundsCenter(const MeshBounds &bounds, const Vec3 fallback) {
  return bounds.valid ? (bounds.min + bounds.max) * 0.5f : fallback;
}

float meshBoundsRadius(const MeshBounds &bounds, const Vec3 center) {
  return bounds.valid ? length(bounds.max - center) : 0.0f;
}

float triangleAreaSquared(const Vec3 a, const Vec3 b, const Vec3 c) {
  const Vec3 area = cross(b - a, c - a) * 0.5f;
  return dot(area, area);
}

float triangleArea(const Vec3 a, const Vec3 b, const Vec3 c) {
  return std::sqrt(triangleAreaSquared(a, b, c));
}

Vec3 triangleNormal(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 fallback) {
  return normalizedOr(cross(b - a, c - a), fallback);
}

bool degenerateTriangle(const Vec3 a, const Vec3 b, const Vec3 c, const float area_epsilon) {
  const float edge_scale =
      std::max({lengthSquared(b - a), lengthSquared(c - a), lengthSquared(c - b)});
  if (edge_scale <= std::numeric_limits<float>::epsilon()) {
    return true;
  }
  return triangleAreaSquared(a, b, c) <= edge_scale * edge_scale * std::max(area_epsilon, 0.0f);
}

std::array<std::array<std::uint32_t, 3>, 2>
triangulateStableQuad(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                      const Vec3 preferred_normal, const QuadTriangulationMode mode) {
  if (chooseDiagonal02(a, b, c, d, preferred_normal, mode)) {
    return {{{0u, 1u, 2u}, {0u, 2u, 3u}}};
  }
  return {{{1u, 2u, 3u}, {1u, 3u, 0u}}};
}

std::uint32_t appendMeshVertex(CpuMesh &mesh, const Vertex &vertex) {
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(vertex);
  return index;
}

void appendOrientedTriangle(CpuMesh &mesh, Vertex a, Vertex b, Vertex c, Vec3 preferred_normal) {
  preferred_normal = normalizedOr(preferred_normal, normalizedOr(a.normal + b.normal + c.normal,
                                                                 {0.0f, 1.0f, 0.0f}));
  const Vec3 face_normal = normalize(cross(b.position - a.position, c.position - a.position));
  if (length(face_normal) <= kModelingEpsilon) {
    return;
  }
  const std::uint32_t ia = appendMeshVertex(mesh, a);
  if (dot(face_normal, preferred_normal) < 0.0f) {
    const std::uint32_t ib = appendMeshVertex(mesh, c);
    const std::uint32_t ic = appendMeshVertex(mesh, b);
    mesh.indices.insert(mesh.indices.end(), {ia, ib, ic});
    return;
  }
  const std::uint32_t ib = appendMeshVertex(mesh, b);
  const std::uint32_t ic = appendMeshVertex(mesh, c);
  mesh.indices.insert(mesh.indices.end(), {ia, ib, ic});
}

void appendOrientedQuad(CpuMesh &mesh, Vertex a, Vertex b, Vertex c, Vertex d,
                        const Vec3 preferred_normal, const QuadTriangulationMode mode) {
  const std::array<Vertex, 4> vertices{a, b, c, d};
  const auto triangles =
      triangulateStableQuad(a.position, b.position, c.position, d.position, preferred_normal, mode);
  for (const auto triangle : triangles) {
    appendOrientedTriangle(mesh, vertices[triangle[0]], vertices[triangle[1]],
                           vertices[triangle[2]], preferred_normal);
  }
}

void appendOrientedTriangleIndices(CpuMesh &mesh, std::uint32_t a, std::uint32_t b,
                                   std::uint32_t c, Vec3 preferred_normal) {
  if (!validIndex(mesh, a) || !validIndex(mesh, b) || !validIndex(mesh, c)) {
    return;
  }
  const Vec3 pa = mesh.vertices[a].position;
  const Vec3 pb = mesh.vertices[b].position;
  const Vec3 pc = mesh.vertices[c].position;
  if (degenerateTriangle(pa, pb, pc)) {
    return;
  }
  preferred_normal = normalizedOr(
      preferred_normal,
      normalizedOr(mesh.vertices[a].normal + mesh.vertices[b].normal + mesh.vertices[c].normal,
                   {0.0f, 1.0f, 0.0f}));
  const Vec3 face_normal = triangleNormal(pa, pb, pc, preferred_normal);
  if (dot(face_normal, preferred_normal) < 0.0f) {
    std::swap(b, c);
  }
  mesh.indices.insert(mesh.indices.end(), {a, b, c});
}

void appendOrientedQuadIndices(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                               const std::uint32_t c, const std::uint32_t d,
                               const Vec3 preferred_normal, const QuadTriangulationMode mode) {
  if (!validIndex(mesh, a) || !validIndex(mesh, b) || !validIndex(mesh, c) ||
      !validIndex(mesh, d)) {
    return;
  }
  const std::array<std::uint32_t, 4> indices{a, b, c, d};
  const auto triangles = triangulateStableQuad(mesh.vertices[a].position, mesh.vertices[b].position,
                                               mesh.vertices[c].position, mesh.vertices[d].position,
                                               preferred_normal, mode);
  for (const auto triangle : triangles) {
    appendOrientedTriangleIndices(mesh, indices[triangle[0]], indices[triangle[1]],
                                  indices[triangle[2]], preferred_normal);
  }
}

void rebuildAngleWeightedNormals(CpuMesh &mesh, const Vec3 fallback) {
  std::vector<Vec3> normals(mesh.vertices.size());
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }
    const Vec3 a = mesh.vertices[ia].position;
    const Vec3 b = mesh.vertices[ib].position;
    const Vec3 c = mesh.vertices[ic].position;
    const Vec3 normal = normalize(cross(b - a, c - a));
    if (length(normal) <= kModelingEpsilon) {
      continue;
    }
    normals[ia] = normals[ia] + normal * safeAngleBetween(b - a, c - a);
    normals[ib] = normals[ib] + normal * safeAngleBetween(c - b, a - b);
    normals[ic] = normals[ic] + normal * safeAngleBetween(a - c, b - c);
  }
  const Vec3 fallback_normal = normalizedOr(fallback, {0.0f, 1.0f, 0.0f});
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    mesh.vertices[i].normal = normalizedOr(normals[i], fallback_normal);
  }
}

MeshTopologyReport validateMeshTopology(const CpuMesh &mesh, const MeshTopologyOptions options) {
  MeshTopologyReport report;
  report.vertices = mesh.vertices.size();
  report.indices = mesh.indices.size();
  report.triangles = mesh.indices.size() / 3u;
  report.bounds = calculateMeshBounds(mesh);

  if (mesh.indices.size() % 3u != 0u) {
    report.invalid_indices += mesh.indices.size() % 3u;
  }

  std::vector<bool> used_vertices(mesh.vertices.size(), false);
  std::unordered_map<EdgeKey, EdgeUse, EdgeKeyHash> edges;
  edges.reserve(mesh.indices.size());

  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      report.invalid_indices += (ia >= mesh.vertices.size() ? 1u : 0u) +
                                (ib >= mesh.vertices.size() ? 1u : 0u) +
                                (ic >= mesh.vertices.size() ? 1u : 0u);
      continue;
    }
    used_vertices[ia] = true;
    used_vertices[ib] = true;
    used_vertices[ic] = true;
    if (ia == ib || ib == ic || ic == ia ||
        degenerateTriangle(mesh.vertices[ia].position, mesh.vertices[ib].position,
                           mesh.vertices[ic].position, options.degenerate_area_epsilon)) {
      ++report.degenerate_triangles;
      continue;
    }
    accumulateEdge(edges, ia, ib);
    accumulateEdge(edges, ib, ic);
    accumulateEdge(edges, ic, ia);
  }

  for (const bool used : used_vertices) {
    if (!used) {
      ++report.isolated_vertices;
    }
  }
  for (const auto &[_, use] : edges) {
    if (use.count == 1u) {
      ++report.open_edges;
    } else if (use.count > 2u) {
      ++report.non_manifold_edges;
    } else if (std::abs(use.winding_balance) == 2) {
      ++report.inconsistent_winding_edges;
    }
  }
  return report;
}

} // namespace aster
