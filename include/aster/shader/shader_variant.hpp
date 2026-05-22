// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/material/material_asset.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class ShaderFeatureFlag : std::uint64_t {
  Textured = 1ull << 0ull,
  NormalMap = 1ull << 1ull,
  OrmTexture = 1ull << 2ull,
  Emissive = 1ull << 3ull,
  Height = 1ull << 4ull,
  Parallax = 1ull << 5ull,
  Triplanar = 1ull << 6ull,
  DecalReceiver = 1ull << 7ull,
  Fog = 1ull << 8ull,
  Shadow = 1ull << 9ull,
  AlphaClip = 1ull << 10ull,
  AlphaBlend = 1ull << 11ull,
  DoubleSided = 1ull << 12ull,
  Instancing = 1ull << 13ull,
  WorldPerceptualPrimitive = 1ull << 14ull,
  ContactField = 1ull << 15ull,
  LightHistory = 1ull << 16ull,
  InteractionResidue = 1ull << 17ull,
};

[[nodiscard]] constexpr std::uint64_t shaderFeatureFlagBit(const ShaderFeatureFlag flag) {
  return static_cast<std::uint64_t>(flag);
}

struct ShaderVariantKey {
  MaterialShadingModel shading_model = MaterialShadingModel::LitPBR;
  MaterialBlendMode blend_mode = MaterialBlendMode::Opaque;
  std::uint64_t feature_mask = 0u;
  std::uint64_t stable_hash = 0u;
  std::string tag;
};

enum class ShaderPermutationDimensionKind : std::uint32_t {
  Boolean,
  DenseInt,
  SparseInt,
};

struct ShaderPermutationDefine {
  std::string name;
  std::string value;
};

struct ShaderPermutationDimension {
  std::string name;
  std::string define_name;
  ShaderPermutationDimensionKind kind = ShaderPermutationDimensionKind::Boolean;
  std::int32_t first_value = 0;
  std::uint32_t value_count = 2u;
  std::vector<std::int32_t> sparse_values;
  std::int32_t default_value = 0;

  [[nodiscard]] static ShaderPermutationDimension boolean(std::string name,
                                                          std::string define_name,
                                                          bool default_value = false);
  [[nodiscard]] static ShaderPermutationDimension denseInt(std::string name,
                                                           std::string define_name,
                                                           std::int32_t first_value,
                                                           std::uint32_t value_count,
                                                           std::int32_t default_value);
  [[nodiscard]] static ShaderPermutationDimension sparseInt(std::string name,
                                                            std::string define_name,
                                                            std::vector<std::int32_t> values,
                                                            std::int32_t default_value);

  [[nodiscard]] bool contains(std::int32_t value) const;
  [[nodiscard]] std::uint32_t valueId(std::int32_t value) const;
  [[nodiscard]] std::uint32_t permutationCount() const;
  [[nodiscard]] std::string defineValue(std::int32_t value) const;
};

struct ShaderPermutationSelection {
  std::map<std::string, std::int32_t> values;

  ShaderPermutationSelection &set(std::string name, std::int32_t value);
  [[nodiscard]] std::int32_t valueFor(const ShaderPermutationDimension &dimension) const;
};

struct StableShaderKey {
  std::uint64_t hash = 0u;
  std::uint64_t permutation_id = 0u;
  std::string backend;
  std::string quality;
  std::string material_domain;
  std::string material_id;
  std::string tag;
  std::vector<ShaderPermutationDefine> defines;
};

class ShaderPermutationSpace {
public:
  ShaderPermutationSpace &add(ShaderPermutationDimension dimension);

  [[nodiscard]] const ShaderPermutationDimension *find(std::string_view name) const;
  [[nodiscard]] const std::vector<ShaderPermutationDimension> &dimensions() const noexcept;
  [[nodiscard]] std::vector<std::string>
  validate(const ShaderPermutationSelection &selection) const;
  [[nodiscard]] std::uint64_t permutationId(const ShaderPermutationSelection &selection) const;
  [[nodiscard]] std::vector<ShaderPermutationDefine>
  defines(const ShaderPermutationSelection &selection) const;
  [[nodiscard]] StableShaderKey stableKey(const ShaderPermutationSelection &selection,
                                          std::string_view backend,
                                          std::string_view quality,
                                          std::string_view material_domain,
                                          std::string_view material_id = {}) const;

private:
  std::vector<ShaderPermutationDimension> dimensions_;
};

[[nodiscard]] std::uint64_t stableShaderHash(std::string_view text);
[[nodiscard]] std::uint64_t stableShaderHashAppend(std::uint64_t hash,
                                                   std::string_view text);
[[nodiscard]] std::uint64_t stableShaderHashAppend(std::uint64_t hash,
                                                   std::uint64_t value);
[[nodiscard]] std::string shaderStableHex(std::uint64_t value);
[[nodiscard]] ShaderPermutationSpace makeMaterialShaderPermutationSpace();
[[nodiscard]] ShaderPermutationSelection
shaderPermutationSelectionForVariant(const ShaderVariantKey &variant,
                                     std::uint32_t backend_id = 0u,
                                     std::uint32_t quality_level = 2u);
[[nodiscard]] StableShaderKey stableShaderKeyForVariant(
    const ShaderVariantKey &variant, std::string_view backend = "software-reference",
    std::uint32_t backend_id = 0u, std::uint32_t quality_level = 2u,
    std::string_view material_domain = "surface", std::string_view material_id = {});
[[nodiscard]] StableShaderKey stableShaderKeyForMaterial(
    const MaterialAsset &asset, std::string_view backend = "software-reference",
    std::uint32_t backend_id = 0u, std::uint32_t quality_level = 2u,
    std::string_view material_domain = "surface");
[[nodiscard]] ShaderVariantKey shaderVariantKeyForMaterial(const MaterialAsset &asset);
[[nodiscard]] std::string shaderVariantTag(const ShaderVariantKey &key);

} // namespace aster
