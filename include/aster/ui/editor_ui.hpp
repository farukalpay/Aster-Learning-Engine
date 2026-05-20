// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"
#include "aster/asset/asset_library.hpp"
#include "aster/asset/asset_production_model.hpp"
#include "aster/material/material_lab.hpp"
#include "aster/physics/xpbd_authoring.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <string>
#include <vector>
#include <filesystem>

namespace aster {

class Scene;

struct EditorRuntimeModel {
  RenderBackendCapabilities backend{};
  const FixedRenderGraph *render_graph = nullptr;
  const AssetDatabase *asset_database = nullptr;
  const AssetLibrary *asset_library = nullptr;
  const AssetProductionModel *asset_production_model = nullptr;
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
  std::size_t selected_asset_tab_ = 0u;
  std::size_t selected_texture_ = 0u;
  std::size_t selected_material_lab_node_ = 0u;
  std::size_t selected_material_lab_mesh_ = 0u;
  std::size_t selected_material_lab_environment_ = 0u;
  std::size_t selected_xpbd_preset_ = 0u;
  std::size_t selected_object_fate_ = 0u;
  std::size_t selected_timeline_event_ = 0u;
  std::size_t selected_resource_provenance_ = 0u;
  std::size_t selected_regression_entry_ = 0u;
  MaterialAsset material_lab_asset_{};
  MaterialAuthoringGraph material_lab_graph_{};
  std::string material_lab_loaded_asset_id_;
  std::filesystem::path material_lab_save_path_;
  std::string material_lab_cache_key_;
  std::vector<MaterialLabPreviewImage> material_lab_previews_;
  std::vector<std::string> material_lab_diagnostics_;
  XpbdMeshAuthoringSettings xpbd_settings_{};
  XpbdMeshAuthoringSession xpbd_session_{};
  std::string xpbd_loaded_asset_id_;
  std::filesystem::path xpbd_source_path_;
  std::vector<std::string> xpbd_diagnostics_;
  bool xpbd_preview_ready_ = false;
  bool material_lab_dirty_ = false;
  bool material_lab_save_supported_ = false;
  bool initialized_ = false;
};

} // namespace aster
