// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

struct PreviewImageRgba8 {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<std::uint8_t> rgba8;
};

struct PreviewCompositeOptions {
  float exposure = 1.0f;
  bool checker_background = true;
  std::uint8_t checker_dark = 42u;
  std::uint8_t checker_light = 76u;
};

struct PreviewCompositeReport {
  bool ok = false;
  std::string content_hash;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] PreviewImageRgba8 makeCheckerPreview(std::uint32_t width, std::uint32_t height,
                                                   std::uint8_t dark = 42u,
                                                   std::uint8_t light = 76u);
[[nodiscard]] PreviewImageRgba8 compositePreviewOver(const PreviewImageRgba8 &foreground,
                                                     const PreviewImageRgba8 &background,
                                                     PreviewCompositeOptions options = {},
                                                     PreviewCompositeReport *report = nullptr);
[[nodiscard]] PreviewImageRgba8 applyPreviewExposure(const PreviewImageRgba8 &image,
                                                     float exposure,
                                                     PreviewCompositeReport *report = nullptr);
[[nodiscard]] std::string hashPreviewImage(const PreviewImageRgba8 &image);

} // namespace aster
