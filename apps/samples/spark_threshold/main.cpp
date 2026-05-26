// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/samples/spark_threshold/spark_threshold.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr const char *kMoveLeft = "spark.move.left";
constexpr const char *kMoveRight = "spark.move.right";
constexpr const char *kMoveUp = "spark.move.up";
constexpr const char *kMoveDown = "spark.move.down";
constexpr const char *kRun = "spark.run";
constexpr const char *kInteract = "spark.interact";
constexpr const char *kReset = "spark.reset";
constexpr const char *kQuit = "spark.quit";

bool hasArgument(const int argc, char **argv, const std::string_view value) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == value) {
      return true;
    }
  }
  return false;
}

std::string argumentString(const int argc, char **argv, const std::string_view name,
                           const std::string &fallback = {}) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

int argumentInt(const int argc, char **argv, const std::string_view name, const int fallback) {
  const std::string value = argumentString(argc, argv, name);
  return value.empty() ? fallback : std::stoi(value);
}

std::uint32_t argumentU32(const int argc, char **argv, const std::string_view name,
                          const std::uint32_t fallback) {
  const std::string value = argumentString(argc, argv, name);
  return value.empty() ? fallback : static_cast<std::uint32_t>(std::stoul(value));
}

std::filesystem::path argumentPath(const int argc, char **argv, const std::string_view name) {
  const std::string value = argumentString(argc, argv, name);
  return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
}

std::filesystem::path defaultProjectPath() {
#if defined(ASTER_SPARK_THRESHOLD_PROJECT)
  return ASTER_SPARK_THRESHOLD_PROJECT;
#else
  return {};
#endif
}

aster::SparkThresholdScriptRoute routeFromArgument(const std::string &value) {
  if (value == "escape" || value == "kacis") {
    return aster::SparkThresholdScriptRoute::Escape;
  }
  if (value == "overload" || value == "asiri-yuk") {
    return aster::SparkThresholdScriptRoute::Overload;
  }
  return aster::SparkThresholdScriptRoute::Resonance;
}

aster::ControlScheme makeControls() {
  aster::ControlScheme scheme;
  for (const char *command :
       {kMoveLeft, kMoveRight, kMoveUp, kMoveDown, kRun, kInteract, kReset, kQuit}) {
    scheme.addCommand(command);
  }
  scheme.bind(kMoveLeft, aster::keyBinding(aster::Key::A));
  scheme.bind(kMoveLeft, aster::keyBinding(aster::Key::Left));
  scheme.bind(kMoveRight, aster::keyBinding(aster::Key::D));
  scheme.bind(kMoveRight, aster::keyBinding(aster::Key::Right));
  scheme.bind(kMoveUp, aster::keyBinding(aster::Key::W));
  scheme.bind(kMoveUp, aster::keyBinding(aster::Key::Up));
  scheme.bind(kMoveDown, aster::keyBinding(aster::Key::S));
  scheme.bind(kMoveDown, aster::keyBinding(aster::Key::Down));
  scheme.bind(kRun, aster::keyBinding(aster::Key::LeftShift));
  scheme.bind(kInteract, aster::keyBinding(aster::Key::E));
  scheme.bind(kInteract, aster::keyBinding(aster::Key::Space));
  scheme.bind(kReset, aster::keyBinding(aster::Key::R));
  scheme.bind(kQuit, aster::keyBinding(aster::Key::Escape));
  return scheme;
}

aster::SimCommand commandFromControls(const aster::ControlState &controls) {
  aster::SimCommand command;
  const float x = controls.strength(kMoveRight) - controls.strength(kMoveLeft);
  const float y = controls.strength(kMoveDown) - controls.strength(kMoveUp);
  command.strafe = static_cast<std::int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f);
  command.forward = static_cast<std::int16_t>(std::clamp(y, -1.0f, 1.0f) * 32767.0f);
  command.set(aster::SimCommandButton::Interact, controls.justPressed(kInteract));
  command.set(aster::SimCommandButton::Run, controls.pressed(kRun));
  return command;
}

aster::RendererSettings sparkRendererSettings() {
  aster::RendererSettings settings;
  settings.exposure = 1.22f;
  settings.ambient_strength = 0.24f;
  settings.ambient_floor = 0.02f;
  settings.sky_ambient_color = {0.08f, 0.12f, 0.16f};
  settings.ground_ambient_color = {0.04f, 0.028f, 0.022f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.35f, 0.82f, 0.22f};
  settings.sun_light.color = {0.80f, 0.88f, 1.0f};
  settings.sun_light.intensity = 0.34f;
  settings.light_rig = {{{-6.4f, 3.0f, -2.2f}, {0.10f, 0.82f, 1.00f}, 1.25f, 5.2f},
                        {{6.8f, 3.2f, -1.2f}, {1.00f, 0.42f, 0.12f}, 1.05f, 5.5f},
                        {{0.0f, 3.4f, 7.0f}, {0.46f, 0.28f, 1.00f}, 0.86f, 5.0f},
                        {{0.0f, 2.2f, 10.4f}, {0.22f, 0.88f, 1.00f}, 1.45f, 4.6f}};
  settings.pipeline.clear_color = {0.004f, 0.006f, 0.010f};
  settings.pipeline.multisampling = true;
  settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
  settings.procedural_surface_normals = true;
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = true;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.015f, 0.040f, 0.055f};
  settings.atmosphere.fog_start = 14.0f;
  settings.atmosphere.fog_end = 38.0f;
  settings.atmosphere.fog_strength = 0.075f;
  settings.atmosphere.saturation = 1.22f;
  settings.atmosphere.contrast = 1.16f;
  settings.post.bloom = true;
  settings.post.fxaa = true;
  settings.post.bloom_threshold = 0.92f;
  settings.post.bloom_intensity = 0.26f;
  settings.surface_scale.physical_texel_density = 920.0f;
  settings.surface_scale.height_normal_coupling = 0.98f;
  settings.surface_scale.roughness_height_coupling = 0.78f;
  settings.surface_scale.macro_frequency_breakup = 0.62f;
  settings.surface_scale.micro_frequency_breakup = 0.82f;
  return settings;
}

aster::OrbitCamera sparkCamera(const aster::SparkThreshold &game) {
  aster::OrbitCamera camera;
  if (!game.status().title_screen &&
      game.status().outcome != aster::SparkThresholdOutcome::Playing) {
    camera.target = {0.0f, 1.18f, 9.85f};
    camera.pitch = aster::radians(28.0f);
    camera.radius = 7.6f;
  } else {
    camera.target = game.cameraTarget();
    camera.pitch = aster::radians(48.0f);
    camera.radius = 11.6f;
  }
  camera.yaw = aster::radians(0.0f);
  camera.vertical_fov = aster::radians(46.0f);
  camera.near_plane = 0.05f;
  camera.far_plane = 90.0f;
  return camera;
}

void drawHud(aster::UiCanvas &canvas, const aster::SparkThresholdHudModel &model,
             const aster::ControlSnapshot &input, const int width, const int height) {
  canvas.beginFrame({static_cast<float>(width), static_cast<float>(height)}, input);
  const aster::UiColor panel{0.008f, 0.012f, 0.018f, 0.84f};
  const aster::UiColor cyan{0.34f, 0.92f, 1.0f, 1.0f};
  const aster::UiColor amber{1.0f, 0.64f, 0.22f, 1.0f};
  const aster::UiColor text{0.86f, 0.94f, 0.94f, 1.0f};
  const aster::UiColor dim{0.62f, 0.74f, 0.76f, 1.0f};

  if (model.title_screen) {
    canvas.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
                    {0.004f, 0.006f, 0.011f, 0.72f});
    const float title_width = canvas.textWidth(model.title, 2.75f);
    canvas.text(model.title, {(static_cast<float>(width) - title_width) * 0.5f,
                              static_cast<float>(height) * 0.30f},
                cyan, 2.75f);
    const float body_width = 620.0f;
    canvas.wrappedText(model.subtitle, {(static_cast<float>(width) - body_width) * 0.5f,
                                        static_cast<float>(height) * 0.42f},
                       body_width, text, 1.35f);
    const char *start = "E / Space: Basla    R: Yeniden baslat    Esc: Cik";
    const float start_width = canvas.textWidth(start, 1.22f);
    canvas.text(start, {(static_cast<float>(width) - start_width) * 0.5f,
                        static_cast<float>(height) * 0.58f},
                amber, 1.22f);
    canvas.endFrame();
    return;
  }

  if (!model.ended) {
    canvas.fillRect({18.0f, 18.0f, 470.0f, 154.0f}, panel);
    canvas.text(model.title, {36.0f, 34.0f}, cyan, 1.82f);
    canvas.text(model.objective, {36.0f, 66.0f}, text, 1.12f);
    canvas.progressBar({36.0f, 96.0f, 420.0f, 12.0f}, model.node_fraction, cyan,
                       {0.06f, 0.08f, 0.10f, 0.90f});
    canvas.progressBar({36.0f, 124.0f, 420.0f, 10.0f}, model.time_fraction, amber,
                       {0.16f, 0.04f, 0.04f, 0.90f});
    canvas.text(model.status_line, {36.0f, 144.0f}, dim, 1.05f);

    canvas.fillRect({18.0f, static_cast<float>(height) - 142.0f, 560.0f, 104.0f}, panel);
    canvas.text(model.guide_title, {36.0f, static_cast<float>(height) - 124.0f}, amber, 1.38f);
    canvas.wrappedText(model.guide_body, {36.0f, static_cast<float>(height) - 96.0f}, 520.0f,
                       text, 1.05f);
    canvas.text(model.guide_hint, {36.0f, static_cast<float>(height) - 54.0f}, dim, 1.0f);
    canvas.text(model.route_hint, {static_cast<float>(width) - 360.0f, 26.0f}, dim, 1.05f);

    if (!model.prompt_line.empty()) {
      const float prompt_width = canvas.textWidth(model.prompt_line, 1.45f);
      canvas.fillRect({(static_cast<float>(width) - prompt_width) * 0.5f - 18.0f,
                       static_cast<float>(height) - 84.0f, prompt_width + 36.0f, 38.0f},
                      {0.010f, 0.026f, 0.030f, 0.72f});
      canvas.text(model.prompt_line, {(static_cast<float>(width) - prompt_width) * 0.5f,
                                      static_cast<float>(height) - 74.0f},
                  cyan, 1.45f);
    }
  }

  if (model.ended) {
    const float box_width = std::min(600.0f, static_cast<float>(width) - 36.0f);
    const float box_y = static_cast<float>(height) - 134.0f;
    canvas.fillRect({(static_cast<float>(width) - box_width) * 0.5f, box_y, box_width, 110.0f},
                    {0.006f, 0.010f, 0.014f, 0.68f});
    const aster::UiColor ending_color =
        model.outcome == aster::SparkThresholdOutcome::OverloadEnding
            ? amber
            : (model.outcome == aster::SparkThresholdOutcome::Defeated
                   ? aster::UiColor{1.0f, 0.25f, 0.30f, 1.0f}
                   : cyan);
    const float title_width = canvas.textWidth(model.ending_title, 2.05f);
    canvas.text(model.ending_title, {(static_cast<float>(width) - title_width) * 0.5f,
                                     box_y + 16.0f},
                ending_color, 2.05f);
    canvas.wrappedText(model.ending_body,
                       {(static_cast<float>(width) - box_width) * 0.5f + 34.0f,
                        box_y + 58.0f},
                       box_width - 68.0f, text, 1.10f);
    const char *restart = "R ile yeniden baslat.";
    const float restart_width = canvas.textWidth(restart, 1.08f);
    canvas.text(restart, {(static_cast<float>(width) - restart_width) * 0.5f,
                          box_y + 88.0f},
                dim, 1.08f);
  }
  canvas.endFrame();
}

void writeCapture(const std::filesystem::path &path, const int width, const int height) {
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  if (path.extension() == ".png") {
    aster::writeFramebufferPng(path, width, height);
  } else {
    aster::writeFramebufferPpm(path, width, height);
  }
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool smoke_test = hasArgument(argc, argv, "--smoke-test");
    const bool frame_report = hasArgument(argc, argv, "--frame-report");
    const bool no_vsync = hasArgument(argc, argv, "--no-vsync");
    const bool capture_title = hasArgument(argc, argv, "--capture-title");
    const bool capture_ending = hasArgument(argc, argv, "--capture-ending");
    const std::filesystem::path screenshot = argumentPath(argc, argv, "--screenshot");
    const int screenshot_frame = argumentInt(argc, argv, "--screenshot-frame", capture_title ? 2 : 18);
    const int run_frames = argumentInt(argc, argv, "--run-frames",
                                       smoke_test ? 24 : (screenshot.empty() ? 0 : screenshot_frame + 1));
    const std::uint32_t seed = argumentU32(argc, argv, "--seed", 0xA57E5A7Eu);
    const aster::SparkThresholdScriptRoute route =
        routeFromArgument(argumentString(argc, argv, "--script-route", "resonance"));
    std::filesystem::path project_path = argumentPath(argc, argv, "--project");
    if (project_path.empty()) {
      project_path = defaultProjectPath();
    }
    (void)project_path;

    const bool scripted = smoke_test || !screenshot.empty() || capture_ending;
    aster::EngineConfig config;
    config.application_name = "Kivilcim Esigi";
    config.initial_width = argumentInt(argc, argv, "--window-width", 1600);
    config.initial_height = argumentInt(argc, argv, "--window-height", 900);
    config.multisample_samples = argumentInt(argc, argv, "--msaa", 4);
    config.enable_vsync = !no_vsync && !scripted && !frame_report;

    aster::Window window(config);
    aster::RenderDevice renderer;
    renderer.initialize();
    aster::UiCanvas hud;
    hud.initialize();

    aster::SparkThreshold game({.seed = seed, .title_screen = true});
    if (scripted && !capture_title) {
      game.startRun();
    }
    renderer.prepareScene(game.scene());
    const aster::ControlScheme control_scheme = makeControls();
    aster::ControlState controls;
    aster::FixedTimestep simulation_clock({.step_seconds = 1.0 / 60.0,
                                           .max_frame_seconds = 1.0 / 20.0,
                                           .max_steps_per_frame = 4u});
    aster::Clock clock;
    aster::RendererSettings settings = sparkRendererSettings();
    int rendered_frames = 0;
    bool captured = false;
    double elapsed = 0.0;
    double frame_seconds_sum = 0.0;
    double visible_object_sum = 0.0;
    double draw_call_sum = 0.0;

    while (window.isOpen()) {
      window.pollEvents();
      const double dt = scripted ? 1.0 / 60.0 : clock.tick();
      elapsed += dt;
      controls.update(control_scheme, window.captureControls(control_scheme));
      if (controls.justPressed(kQuit)) {
        window.requestClose();
      }
      if (controls.justPressed(kReset)) {
        game.reset();
        renderer.prepareScene(game.scene());
        simulation_clock.reset();
      }

      if (!capture_title) {
        const std::size_t steps = simulation_clock.advance(dt);
        for (std::size_t step = 0; step < steps; ++step) {
          const aster::SimCommand command =
              scripted ? game.scriptedCommand(route) : commandFromControls(controls);
          game.updateFixed(command);
          renderer.prepareScene(game.scene());
        }
      }

      const auto [width, height] = window.framebufferSize();
      const aster::OrbitCamera camera = sparkCamera(game);
      const aster::FrameStats stats =
          renderer.render(game.scene(), camera, settings, width, height, elapsed);
      frame_seconds_sum += stats.frame_seconds;
      visible_object_sum += static_cast<double>(stats.visible_objects);
      draw_call_sum += static_cast<double>(stats.draw_calls);

      const auto [hud_width, hud_height] = window.windowSize();
      drawHud(hud, game.hudModel(), controls.snapshot(), hud_width, hud_height);

      const bool ending_ready =
          game.status().outcome != aster::SparkThresholdOutcome::Playing && !game.status().title_screen;
      const bool screenshot_ready =
          capture_ending ? ending_ready : rendered_frames >= screenshot_frame;
      if (!screenshot.empty() && !captured && screenshot_ready) {
        writeCapture(screenshot, width, height);
        captured = true;
        window.requestClose();
      }

      window.swapBuffers();
      ++rendered_frames;
      if (smoke_test && rendered_frames >= 24) {
        window.requestClose();
      }
      if (run_frames > 0 && rendered_frames >= run_frames && !capture_ending) {
        window.requestClose();
      }
      if (capture_ending && game.status().outcome != aster::SparkThresholdOutcome::Playing &&
          screenshot.empty() && rendered_frames > 8) {
        window.requestClose();
      }
    }

    hud.shutdown();
    if (frame_report && rendered_frames > 0) {
      const double frames = static_cast<double>(rendered_frames);
      std::cout << "Spark Threshold frame report: frames=" << rendered_frames
                << " mean_frame_ms=" << (frame_seconds_sum / frames) * 1000.0
                << " visible_objects_mean=" << visible_object_sum / frames
                << " draw_calls_mean=" << draw_call_sum / frames
                << " world_hash=0x" << std::hex << game.worldHash() << std::dec << '\n';
    }
    if (smoke_test) {
      std::cout << "Spark Threshold smoke test passed: frames=" << rendered_frames
                << " world_hash=0x" << std::hex << game.worldHash() << std::dec << '\n';
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Spark Threshold error: " << error.what() << '\n';
    return 1;
  }
}
