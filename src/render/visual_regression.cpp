// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/visual_regression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

struct Pixel {
  std::uint8_t r = 0u;
  std::uint8_t g = 0u;
  std::uint8_t b = 0u;
  std::uint8_t a = 255u;
  bool present = false;
};

std::size_t pixelOffset(const int width, const int x, const int y) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(x)) *
         4u;
}

Pixel pixelAt(const aster::RgbaImageView image, const int x, const int y) {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height || !image.valid()) {
    return {};
  }
  const std::size_t offset = pixelOffset(image.width, x, y);
  return {image.rgba[offset + 0u], image.rgba[offset + 1u], image.rgba[offset + 2u],
          image.rgba[offset + 3u], true};
}

double luminance(const Pixel pixel) {
  const double alpha = static_cast<double>(pixel.a) / 255.0;
  return (0.2126 * static_cast<double>(pixel.r) + 0.7152 * static_cast<double>(pixel.g) +
          0.0722 * static_cast<double>(pixel.b)) *
         alpha;
}

double channelResidual(const std::uint8_t a, const std::uint8_t b, const std::uint8_t tolerance) {
  const int difference = std::abs(static_cast<int>(a) - static_cast<int>(b));
  return static_cast<double>(std::max(0, difference - static_cast<int>(tolerance))) / 255.0;
}

bool isEdgeLike(const aster::RgbaImageView image, const int x, const int y,
                const aster::VisualDiffTolerance tolerance) {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height || !image.valid()) {
    return false;
  }

  double min_luminance = std::numeric_limits<double>::max();
  double max_luminance = std::numeric_limits<double>::lowest();
  std::size_t samples = 0u;
  for (int py = y - 1; py <= y + 1; ++py) {
    for (int px = x - 1; px <= x + 1; ++px) {
      const Pixel sample = pixelAt(image, px, py);
      if (!sample.present) {
        continue;
      }
      const double value = luminance(sample);
      min_luminance = std::min(min_luminance, value);
      max_luminance = std::max(max_luminance, value);
      ++samples;
    }
  }
  return samples >= 3u &&
         (max_luminance - min_luminance) > static_cast<double>(tolerance.edge_luminance);
}

double pixelError(const Pixel approved, const Pixel incoming,
                  const aster::VisualDiffTolerance tolerance) {
  if (!approved.present || !incoming.present) {
    return 1.0;
  }

  const double alpha = channelResidual(approved.a, incoming.a, tolerance.alpha);
  const double luminance_residual =
      std::max(0.0, std::abs(luminance(approved) - luminance(incoming)) -
                        static_cast<double>(tolerance.luminance)) /
      255.0;
  if (tolerance.compare_luminance_only) {
    return std::sqrt((luminance_residual * luminance_residual + alpha * alpha) * 0.5);
  }

  const double red = channelResidual(approved.r, incoming.r, tolerance.red);
  const double green = channelResidual(approved.g, incoming.g, tolerance.green);
  const double blue = channelResidual(approved.b, incoming.b, tolerance.blue);
  return std::sqrt((red * red + green * green + blue * blue + alpha * alpha +
                    luminance_residual * luminance_residual) /
                   5.0);
}

std::uint8_t heatByte(const double value, const int floor) {
  return static_cast<std::uint8_t>(
      std::clamp(floor + static_cast<int>(std::round(value * static_cast<double>(255 - floor))),
                 0, 255));
}

void setPixel(std::vector<std::uint8_t> &rgba, const int width, const int x, const int y,
              const Pixel pixel) {
  const std::size_t offset = pixelOffset(width, x, y);
  rgba[offset + 0u] = pixel.r;
  rgba[offset + 1u] = pixel.g;
  rgba[offset + 2u] = pixel.b;
  rgba[offset + 3u] = pixel.a;
}

void setPixel(std::vector<std::uint8_t> &rgba, const int width, const int x, const int y,
              const std::uint8_t r, const std::uint8_t g, const std::uint8_t b,
              const std::uint8_t a = 255u) {
  setPixel(rgba, width, x, y, Pixel{r, g, b, a, true});
}

void copyPanel(std::vector<std::uint8_t> &sheet, const int sheet_width, const int sheet_height,
               const int dst_x, const int dst_y, const int panel_width, const int panel_height,
               const aster::RgbaImageView image) {
  for (int y = 0; y < panel_height; ++y) {
    for (int x = 0; x < panel_width; ++x) {
      const int sx = dst_x + x;
      const int sy = dst_y + y;
      if (sx < 0 || sy < 0 || sx >= sheet_width || sy >= sheet_height) {
        continue;
      }
      Pixel pixel = pixelAt(image, x, y);
      if (!pixel.present) {
        const bool checker = ((x / 6) + (y / 6)) % 2 == 0;
        pixel = checker ? Pixel{54u, 48u, 66u, 255u, true}
                        : Pixel{28u, 24u, 34u, 255u, true};
      }
      setPixel(sheet, sheet_width, sx, sy, pixel);
    }
  }
}

std::uint32_t crc32Update(std::uint32_t crc, const std::span<const std::uint8_t> bytes) {
  crc = ~crc;
  for (const std::uint8_t byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

std::uint32_t adler32(const std::span<const std::uint8_t> bytes) {
  constexpr std::uint32_t mod = 65521u;
  std::uint32_t a = 1u;
  std::uint32_t b = 0u;
  for (const std::uint8_t byte : bytes) {
    a = (a + byte) % mod;
    b = (b + a) % mod;
  }
  return (b << 16u) | a;
}

void appendBe32(std::vector<std::uint8_t> &bytes, const std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendLe16(std::vector<std::uint8_t> &bytes, const std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void appendChunk(std::vector<std::uint8_t> &png, const std::array<std::uint8_t, 4> type,
                 const std::span<const std::uint8_t> data) {
  appendBe32(png, static_cast<std::uint32_t>(data.size()));
  const std::size_t crc_start = png.size();
  png.insert(png.end(), type.begin(), type.end());
  png.insert(png.end(), data.begin(), data.end());
  const std::uint32_t crc =
      crc32Update(0u, std::span<const std::uint8_t>{png.data() + crc_start,
                                                    png.size() - crc_start});
  appendBe32(png, crc);
}

std::vector<std::uint8_t> makePngBytes(const int width, const int height,
                                       const std::span<const std::uint8_t> rgba) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("PNG output requires positive dimensions.");
  }
  const std::size_t expected =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
  if (rgba.size() < expected) {
    throw std::invalid_argument("PNG output received too few RGBA bytes.");
  }

  std::vector<std::uint8_t> scanlines;
  scanlines.reserve(static_cast<std::size_t>(height) *
                    (static_cast<std::size_t>(width) * 4u + 1u));
  for (int y = 0; y < height; ++y) {
    scanlines.push_back(0u);
    const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u;
    scanlines.insert(scanlines.end(), rgba.begin() + static_cast<std::ptrdiff_t>(row),
                     rgba.begin() + static_cast<std::ptrdiff_t>(row +
                                                                static_cast<std::size_t>(width) *
                                                                    4u));
  }

  std::vector<std::uint8_t> zlib;
  zlib.reserve(scanlines.size() + scanlines.size() / 65535u * 5u + 16u);
  zlib.push_back(0x78u);
  zlib.push_back(0x01u);
  for (std::size_t offset = 0u; offset < scanlines.size();) {
    const std::size_t remaining = scanlines.size() - offset;
    const std::uint16_t length =
        static_cast<std::uint16_t>(std::min<std::size_t>(remaining, 65535u));
    const bool final_block = offset + length == scanlines.size();
    zlib.push_back(final_block ? 0x01u : 0x00u);
    appendLe16(zlib, length);
    appendLe16(zlib, static_cast<std::uint16_t>(~length));
    zlib.insert(zlib.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(offset),
                scanlines.begin() + static_cast<std::ptrdiff_t>(offset + length));
    offset += length;
  }
  appendBe32(zlib, adler32(scanlines));

  std::vector<std::uint8_t> png;
  png.reserve(zlib.size() + 96u);
  constexpr std::array<std::uint8_t, 8> signature{137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  png.insert(png.end(), signature.begin(), signature.end());

  std::array<std::uint8_t, 13> ihdr{};
  ihdr[0] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(width) >> 24u) & 0xffu);
  ihdr[1] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(width) >> 16u) & 0xffu);
  ihdr[2] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(width) >> 8u) & 0xffu);
  ihdr[3] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(width) & 0xffu);
  ihdr[4] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(height) >> 24u) & 0xffu);
  ihdr[5] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(height) >> 16u) & 0xffu);
  ihdr[6] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(height) >> 8u) & 0xffu);
  ihdr[7] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(height) & 0xffu);
  ihdr[8] = 8u;
  ihdr[9] = 6u;
  ihdr[10] = 0u;
  ihdr[11] = 0u;
  ihdr[12] = 0u;
  appendChunk(png, {'I', 'H', 'D', 'R'}, ihdr);
  appendChunk(png, {'I', 'D', 'A', 'T'}, zlib);
  appendChunk(png, {'I', 'E', 'N', 'D'}, std::span<const std::uint8_t>{});
  return png;
}

} // namespace

namespace aster {

VisualDiffTolerance strictVisualDiffTolerance() {
  return {};
}

VisualDiffTolerance previewVisualDiffTolerance() {
  return {.red = 10u,
          .green = 10u,
          .blue = 10u,
          .alpha = 3u,
          .luminance = 8u,
          .edge_luminance = 72u,
          .compare_luminance_only = false,
          .soften_antialiased_edges = true,
          .maximum_local_error = 0.045,
          .maximum_global_error = 0.012};
}

VisualDiffTolerance luminanceVisualDiffTolerance() {
  return {.red = 16u,
          .green = 16u,
          .blue = 16u,
          .alpha = 4u,
          .luminance = 10u,
          .edge_luminance = 72u,
          .compare_luminance_only = true,
          .soften_antialiased_edges = true,
          .maximum_local_error = 0.045,
          .maximum_global_error = 0.010};
}

RgbaImageView rgbaImageView(const SoftwareFrameBuffer &framebuffer) noexcept {
  return {.width = framebuffer.width(), .height = framebuffer.height(), .rgba = framebuffer.rgba8()};
}

VisualDiffResult compareRgbaImages(const RgbaImageView approved, const RgbaImageView incoming,
                                   const VisualDiffTolerance tolerance) {
  if (!approved.valid() || !incoming.valid()) {
    throw std::invalid_argument("Visual diff requires valid RGBA image views.");
  }

  VisualDiffResult result;
  result.dimensions_match = approved.width == incoming.width && approved.height == incoming.height;
  result.width = std::max(approved.width, incoming.width);
  result.height = std::max(approved.height, incoming.height);
  result.pixel_count = static_cast<std::size_t>(result.width) * static_cast<std::size_t>(result.height);
  result.delta_rgba.assign(result.pixel_count * 4u, 0u);

  double global_error_sum = 0.0;
  for (int y = 0; y < result.height; ++y) {
    for (int x = 0; x < result.width; ++x) {
      const Pixel a = pixelAt(approved, x, y);
      const Pixel b = pixelAt(incoming, x, y);
      double local_error = pixelError(a, b, tolerance);
      bool softened_edge = false;
      if (local_error > tolerance.maximum_local_error && tolerance.soften_antialiased_edges &&
          (isEdgeLike(approved, x, y, tolerance) || isEdgeLike(incoming, x, y, tolerance))) {
        const double softened_error = local_error * 0.32;
        if (softened_error <= tolerance.maximum_local_error) {
          local_error = softened_error;
          softened_edge = true;
          ++result.softened_edge_pixel_count;
        }
      }

      result.max_local_error = std::max(result.max_local_error, local_error);
      global_error_sum += local_error;
      if (local_error > 0.0) {
        ++result.drift_pixel_count;
      }
      if (local_error > tolerance.maximum_local_error) {
        ++result.mismatch_count;
      }

      if (!a.present || !b.present) {
        setPixel(result.delta_rgba, result.width, x, y, 255u, 0u, 70u);
      } else if (local_error == 0.0) {
        setPixel(result.delta_rgba, result.width, x, y, 0u, 0u, 0u);
      } else if (softened_edge) {
        setPixel(result.delta_rgba, result.width, x, y, 255u, 190u, 32u);
      } else if (local_error > tolerance.maximum_local_error) {
        setPixel(result.delta_rgba, result.width, x, y, heatByte(local_error, 92), 18u,
                 heatByte(local_error, 90));
      } else {
        setPixel(result.delta_rgba, result.width, x, y, 0u, heatByte(local_error, 104),
                 heatByte(local_error, 160));
      }
    }
  }

  result.global_error =
      result.pixel_count == 0u ? 0.0 : global_error_sum / static_cast<double>(result.pixel_count);
  result.passed = result.dimensions_match && result.mismatch_count == 0u &&
                  result.global_error <= tolerance.maximum_global_error;
  result.summary = result.passed ? "visual diff passed" : "visual diff failed";
  return result;
}

VisualDiffResult compareFrameBuffers(const SoftwareFrameBuffer &approved,
                                     const SoftwareFrameBuffer &incoming,
                                     const VisualDiffTolerance tolerance) {
  return compareRgbaImages(rgbaImageView(approved), rgbaImageView(incoming), tolerance);
}

std::vector<std::uint8_t> makeVisualDiffContactSheet(const RgbaImageView approved,
                                                     const RgbaImageView incoming,
                                                     const VisualDiffResult &result) {
  if (result.width <= 0 || result.height <= 0 ||
      result.delta_rgba.size() < result.pixel_count * 4u) {
    throw std::invalid_argument("Visual diff contact sheet requires a populated comparison result.");
  }

  constexpr int gutter = 4;
  constexpr int strip = 8;
  const int panel_width = result.width;
  const int panel_height = result.height;
  const int sheet_width = panel_width * 3 + gutter * 2;
  const int sheet_height = panel_height + strip;
  std::vector<std::uint8_t> sheet(static_cast<std::size_t>(sheet_width) *
                                      static_cast<std::size_t>(sheet_height) * 4u,
                                  18u);
  for (std::size_t i = 3u; i < sheet.size(); i += 4u) {
    sheet[i] = 255u;
  }

  const auto fill_strip = [&](const int x0, const std::uint8_t r, const std::uint8_t g,
                              const std::uint8_t b) {
    for (int y = 0; y < strip; ++y) {
      for (int x = 0; x < panel_width; ++x) {
        setPixel(sheet, sheet_width, x0 + x, y, r, g, b);
      }
    }
  };
  fill_strip(0, 78u, 196u, 125u);
  fill_strip(panel_width + gutter, 66u, 148u, 226u);
  fill_strip((panel_width + gutter) * 2, result.passed ? 84u : 230u,
             result.passed ? 190u : 74u, result.passed ? 126u : 92u);

  copyPanel(sheet, sheet_width, sheet_height, 0, strip, panel_width, panel_height, approved);
  copyPanel(sheet, sheet_width, sheet_height, panel_width + gutter, strip, panel_width,
            panel_height, incoming);
  copyPanel(sheet, sheet_width, sheet_height, (panel_width + gutter) * 2, strip, panel_width,
            panel_height,
            RgbaImageView{.width = result.width,
                          .height = result.height,
                          .rgba = std::span<const std::uint8_t>{result.delta_rgba.data(),
                                                                 result.delta_rgba.size()}});
  return sheet;
}

void writeRgbaPng(const std::filesystem::path &path, const int width, const int height,
                  const std::span<const std::uint8_t> rgba) {
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const std::vector<std::uint8_t> png = makePngBytes(width, height, rgba);
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open PNG output path.");
  }
  file.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
  if (!file.good()) {
    throw std::runtime_error("Could not write PNG output.");
  }
}

void writeFrameBufferPng(const SoftwareFrameBuffer &framebuffer, const std::filesystem::path &path,
                         const int expected_width, const int expected_height) {
  if (framebuffer.empty()) {
    throw std::runtime_error("PNG framebuffer capture requested before a frame was rendered.");
  }
  if (expected_width > 0 && expected_height > 0 &&
      (framebuffer.width() != expected_width || framebuffer.height() != expected_height)) {
    throw std::runtime_error("PNG framebuffer capture size does not match the active frame.");
  }
  writeRgbaPng(path, framebuffer.width(), framebuffer.height(), framebuffer.rgba8());
}

void writeVisualDiffPng(const VisualDiffResult &result, const std::filesystem::path &path) {
  writeRgbaPng(path, result.width, result.height, result.delta_rgba);
}

VisualDiffArtifactPaths writeVisualDiffArtifactSet(const std::filesystem::path &directory,
                                                   const std::string_view stem,
                                                   const RgbaImageView approved,
                                                   const RgbaImageView incoming,
                                                   const VisualDiffResult &result) {
  const std::string base = stem.empty() ? "visual_diff" : std::string(stem);
  VisualDiffArtifactPaths paths;
  paths.approved = directory / (base + "_approved.png");
  paths.incoming = directory / (base + "_incoming.png");
  paths.delta = directory / (base + "_delta.png");
  paths.contact_sheet = directory / (base + "_contact_sheet.png");

  writeRgbaPng(paths.approved, approved.width, approved.height, approved.rgba);
  writeRgbaPng(paths.incoming, incoming.width, incoming.height, incoming.rgba);
  writeVisualDiffPng(result, paths.delta);
  const std::vector<std::uint8_t> sheet = makeVisualDiffContactSheet(approved, incoming, result);
  const int sheet_width = result.width * 3 + 8;
  const int sheet_height = result.height + 8;
  writeRgbaPng(paths.contact_sheet, sheet_width, sheet_height, sheet);
  return paths;
}

} // namespace aster
