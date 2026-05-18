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
};

struct CompiledMaterial {
  Material material{};
  std::uint32_t permutation_flags = 0u;
  std::uint64_t permutation_key = 0u;
  std::string pipeline_tag;
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

} // namespace aster
