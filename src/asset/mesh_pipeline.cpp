// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/mesh_pipeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace aster {
namespace {

constexpr float kRelativeAreaTolerance =
    std::numeric_limits<float>::epsilon() * std::numeric_limits<float>::epsilon() * 64.0f;

bool finite(const float value) {
  return std::isfinite(value);
}

bool finite(const Vec2 value) {
  return finite(value.x) && finite(value.y);
}

bool finite(const Vec3 value) {
  return finite(value.x) && finite(value.y) && finite(value.z);
}

bool validNormal(const Vec3 normal) {
  const float normal_length = length(normal);
  return finite(normal) && normal_length > 0.5f && normal_length < 1.5f;
}

float triangleAreaSquared(const Vertex &a, const Vertex &b, const Vertex &c) {
  const Vec3 area = cross(b.position - a.position, c.position - a.position);
  return dot(area, area);
}

bool degenerateTriangle(const Vertex &a, const Vertex &b, const Vertex &c) {
  const Vec3 ab = b.position - a.position;
  const Vec3 ac = c.position - a.position;
  const Vec3 bc = c.position - b.position;
  const float edge_scale_squared = std::max({dot(ab, ab), dot(ac, ac), dot(bc, bc)});
  if (edge_scale_squared <= std::numeric_limits<float>::epsilon()) {
    return true;
  }

  const float authoring_area = triangleArea(a.position, b.position, c.position);
  return authoring_area <= std::numeric_limits<float>::epsilon() ||
         triangleAreaSquared(a, b, c) <=
         edge_scale_squared * edge_scale_squared * kRelativeAreaTolerance;
}

Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return normalizeOr(cross(b - a, c - a), {0.0f, 1.0f, 0.0f});
}

void validateMesh(const CpuMesh &mesh) {
  if (mesh.vertices.empty()) {
    throw std::invalid_argument("Mesh contains no vertices.");
  }
  if (mesh.indices.empty()) {
    throw std::invalid_argument("Mesh contains no indices.");
  }
  if (mesh.indices.size() % 3u != 0u) {
    throw std::invalid_argument("Mesh index count must be divisible by 3.");
  }

  for (const Vertex &vertex : mesh.vertices) {
    if (!finite(vertex.position) || !finite(vertex.uv)) {
      throw std::invalid_argument("Mesh contains non-finite vertex attributes.");
    }
  }

  for (const std::uint32_t index : mesh.indices) {
    if (index >= mesh.vertices.size()) {
      throw std::invalid_argument("Mesh contains an index outside the vertex buffer.");
    }
  }
}

void dropDegenerateTriangles(CpuMesh &mesh, MeshDiagnostics &diagnostics) {
  std::vector<std::uint32_t> indices;
  indices.reserve(mesh.indices.size());

  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const std::uint32_t a = mesh.indices[i + 0u];
    const std::uint32_t b = mesh.indices[i + 1u];
    const std::uint32_t c = mesh.indices[i + 2u];
    if (a == b || b == c || c == a ||
        degenerateTriangle(mesh.vertices[a], mesh.vertices[b], mesh.vertices[c])) {
      ++diagnostics.degenerate_triangles;
      continue;
    }
    indices.insert(indices.end(), {a, b, c});
  }

  if (indices.empty()) {
    throw std::invalid_argument("Mesh contains no renderable triangles after validation.");
  }

  mesh.indices = std::move(indices);
}

std::size_t countInvalidNormals(const CpuMesh &mesh) {
  return static_cast<std::size_t>(
      std::count_if(mesh.vertices.begin(), mesh.vertices.end(),
                    [](const Vertex &vertex) { return !validNormal(vertex.normal); }));
}

void rebuildNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }

  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    Vertex &a = mesh.vertices[mesh.indices[i + 0u]];
    Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const Vec3 normal = cross(b.position - a.position, c.position - a.position);
    a.normal = a.normal + normal;
    b.normal = b.normal + normal;
    c.normal = c.normal + normal;
  }

  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f});
    if (!validNormal(vertex.normal)) {
      vertex.normal = {0.0f, 1.0f, 0.0f};
    }
  }
}

CpuMesh expandIndexedTriangles(const CpuMesh &mesh) {
  CpuMesh expanded;
  expanded.vertices.reserve(mesh.indices.size());
  expanded.indices.reserve(mesh.indices.size());

  for (const std::uint32_t index : mesh.indices) {
    expanded.indices.push_back(static_cast<std::uint32_t>(expanded.vertices.size()));
    expanded.vertices.push_back(mesh.vertices[index]);
  }

  return expanded;
}

void generateTangents(CpuMesh &mesh, MeshDiagnostics &diagnostics) {
  CpuMesh expanded = expandIndexedTriangles(mesh);

  for (std::size_t i = 0; i < expanded.indices.size(); i += 3u) {
    Vertex &a = expanded.vertices[expanded.indices[i + 0u]];
    Vertex &b = expanded.vertices[expanded.indices[i + 1u]];
    Vertex &c = expanded.vertices[expanded.indices[i + 2u]];

    const Vec3 edge_ab = b.position - a.position;
    const Vec3 edge_ac = c.position - a.position;
    const Vec2 uv_ab = b.uv - a.uv;
    const Vec2 uv_ac = c.uv - a.uv;
    const float denominator = uv_ab.x * uv_ac.y - uv_ac.x * uv_ab.y;

    Vec3 tangent{};
    float sign = 1.0f;
    if (std::abs(denominator) > 0.000001f) {
      const float inv = 1.0f / denominator;
      tangent = (edge_ab * uv_ac.y - edge_ac * uv_ab.y) * inv;
      const Vec3 bitangent = (edge_ac * uv_ab.x - edge_ab * uv_ac.x) * inv;
      const Vec3 n = normalizeOr(a.normal + b.normal + c.normal,
                                 faceNormal(a.position, b.position, c.position));
      sign = dot(cross(n, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
    } else {
      const Vec3 n = normalizeOr(a.normal + b.normal + c.normal,
                                 faceNormal(a.position, b.position, c.position));
      const Vec3 reference = std::abs(n.y) > 0.80f ? Vec3{1.0f, 0.0f, 0.0f}
                                                    : Vec3{0.0f, 1.0f, 0.0f};
      tangent = normalizeOr(cross(reference, n), {1.0f, 0.0f, 0.0f});
    }

    const auto assign_tangent = [&](Vertex &vertex) {
      Vec3 t = tangent - vertex.normal * dot(vertex.normal, tangent);
      if (length(t) <= 0.0001f) {
        const Vec3 reference = std::abs(vertex.normal.y) > 0.80f ? Vec3{1.0f, 0.0f, 0.0f}
                                                                 : Vec3{0.0f, 1.0f, 0.0f};
        t = cross(reference, vertex.normal);
      }
      t = normalizeOr(t, {1.0f, 0.0f, 0.0f});
      vertex.tangent = {t.x, t.y, t.z, sign};
    };

    assign_tangent(a);
    assign_tangent(b);
    assign_tangent(c);
  }

  diagnostics.generated_tangents = expanded.vertices.size();
  mesh = std::move(expanded);
}

struct VertexKey {
  Vertex vertex{};

  bool operator==(const VertexKey &other) const {
    return std::memcmp(&vertex, &other.vertex, sizeof(Vertex)) == 0;
  }
};

struct VertexKeyHash {
  std::size_t operator()(const VertexKey &key) const {
    const auto *bytes = reinterpret_cast<const unsigned char *>(&key.vertex);
    std::size_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < sizeof(Vertex); ++i) {
      hash ^= bytes[i];
      hash *= 1099511628211ull;
    }
    return hash;
  }
};

void compactEquivalentVertices(CpuMesh &mesh, MeshDiagnostics &diagnostics) {
  std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> remap;
  remap.reserve(mesh.vertices.size());
  std::vector<Vertex> vertices;
  vertices.reserve(mesh.vertices.size());
  std::vector<std::uint32_t> indices;
  indices.reserve(mesh.indices.size());

  for (const std::uint32_t source_index : mesh.indices) {
    const VertexKey key{mesh.vertices[source_index]};
    const auto existing = remap.find(key);
    if (existing != remap.end()) {
      indices.push_back(existing->second);
      continue;
    }

    const std::uint32_t new_index = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back(key.vertex);
    remap.emplace(key, new_index);
    indices.push_back(new_index);
  }

  diagnostics.remapped_vertices = mesh.vertices.size() - vertices.size();
  mesh.vertices = std::move(vertices);
  mesh.indices = std::move(indices);
}

void optimizeIndicesAndVertices(CpuMesh &mesh, const MeshProcessOptions &options) {
  if (options.optimize_vertex_cache) {
    const std::size_t triangle_count = mesh.indices.size() / 3u;
    std::vector<std::vector<std::uint32_t>> vertex_triangles(mesh.vertices.size());
    for (std::uint32_t triangle = 0; triangle < triangle_count; ++triangle) {
      const std::size_t base = static_cast<std::size_t>(triangle) * 3u;
      vertex_triangles[mesh.indices[base + 0u]].push_back(triangle);
      vertex_triangles[mesh.indices[base + 1u]].push_back(triangle);
      vertex_triangles[mesh.indices[base + 2u]].push_back(triangle);
    }

    std::vector<std::uint32_t> reordered;
    reordered.reserve(mesh.indices.size());
    std::vector<std::uint8_t> state(triangle_count, 0u);
    std::deque<std::uint32_t> pending;

    const auto queue_triangle = [&](const std::uint32_t triangle) {
      if (triangle < state.size() && state[triangle] == 0u) {
        state[triangle] = 1u;
        pending.push_back(triangle);
      }
    };

    for (std::uint32_t seed = 0; seed < triangle_count; ++seed) {
      queue_triangle(seed);
      while (!pending.empty()) {
        const std::uint32_t triangle = pending.front();
        pending.pop_front();
        if (state[triangle] == 2u) {
          continue;
        }
        state[triangle] = 2u;
        const std::size_t base = static_cast<std::size_t>(triangle) * 3u;
        const std::array<std::uint32_t, 3> corners{mesh.indices[base + 0u],
                                                   mesh.indices[base + 1u],
                                                   mesh.indices[base + 2u]};
        reordered.insert(reordered.end(), corners.begin(), corners.end());
        for (const std::uint32_t vertex : corners) {
          for (const std::uint32_t adjacent : vertex_triangles[vertex]) {
            queue_triangle(adjacent);
          }
        }
      }

      if (state[seed] == 2u) {
        continue;
      }
    }

    if (reordered.size() == mesh.indices.size()) {
      mesh.indices = std::move(reordered);
    }
  }

  if (options.optimize_vertex_fetch) {
    std::vector<std::uint32_t> remap(mesh.vertices.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (std::uint32_t &index : mesh.indices) {
      if (remap[index] == std::numeric_limits<std::uint32_t>::max()) {
        remap[index] = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(mesh.vertices[index]);
      }
      index = remap[index];
    }
    mesh.vertices = std::move(vertices);
  }
}

} // namespace

CpuMesh prepareMeshForRendering(CpuMesh mesh, const MeshProcessOptions options,
                                MeshDiagnostics *diagnostics) {
  MeshDiagnostics local{};
  local.input_vertices = mesh.vertices.size();
  local.input_indices = mesh.indices.size();

  validateMesh(mesh);

  if (options.drop_degenerate_triangles) {
    dropDegenerateTriangles(mesh, local);
  }

  local.invalid_normals = countInvalidNormals(mesh);
  if (local.invalid_normals > 0u) {
    if (!options.rebuild_invalid_normals) {
      throw std::invalid_argument("Mesh contains invalid normals.");
    }
    rebuildNormals(mesh);
  }

  if (options.generate_tangents) {
    generateTangents(mesh, local);
    compactEquivalentVertices(mesh, local);
  }

  optimizeIndicesAndVertices(mesh, options);

  local.output_vertices = mesh.vertices.size();
  local.output_indices = mesh.indices.size();
  if (diagnostics != nullptr) {
    *diagnostics = local;
  }
  return mesh;
}

MeshMeasure measureMeshForAuthoring(const CpuMesh &mesh) {
  std::vector<Vec3> positions;
  std::vector<Vec2> uvs;
  positions.reserve(mesh.vertices.size());
  uvs.reserve(mesh.vertices.size());
  for (const Vertex &vertex : mesh.vertices) {
    positions.push_back(vertex.position);
    uvs.push_back(vertex.uv);
  }
  return measureTriangleMesh(positions, mesh.indices, uvs);
}

} // namespace aster
