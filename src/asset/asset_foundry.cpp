// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/asset_foundry.hpp"

#include "aster/asset/procedural_asset_graph.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] std::uint64_t fnvSeed() {
  return 1469598103934665603ull;
}

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

void appendHash(std::uint64_t &hash, const std::size_t value) {
  appendHash(hash, static_cast<std::uint32_t>(value & 0xffffffffu));
  appendHash(hash, static_cast<std::uint32_t>((value >> 32u) & 0xffffffffu));
}

void appendHash(std::uint64_t &hash, const Vec3 value) {
  appendHash(hash, value.x);
  appendHash(hash, value.y);
  appendHash(hash, value.z);
}

void appendHash(std::uint64_t &hash, const ProceduralSurfaceLayer &layer) {
  appendHash(hash, layer.macro_variation);
  appendHash(hash, layer.micro_normal_strength);
  appendHash(hash, layer.roughness_variation);
  appendHash(hash, layer.physical_texel_density);
  appendHash(hash, layer.height_normal_coupling);
  appendHash(hash, layer.roughness_height_coupling);
  appendHash(hash, layer.macro_frequency_breakup);
  appendHash(hash, layer.micro_frequency_breakup);
  appendHash(hash, layer.wetness);
  appendHash(hash, layer.height_shading);
  appendHash(hash, layer.pitting_density);
  appendHash(hash, layer.pitting_depth);
  appendHash(hash, layer.oxide_layering);
  appendHash(hash, layer.cavity_grime);
  appendHash(hash, layer.edge_polish);
  appendHash(hash, layer.weld_heat_tint);
  appendHash(hash, layer.axial_scratches);
  appendHash(hash, layer.wet_streaks);
  appendHash(hash, layer.rust_bloom);
  appendHash(hash, layer.black_scab);
  appendHash(hash, layer.paint_remnant);
  appendHash(hash, layer.weld_slag);
  appendHash(hash, layer.rim_soot);
}

[[nodiscard]] std::string hex64(const std::uint64_t value, const std::string_view prefix) {
  std::ostringstream out;
  out << prefix << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

[[nodiscard]] bool containsId(const std::vector<std::string> &ids, const std::string &id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

[[nodiscard]] bool containsText(const std::vector<std::string> &values,
                                const std::string &needle) {
  return std::find(values.begin(), values.end(), needle) != values.end();
}

[[nodiscard]] bool stageExists(const AsterAssetFoundryRecipe &recipe, const std::string &id) {
  return std::any_of(recipe.stages.begin(), recipe.stages.end(),
                     [&](const AsterAssetFoundryStage &stage) { return stage.id == id; });
}

[[nodiscard]] bool duplicateId(const std::vector<std::string> &seen, const std::string &id) {
  return std::find(seen.begin(), seen.end(), id) != seen.end();
}

[[nodiscard]] const AsterAssetFoundrySurfaceContract *
findSurfaceContract(const AsterAssetFoundryRecipe &recipe, const std::string &id) {
  const auto found =
      std::find_if(recipe.surface_contracts.begin(), recipe.surface_contracts.end(),
                   [&](const AsterAssetFoundrySurfaceContract &contract) {
                     return contract.id == id;
                   });
  return found == recipe.surface_contracts.end() ? nullptr : &*found;
}

[[nodiscard]] const AsterAssetFoundryPhysicsProxy *
findPhysicsProxy(const AsterAssetFoundryRecipe &recipe, const std::string &id) {
  const auto found = std::find_if(recipe.physics_proxies.begin(), recipe.physics_proxies.end(),
                                  [&](const AsterAssetFoundryPhysicsProxy &proxy) {
                                    return proxy.id == id;
                                  });
  return found == recipe.physics_proxies.end() ? nullptr : &*found;
}

[[nodiscard]] bool textContains(const std::string &value, const std::string_view needle) {
  return value.find(needle) != std::string::npos;
}

[[nodiscard]] std::string normalizedText(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::replace(value.begin(), value.end(), '_', '-');
  return value;
}

[[nodiscard]] float graphMaterialParamOr(const ProceduralAssetGraphPackage &package,
                                         const std::string_view key, const float fallback) {
  const auto found = package.material.params.find(std::string(key));
  return found == package.material.params.end() ? fallback : found->second;
}

[[nodiscard]] std::uint32_t graphMaterialU32Or(const ProceduralAssetGraphPackage &package,
                                               const std::string_view key,
                                               const std::uint32_t fallback) {
  return static_cast<std::uint32_t>(
      std::max(0.0f, std::round(graphMaterialParamOr(package, key, fallback))));
}

[[nodiscard]] AsterAssetFoundryPhysicsProxy boundsProxyForMesh(const CpuMesh &mesh,
                                                               std::string id) {
  Vec3 min{0.0f, 0.0f, 0.0f};
  Vec3 max{0.0f, 0.0f, 0.0f};
  if (!mesh.vertices.empty()) {
    min = mesh.vertices.front().position;
    max = min;
    for (const Vertex &vertex : mesh.vertices) {
      min.x = std::min(min.x, vertex.position.x);
      min.y = std::min(min.y, vertex.position.y);
      min.z = std::min(min.z, vertex.position.z);
      max.x = std::max(max.x, vertex.position.x);
      max.y = std::max(max.y, vertex.position.y);
      max.z = std::max(max.z, vertex.position.z);
    }
  }
  const Vec3 half_extents = (max - min) * 0.5f;
  return {.id = std::move(id),
          .label = "Aster graph runtime bounds proxy",
          .kind = AsterAssetFoundryPhysicsProxyKind::BoundsBox,
          .center = (min + max) * 0.5f,
          .half_extents = {std::max(half_extents.x, 0.001f),
                           std::max(half_extents.y, 0.001f),
                           std::max(half_extents.z, 0.001f)},
          .radius = std::max({half_extents.x, half_extents.y, half_extents.z, 0.001f}),
          .length = std::max((max - min).z, 0.001f),
          .triangle_budget = mesh.indices.size() / 3u,
          .covers_render_bounds = true,
          .query_enabled = true,
          .material = {.friction = 0.72f, .restitution = 0.02f},
          .filter = {.layer_bits = 1u, .collides_with = 0xffffffffu}};
}

[[nodiscard]] std::vector<AsterAssetFoundryProofArtifact> proofArtifactsFromGraph(
    const ProceduralAssetGraphPackage &package) {
  std::vector<AsterAssetFoundryProofArtifact> artifacts;
  artifacts.reserve(package.proof_artifacts.size());
  for (const ProceduralAssetGraphProofArtifact &artifact : package.proof_artifacts) {
    artifacts.push_back({.id = artifact.id,
                         .role = artifact.role,
                         .path = artifact.path,
                         .kind = artifact.kind,
                         .hash = artifact.hash,
                         .width = artifact.width,
                         .height = artifact.height,
                         .signal_tags = artifact.signal_tags});
  }
  return artifacts;
}

[[nodiscard]] AsterAssetFoundryDiagnosticSeverity severityFromMessage(
    const std::string &message) {
  if (message.rfind("error:", 0u) == 0u) {
    return AsterAssetFoundryDiagnosticSeverity::Error;
  }
  if (message.rfind("warning:", 0u) == 0u) {
    return AsterAssetFoundryDiagnosticSeverity::Warning;
  }
  return AsterAssetFoundryDiagnosticSeverity::Info;
}

[[nodiscard]] std::string stripDiagnosticPrefix(const std::string &message) {
  const std::string error_prefix = "error: ";
  const std::string warning_prefix = "warning: ";
  const std::string info_prefix = "info: ";
  if (message.rfind(error_prefix, 0u) == 0u) {
    return message.substr(error_prefix.size());
  }
  if (message.rfind(warning_prefix, 0u) == 0u) {
    return message.substr(warning_prefix.size());
  }
  if (message.rfind(info_prefix, 0u) == 0u) {
    return message.substr(info_prefix.size());
  }
  return message;
}

[[nodiscard]] bool coverageClaimsSignal(const AsterAssetFoundryBuildResult &result,
                                        const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const AsterAssetFoundrySurfaceCoverage &coverage) {
                       return containsText(coverage.claimed_signals, signal);
                     });
}

[[nodiscard]] bool coverageRejectsSignal(const AsterAssetFoundryBuildResult &result,
                                         const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const AsterAssetFoundrySurfaceCoverage &coverage) {
                       return containsText(coverage.rejected_signals, signal);
                     });
}

[[nodiscard]] float visualBriefWeight(const AsterAssetFoundryRecipe &recipe,
                                      const std::string &signal) {
  for (const AsterAssetFoundrySurfaceContract &contract : recipe.surface_contracts) {
    for (const AsterAssetFoundrySignalRule &rule : contract.required_signals) {
      if (rule.id == signal) {
        return rule.weight;
      }
    }
    for (const AsterAssetFoundrySignalRule &rule : contract.forbidden_signals) {
      if (rule.id == signal) {
        return rule.weight;
      }
    }
  }
  return 1.0f;
}

[[nodiscard]] float signalValue(const AsterPipeSurfaceSignals &signals, const std::string &id) {
  if (id == "orange_rust" || id == "corroded_orange_brown_rust") {
    return signals.orange_rust;
  }
  if (id == "black_oxide" || id == "dark_oxide_cavities") {
    return std::max(signals.black_oxide, signals.black_scab);
  }
  if (id == "pitting" || id == "uneven_pitting") {
    return std::max(signals.pit, signals.pit_edge);
  }
  if (id == "cavity_grime" || id == "surface_occlusion") {
    return signals.cavity_grime;
  }
  if (id == "wet_film" || id == "moisture_response") {
    return signals.wet_film;
  }
  if (id == "axial_scratch" || id == "axial_scratches") {
    return signals.axial_scratch;
  }
  if (id == "rim_soot" || id == "open_hollow_rims") {
    return signals.rim_soot;
  }
  if (id == "weld_slag" || id == "raised_weld_rings") {
    return std::max(signals.weld_slag, signals.weld_scorch);
  }
  if (id == "paint_remnant") {
    return signals.paint_remnant;
  }
  if (id == "clean_plastic_surface") {
    return 1.0f - std::max({signals.pit, signals.cavity_grime, signals.black_oxide,
                            signals.orange_rust, signals.axial_scratch});
  }
  if (id == "monochrome_material") {
    const float variation =
        std::max({signals.orange_rust, signals.black_oxide, signals.wet_film,
                  signals.cavity_grime, signals.pit}) -
        std::min({signals.orange_rust, signals.black_oxide, signals.wet_film,
                  signals.cavity_grime, signals.pit});
    return 1.0f - saturate(variation * 1.65f);
  }
  return 0.0f;
}

[[nodiscard]] AsterAssetFoundrySignalSample
sampleSignal(const CpuMesh &mesh, const AsterAssetFoundrySurfaceContract &contract,
             const AsterAssetFoundrySignalRule &rule) {
  AsterAssetFoundrySignalSample out;
  out.id = rule.id;
  if (mesh.vertices.empty()) {
    return out;
  }
  float sum = 0.0f;
  float maximum = 0.0f;
  std::size_t covered = 0u;
  for (const Vertex &vertex : mesh.vertices) {
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = vertex.position,
                                .normal = vertex.normal,
                                .uv = vertex.uv,
                                .detail_scale = contract.detail_scale},
                               contract.layer, contract.layer.edge_polish,
                               contract.layer.height_shading);
    const float value = saturate(signalValue(signals, rule.id));
    sum += value;
    maximum = std::max(maximum, value);
    covered += value >= rule.minimum_coverage ? 1u : 0u;
  }
  out.average = sum / static_cast<float>(mesh.vertices.size());
  out.maximum = maximum;
  out.coverage = static_cast<float>(covered) / static_cast<float>(mesh.vertices.size());
  return out;
}

[[nodiscard]] AsterAssetFoundrySurfaceCoverage
evaluateSurfaceContract(const CpuMesh &mesh, const AsterAssetFoundrySurfaceContract &contract) {
  AsterAssetFoundrySurfaceCoverage coverage;
  coverage.contract_id = contract.id;
  coverage.sample_count = mesh.vertices.size();
  coverage.passed = true;
  coverage.quality_score = 100u;
  if (mesh.vertices.empty()) {
    coverage.passed = false;
    coverage.quality_score = 0u;
    coverage.diagnostics.push_back("error: surface contract has no mesh samples");
    return coverage;
  }
  if (contract.layer.physical_texel_density < contract.min_physical_texel_density) {
    coverage.passed = false;
    coverage.quality_score = std::min(coverage.quality_score, 70u);
    coverage.diagnostics.push_back("warning: physical texel density is below contract floor");
  }
  if (contract.layer.height_normal_coupling < contract.min_height_normal_coupling) {
    coverage.passed = false;
    coverage.quality_score = std::min(coverage.quality_score, 72u);
    coverage.diagnostics.push_back("warning: height/normal coupling is below contract floor");
  }
  if (contract.layer.roughness_height_coupling < contract.min_roughness_height_coupling) {
    coverage.passed = false;
    coverage.quality_score = std::min(coverage.quality_score, 74u);
    coverage.diagnostics.push_back("warning: roughness/height coupling is below contract floor");
  }
  for (const AsterAssetFoundrySignalRule &rule : contract.required_signals) {
    AsterAssetFoundrySignalSample sample = sampleSignal(mesh, contract, rule);
    const bool passed =
        sample.average >= rule.minimum_average && sample.coverage >= rule.minimum_coverage;
    if (passed) {
      coverage.claimed_signals.push_back(rule.id);
    } else {
      coverage.passed = false;
      coverage.quality_score =
          std::min(coverage.quality_score,
                   static_cast<std::uint32_t>(std::max(0.0f, 86.0f - rule.weight * 16.0f)));
      coverage.diagnostics.push_back("warning: required surface signal below threshold: " +
                                     rule.id);
    }
    coverage.signals.push_back(std::move(sample));
  }
  for (const AsterAssetFoundrySignalRule &rule : contract.forbidden_signals) {
    AsterAssetFoundrySignalSample sample = sampleSignal(mesh, contract, rule);
    const bool present =
        sample.average >= rule.minimum_average || sample.coverage >= rule.minimum_coverage;
    if (present) {
      coverage.passed = false;
      coverage.quality_score =
          std::min(coverage.quality_score,
                   static_cast<std::uint32_t>(std::max(0.0f, 72.0f - rule.weight * 18.0f)));
      coverage.diagnostics.push_back("error: forbidden surface signal present: " + rule.id);
    } else {
      coverage.rejected_signals.push_back(rule.id);
    }
    coverage.signals.push_back(std::move(sample));
  }
  return coverage;
}

[[nodiscard]] CpuMesh decimatedLod(const CpuMesh &mesh, const std::uint32_t level) {
  if (level == 0u) {
    return mesh;
  }
  AssetModifierStack stack;
  stack.asset_id = "generated-lod";
  stack.source_provenance_id = "aster.asset_foundry.lod";
  stack.modifiers.push_back({.id = "lod.decimate." + std::to_string(level),
                             .kind = AssetModifierKind::Decimate,
                             .amount = level == 1u ? 0.52f : 0.28f,
                             .seed = 200u + level,
                             .count = 1u,
                             .creative_variant_tags = {"factory-lod"}});
  return applyAssetModifierStack(mesh, stack).mesh;
}

[[nodiscard]] std::vector<CpuMesh> makeLods(const AsterAssetFoundryRecipe &recipe,
                                            const CpuMesh &mesh) {
  if (!recipe.authored_lods.empty()) {
    return recipe.authored_lods;
  }
  return {decimatedLod(mesh, 0u), decimatedLod(mesh, 1u), decimatedLod(mesh, 2u)};
}

[[nodiscard]] ProceduralSurfaceLayer pipeLayerFromSpec(const AsterPipeAssetSpec &spec) {
  return {.macro_variation = 0.90f,
          .micro_normal_strength = 0.62f,
          .roughness_variation = 0.92f,
          .physical_texel_density = 1024.0f,
          .height_normal_coupling = 0.92f,
          .roughness_height_coupling = 0.74f,
          .macro_frequency_breakup = 0.48f,
          .micro_frequency_breakup = 0.66f,
          .wetness = spec.wetness_strength,
          .height_shading = 0.46f,
          .pitting_density = spec.pitting_density,
          .pitting_depth = spec.pitting_depth,
          .oxide_layering = spec.oxide_layering,
          .cavity_grime = spec.cavity_grime_strength,
          .edge_polish = spec.edge_polish_strength,
          .weld_heat_tint = spec.weld_heat_tint_strength,
          .axial_scratches = spec.axial_scratch_strength,
          .wet_streaks = static_cast<float>(std::max(spec.wet_streak_count, 0)) / 7.0f,
          .rust_bloom = spec.rust_bloom_strength,
          .black_scab = spec.black_scab_strength,
          .paint_remnant = spec.paint_remnant_strength,
          .weld_slag = spec.weld_slag_strength,
          .rim_soot = spec.rim_soot_strength};
}

[[nodiscard]] AsterAssetFoundryPhysicsProxy proxyFromPipe(
    const AsterPipeCollisionProxy &pipe_proxy, const AsterPipeAssetSpec &spec) {
  const float wetness = saturate(spec.wetness_strength);
  const float rust = saturate(spec.rust_strength);
  return {.id = "physics.pipe.runtime-bounds",
          .label = "Rusted pipe runtime bounds proxy",
          .kind = AsterAssetFoundryPhysicsProxyKind::BoundsBox,
          .center = pipe_proxy.center,
          .half_extents = pipe_proxy.half_extents,
          .radius = pipe_proxy.radius,
          .length = pipe_proxy.length,
          .triangle_budget = pipe_proxy.triangle_budget,
          .covers_render_bounds = pipe_proxy.covers_render_bounds,
          .query_enabled = true,
          .material = {.friction = std::clamp(0.86f - wetness * 0.28f + rust * 0.08f, 0.35f,
                                              1.05f),
                       .restitution = 0.01f + wetness * 0.02f},
          .filter = {.layer_bits = 1u, .collides_with = 0xffffffffu}};
}

[[nodiscard]] std::vector<AsterAssetFoundryStage> pipeStages(const AsterPipeAssetSpec &spec) {
  AsterAssetFoundryStage source;
  source.id = "stage.pipe.source";
  source.kind = AsterAssetFoundryStageKind::SourceGeometry;
  source.label = "Aster pipe source assembly";
  source.minimum_quality = 90u;
  source.creative_variant_tags = {"reference-silhouette"};

  AsterAssetFoundryStage polish;
  polish.id = "stage.pipe.modifier-stack";
  polish.kind = AsterAssetFoundryStageKind::ModifierStack;
  polish.label = "Factory modifier pass for weld/rim polish";
  polish.depends_on = {source.id};
  polish.minimum_quality = 75u;
  polish.modifier_stack.asset_id = spec.asset_id;
  polish.modifier_stack.source_provenance_id = "aster.pipe.reference";
  polish.modifier_stack.modifiers = {
      {.id = "factory.weighted-normal",
       .kind = AssetModifierKind::WeightedNormal,
       .amount = 0.0f,
       .creative_variant_tags = {"rounded-rim-normals"}},
      {.id = "factory.micro-displace",
       .kind = AssetModifierKind::Displace,
       .amount = std::max(spec.pitting_depth * 0.35f, 0.00035f),
       .seed = spec.seed ^ 0xA57E0002u,
       .creative_variant_tags = {"micro-pitting"}}};

  AsterAssetFoundryStage surface;
  surface.id = "stage.pipe.surface-contract";
  surface.kind = AsterAssetFoundryStageKind::SurfaceContract;
  surface.label = "Corroded pipe visual brief contract";
  surface.depends_on = {polish.id};
  surface.surface_contract_id = "surface.pipe.corrosion";
  surface.minimum_quality = 82u;

  AsterAssetFoundryStage lod;
  lod.id = "stage.pipe.lod";
  lod.kind = AsterAssetFoundryStageKind::LodRecipe;
  lod.label = "Factory generated LOD chain";
  lod.depends_on = {surface.id};
  lod.minimum_quality = 70u;

  AsterAssetFoundryStage physics;
  physics.id = "stage.pipe.physics";
  physics.kind = AsterAssetFoundryStageKind::PhysicsProxy;
  physics.label = "Runtime collision/query proxy";
  physics.depends_on = {lod.id};
  physics.physics_proxy_id = "physics.pipe.runtime-bounds";
  physics.minimum_quality = 80u;

  AsterAssetFoundryStage quality;
  quality.id = "stage.pipe.quality";
  quality.kind = AsterAssetFoundryStageKind::QualityGate;
  quality.label = "Visual brief and cook contract gate";
  quality.depends_on = {physics.id};
  quality.minimum_quality = 88u;

  AsterAssetFoundryStage package;
  package.id = "stage.pipe.package";
  package.kind = AsterAssetFoundryStageKind::Package;
  package.label = "Asset graph package handoff";
  package.depends_on = {quality.id};
  package.minimum_quality = 80u;
  return {source, polish, surface, lod, physics, quality, package};
}

} // namespace

std::string_view asterAssetFoundryDiagnosticSeverityName(
    const AsterAssetFoundryDiagnosticSeverity severity) noexcept {
  switch (severity) {
  case AsterAssetFoundryDiagnosticSeverity::Info:
    return "info";
  case AsterAssetFoundryDiagnosticSeverity::Warning:
    return "warning";
  case AsterAssetFoundryDiagnosticSeverity::Error:
    return "error";
  }
  return "unknown";
}

std::string_view asterAssetFoundryStageKindName(
    const AsterAssetFoundryStageKind kind) noexcept {
  switch (kind) {
  case AsterAssetFoundryStageKind::SourceGeometry:
    return "source-geometry";
  case AsterAssetFoundryStageKind::ModifierStack:
    return "modifier-stack";
  case AsterAssetFoundryStageKind::SurfaceContract:
    return "surface-contract";
  case AsterAssetFoundryStageKind::LodRecipe:
    return "lod-recipe";
  case AsterAssetFoundryStageKind::PhysicsProxy:
    return "physics-proxy";
  case AsterAssetFoundryStageKind::QualityGate:
    return "quality-gate";
  case AsterAssetFoundryStageKind::Package:
    return "package";
  }
  return "unknown";
}

std::string_view asterAssetFoundryPhysicsProxyKindName(
    const AsterAssetFoundryPhysicsProxyKind kind) noexcept {
  switch (kind) {
  case AsterAssetFoundryPhysicsProxyKind::BoundsBox:
    return "bounds-box";
  case AsterAssetFoundryPhysicsProxyKind::RadialCapsule:
    return "radial-capsule";
  case AsterAssetFoundryPhysicsProxyKind::TriangleMesh:
    return "triangle-mesh";
  }
  return "unknown";
}

AsterAssetFoundryDagReport evaluateAsterAssetFoundryStageDag(
    const AsterAssetFoundryRecipe &recipe) {
  AsterAssetFoundryDagReport report;
  std::map<std::string, std::size_t> index_by_id;
  report.declared_order.reserve(recipe.stages.size());
  for (std::size_t index = 0u; index < recipe.stages.size(); ++index) {
    const std::string &id = recipe.stages[index].id;
    report.declared_order.push_back(id);
    if (!id.empty() && index_by_id.find(id) == index_by_id.end()) {
      index_by_id[id] = index;
    }
  }

  for (std::size_t index = 0u; index < recipe.stages.size(); ++index) {
    const AsterAssetFoundryStage &stage = recipe.stages[index];
    for (const std::string &dependency : stage.depends_on) {
      const auto found = index_by_id.find(dependency);
      if (found == index_by_id.end()) {
        report.missing_dependencies.push_back(stage.id + " <- " + dependency);
        continue;
      }
      if (found->second > index) {
        report.order_violations.push_back(stage.id + " <- " + dependency);
      }
    }
  }

  std::vector<std::uint8_t> state(recipe.stages.size(), 0u);
  const auto visit = [&](auto &&self, const std::size_t index) -> void {
    if (state[index] == 2u) {
      return;
    }
    if (state[index] == 1u) {
      report.acyclic = false;
      return;
    }
    state[index] = 1u;
    const AsterAssetFoundryStage &stage = recipe.stages[index];
    for (const std::string &dependency : stage.depends_on) {
      const auto found = index_by_id.find(dependency);
      if (found == index_by_id.end()) {
        continue;
      }
      if (state[found->second] == 1u) {
        report.acyclic = false;
        report.cycle_edges.push_back(stage.id + " <- " + dependency);
        continue;
      }
      self(self, found->second);
    }
    state[index] = 2u;
    report.topological_order.push_back(stage.id);
  };
  for (std::size_t index = 0u; index < recipe.stages.size(); ++index) {
    visit(visit, index);
  }
  report.ready = report.acyclic && report.missing_dependencies.empty() &&
                 report.order_violations.empty();
  return report;
}

AsterAssetFoundryProductionSession makeAsterAssetFoundryProductionSession(
    const AsterAssetFoundryRecipe &recipe, std::string stable_recipe_hash) {
  if (stable_recipe_hash.empty()) {
    stable_recipe_hash = stableAsterAssetFoundryRecipeHash(recipe);
  }
  std::uint64_t preview_hash = fnvSeed();
  appendHash(preview_hash, stable_recipe_hash);
  for (const AsterAssetFoundryProofArtifact &artifact : recipe.proof_artifacts) {
    appendHash(preview_hash, artifact.id);
    appendHash(preview_hash, artifact.role);
    appendHash(preview_hash, artifact.path.string());
    appendHash(preview_hash, artifact.hash);
    for (const std::string &tag : artifact.signal_tags) {
      appendHash(preview_hash, tag);
    }
  }
  for (const std::string &claim : recipe.visual_brief_claims) {
    appendHash(preview_hash, claim);
  }
  for (const std::string &rejection : recipe.visual_brief_rejections) {
    appendHash(preview_hash, rejection);
  }

  AsterAssetFoundryProductionSession session = recipe.production_session;
  if (session.session_id.empty()) {
    session.session_id = "asset-production:" + recipe.asset_id;
  }
  session.stable_recipe_hash = std::move(stable_recipe_hash);
  if (session.preview_artifact_hash.empty()) {
    session.preview_artifact_hash = hex64(preview_hash, "aster-preview-0x");
  }
  if (session.quality_gate.empty()) {
    session.quality_gate = "foundry-build";
  }
  if (session.cook_steps.empty()) {
    session.cook_steps = {"recipe-validate",
                          "stage-dag-evaluate",
                          "surface-signal-sample",
                          "lod-cook",
                          "physics-proxy-cook",
                          "package-handoff"};
  }
  if (recipe.proof_artifacts.empty()) {
    session.runtime_fallbacks.push_back("missing-proof-artifact");
  }
  if (recipe.authored_lods.empty()) {
    session.runtime_fallbacks.push_back("generated-lod-chain");
  }
  return session;
}

std::vector<AsterAssetFoundryQualityDiagnostic>
validateAsterAssetFoundryRecipe(const AsterAssetFoundryRecipe &recipe) {
  std::vector<AsterAssetFoundryQualityDiagnostic> diagnostics;
  const auto push = [&](const AsterAssetFoundryDiagnosticSeverity severity,
                       std::string category, std::string stage_id, std::string message) {
    diagnostics.push_back({.severity = severity,
                           .category = std::move(category),
                           .stage_id = std::move(stage_id),
                           .message = std::move(message)});
  };

  if (recipe.asset_id.empty()) {
    push(AsterAssetFoundryDiagnosticSeverity::Error, "identity", {},
         "factory recipe is missing an asset id");
  }
  if (recipe.source_mesh.vertices.empty() || recipe.source_mesh.indices.empty()) {
    push(AsterAssetFoundryDiagnosticSeverity::Error, "source", {},
         "factory recipe has no source render mesh");
  }
  if (recipe.stages.empty()) {
    push(AsterAssetFoundryDiagnosticSeverity::Error, "stage", {},
         "factory recipe declares no stages");
  }
  if (recipe.visual_brief_claims.empty()) {
    push(AsterAssetFoundryDiagnosticSeverity::Warning, "visual-brief", {},
         "factory recipe declares no visual brief claims");
  }
  if (recipe.visual_brief_rejections.empty()) {
    push(AsterAssetFoundryDiagnosticSeverity::Warning, "visual-brief", {},
         "factory recipe declares no forbidden-signal rejections");
  }

  std::vector<std::string> stage_ids;
  stage_ids.reserve(recipe.stages.size());
  for (const AsterAssetFoundryStage &stage : recipe.stages) {
    if (stage.id.empty()) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "stage", {},
           "factory stage is missing an id");
      continue;
    }
    if (duplicateId(stage_ids, stage.id)) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "stage", stage.id,
           "factory stage id is duplicated");
    }
    stage_ids.push_back(stage.id);
  }
  for (const AsterAssetFoundryStage &stage : recipe.stages) {
    for (const std::string &dependency : stage.depends_on) {
      if (!stageExists(recipe, dependency)) {
        push(AsterAssetFoundryDiagnosticSeverity::Error, "dependency", stage.id,
             "stage depends on unknown stage: " + dependency);
      }
    }
    if (stage.kind == AsterAssetFoundryStageKind::SurfaceContract &&
        findSurfaceContract(recipe, stage.surface_contract_id) == nullptr) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "surface", stage.id,
           "stage references a missing surface contract: " + stage.surface_contract_id);
    }
    if (stage.kind == AsterAssetFoundryStageKind::PhysicsProxy &&
        findPhysicsProxy(recipe, stage.physics_proxy_id) == nullptr) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "physics", stage.id,
           "stage references a missing physics proxy: " + stage.physics_proxy_id);
    }
  }
  const AsterAssetFoundryDagReport dag = evaluateAsterAssetFoundryStageDag(recipe);
  for (const std::string &edge : dag.order_violations) {
    push(AsterAssetFoundryDiagnosticSeverity::Error, "dependency", {},
         "stage dependency is declared after dependent: " + edge);
  }
  for (const std::string &edge : dag.cycle_edges) {
    push(AsterAssetFoundryDiagnosticSeverity::Error, "dependency", {},
         "stage dependency cycle detected: " + edge);
  }

  std::vector<std::string> contract_ids;
  contract_ids.reserve(recipe.surface_contracts.size());
  for (const AsterAssetFoundrySurfaceContract &contract : recipe.surface_contracts) {
    if (contract.id.empty()) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "surface", {},
           "surface contract is missing an id");
      continue;
    }
    if (duplicateId(contract_ids, contract.id)) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "surface", contract.id,
           "surface contract id is duplicated");
    }
    contract_ids.push_back(contract.id);
    if (contract.required_signals.empty()) {
      push(AsterAssetFoundryDiagnosticSeverity::Warning, "surface", contract.id,
           "surface contract has no required signals");
    }
  }

  std::vector<std::string> proxy_ids;
  proxy_ids.reserve(recipe.physics_proxies.size());
  for (const AsterAssetFoundryPhysicsProxy &proxy : recipe.physics_proxies) {
    if (proxy.id.empty()) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "physics", {},
           "physics proxy is missing an id");
      continue;
    }
    if (duplicateId(proxy_ids, proxy.id)) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "physics", proxy.id,
           "physics proxy id is duplicated");
    }
    proxy_ids.push_back(proxy.id);
    if (proxy.kind == AsterAssetFoundryPhysicsProxyKind::BoundsBox &&
        (proxy.half_extents.x <= 0.0f || proxy.half_extents.y <= 0.0f ||
         proxy.half_extents.z <= 0.0f)) {
      push(AsterAssetFoundryDiagnosticSeverity::Error, "physics", proxy.id,
           "bounds proxy has non-positive half extents");
    }
    if (proxy.kind == AsterAssetFoundryPhysicsProxyKind::TriangleMesh &&
        proxy.triangle_budget == 0u) {
      push(AsterAssetFoundryDiagnosticSeverity::Warning, "physics", proxy.id,
           "triangle mesh proxy has no declared triangle budget");
    }
  }
  return diagnostics;
}

std::vector<AsterAssetFoundryLodSummary>
summarizeAsterAssetFoundryLods(const AsterAssetFoundryBuildResult &result) {
  std::vector<AsterAssetFoundryLodSummary> summaries;
  if (result.lods.empty()) {
    const float triangles = std::max(static_cast<float>(result.mesh.indices.size() / 3u), 1.0f);
    summaries.push_back({.level = 0u,
                         .vertices = result.mesh.vertices.size(),
                         .indices = result.mesh.indices.size(),
                         .triangle_ratio = triangles / triangles,
                         .recommended_screen_coverage = 1.0f,
                         .generated_by_factory = false});
    return summaries;
  }
  const std::vector<CpuMesh> &lods = result.lods;
  const float lod0_triangles = std::max(static_cast<float>(lods.front().indices.size() / 3u),
                                        1.0f);
  summaries.reserve(lods.size());
  for (std::size_t index = 0u; index < lods.size(); ++index) {
    const CpuMesh &lod = lods[index];
    const float triangles = static_cast<float>(lod.indices.size() / 3u);
    const float screen_coverage =
        index == 0u ? 1.0f : (index == 1u ? 0.38f : std::max(0.08f, 0.18f / index));
    summaries.push_back({.level = static_cast<std::uint32_t>(index),
                         .vertices = lod.vertices.size(),
                         .indices = lod.indices.size(),
                         .triangle_ratio = triangles / lod0_triangles,
                         .recommended_screen_coverage = screen_coverage,
                         .generated_by_factory = true});
  }
  return summaries;
}

std::vector<AsterAssetFoundryPhysicsProxySummary>
summarizeAsterAssetFoundryPhysicsProxies(const AsterAssetFoundryRecipe &recipe) {
  std::vector<AsterAssetFoundryPhysicsProxySummary> summaries;
  summaries.reserve(recipe.physics_proxies.size());
  for (const AsterAssetFoundryPhysicsProxy &proxy : recipe.physics_proxies) {
    summaries.push_back(
        {.id = proxy.id,
         .shape = std::string(asterAssetFoundryPhysicsProxyKindName(proxy.kind)),
         .center = proxy.center,
         .half_extents = proxy.half_extents,
         .radius = proxy.radius,
         .length = proxy.length,
         .triangle_budget = proxy.triangle_budget,
         .friction = proxy.material.friction,
         .covers_render_bounds = proxy.covers_render_bounds,
         .query_enabled = proxy.query_enabled});
  }
  return summaries;
}

std::vector<AsterAssetFoundrySurfaceSignalSummary>
summarizeAsterAssetFoundrySurfaceSignals(const AsterAssetFoundryBuildResult &result) {
  std::vector<AsterAssetFoundrySurfaceSignalSummary> summaries;
  for (const AsterAssetFoundrySurfaceCoverage &coverage : result.surface_coverages) {
    summaries.reserve(summaries.size() + coverage.signals.size());
    for (const AsterAssetFoundrySignalSample &sample : coverage.signals) {
      const bool claimed = containsText(coverage.claimed_signals, sample.id);
      const bool rejected = containsText(coverage.rejected_signals, sample.id);
      summaries.push_back({.signal = sample.id,
                           .average = sample.average,
                           .coverage = sample.coverage,
                           .status = claimed ? "claimed" : (rejected ? "rejected" : "needs-work"),
                           .source = coverage.contract_id});
    }
  }
  return summaries;
}

std::vector<AsterAssetFoundryVisualBriefRow>
makeAsterAssetFoundryVisualBriefRows(const AsterAssetFoundryRecipe &recipe,
                                     const AsterAssetFoundryBuildResult &result) {
  std::vector<AsterAssetFoundryVisualBriefRow> rows;
  rows.reserve(recipe.visual_brief_claims.size() + recipe.visual_brief_rejections.size());
  for (const std::string &claim : recipe.visual_brief_claims) {
    const bool direct_claim = containsText(result.visual_brief_claims, claim);
    const bool surface_claim = coverageClaimsSignal(result, claim);
    const bool silhouette_claim =
        claim == "reference_silhouette" && containsText(recipe.creative_variant_tags,
                                                        "reference-silhouette");
    rows.push_back({.signal = claim,
                    .status = (direct_claim || surface_claim || silhouette_claim) ? "claimed"
                                                                                  : "needs-work",
                    .source = surface_claim ? "surface-contract" : "visual-brief",
                    .weight = visualBriefWeight(recipe, claim)});
  }
  for (const std::string &rejection : recipe.visual_brief_rejections) {
    const bool explicit_rejection = containsText(result.visual_brief_rejections, rejection);
    const bool surface_rejection = coverageRejectsSignal(result, rejection);
    rows.push_back({.signal = rejection,
                    .status = (explicit_rejection || surface_rejection) ? "rejected"
                                                                        : "needs-review",
                    .source = surface_rejection ? "surface-contract" : "visual-brief",
                    .weight = visualBriefWeight(recipe, rejection)});
  }
  return rows;
}

AsterAssetFoundryRecipeAudit auditAsterAssetFoundryBuild(
    const AsterAssetFoundryRecipe &recipe, const AsterAssetFoundryBuildResult &result) {
  AsterAssetFoundryRecipeAudit audit;
  audit.asset_id = recipe.asset_id;
  audit.stable_recipe_hash = result.stable_recipe_hash.empty()
                                 ? stableAsterAssetFoundryRecipeHash(recipe)
                                 : result.stable_recipe_hash;
  audit.quality_score = result.quality_score;
  audit.production_ready = result.production_ready;
  audit.lod_summary = summarizeAsterAssetFoundryLods(result);
  audit.visual_brief_rows = makeAsterAssetFoundryVisualBriefRows(recipe, result);
  audit.dag = evaluateAsterAssetFoundryStageDag(recipe);
  audit.production_session = result.production_session.session_id.empty()
                                 ? makeAsterAssetFoundryProductionSession(recipe,
                                                                         audit.stable_recipe_hash)
                                 : result.production_session;
  audit.diagnostics = validateAsterAssetFoundryRecipe(recipe);

  audit.stage_order.reserve(recipe.stages.size());
  std::vector<std::string> completed;
  completed.reserve(recipe.stages.size());
  for (const AsterAssetFoundryStage &stage : recipe.stages) {
    audit.stage_order.push_back(stage.id);
    for (const std::string &dependency : stage.depends_on) {
      if (!containsText(completed, dependency)) {
        audit.missing_dependencies.push_back(stage.id + " <- " + dependency);
      }
    }
    completed.push_back(stage.id);
  }
  for (const AsterAssetFoundrySurfaceContract &contract : recipe.surface_contracts) {
    audit.surface_contract_ids.push_back(contract.id);
  }
  for (const AsterAssetFoundryPhysicsProxy &proxy : recipe.physics_proxies) {
    audit.physics_proxy_ids.push_back(proxy.id);
  }
  for (const AsterAssetFoundryStageReport &stage_report : result.stage_reports) {
    for (const std::string &message : stage_report.diagnostics) {
      audit.diagnostics.push_back(
          {.severity = severityFromMessage(message),
           .category = std::string(asterAssetFoundryStageKindName(stage_report.kind)),
           .stage_id = stage_report.id,
           .message = stripDiagnosticPrefix(message)});
    }
  }
  for (const std::string &message : result.diagnostics) {
    audit.diagnostics.push_back({.severity = severityFromMessage(message),
                                 .category = "build",
                                 .stage_id = {},
                                 .message = stripDiagnosticPrefix(message)});
  }
  return audit;
}

std::vector<std::string>
describeAsterAssetFoundryAudit(const AsterAssetFoundryRecipeAudit &audit) {
  std::vector<std::string> lines;
  lines.reserve(4u + audit.stage_order.size() + audit.lod_summary.size() +
                audit.visual_brief_rows.size() + audit.diagnostics.size());

  std::ostringstream header;
  header << "asset=" << audit.asset_id << " hash=" << audit.stable_recipe_hash
         << " quality=" << audit.quality_score
         << " ready=" << (audit.production_ready ? "true" : "false");
  lines.push_back(header.str());

  for (std::size_t index = 0u; index < audit.stage_order.size(); ++index) {
    std::ostringstream line;
    line << "stage[" << index << "]=" << audit.stage_order[index];
    lines.push_back(line.str());
  }
  for (const std::string &dependency : audit.missing_dependencies) {
    lines.push_back("missing_dependency=" + dependency);
  }
  for (const std::string &stage : audit.dag.topological_order) {
    lines.push_back("dag.topological_stage=" + stage);
  }
  if (!audit.production_session.session_id.empty()) {
    lines.push_back("production_session=" + audit.production_session.session_id +
                    " gate=" + audit.production_session.quality_gate +
                    " preview=" + audit.production_session.preview_artifact_hash);
  }
  for (const AsterAssetFoundryLodSummary &lod : audit.lod_summary) {
    std::ostringstream line;
    line << "lod[" << lod.level << "] vertices=" << lod.vertices
         << " indices=" << lod.indices << " ratio=" << std::fixed
         << std::setprecision(3) << lod.triangle_ratio
         << " screen=" << lod.recommended_screen_coverage;
    lines.push_back(line.str());
  }
  for (const AsterAssetFoundryVisualBriefRow &row : audit.visual_brief_rows) {
    std::ostringstream line;
    line << "visual_brief " << row.signal << "=" << row.status
         << " source=" << row.source << " weight=" << std::fixed
         << std::setprecision(2) << row.weight;
    lines.push_back(line.str());
  }
  for (const AsterAssetFoundryQualityDiagnostic &diagnostic : audit.diagnostics) {
    std::ostringstream line;
    line << asterAssetFoundryDiagnosticSeverityName(diagnostic.severity) << ":"
         << diagnostic.category;
    if (!diagnostic.stage_id.empty()) {
      line << ":" << diagnostic.stage_id;
    }
    line << ":" << diagnostic.message;
    lines.push_back(line.str());
  }
  return lines;
}

std::string stableAsterAssetFoundryRecipeHash(const AsterAssetFoundryRecipe &recipe) {
  std::uint64_t hash = fnvSeed();
  appendHash(hash, recipe.asset_id);
  appendHash(hash, recipe.label);
  appendHash(hash, recipe.source_provenance_id);
  appendHash(hash, recipe.source_mesh.vertices.size());
  appendHash(hash, recipe.source_mesh.indices.size());
  for (const AsterAssetFoundryStage &stage : recipe.stages) {
    appendHash(hash, stage.id);
    appendHash(hash, asterAssetFoundryStageKindName(stage.kind));
    appendHash(hash, stage.minimum_quality);
    appendHash(hash, stage.enabled ? 1u : 0u);
    appendHash(hash, stage.surface_contract_id);
    appendHash(hash, stage.physics_proxy_id);
    appendHash(hash, stableAssetModifierStackId(stage.modifier_stack));
    for (const std::string &dependency : stage.depends_on) {
      appendHash(hash, dependency);
    }
    for (const std::string &tag : stage.creative_variant_tags) {
      appendHash(hash, tag);
    }
  }
  for (const AsterAssetFoundrySurfaceContract &contract : recipe.surface_contracts) {
    appendHash(hash, contract.id);
    appendHash(hash, contract.material_slot);
    appendHash(hash, contract.detail_scale);
    appendHash(hash, contract.min_physical_texel_density);
    appendHash(hash, contract.layer);
    for (const AsterAssetFoundrySignalRule &rule : contract.required_signals) {
      appendHash(hash, rule.id);
      appendHash(hash, rule.minimum_average);
      appendHash(hash, rule.minimum_coverage);
      appendHash(hash, rule.weight);
    }
    for (const AsterAssetFoundrySignalRule &rule : contract.forbidden_signals) {
      appendHash(hash, rule.id);
      appendHash(hash, rule.minimum_average);
      appendHash(hash, rule.minimum_coverage);
      appendHash(hash, rule.weight);
    }
  }
  for (const AsterAssetFoundryPhysicsProxy &proxy : recipe.physics_proxies) {
    appendHash(hash, proxy.id);
    appendHash(hash, asterAssetFoundryPhysicsProxyKindName(proxy.kind));
    appendHash(hash, proxy.center);
    appendHash(hash, proxy.half_extents);
    appendHash(hash, proxy.radius);
    appendHash(hash, proxy.length);
    appendHash(hash, proxy.triangle_budget);
    appendHash(hash, proxy.covers_render_bounds ? 1u : 0u);
    appendHash(hash, proxy.material.friction);
    appendHash(hash, proxy.material.restitution);
  }
  for (const std::string &claim : recipe.visual_brief_claims) {
    appendHash(hash, claim);
  }
  for (const std::string &rejection : recipe.visual_brief_rejections) {
    appendHash(hash, rejection);
  }
  for (const std::string &tag : recipe.creative_variant_tags) {
    appendHash(hash, tag);
  }
  for (const AsterAssetFoundryProofArtifact &artifact : recipe.proof_artifacts) {
    appendHash(hash, artifact.id);
    appendHash(hash, artifact.role);
    appendHash(hash, artifact.path.string());
    appendHash(hash, artifact.kind);
    appendHash(hash, artifact.hash);
    appendHash(hash, artifact.width);
    appendHash(hash, artifact.height);
    for (const std::string &tag : artifact.signal_tags) {
      appendHash(hash, tag);
    }
  }
  appendHash(hash, recipe.production_session.session_id);
  appendHash(hash, recipe.production_session.quality_gate);
  for (const std::string &step : recipe.production_session.cook_steps) {
    appendHash(hash, step);
  }
  for (const std::string &fallback : recipe.production_session.runtime_fallbacks) {
    appendHash(hash, fallback);
  }
  return hex64(hash, "aster-foundry-0x");
}

PhysicsBodyDesc asterAssetFoundryPhysicsBodyDesc(
    const AsterAssetFoundryPhysicsProxy &proxy, std::shared_ptr<const CpuMesh> mesh) {
  PhysicsBodyDesc desc;
  desc.type = PhysicsBodyType::Static;
  desc.position = proxy.center;
  desc.half_extents = proxy.half_extents;
  desc.radius = std::max(proxy.radius, 0.001f);
  desc.material = proxy.material;
  desc.filter = proxy.filter;
  desc.filter.query_enabled = proxy.query_enabled;
  desc.allow_sleep = false;
  switch (proxy.kind) {
  case AsterAssetFoundryPhysicsProxyKind::BoundsBox:
    desc.shape = PhysicsShapeType::Box;
    break;
  case AsterAssetFoundryPhysicsProxyKind::RadialCapsule:
    desc.shape = PhysicsShapeType::Capsule;
    desc.half_extents.y = std::max(proxy.length * 0.5f, proxy.half_extents.y);
    break;
  case AsterAssetFoundryPhysicsProxyKind::TriangleMesh:
    desc.shape = PhysicsShapeType::TriangleMesh;
    desc.mesh = std::move(mesh);
    desc.mesh_double_sided = true;
    break;
  }
  return desc;
}

AsterAssetFoundryBuildResult buildAsterAssetFoundryRecipe(
    const AsterAssetFoundryRecipe &recipe) {
  AsterAssetFoundryBuildResult result;
  result.asset_id = recipe.asset_id;
  result.stable_recipe_hash = stableAsterAssetFoundryRecipeHash(recipe);
  result.mesh = recipe.source_mesh;
  result.material = recipe.material;
  result.dependency_edges = recipe.dependency_edges;
  result.visual_brief_claims = recipe.visual_brief_claims;
  result.visual_brief_rejections = recipe.visual_brief_rejections;
  result.proof_artifacts = recipe.proof_artifacts;
  result.production_session =
      makeAsterAssetFoundryProductionSession(recipe, result.stable_recipe_hash);
  result.creative_variant_tags = recipe.creative_variant_tags;
  result.quality_score = 100u;
  result.production_ready = true;

  std::vector<std::string> completed;
  for (const AsterAssetFoundryStage &stage : recipe.stages) {
    AsterAssetFoundryStageReport report;
    report.id = stage.id;
    report.kind = stage.kind;
    report.executed = stage.enabled;
    report.passed = true;
    report.quality_score = 100u;
    report.input_vertices = result.mesh.vertices.size();
    report.input_indices = result.mesh.indices.size();
    report.creative_variant_tags = stage.creative_variant_tags;
    for (const std::string &dependency : stage.depends_on) {
      report.dependency_edges.push_back("stage:" + dependency + " -> stage:" + stage.id);
    }
    if (!stage.enabled) {
      report.diagnostics.push_back("info: stage disabled");
      result.stage_reports.push_back(std::move(report));
      continue;
    }
    for (const std::string &dependency : stage.depends_on) {
      if (!containsId(completed, dependency)) {
        report.passed = false;
        report.quality_score = 0u;
        report.diagnostics.push_back("error: stage dependency has not completed: " + dependency);
      }
    }
    if (report.passed) {
      switch (stage.kind) {
      case AsterAssetFoundryStageKind::SourceGeometry:
        if (result.mesh.vertices.empty() || result.mesh.indices.empty()) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: source geometry is empty");
        }
        break;
      case AsterAssetFoundryStageKind::ModifierStack:
        if (!stage.modifier_stack.modifiers.empty()) {
          AssetModifierStackResult modified =
              applyAssetModifierStack(result.mesh, stage.modifier_stack);
          result.mesh = std::move(modified.mesh);
          report.quality_score = modified.report.quality_score;
          report.diagnostics.insert(report.diagnostics.end(), modified.report.diagnostics.begin(),
                                    modified.report.diagnostics.end());
          for (const std::string &edge : modified.report.creative_variant_tags) {
            result.dependency_edges.push_back("modifier:" + stage.id + " -> tag:" + edge);
            report.creative_variant_tags.push_back(edge);
            result.creative_variant_tags.push_back(edge);
          }
        }
        break;
      case AsterAssetFoundryStageKind::SurfaceContract: {
        const AsterAssetFoundrySurfaceContract *contract =
            findSurfaceContract(recipe, stage.surface_contract_id);
        if (contract == nullptr) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: missing surface contract: " +
                                       stage.surface_contract_id);
          break;
        }
        AsterAssetFoundrySurfaceCoverage coverage =
            evaluateSurfaceContract(result.mesh, *contract);
        report.passed = coverage.passed;
        report.quality_score = coverage.quality_score;
        report.diagnostics.insert(report.diagnostics.end(), coverage.diagnostics.begin(),
                                  coverage.diagnostics.end());
        result.surface_coverages.push_back(std::move(coverage));
        break;
      }
      case AsterAssetFoundryStageKind::LodRecipe:
        result.lods = makeLods(recipe, result.mesh);
        if (result.lods.size() < 3u) {
          report.passed = false;
          report.quality_score = 55u;
          report.diagnostics.push_back("warning: factory recipe produced fewer than three LODs");
        }
        break;
      case AsterAssetFoundryStageKind::PhysicsProxy: {
        const AsterAssetFoundryPhysicsProxy *proxy =
            findPhysicsProxy(recipe, stage.physics_proxy_id);
        if (proxy == nullptr) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: missing physics proxy: " + stage.physics_proxy_id);
          break;
        }
        std::shared_ptr<const CpuMesh> mesh;
        if (proxy->kind == AsterAssetFoundryPhysicsProxyKind::TriangleMesh) {
          mesh = std::make_shared<const CpuMesh>(result.mesh);
        }
        result.physics_bodies.push_back(asterAssetFoundryPhysicsBodyDesc(*proxy, std::move(mesh)));
        if (!proxy->covers_render_bounds && proxy->kind == AsterAssetFoundryPhysicsProxyKind::BoundsBox) {
          report.quality_score = 82u;
          report.diagnostics.push_back("warning: bounds proxy does not claim render coverage");
        }
        break;
      }
      case AsterAssetFoundryStageKind::QualityGate:
        if (result.visual_brief_claims.empty()) {
          report.passed = false;
          report.quality_score = 68u;
          report.diagnostics.push_back("warning: quality gate has no visual brief claims");
        }
        if (result.visual_brief_rejections.empty()) {
          report.passed = false;
          report.quality_score = std::min(report.quality_score, 70u);
          report.diagnostics.push_back("warning: quality gate has no rejected forbidden signals");
        }
        break;
      case AsterAssetFoundryStageKind::Package:
        result.dependency_edges.push_back("asset_foundry:" + result.stable_recipe_hash +
                                          " -> package:" + recipe.asset_id);
        break;
      }
    }
    report.output_vertices = result.mesh.vertices.size();
    report.output_indices = result.mesh.indices.size();
    if (report.quality_score < stage.minimum_quality) {
      report.passed = false;
      report.diagnostics.push_back("error: stage quality below minimum for " + stage.id);
    }
    result.quality_score = std::min(result.quality_score, report.quality_score);
    if (!report.passed) {
      result.production_ready = false;
      for (const std::string &diagnostic : report.diagnostics) {
        result.diagnostics.push_back(stage.id + ": " + diagnostic);
      }
    }
    completed.push_back(stage.id);
    result.stage_reports.push_back(std::move(report));
  }
  if (result.physics_bodies.empty()) {
    result.production_ready = false;
    result.quality_score = std::min(result.quality_score, 70u);
    result.diagnostics.push_back("error: factory recipe did not emit physics proxies");
  }
  if (result.lods.empty()) {
    result.lods = makeLods(recipe, result.mesh);
  }
  if (result.mesh.vertices.empty() || result.mesh.indices.empty()) {
    result.production_ready = false;
    result.quality_score = 0u;
    result.diagnostics.push_back("error: factory recipe produced empty render mesh");
  }
  std::sort(result.creative_variant_tags.begin(), result.creative_variant_tags.end());
  result.creative_variant_tags.erase(std::unique(result.creative_variant_tags.begin(),
                                                 result.creative_variant_tags.end()),
                                     result.creative_variant_tags.end());
  result.production_session.quality_gate =
      result.production_ready ? "production-ready" : "needs-review";
  return result;
}

AsterAssetFoundryRecipe makeAsterPipeFoundryRecipe(AsterPipeAssetSpec spec,
                                                   const AsterPipeFoundryVariant variant) {
  spec.include_flanges = variant == AsterPipeFoundryVariant::IndustrialHardware;
  spec.include_bolts = variant == AsterPipeFoundryVariant::IndustrialHardware;
  const AsterPipeAsset pipe = makeAsterPipeAsset(spec);

  AsterAssetFoundryRecipe recipe;
  recipe.asset_id = spec.asset_id;
  recipe.label = variant == AsterPipeFoundryVariant::IndustrialHardware
                     ? "Aster rusted pipe industrial hardware recipe"
                     : "Aster rusted pipe reference silhouette recipe";
  recipe.source_provenance_id = "aster.asset_foundry.pipe.v2";
  recipe.source_mesh = pipe.mergedRenderMesh();
  recipe.material = makeAsterPipeMaterial("pipe.body");
  for (const AsterPipeLod &lod : pipe.lods) {
    recipe.authored_lods.push_back(lod.mesh);
  }
  recipe.stages = pipeStages(spec);
  recipe.dependency_edges = pipe.cook_report.dependency_edges;
  recipe.dependency_edges.push_back("asset_foundry.recipe -> pipe_runtime_asset");
  recipe.dependency_edges.push_back("surface_contract -> procedural_surface_signals");
  recipe.dependency_edges.push_back("physics_proxy -> PhysicsBodyDesc");
  recipe.creative_variant_tags =
      variant == AsterPipeFoundryVariant::IndustrialHardware
          ? std::vector<std::string>{"industrial-hardware", "optional-flange-bolts"}
          : std::vector<std::string>{"reference-silhouette", "no-extra-hardware"};
  recipe.visual_brief_claims = {"corroded_orange_brown_rust",
                                "dark_oxide_cavities",
                                "raised_weld_rings",
                                "open_hollow_rims",
                                "uneven_pitting",
                                "axial_scratches",
                                "reference_silhouette"};
  recipe.visual_brief_rejections = {"smooth_black_pipe",
                                    "decorative_bolts_without_reference",
                                    "clean_plastic_surface",
                                    "monochrome_material"};

  recipe.surface_contracts.push_back(
      {.id = "surface.pipe.corrosion",
       .label = "Rusted pipe corrosion and hollow rim visual contract",
       .material_slot = "pipe.body",
       .layer = pipeLayerFromSpec(spec),
       .detail_scale = 11.25f,
       .min_physical_texel_density = 768.0f,
       .min_height_normal_coupling = 0.70f,
       .min_roughness_height_coupling = 0.54f,
       .required_signals = {{"corroded_orange_brown_rust", 0.08f, 0.04f, 1.0f},
                            {"dark_oxide_cavities", 0.04f, 0.02f, 0.85f},
                            {"uneven_pitting", 0.015f, 0.015f, 0.85f},
                            {"axial_scratches", 0.015f, 0.012f, 0.65f},
                            {"open_hollow_rims", 0.004f, 0.001f, 0.95f},
                            {"raised_weld_rings", 0.003f, 0.001f, 1.0f}},
       .forbidden_signals = {{"clean_plastic_surface", 0.72f, 0.48f, 0.85f},
                             {"monochrome_material", 0.86f, 0.54f, 0.80f}}});

  if (!pipe.collision_proxies.empty()) {
    recipe.physics_proxies.push_back(proxyFromPipe(pipe.collision_proxies.front(), spec));
  }
  return recipe;
}

AsterAssetFoundryRecipe makeAsterAssetFoundryRecipeFromGraph(
    const ProceduralAssetGraphPackage &package,
    std::vector<AsterAssetFoundryQualityDiagnostic> *diagnostics) {
  const std::string primitive = normalizedText(package.mesh.primitive);
  const bool pipe_graph =
      textContains(primitive, "pipe") ||
      containsText(package.factory_report.visual_brief_claims, "raised_weld_rings") ||
      containsText(package.factory_report.visual_brief_claims, "open_hollow_rims");
  AsterAssetFoundryRecipe recipe;
  if (pipe_graph) {
    AsterPipeAssetSpec spec{.asset_id = package.id.empty() ? package.asset_guid : package.id,
                            .length = graphMaterialParamOr(package, "length", 5.2f),
                            .outer_radius = graphMaterialParamOr(package, "outer_radius", 0.54f),
                            .wall_thickness =
                                graphMaterialParamOr(package, "wall_thickness", 0.090f),
                            .radial_segments =
                                static_cast<int>(graphMaterialU32Or(package, "radial_segments", 96u)),
                            .length_segments =
                                static_cast<int>(graphMaterialU32Or(package, "length_segments", 24u)),
                            .include_longitudinal_seam = false,
                            .include_flanges = textContains(primitive, "industrial"),
                            .include_bolts = textContains(primitive, "industrial"),
                            .bolt_count_per_flange =
                                static_cast<int>(graphMaterialU32Or(package, "bolt_count", 10u)),
                            .rust_strength =
                                graphMaterialParamOr(package, "rust_strength",
                                                     graphMaterialParamOr(package, "rust_bloom",
                                                                          0.86f)),
                            .wetness_strength = graphMaterialParamOr(package, "wetness", 0.24f),
                            .pitting_density =
                                graphMaterialParamOr(package, "pitting_density", 0.72f),
                            .pitting_depth = graphMaterialParamOr(package, "pitting_depth", 0.0022f),
                            .oxide_layering =
                                graphMaterialParamOr(package, "oxide_layering", 0.86f),
                            .cavity_grime_strength =
                                graphMaterialParamOr(package, "cavity_grime", 0.70f),
                            .edge_polish_strength =
                                graphMaterialParamOr(package, "edge_polish", 0.36f),
                            .weld_heat_tint_strength =
                                graphMaterialParamOr(package, "weld_heat_tint", 0.48f),
                            .axial_scratch_strength =
                                graphMaterialParamOr(package, "axial_scratches", 0.66f),
                            .rust_bloom_strength =
                                graphMaterialParamOr(package, "rust_bloom", 0.86f),
                            .black_scab_strength =
                                graphMaterialParamOr(package, "black_scab", 0.74f),
                            .paint_remnant_strength =
                                graphMaterialParamOr(package, "paint_remnant", 0.18f),
                            .weld_slag_strength =
                                graphMaterialParamOr(package, "weld_slag", 0.82f),
                            .rim_soot_strength =
                                graphMaterialParamOr(package, "rim_soot", 0.88f),
                            .wet_streak_count =
                                static_cast<int>(graphMaterialU32Or(package, "wet_streaks", 7u))};
    recipe = makeAsterPipeFoundryRecipe(
        spec, textContains(primitive, "industrial") ? AsterPipeFoundryVariant::IndustrialHardware
                                                    : AsterPipeFoundryVariant::ReferenceSilhouette);
    recipe.label = package.name.empty() ? "Aster graph pipe foundry recipe" : package.name;
    recipe.source_provenance_id = package.asset_guid.empty() ? package.id : package.asset_guid;
  } else {
    recipe.asset_id = package.id.empty() ? package.asset_guid : package.id;
    recipe.label = package.name.empty() ? recipe.asset_id : package.name;
    recipe.source_provenance_id = package.asset_guid;
    recipe.source_mesh = proceduralAssetGraphMesh(package);
    recipe.material = proceduralAssetGraphMaterial(package);
    AsterAssetFoundryStage source;
    source.id = "stage.graph.source";
    source.kind = AsterAssetFoundryStageKind::SourceGeometry;
    source.label = "Aster graph source mesh";
    source.minimum_quality = 70u;
    AsterAssetFoundryStage lod;
    lod.id = "stage.graph.lod";
    lod.kind = AsterAssetFoundryStageKind::LodRecipe;
    lod.label = "Aster graph generated LODs";
    lod.depends_on = {source.id};
    lod.minimum_quality = 60u;
    AsterAssetFoundryStage physics;
    physics.id = "stage.graph.physics";
    physics.kind = AsterAssetFoundryStageKind::PhysicsProxy;
    physics.label = "Aster graph bounds proxy";
    physics.depends_on = {lod.id};
    physics.physics_proxy_id = "physics.graph.runtime-bounds";
    physics.minimum_quality = 70u;
    AsterAssetFoundryStage quality;
    quality.id = "stage.graph.quality";
    quality.kind = AsterAssetFoundryStageKind::QualityGate;
    quality.label = "Aster graph quality gate";
    quality.depends_on = {physics.id};
    quality.minimum_quality = 60u;
    AsterAssetFoundryStage package_stage;
    package_stage.id = "stage.graph.package";
    package_stage.kind = AsterAssetFoundryStageKind::Package;
    package_stage.label = "Aster graph package handoff";
    package_stage.depends_on = {quality.id};
    package_stage.minimum_quality = 60u;
    recipe.stages = {source, lod, physics, quality, package_stage};
    recipe.physics_proxies.push_back(boundsProxyForMesh(recipe.source_mesh,
                                                        "physics.graph.runtime-bounds"));
  }

  recipe.proof_artifacts = proofArtifactsFromGraph(package);
  recipe.dependency_edges.clear();
  for (const ProceduralAssetGraphEdge &edge : package.edges) {
    recipe.dependency_edges.push_back("graph:" + edge.from + " -> " + edge.to + ":" + edge.role);
  }
  recipe.dependency_edges.push_back("asset_graph:" + package.id + " -> foundry_recipe");
  if (!package.factory_report.stable_recipe_hash.empty()) {
    recipe.dependency_edges.push_back("graph_factory_hash:" +
                                      package.factory_report.stable_recipe_hash);
  }
  if (!package.factory_report.visual_brief_claims.empty()) {
    recipe.visual_brief_claims = package.factory_report.visual_brief_claims;
  }
  if (!package.factory_report.visual_brief_rejections.empty()) {
    recipe.visual_brief_rejections = package.factory_report.visual_brief_rejections;
  }
  if (recipe.visual_brief_claims.empty()) {
    for (const ProceduralAssetGraphFactorySignalCoverage &signal :
         package.factory_report.surface_signal_coverage) {
      if (signal.status == "claimed") {
        recipe.visual_brief_claims.push_back(signal.signal);
      }
    }
  }
  recipe.creative_variant_tags.push_back("graph-owned");
  recipe.creative_variant_tags.push_back("headless-foundry");

  if (diagnostics != nullptr) {
    if (package.proof_artifacts.empty()) {
      diagnostics->push_back({.severity = AsterAssetFoundryDiagnosticSeverity::Warning,
                              .category = "proof",
                              .stage_id = {},
                              .message = "graph package does not list proof artifacts"});
    }
    for (const MaterialDiagnostic &diagnostic : package.diagnostics) {
      diagnostics->push_back({.severity = diagnostic.severity == MaterialDiagnosticSeverity::Error
                                              ? AsterAssetFoundryDiagnosticSeverity::Error
                                              : AsterAssetFoundryDiagnosticSeverity::Warning,
                              .category = "graph",
                              .stage_id = {},
                              .message = diagnostic.message});
    }
    for (const ProceduralAssetGraphNode &node : package.nodes) {
      if (node.capability_status == "unsupported") {
        diagnostics->push_back({.severity = AsterAssetFoundryDiagnosticSeverity::Warning,
                                .category = "graph-node",
                                .stage_id = node.id,
                                .message = "unsupported node preserved as descriptor metadata"});
      }
    }
    for (const AsterAssetFoundryQualityDiagnostic &diagnostic :
         validateAsterAssetFoundryRecipe(recipe)) {
      diagnostics->push_back(diagnostic);
    }
  }
  return recipe;
}

} // namespace aster
