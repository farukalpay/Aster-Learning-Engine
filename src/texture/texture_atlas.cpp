// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_atlas.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace aster {

namespace {

struct FreeSpan {
  std::uint32_t start = 0u;
  std::uint32_t length = 0u;
};

struct AtlasShelf {
  std::uint32_t y = 0u;
  std::uint32_t height = 0u;
  std::vector<FreeSpan> free_spans;
};

struct TexturePlacementInput {
  std::size_t original_index = 0u;
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
};

std::uint32_t nextPowerOfTwo(std::uint32_t value) {
  value = std::max(value, 1u);
  --value;
  value |= value >> 1u;
  value |= value >> 2u;
  value |= value >> 4u;
  value |= value >> 8u;
  value |= value >> 16u;
  return value + 1u;
}

void finalizeAtlasPlan(TextureAtlasPlan &plan, const std::uint64_t occupied_area) {
  if (plan.width == 0u || plan.height == 0u) {
    plan.occupancy = 0.0;
    return;
  }
  const double area = static_cast<double>(plan.width) * static_cast<double>(plan.height);
  plan.occupancy = area <= 0.0 ? 0.0 : static_cast<double>(occupied_area) / area;
  for (TextureAtlasEntry &entry : plan.entries) {
    entry.u0 = static_cast<float>(entry.x) / static_cast<float>(plan.width);
    entry.v0 = static_cast<float>(entry.y) / static_cast<float>(plan.height);
    entry.u1 = static_cast<float>(entry.x + entry.width) / static_cast<float>(plan.width);
    entry.v1 = static_cast<float>(entry.y + entry.height) / static_cast<float>(plan.height);
  }
}

std::array<std::uint8_t, 3> atlasColor(const std::size_t index) {
  const std::uint32_t hash =
      static_cast<std::uint32_t>((index + 1u) * 747796405u + 2891336453u);
  return {static_cast<std::uint8_t>(72u + (hash & 0x7fu)),
          static_cast<std::uint8_t>(72u + ((hash >> 8u) & 0x7fu)),
          static_cast<std::uint8_t>(72u + ((hash >> 16u) & 0x7fu))};
}

} // namespace

TextureAtlasPlan packTextureAtlasRows(const std::vector<TextureAssetMetadata> &textures,
                                      const std::uint32_t max_width) {
  TextureAtlasPlan plan;
  std::uint32_t x = 0u;
  std::uint32_t y = 0u;
  std::uint32_t row_height = 0u;
  plan.width = std::max(max_width, 1u);
  std::uint64_t occupied_area = 0u;
  for (const TextureAssetMetadata &texture : textures) {
    const std::uint32_t width = std::max(texture.width, 1u);
    const std::uint32_t height = std::max(texture.height, 1u);
    if (x > 0u && x + width > plan.width) {
      x = 0u;
      y += row_height;
      row_height = 0u;
    }
    plan.entries.push_back({.texture = texture,
                            .x = x,
                            .y = y,
                            .width = width,
                            .height = height});
    occupied_area += static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    x += width;
    row_height = std::max(row_height, height);
  }
  plan.height = y + row_height;
  finalizeAtlasPlan(plan, occupied_area);
  return plan;
}

TextureAtlasPlan packTextureAtlasTight(const std::vector<TextureAssetMetadata> &textures,
                                       TextureAtlasPackingOptions options) {
  options.max_width = std::max(options.max_width, 1u);

  TextureAtlasPlan plan;
  plan.width = options.max_width;
  plan.padding = options.padding;
  plan.entries.resize(textures.size());
  if (textures.empty()) {
    return plan;
  }

  std::vector<TexturePlacementInput> ordered;
  ordered.reserve(textures.size());
  std::uint32_t widest = 1u;
  std::uint64_t occupied_area = 0u;
  for (std::size_t i = 0u; i < textures.size(); ++i) {
    const std::uint32_t width = std::max(textures[i].width, 1u);
    const std::uint32_t height = std::max(textures[i].height, 1u);
    ordered.push_back({.original_index = i, .width = width, .height = height});
    widest = std::max(widest, width + options.padding * 2u);
    occupied_area += static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  }
  plan.width = std::max(plan.width, widest);

  std::sort(ordered.begin(), ordered.end(), [](const TexturePlacementInput &a,
                                               const TexturePlacementInput &b) {
    const std::uint64_t area_a = static_cast<std::uint64_t>(a.width) * a.height;
    const std::uint64_t area_b = static_cast<std::uint64_t>(b.width) * b.height;
    if (std::max(a.width, a.height) != std::max(b.width, b.height)) {
      return std::max(a.width, a.height) > std::max(b.width, b.height);
    }
    if (area_a != area_b) {
      return area_a > area_b;
    }
    return a.original_index < b.original_index;
  });

  std::vector<AtlasShelf> shelves;
  std::uint32_t used_height = 0u;
  for (const TexturePlacementInput &input : ordered) {
    const std::uint32_t padded_width = input.width + options.padding * 2u;
    const std::uint32_t padded_height = input.height + options.padding * 2u;

    std::size_t best_shelf = shelves.size();
    std::size_t best_span = 0u;
    std::uint32_t best_waste = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t shelf_index = 0u; shelf_index < shelves.size(); ++shelf_index) {
      AtlasShelf &shelf = shelves[shelf_index];
      if (shelf.height < padded_height) {
        continue;
      }
      for (std::size_t span_index = 0u; span_index < shelf.free_spans.size(); ++span_index) {
        const FreeSpan span = shelf.free_spans[span_index];
        if (span.length < padded_width) {
          continue;
        }
        const std::uint32_t waste = span.length - padded_width + shelf.height - padded_height;
        if (waste < best_waste) {
          best_shelf = shelf_index;
          best_span = span_index;
          best_waste = waste;
        }
      }
    }

    if (best_shelf == shelves.size()) {
      shelves.push_back({.y = used_height,
                         .height = padded_height,
                         .free_spans = {FreeSpan{.start = 0u, .length = plan.width}}});
      best_shelf = shelves.size() - 1u;
      best_span = 0u;
      used_height += padded_height;
    }

    AtlasShelf &shelf = shelves[best_shelf];
    FreeSpan &span = shelf.free_spans[best_span];
    const std::uint32_t padded_x = span.start;
    const std::uint32_t padded_y = shelf.y;
    span.start += padded_width;
    span.length -= padded_width;
    if (span.length == 0u) {
      shelf.free_spans.erase(shelf.free_spans.begin() + static_cast<std::ptrdiff_t>(best_span));
    }

    TextureAtlasEntry entry;
    entry.texture = textures[input.original_index];
    entry.x = padded_x + options.padding;
    entry.y = padded_y + options.padding;
    entry.width = input.width;
    entry.height = input.height;
    plan.entries[input.original_index] = std::move(entry);
  }

  plan.height = used_height;
  if (options.power_of_two_extent) {
    plan.width = nextPowerOfTwo(plan.width);
    plan.height = nextPowerOfTwo(plan.height);
  }
  if (!options.preserve_input_order) {
    std::sort(plan.entries.begin(), plan.entries.end(), [](const TextureAtlasEntry &a,
                                                           const TextureAtlasEntry &b) {
      if (a.y != b.y) {
        return a.y < b.y;
      }
      return a.x < b.x;
    });
  }
  finalizeAtlasPlan(plan, occupied_area);
  return plan;
}

std::vector<std::uint8_t> makeTextureAtlasDebugRgba(const TextureAtlasPlan &plan) {
  if (plan.width == 0u || plan.height == 0u) {
    return {};
  }

  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(plan.width) *
                                     static_cast<std::size_t>(plan.height) * 4u,
                                 18u);
  for (std::uint32_t y = 0u; y < plan.height; ++y) {
    for (std::uint32_t x = 0u; x < plan.width; ++x) {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(plan.width) + x) * 4u;
      const bool checker = ((x / 8u) + (y / 8u)) % 2u == 0u;
      rgba[offset + 0u] = checker ? 24u : 34u;
      rgba[offset + 1u] = checker ? 28u : 32u;
      rgba[offset + 2u] = checker ? 36u : 44u;
      rgba[offset + 3u] = 255u;
    }
  }

  for (std::size_t i = 0u; i < plan.entries.size(); ++i) {
    const TextureAtlasEntry &entry = plan.entries[i];
    const auto color = atlasColor(i);
    const std::uint32_t max_y = std::min(plan.height, entry.y + entry.height);
    const std::uint32_t max_x = std::min(plan.width, entry.x + entry.width);
    for (std::uint32_t y = entry.y; y < max_y; ++y) {
      for (std::uint32_t x = entry.x; x < max_x; ++x) {
        const std::size_t offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(plan.width) + x) * 4u;
        const bool border = y == entry.y || x == entry.x || y + 1u == max_y || x + 1u == max_x;
        rgba[offset + 0u] = border ? 255u : color[0u];
        rgba[offset + 1u] = border ? 255u : color[1u];
        rgba[offset + 2u] = border ? 255u : color[2u];
        rgba[offset + 3u] = 255u;
      }
    }
  }
  return rgba;
}

} // namespace aster
