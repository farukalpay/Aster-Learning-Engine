// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/material/material_asset.hpp"
#include "aster/net/lockstep_command_channel.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/samples/aster_grid_tactics/aster_grid_tactics.hpp"
#include "aster/texture/runtime_texture.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char *kMoveLeft = "grid.move.left";
constexpr const char *kMoveRight = "grid.move.right";
constexpr const char *kMoveUp = "grid.move.up";
constexpr const char *kMoveDown = "grid.move.down";
constexpr const char *kInteract = "grid.interact";
constexpr const char *kReset = "grid.reset";
constexpr const char *kQuit = "grid.quit";

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
  if (value.empty()) {
    return fallback;
  }
  return std::stoi(value);
}

std::uint32_t argumentU32(const int argc, char **argv, const std::string_view name,
                          const std::uint32_t fallback) {
  const std::string value = argumentString(argc, argv, name);
  if (value.empty()) {
    return fallback;
  }
  return static_cast<std::uint32_t>(std::stoul(value));
}

std::filesystem::path argumentPath(const int argc, char **argv, const std::string_view name) {
  const std::string value = argumentString(argc, argv, name);
  return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
}

std::filesystem::path defaultProjectPath() {
#if defined(ASTER_GRID_TACTICS_PROJECT)
  return ASTER_GRID_TACTICS_PROJECT;
#else
  return {};
#endif
}

void printMaterialDiagnostics(const std::vector<aster::MaterialDiagnostic> &diagnostics) {
  for (const aster::MaterialDiagnostic &diagnostic : diagnostics) {
    const char *severity =
        diagnostic.severity == aster::MaterialDiagnosticSeverity::Error ? "error" : "warning";
    std::cerr << "Aster Grid Tactics material " << severity << ": ";
    if (!diagnostic.source_path.empty()) {
      std::cerr << diagnostic.source_path.string() << ": ";
    }
    std::cerr << diagnostic.message << '\n';
  }
}

std::shared_ptr<aster::MaterialResourceLibrary>
loadGridMaterialLibrary(const std::filesystem::path &project_path) {
  auto library = std::make_shared<aster::MaterialResourceLibrary>();
  if (project_path.empty()) {
    return library;
  }
  const std::filesystem::path root = project_path.parent_path();
  const std::filesystem::path material_root = root / "materials";
  const char *material_files[] = {"grid_lab_floor.astermat", "grid_lab_wall.astermat",
                                  "grid_neon.astermat", "grid_security_red.astermat",
                                  "grid_operator_blue.astermat", "grid_energy_green.astermat"};
  for (const char *file : material_files) {
    const std::filesystem::path path = material_root / file;
    const aster::MaterialAssetLoadResult loaded = aster::loadMaterialAsset(path);
    printMaterialDiagnostics(loaded.diagnostics);
    if (!loaded.ok()) {
      throw std::runtime_error("failed to parse Grid Tactics material: " + path.string());
    }
    if (!library->addMaterialAsset(loaded.value, path.parent_path(),
                                   {.require_existing_files = true})) {
      throw std::runtime_error("failed to add Grid Tactics material runtime resource: " +
                               loaded.value.id);
    }
  }
  return library;
}

aster::ControlScheme makeControls() {
  aster::ControlScheme scheme;
  for (const char *command : {kMoveLeft, kMoveRight, kMoveUp, kMoveDown, kInteract, kReset, kQuit}) {
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
  return command;
}

aster::OrbitCamera gridCamera() {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = aster::radians(88.0f);
  camera.radius = 19.0f;
  camera.projection_mode = aster::CameraProjectionMode::Orthographic;
  camera.orthographic_height = 13.2f;
  camera.near_plane = 0.05f;
  camera.far_plane = 80.0f;
  return camera;
}

aster::RendererSettings gridRendererSettings() {
  aster::RendererSettings settings;
  settings.exposure = 1.30f;
  settings.ambient_strength = 0.46f;
  settings.ambient_floor = 0.08f;
  settings.sky_ambient_color = {0.30f, 0.42f, 0.52f};
  settings.ground_ambient_color = {0.08f, 0.12f, 0.15f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.20f, 0.86f, 0.26f};
  settings.sun_light.color = {0.72f, 0.92f, 1.0f};
  settings.sun_light.intensity = 0.82f;
  settings.light_rig = {{{-5.0f, 7.0f, -3.0f}, {0.18f, 0.68f, 1.00f}, 0.44f, 4.0f},
                        {{5.5f, 6.0f, 4.0f}, {1.00f, 0.20f, 0.36f}, 0.30f, 4.5f},
                        {{0.0f, 8.0f, 0.0f}, {0.42f, 1.00f, 0.62f}, 0.42f, 6.5f}};
  settings.pipeline.clear_color = {0.010f, 0.014f, 0.018f};
  settings.pipeline.multisampling = true;
  settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
  settings.procedural_surface_normals = true;
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = false;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.025f, 0.060f, 0.075f};
  settings.atmosphere.fog_start = 16.0f;
  settings.atmosphere.fog_end = 44.0f;
  settings.atmosphere.fog_strength = 0.035f;
  settings.atmosphere.saturation = 1.18f;
  settings.atmosphere.contrast = 1.18f;
  settings.post.bloom = true;
  settings.post.fxaa = true;
  settings.post.bloom_threshold = 1.05f;
  settings.post.bloom_intensity = 0.20f;
  settings.surface_scale.physical_texel_density = 960.0f;
  settings.surface_scale.height_normal_coupling = 1.05f;
  settings.surface_scale.roughness_height_coupling = 0.82f;
  settings.surface_scale.macro_frequency_breakup = 0.62f;
  settings.surface_scale.micro_frequency_breakup = 0.82f;
  return settings;
}

void drawHud(aster::UiCanvas &canvas, const aster::AsterGridTacticsHudModel &model,
             const aster::ControlSnapshot &input, const int width, const int height) {
  canvas.beginFrame({static_cast<float>(width), static_cast<float>(height)}, input);
  canvas.fillRect({0.0f, 0.0f, static_cast<float>(width), 82.0f}, {0.008f, 0.012f, 0.015f, 0.82f});
  canvas.fillRect({0.0f, 74.0f, static_cast<float>(width), 2.0f}, {0.12f, 0.80f, 0.72f, 0.38f});
  canvas.text(model.title, {20.0f, 16.0f}, {0.64f, 0.92f, 1.0f, 1.0f}, 1.68f);
  canvas.text(model.objective, {20.0f, 47.0f}, {0.72f, 0.90f, 0.84f, 1.0f}, 1.05f);
  float chip_x = static_cast<float>(width) * 0.38f;
  const auto chip = [&](const std::string &text, const aster::UiColor color) {
    const float text_width = canvas.textWidth(text, 1.12f);
    canvas.fillRect({chip_x - 12.0f, 20.0f, text_width + 24.0f, 30.0f},
                    {0.016f, 0.032f, 0.034f, 0.88f});
    canvas.text(text, {chip_x, 29.0f}, color, 1.12f);
    chip_x += text_width + 42.0f;
  };
  chip(model.power_label, {0.40f, 1.0f, 0.58f, 1.0f});
  chip(model.terminal_label, model.terminal_hacked ? aster::UiColor{0.46f, 1.0f, 0.82f, 1.0f}
                                                   : aster::UiColor{0.95f, 0.78f, 0.42f, 1.0f});
  chip(model.exit_label, model.victory || model.door_open ? aster::UiColor{0.46f, 1.0f, 0.62f, 1.0f}
                                                          : aster::UiColor{1.0f, 0.34f, 0.40f, 1.0f});
  canvas.text(model.status_line, {static_cast<float>(width) - 318.0f, 20.0f},
              {0.72f, 0.88f, 0.92f, 1.0f}, 1.06f);
  canvas.progressBar({static_cast<float>(width) - 318.0f, 50.0f, 286.0f, 12.0f},
                     model.overload_fraction, {0.35f, 0.95f, 0.55f, 0.95f},
                     {0.40f, 0.06f, 0.08f, 0.95f});
  if (!model.callout_line.empty() && !(model.victory || model.defeated)) {
    const float callout_width = canvas.textWidth(model.callout_line, 1.45f);
    canvas.fillRect({(static_cast<float>(width) - callout_width) * 0.5f - 16.0f,
                     static_cast<float>(height) - 78.0f, callout_width + 32.0f, 34.0f},
                    {0.020f, 0.045f, 0.040f, 0.60f});
    canvas.text(model.callout_line, {(static_cast<float>(width) - callout_width) * 0.5f,
                                     static_cast<float>(height) - 70.0f},
                {0.42f, 1.0f, 0.68f, 1.0f}, 1.45f);
  }
  if (model.victory || model.defeated) {
    const char *label = model.victory ? "EXTRACTION COMPLETE" : "RUN FAILED";
    const aster::UiColor color =
        model.victory ? aster::UiColor{0.35f, 1.0f, 0.58f, 1.0f}
                      : aster::UiColor{1.0f, 0.22f, 0.28f, 1.0f};
    const float text_width = canvas.textWidth(label, 2.3f);
    canvas.fillRect({0.0f, static_cast<float>(height) * 0.44f - 38.0f,
                     static_cast<float>(width), 100.0f},
                    {0.010f, 0.018f, 0.020f, 0.70f});
    canvas.text(label, {(static_cast<float>(width) - text_width) * 0.5f,
                        static_cast<float>(height) * 0.44f},
                color, 2.3f);
    const char *restart = "R / SPACE TO RESTART";
    const float restart_width = canvas.textWidth(restart, 1.15f);
    canvas.text(restart, {(static_cast<float>(width) - restart_width) * 0.5f,
                          static_cast<float>(height) * 0.44f + 38.0f},
                {0.82f, 0.92f, 0.90f, 1.0f}, 1.15f);
  }
  canvas.endFrame();
}

std::filesystem::path framePath(const std::filesystem::path &directory, const int frame) {
  std::ostringstream name;
  name << "frame_";
  name.width(4);
  name.fill('0');
  name << frame << ".ppm";
  return directory / name.str();
}

void writeCapture(const std::filesystem::path &path, const int width, const int height) {
  if (path.extension() == ".png") {
    aster::writeFramebufferPng(path, width, height);
  } else {
    aster::writeFramebufferPpm(path, width, height);
  }
}

bool runReplaySelfTest(const std::uint32_t seed, const int ticks) {
  aster::AsterGridTactics local({.seed = seed});
  aster::AsterGridTactics remote({.seed = seed});
  aster::net::LockstepCommandChannel outgoing(77u);
  aster::net::LockstepCommandChannel incoming(77u);
  for (int tick = 0; tick < ticks; ++tick) {
    const aster::SimCommand command = local.scriptedCommand(static_cast<std::uint32_t>(tick));
    outgoing.pushLocal(command);
    const aster::net::NetMessage message = outgoing.buildMessage(1u, 2u, 1u);
    if (!incoming.receive(message)) {
      return false;
    }
    const std::optional<aster::SimCommand> received =
        incoming.commandForTick(static_cast<std::uint32_t>(tick));
    if (!received.has_value()) {
      return false;
    }
    local.updateFixed(command);
    remote.updateFixed(*received);
    if (local.worldHash() != remote.worldHash()) {
      return false;
    }
  }
  std::cout << "Aster Grid Tactics replay self-test passed: ticks=" << ticks
            << " hash=0x" << std::hex << local.worldHash() << std::dec
            << " checksum=" << local.replay().checksum() << '\n';
  return true;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool replay_self_test = hasArgument(argc, argv, "--replay-self-test");
    const bool smoke_test = hasArgument(argc, argv, "--smoke-test");
    const bool frame_report = hasArgument(argc, argv, "--frame-report");
    const bool no_vsync = hasArgument(argc, argv, "--no-vsync");
    const bool debug_grid = hasArgument(argc, argv, "--debug-grid");
    const std::uint32_t seed = argumentU32(argc, argv, "--seed", 0xA57E2026u);
    const int replay_ticks = argumentInt(argc, argv, "--ticks", 260);
    std::filesystem::path project_path = argumentPath(argc, argv, "--project");
    if (project_path.empty()) {
      project_path = defaultProjectPath();
    }
    if (replay_self_test) {
      return runReplaySelfTest(seed, replay_ticks) ? 0 : 2;
    }

    const std::filesystem::path screenshot = argumentPath(argc, argv, "--screenshot");
    const int screenshot_frame = argumentInt(argc, argv, "--screenshot-frame", 18);
    const std::filesystem::path sequence_dir = argumentPath(argc, argv, "--capture-sequence");
    const int capture_frames = argumentInt(argc, argv, "--capture-frames", 90);
    const int run_frames =
        argumentInt(argc, argv, "--run-frames", smoke_test ? 12 : (screenshot.empty() ? 0 : screenshot_frame + 1));
    const bool scripted = smoke_test || !screenshot.empty() || !sequence_dir.empty();

    aster::EngineConfig config;
    config.application_name = "Aster Grid Tactics";
    config.initial_width = argumentInt(argc, argv, "--window-width", 1600);
    config.initial_height = argumentInt(argc, argv, "--window-height", 900);
    config.multisample_samples = argumentInt(argc, argv, "--msaa", 4);
    config.enable_vsync = !no_vsync && !scripted && !frame_report;

    aster::Window window(config);
    aster::RenderDevice renderer;
    renderer.initialize();
    const std::shared_ptr<aster::MaterialResourceLibrary> material_library =
        loadGridMaterialLibrary(project_path);
    renderer.setMaterialResourceLibrary(material_library);
    aster::UiCanvas hud;
    hud.initialize();

    aster::AsterGridTacticsTuning tuning;
    tuning.seed = seed;
    tuning.debug_grid = debug_grid;
    aster::AsterGridTactics game(tuning);
    renderer.prepareScene(game.scene());
    const aster::ControlScheme control_scheme = makeControls();
    aster::ControlState controls;
    aster::FixedTimestep simulation_clock({.step_seconds = 1.0 / 60.0,
                                           .max_frame_seconds = 1.0 / 20.0,
                                           .max_steps_per_frame = 4u});
    aster::Clock clock;
    aster::OrbitCamera camera = gridCamera();
    aster::RendererSettings settings = gridRendererSettings();
    if (!sequence_dir.empty()) {
      std::filesystem::create_directories(sequence_dir);
    }

    int rendered_frames = 0;
    double elapsed = 0.0;
    double frame_seconds_sum = 0.0;
    double draw_call_sum = 0.0;
    double visible_object_sum = 0.0;
    bool captured = false;
    while (window.isOpen()) {
      window.pollEvents();
      const double dt = scripted ? 1.0 / 60.0 : clock.tick();
      elapsed += dt;
      controls.update(control_scheme, window.captureControls(control_scheme));
      if (controls.justPressed(kQuit)) {
        window.requestClose();
      }
      const bool ended = game.status().outcome != aster::AsterGridTacticsOutcome::Playing;
      if (controls.justPressed(kReset) || (ended && controls.justPressed(kInteract))) {
        game.reset();
        renderer.prepareScene(game.scene());
        simulation_clock.reset();
      }

      const std::size_t steps = simulation_clock.advance(dt);
      for (std::size_t step = 0; step < steps; ++step) {
        const aster::SimCommand command =
            scripted ? game.scriptedCommand(game.status().tick) : commandFromControls(controls);
        game.updateFixed(command);
        renderer.prepareScene(game.scene());
      }

      const auto [width, height] = window.framebufferSize();
      const aster::FrameStats stats =
          renderer.render(game.scene(), camera, settings, width, height, elapsed);
      frame_seconds_sum += stats.frame_seconds;
      draw_call_sum += static_cast<double>(stats.draw_calls);
      visible_object_sum += static_cast<double>(stats.visible_objects);

      const auto [hud_width, hud_height] = window.windowSize();
      drawHud(hud, game.hudModel(), controls.snapshot(), hud_width, hud_height);

      if (!sequence_dir.empty()) {
        aster::writeFramebufferPpm(framePath(sequence_dir, rendered_frames), width, height);
        if (rendered_frames + 1 >= capture_frames) {
          window.requestClose();
        }
      } else if (!screenshot.empty() && !captured && rendered_frames >= screenshot_frame) {
        writeCapture(screenshot, width, height);
        captured = true;
        window.requestClose();
      }

      window.swapBuffers();
      ++rendered_frames;
      if (smoke_test && rendered_frames >= 12) {
        window.requestClose();
      }
      if (run_frames > 0 && rendered_frames >= run_frames) {
        window.requestClose();
      }
    }
    hud.shutdown();
    if (frame_report && rendered_frames > 0) {
      const double frames = static_cast<double>(rendered_frames);
      std::cout << "Aster Grid Tactics frame report: frames=" << rendered_frames
                << " mean_frame_ms=" << (frame_seconds_sum / frames) * 1000.0
                << " visible_objects_mean=" << visible_object_sum / frames
                << " draw_calls_mean=" << draw_call_sum / frames
                << " world_hash=0x" << std::hex << game.worldHash() << std::dec << '\n';
    }
    if (smoke_test) {
      std::cout << "Aster Grid Tactics smoke test passed: frames=" << rendered_frames
                << " world_hash=0x" << std::hex << game.worldHash() << std::dec << '\n';
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Aster Grid Tactics error: " << error.what() << '\n';
    return 1;
  }
}
