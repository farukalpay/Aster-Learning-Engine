// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/software_framebuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct RgbaImageView {
  int width = 0;
  int height = 0;
  std::span<const std::uint8_t> rgba{};

  [[nodiscard]] bool valid() const noexcept {
    return width > 0 && height > 0 &&
           rgba.size() >= static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
  }
};

struct VisualDiffTolerance {
  std::uint8_t red = 0u;
  std::uint8_t green = 0u;
  std::uint8_t blue = 0u;
  std::uint8_t alpha = 0u;
  std::uint8_t luminance = 0u;
  std::uint8_t edge_luminance = 96u;
  bool compare_luminance_only = false;
  bool soften_antialiased_edges = false;
  double maximum_local_error = 0.0;
  double maximum_global_error = 0.0;
};

struct VisualDiffResult {
  bool passed = false;
  bool dimensions_match = false;
  int width = 0;
  int height = 0;
  std::size_t pixel_count = 0u;
  std::size_t mismatch_count = 0u;
  std::size_t drift_pixel_count = 0u;
  std::size_t softened_edge_pixel_count = 0u;
  double max_local_error = 0.0;
  double global_error = 0.0;
  std::vector<std::uint8_t> delta_rgba;
  std::string summary;
};

struct VisualDiffArtifactPaths {
  std::filesystem::path approved;
  std::filesystem::path incoming;
  std::filesystem::path delta;
  std::filesystem::path contact_sheet;
};

[[nodiscard]] VisualDiffTolerance strictVisualDiffTolerance();
[[nodiscard]] VisualDiffTolerance previewVisualDiffTolerance();
[[nodiscard]] VisualDiffTolerance luminanceVisualDiffTolerance();

[[nodiscard]] RgbaImageView rgbaImageView(const SoftwareFrameBuffer &framebuffer) noexcept;
[[nodiscard]] VisualDiffResult compareRgbaImages(RgbaImageView approved, RgbaImageView incoming,
                                                 VisualDiffTolerance tolerance);
[[nodiscard]] VisualDiffResult compareFrameBuffers(const SoftwareFrameBuffer &approved,
                                                   const SoftwareFrameBuffer &incoming,
                                                   VisualDiffTolerance tolerance);
[[nodiscard]] std::vector<std::uint8_t>
makeVisualDiffContactSheet(RgbaImageView approved, RgbaImageView incoming,
                           const VisualDiffResult &result);

void writeRgbaPng(const std::filesystem::path &path, int width, int height,
                  std::span<const std::uint8_t> rgba);
void writeFrameBufferPng(const SoftwareFrameBuffer &framebuffer, const std::filesystem::path &path,
                         int expected_width = 0, int expected_height = 0);
void writeVisualDiffPng(const VisualDiffResult &result, const std::filesystem::path &path);
[[nodiscard]] VisualDiffArtifactPaths
writeVisualDiffArtifactSet(const std::filesystem::path &directory, std::string_view stem,
                           RgbaImageView approved, RgbaImageView incoming,
                           const VisualDiffResult &result);

} // namespace aster
