// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/editor_ui.hpp"

#include "aster/platform/window.hpp"
#include "aster/scene/scene.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

constexpr aster::UiColor kPanel{0.026f, 0.032f, 0.038f, 0.88f};
constexpr aster::UiColor kPanelEdge{0.72f, 0.52f, 0.26f, 0.34f};
constexpr aster::UiColor kText{0.91f, 0.93f, 0.89f, 1.0f};
constexpr aster::UiColor kDim{0.55f, 0.63f, 0.63f, 1.0f};
constexpr aster::UiColor kAmber{0.88f, 0.58f, 0.23f, 1.0f};
constexpr float kPanelMargin = 16.0f;
constexpr float kPanelPad = 18.0f;

bool pointInRect(const aster::Vec2 point, const aster::UiRect rect) {
  return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
         point.y <= rect.y + rect.height;
}

void drawPanelTexture(aster::UiCanvas &canvas, const aster::UiRect rect) {
  canvas.fillRoundRect(rect, 8.0f, kPanel);
  canvas.strokeRect(rect, kPanelEdge, 1.0f);
  for (int i = 0; i < 7; ++i) {
    const float y = rect.y + 18.0f + static_cast<float>(i) * 44.0f;
    canvas.line({rect.x + 14.0f, y}, {rect.x + rect.width - 16.0f, y + 14.0f},
                {0.88f, 0.65f, 0.34f, 0.035f}, 1.0f);
  }
}

void section(aster::UiCanvas &canvas, const std::string &title, const float x, float &y,
             const float width) {
  canvas.text(title, {x, y}, kAmber, 1.8f);
  y += 18.0f;
  canvas.line({x, y}, {x + width, y}, {0.82f, 0.60f, 0.32f, 0.26f}, 1.0f);
  y += 18.0f;
}

void sliderRow(aster::UiCanvas &canvas, const std::string &label, float &value,
               const float min_value, const float max_value, const float x, float &y,
               const float width, const std::string &id, const float visible_top,
               const float visible_bottom) {
  const float row_height = canvas.sliderHeight(label, width);
  if (y >= visible_top && y + row_height <= visible_bottom) {
    canvas.slider({x, y + 17.0f, width, 8.0f}, label, value, min_value, max_value, id);
  }
  y += row_height + 3.0f;
}

void checkboxRow(aster::UiCanvas &canvas, const std::string &label, bool &value, const float x,
                 float &y, const float width, const std::string &id, const float visible_top,
                 const float visible_bottom) {
  const float row_height = canvas.checkboxHeight(label, width);
  if (y >= visible_top && y + row_height <= visible_bottom) {
    canvas.checkbox({x, y, width, row_height}, label, value, id);
  }
  y += row_height + 3.0f;
}

void renderStyleRow(aster::UiCanvas &canvas, aster::RendererSettings &settings, const float x,
                    float &y, const float width, const float visible_top,
                    const float visible_bottom) {
  constexpr float kRowHeight = 34.0f;
  const float button_width = std::max((width - 8.0f) * 0.5f, 84.0f);
  if (y >= visible_top && y + kRowHeight <= visible_bottom) {
    const bool neutral = settings.style.preset == aster::RenderStylePreset::Neutral;
    const bool retro = settings.style.preset == aster::RenderStylePreset::RetroHorrorReadable;
    if (canvas.button({x, y, button_width, 30.0f}, neutral ? "Neutral*" : "Neutral",
                      "style.neutral") &&
        !neutral) {
      aster::applyRenderStyleProfile(
          settings, aster::makeRenderStyleProfile(aster::RenderStylePreset::Neutral));
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f},
                      retro ? "Retro Horror*" : "Retro Horror", "style.retro") &&
        !retro) {
      aster::applyRenderStyleProfile(
          settings,
          aster::makeRenderStyleProfile(aster::RenderStylePreset::RetroHorrorReadable));
    }
  }
  y += kRowHeight + 3.0f;
}

void textRow(aster::UiCanvas &canvas, const std::string_view label, const std::string_view value,
             const float x, float &y, const float width, const float visible_top,
             const float visible_bottom) {
  constexpr float kRowHeight = 20.0f;
  if (y >= visible_top && y + kRowHeight <= visible_bottom) {
    canvas.text(label, {x, y}, kDim, 1.22f);
    const float value_scale =
        canvas.fittedTextScale(value, std::max(width * 0.52f, 0.0f), 1.18f, 0.82f);
    const float value_width = canvas.textWidth(value, value_scale);
    canvas.text(value, {x + width - value_width, y}, kText, value_scale);
  }
  y += kRowHeight;
}

std::string yesNo(const bool value) {
  return value ? "yes" : "no";
}

void drawBackendSummary(aster::UiCanvas &canvas, const aster::EditorRuntimeModel &runtime,
                        const float x, float &y, const float width, const float visible_top,
                        const float visible_bottom) {
  section(canvas, "Backend", x, y, width);
  textRow(canvas, "Name", runtime.backend.name, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Kind", aster::renderBackendKindName(runtime.backend.kind), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "GPU", yesNo(runtime.backend.gpu), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Shader materials", yesNo(runtime.backend.supports_shader_materials), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Instancing", yesNo(runtime.backend.supports_instancing), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Capture", yesNo(runtime.backend.supports_capture), x, y, width, visible_top,
          visible_bottom);
}

void drawGraphSummary(aster::UiCanvas &canvas, std::size_t &selected_pass,
                      const aster::FixedRenderGraph *graph, const float x, float &y,
                      const float width, const float visible_top, const float visible_bottom) {
  section(canvas, "Render Graph", x, y, width);
  if (graph == nullptr || graph->passes.empty()) {
    textRow(canvas, "Passes", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  if (selected_pass >= graph->passes.size()) {
    selected_pass = graph->passes.size() - 1u;
  }
  const std::string pass_count = std::to_string(graph->passes.size());
  textRow(canvas, "Passes", pass_count, x, y, width, visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "graph.prev") &&
        selected_pass > 0u) {
      --selected_pass;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "graph.next") &&
        selected_pass + 1u < graph->passes.size()) {
      ++selected_pass;
    }
  }
  y += 38.0f;
  const aster::framegraph::CompiledPass &pass = graph->passes[selected_pass];
  const std::string pass_label =
      std::to_string(selected_pass + 1u) + " " + pass.name;
  textRow(canvas, "Selected", pass_label, x, y, width, visible_top, visible_bottom);
  char buffer[64]{};
  std::snprintf(buffer, sizeof(buffer), "0x%08x", pass.read_mask);
  textRow(canvas, "Reads", buffer, x, y, width, visible_top, visible_bottom);
  std::snprintf(buffer, sizeof(buffer), "0x%08x", pass.write_mask);
  textRow(canvas, "Writes", buffer, x, y, width, visible_top, visible_bottom);
}

std::string clippedValue(const std::string &value, const std::size_t max_size = 44u) {
  if (value.size() <= max_size) {
    return value;
  }
  return value.substr(0u, max_size - 3u) + "...";
}

std::string firstOrNone(const std::vector<std::string> &values) {
  return values.empty() ? std::string("none") : clippedValue(values.front());
}

void drawAssetFactoryPanel(aster::UiCanvas &canvas, std::size_t &selected_asset,
                           const aster::EditorRuntimeModel &runtime, const float x, float &y,
                           const float width, const float visible_top,
                           const float visible_bottom) {
  section(canvas, "Asset Factory", x, y, width);
  const aster::AssetDatabase *database = runtime.asset_database;
  const aster::AssetLibrary *library = runtime.asset_library;
  if (database == nullptr) {
    textRow(canvas, "Asset DB", "not loaded", x, y, width, visible_top, visible_bottom);
    return;
  }
  textRow(canvas, "Schema", std::to_string(database->schema_version), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Assets", std::to_string(database->records.size()), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Graph", std::to_string(database->asset_graph.nodes.size()) + " nodes / " +
                              std::to_string(database->asset_graph.edges.size()) + " edges",
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Fate reports", std::to_string(database->fate_reports.size()), x, y, width,
          visible_top, visible_bottom);
  if (library == nullptr || library->assets.empty()) {
    return;
  }
  selected_asset = std::min(selected_asset, library->assets.size() - 1u);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "asset.prev") &&
        selected_asset > 0u) {
      --selected_asset;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "asset.next") &&
        selected_asset + 1u < library->assets.size()) {
      ++selected_asset;
    }
  }
  y += 38.0f;
  const aster::AssetRepresentation &asset = library->assets[selected_asset];
  textRow(canvas, "Selected", clippedValue(asset.name), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Kind", asset.kind, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Ready", yesNo(asset.production_ready), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Source", clippedValue(asset.source_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Material hash", clippedValue(asset.derived_hashes.material_hash), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Shader key", clippedValue(asset.derived_hashes.shader_variant_key), x, y, width,
          visible_top, visible_bottom);
  if (!asset.diagnostics.empty()) {
    textRow(canvas, "Diagnostic", clippedValue(asset.diagnostics.front()), x, y, width,
            visible_top, visible_bottom);
  }
}

void drawObjectFatePanel(aster::UiCanvas &canvas, std::size_t &selected_object,
                         const aster::FrameForensics *forensics, const float x, float &y,
                         const float width, const float visible_top,
                         const float visible_bottom) {
  section(canvas, "Object Fate", x, y, width);
  if (forensics == nullptr || forensics->object_fates.empty()) {
    textRow(canvas, "Objects", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_object = std::min(selected_object, forensics->object_fates.size() - 1u);
  textRow(canvas, "Records", std::to_string(forensics->object_fates.size()), x, y, width,
          visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "fate.prev") &&
        selected_object > 0u) {
      --selected_object;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "fate.next") &&
        selected_object + 1u < forensics->object_fates.size()) {
      ++selected_object;
    }
  }
  y += 38.0f;
  const aster::ObjectRenderFateTrace &fate = forensics->object_fates[selected_object];
  textRow(canvas, "Object", clippedValue(fate.object_name), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Visible", yesNo(fate.visible), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Mesh", clippedValue(fate.mesh_key), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Material", clippedValue(fate.material_asset_id), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Shader", clippedValue(fate.shader_variant_key), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Pass", firstOrNone(fate.pass_list), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Texture", firstOrNone(fate.texture_roles), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Resource", firstOrNone(fate.resource_transitions), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Capture", firstOrNone(fate.capture_labels), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Contribution", fate.final_contribution, x, y, width, visible_top,
          visible_bottom);
}

void drawFramePanel(aster::UiCanvas &canvas, const aster::UiRect panel,
                    const aster::FrameStats &stats) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  canvas.text("Frame", {x, y}, kAmber, 1.8f);
  y += 28.0f;
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "Frame %.2f ms", stats.frame_seconds * 1000.0);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  const double fps = stats.frame_seconds > 0.0 ? 1.0 / stats.frame_seconds : 0.0;
  std::snprintf(buffer, sizeof(buffer), "Rate %.1f fps", fps);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Resolution %d x %d", stats.framebuffer_width,
                stats.framebuffer_height);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Draw calls %zu", stats.draw_calls);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Visible %zu culled %zu", stats.visible_objects,
                stats.culled_objects);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Groups %zu Rust %.3f ms", stats.instance_groups,
                stats.rust_plan_seconds * 1000.0);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Pipelines %zu materials %zu", stats.pipeline_switches,
                stats.material_permutations);
  canvas.text(buffer, {x, y}, kText, 1.45f);
}

void drawCameraPanel(aster::UiCanvas &canvas, const aster::UiRect panel,
                     aster::OrbitCamera &camera) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  const float visible_top = panel.y + kPanelPad;
  const float visible_bottom = panel.y + panel.height - kPanelPad;
  section(canvas, "Camera", x, y, width);
  sliderRow(canvas, "Yaw", camera.yaw, -3.14159f, 3.14159f, x, y, width, "yaw",
            visible_top, visible_bottom);
  sliderRow(canvas, "Pitch", camera.pitch, aster::radians(-80.0f), aster::radians(80.0f), x, y,
            width, "pitch", visible_top, visible_bottom);
  sliderRow(canvas, "Radius", camera.radius, 1.8f, 24.0f, x, y, width, "radius", visible_top,
            visible_bottom);
}

void drawSceneSummary(aster::UiCanvas &canvas, const aster::Scene &scene, const float x, float &y,
                      const float width, const float visible_top, const float visible_bottom) {
  const float row_top = y;
  constexpr float kSummaryHeight = 76.0f;
  if (row_top < visible_top || row_top + kSummaryHeight > visible_bottom) {
    y += kSummaryHeight;
    return;
  }
  section(canvas, "Scene", x, y, width);
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "Objects %zu", scene.objects().size());
  canvas.text(buffer, {x, y}, kText, 1.45f);
}

} // namespace

namespace aster {

EditorUi::~EditorUi() {
  shutdown();
}

void EditorUi::initialize() {
  canvas_.initialize();
  initialized_ = true;
}

void EditorUi::beginFrame(const Vec2 viewport_size, const ControlSnapshot &input) {
  input_ = input;
  canvas_.beginFrame(viewport_size, input);
}

void EditorUi::draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings,
                    const FrameStats &stats, const EditorRuntimeModel &runtime) {
  const Vec2 viewport = canvas_.viewportSize();
  const float panel_height = std::max(320.0f, viewport.y - kPanelMargin * 2.0f);
  const float panel_width = std::min(
      std::max(viewport.x - kPanelMargin * 2.0f, 260.0f),
      std::clamp(viewport.x < 760.0f ? viewport.x - kPanelMargin * 2.0f : viewport.x * 0.34f,
                 360.0f, 460.0f));
  const UiRect panel{kPanelMargin, kPanelMargin, panel_width, panel_height};
  drawPanelTexture(canvas_, panel);
  if (pointInRect(input_.pointer, panel) && input_.scroll.y != 0.0f) {
    renderer_panel_scroll_ =
        std::max(0.0f, renderer_panel_scroll_ - input_.scroll.y * 36.0f);
  }

  canvas_.pushClip({panel.x + 1.0f, panel.y + 1.0f, panel.width - 2.0f, panel.height - 2.0f});
  float y = panel.y + kPanelPad - renderer_panel_scroll_;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  const float panel_bottom = panel.y + panel.height - kPanelPad;
  const float visible_top = panel.y + kPanelPad;

  canvas_.text("Aster Learning Studio", {x, y}, kText, 2.2f);
  y += 31.0f;
  canvas_.wrappedText("Native engine controls over scene, renderer, and camera state.", {x, y},
                      width, kDim, 1.3f);
  y += canvas_.wrappedTextHeight("Native engine controls over scene, renderer, and camera state.",
                                 width, 1.3f) +
       22.0f;

  section(canvas_, "Renderer", x, y, width);
  sliderRow(canvas_, "Exposure", settings.exposure, 0.2f, 2.5f, x, y, width, "exposure",
            visible_top, panel_bottom);
  sliderRow(canvas_, "Ambient", settings.ambient_strength, 0.0f, 0.6f, x, y, width, "ambient",
            visible_top, panel_bottom);
  checkboxRow(canvas_, "Grounding", settings.grounding.enabled, x, y, width, "grounding",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Contact shadows", settings.grounding.contact_shadows, x, y, width,
              "contact.shadows", visible_top, panel_bottom);
  sliderRow(canvas_, "Ground AO", settings.grounding.surface_occlusion_strength, 0.0f, 0.7f, x, y,
            width, "ground.ao", visible_top, panel_bottom);
  sliderRow(canvas_, "AO mix", settings.grounding.surface_occlusion_mix, 0.0f, 1.0f, x, y, width,
            "ground.ao.mix", visible_top, panel_bottom);
  sliderRow(canvas_, "AO floor", settings.grounding.surface_occlusion_min, 0.0f, 1.0f, x, y, width,
            "ground.ao.floor", visible_top, panel_bottom);
  sliderRow(canvas_, "Shadow weight", settings.grounding.contact_shadow_strength, 0.0f, 0.8f, x, y,
            width, "contact.weight", visible_top, panel_bottom);
  checkboxRow(canvas_, "Atmosphere", settings.atmosphere.enabled, x, y, width, "atmosphere",
              visible_top, panel_bottom);
  renderStyleRow(canvas_, settings, x, y, width, visible_top, panel_bottom);
  sliderRow(canvas_, "Fog", settings.atmosphere.fog_strength, 0.0f, 0.6f, x, y, width, "fog",
            visible_top, panel_bottom);
  sliderRow(canvas_, "Saturation", settings.atmosphere.saturation, 0.0f, 1.4f, x, y, width,
            "saturation", visible_top, panel_bottom);
  sliderRow(canvas_, "Contrast", settings.atmosphere.contrast, 0.5f, 1.5f, x, y, width,
            "contrast", visible_top, panel_bottom);
  checkboxRow(canvas_, "ACES tone map", settings.use_aces_tonemap, x, y, width, "aces",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Animation", settings.animate_scene, x, y, width, "animation", visible_top,
              panel_bottom);
  checkboxRow(canvas_, "Depth test", settings.pipeline.depth_test, x, y, width, "depth",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Back-face culling", settings.pipeline.back_face_culling, x, y, width,
              "cull", visible_top, panel_bottom);
  checkboxRow(canvas_, "Multisampling", settings.pipeline.multisampling, x, y, width, "msaa",
              visible_top, panel_bottom);
  y += 8.0f;
  drawBackendSummary(canvas_, runtime, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawGraphSummary(canvas_, selected_graph_pass_, runtime.render_graph, x, y, width, visible_top,
                   panel_bottom);
  y += 8.0f;
  drawSceneSummary(canvas_, scene, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawAssetFactoryPanel(canvas_, selected_asset_, runtime, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawObjectFatePanel(canvas_, selected_object_fate_, runtime.frame_forensics, x, y, width,
                      visible_top, panel_bottom);
  const float content_height = y + kPanelPad + renderer_panel_scroll_ - panel.y;
  const float max_scroll = std::max(0.0f, content_height - panel.height);
  renderer_panel_scroll_ = std::clamp(renderer_panel_scroll_, 0.0f, max_scroll);
  if (max_scroll > 1.0f) {
    const float track_height = panel.height - 32.0f;
    const float thumb_height = std::max(34.0f, track_height * (panel.height / content_height));
    const float thumb_y = panel.y + 16.0f +
                          (track_height - thumb_height) *
                              (renderer_panel_scroll_ / std::max(max_scroll, 0.001f));
    canvas_.fillRoundRect({panel.x + panel.width - 9.0f, thumb_y, 4.0f, thumb_height}, 2.0f,
                          {0.78f, 0.58f, 0.31f, 0.46f});
  }
  canvas_.popClip();

  if (viewport.x >= 900.0f) {
    const float side_width = 324.0f;
    const float side_x = viewport.x - side_width - kPanelMargin;
    const UiRect frame_panel{side_x, kPanelMargin, side_width, 206.0f};
    drawFramePanel(canvas_, frame_panel, stats);
    const UiRect camera_panel{side_x, frame_panel.y + frame_panel.height + 14.0f, side_width,
                              198.0f};
    drawCameraPanel(canvas_, camera_panel, camera);
  }
}

void EditorUi::endFrame() {
  canvas_.endFrame();
}

void EditorUi::shutdown() {
  if (!initialized_) {
    return;
  }
  canvas_.shutdown();
  initialized_ = false;
}

bool EditorUi::wantsMouse() const {
  return canvas_.wantsMouse();
}

bool EditorUi::wantsKeyboard() const {
  return canvas_.wantsKeyboard();
}

} // namespace aster
