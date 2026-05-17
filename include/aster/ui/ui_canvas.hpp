// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/input/control_scheme.hpp"
#include "aster/math/vec.hpp"

#include <string_view>
#include <vector>

namespace aster {

struct UiColor {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct UiRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

class UiCanvas {
public:
  UiCanvas() = default;
  ~UiCanvas();

  UiCanvas(const UiCanvas &) = delete;
  UiCanvas &operator=(const UiCanvas &) = delete;

  void initialize();
  void beginFrame(Vec2 viewport_size, const ControlSnapshot &input);
  void endFrame();
  void shutdown();

  [[nodiscard]] Vec2 viewportSize() const;
  [[nodiscard]] Vec2 pointerPosition() const;
  [[nodiscard]] bool mouseDown() const;
  [[nodiscard]] bool mousePressed() const;
  [[nodiscard]] bool mouseReleased() const;
  [[nodiscard]] bool wantsMouse() const;
  [[nodiscard]] bool wantsKeyboard() const;

  [[nodiscard]] float textWidth(std::string_view text, float scale = 2.0f) const;
  [[nodiscard]] float textHeight(float scale = 2.0f) const;
  [[nodiscard]] float fittedTextScale(std::string_view text, float max_width,
                                      float preferred_scale = 2.0f,
                                      float minimum_scale = 1.0f) const;
  [[nodiscard]] float wrappedTextHeight(std::string_view text, float width,
                                        float scale = 2.0f) const;
  [[nodiscard]] float checkboxHeight(std::string_view label, float width,
                                     float scale = 1.35f) const;
  [[nodiscard]] float sliderHeight(std::string_view label, float width,
                                   float label_scale = 1.5f) const;

  void fillRect(UiRect rect, UiColor color);
  void fillRoundRect(UiRect rect, float radius, UiColor color);
  void strokeRect(UiRect rect, UiColor color, float thickness = 1.0f);
  void pushClip(UiRect rect);
  void popClip();
  void line(Vec2 from, Vec2 to, UiColor color, float thickness = 1.0f);
  void fillCircle(Vec2 center, float radius, UiColor color, int segments = 28);
  void strokeCircle(Vec2 center, float radius, UiColor color, float thickness = 1.0f,
                    int segments = 28);
  void text(std::string_view text, Vec2 position, UiColor color, float scale = 2.0f);
  void wrappedText(std::string_view text, Vec2 position, float width, UiColor color,
                   float scale = 2.0f);
  void progressBar(UiRect rect, float value, UiColor fill, UiColor track);

  bool button(UiRect rect, std::string_view label, std::string_view id = {});
  bool slider(UiRect rect, std::string_view label, float &value, float min_value, float max_value,
              std::string_view id = {});
  bool checkbox(UiRect rect, std::string_view label, bool &value, std::string_view id = {});

private:
  struct Vertex {
    float x = 0.0f;
    float y = 0.0f;
    UiColor color{};
  };

  struct RectCommand {
    UiRect rect{};
    UiColor color{};
  };

  [[nodiscard]] bool contains(UiRect rect, Vec2 point) const;
  [[nodiscard]] UiRect currentClip() const;
  [[nodiscard]] bool rectIntersectsClip(UiRect rect) const;
  [[nodiscard]] bool triangleInsideClip(Vertex a, Vertex b, Vertex c) const;
  [[nodiscard]] unsigned int controlId(std::string_view label, std::string_view id) const;
  void appendTriangle(Vertex a, Vertex b, Vertex c);
  void drawGlyph(char glyph, Vec2 position, UiColor color, float scale);

  int viewport_width_ = 1;
  int viewport_height_ = 1;
  Vec2 pointer_{};
  bool mouse_down_ = false;
  bool mouse_pressed_ = false;
  bool mouse_released_ = false;
  bool hovered_this_frame_ = false;
  bool hovered_last_frame_ = false;
  bool wants_mouse_ = false;
  unsigned int active_control_ = 0;
  std::vector<RectCommand> rects_{};
  std::vector<Vertex> vertices_{};
  std::vector<UiRect> clip_stack_{};
};

} // namespace aster
