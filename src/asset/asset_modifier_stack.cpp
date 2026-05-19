// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_modifier_stack.hpp"

#include "aster/geometry/geometry_operations.hpp"

#include <algorithm>
#include <bit>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aster {
namespace {

void appendHash(std::uint64_t &hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
}

void appendHash(std::uint64_t &hash, const float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  for (std::uint32_t shift = 0u; shift < 32u; shift += 8u) {
    hash ^= static_cast<std::uint8_t>((bits >> shift) & 0xffu);
    hash *= 1099511628211ull;
  }
}

[[nodiscard]] std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << "aster-stack-0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

void absorbReport(AssetModifierStackReport &report, MeshAuthoringReport op_report) {
  report.quality_score = std::min(report.quality_score, op_report.quality_score);
  for (const MeshAuthoringIssue &issue : op_report.issues) {
    report.diagnostics.push_back(issue.severity + ":" + issue.operation + ":" + issue.message);
    if (issue.severity == "warning") {
      report.degradation_reasons.push_back(issue.message);
    }
  }
  report.operator_reports.push_back(std::move(op_report));
}

[[nodiscard]] EditableMesh runEditableModifier(const EditableMesh &mesh,
                                               const AssetModifierDesc &modifier,
                                               MeshAuthoringReport &report) {
  switch (modifier.kind) {
  case AssetModifierKind::Triangulate:
    return triangulateEditableMesh(mesh, &report);
  case AssetModifierKind::Weld:
    return weldEditableVertices(mesh, modifier.epsilon, &report);
  case AssetModifierKind::RecalculateNormals:
    return recalculateEditableNormals(mesh, &report);
  case AssetModifierKind::Mirror:
    return mirrorEditableMesh(mesh, modifier.mirror_axis, true, &report);
  case AssetModifierKind::Inset:
    return insetEditableFaces(mesh, modifier.amount, &report);
  case AssetModifierKind::Extrude:
    return extrudeEditableFaces(mesh, modifier.amount, &report);
  case AssetModifierKind::Bevel:
    return insetEditableFaces(mesh, std::max(0.0f, modifier.amount) * 0.5f, &report);
  case AssetModifierKind::Transform:
  default:
    report = {};
    report.operation = std::string(assetModifierKindName(modifier.kind));
    report.input_vertices = mesh.vertices.size();
    report.input_faces = mesh.faces.size();
    report.output_vertices = mesh.vertices.size();
    report.output_faces = mesh.faces.size();
    return mesh;
  }
}

} // namespace

std::string_view assetModifierKindName(const AssetModifierKind kind) {
  switch (kind) {
  case AssetModifierKind::Transform:
    return "transform";
  case AssetModifierKind::Triangulate:
    return "triangulate";
  case AssetModifierKind::Weld:
    return "weld";
  case AssetModifierKind::RecalculateNormals:
    return "recalculate-normals";
  case AssetModifierKind::Mirror:
    return "mirror";
  case AssetModifierKind::Inset:
    return "inset";
  case AssetModifierKind::Extrude:
    return "extrude";
  case AssetModifierKind::Bevel:
    return "bevel";
  }
  return "unknown";
}

std::string stableAssetModifierStackId(const AssetModifierStack &stack) {
  std::uint64_t hash = 1469598103934665603ull;
  appendHash(hash, stack.asset_id);
  appendHash(hash, stack.source_provenance_id);
  for (const AssetModifierDesc &modifier : stack.modifiers) {
    appendHash(hash, modifier.id);
    appendHash(hash, assetModifierKindName(modifier.kind));
    appendHash(hash, modifier.amount);
    appendHash(hash, modifier.epsilon);
    appendHash(hash, modifier.transform.position.x);
    appendHash(hash, modifier.transform.position.y);
    appendHash(hash, modifier.transform.position.z);
    appendHash(hash, modifier.transform.scale.x);
    appendHash(hash, modifier.transform.scale.y);
    appendHash(hash, modifier.transform.scale.z);
    for (const std::string &tag : modifier.creative_variant_tags) {
      appendHash(hash, tag);
    }
  }
  return hex64(hash);
}

AssetModifierStackResult applyAssetModifierStack(const CpuMesh &source,
                                                 const AssetModifierStack &stack) {
  AssetModifierStackResult result;
  result.mesh = source;
  result.report.stable_provenance_id = stableAssetModifierStackId(stack);
  result.report.input_vertices = source.vertices.size();
  result.report.input_indices = source.indices.size();

  for (const AssetModifierDesc &modifier : stack.modifiers) {
    if (!modifier.enabled) {
      result.report.degradation_reasons.push_back("disabled modifier: " + modifier.id);
      continue;
    }
    result.report.creative_variant_tags.insert(result.report.creative_variant_tags.end(),
                                               modifier.creative_variant_tags.begin(),
                                               modifier.creative_variant_tags.end());
    if (modifier.kind == AssetModifierKind::Transform) {
      result.mesh = transformCpuMesh(result.mesh, modifier.transform);
      MeshAuthoringReport report;
      report.operation = std::string(assetModifierKindName(modifier.kind));
      report.input_vertices = source.vertices.size();
      report.input_faces = source.indices.size() / 3u;
      report.output_vertices = result.mesh.vertices.size();
      report.output_faces = result.mesh.indices.size() / 3u;
      absorbReport(result.report, std::move(report));
      continue;
    }
    EditableMesh editable = editableMeshFromCpuMesh(result.mesh, result.report.stable_provenance_id);
    MeshAuthoringReport op_report;
    editable = runEditableModifier(editable, modifier, op_report);
    result.mesh = cpuMeshFromEditableMesh(editable);
    absorbReport(result.report, std::move(op_report));
  }

  std::sort(result.report.creative_variant_tags.begin(),
            result.report.creative_variant_tags.end());
  result.report.creative_variant_tags.erase(
      std::unique(result.report.creative_variant_tags.begin(),
                  result.report.creative_variant_tags.end()),
      result.report.creative_variant_tags.end());
  result.report.output_vertices = result.mesh.vertices.size();
  result.report.output_indices = result.mesh.indices.size();
  return result;
}

} // namespace aster
