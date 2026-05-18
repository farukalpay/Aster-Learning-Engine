// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>

namespace aster::rhi {

enum class ResourceState : std::uint32_t {
  Undefined,
  CopySource,
  CopyDestination,
  ShaderRead,
  ShaderWrite,
  ColorAttachment,
  DepthAttachment,
  Present,
  Readback,
};

struct ResourceBarrier {
  std::uint64_t resource_id = 0u;
  ResourceState before = ResourceState::Undefined;
  ResourceState after = ResourceState::Undefined;
};

} // namespace aster::rhi
