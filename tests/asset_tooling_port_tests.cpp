// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "test_support.hpp"

#include "aster/asset/asset_io.hpp"
#include "aster/asset/asset_foundry.hpp"
#include "aster/asset/asset_library.hpp"
#include "aster/asset/asset_modifier_stack.hpp"
#include "aster/asset/asset_registry.hpp"
#include "aster/asset/derived_asset_cache.hpp"
#include "aster/asset/procedural_graph_runtime.hpp"
#include "aster/geometry/geometry_operations.hpp"
#include "aster/geometry/mesh_authoring.hpp"
#include "aster/physics/xpbd_authoring.hpp"
#include "aster/physics/xpbd_constraints.hpp"
#include "aster/render/preview_compositor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void assertUserFacingOwnershipLanguageIsClean() {
  const std::filesystem::path root = ASTER_SOURCE_DIR;
  const std::vector<std::filesystem::path> paths{
      root / "README.md",
      root / "docs",
      root / "include" / "aster",
      root / "src",
      root / "apps",
  };
  const std::vector<std::string> banned{
      std::string("Asset") + "Factory",
      std::string("FarukAlpay") + "Asset" + "Factory",
      std::string("Asset") + " Factory",
      std::string("Asset") + "Factory-inspired",
      "FarukAlpayEngine",
      "UObject",
      "UCLASS",
      "UPROPERTY",
      "UFUNCTION",
      "ThirdPartyNot",
      "Third Party Notices",
  };
  for (const std::filesystem::path &path : paths) {
    if (std::filesystem::is_regular_file(path)) {
      const std::string text = readText(path);
      for (const std::string &term : banned) {
        assert(text.find(term) == std::string::npos);
      }
      continue;
    }
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::recursive_directory_iterator(path)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const std::string text = readText(entry.path());
      for (const std::string &term : banned) {
        assert(text.find(term) == std::string::npos);
      }
    }
  }
}

aster::AssetDatabase makeTinyDatabase() {
  aster::AssetDatabase database;
  database.schema_version = 2u;
  database.platform = "desktop";
  database.source_path = "assetdb.asterdb.json";
  database.asset_graph.edges.push_back(
      {.from = "material.wet", .to = "texture.albedo", .role = "albedo", .present = true});
  aster::AssetDatabaseRecord record;
  record.guid = "asset-guid-1";
  record.id = "material.wet";
  record.kind = "material";
  record.source_path = "materials/wet.astermat";
  record.platform = "desktop";
  record.import_preset.name = "production";
  record.import_preset.lod_policy = "lod-chain";
  record.derived_hashes.source_hash = "source-hash-wet";
  record.derived_hashes.dependency_hash = "dependency-hash-wet";
  record.derived_hashes.artifact_hash = "artifact-hash-wet";
  record.derived_hashes.pipeline_cache_key = "pipeline-wet";
  record.fate_report.production_ready = true;
  record.outputs.push_back({.role = "preview", .kind = "ppm", .path = "preview/wet.ppm"});
  record.dependency_edges.push_back(
      {.from = "material.wet", .to = "texture.albedo", .role = "albedo", .present = true});
  database.records.push_back(record);
  return database;
}

void assertAssetLibrary() {
  const aster::AssetDatabase database = makeTinyDatabase();
  const aster::AssetLibrary library =
      aster::AssetLibrary::fromDatabase(database, std::filesystem::temp_directory_path());
  assert(library.assets.size() == 1u);
  assert(library.catalogs.size() == 1u);
  assert(library.find("material.wet") != nullptr);
  assert(library.assetsInCatalog("Assets/Material").size() == 1u);
  assert(library.catalog_tree.findChild("Assets") != nullptr);
  assert(!library.assets.front().tags.empty());
  assert(!library.assets.front().dependency_ids.empty());
  assert(!library.assets.front().catalog_path.empty());
  assert(!library.assets.front().metadata.empty());

  aster::AssetCatalogPath dirty_path(" Assets//Material:Runtime ");
  const aster::AssetCatalogPath clean_path = dirty_path.cleanup();
  assert(clean_path.str() == "Assets/Material-Runtime");
  assert(clean_path.simpleName() == "Material-Runtime");
  assert(clean_path.isContainedIn(aster::AssetCatalogPath("Assets")));
  assert(clean_path.rebase(aster::AssetCatalogPath("Assets"),
                           aster::AssetCatalogPath("Library")).str() ==
         "Library/Material-Runtime");

  aster::AssetCatalogStore store = aster::makeAssetCatalogStoreFromLibrary(library);
  assert(store.catalogs.size() == 1u);
  assert(store.findByPath(aster::AssetCatalogPath("Assets/Material")) != nullptr);
  assert(store.buildTree().findChild("Assets") != nullptr);
  aster::AssetCatalogRecord extra;
  extra.path = aster::AssetCatalogPath("Assets/Generated");
  extra.tags = {"generated"};
  store.upsert(extra);
  assert(store.findByPath(aster::AssetCatalogPath("Assets/Generated")) != nullptr);
  assert(!aster::stableAssetCatalogId(aster::AssetCatalogPath("Assets/Generated")).empty());
  const aster::AssetLibraryManifest manifest = aster::buildAssetLibraryManifest(library);
  assert(!manifest.catalogs.empty());

  const aster::AssetFoundryReport foundry = aster::buildAssetFoundryReport(library);
  assert(foundry.catalog_audit.asset_count == 1u);
  assert(foundry.catalog_audit.production_ready_assets == 1u);
  assert(foundry.import_recipes.size() == 1u);
  assert(foundry.import_recipes.front().catalog_path.str() == "Assets/Material");
  assert(!foundry.import_recipes.front().production_readiness_reasons.empty());
  assert(std::find(foundry.catalog_audit.production_readiness_reasons.begin(),
                   foundry.catalog_audit.production_readiness_reasons.end(),
                   "production-ready") !=
         foundry.catalog_audit.production_readiness_reasons.end());

  const aster::CookLineageReport lineage = aster::buildCookLineageReport(database);
  assert(lineage.asset_count == 1u);
  assert(lineage.production_ready_assets == 1u);
  assert(lineage.assets.front().id == "material.wet");
  assert(!lineage.assets.front().production_readiness_reasons.empty());

  aster::NodePreviewCache preview_cache;
  preview_cache.put({.node_id = "material.wet.preview",
                     .refresh_state = 1u,
                     .width = 1u,
                     .height = 1u,
                     .content_hash = 0xabcdu,
                     .rgba8 = {255u, 255u, 255u, 255u}});
  const aster::AsterAssetFoundryStory story =
      aster::buildAsterAssetFoundryStory(library, lineage, &preview_cache);
  assert(story.production_ready);
  assert(story.asset_count == 1u);
  assert(story.production_ready_assets == 1u);
  assert(story.dependency_edge_count == 1u);
  assert(story.preview_artifacts == 1u);
  assert(story.node_preview_records == 1u);
  assert(std::any_of(story.steps.begin(), story.steps.end(),
                     [](const aster::AsterAssetFoundryLineageStep &step) {
                       return step.id == "cook-lineage" && step.ready;
                     }));
  assert(std::any_of(story.steps.begin(), story.steps.end(),
                     [](const aster::AsterAssetFoundryLineageStep &step) {
                       return step.id == "stable-guid-hash" && step.ready;
                     }));
}

void assertAssetRegistryAndDerivedCache() {
  const aster::AssetDatabase database = makeTinyDatabase();
  const aster::AssetLibrary library =
      aster::AssetLibrary::fromDatabase(database, std::filesystem::temp_directory_path());

  aster::AssetRegistry registry;
  std::vector<std::string> registry_events;
  aster::SignalConnection registry_connection =
      registry.changes().connect([&](const aster::AssetRegistryChangeEvent &event) {
        registry_events.push_back(aster::assetRegistryChangeKindName(event.kind));
      });
  registry.scanLibrary(library);
  assert(registry.records().size() == 1u);
  assert(!registry_events.empty());
  assert(registry.find("material.wet") != nullptr);
  assert(registry.find("asset-guid-1") != nullptr);
  assert(registry.assetsByKind("material").size() == 1u);
  assert(registry.assetsByCatalogPath("Assets", true).size() == 1u);
  assert(registry.query({.tags = {"material", "production-ready"}}).size() == 1u);
  assert(registry.query({.metadata_equals = {{"kind", "material"}}}).size() == 1u);
  const std::vector<std::string> dependencies = registry.dependencies("material.wet");
  assert(dependencies.size() == 1u);
  assert(dependencies.front() == "texture.albedo");
  const std::vector<std::string> referencers = registry.referencers("texture.albedo");
  assert(referencers.size() == 1u);
  assert(referencers.front() == "material.wet");

  aster::AssetRegistryRecord texture;
  texture.id = "texture.albedo";
  texture.guid = "texture-guid-1";
  texture.name = "Wet Albedo";
  texture.kind = "texture";
  texture.catalog_path = "Assets/Texture";
  texture.tags = {"texture", "runtime"};
  assert(registry.upsert(texture));
  assert(registry.records().size() == 2u);
  assert(registry.assetsByKind("texture").size() == 1u);
  assert(registry.catalogPaths().size() == 2u);
  assert(registry.remove("texture-guid-1"));
  assert(registry.records().size() == 1u);
  registry_connection.disconnect();

  const std::filesystem::path cache_root =
      std::filesystem::temp_directory_path() / "aster_derived_asset_cache_test";
  std::filesystem::remove_all(cache_root);
  aster::DerivedAssetCache cache(cache_root, aster::DerivedAssetCacheBackend::MemoryAndFilesystem,
                                 {.worker_count = 1u, .deterministic = true});
  std::vector<std::string> cache_events;
  aster::SignalConnection cache_connection =
      cache.events().connect([&](const aster::DerivedAssetCacheEvent &event) {
        cache_events.push_back(aster::derivedAssetCacheEventKindName(event.kind));
      });
  const std::string key =
      aster::DerivedAssetCache::buildCacheKey("mesh bake", "v1", "materials/wet:rock");
  assert(key.find('/') == std::string::npos);
  assert(key.find(':') == std::string::npos);
  aster::DerivedAssetBytes bytes;
  assert(!cache.get(key, bytes));

  const aster::DerivedAssetAsyncHandle handle =
      cache.buildAsync({.plugin_name = "mesh bake",
                        .version = "v1",
                        .key_suffix = "materials/wet:rock",
                        .build = []() {
                          return aster::DerivedAssetBytes{1u, 2u, 3u, 5u, 8u};
                        }});
  assert(!cache.pollAsyncCompletion(handle));
  cache.waitForIdle();
  assert(cache.pollAsyncCompletion(handle));
  bool data_was_built = false;
  assert(cache.getAsyncResult(handle, bytes, &data_was_built));
  assert(data_was_built);
  assert(bytes.size() == 5u);
  assert(std::filesystem::exists(cache.filePathForKey(key)));

  cache.clearMemory();
  aster::DerivedAssetBytes from_disk;
  assert(cache.get(key, from_disk));
  assert(from_disk == bytes);
  const aster::DerivedAssetAsyncHandle cached_handle =
      cache.buildAsync({.plugin_name = "mesh bake",
                        .version = "v1",
                        .key_suffix = "materials/wet:rock",
                        .build = []() {
                          return aster::DerivedAssetBytes{99u};
                        }});
  assert(cache.pollAsyncCompletion(cached_handle));
  aster::DerivedAssetBytes cached_bytes;
  bool cached_was_built = true;
  assert(cache.getAsyncResult(cached_handle, cached_bytes, &cached_was_built));
  assert(!cached_was_built);
  assert(cached_bytes == bytes);
  const aster::DerivedAssetCacheStats stats = cache.stats();
  assert(stats.builds == 1u);
  assert(stats.hits >= 2u);
  assert(stats.misses >= 1u);
  assert(!cache_events.empty());
  cache_connection.disconnect();
  std::filesystem::remove_all(cache_root);
}

void assertMeshAuthoringAndModifiers() {
  const aster::CpuMesh box = aster::makeBox();
  aster::EditableMesh editable = aster::editableMeshFromCpuMesh(box, "box");
  aster::MeshAuthoringReport validate = aster::validateEditableMesh(editable);
  assert(validate.ok);

  aster::MeshAuthoringReport extrude_report;
  editable = aster::extrudeEditableFaces(editable, 0.1f, &extrude_report);
  assert(extrude_report.output_faces > extrude_report.input_faces);

  aster::MeshAuthoringReport tri_report;
  editable = aster::triangulateEditableMesh(editable, &tri_report);
  assert(tri_report.generated_triangles == editable.faces.size());

  aster::CpuMesh cooked = aster::cpuMeshFromEditableMesh(editable);
  assert(!cooked.vertices.empty());
  assert(cooked.indices.size() % 3u == 0u);

  aster::AssetModifierStack stack;
  stack.asset_id = "mesh.box";
  stack.source_provenance_id = "authoring-test";
  stack.modifiers.push_back({.id = "tri", .kind = aster::AssetModifierKind::Triangulate});
  stack.modifiers.push_back({.id = "weld",
                             .kind = aster::AssetModifierKind::Weld,
                             .epsilon = 0.0001f,
                             .creative_variant_tags = {"wear:clean"}});
  stack.modifiers.push_back({.id = "array",
                             .kind = aster::AssetModifierKind::Array,
                             .transform = {.position = {1.4f, 0.0f, 0.0f}},
                             .count = 2u,
                             .creative_variant_tags = {"layout:paired"}});
  stack.modifiers.push_back({.id = "solidify",
                             .kind = aster::AssetModifierKind::Solidify,
                             .amount = 0.02f});
  stack.modifiers.push_back({.id = "smooth",
                             .kind = aster::AssetModifierKind::Smooth,
                             .amount = 0.12f,
                             .count = 1u});
  stack.modifiers.push_back({.id = "weighted",
                             .kind = aster::AssetModifierKind::WeightedNormal});
  stack.modifiers.push_back({.id = "decimate",
                             .kind = aster::AssetModifierKind::Decimate,
                             .amount = 0.85f,
                             .seed = 7u});
  const aster::AssetModifierStackResult result = aster::applyAssetModifierStack(box, stack);
  assert(!result.report.stable_provenance_id.empty());
  assert(!result.mesh.vertices.empty());
  assert(result.report.quality_score > 0u);
  assert(result.report.creative_variant_tags.size() == 2u);
  assert(result.report.output_vertices == result.mesh.vertices.size());
  assert(result.report.output_indices == result.mesh.indices.size());
  assert(result.mesh.indices.size() >= box.indices.size());
  assert(aster::assetModifierKindName(aster::AssetModifierKind::Solidify) == "solidify");

  aster::MeshAuthoringRecipe recipe;
  recipe.id = "recipe.box.variant";
  recipe.provenance_id = "authoring-recipe-test";
  recipe.source_mesh = box;
  recipe.variant_intent_tags = {"profile:test"};
  recipe.steps.push_back({.id = "tri", .kind = aster::MeshAuthoringRecipeStepKind::Triangulate});
  recipe.steps.push_back({.id = "uv",
                          .kind = aster::MeshAuthoringRecipeStepKind::UvPack,
                          .uv_policy = {.padding = 0.04f},
                          .variant_intent_tags = {"uv:packed"}});
  recipe.steps.push_back({.id = "normals",
                          .kind = aster::MeshAuthoringRecipeStepKind::RecalculateNormals});
  const aster::MeshAuthoringRecipeResult recipe_result =
      aster::applyMeshAuthoringRecipe(recipe);
  assert(!recipe_result.mesh.vertices.empty());
  assert(recipe_result.quality_score > 0u);
  assert(recipe_result.reports.size() == 3u);
  assert(std::find(recipe_result.variant_intent_tags.begin(),
                   recipe_result.variant_intent_tags.end(), "uv:packed") !=
         recipe_result.variant_intent_tags.end());
  assert(aster::summarizeMeshAuthoringRecipe(recipe_result).find("quality=") !=
         std::string::npos);

  aster::MeshBooleanRequest boolean_request;
  boolean_request.operation = aster::MeshBooleanOperation::Union;
  boolean_request.solver = aster::MeshBooleanSolver::AsterReference;
  boolean_request.variant_intent_tags = {"boolean:union-proxy"};
  boolean_request.inputs.push_back({.label = "a", .mesh = box});
  boolean_request.inputs.push_back({.label = "b",
                                    .mesh = box,
                                    .transform = {.position = {1.25f, 0.0f, 0.0f}}});
  const aster::MeshBooleanResult boolean_result =
      aster::evaluateMeshBooleanRequest(boolean_request);
  assert(boolean_result.report.ok);
  assert(boolean_result.mesh.vertices.size() == box.vertices.size() * 2u);
  assert(aster::meshBooleanOperationName(aster::MeshBooleanOperation::Union) ==
         std::string("union"));
}

void assertGeometryOperations() {
  aster::GeometrySet set;
  set.meshes.push_back({.id = "a", .mesh = aster::makeBox()});
  aster::GeometryMeshPart shifted{.id = "b", .mesh = aster::makeBox()};
  shifted.transform.position = {3.0f, 0.0f, 0.0f};
  set.meshes.push_back(shifted);
  const aster::CpuMesh joined = aster::joinGeometryMeshes(set);
  assert(joined.vertices.size() == aster::makeBox().vertices.size() * 2u);
  assert(aster::separateDisconnectedMeshIslands(joined).size() >= 2u);
  const std::vector<aster::GeometryPoint> points =
      aster::scatterPointsOnMesh(joined, {.count = 8u, .seed = 42u, .radius = 0.5f});
  const std::vector<aster::GeometryPoint> points_again =
      aster::scatterPointsOnMesh(joined, {.count = 8u, .seed = 42u, .radius = 0.5f});
  assert(points.size() == 8u);
  assert(points.front().position.x == points_again.front().position.x);
  assert(aster::instanceMeshOnPoints(aster::makePlane(1.0f), points).meshes.size() == 8u);
}

void assertMeshIo() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "aster_mesh_io_port_test";
  std::filesystem::create_directories(dir);
  const std::filesystem::path obj = dir / "tri.obj";
  {
    std::ofstream file(obj);
    file << "o conduit_piece\n";
    file << "g rim_band\n";
    file << "usemtl rusted_metal\n";
    file << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    file << "vt 0 0\nvt 1 0\nvt 0 1\n";
    file << "f 1/1 2/2 3/3\n";
  }
  const aster::AssetMeshImportResult imported = aster::importMeshAsset(obj);
  assert(imported.report.ok);
  assert(imported.mesh.indices.size() == 3u);
  assert(imported.report.source.objects.size() == 1u);
  assert(imported.report.source.objects.front() == "conduit_piece");
  assert(imported.report.source.groups.front() == "rim_band");
  assert(imported.report.source.materials.front() == "rusted_metal");
  assert(imported.report.source.facets.size() == 1u);
  assert(imported.report.source.facets.front().triangle_count == 1u);

  aster::AssetMeshImportOptions scaled_options;
  scaled_options.unit_scale = 2.0f;
  scaled_options.source_up = aster::AssetMeshAxis::PositiveY;
  scaled_options.source_forward = aster::AssetMeshAxis::PositiveZ;
  scaled_options.target_up = aster::AssetMeshAxis::PositiveZ;
  scaled_options.target_forward = aster::AssetMeshAxis::NegativeY;
  const aster::AssetMeshImportResult normalized = aster::importMeshAsset(obj, scaled_options);
  assert(normalized.report.ok);
  assert(aster::assetMeshAxisName(aster::AssetMeshAxis::NegativeY) == "-y");
  assert(std::abs(aster::length(normalized.mesh.vertices[1].position) - 2.0f) < 0.001f);
  const aster::AssetMeshIoReport exported =
      aster::exportMeshAssetObj(imported.mesh, dir / "tri_export.obj");
  assert(exported.ok);
  assert(!exported.stable_source_hash.empty());
  const aster::AssetMeshIoReport exported_ply =
      aster::exportMeshAssetPly(imported.mesh, dir / "tri_export.ply");
  const aster::AssetMeshIoReport exported_stl =
      aster::exportMeshAssetStl(imported.mesh, dir / "tri_export.stl");
  assert(exported_ply.ok);
  assert(exported_stl.ok);
  assert(aster::importMeshAsset(dir / "tri_export.ply").report.ok);
  assert(aster::importMeshAsset(dir / "tri_export.stl").report.ok);
  const aster::AssetMeshImportResult unsupported =
      aster::importMeshAsset(dir / "not_vendored.fbx", aster::AssetMeshFormat::Fbx);
  assert(!unsupported.report.ok);
  assert(!unsupported.report.diagnostics.empty());
  const aster::AssetMeshImportResult unsupported_gltf =
      aster::importMeshAsset(dir / "not_vendored.glb", aster::AssetMeshFormat::Gltf);
  assert(!unsupported_gltf.report.ok);
  assert(unsupported_gltf.report.diagnostics.front().find("not vendored") !=
         std::string::npos);
}

void assertProceduralRuntime() {
  aster::ProceduralAssetGraphPackage package;
  package.id = "graph.runtime";
  package.asset_guid = "graph-guid";
  package.runtime_model = "aster-runtime";
  package.mesh.primitive = "sphere";
  package.quality.score = 96u;
  package.quality.production_ready = true;
  package.factory_report.visual_brief_claims = {"runtime_graph_mesh"};
  package.factory_report.visual_brief_rejections = {"empty_mesh"};
  package.proof_artifacts.push_back({.id = "proof.runtime.preview",
                                     .role = "preview",
                                     .path = "tests/artifacts/asset_foundry_headless/preview.png",
                                     .kind = "png",
                                     .width = 64u,
                                     .height = 64u,
                                     .signal_tags = {"runtime", "graph"}});
  package.nodes.push_back({.id = "mesh.surface",
                           .kind = "mesh_primitive",
                           .role = "mesh",
                           .params = {{"primitive", "sphere"}},
                           .capability_status = "runtime-reference"});
  package.nodes.push_back({.id = "modifier.bevel",
                           .kind = "bevel_modifier",
                           .role = "modifier",
                           .params = {{"width", "0.02"}},
                           .capability_status = "runtime-procedural-reference"});
  package.nodes.push_back({.id = "modifier.weighted",
                           .kind = "weighted_normal",
                           .role = "modifier",
                           .capability_status = "runtime-reference"});
  const aster::ProceduralGraphEvaluationResult result =
      aster::evaluateProceduralAssetGraph(package);
  assert(result.production_ready);
  assert(!result.stable_provenance_id.empty());
  assert(!result.mesh.vertices.empty());
  assert(result.diagnostics.empty());

  std::vector<aster::AsterAssetFoundryQualityDiagnostic> recipe_diagnostics;
  const aster::AsterAssetFoundryRecipe recipe =
      aster::makeAsterAssetFoundryRecipeFromGraph(package, &recipe_diagnostics);
  const aster::AsterAssetFoundryBuildResult build =
      aster::buildAsterAssetFoundryRecipe(recipe);
  assert(!recipe.asset_id.empty());
  assert(build.production_ready);
  assert(build.proof_artifacts.size() == 1u);
}

void assertPreviewAndSimulation() {
  aster::PreviewImageRgba8 foreground;
  foreground.width = 2u;
  foreground.height = 2u;
  foreground.rgba8 = {255u, 0u, 0u, 128u, 0u, 255u, 0u, 255u,
                      0u, 0u, 255u, 64u, 255u, 255u, 255u, 0u};
  aster::PreviewCompositeReport preview_report;
  const aster::PreviewImageRgba8 composited =
      aster::compositePreviewOver(foreground, {}, {}, &preview_report);
  assert(preview_report.ok);
  assert(composited.rgba8.size() == 16u);

  std::vector<aster::XpbdParticle> particles{
      {.position = {0.0f, 0.0f, 0.0f}, .inverse_mass = 0.0f, .pinned = true},
      {.position = {2.0f, 0.0f, 0.0f}, .inverse_mass = 1.0f},
  };
  const std::vector<aster::XpbdDistanceConstraint> constraints{
      {.a = 0u, .b = 1u, .rest_length = 1.0f}};
  const aster::XpbdSimulationReport report = aster::simulateXpbdDistanceConstraints(
      particles, constraints, {.dt = 1.0f / 60.0f, .iterations = 8, .gravity = {}});
  assert(report.constraints_solved > 0u);
  assert(std::abs(aster::length(particles[1].position - particles[0].position) - 1.0f) < 0.1f);
}

void assertXpbdAuthoringSourceEdit() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "aster_xpbd_authoring_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path obj = dir / "cloth_patch.obj";
  {
    std::ofstream file(obj);
    file << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    file << "vt 0 0\nvt 1 0\nvt 0 1\n";
    file << "f 1/1 2/2 3/3\n";
  }

  aster::XpbdMeshAuthoringSettings settings;
  settings.frames = 6u;
  settings.simulation.dt = 1.0f / 30.0f;
  settings.simulation.iterations = 12;
  settings.simulation.gravity = {0.0f, -18.0f, 0.0f};
  settings.compliance = 0.001f;
  settings.damping = 0.02f;
  settings.linear_damping = 0.03f;
  settings.pin_rule.axis = aster::XpbdPinAxis::Y;
  settings.pin_rule.threshold = 0.9f;
  settings.pin_rule.pin_greater_equal = true;

  const aster::AssetMeshImportResult before = aster::importMeshAsset(obj);
  assert(before.report.ok);
  aster::XpbdMeshAuthoringSession session =
      aster::makeXpbdMeshAuthoringSession(before.mesh, settings);
  assert(session.source_loaded);
  assert(session.pinned_particles == 1u);
  const aster::XpbdSimulationReport sim = aster::simulateXpbdMeshAuthoringSession(session);
  assert(sim.stable);
  assert(sim.constraints_solved > 0u);
  bool moved = false;
  for (std::size_t i = 0u; i < before.mesh.vertices.size(); ++i) {
    moved = moved || aster::length(session.preview_mesh.vertices[i].position -
                                   before.mesh.vertices[i].position) > 0.00001f;
  }
  assert(moved);

  const aster::XpbdSourceEditReport edit =
      aster::simulateXpbdMeshSourceEdit(obj, settings);
  assert(edit.ok);
  assert(edit.applied);
  assert(edit.editable_source);
  assert(std::filesystem::exists(edit.backup_path));
  const aster::AssetMeshImportResult after = aster::importMeshAsset(obj);
  assert(after.report.ok);
  assert(after.mesh.vertices.size() == before.mesh.vertices.size());
  bool source_changed = false;
  for (std::size_t i = 0u; i < before.mesh.vertices.size(); ++i) {
    source_changed = source_changed || aster::length(after.mesh.vertices[i].position -
                                                     before.mesh.vertices[i].position) > 0.00001f;
  }
  assert(source_changed);
}

} // namespace

int main() {
  assertUserFacingOwnershipLanguageIsClean();
  assertAssetLibrary();
  assertAssetRegistryAndDerivedCache();
  assertMeshAuthoringAndModifiers();
  assertGeometryOperations();
  assertMeshIo();
  assertProceduralRuntime();
  assertPreviewAndSimulation();
  assertXpbdAuthoringSourceEdit();
  return 0;
}
