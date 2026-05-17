// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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
  assert(asset.cache_metadata.cache_version == 1u);
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

} // namespace

int main() {
  testCompiledSceneAssetCacheLoad();
  std::cout << "asset_cache_tests passed.\n";
  return 0;
}
