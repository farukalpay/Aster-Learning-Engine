// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/asset_modifier_stack.hpp"

#include "aster/geometry/geometry_operations.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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

void appendHash(std::uint64_t &hash, const std::uint32_t value) {
  for (std::uint32_t shift = 0u; shift < 32u; shift += 8u) {
    hash ^= static_cast<std::uint8_t>((value >> shift) & 0xffu);
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

[[nodiscard]] MeshAuthoringReport makeCpuReport(const std::string_view operation,
                                                const CpuMesh &input,
                                                const CpuMesh &output) {
  MeshAuthoringReport report;
  report.operation = std::string(operation);
  report.input_vertices = input.vertices.size();
  report.input_faces = input.indices.size() / 3u;
  report.output_vertices = output.vertices.size();
  report.output_faces = output.indices.size() / 3u;
  report.generated_triangles = output.indices.size() / 3u;
  report.ok = !output.vertices.empty() && output.indices.size() % 3u == 0u;
  report.quality_score = report.ok ? 100u : 0u;
  if (!report.ok) {
    report.issues.push_back({"error", report.operation, "modifier produced an invalid mesh", 0u});
  }
  return report;
}

void rebuildAreaWeightedNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    if (mesh.indices[i] >= mesh.vertices.size() || mesh.indices[i + 1u] >= mesh.vertices.size() ||
        mesh.indices[i + 2u] >= mesh.vertices.size()) {
      continue;
    }
    Vertex &a = mesh.vertices[mesh.indices[i]];
    Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const Vec3 normal = cross(b.position - a.position, c.position - a.position);
    a.normal += normal;
    b.normal += normal;
    c.normal += normal;
  }
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f});
  }
}

[[nodiscard]] CpuMesh arrayCpuMesh(const CpuMesh &mesh, const AssetModifierDesc &modifier) {
  const std::uint32_t count = std::max(1u, modifier.count);
  CpuMesh out;
  out.vertices.reserve(mesh.vertices.size() * count);
  out.indices.reserve(mesh.indices.size() * count);
  for (std::uint32_t copy = 0u; copy < count; ++copy) {
    Transform transform = modifier.transform;
    transform.position = modifier.transform.position * static_cast<float>(copy);
    if (lengthSquared(modifier.transform.position) <= 0.0000001f) {
      const float spacing = modifier.amount == 0.0f ? 1.0f : modifier.amount;
      transform.position = {spacing * static_cast<float>(copy), 0.0f, 0.0f};
    }
    const CpuMesh part = copy == 0u ? mesh : transformCpuMesh(mesh, transform);
    const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
    out.vertices.insert(out.vertices.end(), part.vertices.begin(), part.vertices.end());
    for (const std::uint32_t index : part.indices) {
      out.indices.push_back(base + index);
    }
  }
  return out;
}

[[nodiscard]] CpuMesh solidifyCpuMesh(const CpuMesh &mesh, const AssetModifierDesc &modifier) {
  CpuMesh out = mesh;
  const float amount = modifier.amount == 0.0f ? 0.02f : modifier.amount;
  const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
  out.vertices.reserve(mesh.vertices.size() * 2u);
  for (const Vertex &vertex : mesh.vertices) {
    Vertex inner = vertex;
    inner.position -= normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f}) * amount;
    inner.normal = inner.normal * -1.0f;
    out.vertices.push_back(inner);
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    out.indices.insert(out.indices.end(),
                       {base + mesh.indices[i + 2u], base + mesh.indices[i + 1u],
                        base + mesh.indices[i]});
  }
  return out;
}

[[nodiscard]] float deterministicNoise(const Vec3 position, const std::uint32_t seed) {
  const float value = std::sin(position.x * 12.9898f + position.y * 78.233f +
                               position.z * 37.719f + static_cast<float>(seed) * 0.017f) *
                      43758.5453f;
  return (value - std::floor(value)) * 2.0f - 1.0f;
}

[[nodiscard]] CpuMesh displaceCpuMesh(const CpuMesh &mesh, const AssetModifierDesc &modifier) {
  CpuMesh out = mesh;
  for (Vertex &vertex : out.vertices) {
    const float noise = deterministicNoise(vertex.position, modifier.seed);
    vertex.position += normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f}) * modifier.amount * noise;
  }
  return out;
}

[[nodiscard]] CpuMesh smoothCpuMesh(const CpuMesh &mesh, const AssetModifierDesc &modifier) {
  CpuMesh out = mesh;
  const std::uint32_t iterations = std::max(1u, modifier.count);
  const float alpha = std::clamp(modifier.amount == 0.0f ? 0.35f : modifier.amount, 0.0f, 1.0f);
  std::vector<std::vector<std::uint32_t>> neighbors(out.vertices.size());
  for (std::size_t i = 0u; i + 2u < out.indices.size(); i += 3u) {
    const std::array<std::uint32_t, 3u> tri{out.indices[i], out.indices[i + 1u],
                                            out.indices[i + 2u]};
    for (std::uint32_t corner = 0u; corner < 3u; ++corner) {
      const std::uint32_t a = tri[corner];
      const std::uint32_t b = tri[(corner + 1u) % 3u];
      if (a < neighbors.size() && b < neighbors.size()) {
        neighbors[a].push_back(b);
        neighbors[b].push_back(a);
      }
    }
  }
  for (std::vector<std::uint32_t> &values : neighbors) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
  }
  for (std::uint32_t iteration = 0u; iteration < iterations; ++iteration) {
    std::vector<Vec3> positions(out.vertices.size());
    for (std::size_t i = 0u; i < out.vertices.size(); ++i) {
      if (neighbors[i].empty()) {
        positions[i] = out.vertices[i].position;
        continue;
      }
      Vec3 average{};
      for (const std::uint32_t neighbor : neighbors[i]) {
        average += out.vertices[neighbor].position;
      }
      average = average / static_cast<float>(neighbors[i].size());
      positions[i] = out.vertices[i].position * (1.0f - alpha) + average * alpha;
    }
    for (std::size_t i = 0u; i < out.vertices.size(); ++i) {
      out.vertices[i].position = positions[i];
    }
  }
  rebuildAreaWeightedNormals(out);
  return out;
}

[[nodiscard]] CpuMesh decimateCpuMesh(const CpuMesh &mesh, const AssetModifierDesc &modifier,
                                      MeshAuthoringReport &report) {
  CpuMesh out;
  out.vertices = mesh.vertices;
  const float keep_ratio = std::clamp(modifier.amount <= 0.0f ? 0.5f : modifier.amount,
                                      0.05f, 1.0f);
  const std::uint32_t stride =
      std::max(2u, static_cast<std::uint32_t>(std::round(1.0f / (1.0f - keep_ratio + 0.0001f))));
  for (std::size_t i = 0u, triangle = 0u; i + 2u < mesh.indices.size(); i += 3u, ++triangle) {
    if (keep_ratio >= 0.999f || ((triangle + modifier.seed) % stride) != 0u) {
      out.indices.insert(out.indices.end(), {mesh.indices[i], mesh.indices[i + 1u],
                                             mesh.indices[i + 2u]});
    }
  }
  if (out.indices.empty() && mesh.indices.size() >= 3u) {
    out.indices.insert(out.indices.end(), {mesh.indices[0u], mesh.indices[1u], mesh.indices[2u]});
  }
  report = makeCpuReport(assetModifierKindName(modifier.kind), mesh, out);
  report.removed_faces = mesh.indices.size() / 3u - out.indices.size() / 3u;
  if (report.removed_faces > 0u) {
    report.issues.push_back({"warning", report.operation,
                             "deterministic decimate removed render triangles", 0u});
    report.quality_score = std::max<std::uint32_t>(40u, 100u -
                                                            static_cast<std::uint32_t>(
                                                                report.removed_faces));
  }
  return out;
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
  case AssetModifierKind::Array:
  case AssetModifierKind::Solidify:
  case AssetModifierKind::Displace:
  case AssetModifierKind::WeightedNormal:
  case AssetModifierKind::Smooth:
  case AssetModifierKind::Decimate:
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
  case AssetModifierKind::Array:
    return "array";
  case AssetModifierKind::Solidify:
    return "solidify";
  case AssetModifierKind::Displace:
    return "displace";
  case AssetModifierKind::WeightedNormal:
    return "weighted-normal";
  case AssetModifierKind::Smooth:
    return "smooth";
  case AssetModifierKind::Decimate:
    return "decimate";
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
    appendHash(hash, static_cast<std::uint32_t>(modifier.mirror_axis));
    appendHash(hash, modifier.seed);
    appendHash(hash, modifier.count);
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
    if (modifier.kind == AssetModifierKind::Array ||
        modifier.kind == AssetModifierKind::Solidify ||
        modifier.kind == AssetModifierKind::Displace ||
        modifier.kind == AssetModifierKind::WeightedNormal ||
        modifier.kind == AssetModifierKind::Smooth ||
        modifier.kind == AssetModifierKind::Decimate) {
      const CpuMesh before = result.mesh;
      MeshAuthoringReport report;
      switch (modifier.kind) {
      case AssetModifierKind::Array:
        result.mesh = arrayCpuMesh(result.mesh, modifier);
        break;
      case AssetModifierKind::Solidify:
        result.mesh = solidifyCpuMesh(result.mesh, modifier);
        break;
      case AssetModifierKind::Displace:
        result.mesh = displaceCpuMesh(result.mesh, modifier);
        break;
      case AssetModifierKind::WeightedNormal:
        rebuildAreaWeightedNormals(result.mesh);
        break;
      case AssetModifierKind::Smooth:
        result.mesh = smoothCpuMesh(result.mesh, modifier);
        break;
      case AssetModifierKind::Decimate:
        result.mesh = decimateCpuMesh(result.mesh, modifier, report);
        break;
      default:
        break;
      }
      if (modifier.kind != AssetModifierKind::Decimate) {
        report = makeCpuReport(assetModifierKindName(modifier.kind), before, result.mesh);
      }
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
