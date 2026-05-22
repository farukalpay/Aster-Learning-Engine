// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>

namespace aster::graphics_core7 {

struct NativeShadowAtlasReport {
  bool requested = false;
  bool resource_advertised = false;
  bool capture_available = false;
  bool sampled_by_final_frame = false;
  std::uint32_t atlas_size = 0u;
  std::uint32_t cascade_count = 0u;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] NativeShadowAtlasReport inspectNativeShadowAtlas(
    bool requested, bool resource_advertised, bool capture_available, bool sampled_by_final_frame,
    std::uint32_t atlas_size, std::uint32_t cascade_count);

} // namespace aster::graphics_core7
