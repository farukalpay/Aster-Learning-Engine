// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/asset/asset_io.hpp"
#include "aster/asset/asset_library.hpp"
#include "aster/asset/asset_modifier_stack.hpp"
#include "aster/asset/procedural_graph_runtime.hpp"
#include "aster/geometry/geometry_operations.hpp"
#include "aster/geometry/mesh_authoring.hpp"
#include "aster/physics/xpbd_constraints.hpp"
#include "aster/render/preview_compositor.hpp"

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
      root / "src" / "ui",
  };
  const std::vector<std::string> banned{
      std::string("Asset") + "Factory",
      std::string("FarukAlpay") + "Asset" + "Factory",
      std::string("Asset") + " Factory",
      std::string("Asset") + "Factory-inspired",
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
  const aster::AssetModifierStackResult result = aster::applyAssetModifierStack(box, stack);
  assert(!result.report.stable_provenance_id.empty());
  assert(!result.mesh.vertices.empty());
  assert(result.report.quality_score > 0u);
  assert(result.report.creative_variant_tags.size() == 1u);
}

void assertGeometryOperations() {
  aster::GeometrySet set;
  set.meshes.push_back({.id = "a", .mesh = aster::makeBox()});
  aster::GeometryMeshPart shifted{.id = "b", .mesh = aster::makeBox()};
  shifted.transform.position = {2.0f, 0.0f, 0.0f};
  set.meshes.push_back(shifted);
  const aster::CpuMesh joined = aster::joinGeometryMeshes(set);
  assert(joined.vertices.size() == aster::makeBox().vertices.size() * 2u);
  assert(aster::separateDisconnectedMeshIslands(joined).size() == 2u);
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
    file << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    file << "vt 0 0\nvt 1 0\nvt 0 1\n";
    file << "f 1/1 2/2 3/3\n";
  }
  const aster::AssetMeshImportResult imported = aster::importMeshAsset(obj);
  assert(imported.report.ok);
  assert(imported.mesh.indices.size() == 3u);
  const aster::AssetMeshIoReport exported =
      aster::exportMeshAssetObj(imported.mesh, dir / "tri_export.obj");
  assert(exported.ok);
  assert(!exported.stable_source_hash.empty());
}

void assertProceduralRuntime() {
  aster::ProceduralAssetGraphPackage package;
  package.id = "graph.runtime";
  package.asset_guid = "graph-guid";
  package.runtime_model = "aster-runtime";
  package.mesh.primitive = "sphere";
  package.quality.score = 96u;
  package.quality.production_ready = true;
  package.nodes.push_back({.id = "mesh.surface",
                           .kind = "mesh_primitive",
                           .role = "mesh",
                           .params = {{"primitive", "sphere"}},
                           .capability_status = "runtime-reference"});
  const aster::ProceduralGraphEvaluationResult result =
      aster::evaluateProceduralAssetGraph(package);
  assert(result.production_ready);
  assert(!result.stable_provenance_id.empty());
  assert(!result.mesh.vertices.empty());
  assert(result.diagnostics.empty());
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

} // namespace

int main() {
  assertUserFacingOwnershipLanguageIsClean();
  assertAssetLibrary();
  assertMeshAuthoringAndModifiers();
  assertGeometryOperations();
  assertMeshIo();
  assertProceduralRuntime();
  assertPreviewAndSimulation();
  return 0;
}
