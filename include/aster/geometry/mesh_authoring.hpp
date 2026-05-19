// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/mesh.hpp"

#include <cstddef>
#include <cstdint>
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

} // namespace aster
