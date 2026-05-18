// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/shader/shader_variant.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

enum class ShaderResourceKind {
  UniformBuffer,
  Texture,
  Sampler,
};

struct ShaderResourceBinding {
  std::string name;
  ShaderResourceKind kind = ShaderResourceKind::UniformBuffer;
  std::uint32_t binding = 0u;
  std::uint32_t count = 1u;
};

struct ShaderReflection {
  std::vector<ShaderResourceBinding> resources;
};

[[nodiscard]] ShaderReflection reflectShaderVariantBindings(const ShaderVariantKey &variant);

} // namespace aster
