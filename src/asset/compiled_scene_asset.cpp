// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/scene_asset_importer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace aster {
namespace {

constexpr std::array<std::uint8_t, 8> kCacheMagic{'A', 'S', 'T', 'R', 'C', 'V', '1', '\0'};
constexpr std::uint32_t kCacheVersion = 2u;
constexpr std::uint32_t kEndianMarker = 0x12345678u;
constexpr std::size_t kHeaderSize = 88u;
constexpr std::size_t kChunkEntrySize = 56u;
constexpr std::uint32_t kChunkMetadata = 1u;
constexpr std::uint32_t kChunkSceneGraph = 2u;
constexpr std::uint32_t kChunkMaterials = 3u;
constexpr std::uint32_t kChunkMeshes = 4u;
constexpr std::uint32_t kChunkCollision = 5u;

struct ByteView {
  const std::uint8_t *data = nullptr;
  std::size_t size = 0;
};

struct ChunkEntry {
  std::uint32_t kind = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
};

class Reader {
public:
  explicit Reader(ByteView view) : view_(view) {}

  [[nodiscard]] std::uint8_t u8() {
    return bytes(1u)[0];
  }

  [[nodiscard]] std::uint32_t u32() {
    const std::uint8_t *data = bytes(4u);
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) |
           (static_cast<std::uint32_t>(data[3]) << 24u);
  }

  [[nodiscard]] std::uint64_t u64() {
    const std::uint8_t *data = bytes(8u);
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
      value |= static_cast<std::uint64_t>(data[i]) << (static_cast<unsigned>(i) * 8u);
    }
    return value;
  }

  [[nodiscard]] int i32() {
    const std::uint32_t bits = u32();
    std::int32_t value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return static_cast<int>(value);
  }

  [[nodiscard]] float f32() {
    const std::uint32_t bits = u32();
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }

  [[nodiscard]] std::string string() {
    const std::uint32_t size = u32();
    const std::uint8_t *data = bytes(size);
    return {reinterpret_cast<const char *>(data), reinterpret_cast<const char *>(data) + size};
  }

  [[nodiscard]] SceneAssetHash hash() {
    SceneAssetHash out;
    const std::uint8_t *data = bytes(out.bytes.size());
    std::copy(data, data + out.bytes.size(), out.bytes.begin());
    return out;
  }

  [[nodiscard]] Vec2 vec2() {
    return {f32(), f32()};
  }

  [[nodiscard]] Vec3 vec3() {
    return {f32(), f32(), f32()};
  }

  [[nodiscard]] Vec4 vec4() {
    return {f32(), f32(), f32(), f32()};
  }

  void skip(const std::size_t size) {
    (void)bytes(size);
  }

private:
  [[nodiscard]] const std::uint8_t *bytes(const std::size_t size) {
    if (size > view_.size || cursor_ > view_.size - size) {
      throw std::runtime_error("Compiled scene asset cache is truncated.");
    }
    const std::uint8_t *out = view_.data + cursor_;
    cursor_ += size;
    return out;
  }

  ByteView view_{};
  std::size_t cursor_ = 0;
};

[[nodiscard]] std::size_t checkedSize(const std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error("Compiled scene asset cache size exceeds this platform.");
  }
  return static_cast<std::size_t>(value);
}

[[nodiscard]] std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open compiled scene asset cache: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] MeshDiagnostics readDiagnostics(Reader &reader) {
  MeshDiagnostics diagnostics;
  diagnostics.input_vertices = checkedSize(reader.u64());
  diagnostics.input_indices = checkedSize(reader.u64());
  diagnostics.output_vertices = checkedSize(reader.u64());
  diagnostics.output_indices = checkedSize(reader.u64());
  diagnostics.degenerate_triangles = checkedSize(reader.u64());
  diagnostics.invalid_normals = checkedSize(reader.u64());
  diagnostics.generated_tangents = checkedSize(reader.u64());
  diagnostics.remapped_vertices = checkedSize(reader.u64());
  return diagnostics;
}

[[nodiscard]] SceneAssetCacheMetadata readMetadata(ByteView chunk) {
  Reader reader(chunk);
  SceneAssetCacheMetadata metadata;
  metadata.valid = true;
  metadata.cache_version = reader.u32();
  metadata.compiler_version = reader.u32();
  metadata.source_hash = reader.hash();
  metadata.options_hash = reader.hash();
  metadata.compiler_options = reader.string();
  metadata.material_count = checkedSize(reader.u32());
  metadata.mesh_count = checkedSize(reader.u32());
  metadata.collision_mesh_count = checkedSize(reader.u32());
  metadata.scene_node_count = checkedSize(reader.u32());
  metadata.total_vertices = checkedSize(reader.u64());
  metadata.total_indices = checkedSize(reader.u64());
  metadata.total_collision_triangles = checkedSize(reader.u64());
  metadata.diagnostics = readDiagnostics(reader);
  return metadata;
}

[[nodiscard]] std::vector<SceneAssetNode> readSceneGraph(ByteView chunk) {
  Reader reader(chunk);
  const std::size_t count = checkedSize(reader.u32());
  std::vector<SceneAssetNode> nodes;
  nodes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    SceneAssetNode node;
    node.name = reader.string();
    node.parent_index = reader.i32();
    for (float &value : node.transform.m) {
      value = reader.f32();
    }
    node.first_mesh = checkedSize(reader.u32());
    node.mesh_count = checkedSize(reader.u32());
    nodes.push_back(std::move(node));
  }
  return nodes;
}

[[nodiscard]] MaterialAlphaMode alphaModeFromValue(const std::uint32_t value) {
  switch (value) {
  case 1u:
    return MaterialAlphaMode::Masked;
  case 2u:
    return MaterialAlphaMode::DitheredCoverage;
  case 3u:
    return MaterialAlphaMode::Blend;
  default:
    return MaterialAlphaMode::Opaque;
  }
}

[[nodiscard]] MaterialDepthWrite depthWriteFromValue(const std::uint32_t value) {
  switch (value) {
  case 1u:
    return MaterialDepthWrite::Enabled;
  case 2u:
    return MaterialDepthWrite::Disabled;
  default:
    return MaterialDepthWrite::Auto;
  }
}

[[nodiscard]] std::vector<SceneMaterialSlot> readMaterials(ByteView chunk) {
  Reader reader(chunk);
  const std::size_t count = checkedSize(reader.u32());
  std::vector<SceneMaterialSlot> materials;
  materials.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    SceneMaterialSlot slot;
    slot.name = reader.string();
    slot.material.base_color = reader.vec3();
    slot.material.emission_color = reader.vec3();
    slot.material.roughness = reader.f32();
    slot.material.metallic = reader.f32();
    slot.material.emission_strength = reader.f32();
    slot.material.opacity = reader.f32();
    slot.material.double_sided = reader.u8() != 0u;
    slot.has_base_color_texture = reader.u8() != 0u;
    slot.has_metallic_roughness_texture = reader.u8() != 0u;
    slot.has_normal_texture = reader.u8() != 0u;
    slot.has_occlusion_texture = reader.u8() != 0u;
    reader.skip(3u);
    slot.material.alpha_mode = alphaModeFromValue(reader.u32());
    slot.material.depth_write = depthWriteFromValue(reader.u32());
    slot.permutation_key = reader.u64();
    slot.permutation_flags = reader.u32();
    slot.pipeline_tag = reader.string();
    slot.material.compiled_permutation_key = slot.permutation_key;
    slot.material.compiled_permutation_flags = slot.permutation_flags;
    const std::size_t dependency_count = checkedSize(reader.u32());
    slot.texture_dependencies.reserve(dependency_count);
    for (std::size_t dependency_index = 0; dependency_index < dependency_count;
         ++dependency_index) {
      SceneTextureDependency dependency;
      dependency.role = reader.string();
      dependency.uri = reader.string();
      dependency.present = reader.u8() != 0u;
      reader.skip(3u);
      dependency.hash = reader.hash();
      slot.texture_dependencies.push_back(std::move(dependency));
    }
    materials.push_back(std::move(slot));
  }
  return materials;
}

void readBounds(Reader &reader, Vec3 &minimum, Vec3 &maximum, bool &valid) {
  minimum = reader.vec3();
  maximum = reader.vec3();
  valid = reader.u8() != 0u;
  reader.skip(3u);
}

[[nodiscard]] std::vector<SceneMeshChunk> readMeshes(ByteView chunk) {
  Reader reader(chunk);
  const std::size_t count = checkedSize(reader.u32());
  std::vector<SceneMeshChunk> meshes;
  meshes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    SceneMeshChunk mesh;
    mesh.name = reader.string();
    mesh.material_slot = checkedSize(reader.u32());
    readBounds(reader, mesh.bounds_min, mesh.bounds_max, mesh.bounds_valid);
    mesh.diagnostics = readDiagnostics(reader);
    const std::size_t vertex_count = checkedSize(reader.u32());
    const std::size_t index_count = checkedSize(reader.u32());
    mesh.mesh.vertices.reserve(vertex_count);
    mesh.mesh.indices.reserve(index_count);
    for (std::size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
      Vertex vertex;
      vertex.position = reader.vec3();
      vertex.normal = reader.vec3();
      vertex.uv = reader.vec2();
      vertex.tangent = reader.vec4();
      vertex.ambient_occlusion = reader.f32();
      mesh.mesh.vertices.push_back(vertex);
    }
    for (std::size_t index = 0; index < index_count; ++index) {
      mesh.mesh.indices.push_back(reader.u32());
    }
    meshes.push_back(std::move(mesh));
  }
  return meshes;
}

[[nodiscard]] std::vector<SceneCollisionMesh> readCollision(ByteView chunk) {
  Reader reader(chunk);
  const std::size_t count = checkedSize(reader.u32());
  std::vector<SceneCollisionMesh> meshes;
  meshes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    SceneCollisionMesh mesh;
    mesh.name = reader.string();
    mesh.mesh_chunk = checkedSize(reader.u32());
    readBounds(reader, mesh.bounds_min, mesh.bounds_max, mesh.bounds_valid);
    const std::size_t triangle_count = checkedSize(reader.u32());
    mesh.triangles.reserve(triangle_count);
    for (std::size_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
      SceneCollisionTriangle triangle;
      triangle.a = reader.vec3();
      triangle.b = reader.vec3();
      triangle.c = reader.vec3();
      triangle.normal = reader.vec3();
      mesh.triangles.push_back(triangle);
    }
    meshes.push_back(std::move(mesh));
  }
  return meshes;
}

[[nodiscard]] ByteView chunkView(const std::vector<std::uint8_t> &bytes, const ChunkEntry entry,
                                 const std::size_t table_end) {
  const std::size_t offset = checkedSize(entry.offset);
  const std::size_t size = checkedSize(entry.size);
  if (offset < table_end || size > bytes.size() || offset > bytes.size() - size) {
    throw std::runtime_error("Compiled scene asset cache chunk range is invalid.");
  }
  return {bytes.data() + offset, size};
}

[[nodiscard]] ByteView requiredChunk(const std::unordered_map<std::uint32_t, ByteView> &chunks,
                                     const std::uint32_t kind) {
  const auto found = chunks.find(kind);
  if (found == chunks.end()) {
    throw std::runtime_error("Compiled scene asset cache is missing a required chunk.");
  }
  return found->second;
}

void validateCounts(const SceneAsset &asset) {
  if (asset.cache_metadata.material_count != asset.material_slots.size() ||
      asset.cache_metadata.mesh_count != asset.mesh_chunks.size() ||
      asset.cache_metadata.collision_mesh_count != asset.collision_meshes.size() ||
      asset.cache_metadata.scene_node_count != asset.scene_nodes.size()) {
    throw std::runtime_error("Compiled scene asset cache metadata counts do not match payloads.");
  }
}

} // namespace

SceneAsset loadCompiledSceneAsset(const std::filesystem::path &path) {
  const std::vector<std::uint8_t> bytes = readBinaryFile(path);
  if (bytes.size() < kHeaderSize) {
    throw std::runtime_error("Compiled scene asset cache is shorter than its header.");
  }

  Reader header({bytes.data(), bytes.size()});
  const std::uint8_t *magic_data = bytes.data();
  if (!std::equal(kCacheMagic.begin(), kCacheMagic.end(), magic_data)) {
    throw std::runtime_error("Compiled scene asset cache has an invalid magic.");
  }
  header.skip(kCacheMagic.size());
  const std::uint32_t cache_version = header.u32();
  if (cache_version != kCacheVersion) {
    throw std::runtime_error("Unsupported compiled scene asset cache version.");
  }
  if (header.u32() != kEndianMarker) {
    throw std::runtime_error("Compiled scene asset cache endian marker is invalid.");
  }
  (void)header.u32();
  const std::size_t chunk_count = checkedSize(header.u32());
  const SceneAssetHash header_source_hash = header.hash();
  const SceneAssetHash header_options_hash = header.hash();

  const std::size_t table_size = chunk_count * kChunkEntrySize;
  if (chunk_count != 0u && table_size / chunk_count != kChunkEntrySize) {
    throw std::runtime_error("Compiled scene asset cache chunk table is too large.");
  }
  const std::size_t table_end = kHeaderSize + table_size;
  if (table_end < kHeaderSize || bytes.size() < table_end) {
    throw std::runtime_error("Compiled scene asset cache has a truncated chunk table.");
  }

  std::vector<ChunkEntry> entries;
  entries.reserve(chunk_count);
  for (std::size_t i = 0; i < chunk_count; ++i) {
    ChunkEntry entry;
    entry.kind = header.u32();
    (void)header.u32();
    entry.offset = header.u64();
    entry.size = header.u64();
    (void)header.hash();
    entries.push_back(entry);
  }

  std::unordered_map<std::uint32_t, ByteView> chunks;
  chunks.reserve(entries.size());
  for (const ChunkEntry entry : entries) {
    chunks.emplace(entry.kind, chunkView(bytes, entry, table_end));
  }

  SceneAsset asset;
  asset.cache_metadata = readMetadata(requiredChunk(chunks, kChunkMetadata));
  if (asset.cache_metadata.source_hash.bytes != header_source_hash.bytes ||
      asset.cache_metadata.options_hash.bytes != header_options_hash.bytes) {
    throw std::runtime_error("Compiled scene asset cache header hashes do not match metadata.");
  }
  asset.scene_nodes = readSceneGraph(requiredChunk(chunks, kChunkSceneGraph));
  asset.material_slots = readMaterials(requiredChunk(chunks, kChunkMaterials));
  asset.mesh_chunks = readMeshes(requiredChunk(chunks, kChunkMeshes));
  asset.collision_meshes = readCollision(requiredChunk(chunks, kChunkCollision));
  validateCounts(asset);
  return asset;
}

} // namespace aster
