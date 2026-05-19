// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"
#include "aster/asset/asset_factory.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/ui/ui_canvas.hpp"

namespace aster {

class Scene;

struct EditorRuntimeModel {
  RenderBackendCapabilities backend{};
  const FixedRenderGraph *render_graph = nullptr;
  const AssetDatabase *asset_database = nullptr;
  const AssetLibrary *asset_library = nullptr;
  const FrameForensics *frame_forensics = nullptr;
};

class EditorUi {
public:
  EditorUi() = default;
  ~EditorUi();

  EditorUi(const EditorUi &) = delete;
  EditorUi &operator=(const EditorUi &) = delete;

  void initialize();
  void beginFrame(Vec2 viewport_size, const ControlSnapshot &input);
  void draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings, const FrameStats &stats,
            const EditorRuntimeModel &runtime = {});
  void endFrame();
  void shutdown();

  [[nodiscard]] bool wantsMouse() const;
  [[nodiscard]] bool wantsKeyboard() const;

private:
  UiCanvas canvas_;
  ControlSnapshot input_{};
  float renderer_panel_scroll_ = 0.0f;
  std::size_t selected_graph_pass_ = 0u;
  std::size_t selected_asset_ = 0u;
  std::size_t selected_object_fate_ = 0u;
  bool initialized_ = false;
};

} // namespace aster
