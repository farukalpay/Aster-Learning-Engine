// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/scene/scene.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace aster {

struct SceneMaterialSlot {
  std::string name;
  Material material{};
  bool has_base_color_texture = false;
  bool has_metallic_roughness_texture = false;
  bool has_normal_texture = false;
  bool has_occlusion_texture = false;
};

struct SceneMeshChunk {
  std::string name;
  CpuMesh mesh;
  std::size_t material_slot = 0;
  MeshDiagnostics diagnostics{};
};

struct SceneAsset {
  std::vector<SceneMaterialSlot> material_slots;
  std::vector<SceneMeshChunk> mesh_chunks;
};

enum class AssetOriginPolicy {
  Keep,
  Center,
  CenterOnGround,
};

struct SceneAssetImportOptions {
  MeshProcessOptions mesh_options{};
  bool apply_node_transforms = true;
  float unit_scale = 1.0f;
  AssetOriginPolicy origin_policy = AssetOriginPolicy::Keep;
};

[[nodiscard]] SceneAsset importSceneAsset(const std::filesystem::path &path,
                                          SceneAssetImportOptions options = {});

} // namespace aster
