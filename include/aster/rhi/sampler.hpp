// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <string>

namespace aster::rhi {

enum class FilterMode {
  Nearest,
  Linear,
};

enum class AddressMode {
  ClampToEdge,
  Repeat,
  MirroredRepeat,
};

struct SamplerDesc {
  FilterMode min_filter = FilterMode::Linear;
  FilterMode mag_filter = FilterMode::Linear;
  FilterMode mip_filter = FilterMode::Linear;
  AddressMode address_u = AddressMode::ClampToEdge;
  AddressMode address_v = AddressMode::ClampToEdge;
  AddressMode address_w = AddressMode::ClampToEdge;
  float min_lod = 0.0f;
  float max_lod = 1000.0f;
  std::string debug_label;
};

} // namespace aster::rhi
