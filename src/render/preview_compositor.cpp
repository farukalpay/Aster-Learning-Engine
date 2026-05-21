// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/preview_compositor.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace aster {
namespace {

[[nodiscard]] std::uint8_t clampByte(const float value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

} // namespace

PreviewImageRgba8 makeCheckerPreview(const std::uint32_t width, const std::uint32_t height,
                                     const std::uint8_t dark, const std::uint8_t light) {
  PreviewImageRgba8 image;
  image.width = width;
  image.height = height;
  image.rgba8.resize(static_cast<std::size_t>(width) * height * 4u, 255u);
  for (std::uint32_t y = 0u; y < height; ++y) {
    for (std::uint32_t x = 0u; x < width; ++x) {
      const bool bright = ((x / 12u) + (y / 12u)) % 2u == 0u;
      const std::uint8_t value = bright ? light : dark;
      const std::size_t base = (static_cast<std::size_t>(y) * width + x) * 4u;
      image.rgba8[base + 0u] = value;
      image.rgba8[base + 1u] = value;
      image.rgba8[base + 2u] = value;
      image.rgba8[base + 3u] = 255u;
    }
  }
  return image;
}

PreviewImageRgba8 compositePreviewOver(const PreviewImageRgba8 &foreground,
                                       const PreviewImageRgba8 &background,
                                       const PreviewCompositeOptions options,
                                       PreviewCompositeReport *report) {
  PreviewCompositeReport local;
  if (foreground.width == 0u || foreground.height == 0u ||
      foreground.rgba8.size() != static_cast<std::size_t>(foreground.width) * foreground.height * 4u) {
    local.diagnostics.push_back("error: foreground preview is invalid");
    if (report != nullptr) {
      *report = local;
    }
    return {};
  }
  PreviewImageRgba8 base = background;
  if (options.checker_background || base.width != foreground.width ||
      base.height != foreground.height ||
      base.rgba8.size() != static_cast<std::size_t>(foreground.width) * foreground.height * 4u) {
    base = makeCheckerPreview(foreground.width, foreground.height, options.checker_dark,
                              options.checker_light);
  }
  PreviewImageRgba8 out = base;
  for (std::size_t i = 0u; i + 3u < foreground.rgba8.size(); i += 4u) {
    const float alpha = static_cast<float>(foreground.rgba8[i + 3u]) / 255.0f;
    for (std::size_t channel = 0u; channel < 3u; ++channel) {
      const float fg = static_cast<float>(foreground.rgba8[i + channel]) * options.exposure;
      const float bg = static_cast<float>(base.rgba8[i + channel]);
      out.rgba8[i + channel] = clampByte(fg * alpha + bg * (1.0f - alpha));
    }
    out.rgba8[i + 3u] = 255u;
  }
  local.ok = true;
  local.content_hash = hashPreviewImage(out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

PreviewImageRgba8 applyPreviewExposure(const PreviewImageRgba8 &image, const float exposure,
                                       PreviewCompositeReport *report) {
  PreviewCompositeReport local;
  PreviewImageRgba8 out = image;
  if (image.rgba8.size() != static_cast<std::size_t>(image.width) * image.height * 4u) {
    local.diagnostics.push_back("error: preview image dimensions do not match payload");
    if (report != nullptr) {
      *report = local;
    }
    return {};
  }
  for (std::size_t i = 0u; i + 3u < out.rgba8.size(); i += 4u) {
    out.rgba8[i + 0u] = clampByte(static_cast<float>(out.rgba8[i + 0u]) * exposure);
    out.rgba8[i + 1u] = clampByte(static_cast<float>(out.rgba8[i + 1u]) * exposure);
    out.rgba8[i + 2u] = clampByte(static_cast<float>(out.rgba8[i + 2u]) * exposure);
  }
  local.ok = true;
  local.content_hash = hashPreviewImage(out);
  if (report != nullptr) {
    *report = local;
  }
  return out;
}

std::string hashPreviewImage(const PreviewImageRgba8 &image) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto append = [&](const std::uint64_t value) {
    for (std::uint32_t shift = 0u; shift < 64u; shift += 8u) {
      hash ^= static_cast<std::uint8_t>((value >> shift) & 0xffu);
      hash *= 1099511628211ull;
    }
  };
  append(image.width);
  append(image.height);
  for (const std::uint8_t value : image.rgba8) {
    hash ^= value;
    hash *= 1099511628211ull;
  }
  std::ostringstream out;
  out << "0x" << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

} // namespace aster
