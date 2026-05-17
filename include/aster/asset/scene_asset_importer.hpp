// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/math/mat4.hpp"
#include "aster/scene/scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aster {

struct SceneAssetHash {
  std::array<std::uint8_t, 32> bytes{};
};

struct SceneTextureDependency {
  std::string role;
  std::string uri;
  bool present = false;
  SceneAssetHash hash{};
};

struct SceneMaterialSlot {
  std::string name;
  Material material{};
  bool has_base_color_texture = false;
  bool has_metallic_roughness_texture = false;
  bool has_normal_texture = false;
  bool has_occlusion_texture = false;
  std::vector<SceneTextureDependency> texture_dependencies;
};

struct SceneAssetNode {
  std::string name;
  int parent_index = -1;
  Mat4 transform{};
  std::size_t first_mesh = 0;
  std::size_t mesh_count = 0;
};

struct SceneMeshChunk {
  std::string name;
  CpuMesh mesh;
  std::size_t material_slot = 0;
  MeshDiagnostics diagnostics{};
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  bool bounds_valid = false;
};

struct SceneCollisionTriangle {
  Vec3 a{};
  Vec3 b{};
  Vec3 c{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
};

struct SceneCollisionMesh {
  std::string name;
  std::size_t mesh_chunk = 0;
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  bool bounds_valid = false;
  std::vector<SceneCollisionTriangle> triangles;
};

struct SceneAssetCacheMetadata {
  bool valid = false;
  std::uint32_t cache_version = 0;
  std::uint32_t compiler_version = 0;
  SceneAssetHash source_hash{};
  SceneAssetHash options_hash{};
  std::string compiler_options;
  std::size_t material_count = 0;
  std::size_t mesh_count = 0;
  std::size_t collision_mesh_count = 0;
  std::size_t scene_node_count = 0;
  std::size_t total_vertices = 0;
  std::size_t total_indices = 0;
  std::size_t total_collision_triangles = 0;
  MeshDiagnostics diagnostics{};
};

struct SceneAsset {
  SceneAssetCacheMetadata cache_metadata{};
  std::vector<SceneAssetNode> scene_nodes;
  std::vector<SceneMaterialSlot> material_slots;
  std::vector<SceneMeshChunk> mesh_chunks;
  std::vector<SceneCollisionMesh> collision_meshes;
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
[[nodiscard]] SceneAsset loadCompiledSceneAsset(const std::filesystem::path &path);

} // namespace aster
