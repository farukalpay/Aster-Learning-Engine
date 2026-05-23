// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/aster_grid_tactics/aster_grid_tactics.hpp"

#include "aster/geometry/energy_conduit_mesh.hpp"
#include "aster/math/hash.hpp"
#include "aster/render/mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace aster {
namespace {

constexpr int kScriptStepTicks = 7;
constexpr float kTileSize = 1.0f;

enum class TileKind : std::uint8_t {
  Floor,
  Wall,
  Door,
  Terminal,
  EnergyNode,
  Extraction,
  Turret,
};

enum class Facing : std::uint8_t {
  North,
  East,
  South,
  West,
};

struct GuardState {
  AsterGridCoord position{};
  AsterGridCoord visual_from{};
  std::uint32_t visual_move_tick = 0u;
  std::vector<AsterGridCoord> patrol;
  std::size_t patrol_index = 0u;
  int patrol_direction = 1;
  Facing facing = Facing::East;
};

struct ProjectileState {
  AsterGridCoord position{};
  AsterGridCoord direction{};
  int age_ticks = 0;
};

struct ScriptAction {
  int dx = 0;
  int dy = 0;
  bool interact = false;
  int ticks = kScriptStepTicks;
};

[[nodiscard]] bool sameCoord(const AsterGridCoord lhs, const AsterGridCoord rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

[[nodiscard]] std::uint32_t coordBits(const AsterGridCoord coord) {
  return (static_cast<std::uint32_t>(coord.x & 0xffff) << 16u) |
         static_cast<std::uint32_t>(coord.y & 0xffff);
}

[[nodiscard]] AsterGridCoord facingDelta(const Facing facing) {
  switch (facing) {
  case Facing::North:
    return {0, -1};
  case Facing::East:
    return {1, 0};
  case Facing::South:
    return {0, 1};
  case Facing::West:
    return {-1, 0};
  }
  return {};
}

[[nodiscard]] Facing facingFromDelta(const AsterGridCoord delta) {
  if (std::abs(delta.x) >= std::abs(delta.y)) {
    return delta.x < 0 ? Facing::West : Facing::East;
  }
  return delta.y < 0 ? Facing::North : Facing::South;
}

[[nodiscard]] float facingYaw(const Facing facing) {
  switch (facing) {
  case Facing::North:
    return radians(180.0f);
  case Facing::East:
    return radians(90.0f);
  case Facing::South:
    return 0.0f;
  case Facing::West:
    return radians(-90.0f);
  }
  return 0.0f;
}

[[nodiscard]] std::vector<ScriptAction> scriptedActions() {
  std::vector<ScriptAction> actions;
  const auto move = [&](const int dx, const int dy, const int count) {
    for (int i = 0; i < count; ++i) {
      actions.push_back({dx, dy, false, kScriptStepTicks});
    }
  };
  const auto interact = [&] { actions.push_back({0, 0, true, 3}); };

  move(1, 0, 3);
  move(0, 1, 1);
  interact();
  move(1, 0, 1);
  move(0, 1, 3);
  interact();
  move(1, 0, 8);
  move(0, 1, 2);
  interact();
  move(1, 0, 3);
  move(0, 1, 3);
  return actions;
}

[[nodiscard]] float tileVariation(const AsterGridCoord coord, const std::uint32_t salt = 0u) {
  std::uint32_t hash = static_cast<std::uint32_t>(coord.x * 73856093u) ^
                       static_cast<std::uint32_t>(coord.y * 19349663u) ^ salt;
  hash ^= hash >> 13u;
  hash *= 0x85ebca6bu;
  hash ^= hash >> 16u;
  return static_cast<float>(hash & 0xffu) / 255.0f;
}

[[nodiscard]] Material gridMaterial(
    const std::string &asset_id, const LinearRgb base, const EmissionColor emission = {},
    const float emission_strength = 0.0f, const float opacity = 1.0f,
    const MaterialSurfaceProfile profile = MaterialSurfaceProfile::CorrodedMetal,
    const SurfacePattern pattern = SurfacePattern::WeatheredMetal) {
  MaterialDesc desc;
  desc.base_color = base;
  desc.emission_color = emission;
  desc.emission_strength = emission_strength;
  desc.roughness = 0.66f;
  desc.metallic = profile == MaterialSurfaceProfile::CorrodedMetal ? 0.42f : 0.08f;
  desc.opacity = opacity;
  desc.double_sided = true;
  desc.cull_mode = FaceCullMode::None;
  desc.surface_profile = profile;
  desc.surface_pattern = pattern;
  desc.detail_strength = 0.48f;
  desc.detail_scale = 4.2f;
  desc.pattern_scale = {5.0f, 4.4f};
  desc.pattern_depth = 0.18f;
  desc.pattern_contrast = 0.38f;
  desc.edge_wear = 0.26f;
  desc.ambient_occlusion = 0.88f;
  desc.procedural.macro_variation = 0.42f;
  desc.procedural.micro_normal_strength = 0.46f;
  desc.procedural.roughness_variation = 0.26f;
  desc.procedural.physical_texel_density = 960.0f;
  desc.procedural.height_normal_coupling = 0.92f;
  desc.procedural.roughness_height_coupling = 0.74f;
  desc.procedural.macro_frequency_breakup = 0.58f;
  desc.procedural.micro_frequency_breakup = 0.72f;
  desc.procedural.height_shading = 0.34f;
  desc.procedural.pitting_density = 0.62f;
  desc.procedural.pitting_depth = 0.012f;
  desc.procedural.oxide_layering = 0.46f;
  desc.procedural.cavity_grime = 0.34f;
  desc.procedural.edge_polish = 0.28f;
  desc.procedural.axial_scratches = 0.38f;
  desc.procedural.rust_bloom = 0.22f;
  desc.receives_shadows = true;
  if (opacity < 0.99f) {
    desc.alpha_mode = MaterialAlphaMode::Blend;
    desc.depth_write = MaterialDepthWrite::Disabled;
    desc.depth_policy.layer = RenderDepthLayer::DebugOverlay;
    desc.depth_policy.constant_bias = -0.01f;
    desc.receives_shadows = false;
  }
  Material material = makeMaterial(desc);
  material.asset_id = asset_id;
  return material;
}

[[nodiscard]] Material neonMaterial(const std::string &asset_id, const LinearRgb base,
                                    const EmissionColor emission,
                                    const float emission_strength,
                                    const float opacity = 0.96f) {
  Material material = gridMaterial(asset_id, base, emission, emission_strength, opacity,
                                   MaterialSurfaceProfile::EmissiveLens, SurfacePattern::None);
  material.roughness = 0.24f;
  material.metallic = 0.0f;
  material.pattern_depth = 0.05f;
  material.pattern_contrast = 0.12f;
  material.procedural.micro_normal_strength = 0.08f;
  material.procedural.height_shading = 0.05f;
  return material;
}

void makeRuntimePresentationMaterial(Material &material, const float detail = 0.18f) {
  material.asset_id.clear();
  material.surface_profile = MaterialSurfaceProfile::Plain;
  material.surface_pattern = SurfacePattern::None;
  material.detail_strength = detail * 0.06f;
  material.edge_wear = 0.0f;
  material.pattern_depth = 0.0f;
  material.pattern_contrast = 0.0f;
  material.procedural = {};
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedPlane() {
  return std::make_shared<const CpuMesh>(makePlane(1.0f));
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedMesh(CpuMesh mesh) {
  return std::make_shared<const CpuMesh>(std::move(mesh));
}

[[nodiscard]] CpuMesh makeOperatorSuitMesh() {
  CpuMesh mesh;
  mesh.vertices = {
      {{0.00f, 0.0f, 0.54f}, {0.0f, 1.0f, 0.0f}, {0.50f, 1.00f}},
      {{-0.20f, 0.0f, 0.30f}, {0.0f, 1.0f, 0.0f}, {0.34f, 0.76f}},
      {{-0.48f, 0.0f, 0.12f}, {0.0f, 1.0f, 0.0f}, {0.10f, 0.60f}},
      {{-0.38f, 0.0f, -0.28f}, {0.0f, 1.0f, 0.0f}, {0.18f, 0.20f}},
      {{-0.12f, 0.0f, -0.46f}, {0.0f, 1.0f, 0.0f}, {0.40f, 0.02f}},
      {{0.12f, 0.0f, -0.46f}, {0.0f, 1.0f, 0.0f}, {0.60f, 0.02f}},
      {{0.38f, 0.0f, -0.28f}, {0.0f, 1.0f, 0.0f}, {0.82f, 0.20f}},
      {{0.48f, 0.0f, 0.12f}, {0.0f, 1.0f, 0.0f}, {0.90f, 0.60f}},
      {{0.20f, 0.0f, 0.30f}, {0.0f, 1.0f, 0.0f}, {0.66f, 0.76f}},
      {{0.00f, 0.0f, 0.10f}, {0.0f, 1.0f, 0.0f}, {0.50f, 0.50f}},
  };
  mesh.indices = {9u, 0u, 1u, 9u, 1u, 2u, 9u, 2u, 3u, 9u, 3u, 4u,
                  9u, 4u, 5u, 9u, 5u, 6u, 9u, 6u, 7u, 9u, 7u, 8u,
                  9u, 8u, 0u};
  return mesh;
}

[[nodiscard]] CpuMesh makeGuardDroneMesh() {
  CpuMesh mesh;
  mesh.vertices = {
      {{0.00f, 0.0f, 0.44f}, {0.0f, 1.0f, 0.0f}, {0.50f, 1.00f}},
      {{-0.18f, 0.0f, 0.25f}, {0.0f, 1.0f, 0.0f}, {0.34f, 0.74f}},
      {{-0.50f, 0.0f, 0.16f}, {0.0f, 1.0f, 0.0f}, {0.02f, 0.62f}},
      {{-0.34f, 0.0f, -0.08f}, {0.0f, 1.0f, 0.0f}, {0.16f, 0.40f}},
      {{-0.22f, 0.0f, -0.38f}, {0.0f, 1.0f, 0.0f}, {0.34f, 0.02f}},
      {{0.00f, 0.0f, -0.22f}, {0.0f, 1.0f, 0.0f}, {0.50f, 0.22f}},
      {{0.22f, 0.0f, -0.38f}, {0.0f, 1.0f, 0.0f}, {0.66f, 0.02f}},
      {{0.34f, 0.0f, -0.08f}, {0.0f, 1.0f, 0.0f}, {0.84f, 0.40f}},
      {{0.50f, 0.0f, 0.16f}, {0.0f, 1.0f, 0.0f}, {0.98f, 0.62f}},
      {{0.18f, 0.0f, 0.25f}, {0.0f, 1.0f, 0.0f}, {0.66f, 0.74f}},
  };
  mesh.indices = {5u, 0u, 1u, 5u, 1u, 2u, 5u, 2u, 3u, 5u, 3u, 4u,
                  5u, 4u, 6u, 5u, 6u, 7u, 5u, 7u, 8u, 5u, 8u, 9u,
                  5u, 9u, 0u};
  return mesh;
}

[[nodiscard]] CpuMesh makeBladeMesh(const float half_width, const float length) {
  CpuMesh mesh;
  mesh.vertices = {
      {{-half_width, 0.0f, -length * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{half_width, 0.0f, -length * 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{half_width * 0.62f, 0.0f, length * 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-half_width * 0.62f, 0.0f, length * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
  };
  mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
  return mesh;
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedRing(const float radius,
                                                            const float band_width,
                                                            const int segments = 64) {
  EnergyConduitRingSpec spec;
  spec.radius = radius;
  spec.band_width = band_width;
  spec.segments = segments;
  return makeSharedMesh(makeEnergyConduitRingMesh(spec));
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeConeMesh(const int range) {
  auto mesh = std::make_shared<CpuMesh>();
  const float reach = static_cast<float>(range) + 0.20f;
  const float half_width = std::max(1.0f, reach * 0.48f);
  mesh->vertices = {
      {{0.0f, 0.0f, -0.18f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.0f}},
      {{-half_width, 0.0f, reach}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
      {{half_width, 0.0f, reach}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
  };
  mesh->indices = {0u, 1u, 2u};
  return mesh;
}

[[nodiscard]] const char *visualEventText(const AsterGridTacticsVisualEvent event) {
  switch (event) {
  case AsterGridTacticsVisualEvent::None:
    return "";
  case AsterGridTacticsVisualEvent::TerminalHack:
    return "TERMINAL BREACHED";
  case AsterGridTacticsVisualEvent::EnergyReroute:
    return "CONDUIT REROUTED";
  case AsterGridTacticsVisualEvent::TurretOffline:
    return "TURRET OFFLINE";
  case AsterGridTacticsVisualEvent::ExtractionOpen:
    return "EXTRACTION GATE LIVE";
  case AsterGridTacticsVisualEvent::ExtractionComplete:
    return "EXTRACTION BLOOM";
  case AsterGridTacticsVisualEvent::Defeat:
    return "RUN COMPROMISED";
  }
  return "";
}

} // namespace

struct AsterGridTactics::Impl {
  explicit Impl(AsterGridTacticsTuning tuning_in)
      : tuning(std::move(tuning_in)),
        grid(static_cast<std::size_t>(std::max(tuning.width, 1) * std::max(tuning.height, 1)),
             TileKind::Floor),
        quad_mesh(makeSharedPlane()),
        cone_mesh(makeConeMesh(5)),
        operator_mesh(makeSharedMesh(makeOperatorSuitMesh())),
        guard_mesh(makeSharedMesh(makeGuardDroneMesh())),
        blade_mesh(makeSharedMesh(makeBladeMesh(0.12f, 0.72f))),
        node_ring_mesh(makeSharedRing(0.48f, 0.070f, 56)),
        extraction_ring_mesh(makeSharedRing(0.62f, 0.085f, 72)),
        pulse_ring_mesh(makeSharedRing(0.86f, 0.055f, 72)) {
    reset();
  }

  AsterGridTacticsTuning tuning;
  std::vector<TileKind> grid;
  Scene scene;
  CommandReplay replay;
  DeterministicRandomStream rng{0u};
  AsterGridTacticsStatus status;
  std::vector<AsterGridCoord> energy_nodes;
  std::vector<bool> energy_rerouted;
  AsterGridCoord terminal{4, 2};
  AsterGridCoord door{8, 5};
  AsterGridCoord turret{12, 4};
  AsterGridCoord extraction{16, 10};
  std::vector<GuardState> guards;
  std::vector<ProjectileState> projectiles;
  AsterGridCoord player_visual_from{1, 1};
  std::uint32_t player_visual_move_tick = 0u;
  std::shared_ptr<const CpuMesh> quad_mesh;
  std::shared_ptr<const CpuMesh> cone_mesh;
  std::shared_ptr<const CpuMesh> operator_mesh;
  std::shared_ptr<const CpuMesh> guard_mesh;
  std::shared_ptr<const CpuMesh> blade_mesh;
  std::shared_ptr<const CpuMesh> node_ring_mesh;
  std::shared_ptr<const CpuMesh> extraction_ring_mesh;
  std::shared_ptr<const CpuMesh> pulse_ring_mesh;
  Facing player_facing = Facing::East;
  int player_cooldown_ticks = 0;
  std::uint32_t command_sequence = 0u;

  [[nodiscard]] std::size_t index(const int x, const int y) const {
    return static_cast<std::size_t>(y * tuning.width + x);
  }

  [[nodiscard]] bool inside(const AsterGridCoord coord) const {
    return coord.x >= 0 && coord.y >= 0 && coord.x < tuning.width && coord.y < tuning.height;
  }

  [[nodiscard]] TileKind tile(const AsterGridCoord coord) const {
    if (!inside(coord)) {
      return TileKind::Wall;
    }
    return grid[index(coord.x, coord.y)];
  }

  void setTile(const AsterGridCoord coord, const TileKind kind) {
    if (inside(coord)) {
      grid[index(coord.x, coord.y)] = kind;
    }
  }

  [[nodiscard]] bool blocksSight(const AsterGridCoord coord) const {
    const TileKind kind = tile(coord);
    return kind == TileKind::Wall || (kind == TileKind::Door && !status.door_open);
  }

  [[nodiscard]] bool walkable(const AsterGridCoord coord) const {
    const TileKind kind = tile(coord);
    return kind != TileKind::Wall && (kind != TileKind::Door || status.door_open);
  }

  [[nodiscard]] Vec3 worldPosition(const AsterGridCoord coord, const float y = 0.0f) const {
    const float origin_x = static_cast<float>(tuning.width) * -0.5f;
    const float origin_z = static_cast<float>(tuning.height) * -0.5f;
    return {origin_x + (static_cast<float>(coord.x) + 0.5f) * kTileSize, y,
            origin_z + (static_cast<float>(coord.y) + 0.5f) * kTileSize};
  }

  [[nodiscard]] Vec3 worldOffset(const AsterGridCoord coord, const float dx, const float dz,
                                 const float y) const {
    const Vec3 center = worldPosition(coord, y);
    return {center.x + dx, y, center.z + dz};
  }

  [[nodiscard]] Vec3 interpolatedWorldPosition(const AsterGridCoord from,
                                               const AsterGridCoord to,
                                               const std::uint32_t move_tick,
                                               const int duration_ticks,
                                               const float y) const {
    const Vec3 start = worldPosition(from, y);
    const Vec3 end = worldPosition(to, y);
    if (sameCoord(from, to) || duration_ticks <= 1 || status.tick <= move_tick) {
      return end;
    }
    const float raw =
        std::clamp(static_cast<float>(status.tick - move_tick) /
                       static_cast<float>(std::max(duration_ticks, 1)),
                   0.0f, 1.0f);
    const float t = raw * raw * (3.0f - 2.0f * raw);
    return {start.x + (end.x - start.x) * t, y, start.z + (end.z - start.z) * t};
  }

  void reset() {
    rng.reset(tuning.seed);
    std::fill(grid.begin(), grid.end(), TileKind::Floor);
    replay.clear();
    guards.clear();
    projectiles.clear();
    energy_nodes = {{5, 5}, {13, 7}};
    energy_rerouted.assign(energy_nodes.size(), false);
    command_sequence = 0u;
    player_cooldown_ticks = 0;
    player_facing = Facing::East;
    player_visual_from = {1, 1};
    player_visual_move_tick = 0u;

    for (int x = 0; x < tuning.width; ++x) {
      setTile({x, 0}, TileKind::Wall);
      setTile({x, tuning.height - 1}, TileKind::Wall);
    }
    for (int y = 0; y < tuning.height; ++y) {
      setTile({0, y}, TileKind::Wall);
      setTile({tuning.width - 1, y}, TileKind::Wall);
    }
    for (int y = 2; y < tuning.height - 2; ++y) {
      if (y != 5) {
        setTile({8, y}, TileKind::Wall);
      }
    }
    setTile(door, TileKind::Door);
    setTile(terminal, TileKind::Terminal);
    setTile(turret, TileKind::Turret);
    setTile(extraction, TileKind::Extraction);
    for (const AsterGridCoord node : energy_nodes) {
      setTile(node, TileKind::EnergyNode);
    }
    for (int i = 0; i < 12; ++i) {
      const AsterGridCoord decal{2 + static_cast<int>(rng.nextU32() % 14u),
                                 2 + static_cast<int>(rng.nextU32() % 8u)};
      if (tile(decal) == TileKind::Floor && !sameCoord(decal, {1, 1})) {
        // Procedural floor variation is represented in the scene pass, not as blockers.
      }
    }

    guards.push_back({.position = {6, 8},
                      .visual_from = {6, 8},
                      .visual_move_tick = 0u,
                      .patrol = {{6, 8}, {7, 8}, {8, 8}, {9, 8}, {10, 8}, {11, 8}},
                      .patrol_index = 0u,
                      .patrol_direction = 1,
                      .facing = Facing::East});
    guards.push_back({.position = {14, 4},
                      .visual_from = {14, 4},
                      .visual_move_tick = 0u,
                      .patrol = {{14, 4}, {14, 5}, {14, 6}, {14, 7}, {14, 8}},
                      .patrol_index = 0u,
                      .patrol_direction = 1,
                      .facing = Facing::South});

    status = {};
    status.player = {1, 1};
    status.outcome = AsterGridTacticsOutcome::Playing;
    status.visual_event = AsterGridTacticsVisualEvent::None;
    status.visual_event_tick = 0u;
    status.overload_ticks_remaining = tuning.overload_ticks;
    status.total_energy_nodes = static_cast<int>(energy_nodes.size());
    status.world_hash = computeWorldHash();
    rebuildScene();
  }

  void fail(const std::string &reason) {
    if (status.outcome == AsterGridTacticsOutcome::Playing) {
      status.outcome = AsterGridTacticsOutcome::Defeated;
      status.defeat_reason = reason;
      triggerVisualEvent(AsterGridTacticsVisualEvent::Defeat);
    }
  }

  void triggerVisualEvent(const AsterGridTacticsVisualEvent event) {
    status.visual_event = event;
    status.visual_event_tick = status.tick;
  }

  [[nodiscard]] bool adjacentOrSame(const AsterGridCoord lhs, const AsterGridCoord rhs) const {
    return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y) <= 1;
  }

  void handleInteract() {
    if (adjacentOrSame(status.player, terminal) && status.terminals_hacked == 0) {
      status.terminals_hacked = 1;
      status.door_open = true;
      status.turret_disabled = true;
      triggerVisualEvent(AsterGridTacticsVisualEvent::TerminalHack);
    }
    for (std::size_t i = 0u; i < energy_nodes.size(); ++i) {
      if (!energy_rerouted[i] && adjacentOrSame(status.player, energy_nodes[i])) {
        energy_rerouted[i] = true;
        status.energy_nodes_rerouted =
            static_cast<int>(std::count(energy_rerouted.begin(), energy_rerouted.end(), true));
        triggerVisualEvent(status.energy_nodes_rerouted == status.total_energy_nodes
                               ? AsterGridTacticsVisualEvent::ExtractionOpen
                               : AsterGridTacticsVisualEvent::EnergyReroute);
        break;
      }
    }
    if (sameCoord(status.player, extraction) && status.energy_nodes_rerouted == status.total_energy_nodes &&
        status.terminals_hacked > 0 && status.outcome == AsterGridTacticsOutcome::Playing) {
      status.outcome = AsterGridTacticsOutcome::Victory;
      triggerVisualEvent(AsterGridTacticsVisualEvent::ExtractionComplete);
    }
  }

  void updatePlayer(const SimCommand &command) {
    if (command.pressed(SimCommandButton::Interact)) {
      handleInteract();
    }
    if (player_cooldown_ticks > 0) {
      --player_cooldown_ticks;
      return;
    }
    const Vec2 axis = simCommandMoveAxis(command);
    AsterGridCoord delta{};
    if (std::abs(axis.x) > 0.35f || std::abs(axis.y) > 0.35f) {
      if (std::abs(axis.x) >= std::abs(axis.y)) {
        delta.x = axis.x < 0.0f ? -1 : 1;
      } else {
        delta.y = axis.y < 0.0f ? -1 : 1;
      }
    }
    if (delta.x == 0 && delta.y == 0) {
      return;
    }
    player_facing = facingFromDelta(delta);
    const AsterGridCoord next{status.player.x + delta.x, status.player.y + delta.y};
    if (walkable(next)) {
      player_visual_from = status.player;
      player_visual_move_tick = status.tick;
      status.player = next;
    }
    player_cooldown_ticks = std::max(tuning.player_step_ticks - 1, 0);
  }

  [[nodiscard]] bool clearLine(const AsterGridCoord from, const AsterGridCoord to) const {
    const int dx = (to.x > from.x) - (to.x < from.x);
    const int dy = (to.y > from.y) - (to.y < from.y);
    AsterGridCoord cursor{from.x + dx, from.y + dy};
    while (!sameCoord(cursor, to)) {
      if (blocksSight(cursor)) {
        return false;
      }
      cursor.x += dx;
      cursor.y += dy;
    }
    return true;
  }

  [[nodiscard]] bool guardSeesPlayer(const GuardState &guard) const {
    const AsterGridCoord delta{status.player.x - guard.position.x,
                               status.player.y - guard.position.y};
    const AsterGridCoord forward = facingDelta(guard.facing);
    const int axial = delta.x * forward.x + delta.y * forward.y;
    const int lateral = std::abs(delta.x * forward.y - delta.y * forward.x);
    return axial > 0 && axial <= 5 && lateral <= std::max(1, axial / 2) &&
           clearLine(guard.position, status.player);
  }

  void updateGuards() {
    if (status.tick % static_cast<std::uint32_t>(std::max(tuning.guard_step_ticks, 1)) == 0u) {
      for (GuardState &guard : guards) {
        if (guard.patrol.empty()) {
          continue;
        }
        const std::size_t old_index = guard.patrol_index;
        if (guard.patrol_index == 0u) {
          guard.patrol_direction = 1;
        } else if (guard.patrol_index + 1u == guard.patrol.size()) {
          guard.patrol_direction = -1;
        }
        guard.patrol_index = static_cast<std::size_t>(
            static_cast<int>(guard.patrol_index) + guard.patrol_direction);
        guard.visual_from = guard.position;
        guard.visual_move_tick = status.tick;
        guard.position = guard.patrol[guard.patrol_index];
        const AsterGridCoord previous = guard.patrol[old_index];
        guard.facing = facingFromDelta({guard.position.x - previous.x, guard.position.y - previous.y});
      }
    }
    for (const GuardState &guard : guards) {
      if (guardSeesPlayer(guard)) {
        fail("guard vision cone");
      }
    }
  }

  void fireTurretIfNeeded() {
    if (status.turret_disabled || status.tick % static_cast<std::uint32_t>(std::max(tuning.turret_fire_ticks, 1)) != 0u) {
      return;
    }
    AsterGridCoord direction{};
    if (status.player.y == turret.y) {
      direction.x = status.player.x < turret.x ? -1 : 1;
    } else if (status.player.x == turret.x) {
      direction.y = status.player.y < turret.y ? -1 : 1;
    }
    if ((direction.x != 0 || direction.y != 0) && clearLine(turret, status.player)) {
      projectiles.push_back({.position = turret, .direction = direction});
    }
  }

  void updateProjectiles() {
    fireTurretIfNeeded();
    for (ProjectileState &projectile : projectiles) {
      ++projectile.age_ticks;
      if (projectile.age_ticks % 3 != 0) {
        continue;
      }
      projectile.position.x += projectile.direction.x;
      projectile.position.y += projectile.direction.y;
      if (sameCoord(projectile.position, status.player)) {
        fail("turret projectile");
      }
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                     [&](const ProjectileState &projectile) {
                                       return !inside(projectile.position) ||
                                              blocksSight(projectile.position) ||
                                              projectile.age_ticks > 96;
                                     }),
                      projectiles.end());
  }

  void updateFixed(SimCommand command) {
    if (status.outcome != AsterGridTacticsOutcome::Playing) {
      return;
    }
    command.tick = status.tick;
    command.sequence = command_sequence++;
    replay.record(command);
    updatePlayer(command);
    updateGuards();
    updateProjectiles();
    if (status.outcome == AsterGridTacticsOutcome::Playing) {
      --status.overload_ticks_remaining;
      if (status.overload_ticks_remaining <= 0) {
        fail("energy overload");
      }
      if (sameCoord(status.player, extraction) &&
          status.energy_nodes_rerouted == status.total_energy_nodes && status.terminals_hacked > 0) {
        status.outcome = AsterGridTacticsOutcome::Victory;
        triggerVisualEvent(AsterGridTacticsVisualEvent::ExtractionComplete);
      }
    }
    ++status.tick;
    status.replay_checksum = replay.checksum();
    status.world_hash = computeWorldHash();
    rebuildScene();
  }

  [[nodiscard]] std::uint64_t computeWorldHash() const {
    std::uint64_t hash = 0xA57E600D7A011C5ull;
    hash = hashCombine64(hash, tuning.seed);
    hash = hashCombine64(hash, status.tick);
    hash = hashCombine64(hash, coordBits(status.player));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.outcome));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.overload_ticks_remaining));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.energy_nodes_rerouted));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.terminals_hacked));
    hash = hashCombine64(hash, status.door_open ? 1u : 0u);
    hash = hashCombine64(hash, status.turret_disabled ? 1u : 0u);
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.visual_event));
    hash = hashCombine64(hash, status.visual_event_tick);
    hash = hashCombine64(hash, static_cast<std::uint64_t>(player_facing));
    for (const GuardState &guard : guards) {
      hash = hashCombine64(hash, coordBits(guard.position));
      hash = hashCombine64(hash, static_cast<std::uint64_t>(guard.facing));
    }
    for (const ProjectileState &projectile : projectiles) {
      hash = hashCombine64(hash, coordBits(projectile.position));
      hash = hashCombine64(hash, coordBits(projectile.direction));
    }
    return hashCombine64(hash, replay.checksum());
  }

  void addObject(std::string name, const AsterGridCoord coord, const Material &material,
                 const Vec3 scale = {0.92f, 0.06f, 0.92f}, const float y = 0.03f,
                 const MeshPrimitive primitive = MeshPrimitive::Box) {
    RenderObject object;
    object.name = std::move(name);
    object.primitive = primitive;
    object.custom_mesh = nullptr;
    object.transform.position = worldPosition(coord, y);
    object.transform.scale = scale;
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = primitive != MeshPrimitive::Plane;
    object.casts_contact_shadow = primitive != MeshPrimitive::Plane && scale.y > 0.10f;
    object.contact_shadow_strength = 0.42f;
    object.contact_shadow_radius_scale = 0.72f;
    object.auto_contact_shadow = primitive != MeshPrimitive::Plane;
    scene.objects().push_back(std::move(object));
  }

  void addWorldBox(std::string name, const Vec3 position, const Vec3 scale,
                   const Material &material, const bool contact_shadow = false) {
    RenderObject object;
    object.name = std::move(name);
    object.primitive = MeshPrimitive::Box;
    object.transform.position = position;
    object.transform.scale = scale;
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = false;
    object.casts_contact_shadow = contact_shadow;
    object.contact_shadow_strength = contact_shadow ? 0.40f : 0.0f;
    object.auto_contact_shadow = contact_shadow;
    scene.objects().push_back(std::move(object));
  }

  void addMeshObject(std::string name, std::shared_ptr<const CpuMesh> mesh, const Vec3 position,
                     const float yaw, const Vec3 scale, const Material &material,
                     const bool contact_shadow = false) {
    RenderObject object;
    object.name = std::move(name);
    object.primitive = MeshPrimitive::Plane;
    object.custom_mesh = std::move(mesh);
    object.transform = Transform::fromEuler(position, {0.0f, yaw, 0.0f}, scale);
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = false;
    object.casts_contact_shadow = contact_shadow;
    object.contact_shadow_strength = contact_shadow ? 0.32f : 0.0f;
    object.contact_shadow_radius_scale = 0.82f;
    object.auto_contact_shadow = contact_shadow;
    scene.objects().push_back(std::move(object));
  }

  [[nodiscard]] float eventIntensity(const AsterGridTacticsVisualEvent event,
                                     const std::uint32_t duration_ticks) const {
    if (status.visual_event != event || status.tick < status.visual_event_tick) {
      return 0.0f;
    }
    const std::uint32_t age = status.tick - status.visual_event_tick;
    if (age > duration_ticks) {
      return 0.0f;
    }
    const float t = 1.0f - static_cast<float>(age) / static_cast<float>(duration_ticks);
    return std::clamp(t * t, 0.0f, 1.0f);
  }

  [[nodiscard]] float tickPulse(const float rate, const float phase = 0.0f) const {
    return 0.5f + 0.5f * std::sin(static_cast<float>(status.tick) * rate + phase);
  }

  void addWorldSegmentBox(std::string name, const AsterGridCoord from, const AsterGridCoord to,
                          const float width, const float height, const Material &material,
                          const float y = 0.070f) {
    const Vec3 a = worldPosition(from, y);
    const Vec3 b = worldPosition(to, y);
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float length = std::max(std::sqrt(dx * dx + dz * dz), 0.001f);
    RenderObject object;
    object.name = std::move(name);
    object.primitive = MeshPrimitive::Box;
    object.transform.position = {(a.x + b.x) * 0.5f, y, (a.z + b.z) * 0.5f};
    object.transform.rotation = quatFromEulerXyz({0.0f, std::atan2(dx, dz), 0.0f});
    object.transform.scale = {width, height, length};
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = false;
    object.auto_contact_shadow = false;
    scene.objects().push_back(std::move(object));
  }

  void addRoutedWorldSegmentBoxes(const std::string &name, const AsterGridCoord from,
                                  const AsterGridCoord to, const float width,
                                  const float height, const Material &material,
                                  const float y = 0.070f) {
    if (from.x == to.x || from.y == to.y) {
      addWorldSegmentBox(name, from, to, width, height, material, y);
      return;
    }
    const AsterGridCoord bend{to.x, from.y};
    addWorldSegmentBox(name, from, bend, width, height, material, y);
    addWorldSegmentBox(name, bend, to, width, height, material, y);
  }

  void addPanelFrame(const Vec3 center, const float width, const float depth,
                     const Material &seam) {
    const float half_width = width * 0.5f;
    const float half_depth = depth * 0.5f;
    addWorldBox("Gunmetal panel seam horizontal", {center.x, 0.076f, center.z - half_depth},
                {std::max(width - 0.08f, 0.12f), 0.012f, 0.018f}, seam);
    addWorldBox("Gunmetal panel seam horizontal", {center.x, 0.076f, center.z + half_depth},
                {std::max(width - 0.08f, 0.12f), 0.012f, 0.018f}, seam);
    addWorldBox("Gunmetal panel seam vertical", {center.x - half_width, 0.077f, center.z},
                {0.018f, 0.012f, std::max(depth - 0.08f, 0.12f)}, seam);
    addWorldBox("Gunmetal panel seam vertical", {center.x + half_width, 0.077f, center.z},
                {0.018f, 0.012f, std::max(depth - 0.08f, 0.12f)}, seam);
  }

  void addCone(const GuardState &guard, const Material &material) {
    Material danger = material;
    danger.asset_id.clear();
    danger.opacity = 0.16f;
    danger.emission_strength = 0.12f;
    const AsterGridCoord forward = facingDelta(guard.facing);
    for (int step = 1; step <= 1; ++step) {
      const AsterGridCoord current{guard.position.x + forward.x * step,
                                   guard.position.y + forward.y * step};
      if (!inside(current) || blocksSight(current)) {
        break;
      }
      addWorldBox("Guard vision cone", worldPosition(current, 0.104f),
                  {0.48f, 0.014f, 0.48f}, danger);
    }
  }

  void addEnergyLine(const AsterGridCoord from, const AsterGridCoord to, const bool active,
                     Material material) {
    const float pulse = active ? tickPulse(0.23f, static_cast<float>(from.x + to.y)) : 0.0f;
    if (active) {
      material.emission_strength += 0.65f + pulse * 0.85f;
      material.opacity = std::min(1.0f, material.opacity + 0.04f);
    } else {
      material.opacity = 0.54f;
      material.emission_strength = 0.08f;
      material.base_color = material.base_color * 0.62f;
    }
    Material core = material;
    core.asset_id.clear();
    core.opacity = active ? 0.86f : 0.36f;
    core.emission_strength = active ? material.emission_strength + 0.70f : 0.10f;
    (void)pulse;
    addWorldSegmentBox(active ? "Routed energy conduit" : "Dormant energy conduit", from, to,
                       active ? 0.18f : 0.12f, active ? 0.030f : 0.022f,
                       active ? core : material, active ? 0.128f : 0.104f);
  }

  void addEnergyRoute(const AsterGridCoord from, const AsterGridCoord to, const bool active,
                      const Material &material) {
    if (from.x == to.x || from.y == to.y) {
      addEnergyLine(from, to, active, material);
      return;
    }
    const AsterGridCoord bend{to.x, from.y};
    addEnergyLine(from, bend, active, material);
    addEnergyLine(bend, to, active, material);
  }

  void addGridLines(const Material &material) {
    const float origin_x = static_cast<float>(tuning.width) * -0.5f;
    const float origin_z = static_cast<float>(tuning.height) * -0.5f;
    for (int x = 0; x <= tuning.width; ++x) {
      addWorldBox("Debug floor grid vertical", {origin_x + static_cast<float>(x), 0.080f, 0.0f},
                  {0.010f, 0.018f, static_cast<float>(tuning.height)}, material);
    }
    for (int y = 0; y <= tuning.height; ++y) {
      addWorldBox("Debug floor grid horizontal", {0.0f, 0.081f, origin_z + static_cast<float>(y)},
                  {static_cast<float>(tuning.width), 0.018f, 0.010f}, material);
    }
  }

  void rebuildScene() {
    scene.objects().clear();
    scene.reflectionProbes().clear();

    Material floor_base = gridMaterial("material.grid_lab_floor", {0.040f, 0.048f, 0.055f},
                                       {0.00f, 0.022f, 0.030f}, 0.020f);
    floor_base.pattern_scale = {2.8f, 2.4f};
    floor_base.detail_strength = 0.36f;
    floor_base.pattern_depth = 0.12f;
    floor_base.procedural.macro_variation = 0.24f;
    floor_base.procedural.micro_frequency_breakup = 0.52f;
    Material floor_panel = floor_base;
    floor_panel.base_color = {0.058f, 0.070f, 0.078f};
    floor_panel.pattern_depth = 0.10f;
    Material cable_trench =
        gridMaterial("material.grid_lab_wall", {0.020f, 0.028f, 0.032f}, {0.0f, 0.055f, 0.060f}, 0.08f);
    cable_trench.pattern_depth = 0.08f;
    Material panel_seam =
        neonMaterial("material.grid_neon", {0.012f, 0.030f, 0.034f}, {0.10f, 0.72f, 0.78f}, 0.36f, 0.58f);
    Material panel_shadow =
        gridMaterial("material.grid_lab_wall", {0.010f, 0.014f, 0.016f}, {0.0f, 0.0f, 0.0f}, 0.0f);
    Material wall = gridMaterial("material.grid_lab_wall", {0.045f, 0.058f, 0.066f},
                                 {0.04f, 0.12f, 0.16f}, 0.10f);
    wall.pattern_depth = 0.24f;
    wall.procedural.cavity_grime = 0.52f;
    Material debug_grid =
        neonMaterial("material.grid_neon", {0.02f, 0.10f, 0.12f}, {0.10f, 0.55f, 0.58f}, 0.32f, 0.38f);
    Material door_closed =
        gridMaterial("material.grid_security_red", {0.18f, 0.040f, 0.055f},
                     {1.0f, 0.08f, 0.14f}, 1.30f);
    Material door_open =
        neonMaterial("material.grid_energy_green", {0.018f, 0.12f, 0.080f},
                     {0.14f, 1.00f, 0.50f}, 1.25f, 0.74f);
    Material terminal_body =
        gridMaterial("material.grid_lab_wall", {0.030f, 0.060f, 0.070f}, {0.08f, 0.36f, 0.42f}, 0.26f);
    Material terminal_screen =
        neonMaterial("material.grid_neon", {0.24f, 0.12f, 0.018f}, {1.0f, 0.62f, 0.12f},
                     status.terminals_hacked > 0 ? 2.60f : 1.35f, 0.95f);
    Material scan_line =
        neonMaterial("material.grid_neon", {0.12f, 0.42f, 0.34f}, {0.22f, 1.0f, 0.80f}, 3.6f, 0.82f);
    Material energy_off =
        gridMaterial("material.grid_lab_wall", {0.034f, 0.052f, 0.046f}, {0.06f, 0.22f, 0.14f}, 0.28f);
    Material energy_on =
        neonMaterial("material.grid_energy_green", {0.020f, 0.170f, 0.120f}, {0.18f, 1.0f, 0.66f},
                     2.45f, 0.94f);
    Material extraction_mat =
        neonMaterial("material.grid_energy_green", {0.020f, 0.13f, 0.085f}, {0.18f, 1.0f, 0.48f},
                     status.energy_nodes_rerouted == status.total_energy_nodes ? 2.40f : 0.62f,
                     0.92f);
    Material player =
        gridMaterial("material.grid_operator_blue", {0.035f, 0.090f, 0.170f}, {0.08f, 0.38f, 0.72f}, 0.34f,
                     1.0f, MaterialSurfaceProfile::Resin, SurfacePattern::AmberResin);
    player.pattern_depth = 0.06f;
    Material player_core =
        neonMaterial("material.grid_operator_blue", {0.04f, 0.20f, 0.62f}, {0.24f, 0.80f, 1.0f},
                     1.55f + tickPulse(0.34f) * 0.38f);
    Material guard =
        gridMaterial("material.grid_security_red", {0.170f, 0.025f, 0.038f}, {1.0f, 0.03f, 0.10f}, 0.48f,
                     1.0f, MaterialSurfaceProfile::CorrodedMetal, SurfacePattern::WeatheredMetal);
    Material guard_eye =
        neonMaterial("material.grid_security_red", {0.70f, 0.015f, 0.055f}, {1.0f, 0.04f, 0.10f}, 2.35f);
    Material cone =
        neonMaterial("material.grid_security_red", {0.70f, 0.035f, 0.080f}, {1.0f, 0.04f, 0.11f},
                     status.turret_disabled ? 0.38f : 0.62f, status.turret_disabled ? 0.14f : 0.22f);
    Material turret_mat =
        status.turret_disabled
            ? gridMaterial("material.grid_lab_wall", {0.050f, 0.070f, 0.072f}, {0.02f, 0.07f, 0.06f}, 0.08f)
            : gridMaterial("material.grid_security_red", {0.22f, 0.050f, 0.045f}, {1.0f, 0.30f, 0.08f}, 1.50f);
    Material projectile =
        neonMaterial("material.grid_security_red", {0.86f, 0.18f, 0.04f}, {1.0f, 0.20f, 0.03f}, 2.8f);
    makeRuntimePresentationMaterial(floor_base, 0.075f);
    makeRuntimePresentationMaterial(floor_panel, 0.090f);
    makeRuntimePresentationMaterial(cable_trench, 0.080f);
    makeRuntimePresentationMaterial(panel_seam, 0.040f);
    makeRuntimePresentationMaterial(panel_shadow, 0.045f);
    makeRuntimePresentationMaterial(wall, 0.120f);
    makeRuntimePresentationMaterial(debug_grid, 0.040f);
    makeRuntimePresentationMaterial(door_closed, 0.100f);
    makeRuntimePresentationMaterial(door_open, 0.040f);
    makeRuntimePresentationMaterial(terminal_body, 0.090f);
    makeRuntimePresentationMaterial(terminal_screen, 0.035f);
    makeRuntimePresentationMaterial(scan_line, 0.035f);
    makeRuntimePresentationMaterial(energy_off, 0.060f);
    makeRuntimePresentationMaterial(energy_on, 0.035f);
    makeRuntimePresentationMaterial(extraction_mat, 0.035f);
    makeRuntimePresentationMaterial(player, 0.075f);
    makeRuntimePresentationMaterial(player_core, 0.030f);
    makeRuntimePresentationMaterial(guard, 0.080f);
    makeRuntimePresentationMaterial(guard_eye, 0.030f);
    makeRuntimePresentationMaterial(cone, 0.020f);
    makeRuntimePresentationMaterial(turret_mat, 0.080f);
    makeRuntimePresentationMaterial(projectile, 0.030f);

    addWorldBox("Gunmetal lab floor foundation", {0.0f, 0.010f, 0.0f},
                {static_cast<float>(tuning.width), 0.030f, static_cast<float>(tuning.height)},
                floor_base);

    const float origin_x = static_cast<float>(tuning.width) * -0.5f;
    const float origin_z = static_cast<float>(tuning.height) * -0.5f;
    for (int y = 1; y < tuning.height - 1; y += 3) {
      for (int x = 1; x < tuning.width - 1; x += 4) {
        Material panel = floor_panel;
        const float variation = tileVariation({x, y}, tuning.seed);
        panel.base_color = panel.base_color * (0.88f + variation * 0.16f);
        if (x >= 10 && y >= 3) {
          panel.base_color = panel.base_color * 0.72f + LinearRgb{0.060f, 0.032f, 0.052f} * 0.28f;
        } else if (x <= 5 && y >= 6) {
          panel.base_color = panel.base_color * 0.78f + LinearRgb{0.070f, 0.082f, 0.076f} * 0.22f;
        }
        panel.procedural.wetness = variation > 0.76f ? 0.08f : 0.02f;
        const float width = std::min(3.58f, static_cast<float>(tuning.width - 1 - x));
        const float height = std::min(2.58f, static_cast<float>(tuning.height - 1 - y));
        const Vec3 panel_center{origin_x + static_cast<float>(x) + width * 0.5f,
                                0.038f,
                                origin_z + static_cast<float>(y) + height * 0.5f};
        addWorldBox("Gunmetal floor panel", panel_center, {width, 0.025f, height}, panel);
        addPanelFrame(panel_center, width, height, panel_seam);
      }
    }

    addWorldBox("Reactor Lab Bench", worldOffset({3, 2}, 0.0f, 0.0f, 0.160f),
                {1.68f, 0.20f, 0.34f}, terminal_body, true);
    addWorldBox("Reactor Bench Worklight", worldOffset({3, 2}, 0.0f, -0.02f, 0.282f),
                {1.24f, 0.020f, 0.050f}, panel_seam);
    addWorldBox("Security Server Rack", worldOffset({12, 2}, 0.0f, 0.0f, 0.260f),
                {0.42f, 0.48f, 1.12f}, wall, true);
    addWorldBox("Security Rack Status Slit", worldOffset({12, 2}, 0.0f, -0.36f, 0.520f),
                {0.28f, 0.020f, 0.050f}, door_closed);
    addWorldBox("Extraction Threshold Plate", worldOffset(extraction, 0.0f, 0.50f, 0.130f),
                {1.22f, 0.040f, 0.12f},
                status.energy_nodes_rerouted == status.total_energy_nodes ? extraction_mat : cable_trench);

    if (!energy_nodes.empty()) {
      addRoutedWorldSegmentBoxes("Inset energy cable trench", terminal, energy_nodes[0], 0.34f,
                                 0.030f, cable_trench, 0.068f);
    }
    if (energy_nodes.size() > 1u) {
      addRoutedWorldSegmentBoxes("Inset energy cable trench", energy_nodes[0], energy_nodes[1],
                                 0.34f, 0.030f, cable_trench, 0.068f);
      addRoutedWorldSegmentBoxes("Inset energy cable trench", energy_nodes[1], extraction, 0.34f,
                                 0.030f, cable_trench, 0.068f);
    }

    for (int y = 0; y < tuning.height; ++y) {
      for (int x = 0; x < tuning.width; ++x) {
        const AsterGridCoord coord{x, y};
        switch (tile(coord)) {
        case TileKind::Wall:
          addObject("Raised machinery wall", coord, wall, {0.92f, 0.34f, 0.92f}, 0.205f);
          break;
        case TileKind::Door:
          addObject(status.door_open ? "Open security door slab" : "Closed security door slab",
                    coord, status.door_open ? door_open : door_closed,
                    status.door_open ? Vec3{0.92f, 0.075f, 0.92f} : Vec3{0.92f, 0.62f, 0.92f},
                    status.door_open ? 0.105f : 0.33f);
          addWorldBox("Security door side rail", worldOffset(coord, -0.48f, 0.0f, 0.260f),
                      {0.10f, 0.36f, 0.72f}, status.door_open ? door_open : door_closed, true);
          addWorldBox("Security door side rail", worldOffset(coord, 0.48f, 0.0f, 0.260f),
                      {0.10f, 0.36f, 0.72f}, status.door_open ? door_open : door_closed, true);
          break;
        case TileKind::Terminal: {
          addObject("Terminal Console Base", coord, terminal_body, {0.88f, 0.24f, 0.70f}, 0.150f);
          addWorldBox("Terminal Console Screen", worldOffset(coord, -0.04f, -0.10f, 0.318f),
                      {0.62f, 0.046f, 0.30f}, terminal_screen);
          addWorldBox("Terminal Key Bank", worldOffset(coord, -0.04f, 0.26f, 0.210f),
                      {0.48f, 0.036f, 0.12f}, panel_shadow);
          addWorldBox("Terminal Cable Port", worldOffset(coord, 0.42f, 0.0f, 0.190f),
                      {0.08f, 0.060f, 0.44f}, cable_trench);
          if (status.terminals_hacked > 0 ||
              eventIntensity(AsterGridTacticsVisualEvent::TerminalHack, 80u) > 0.0f) {
            const float scan_energy = eventIntensity(AsterGridTacticsVisualEvent::TerminalHack, 80u);
            Material scan = scan_line;
            scan.opacity = std::min(0.42f, 0.20f + scan_energy * 0.18f);
            scan.asset_id.clear();
            addWorldBox("Terminal Hack Scanline", worldOffset(coord, -0.04f, -0.10f, 0.382f),
                        {0.56f, 0.018f, 0.038f}, scan);
          }
          break;
        }
        case TileKind::EnergyNode: {
          const auto found = std::find_if(energy_nodes.begin(), energy_nodes.end(),
                                          [&](const AsterGridCoord node) {
                                            return sameCoord(node, coord);
                                          });
          const std::size_t node_index =
              found == energy_nodes.end() ? 0u : static_cast<std::size_t>(found - energy_nodes.begin());
          const bool active = node_index < energy_rerouted.size() && energy_rerouted[node_index];
          const Material &node_material = active ? energy_on : energy_off;
          addObject(active ? "Rerouted Energy Node Housing" : "Dormant Energy Node Housing", coord,
                    cable_trench, {0.92f, 0.14f, 0.92f}, 0.115f);
          addWorldBox("Energy Node Anchor Clamp", worldOffset(coord, -0.48f, 0.0f, 0.205f),
                      {0.12f, 0.10f, 0.44f}, cable_trench);
          addWorldBox("Energy Node Anchor Clamp", worldOffset(coord, 0.48f, 0.0f, 0.205f),
                      {0.12f, 0.10f, 0.44f}, cable_trench);
          addWorldBox("Energy Node Anchor Clamp", worldOffset(coord, 0.0f, -0.48f, 0.205f),
                      {0.44f, 0.10f, 0.12f}, cable_trench);
          addWorldBox("Energy Node Anchor Clamp", worldOffset(coord, 0.0f, 0.48f, 0.205f),
                      {0.44f, 0.10f, 0.12f}, cable_trench);
          addMeshObject(active ? "Energy Node Ring" : "Dormant Energy Node Ring", node_ring_mesh,
                        worldPosition(coord, 0.225f), 0.0f,
                        active ? Vec3{1.22f, 1.0f, 1.22f} : Vec3{1.05f, 1.0f, 1.05f},
                        node_material);
          addObject(active ? "Energy Node Lens" : "Dormant Energy Node Lens", coord, node_material,
                    active ? Vec3{0.48f, 0.56f, 0.48f} : Vec3{0.38f, 0.32f, 0.38f},
                    active ? 0.45f : 0.31f, MeshPrimitive::Crystal);
          break;
        }
        case TileKind::Extraction: {
          const bool live = status.energy_nodes_rerouted == status.total_energy_nodes && status.terminals_hacked > 0;
          Material gate = live ? extraction_mat : energy_off;
          addObject(live ? "Extraction Gate Base" : "Sealed Extraction Gate Base", coord,
                    cable_trench, {0.86f, 0.12f, 0.86f}, 0.110f);
          addMeshObject(live ? "Extraction Gate Ring" : "Sealed Extraction Gate Ring",
                        extraction_ring_mesh, worldPosition(coord, 0.245f), 0.0f,
                        live ? Vec3{1.32f, 1.0f, 1.32f} : Vec3{1.08f, 1.0f, 1.08f}, gate);
          addWorldBox("Extraction Gate Left Pylon",
                      worldOffset(coord, -0.52f, 0.0f, 0.30f), {0.12f, 0.42f, 0.56f},
                      live ? extraction_mat : wall, true);
          addWorldBox("Extraction Gate Right Pylon",
                      worldOffset(coord, 0.52f, 0.0f, 0.30f), {0.12f, 0.42f, 0.56f},
                      live ? extraction_mat : wall, true);
          if (status.outcome == AsterGridTacticsOutcome::Victory) {
            Material burst = extraction_mat;
            burst.asset_id.clear();
            burst.opacity = 0.64f;
            burst.emission_strength += 1.8f + tickPulse(0.18f) * 0.9f;
            addMeshObject("Extraction Burst Ring", pulse_ring_mesh, worldPosition(coord, 0.275f),
                          0.0f, {1.78f, 1.0f, 1.78f}, burst);
          }
          break;
        }
        case TileKind::Turret: {
          const float dim = status.turret_disabled ? 0.42f : 1.0f;
          Material aperture = status.turret_disabled ? energy_off : projectile;
          aperture.emission_strength *= dim;
          addObject(status.turret_disabled ? "Security Turret Disabled Base" : "Security Turret Base",
                    coord, turret_mat, {0.82f, 0.22f, 0.82f}, 0.150f);
          addMeshObject(status.turret_disabled ? "Security Turret Disabled Ring" : "Security Turret Tracking Ring",
                        node_ring_mesh, worldPosition(coord, 0.272f), 0.0f,
                        {0.78f, 1.0f, 0.78f}, aperture);
          addWorldBox(status.turret_disabled ? "Security Turret Offline Barrel" : "Security Turret Barrel",
                      worldOffset(coord, 0.0f, -0.20f, 0.350f), {0.20f, 0.060f, 0.46f},
                      aperture);
          addObject(status.turret_disabled ? "Security Turret Dim Aperture" : "Security Turret Aperture",
                    coord, aperture, {0.18f, 0.08f, 0.18f}, 0.405f);
          break;
        }
        case TileKind::Floor:
          break;
        }
      }
    }
    if (tuning.debug_grid) {
      addGridLines(debug_grid);
    }

    if (!energy_nodes.empty()) {
      addEnergyRoute(terminal, energy_nodes[0], !energy_rerouted.empty() && energy_rerouted[0],
                     !energy_rerouted.empty() && energy_rerouted[0] ? energy_on : energy_off);
    }
    if (energy_nodes.size() > 1u) {
      addEnergyRoute(energy_nodes[0], energy_nodes[1],
                     energy_rerouted.size() > 1u && energy_rerouted[1],
                     energy_rerouted.size() > 1u && energy_rerouted[1] ? energy_on : energy_off);
      addEnergyRoute(energy_nodes[1], extraction,
                     status.energy_nodes_rerouted == status.total_energy_nodes,
                     status.energy_nodes_rerouted == status.total_energy_nodes ? energy_on : energy_off);
    }

    for (const GuardState &guard_state : guards) {
      addCone(guard_state, cone);
      const float yaw = facingYaw(guard_state.facing);
      const Vec3 guard_center =
          interpolatedWorldPosition(guard_state.visual_from, guard_state.position,
                                    guard_state.visual_move_tick, tuning.guard_step_ticks, 0.365f);
      const auto guardOffset = [&](const float lateral, const float forward, const float y) {
        return Vec3{guard_center.x + std::cos(yaw) * lateral + std::sin(yaw) * forward, y,
                    guard_center.z - std::sin(yaw) * lateral + std::cos(yaw) * forward};
      };
      addMeshObject("Guard Drone Hull", guard_mesh, guard_center, yaw, {1.32f, 1.0f, 1.32f},
                    guard, true);
      addWorldBox("Guard Drone Eye Slit", guardOffset(0.0f, 0.25f, 0.426f),
                  {0.34f, 0.050f, 0.060f}, guard_eye);
    }
    (void)projectile;
    const float player_yaw = facingYaw(player_facing);
    const Vec3 player_center =
        interpolatedWorldPosition(player_visual_from, status.player, player_visual_move_tick,
                                  tuning.player_step_ticks, 0.375f);
    const auto playerOffset = [&](const float lateral, const float forward, const float y) {
      return Vec3{player_center.x + std::cos(player_yaw) * lateral + std::sin(player_yaw) * forward,
                  y,
                  player_center.z - std::sin(player_yaw) * lateral + std::cos(player_yaw) * forward};
    };
    addMeshObject("Operator Hull", operator_mesh, player_center, player_yaw,
                  {1.34f, 1.0f, 1.34f}, player, true);
    addWorldBox("Operator Shoulder Pod", playerOffset(-0.28f, -0.08f, 0.405f),
                {0.16f, 0.070f, 0.28f}, player);
    addWorldBox("Operator Shoulder Pod", playerOffset(0.28f, -0.08f, 0.405f),
                {0.16f, 0.070f, 0.28f}, player);
    addWorldBox("Operator Core Lens", playerOffset(0.0f, -0.02f, 0.468f),
                {0.28f, 0.048f, 0.28f}, player_core);
  }

  [[nodiscard]] SimCommand scriptedCommand(const std::uint32_t tick) const {
    SimCommand command;
    command.tick = tick;
    std::uint32_t cursor = 0u;
    const std::vector<ScriptAction> actions = scriptedActions();
    for (const ScriptAction &action : actions) {
      const std::uint32_t end = cursor + static_cast<std::uint32_t>(std::max(action.ticks, 1));
      if (tick >= cursor && tick < end) {
        command.strafe = static_cast<std::int16_t>(action.dx * 32767);
        command.forward = static_cast<std::int16_t>(action.dy * 32767);
        command.set(SimCommandButton::Interact, action.interact);
        return command;
      }
      cursor = end;
    }
    return command;
  }

  [[nodiscard]] AsterGridTacticsHudModel hudModel() const {
    AsterGridTacticsHudModel model;
    model.title = "Aster Grid Tactics";
    model.terminal_hacked = status.terminals_hacked > 0;
    model.door_open = status.door_open;
    model.turret_disabled = status.turret_disabled;
    model.victory = status.outcome == AsterGridTacticsOutcome::Victory;
    model.defeated = status.outcome == AsterGridTacticsOutcome::Defeated;
    model.overload_fraction = tuning.overload_ticks <= 0
                                  ? 0.0f
                                  : std::clamp(static_cast<float>(status.overload_ticks_remaining) /
                                                   static_cast<float>(tuning.overload_ticks),
                                               0.0f, 1.0f);
    model.power_label = status.energy_nodes_rerouted == status.total_energy_nodes
                            ? "POWER ONLINE"
                            : "POWER " + std::to_string(status.energy_nodes_rerouted) + "/" +
                                  std::to_string(status.total_energy_nodes);
    model.terminal_label = model.terminal_hacked ? "TERMINAL BREACHED" : "TERMINAL LOCKED";
    model.exit_label = model.victory
                           ? "EXTRACTED"
                           : (status.energy_nodes_rerouted == status.total_energy_nodes &&
                                      model.terminal_hacked
                                  ? "EXIT LIVE"
                                  : "EXIT SEALED");
    model.objective = status.energy_nodes_rerouted < status.total_energy_nodes
                          ? "Route power. Breach terminal. Slip to extraction."
                          : "Extraction gate is live. Move.";
    if (model.defeated && !status.defeat_reason.empty()) {
      model.status_line = "ALERT: " + status.defeat_reason;
    } else if (model.victory) {
      model.status_line = "SIGNAL CLEAN";
    } else if (model.turret_disabled) {
      model.status_line = "SECURITY LOOP DARK";
    } else {
      model.status_line = "LOW PROFILE";
    }
    if (status.visual_event != AsterGridTacticsVisualEvent::None &&
        status.tick >= status.visual_event_tick && status.tick - status.visual_event_tick <= 96u) {
      model.callout_line = visualEventText(status.visual_event);
    }
    return model;
  }
};

AsterGridTactics::AsterGridTactics(AsterGridTacticsTuning tuning)
    : impl_(std::make_unique<Impl>(std::move(tuning))) {}

AsterGridTactics::~AsterGridTactics() = default;

AsterGridTactics::AsterGridTactics(AsterGridTactics &&) noexcept = default;

AsterGridTactics &AsterGridTactics::operator=(AsterGridTactics &&) noexcept = default;

void AsterGridTactics::reset() {
  impl_->reset();
}

void AsterGridTactics::updateFixed(const SimCommand command) {
  impl_->updateFixed(command);
}

SimCommand AsterGridTactics::scriptedCommand(const std::uint32_t tick) const {
  return impl_->scriptedCommand(tick);
}

const Scene &AsterGridTactics::scene() const {
  return impl_->scene;
}

const AsterGridTacticsStatus &AsterGridTactics::status() const {
  return impl_->status;
}

const CommandReplay &AsterGridTactics::replay() const {
  return impl_->replay;
}

AsterGridTacticsHudModel AsterGridTactics::hudModel() const {
  return impl_->hudModel();
}

std::uint64_t AsterGridTactics::worldHash() const {
  return impl_->status.world_hash;
}

} // namespace aster
