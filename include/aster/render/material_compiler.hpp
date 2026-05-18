// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/scene/scene.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace aster {

enum class MaterialPermutationFlag : std::uint32_t {
  Textured = 1u << 0u,
  Procedural = 1u << 1u,
  Transparent = 1u << 2u,
  DoubleSided = 1u << 3u,
  DepthWrite = 1u << 4u,
  ContactShadow = 1u << 5u,
  ShaderVariant = 1u << 6u,
  DepthBias = 1u << 7u,
};

struct CompiledMaterial {
  Material material{};
  std::uint32_t permutation_flags = 0u;
  std::uint64_t permutation_key = 0u;
  std::string pipeline_tag;
};

struct MaterialPermutationArtifact {
  std::uint64_t permutation_key = 0u;
  std::uint64_t source_hash = 0u;
  std::uint64_t artifact_hash = 0u;
  std::uint32_t permutation_flags = 0u;
  std::string pipeline_tag;
  std::string shader_variant_tag;
  std::string backend_name;
  std::string fallback_reason;
};

[[nodiscard]] constexpr std::uint32_t materialPermutationFlagBit(
    const MaterialPermutationFlag flag) {
  return static_cast<std::uint32_t>(flag);
}

[[nodiscard]] bool materialHasProceduralPermutation(const Material &material);
[[nodiscard]] CompiledMaterial compileMaterialForRendering(const Material &material,
                                                          bool has_texture_dependencies = false,
                                                          std::string_view material_id = {});
[[nodiscard]] std::uint64_t materialPermutationKey(const Material &material,
                                                   bool has_texture_dependencies = false);
[[nodiscard]] std::string materialPipelineTag(const CompiledMaterial &material);
[[nodiscard]] MaterialPermutationArtifact materialPermutationArtifactFor(
    const CompiledMaterial &material, std::string_view backend_name,
    std::string_view shader_variant_tag = {}, std::string_view fallback_reason = {});

} // namespace aster
