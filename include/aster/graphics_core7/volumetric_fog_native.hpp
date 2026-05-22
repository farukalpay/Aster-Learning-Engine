// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>

namespace aster::graphics_core7 {

struct NativeVolumetricFogReport {
  bool requested = false;
  bool resource_advertised = false;
  bool capture_available = false;
  bool sampled_by_final_frame = false;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] NativeVolumetricFogReport inspectNativeVolumetricFog(
    bool requested, bool resource_advertised, bool capture_available, bool sampled_by_final_frame,
    std::uint32_t width, std::uint32_t height);

} // namespace aster::graphics_core7
