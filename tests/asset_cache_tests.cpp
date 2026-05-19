// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/asset/asset_database.hpp"
#include "aster/asset/asset_factory.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/texture/runtime_texture.hpp"

#include <algorithm>
#include <cassert>
#include <array>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

bool nonZeroHash(const aster::SceneAssetHash &hash) {
  for (const std::uint8_t byte : hash.bytes) {
    if (byte != 0u) {
      return true;
    }
  }
  return false;
}

void writeTinyKtx2Header(const std::filesystem::path &path, const std::uint32_t width,
                         const std::uint32_t height, const std::uint32_t mip_count,
                         const std::uint32_t vk_format) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  const std::array<unsigned char, 12> signature{0xabu, 'K', 'T', 'X', ' ', '2',
                                                '0',   0xbbu, '\r', '\n', 0x1au, '\n'};
  out.write(reinterpret_cast<const char *>(signature.data()),
            static_cast<std::streamsize>(signature.size()));
  const auto write_le32 = [&](const std::uint32_t value) {
    const std::array<unsigned char, 4> bytes{
        static_cast<unsigned char>(value & 0xffu),
        static_cast<unsigned char>((value >> 8u) & 0xffu),
        static_cast<unsigned char>((value >> 16u) & 0xffu),
        static_cast<unsigned char>((value >> 24u) & 0xffu),
    };
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  };
  write_le32(vk_format);
  write_le32(1u);
  write_le32(width);
  write_le32(height);
  write_le32(0u);
  write_le32(0u);
  write_le32(1u);
  write_le32(mip_count);
  write_le32(1u);
  out.write("ASTER_CPP_TEST_KTX2_PAYLOAD", 27);
  assert(out.good());
}

std::filesystem::path writeMaterialCookProject() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "aster_material_cook_fixture";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "materials");
  std::filesystem::create_directories(dir / "textures");
  writeTinyKtx2Header(dir / "textures" / "albedo.ktx2", 16u, 8u, 4u, 43u);
  writeTinyKtx2Header(dir / "textures" / "normal.ktx2", 16u, 8u, 4u, 37u);
  writeTinyKtx2Header(dir / "textures" / "orm.ktx2", 16u, 8u, 4u, 37u);
  {
    std::ofstream material(dir / "materials" / "wet_rock.astermat");
    material << R"mat(material CookedWetRock {
  name: "Cooked Wet Rock"
  shading_model: LitPBR
  surface_profile: StratifiedRock
  blend_mode: Opaque
  cull_mode: Back
  textures {
    albedo: "../textures/albedo.ktx2"
    normal: "../textures/normal.ktx2"
    orm: "../textures/orm.ktx2"
  }
  params {
    base_color_r: 0.31
    base_color_g: 0.28
    base_color_b: 0.24
    roughness: 0.79
    metallic: 0.0
  }
  features {
    triplanar: true
    normal_map: true
  }
}
)mat";
  }
  {
    std::ofstream project(dir / "project.asterproj");
    project << R"json({
	  "schema_version": 2,
	  "name": "Cook Fixture",
	  "assets": [
	    {
	      "id": "material.cooked_wet_rock",
	      "guid": "asset-v2-cooked-wet-rock-0000000000000001",
	      "kind": "material",
	      "path": "materials/wet_rock.astermat",
	      "import_preset": "default"
	    }
	  ]
	}
	)json";
  }
  return dir / "project.asterproj";
}

void testCompiledSceneAssetCacheLoad() {
  const std::filesystem::path scene = writeGeneratedNormalTangentFixture();
  const std::filesystem::path cache = scene.parent_path() / "normal_tangent_probe.astercache";
  const std::filesystem::path assetc = ASTER_ASSETC_EXECUTABLE;
  const std::string command = shellQuote(assetc) + " compile --input " + shellQuote(scene) +
                              " --output " + shellQuote(cache) + " --origin center-on-ground";
  const int result = std::system(command.c_str());
  assert(result == 0);
  assert(std::filesystem::exists(cache));

  const aster::SceneAsset asset = aster::loadCompiledSceneAsset(cache);
  assert(asset.cache_metadata.valid);
  assert(asset.cache_metadata.cache_version == 3u);
  assert(asset.cache_metadata.material_count == 2u);
  assert(asset.cache_metadata.mesh_count == 1u);
  assert(asset.cache_metadata.collision_mesh_count == 1u);
  assert(asset.cache_metadata.scene_node_count == 1u);
  assert(asset.cache_metadata.compiler_options.find("center-on-ground") != std::string::npos);
  assert(nonZeroHash(asset.cache_metadata.source_hash));
  assert(nonZeroHash(asset.cache_metadata.options_hash));

  assert(asset.material_slots.size() == 2u);
  const aster::SceneMaterialSlot &material = asset.material_slots[1];
  assert(material.has_base_color_texture);
  assert(material.has_metallic_roughness_texture);
  assert(material.has_normal_texture);
  assert(material.has_occlusion_texture);
  assert(material.texture_dependencies.size() == 4u);
  assert(material.material.double_sided);
  assert(material.material.alpha_mode == aster::MaterialAlphaMode::Masked);
  assert(aster::classifyMaterialRenderQueue(material.material) == aster::MaterialRenderQueue::Masked);
  assert(material.permutation_key != 0u);
  assert(material.material.compiled_permutation_key == material.permutation_key);
  assert(material.permutation_flags == material.material.compiled_permutation_flags);
  assert((material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::Textured)) != 0u);
  assert((material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::DoubleSided)) != 0u);
  assert((material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::DepthWrite)) != 0u);
  assert(material.pipeline_tag.find("opaque.depth-write.double-sided.textured") !=
         std::string::npos);
  const aster::CompiledMaterial expected_material =
      aster::compileMaterialForRendering(material.material, true, material.name);
  assert(expected_material.permutation_key == material.permutation_key);
  assert(expected_material.permutation_flags == material.permutation_flags);
  assert(expected_material.pipeline_tag == material.pipeline_tag);

  assert(asset.mesh_chunks.size() == 1u);
  const aster::SceneMeshChunk &mesh = asset.mesh_chunks.front();
  assert(mesh.material_slot == 1u);
  assert(mesh.bounds_valid);
  assert(mesh.mesh.indices.size() == 6u);
  assert(mesh.diagnostics.generated_tangents == 6u);
  float min_y = std::numeric_limits<float>::max();
  for (const aster::Vertex &vertex : mesh.mesh.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    min_y = std::min(min_y, vertex.position.y);
  }
  expectNear(min_y, 0.0f, 0.001f);

  assert(asset.collision_meshes.size() == 1u);
  assert(asset.collision_meshes.front().mesh_chunk == 0u);
  assert(asset.collision_meshes.front().triangles.size() == 2u);
  assert(asset.cache_metadata.total_collision_triangles == 2u);

  std::filesystem::remove_all(scene.parent_path());
}

void testAssetDatabaseAndMaterialBinLoad() {
  const std::filesystem::path project = writeMaterialCookProject();
  const std::filesystem::path output = project.parent_path() / "cooked" / "desktop";
  const std::filesystem::path assetc = ASTER_ASSETC_EXECUTABLE;
  const std::string command = shellQuote(assetc) + " cook --project " + shellQuote(project) +
                              " --platform desktop --output " + shellQuote(output);
  const int result = std::system(command.c_str());
  assert(result == 0);

  const aster::AssetDatabase database = aster::loadAssetDatabase(output / "assetdb.asterdb.json");
  assert(database.schema_version == 2u);
  assert(database.artifact_manifest == "asset-manifest.astermanifest.json");
  assert(database.records.size() == 1u);
  assert(database.asset_graph.nodes.size() == 1u);
  assert(!database.asset_graph.project_fingerprint.empty());
  assert(database.fate_reports.size() == 1u);
  const aster::AssetDatabaseRecord *record =
      aster::findAssetRecord(database, "material.cooked_wet_rock");
  assert(record != nullptr);
  assert(record->kind == "material");
  assert(record->diagnostics.empty());
  assert(!record->derived_hashes.material_hash.empty());
  assert(!record->derived_hashes.shader_variant_key.empty());
  assert(record->fate_report.production_ready);
  assert(!record->fate_report.render_contract.empty());
  const std::optional<std::filesystem::path> material_bin =
      aster::findAssetOutputPath(*record, output, "materialbin", "materialbin");
  assert(material_bin.has_value());
  assert(std::filesystem::exists(*material_bin));

  const aster::AssetLibrary asset_library = aster::AssetLibrary::fromDatabase(database, output);
  assert(asset_library.assets.size() == 1u);
  assert(!asset_library.catalogs.empty());
  const aster::AssetRepresentation *asset = asset_library.find("material.cooked_wet_rock");
  assert(asset != nullptr);
  assert(asset->production_ready);
  assert(!asset->derived_hashes.material_hash.empty());
  const aster::DiskFileHashService hash_service(output);
  const std::string db_hash = hash_service.getHash(output / "assetdb.asterdb.json");
  assert(!db_hash.empty());
  assert(hash_service.fileMatches(output / "assetdb.asterdb.json", "fnv1a64", db_hash,
                                  std::filesystem::file_size(output / "assetdb.asterdb.json")));
  const std::vector<aster::AssetFileListEntry> file_list = aster::scanAssetFiles(output);
  assert(!file_list.empty());
  assert(std::any_of(file_list.begin(), file_list.end(),
                     [](const aster::AssetFileListEntry &entry) {
                       return entry.kind == "texture" || entry.kind == "database";
                     }));

  const aster::CookedMaterialAsset cooked = aster::loadCookedMaterialAsset(*material_bin);
  assert(cooked.asset.id == "CookedWetRock");
  assert(cooked.textures.size() == 3u);
  assert(cooked.shader_variant_key != 0u);
  assert(cooked.asset.params.at("roughness") > 0.70f);
  const auto orm = std::find_if(cooked.textures.begin(), cooked.textures.end(),
                                [](const aster::CookedMaterialTextureRecord &texture) {
                                  return texture.role == "orm";
                                });
  assert(orm != cooked.textures.end());
  assert(orm->source_format == "ktx2");
  assert(orm->runtime_format == "ktx2");
  assert(orm->encoder == "passthrough-ktx2");
  assert(orm->byte_cost > 0u);

  aster::MaterialResourceLibrary library;
  assert(library.addCookedMaterialAsset(cooked, {.require_existing_files = true}));
  const aster::MaterialRuntimeResource *resource = library.find("CookedWetRock");
  assert(resource != nullptr);
  assert(!resource->texture_set.find("albedo")->fallback);
  assert(!resource->texture_set.find("normal")->fallback);
  assert(!resource->texture_set.find("orm")->fallback);
  std::filesystem::remove_all(project.parent_path());
}

} // namespace

int main() {
  testCompiledSceneAssetCacheLoad();
  testAssetDatabaseAndMaterialBinLoad();
  std::cout << "asset_cache_tests passed.\n";
  return 0;
}
