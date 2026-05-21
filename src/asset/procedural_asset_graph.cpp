// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/procedural_asset_graph.hpp"

#include "aster/asset/asset_factory.hpp"
#include "aster/asset/json_document.hpp"
#include "aster/asset/pipe_runtime_asset.hpp"
#include "aster/geometry/primate_anatomy.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace aster {
namespace {

using asset_json::Value;

[[nodiscard]] const Value *objectField(const Value &json, const std::string_view key) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Object ? value : nullptr;
}

[[nodiscard]] const Value *arrayField(const Value &json, const std::string_view key) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Array ? value : nullptr;
}

[[nodiscard]] std::string normalized(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::replace(value.begin(), value.end(), '_', '-');
  return value;
}

[[nodiscard]] int hexNibble(const char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

[[nodiscard]] std::uint64_t hexHashPrefixOr(const std::string_view value,
                                            const std::uint64_t fallback = 0u) {
  std::size_t offset = value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0 ? 2u : 0u;
  std::uint64_t parsed = 0u;
  std::uint32_t digits = 0u;
  for (; offset < value.size() && digits < 16u; ++offset) {
    const int nibble = hexNibble(value[offset]);
    if (nibble < 0) {
      return digits == 0u ? fallback : parsed;
    }
    parsed = (parsed << 4u) | static_cast<std::uint64_t>(nibble);
    ++digits;
  }
  return digits == 0u ? fallback : parsed;
}

[[nodiscard]] MaterialSurfaceProfile parseSurfaceProfile(const std::string &value) {
  const std::string profile = normalized(value);
  if (profile == "stratified-rock" || profile == "cave-rock" || profile == "rock") {
    return MaterialSurfaceProfile::StratifiedRock;
  }
  if (profile == "corroded-metal" || profile == "weathered-metal" ||
      profile == "rusted-metal") {
    return MaterialSurfaceProfile::CorrodedMetal;
  }
  if (profile == "moss" || profile == "foliage") {
    return MaterialSurfaceProfile::Foliage;
  }
  if (profile == "biological-integument" || profile == "integument" ||
      profile == "skin-fur" || profile == "fur-skin" || profile == "dermal-fur") {
    return MaterialSurfaceProfile::BiologicalIntegument;
  }
  if (profile == "plain" || profile == "none") {
    return MaterialSurfaceProfile::Plain;
  }
  return MaterialSurfaceProfile::Auto;
}

[[nodiscard]] MaterialBlendMode parseBlendMode(const std::string &value) {
  const std::string mode = normalized(value);
  if (mode == "masked" || mode == "alpha-clip") {
    return MaterialBlendMode::Masked;
  }
  if (mode == "blend") {
    return MaterialBlendMode::Blend;
  }
  return MaterialBlendMode::Opaque;
}

[[nodiscard]] std::map<std::string, std::string> stringMapFrom(const Value &json) {
  std::map<std::string, std::string> out;
  if (json.kind != Value::Kind::Object) {
    return out;
  }
  for (const auto &[key, value] : json.object) {
    if (value.kind == Value::Kind::String) {
      out[key] = value.string;
    } else if (value.kind == Value::Kind::Number) {
      out[key] = std::to_string(value.number);
    } else if (value.kind == Value::Kind::Bool) {
      out[key] = value.boolean ? "true" : "false";
    }
  }
  return out;
}

void readFallbackArray(const Value &fallback, const std::string_view key,
                       const char *r, const char *g, const char *b, MaterialAsset &material) {
  const Value *array = fallback.find(key);
  if (array == nullptr || array->kind != Value::Kind::Array || array->array.size() < 3u) {
    return;
  }
  if (array->array[0].kind == Value::Kind::Number) {
    material.params[r] = static_cast<float>(array->array[0].number);
  }
  if (array->array[1].kind == Value::Kind::Number) {
    material.params[g] = static_cast<float>(array->array[1].number);
  }
  if (array->array[2].kind == Value::Kind::Number) {
    material.params[b] = static_cast<float>(array->array[2].number);
  }
}

[[nodiscard]] std::vector<ProceduralAssetGraphNode> nodesFrom(const Value &json) {
  std::vector<ProceduralAssetGraphNode> nodes;
  if (const Value *array = arrayField(json, "nodes")) {
    nodes.reserve(array->array.size());
    for (const Value &node_json : array->array) {
      ProceduralAssetGraphNode node;
      node.id = asset_json::textOr(node_json, "id");
      node.kind = asset_json::textOr(node_json, "kind");
      node.role = asset_json::textOr(node_json, "role");
      node.label = asset_json::textOr(node_json, "label");
      node.capability_status = asset_json::textOr(node_json, "capability_status");
      if (const Value *params = objectField(node_json, "params")) {
        node.params = stringMapFrom(*params);
      }
      nodes.push_back(std::move(node));
    }
  }
  return nodes;
}

[[nodiscard]] std::vector<ProceduralAssetGraphEdge> edgesFrom(const Value &json) {
  std::vector<ProceduralAssetGraphEdge> edges;
  if (const Value *array = arrayField(json, "edges")) {
    edges.reserve(array->array.size());
    for (const Value &edge_json : array->array) {
      edges.push_back({.from = asset_json::textOr(edge_json, "from"),
                       .to = asset_json::textOr(edge_json, "to"),
                       .role = asset_json::textOr(edge_json, "role")});
    }
  }
  return edges;
}

[[nodiscard]] float materialParamOr(const MaterialAsset &material, const std::string_view key,
                                    const float fallback) {
  const auto found = material.params.find(std::string(key));
  return found == material.params.end() ? fallback : found->second;
}

[[nodiscard]] int materialParamIntOr(const MaterialAsset &material, const std::string_view key,
                                     const int fallback) {
  return static_cast<int>(std::lround(materialParamOr(material, key, static_cast<float>(fallback))));
}

[[nodiscard]] ProceduralAssetGraphQualityReport qualityFrom(const Value &json) {
  ProceduralAssetGraphQualityReport quality;
  const Value *quality_json = objectField(json, "quality");
  if (quality_json == nullptr) {
    return quality;
  }
  quality.score = asset_json::u32Or(*quality_json, "score");
  quality.production_ready = asset_json::boolOr(*quality_json, "production_ready");
  if (const Value *issues = arrayField(*quality_json, "issues")) {
    for (const Value &issue : issues->array) {
      quality.issues.push_back({.severity = asset_json::textOr(issue, "severity"),
                                .category = asset_json::textOr(issue, "category"),
                                .node = asset_json::textOr(issue, "node"),
                                .message = asset_json::textOr(issue, "message")});
    }
  }
  return quality;
}

[[nodiscard]] ProceduralAssetGraphProductionSession productionSessionFrom(const Value &json) {
  ProceduralAssetGraphProductionSession session;
  const Value *session_json = objectField(json, "production_session");
  if (session_json == nullptr) {
    return session;
  }
  session.session_id = asset_json::textOr(*session_json, "session_id");
  session.graph_hash = asset_json::textOr(*session_json, "graph_hash");
  session.preview_artifact_hash = asset_json::textOr(*session_json, "preview_artifact_hash");
  session.quality_gate = asset_json::textOr(*session_json, "quality_gate");
  if (const Value *steps = arrayField(*session_json, "cook_steps")) {
    for (const Value &step : steps->array) {
      if (step.kind == Value::Kind::String) {
        session.cook_steps.push_back(step.string);
      }
    }
  }
  return session;
}

[[nodiscard]] std::vector<std::string> stringArrayFrom(const Value &json,
                                                       const std::string_view key) {
  std::vector<std::string> out;
  const Value *array = json.find(key);
  if (array == nullptr || array->kind != Value::Kind::Array) {
    return out;
  }
  out.reserve(array->array.size());
  for (const Value &entry : array->array) {
    if (entry.kind == Value::Kind::String) {
      out.push_back(entry.string);
    }
  }
  return out;
}

[[nodiscard]] ProceduralAssetGraphFactoryReport factoryReportFrom(const Value &json) {
  ProceduralAssetGraphFactoryReport report;
  const Value *factory_json = objectField(json, "factory_report");
  if (factory_json == nullptr) {
    return report;
  }
  report.stable_recipe_hash = asset_json::textOr(*factory_json, "stable_recipe_hash");
  report.visual_brief_claims = stringArrayFrom(*factory_json, "visual_brief_claims");
  report.visual_brief_rejections =
      stringArrayFrom(*factory_json, "visual_brief_rejections");

  if (const Value *stages = arrayField(*factory_json, "stage_diagnostics")) {
    report.stage_diagnostics.reserve(stages->array.size());
    for (const Value &stage_json : stages->array) {
      ProceduralAssetGraphFactoryStageReport stage;
      stage.id = asset_json::textOr(stage_json, "id");
      stage.kind = asset_json::textOr(stage_json, "kind");
      stage.status = asset_json::textOr(stage_json, "status");
      stage.diagnostics = stringArrayFrom(stage_json, "diagnostics");
      report.stage_diagnostics.push_back(std::move(stage));
    }
  }
  if (const Value *signals = arrayField(*factory_json, "surface_signal_coverage")) {
    report.surface_signal_coverage.reserve(signals->array.size());
    for (const Value &signal_json : signals->array) {
      report.surface_signal_coverage.push_back(
          {.signal = asset_json::textOr(signal_json, "signal"),
           .average = asset_json::f32Or(signal_json, "average"),
           .coverage = asset_json::f32Or(signal_json, "coverage"),
           .status = asset_json::textOr(signal_json, "status")});
    }
  }
  if (const Value *summary = objectField(*factory_json, "collision_proxy_summary")) {
    report.collision_proxy_summary = stringMapFrom(*summary);
  }
  return report;
}

[[nodiscard]] MaterialAsset materialFrom(const Value &root,
                                         const ProceduralAssetGraphPackage &package) {
  MaterialAsset material;
  const Value *material_json = objectField(root, "material");
  if (material_json == nullptr) {
    return material;
  }
  material.id = asset_json::textOr(*material_json, "id", package.id);
  material.name = package.name.empty() ? material.id : package.name;
  material.source_path = package.source_path;
  material.surface_profile =
      parseSurfaceProfile(asset_json::textOr(*material_json, "surface_profile"));
  material.explicit_features["runtime_procedural"] = true;
  material.procedural_graph_guid = package.asset_guid;
  material.procedural_graph_node = package.nodes.empty() ? std::string() : package.nodes.front().id;
  material.procedural_capability_status =
      package.nodes.empty() ? std::string("runtime-procedural-reference")
                            : package.nodes.front().capability_status;
  material.procedural_shader_variant_key = package.shader_variant_key;
  material.procedural_pipeline_key = package.pipeline_key;
  if (const Value *preview = objectField(root, "preview")) {
    material.preview = stringMapFrom(*preview);
  }
  material.provenance["runtime_model"] = package.runtime_model;
  material.provenance["source_graph"] = package.id;
  if (!package.production_session.session_id.empty()) {
    material.provenance["production_session"] = package.production_session.session_id;
  }
  if (!package.production_session.preview_artifact_hash.empty()) {
    material.provenance["preview_artifact_hash"] = package.production_session.preview_artifact_hash;
  }
  material.quality_profile["asset_graph_score"] = std::to_string(package.quality.score);
  material.quality_profile["asset_graph_production_ready"] =
      package.quality.production_ready ? "true" : "false";

  if (const Value *fallback = objectField(*material_json, "fallback")) {
    material.surface_profile =
        parseSurfaceProfile(asset_json::textOr(*fallback, "surface_profile"));
    material.blend_mode = parseBlendMode(asset_json::textOr(*fallback, "alpha_mode"));
    material.cull_mode = asset_json::boolOr(*fallback, "double_sided")
                             ? MaterialAssetCullMode::None
                             : MaterialAssetCullMode::Back;
    material.receives_shadows = asset_json::boolOr(*fallback, "receives_shadows", true);
    readFallbackArray(*fallback, "base_color", "base_color_r", "base_color_g",
                      "base_color_b", material);
    readFallbackArray(*fallback, "emission_color", "emission_r", "emission_g", "emission_b",
                      material);
    material.params["roughness"] = asset_json::f32Or(*fallback, "roughness", 0.55f);
    material.params["metallic"] = asset_json::f32Or(*fallback, "metallic", 0.0f);
    material.params["opacity"] = asset_json::f32Or(*fallback, "opacity", 1.0f);
    material.params["emission_strength"] =
        asset_json::f32Or(*fallback, "emission_strength", 0.0f);
  }
  if (const Value *params = objectField(*material_json, "params")) {
    for (const auto &[key, value] : params->object) {
      if (value.kind == Value::Kind::Number) {
        material.params[key] = static_cast<float>(value.number);
      }
    }
  }
  if (const Value *features = objectField(*material_json, "features")) {
    for (const auto &[key, value] : features->object) {
      if (value.kind == Value::Kind::Bool) {
        material.explicit_features[key] = value.boolean;
      }
    }
  }
  return material;
}

} // namespace

ProceduralAssetGraphPackage loadProceduralAssetGraphPackage(const std::filesystem::path &path) {
  const Value root = asset_json::parseFile(path);
  ProceduralAssetGraphPackage package;
  package.package_path = path;
  package.asset_guid = asset_json::textOr(root, "asset_guid");
  package.id = asset_json::textOr(root, "id");
  package.name = asset_json::textOr(root, "name", package.id);
  package.source_path = asset_json::textOr(root, "source_path");
  package.runtime_model = asset_json::textOr(root, "runtime_model");
  package.nodes = nodesFrom(root);
  package.edges = edgesFrom(root);
  package.production_session = productionSessionFrom(root);
  package.factory_report = factoryReportFrom(root);
  package.quality = qualityFrom(root);

  if (const Value *material = objectField(root, "material")) {
    package.shader_variant_tag = asset_json::textOr(*material, "shader_variant_tag");
    package.pipeline_tag = asset_json::textOr(*material, "pipeline_tag");
    package.feature_mask = asset_json::u64Or(*material, "feature_mask");
    package.shader_variant_key = asset_json::u64Or(*material, "shader_variant_key");
  }
  if (const Value *derived = objectField(root, "derived_hashes")) {
    package.pipeline_key = hexHashPrefixOr(asset_json::textOr(*derived, "pipeline_cache_key"),
                                           asset_json::u64Or(*derived, "pipeline_cache_key"));
  }
  if (const Value *mesh = objectField(root, "mesh")) {
    package.mesh = {.primitive = asset_json::textOr(*mesh, "primitive"),
                    .uv_policy = asset_json::textOr(*mesh, "uv_policy"),
                    .tangent_policy = asset_json::textOr(*mesh, "tangent_policy"),
                    .collision_proxy = asset_json::textOr(*mesh, "collision_proxy"),
                    .lod_policy = asset_json::textOr(*mesh, "lod_policy")};
  }
  package.material = materialFrom(root, package);
  for (const ProceduralAssetGraphQualityIssue &issue : package.quality.issues) {
    package.diagnostics.push_back(
        {.severity = issue.severity == "error" ? MaterialDiagnosticSeverity::Error
                                                : MaterialDiagnosticSeverity::Warning,
         .source_path = package.source_path,
         .message = issue.category + ":" + issue.node + ":" + issue.message});
  }
  return package;
}

Material proceduralAssetGraphMaterial(const ProceduralAssetGraphPackage &package) {
  return resolveMaterialAssetFallback(package.material);
}

CpuMesh proceduralAssetGraphMesh(const ProceduralAssetGraphPackage &package) {
  const std::string primitive = normalized(package.mesh.primitive);
  if (primitive == "cercopithecidae" || primitive == "primate-cercopithecidae") {
    return makeCercopithecidaeMesh({.surface_segments = 36,
                                    .surface_rings = 18,
                                    .include_soft_tissue = true,
                                    .include_muscle_insertions = true,
                                    .include_surface_pads = true,
                                    .include_surface_detail = true,
                                    .fur_strand_guides = 96,
                                    .surface_detail_strength = 1.0f,
                                    .include_integument = true,
                                    .integument_epidermal_layers = 3,
                                    .integument_shell_offset = 0.022f,
                                    .pigment_heterogeneity = 0.68f,
                                    .vascular_translucency = 0.42f,
                                    .follicle_density = 1.22f,
                                    .gland_cluster_count = 36,
                                    .tension_line_strength = 1.15f});
  }
  if (primitive == "rusted-pipe" || primitive == "industrial-pipe" ||
      primitive == "production-rusted-pipe") {
    AsterPipeAssetSpec spec{.asset_id = package.id,
                            .length = 5.2f,
                            .outer_radius = 0.54f,
                            .wall_thickness = 0.090f,
                            .radial_segments = 96,
                            .length_segments = 24,
                            .include_longitudinal_seam = false,
                            .include_flanges = false,
                            .include_bolts = false,
                            .bolt_count_per_flange = 10,
                            .rust_strength =
                                materialParamOr(package.material, "rust_strength", 0.86f),
                            .wetness_strength =
                                materialParamOr(package.material, "wetness", 0.24f),
                            .pitting_density =
                                materialParamOr(package.material, "pitting_density", 0.72f),
                            .pitting_depth =
                                materialParamOr(package.material, "pitting_depth", 0.0022f),
                            .oxide_layering =
                                materialParamOr(package.material, "oxide_layering", 0.86f),
                            .cavity_grime_strength =
                                materialParamOr(package.material, "cavity_grime", 0.70f),
                            .edge_polish_strength =
                                materialParamOr(package.material, "edge_polish", 0.36f),
                            .weld_heat_tint_strength =
                                materialParamOr(package.material, "weld_heat_tint", 0.48f),
                            .axial_scratch_strength =
                                materialParamOr(package.material, "axial_scratches", 0.66f),
                            .rust_bloom_strength =
                                materialParamOr(package.material, "rust_bloom", 0.86f),
                            .black_scab_strength =
                                materialParamOr(package.material, "black_scab", 0.74f),
                            .paint_remnant_strength =
                                materialParamOr(package.material, "paint_remnant", 0.18f),
                            .weld_slag_strength =
                                materialParamOr(package.material, "weld_slag", 0.82f),
                            .rim_soot_strength =
                                materialParamOr(package.material, "rim_soot", 0.88f),
                            .wet_streak_count =
                                materialParamIntOr(package.material, "wet_streaks", 7)};
    const AsterPipeFoundryVariant variant =
        primitive == "industrial-pipe" ? AsterPipeFoundryVariant::IndustrialHardware
                                       : AsterPipeFoundryVariant::ReferenceSilhouette;
    return buildAsterAssetFoundryRecipe(makeAsterPipeFoundryRecipe(spec, variant)).mesh;
  }
  if (primitive == "sphere" || primitive == "uv-sphere") {
    return makeUvSphere(32, 16, 1.0f);
  }
  if (primitive == "box" || primitive == "cube") {
    return makeBox();
  }
  if (primitive == "plane") {
    return makePlane(2.0f);
  }
  if (primitive == "crystal") {
    return makeCrystal(8, 0.5f, 1.2f);
  }
  if (primitive == "pillar") {
    return makePillar(12, 0.45f, 1.4f);
  }
  return makeRock(18, 10, 1.0f);
}

MaterialAuthoringGraph materialAuthoringGraphForPackage(
    const ProceduralAssetGraphPackage &package) {
  MaterialAuthoringGraph graph;
  graph.source_id = package.id;
  graph.source_kind = "assetgraphbin";
  graph.nodes.reserve(package.nodes.size());
  for (const ProceduralAssetGraphNode &source : package.nodes) {
    graph.nodes.push_back({.id = source.id,
                           .label = source.label.empty() ? source.id : source.label,
                           .operation = source.kind,
                           .role = source.role,
                           .params = source.params,
                           .op = MaterialGraphOperation::Unknown,
                           .value_type = MaterialGraphValueType::Unknown,
                           .capability_status = source.capability_status,
                           .editable = true,
                           .persisted = false});
  }
  graph.edges.reserve(package.edges.size());
  for (const ProceduralAssetGraphEdge &edge : package.edges) {
    graph.edges.push_back({.from = edge.from, .to = edge.to, .role = edge.role});
  }
  return graph;
}

} // namespace aster
