// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/procedural_graph_runtime.hpp"

#include <algorithm>
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
  result.material = proceduralAssetGraphMaterial(package);
  result.material_graph = materialAuthoringGraphForPackage(package);
  return result;
}

} // namespace aster
