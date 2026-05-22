// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/rhi/device.hpp"

#include <cstdint>
#include <string>

namespace aster::graphics_core7 {

struct RhiDeviceContractReport {
  bool native_backend = false;
  bool shader_materials = false;
  bool texture_sampling = false;
  bool storage_buffers = false;
  bool texture_arrays = false;
  bool presentation_surface = false;
  bool gpu_timestamps = false;
  bool hdr_render_targets = false;
  bool msaa = false;
  std::uint64_t contract_hash = 0u;
  std::string backend_label;
};

[[nodiscard]] RhiDeviceContractReport inspectRhiDeviceContract(
    const rhi::DeviceCapabilities &capabilities, bool native_backend);

} // namespace aster::graphics_core7
