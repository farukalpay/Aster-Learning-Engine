// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/render/mesh.hpp"
#include "aster/math/transform.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aster {

enum class MeshMirrorAxis {
  X,
  Y,
  Z,
};

struct MeshAuthoringIssue {
  std::string severity;
  std::string operation;
  std::string message;
  std::size_t element_index = 0u;
};

struct MeshAuthoringReport {
  std::string operation;
  std::size_t input_vertices = 0u;
  std::size_t input_faces = 0u;
  std::size_t output_vertices = 0u;
  std::size_t output_faces = 0u;
  std::size_t generated_triangles = 0u;
  std::size_t welded_vertices = 0u;
  std::size_t removed_faces = 0u;
  std::uint32_t quality_score = 100u;
  bool ok = true;
  std::vector<MeshAuthoringIssue> issues;
};

struct EditableMeshVertex {
  Vec3 position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  Vec2 uv{};
  Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  float ambient_occlusion = 1.0f;
  std::uint32_t source_index = 0u;
};

struct EditableMeshFace {
  std::vector<std::uint32_t> vertices;
  std::uint32_t material_slot = 0u;
  std::string tag;
};

struct EditableMesh {
  std::string provenance_id;
  std::vector<EditableMeshVertex> vertices;
  std::vector<EditableMeshFace> faces;
};

enum class MeshBooleanOperation {
  Union,
  Intersect,
  Difference,
};

enum class MeshBooleanSolver {
  AsterReference,
  FastApproximate,
  ExactRequested,
};

struct MeshBooleanInput {
  std::string label;
  CpuMesh mesh;
  Transform transform{};
};

struct MeshBooleanRequest {
  std::string id;
  MeshBooleanOperation operation = MeshBooleanOperation::Union;
  MeshBooleanSolver solver = MeshBooleanSolver::AsterReference;
  bool require_watertight = false;
  bool allow_approximate = true;
  std::vector<MeshBooleanInput> inputs;
  std::vector<std::string> variant_intent_tags;
};

struct MeshBooleanResult {
  CpuMesh mesh;
  MeshAuthoringReport report;
  std::vector<std::string> diagnostics;
  std::vector<std::string> variant_intent_tags;
};

struct MeshUvPackingPolicy {
  float padding = 0.02f;
  bool normalize_to_unit_square = true;
  bool preserve_aspect = true;
  std::string channel = "uv0";
};

enum class MeshPrimitiveRecipeKind {
  Box,
  Plane,
  Sphere,
  Rock,
  Pillar,
};

struct MeshPrimitiveRecipe {
  MeshPrimitiveRecipeKind kind = MeshPrimitiveRecipeKind::Box;
  float radius = 0.5f;
  float height = 1.0f;
  float size = 1.0f;
  std::uint32_t segments = 16u;
  std::uint32_t rings = 8u;
};

enum class MeshAuthoringRecipeStepKind {
  Validate,
  Triangulate,
  Weld,
  RecalculateNormals,
  Mirror,
  Inset,
  Extrude,
  UvPack,
  Boolean,
  Primitive,
};

struct MeshAuthoringRecipeStep {
  std::string id;
  MeshAuthoringRecipeStepKind kind = MeshAuthoringRecipeStepKind::Validate;
  float amount = 0.0f;
  float epsilon = 0.0001f;
  MeshMirrorAxis mirror_axis = MeshMirrorAxis::X;
  MeshUvPackingPolicy uv_policy{};
  MeshBooleanRequest boolean_request{};
  MeshPrimitiveRecipe primitive{};
  std::vector<std::string> variant_intent_tags;
};

struct MeshAuthoringRecipe {
  std::string id;
  std::string provenance_id;
  std::optional<CpuMesh> source_mesh;
  std::vector<MeshAuthoringRecipeStep> steps;
  std::vector<std::string> variant_intent_tags;
};

struct MeshAuthoringRecipeResult {
  CpuMesh mesh;
  EditableMesh editable_mesh;
  std::vector<MeshAuthoringReport> reports;
  std::vector<std::string> diagnostics;
  std::vector<std::string> variant_intent_tags;
  std::uint32_t quality_score = 100u;
};

[[nodiscard]] EditableMesh editableMeshFromCpuMesh(const CpuMesh &mesh,
                                                   std::string provenance_id = {});
[[nodiscard]] CpuMesh cpuMeshFromEditableMesh(const EditableMesh &mesh,
                                              MeshAuthoringReport *report = nullptr);
[[nodiscard]] MeshAuthoringReport validateEditableMesh(const EditableMesh &mesh);

[[nodiscard]] EditableMesh triangulateEditableMesh(const EditableMesh &mesh,
                                                   MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh recalculateEditableNormals(const EditableMesh &mesh,
                                                      MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh weldEditableVertices(const EditableMesh &mesh, float epsilon,
                                                MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh mirrorEditableMesh(const EditableMesh &mesh, MeshMirrorAxis axis,
                                              bool flip_winding = true,
                                              MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh insetEditableFaces(const EditableMesh &mesh, float amount,
                                              MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh extrudeEditableFaces(const EditableMesh &mesh, float distance,
                                                MeshAuthoringReport *report = nullptr);
[[nodiscard]] EditableMesh packEditableMeshUvIslands(const EditableMesh &mesh,
                                                     MeshUvPackingPolicy policy = {},
                                                     MeshAuthoringReport *report = nullptr);
[[nodiscard]] CpuMesh makeMeshPrimitiveRecipe(const MeshPrimitiveRecipe &recipe);
[[nodiscard]] MeshBooleanResult evaluateMeshBooleanRequest(const MeshBooleanRequest &request);
[[nodiscard]] MeshAuthoringRecipeResult applyMeshAuthoringRecipe(const MeshAuthoringRecipe &recipe);
[[nodiscard]] std::string summarizeMeshAuthoringRecipe(const MeshAuthoringRecipeResult &result);
[[nodiscard]] const char *meshBooleanOperationName(MeshBooleanOperation operation);
[[nodiscard]] const char *meshBooleanSolverName(MeshBooleanSolver solver);
[[nodiscard]] const char *meshPrimitiveRecipeKindName(MeshPrimitiveRecipeKind kind);
[[nodiscard]] const char *meshAuthoringRecipeStepKindName(MeshAuthoringRecipeStepKind kind);

} // namespace aster
