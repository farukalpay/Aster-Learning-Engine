// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/editor_ui.hpp"

#include "aster/platform/window.hpp"
#include "aster/scene/scene.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

constexpr aster::UiColor kPanel{0.026f, 0.032f, 0.038f, 0.88f};
constexpr aster::UiColor kPanelEdge{0.72f, 0.52f, 0.26f, 0.34f};
constexpr aster::UiColor kText{0.91f, 0.93f, 0.89f, 1.0f};
constexpr aster::UiColor kDim{0.55f, 0.63f, 0.63f, 1.0f};
constexpr aster::UiColor kAmber{0.88f, 0.58f, 0.23f, 1.0f};
constexpr float kPanelMargin = 16.0f;
constexpr float kPanelPad = 18.0f;

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
               const float width, const std::string &id) {
  canvas.slider({x, y + 19.0f, width, 9.0f}, label, value, min_value, max_value, id);
  y += 40.0f;
}

void checkboxRow(aster::UiCanvas &canvas, const std::string &label, bool &value, const float x,
                 float &y, const float width, const std::string &id) {
  canvas.checkbox({x, y, width, 28.0f}, label, value, id);
  y += 30.0f;
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
}

void drawCameraPanel(aster::UiCanvas &canvas, const aster::UiRect panel,
                     aster::OrbitCamera &camera) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  section(canvas, "Camera", x, y, width);
  sliderRow(canvas, "Yaw", camera.yaw, -3.14159f, 3.14159f, x, y, width, "yaw");
  sliderRow(canvas, "Pitch", camera.pitch, aster::radians(-80.0f), aster::radians(80.0f), x, y,
            width, "pitch");
  sliderRow(canvas, "Radius", camera.radius, 1.8f, 24.0f, x, y, width, "radius");
}

void drawSceneSummary(aster::UiCanvas &canvas, const aster::Scene &scene, const float x, float &y,
                      const float width, const float bottom) {
  if (y + 76.0f > bottom) {
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

void EditorUi::initialize(Window &window) {
  canvas_.initialize(window.nativeHandle());
  initialized_ = true;
}

void EditorUi::beginFrame() {
  canvas_.beginFrame();
}

void EditorUi::draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings,
                    const FrameStats &stats) {
  const Vec2 viewport = canvas_.viewportSize();
  const float panel_height = std::max(320.0f, viewport.y - kPanelMargin * 2.0f);
  const UiRect panel{kPanelMargin, kPanelMargin, 420.0f, panel_height};
  drawPanelTexture(canvas_, panel);

  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  const float panel_bottom = panel.y + panel.height - kPanelPad;

  canvas_.text("Aster Learning Studio", {x, y}, kText, 2.2f);
  y += 31.0f;
  canvas_.wrappedText("Native engine controls over scene, renderer, and camera state.", {x, y},
                      width, kDim, 1.3f);
  y += canvas_.wrappedTextHeight("Native engine controls over scene, renderer, and camera state.",
                                 width, 1.3f) +
       22.0f;

  section(canvas_, "Renderer", x, y, width);
  sliderRow(canvas_, "Exposure", settings.exposure, 0.2f, 2.5f, x, y, width, "exposure");
  sliderRow(canvas_, "Ambient", settings.ambient_strength, 0.0f, 0.6f, x, y, width, "ambient");
  checkboxRow(canvas_, "Grounding", settings.grounding.enabled, x, y, width, "grounding");
  checkboxRow(canvas_, "Contact shadows", settings.grounding.contact_shadows, x, y, width,
              "contact.shadows");
  sliderRow(canvas_, "Ground AO", settings.grounding.surface_occlusion_strength, 0.0f, 0.7f, x, y,
            width, "ground.ao");
  sliderRow(canvas_, "AO mix", settings.grounding.surface_occlusion_mix, 0.0f, 1.0f, x, y, width,
            "ground.ao.mix");
  sliderRow(canvas_, "AO floor", settings.grounding.surface_occlusion_min, 0.0f, 1.0f, x, y, width,
            "ground.ao.floor");
  sliderRow(canvas_, "Shadow weight", settings.grounding.contact_shadow_strength, 0.0f, 0.8f, x, y,
            width, "contact.weight");
  checkboxRow(canvas_, "Atmosphere", settings.atmosphere.enabled, x, y, width, "atmosphere");
  sliderRow(canvas_, "Fog", settings.atmosphere.fog_strength, 0.0f, 0.6f, x, y, width, "fog");
  sliderRow(canvas_, "Saturation", settings.atmosphere.saturation, 0.0f, 1.4f, x, y, width,
            "saturation");
  sliderRow(canvas_, "Contrast", settings.atmosphere.contrast, 0.5f, 1.5f, x, y, width, "contrast");
  checkboxRow(canvas_, "ACES tone map", settings.use_aces_tonemap, x, y, width, "aces");
  checkboxRow(canvas_, "Animation", settings.animate_scene, x, y, width, "animation");
  checkboxRow(canvas_, "Depth test", settings.pipeline.depth_test, x, y, width, "depth");
  checkboxRow(canvas_, "Back-face culling", settings.pipeline.back_face_culling, x, y, width,
              "cull");
  checkboxRow(canvas_, "Multisampling", settings.pipeline.multisampling, x, y, width, "msaa");
  y += 8.0f;
  drawSceneSummary(canvas_, scene, x, y, width, panel_bottom);

  if (viewport.x >= 900.0f) {
    const float side_width = 324.0f;
    const float side_x = viewport.x - side_width - kPanelMargin;
    const UiRect frame_panel{side_x, kPanelMargin, side_width, 138.0f};
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
