// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/mesh_pipeline.hpp"

#include <mesh_workbench.h>
#include <surface_basis.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aster {
namespace {

constexpr float kRelativeAreaTolerance =
    std::numeric_limits<float>::epsilon() * std::numeric_limits<float>::epsilon() * 64.0f;

struct TangentBuildMesh {
  CpuMesh *mesh = nullptr;
};

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

  return triangleAreaSquared(a, b, c) <=
         edge_scale_squared * edge_scale_squared * kRelativeAreaTolerance;
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
    vertex.normal = normalize(vertex.normal);
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

int basisFaceCount(const SSurfaceBasisContext *context) {
  const auto *data = static_cast<const TangentBuildMesh *>(context->m_pUserData);
  return static_cast<int>(data->mesh->indices.size() / 3u);
}

int basisVerticesForFace(const SSurfaceBasisContext *, const int) {
  return 3;
}

Vertex &basisVertex(const SSurfaceBasisContext *context, const int face, const int vertex) {
  const auto *data = static_cast<const TangentBuildMesh *>(context->m_pUserData);
  const std::uint32_t index =
      data->mesh->indices[static_cast<std::size_t>(face) * 3u + static_cast<std::size_t>(vertex)];
  return data->mesh->vertices[index];
}

void basisPosition(const SSurfaceBasisContext *context, float out[], const int face,
                   const int vertex) {
  const Vec3 position = basisVertex(context, face, vertex).position;
  out[0] = position.x;
  out[1] = position.y;
  out[2] = position.z;
}

void basisNormal(const SSurfaceBasisContext *context, float out[], const int face,
                 const int vertex) {
  const Vec3 normal = basisVertex(context, face, vertex).normal;
  out[0] = normal.x;
  out[1] = normal.y;
  out[2] = normal.z;
}

void basisTexCoord(const SSurfaceBasisContext *context, float out[], const int face,
                   const int vertex) {
  const Vec2 uv = basisVertex(context, face, vertex).uv;
  out[0] = uv.x;
  out[1] = uv.y;
}

void basisSetTangent(const SSurfaceBasisContext *context, const float tangent[], const float sign,
                     const int face, const int vertex) {
  Vertex &out = basisVertex(context, face, vertex);
  out.tangent = {tangent[0], tangent[1], tangent[2], sign};
}

void generateTangents(CpuMesh &mesh, MeshDiagnostics &diagnostics) {
  CpuMesh expanded = expandIndexedTriangles(mesh);
  TangentBuildMesh data{&expanded};
  SSurfaceBasisInterface callbacks{};
  callbacks.m_getNumFaces = basisFaceCount;
  callbacks.m_getNumVerticesOfFace = basisVerticesForFace;
  callbacks.m_getPosition = basisPosition;
  callbacks.m_getNormal = basisNormal;
  callbacks.m_getTexCoord = basisTexCoord;
  callbacks.m_setTSpaceBasic = basisSetTangent;

  SSurfaceBasisContext context{};
  context.m_pInterface = &callbacks;
  context.m_pUserData = &data;

  if (!genTangSpaceDefault(&context)) {
    throw std::runtime_error("SurfaceBasis tangent generation failed.");
  }

  diagnostics.generated_tangents = expanded.vertices.size();
  mesh = std::move(expanded);
}

void compactEquivalentVertices(CpuMesh &mesh, MeshDiagnostics &diagnostics) {
  std::vector<unsigned int> remap(mesh.vertices.size());
  const std::size_t vertex_count =
      meshwork_generateVertexRemap(remap.data(), mesh.indices.data(), mesh.indices.size(),
                                   mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));

  std::vector<Vertex> vertices(vertex_count);
  std::vector<std::uint32_t> indices(mesh.indices.size());
  meshwork_remapVertexBuffer(vertices.data(), mesh.vertices.data(), mesh.vertices.size(),
                             sizeof(Vertex), remap.data());
  meshwork_remapIndexBuffer(indices.data(), mesh.indices.data(), mesh.indices.size(), remap.data());

  diagnostics.remapped_vertices = mesh.vertices.size() - vertex_count;
  mesh.vertices = std::move(vertices);
  mesh.indices = std::move(indices);
}

void optimizeIndicesAndVertices(CpuMesh &mesh, const MeshProcessOptions &options) {
  if (options.optimize_vertex_cache) {
    std::vector<std::uint32_t> optimized(mesh.indices.size());
    meshwork_optimizeVertexCache(optimized.data(), mesh.indices.data(), mesh.indices.size(),
                                 mesh.vertices.size());
    mesh.indices = std::move(optimized);
  }

  if (options.optimize_vertex_fetch) {
    std::vector<Vertex> vertices(mesh.vertices.size());
    const std::size_t vertex_count =
        meshwork_optimizeVertexFetch(vertices.data(), mesh.indices.data(), mesh.indices.size(),
                                     mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));
    vertices.resize(vertex_count);
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

} // namespace aster
