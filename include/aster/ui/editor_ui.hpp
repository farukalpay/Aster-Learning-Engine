// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/ui/ui_canvas.hpp"

namespace aster {

class Scene;
class Window;

class EditorUi {
public:
  EditorUi() = default;
  ~EditorUi();

  EditorUi(const EditorUi &) = delete;
  EditorUi &operator=(const EditorUi &) = delete;

  void initialize(Window &window);
  void beginFrame();
  void draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings, const FrameStats &stats);
  void endFrame();
  void shutdown();

  [[nodiscard]] bool wantsMouse() const;
  [[nodiscard]] bool wantsKeyboard() const;

private:
  UiCanvas canvas_;
  bool initialized_ = false;
};

} // namespace aster
