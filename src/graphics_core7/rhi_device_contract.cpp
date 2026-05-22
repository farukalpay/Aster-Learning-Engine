// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/rhi_device_contract.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

RhiDeviceContractReport inspectRhiDeviceContract(const rhi::DeviceCapabilities &capabilities,
                                                 const bool native_backend) {
  RhiDeviceContractReport report;
  report.native_backend = native_backend;
  report.shader_materials = capabilities.shader_materials;
  report.texture_sampling = capabilities.texture_sampling;
  report.storage_buffers = capabilities.storage_buffers;
  report.texture_arrays = capabilities.texture_arrays;
  report.presentation_surface = capabilities.presentation != rhi::PresentationMode::None;
  report.gpu_timestamps = capabilities.gpu_timestamps;
  report.hdr_render_targets = capabilities.hdr_render_targets;
  report.msaa = capabilities.msaa;
  report.backend_label = std::to_string(static_cast<std::uint32_t>(capabilities.backend));
  std::uint64_t hash = 0xA57E700700000101ull;
  hash = hashCombine64(hash, static_cast<std::uint64_t>(capabilities.backend));
  hash = hashCombine64(hash, native_backend ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.shader_materials ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.texture_sampling ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.storage_buffers ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.texture_arrays ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.gpu_timestamps ? 1u : 0u);
  hash = hashCombine64(hash, capabilities.hdr_render_targets ? 1u : 0u);
  report.contract_hash = hashCombine64(hash, capabilities.msaa ? 1u : 0u);
  return report;
}

} // namespace aster::graphics_core7
