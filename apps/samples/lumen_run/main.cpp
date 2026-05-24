// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/core/profiler.hpp"
#include "aster/game_sdk/game_sdk.hpp"
#include "aster/samples/lumen_run/lumen_run.hpp"
#include "aster/systems/third_person_camera.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/math/hash.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/render/visual_regression.hpp"
#include "aster/ui/hud_layer.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char *kPause = "ui.pause";
constexpr const char *kReset = "run.reset";
constexpr const char *kMoveLeft = "runner.move.left";
constexpr const char *kMoveRight = "runner.move.right";
constexpr const char *kMoveUp = "runner.move.up";
constexpr const char *kMoveDown = "runner.move.down";
constexpr const char *kJump = "runner.jump";
constexpr const char *kRun = "runner.run";
constexpr const char *kInventory = "ui.inventory.toggle";
constexpr const char *kInventoryRotate = "ui.inventory.rotate";
constexpr const char *kInventoryRecenter = "ui.inventory.recenter";
constexpr const char *kCommandAim = "runner.command.aim";
constexpr const char *kCameraOrbit = "camera.orbit";
constexpr const char *kInteract = "world.interact";
constexpr const char *kSecondaryInteract = "world.interact.secondary";
constexpr const char *kHotbar1 = "hotbar.slot.1";
constexpr const char *kHotbar2 = "hotbar.slot.2";
constexpr const char *kHotbar3 = "hotbar.slot.3";
constexpr const char *kHotbar4 = "hotbar.slot.4";
constexpr const char *kHotbar5 = "hotbar.slot.5";
constexpr const char *kHotbar6 = "hotbar.slot.6";
constexpr const char *kCaveDebugOverlayToggle = "debug.cave_overlay.toggle";
constexpr const char *kCaveDebugOverlayCollision = "debug.cave_overlay.collision";
constexpr const char *kCaveDebugOverlayInteractable = "debug.cave_overlay.interactable";
constexpr const char *kCaveDebugOverlayMining = "debug.cave_overlay.mining";
constexpr const char *kCaveDebugOverlaySpawn = "debug.cave_overlay.spawn";
constexpr const char *kCaveDebugOverlayCamera = "debug.cave_overlay.camera";
constexpr const char *kCaveDebugOverlayWalkable = "debug.cave_overlay.walkable";
constexpr int kInteractiveWindowWidth = 1280;
constexpr int kInteractiveWindowHeight = 720;
constexpr double kSimulationStepSeconds = 1.0 / 60.0;
constexpr double kMaxSimulatedFrameSeconds = 1.0 / 20.0;
constexpr double kDefaultInteractiveFrameCapSeconds = 1.0 / 60.0;
constexpr double kFramePacingSchedulerGuardSeconds = 0.0012;
constexpr double kFramePacingYieldThresholdSeconds = 0.00018;
constexpr std::size_t kMaxSimulationStepsPerFrame = 4;
constexpr float kGameplayCameraYaw = 0.0f;
constexpr float kDeepCaveStressStartProgress = 10.0f;
constexpr float kDeepCaveStressMetersPerSecond = 10.0f;
const float kGameplayCameraPitch = aster::radians(22.0f);

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

std::filesystem::path defaultProjectPath() {
#if defined(ASTER_LUMEN_RUN_PROJECT)
  return ASTER_LUMEN_RUN_PROJECT;
#else
  return {};
#endif
}

const aster::sdk::ProjectAssetRef *
findProjectAsset(const aster::sdk::ProjectDocument &project, const std::string_view id) {
  const auto asset = std::find_if(project.assets.begin(), project.assets.end(),
                                  [id](const aster::sdk::ProjectAssetRef &candidate) {
                                    return candidate.id == id;
                                  });
  return asset == project.assets.end() ? nullptr : &*asset;
}

const aster::sdk::ProjectAssetRef *
findProjectAssetByKind(const aster::sdk::ProjectDocument &project, const aster::sdk::AssetKind kind) {
  const auto asset = std::find_if(project.assets.begin(), project.assets.end(),
                                  [kind](const aster::sdk::ProjectAssetRef &candidate) {
                                    return candidate.kind == kind;
                                  });
  return asset == project.assets.end() ? nullptr : &*asset;
}

std::filesystem::path projectAssetPath(const std::filesystem::path &project_path,
                                       const aster::sdk::ProjectAssetRef &asset) {
  const std::filesystem::path root = project_path.parent_path();
  return asset.path.is_absolute() ? asset.path : root / asset.path;
}

void printAuthoringDiagnostics(const std::vector<aster::sdk::Diagnostic> &diagnostics) {
  for (const aster::sdk::Diagnostic &diagnostic : diagnostics) {
    const char *severity =
        diagnostic.severity == aster::sdk::DiagnosticSeverity::Error ? "error" : "warning";
    std::cerr << "Lumen Run project " << severity << ": ";
    if (!diagnostic.source.empty()) {
      std::cerr << diagnostic.source.string();
    }
    if (!diagnostic.path.empty()) {
      std::cerr << " " << diagnostic.path;
    }
    std::cerr << ": " << diagnostic.message << '\n';
  }
}

void appendAuthoringDiagnostics(std::vector<aster::sdk::Diagnostic> &out,
                                std::vector<aster::sdk::Diagnostic> diagnostics) {
  out.insert(out.end(), std::make_move_iterator(diagnostics.begin()),
             std::make_move_iterator(diagnostics.end()));
}

bool hasAuthoringErrors(const std::vector<aster::sdk::Diagnostic> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const aster::sdk::Diagnostic &diagnostic) {
                       return diagnostic.severity == aster::sdk::DiagnosticSeverity::Error;
                     });
}

aster::LumenAuthoringData loadLumenAuthoring(const std::filesystem::path &project_path,
                                             std::vector<aster::sdk::Diagnostic> &diagnostics) {
  aster::LumenAuthoringData authoring;
  if (project_path.empty()) {
    return authoring;
  }

  const aster::sdk::LoadResult<aster::sdk::ProjectDocument> project =
      aster::sdk::loadProjectDocument(project_path);
  appendAuthoringDiagnostics(diagnostics, project.diagnostics);
  if (!project.ok()) {
    return authoring;
  }
  authoring.project = project.value;

  const aster::sdk::ProjectAssetRef *scene_asset =
      findProjectAsset(authoring.project, authoring.project.startup_scene);
  if (scene_asset != nullptr) {
    const auto scene = aster::sdk::loadSceneDocument(projectAssetPath(project_path, *scene_asset));
    appendAuthoringDiagnostics(diagnostics, scene.diagnostics);
    if (scene.ok()) {
      authoring.scene = scene.value;
    }
  }

  const aster::sdk::ProjectAssetRef *cave_asset =
      findProjectAssetByKind(authoring.project, aster::sdk::AssetKind::Cave);
  if (cave_asset != nullptr) {
    const std::filesystem::path cave_path = projectAssetPath(project_path, *cave_asset);
    const auto cave = aster::sdk::loadCaveDocument(cave_path);
    appendAuthoringDiagnostics(diagnostics, cave.diagnostics);
    if (cave.ok()) {
      authoring.cave = cave.value;
      appendAuthoringDiagnostics(
          diagnostics,
          aster::sdk::validateCaveDocument(authoring.cave, &authoring.project,
                                           authoring.scene.id.empty() ? nullptr : &authoring.scene,
                                           cave_path));
    }
  }

  authoring.valid = !authoring.project.name.empty() && !authoring.scene.id.empty() &&
                    !authoring.cave.id.empty() && !hasAuthoringErrors(diagnostics);
  return authoring;
}

std::uint32_t caveOverlayAllLayerMask() {
  return static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::Collision) |
         static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::Interactable) |
         static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::MiningTarget) |
         static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::SpawnVolume) |
         static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::CameraObstruction) |
         static_cast<std::uint32_t>(aster::CaveDebugOverlayLayer::Walkable);
}

void toggleCaveOverlayLayer(aster::LumenRun &game, const aster::CaveDebugOverlayLayer layer) {
  const std::uint32_t bit = static_cast<std::uint32_t>(layer);
  std::uint32_t mask = game.caveDebugOverlayLayerMask();
  mask = (mask & bit) != 0u ? (mask & ~bit) : (mask | bit);
  game.setCaveDebugOverlayLayerMask(mask);
}

std::string argumentString(const int argc, char **argv, const std::string_view name,
                           std::string fallback = {}) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return fallback;
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

void sleepForFrameCap(const aster::Clock &clock, const double frame_start_seconds,
                      const double target_frame_seconds) {
  if (target_frame_seconds <= 0.0) {
    return;
  }
  const double elapsed = clock.now() - frame_start_seconds;
  const double remaining = target_frame_seconds - elapsed;
  if (remaining > kFramePacingSchedulerGuardSeconds + kFramePacingYieldThresholdSeconds) {
    std::this_thread::sleep_for(
        std::chrono::duration<double>(remaining - kFramePacingSchedulerGuardSeconds));
  }
  while (target_frame_seconds - (clock.now() - frame_start_seconds) >
         kFramePacingYieldThresholdSeconds) {
    std::this_thread::yield();
  }
}

aster::Vec3 mixVec(const aster::Vec3 a, const aster::Vec3 b, const float t) {
  const float amount = std::clamp(t, 0.0f, 1.0f);
  return a + (b - a) * amount;
}

struct RenderEnvironmentBaseline {
  aster::LightRig light_rig{};
  aster::DirectionalLight sun_light{};
  float ambient_strength = 0.0f;
  float ambient_floor = 0.0f;
  aster::Vec3 sky_ambient_color{};
  aster::Vec3 ground_ambient_color{};
  aster::AtmosphereSettings atmosphere{};
  aster::Vec3 clear_color{};
  float exposure = 1.0f;
  float indirect_albedo_floor = 0.035f;
};

float caveCameraLookBlend(const aster::CaveLightingState &light) {
  return std::clamp(light.interior + light.entrance_light * 0.35f, 0.0f, 1.0f);
}

aster::ThirdPersonFollowSettings cameraFollowSettings(const float target_response,
                                                      const float cave_look_blend) {
  const float blend = std::clamp(cave_look_blend, 0.0f, 1.0f);
  return {.yaw_sensitivity = 0.0038f,
          .pitch_sensitivity = 0.0032f,
          .min_pitch = std::lerp(aster::radians(10.0f), aster::radians(-14.0f), blend),
          .max_pitch = std::lerp(aster::radians(54.0f), aster::radians(74.0f), blend),
          .target_response = target_response};
}

void restoreRenderEnvironment(aster::RendererSettings &settings,
                              const RenderEnvironmentBaseline &baseline) {
  settings.light_rig = baseline.light_rig;
  settings.sun_light = baseline.sun_light;
  settings.ambient_strength = baseline.ambient_strength;
  settings.ambient_floor = baseline.ambient_floor;
  settings.sky_ambient_color = baseline.sky_ambient_color;
  settings.ground_ambient_color = baseline.ground_ambient_color;
  settings.atmosphere = baseline.atmosphere;
  settings.pipeline.clear_color = baseline.clear_color;
  settings.exposure = baseline.exposure;
  settings.indirect_albedo_floor = baseline.indirect_albedo_floor;
}

void applyCaveRenderEnvironment(aster::RendererSettings &settings,
                                const RenderEnvironmentBaseline &baseline,
                                const aster::CaveLightingState &light) {
  if (light.interior <= 0.001f) {
    return;
  }

  const float interior = std::clamp(light.interior, 0.0f, 1.0f);
  const float cave_depth = std::clamp(light.depth, 0.0f, 1.0f);
  const float chamber_fill = std::clamp(light.chamber, 0.0f, 1.0f);
  const float source_fill = std::clamp(light.wall_light, 0.0f, 1.0f);
  const aster::Vec3 warm_source = {light.wall_light_color.x * 0.058f,
                                   light.wall_light_color.y * 0.061f,
                                   light.wall_light_color.z * 0.052f};
  const aster::Vec3 cave_source_ambient =
      mixVec({0.012f, 0.010f, 0.009f}, warm_source, 0.18f + source_fill * 0.34f);
  const aster::Vec3 cave_sky_tint =
      mixVec({0.007f, 0.007f, 0.008f}, cave_source_ambient, 0.14f + source_fill * 0.24f);
  const aster::Vec3 cave_ground_tint =
      mixVec({0.009f, 0.008f, 0.007f}, cave_source_ambient * 0.56f, 0.10f + source_fill * 0.20f);
  const aster::Vec3 cave_air_tint =
      mixVec({0.014f, 0.013f, 0.012f}, light.wall_light_color * 0.034f, source_fill * 0.22f);

  settings.ambient_strength =
      std::lerp(baseline.ambient_strength,
                0.034f + source_fill * 0.018f + chamber_fill * 0.008f - cave_depth * 0.012f,
                interior);
  settings.ambient_floor =
      std::lerp(baseline.ambient_floor, 0.005f + source_fill * 0.003f, interior);
  settings.indirect_albedo_floor =
      std::lerp(baseline.indirect_albedo_floor, 0.006f + chamber_fill * 0.002f, interior);
  settings.sky_ambient_color = mixVec(baseline.sky_ambient_color, cave_sky_tint, interior);
  settings.ground_ambient_color = mixVec(baseline.ground_ambient_color, cave_ground_tint, interior);
  settings.pipeline.clear_color =
      mixVec(baseline.clear_color, {0.006f, 0.004f, 0.003f}, interior);
  settings.exposure =
      std::lerp(baseline.exposure, 0.82f + source_fill * 0.032f + chamber_fill * 0.012f,
                interior);
  settings.atmosphere.fog_color =
      mixVec(baseline.atmosphere.fog_color, cave_air_tint, interior);
  settings.atmosphere.fog_start = std::lerp(baseline.atmosphere.fog_start, 0.78f, interior);
  settings.atmosphere.fog_end =
      std::lerp(baseline.atmosphere.fog_end, 9.8f + source_fill * 2.8f + chamber_fill * 1.6f,
                interior);
  settings.atmosphere.fog_strength =
      std::lerp(baseline.atmosphere.fog_strength, 0.18f + cave_depth * 0.06f, interior);
  settings.atmosphere.local_light_scattering =
      std::lerp(baseline.atmosphere.local_light_scattering,
                0.25f + source_fill * 0.09f + chamber_fill * 0.032f, interior);
  settings.atmosphere.local_light_extinction =
      std::lerp(baseline.atmosphere.local_light_extinction, 0.060f + cave_depth * 0.026f,
                interior);
  settings.atmosphere.source_glow_strength =
      std::lerp(baseline.atmosphere.source_glow_strength, 1.02f + source_fill * 0.18f, interior);
  settings.atmosphere.source_glow_radius_scale =
      std::lerp(baseline.atmosphere.source_glow_radius_scale, 1.04f, interior);
  settings.atmosphere.phase_anisotropy =
      std::lerp(baseline.atmosphere.phase_anisotropy, 0.28f, interior);
  settings.atmosphere.volumetric_light_steps = std::max(settings.atmosphere.volumetric_light_steps, 6u);
  settings.atmosphere.saturation =
      std::lerp(baseline.atmosphere.saturation, 0.66f + source_fill * 0.10f, interior);
  settings.atmosphere.contrast = std::lerp(baseline.atmosphere.contrast, 1.06f, interior);
  settings.post.bloom = true;
  settings.post.bloom_threshold = std::lerp(settings.post.bloom_threshold, 1.24f, interior);
  settings.post.bloom_intensity = std::lerp(settings.post.bloom_intensity, 0.09f, interior);
  settings.sun_light.intensity =
      std::lerp(baseline.sun_light.intensity, 0.0f, interior);
  settings.style.unlit_mix = std::lerp(settings.style.unlit_mix, 0.0f, interior);
  for (std::size_t i = 0; i < settings.light_rig.size(); ++i) {
    settings.light_rig[i].intensity *= std::lerp(1.0f, 0.08f, interior);
  }
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

struct StartupSample {
  std::string label;
  double seconds = 0.0;
};

void printStartupSummary(const std::vector<StartupSample> &samples) {
  double total = 0.0;
  std::cout << std::fixed << std::setprecision(3);
  for (const StartupSample &sample : samples) {
    total += sample.seconds;
    std::cout << "Startup report " << sample.label
              << ": ms=" << secondsToMilliseconds(sample.seconds) << '\n';
  }
  std::cout << "Startup report total: ms=" << secondsToMilliseconds(total) << '\n';
}

const char *lumenWorldGateVerdictName(const aster::LumenWorldGateVerdict verdict) {
  switch (verdict) {
  case aster::LumenWorldGateVerdict::Unknown:
    return "unknown";
  case aster::LumenWorldGateVerdict::Accepted:
    return "accepted";
  case aster::LumenWorldGateVerdict::Quarantined:
    return "quarantined";
  }
  return "unknown";
}

void printLumenWorldGateReport(const aster::LumenCaveWorldGateReport &report) {
  std::cout << "Lumen Run cave world gate: verdict="
            << lumenWorldGateVerdictName(report.verdict) << " seed=" << report.seed
            << " region_id=" << report.region_id
            << " probe_trace_hash=" << report.probe_trace_hash
            << " nav=" << (report.navigation_valid ? "pass" : "fail")
            << " checked_steps=" << report.checked_steps
            << " blocked_steps=" << report.blocked_steps
            << " resources=" << report.reachable_resources << "/" << report.required_resources
            << " encounters=" << report.reachable_encounters
            << " encounter_budget=" << report.encounter_budget
            << " salience=" << report.perceptual_salience_score
            << "/" << report.perceptual_minimum_salience
            << " continuity=" << report.perceptual_continuity_score
            << "/" << report.perceptual_continuity_minimum_score << '\n';
  std::cout << "Lumen Run cave world gate diagnostic: " << report.diagnostic << '\n';
}

std::uint64_t lumenFrameProofHash(const aster::LumenWorldForensics &world,
                                  const aster::FrameStats &stats, const int width,
                                  const int height, const int rendered_frames,
                                  const std::uint64_t salt) {
  std::uint64_t hash = aster::hashCombine64(0x4C554D454E46524Dull, salt);
  hash = aster::hashCombine64(hash, world.world_transition_hash);
  hash = aster::hashCombine64(hash, world.trace_hash);
  hash = aster::hashCombine64(hash, world.cave_gate.probe_trace_hash);
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(world.epoch));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(width));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(height));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(rendered_frames));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(stats.visible_objects));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(stats.draw_calls));
  hash = aster::hashCombine64(hash, static_cast<std::uint64_t>(stats.active_point_lights));
  hash = aster::hashCombine64(
      hash,
      static_cast<std::uint64_t>(
          aster::stableHash32(world.cave_gate.perceptual_salience_score)));
  hash = aster::hashCombine64(hash, world.cave_gate.perceptual_continuity_report_hash);
  hash = aster::hashCombine64(hash, world.coal_mining_reaction.reaction_package_hash);
  hash = aster::hashCombine64(hash, world.perceptual_state.perceptual_state_hash);
  hash = aster::hashCombine64(hash, world.perceptual_state.semantic_budget_hash);
  return hash;
}

aster::ControlScheme makeRunControls() {
  aster::ControlScheme controls;
  controls.bind(kPause, aster::keyBinding(aster::Key::Escape));
  controls.bind(kReset, aster::keyBinding(aster::Key::R));

  controls.bind(kMoveLeft, aster::keyBinding(aster::Key::A));
  controls.bind(kMoveLeft, aster::keyBinding(aster::Key::Left));
  controls.bind(kMoveRight, aster::keyBinding(aster::Key::D));
  controls.bind(kMoveRight, aster::keyBinding(aster::Key::Right));
  controls.bind(kMoveUp, aster::keyBinding(aster::Key::W));
  controls.bind(kMoveUp, aster::keyBinding(aster::Key::Up));
  controls.bind(kMoveDown, aster::keyBinding(aster::Key::S));
  controls.bind(kMoveDown, aster::keyBinding(aster::Key::Down));
  controls.bind(kJump, aster::keyBinding(aster::Key::Space));
  controls.bind(kRun, aster::keyBinding(aster::Key::LeftShift));
  controls.bind(kInventory, aster::keyBinding(aster::Key::Tab));
  controls.bind(kInventoryRotate, aster::mouseBinding(aster::MouseButton::Left));
  controls.bind(kInventoryRecenter, aster::mouseBinding(aster::MouseButton::Right));
  controls.bind(kCommandAim, aster::keyBinding(aster::Key::LeftSuper));
  controls.bind(kCommandAim, aster::keyBinding(aster::Key::RightSuper));
  controls.bind(kCameraOrbit, aster::mouseBinding(aster::MouseButton::Left));
  controls.bind(kCameraOrbit, aster::mouseBinding(aster::MouseButton::Right));
  controls.bind(kInteract, aster::keyBinding(aster::Key::E));
  controls.bind(kSecondaryInteract, aster::mouseBinding(aster::MouseButton::Right));
  controls.bind(kHotbar1, aster::keyBinding(aster::Key::Num1));
  controls.bind(kHotbar2, aster::keyBinding(aster::Key::Num2));
  controls.bind(kHotbar3, aster::keyBinding(aster::Key::Num3));
  controls.bind(kHotbar4, aster::keyBinding(aster::Key::Num4));
  controls.bind(kHotbar5, aster::keyBinding(aster::Key::Num5));
  controls.bind(kHotbar6, aster::keyBinding(aster::Key::Num6));
  controls.bind(kCaveDebugOverlayToggle, aster::keyBinding(aster::Key::F1));
  controls.bind(kCaveDebugOverlayCollision, aster::keyBinding(aster::Key::F2));
  controls.bind(kCaveDebugOverlayInteractable, aster::keyBinding(aster::Key::F3));
  controls.bind(kCaveDebugOverlayMining, aster::keyBinding(aster::Key::F4));
  controls.bind(kCaveDebugOverlaySpawn, aster::keyBinding(aster::Key::F5));
  controls.bind(kCaveDebugOverlayCamera, aster::keyBinding(aster::Key::F6));
  controls.bind(kCaveDebugOverlayWalkable, aster::keyBinding(aster::Key::F7));
  return controls;
}

aster::Vec2 movementAxis(const aster::ControlState &controls) {
  aster::Vec2 axis{
      controls.strength(kMoveRight) - controls.strength(kMoveLeft),
      controls.strength(kMoveUp) - controls.strength(kMoveDown),
  };
  if (aster::length(axis) > 1.0f) {
    axis = aster::normalize(axis);
  }
  return axis;
}

aster::Vec2 attractAxis(const float seconds) {
  if (seconds < 1.15f) {
    return aster::normalize(aster::Vec2{0.70f, 0.54f});
  }
  if (seconds < 2.55f) {
    return {0.16f, 0.12f};
  }
  if (seconds < 3.85f) {
    return {-0.10f, 0.18f};
  }
  return {0.0f, 0.0f};
}

bool scriptedRun(const float seconds) {
  return seconds > 3.40f && seconds < 4.20f;
}

bool scriptedJump(const float seconds) {
  return seconds > 1.04f && seconds < 1.10f;
}

aster::Vec2 caveEntryAxis(const float seconds) {
  if (seconds < 1.05f) {
    return aster::normalize(aster::Vec2{0.64f, -0.76f});
  }
  if (seconds < 3.80f) {
    return aster::normalize(aster::Vec2{0.14f, -0.99f});
  }
  if (seconds < 5.60f) {
    return {0.0f, -0.78f};
  }
  return {};
}

bool caveEntryRun(const float seconds) {
  return seconds > 0.62f && seconds < 4.85f;
}

aster::Vec3 caveEntryCameraTarget(const aster::Vec3 player_position, const float seconds) {
  (void)seconds;
  return {player_position.x, player_position.y + 0.34f,
          std::max(player_position.z - 0.22f, -90.0f)};
}

std::filesystem::path framePath(const std::filesystem::path &directory, const int frame) {
  std::ostringstream name;
  name << "frame_";
  name.width(4);
  name.fill('0');
  name << frame << ".ppm";
  return directory / name.str();
}

std::string lowerPathExtension(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

void writeActiveFramebufferCapture(const std::filesystem::path &path, const int width,
                                   const int height) {
  if (lowerPathExtension(path) == ".png") {
    aster::writeFramebufferPng(path, width, height);
  } else {
    aster::writeFramebufferPpm(path, width, height);
  }
}

std::string jsonEscape(const std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8u);
  for (const char c : text) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

aster::RendererSettings makeVisionRendererSettings(const aster::RenderStyleProfile &render_style) {
  aster::RendererSettings settings;
  settings.procedural_surface_normals = true;
  settings.exposure = 1.34f;
  settings.ambient_strength = 0.36f;
  settings.ambient_floor = 0.072f;
  settings.sky_ambient_color = {0.48f, 0.56f, 0.66f};
  settings.ground_ambient_color = {0.25f, 0.24f, 0.19f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.46f, 0.86f, 0.30f};
  settings.sun_light.color = {1.00f, 0.84f, 0.58f};
  settings.sun_light.intensity = 1.72f;
  settings.light_rig = {{{-4.6f, 3.2f, 2.8f}, {4.4f, 3.25f, 2.05f}, 0.28f, 3.0f},
                        {{4.8f, 2.4f, -3.4f}, {1.15f, 1.35f, 2.10f}, 0.22f, 3.5f},
                        {{0.0f, 2.8f, -5.8f}, {1.35f, 1.02f, 0.72f}, 0.16f, 4.0f},
                        {{0.0f, 2.0f, 4.8f}, {0.76f, 0.86f, 1.12f}, 0.13f, 3.5f}};
  settings.pipeline.clear_color = {0.092f, 0.122f, 0.158f};
  settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = true;
  settings.grounding.auto_contact_shadows = true;
  settings.grounding.surface_occlusion_strength = 0.34f;
  settings.grounding.surface_occlusion_height = 1.05f;
  settings.grounding.surface_occlusion_mix = 0.28f;
  settings.grounding.surface_occlusion_min = 0.78f;
  settings.grounding.contact_shadow_strength = 0.24f;
  settings.grounding.contact_shadow_radius_scale = 1.12f;
  settings.grounding.contact_shadow_max_radius = 1.18f;
  settings.grounding.contact_shadow_receiver_height = 1.06f;
  settings.grounding.contact_shadow_receiver_bias = 0.020f;
  settings.grounding.contact_shadow_detail_scale = 10.0f;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.175f, 0.205f, 0.250f};
  settings.atmosphere.fog_start = 8.0f;
  settings.atmosphere.fog_end = 28.0f;
  settings.atmosphere.fog_strength = 0.10f;
  settings.atmosphere.saturation = 1.22f;
  settings.atmosphere.contrast = 1.10f;
  settings.atmosphere.shadow_tint = {0.52f, 0.64f, 0.86f};
  settings.atmosphere.shadow_tint_strength = 0.16f;
  settings.atmosphere.highlight_tint = {1.10f, 1.02f, 0.82f};
  settings.atmosphere.highlight_tint_strength = 0.10f;
  settings.style = render_style;
  aster::applyRenderStyleProfile(settings, render_style);
  return settings;
}

RenderEnvironmentBaseline baselineFromSettings(const aster::RendererSettings &settings) {
  return {.light_rig = settings.light_rig,
          .sun_light = settings.sun_light,
          .ambient_strength = settings.ambient_strength,
          .ambient_floor = settings.ambient_floor,
          .sky_ambient_color = settings.sky_ambient_color,
          .ground_ambient_color = settings.ground_ambient_color,
          .atmosphere = settings.atmosphere,
          .clear_color = settings.pipeline.clear_color,
          .exposure = settings.exposure,
          .indirect_albedo_floor = settings.indirect_albedo_floor};
}

struct VisionFrameMetrics {
  std::string label;
  float progress = 0.0f;
  std::filesystem::path png_path;
  std::uint64_t visible_void_rays = 0u;
  std::uint64_t black_void_pixels = 0u;
  std::uint64_t zfight_candidate_pixels = 0u;
  std::uint64_t support_render_mismatch_count = 0u;
  float max_support_render_delta_m = 0.0f;
};

struct VisionStageMetrics {
  std::string label;
  float start_progress = 0.0f;
  float end_progress = 0.0f;
  float progress_delta_m = 0.0f;
  std::uint32_t blocked_frames = 0u;
  std::uint32_t upper_terrain_snap_count = 0u;
  std::uint32_t respawn_count = 0u;
};

struct VisionPlaytestMetrics {
  std::string route;
  std::filesystem::path output_dir;
  std::vector<VisionFrameMetrics> frames;
  std::vector<VisionStageMetrics> stages;
  std::uint64_t visible_void_rays = 0u;
  std::uint64_t black_void_pixels = 0u;
  std::uint64_t zfight_candidate_pixels = 0u;
  std::uint64_t support_render_mismatch_count = 0u;
  std::uint32_t blocked_frames = 0u;
  std::uint32_t respawn_count = 0u;
  std::uint32_t upper_terrain_snap_count = 0u;
  float max_support_render_delta_m = 0.0f;
  bool accepted = false;
};

VisionFrameMetrics analyzeVisionFrame(const std::string &label, const float progress,
                                      const std::filesystem::path &png_path,
                                      const aster::SoftwarePreviewResult &frame,
                                      const aster::SoftwarePreviewResult &jitter) {
  VisionFrameMetrics metrics;
  metrics.label = label;
  metrics.progress = progress;
  metrics.png_path = png_path;
  const int width = frame.probe.width;
  const int height = frame.probe.height;
  const std::span<const std::uint8_t> rgba = frame.framebuffer.rgba8();
  if (width <= 0 || height <= 0 || frame.probe.pixels.size() != jitter.probe.pixels.size()) {
    return metrics;
  }
  const int min_x = std::max(0, static_cast<int>(static_cast<float>(width) * 0.06f));
  const int max_x = std::min(width, static_cast<int>(static_cast<float>(width) * 0.94f));
  const int min_y = std::max(0, static_cast<int>(static_cast<float>(height) * 0.16f));
  const int max_y = std::min(height, static_cast<int>(static_cast<float>(height) * 0.92f));
  for (int y = min_y; y < max_y; ++y) {
    for (int x = min_x; x < max_x; ++x) {
      const std::size_t index =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
      const aster::SoftwarePreviewProbePixel &hit = frame.probe.pixels[index];
      const aster::SoftwarePreviewProbePixel &jitter_hit = jitter.probe.pixels[index];
      if (hit.hit != 0u && jitter_hit.hit != 0u &&
          hit.render_role == aster::MaterialRenderRole::Surface &&
          jitter_hit.render_role == aster::MaterialRenderRole::Surface &&
          hit.depth_layer == aster::RenderDepthLayer::BaseSurface &&
          jitter_hit.depth_layer == aster::RenderDepthLayer::BaseSurface &&
          hit.object_label_hash != jitter_hit.object_label_hash &&
          std::abs(hit.distance - jitter_hit.distance) <= 0.00035f &&
          aster::dot(aster::normalize(hit.normal), aster::normalize(jitter_hit.normal)) > 0.98f) {
        ++metrics.zfight_candidate_pixels;
      }
      const std::size_t base = index * 4u;
      if (base + 2u < rgba.size()) {
        const std::uint32_t luma =
            static_cast<std::uint32_t>(rgba[base + 0u]) * 54u +
            static_cast<std::uint32_t>(rgba[base + 1u]) * 183u +
            static_cast<std::uint32_t>(rgba[base + 2u]) * 19u;
        if (hit.hit == 0u && luma <= 2u * 256u) {
          ++metrics.visible_void_rays;
          ++metrics.black_void_pixels;
        }
      }
    }
  }
  return metrics;
}

void writeVisionJson(const VisionPlaytestMetrics &metrics) {
  const std::filesystem::path json_path = metrics.output_dir / (metrics.route + ".json");
  std::ofstream file(json_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not write Lumen cave vision metrics: " + json_path.string());
  }
  file << std::fixed << std::setprecision(6);
  file << "{\n";
  file << "  \"route\": \"" << jsonEscape(metrics.route) << "\",\n";
  file << "  \"accepted\": " << (metrics.accepted ? "true" : "false") << ",\n";
  file << "  \"visible_void_rays\": " << metrics.visible_void_rays << ",\n";
  file << "  \"black_void_pixels\": " << metrics.black_void_pixels << ",\n";
  file << "  \"zfight_candidate_pixels\": " << metrics.zfight_candidate_pixels << ",\n";
  file << "  \"support_render_mismatch_count\": " << metrics.support_render_mismatch_count
       << ",\n";
  file << "  \"max_support_render_delta_m\": " << metrics.max_support_render_delta_m << ",\n";
  file << "  \"blocked_frames\": " << metrics.blocked_frames << ",\n";
  file << "  \"respawn_count\": " << metrics.respawn_count << ",\n";
  file << "  \"upper_terrain_snap_count\": " << metrics.upper_terrain_snap_count << ",\n";
  file << "  \"thresholds\": {\n";
  file << "    \"visible_void_rays\": 0,\n";
  file << "    \"zfight_candidate_pixels\": 0,\n";
  file << "    \"max_support_render_delta_m\": 0.080000,\n";
  file << "    \"blocked_frames\": 0,\n";
  file << "    \"respawn_count\": 0,\n";
  file << "    \"upper_terrain_snap_count\": 0\n";
  file << "  },\n";
  file << "  \"stages\": [\n";
  for (std::size_t i = 0; i < metrics.stages.size(); ++i) {
    const VisionStageMetrics &stage = metrics.stages[i];
    file << "    {\"label\": \"" << jsonEscape(stage.label) << "\", \"start_progress\": "
         << stage.start_progress << ", \"end_progress\": " << stage.end_progress
         << ", \"progress_delta_m\": " << stage.progress_delta_m
         << ", \"blocked_frames\": " << stage.blocked_frames
         << ", \"respawn_count\": " << stage.respawn_count
         << ", \"upper_terrain_snap_count\": " << stage.upper_terrain_snap_count << "}";
    file << (i + 1u < metrics.stages.size() ? ",\n" : "\n");
  }
  file << "  ],\n";
  file << "  \"frames\": [\n";
  for (std::size_t i = 0; i < metrics.frames.size(); ++i) {
    const VisionFrameMetrics &frame = metrics.frames[i];
    file << "    {\"label\": \"" << jsonEscape(frame.label) << "\", \"progress\": "
         << frame.progress << ", \"visible_void_rays\": " << frame.visible_void_rays
         << ", \"black_void_pixels\": " << frame.black_void_pixels
         << ", \"zfight_candidate_pixels\": " << frame.zfight_candidate_pixels
         << ", \"support_render_mismatch_count\": " << frame.support_render_mismatch_count
         << ", \"max_support_render_delta_m\": " << frame.max_support_render_delta_m
         << ", \"png_path\": \"" << jsonEscape(frame.png_path.string()) << "\"}";
    file << (i + 1u < metrics.frames.size() ? ",\n" : "\n");
  }
  file << "  ]\n";
  file << "}\n";
}

void simulateVisionStage(aster::LumenRun &game, VisionPlaytestMetrics &metrics,
                         const std::string &label, const float start_progress,
                         const float end_progress, const float min_progress_delta_m) {
  VisionStageMetrics stage;
  stage.label = label;
  stage.start_progress = start_progress;
  stage.end_progress = end_progress;
  game.relocatePlayer(game.caveFrameReportPosition(start_progress),
                      game.caveFrameReportCameraYaw(start_progress));
  game.updateRenderInterpolation(1.0f);
  const int initial_lives = game.status().lives;
  const aster::Vec3 start = game.playerPosition();
  const aster::Vec3 target = game.caveFrameReportPosition(end_progress);
  for (int frame = 0; frame < 180; ++frame) {
    const aster::Vec3 player = game.playerPosition();
    const aster::Vec3 to_target{target.x - player.x, 0.0f, target.z - player.z};
    const float distance = aster::length(to_target);
    aster::Vec2 axis{};
    if (distance > 0.08f) {
      const aster::Vec3 dir = to_target / distance;
      axis = {dir.x, dir.z};
    }
    game.update(static_cast<float>(kSimulationStepSeconds), axis, true, false);
    game.updateRenderInterpolation(1.0f);
    const aster::Vec3 after = game.playerPosition();
    const float stage_ceiling_guard = std::max(start.y, target.y) + 2.10f;
    if (after.y > stage_ceiling_guard) {
      ++stage.upper_terrain_snap_count;
    }
  }
  const aster::Vec3 final_player = game.playerPosition();
  const float final_distance =
      aster::length(aster::Vec3{target.x - final_player.x, 0.0f, target.z - final_player.z});
  const float total_distance =
      aster::length(aster::Vec3{target.x - start.x, 0.0f, target.z - start.z});
  stage.progress_delta_m = std::max(total_distance - final_distance, 0.0f);
  if (stage.progress_delta_m + 0.001f < min_progress_delta_m) {
    ++stage.blocked_frames;
  }
  stage.respawn_count =
      game.status().lives < initial_lives ? static_cast<std::uint32_t>(initial_lives - game.status().lives)
                                          : 0u;
  metrics.blocked_frames += stage.blocked_frames;
  metrics.respawn_count += stage.respawn_count;
  metrics.upper_terrain_snap_count += stage.upper_terrain_snap_count;
  metrics.stages.push_back(stage);
}

VisionFrameMetrics renderVisionFrame(aster::LumenRun &game, const std::string &label,
                                     const float progress, const int width, const int height,
                                     const aster::RenderStyleProfile &render_style,
                                     const std::filesystem::path &output_dir) {
  game.relocatePlayer(game.caveFrameReportPosition(progress), game.caveFrameReportCameraYaw(progress));
  game.updateRenderInterpolation(1.0f);
  aster::OrbitCamera camera;
  camera.target = game.caveFrameReportLookTarget(progress, 1.15f);
  camera.pitch = aster::radians(6.0f);
  camera.yaw = game.caveFrameReportCameraYaw(progress);
  camera.radius = game.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, 2.70f);
  camera.vertical_fov = aster::radians(54.0f);
  aster::RendererSettings settings = makeVisionRendererSettings(render_style);
  const RenderEnvironmentBaseline baseline = baselineFromSettings(settings);
  const aster::CaveLightingState cave_light = game.caveLightingStateAt(camera.target);
  restoreRenderEnvironment(settings, baseline);
  aster::applyRenderStyleProfile(settings, render_style);
  applyCaveRenderEnvironment(settings, baseline, cave_light);
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    settings.light_rig.push_back({light.position, light.color, light.intensity, light.source_radius});
  }
  const aster::SoftwarePreviewOptions options{.width = width,
                                              .height = height,
                                              .samples_per_axis = 1,
                                              .frame_seconds = static_cast<double>(progress),
                                              .settings = settings};
  aster::SoftwarePreviewResult frame =
      aster::renderSoftwarePreviewWithProbe(game.scene(), camera, options);
  aster::OrbitCamera jitter_camera = camera;
  jitter_camera.yaw += aster::radians(0.045f);
  aster::SoftwarePreviewResult jitter =
      aster::renderSoftwarePreviewWithProbe(game.scene(), jitter_camera, options);
  const std::filesystem::path png_path = output_dir / (label + ".png");
  aster::writeFrameBufferPng(frame.framebuffer, png_path, width, height);
  return analyzeVisionFrame(label, progress, png_path, frame, jitter);
}

VisionPlaytestMetrics runLumenCaveVisionPlaytest(aster::LumenRun &game,
                                                 const std::string &route,
                                                 const std::filesystem::path &output_dir,
                                                 const int width, const int height,
                                                 const aster::RenderStyleProfile &render_style) {
  if (route != "cave-regression") {
    throw std::runtime_error("unknown Lumen Run vision route '" + route +
                             "'; expected cave-regression");
  }
  std::filesystem::create_directories(output_dir);
  game.setPlayerAvatarVisible(false);
  VisionPlaytestMetrics metrics;
  metrics.route = route;
  metrics.output_dir = output_dir;
  simulateVisionStage(game, metrics, "cave_entry_to_web_area", 1.0f, 8.5f, 2.8f);
  simulateVisionStage(game, metrics, "web_area_to_chamber", 8.5f, 18.0f, 3.4f);
  simulateVisionStage(game, metrics, "chamber_to_deep_connector", 18.0f, 31.0f, 4.2f);
  simulateVisionStage(game, metrics, "deep_backtrack", 31.0f, 12.0f, 3.8f);

  const std::vector<std::pair<std::string, float>> probes = {{"cave_entrance", 3.0f},
                                                             {"web_area", 9.0f},
                                                             {"chamber", 18.0f},
                                                             {"deep_connector", 30.0f},
                                                             {"backtrack", 12.0f}};
  for (const auto &[label, progress] : probes) {
    VisionFrameMetrics frame =
        renderVisionFrame(game, label, progress, width, height, render_style, output_dir);
    metrics.visible_void_rays += frame.visible_void_rays;
    metrics.black_void_pixels += frame.black_void_pixels;
    metrics.zfight_candidate_pixels += frame.zfight_candidate_pixels;
    metrics.support_render_mismatch_count += frame.support_render_mismatch_count;
    metrics.max_support_render_delta_m =
        std::max(metrics.max_support_render_delta_m, frame.max_support_render_delta_m);
    metrics.frames.push_back(std::move(frame));
  }
  metrics.accepted = metrics.visible_void_rays == 0u &&
                     metrics.zfight_candidate_pixels == 0u &&
                     metrics.max_support_render_delta_m <= 0.08f &&
                     metrics.blocked_frames == 0u && metrics.respawn_count == 0u &&
                     metrics.upper_terrain_snap_count == 0u;
  writeVisionJson(metrics);
  return metrics;
}

struct LightingFrameMetrics {
  std::string label;
  float progress = 0.0f;
  std::filesystem::path png_path;
  std::filesystem::path heatmap_path;
  std::uint64_t source_visible_pixels = 0u;
  std::uint64_t air_scatter_pixels = 0u;
  std::uint64_t light_source_unreadable_count = 0u;
  std::uint64_t volumetric_light_missing_count = 0u;
  std::uint64_t light_falloff_discontinuity_count = 0u;
  std::uint64_t cave_light_exposure_underflow_count = 0u;
  std::uint64_t cave_light_exposure_overflow_count = 0u;
  std::uint64_t cave_light_exposure_overbright_count = 0u;
  std::uint64_t overexposed_pixels = 0u;
  float source_mean_luminance = 0.0f;
  float air_scatter_mean_luminance = 0.0f;
  float frame_mean_luminance = 0.0f;
  float surface_direct_mean_luminance = 0.0f;
  float source_to_air_ratio = 0.0f;
  float falloff_continuity_score = 1.0f;
  float temporal_lighting_delta = 0.0f;
};

struct LightingPlaytestMetrics {
  std::string route;
  std::filesystem::path output_dir;
  std::vector<LightingFrameMetrics> frames;
  std::uint64_t source_visible_pixels = 0u;
  std::uint64_t air_scatter_pixels = 0u;
  std::uint64_t light_source_unreadable_count = 0u;
  std::uint64_t volumetric_light_missing_count = 0u;
  std::uint64_t light_falloff_discontinuity_count = 0u;
  std::uint64_t cave_light_exposure_underflow_count = 0u;
  std::uint64_t cave_light_exposure_overflow_count = 0u;
  std::uint64_t cave_light_exposure_overbright_count = 0u;
  std::uint64_t overexposed_pixels = 0u;
  float min_source_mean_luminance = 100000.0f;
  float min_air_scatter_mean_luminance = 100000.0f;
  float min_frame_mean_luminance = 100000.0f;
  float max_frame_mean_luminance = 0.0f;
  float min_falloff_continuity_score = 1.0f;
  float max_temporal_lighting_delta = 0.0f;
  bool accepted = false;
};

LightingFrameMetrics analyzeLightingFrame(const std::string &label, const float progress,
                                          const std::filesystem::path &png_path,
                                          const std::filesystem::path &heatmap_path,
                                          const aster::SoftwarePreviewResult &frame) {
  constexpr float kSourceThreshold = 0.045f;
  constexpr float kAirThreshold = 0.0012f;
  constexpr float kMinSourceMean = 0.055f;
  constexpr float kMinAirMean = 0.0018f;
  constexpr std::uint64_t kMinAirPixels = 96u;
  constexpr float kMinRatio = 1.20f;
  constexpr float kMaxRatio = 240.0f;
  constexpr float kMinFalloff = 0.18f;
  constexpr float kMinFrameMean = 0.005f;
  constexpr float kMaxFrameMean = 0.255f;
  constexpr float kOverexposedLuminance = 232.0f;
  constexpr float kMaxOverexposedFraction = 0.028f;
  LightingFrameMetrics metrics;
  metrics.label = label;
  metrics.progress = progress;
  metrics.png_path = png_path;
  metrics.heatmap_path = heatmap_path;
  double source_sum = 0.0;
  double air_sum = 0.0;
  double direct_sum = 0.0;
  double direct_delta_sum = 0.0;
  float previous_direct = -1.0f;
  std::uint64_t direct_pixels = 0u;
  for (const aster::SoftwareLightingProbePixel &pixel : frame.lighting.pixels) {
    if (pixel.source_readability_luminance >= kSourceThreshold) {
      source_sum += pixel.source_readability_luminance;
      ++metrics.source_visible_pixels;
    }
    if (pixel.volumetric_light_luminance >= kAirThreshold) {
      air_sum += pixel.volumetric_light_luminance;
      ++metrics.air_scatter_pixels;
    }
    if (pixel.direct_light_luminance > 0.00001f) {
      direct_sum += pixel.direct_light_luminance;
      if (previous_direct >= 0.0f) {
        direct_delta_sum += std::abs(pixel.direct_light_luminance - previous_direct);
      }
      previous_direct = pixel.direct_light_luminance;
      ++direct_pixels;
    }
  }
  const std::span<const std::uint8_t> rgba = frame.framebuffer.rgba8();
  double frame_luma_sum = 0.0;
  std::uint64_t frame_pixels = 0u;
  for (std::size_t offset = 0u; offset + 3u < rgba.size(); offset += 4u) {
    const float red = static_cast<float>(rgba[offset + 0u]);
    const float green = static_cast<float>(rgba[offset + 1u]);
    const float blue = static_cast<float>(rgba[offset + 2u]);
    const float luma = red * 0.2126f + green * 0.7152f + blue * 0.0722f;
    frame_luma_sum += static_cast<double>(luma / 255.0f);
    ++frame_pixels;
    const float max_channel = std::max(red, std::max(green, blue));
    if (luma >= kOverexposedLuminance && max_channel >= 254.0f) {
      ++metrics.overexposed_pixels;
    }
  }
  metrics.source_mean_luminance =
      metrics.source_visible_pixels > 0u
          ? static_cast<float>(source_sum / static_cast<double>(metrics.source_visible_pixels))
          : 0.0f;
  metrics.air_scatter_mean_luminance =
      metrics.air_scatter_pixels > 0u
          ? static_cast<float>(air_sum / static_cast<double>(metrics.air_scatter_pixels))
          : 0.0f;
  metrics.surface_direct_mean_luminance =
      direct_pixels > 0u ? static_cast<float>(direct_sum / static_cast<double>(direct_pixels))
                         : 0.0f;
  metrics.frame_mean_luminance =
      frame_pixels > 0u ? static_cast<float>(frame_luma_sum / static_cast<double>(frame_pixels))
                        : 0.0f;
  const float direct_delta =
      direct_pixels > 1u ? static_cast<float>(direct_delta_sum / static_cast<double>(direct_pixels - 1u))
                         : 0.0f;
  metrics.falloff_continuity_score =
      metrics.surface_direct_mean_luminance > 0.000001f
          ? std::clamp(1.0f - direct_delta / (metrics.surface_direct_mean_luminance + 0.0001f),
                       0.0f, 1.0f)
          : 1.0f;
  metrics.source_to_air_ratio =
      metrics.air_scatter_mean_luminance > 0.000001f
          ? metrics.source_mean_luminance / metrics.air_scatter_mean_luminance
          : 999.0f;
  if (metrics.source_mean_luminance < kMinSourceMean || metrics.source_visible_pixels == 0u) {
    metrics.light_source_unreadable_count = 1u;
  }
  if (metrics.air_scatter_pixels < kMinAirPixels ||
      metrics.air_scatter_mean_luminance < kMinAirMean) {
    metrics.volumetric_light_missing_count = 1u;
  }
  if (metrics.falloff_continuity_score < kMinFalloff) {
    metrics.light_falloff_discontinuity_count = 1u;
  }
  if (metrics.source_to_air_ratio < kMinRatio || metrics.source_to_air_ratio > kMaxRatio) {
    metrics.cave_light_exposure_underflow_count = 1u;
  }
  const std::uint64_t pixel_count =
      static_cast<std::uint64_t>(std::max(frame.framebuffer.width(), 0)) *
      static_cast<std::uint64_t>(std::max(frame.framebuffer.height(), 0));
  const std::uint64_t max_overexposed_pixels =
      std::max<std::uint64_t>(64u, static_cast<std::uint64_t>(
                                       static_cast<double>(pixel_count) *
                                       static_cast<double>(kMaxOverexposedFraction)));
  if (metrics.overexposed_pixels > max_overexposed_pixels) {
    metrics.cave_light_exposure_overflow_count = 1u;
  }
  if (metrics.frame_mean_luminance < kMinFrameMean ||
      metrics.frame_mean_luminance > kMaxFrameMean) {
    metrics.cave_light_exposure_overbright_count = 1u;
  }
  return metrics;
}

void writeLightingHeatmap(const aster::SoftwarePreviewResult &frame,
                          const std::filesystem::path &heatmap_path) {
  const int width = frame.lighting.width;
  const int height = frame.lighting.height;
  if (width <= 0 || height <= 0) {
    return;
  }
  float direct_max = 0.001f;
  float volume_max = 0.001f;
  float source_max = 0.001f;
  for (const aster::SoftwareLightingProbePixel &pixel : frame.lighting.pixels) {
    direct_max = std::max(direct_max, pixel.direct_light_luminance);
    volume_max = std::max(volume_max, pixel.volumetric_light_luminance);
    source_max = std::max(source_max, pixel.source_readability_luminance);
  }
  std::vector<std::uint8_t> heatmap(static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 4u);
  for (std::size_t i = 0; i < frame.lighting.pixels.size(); ++i) {
    const aster::SoftwareLightingProbePixel &pixel = frame.lighting.pixels[i];
    const std::size_t base = i * 4u;
    heatmap[base + 0u] = static_cast<std::uint8_t>(
        std::clamp(std::lround(std::clamp(pixel.direct_light_luminance / direct_max, 0.0f, 1.0f) *
                               255.0f),
                   0l, 255l));
    heatmap[base + 1u] = static_cast<std::uint8_t>(
        std::clamp(std::lround(std::clamp(pixel.volumetric_light_luminance / volume_max, 0.0f,
                                          1.0f) *
                               255.0f),
                   0l, 255l));
    heatmap[base + 2u] = static_cast<std::uint8_t>(
        std::clamp(std::lround(std::clamp(pixel.source_readability_luminance / source_max, 0.0f,
                                          1.0f) *
                               255.0f),
                   0l, 255l));
    heatmap[base + 3u] = 255u;
  }
  aster::writeRgbaPng(heatmap_path, width, height, heatmap);
}

LightingFrameMetrics renderLightingFrame(aster::LumenRun &game, const std::string &label,
                                         const float progress, const int width, const int height,
                                         const aster::RenderStyleProfile &render_style,
                                         const std::filesystem::path &output_dir) {
  game.relocatePlayer(game.caveFrameReportPosition(progress), game.caveFrameReportCameraYaw(progress));
  game.updateRenderInterpolation(1.0f);
  aster::OrbitCamera camera;
  camera.target = game.caveFrameReportLookTarget(progress, 1.25f);
  camera.pitch = aster::radians(5.0f);
  camera.yaw = game.caveFrameReportCameraYaw(progress);
  camera.radius = game.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, 2.55f);
  camera.vertical_fov = aster::radians(54.0f);
  aster::RendererSettings settings = makeVisionRendererSettings(render_style);
  const RenderEnvironmentBaseline baseline = baselineFromSettings(settings);
  const aster::CaveLightingState cave_light = game.caveLightingStateAt(camera.target);
  restoreRenderEnvironment(settings, baseline);
  aster::applyRenderStyleProfile(settings, render_style);
  applyCaveRenderEnvironment(settings, baseline, cave_light);
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    settings.light_rig.push_back({light.position, light.color, light.intensity, light.source_radius});
  }
  const aster::SoftwarePreviewResult frame =
      aster::renderSoftwarePreviewWithProbe(game.scene(), camera,
                                            {.width = width,
                                             .height = height,
                                             .samples_per_axis = 1,
                                             .frame_seconds = static_cast<double>(progress),
                                             .settings = settings});
  const std::filesystem::path png_path = output_dir / (label + ".png");
  const std::filesystem::path heatmap_path = output_dir / (label + ".lighting.png");
  aster::writeFrameBufferPng(frame.framebuffer, png_path, width, height);
  writeLightingHeatmap(frame, heatmap_path);
  return analyzeLightingFrame(label, progress, png_path, heatmap_path, frame);
}

void writeLightingJson(const LightingPlaytestMetrics &metrics) {
  const std::filesystem::path json_path = metrics.output_dir / (metrics.route + ".lighting.json");
  std::ofstream file(json_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not write Lumen cave lighting metrics: " + json_path.string());
  }
  file << std::fixed << std::setprecision(6);
  file << "{\n";
  file << "  \"route\": \"" << jsonEscape(metrics.route) << "\",\n";
  file << "  \"accepted\": " << (metrics.accepted ? "true" : "false") << ",\n";
  file << "  \"source_visible_pixels\": " << metrics.source_visible_pixels << ",\n";
  file << "  \"air_scatter_pixels\": " << metrics.air_scatter_pixels << ",\n";
  file << "  \"light_source_unreadable_count\": " << metrics.light_source_unreadable_count << ",\n";
  file << "  \"volumetric_light_missing_count\": " << metrics.volumetric_light_missing_count
       << ",\n";
  file << "  \"light_falloff_discontinuity_count\": "
       << metrics.light_falloff_discontinuity_count << ",\n";
  file << "  \"cave_light_exposure_underflow_count\": "
       << metrics.cave_light_exposure_underflow_count << ",\n";
  file << "  \"cave_light_exposure_overflow_count\": "
       << metrics.cave_light_exposure_overflow_count << ",\n";
  file << "  \"cave_light_exposure_overbright_count\": "
       << metrics.cave_light_exposure_overbright_count << ",\n";
  file << "  \"overexposed_pixels\": " << metrics.overexposed_pixels << ",\n";
  file << "  \"min_frame_mean_luminance\": " << metrics.min_frame_mean_luminance << ",\n";
  file << "  \"max_frame_mean_luminance\": " << metrics.max_frame_mean_luminance << ",\n";
  file << "  \"min_source_mean_luminance\": " << metrics.min_source_mean_luminance << ",\n";
  file << "  \"min_air_scatter_mean_luminance\": " << metrics.min_air_scatter_mean_luminance
       << ",\n";
  file << "  \"min_falloff_continuity_score\": " << metrics.min_falloff_continuity_score
       << ",\n";
  file << "  \"max_temporal_lighting_delta\": " << metrics.max_temporal_lighting_delta << ",\n";
  file << "  \"frames\": [\n";
  for (std::size_t i = 0; i < metrics.frames.size(); ++i) {
    const LightingFrameMetrics &frame = metrics.frames[i];
    file << "    {\"label\": \"" << jsonEscape(frame.label) << "\", \"progress\": "
         << frame.progress << ", \"source_visible_pixels\": " << frame.source_visible_pixels
         << ", \"air_scatter_pixels\": " << frame.air_scatter_pixels
         << ", \"light_source_unreadable_count\": " << frame.light_source_unreadable_count
         << ", \"volumetric_light_missing_count\": " << frame.volumetric_light_missing_count
         << ", \"light_falloff_discontinuity_count\": "
         << frame.light_falloff_discontinuity_count
         << ", \"cave_light_exposure_underflow_count\": "
         << frame.cave_light_exposure_underflow_count
         << ", \"cave_light_exposure_overflow_count\": "
         << frame.cave_light_exposure_overflow_count
         << ", \"cave_light_exposure_overbright_count\": "
         << frame.cave_light_exposure_overbright_count
         << ", \"overexposed_pixels\": " << frame.overexposed_pixels
         << ", \"frame_mean_luminance\": " << frame.frame_mean_luminance
         << ", \"source_mean_luminance\": " << frame.source_mean_luminance
         << ", \"air_scatter_mean_luminance\": " << frame.air_scatter_mean_luminance
         << ", \"surface_direct_mean_luminance\": " << frame.surface_direct_mean_luminance
         << ", \"source_to_air_ratio\": " << frame.source_to_air_ratio
         << ", \"falloff_continuity_score\": " << frame.falloff_continuity_score
         << ", \"temporal_lighting_delta\": " << frame.temporal_lighting_delta
         << ", \"png_path\": \"" << jsonEscape(frame.png_path.string())
         << "\", \"heatmap_path\": \"" << jsonEscape(frame.heatmap_path.string()) << "\"}";
    file << (i + 1u < metrics.frames.size() ? ",\n" : "\n");
  }
  file << "  ]\n";
  file << "}\n";
}

LightingPlaytestMetrics runLumenCaveLightingPlaytest(
    aster::LumenRun &game, const std::string &route, const std::filesystem::path &output_dir,
    const int width, const int height, const aster::RenderStyleProfile &render_style) {
  if (route != "cave-regression") {
    throw std::runtime_error("unknown Lumen Run lighting route '" + route +
                             "'; expected cave-regression");
  }
  std::filesystem::create_directories(output_dir);
  game.setPlayerAvatarVisible(false);
  LightingPlaytestMetrics metrics;
  metrics.route = route;
  metrics.output_dir = output_dir;
  const std::vector<std::pair<std::string, float>> probes = {{"cave_entrance", 3.0f},
                                                             {"web_area", 9.0f},
                                                             {"chamber", 18.0f},
                                                             {"deep_connector", 30.0f},
                                                             {"backtrack", 12.0f}};
  for (const auto &[label, progress] : probes) {
    LightingFrameMetrics frame =
        renderLightingFrame(game, label, progress, width, height, render_style, output_dir);
    metrics.source_visible_pixels += frame.source_visible_pixels;
    metrics.air_scatter_pixels += frame.air_scatter_pixels;
    metrics.light_source_unreadable_count += frame.light_source_unreadable_count;
    metrics.volumetric_light_missing_count += frame.volumetric_light_missing_count;
    metrics.light_falloff_discontinuity_count += frame.light_falloff_discontinuity_count;
    metrics.cave_light_exposure_underflow_count += frame.cave_light_exposure_underflow_count;
    metrics.cave_light_exposure_overflow_count += frame.cave_light_exposure_overflow_count;
    metrics.cave_light_exposure_overbright_count += frame.cave_light_exposure_overbright_count;
    metrics.overexposed_pixels += frame.overexposed_pixels;
    metrics.min_source_mean_luminance =
        std::min(metrics.min_source_mean_luminance, frame.source_mean_luminance);
    metrics.min_air_scatter_mean_luminance =
        std::min(metrics.min_air_scatter_mean_luminance, frame.air_scatter_mean_luminance);
    metrics.min_frame_mean_luminance =
        std::min(metrics.min_frame_mean_luminance, frame.frame_mean_luminance);
    metrics.max_frame_mean_luminance =
        std::max(metrics.max_frame_mean_luminance, frame.frame_mean_luminance);
    metrics.min_falloff_continuity_score =
        std::min(metrics.min_falloff_continuity_score, frame.falloff_continuity_score);
    metrics.max_temporal_lighting_delta =
        std::max(metrics.max_temporal_lighting_delta, frame.temporal_lighting_delta);
    metrics.frames.push_back(std::move(frame));
  }
  metrics.accepted = metrics.light_source_unreadable_count == 0u &&
                     metrics.volumetric_light_missing_count == 0u &&
                     metrics.light_falloff_discontinuity_count == 0u &&
                     metrics.cave_light_exposure_underflow_count == 0u &&
                     metrics.cave_light_exposure_overflow_count == 0u &&
                     metrics.cave_light_exposure_overbright_count == 0u;
  writeLightingJson(metrics);
  return metrics;
}

aster::InventorySlotModel
inventorySlot(std::string label, std::string detail, std::string quantity, const aster::Vec3 tint,
              const bool selected = false, std::string item_id = {},
              const aster::InventorySlotRole role = aster::InventorySlotRole::None) {
  return {.item_id = std::move(item_id),
          .label = std::move(label),
          .detail = std::move(detail),
          .quantity = std::move(quantity),
          .tint = tint,
          .filled = true,
          .selected = selected,
          .role = role};
}

std::vector<aster::InventorySlotModel> paddedSlots(std::vector<aster::InventorySlotModel> slots,
                                                   const std::size_t count) {
  slots.resize(count);
  return slots;
}

void markDropTargets(std::vector<aster::InventorySlotModel> &slots,
                     const aster::InventorySlotRole role) {
  for (aster::InventorySlotModel &slot : slots) {
    slot.role = role;
    slot.drop_target = true;
  }
}

std::vector<aster::InventorySlotModel>
inventoryHotbarSlots(const aster::HotbarHudModel &hotbar) {
  std::vector<aster::InventorySlotModel> slots;
  slots.reserve(hotbar.slots.size());
  for (const aster::HotbarSlotModel &hotbar_slot : hotbar.slots) {
    aster::InventorySlotModel slot =
        hotbar_slot.filled
            ? inventorySlot(hotbar_slot.label, hotbar_slot.key, hotbar_slot.quantity,
                            hotbar_slot.tint, hotbar_slot.selected, {},
                            aster::InventorySlotRole::PlayerHotbar)
            : inventorySlot(hotbar_slot.key, "empty", "", hotbar_slot.tint, hotbar_slot.selected,
                            {}, aster::InventorySlotRole::PlayerHotbar);
    slot.filled = hotbar_slot.filled;
    slot.drop_target = true;
    slots.push_back(std::move(slot));
  }
  return slots;
}

aster::InventoryOverlayModel inventoryModel(const aster::LumenStatus &status, const bool open,
                                            const int torch_count,
                                            const bool supply_crate_nearby,
                                            const aster::HotbarHudModel &hotbar) {
  aster::InventoryOverlayModel inventory;
  inventory.open = open;
  inventory.world_character_preview = true;
  inventory.title = "Inventory";
  inventory.subtitle = "Tab";
  inventory.character_name = "Wool runner";
  inventory.character_status =
      "Shards " + std::to_string(status.score) + " / " + std::to_string(status.total_shards);
  inventory.search_hint = "Search craftables";
  inventory.equipment_slots = {
      inventorySlot("Head", "soft ears", "", {0.40f, 0.24f, 0.13f}),
      inventorySlot("Body", "wool coat", "", {0.46f, 0.28f, 0.15f}, true),
      inventorySlot("Hands", "small paws", "", {0.38f, 0.22f, 0.12f}),
      inventorySlot("Feet", "soft feet", "", {0.34f, 0.19f, 0.10f}),
  };
  inventory.backpack.title = "Backpack";
  inventory.backpack.columns = 6;
  inventory.backpack.slots = paddedSlots(
      {inventorySlot("Amber", "recovered shards", std::to_string(status.score),
                     {0.82f, 0.50f, 0.18f}, true),
       inventorySlot("Torch", "carried supply", std::to_string(torch_count), {0.98f, 0.52f, 0.16f},
                     false, "torch", aster::InventorySlotRole::PlayerBackpack),
       inventorySlot("Thread", "repair fiber", "3", {0.62f, 0.48f, 0.32f}),
       inventorySlot("Core", "signal relay", "1", {0.18f, 0.56f, 0.58f}),
       inventorySlot("Map", "station court", "1", {0.28f, 0.38f, 0.46f}),
       inventorySlot("Lamp", "warm light", "1", {0.78f, 0.55f, 0.24f}),
       inventorySlot("Snack", "focus", "2", {0.45f, 0.38f, 0.22f})},
      24u);
  markDropTargets(inventory.backpack.slots, aster::InventorySlotRole::PlayerBackpack);
  inventory.hotbar.title = "Hotbar";
  inventory.hotbar.columns = 6;
  inventory.hotbar.slots = paddedSlots(inventoryHotbarSlots(hotbar), 6u);
  markDropTargets(inventory.hotbar.slots, aster::InventorySlotRole::PlayerHotbar);
  if (supply_crate_nearby) {
    aster::InventorySlotModel torch_supply =
        inventorySlot("Torch", "unlimited", "", {0.98f, 0.52f, 0.16f}, false, "torch",
                      aster::InventorySlotRole::Supply);
    torch_supply.infinite_quantity = true;
    torch_supply.draggable = true;
    inventory.secondary_inventory_visible = true;
    inventory.secondary_tab = "Supply";
    inventory.secondary_inventory_title = "Supply Crate";
    inventory.secondary_inventory_status = "Drag torch into your pack or hotbar.";
    inventory.secondary_inventory = {.title = "Crate", .columns = 1, .slots = {torch_supply}};
  }
  inventory.recipes = {
      {"Signal flare", "2 Amber + 1 Thread", status.score >= 2},
      {"Soft patch", "1 Thread", true},
      {"Gate marker", "4 Amber", status.score >= 4},
      {"Lantern polish", "1 Amber", status.score >= 1},
  };
  return inventory;
}

aster::HudModel hudModel(const aster::LumenStatus &status, const bool inventory_open,
                         const int torch_count, const bool supply_crate_nearby,
                         const bool pause_open, const bool pause_options_open,
                         const aster::PointerCueModel pointer,
                         const aster::GameCursorModel game_cursor,
                         const aster::FocusPromptModel focus_prompt, aster::HotbarHudModel hotbar,
                         aster::ChestContentsHudModel chest_contents,
                         const aster::AutomapModel &automap,
                         const aster::ClassicHudSignalModel classic_signals,
                         const aster::TransitionWipeFrame transition_wipe) {
  aster::HudModel model;
  model.title = "Lumen Run";
  model.subtitle =
      "Recover amber shards across the floodlit station court before the sentinels close in.";
  model.score = status.score;
  model.total = status.total_shards;
  model.lives = status.lives;
  model.health = status.health;
  model.max_health = status.max_health;
  model.elapsed_seconds = status.elapsed_seconds;
  model.victory = status.victory;
  model.defeated = status.defeated;
  model.controls.button_label = "Controls";
  model.controls.title = "Run controls";
  model.controls.subtitle = "Keyboard bindings";
  model.controls.footer = "Terminal: ./build/aster_lumen_run";
  model.controls.open_by_default = false;
  model.controls.entries = {
      {"W", "Forward", true},        {"A", "Left", true},
      {"S", "Back", true},           {"D", "Right", true},
      {"Space", "Jump / Fork up", true},      {"Left Shift", "Run / Fork down", true},
      {"Mouse", "Look", true},       {"E", "Interact", true},
      {"1-6", "Equip", true},        {"Command+Click", "Point", false},
      {"Mouse Wheel", "Zoom", true}, {"Tab", "Inventory", false},
      {"R", "Restart", false},       {"Esc", "Menu", false},
  };
  model.inventory =
      inventoryModel(status, inventory_open, torch_count, supply_crate_nearby, hotbar);
  model.visibility = aster::hudVisibilityForState(
      {.inventory_open = inventory_open, .pause_open = pause_open, .defeated = status.defeated});
  model.pointer = pointer;
  model.game_cursor = game_cursor;
  model.game_cursor.visible = model.game_cursor.visible && model.visibility.game_cursor;
  model.pause.open = pause_open;
  model.pause.options_open = pause_options_open;
  model.focus_prompt = focus_prompt;
  hotbar.visible = hotbar.visible && model.visibility.hotbar;
  chest_contents.visible = chest_contents.visible && !inventory_open && !pause_open &&
                           !status.defeated;
  model.hotbar = std::move(hotbar);
  model.chest_contents = std::move(chest_contents);
  model.automap = {.visible = classic_signals.visible || transition_wipe.active, .map = automap};
  model.classic_signals = classic_signals;
  model.transition_wipe = transition_wipe;
  return model;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const std::filesystem::path screenshot_path = argumentPath(argc, argv, "--screenshot");
    const std::filesystem::path sequence_path = argumentPath(argc, argv, "--capture-sequence");
    const std::filesystem::path profile_capture_path =
        argumentPath(argc, argv, "--profile-capture");
    const int sequence_frames = argumentInt(argc, argv, "--capture-frames", 144);
    const std::string capture_route = argumentString(argc, argv, "--capture-route", "attract");
    const int run_frames = argumentInt(argc, argv, "--run-frames", 0);
    const int screenshot_frame = std::max(0, argumentInt(argc, argv, "--screenshot-frame", 80));
    const bool capture_hud = hasArgument(argc, argv, "--capture-hud");
    const bool smoke_test = hasArgument(argc, argv, "--smoke-test");
    const bool validate_cave = hasArgument(argc, argv, "--validate-cave");
    const bool playtest_vision = hasArgument(argc, argv, "--playtest-vision");
    const std::string vision_route = argumentString(argc, argv, "--vision-route", "cave-regression");
    std::filesystem::path vision_out = argumentPath(argc, argv, "--vision-out");
    if (vision_out.empty()) {
      vision_out = std::filesystem::path("build") / "tmp" / "lumen_run_cave_vision";
    }
    const int vision_width = std::max(64, argumentInt(argc, argv, "--vision-width", 640));
    const int vision_height = std::max(64, argumentInt(argc, argv, "--vision-height", 360));
    const bool playtest_lighting = hasArgument(argc, argv, "--playtest-lighting");
    const std::string lighting_route =
        argumentString(argc, argv, "--lighting-route", "cave-regression");
    std::filesystem::path lighting_out = argumentPath(argc, argv, "--lighting-out");
    if (lighting_out.empty()) {
      lighting_out = std::filesystem::path("build") / "tmp" / "lumen_run_cave_lighting";
    }
    const int lighting_width = std::max(64, argumentInt(argc, argv, "--lighting-width", 640));
    const int lighting_height = std::max(64, argumentInt(argc, argv, "--lighting-height", 360));
    const bool debug_cave_overlay = hasArgument(argc, argv, "--debug-cave-overlay");
    const bool profile_enabled =
        hasArgument(argc, argv, "--profile") || !profile_capture_path.empty();
    const bool frame_report_enabled = hasArgument(argc, argv, "--frame-report");
    const bool full_frame_forensics = hasArgument(argc, argv, "--full-frame-forensics");
    const std::string frame_report_route =
        argumentString(argc, argv, "--frame-report-route", "interactive");
    const bool startup_report_enabled = hasArgument(argc, argv, "--startup-report");
    const bool open_chest_for_capture = hasArgument(argc, argv, "--open-chest");
    const bool activate_prism_relay_for_capture =
        hasArgument(argc, argv, "--activate-prism-relay");
    const bool player_at_prism_relay_for_capture =
        hasArgument(argc, argv, "--player-at-prism-relay");
    const bool player_at_supply_crate_for_capture =
        hasArgument(argc, argv, "--player-at-supply-crate");
    const bool player_position_override = hasArgument(argc, argv, "--player-x") ||
                                          hasArgument(argc, argv, "--player-y") ||
                                          hasArgument(argc, argv, "--player-z");
    const std::string take_chest_item_for_capture = argumentString(argc, argv, "--take-chest-item");
    const int select_hotbar_for_capture = argumentInt(argc, argv, "--select-hotbar", 0);
    const int frame_report_warmup =
        std::max(0, argumentInt(argc, argv, "--frame-report-warmup", 0));
    const float lag_budget_ms = argumentFloat(argc, argv, "--lag-budget-ms", -1.0f);
    const std::optional<double> lag_budget_seconds =
        lag_budget_ms > 0.0f ? std::optional<double>{static_cast<double>(lag_budget_ms) / 1000.0}
                             : std::nullopt;
    const bool sequence_capture = !sequence_path.empty();
    const bool scripted_frame_report_route =
        frame_report_enabled && frame_report_route != "interactive";
    const std::string playback_route =
        scripted_frame_report_route ? frame_report_route : capture_route;
    const bool cave_entry_capture = playback_route == "cave-entry";
    const bool deep_cave_capture = playback_route == "deep-cave";
    const bool deep_cave_stress_capture = playback_route == "deep-cave-stress";
    const bool classic_gauntlet_capture = playback_route == "classic-gauntlet";
    const bool construction_yard_seat_capture = playback_route == "construction-yard-seat";
    const bool construction_yard_shred_capture = playback_route == "construction-yard-shred";
    const bool construction_yard_exit_capture = playback_route == "construction-yard-exit";
    const bool construction_yard_capture = playback_route == "construction-yard" ||
                                           construction_yard_seat_capture ||
                                           construction_yard_shred_capture ||
                                           construction_yard_exit_capture;
    const float deep_cave_capture_progress =
        argumentFloat(argc, argv, "--deep-cave-progress", 16.0f);
    const float deep_cave_capture_look_ahead =
        argumentFloat(argc, argv, "--deep-cave-look-ahead", 1.0f);
    const bool scripted_capture =
        !screenshot_path.empty() || sequence_capture || scripted_frame_report_route;
    const bool unlocked = hasArgument(argc, argv, "--unlocked");
    const bool no_vsync = hasArgument(argc, argv, "--no-vsync");
    const std::string render_style_name = argumentString(argc, argv, "--render-style", "neutral");
    const std::optional<aster::RenderStylePreset> render_style_preset =
        aster::parseRenderStylePreset(render_style_name);
    if (!render_style_preset.has_value()) {
      throw std::runtime_error("unknown render style '" + render_style_name +
                               "'; expected neutral or retro-horror");
    }
    const aster::RenderStyleProfile render_style =
        aster::makeRenderStyleProfile(*render_style_preset);
    const double target_frame_seconds = (!scripted_capture && !unlocked && !frame_report_enabled)
                                            ? kDefaultInteractiveFrameCapSeconds
                                            : 0.0;
    aster::Clock startup_clock;
    double startup_previous = startup_clock.now();
    std::vector<StartupSample> startup_samples;
    const auto mark_startup = [&](std::string label) {
      if (!startup_report_enabled) {
        return;
      }
      const double now = startup_clock.now();
      startup_samples.push_back({std::move(label), now - startup_previous});
      startup_previous = now;
    };

    std::filesystem::path project_path = argumentPath(argc, argv, "--project");
    if (project_path.empty()) {
      project_path = defaultProjectPath();
    }
    std::vector<aster::sdk::Diagnostic> authoring_diagnostics;
    aster::LumenAuthoringData authoring = loadLumenAuthoring(project_path, authoring_diagnostics);
    printAuthoringDiagnostics(authoring_diagnostics);
    if (!project_path.empty()) {
      if (hasAuthoringErrors(authoring_diagnostics) || !authoring.valid) {
        throw std::runtime_error("failed to validate Lumen Run cave authoring: " +
                                 project_path.string());
      }
      mark_startup("project_authoring_load");
    }
    if (validate_cave) {
      aster::LumenRun validation_game(authoring);
      printLumenWorldGateReport(validation_game.caveWorldGateReport());
      if (!validation_game.caveWorldGateAccepted()) {
        throw std::runtime_error("Lumen Run cave world gate rejected publish: " +
                                 validation_game.caveWorldGateReport().diagnostic);
      }
      std::cout << "Lumen Run cave validation passed before render extraction: "
                << (authoring.cave.id.empty() ? "fallback generated cave" : authoring.cave.id)
                << '\n';
      return 0;
    }
    if (playtest_vision) {
      aster::LumenRun vision_game(authoring);
      if (!vision_game.caveWorldGateAccepted()) {
        throw std::runtime_error("Lumen Run cave world gate rejected vision playtest: " +
                                 vision_game.caveWorldGateReport().diagnostic);
      }
      const VisionPlaytestMetrics metrics =
          runLumenCaveVisionPlaytest(vision_game, vision_route, vision_out, vision_width,
                                     vision_height, render_style);
      std::cout << "Lumen Run cave vision route '" << metrics.route
                << "' wrote artifacts to " << metrics.output_dir << '\n';
      std::cout << "  visible_void_rays=" << metrics.visible_void_rays
                << " zfight_candidate_pixels=" << metrics.zfight_candidate_pixels
                << " max_support_render_delta_m=" << metrics.max_support_render_delta_m
                << " blocked_frames=" << metrics.blocked_frames
                << " respawn_count=" << metrics.respawn_count
                << " upper_terrain_snap_count=" << metrics.upper_terrain_snap_count << '\n';
      if (!metrics.accepted) {
        throw std::runtime_error("Lumen Run cave vision playtest failed; inspect " +
                                 (metrics.output_dir / (metrics.route + ".json")).string());
      }
      return 0;
    }
    if (playtest_lighting) {
      aster::LumenRun lighting_game(authoring);
      if (!lighting_game.caveWorldGateAccepted()) {
        throw std::runtime_error("Lumen Run cave world gate rejected lighting playtest: " +
                                 lighting_game.caveWorldGateReport().diagnostic);
      }
      const LightingPlaytestMetrics metrics = runLumenCaveLightingPlaytest(
          lighting_game, lighting_route, lighting_out, lighting_width, lighting_height, render_style);
      std::cout << "Lumen Run cave lighting route '" << metrics.route
                << "' wrote artifacts to " << metrics.output_dir << '\n';
      std::cout << "  source_visible_pixels=" << metrics.source_visible_pixels
                << " air_scatter_pixels=" << metrics.air_scatter_pixels
                << " light_source_unreadable_count=" << metrics.light_source_unreadable_count
                << " volumetric_light_missing_count=" << metrics.volumetric_light_missing_count
                << " light_falloff_discontinuity_count="
                << metrics.light_falloff_discontinuity_count
                << " cave_light_exposure_underflow_count="
                << metrics.cave_light_exposure_underflow_count
                << " cave_light_exposure_overflow_count="
                << metrics.cave_light_exposure_overflow_count
                << " cave_light_exposure_overbright_count="
                << metrics.cave_light_exposure_overbright_count
                << " max_frame_mean_luminance=" << metrics.max_frame_mean_luminance
                << " overexposed_pixels=" << metrics.overexposed_pixels << '\n';
      if (!metrics.accepted) {
        throw std::runtime_error("Lumen Run cave lighting playtest failed; inspect " +
                                 (metrics.output_dir / (metrics.route + ".lighting.json")).string());
      }
      return 0;
    }

    aster::EngineConfig config;
    config.application_name = "Aster Learning Engine - Lumen Run";
    config.initial_width = argumentInt(argc, argv, "--window-width",
                                       scripted_capture ? 1600 : kInteractiveWindowWidth);
    config.initial_height = argumentInt(argc, argv, "--window-height",
                                        scripted_capture ? 900 : kInteractiveWindowHeight);
    config.multisample_samples = argumentInt(argc, argv, "--msaa", scripted_capture ? 4 : 0);
    config.enable_vsync = !scripted_capture && !unlocked && !frame_report_enabled && !no_vsync;

    aster::Window window(config);
    mark_startup("window");
    aster::RenderDevice renderer;
    renderer.initialize();
    mark_startup("renderer_initialize");

    if (profile_enabled) {
      aster::profile::startCapture();
    }

    aster::LumenRun game(authoring);
    game.setCaveDebugOverlayEnabled(debug_cave_overlay);
    if (debug_cave_overlay) {
      game.setCaveDebugOverlayLayerMask(caveOverlayAllLayerMask());
    }
    if (deep_cave_capture || deep_cave_stress_capture || classic_gauntlet_capture) {
      game.setPlayerAvatarVisible(false);
    }
    mark_startup("game_reset");
    if (!game.caveWorldGateAccepted()) {
      throw std::runtime_error("refusing to publish Lumen Run generated cave region: " +
                               game.caveWorldGateReport().diagnostic);
    }
    if (cave_entry_capture && !player_position_override) {
      const aster::Vec3 crate = game.supplyCratePosition();
      game.relocatePlayer(crate + aster::Vec3{1.10f, 0.0f, 1.00f},
                          aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", 180.0f)));
    } else if ((deep_cave_capture || deep_cave_stress_capture) &&
               !player_position_override) {
      game.relocatePlayer(
          game.caveFrameReportPosition(deep_cave_stress_capture ? kDeepCaveStressStartProgress
                                                                 : deep_cave_capture_progress),
          aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", 0.0f)));
    } else if (classic_gauntlet_capture && !player_position_override) {
      game.relocatePlayer(game.classicGauntletEntryPosition(),
                          game.classicGauntletCameraYaw());
    } else if (construction_yard_capture && !player_position_override) {
      const aster::Vec3 forklift = game.constructionForkliftPosition();
      game.relocatePlayer(forklift + aster::Vec3{-0.95f, 0.0f, -0.35f}, aster::radians(90.0f));
      const aster::Vec3 focus_origin = game.playerPosition() + aster::Vec3{0.0f, 0.62f, 0.0f};
      game.updateInteractionFocus(focus_origin, aster::normalize(forklift - focus_origin),
                                  1.0f / 60.0f);
      game.interactFocused();
      const auto advance_construction_capture = [&](const int frame_index) {
        aster::Vec2 yard_axis{};
        if (!construction_yard_seat_capture) {
          const bool shredding_started = game.constructionShredderActive() ||
                                         game.constructionShredderConsumedPipeCount() > 0;
          yard_axis = shredding_started ? aster::Vec2{} : aster::Vec2{0.0f, 1.0f};
        }
        game.update(1.0f / 60.0f, yard_axis, false,
                    !construction_yard_seat_capture && frame_index < 44);
      };
      if (construction_yard_seat_capture) {
        for (int i = 0; i < 10; ++i) {
          advance_construction_capture(i);
        }
      } else if (construction_yard_shred_capture) {
        for (int i = 0; i < 190; ++i) {
          advance_construction_capture(i);
          if (game.constructionShredderConsumedPipeCount() >= 2 &&
              game.constructionScrapFragmentCount() > 0u) {
            break;
          }
        }
      } else {
        for (int i = 0; i < 150; ++i) {
          advance_construction_capture(i);
        }
      }
      if (construction_yard_exit_capture && game.constructionForkliftMounted()) {
        const aster::Vec3 exit_focus =
            game.constructionForkliftPosition() + aster::Vec3{0.0f, 1.20f, 0.0f};
        game.updateInteractionFocus(game.playerPosition() + aster::Vec3{0.0f, 0.42f, 0.0f},
                                    aster::normalize(exit_focus - game.playerPosition()),
                                    1.0f / 60.0f);
        game.interactFocused();
        for (int i = 0; i < 18; ++i) {
          game.update(1.0f / 60.0f, {}, false, false);
        }
      }
    } else if (player_at_prism_relay_for_capture) {
      const aster::Vec3 base = game.prismRelayBasePosition();
      game.relocatePlayer(base + aster::Vec3{1.45f, 0.0f, 1.10f},
                          aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", -132.0f)));
    } else if (player_at_supply_crate_for_capture) {
      game.relocatePlayer(game.supplyCratePosition(),
                          aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", 0.0f)));
    } else if (player_position_override) {
      const aster::Vec3 player = game.playerPosition();
      game.relocatePlayer({argumentFloat(argc, argv, "--player-x", player.x),
                           argumentFloat(argc, argv, "--player-y", player.y),
                           argumentFloat(argc, argv, "--player-z", player.z)},
                          aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", 0.0f)));
    }
    if (open_chest_for_capture || !take_chest_item_for_capture.empty()) {
      game.openChest();
    }
    if (!take_chest_item_for_capture.empty()) {
      (void)game.takeChestItem(take_chest_item_for_capture);
    }
    if (select_hotbar_for_capture > 0) {
      game.selectHotbarSlot(static_cast<std::size_t>(select_hotbar_for_capture - 1));
    }
    if (activate_prism_relay_for_capture) {
      game.activatePrismRelay();
    }
    renderer.prepareScene(game.scene());
    mark_startup("prepare_scene");
    aster::ControlScheme control_scheme = makeRunControls();
    aster::ControlState control_state;
    aster::OrbitCamera camera;
    camera.pitch = kGameplayCameraPitch;
    camera.yaw = kGameplayCameraYaw;
    camera.radius = 7.2f;
    aster::Vec3 scripted_camera_target = {2.25f, 0.48f, -0.95f};
    if (scripted_capture) {
      const aster::Vec3 player = game.playerPosition();
      scripted_camera_target =
          cave_entry_capture
              ? caveEntryCameraTarget(player, 0.0f)
              : (classic_gauntlet_capture
                     ? game.classicGauntletLookTarget()
                     : ((deep_cave_capture || deep_cave_stress_capture)
                     ? game.caveFrameReportLookTarget(
                           deep_cave_stress_capture ? kDeepCaveStressStartProgress
                                                    : deep_cave_capture_progress,
                           deep_cave_capture_look_ahead)
                     : aster::Vec3{argumentFloat(argc, argv, "--camera-target-x", 2.25f),
                                   argumentFloat(argc, argv, "--camera-target-y", 0.48f),
                                   argumentFloat(argc, argv, "--camera-target-z", -0.95f)}));
      if (construction_yard_capture) {
        if (construction_yard_seat_capture) {
          scripted_camera_target =
              game.constructionForkliftPosition() + aster::Vec3{-0.42f, 1.20f, -0.20f};
        } else if (construction_yard_exit_capture) {
          scripted_camera_target = game.playerPosition() + aster::Vec3{0.0f, 0.42f, 0.0f};
        } else if (construction_yard_shred_capture) {
          scripted_camera_target =
              game.constructionShredderPosition() + aster::Vec3{1.10f, 0.72f, -0.10f};
        } else {
          scripted_camera_target =
              (game.constructionForkliftPosition() + game.constructionPalletPosition() +
               game.constructionShredderPosition()) /
                  3.0f +
              aster::Vec3{0.0f, 0.70f, 0.0f};
        }
      }
      float default_camera_pitch_deg = 28.0f;
      float default_camera_yaw_deg = -31.0f;
      float default_camera_radius = 7.8f;
      float default_camera_fov_deg = 54.0f;
      if (construction_yard_seat_capture) {
        default_camera_pitch_deg = 6.0f;
        default_camera_yaw_deg = -24.0f;
        default_camera_radius = 3.20f;
        default_camera_fov_deg = 42.0f;
      } else if (construction_yard_exit_capture) {
        default_camera_pitch_deg = 7.0f;
        default_camera_yaw_deg = 70.0f;
        default_camera_radius = 4.60f;
        default_camera_fov_deg = 46.0f;
      } else if (construction_yard_shred_capture) {
        default_camera_pitch_deg = 9.0f;
        default_camera_yaw_deg = 20.0f;
        default_camera_radius = 6.00f;
        default_camera_fov_deg = 46.0f;
      } else if (construction_yard_capture) {
        default_camera_pitch_deg = 11.0f;
        default_camera_yaw_deg = -62.0f;
        default_camera_radius = 7.6f;
        default_camera_fov_deg = 50.0f;
      } else if (cave_entry_capture) {
        default_camera_pitch_deg = 12.0f;
        default_camera_yaw_deg = 0.0f;
        default_camera_radius = 7.2f;
        default_camera_fov_deg = 46.0f;
      } else if (classic_gauntlet_capture) {
        default_camera_pitch_deg = 8.0f;
        default_camera_yaw_deg = aster::degrees(game.classicGauntletCameraYaw());
        default_camera_radius = 5.4f;
      } else if (deep_cave_capture || deep_cave_stress_capture) {
        default_camera_pitch_deg = 6.0f;
        default_camera_yaw_deg = 180.0f;
        default_camera_radius = 2.70f;
      }
      camera.pitch =
          aster::radians(argumentFloat(argc, argv, "--camera-pitch-deg",
                                       default_camera_pitch_deg));
      camera.yaw =
          aster::radians(argumentFloat(argc, argv, "--camera-yaw-deg", default_camera_yaw_deg));
      camera.radius = argumentFloat(argc, argv, "--camera-radius", default_camera_radius);
      camera.vertical_fov = aster::radians(std::clamp(
          argumentFloat(argc, argv, "--camera-fov-deg", default_camera_fov_deg),
          18.0f,
          72.0f));
    }
    float gameplay_camera_radius = camera.radius;
    float inventory_camera_radius = 2.25f;

    aster::RendererSettings settings;
    settings.forensics.detailed_traces = full_frame_forensics;
    settings.forensics.capture_payloads = full_frame_forensics;
    settings.forensics.backend_certification = full_frame_forensics;
    settings.procedural_surface_normals = !hasArgument(argc, argv, "--flat-surface-normals") ||
                                          hasArgument(argc, argv, "--surface-normal-detail");
    settings.exposure = 1.34f;
    settings.ambient_strength = 0.36f;
    settings.ambient_floor = 0.072f;
    settings.sky_ambient_color = {0.48f, 0.56f, 0.66f};
    settings.ground_ambient_color = {0.25f, 0.24f, 0.19f};
    settings.sun_light.enabled = true;
    settings.sun_light.direction_to_light = {-0.46f, 0.86f, 0.30f};
    settings.sun_light.color = {1.00f, 0.84f, 0.58f};
    settings.sun_light.intensity = 1.72f;
    settings.light_rig = {{{-4.6f, 3.2f, 2.8f}, {4.4f, 3.25f, 2.05f}, 0.28f, 3.0f},
                          {{4.8f, 2.4f, -3.4f}, {1.15f, 1.35f, 2.10f}, 0.22f, 3.5f},
                          {{0.0f, 2.8f, -5.8f}, {1.35f, 1.02f, 0.72f}, 0.16f, 4.0f},
                          {{0.0f, 2.0f, 4.8f}, {0.76f, 0.86f, 1.12f}, 0.13f, 3.5f}};
    settings.clustered_lighting.enabled = true;
    settings.clustered_lighting.cluster_count_x = 10u;
    settings.clustered_lighting.cluster_count_y = 5u;
    settings.clustered_lighting.cluster_count_z = 10u;
    settings.clustered_lighting.max_visible_lights = 48u;
    settings.clustered_lighting.max_lights_per_cluster = 10u;
    settings.light_policy.max_point_lights = settings.clustered_lighting.max_visible_lights;
    settings.pipeline.clear_color = {0.092f, 0.122f, 0.158f};
    settings.pipeline.multisampling = config.multisample_samples > 0;
    settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.surface_occlusion_strength = 0.34f;
    settings.grounding.surface_occlusion_height = 1.05f;
    settings.grounding.surface_occlusion_mix = 0.28f;
    settings.grounding.surface_occlusion_min = 0.78f;
    settings.grounding.contact_shadow_strength = 0.24f;
    settings.grounding.contact_shadow_radius_scale = 1.12f;
    settings.grounding.contact_shadow_max_radius = 1.18f;
    settings.grounding.contact_shadow_receiver_height = 1.06f;
    settings.grounding.contact_shadow_receiver_bias = 0.020f;
    settings.grounding.contact_shadow_detail_scale = 10.0f;
    settings.atmosphere.enabled = true;
    settings.atmosphere.fog_color = {0.175f, 0.205f, 0.250f};
    settings.atmosphere.fog_start = 8.0f;
    settings.atmosphere.fog_end = 28.0f;
    settings.atmosphere.fog_strength = 0.10f;
    settings.atmosphere.saturation = 1.22f;
    settings.atmosphere.contrast = 1.10f;
    settings.atmosphere.shadow_tint = {0.52f, 0.64f, 0.86f};
    settings.atmosphere.shadow_tint_strength = 0.16f;
    settings.atmosphere.highlight_tint = {1.10f, 1.02f, 0.82f};
    settings.atmosphere.highlight_tint_strength = 0.10f;
    settings.style = render_style;
    const RenderEnvironmentBaseline base_render_environment{
        .light_rig = settings.light_rig,
        .sun_light = settings.sun_light,
        .ambient_strength = settings.ambient_strength,
        .ambient_floor = settings.ambient_floor,
        .sky_ambient_color = settings.sky_ambient_color,
        .ground_ambient_color = settings.ground_ambient_color,
        .atmosphere = settings.atmosphere,
        .clear_color = settings.pipeline.clear_color,
        .exposure = settings.exposure,
        .indirect_albedo_floor = settings.indirect_albedo_floor};

    aster::HudLayer hud;
    hud.initialize();
    mark_startup("hud_initialize");

    aster::Clock clock;
    aster::FrameTimeStats frame_times;
    aster::FrameTimeStats update_times;
    aster::FrameTimeStats render_times;
    aster::FrameTimeStats hud_times;
    aster::FrameTimeStats swap_times;
    std::size_t render_counter_samples = 0;
    double visible_object_sum = 0.0;
    double culled_object_sum = 0.0;
    double draw_call_sum = 0.0;
    double instance_group_sum = 0.0;
    double lod_culled_object_sum = 0.0;
    double visibility_hint_object_sum = 0.0;
    double dynamic_mesh_object_sum = 0.0;
    double dynamic_mesh_cache_entry_sum = 0.0;
    double pipeline_switch_sum = 0.0;
    double material_permutation_sum = 0.0;
    double material_cache_hit_sum = 0.0;
    double material_cache_miss_sum = 0.0;
    double rust_plan_seconds_sum = 0.0;
    double render_encode_seconds_sum = 0.0;
    double active_point_light_sum = 0.0;
    double clustered_light_cluster_sum = 0.0;
    double clustered_light_assignment_sum = 0.0;
    double clustered_visible_light_sum = 0.0;
    double frame_forensics_pass_sum = 0.0;
    double frame_forensics_resource_sum = 0.0;
    double frame_forensics_material_sum = 0.0;
    double clustered_light_fallback_sum = 0.0;
    double world_linked_frame_sum = 0.0;
    double world_navigation_valid_sum = 0.0;
    double world_perceptual_salience_sum = 0.0;
    aster::FixedTimestep simulation_clock(
        {kSimulationStepSeconds, kMaxSimulatedFrameSeconds, kMaxSimulationStepsPerFrame});
    int rendered_frames = 0;
    bool captured = false;
    bool inventory_open = hasArgument(argc, argv, "--open-inventory");
    bool pause_open =
        hasArgument(argc, argv, "--open-menu") || hasArgument(argc, argv, "--open-options");
    bool pause_options_open = hasArgument(argc, argv, "--open-options");
    float inventory_preview_yaw = 0.0f;
    bool jump_buffered = false;
    aster::ThirdPersonFollowState camera_follow_state;
    aster::ThirdPersonFollowPose camera_follow_pose;
    aster::Vec2 previous_pointer{};
    bool have_previous_pointer = false;
    aster::CursorMode applied_cursor_mode = aster::CursorMode::Normal;
    if (!pause_open && !inventory_open && !scripted_capture) {
      window.setCursorMode(aster::CursorMode::Disabled);
      applied_cursor_mode = aster::CursorMode::Disabled;
    }
    while (window.isOpen()) {
      window.pollEvents();
      const double raw_frame_dt = clock.tick();
      const double frame_start_seconds = clock.now();
      const bool collect_frame_sample =
          frame_report_enabled && rendered_frames > frame_report_warmup;
      if (profile_enabled) {
        ASTER_PROFILE_FRAME("Lumen Run");
      }
      double frame_dt = raw_frame_dt;
      double elapsed = clock.now();
      if (scripted_capture) {
        frame_dt = 1.0 / 60.0;
        elapsed = static_cast<double>(rendered_frames) / 60.0;
      }

      control_state.update(control_scheme, window.captureControls(control_scheme));
      if (control_state.justPressed(kPause) && !scripted_capture) {
        pause_open = !pause_open;
        if (pause_open) {
          inventory_open = false;
        } else {
          pause_options_open = false;
        }
        jump_buffered = false;
        simulation_clock.reset();
      }
      if (control_state.justPressed(kReset)) {
        game.reset();
        renderer.prepareScene(game.scene());
        simulation_clock.reset();
        camera_follow_state = {};
        camera_follow_pose = {};
        have_previous_pointer = false;
        jump_buffered = false;
        pause_open = false;
        pause_options_open = false;
      }
      if (!pause_open && control_state.justPressed(kInventory)) {
        inventory_open = !inventory_open;
      }
      if (!scripted_capture) {
        if (control_state.justPressed(kCaveDebugOverlayToggle)) {
          game.setCaveDebugOverlayEnabled(!game.caveDebugOverlayEnabled());
          if (game.caveDebugOverlayEnabled() && game.caveDebugOverlayLayerMask() == 0u) {
            game.setCaveDebugOverlayLayerMask(caveOverlayAllLayerMask());
          }
          renderer.prepareScene(game.scene());
        }
        if (game.caveDebugOverlayEnabled()) {
          bool cave_overlay_layers_changed = false;
          if (control_state.justPressed(kCaveDebugOverlayCollision)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::Collision);
            cave_overlay_layers_changed = true;
          }
          if (control_state.justPressed(kCaveDebugOverlayInteractable)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::Interactable);
            cave_overlay_layers_changed = true;
          }
          if (control_state.justPressed(kCaveDebugOverlayMining)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::MiningTarget);
            cave_overlay_layers_changed = true;
          }
          if (control_state.justPressed(kCaveDebugOverlaySpawn)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::SpawnVolume);
            cave_overlay_layers_changed = true;
          }
          if (control_state.justPressed(kCaveDebugOverlayCamera)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::CameraObstruction);
            cave_overlay_layers_changed = true;
          }
          if (control_state.justPressed(kCaveDebugOverlayWalkable)) {
            toggleCaveOverlayLayer(game, aster::CaveDebugOverlayLayer::Walkable);
            cave_overlay_layers_changed = true;
          }
          if (cave_overlay_layers_changed) {
            renderer.prepareScene(game.scene());
          }
        }
      }
      const float scroll_y = control_state.snapshot().scroll.y;
      if (!pause_open && !scripted_capture && scroll_y != 0.0f) {
        if (inventory_open) {
          inventory_camera_radius =
              std::clamp(inventory_camera_radius - scroll_y * 0.18f, 1.45f, 3.20f);
        } else {
          gameplay_camera_radius =
              std::clamp(gameplay_camera_radius - scroll_y * 0.55f, 3.20f, 12.80f);
        }
      }
      const aster::Vec2 pointer = control_state.snapshot().pointer;
      const bool have_pointer_delta = have_previous_pointer;
      const aster::Vec2 pointer_delta =
          have_pointer_delta ? pointer - previous_pointer : aster::Vec2{};
      if (inventory_open && have_previous_pointer && control_state.pressed(kInventoryRotate)) {
        inventory_preview_yaw += (pointer.x - previous_pointer.x) * 0.008f;
      }
      if (inventory_open && control_state.justPressed(kInventoryRecenter)) {
        inventory_preview_yaw = 0.0f;
      }
      previous_pointer = pointer;
      have_previous_pointer = true;
      const bool chest_ui_active =
          game.chestInterfaceOpen() && !pause_open && !inventory_open && !scripted_capture;
      const bool command_aim_active = !pause_open && !inventory_open && !scripted_capture &&
                                      !chest_ui_active && control_state.pressed(kCommandAim);
      if (command_aim_active && control_state.pressed(kCameraOrbit)) {
        const auto [window_width, window_height] = window.windowSize();
        const aster::Viewport viewport{{},
                                       {static_cast<float>(window_width),
                                        static_cast<float>(window_height)}};
        const aster::CameraRay ray =
            camera.screenRay({pointer.x, pointer.y, 0.0f}, viewport);
        (void)game.pointAvatarAtRay(ray.origin.value, ray.direction.value);
      }
      const bool camera_follow_active = !pause_open && !inventory_open && !scripted_capture &&
                                        !command_aim_active && !chest_ui_active;
      const aster::CursorMode desired_cursor_mode =
          camera_follow_active
              ? aster::CursorMode::Disabled
              : ((command_aim_active || chest_ui_active) ? aster::CursorMode::Hidden
                                                         : aster::CursorMode::Normal);
      bool cursor_mode_changed = false;
      if (desired_cursor_mode != applied_cursor_mode) {
        window.setCursorMode(desired_cursor_mode);
        have_previous_pointer = false;
        applied_cursor_mode = desired_cursor_mode;
        cursor_mode_changed = true;
      }
      const bool camera_pointer_delta_valid = have_pointer_delta && !cursor_mode_changed;

      const double update_start = clock.now();
      aster::Vec2 axis = movementAxis(control_state);
      bool run = control_state.pressed(kRun);
      if (control_state.justPressed(kJump)) {
        jump_buffered = true;
      }
      if (!pause_open && !inventory_open && !scripted_capture) {
        if (control_state.justPressed(kHotbar1)) {
          game.selectHotbarSlot(0u);
        } else if (control_state.justPressed(kHotbar2)) {
          game.selectHotbarSlot(1u);
        } else if (control_state.justPressed(kHotbar3)) {
          game.selectHotbarSlot(2u);
        } else if (control_state.justPressed(kHotbar4)) {
          game.selectHotbarSlot(3u);
        } else if (control_state.justPressed(kHotbar5)) {
          game.selectHotbarSlot(4u);
        } else if (control_state.justPressed(kHotbar6)) {
          game.selectHotbarSlot(5u);
        }
      }
      bool jump = jump_buffered;
      if (pause_open) {
        axis = {};
        run = false;
        jump = false;
        jump_buffered = false;
      } else if (inventory_open) {
        axis = {};
        run = false;
        jump = false;
        jump_buffered = false;
      } else if (scripted_capture) {
        if (cave_entry_capture) {
          axis = caveEntryAxis(static_cast<float>(elapsed));
          run = caveEntryRun(static_cast<float>(elapsed));
          jump = false;
        } else if (construction_yard_capture) {
          axis = {};
          run = false;
          jump = false;
        } else if (deep_cave_capture || deep_cave_stress_capture || classic_gauntlet_capture) {
          axis = {};
          run = false;
          jump = false;
        } else {
          axis = attractAxis(static_cast<float>(elapsed));
          run = scriptedRun(static_cast<float>(elapsed));
          jump = scriptedJump(static_cast<float>(elapsed));
        }
        jump_buffered = false;
      }

      const aster::Vec3 pre_update_player = game.playerPosition();
      const aster::CaveLightingState pre_update_cave_light =
          game.caveLightingStateAt(pre_update_player);
      camera_follow_pose = aster::updateThirdPersonFollow(
          camera_follow_state,
          cameraFollowSettings(0.0f, caveCameraLookBlend(pre_update_cave_light)),
          {.active = camera_follow_active,
           .has_pointer_delta = camera_pointer_delta_valid,
           .pointer_delta = pointer_delta,
           .focus_target = {pre_update_player.x, pre_update_player.y + 0.32f, pre_update_player.z},
           .fallback_yaw = kGameplayCameraYaw,
           .fallback_pitch = kGameplayCameraPitch},
          static_cast<float>(frame_dt));

      if (scripted_capture) {
        if (deep_cave_stress_capture) {
          const float progress =
              kDeepCaveStressStartProgress +
              static_cast<float>(elapsed) * kDeepCaveStressMetersPerSecond;
          game.relocatePlayer(
              game.caveFrameReportPosition(progress),
              aster::radians(argumentFloat(argc, argv, "--player-yaw-deg", 0.0f)));
        }
        game.update(static_cast<float>(frame_dt), axis, run, jump);
      } else if (!pause_open) {
        const float movement_camera_yaw = camera_follow_pose.camera_yaw;
        const aster::Vec2 world_axis = game.constructionForkliftMounted()
                                           ? axis
                                           : aster::cameraRelativeMoveAxis(axis, movement_camera_yaw);
        const std::size_t simulation_steps = simulation_clock.advance(frame_dt);
        for (std::size_t step = 0; step < simulation_steps; ++step) {
          const bool step_jump = jump_buffered;
          game.update(static_cast<float>(simulation_clock.stepSeconds()), world_axis, run,
                      step_jump);
          if (step_jump) {
            jump_buffered = false;
          }
        }
      } else {
        simulation_clock.reset();
      }

      const float render_alpha = (!scripted_capture && !pause_open)
                                     ? static_cast<float>(simulation_clock.interpolationAlpha())
                                     : 1.0f;
      game.updateRenderInterpolation(render_alpha);
      const aster::Vec3 player = game.playerRenderPosition();
      if (cave_entry_capture) {
        scripted_camera_target = caveEntryCameraTarget(player, static_cast<float>(elapsed));
      } else if (construction_yard_capture) {
        if (construction_yard_seat_capture) {
          scripted_camera_target =
              game.constructionForkliftPosition() + aster::Vec3{-0.42f, 1.20f, -0.20f};
        } else if (construction_yard_exit_capture) {
          scripted_camera_target = player + aster::Vec3{0.0f, 0.42f, 0.0f};
        } else if (construction_yard_shred_capture) {
          scripted_camera_target =
              game.constructionShredderPosition() + aster::Vec3{1.10f, 0.72f, -0.10f};
        } else {
          scripted_camera_target =
              (game.constructionForkliftPosition() + game.constructionPalletPosition() +
               game.constructionShredderPosition()) /
                  3.0f +
              aster::Vec3{0.0f, 0.70f, 0.0f};
        }
      } else if (classic_gauntlet_capture) {
        scripted_camera_target = game.classicGauntletLookTarget();
      } else if (deep_cave_capture || deep_cave_stress_capture) {
        const float progress = deep_cave_stress_capture
                                   ? kDeepCaveStressStartProgress +
                                         static_cast<float>(elapsed) * kDeepCaveStressMetersPerSecond
                                   : deep_cave_capture_progress;
        scripted_camera_target =
            game.caveFrameReportLookTarget(progress, deep_cave_capture_look_ahead);
      }
      const aster::CaveLightingState cave_light =
          game.caveLightingStateAt(scripted_capture ? scripted_camera_target : player);
      camera_follow_pose = aster::updateThirdPersonFollow(
          camera_follow_state,
          cameraFollowSettings(16.0f, caveCameraLookBlend(cave_light)),
          {.active = camera_follow_active,
           .has_pointer_delta = false,
           .pointer_delta = {},
           .focus_target = {player.x, player.y + 0.32f, player.z},
           .fallback_yaw = kGameplayCameraYaw,
           .fallback_pitch = kGameplayCameraPitch},
          static_cast<float>(frame_dt));
      if (inventory_open) {
        game.clearAvatarPointTarget();
        game.setAvatarPreviewYaw(inventory_preview_yaw);
        camera.target = {player.x, player.y + 0.08f, player.z};
        camera.pitch = aster::radians(8.0f);
        camera.yaw = 0.0f;
        camera.radius = inventory_camera_radius;
      } else {
        game.clearAvatarPreviewYaw();
        camera.target = scripted_capture ? scripted_camera_target
                                         : aster::Vec3{player.x, player.y + 0.32f, player.z};
        if (scripted_capture) {
          if (!cave_entry_capture) {
            camera.radius =
                game.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, camera.radius);
          }
        } else {
          camera.target = camera_follow_pose.camera_target;
          camera.pitch = camera_follow_pose.camera_pitch;
          camera.yaw = camera_follow_pose.camera_yaw;
          const float cave_camera_blend = caveCameraLookBlend(cave_light);
          const float desired_radius = std::lerp(gameplay_camera_radius, 2.85f, cave_camera_blend);
          camera.radius =
              game.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, desired_radius);
        }
      }

      settings.line_of_sight_fade = {
          .enabled = !pause_open && !inventory_open && !scripted_capture,
          .camera_position = camera.position(),
          .target_position = {player.x, player.y + 0.30f, player.z},
          .radius = 0.72f,
          .softness = 0.26f,
          .min_opacity = 0.24f,
          .camera_clearance = 0.35f,
          .target_clearance = 0.58f,
          .max_object_radius = 3.40f,
      };
      if (!pause_open && !inventory_open && !scripted_capture) {
        const auto [window_width, window_height] = window.windowSize();
        const aster::Vec2 center_pointer{static_cast<float>(window_width) * 0.5f,
                                         static_cast<float>(window_height) * 0.5f};
        const aster::Viewport viewport{{},
                                       {static_cast<float>(window_width),
                                        static_cast<float>(window_height)}};
        const aster::CameraRay focus_ray =
            camera.screenRay({center_pointer.x, center_pointer.y, 0.0f}, viewport);
        game.updateInteractionFocus(focus_ray.origin.value, focus_ray.direction.value,
                                    static_cast<float>(frame_dt));
        if (control_state.justPressed(kInteract)) {
          game.interactFocused();
        }
        if (control_state.justPressed(kSecondaryInteract)) {
          game.secondaryInteractFocused(focus_ray.origin.value, focus_ray.direction.value);
        }
      } else {
        game.updateInteractionFocus(camera.position(), {0.0f, -1.0f, 0.0f},
                                    static_cast<float>(frame_dt));
      }
      restoreRenderEnvironment(settings, base_render_environment);
      aster::applyRenderStyleProfile(settings, render_style);
      if (const std::optional<aster::DynamicPointLight> light = game.pondAccentLight();
          light.has_value() && light->active) {
        settings.light_rig.push_back({light->position, light->color, light->intensity,
                                      light->source_radius});
      }
      if (const std::optional<aster::DynamicPointLight> light = game.prismRelayLight();
          light.has_value() && light->active) {
        settings.light_rig.push_back({light->position, light->color, light->intensity,
                                      light->source_radius});
      }
      applyCaveRenderEnvironment(settings, base_render_environment, cave_light);
      if (!cave_light.wall_lights.empty()) {
        for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
          settings.light_rig.push_back(
              {light.position, light.color, light.intensity, light.source_radius});
        }
      }
      if (const std::optional<aster::DynamicPointLight> light = game.equippedLight();
          light.has_value() && light->active) {
        const float cave_torch_gain = game.heldTorchLightGain(cave_light);
        if (cave_torch_gain > 0.001f) {
          settings.light_rig.push_back({light->position, light->color,
                                        light->intensity * cave_torch_gain, light->source_radius});
        }
      }
      if (collect_frame_sample) {
        update_times.addSample(clock.now() - update_start);
      }

      const auto [width, height] = window.framebufferSize();
      const double render_start = clock.now();
      const aster::FrameStats render_stats =
          renderer.render(game.scene(), camera, settings, width, height, elapsed);
      const std::uint64_t render_extraction_hash =
          lumenFrameProofHash(game.worldForensics(), render_stats, width, height, rendered_frames,
                              0xE87AC710CULL);
      const std::uint64_t frame_submission_hash =
          lumenFrameProofHash(game.worldForensics(), render_stats, width, height, rendered_frames,
                              render_extraction_hash);
      game.noteRenderExtraction(render_extraction_hash, frame_submission_hash,
                                static_cast<float>(render_stats.frame_seconds * 1000.0));
      const aster::LumenWorldForensics &world_forensics = game.worldForensics();
      renderer.stampLastFrameCausalTrace(
          world_forensics.trace_hash, world_forensics.epoch,
          world_forensics.render_extraction_hash, world_forensics.cave_gate.probe_trace_hash,
          world_forensics.world_transition_hash, world_forensics.actor_state_delta_hash,
          world_forensics.sensory_event_hash, world_forensics.visibility_set_hash,
          world_forensics.cave_gate.encounter_budget_hash,
          world_forensics.cave_gate.navigation_valid, world_forensics.streaming_region_id,
          world_forensics.cave_gate.perceptual_salience_score,
          world_forensics.coal_mining_reaction.accepted,
          world_forensics.coal_mining_reaction.required_channel_mask,
          world_forensics.coal_mining_reaction.observed_channel_mask,
          world_forensics.coal_mining_reaction.missing_channel_mask,
          world_forensics.coal_mining_reaction.continuity_score,
          world_forensics.coal_mining_reaction.minimum_score,
          world_forensics.coal_mining_reaction.reaction_package_hash,
          world_forensics.coal_mining_reaction.material_memory_hash,
          world_forensics.coal_mining_reaction.lighting_atmosphere_hash,
          world_forensics.coal_mining_reaction.ai_attention_hash,
          world_forensics.coal_mining_reaction.streaming_residency_lod_hash,
          world_forensics.coal_mining_reaction.resource_state_hash,
          world_forensics.coal_mining_reaction.event_residue_hash,
          world_forensics.coal_mining_reaction.readability_audit_hash);
      renderer.stampLastFramePerceptionLedger(world_forensics.perception_ledger,
                                              world_forensics.perception_object_traces);
      renderer.stampLastFramePerceptualState(world_forensics.perceptual_state);
      renderer.stampLastFramePerceptualSchedule(world_forensics.perceptual_schedule);
      renderer.stampLastFrameBeliefReport(world_forensics.belief_report);
      if (collect_frame_sample) {
        render_times.addSample(clock.now() - render_start);
        ++render_counter_samples;
        visible_object_sum += static_cast<double>(render_stats.visible_objects);
        culled_object_sum += static_cast<double>(render_stats.culled_objects);
        draw_call_sum += static_cast<double>(render_stats.draw_calls);
        instance_group_sum += static_cast<double>(render_stats.instance_groups);
        lod_culled_object_sum += static_cast<double>(render_stats.lod_culled_objects);
        visibility_hint_object_sum += static_cast<double>(render_stats.visibility_hint_objects);
        dynamic_mesh_object_sum += static_cast<double>(render_stats.dynamic_mesh_objects);
        dynamic_mesh_cache_entry_sum +=
            static_cast<double>(render_stats.dynamic_mesh_cache_entries);
        pipeline_switch_sum += static_cast<double>(render_stats.pipeline_switches);
        material_permutation_sum += static_cast<double>(render_stats.material_permutations);
        material_cache_hit_sum += static_cast<double>(render_stats.material_variant_cache_hits);
        material_cache_miss_sum += static_cast<double>(render_stats.material_variant_cache_misses);
        rust_plan_seconds_sum += render_stats.rust_plan_seconds;
        render_encode_seconds_sum += render_stats.render_encode_seconds;
        active_point_light_sum += static_cast<double>(render_stats.active_point_lights);
        clustered_light_cluster_sum += static_cast<double>(render_stats.clustered_light_clusters);
        clustered_light_assignment_sum +=
            static_cast<double>(render_stats.clustered_light_assignments);
        const aster::FrameForensics &forensics = renderer.lastFrameForensics();
        clustered_visible_light_sum +=
            static_cast<double>(forensics.clustered_lights.visible_lights.size());
        frame_forensics_pass_sum += static_cast<double>(forensics.passes.size());
        frame_forensics_resource_sum += static_cast<double>(forensics.resource_traces.size());
        frame_forensics_material_sum += static_cast<double>(forensics.material_bindings.size());
        world_linked_frame_sum += forensics.world_transition_linked ? 1.0 : 0.0;
        world_navigation_valid_sum += forensics.navigation_valid ? 1.0 : 0.0;
        world_perceptual_salience_sum += forensics.perceptual_salience_score;
        const bool clustered_fallback =
            std::any_of(forensics.events.begin(), forensics.events.end(),
                        [](const aster::FrameDiagnosticEvent &event) {
                          return event.kind ==
                                 aster::FrameDiagnosticKind::ClusteredLightingFallback;
                        });
        clustered_light_fallback_sum += clustered_fallback ? 1.0 : 0.0;
      }
      if (!scripted_capture || capture_hud) {
        const double hud_start = clock.now();
        const aster::PointerCueModel pointer_cue{.active = command_aim_active && !pause_open,
                                                 .pressed = command_aim_active &&
                                                            control_state.pressed(kCameraOrbit),
                                                 .position = pointer};
        const aster::GameCursorModel game_cursor{.visible = game.chestInterfaceOpen() &&
                                                            !pause_open && !inventory_open &&
                                                            (!scripted_capture || capture_hud),
                                                 .pressed = control_state.pressed(kCameraOrbit),
                                                 .position = pointer};
        const auto [hud_width, hud_height] = window.windowSize();
        hud.beginFrame({static_cast<float>(hud_width), static_cast<float>(hud_height)},
                       control_state.snapshot());
        const aster::HudAction hud_action = hud.draw(
            hudModel(game.status(), inventory_open, game.torchCount(), game.supplyCrateNearby(),
                     pause_open, pause_options_open, pointer_cue, game_cursor,
                     game.focusPromptModel(), game.hotbarHudModel(), game.chestContentsHudModel(),
                     game.classicGauntletAutomap(), game.classicHudSignals(),
                     game.classicTransitionWipe()));
        if (hud_action == aster::HudAction::CloseChest) {
          game.closeChest();
        } else if (hud_action == aster::HudAction::TransferSupplyTorch) {
          (void)game.takeSupplyTorch();
        } else if (hud_action == aster::HudAction::Resume) {
          pause_open = false;
          pause_options_open = false;
          jump_buffered = false;
          simulation_clock.reset();
        } else if (hud_action == aster::HudAction::ToggleOptions) {
          pause_options_open = !pause_options_open;
        } else if (hud_action == aster::HudAction::Quit) {
          window.requestClose();
        }
        hud.endFrame();
        if (collect_frame_sample) {
          hud_times.addSample(clock.now() - hud_start);
        }
      }

      if (collect_frame_sample) {
        frame_times.addSample(clock.now() - frame_start_seconds);
      }

      if (sequence_capture) {
        aster::writeFramebufferPpm(framePath(sequence_path, rendered_frames), width, height);
        if (rendered_frames + 1 >= sequence_frames) {
          window.requestClose();
        }
      } else if (!screenshot_path.empty() && !captured && rendered_frames >= screenshot_frame) {
        writeActiveFramebufferCapture(screenshot_path, width, height);
        captured = true;
        window.requestClose();
      }

      const double swap_start = clock.now();
      window.swapBuffers();
      if (collect_frame_sample) {
        swap_times.addSample(clock.now() - swap_start);
      }
      ++rendered_frames;

      if (smoke_test && rendered_frames >= 4) {
        window.requestClose();
      }
      if (run_frames > 0 && rendered_frames >= run_frames) {
        window.requestClose();
      }
      if (window.isOpen()) {
        sleepForFrameCap(clock, frame_start_seconds, target_frame_seconds);
      }
    }

    if (profile_enabled) {
      aster::profile::stopCapture();
      if (!profile_capture_path.empty()) {
        const std::string capture_path = profile_capture_path.string();
        aster::profile::saveCapture(capture_path.c_str());
      }
      aster::profile::shutdown();
    }

    if (frame_report_enabled) {
      const aster::FrameTimeSummary summary = frame_times.summarize(lag_budget_seconds);
      std::cout << std::fixed << std::setprecision(3);
      std::cout << "Frame report warmup: frames=" << frame_report_warmup << '\n';
      std::cout << "Frame report route: "
                << (scripted_frame_report_route ? playback_route : "interactive") << '\n';
      printFrameSummary("Frame report", summary);
      printFrameSummary("Update report", update_times.summarize());
      printFrameSummary("Render report", render_times.summarize());
      printFrameSummary("HUD report", hud_times.summarize());
      printFrameSummary("Swap report", swap_times.summarize());
      if (render_counter_samples > 0u) {
        const double samples = static_cast<double>(render_counter_samples);
        std::cout << "Render counters: samples=" << render_counter_samples
                  << " visible_objects_mean=" << visible_object_sum / samples
                  << " culled_objects_mean=" << culled_object_sum / samples
                  << " draw_calls_mean=" << draw_call_sum / samples
                  << " instance_groups_mean=" << instance_group_sum / samples
                  << " lod_culled_mean=" << lod_culled_object_sum / samples
                  << " visibility_hints_mean=" << visibility_hint_object_sum / samples
                  << " dynamic_meshes_mean=" << dynamic_mesh_object_sum / samples
                  << " dynamic_mesh_cache_mean=" << dynamic_mesh_cache_entry_sum / samples
                  << " pipeline_switches_mean=" << pipeline_switch_sum / samples
                  << " material_permutations_mean=" << material_permutation_sum / samples
                  << " material_cache_hits_mean=" << material_cache_hit_sum / samples
                  << " material_cache_misses_mean=" << material_cache_miss_sum / samples
                  << " active_point_lights_mean=" << active_point_light_sum / samples
                  << " clustered_clusters_mean=" << clustered_light_cluster_sum / samples
                  << " clustered_assignments_mean=" << clustered_light_assignment_sum / samples
                  << " clustered_visible_lights_mean=" << clustered_visible_light_sum / samples
                  << " forensics_passes_mean=" << frame_forensics_pass_sum / samples
                  << " forensics_resources_mean=" << frame_forensics_resource_sum / samples
                  << " forensics_materials_mean=" << frame_forensics_material_sum / samples
                  << " clustered_fallback_frames=" << clustered_light_fallback_sum
                  << " rust_plan_ms_mean="
                  << secondsToMilliseconds(rust_plan_seconds_sum / samples)
                  << " render_encode_ms_mean="
                  << secondsToMilliseconds(render_encode_seconds_sum / samples) << '\n';
        const aster::LumenWorldForensics &world = game.worldForensics();
        std::cout << "World report: gate="
                  << lumenWorldGateVerdictName(world.cave_gate.verdict)
                  << " region_id=" << world.streaming_region_id
                  << " probe_trace_hash=" << world.cave_gate.probe_trace_hash
                  << " world_transition_hash=" << world.world_transition_hash
                  << " linked_frames=" << world_linked_frame_sum
                  << " nav_valid_frames=" << world_navigation_valid_sum
                  << " salience_mean=" << world_perceptual_salience_sum / samples
                  << " actor_delta_count=" << world.actor_delta_count
                  << " perceptual_state_hash=" << world.perceptual_state.perceptual_state_hash
                  << " continuity_debt=" << world.perceptual_state.continuity_debt
                  << " perceptual_runtime_accepted="
                  << (world.perceptual_state.accepted ? 1 : 0)
                  << " render_extraction_hash=" << world.render_extraction_hash
                  << " frame_submission_hash=" << world.frame_submission_hash << '\n';
      }
    }
    if (startup_report_enabled) {
      printStartupSummary(startup_samples);
    }
  } catch (const std::exception &error) {
    std::cerr << "Lumen Run failed: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
