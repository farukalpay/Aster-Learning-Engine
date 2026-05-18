// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/adapter.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct DeviceCapabilities {
  BackendKind backend = BackendKind::Unknown;
  bool shader_materials = false;
  bool texture_sampling = false;
  bool instancing = false;
  bool capture = false;
  bool ui_composite = false;
  bool gpu_timestamps = false;
};

struct DeviceDesc {
  AdapterDesc adapter{};
  std::string debug_label;
};

} // namespace aster::rhi
