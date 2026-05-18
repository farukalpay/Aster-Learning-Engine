// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace aster::rhi {

enum class BufferUsage : std::uint32_t {
  Vertex = 1u << 0u,
  Index = 1u << 1u,
  Uniform = 1u << 2u,
  Storage = 1u << 3u,
  Upload = 1u << 4u,
  Readback = 1u << 5u,
  Indirect = 1u << 6u,
};

[[nodiscard]] constexpr std::uint32_t bufferUsageBit(const BufferUsage usage) {
  return static_cast<std::uint32_t>(usage);
}

struct BufferDesc {
  std::size_t byte_size = 0u;
  std::uint32_t usage = 0u;
  bool cpu_visible = false;
  std::string debug_label;
};

} // namespace aster::rhi
