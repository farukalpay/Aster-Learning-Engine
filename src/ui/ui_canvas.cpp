// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/ui_canvas.hpp"

#include "aster/render/software_framebuffer.hpp"

#include <DESKTOP_WINDOW/desktop_window3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

const std::array<std::uint8_t, 7> &glyphRows(const char value) {
  static constexpr std::array<std::uint8_t, 7> blank{0, 0, 0, 0, 0, 0, 0};
  static constexpr std::array<std::array<std::uint8_t, 7>, 36> alpha_numeric{{
      {14, 17, 17, 31, 17, 17, 17}, // A
      {30, 17, 17, 30, 17, 17, 30}, // B
      {14, 17, 16, 16, 16, 17, 14}, // C
      {30, 17, 17, 17, 17, 17, 30}, // D
      {31, 16, 16, 30, 16, 16, 31}, // E
      {31, 16, 16, 30, 16, 16, 16}, // F
      {14, 17, 16, 23, 17, 17, 15}, // G
      {17, 17, 17, 31, 17, 17, 17}, // H
      {14, 4, 4, 4, 4, 4, 14},      // I
      {7, 2, 2, 2, 2, 18, 12},      // J
      {17, 18, 20, 24, 20, 18, 17}, // K
      {16, 16, 16, 16, 16, 16, 31}, // L
      {17, 27, 21, 21, 17, 17, 17}, // M
      {17, 25, 21, 19, 17, 17, 17}, // N
      {14, 17, 17, 17, 17, 17, 14}, // O
      {30, 17, 17, 30, 16, 16, 16}, // P
      {14, 17, 17, 17, 21, 18, 13}, // Q
      {30, 17, 17, 30, 20, 18, 17}, // R
      {15, 16, 16, 14, 1, 1, 30},   // S
      {31, 4, 4, 4, 4, 4, 4},       // T
      {17, 17, 17, 17, 17, 17, 14}, // U
      {17, 17, 17, 17, 10, 10, 4},  // V
      {17, 17, 17, 21, 21, 27, 17}, // W
      {17, 10, 4, 4, 4, 10, 17},    // X
      {17, 10, 4, 4, 4, 4, 4},      // Y
      {31, 1, 2, 4, 8, 16, 31},     // Z
      {14, 17, 19, 21, 25, 17, 14}, // 0
      {4, 12, 4, 4, 4, 4, 14},      // 1
      {14, 17, 1, 2, 4, 8, 31},     // 2
      {30, 1, 1, 14, 1, 1, 30},     // 3
      {2, 6, 10, 18, 31, 2, 2},     // 4
      {31, 16, 30, 1, 1, 17, 14},   // 5
      {6, 8, 16, 30, 17, 17, 14},   // 6
      {31, 1, 2, 4, 8, 8, 8},       // 7
      {14, 17, 17, 14, 17, 17, 14}, // 8
      {14, 17, 17, 15, 1, 2, 12},   // 9
  }};
  static constexpr std::array<std::uint8_t, 7> space{0, 0, 0, 0, 0, 0, 0};
  static constexpr std::array<std::uint8_t, 7> dot{0, 0, 0, 0, 0, 12, 12};
  static constexpr std::array<std::uint8_t, 7> comma{0, 0, 0, 0, 0, 12, 8};
  static constexpr std::array<std::uint8_t, 7> colon{0, 12, 12, 0, 12, 12, 0};
  static constexpr std::array<std::uint8_t, 7> slash{1, 2, 2, 4, 8, 8, 16};
  static constexpr std::array<std::uint8_t, 7> dash{0, 0, 0, 14, 0, 0, 0};
  static constexpr std::array<std::uint8_t, 7> plus{0, 4, 4, 31, 4, 4, 0};
  static constexpr std::array<std::uint8_t, 7> bang{4, 4, 4, 4, 4, 0, 4};
  static constexpr std::array<std::uint8_t, 7> question{14, 17, 1, 2, 4, 0, 4};
  static constexpr std::array<std::uint8_t, 7> percent{17, 2, 4, 4, 8, 17, 0};
  static constexpr std::array<std::uint8_t, 7> left_paren{2, 4, 8, 8, 8, 4, 2};
  static constexpr std::array<std::uint8_t, 7> right_paren{8, 4, 2, 2, 2, 4, 8};

  if (value >= 'A' && value <= 'Z') {
    return alpha_numeric[static_cast<std::size_t>(value - 'A')];
  }
  if (value >= 'a' && value <= 'z') {
    return alpha_numeric[static_cast<std::size_t>(value - 'a')];
  }
  if (value >= '0' && value <= '9') {
    return alpha_numeric[26u + static_cast<std::size_t>(value - '0')];
  }
  switch (value) {
  case ' ':
    return space;
  case '.':
    return dot;
  case ',':
    return comma;
  case ':':
    return colon;
  case '/':
  case '\\':
    return slash;
  case '-':
  case '_':
    return dash;
  case '+':
    return plus;
  case '!':
    return bang;
  case '?':
    return question;
  case '%':
    return percent;
  case '(':
  case '[':
    return left_paren;
  case ')':
  case ']':
    return right_paren;
  default:
    return blank;
  }
}

float clamp01(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float glyphAdvance(const float scale) {
  return 6.0f * scale;
}

} // namespace

namespace aster {

UiCanvas::~UiCanvas() {
  shutdown();
}

void UiCanvas::initialize(DESKTOP_WINDOWwindow *window) {
  window_ = window;
}

void UiCanvas::beginFrame() {
  if (window_ == nullptr) {
    return;
  }

  desktop_windowGetWindowSize(window_, &viewport_width_, &viewport_height_);
  viewport_width_ = std::max(viewport_width_, 1);
  viewport_height_ = std::max(viewport_height_, 1);

  double pointer_x = 0.0;
  double pointer_y = 0.0;
  desktop_windowGetCursorPos(window_, &pointer_x, &pointer_y);
  pointer_ = {static_cast<float>(pointer_x), static_cast<float>(pointer_y)};

  const bool down = desktop_windowGetMouseButton(window_, DESKTOP_WINDOW_MOUSE_BUTTON_LEFT) ==
                    DESKTOP_WINDOW_PRESS;
  mouse_pressed_ = down && !mouse_down_;
  mouse_released_ = !down && mouse_down_;
  mouse_down_ = down;

  hovered_last_frame_ = hovered_this_frame_;
  hovered_this_frame_ = false;
  wants_mouse_ = active_control_ != 0u || hovered_last_frame_;
  rects_.clear();
  vertices_.clear();
}

void UiCanvas::endFrame() {
  if (rects_.empty() && vertices_.empty()) {
    return;
  }

  SoftwareFrameBuffer &framebuffer = activeFrameBuffer();
  if (framebuffer.empty()) {
    return;
  }

  const float scale_x =
      static_cast<float>(framebuffer.width()) / static_cast<float>(std::max(viewport_width_, 1));
  const float scale_y =
      static_cast<float>(framebuffer.height()) / static_cast<float>(std::max(viewport_height_, 1));
  const auto convertColor = [](const UiColor color) {
    return FrameColor{static_cast<std::uint8_t>(clamp01(color.r) * 255.0f + 0.5f),
                      static_cast<std::uint8_t>(clamp01(color.g) * 255.0f + 0.5f),
                      static_cast<std::uint8_t>(clamp01(color.b) * 255.0f + 0.5f),
                      static_cast<std::uint8_t>(clamp01(color.a) * 255.0f + 0.5f)};
  };
  for (const RectCommand &command : rects_) {
    framebuffer.fillUiRect(command.rect.x * scale_x, command.rect.y * scale_y,
                           command.rect.width * scale_x, command.rect.height * scale_y,
                           convertColor(command.color));
  }
  for (std::size_t i = 0; i + 2u < vertices_.size(); i += 3u) {
    const auto convert = [&](const Vertex vertex) {
      return FrameVertex{
          .x = vertex.x * scale_x,
          .y = vertex.y * scale_y,
          .depth = 0.0f,
          .color = convertColor(vertex.color),
      };
    };
    framebuffer.drawUiTriangle(convert(vertices_[i + 0u]), convert(vertices_[i + 1u]),
                               convert(vertices_[i + 2u]));
  }
}

void UiCanvas::shutdown() {
  window_ = nullptr;
}

Vec2 UiCanvas::viewportSize() const {
  return {static_cast<float>(viewport_width_), static_cast<float>(viewport_height_)};
}

Vec2 UiCanvas::pointerPosition() const {
  return pointer_;
}

bool UiCanvas::wantsMouse() const {
  return wants_mouse_ || hovered_this_frame_ || active_control_ != 0u;
}

bool UiCanvas::wantsKeyboard() const {
  return false;
}

float UiCanvas::textWidth(const std::string_view text, const float scale) const {
  return static_cast<float>(text.size()) * glyphAdvance(scale);
}

float UiCanvas::textHeight(const float scale) const {
  return 7.0f * scale;
}

float UiCanvas::wrappedTextHeight(const std::string_view text_value, const float width,
                                  const float scale) const {
  if (text_value.empty()) {
    return 0.0f;
  }
  const float line_height = textHeight(scale) + 3.0f * scale;
  float x = 0.0f;
  float y = line_height;
  std::size_t word_start = 0u;
  while (word_start < text_value.size()) {
    while (word_start < text_value.size() && text_value[word_start] == ' ') {
      word_start++;
    }
    std::size_t word_end = word_start;
    while (word_end < text_value.size() && text_value[word_end] != ' ') {
      word_end++;
    }
    const float word_width =
        static_cast<float>(word_end - word_start) * glyphAdvance(scale) + glyphAdvance(scale);
    if (x > 0.0f && x + word_width > width) {
      x = 0.0f;
      y += line_height;
    }
    x += word_width;
    word_start = word_end + 1u;
  }
  return y;
}

void UiCanvas::fillRect(const UiRect rect, const UiColor color) {
  if (rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
    return;
  }
  rects_.push_back({rect, color});
}

void UiCanvas::fillRoundRect(const UiRect rect, const float radius, const UiColor color) {
  const float r = std::max(0.0f, std::min(radius, std::min(rect.width, rect.height) * 0.5f));
  if (r <= 0.5f) {
    fillRect(rect, color);
    return;
  }

  fillRect({rect.x + r, rect.y, rect.width - 2.0f * r, rect.height}, color);
  fillRect({rect.x, rect.y + r, r, rect.height - 2.0f * r}, color);
  fillRect({rect.x + rect.width - r, rect.y + r, r, rect.height - 2.0f * r}, color);

  const auto corner = [&](const Vec2 center, const float start, const float end) {
    constexpr int kSegments = 8;
    for (int i = 0; i < kSegments; ++i) {
      const float a = start + (end - start) * static_cast<float>(i) / static_cast<float>(kSegments);
      const float b =
          start + (end - start) * static_cast<float>(i + 1) / static_cast<float>(kSegments);
      appendTriangle({center.x, center.y, color},
                     {center.x + std::cos(a) * r, center.y + std::sin(a) * r, color},
                     {center.x + std::cos(b) * r, center.y + std::sin(b) * r, color});
    }
  };

  corner({rect.x + r, rect.y + r}, 3.14159265f, 4.71238898f);
  corner({rect.x + rect.width - r, rect.y + r}, 4.71238898f, 6.28318531f);
  corner({rect.x + rect.width - r, rect.y + rect.height - r}, 0.0f, 1.57079633f);
  corner({rect.x + r, rect.y + rect.height - r}, 1.57079633f, 3.14159265f);
}

void UiCanvas::strokeRect(const UiRect rect, const UiColor color, const float thickness) {
  const float t = std::max(thickness, 1.0f);
  fillRect({rect.x, rect.y, rect.width, t}, color);
  fillRect({rect.x, rect.y + rect.height - t, rect.width, t}, color);
  fillRect({rect.x, rect.y, t, rect.height}, color);
  fillRect({rect.x + rect.width - t, rect.y, t, rect.height}, color);
}

void UiCanvas::line(const Vec2 from, const Vec2 to, const UiColor color, const float thickness) {
  Vec2 direction = to - from;
  const float len = length(direction);
  if (len <= 0.0001f) {
    return;
  }
  direction = direction / len;
  const Vec2 normal{-direction.y * thickness * 0.5f, direction.x * thickness * 0.5f};
  appendTriangle({from.x + normal.x, from.y + normal.y, color},
                 {to.x + normal.x, to.y + normal.y, color},
                 {to.x - normal.x, to.y - normal.y, color});
  appendTriangle({from.x + normal.x, from.y + normal.y, color},
                 {to.x - normal.x, to.y - normal.y, color},
                 {from.x - normal.x, from.y - normal.y, color});
}

void UiCanvas::fillCircle(const Vec2 center, const float radius, const UiColor color,
                          const int segments) {
  const int safe_segments = std::max(segments, 8);
  for (int i = 0; i < safe_segments; ++i) {
    const float a = static_cast<float>(i) * 6.28318531f / static_cast<float>(safe_segments);
    const float b = static_cast<float>(i + 1) * 6.28318531f / static_cast<float>(safe_segments);
    appendTriangle({center.x, center.y, color},
                   {center.x + std::cos(a) * radius, center.y + std::sin(a) * radius, color},
                   {center.x + std::cos(b) * radius, center.y + std::sin(b) * radius, color});
  }
}

void UiCanvas::strokeCircle(const Vec2 center, const float radius, const UiColor color,
                            const float thickness, const int segments) {
  const int safe_segments = std::max(segments, 8);
  const float inner = std::max(0.0f, radius - thickness);
  for (int i = 0; i < safe_segments; ++i) {
    const float a = static_cast<float>(i) * 6.28318531f / static_cast<float>(safe_segments);
    const float b = static_cast<float>(i + 1) * 6.28318531f / static_cast<float>(safe_segments);
    const Vec2 outer_a{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
    const Vec2 outer_b{center.x + std::cos(b) * radius, center.y + std::sin(b) * radius};
    const Vec2 inner_a{center.x + std::cos(a) * inner, center.y + std::sin(a) * inner};
    const Vec2 inner_b{center.x + std::cos(b) * inner, center.y + std::sin(b) * inner};
    appendTriangle({outer_a.x, outer_a.y, color}, {outer_b.x, outer_b.y, color},
                   {inner_b.x, inner_b.y, color});
    appendTriangle({outer_a.x, outer_a.y, color}, {inner_b.x, inner_b.y, color},
                   {inner_a.x, inner_a.y, color});
  }
}

void UiCanvas::text(const std::string_view text_value, Vec2 position, const UiColor color,
                    const float scale) {
  for (const char glyph : text_value) {
    drawGlyph(glyph, position, color, scale);
    position.x += glyphAdvance(scale);
  }
}

void UiCanvas::wrappedText(const std::string_view text_value, Vec2 position, const float width,
                           const UiColor color, const float scale) {
  const float line_height = textHeight(scale) + 3.0f * scale;
  float x = 0.0f;
  std::size_t word_start = 0u;
  while (word_start < text_value.size()) {
    while (word_start < text_value.size() && text_value[word_start] == ' ') {
      word_start++;
    }
    std::size_t word_end = word_start;
    while (word_end < text_value.size() && text_value[word_end] != ' ') {
      word_end++;
    }
    const std::string_view word = text_value.substr(word_start, word_end - word_start);
    const float word_width = textWidth(word, scale);
    if (x > 0.0f && x + word_width > width) {
      x = 0.0f;
      position.y += line_height;
    }
    text(word, {position.x + x, position.y}, color, scale);
    x += word_width + glyphAdvance(scale);
    word_start = word_end + 1u;
  }
}

void UiCanvas::progressBar(const UiRect rect, const float value, const UiColor fill,
                           const UiColor track) {
  fillRoundRect(rect, 5.0f, track);
  fillRoundRect({rect.x, rect.y, rect.width * clamp01(value), rect.height}, 5.0f, fill);
}

bool UiCanvas::button(const UiRect rect, const std::string_view label, const std::string_view id) {
  const unsigned int control = controlId(label, id);
  const bool hot = contains(rect, pointer_);
  hovered_this_frame_ = hovered_this_frame_ || hot;
  if (hot && mouse_pressed_) {
    active_control_ = control;
  }
  const bool clicked = hot && mouse_released_ && active_control_ == control;
  if (mouse_released_ && active_control_ == control) {
    active_control_ = 0u;
  }

  const UiColor base = active_control_ == control ? UiColor{0.44f, 0.31f, 0.16f, 0.96f}
                       : hot                      ? UiColor{0.22f, 0.32f, 0.31f, 0.96f}
                                                  : UiColor{0.11f, 0.18f, 0.19f, 0.92f};
  fillRoundRect(rect, 6.0f, base);
  strokeRect(rect, {0.78f, 0.61f, 0.33f, hot ? 0.58f : 0.28f}, 1.0f);
  const float label_width = textWidth(label, 2.0f);
  text(label, {rect.x + (rect.width - label_width) * 0.5f, rect.y + 10.0f},
       {0.95f, 0.93f, 0.84f, 1.0f}, 2.0f);
  return clicked;
}

bool UiCanvas::slider(const UiRect rect, const std::string_view label, float &value,
                      const float min_value, const float max_value, const std::string_view id) {
  const unsigned int control = controlId(label, id);
  const bool hot = contains(rect, pointer_);
  hovered_this_frame_ = hovered_this_frame_ || hot;
  if (hot && mouse_pressed_) {
    active_control_ = control;
  }
  if (!mouse_down_ && active_control_ == control) {
    active_control_ = 0u;
  }
  if (active_control_ == control) {
    const float t = clamp01((pointer_.x - rect.x) / std::max(rect.width, 1.0f));
    value = min_value + (max_value - min_value) * t;
  }

  const float normalized =
      max_value == min_value ? 0.0f : (value - min_value) / (max_value - min_value);
  const float label_y = rect.y - 17.0f;
  text(label, {rect.x, label_y}, {0.72f, 0.78f, 0.76f, 1.0f}, 1.5f);
  fillRoundRect(rect, 4.0f, {0.06f, 0.09f, 0.10f, 0.94f});
  fillRoundRect({rect.x, rect.y, rect.width * clamp01(normalized), rect.height}, 4.0f,
                {0.72f, 0.47f, 0.22f, 0.92f});
  fillCircle({rect.x + rect.width * clamp01(normalized), rect.y + rect.height * 0.5f}, 7.0f,
             {0.95f, 0.69f, 0.32f, 1.0f}, 20);
  return active_control_ == control;
}

bool UiCanvas::checkbox(const UiRect rect, const std::string_view label, bool &value,
                        const std::string_view id) {
  const unsigned int control = controlId(label, id);
  const bool hot = contains(rect, pointer_);
  hovered_this_frame_ = hovered_this_frame_ || hot;
  if (hot && mouse_pressed_) {
    active_control_ = control;
  }
  const bool clicked = hot && mouse_released_ && active_control_ == control;
  if (clicked) {
    value = !value;
  }
  if (mouse_released_ && active_control_ == control) {
    active_control_ = 0u;
  }

  const UiRect box{rect.x, rect.y + 3.0f, 18.0f, 18.0f};
  fillRoundRect(box, 4.0f,
                value ? UiColor{0.73f, 0.49f, 0.22f, 0.96f} : UiColor{0.06f, 0.09f, 0.10f, 0.94f});
  strokeRect(box, {0.80f, 0.62f, 0.36f, hot ? 0.65f : 0.34f}, 1.0f);
  if (value) {
    line({box.x + 4.0f, box.y + 9.5f}, {box.x + 8.0f, box.y + 14.0f}, {0.07f, 0.08f, 0.07f, 1.0f},
         2.0f);
    line({box.x + 8.0f, box.y + 14.0f}, {box.x + 15.0f, box.y + 4.0f}, {0.07f, 0.08f, 0.07f, 1.0f},
         2.0f);
  }
  text(label, {rect.x + 28.0f, rect.y + 6.0f}, {0.88f, 0.91f, 0.88f, 1.0f}, 1.6f);
  return clicked;
}

bool UiCanvas::contains(const UiRect rect, const Vec2 point) const {
  return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
         point.y <= rect.y + rect.height;
}

unsigned int UiCanvas::controlId(const std::string_view label, const std::string_view id) const {
  const std::string_view key = id.empty() ? label : id;
  unsigned int hash = 2166136261u;
  for (const char c : key) {
    hash ^= static_cast<unsigned int>(static_cast<unsigned char>(c));
    hash *= 16777619u;
  }
  return hash == 0u ? 1u : hash;
}

void UiCanvas::appendTriangle(const Vertex a, const Vertex b, const Vertex c) {
  vertices_.push_back(a);
  vertices_.push_back(b);
  vertices_.push_back(c);
}

void UiCanvas::drawGlyph(const char glyph, const Vec2 position, const UiColor color,
                         const float scale) {
  const std::array<std::uint8_t, 7> &rows = glyphRows(glyph);
  for (std::size_t y = 0; y < rows.size(); ++y) {
    for (int x = 0; x < 5; ++x) {
      if ((rows[y] & (1u << static_cast<unsigned int>(4 - x))) != 0u) {
        fillRect({position.x + static_cast<float>(x) * scale,
                  position.y + static_cast<float>(y) * scale, scale, scale},
                 color);
      }
    }
  }
}

} // namespace aster
