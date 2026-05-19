// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/geometry_operations.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <unordered_map>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] Vec3 triangleNormal(const Vertex &a, const Vertex &b, const Vertex &c) {
  return normalizeOr(cross(b.position - a.position, c.position - a.position),
                     {0.0f, 1.0f, 0.0f});
}

[[nodiscard]] float triangleAreaValue(const Vertex &a, const Vertex &b, const Vertex &c) {
  return length(cross(b.position - a.position, c.position - a.position)) * 0.5f;
}

struct DisjointSet {
  std::vector<std::uint32_t> parent;

  explicit DisjointSet(const std::size_t count) : parent(count) {
    std::iota(parent.begin(), parent.end(), 0u);
  }

  std::uint32_t find(const std::uint32_t value) {
    if (parent[value] != value) {
      parent[value] = find(parent[value]);
    }
    return parent[value];
  }

  void unite(const std::uint32_t a, const std::uint32_t b) {
    const std::uint32_t root_a = find(a);
    const std::uint32_t root_b = find(b);
    if (root_a != root_b) {
      parent[root_b] = root_a;
    }
  }
};

} // namespace

CpuMesh transformCpuMesh(const CpuMesh &mesh, const Transform &transform) {
  CpuMesh out = mesh;
  for (Vertex &vertex : out.vertices) {
    vertex.position = transformPoint(transform, vertex.position);
    vertex.normal = normalizeOr(transformVector(transform, vertex.normal), {0.0f, 1.0f, 0.0f});
    const Vec3 tangent = normalizeOr(
        transformVector(transform, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}),
        {1.0f, 0.0f, 0.0f});
    vertex.tangent = {tangent.x, tangent.y, tangent.z, vertex.tangent.w};
  }
  return out;
}

GeometrySet makeGeometrySet(CpuMesh mesh, std::string id) {
  GeometrySet set;
  set.meshes.push_back({.id = std::move(id), .mesh = std::move(mesh)});
  return set;
}

CpuMesh joinGeometryMeshes(const GeometrySet &set, const GeometryJoinOptions options) {
  CpuMesh out;
  std::size_t vertex_count = 0u;
  std::size_t index_count = 0u;
  for (const GeometryMeshPart &part : set.meshes) {
    vertex_count += part.mesh.vertices.size();
    index_count += part.mesh.indices.size();
  }
  out.vertices.reserve(vertex_count);
  out.indices.reserve(index_count);
  for (const GeometryMeshPart &part : set.meshes) {
    const CpuMesh mesh = options.apply_transforms ? transformCpuMesh(part.mesh, part.transform)
                                                  : part.mesh;
    const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
    out.vertices.insert(out.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    for (const std::uint32_t index : mesh.indices) {
      out.indices.push_back(base + index);
    }
  }
  if (options.rebuild_flat_normals) {
    for (std::size_t i = 0u; i + 2u < out.indices.size(); i += 3u) {
      Vertex &a = out.vertices[out.indices[i]];
      Vertex &b = out.vertices[out.indices[i + 1u]];
      Vertex &c = out.vertices[out.indices[i + 2u]];
      const Vec3 normal = triangleNormal(a, b, c);
      a.normal = normal;
      b.normal = normal;
      c.normal = normal;
    }
  }
  return out;
}

std::vector<CpuMesh> separateDisconnectedMeshIslands(const CpuMesh &mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    return {};
  }
  DisjointSet sets(mesh.vertices.size());
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t a = mesh.indices[i];
    const std::uint32_t b = mesh.indices[i + 1u];
    const std::uint32_t c = mesh.indices[i + 2u];
    if (a < mesh.vertices.size() && b < mesh.vertices.size() && c < mesh.vertices.size()) {
      sets.unite(a, b);
      sets.unite(a, c);
    }
  }

  std::unordered_map<std::uint32_t, std::size_t> island_index;
  std::vector<CpuMesh> islands;
  std::vector<std::unordered_map<std::uint32_t, std::uint32_t>> remaps;
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t root = sets.find(mesh.indices[i]);
    const auto [it, inserted] = island_index.emplace(root, islands.size());
    if (inserted) {
      islands.push_back({});
      remaps.emplace_back();
    }
    CpuMesh &island = islands[it->second];
    auto &remap = remaps[it->second];
    for (std::size_t corner = 0u; corner < 3u; ++corner) {
      const std::uint32_t source = mesh.indices[i + corner];
      const auto found = remap.find(source);
      if (found != remap.end()) {
        island.indices.push_back(found->second);
        continue;
      }
      const std::uint32_t next = static_cast<std::uint32_t>(island.vertices.size());
      remap.emplace(source, next);
      island.vertices.push_back(mesh.vertices[source]);
      island.indices.push_back(next);
    }
  }
  return islands;
}

std::vector<GeometryPoint> meshToPoints(const CpuMesh &mesh) {
  std::vector<GeometryPoint> points;
  points.reserve(mesh.vertices.size());
  for (std::size_t i = 0u; i < mesh.vertices.size(); ++i) {
    const Vertex &vertex = mesh.vertices[i];
    points.push_back({.position = vertex.position,
                      .normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f}),
                      .radius = 1.0f,
                      .seed = static_cast<std::uint32_t>(i)});
  }
  return points;
}

std::vector<GeometryPoint> scatterPointsOnMesh(const CpuMesh &mesh,
                                               const GeometryScatterOptions options) {
  if (mesh.indices.size() < 3u || options.count == 0u) {
    return {};
  }
  std::vector<float> cumulative;
  cumulative.reserve(mesh.indices.size() / 3u);
  float total_area = 0.0f;
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const Vertex &a = mesh.vertices[mesh.indices[i]];
    const Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    const Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    total_area += std::max(0.000001f, triangleAreaValue(a, b, c));
    cumulative.push_back(total_area);
  }
  std::mt19937 rng(options.seed);
  std::uniform_real_distribution<float> area_dist(0.0f, total_area);
  std::uniform_real_distribution<float> unit_dist(0.0f, 1.0f);
  std::vector<GeometryPoint> points;
  points.reserve(options.count);
  for (std::uint32_t i = 0u; i < options.count; ++i) {
    const auto found = std::lower_bound(cumulative.begin(), cumulative.end(), area_dist(rng));
    const std::size_t triangle = static_cast<std::size_t>(found - cumulative.begin());
    const std::size_t base = triangle * 3u;
    const Vertex &a = mesh.vertices[mesh.indices[base]];
    const Vertex &b = mesh.vertices[mesh.indices[base + 1u]];
    const Vertex &c = mesh.vertices[mesh.indices[base + 2u]];
    float u = unit_dist(rng);
    float v = unit_dist(rng);
    if (u + v > 1.0f) {
      u = 1.0f - u;
      v = 1.0f - v;
    }
    const float w = 1.0f - u - v;
    points.push_back({.position = a.position * w + b.position * u + c.position * v,
                      .normal = triangleNormal(a, b, c),
                      .radius = options.radius,
                      .seed = options.seed ^ (i * 747796405u)});
  }
  return points;
}

GeometrySet instanceMeshOnPoints(const CpuMesh &prototype, const std::vector<GeometryPoint> &points,
                                 std::string id_prefix) {
  GeometrySet set;
  set.meshes.reserve(points.size());
  for (std::size_t i = 0u; i < points.size(); ++i) {
    const GeometryPoint &point = points[i];
    GeometryMeshPart part;
    part.id = id_prefix + "." + std::to_string(i);
    part.mesh = prototype;
    part.transform.position = point.position;
    part.transform.scale = {point.radius, point.radius, point.radius};
    part.source_index = static_cast<std::uint32_t>(i);
    set.meshes.push_back(std::move(part));
  }
  return set;
}

} // namespace aster
