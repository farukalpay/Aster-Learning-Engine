// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/core/profiler.hpp"
#include "aster/game_sdk/game_sdk.hpp"
#include "aster/samples/lumen_run.hpp"
#include "aster/systems/third_person_camera.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/ui/hud_layer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
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
  const aster::Vec3 red_source = {light.wall_light_color.x * 0.020f,
                                  light.wall_light_color.y * 0.004f,
                                  light.wall_light_color.z * 0.003f};
  const aster::Vec3 cave_source_ambient =
      mixVec({0.004f, 0.004f, 0.004f}, red_source, source_fill * 0.28f);
  const aster::Vec3 cave_sky_tint =
      mixVec({0.003f, 0.003f, 0.004f}, cave_source_ambient, source_fill * 0.18f);
  const aster::Vec3 cave_ground_tint =
      mixVec({0.003f, 0.0025f, 0.002f}, cave_source_ambient * 0.60f, source_fill * 0.16f);

  settings.ambient_strength =
      std::lerp(baseline.ambient_strength,
                0.014f + source_fill * 0.004f + chamber_fill * 0.003f - cave_depth * 0.004f,
                interior);
  settings.ambient_floor = std::lerp(baseline.ambient_floor, 0.0f, interior);
  settings.indirect_albedo_floor = std::lerp(baseline.indirect_albedo_floor, 0.0f, interior);
  settings.sky_ambient_color = mixVec(baseline.sky_ambient_color, cave_sky_tint, interior);
  settings.ground_ambient_color = mixVec(baseline.ground_ambient_color, cave_ground_tint, interior);
  settings.pipeline.clear_color =
      mixVec(baseline.clear_color, {0.001f, 0.001f, 0.001f}, interior);
  settings.exposure =
      std::lerp(baseline.exposure, 0.84f + source_fill * 0.035f + chamber_fill * 0.012f, interior);
  settings.atmosphere.fog_color =
      mixVec(baseline.atmosphere.fog_color, {0.004f, 0.003f, 0.003f}, interior);
  settings.atmosphere.fog_start = std::lerp(baseline.atmosphere.fog_start, 2.0f, interior);
  settings.atmosphere.fog_end =
      std::lerp(baseline.atmosphere.fog_end, 6.8f + source_fill * 1.4f + chamber_fill * 1.2f,
                interior);
  settings.atmosphere.fog_strength =
      std::lerp(baseline.atmosphere.fog_strength, 0.46f + cave_depth * 0.20f, interior);
  settings.atmosphere.saturation =
      std::lerp(baseline.atmosphere.saturation, 0.42f + source_fill * 0.20f, interior);
  settings.atmosphere.contrast = std::lerp(baseline.atmosphere.contrast, 1.32f, interior);
  settings.sun_light.intensity =
      std::lerp(baseline.sun_light.intensity, 0.0f, interior);
  settings.style.unlit_mix = std::lerp(settings.style.unlit_mix, 0.0f, interior);
  for (std::size_t i = 0; i < settings.light_rig.size(); ++i) {
    settings.light_rig[i].intensity *= std::lerp(1.0f, 0.0f, interior);
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
                         aster::ChestContentsHudModel chest_contents) {
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
      {"Space", "Jump!", true},      {"Left Shift", "Run", true},
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
    const bool debug_cave_overlay = hasArgument(argc, argv, "--debug-cave-overlay");
    const bool profile_enabled =
        hasArgument(argc, argv, "--profile") || !profile_capture_path.empty();
    const bool frame_report_enabled = hasArgument(argc, argv, "--frame-report");
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
      std::cout << "Lumen Run cave validation passed: "
                << (authoring.cave.id.empty() ? "no cave asset" : authoring.cave.id) << '\n';
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
    if (deep_cave_capture || deep_cave_stress_capture) {
      game.setPlayerAvatarVisible(false);
    }
    mark_startup("game_reset");
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
              : ((deep_cave_capture || deep_cave_stress_capture)
                     ? game.caveFrameReportLookTarget(
                           deep_cave_stress_capture ? kDeepCaveStressStartProgress
                                                    : deep_cave_capture_progress,
                           deep_cave_capture_look_ahead)
                     : aster::Vec3{argumentFloat(argc, argv, "--camera-target-x", 2.25f),
                                   argumentFloat(argc, argv, "--camera-target-y", 0.48f),
                                   argumentFloat(argc, argv, "--camera-target-z", -0.95f)});
      camera.pitch = aster::radians(argumentFloat(
          argc, argv, "--camera-pitch-deg",
          cave_entry_capture
              ? 12.0f
              : ((deep_cave_capture || deep_cave_stress_capture) ? 6.0f : 28.0f)));
      camera.yaw = aster::radians(argumentFloat(
          argc, argv, "--camera-yaw-deg",
          cave_entry_capture
              ? 0.0f
              : ((deep_cave_capture || deep_cave_stress_capture) ? 180.0f : -31.0f)));
      camera.radius =
          argumentFloat(argc, argv, "--camera-radius",
                        cave_entry_capture ? 7.2f
                                           : ((deep_cave_capture || deep_cave_stress_capture)
                                                  ? 2.70f
                                                  : 7.8f));
      camera.vertical_fov = aster::radians(std::clamp(
          argumentFloat(argc, argv, "--camera-fov-deg", cave_entry_capture ? 46.0f : 54.0f), 18.0f,
          72.0f));
    }
    float gameplay_camera_radius = camera.radius;
    float inventory_camera_radius = 2.25f;

    aster::RendererSettings settings;
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
        (void)game.pointAvatarAtRay(ray.origin, ray.direction);
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
        } else if (deep_cave_capture || deep_cave_stress_capture) {
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
        const aster::Vec2 world_axis = aster::cameraRelativeMoveAxis(axis, movement_camera_yaw);
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
        game.updateInteractionFocus(focus_ray.origin, focus_ray.direction,
                                    static_cast<float>(frame_dt));
        if (control_state.justPressed(kInteract)) {
          game.interactFocused();
        }
        if (control_state.justPressed(kSecondaryInteract)) {
          game.secondaryInteractFocused(focus_ray.origin, focus_ray.direction);
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
        const float cave_torch_gain = std::lerp(1.0f, 0.0f, cave_light.interior);
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
                     game.focusPromptModel(), game.hotbarHudModel(), game.chestContentsHudModel()));
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
        aster::writeFramebufferPpm(screenshot_path, width, height);
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
