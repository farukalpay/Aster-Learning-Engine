// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/scene/scene.hpp"
#include "aster/ui/editor_ui.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

namespace {

constexpr const char *kClose = "app.close";
constexpr const char *kOrbitLeft = "camera.orbit.left";
constexpr const char *kOrbitRight = "camera.orbit.right";
constexpr const char *kOrbitUp = "camera.orbit.up";
constexpr const char *kOrbitDown = "camera.orbit.down";
constexpr const char *kOrbitPointer = "camera.orbit.pointer";
constexpr const char *kZoomIn = "camera.zoom.in";
constexpr const char *kZoomOut = "camera.zoom.out";
constexpr double kDefaultInteractiveFrameCapSeconds = 1.0 / 60.0;
constexpr double kFramePacingSchedulerGuardSeconds = 0.0012;
constexpr double kFramePacingYieldThresholdSeconds = 0.00018;

bool hasArgument(const int argc, char **argv, const std::string_view value) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == value) {
      return true;
    }
  }
  return false;
}

std::filesystem::path argumentPath(const int argc, char **argv, const std::string_view name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return {};
}

int argumentInt(const int argc, char **argv, const std::string_view name, const int fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return std::stoi(argv[i + 1]);
    }
  }
  return fallback;
}

float argumentFloat(const int argc, char **argv, const std::string_view name,
                    const float fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return std::stof(argv[i + 1]);
    }
  }
  return fallback;
}

double secondsToMilliseconds(const double seconds) {
  return seconds * 1000.0;
}

void printFrameSummary(const char *label, const aster::FrameTimeSummary &summary) {
  std::cout << label << ": samples=" << summary.samples
            << " min_ms=" << secondsToMilliseconds(summary.min_seconds)
            << " mean_ms=" << secondsToMilliseconds(summary.mean_seconds)
            << " p95_ms=" << secondsToMilliseconds(summary.p95_seconds)
            << " max_ms=" << secondsToMilliseconds(summary.max_seconds);
  if (summary.budget_seconds.has_value()) {
    std::cout << " budget_ms=" << secondsToMilliseconds(*summary.budget_seconds)
              << " over_budget=" << summary.over_budget;
  }
  std::cout << '\n';
}

void sleepForFrameCap(const aster::Clock &clock, const double frame_start_seconds,
                      const double target_frame_seconds) {
  if (target_frame_seconds <= 0.0) {
    return;
  }
  const double remaining = target_frame_seconds - (clock.now() - frame_start_seconds);
  if (remaining > kFramePacingSchedulerGuardSeconds + kFramePacingYieldThresholdSeconds) {
    std::this_thread::sleep_for(
        std::chrono::duration<double>(remaining - kFramePacingSchedulerGuardSeconds));
  }
  while (target_frame_seconds - (clock.now() - frame_start_seconds) >
         kFramePacingYieldThresholdSeconds) {
    std::this_thread::yield();
  }
}

aster::ControlScheme makeStudioControls() {
  aster::ControlScheme controls;
  controls.bind(kClose, aster::keyBinding(aster::Key::Escape));
  controls.bind(kOrbitLeft, aster::keyBinding(aster::Key::Left));
  controls.bind(kOrbitRight, aster::keyBinding(aster::Key::Right));
  controls.bind(kOrbitUp, aster::keyBinding(aster::Key::Up));
  controls.bind(kOrbitDown, aster::keyBinding(aster::Key::Down));
  controls.bind(kOrbitPointer, aster::mouseBinding(aster::MouseButton::Right));
  controls.bind(kZoomIn, aster::keyBinding(aster::Key::Q));
  controls.bind(kZoomOut, aster::keyBinding(aster::Key::E));
  return controls;
}

void updateCameraFromInput(aster::Window &window, aster::EditorUi &ui, aster::OrbitCamera &camera,
                           const aster::ControlState &controls) {
  static float previous_x = 0.0f;
  static float previous_y = 0.0f;
  static bool dragging = false;

  if (controls.justPressed(kClose)) {
    window.requestClose();
  }

  if (ui.wantsKeyboard()) {
    return;
  }

  constexpr float kKeyOrbitStep = 0.018f;
  constexpr float kKeyZoomStep = 0.055f;
  const float orbit_x = controls.strength(kOrbitRight) - controls.strength(kOrbitLeft);
  const float orbit_y = controls.strength(kOrbitDown) - controls.strength(kOrbitUp);
  camera.orbit(orbit_x * kKeyOrbitStep, orbit_y * kKeyOrbitStep);

  const float zoom = controls.strength(kZoomOut) - controls.strength(kZoomIn);
  camera.zoom(zoom * kKeyZoomStep);

  if (ui.wantsMouse() || !controls.pressed(kOrbitPointer)) {
    dragging = false;
    return;
  }

  const aster::Vec2 pointer = controls.snapshot().pointer;
  if (!dragging) {
    previous_x = pointer.x;
    previous_y = pointer.y;
    dragging = true;
  }

  constexpr float kPointerOrbitScale = 0.0065f;
  camera.orbit((pointer.x - previous_x) * kPointerOrbitScale,
               (pointer.y - previous_y) * kPointerOrbitScale);
  previous_x = pointer.x;
  previous_y = pointer.y;
}

} // namespace

int main(int argc, char **argv) {
  try {
    aster::EngineConfig config;
    const bool smoke_test = hasArgument(argc, argv, "--smoke-test");
    const std::filesystem::path screenshot_path = argumentPath(argc, argv, "--screenshot");
    const bool scripted_capture = !screenshot_path.empty();
    const bool frame_report_enabled = hasArgument(argc, argv, "--frame-report");
    const int run_frames = argumentInt(argc, argv, "--run-frames", 0);
    const int frame_report_warmup =
        std::max(0, argumentInt(argc, argv, "--frame-report-warmup", 0));
    const float lag_budget_ms = argumentFloat(argc, argv, "--lag-budget-ms", -1.0f);
    const std::optional<double> lag_budget_seconds =
        lag_budget_ms > 0.0f ? std::optional<double>{static_cast<double>(lag_budget_ms) / 1000.0}
                             : std::nullopt;
    const bool unlocked = hasArgument(argc, argv, "--unlocked");
    config.enable_vsync = !scripted_capture && !unlocked && !frame_report_enabled &&
                          !hasArgument(argc, argv, "--no-vsync");
    config.initial_width = argumentInt(argc, argv, "--window-width", config.initial_width);
    config.initial_height = argumentInt(argc, argv, "--window-height", config.initial_height);
    const double target_frame_seconds = (!scripted_capture && !unlocked && !frame_report_enabled)
                                            ? kDefaultInteractiveFrameCapSeconds
                                            : 0.0;

    aster::Window window(config);
    aster::RenderDevice renderer;
    renderer.initialize();

    aster::Scene scene = aster::Scene::makeShowcase();
    renderer.prepareScene(scene);
    aster::OrbitCamera camera;
    if (!screenshot_path.empty()) {
      camera.target = {0.0f, 1.18f, -0.35f};
      camera.pitch = aster::radians(22.0f);
      camera.yaw = aster::radians(30.0f);
      camera.radius = 7.8f;
    }
    aster::RendererSettings settings;
    settings.exposure = 1.16f;
    settings.ambient_strength = 0.28f;
    settings.sun_light.enabled = true;
    settings.sun_light.direction_to_light = {-0.48f, 0.84f, 0.26f};
    settings.sun_light.color = {1.0f, 0.82f, 0.56f};
    settings.sun_light.intensity = 1.04f;
    settings.pipeline.clear_color = {0.060f, 0.072f, 0.088f};
    settings.pipeline.tone_mapper = aster::ToneMapper::Reinhard;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.surface_occlusion_strength = 0.34f;
    settings.grounding.surface_occlusion_mix = 0.34f;
    settings.atmosphere.enabled = true;
    settings.atmosphere.fog_color = {0.13f, 0.15f, 0.18f};
    settings.atmosphere.fog_start = 7.0f;
    settings.atmosphere.fog_end = 20.0f;
    settings.atmosphere.fog_strength = 0.12f;
    aster::ControlScheme control_scheme = makeStudioControls();
    aster::ControlState control_state;
    aster::EditorUi ui;
    ui.initialize();

    aster::Clock clock;
    aster::FrameTimeStats frame_times;
    aster::FrameTimeStats render_times;
    aster::FrameTimeStats ui_times;
    int rendered_frames = 0;
    bool captured = false;
    while (window.isOpen()) {
      window.pollEvents();
      const double dt = clock.tick();
      const double frame_start_seconds = clock.now();
      const double elapsed = clock.now();
      const bool collect_frame_sample =
          frame_report_enabled && rendered_frames > frame_report_warmup;
      control_state.update(control_scheme, window.captureControls(control_scheme));

      const double ui_start = clock.now();
      const auto [ui_width, ui_height] = window.windowSize();
      ui.beginFrame({static_cast<float>(ui_width), static_cast<float>(ui_height)},
                    control_state.snapshot());
      updateCameraFromInput(window, ui, camera, control_state);

      const auto [width, height] = window.framebufferSize();
      const double render_start = clock.now();
      const aster::FrameStats stats =
          renderer.render(scene, camera, settings, width, height, elapsed);
      if (collect_frame_sample) {
        render_times.addSample(clock.now() - render_start);
      }
      aster::FrameStats displayed_stats = stats;
      displayed_stats.frame_seconds = dt;

      ui.draw(scene, camera, settings, displayed_stats);
      ui.endFrame();
      if (collect_frame_sample) {
        ui_times.addSample(clock.now() - ui_start);
        frame_times.addSample(clock.now() - frame_start_seconds);
      }

      if (!screenshot_path.empty() && !captured && rendered_frames >= 2) {
        aster::writeFramebufferPpm(screenshot_path, width, height);
        captured = true;
        window.requestClose();
      }

      window.swapBuffers();
      ++rendered_frames;

      if (smoke_test && rendered_frames >= 3) {
        window.requestClose();
      }
      if (run_frames > 0 && rendered_frames >= run_frames) {
        window.requestClose();
      }
      if (window.isOpen()) {
        sleepForFrameCap(clock, frame_start_seconds, target_frame_seconds);
      }
    }
    if (frame_report_enabled) {
      std::cout << std::fixed << std::setprecision(3);
      std::cout << "Studio frame report warmup: frames=" << frame_report_warmup << '\n';
      printFrameSummary("Studio frame report", frame_times.summarize(lag_budget_seconds));
      printFrameSummary("Studio render report", render_times.summarize());
      printFrameSummary("Studio UI report", ui_times.summarize());
    }
  } catch (const std::exception &error) {
    std::cerr << "Aster Learning Studio failed: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
