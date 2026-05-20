// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_factory.hpp"
#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/asset/procedural_graph_runtime.hpp"
#include "aster/geometry/mesh_modeling.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

#ifndef ASTER_ASSETC_EXECUTABLE
#define ASTER_ASSETC_EXECUTABLE ""
#endif

namespace {

std::string shellQuote(const std::filesystem::path &path) {
  std::string value = path.string();
  std::string quoted = "'";
  for (const char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(c);
    }
  }
  quoted += "'";
  return quoted;
}

void requireCommand(const std::string &command) {
  const int result = std::system(command.c_str());
  if (result != 0) {
    throw std::runtime_error("command failed: " + command);
  }
}

std::filesystem::path findAssetGraphBin(const std::filesystem::path &root) {
  if (!std::filesystem::exists(root)) {
    return {};
  }
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (entry.path().extension() == ".assetgraphbin") {
      return entry.path();
    }
  }
  return {};
}

bool startsWith(const std::string &value, const std::string &prefix) {
  return value.rfind(prefix, 0u) == 0u;
}

bool hasDiagnosticContaining(const aster::AsterAssetFactoryBuildResult &result,
                             const std::string &needle) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                     [&](const std::string &diagnostic) {
                       return diagnostic.find(needle) != std::string::npos;
                     });
}

bool hasPartContaining(const aster::AsterPipeAsset &asset, const std::string &needle) {
  return std::any_of(asset.parts.begin(), asset.parts.end(),
                     [&](const aster::AsterPipeAssetPart &part) {
                       return part.name.find(needle) != std::string::npos;
                     });
}

bool hasClaim(const aster::AsterAssetFactoryBuildResult &result, const std::string &claim) {
  return std::find(result.visual_brief_claims.begin(), result.visual_brief_claims.end(), claim) !=
         result.visual_brief_claims.end();
}

bool hasRejectedSignal(const aster::AsterAssetFactoryBuildResult &result,
                       const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const aster::AsterAssetFactorySurfaceCoverage &coverage) {
                       return std::find(coverage.rejected_signals.begin(),
                                        coverage.rejected_signals.end(), signal) !=
                              coverage.rejected_signals.end();
                     });
}

bool hasVisualBriefStatus(const aster::AsterAssetFactoryRecipeAudit &audit,
                          const std::string &signal, const std::string &status) {
  return std::any_of(audit.visual_brief_rows.begin(), audit.visual_brief_rows.end(),
                     [&](const aster::AsterAssetFactoryVisualBriefRow &row) {
                       return row.signal == signal && row.status == status;
                     });
}

void assertFiniteMesh(const aster::CpuMesh &mesh) {
  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() % 3u == 0u);
  for (const aster::Vertex &vertex : mesh.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(aster::length(vertex.normal) > 0.35f);
  }
  const aster::MeshTopologyReport report = aster::validateMeshTopology(mesh);
  assert(report.indexable());
  assert(report.invalid_indices == 0u);
  assert(report.degenerate_triangles == 0u);
}

aster::AsterPipeAssetSpec testPipeSpec() {
  return {.asset_id = "test.factory.rusted_pipe",
          .length = 5.2f,
          .outer_radius = 0.54f,
          .wall_thickness = 0.090f,
          .radial_segments = 64,
          .length_segments = 16,
          .rust_strength = 0.98f,
          .wetness_strength = 0.20f,
          .pitting_density = 0.96f,
          .pitting_depth = 0.0055f,
          .oxide_layering = 0.92f,
          .cavity_grime_strength = 0.84f,
          .axial_scratch_strength = 0.82f,
          .rust_bloom_strength = 0.92f,
          .black_scab_strength = 0.78f,
          .paint_remnant_strength = 0.06f,
          .weld_slag_strength = 0.88f,
          .rim_soot_strength = 0.92f};
}

void testDeterministicRecipeHash() {
  const aster::AsterAssetFactoryRecipe recipe =
      aster::makeAsterPipeFactoryRecipe(testPipeSpec());
  const std::string first = aster::stableAsterAssetFactoryRecipeHash(recipe);
  const std::string second = aster::stableAsterAssetFactoryRecipeHash(recipe);
  assert(first == second);
  assert(startsWith(first, "aster-factory-0x"));

  aster::AsterPipeAssetSpec changed = testPipeSpec();
  changed.seed ^= 0x51u;
  const std::string changed_hash =
      aster::stableAsterAssetFactoryRecipeHash(aster::makeAsterPipeFactoryRecipe(changed));
  assert(first != changed_hash);
}

void testRecipeBuildStagesAndLods() {
  const aster::AsterAssetFactoryRecipe recipe =
      aster::makeAsterPipeFactoryRecipe(testPipeSpec());
  const aster::AsterAssetFactoryBuildResult result =
      aster::buildAsterAssetFactoryRecipe(recipe);
  assert(result.production_ready);
  assert(result.quality_score >= 82u);
  assert(result.stage_reports.size() == recipe.stages.size());
  assert(result.surface_coverages.size() == 1u);
  assert(result.physics_bodies.size() == 1u);
  assert(result.lods.size() == 3u);
  assert(result.lods[0].indices.size() > result.lods[1].indices.size());
  assert(result.lods[1].indices.size() > result.lods[2].indices.size());
  assert(hasClaim(result, "corroded_orange_brown_rust"));
  assert(hasClaim(result, "raised_weld_rings"));
  assert(hasRejectedSignal(result, "clean_plastic_surface"));
  assert(hasRejectedSignal(result, "monochrome_material"));
  const std::vector<aster::AsterAssetFactoryQualityDiagnostic> validation =
      aster::validateAsterAssetFactoryRecipe(recipe);
  assert(std::none_of(validation.begin(), validation.end(),
                      [](const aster::AsterAssetFactoryQualityDiagnostic &diagnostic) {
                        return diagnostic.severity ==
                               aster::AsterAssetFactoryDiagnosticSeverity::Error;
                      }));
  const aster::AsterAssetFactoryRecipeAudit audit =
      aster::auditAsterAssetFactoryBuild(recipe, result);
  assert(audit.production_ready);
  assert(audit.stage_order.size() == recipe.stages.size());
  assert(audit.missing_dependencies.empty());
  assert(audit.lod_summary.size() == 3u);
  assert(audit.lod_summary[1].triangle_ratio < audit.lod_summary[0].triangle_ratio);
  assert(audit.surface_contract_ids.front() == "surface.pipe.corrosion");
  assert(audit.physics_proxy_ids.front() == "physics.pipe.runtime-bounds");
  assert(hasVisualBriefStatus(audit, "raised_weld_rings", "claimed"));
  assert(hasVisualBriefStatus(audit, "decorative_bolts_without_reference", "rejected"));
  const std::vector<aster::AsterAssetFactoryPhysicsProxySummary> proxy_summary =
      aster::summarizeAsterAssetFactoryPhysicsProxies(recipe);
  assert(proxy_summary.size() == 1u);
  assert(proxy_summary.front().shape == "bounds-box");
  assert(proxy_summary.front().covers_render_bounds);
  assert(proxy_summary.front().friction > 0.70f);
  const std::vector<std::string> audit_lines = aster::describeAsterAssetFactoryAudit(audit);
  assert(std::any_of(audit_lines.begin(), audit_lines.end(), [](const std::string &line) {
    return line.find("visual_brief raised_weld_rings=claimed") != std::string::npos;
  }));
  assert(std::any_of(audit_lines.begin(), audit_lines.end(), [](const std::string &line) {
    return line.find("lod[1]") != std::string::npos;
  }));
  assertFiniteMesh(result.mesh);
  for (const aster::CpuMesh &lod : result.lods) {
    assertFiniteMesh(lod);
  }
}

void testStageDependencyDiagnostics() {
  aster::AsterAssetFactoryRecipe recipe = aster::makeAsterPipeFactoryRecipe(testPipeSpec());
  std::swap(recipe.stages[0], recipe.stages[1]);
  const aster::AsterAssetFactoryBuildResult result =
      aster::buildAsterAssetFactoryRecipe(recipe);
  assert(!result.production_ready);
  assert(hasDiagnosticContaining(result, "stage dependency has not completed"));
  const aster::AsterAssetFactoryRecipeAudit audit =
      aster::auditAsterAssetFactoryBuild(recipe, result);
  assert(!audit.missing_dependencies.empty());
  assert(std::any_of(audit.diagnostics.begin(), audit.diagnostics.end(),
                     [](const aster::AsterAssetFactoryQualityDiagnostic &diagnostic) {
                       return diagnostic.severity ==
                              aster::AsterAssetFactoryDiagnosticSeverity::Error;
                     }));
}

void testRecipeValidationCatchesBrokenContracts() {
  aster::AsterAssetFactoryRecipe recipe = aster::makeAsterPipeFactoryRecipe(testPipeSpec());
  recipe.surface_contracts.clear();
  recipe.physics_proxies.front().half_extents.y = 0.0f;
  const std::vector<aster::AsterAssetFactoryQualityDiagnostic> diagnostics =
      aster::validateAsterAssetFactoryRecipe(recipe);
  assert(std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const aster::AsterAssetFactoryQualityDiagnostic &diagnostic) {
                       return diagnostic.category == "surface" &&
                              diagnostic.severity ==
                                  aster::AsterAssetFactoryDiagnosticSeverity::Error;
                     }));
  assert(std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const aster::AsterAssetFactoryQualityDiagnostic &diagnostic) {
                       return diagnostic.category == "physics" &&
                              diagnostic.message.find("non-positive") != std::string::npos;
                     }));
  const aster::AsterAssetFactoryBuildResult result =
      aster::buildAsterAssetFactoryRecipe(recipe);
  const aster::AsterAssetFactoryRecipeAudit audit =
      aster::auditAsterAssetFactoryBuild(recipe, result);
  const std::vector<std::string> lines = aster::describeAsterAssetFactoryAudit(audit);
  assert(std::any_of(lines.begin(), lines.end(), [](const std::string &line) {
    return line.find("surface") != std::string::npos;
  }));
}

void testDefaultAndHardwareVariants() {
  const aster::AsterPipeAsset reference_pipe = aster::makeAsterPipeAsset(testPipeSpec());
  assert(!hasPartContaining(reference_pipe, "flange"));
  assert(!hasPartContaining(reference_pipe, "bolt"));
  assert(hasPartContaining(reference_pipe, "weld"));

  aster::AsterPipeAssetSpec hardware_spec = testPipeSpec();
  hardware_spec.include_flanges = true;
  hardware_spec.include_bolts = true;
  const aster::AsterPipeAsset hardware_pipe = aster::makeAsterPipeAsset(hardware_spec);
  assert(hasPartContaining(hardware_pipe, "flange"));
  assert(hasPartContaining(hardware_pipe, "bolt"));

  const aster::AsterAssetFactoryBuildResult hardware_result =
      aster::buildAsterAssetFactoryRecipe(aster::makeAsterPipeFactoryRecipe(
          testPipeSpec(), aster::AsterPipeFactoryVariant::IndustrialHardware));
  assert(hardware_result.production_ready);
}

void testPhysicsProxyConversionAndQueries() {
  aster::AsterPipeAssetSpec dry = testPipeSpec();
  dry.wetness_strength = 0.0f;
  aster::AsterPipeAssetSpec wet = testPipeSpec();
  wet.wetness_strength = 0.45f;

  const aster::AsterAssetFactoryBuildResult dry_result =
      aster::buildAsterAssetFactoryRecipe(aster::makeAsterPipeFactoryRecipe(dry));
  const aster::AsterAssetFactoryBuildResult wet_result =
      aster::buildAsterAssetFactoryRecipe(aster::makeAsterPipeFactoryRecipe(wet));
  assert(dry_result.production_ready);
  assert(wet_result.production_ready);
  assert(dry_result.physics_bodies.front().material.friction >
         wet_result.physics_bodies.front().material.friction);
  assert(wet_result.physics_bodies.front().shape == aster::PhysicsShapeType::Box);
  assert(wet_result.physics_bodies.front().filter.query_enabled);

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  const aster::PhysicsBodyHandle pipe = world.addBody(wet_result.physics_bodies.front());

  aster::PhysicsRayHit ray_hit;
  assert(world.raycast({{0.0f, 1.4f, 0.0f}, {0.0f, -1.0f, 0.0f}, 2.2f}, ray_hit));
  assert(aster::samePhysicsHandle(ray_hit.body, pipe));

  aster::PhysicsShapeCastHit cast_hit;
  assert(world.castSphere({{0.0f, 1.4f, 0.0f}, {0.0f, -2.0f, 0.0f}, 0.12f}, cast_hit));
  assert(aster::samePhysicsHandle(cast_hit.body, pipe));

  const aster::PhysicsBodyHandle ball =
      world.addBody({aster::PhysicsBodyType::Dynamic,
                     aster::PhysicsShapeType::Sphere,
                     {0.0f, 1.25f, 0.0f},
                     {0.16f, 0.16f, 0.16f},
                     0.16f,
                     1.0f,
                     {0.50f, 0.0f}});
  for (int i = 0; i < 90; ++i) {
    world.step(1.0f / 60.0f);
  }
  assert(world.body(ball).position.y > 0.55f);
  assert(!world.contacts().empty());
}

void testRegistryFactoryNodes() {
  const aster::ProceduralNodeRegistry registry = aster::makeDefaultProceduralNodeRegistry();
  assert(registry.find("factory_recipe") != nullptr);
  assert(registry.find("factory_stage") != nullptr);
  assert(registry.find("surface_contract") != nullptr);
  assert(registry.find("physics_proxy") != nullptr);
  assert(registry.find("lod_recipe") != nullptr);
  assert(registry.find("quality_signal") != nullptr);
  assert(registry.find("visual_brief_claim") != nullptr);
}

void testAssetGraphFactoryReport() {
  const std::filesystem::path source_dir = ASTER_SOURCE_DIR;
  const std::filesystem::path assetc = ASTER_ASSETC_EXECUTABLE;
  if (assetc.empty() || !std::filesystem::exists(assetc)) {
    throw std::runtime_error("ASTER_ASSETC_EXECUTABLE is not available for factory graph test");
  }
  const std::filesystem::path graph =
      source_dir / "showcases" / "pipe_lab" / "rusted_pipe.astergraph";
  const std::filesystem::path packaged =
      std::filesystem::temp_directory_path() / "aster_asset_factory_graph";
  std::filesystem::remove_all(packaged);
  requireCommand(shellQuote(assetc) + " graph-package --input " + shellQuote(graph) +
                 " --output " + shellQuote(packaged));

  const std::filesystem::path assetgraphbin = findAssetGraphBin(packaged);
  if (assetgraphbin.empty()) {
    throw std::runtime_error("factory graph package did not emit an .assetgraphbin");
  }
  const aster::ProceduralAssetGraphPackage package =
      aster::loadProceduralAssetGraphPackage(assetgraphbin);
  assert(startsWith(package.factory_report.stable_recipe_hash, "0x"));
  assert(package.factory_report.stage_diagnostics.size() >= 6u);
  assert(std::all_of(package.factory_report.stage_diagnostics.begin(),
                     package.factory_report.stage_diagnostics.end(),
                     [](const aster::ProceduralAssetGraphFactoryStageReport &stage) {
                       return stage.status == "ready";
                     }));
  assert(std::any_of(package.factory_report.surface_signal_coverage.begin(),
                     package.factory_report.surface_signal_coverage.end(),
                     [](const aster::ProceduralAssetGraphFactorySignalCoverage &signal) {
                       return signal.signal == "raised_weld_rings" &&
                              signal.status == "claimed";
                     }));
  assert(package.factory_report.collision_proxy_summary.at("shape") ==
         "pipe-runtime-bounds");
  assert(std::find(package.factory_report.visual_brief_claims.begin(),
                   package.factory_report.visual_brief_claims.end(),
                   "reference_silhouette") !=
         package.factory_report.visual_brief_claims.end());
  assert(std::find(package.factory_report.visual_brief_rejections.begin(),
                   package.factory_report.visual_brief_rejections.end(),
                   "decorative_bolts_without_reference") !=
         package.factory_report.visual_brief_rejections.end());
  for (std::uint32_t bit = 57u; bit <= 63u; ++bit) {
    assert((package.feature_mask & (1ull << bit)) != 0u);
  }
}

} // namespace

int main() {
  std::cout << "asset_factory_tests: deterministic hash\n";
  testDeterministicRecipeHash();
  std::cout << "asset_factory_tests: recipe build\n";
  testRecipeBuildStagesAndLods();
  std::cout << "asset_factory_tests: dependency diagnostics\n";
  testStageDependencyDiagnostics();
  std::cout << "asset_factory_tests: validation diagnostics\n";
  testRecipeValidationCatchesBrokenContracts();
  std::cout << "asset_factory_tests: reference and hardware variants\n";
  testDefaultAndHardwareVariants();
  std::cout << "asset_factory_tests: physics proxy\n";
  testPhysicsProxyConversionAndQueries();
  std::cout << "asset_factory_tests: registry nodes\n";
  testRegistryFactoryNodes();
  std::cout << "asset_factory_tests: graph factory report\n";
  testAssetGraphFactoryReport();
  std::cout << "asset_factory_tests: passed\n";
  return 0;
}
