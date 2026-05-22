// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>

namespace aster::graphics_core7 {

struct ReflectionProbeAtlasReport {
  bool requested = false;
  bool resource_advertised = false;
  bool capture_available = false;
  bool sampled_by_final_frame = false;
  std::size_t probe_count = 0u;
  std::uint32_t face_size = 0u;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] ReflectionProbeAtlasReport inspectReflectionProbeAtlas(
    bool requested, bool resource_advertised, bool capture_available, bool sampled_by_final_frame,
    std::size_t probe_count, std::uint32_t face_size);

} // namespace aster::graphics_core7
