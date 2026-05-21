// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/geometry/mesh_authoring.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
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

[[nodiscard]] CpuMesh transformedMesh(CpuMesh mesh, const Transform &transform) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.position = transformPoint(transform, vertex.position);
    vertex.normal = normalizeOr(transformVector(transform, vertex.normal), {0.0f, 1.0f, 0.0f});
    const Vec3 tangent =
        transformVector(transform, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
    vertex.tangent.x = tangent.x;
    vertex.tangent.y = tangent.y;
    vertex.tangent.z = tangent.z;
  }
  return mesh;
}

void appendMesh(CpuMesh &dst, const CpuMesh &src) {
  const std::uint32_t offset = static_cast<std::uint32_t>(dst.vertices.size());
  dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
  dst.indices.reserve(dst.indices.size() + src.indices.size());
  for (const std::uint32_t index : src.indices) {
    dst.indices.push_back(index + offset);
  }
}

struct MeshBounds {
  Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
           std::numeric_limits<float>::max()};
  Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
           std::numeric_limits<float>::lowest()};
};

[[nodiscard]] MeshBounds meshBounds(const CpuMesh &mesh) {
  MeshBounds bounds;
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

[[nodiscard]] bool boundsOverlap(const MeshBounds &lhs, const MeshBounds &rhs) {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y &&
         lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

void appendUniqueStrings(std::vector<std::string> &dst, const std::vector<std::string> &src) {
  dst.insert(dst.end(), src.begin(), src.end());
  std::sort(dst.begin(), dst.end());
  dst.erase(std::unique(dst.begin(), dst.end()), dst.end());
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

EditableMesh packEditableMeshUvIslands(const EditableMesh &mesh, const MeshUvPackingPolicy policy,
                                       MeshAuthoringReport *report) {
  MeshAuthoringReport local;
  initializeReport(local, mesh, "uv-pack");
  EditableMesh out = mesh;
  if (out.vertices.empty()) {
    finishReport(local, out);
    if (report != nullptr) {
      *report = local;
    }
    return out;
  }

  Vec2 min_uv{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  Vec2 max_uv{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
  for (const EditableMeshVertex &vertex : out.vertices) {
    min_uv.x = std::min(min_uv.x, vertex.uv.x);
    min_uv.y = std::min(min_uv.y, vertex.uv.y);
    max_uv.x = std::max(max_uv.x, vertex.uv.x);
    max_uv.y = std::max(max_uv.y, vertex.uv.y);
  }
  const Vec2 span{std::max(0.00001f, max_uv.x - min_uv.x),
                  std::max(0.00001f, max_uv.y - min_uv.y)};
  const float padding = std::clamp(policy.padding, 0.0f, 0.45f);
  const float scale_x = 1.0f - padding * 2.0f;
  const float scale_y = 1.0f - padding * 2.0f;
  const float uniform = std::min(scale_x / span.x, scale_y / span.y);
  for (EditableMeshVertex &vertex : out.vertices) {
    Vec2 uv{(vertex.uv.x - min_uv.x) / span.x, (vertex.uv.y - min_uv.y) / span.y};
    if (policy.normalize_to_unit_square && policy.preserve_aspect) {
      uv = {(vertex.uv.x - min_uv.x) * uniform + padding,
            (vertex.uv.y - min_uv.y) * uniform + padding};
    } else if (policy.normalize_to_unit_square) {
      uv = {uv.x * scale_x + padding, uv.y * scale_y + padding};
    }
    vertex.uv = uv;
  }
  finishReport(local, out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

CpuMesh makeMeshPrimitiveRecipe(const MeshPrimitiveRecipe &recipe) {
  switch (recipe.kind) {
  case MeshPrimitiveRecipeKind::Box:
    return makeBox();
  case MeshPrimitiveRecipeKind::Plane:
    return makePlane(recipe.size);
  case MeshPrimitiveRecipeKind::Sphere:
    return makeUvSphere(static_cast<int>(std::max<std::uint32_t>(3u, recipe.segments)),
                        static_cast<int>(std::max<std::uint32_t>(2u, recipe.rings)),
                        recipe.radius);
  case MeshPrimitiveRecipeKind::Rock:
    return makeRock(static_cast<int>(std::max<std::uint32_t>(3u, recipe.segments)),
                    static_cast<int>(std::max<std::uint32_t>(2u, recipe.rings)),
                    recipe.radius);
  case MeshPrimitiveRecipeKind::Pillar:
    return makePillar(static_cast<int>(std::max<std::uint32_t>(3u, recipe.segments)),
                      recipe.radius, recipe.height);
  }
  return makeBox();
}

MeshBooleanResult evaluateMeshBooleanRequest(const MeshBooleanRequest &request) {
  MeshBooleanResult result;
  result.variant_intent_tags = request.variant_intent_tags;
  result.report.operation = std::string("boolean-") + meshBooleanOperationName(request.operation);
  result.report.input_vertices = 0u;
  result.report.input_faces = 0u;
  for (const MeshBooleanInput &input : request.inputs) {
    result.report.input_vertices += input.mesh.vertices.size();
    result.report.input_faces += input.mesh.indices.size() / 3u;
  }
  if (request.inputs.empty()) {
    result.report.ok = false;
    result.report.quality_score = 0u;
    result.report.issues.push_back({"error", result.report.operation,
                                    "boolean request has no inputs", 0u});
    result.diagnostics.push_back("error: boolean request has no inputs");
    return result;
  }
  if (request.solver == MeshBooleanSolver::ExactRequested && !request.allow_approximate) {
    result.report.ok = false;
    result.report.quality_score = 0u;
    result.report.issues.push_back({"error", result.report.operation,
                                    "exact boolean solver is not enabled in this build", 0u});
    result.diagnostics.push_back("error: exact boolean solver is not enabled");
    return result;
  }
  if (request.solver == MeshBooleanSolver::ExactRequested) {
    result.diagnostics.push_back("warning: exact boolean request used Aster reference fallback");
  }

  std::vector<CpuMesh> transformed;
  transformed.reserve(request.inputs.size());
  for (const MeshBooleanInput &input : request.inputs) {
    transformed.push_back(transformedMesh(input.mesh, input.transform));
  }

  switch (request.operation) {
  case MeshBooleanOperation::Union:
    for (const CpuMesh &mesh : transformed) {
      appendMesh(result.mesh, mesh);
    }
    break;
  case MeshBooleanOperation::Difference:
    result.mesh = transformed.front();
    if (transformed.size() > 1u) {
      result.diagnostics.push_back("warning: difference kept base mesh as conservative proxy");
    }
    break;
  case MeshBooleanOperation::Intersect: {
    bool overlaps = true;
    const MeshBounds first = meshBounds(transformed.front());
    for (std::size_t i = 1u; i < transformed.size(); ++i) {
      overlaps = overlaps && boundsOverlap(first, meshBounds(transformed[i]));
    }
    if (overlaps) {
      result.mesh = transformed.front();
      result.diagnostics.push_back("warning: intersection used overlapping bounds proxy");
    } else {
      result.diagnostics.push_back("warning: intersection inputs do not overlap");
    }
    break;
  }
  }

  result.report.output_vertices = result.mesh.vertices.size();
  result.report.output_faces = result.mesh.indices.size() / 3u;
  result.report.generated_triangles = result.mesh.indices.size() / 3u;
  result.report.ok = std::none_of(result.report.issues.begin(), result.report.issues.end(),
                                  [](const MeshAuthoringIssue &issue) {
                                    return issue.severity == "error";
                                  });
  result.report.quality_score = result.report.ok ? (result.diagnostics.empty() ? 100u : 82u) : 0u;
  return result;
}

MeshAuthoringRecipeResult applyMeshAuthoringRecipe(const MeshAuthoringRecipe &recipe) {
  MeshAuthoringRecipeResult result;
  result.variant_intent_tags = recipe.variant_intent_tags;
  CpuMesh current = recipe.source_mesh.value_or(CpuMesh{});
  EditableMesh editable = editableMeshFromCpuMesh(current, recipe.provenance_id);
  if (current.vertices.empty() && recipe.steps.empty()) {
    result.diagnostics.push_back("error: mesh authoring recipe has no source mesh or steps");
    result.quality_score = 0u;
    return result;
  }

  for (const MeshAuthoringRecipeStep &step : recipe.steps) {
    appendUniqueStrings(result.variant_intent_tags, step.variant_intent_tags);
    MeshAuthoringReport report;
    switch (step.kind) {
    case MeshAuthoringRecipeStepKind::Primitive:
      current = makeMeshPrimitiveRecipe(step.primitive);
      editable = editableMeshFromCpuMesh(current, recipe.provenance_id);
      report = validateEditableMesh(editable);
      report.operation = "primitive";
      break;
    case MeshAuthoringRecipeStepKind::Validate:
      report = validateEditableMesh(editable);
      break;
    case MeshAuthoringRecipeStepKind::Triangulate:
      editable = triangulateEditableMesh(editable, &report);
      break;
    case MeshAuthoringRecipeStepKind::Weld:
      editable = weldEditableVertices(editable, step.epsilon, &report);
      break;
    case MeshAuthoringRecipeStepKind::RecalculateNormals:
      editable = recalculateEditableNormals(editable, &report);
      break;
    case MeshAuthoringRecipeStepKind::Mirror:
      editable = mirrorEditableMesh(editable, step.mirror_axis, true, &report);
      break;
    case MeshAuthoringRecipeStepKind::Inset:
      editable = insetEditableFaces(editable, step.amount, &report);
      break;
    case MeshAuthoringRecipeStepKind::Extrude:
      editable = extrudeEditableFaces(editable, step.amount, &report);
      break;
    case MeshAuthoringRecipeStepKind::UvPack:
      editable = packEditableMeshUvIslands(editable, step.uv_policy, &report);
      break;
    case MeshAuthoringRecipeStepKind::Boolean: {
      MeshBooleanRequest request = step.boolean_request;
      if (request.inputs.empty() && !current.vertices.empty()) {
        request.inputs.push_back({.label = "current", .mesh = current});
      }
      const MeshBooleanResult boolean_result = evaluateMeshBooleanRequest(request);
      current = boolean_result.mesh;
      editable = editableMeshFromCpuMesh(current, recipe.provenance_id);
      report = boolean_result.report;
      result.diagnostics.insert(result.diagnostics.end(), boolean_result.diagnostics.begin(),
                                boolean_result.diagnostics.end());
      appendUniqueStrings(result.variant_intent_tags, boolean_result.variant_intent_tags);
      break;
    }
    }
    result.reports.push_back(report);
    result.quality_score = std::min(result.quality_score, report.quality_score);
    for (const MeshAuthoringIssue &issue : report.issues) {
      result.diagnostics.push_back(issue.severity + ": " + issue.operation + ": " +
                                   issue.message);
    }
    if (step.kind != MeshAuthoringRecipeStepKind::Boolean &&
        step.kind != MeshAuthoringRecipeStepKind::Primitive) {
      current = cpuMeshFromEditableMesh(editable);
    }
  }
  result.mesh = current;
  result.editable_mesh = editable;
  if (result.reports.empty()) {
    result.reports.push_back(validateEditableMesh(editable));
  }
  return result;
}

std::string summarizeMeshAuthoringRecipe(const MeshAuthoringRecipeResult &result) {
  std::ostringstream out;
  out << "mesh-recipe vertices=" << result.mesh.vertices.size()
      << " indices=" << result.mesh.indices.size() << " reports=" << result.reports.size()
      << " quality=" << result.quality_score;
  if (!result.variant_intent_tags.empty()) {
    out << " variants=";
    for (std::size_t i = 0u; i < result.variant_intent_tags.size(); ++i) {
      if (i > 0u) {
        out << ",";
      }
      out << result.variant_intent_tags[i];
    }
  }
  return out.str();
}

const char *meshBooleanOperationName(const MeshBooleanOperation operation) {
  switch (operation) {
  case MeshBooleanOperation::Union:
    return "union";
  case MeshBooleanOperation::Intersect:
    return "intersect";
  case MeshBooleanOperation::Difference:
    return "difference";
  }
  return "unknown";
}

const char *meshBooleanSolverName(const MeshBooleanSolver solver) {
  switch (solver) {
  case MeshBooleanSolver::AsterReference:
    return "aster-reference";
  case MeshBooleanSolver::FastApproximate:
    return "fast-approximate";
  case MeshBooleanSolver::ExactRequested:
    return "exact-requested";
  }
  return "unknown";
}

const char *meshPrimitiveRecipeKindName(const MeshPrimitiveRecipeKind kind) {
  switch (kind) {
  case MeshPrimitiveRecipeKind::Box:
    return "box";
  case MeshPrimitiveRecipeKind::Plane:
    return "plane";
  case MeshPrimitiveRecipeKind::Sphere:
    return "sphere";
  case MeshPrimitiveRecipeKind::Rock:
    return "rock";
  case MeshPrimitiveRecipeKind::Pillar:
    return "pillar";
  }
  return "unknown";
}

const char *meshAuthoringRecipeStepKindName(const MeshAuthoringRecipeStepKind kind) {
  switch (kind) {
  case MeshAuthoringRecipeStepKind::Validate:
    return "validate";
  case MeshAuthoringRecipeStepKind::Triangulate:
    return "triangulate";
  case MeshAuthoringRecipeStepKind::Weld:
    return "weld";
  case MeshAuthoringRecipeStepKind::RecalculateNormals:
    return "recalculate-normals";
  case MeshAuthoringRecipeStepKind::Mirror:
    return "mirror";
  case MeshAuthoringRecipeStepKind::Inset:
    return "inset";
  case MeshAuthoringRecipeStepKind::Extrude:
    return "extrude";
  case MeshAuthoringRecipeStepKind::UvPack:
    return "uv-pack";
  case MeshAuthoringRecipeStepKind::Boolean:
    return "boolean";
  case MeshAuthoringRecipeStepKind::Primitive:
    return "primitive";
  }
  return "unknown";
}

} // namespace aster
