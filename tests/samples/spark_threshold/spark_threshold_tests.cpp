// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/spark_threshold/spark_threshold.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

void runScript(aster::SparkThreshold &game, const aster::SparkThresholdScriptRoute route,
               const int max_ticks = 2400) {
  game.startRun();
  for (int tick = 0; tick < max_ticks &&
                     game.status().outcome == aster::SparkThresholdOutcome::Playing;
       ++tick) {
    game.updateFixed(game.scriptedCommand(route));
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

void testScriptedRunReachesResonanceEnding() {
  aster::SparkThreshold game({.title_screen = false});
  runScript(game, aster::SparkThresholdScriptRoute::Resonance);
  assert(game.status().outcome == aster::SparkThresholdOutcome::ResonanceEnding);
  assert(game.status().activated_nodes == 3);
  assert(game.status().mine_active);
  assert(game.status().scrap_active);
  assert(game.status().castle_active);
  assert(game.hudModel().ending_title == "Rezonans");
  assert(game.replay().checksum() != 0u);
  assert(game.worldHash() != 0u);
}

void testScriptedRunReachesEscapeEnding() {
  aster::SparkThreshold game({.title_screen = false});
  runScript(game, aster::SparkThresholdScriptRoute::Escape);
  assert(game.status().outcome == aster::SparkThresholdOutcome::EscapeEnding);
  assert(game.status().activated_nodes == 2);
  assert(game.status().mine_active);
  assert(!game.status().scrap_active);
  assert(game.status().castle_active);
  assert(game.hudModel().ending_title == "Kacis");
}

void testScriptedRunReachesOverloadEnding() {
  aster::SparkThreshold game({.title_screen = false});
  runScript(game, aster::SparkThresholdScriptRoute::Overload);
  assert(game.status().outcome == aster::SparkThresholdOutcome::OverloadEnding);
  assert(game.status().activated_nodes == 3);
  assert(game.status().scrap_active);
  assert(game.status().first_node == aster::SparkThresholdNode::Scrap);
  assert(game.hudModel().ending_title == "Asiri Yuk");
}

void testResetRestoresTitleAndPlayableState() {
  aster::SparkThreshold game;
  assert(game.status().title_screen);
  game.startRun();
  for (int i = 0; i < 24; ++i) {
    game.updateFixed(game.scriptedCommand(aster::SparkThresholdScriptRoute::Escape));
  }
  assert(!game.status().title_screen);
  game.reset();
  assert(game.status().title_screen);
  assert(game.status().outcome == aster::SparkThresholdOutcome::Playing);
  assert(game.status().activated_nodes == 0);
  assert(game.status().tick == 0u);
  assert(game.hudModel().title == "Kivilcim Esigi");
}

void testHudUsesTurkishPlayerFacingLanguage() {
  aster::SparkThreshold game;
  const aster::SparkThresholdHudModel title = game.hudModel();
  assert(title.title == "Kivilcim Esigi");
  assert(title.guide_body.find("Kivilcim") != std::string::npos);
  assert(title.objective.find("POWER") == std::string::npos);
  assert(title.status_line.find("HASH") == std::string::npos);

  runScript(game, aster::SparkThresholdScriptRoute::Escape);
  const aster::SparkThresholdHudModel end = game.hudModel();
  assert(end.ending_title == "Kacis");
  assert(end.ending_body.find("gecitten") != std::string::npos);
  assert(end.status_line.find("TICK") == std::string::npos);
}

void testSceneContainsVisualSignalsAndRenders() {
  aster::SparkThreshold game({.title_screen = false});
  game.startRun();
  for (int i = 0; i < 180; ++i) {
    game.updateFixed(game.scriptedCommand(aster::SparkThresholdScriptRoute::Resonance));
  }
  assert(hasObjectNamed(game.scene(), "Spark Threshold Operator"));
  assert(hasObjectNamed(game.scene(), "Neon mine gallery wet basalt floor"));
  assert(hasObjectNamed(game.scene(), "Scrap energy yard oxidized deck"));
  assert(hasObjectNamed(game.scene(), "Ancient castle threshold stone dais"));
  assert(hasObjectPrefix(game.scene(), "Mine Energy Node"));
  assert(hasObjectPrefix(game.scene(), "Scrap Dynamo"));
  assert(hasObjectPrefix(game.scene(), "Castle Prism"));
  assert(hasObjectNamed(game.scene(), "Dormant Final Gate Ring") ||
         hasObjectNamed(game.scene(), "Final Threshold Gate Ring"));
  assert(!game.scene().reflectionProbes().empty());

  aster::OrbitCamera camera;
  camera.target = game.cameraTarget();
  camera.yaw = 0.0f;
  camera.pitch = aster::radians(52.0f);
  camera.radius = 12.8f;
  camera.vertical_fov = aster::radians(46.0f);
  aster::RendererSettings settings;
  settings.exposure = 1.1f;
  settings.ambient_strength = 0.28f;
  settings.pipeline.clear_color = {0.004f, 0.006f, 0.010f};
  settings.post.bloom = true;
  settings.procedural_surface_normals = true;
  const aster::SoftwareFrameBuffer frame =
      aster::renderSoftwarePreview(game.scene(), camera,
                                   {.width = 160, .height = 100, .settings = settings});
  bool nonblank = false;
  for (std::size_t i = 0u; i + 3u < frame.rgba8().size(); i += 4u) {
    nonblank = nonblank || frame.rgba8()[i + 0u] > 14u ||
               frame.rgba8()[i + 1u] > 14u || frame.rgba8()[i + 2u] > 14u;
  }
  assert(nonblank);
}

void testDeterministicReplayHashesMatch() {
  aster::SparkThreshold first({.seed = 144u, .title_screen = false});
  aster::SparkThreshold second({.seed = 144u, .title_screen = false});
  runScript(first, aster::SparkThresholdScriptRoute::Resonance, 1600);
  runScript(second, aster::SparkThresholdScriptRoute::Resonance, 1600);
  assert(first.status().outcome == second.status().outcome);
  assert(first.worldHash() == second.worldHash());
  assert(first.replay().checksum() == second.replay().checksum());
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
      {"scripted_run_reaches_resonance_ending", testScriptedRunReachesResonanceEnding},
      {"scripted_run_reaches_escape_ending", testScriptedRunReachesEscapeEnding},
      {"scripted_run_reaches_overload_ending", testScriptedRunReachesOverloadEnding},
      {"reset_restores_title_and_playable_state", testResetRestoresTitleAndPlayableState},
      {"hud_uses_turkish_player_facing_language", testHudUsesTurkishPlayerFacingLanguage},
      {"scene_contains_visual_signals_and_renders", testSceneContainsVisualSignalsAndRenders},
      {"deterministic_replay_hashes_match", testDeterministicReplayHashesMatch},
  };
  bool ran = false;
  for (const NamedTest &test : tests) {
    if (selected(test.name, argc, argv)) {
      test.run();
      ran = true;
    }
  }
  assert(ran);
  std::cout << "spark_threshold_tests passed.\n";
  return 0;
}
