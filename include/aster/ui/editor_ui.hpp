// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/ui/ui_canvas.hpp"

namespace aster {

class Scene;

class EditorUi {
public:
  EditorUi() = default;
  ~EditorUi();

  EditorUi(const EditorUi &) = delete;
  EditorUi &operator=(const EditorUi &) = delete;

  void initialize();
  void beginFrame(Vec2 viewport_size, const ControlSnapshot &input);
  void draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings, const FrameStats &stats);
  void endFrame();
  void shutdown();

  [[nodiscard]] bool wantsMouse() const;
  [[nodiscard]] bool wantsKeyboard() const;

private:
  UiCanvas canvas_;
  ControlSnapshot input_{};
  float renderer_panel_scroll_ = 0.0f;
  bool initialized_ = false;
};

} // namespace aster
