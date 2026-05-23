// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/asset_factory.hpp"
#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/asset/procedural_graph_runtime.hpp"
#include "aster/geometry/mesh_modeling.hpp"
#include "aster/render/visual_regression.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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

bool hasDiagnosticContaining(const aster::AsterAssetFoundryBuildResult &result,
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

bool hasClaim(const aster::AsterAssetFoundryBuildResult &result, const std::string &claim) {
  return std::find(result.visual_brief_claims.begin(), result.visual_brief_claims.end(), claim) !=
         result.visual_brief_claims.end();
}

bool hasRejectedSignal(const aster::AsterAssetFoundryBuildResult &result,
                       const std::string &signal) {
  return std::any_of(result.surface_coverages.begin(), result.surface_coverages.end(),
                     [&](const aster::AsterAssetFoundrySurfaceCoverage &coverage) {
                       return std::find(coverage.rejected_signals.begin(),
                                        coverage.rejected_signals.end(), signal) !=
                              coverage.rejected_signals.end();
                     });
}

bool hasVisualBriefStatus(const aster::AsterAssetFoundryRecipeAudit &audit,
                          const std::string &signal, const std::string &status) {
  return std::any_of(audit.visual_brief_rows.begin(), audit.visual_brief_rows.end(),
                     [&](const aster::AsterAssetFoundryVisualBriefRow &row) {
                       return row.signal == signal && row.status == status;
                     });
}

struct ArtifactImage {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<std::uint8_t> rgba8;
};

ArtifactImage makeImage(const std::uint32_t width, const std::uint32_t height,
                        const std::array<std::uint8_t, 4u> color) {
  ArtifactImage image;
  image.width = width;
  image.height = height;
  image.rgba8.resize(static_cast<std::size_t>(width) * height * 4u);
  for (std::size_t i = 0u; i + 3u < image.rgba8.size(); i += 4u) {
    image.rgba8[i] = color[0];
    image.rgba8[i + 1u] = color[1];
    image.rgba8[i + 2u] = color[2];
    image.rgba8[i + 3u] = color[3];
  }
  return image;
}

void putPixel(ArtifactImage &image, const int x, const int y,
              const std::array<std::uint8_t, 4u> color) {
  if (x < 0 || y < 0 || x >= static_cast<int>(image.width) ||
      y >= static_cast<int>(image.height)) {
    return;
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4u;
  image.rgba8[offset] = color[0];
  image.rgba8[offset + 1u] = color[1];
  image.rgba8[offset + 2u] = color[2];
  image.rgba8[offset + 3u] = color[3];
}

void fillRect(ArtifactImage &image, int x, int y, int width, int height,
              const std::array<std::uint8_t, 4u> color) {
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      putPixel(image, x + col, y + row, color);
    }
  }
}

void drawLine(ArtifactImage &image, int x0, int y0, const int x1, const int y1,
              const std::array<std::uint8_t, 4u> color) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    putPixel(image, x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void assertImageProof(const ArtifactImage &image) {
  assert(image.width >= 128u);
  assert(image.height >= 96u);
  std::size_t active = 0u;
  for (std::size_t i = 0u; i + 3u < image.rgba8.size(); i += 4u) {
    active += image.rgba8[i] > 48u || image.rgba8[i + 1u] > 48u || image.rgba8[i + 2u] > 48u
                  ? 1u
                  : 0u;
  }
  assert(active > image.width * image.height / 18u);
}

ArtifactImage makeGraphArtifact(const aster::AsterAssetFoundryRecipe &recipe,
                                const aster::AsterAssetFoundryBuildResult &result) {
  ArtifactImage image = makeImage(640u, 300u, {12u, 16u, 20u, 255u});
  fillRect(image, 0, 0, 640, 40, {30u, 38u, 46u, 255u});
  const int count = static_cast<int>(recipe.stages.size());
  for (int i = 0; i < count; ++i) {
    const int x = 28 + i * 86;
    const int y = 112 + ((i % 2) * 34);
    const bool passed = i < static_cast<int>(result.stage_reports.size()) &&
                        result.stage_reports[static_cast<std::size_t>(i)].passed;
    fillRect(image, x, y, 64, 44, passed ? std::array<std::uint8_t, 4u>{74u, 138u, 104u, 255u}
                                         : std::array<std::uint8_t, 4u>{148u, 72u, 62u, 255u});
    fillRect(image, x + 4, y + 4, 56, 8, {204u, 184u, 118u, 255u});
    if (i > 0) {
      drawLine(image, x - 22, y + 22, x, y + 22, {160u, 184u, 190u, 255u});
    }
  }
  const int quality_width = static_cast<int>(std::clamp(result.quality_score, 0u, 100u)) * 5;
  fillRect(image, 70, 246, quality_width, 18, {214u, 116u, 70u, 255u});
  fillRect(image, 70 + quality_width, 246, 500 - quality_width, 18, {52u, 58u, 66u, 255u});
  return image;
}

ArtifactImage makeSignalArtifact(const aster::AsterAssetFoundryBuildResult &result) {
  ArtifactImage image = makeImage(640u, 360u, {15u, 18u, 22u, 255u});
  const std::vector<aster::AsterAssetFoundrySurfaceSignalSummary> signals =
      aster::summarizeAsterAssetFoundrySurfaceSignals(result);
  int y = 30;
  for (const aster::AsterAssetFoundrySurfaceSignalSummary &signal : signals) {
    const int average = static_cast<int>(std::clamp(signal.average, 0.0f, 1.0f) * 420.0f);
    const int coverage = static_cast<int>(std::clamp(signal.coverage, 0.0f, 1.0f) * 420.0f);
    const bool rejected = signal.status == "rejected";
    fillRect(image, 30, y, 420, 10, {44u, 48u, 54u, 255u});
    fillRect(image, 30, y, average, 10,
             rejected ? std::array<std::uint8_t, 4u>{94u, 124u, 182u, 255u}
                      : std::array<std::uint8_t, 4u>{190u, 103u, 58u, 255u});
    fillRect(image, 30, y + 13, coverage, 8, {199u, 165u, 84u, 255u});
    y += 34;
    if (y > 320) {
      break;
    }
  }
  fillRect(image, 500, 32, 62, 62, {192u, 87u, 45u, 255u});
  fillRect(image, 520, 112, 78, 26, {50u, 52u, 56u, 255u});
  fillRect(image, 512, 154, 56, 32, {150u, 120u, 72u, 255u});
  return image;
}

ArtifactImage makeTopologyArtifact(const aster::CpuMesh &mesh) {
  ArtifactImage image = makeImage(640u, 360u, {10u, 13u, 17u, 255u});
  const aster::MeshBounds bounds = aster::calculateMeshBounds(mesh);
  const float span_x = std::max(bounds.max.x - bounds.min.x, 0.001f);
  const float span_z = std::max(bounds.max.z - bounds.min.z, 0.001f);
  const auto project = [&](const aster::Vec3 position) {
    const int x = 30 + static_cast<int>(((position.x - bounds.min.x) / span_x) * 580.0f);
    const int y = 330 - static_cast<int>(((position.z - bounds.min.z) / span_z) * 300.0f);
    return std::pair<int, int>{x, y};
  };
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const auto a = project(mesh.vertices[mesh.indices[i]].position);
    const auto b = project(mesh.vertices[mesh.indices[i + 1u]].position);
    const auto c = project(mesh.vertices[mesh.indices[i + 2u]].position);
    const std::array<std::uint8_t, 4u> color{
        static_cast<std::uint8_t>(80u + (i / 3u) % 120u), 112u, 138u, 255u};
    drawLine(image, a.first, a.second, b.first, b.second, color);
    drawLine(image, b.first, b.second, c.first, c.second, color);
    drawLine(image, c.first, c.second, a.first, a.second, color);
  }
  return image;
}

ArtifactImage makeProofBadgeArtifact(const aster::AsterAssetFoundryBuildResult &result) {
  ArtifactImage image = makeImage(640u, 360u, {9u, 12u, 16u, 255u});
  fillRect(image, 44, 42, 552, 80, result.production_ready
                                     ? std::array<std::uint8_t, 4u>{46u, 118u, 82u, 255u}
                                     : std::array<std::uint8_t, 4u>{128u, 66u, 58u, 255u});
  const int quality = static_cast<int>(std::clamp(result.quality_score, 0u, 100u));
  fillRect(image, 72, 150, quality * 5, 22, {210u, 118u, 58u, 255u});
  fillRect(image, 72 + quality * 5, 150, 500 - quality * 5, 22, {45u, 50u, 58u, 255u});
  const std::array<std::uint8_t, 4u> claim{186u, 134u, 72u, 255u};
  const std::array<std::uint8_t, 4u> reject{86u, 118u, 176u, 255u};
  for (std::size_t i = 0u; i < result.visual_brief_claims.size() && i < 7u; ++i) {
    fillRect(image, 78 + static_cast<int>(i) * 68, 210, 46, 46, claim);
    fillRect(image, 86 + static_cast<int>(i) * 68, 218, 30, 8, {240u, 198u, 112u, 255u});
  }
  for (std::size_t i = 0u; i < result.visual_brief_rejections.size() && i < 4u; ++i) {
    fillRect(image, 112 + static_cast<int>(i) * 96, 284, 58, 26, reject);
  }
  drawLine(image, 44, 122, 596, 42, {148u, 174u, 182u, 255u});
  drawLine(image, 44, 42, 596, 122, {148u, 174u, 182u, 255u});
  return image;
}

ArtifactImage makeContactSheet(const std::vector<ArtifactImage> &images) {
  ArtifactImage sheet = makeImage(1280u, 720u, {7u, 9u, 12u, 255u});
  for (std::size_t index = 0u; index < images.size() && index < 4u; ++index) {
    const ArtifactImage &source = images[index];
    const int offset_x = index % 2u == 0u ? 0 : 640;
    const int offset_y = index < 2u ? 0 : 360;
    for (std::uint32_t y = 0u; y < std::min(source.height, 360u); ++y) {
      for (std::uint32_t x = 0u; x < std::min(source.width, 640u); ++x) {
        const std::size_t src = (static_cast<std::size_t>(y) * source.width + x) * 4u;
        putPixel(sheet, offset_x + static_cast<int>(x), offset_y + static_cast<int>(y),
                 {source.rgba8[src], source.rgba8[src + 1u], source.rgba8[src + 2u],
                  source.rgba8[src + 3u]});
      }
    }
  }
  return sheet;
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
  const aster::AsterAssetFoundryRecipe recipe =
      aster::makeAsterPipeFoundryRecipe(testPipeSpec());
  const std::string first = aster::stableAsterAssetFoundryRecipeHash(recipe);
  const std::string second = aster::stableAsterAssetFoundryRecipeHash(recipe);
  assert(first == second);
  assert(startsWith(first, "aster-factory-0x"));

  aster::AsterPipeAssetSpec changed = testPipeSpec();
  changed.seed ^= 0x51u;
  const std::string changed_hash =
      aster::stableAsterAssetFoundryRecipeHash(aster::makeAsterPipeFoundryRecipe(changed));
  assert(first != changed_hash);
}

void testRecipeBuildStagesAndLods() {
  const aster::AsterAssetFoundryRecipe recipe =
      aster::makeAsterPipeFoundryRecipe(testPipeSpec());
  const aster::AsterAssetFoundryBuildResult result =
      aster::buildAsterAssetFoundryRecipe(recipe);
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
  const std::vector<aster::AsterAssetFoundryQualityDiagnostic> validation =
      aster::validateAsterAssetFoundryRecipe(recipe);
  assert(std::none_of(validation.begin(), validation.end(),
                      [](const aster::AsterAssetFoundryQualityDiagnostic &diagnostic) {
                        return diagnostic.severity ==
                               aster::AsterAssetFoundryDiagnosticSeverity::Error;
                      }));
  const aster::AsterAssetFoundryRecipeAudit audit =
      aster::auditAsterAssetFoundryBuild(recipe, result);
  assert(audit.production_ready);
  assert(audit.stage_order.size() == recipe.stages.size());
  assert(audit.missing_dependencies.empty());
  assert(audit.lod_summary.size() == 3u);
  assert(audit.lod_summary[1].triangle_ratio < audit.lod_summary[0].triangle_ratio);
  assert(audit.surface_contract_ids.front() == "surface.pipe.corrosion");
  assert(audit.physics_proxy_ids.front() == "physics.pipe.runtime-bounds");
  assert(hasVisualBriefStatus(audit, "raised_weld_rings", "claimed"));
  assert(hasVisualBriefStatus(audit, "decorative_bolts_without_reference", "rejected"));
  const std::vector<aster::AsterAssetFoundryPhysicsProxySummary> proxy_summary =
      aster::summarizeAsterAssetFoundryPhysicsProxies(recipe);
  assert(proxy_summary.size() == 1u);
  assert(proxy_summary.front().shape == "bounds-box");
  assert(proxy_summary.front().covers_render_bounds);
  assert(proxy_summary.front().friction > 0.70f);
  const std::vector<std::string> audit_lines = aster::describeAsterAssetFoundryAudit(audit);
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
  aster::AsterAssetFoundryRecipe recipe = aster::makeAsterPipeFoundryRecipe(testPipeSpec());
  std::swap(recipe.stages[0], recipe.stages[1]);
  const aster::AsterAssetFoundryBuildResult result =
      aster::buildAsterAssetFoundryRecipe(recipe);
  assert(!result.production_ready);
  assert(hasDiagnosticContaining(result, "stage dependency has not completed"));
  const aster::AsterAssetFoundryRecipeAudit audit =
      aster::auditAsterAssetFoundryBuild(recipe, result);
  assert(!audit.missing_dependencies.empty());
  assert(std::any_of(audit.diagnostics.begin(), audit.diagnostics.end(),
                     [](const aster::AsterAssetFoundryQualityDiagnostic &diagnostic) {
                       return diagnostic.severity ==
                              aster::AsterAssetFoundryDiagnosticSeverity::Error;
                     }));
}

void testRecipeValidationCatchesBrokenContracts() {
  aster::AsterAssetFoundryRecipe recipe = aster::makeAsterPipeFoundryRecipe(testPipeSpec());
  recipe.surface_contracts.clear();
  recipe.physics_proxies.front().half_extents.y = 0.0f;
  const std::vector<aster::AsterAssetFoundryQualityDiagnostic> diagnostics =
      aster::validateAsterAssetFoundryRecipe(recipe);
  assert(std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const aster::AsterAssetFoundryQualityDiagnostic &diagnostic) {
                       return diagnostic.category == "surface" &&
                              diagnostic.severity ==
                                  aster::AsterAssetFoundryDiagnosticSeverity::Error;
                     }));
  assert(std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const aster::AsterAssetFoundryQualityDiagnostic &diagnostic) {
                       return diagnostic.category == "physics" &&
                              diagnostic.message.find("non-positive") != std::string::npos;
                     }));
  const aster::AsterAssetFoundryBuildResult result =
      aster::buildAsterAssetFoundryRecipe(recipe);
  const aster::AsterAssetFoundryRecipeAudit audit =
      aster::auditAsterAssetFoundryBuild(recipe, result);
  const std::vector<std::string> lines = aster::describeAsterAssetFoundryAudit(audit);
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

  const aster::AsterAssetFoundryBuildResult hardware_result =
      aster::buildAsterAssetFoundryRecipe(aster::makeAsterPipeFoundryRecipe(
          testPipeSpec(), aster::AsterPipeFoundryVariant::IndustrialHardware));
  assert(hardware_result.production_ready);
}

void testPhysicsProxyConversionAndQueries() {
  aster::AsterPipeAssetSpec dry = testPipeSpec();
  dry.wetness_strength = 0.0f;
  aster::AsterPipeAssetSpec wet = testPipeSpec();
  wet.wetness_strength = 0.45f;

  const aster::AsterAssetFoundryBuildResult dry_result =
      aster::buildAsterAssetFoundryRecipe(aster::makeAsterPipeFoundryRecipe(dry));
  const aster::AsterAssetFoundryBuildResult wet_result =
      aster::buildAsterAssetFoundryRecipe(aster::makeAsterPipeFoundryRecipe(wet));
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

void writeArtifactPng(const std::filesystem::path &path, const ArtifactImage &image) {
  assertImageProof(image);
  aster::writeRgbaPng(path, static_cast<int>(image.width), static_cast<int>(image.height),
                      image.rgba8);
  assert(std::filesystem::exists(path));
}

void writeIndustrialConduitArtifacts(const std::filesystem::path &directory) {
  std::filesystem::create_directories(directory);
  aster::AsterPipeAssetSpec spec = testPipeSpec();
  spec.asset_id = "asset.pipe.industrial_conduit_proof";
  spec.length = 6.4f;
  spec.outer_radius = 0.46f;
  spec.wall_thickness = 0.082f;
  spec.rust_strength = 1.0f;
  spec.pitting_density = 1.22f;
  spec.oxide_layering = 0.94f;
  spec.cavity_grime_strength = 0.88f;
  spec.axial_scratch_strength = 0.84f;
  spec.weld_slag_strength = 0.92f;
  spec.rim_soot_strength = 0.96f;
  spec.include_longitudinal_seam = true;
  aster::AsterAssetFoundryRecipe recipe = aster::makeAsterPipeFoundryRecipe(spec);
  recipe.proof_artifacts.push_back({.id = "proof.industrial_conduit.graph",
                                    .role = "graph",
                                    .path = "industrial_conduit_graph.png",
                                    .kind = "png",
                                    .width = 640u,
                                    .height = 300u,
                                    .signal_tags = {"foundry", "stage-order"}});
  recipe.proof_artifacts.push_back({.id = "proof.industrial_conduit.surface",
                                    .role = "surface-signals",
                                    .path = "industrial_conduit_surface_signals.png",
                                    .kind = "png",
                                    .width = 640u,
                                    .height = 360u,
                                    .signal_tags = {"rust", "oxide", "weld", "rim"}});
  const aster::AsterAssetFoundryBuildResult result =
      aster::buildAsterAssetFoundryRecipe(recipe);
  assert(result.production_ready);
  assert(hasClaim(result, "corroded_orange_brown_rust"));
  assert(hasClaim(result, "dark_oxide_cavities"));
  assert(hasClaim(result, "raised_weld_rings"));
  assert(hasClaim(result, "open_hollow_rims"));
  assert(hasRejectedSignal(result, "clean_plastic_surface"));
  assert(hasRejectedSignal(result, "monochrome_material"));
  const aster::MeshTopologyReport topology = aster::validateMeshTopology(result.mesh);
  assert(topology.indexable());
  assert(topology.degenerate_triangles == 0u);

  ArtifactImage graph = makeGraphArtifact(recipe, result);
  ArtifactImage signals = makeSignalArtifact(result);
  ArtifactImage topology_image = makeTopologyArtifact(result.mesh);
  ArtifactImage proof = makeProofBadgeArtifact(result);
  ArtifactImage contact = makeContactSheet({graph, signals, topology_image, proof});
  writeArtifactPng(directory / "industrial_conduit_graph.png", graph);
  writeArtifactPng(directory / "industrial_conduit_surface_signals.png", signals);
  writeArtifactPng(directory / "industrial_conduit_topology_overlay.png", topology_image);
  writeArtifactPng(directory / "industrial_conduit_contact_sheet.png", contact);

  std::ofstream readme(directory / "README.md");
  readme << "# Aster Headless Foundry Artifacts\n\n";
  readme << "Generated by `aster_asset_factory_tests write-artifacts`.\n\n";
  readme << "- `industrial_conduit_graph.png`: recipe stage graph and quality bar.\n";
  readme << "- `industrial_conduit_surface_signals.png`: required and rejected visual signals.\n";
  readme << "- `industrial_conduit_topology_overlay.png`: render mesh topology projection.\n";
  readme << "- `industrial_conduit_contact_sheet.png`: combined review sheet.\n\n";
  readme << "Visual gate claims rust, oxide cavities, raised weld/rim contact detail, pitting, "
            "and axial scratches while rejecting clean plastic and monochrome material reads.\n";
  readme.close();
  assert(std::filesystem::exists(directory / "README.md"));
}

void testIndustrialConduitArtifactWriter() {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "aster_asset_foundry_headless_artifacts_test";
  std::filesystem::remove_all(directory);
  writeIndustrialConduitArtifacts(directory);
  assert(std::filesystem::exists(directory / "industrial_conduit_graph.png"));
  assert(std::filesystem::exists(directory / "industrial_conduit_surface_signals.png"));
  assert(std::filesystem::exists(directory / "industrial_conduit_topology_overlay.png"));
  assert(std::filesystem::exists(directory / "industrial_conduit_contact_sheet.png"));
  const std::string readme = readText(directory / "README.md");
  assert(readme.find("industrial_conduit_graph.png") != std::string::npos);
  assert(readme.find("industrial_conduit_surface_signals.png") != std::string::npos);
  std::filesystem::remove_all(directory);
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::string_view(argv[1]) == "write-artifacts") {
    writeIndustrialConduitArtifacts(std::filesystem::path(ASTER_SOURCE_DIR) / "tests" /
                                    "artifacts" / "asset_foundry_headless");
    return 0;
  }
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
  std::cout << "asset_factory_tests: industrial conduit artifacts\n";
  testIndustrialConduitArtifactWriter();
  std::cout << "asset_factory_tests: passed\n";
  return 0;
}
