// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace aster {

struct FrameColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 255;
};

struct FrameVertex {
  float x = 0.0f;
  float y = 0.0f;
  float depth = 1.0f;
  FrameColor color{};
};

class SoftwareFrameBuffer {
public:
  void resize(int width, int height);
  void clear(Vec3 color);
  void clearTransparent();
  void clearTransparentColor();
  void clearDepth(float depth = 0.0f);
  void replaceRgba8(int width, int height, std::span<const std::uint8_t> pixels);

  void drawTriangle(FrameVertex a, FrameVertex b, FrameVertex c, bool depth_test, bool depth_write,
                    bool alpha_blend, float depth_bias_constant = 0.0f,
                    float depth_bias_slope = 0.0f);
  void drawUiTriangle(FrameVertex a, FrameVertex b, FrameVertex c);
  void fillUiRect(float x, float y, float width, float height, FrameColor color);

  void writePpm(const std::filesystem::path &path, int expected_width, int expected_height) const;

  [[nodiscard]] int width() const noexcept {
    return width_;
  }

  [[nodiscard]] int height() const noexcept {
    return height_;
  }

  [[nodiscard]] bool empty() const noexcept {
    return width_ <= 0 || height_ <= 0 || rgba_.empty();
  }

  [[nodiscard]] std::span<const std::uint8_t> rgba8() const noexcept {
    return rgba_;
  }

private:
  void putPixel(int x, int y, float depth, FrameColor color, bool depth_test, bool depth_write,
                bool alpha_blend);

  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> rgba_{};
  std::vector<float> depth_{};
};

[[nodiscard]] SoftwareFrameBuffer &activeFrameBuffer();

[[nodiscard]] FrameColor frameColor(Vec3 color, float alpha = 1.0f);

} // namespace aster
