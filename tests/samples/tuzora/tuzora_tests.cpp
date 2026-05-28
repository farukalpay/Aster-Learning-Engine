// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/net/lockstep_command_channel.hpp"
#include "aster/render/camera2d.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/tuzora/tuzora.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

void runScript(aster::Tuzora &game, const int max_ticks) {
  for (int tick = 0; tick < max_ticks && game.status().outcome == aster::TuzoraOutcome::Playing;
       ++tick) {
    game.updateFixed(game.scriptedCommand(game.status().tick));
  }
}

bool hasObjectNamed(const aster::Scene &scene, const std::string_view name) {
  const auto &objects = scene.objects();
  return std::any_of(objects.begin(), objects.end(),
                     [&](const aster::RenderObject &object) { return object.name == name; });
}

bool hasObjectPrefix(const aster::Scene &scene, const std::string_view prefix) {
  const auto &objects = scene.objects();
  return std::any_of(objects.begin(), objects.end(), [&](const aster::RenderObject &object) {
    return object.name.rfind(std::string(prefix), 0u) == 0u;
  });
}

bool framebufferNonBlank(const aster::SoftwareFrameBuffer &frame) {
  bool nonblank = false;
  for (std::size_t i = 0u; i + 3u < frame.rgba8().size(); i += 4u) {
    nonblank = nonblank || frame.rgba8()[i + 0u] > 16u || frame.rgba8()[i + 1u] > 16u ||
               frame.rgba8()[i + 2u] > 16u;
  }
  return nonblank;
}

aster::SimCommand moveRightCommand(const std::uint32_t tick, const bool jump = false) {
  aster::SimCommand command;
  command.tick = tick;
  command.strafe = 32767;
  command.set(aster::SimCommandButton::Run, true);
  command.set(aster::SimCommandButton::Jump, jump);
  return command;
}

void testDeterministicMovementAndCollision() {
  aster::Tuzora first({.seed = 41u});
  aster::Tuzora second({.seed = 41u});
  const float start_x = first.status().player.x;
  for (int i = 0; i < 96; ++i) {
    const aster::SimCommand command = moveRightCommand(first.status().tick, i == 4);
    first.updateFixed(command);
    second.updateFixed(command);
  }
  assert(first.worldHash() == second.worldHash());
  assert(first.status().player.x > start_x + 1.0f);
  assert(first.status().player.y > 1.0f);
  assert(first.status().player.y < 8.0f);
}

void testScriptedRunLightsSignal() {
  aster::Tuzora game;
  runScript(game, 2200);
  assert(game.status().outcome == aster::TuzoraOutcome::SignalLit);
  assert(game.status().signal_lit);
  assert(game.status().pickups_collected >= 5);
  assert(game.status().signal_parts_installed >= 2);
  assert(game.status().lamps_placed >= 1);
  assert(game.status().blocks_broken >= 1);
  assert(game.status().blocks_placed >= 1);
  assert(game.replay().checksum() != 0u);
  assert(game.worldHash() != 0u);
}

void testDeterministicReplayHashesMatch() {
  aster::Tuzora first({.seed = 77u});
  aster::Tuzora second({.seed = 77u});
  runScript(first, 900);
  runScript(second, 900);
  assert(first.worldHash() == second.worldHash());
  assert(first.replay().checksum() == second.replay().checksum());
}

void testLockstepCommandChannelReplaysScript() {
  aster::Tuzora local({.seed = 91u});
  aster::Tuzora remote({.seed = 91u});
  aster::net::LockstepCommandChannel outgoing(91u);
  aster::net::LockstepCommandChannel incoming(91u);
  for (int step = 0; step < 900 && local.status().outcome == aster::TuzoraOutcome::Playing;
       ++step) {
    const aster::SimCommand command = local.scriptedCommand(local.status().tick);
    const std::uint32_t command_tick = command.tick;
    outgoing.pushLocal(command);
    assert(incoming.receive(outgoing.buildMessage(1u, 2u, 1u)));
    const std::optional<aster::SimCommand> received = incoming.commandForTick(command_tick);
    assert(received.has_value());
    local.updateFixed(command);
    remote.updateFixed(*received);
  }
  assert(local.worldHash() == remote.worldHash());
  assert(incoming.nextExpectedTick() == local.status().tick);
}

void testBreakPlaceAndObjectiveCompletion() {
  aster::Tuzora game;
  runScript(game, 1700);
  assert(game.status().blocks_broken >= 1);
  assert(game.status().blocks_placed >= 1);
  assert(game.status().signal_parts_installed >= 2);
  assert(game.status().signal_lit);
  assert(game.hudModel().status_line == "SIGNAL LIT");
}

void testCaptureSceneContainsRequiredObjectsAndRenders() {
  aster::Tuzora game;
  runScript(game, 2200);
  assert(hasObjectPrefix(game.scene(), "Tuzora player"));
  assert(hasObjectNamed(game.scene(), "Tuzora sea parallax band"));
  assert(hasObjectNamed(game.scene(), "Tuzora apartment sunwashed blocks"));
  assert(hasObjectNamed(game.scene(), "Tuzora lamp glow placed"));
  assert(hasObjectPrefix(game.scene(), "Tuzora placed block"));
  assert(hasObjectNamed(game.scene(), "Tuzora rooftop signal lit"));

  aster::Camera2DConfig camera_config;
  camera_config.min_target = {5.0f, 2.7f};
  camera_config.max_target = {46.0f, 9.0f};
  camera_config.orthographic_height = 12.0f;
  camera_config.distance = 28.0f;
  aster::Camera2DState camera_state;
  aster::resetCamera2D(camera_state, game.cameraTarget());
  aster::RendererSettings settings;
  settings.exposure = 1.2f;
  settings.ambient_strength = 0.8f;
  settings.pipeline.clear_color = {0.26f, 0.42f, 0.58f};
  settings.pipeline.multisampling = false;
  const aster::SoftwareFrameBuffer frame = aster::renderSoftwarePreview(
      game.scene(), aster::makeCamera2DOrbit(camera_state, camera_config),
      {.width = 180, .height = 112, .settings = settings});
  assert(framebufferNonBlank(frame));
}

void testHudUsesPlayerFacingLanguage() {
  aster::Tuzora game;
  const aster::TuzoraHudModel start = game.hudModel();
  assert(start.title == "TUZORA");
  assert(start.objective == "Light the rooftop signal before night.");
  assert(start.status_line.find("TICK") == std::string::npos);
  assert(start.status_line.find("HASH") == std::string::npos);
  assert(start.resource_line.find("shell") != std::string::npos);
  assert(start.hotbar_line.find("sand") != std::string::npos);

  runScript(game, 2200);
  const aster::TuzoraHudModel end = game.hudModel();
  assert(end.signal_lit);
  assert(end.status_line == "SIGNAL LIT");
  assert(end.objective == "Signal is alive above the roofs.");
}

void testResetRestoresPlayableState() {
  aster::Tuzora game({.day_ticks = 1});
  game.updateFixed(game.scriptedCommand(game.status().tick));
  game.updateFixed(game.scriptedCommand(game.status().tick));
  assert(game.status().outcome == aster::TuzoraOutcome::Nightfall);
  game.reset();
  assert(game.status().outcome == aster::TuzoraOutcome::Playing);
  assert(game.status().tick == 0u);
  assert(game.status().pickups_collected == 0);
  assert(game.status().blocks_placed == 0);
  assert(game.status().blocks_broken == 0);
  assert(!game.status().signal_lit);
}

struct NamedTest {
  const char *name = "";
  void (*run)() = nullptr;
};

bool selected(const char *name, const int argc, const char **argv) {
  if (argc <= 1) {
    return true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], name) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(const int argc, const char **argv) {
  const NamedTest tests[] = {
      {"deterministic_movement_and_collision", testDeterministicMovementAndCollision},
      {"scripted_run_lights_signal", testScriptedRunLightsSignal},
      {"deterministic_replay_hashes_match", testDeterministicReplayHashesMatch},
      {"lockstep_command_channel_replays_script", testLockstepCommandChannelReplaysScript},
      {"break_place_and_objective_completion", testBreakPlaceAndObjectiveCompletion},
      {"capture_scene_contains_required_objects_and_renders",
       testCaptureSceneContainsRequiredObjectsAndRenders},
      {"hud_uses_player_facing_language", testHudUsesPlayerFacingLanguage},
      {"reset_restores_playable_state", testResetRestoresPlayableState},
  };
  bool ran = false;
  for (const NamedTest &test : tests) {
    if (selected(test.name, argc, argv)) {
      test.run();
      ran = true;
    }
  }
  assert(ran);
  std::cout << "aster_tuzora_tests passed.\n";
  return 0;
}
