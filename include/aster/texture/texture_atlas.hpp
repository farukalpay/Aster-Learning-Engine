// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/texture/texture_asset.hpp"

#include <vector>

namespace aster {

struct TextureAtlasEntry {
  TextureAssetMetadata texture;
  std::uint32_t x = 0u;
  std::uint32_t y = 0u;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  bool rotated = false;
  float u0 = 0.0f;
  float v0 = 0.0f;
  float u1 = 0.0f;
  float v1 = 0.0f;
};

struct TextureAtlasPlan {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t padding = 0u;
  double occupancy = 0.0;
  std::vector<TextureAtlasEntry> entries;
};

struct TextureAtlasPackingOptions {
  std::uint32_t max_width = 2048u;
  std::uint32_t padding = 0u;
  bool power_of_two_extent = false;
  bool preserve_input_order = true;
};

[[nodiscard]] TextureAtlasPlan packTextureAtlasRows(const std::vector<TextureAssetMetadata> &textures,
                                                    std::uint32_t max_width);
[[nodiscard]] TextureAtlasPlan
packTextureAtlasTight(const std::vector<TextureAssetMetadata> &textures,
                      TextureAtlasPackingOptions options = {});
[[nodiscard]] std::vector<std::uint8_t> makeTextureAtlasDebugRgba(const TextureAtlasPlan &plan);

} // namespace aster
