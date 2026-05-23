// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/net/lockstep_command_channel.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/aster_grid_tactics/aster_grid_tactics.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

void runScript(aster::AsterGridTactics &game, const int ticks) {
  for (int tick = 0; tick < ticks; ++tick) {
    game.updateFixed(game.scriptedCommand(game.status().tick));
  }
}

bool hasObjectNamed(const aster::Scene &scene, const std::string_view name) {
  const auto &objects = scene.objects();
  return std::any_of(objects.begin(), objects.end(), [&](const aster::RenderObject &object) {
    return object.name == name;
  });
}

bool hasObjectPrefix(const aster::Scene &scene, const std::string_view prefix) {
  const auto &objects = scene.objects();
  return std::any_of(objects.begin(), objects.end(), [&](const aster::RenderObject &object) {
    return object.name.rfind(std::string(prefix), 0u) == 0u;
  });
}

bool hasCustomMeshObject(const aster::Scene &scene, const std::string_view name) {
  const auto &objects = scene.objects();
  return std::any_of(objects.begin(), objects.end(), [&](const aster::RenderObject &object) {
    return object.name == name && object.custom_mesh != nullptr;
  });
}

void testScriptedRunReachesVictory() {
  aster::AsterGridTactics game;
  runScript(game, 260);
  assert(game.status().outcome == aster::AsterGridTacticsOutcome::Victory);
  assert(game.status().energy_nodes_rerouted == game.status().total_energy_nodes);
  assert(game.status().terminals_hacked == 1);
  assert(game.status().door_open);
  assert(game.status().turret_disabled);
  assert(game.status().visual_event == aster::AsterGridTacticsVisualEvent::ExtractionComplete);
  assert(game.hudModel().callout_line == "EXTRACTION BLOOM");
  assert(game.replay().checksum() != 0u);
  assert(game.worldHash() != 0u);
}

void testDeterministicReplayHashesMatch() {
  aster::AsterGridTactics first({.seed = 77u});
  aster::AsterGridTactics second({.seed = 77u});
  runScript(first, 180);
  runScript(second, 180);
  assert(first.worldHash() == second.worldHash());
  assert(first.replay().checksum() == second.replay().checksum());
}

void testLockstepCommandChannelReplaysScript() {
  aster::AsterGridTactics local({.seed = 91u});
  aster::AsterGridTactics remote({.seed = 91u});
  aster::net::LockstepCommandChannel outgoing(77u);
  aster::net::LockstepCommandChannel incoming(77u);
  for (int step = 0; step < 180 &&
                     local.status().outcome == aster::AsterGridTacticsOutcome::Playing;
       ++step) {
    const aster::SimCommand command = local.scriptedCommand(local.status().tick);
    const std::uint32_t command_tick = command.tick;
    outgoing.pushLocal(command);
    assert(incoming.receive(outgoing.buildMessage(1u, 2u, 1u)));
    const auto received = incoming.commandForTick(command_tick);
    assert(received.has_value());
    local.updateFixed(command);
    remote.updateFixed(*received);
  }
  assert(local.worldHash() == remote.worldHash());
  assert(incoming.nextExpectedTick() == local.status().tick);
}

void testSceneContainsGameplaySignalsAndRenders() {
  aster::AsterGridTactics game;
  runScript(game, 90);
  assert(hasObjectNamed(game.scene(), "Operator Hull"));
  assert(hasObjectNamed(game.scene(), "Guard Drone Hull"));
  assert(hasObjectNamed(game.scene(), "Guard vision cone"));
  assert(hasObjectNamed(game.scene(), "Terminal Console Screen"));
  assert(hasObjectNamed(game.scene(), "Energy Node Ring"));
  assert(hasObjectNamed(game.scene(), "Routed energy conduit"));
  assert(hasObjectNamed(game.scene(), "Extraction Gate Ring") ||
         hasObjectNamed(game.scene(), "Sealed Extraction Gate Ring"));
  assert(hasObjectNamed(game.scene(), "Security Turret Base") ||
         hasObjectNamed(game.scene(), "Security Turret Disabled Base"));
  assert(hasCustomMeshObject(game.scene(), "Operator Hull"));
  assert(hasCustomMeshObject(game.scene(), "Guard Drone Hull"));
  assert(!hasObjectPrefix(game.scene(), "Debug floor grid"));

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = aster::radians(88.0f);
  camera.radius = 19.0f;
  camera.projection_mode = aster::CameraProjectionMode::Orthographic;
  camera.orthographic_height = 13.2f;
  aster::RendererSettings settings;
  settings.exposure = 1.2f;
  settings.ambient_strength = 0.7f;
  settings.pipeline.clear_color = {0.01f, 0.02f, 0.03f};
  const aster::SoftwareFrameBuffer frame =
      aster::renderSoftwarePreview(game.scene(), camera,
                                   {.width = 160, .height = 100, .settings = settings});
  bool nonblank = false;
  for (std::size_t i = 0u; i + 3u < frame.rgba8().size(); i += 4u) {
    nonblank = nonblank || frame.rgba8()[i + 0u] > 16u || frame.rgba8()[i + 1u] > 16u ||
               frame.rgba8()[i + 2u] > 16u;
  }
  assert(nonblank);
}

void testDebugGridIsOptIn() {
  aster::AsterGridTactics clean;
  assert(!hasObjectPrefix(clean.scene(), "Debug floor grid"));

  aster::AsterGridTacticsTuning tuning;
  tuning.debug_grid = true;
  aster::AsterGridTactics debug(tuning);
  assert(hasObjectPrefix(debug.scene(), "Debug floor grid"));
}

void testHudUsesPlayerFacingLanguage() {
  aster::AsterGridTactics game;
  const aster::AsterGridTacticsHudModel start = game.hudModel();
  assert(start.power_label == "POWER 0/2");
  assert(start.terminal_label == "TERMINAL LOCKED");
  assert(start.exit_label == "EXIT SEALED");
  assert(start.status_line.find("TICK") == std::string::npos);
  assert(start.status_line.find("HASH") == std::string::npos);

  runScript(game, 260);
  const aster::AsterGridTacticsHudModel end = game.hudModel();
  assert(end.victory);
  assert(end.power_label == "POWER ONLINE");
  assert(end.terminal_label == "TERMINAL BREACHED");
  assert(end.exit_label == "EXTRACTED");
  assert(end.status_line == "SIGNAL CLEAN");
}

void testResetRestoresPlayableState() {
  aster::AsterGridTactics game({.overload_ticks = 1});
  game.updateFixed(game.scriptedCommand(game.status().tick));
  assert(game.status().outcome == aster::AsterGridTacticsOutcome::Defeated);
  game.reset();
  assert(game.status().outcome == aster::AsterGridTacticsOutcome::Playing);
  assert(game.status().tick == 0u);
  assert(game.status().energy_nodes_rerouted == 0);
  assert(game.status().terminals_hacked == 0);
  assert(game.status().overload_ticks_remaining == 1);
  assert(game.status().visual_event == aster::AsterGridTacticsVisualEvent::None);
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
      {"scripted_run_reaches_victory", testScriptedRunReachesVictory},
      {"deterministic_replay_hashes_match", testDeterministicReplayHashesMatch},
      {"lockstep_command_channel_replays_script", testLockstepCommandChannelReplaysScript},
      {"scene_contains_gameplay_signals_and_renders", testSceneContainsGameplaySignalsAndRenders},
      {"debug_grid_is_opt_in", testDebugGridIsOptIn},
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
  std::cout << "aster_grid_tactics_tests passed.\n";
  return 0;
}
