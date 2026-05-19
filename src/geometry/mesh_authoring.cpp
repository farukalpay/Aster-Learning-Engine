// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/mesh_authoring.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace aster {
namespace {

struct QuantizedPosition {
  long long x = 0;
  long long y = 0;
  long long z = 0;

  bool operator==(const QuantizedPosition &other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct QuantizedPositionHash {
  std::size_t operator()(const QuantizedPosition key) const {
    std::size_t hash = 1469598103934665603ull;
    const auto append = [&](const long long value) {
      const auto bits = static_cast<unsigned long long>(value);
      hash ^= static_cast<std::size_t>(bits + 0x9e3779b97f4a7c15ull + (hash << 6u) +
                                       (hash >> 2u));
      hash *= 1099511628211ull;
    };
    append(key.x);
    append(key.y);
    append(key.z);
    return hash;
  }
};

[[nodiscard]] QuantizedPosition quantize(const Vec3 value, const float epsilon) {
  const float scale_value = epsilon > 0.0f ? 1.0f / epsilon : 1000000.0f;
  return {std::llround(value.x * scale_value),
          std::llround(value.y * scale_value),
          std::llround(value.z * scale_value)};
}

[[nodiscard]] Vec3 faceNormal(const EditableMesh &mesh, const EditableMeshFace &face) {
  if (face.vertices.size() < 3u) {
    return {0.0f, 1.0f, 0.0f};
  }
  const Vec3 a = mesh.vertices[face.vertices[0u]].position;
  for (std::size_t i = 1u; i + 1u < face.vertices.size(); ++i) {
    const Vec3 b = mesh.vertices[face.vertices[i]].position;
    const Vec3 c = mesh.vertices[face.vertices[i + 1u]].position;
    const Vec3 normal = cross(b - a, c - a);
    if (lengthSquared(normal) > 0.00000001f) {
      return normalize(normal);
    }
  }
  return {0.0f, 1.0f, 0.0f};
}

void initializeReport(MeshAuthoringReport &report, const EditableMesh &mesh,
                      const char *operation) {
  report = {};
  report.operation = operation;
  report.input_vertices = mesh.vertices.size();
  report.input_faces = mesh.faces.size();
}

void finishReport(MeshAuthoringReport &report, const EditableMesh &mesh) {
  report.output_vertices = mesh.vertices.size();
  report.output_faces = mesh.faces.size();
  report.ok = std::none_of(report.issues.begin(), report.issues.end(),
                           [](const MeshAuthoringIssue &issue) {
                             return issue.severity == "error";
                           });
  const std::uint32_t penalty = static_cast<std::uint32_t>(
      std::min<std::size_t>(100u, report.issues.size() * 8u + report.removed_faces * 3u));
  report.quality_score = report.ok ? 100u - penalty : 0u;
}

[[nodiscard]] float component(const Vec3 value, const MeshMirrorAxis axis) {
  switch (axis) {
  case MeshMirrorAxis::X:
    return value.x;
  case MeshMirrorAxis::Y:
    return value.y;
  case MeshMirrorAxis::Z:
  default:
    return value.z;
  }
}

void setComponent(Vec3 &value, const MeshMirrorAxis axis, const float component_value) {
  switch (axis) {
  case MeshMirrorAxis::X:
    value.x = component_value;
    break;
  case MeshMirrorAxis::Y:
    value.y = component_value;
    break;
  case MeshMirrorAxis::Z:
    value.z = component_value;
    break;
  }
}

[[nodiscard]] float component(const Vec4 value, const MeshMirrorAxis axis) {
  switch (axis) {
  case MeshMirrorAxis::X:
    return value.x;
  case MeshMirrorAxis::Y:
    return value.y;
  case MeshMirrorAxis::Z:
  default:
    return value.z;
  }
}

void setComponent(Vec4 &value, const MeshMirrorAxis axis, const float component_value) {
  switch (axis) {
  case MeshMirrorAxis::X:
    value.x = component_value;
    break;
  case MeshMirrorAxis::Y:
    value.y = component_value;
    break;
  case MeshMirrorAxis::Z:
    value.z = component_value;
    break;
  }
}

} // namespace

EditableMesh editableMeshFromCpuMesh(const CpuMesh &mesh, std::string provenance_id) {
  EditableMesh out;
  out.provenance_id = std::move(provenance_id);
  out.vertices.reserve(mesh.vertices.size());
  for (std::size_t i = 0u; i < mesh.vertices.size(); ++i) {
    const Vertex &vertex = mesh.vertices[i];
    out.vertices.push_back({.position = vertex.position,
                            .normal = vertex.normal,
                            .uv = vertex.uv,
                            .tangent = vertex.tangent,
                            .ambient_occlusion = vertex.ambient_occlusion,
                            .source_index = static_cast<std::uint32_t>(i)});
  }
  out.faces.reserve(mesh.indices.size() / 3u);
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    out.faces.push_back({.vertices = {mesh.indices[i], mesh.indices[i + 1u], mesh.indices[i + 2u]}});
  }
  return out;
}

CpuMesh cpuMeshFromEditableMesh(const EditableMesh &mesh, MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "cpu-mesh-export");
  CpuMesh out;
  out.vertices.reserve(mesh.vertices.size());
  for (const EditableMeshVertex &vertex : mesh.vertices) {
    out.vertices.push_back({.position = vertex.position,
                            .normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f}),
                            .uv = vertex.uv,
                            .tangent = vertex.tangent,
                            .ambient_occlusion = vertex.ambient_occlusion});
  }
  for (std::size_t face_index = 0u; face_index < mesh.faces.size(); ++face_index) {
    const EditableMeshFace &face = mesh.faces[face_index];
    if (face.vertices.size() < 3u) {
      ++local.removed_faces;
      local.issues.push_back({"warning", local.operation, "face has fewer than three vertices",
                              face_index});
      continue;
    }
    for (const std::uint32_t index : face.vertices) {
      if (index >= mesh.vertices.size()) {
        local.issues.push_back(
            {"error", local.operation, "face references a missing vertex", face_index});
        continue;
      }
    }
    for (std::size_t i = 1u; i + 1u < face.vertices.size(); ++i) {
      out.indices.insert(out.indices.end(), {face.vertices[0u], face.vertices[i],
                                             face.vertices[i + 1u]});
      ++local.generated_triangles;
    }
  }
  finishReport(local, mesh);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

MeshAuthoringReport validateEditableMesh(const EditableMesh &mesh) {
  MeshAuthoringReport report;
  initializeReport(report, mesh, "validate");
  for (std::size_t i = 0u; i < mesh.vertices.size(); ++i) {
    const EditableMeshVertex &vertex = mesh.vertices[i];
    if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
        !std::isfinite(vertex.position.z)) {
      report.issues.push_back({"error", report.operation, "vertex position is not finite", i});
    }
  }
  for (std::size_t face_index = 0u; face_index < mesh.faces.size(); ++face_index) {
    const EditableMeshFace &face = mesh.faces[face_index];
    if (face.vertices.size() < 3u) {
      report.issues.push_back({"error", report.operation, "face has fewer than three vertices",
                               face_index});
      continue;
    }
    for (const std::uint32_t index : face.vertices) {
      if (index >= mesh.vertices.size()) {
        report.issues.push_back(
            {"error", report.operation, "face references a missing vertex", face_index});
      }
    }
    if (lengthSquared(faceNormal(mesh, face)) <= 0.00000001f) {
      report.issues.push_back({"warning", report.operation, "face normal is degenerate",
                               face_index});
    }
  }
  finishReport(report, mesh);
  return report;
}

EditableMesh triangulateEditableMesh(const EditableMesh &mesh, MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "triangulate");
  EditableMesh out = mesh;
  out.faces.clear();
  for (std::size_t face_index = 0u; face_index < mesh.faces.size(); ++face_index) {
    const EditableMeshFace &face = mesh.faces[face_index];
    if (face.vertices.size() < 3u) {
      ++local.removed_faces;
      local.issues.push_back({"warning", local.operation, "discarded non-polygon face",
                              face_index});
      continue;
    }
    for (std::size_t i = 1u; i + 1u < face.vertices.size(); ++i) {
      out.faces.push_back({.vertices = {face.vertices[0u], face.vertices[i],
                                        face.vertices[i + 1u]},
                           .material_slot = face.material_slot,
                           .tag = face.tag});
      ++local.generated_triangles;
    }
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

EditableMesh recalculateEditableNormals(const EditableMesh &mesh, MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "recalculate-normals");
  EditableMesh out = mesh;
  for (EditableMeshVertex &vertex : out.vertices) {
    vertex.normal = {};
  }
  for (const EditableMeshFace &face : out.faces) {
    const Vec3 normal = faceNormal(out, face);
    for (const std::uint32_t index : face.vertices) {
      if (index < out.vertices.size()) {
        out.vertices[index].normal += normal;
      }
    }
  }
  for (EditableMeshVertex &vertex : out.vertices) {
    vertex.normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f});
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

EditableMesh weldEditableVertices(const EditableMesh &mesh, const float epsilon,
                                  MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "weld");
  EditableMesh out;
  out.provenance_id = mesh.provenance_id;
  std::unordered_map<QuantizedPosition, std::uint32_t, QuantizedPositionHash> remap_by_position;
  std::vector<std::uint32_t> remap(mesh.vertices.size(), 0u);
  for (std::size_t i = 0u; i < mesh.vertices.size(); ++i) {
    const QuantizedPosition key = quantize(mesh.vertices[i].position, epsilon);
    const auto found = remap_by_position.find(key);
    if (found != remap_by_position.end()) {
      remap[i] = found->second;
      ++local.welded_vertices;
      continue;
    }
    const std::uint32_t new_index = static_cast<std::uint32_t>(out.vertices.size());
    remap_by_position.emplace(key, new_index);
    remap[i] = new_index;
    out.vertices.push_back(mesh.vertices[i]);
  }
  out.faces.reserve(mesh.faces.size());
  for (std::size_t face_index = 0u; face_index < mesh.faces.size(); ++face_index) {
    EditableMeshFace face = mesh.faces[face_index];
    for (std::uint32_t &index : face.vertices) {
      index = index < remap.size() ? remap[index] : index;
    }
    std::vector<std::uint32_t> unique = face.vertices;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    if (unique.size() < 3u) {
      ++local.removed_faces;
      local.issues.push_back({"warning", local.operation, "weld collapsed a face", face_index});
      continue;
    }
    out.faces.push_back(std::move(face));
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

EditableMesh mirrorEditableMesh(const EditableMesh &mesh, const MeshMirrorAxis axis,
                                const bool flip_winding, MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "mirror");
  EditableMesh out = mesh;
  for (EditableMeshVertex &vertex : out.vertices) {
    setComponent(vertex.position, axis, -component(vertex.position, axis));
    setComponent(vertex.normal, axis, -component(vertex.normal, axis));
    setComponent(vertex.tangent, axis, -component(vertex.tangent, axis));
  }
  if (flip_winding) {
    for (EditableMeshFace &face : out.faces) {
      std::reverse(face.vertices.begin(), face.vertices.end());
    }
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

EditableMesh insetEditableFaces(const EditableMesh &mesh, const float amount,
                                MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "inset");
  EditableMesh out = mesh;
  for (const EditableMeshFace &face : mesh.faces) {
    if (face.vertices.empty()) {
      continue;
    }
    Vec3 centroid{};
    for (const std::uint32_t index : face.vertices) {
      centroid += mesh.vertices[index].position;
    }
    centroid *= 1.0f / static_cast<float>(face.vertices.size());
    for (const std::uint32_t index : face.vertices) {
      if (index < out.vertices.size()) {
        const Vec3 offset = centroid - out.vertices[index].position;
        out.vertices[index].position += offset * std::clamp(amount, 0.0f, 0.95f);
      }
    }
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

EditableMesh extrudeEditableFaces(const EditableMesh &mesh, const float distance,
                                  MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "extrude");
  EditableMesh out = mesh;
  const std::size_t original_face_count = mesh.faces.size();
  for (std::size_t face_index = 0u; face_index < original_face_count; ++face_index) {
    const EditableMeshFace &face = mesh.faces[face_index];
    if (face.vertices.size() < 3u) {
      continue;
    }
    const Vec3 normal = faceNormal(mesh, face);
    EditableMeshFace cap = face;
    for (std::uint32_t &index : cap.vertices) {
      EditableMeshVertex vertex = mesh.vertices[index];
      vertex.position += normal * distance;
      index = static_cast<std::uint32_t>(out.vertices.size());
      out.vertices.push_back(vertex);
    }
    out.faces.push_back(cap);
    for (std::size_t i = 0u; i < face.vertices.size(); ++i) {
      const std::size_t next = (i + 1u) % face.vertices.size();
      out.faces.push_back({.vertices = {face.vertices[i], face.vertices[next],
                                        cap.vertices[next], cap.vertices[i]},
                           .material_slot = face.material_slot,
                           .tag = "extrude.side"});
    }
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

} // namespace aster
