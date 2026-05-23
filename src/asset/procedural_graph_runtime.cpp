// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/procedural_graph_runtime.hpp"

#include "aster/asset/asset_modifier_stack.hpp"
#include "aster/geometry/geometry_operations.hpp"

#include <algorithm>
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

[[nodiscard]] std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << "aster-graph-0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

[[nodiscard]] float nodeParamF32(const ProceduralAssetGraphNode &node,
                                 const std::string_view key, const float fallback) {
  const auto found = node.params.find(std::string(key));
  if (found == node.params.end()) {
    return fallback;
  }
  try {
    return std::stof(found->second);
  } catch (...) {
    return fallback;
  }
}

[[nodiscard]] std::uint32_t nodeParamU32(const ProceduralAssetGraphNode &node,
                                         const std::string_view key,
                                         const std::uint32_t fallback) {
  return static_cast<std::uint32_t>(std::max(0.0f, std::round(nodeParamF32(node, key, fallback))));
}

[[nodiscard]] bool nodeParamBool(const ProceduralAssetGraphNode &node,
                                 const std::string_view key, const bool fallback) {
  const auto found = node.params.find(std::string(key));
  if (found == node.params.end()) {
    return fallback;
  }
  return found->second == "true" || found->second == "yes" || found->second == "1";
}

[[nodiscard]] Vec3 nodeParamVec3(const ProceduralAssetGraphNode &node,
                                 const std::string_view prefix, const Vec3 fallback) {
  return {nodeParamF32(node, std::string(prefix) + "_x", fallback.x),
          nodeParamF32(node, std::string(prefix) + "_y", fallback.y),
          nodeParamF32(node, std::string(prefix) + "_z", fallback.z)};
}

[[nodiscard]] AssetModifierKind modifierKindForNode(const std::string &kind) {
  if (kind == "transform_geometry") {
    return AssetModifierKind::Transform;
  }
  if (kind == "merge_by_distance") {
    return AssetModifierKind::Weld;
  }
  if (kind == "triangulate") {
    return AssetModifierKind::Triangulate;
  }
  if (kind == "extrude_mesh") {
    return AssetModifierKind::Extrude;
  }
  if (kind == "bevel_modifier" || kind == "bevel") {
    return AssetModifierKind::Bevel;
  }
  if (kind == "array_modifier") {
    return AssetModifierKind::Array;
  }
  if (kind == "solidify_modifier") {
    return AssetModifierKind::Solidify;
  }
  if (kind == "displace_modifier") {
    return AssetModifierKind::Displace;
  }
  if (kind == "weighted_normal" || kind == "soft_rim_normals") {
    return AssetModifierKind::WeightedNormal;
  }
  if (kind == "smooth_modifier") {
    return AssetModifierKind::Smooth;
  }
  if (kind == "decimate_modifier") {
    return AssetModifierKind::Decimate;
  }
  return AssetModifierKind::Transform;
}

[[nodiscard]] bool isRuntimeModifierNode(const std::string &kind) {
  return kind == "transform_geometry" || kind == "merge_by_distance" || kind == "triangulate" ||
         kind == "extrude_mesh" || kind == "bevel_modifier" || kind == "bevel" ||
         kind == "array_modifier" || kind == "solidify_modifier" ||
         kind == "displace_modifier" || kind == "weighted_normal" ||
         kind == "soft_rim_normals" || kind == "smooth_modifier" ||
         kind == "decimate_modifier";
}

[[nodiscard]] AssetModifierDesc modifierFromNode(const ProceduralAssetGraphNode &node) {
  AssetModifierDesc desc;
  desc.id = node.id;
  desc.kind = modifierKindForNode(node.kind);
  desc.amount = nodeParamF32(node, "amount",
                             nodeParamF32(node, "width",
                                          desc.kind == AssetModifierKind::Decimate ? 0.72f
                                                                                  : 0.02f));
  desc.epsilon = nodeParamF32(node, "epsilon", nodeParamF32(node, "distance", 0.0001f));
  desc.seed = nodeParamU32(node, "seed", 0xA57E2026u);
  desc.count = std::max(1u, nodeParamU32(node, "count", nodeParamU32(node, "copies", 1u)));
  desc.transform.position = nodeParamVec3(node, "translate", {});
  desc.transform.scale = nodeParamVec3(node, "scale", {1.0f, 1.0f, 1.0f});
  desc.enabled = nodeParamBool(node, "enabled", true);
  if (!node.role.empty()) {
    desc.creative_variant_tags.push_back("graph-role:" + node.role);
  }
  return desc;
}

void applyRuntimeGraphNodes(ProceduralGraphEvaluationResult &result,
                            const ProceduralAssetGraphPackage &package) {
  AssetModifierStack stack;
  stack.asset_id = package.id;
  stack.source_provenance_id = result.stable_provenance_id;
  std::vector<GeometryPoint> scattered_points;

  const auto flushStack = [&]() {
    if (stack.modifiers.empty()) {
      return;
    }
    const AssetModifierStackResult modified = applyAssetModifierStack(result.mesh, stack);
    result.mesh = modified.mesh;
    result.quality_score = std::min(result.quality_score, modified.report.quality_score);
    for (const std::string &diagnostic : modified.report.diagnostics) {
      result.diagnostics.push_back({"warning", stack.asset_id, diagnostic});
    }
    stack.modifiers.clear();
  };

  for (const ProceduralAssetGraphNode &node : package.nodes) {
    if (isRuntimeModifierNode(node.kind)) {
      stack.modifiers.push_back(modifierFromNode(node));
      continue;
    }
    if (node.kind == "scatter_points") {
      flushStack();
      scattered_points = scatterPointsOnMesh(
          result.mesh, {.count = nodeParamU32(node, "count", 16u),
                        .seed = nodeParamU32(node, "seed", 1u),
                        .radius = nodeParamF32(node, "radius", 1.0f)});
      continue;
    }
    if (node.kind == "instance_on_points") {
      flushStack();
      if (!scattered_points.empty()) {
        GeometrySet instances = instanceMeshOnPoints(result.mesh, scattered_points, node.id);
        result.mesh = joinGeometryMeshes(instances);
      }
      continue;
    }
    if (node.kind == "join_geometry") {
      flushStack();
      const std::uint32_t copies = std::max(1u, nodeParamU32(node, "copies", 1u));
      if (copies > 1u) {
        GeometrySet set;
        set.meshes.reserve(copies);
        for (std::uint32_t copy = 0u; copy < copies; ++copy) {
          GeometryMeshPart part;
          part.id = node.id + "." + std::to_string(copy);
          part.mesh = result.mesh;
          part.transform.position = {nodeParamF32(node, "spacing", 1.0f) *
                                         static_cast<float>(copy),
                                     0.0f, 0.0f};
          set.meshes.push_back(std::move(part));
        }
        result.mesh = joinGeometryMeshes(set);
      }
      continue;
    }
  }
  flushStack();
}

} // namespace

void ProceduralNodeRegistry::registerNode(ProceduralNodeDescriptor descriptor) {
  const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                  [&](const ProceduralNodeDescriptor &node) {
                                    return node.kind == descriptor.kind;
                                  });
  if (found != nodes_.end()) {
    *found = std::move(descriptor);
    return;
  }
  nodes_.push_back(std::move(descriptor));
}

const ProceduralNodeDescriptor *ProceduralNodeRegistry::find(
    const std::string_view kind) const noexcept {
  const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                  [&](const ProceduralNodeDescriptor &node) {
                                    return node.kind == kind;
                                  });
  return found == nodes_.end() ? nullptr : &*found;
}

const std::vector<ProceduralNodeDescriptor> &ProceduralNodeRegistry::nodes() const noexcept {
  return nodes_;
}

ProceduralNodeRegistry makeDefaultProceduralNodeRegistry() {
  ProceduralNodeRegistry registry;
  registry.registerNode({"mesh_primitive", "mesh", "runtime-reference", {"primitive"}});
  registry.registerNode({"pipe_body", "mesh", "runtime-procedural-reference", {"primitive"}});
  registry.registerNode({"factory_recipe", "asset-factory", "runtime-procedural-reference", {"target"}});
  registry.registerNode({"factory_stage", "asset-factory-stage", "runtime-procedural-reference", {"kind"}});
  registry.registerNode({"surface_contract", "asset-factory-surface", "runtime-procedural-reference", {}});
  registry.registerNode({"physics_proxy", "asset-factory-physics", "runtime-procedural-reference", {"shape"}});
  registry.registerNode({"lod_recipe", "asset-factory-lod", "runtime-procedural-reference", {"levels"}});
  registry.registerNode({"quality_signal", "asset-factory-quality", "runtime-procedural-reference", {"signal"}});
  registry.registerNode({"visual_brief_claim", "asset-factory-quality", "runtime-procedural-reference", {"signal"}});
  registry.registerNode({"transform_geometry", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"join_geometry", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"scatter_points", "point-cloud", "runtime-reference", {}});
  registry.registerNode({"instance_on_points", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"merge_by_distance", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"triangulate", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"extrude_mesh", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"bevel_modifier", "mesh-operator", "runtime-procedural-reference", {}});
  registry.registerNode({"bevel", "mesh-operator", "descriptor-only-reference", {}});
  registry.registerNode({"array_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"solidify_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"displace_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"weighted_normal", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"smooth_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"decimate_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"material", "material", "runtime-reference", {}});
  registry.registerNode({"material_assignment", "material", "runtime-procedural-reference", {}});
  registry.registerNode({"texture_sample", "material-layer", "runtime-reference", {"role"}});
  registry.registerNode({"triplanar", "material-layer", "runtime-reference", {}});
  registry.registerNode({"height_blend", "material-layer", "runtime-reference", {}});
  registry.registerNode({"normal_map", "material-layer", "runtime-reference", {}});
  registry.registerNode({"normal_height", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"rust_mask", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"wetness_flow", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"edge_wear", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"cavity_dirt", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"weld_seam", "mesh-detail", "runtime-procedural-reference", {}});
  registry.registerNode({"wetness", "material-layer", "runtime-reference", {}});
  registry.registerNode({"uv_policy", "mesh-policy", "cook-reference", {}});
  registry.registerNode({"tangent_validation", "mesh-policy", "cook-reference", {}});
  registry.registerNode({"lod_policy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"lod_generator", "runtime-policy", "runtime-procedural-reference", {}});
  registry.registerNode({"collision_proxy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"creative_variant", "variant", "runtime-reference", {}});
  registry.registerNode({"prefab_variant", "variant", "runtime-procedural-reference", {}});
  registry.registerNode({"cook_export", "package", "runtime-procedural-reference", {}});
  registry.registerNode({"diagnostic", "quality", "runtime-procedural-reference", {}});
  registry.registerNode({"provenance", "quality", "runtime-reference", {}});
  registry.registerNode({"anatomy_landmark", "biology", "runtime-procedural-reference", {}});
  registry.registerNode({"follicle_distribution", "biology", "runtime-procedural-reference", {}});
  registry.registerNode({"epidermal_strata", "biology", "runtime-procedural-reference", {}});
  return registry;
}

std::string stableProceduralGraphProvenanceId(const ProceduralAssetGraphPackage &package) {
  std::uint64_t hash = 1469598103934665603ull;
  appendHash(hash, package.asset_guid);
  appendHash(hash, package.id);
  appendHash(hash, package.runtime_model);
  appendHash(hash, package.shader_variant_tag);
  appendHash(hash, package.pipeline_tag);
  for (const ProceduralAssetGraphNode &node : package.nodes) {
    appendHash(hash, node.id);
    appendHash(hash, node.kind);
    appendHash(hash, node.role);
  }
  for (const ProceduralAssetGraphEdge &edge : package.edges) {
    appendHash(hash, edge.from);
    appendHash(hash, edge.to);
    appendHash(hash, edge.role);
  }
  return hex64(hash);
}

ProceduralGraphEvaluationResult evaluateProceduralAssetGraph(
    const ProceduralAssetGraphPackage &package, const ProceduralNodeRegistry &registry) {
  ProceduralGraphEvaluationResult result;
  result.package_id = package.id;
  result.stable_provenance_id = stableProceduralGraphProvenanceId(package);
  result.quality_score = package.quality.score;
  result.production_ready = package.quality.production_ready;

  for (const ProceduralAssetGraphNode &node : package.nodes) {
    const ProceduralNodeDescriptor *descriptor = registry.find(node.kind);
    if (descriptor == nullptr) {
      result.diagnostics.push_back({"warning", node.id,
                                    "node kind is not registered in Aster runtime"});
      result.quality_score = std::min(result.quality_score, 72u);
      continue;
    }
    for (const std::string &param : descriptor->required_params) {
      if (node.params.find(param) == node.params.end()) {
        result.diagnostics.push_back({"error", node.id,
                                      "required node parameter is missing: " + param});
        result.production_ready = false;
        result.quality_score = 0u;
      }
    }
  }
  for (const ProceduralAssetGraphQualityIssue &issue : package.quality.issues) {
    result.diagnostics.push_back({issue.severity, issue.node, issue.message});
  }

  result.mesh = proceduralAssetGraphMesh(package);
  applyRuntimeGraphNodes(result, package);
  result.material = proceduralAssetGraphMaterial(package);
  result.material_graph = materialAuthoringGraphForPackage(package);
  return result;
}

} // namespace aster
