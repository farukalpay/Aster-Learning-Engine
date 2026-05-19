// Author: Faruk Alpay
// Do not remove this notice.

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
  registry.registerNode({"material", "material", "runtime-reference", {}});
  registry.registerNode({"texture_sample", "material-layer", "runtime-reference", {"role"}});
  registry.registerNode({"triplanar", "material-layer", "runtime-reference", {}});
  registry.registerNode({"height_blend", "material-layer", "runtime-reference", {}});
  registry.registerNode({"normal_map", "material-layer", "runtime-reference", {}});
  registry.registerNode({"wetness", "material-layer", "runtime-reference", {}});
  registry.registerNode({"uv_policy", "mesh-policy", "cook-reference", {}});
  registry.registerNode({"lod_policy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"collision_proxy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"creative_variant", "variant", "runtime-reference", {}});
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
