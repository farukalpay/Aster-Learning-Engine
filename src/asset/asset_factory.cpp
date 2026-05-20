// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_factory.hpp"

#include <algorithm>
#include <bit>
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

[[nodiscard]] bool stageExists(const AsterAssetFactoryRecipe &recipe, const std::string &id) {
  return std::any_of(recipe.stages.begin(), recipe.stages.end(),
                     [&](const AsterAssetFactoryStage &stage) { return stage.id == id; });
}

[[nodiscard]] bool duplicateId(const std::vector<std::string> &seen, const std::string &id) {
  return std::find(seen.begin(), seen.end(), id) != seen.end();
}

[[nodiscard]] const AsterAssetFactorySurfaceContract *
findSurfaceContract(const AsterAssetFactoryRecipe &recipe, const std::string &id) {
  const auto found =
      std::find_if(recipe.surface_contracts.begin(), recipe.surface_contracts.end(),
                   [&](const AsterAssetFactorySurfaceContract &contract) {
                     return contract.id == id;
                   });
  return found == recipe.surface_contracts.end() ? nullptr : &*found;
}

[[nodiscard]] const AsterAssetFactoryPhysicsProxy *
findPhysicsProxy(const AsterAssetFactoryRecipe &recipe, const std::string &id) {
  const auto found = std::find_if(recipe.physics_proxies.begin(), recipe.physics_proxies.end(),
                                  [&](const AsterAssetFactoryPhysicsProxy &proxy) {
                                    return proxy.id == id;
                                  });
  return found == recipe.physics_proxies.end() ? nullptr : &*found;
}

[[nodiscard]] AsterAssetFactoryDiagnosticSeverity severityFromMessage(
    const std::string &message) {
  if (message.rfind("error:", 0u) == 0u) {
    return AsterAssetFactoryDiagnosticSeverity::Error;
  }
  if (message.rfind("warning:", 0u) == 0u) {
    return AsterAssetFactoryDiagnosticSeverity::Warning;
  }
  return AsterAssetFactoryDiagnosticSeverity::Info;
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

[[nodiscard]] bool coverageClaimsSignal(const AsterAssetFactoryBuildResult &result,
                                        const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const AsterAssetFactorySurfaceCoverage &coverage) {
                       return containsText(coverage.claimed_signals, signal);
                     });
}

[[nodiscard]] bool coverageRejectsSignal(const AsterAssetFactoryBuildResult &result,
                                         const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const AsterAssetFactorySurfaceCoverage &coverage) {
                       return containsText(coverage.rejected_signals, signal);
                     });
}

[[nodiscard]] float visualBriefWeight(const AsterAssetFactoryRecipe &recipe,
                                      const std::string &signal) {
  for (const AsterAssetFactorySurfaceContract &contract : recipe.surface_contracts) {
    for (const AsterAssetFactorySignalRule &rule : contract.required_signals) {
      if (rule.id == signal) {
        return rule.weight;
      }
    }
    for (const AsterAssetFactorySignalRule &rule : contract.forbidden_signals) {
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

[[nodiscard]] AsterAssetFactorySignalSample
sampleSignal(const CpuMesh &mesh, const AsterAssetFactorySurfaceContract &contract,
             const AsterAssetFactorySignalRule &rule) {
  AsterAssetFactorySignalSample out;
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

[[nodiscard]] AsterAssetFactorySurfaceCoverage
evaluateSurfaceContract(const CpuMesh &mesh, const AsterAssetFactorySurfaceContract &contract) {
  AsterAssetFactorySurfaceCoverage coverage;
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
  for (const AsterAssetFactorySignalRule &rule : contract.required_signals) {
    AsterAssetFactorySignalSample sample = sampleSignal(mesh, contract, rule);
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
  for (const AsterAssetFactorySignalRule &rule : contract.forbidden_signals) {
    AsterAssetFactorySignalSample sample = sampleSignal(mesh, contract, rule);
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
  stack.source_provenance_id = "asset-factory";
  stack.modifiers.push_back({.id = "lod.decimate." + std::to_string(level),
                             .kind = AssetModifierKind::Decimate,
                             .amount = level == 1u ? 0.52f : 0.28f,
                             .seed = 200u + level,
                             .count = 1u,
                             .creative_variant_tags = {"factory-lod"}});
  return applyAssetModifierStack(mesh, stack).mesh;
}

[[nodiscard]] std::vector<CpuMesh> makeLods(const AsterAssetFactoryRecipe &recipe,
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

[[nodiscard]] AsterAssetFactoryPhysicsProxy proxyFromPipe(
    const AsterPipeCollisionProxy &pipe_proxy, const AsterPipeAssetSpec &spec) {
  const float wetness = saturate(spec.wetness_strength);
  const float rust = saturate(spec.rust_strength);
  return {.id = "physics.pipe.runtime-bounds",
          .label = "Rusted pipe runtime bounds proxy",
          .kind = AsterAssetFactoryPhysicsProxyKind::BoundsBox,
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

[[nodiscard]] std::vector<AsterAssetFactoryStage> pipeStages(const AsterPipeAssetSpec &spec) {
  AsterAssetFactoryStage source;
  source.id = "stage.pipe.source";
  source.kind = AsterAssetFactoryStageKind::SourceGeometry;
  source.label = "Aster pipe source assembly";
  source.minimum_quality = 90u;
  source.creative_variant_tags = {"reference-silhouette"};

  AsterAssetFactoryStage polish;
  polish.id = "stage.pipe.modifier-stack";
  polish.kind = AsterAssetFactoryStageKind::ModifierStack;
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

  AsterAssetFactoryStage surface;
  surface.id = "stage.pipe.surface-contract";
  surface.kind = AsterAssetFactoryStageKind::SurfaceContract;
  surface.label = "Corroded pipe visual brief contract";
  surface.depends_on = {polish.id};
  surface.surface_contract_id = "surface.pipe.corrosion";
  surface.minimum_quality = 82u;

  AsterAssetFactoryStage lod;
  lod.id = "stage.pipe.lod";
  lod.kind = AsterAssetFactoryStageKind::LodRecipe;
  lod.label = "Factory generated LOD chain";
  lod.depends_on = {surface.id};
  lod.minimum_quality = 70u;

  AsterAssetFactoryStage physics;
  physics.id = "stage.pipe.physics";
  physics.kind = AsterAssetFactoryStageKind::PhysicsProxy;
  physics.label = "Runtime collision/query proxy";
  physics.depends_on = {lod.id};
  physics.physics_proxy_id = "physics.pipe.runtime-bounds";
  physics.minimum_quality = 80u;

  AsterAssetFactoryStage quality;
  quality.id = "stage.pipe.quality";
  quality.kind = AsterAssetFactoryStageKind::QualityGate;
  quality.label = "Visual brief and cook contract gate";
  quality.depends_on = {physics.id};
  quality.minimum_quality = 88u;

  AsterAssetFactoryStage package;
  package.id = "stage.pipe.package";
  package.kind = AsterAssetFactoryStageKind::Package;
  package.label = "Asset graph package handoff";
  package.depends_on = {quality.id};
  package.minimum_quality = 80u;
  return {source, polish, surface, lod, physics, quality, package};
}

} // namespace

std::string_view asterAssetFactoryDiagnosticSeverityName(
    const AsterAssetFactoryDiagnosticSeverity severity) noexcept {
  switch (severity) {
  case AsterAssetFactoryDiagnosticSeverity::Info:
    return "info";
  case AsterAssetFactoryDiagnosticSeverity::Warning:
    return "warning";
  case AsterAssetFactoryDiagnosticSeverity::Error:
    return "error";
  }
  return "unknown";
}

std::string_view asterAssetFactoryStageKindName(
    const AsterAssetFactoryStageKind kind) noexcept {
  switch (kind) {
  case AsterAssetFactoryStageKind::SourceGeometry:
    return "source-geometry";
  case AsterAssetFactoryStageKind::ModifierStack:
    return "modifier-stack";
  case AsterAssetFactoryStageKind::SurfaceContract:
    return "surface-contract";
  case AsterAssetFactoryStageKind::LodRecipe:
    return "lod-recipe";
  case AsterAssetFactoryStageKind::PhysicsProxy:
    return "physics-proxy";
  case AsterAssetFactoryStageKind::QualityGate:
    return "quality-gate";
  case AsterAssetFactoryStageKind::Package:
    return "package";
  }
  return "unknown";
}

std::string_view asterAssetFactoryPhysicsProxyKindName(
    const AsterAssetFactoryPhysicsProxyKind kind) noexcept {
  switch (kind) {
  case AsterAssetFactoryPhysicsProxyKind::BoundsBox:
    return "bounds-box";
  case AsterAssetFactoryPhysicsProxyKind::RadialCapsule:
    return "radial-capsule";
  case AsterAssetFactoryPhysicsProxyKind::TriangleMesh:
    return "triangle-mesh";
  }
  return "unknown";
}

std::vector<AsterAssetFactoryQualityDiagnostic>
validateAsterAssetFactoryRecipe(const AsterAssetFactoryRecipe &recipe) {
  std::vector<AsterAssetFactoryQualityDiagnostic> diagnostics;
  const auto push = [&](const AsterAssetFactoryDiagnosticSeverity severity,
                       std::string category, std::string stage_id, std::string message) {
    diagnostics.push_back({.severity = severity,
                           .category = std::move(category),
                           .stage_id = std::move(stage_id),
                           .message = std::move(message)});
  };

  if (recipe.asset_id.empty()) {
    push(AsterAssetFactoryDiagnosticSeverity::Error, "identity", {},
         "factory recipe is missing an asset id");
  }
  if (recipe.source_mesh.vertices.empty() || recipe.source_mesh.indices.empty()) {
    push(AsterAssetFactoryDiagnosticSeverity::Error, "source", {},
         "factory recipe has no source render mesh");
  }
  if (recipe.stages.empty()) {
    push(AsterAssetFactoryDiagnosticSeverity::Error, "stage", {},
         "factory recipe declares no stages");
  }
  if (recipe.visual_brief_claims.empty()) {
    push(AsterAssetFactoryDiagnosticSeverity::Warning, "visual-brief", {},
         "factory recipe declares no visual brief claims");
  }
  if (recipe.visual_brief_rejections.empty()) {
    push(AsterAssetFactoryDiagnosticSeverity::Warning, "visual-brief", {},
         "factory recipe declares no forbidden-signal rejections");
  }

  std::vector<std::string> stage_ids;
  stage_ids.reserve(recipe.stages.size());
  for (const AsterAssetFactoryStage &stage : recipe.stages) {
    if (stage.id.empty()) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "stage", {},
           "factory stage is missing an id");
      continue;
    }
    if (duplicateId(stage_ids, stage.id)) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "stage", stage.id,
           "factory stage id is duplicated");
    }
    stage_ids.push_back(stage.id);
  }
  for (const AsterAssetFactoryStage &stage : recipe.stages) {
    for (const std::string &dependency : stage.depends_on) {
      if (!stageExists(recipe, dependency)) {
        push(AsterAssetFactoryDiagnosticSeverity::Error, "dependency", stage.id,
             "stage depends on unknown stage: " + dependency);
      }
    }
    if (stage.kind == AsterAssetFactoryStageKind::SurfaceContract &&
        findSurfaceContract(recipe, stage.surface_contract_id) == nullptr) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "surface", stage.id,
           "stage references a missing surface contract: " + stage.surface_contract_id);
    }
    if (stage.kind == AsterAssetFactoryStageKind::PhysicsProxy &&
        findPhysicsProxy(recipe, stage.physics_proxy_id) == nullptr) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "physics", stage.id,
           "stage references a missing physics proxy: " + stage.physics_proxy_id);
    }
  }

  std::vector<std::string> contract_ids;
  contract_ids.reserve(recipe.surface_contracts.size());
  for (const AsterAssetFactorySurfaceContract &contract : recipe.surface_contracts) {
    if (contract.id.empty()) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "surface", {},
           "surface contract is missing an id");
      continue;
    }
    if (duplicateId(contract_ids, contract.id)) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "surface", contract.id,
           "surface contract id is duplicated");
    }
    contract_ids.push_back(contract.id);
    if (contract.required_signals.empty()) {
      push(AsterAssetFactoryDiagnosticSeverity::Warning, "surface", contract.id,
           "surface contract has no required signals");
    }
  }

  std::vector<std::string> proxy_ids;
  proxy_ids.reserve(recipe.physics_proxies.size());
  for (const AsterAssetFactoryPhysicsProxy &proxy : recipe.physics_proxies) {
    if (proxy.id.empty()) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "physics", {},
           "physics proxy is missing an id");
      continue;
    }
    if (duplicateId(proxy_ids, proxy.id)) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "physics", proxy.id,
           "physics proxy id is duplicated");
    }
    proxy_ids.push_back(proxy.id);
    if (proxy.kind == AsterAssetFactoryPhysicsProxyKind::BoundsBox &&
        (proxy.half_extents.x <= 0.0f || proxy.half_extents.y <= 0.0f ||
         proxy.half_extents.z <= 0.0f)) {
      push(AsterAssetFactoryDiagnosticSeverity::Error, "physics", proxy.id,
           "bounds proxy has non-positive half extents");
    }
    if (proxy.kind == AsterAssetFactoryPhysicsProxyKind::TriangleMesh &&
        proxy.triangle_budget == 0u) {
      push(AsterAssetFactoryDiagnosticSeverity::Warning, "physics", proxy.id,
           "triangle mesh proxy has no declared triangle budget");
    }
  }
  return diagnostics;
}

std::vector<AsterAssetFactoryLodSummary>
summarizeAsterAssetFactoryLods(const AsterAssetFactoryBuildResult &result) {
  std::vector<AsterAssetFactoryLodSummary> summaries;
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

std::vector<AsterAssetFactoryPhysicsProxySummary>
summarizeAsterAssetFactoryPhysicsProxies(const AsterAssetFactoryRecipe &recipe) {
  std::vector<AsterAssetFactoryPhysicsProxySummary> summaries;
  summaries.reserve(recipe.physics_proxies.size());
  for (const AsterAssetFactoryPhysicsProxy &proxy : recipe.physics_proxies) {
    summaries.push_back(
        {.id = proxy.id,
         .shape = std::string(asterAssetFactoryPhysicsProxyKindName(proxy.kind)),
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

std::vector<AsterAssetFactoryVisualBriefRow>
makeAsterAssetFactoryVisualBriefRows(const AsterAssetFactoryRecipe &recipe,
                                     const AsterAssetFactoryBuildResult &result) {
  std::vector<AsterAssetFactoryVisualBriefRow> rows;
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

AsterAssetFactoryRecipeAudit auditAsterAssetFactoryBuild(
    const AsterAssetFactoryRecipe &recipe, const AsterAssetFactoryBuildResult &result) {
  AsterAssetFactoryRecipeAudit audit;
  audit.asset_id = recipe.asset_id;
  audit.stable_recipe_hash = result.stable_recipe_hash.empty()
                                 ? stableAsterAssetFactoryRecipeHash(recipe)
                                 : result.stable_recipe_hash;
  audit.quality_score = result.quality_score;
  audit.production_ready = result.production_ready;
  audit.lod_summary = summarizeAsterAssetFactoryLods(result);
  audit.visual_brief_rows = makeAsterAssetFactoryVisualBriefRows(recipe, result);
  audit.diagnostics = validateAsterAssetFactoryRecipe(recipe);

  audit.stage_order.reserve(recipe.stages.size());
  std::vector<std::string> completed;
  completed.reserve(recipe.stages.size());
  for (const AsterAssetFactoryStage &stage : recipe.stages) {
    audit.stage_order.push_back(stage.id);
    for (const std::string &dependency : stage.depends_on) {
      if (!containsText(completed, dependency)) {
        audit.missing_dependencies.push_back(stage.id + " <- " + dependency);
      }
    }
    completed.push_back(stage.id);
  }
  for (const AsterAssetFactorySurfaceContract &contract : recipe.surface_contracts) {
    audit.surface_contract_ids.push_back(contract.id);
  }
  for (const AsterAssetFactoryPhysicsProxy &proxy : recipe.physics_proxies) {
    audit.physics_proxy_ids.push_back(proxy.id);
  }
  for (const AsterAssetFactoryStageReport &stage_report : result.stage_reports) {
    for (const std::string &message : stage_report.diagnostics) {
      audit.diagnostics.push_back(
          {.severity = severityFromMessage(message),
           .category = std::string(asterAssetFactoryStageKindName(stage_report.kind)),
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
describeAsterAssetFactoryAudit(const AsterAssetFactoryRecipeAudit &audit) {
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
  for (const AsterAssetFactoryLodSummary &lod : audit.lod_summary) {
    std::ostringstream line;
    line << "lod[" << lod.level << "] vertices=" << lod.vertices
         << " indices=" << lod.indices << " ratio=" << std::fixed
         << std::setprecision(3) << lod.triangle_ratio
         << " screen=" << lod.recommended_screen_coverage;
    lines.push_back(line.str());
  }
  for (const AsterAssetFactoryVisualBriefRow &row : audit.visual_brief_rows) {
    std::ostringstream line;
    line << "visual_brief " << row.signal << "=" << row.status
         << " source=" << row.source << " weight=" << std::fixed
         << std::setprecision(2) << row.weight;
    lines.push_back(line.str());
  }
  for (const AsterAssetFactoryQualityDiagnostic &diagnostic : audit.diagnostics) {
    std::ostringstream line;
    line << asterAssetFactoryDiagnosticSeverityName(diagnostic.severity) << ":"
         << diagnostic.category;
    if (!diagnostic.stage_id.empty()) {
      line << ":" << diagnostic.stage_id;
    }
    line << ":" << diagnostic.message;
    lines.push_back(line.str());
  }
  return lines;
}

std::string stableAsterAssetFactoryRecipeHash(const AsterAssetFactoryRecipe &recipe) {
  std::uint64_t hash = fnvSeed();
  appendHash(hash, recipe.asset_id);
  appendHash(hash, recipe.label);
  appendHash(hash, recipe.source_provenance_id);
  appendHash(hash, recipe.source_mesh.vertices.size());
  appendHash(hash, recipe.source_mesh.indices.size());
  for (const AsterAssetFactoryStage &stage : recipe.stages) {
    appendHash(hash, stage.id);
    appendHash(hash, asterAssetFactoryStageKindName(stage.kind));
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
  for (const AsterAssetFactorySurfaceContract &contract : recipe.surface_contracts) {
    appendHash(hash, contract.id);
    appendHash(hash, contract.material_slot);
    appendHash(hash, contract.detail_scale);
    appendHash(hash, contract.min_physical_texel_density);
    appendHash(hash, contract.layer);
    for (const AsterAssetFactorySignalRule &rule : contract.required_signals) {
      appendHash(hash, rule.id);
      appendHash(hash, rule.minimum_average);
      appendHash(hash, rule.minimum_coverage);
      appendHash(hash, rule.weight);
    }
    for (const AsterAssetFactorySignalRule &rule : contract.forbidden_signals) {
      appendHash(hash, rule.id);
      appendHash(hash, rule.minimum_average);
      appendHash(hash, rule.minimum_coverage);
      appendHash(hash, rule.weight);
    }
  }
  for (const AsterAssetFactoryPhysicsProxy &proxy : recipe.physics_proxies) {
    appendHash(hash, proxy.id);
    appendHash(hash, asterAssetFactoryPhysicsProxyKindName(proxy.kind));
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
  return hex64(hash, "aster-factory-0x");
}

PhysicsBodyDesc asterAssetFactoryPhysicsBodyDesc(
    const AsterAssetFactoryPhysicsProxy &proxy, std::shared_ptr<const CpuMesh> mesh) {
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
  case AsterAssetFactoryPhysicsProxyKind::BoundsBox:
    desc.shape = PhysicsShapeType::Box;
    break;
  case AsterAssetFactoryPhysicsProxyKind::RadialCapsule:
    desc.shape = PhysicsShapeType::Capsule;
    desc.half_extents.y = std::max(proxy.length * 0.5f, proxy.half_extents.y);
    break;
  case AsterAssetFactoryPhysicsProxyKind::TriangleMesh:
    desc.shape = PhysicsShapeType::TriangleMesh;
    desc.mesh = std::move(mesh);
    desc.mesh_double_sided = true;
    break;
  }
  return desc;
}

AsterAssetFactoryBuildResult buildAsterAssetFactoryRecipe(
    const AsterAssetFactoryRecipe &recipe) {
  AsterAssetFactoryBuildResult result;
  result.asset_id = recipe.asset_id;
  result.stable_recipe_hash = stableAsterAssetFactoryRecipeHash(recipe);
  result.mesh = recipe.source_mesh;
  result.material = recipe.material;
  result.dependency_edges = recipe.dependency_edges;
  result.visual_brief_claims = recipe.visual_brief_claims;
  result.visual_brief_rejections = recipe.visual_brief_rejections;
  result.quality_score = 100u;
  result.production_ready = true;

  std::vector<std::string> completed;
  for (const AsterAssetFactoryStage &stage : recipe.stages) {
    AsterAssetFactoryStageReport report;
    report.id = stage.id;
    report.kind = stage.kind;
    report.executed = stage.enabled;
    report.passed = true;
    report.quality_score = 100u;
    report.input_vertices = result.mesh.vertices.size();
    report.input_indices = result.mesh.indices.size();
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
      case AsterAssetFactoryStageKind::SourceGeometry:
        if (result.mesh.vertices.empty() || result.mesh.indices.empty()) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: source geometry is empty");
        }
        break;
      case AsterAssetFactoryStageKind::ModifierStack:
        if (!stage.modifier_stack.modifiers.empty()) {
          AssetModifierStackResult modified =
              applyAssetModifierStack(result.mesh, stage.modifier_stack);
          result.mesh = std::move(modified.mesh);
          report.quality_score = modified.report.quality_score;
          report.diagnostics.insert(report.diagnostics.end(), modified.report.diagnostics.begin(),
                                    modified.report.diagnostics.end());
          for (const std::string &edge : modified.report.creative_variant_tags) {
            result.dependency_edges.push_back("modifier:" + stage.id + " -> tag:" + edge);
          }
        }
        break;
      case AsterAssetFactoryStageKind::SurfaceContract: {
        const AsterAssetFactorySurfaceContract *contract =
            findSurfaceContract(recipe, stage.surface_contract_id);
        if (contract == nullptr) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: missing surface contract: " +
                                       stage.surface_contract_id);
          break;
        }
        AsterAssetFactorySurfaceCoverage coverage =
            evaluateSurfaceContract(result.mesh, *contract);
        report.passed = coverage.passed;
        report.quality_score = coverage.quality_score;
        report.diagnostics.insert(report.diagnostics.end(), coverage.diagnostics.begin(),
                                  coverage.diagnostics.end());
        result.surface_coverages.push_back(std::move(coverage));
        break;
      }
      case AsterAssetFactoryStageKind::LodRecipe:
        result.lods = makeLods(recipe, result.mesh);
        if (result.lods.size() < 3u) {
          report.passed = false;
          report.quality_score = 55u;
          report.diagnostics.push_back("warning: factory recipe produced fewer than three LODs");
        }
        break;
      case AsterAssetFactoryStageKind::PhysicsProxy: {
        const AsterAssetFactoryPhysicsProxy *proxy =
            findPhysicsProxy(recipe, stage.physics_proxy_id);
        if (proxy == nullptr) {
          report.passed = false;
          report.quality_score = 0u;
          report.diagnostics.push_back("error: missing physics proxy: " + stage.physics_proxy_id);
          break;
        }
        std::shared_ptr<const CpuMesh> mesh;
        if (proxy->kind == AsterAssetFactoryPhysicsProxyKind::TriangleMesh) {
          mesh = std::make_shared<const CpuMesh>(result.mesh);
        }
        result.physics_bodies.push_back(asterAssetFactoryPhysicsBodyDesc(*proxy, std::move(mesh)));
        if (!proxy->covers_render_bounds && proxy->kind == AsterAssetFactoryPhysicsProxyKind::BoundsBox) {
          report.quality_score = 82u;
          report.diagnostics.push_back("warning: bounds proxy does not claim render coverage");
        }
        break;
      }
      case AsterAssetFactoryStageKind::QualityGate:
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
      case AsterAssetFactoryStageKind::Package:
        result.dependency_edges.push_back("factory:" + result.stable_recipe_hash +
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
  return result;
}

AsterAssetFactoryRecipe makeAsterPipeFactoryRecipe(AsterPipeAssetSpec spec,
                                                   const AsterPipeFactoryVariant variant) {
  spec.include_flanges = variant == AsterPipeFactoryVariant::IndustrialHardware;
  spec.include_bolts = variant == AsterPipeFactoryVariant::IndustrialHardware;
  const AsterPipeAsset pipe = makeAsterPipeAsset(spec);

  AsterAssetFactoryRecipe recipe;
  recipe.asset_id = spec.asset_id;
  recipe.label = variant == AsterPipeFactoryVariant::IndustrialHardware
                     ? "Aster rusted pipe industrial hardware recipe"
                     : "Aster rusted pipe reference silhouette recipe";
  recipe.source_provenance_id = "aster.asset_factory.pipe.v2";
  recipe.source_mesh = pipe.mergedRenderMesh();
  recipe.material = makeAsterPipeMaterial("pipe.body");
  for (const AsterPipeLod &lod : pipe.lods) {
    recipe.authored_lods.push_back(lod.mesh);
  }
  recipe.stages = pipeStages(spec);
  recipe.dependency_edges = pipe.cook_report.dependency_edges;
  recipe.dependency_edges.push_back("asset_factory.recipe -> pipe_runtime_asset");
  recipe.dependency_edges.push_back("surface_contract -> procedural_surface_signals");
  recipe.dependency_edges.push_back("physics_proxy -> PhysicsBodyDesc");
  recipe.creative_variant_tags =
      variant == AsterPipeFactoryVariant::IndustrialHardware
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

} // namespace aster
