// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/software_framebuffer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {

float edgeFunction(const aster::FrameVertex &a, const aster::FrameVertex &b, const float x,
                   const float y) {
  return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

std::uint8_t toByte(const float value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

aster::FrameColor mixColor(const aster::FrameVertex &a, const aster::FrameVertex &b,
                           const aster::FrameVertex &c, const float wa, const float wb,
                           const float wc) {
  return {
      toByte((static_cast<float>(a.color.r) * wa + static_cast<float>(b.color.r) * wb +
              static_cast<float>(c.color.r) * wc) /
             255.0f),
      toByte((static_cast<float>(a.color.g) * wa + static_cast<float>(b.color.g) * wb +
              static_cast<float>(c.color.g) * wc) /
             255.0f),
      toByte((static_cast<float>(a.color.b) * wa + static_cast<float>(b.color.b) * wb +
              static_cast<float>(c.color.b) * wc) /
             255.0f),
      toByte((static_cast<float>(a.color.a) * wa + static_cast<float>(b.color.a) * wb +
              static_cast<float>(c.color.a) * wc) /
             255.0f),
  };
}

void blendPremultiplied(std::vector<std::uint8_t> &rgba, const std::size_t base,
                        const aster::FrameColor color) {
  if (color.a == 255u) {
    rgba[base + 0u] = color.r;
    rgba[base + 1u] = color.g;
    rgba[base + 2u] = color.b;
    rgba[base + 3u] = color.a;
    return;
  }

  const std::uint32_t source_alpha = color.a;
  const std::uint32_t dest_keep = 255u - source_alpha;
  rgba[base + 0u] =
      static_cast<std::uint8_t>((static_cast<std::uint32_t>(color.r) * source_alpha +
                                 static_cast<std::uint32_t>(rgba[base + 0u]) * dest_keep + 127u) /
                                255u);
  rgba[base + 1u] =
      static_cast<std::uint8_t>((static_cast<std::uint32_t>(color.g) * source_alpha +
                                 static_cast<std::uint32_t>(rgba[base + 1u]) * dest_keep + 127u) /
                                255u);
  rgba[base + 2u] =
      static_cast<std::uint8_t>((static_cast<std::uint32_t>(color.b) * source_alpha +
                                 static_cast<std::uint32_t>(rgba[base + 2u]) * dest_keep + 127u) /
                                255u);
  rgba[base + 3u] = static_cast<std::uint8_t>(
      source_alpha + (static_cast<std::uint32_t>(rgba[base + 3u]) * dest_keep + 127u) / 255u);
}

} // namespace

namespace aster {

void SoftwareFrameBuffer::resize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    width_ = 0;
    height_ = 0;
    rgba_.clear();
    depth_.clear();
    return;
  }

  if (width == width_ && height == height_) {
    return;
  }

  width_ = width;
  height_ = height;
  rgba_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4u, 0u);
  depth_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0.0f);
}

void SoftwareFrameBuffer::clear(const Vec3 color) {
  const FrameColor clear_color = frameColor(color);
  for (std::size_t i = 0; i + 3u < rgba_.size(); i += 4u) {
    rgba_[i + 0u] = clear_color.r;
    rgba_[i + 1u] = clear_color.g;
    rgba_[i + 2u] = clear_color.b;
    rgba_[i + 3u] = clear_color.a;
  }
  clearDepth();
}

void SoftwareFrameBuffer::clearTransparent() {
  clearTransparentColor();
  clearDepth();
}

void SoftwareFrameBuffer::clearTransparentColor() {
  if (!rgba_.empty()) {
    std::memset(rgba_.data(), 0, rgba_.size());
  }
}

void SoftwareFrameBuffer::clearDepth(const float depth) {
  std::fill(depth_.begin(), depth_.end(), depth);
}

void SoftwareFrameBuffer::replaceRgba8(const int width, const int height,
                                       const std::span<const std::uint8_t> pixels) {
  if (width <= 0 || height <= 0) {
    resize(0, 0);
    return;
  }

  const std::size_t expected_size =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
  if (pixels.size() != expected_size) {
    throw std::runtime_error("Framebuffer upload size does not match the target dimensions.");
  }

  width_ = width;
  height_ = height;
  rgba_.assign(pixels.begin(), pixels.end());
  depth_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0.0f);
}

void SoftwareFrameBuffer::drawTriangle(const FrameVertex a, const FrameVertex b,
                                       const FrameVertex c, const bool depth_test,
                                       const bool depth_write, const bool alpha_blend,
                                       const float depth_bias_constant,
                                       const float depth_bias_slope) {
  if (empty()) {
    return;
  }

  const float area = edgeFunction(a, b, c.x, c.y);
  if (std::abs(area) <= 0.00001f) {
    return;
  }

  const int min_x = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
  const int max_x = std::min(width_ - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
  const int min_y = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
  const int max_y = std::min(height_ - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));

  const float inv_area = 1.0f / area;
  const float dzdx =
      ((b.depth - a.depth) * (c.y - a.y) - (c.depth - a.depth) * (b.y - a.y)) * inv_area;
  const float dzdy =
      ((c.depth - a.depth) * (b.x - a.x) - (b.depth - a.depth) * (c.x - a.x)) * inv_area;
  const float slope_bias =
      std::max(std::abs(dzdx), std::abs(dzdy)) * std::max(depth_bias_slope, 0.0f);
  const float depth_bias = depth_bias_constant + slope_bias;
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;
      const float wa = edgeFunction(b, c, px, py) * inv_area;
      const float wb = edgeFunction(c, a, px, py) * inv_area;
      const float wc = edgeFunction(a, b, px, py) * inv_area;
      if (wa < 0.0f || wb < 0.0f || wc < 0.0f) {
        continue;
      }
      const float depth = a.depth * wa + b.depth * wb + c.depth * wc + depth_bias;
      putPixel(x, y, depth, mixColor(a, b, c, wa, wb, wc), depth_test, depth_write, alpha_blend);
    }
  }
}

void SoftwareFrameBuffer::drawUiTriangle(const FrameVertex a, const FrameVertex b,
                                         const FrameVertex c) {
  drawTriangle(a, b, c, false, false, true);
}

void SoftwareFrameBuffer::fillUiRect(const float x, const float y, const float width,
                                     const float height, const FrameColor color) {
  if (empty() || width <= 0.0f || height <= 0.0f || color.a == 0u) {
    return;
  }

  const int min_x = std::max(0, static_cast<int>(std::floor(x)));
  const int min_y = std::max(0, static_cast<int>(std::floor(y)));
  const int max_x = std::min(width_, static_cast<int>(std::ceil(x + width)));
  const int max_y = std::min(height_, static_cast<int>(std::ceil(y + height)));
  if (min_x >= max_x || min_y >= max_y) {
    return;
  }

  for (int py = min_y; py < max_y; ++py) {
    std::size_t base = (static_cast<std::size_t>(py) * static_cast<std::size_t>(width_) +
                        static_cast<std::size_t>(min_x)) *
                       4u;
    for (int px = min_x; px < max_x; ++px) {
      blendPremultiplied(rgba_, base, color);
      base += 4u;
    }
  }
}

void SoftwareFrameBuffer::writePpm(const std::filesystem::path &path, const int expected_width,
                                   const int expected_height) const {
  if (empty()) {
    throw std::runtime_error("Framebuffer capture requested before a frame was rendered.");
  }
  if (expected_width > 0 && expected_height > 0 &&
      (expected_width != width_ || expected_height != height_)) {
    throw std::runtime_error("Framebuffer capture size does not match the active frame.");
  }

  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open framebuffer capture path.");
  }

  file << "P6\n" << width_ << ' ' << height_ << "\n255\n";
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                             static_cast<std::size_t>(x)) *
                            4u;
      const char rgb[3] = {static_cast<char>(rgba_[i + 0u]), static_cast<char>(rgba_[i + 1u]),
                           static_cast<char>(rgba_[i + 2u])};
      file.write(rgb, sizeof(rgb));
    }
  }
}

void SoftwareFrameBuffer::putPixel(const int x, const int y, const float depth,
                                   const FrameColor color, const bool depth_test,
                                   const bool depth_write, const bool alpha_blend) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }

  const std::size_t pixel =
      static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
  if (depth_test && (depth < 0.0f || depth > 1.0f || depth <= depth_[pixel])) {
    return;
  }

  const std::size_t base = pixel * 4u;
  if (alpha_blend) {
    blendPremultiplied(rgba_, base, color);
  } else {
    rgba_[base + 0u] = color.r;
    rgba_[base + 1u] = color.g;
    rgba_[base + 2u] = color.b;
    rgba_[base + 3u] = color.a;
  }

  if (depth_write) {
    depth_[pixel] = depth;
  }
}

SoftwareFrameBuffer &activeFrameBuffer() {
  static SoftwareFrameBuffer framebuffer;
  return framebuffer;
}

FrameColor frameColor(const Vec3 color, const float alpha) {
  return {toByte(color.x), toByte(color.y), toByte(color.z), toByte(alpha)};
}

} // namespace aster
