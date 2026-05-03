// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/scene_asset_importer.hpp"

#define ASSETDOC_IMPLEMENTATION
#include <scene_document_codec.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace aster {
namespace {

struct SceneFormatDataDeleter {
  void operator()(assetdoc_data *data) const {
    assetdoc_free(data);
  }
};

using SceneFormatData = std::unique_ptr<assetdoc_data, SceneFormatDataDeleter>;

struct Bounds {
  Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  Vec3 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};
  bool valid = false;
};

Vec3 transformPoint(const float matrix[16], const Vec3 value) {
  return {
      matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z + matrix[12],
      matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z + matrix[13],
      matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z + matrix[14],
  };
}

Vec3 transformVector(const float matrix[16], const Vec3 value) {
  return normalize({
      matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z,
      matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z,
      matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z,
  });
}

Vec3 transformNormal(const float matrix[16], const Vec3 value) {
  const float a00 = matrix[0];
  const float a01 = matrix[4];
  const float a02 = matrix[8];
  const float a10 = matrix[1];
  const float a11 = matrix[5];
  const float a12 = matrix[9];
  const float a20 = matrix[2];
  const float a21 = matrix[6];
  const float a22 = matrix[10];

  const float c00 = a11 * a22 - a12 * a21;
  const float c01 = a12 * a20 - a10 * a22;
  const float c02 = a10 * a21 - a11 * a20;
  const float c10 = a02 * a21 - a01 * a22;
  const float c11 = a00 * a22 - a02 * a20;
  const float c12 = a01 * a20 - a00 * a21;
  const float c20 = a01 * a12 - a02 * a11;
  const float c21 = a02 * a10 - a00 * a12;
  const float c22 = a00 * a11 - a01 * a10;

  const float determinant = a00 * c00 + a01 * c01 + a02 * c02;
  if (std::abs(determinant) <= 0.000001f) {
    return transformVector(matrix, value);
  }

  const float inv_det = 1.0f / determinant;
  return normalize({
      (c00 * value.x + c01 * value.y + c02 * value.z) * inv_det,
      (c10 * value.x + c11 * value.y + c12 * value.z) * inv_det,
      (c20 * value.x + c21 * value.y + c22 * value.z) * inv_det,
  });
}

Vec3 color(const float values[4]) {
  return {values[0], values[1], values[2]};
}

Vec3 color3(const float values[3]) {
  return {values[0], values[1], values[2]};
}

std::size_t materialSlot(const assetdoc_data &data, const assetdoc_material *material) {
  if (material == nullptr) {
    return 0u;
  }
  return static_cast<std::size_t>(assetdoc_material_index(&data, material)) + 1u;
}

const assetdoc_accessor *attribute(const assetdoc_primitive &primitive,
                                   const assetdoc_attribute_type type, const int index = 0) {
  for (assetdoc_size i = 0; i < primitive.attributes_count; ++i) {
    const assetdoc_attribute &candidate = primitive.attributes[i];
    if (candidate.type == type && candidate.index == index) {
      return candidate.data;
    }
  }
  return nullptr;
}

bool hasTangents(const assetdoc_primitive &primitive) {
  return attribute(primitive, assetdoc_attribute_type_tangent) != nullptr;
}

Vec3 readVec3(const assetdoc_accessor &accessor, const assetdoc_size index) {
  float values[3]{};
  if (!assetdoc_accessor_read_float(&accessor, index, values, 3)) {
    throw std::runtime_error("Failed to read scene asset vec3 accessor.");
  }
  return {values[0], values[1], values[2]};
}

Vec4 readVec4(const assetdoc_accessor &accessor, const assetdoc_size index) {
  float values[4]{};
  if (!assetdoc_accessor_read_float(&accessor, index, values, 4)) {
    throw std::runtime_error("Failed to read scene asset vec4 accessor.");
  }
  return {values[0], values[1], values[2], values[3]};
}

Vec2 readVec2(const assetdoc_accessor &accessor, const assetdoc_size index) {
  float values[2]{};
  if (!assetdoc_accessor_read_float(&accessor, index, values, 2)) {
    throw std::runtime_error("Failed to read scene asset vec2 accessor.");
  }
  return {values[0], values[1]};
}

std::vector<std::uint32_t> readElementIndices(const assetdoc_primitive &primitive,
                                              const std::size_t vertex_count) {
  if (primitive.indices == nullptr) {
    std::vector<std::uint32_t> indices(vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i) {
      indices[i] = static_cast<std::uint32_t>(i);
    }
    return indices;
  }

  std::vector<std::uint32_t> indices(primitive.indices->count);
  for (assetdoc_size i = 0; i < primitive.indices->count; ++i) {
    indices[i] = static_cast<std::uint32_t>(assetdoc_accessor_read_index(primitive.indices, i));
  }
  return indices;
}

std::vector<std::uint32_t> triangulate(const assetdoc_primitive &primitive,
                                       const std::vector<std::uint32_t> &source) {
  std::vector<std::uint32_t> triangles;
  switch (primitive.type) {
  case assetdoc_primitive_type_triangles:
    if (source.size() % 3u != 0u) {
      throw std::runtime_error("Scene asset triangle primitive has non-triangular index count.");
    }
    return source;
  case assetdoc_primitive_type_triangle_strip:
    if (source.size() < 3u) {
      return {};
    }
    triangles.reserve((source.size() - 2u) * 3u);
    for (std::size_t i = 0; i + 2u < source.size(); ++i) {
      if (i % 2u == 0u) {
        triangles.insert(triangles.end(), {source[i], source[i + 1u], source[i + 2u]});
      } else {
        triangles.insert(triangles.end(), {source[i + 1u], source[i], source[i + 2u]});
      }
    }
    return triangles;
  case assetdoc_primitive_type_triangle_fan:
    if (source.size() < 3u) {
      return {};
    }
    triangles.reserve((source.size() - 2u) * 3u);
    for (std::size_t i = 1; i + 1u < source.size(); ++i) {
      triangles.insert(triangles.end(), {source[0], source[i], source[i + 1u]});
    }
    return triangles;
  default:
    throw std::runtime_error("Only triangle scene asset primitives can be imported as meshes.");
  }
}

SceneMaterialSlot importedMaterialSlot(const assetdoc_material *source) {
  SceneMaterialSlot out;
  if (source == nullptr) {
    out.name = "Default";
    return out;
  }

  out.name = source->name != nullptr ? source->name : "";
  out.material.base_color = color(source->pbr_metallic_roughness.base_color_factor);
  out.material.metallic = source->pbr_metallic_roughness.metallic_factor;
  out.material.roughness = source->pbr_metallic_roughness.roughness_factor;
  out.material.emission_color = color3(source->emissive_factor);
  out.material.emission_strength =
      std::max({out.material.emission_color.x, out.material.emission_color.y,
                out.material.emission_color.z});
  out.material.ambient_occlusion =
      source->occlusion_texture.texture != nullptr ? source->occlusion_texture.scale : 1.0f;
  out.material.double_sided = source->double_sided != 0;
  out.has_base_color_texture = source->pbr_metallic_roughness.base_color_texture.texture != nullptr;
  out.has_metallic_roughness_texture =
      source->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr;
  out.has_normal_texture = source->normal_texture.texture != nullptr;
  out.has_occlusion_texture = source->occlusion_texture.texture != nullptr;
  return out;
}

CpuMesh meshFromPrimitive(const assetdoc_primitive &primitive, const float matrix[16],
                          const bool apply_transform, const float unit_scale) {
  const assetdoc_accessor *positions = attribute(primitive, assetdoc_attribute_type_position);
  if (positions == nullptr) {
    throw std::runtime_error("Scene asset primitive is missing POSITION.");
  }

  const assetdoc_accessor *normals = attribute(primitive, assetdoc_attribute_type_normal);
  const assetdoc_accessor *texcoords = attribute(primitive, assetdoc_attribute_type_texcoord, 0);
  const assetdoc_accessor *tangents = attribute(primitive, assetdoc_attribute_type_tangent);

  CpuMesh mesh;
  mesh.vertices.reserve(positions->count);
  for (assetdoc_size i = 0; i < positions->count; ++i) {
    Vertex vertex;
    vertex.position = readVec3(*positions, i);
    if (normals != nullptr) {
      vertex.normal = readVec3(*normals, i);
    }
    if (texcoords != nullptr) {
      vertex.uv = readVec2(*texcoords, i);
    }
    if (tangents != nullptr) {
      vertex.tangent = readVec4(*tangents, i);
    }

    if (apply_transform) {
      vertex.position = transformPoint(matrix, vertex.position);
      vertex.normal = transformNormal(matrix, vertex.normal);
      const Vec3 tangent =
          transformVector(matrix, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
      vertex.tangent = {tangent.x, tangent.y, tangent.z, vertex.tangent.w};
    }

    vertex.position = vertex.position * unit_scale;
    mesh.vertices.push_back(vertex);
  }

  mesh.indices = triangulate(primitive, readElementIndices(primitive, mesh.vertices.size()));
  return mesh;
}

void includeInBounds(Bounds &bounds, const Vec3 position) {
  bounds.minimum.x = std::min(bounds.minimum.x, position.x);
  bounds.minimum.y = std::min(bounds.minimum.y, position.y);
  bounds.minimum.z = std::min(bounds.minimum.z, position.z);
  bounds.maximum.x = std::max(bounds.maximum.x, position.x);
  bounds.maximum.y = std::max(bounds.maximum.y, position.y);
  bounds.maximum.z = std::max(bounds.maximum.z, position.z);
  bounds.valid = true;
}

Bounds sceneAssetBounds(const SceneAsset &asset) {
  Bounds bounds;
  for (const SceneMeshChunk &chunk : asset.mesh_chunks) {
    for (const Vertex &vertex : chunk.mesh.vertices) {
      includeInBounds(bounds, vertex.position);
    }
  }
  return bounds;
}

void applyOriginPolicy(SceneAsset &asset, const AssetOriginPolicy policy) {
  if (policy == AssetOriginPolicy::Keep) {
    return;
  }

  const Bounds bounds = sceneAssetBounds(asset);
  if (!bounds.valid) {
    return;
  }

  Vec3 offset{};
  if (policy == AssetOriginPolicy::Center) {
    offset = (bounds.minimum + bounds.maximum) * 0.5f;
  } else if (policy == AssetOriginPolicy::CenterOnGround) {
    offset = {(bounds.minimum.x + bounds.maximum.x) * 0.5f, bounds.minimum.y,
              (bounds.minimum.z + bounds.maximum.z) * 0.5f};
  }

  for (SceneMeshChunk &chunk : asset.mesh_chunks) {
    for (Vertex &vertex : chunk.mesh.vertices) {
      vertex.position = vertex.position - offset;
    }
  }
}

void importNode(const assetdoc_data &data, const assetdoc_node &node,
                const SceneAssetImportOptions &options, SceneAsset &asset) {
  if (node.mesh != nullptr) {
    float matrix[16]{};
    assetdoc_node_transform_world(&node, matrix);
    for (assetdoc_size i = 0; i < node.mesh->primitives_count; ++i) {
      const assetdoc_primitive &primitive = node.mesh->primitives[i];
      SceneMeshChunk chunk;
      chunk.name = node.mesh->name != nullptr ? node.mesh->name : "";
      if (node.name != nullptr && node.name[0] != '\0') {
        chunk.name = std::string(node.name) + "/" + chunk.name;
      }
      chunk.material_slot = materialSlot(data, primitive.material);
      MeshProcessOptions mesh_options = options.mesh_options;
      if (hasTangents(primitive)) {
        mesh_options.generate_tangents = false;
      }
      chunk.mesh = prepareMeshForRendering(
          meshFromPrimitive(primitive, matrix, options.apply_node_transforms, options.unit_scale),
          mesh_options, &chunk.diagnostics);
      asset.mesh_chunks.push_back(std::move(chunk));
    }
  }

  for (assetdoc_size i = 0; i < node.children_count; ++i) {
    importNode(data, *node.children[i], options, asset);
  }
}

} // namespace

SceneAsset importSceneAsset(const std::filesystem::path &path,
                            const SceneAssetImportOptions options) {
  assetdoc_options parser_options{};
  assetdoc_data *raw_data = nullptr;
  assetdoc_result result = assetdoc_parse_file(&parser_options, path.string().c_str(), &raw_data);
  if (result != assetdoc_result_success) {
    throw std::runtime_error("Failed to parse scene asset file: " + path.string());
  }

  SceneFormatData data(raw_data);
  result = assetdoc_load_buffers(&parser_options, data.get(), path.string().c_str());
  if (result != assetdoc_result_success) {
    throw std::runtime_error("Failed to load scene asset buffers: " + path.string());
  }

  result = assetdoc_validate(data.get());
  if (result != assetdoc_result_success) {
    throw std::runtime_error("Scene asset validation failed: " + path.string());
  }

  SceneAsset asset;
  asset.material_slots.push_back(importedMaterialSlot(nullptr));
  for (assetdoc_size i = 0; i < data->materials_count; ++i) {
    asset.material_slots.push_back(importedMaterialSlot(&data->materials[i]));
  }

  const assetdoc_scene *scene = data->scene != nullptr ? data->scene : data->scenes;
  if (scene == nullptr) {
    throw std::runtime_error("Scene asset contains no scene.");
  }

  for (assetdoc_size i = 0; i < scene->nodes_count; ++i) {
    importNode(*data, *scene->nodes[i], options, asset);
  }

  if (asset.mesh_chunks.empty()) {
    throw std::runtime_error("Scene asset contains no renderable mesh primitives.");
  }

  applyOriginPolicy(asset, options.origin_policy);
  return asset;
}

} // namespace aster
