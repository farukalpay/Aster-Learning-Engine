// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster::rhi {

enum class ShaderStage : std::uint32_t {
  Vertex = 1u << 0u,
  Fragment = 1u << 1u,
  Compute = 1u << 2u,
};

[[nodiscard]] constexpr std::uint32_t shaderStageBit(const ShaderStage stage) {
  return static_cast<std::uint32_t>(stage);
}

struct ShaderModuleDesc {
  std::uint32_t stages = 0u;
  std::string entry_point;
  std::vector<std::uint8_t> bytecode;
  std::string debug_label;
};

} // namespace aster::rhi
