// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"

#include <cstdint>
#include <functional>

namespace aster::framegraph {

enum class ResourceLifetime : std::uint32_t {
  Transient,
  Imported,
  Readback,
};

struct ResourceHandle {
  std::uint32_t index = 0u;
  std::uint32_t generation = 0u;

  [[nodiscard]] constexpr bool valid() const {
    return generation != 0u;
  }

  [[nodiscard]] friend constexpr bool operator==(const ResourceHandle lhs,
                                                 const ResourceHandle rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
  }
};

struct ResourceDesc {
  ResourceLifetime lifetime = ResourceLifetime::Transient;
  rhi::ImageFormat format = rhi::ImageFormat::Unknown;
  rhi::ImageExtent extent{};
  std::uint32_t usage = 0u;
};

} // namespace aster::framegraph

namespace std {

template <> struct hash<aster::framegraph::ResourceHandle> {
  [[nodiscard]] std::size_t
  operator()(const aster::framegraph::ResourceHandle handle) const noexcept {
    return std::hash<std::uint64_t>{}(
        (static_cast<std::uint64_t>(handle.generation) << 32u) | handle.index);
  }
};

} // namespace std
