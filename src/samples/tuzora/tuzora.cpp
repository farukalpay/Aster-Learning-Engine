// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/tuzora/tuzora.hpp"

#include "aster/math/hash.hpp"
#include "aster/render/parallax2d.hpp"
#include "aster/render/sprite2d.hpp"
#include "aster/scene/tile_layer2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace aster {
namespace {

constexpr float kPlayerHalfWidth = 0.30f;
constexpr float kPlayerHalfHeight = 0.78f;
constexpr float kGravityPerTick = -0.0185f;
constexpr float kJumpVelocity = 0.36f;
constexpr float kWalkSpeed = 0.105f;
constexpr float kRunSpeed = 0.142f;
constexpr Vec2 kSignalPosition{34.5f, 6.45f};

enum class PickupKind : std::uint8_t {
  Shells,
  Wood,
  Scrap,
  ShadeCloth,
  SignalPart,
};

struct Pickup {
  std::string id;
  PickupKind kind = PickupKind::Shells;
  Vec2 position{};
  bool collected = false;
};

struct PlacedMarker {
  std::string id;
  Vec2 position{};
  TuzoraBuildSlot slot = TuzoraBuildSlot::Lamp;
};

struct ScriptGoal {
  Vec2 target{};
  bool interact = false;
  bool primary = false;
  bool secondary = false;
  TuzoraBuildSlot slot = TuzoraBuildSlot::WoodPlank;
};

[[nodiscard]] float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] int slotIndex(const TuzoraBuildSlot slot) {
  return static_cast<int>(slot);
}

[[nodiscard]] const char *slotName(const TuzoraBuildSlot slot) {
  switch (slot) {
  case TuzoraBuildSlot::SandStone:
    return "sand";
  case TuzoraBuildSlot::WoodPlank:
    return "wood";
  case TuzoraBuildSlot::ShadeCloth:
    return "shade";
  case TuzoraBuildSlot::Lamp:
    return "lamp";
  case TuzoraBuildSlot::SignalPart:
    return "signal";
  }
  return "wood";
}

[[nodiscard]] std::uint64_t hashText(std::uint64_t hash, const std::string &text) {
  for (const char c : text) {
    hash = hashCombine64(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return hash;
}

[[nodiscard]] std::uint64_t hashFloat(std::uint64_t hash, const float value) {
  return hashCombine64(hash, static_cast<std::uint64_t>(stableHash32(value)));
}

[[nodiscard]] float planarDistance(const Vec2 lhs, const Vec2 rhs) {
  const float dx = lhs.x - rhs.x;
  const float dy = lhs.y - rhs.y;
  return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] Material flatMaterial(const std::string &id, const LinearRgb base,
                                    const EmissionColor emission = {},
                                    const float emission_strength = 0.0f,
                                    const float opacity = 1.0f) {
  Material material = makeSpriteMaterial2D(id, base, emission, emission_strength, opacity);
  material.detail_strength = 0.02f;
  return material;
}

[[nodiscard]] Material tileMaterial(const std::string &tile_id, const std::uint8_t autotile_mask,
                                    const float day_fraction) {
  const float heat = saturate(1.0f - std::abs(day_fraction - 0.44f) * 1.65f);
  if (tile_id.find("wood") != std::string::npos || tile_id == "pier") {
    return flatMaterial("material.tuzora.wood_plank",
                        {0.56f + heat * 0.08f, 0.34f + heat * 0.05f, 0.18f}, {}, 0.0f, 1.0f);
  }
  if (tile_id.find("lamp") != std::string::npos) {
    return flatMaterial("material.tuzora.lamp", {0.82f, 0.54f, 0.20f}, {1.0f, 0.56f, 0.20f}, 0.7f,
                        1.0f);
  }
  if (tile_id.find("shade") != std::string::npos) {
    return flatMaterial("material.tuzora.shade_cloth", {0.18f, 0.50f, 0.58f}, {0.02f, 0.08f, 0.10f},
                        0.08f, 0.92f);
  }
  if (tile_id.find("crate") != std::string::npos) {
    return flatMaterial("material.tuzora.scrap_crate", {0.42f, 0.30f, 0.24f}, {0.12f, 0.05f, 0.02f},
                        0.06f, 1.0f);
  }
  const float edge = static_cast<float>(autotile_mask & 0x3u) * 0.012f;
  return flatMaterial("material.tuzora.sand_stone",
                      {0.72f + heat * 0.12f - edge, 0.58f + heat * 0.09f, 0.36f + heat * 0.04f}, {},
                      0.0f, 1.0f);
}

[[nodiscard]] SpriteAtlasRegion2D region(const std::string &id, const Vec2 size) {
  return {.id = id, .uv_min = {0.0f, 0.0f}, .uv_max = {1.0f, 1.0f}, .size = size};
}

} // namespace

struct Tuzora::Impl {
  explicit Impl(TuzoraTuning tuning_in)
      : tuning(std::move(tuning_in)),
        tile_mesh(makeSharedSpriteQuad2D({.region = region("tile", {1.0f, 1.0f})})),
        player_mesh(makeSharedSpriteQuad2D({.region = region("player", {0.72f, 1.54f})})),
        pickup_mesh(makeSharedSpriteQuad2D({.region = region("pickup", {0.42f, 0.42f})})),
        signal_mesh(makeSharedSpriteQuad2D({.region = region("signal", {1.18f, 1.60f})})),
        glow_mesh(makeSharedSpriteQuad2D({.region = region("glow", {2.4f, 2.4f})})),
        band_mesh(makeSharedSpriteQuad2D({.region = region("band", {1.0f, 1.0f})})) {
    reset();
  }

  TuzoraTuning tuning;
  Scene scene;
  SparseTileLayer2D tiles;
  CommandReplay replay;
  DeterministicRandomStream rng{0u};
  TuzoraStatus status;
  Vec2 player{6.2f, 1.92f};
  Vec2 velocity{};
  std::uint32_t command_sequence = 0u;
  std::shared_ptr<const CpuMesh> tile_mesh;
  std::shared_ptr<const CpuMesh> player_mesh;
  std::shared_ptr<const CpuMesh> pickup_mesh;
  std::shared_ptr<const CpuMesh> signal_mesh;
  std::shared_ptr<const CpuMesh> glow_mesh;
  std::shared_ptr<const CpuMesh> band_mesh;
  std::vector<Pickup> pickups;
  std::vector<PlacedMarker> markers;

  void reset() {
    rng.reset(tuning.seed);
    replay.clear();
    command_sequence = 0u;
    player = {6.2f, 1.92f};
    velocity = {};
    markers.clear();
    status = {};
    status.player = player;
    status.velocity = velocity;
    status.on_ground = false;
    status.facing = 1;
    status.selected_slot = TuzoraBuildSlot::WoodPlank;
    buildInitialTiles();
    pickups = {
        {"shells_at_warm_sand", PickupKind::Shells, {8.4f, 1.38f}, false},
        {"pier_plank_bundle", PickupKind::Wood, {16.6f, 4.36f}, false},
        {"old_roof_scrap", PickupKind::Scrap, {23.6f, 6.36f}, false},
        {"rolled_shade_cloth", PickupKind::ShadeCloth, {27.0f, 6.36f}, false},
        {"signal_parts_case", PickupKind::SignalPart, {30.2f, 6.36f}, false},
    };
    refreshStatus("Find supplies, build shade, repair the signal.");
    rebuildScene();
  }

  void buildInitialTiles() {
    tiles.clear();
    for (int x = -12; x <= 78; ++x) {
      tiles.set({x, 0}, "sand", TileLayerRole2D::Solid, true);
    }
    for (int x = 42; x <= 70; ++x) {
      tiles.set({x, 1}, "pier", TileLayerRole2D::Solid, true);
    }
    for (int x = 12; x <= 13; ++x) {
      tiles.set({x, 1}, "sand_step", TileLayerRole2D::Solid, true);
    }
    for (int x = 14; x <= 15; ++x) {
      tiles.set({x, 2}, "sand_step", TileLayerRole2D::Solid, true);
    }
    for (int x = 16; x <= 17; ++x) {
      tiles.set({x, 3}, "sand_step", TileLayerRole2D::Solid, true);
    }
    for (int x = 18; x <= 19; ++x) {
      tiles.set({x, 4}, "sand_step", TileLayerRole2D::Solid, true);
    }
    for (int x = 20; x <= 38; ++x) {
      tiles.set({x, 5}, "roof_tile", TileLayerRole2D::Solid, true);
    }
    tiles.set({28, 6}, "scrap_crate", TileLayerRole2D::Solid, true);
    tiles.rebuildAutotileMasks();
  }

  [[nodiscard]] TileAabb2D playerBoundsAt(const Vec2 position) const {
    return {.center = position, .half_extents = {kPlayerHalfWidth, kPlayerHalfHeight}};
  }

  [[nodiscard]] bool collidesAt(const Vec2 position) const {
    return tiles.intersectsSolid(playerBoundsAt(position));
  }

  [[nodiscard]] bool onGroundAt(const Vec2 position) const {
    return tiles.intersectsSolid({.center = {position.x, position.y - 0.04f},
                                  .half_extents = {kPlayerHalfWidth * 0.92f, kPlayerHalfHeight}});
  }

  void moveHorizontal(const float dx) {
    if (std::abs(dx) <= 0.0001f) {
      return;
    }
    Vec2 next{player.x + dx, player.y};
    if (!collidesAt(next)) {
      player = next;
      return;
    }
    for (const float step : {0.36f, 0.72f, 1.02f}) {
      next = {player.x + dx, player.y + step};
      if (!collidesAt(next)) {
        player = next;
        velocity.y = std::max(velocity.y, 0.0f);
        return;
      }
    }
    velocity.x = 0.0f;
  }

  void moveVertical(const float dy) {
    if (std::abs(dy) <= 0.0001f) {
      return;
    }
    Vec2 next{player.x, player.y + dy};
    if (!collidesAt(next)) {
      player = next;
      return;
    }
    const float step = dy > 0.0f ? 0.02f : -0.02f;
    while (std::abs(next.y - player.y) > std::abs(step)) {
      Vec2 candidate{player.x, player.y + step};
      if (collidesAt(candidate)) {
        break;
      }
      player = candidate;
      next = {player.x, next.y - step};
    }
    velocity.y = 0.0f;
  }

  [[nodiscard]] TileCoord2D facingTile() const {
    return tileCoordAt({player.x + static_cast<float>(status.facing) * 0.72f,
                        player.y - kPlayerHalfHeight * 0.30f});
  }

  [[nodiscard]] bool playerOverlapsTile(const TileCoord2D coord) const {
    const Vec2 center{static_cast<float>(coord.x) + 0.5f, static_cast<float>(coord.y) + 0.5f};
    return std::abs(center.x - player.x) < kPlayerHalfWidth + 0.50f &&
           std::abs(center.y - player.y) < kPlayerHalfHeight + 0.50f;
  }

  bool spendForSlot(const TuzoraBuildSlot slot) {
    switch (slot) {
    case TuzoraBuildSlot::SandStone:
      if (status.shells <= 0) {
        return false;
      }
      --status.shells;
      return true;
    case TuzoraBuildSlot::WoodPlank:
      if (status.wood <= 0) {
        return false;
      }
      --status.wood;
      return true;
    case TuzoraBuildSlot::ShadeCloth:
      if (status.shade_cloth <= 0) {
        return false;
      }
      --status.shade_cloth;
      return true;
    case TuzoraBuildSlot::Lamp:
      if (status.scrap <= 0) {
        return false;
      }
      --status.scrap;
      return true;
    case TuzoraBuildSlot::SignalPart:
      if (status.signal_parts <= 0) {
        return false;
      }
      --status.signal_parts;
      return true;
    }
    return false;
  }

  void refundForTile(const std::string &tile_id) {
    if (tile_id.find("wood") != std::string::npos || tile_id == "pier") {
      ++status.wood;
    } else if (tile_id.find("shade") != std::string::npos) {
      ++status.shade_cloth;
    } else if (tile_id.find("crate") != std::string::npos) {
      ++status.scrap;
    } else {
      ++status.shells;
    }
  }

  void collectPickup(Pickup &pickup) {
    pickup.collected = true;
    ++status.pickups_collected;
    switch (pickup.kind) {
    case PickupKind::Shells:
      status.shells += 3;
      status.event_line = "Pockets full of warm shells.";
      break;
    case PickupKind::Wood:
      status.wood += 3;
      status.event_line = "Sun-dried planks are usable.";
      break;
    case PickupKind::Scrap:
      status.scrap += 2;
      status.event_line = "Old metal can hold a lamp.";
      break;
    case PickupKind::ShadeCloth:
      status.shade_cloth += 2;
      status.event_line = "Shade cloth catches the sea breeze.";
      break;
    case PickupKind::SignalPart:
      status.signal_parts += 2;
      status.event_line = "Signal parts still have charge.";
      break;
    }
  }

  void interact() {
    for (Pickup &pickup : pickups) {
      if (!pickup.collected && planarDistance(player, pickup.position) < 1.10f) {
        collectPickup(pickup);
        return;
      }
    }
    if (planarDistance(player, kSignalPosition) < 1.65f) {
      if (status.signal_parts > 0 && status.signal_parts_installed < 2) {
        --status.signal_parts;
        ++status.signal_parts_installed;
        status.event_line = status.signal_parts_installed == 1 ? "Signal mast is upright."
                                                               : "Signal lens is aligned.";
        return;
      }
      if (status.signal_parts_installed >= 2 && status.lamps_placed > 0) {
        status.signal_lit = true;
        status.outcome = TuzoraOutcome::SignalLit;
        status.event_line = "Signal lit over the warm roofs.";
        return;
      }
      status.event_line = "The signal needs parts and a nearby lamp.";
      return;
    }
    status.event_line = "Sea wind, hot concrete, distant voices.";
  }

  void breakFacingTile() {
    const TileCoord2D target = facingTile();
    const TileCell2D *cell = tiles.cell(target);
    if (cell == nullptr || cell->tile_id == "roof_tile" || cell->tile_id == "sand_step") {
      status.event_line = "That piece is holding the place together.";
      return;
    }
    const std::string tile_id = cell->tile_id;
    if (tiles.erase(target)) {
      refundForTile(tile_id);
      ++status.blocks_broken;
      tiles.rebuildAutotileMasks();
      status.event_line = "Block loosened by hand.";
    }
  }

  void placeSelected() {
    const TuzoraBuildSlot slot = status.selected_slot;
    if (slot == TuzoraBuildSlot::Lamp) {
      if (!spendForSlot(slot)) {
        status.event_line = "Need scrap for a lamp.";
        return;
      }
      markers.push_back({"placed_lamp_" + std::to_string(markers.size()), player, slot});
      ++status.lamps_placed;
      status.event_line = "Lamp glow makes the roof feel occupied.";
      return;
    }
    if (slot == TuzoraBuildSlot::SignalPart) {
      if (planarDistance(player, kSignalPosition) >= 1.65f) {
        status.event_line = "Signal parts belong on the rooftop mast.";
        return;
      }
      interact();
      return;
    }

    const TileCoord2D target = facingTile();
    if (tiles.cell(target) != nullptr || playerOverlapsTile(target)) {
      status.event_line = "No room to place that here.";
      return;
    }
    if (!spendForSlot(slot)) {
      status.event_line = std::string("Need more ") + slotName(slot) + ".";
      return;
    }
    const char *tile_id =
        slot == TuzoraBuildSlot::WoodPlank
            ? "placed_wood"
            : (slot == TuzoraBuildSlot::ShadeCloth ? "placed_shade" : "placed_sand");
    const bool solid = slot != TuzoraBuildSlot::ShadeCloth;
    tiles.set(target, tile_id, solid ? TileLayerRole2D::Solid : TileLayerRole2D::Foreground, solid);
    tiles.rebuildAutotileMasks();
    ++status.blocks_placed;
    status.event_line = slot == TuzoraBuildSlot::ShadeCloth ? "Shade cloth snaps in the wind."
                                                            : "A hand-placed block holds.";
  }

  void updateFixed(SimCommand command) {
    command.tick = status.tick;
    command.sequence = command_sequence++;
    replay.record(command);
    status.event_line.clear();

    if (command.look >= 1 && command.look <= 5) {
      status.selected_slot = static_cast<TuzoraBuildSlot>(command.look - 1);
    }

    if (status.outcome == TuzoraOutcome::Playing) {
      const float axis = std::clamp(static_cast<float>(command.strafe) / 32767.0f, -1.0f, 1.0f);
      if (std::abs(axis) > 0.08f) {
        status.facing = axis < 0.0f ? -1 : 1;
      }
      status.on_ground = onGroundAt(player);
      if (command.pressed(SimCommandButton::Jump) && status.on_ground) {
        velocity.y = kJumpVelocity;
      }
      velocity.x = axis * (command.pressed(SimCommandButton::Run) ? kRunSpeed : kWalkSpeed);
      velocity.y = std::max(velocity.y + kGravityPerTick, -0.42f);
      moveHorizontal(velocity.x);
      moveVertical(velocity.y);
      status.on_ground = onGroundAt(player);
      if (status.on_ground && velocity.y < 0.0f) {
        velocity.y = 0.0f;
      }
      if (command.pressed(SimCommandButton::Interact)) {
        interact();
      }
      if (command.pressed(SimCommandButton::Primary)) {
        breakFacingTile();
      }
      if (command.pressed(SimCommandButton::Secondary)) {
        placeSelected();
      }
      if (!status.signal_lit && status.tick >= static_cast<std::uint32_t>(tuning.day_ticks)) {
        status.night_started = true;
        status.outcome = TuzoraOutcome::Nightfall;
        status.event_line = "Night arrives before the signal answers.";
      }
    }

    ++status.tick;
    refreshStatus(status.event_line);
    rebuildScene();
  }

  [[nodiscard]] ScriptGoal scriptGoal() const {
    if (status.pickups_collected < 1) {
      return {.target = {8.4f, 1.92f}, .interact = planarDistance(player, {8.4f, 1.92f}) < 0.72f};
    }
    if (status.pickups_collected < 2) {
      return {.target = {16.6f, 4.92f}, .interact = planarDistance(player, {16.6f, 4.92f}) < 0.82f};
    }
    if (status.pickups_collected < 3) {
      return {.target = {23.6f, 6.92f}, .interact = planarDistance(player, {23.6f, 6.92f}) < 0.82f};
    }
    if (status.pickups_collected < 4) {
      return {.target = {27.0f, 6.92f}, .interact = planarDistance(player, {27.0f, 6.92f}) < 0.82f};
    }
    if (status.pickups_collected < 5) {
      return {.target = {30.2f, 6.92f}, .interact = planarDistance(player, {30.2f, 6.92f}) < 0.82f};
    }
    if (status.blocks_broken == 0) {
      return {.target = {29.36f, 6.92f},
              .primary = planarDistance(player, {29.36f, 6.92f}) < 0.34f};
    }
    if (status.blocks_placed == 0) {
      return {.target = {31.70f, 6.92f},
              .secondary = planarDistance(player, {31.70f, 6.92f}) < 0.42f,
              .slot = TuzoraBuildSlot::WoodPlank};
    }
    if (status.signal_parts_installed < 2) {
      return {.target = kSignalPosition,
              .interact = planarDistance(player, kSignalPosition) < 1.10f,
              .slot = TuzoraBuildSlot::SignalPart};
    }
    if (status.lamps_placed == 0) {
      return {.target = {33.1f, 6.92f},
              .secondary = planarDistance(player, {33.1f, 6.92f}) < 0.55f,
              .slot = TuzoraBuildSlot::Lamp};
    }
    return {.target = kSignalPosition, .interact = planarDistance(player, kSignalPosition) < 1.10f};
  }

  [[nodiscard]] SimCommand scriptedCommand(std::uint32_t tick) const {
    SimCommand command;
    command.tick = tick;
    if (status.outcome != TuzoraOutcome::Playing) {
      return command;
    }
    const ScriptGoal goal = scriptGoal();
    const float dx = goal.target.x - player.x;
    const float dy = goal.target.y - player.y;
    if (std::abs(dx) > 0.14f) {
      command.strafe = static_cast<std::int16_t>((dx < 0.0f ? -1.0f : 1.0f) * 32767.0f);
      command.set(SimCommandButton::Run, true);
    }
    if (dy > 0.58f && (tick % 22u) == 0u) {
      command.set(SimCommandButton::Jump, true);
    }
    command.look = static_cast<std::int16_t>(slotIndex(goal.slot) + 1);
    command.set(SimCommandButton::Interact, goal.interact);
    command.set(SimCommandButton::Primary, goal.primary);
    command.set(SimCommandButton::Secondary, goal.secondary);
    return command;
  }

  void refreshStatus(const std::string &line) {
    status.player = player;
    status.velocity = velocity;
    status.day_fraction =
        tuning.day_ticks <= 0
            ? 1.0f
            : saturate(static_cast<float>(status.tick) / static_cast<float>(tuning.day_ticks));
    status.night_started = status.day_fraction >= 1.0f && !status.signal_lit;
    status.replay_checksum = replay.checksum();
    status.prompt_line = line;
    status.world_hash = computeWorldHash();
  }

  [[nodiscard]] std::uint64_t computeWorldHash() const {
    std::uint64_t hash = 0xA57E7A2026ull;
    hash = hashFloat(hash, player.x);
    hash = hashFloat(hash, player.y);
    hash = hashFloat(hash, velocity.x);
    hash = hashFloat(hash, velocity.y);
    hash = hashCombine64(hash, status.shells);
    hash = hashCombine64(hash, status.wood);
    hash = hashCombine64(hash, status.scrap);
    hash = hashCombine64(hash, status.shade_cloth);
    hash = hashCombine64(hash, status.signal_parts);
    hash = hashCombine64(hash, status.signal_parts_installed);
    hash = hashCombine64(hash, status.lamps_placed);
    hash = hashCombine64(hash, status.blocks_placed);
    hash = hashCombine64(hash, status.blocks_broken);
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.outcome));
    for (const TileCell2D &cell : tiles.cells()) {
      hash = hashCombine64(hash, static_cast<std::uint64_t>(cell.coord.x & 0xffff));
      hash = hashCombine64(hash, static_cast<std::uint64_t>(cell.coord.y & 0xffff));
      hash = hashText(hash, cell.tile_id);
    }
    for (const Pickup &pickup : pickups) {
      hash = hashCombine64(hash, pickup.collected ? 1u : 0u);
    }
    return hash;
  }

  void addObject(std::string name, std::shared_ptr<const CpuMesh> mesh, Material material,
                 const Vec2 position, const float depth, const Vec2 scale = {1.0f, 1.0f}) {
    scene.objects().push_back(makeSpriteObject2D(std::move(name), std::move(mesh),
                                                 std::move(material), position, depth, scale));
  }

  void addBand(const std::string &name, const Vec2 position, const Vec2 scale, const float depth,
               const Material &material) {
    addObject(name, band_mesh, material, position, depth, scale);
  }

  void addPixel(const std::string &name, const Vec2 position, const Vec2 scale, const float depth,
                const LinearRgb color, const EmissionColor emission = {},
                const float emission_strength = 0.0f, const float opacity = 1.0f) {
    addBand(name, position, scale, depth,
            flatMaterial("material." + name, color, emission, emission_strength, opacity));
  }

  void addBackdrop() {
    const float dusk = saturate((status.day_fraction - 0.62f) * 2.8f);
    const float night = saturate((status.day_fraction - 0.82f) * 5.0f);
    const Vec2 camera = cameraTarget();
    const LinearRgb sky_color{0.34f - night * 0.22f, 0.65f - night * 0.36f, 0.92f - night * 0.52f};
    addBand("Tuzora late summer sky gradient", {camera.x, 8.9f}, {92.0f, 26.0f}, -14.0f,
            flatMaterial("material.tuzora.sky", sky_color, {0.85f, 0.42f, 0.18f},
                         0.08f + dusk * 0.20f));
    addBand("Tuzora peach horizon glow", {camera.x, 4.95f}, {92.0f, 4.4f}, -13.9f,
            flatMaterial("material.tuzora.horizon_glow", {0.96f, 0.58f, 0.24f},
                         {0.92f, 0.28f, 0.08f}, 0.22f, 0.74f));
    addBand("Tuzora sunset disc", {camera.x + 8.0f, 10.2f - dusk * 1.8f}, {4.0f, 4.0f}, -13.6f,
            flatMaterial("material.tuzora.sun", {1.0f, 0.62f, 0.24f}, {1.0f, 0.38f, 0.12f}, 0.68f));
    addBand("Tuzora sun hot center", {camera.x + 8.0f, 10.2f - dusk * 1.8f}, {1.8f, 1.8f}, -13.55f,
            flatMaterial("material.tuzora.sun_center", {1.0f, 0.82f, 0.35f}, {1.0f, 0.55f, 0.16f},
                         1.05f));
    const ParallaxLayer2D hills{.name = "hills",
                                .base_position = {0.0f, 4.2f},
                                .scroll_scale = {0.18f, 0.02f},
                                .depth = -13.0f};
    addBand(
        "Tuzora distant Taurus hills parallax", parallaxPosition2D(hills, camera), {82.0f, 5.2f},
        hills.depth,
        flatMaterial("material.tuzora.hills", {0.16f, 0.29f, 0.34f}, {0.05f, 0.08f, 0.06f}, 0.12f));
    const ParallaxLayer2D sea{.name = "sea",
                              .base_position = {0.0f, 1.95f},
                              .scroll_scale = {0.36f, 0.0f},
                              .depth = -12.5f};
    addBand("Tuzora sea parallax band", parallaxPosition2D(sea, camera), {96.0f, 3.2f}, sea.depth,
            flatMaterial("material.tuzora.sea", {0.04f, 0.58f - night * 0.20f, 0.72f},
                         {0.06f, 0.44f, 0.56f}, 0.22f + dusk * 0.10f));
    addBand("Tuzora apartment sunwashed blocks", {24.5f, 4.0f}, {13.4f, 10.0f}, -8.5f,
            flatMaterial("material.tuzora.apartment", {0.92f, 0.67f, 0.36f}, {0.36f, 0.14f, 0.04f},
                         0.10f));
    addBand("Tuzora apartment cool shadow side", {30.7f, 4.0f}, {1.0f, 10.0f}, -8.3f,
            flatMaterial("material.tuzora.apartment_shadow", {0.58f, 0.46f, 0.35f},
                         {0.04f, 0.02f, 0.01f}, 0.04f));
    addBand("Tuzora roof water tank", {28.7f, 8.95f}, {1.18f, 0.84f}, -7.7f,
            flatMaterial("material.tuzora.water_tank", {0.18f, 0.26f, 0.28f}, {0.04f, 0.08f, 0.08f},
                         0.06f));
    addBand("Tuzora balcony silhouettes with friends", {22.0f, 7.1f}, {7.2f, 0.42f}, -7.9f,
            flatMaterial("material.tuzora.balcony", {0.08f, 0.09f, 0.10f}, {0.02f, 0.02f, 0.02f},
                         0.05f));
    addBand("Tuzora late-summer haze veil", {camera.x, 5.8f}, {90.0f, 12.0f}, -6.2f,
            flatMaterial("material.tuzora.haze", {0.98f, 0.62f, 0.28f}, {0.56f, 0.22f, 0.05f},
                         0.10f, 0.10f + dusk * 0.12f));
  }

  void addAtmosphereDetails() {
    const Vec2 camera = cameraTarget();
    for (int i = 0; i < 6; ++i) {
      const float x = camera.x - 27.0f + static_cast<float>(i) * 9.4f;
      const float y = 7.1f + static_cast<float>(i % 3) * 0.55f;
      addPixel("Tuzora tiny heat cloud " + std::to_string(i), {x, y}, {3.2f, 0.34f}, -13.2f,
               {1.0f, 0.72f, 0.45f}, {0.35f, 0.12f, 0.04f}, 0.08f, 0.38f);
      addPixel("Tuzora tiny heat cloud cap " + std::to_string(i), {x + 0.82f, y + 0.22f},
               {1.45f, 0.22f}, -13.15f, {1.0f, 0.80f, 0.56f}, {0.28f, 0.10f, 0.03f}, 0.06f, 0.34f);
    }

    for (int i = 0; i < 9; ++i) {
      const float x = camera.x - 23.0f + static_cast<float>(i) * 5.7f;
      const float y = 2.62f + static_cast<float>(i % 3) * 0.28f;
      addBand("Tuzora sea shimmer " + std::to_string(i), {x, y}, {2.8f, 0.065f}, -5.9f,
              flatMaterial("material.tuzora.sea_shimmer", {0.66f, 0.88f, 0.82f},
                           {0.36f, 0.54f, 0.42f}, 0.22f, 0.52f));
      addPixel("Tuzora sea dark ripple " + std::to_string(i), {x + 1.7f, y - 0.22f}, {1.4f, 0.055f},
               -6.0f, {0.02f, 0.32f, 0.44f}, {0.01f, 0.12f, 0.16f}, 0.06f, 0.46f);
    }

    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 5; ++column) {
        const float warm = static_cast<float>((row + column) % 2) * 0.10f;
        addBand(
            "Tuzora apartment window " + std::to_string(row) + "-" + std::to_string(column),
            {20.4f + static_cast<float>(column) * 2.1f, 3.35f + static_cast<float>(row) * 1.55f},
            {0.62f, 0.52f}, -7.6f,
            flatMaterial("material.tuzora.window", {0.18f, 0.20f + warm, 0.22f + warm},
                         {0.42f, 0.24f + warm, 0.12f}, 0.12f + warm));
        addPixel(
            "Tuzora apartment curtain " + std::to_string(row) + "-" + std::to_string(column),
            {20.18f + static_cast<float>(column) * 2.1f, 3.35f + static_cast<float>(row) * 1.55f},
            {0.08f, 0.50f}, -7.5f, {0.88f, 0.45f, 0.22f}, {0.22f, 0.08f, 0.02f}, 0.05f);
      }
    }

    for (int i = 0; i < 4; ++i) {
      const float x = 19.4f + static_cast<float>(i) * 1.4f;
      addBand("Tuzora friend silhouette " + std::to_string(i), {x, 7.45f}, {0.22f, 0.74f}, -7.4f,
              flatMaterial("material.tuzora.friend_silhouette", {0.045f, 0.050f, 0.055f},
                           {0.02f, 0.02f, 0.025f}, 0.04f));
      addBand("Tuzora friend head " + std::to_string(i), {x, 7.95f}, {0.28f, 0.24f}, -7.35f,
              flatMaterial("material.tuzora.friend_head", {0.035f, 0.040f, 0.045f},
                           {0.02f, 0.02f, 0.025f}, 0.04f));
    }

    for (const float x : {5.1f, 10.7f, 39.6f}) {
      addBand("Tuzora citrus trunk " + std::to_string(static_cast<int>(x * 10.0f)), {x, 1.82f},
              {0.28f, 1.62f}, -2.6f,
              flatMaterial("material.tuzora.citrus_trunk", {0.36f, 0.19f, 0.08f},
                           {0.08f, 0.03f, 0.01f}, 0.05f));
      addPixel("Tuzora citrus leaf left " + std::to_string(static_cast<int>(x * 10.0f)),
               {x - 0.48f, 2.85f}, {1.18f, 0.78f}, -2.50f, {0.08f, 0.40f, 0.22f},
               {0.02f, 0.10f, 0.04f}, 0.08f);
      addPixel("Tuzora citrus leaf right " + std::to_string(static_cast<int>(x * 10.0f)),
               {x + 0.50f, 2.90f}, {1.18f, 0.78f}, -2.49f, {0.10f, 0.48f, 0.26f},
               {0.02f, 0.12f, 0.04f}, 0.08f);
      addPixel("Tuzora citrus leaf crown " + std::to_string(static_cast<int>(x * 10.0f)),
               {x + 0.05f, 3.42f}, {1.22f, 0.72f}, -2.45f, {0.13f, 0.56f, 0.30f},
               {0.02f, 0.12f, 0.04f}, 0.08f);
      addPixel("Tuzora citrus leaf shadow " + std::to_string(static_cast<int>(x * 10.0f)),
               {x + 0.12f, 2.42f}, {1.42f, 0.30f}, -2.43f, {0.05f, 0.27f, 0.18f},
               {0.0f, 0.04f, 0.02f}, 0.04f);
      addBand("Tuzora citrus fruit " + std::to_string(static_cast<int>(x * 10.0f)),
              {x - 0.36f, 2.96f}, {0.18f, 0.18f}, -2.35f,
              flatMaterial("material.tuzora.citrus_fruit", {0.94f, 0.54f, 0.12f},
                           {0.48f, 0.20f, 0.02f}, 0.14f));
      addPixel("Tuzora citrus fruit two " + std::to_string(static_cast<int>(x * 10.0f)),
               {x + 0.44f, 3.13f}, {0.14f, 0.14f}, -2.34f, {1.0f, 0.62f, 0.12f},
               {0.50f, 0.20f, 0.02f}, 0.12f);
    }

    for (int i = 0; i < 7; ++i) {
      addBand("Tuzora rooftop rail " + std::to_string(i),
              {21.0f + static_cast<float>(i) * 2.5f, 6.16f}, {1.6f, 0.10f}, -2.15f,
              flatMaterial("material.tuzora.roof_rail", {0.30f, 0.24f, 0.18f},
                           {0.10f, 0.04f, 0.01f}, 0.05f));
    }
    addPixel("Tuzora rooftop shade awning", {26.6f, 6.62f}, {2.8f, 0.24f}, -2.08f,
             {0.08f, 0.48f, 0.58f}, {0.02f, 0.12f, 0.12f}, 0.10f);
    addPixel("Tuzora rooftop shade rope", {25.10f, 6.36f}, {0.08f, 0.52f}, -2.06f,
             {0.85f, 0.72f, 0.48f}, {0.12f, 0.08f, 0.02f}, 0.04f);
    addPixel("Tuzora rooftop shade rope far", {28.10f, 6.36f}, {0.08f, 0.52f}, -2.06f,
             {0.85f, 0.72f, 0.48f}, {0.12f, 0.08f, 0.02f}, 0.04f);
  }

  void addTiles() {
    for (const TileCell2D &cell : tiles.cells()) {
      const Vec2 position{static_cast<float>(cell.coord.x) + 0.5f,
                          static_cast<float>(cell.coord.y) + 0.5f};
      std::string name =
          cell.tile_id.find("placed") == 0u ? "Tuzora placed block " : "Tuzora terrain tile ";
      name +=
          cell.tile_id + " " + std::to_string(cell.coord.x) + "," + std::to_string(cell.coord.y);
      addObject(name, tile_mesh,
                tileMaterial(cell.tile_id, cell.autotile_mask, status.day_fraction), position,
                cell.layer == TileLayerRole2D::Foreground ? -2.2f : -3.0f);
      const bool exposed_top = !tiles.solidAt({cell.coord.x, cell.coord.y + 1});
      if (exposed_top) {
        const bool wood = cell.tile_id.find("wood") != std::string::npos || cell.tile_id == "pier";
        const bool shade = cell.tile_id.find("shade") != std::string::npos;
        const LinearRgb cap_color = wood    ? LinearRgb{0.78f, 0.48f, 0.22f}
                                    : shade ? LinearRgb{0.16f, 0.68f, 0.76f}
                                            : LinearRgb{0.96f, 0.78f, 0.45f};
        addPixel("Tuzora tile sun cap " + std::to_string(cell.coord.x) + "," +
                     std::to_string(cell.coord.y),
                 {position.x, position.y + 0.43f}, {0.92f, 0.10f},
                 cell.layer == TileLayerRole2D::Foreground ? -2.12f : -2.86f, cap_color,
                 {0.20f, 0.08f, 0.02f}, 0.05f);
      }
      if (((cell.coord.x * 31 + cell.coord.y * 17) & 3) == 0 &&
          cell.tile_id.find("sand") != std::string::npos) {
        addPixel("Tuzora sand grain " + std::to_string(cell.coord.x) + "," +
                     std::to_string(cell.coord.y),
                 {position.x - 0.22f, position.y + 0.04f}, {0.10f, 0.06f}, -2.82f,
                 {0.56f, 0.40f, 0.20f}, {}, 0.0f, 0.58f);
      }
      if (cell.tile_id.find("wood") != std::string::npos || cell.tile_id == "pier") {
        addPixel("Tuzora plank groove " + std::to_string(cell.coord.x) + "," +
                     std::to_string(cell.coord.y),
                 {position.x, position.y - 0.07f}, {0.84f, 0.05f}, -2.84f, {0.33f, 0.18f, 0.08f},
                 {}, 0.0f, 0.62f);
      }
      if (tuning.debug_tiles) {
        addObject("Debug Tuzora tile grid " + std::to_string(cell.coord.x) + "," +
                      std::to_string(cell.coord.y),
                  tile_mesh,
                  flatMaterial("material.tuzora.debug", {0.1f, 0.9f, 0.8f}, {}, 0.0f, 0.18f),
                  position, -1.6f, {1.04f, 1.04f});
      }
    }
  }

  void addPickups() {
    for (const Pickup &pickup : pickups) {
      if (pickup.collected) {
        continue;
      }
      LinearRgb color{0.92f, 0.80f, 0.52f};
      EmissionColor emission{0.08f, 0.05f, 0.02f};
      if (pickup.kind == PickupKind::Wood) {
        color = {0.58f, 0.34f, 0.16f};
      } else if (pickup.kind == PickupKind::Scrap) {
        color = {0.46f, 0.48f, 0.50f};
      } else if (pickup.kind == PickupKind::ShadeCloth) {
        color = {0.16f, 0.62f, 0.72f};
      } else if (pickup.kind == PickupKind::SignalPart) {
        color = {0.90f, 0.44f, 0.16f};
        emission = {0.60f, 0.18f, 0.04f};
      }
      addObject("Tuzora collectible " + pickup.id, pickup_mesh,
                flatMaterial("material.tuzora.pickup", color, emission, 0.20f), pickup.position,
                -1.1f);
      if (pickup.kind == PickupKind::Shells) {
        addPixel("Tuzora shell glint " + pickup.id,
                 {pickup.position.x + 0.10f, pickup.position.y + 0.08f}, {0.13f, 0.06f}, -0.92f,
                 {1.0f, 0.92f, 0.70f}, {0.30f, 0.18f, 0.06f}, 0.18f);
        addPixel("Tuzora shell shadow " + pickup.id,
                 {pickup.position.x - 0.08f, pickup.position.y - 0.10f}, {0.22f, 0.05f}, -0.91f,
                 {0.44f, 0.30f, 0.16f}, {}, 0.0f, 0.48f);
      } else if (pickup.kind == PickupKind::Wood) {
        addPixel("Tuzora plank pickup top " + pickup.id,
                 {pickup.position.x, pickup.position.y + 0.12f}, {0.54f, 0.08f}, -0.92f,
                 {0.82f, 0.48f, 0.20f}, {0.20f, 0.08f, 0.02f}, 0.08f);
        addPixel("Tuzora plank pickup lower " + pickup.id,
                 {pickup.position.x + 0.06f, pickup.position.y - 0.06f}, {0.50f, 0.07f}, -0.91f,
                 {0.38f, 0.20f, 0.08f});
      } else if (pickup.kind == PickupKind::Scrap) {
        addPixel("Tuzora scrap sharp highlight " + pickup.id,
                 {pickup.position.x + 0.08f, pickup.position.y + 0.10f}, {0.20f, 0.06f}, -0.92f,
                 {0.82f, 0.86f, 0.80f}, {0.18f, 0.18f, 0.14f}, 0.08f);
      } else if (pickup.kind == PickupKind::ShadeCloth) {
        addPixel("Tuzora rolled cloth stripe " + pickup.id,
                 {pickup.position.x - 0.04f, pickup.position.y + 0.08f}, {0.48f, 0.08f}, -0.92f,
                 {0.05f, 0.30f, 0.38f}, {0.0f, 0.08f, 0.10f}, 0.06f);
      } else if (pickup.kind == PickupKind::SignalPart) {
        addPixel("Tuzora signal part ember " + pickup.id,
                 {pickup.position.x + 0.09f, pickup.position.y + 0.08f}, {0.16f, 0.16f}, -0.92f,
                 {1.0f, 0.62f, 0.16f}, {1.0f, 0.32f, 0.05f}, 0.46f);
      }
    }
  }

  void addSignalAndMarkers() {
    addObject(status.signal_lit ? "Tuzora rooftop signal lit" : "Tuzora rooftop signal",
              signal_mesh,
              status.signal_lit ? flatMaterial("material.tuzora.signal_lit", {0.98f, 0.72f, 0.28f},
                                               {1.0f, 0.54f, 0.12f}, 1.6f)
                                : flatMaterial("material.tuzora.signal", {0.52f, 0.42f, 0.36f},
                                               {0.12f, 0.04f, 0.02f}, 0.10f),
              kSignalPosition, -0.9f, {0.34f, 0.92f});
    addPixel("Tuzora signal mast dark line", {kSignalPosition.x, kSignalPosition.y - 0.08f},
             {0.10f, 1.78f}, -0.72f, {0.14f, 0.11f, 0.10f}, {0.04f, 0.02f, 0.01f}, 0.06f);
    addPixel("Tuzora signal dish left", {kSignalPosition.x - 0.32f, kSignalPosition.y + 0.42f},
             {0.46f, 0.20f}, -0.70f, {0.72f, 0.56f, 0.38f}, {0.18f, 0.08f, 0.02f}, 0.10f);
    addPixel("Tuzora signal lens", {kSignalPosition.x + 0.18f, kSignalPosition.y + 0.48f},
             {0.18f, 0.18f}, -0.66f,
             status.signal_lit ? LinearRgb{1.0f, 0.72f, 0.20f} : LinearRgb{0.46f, 0.32f, 0.18f},
             status.signal_lit ? EmissionColor{1.0f, 0.42f, 0.08f}
                               : EmissionColor{0.12f, 0.04f, 0.02f},
             status.signal_lit ? 1.2f : 0.12f);
    if (status.signal_lit) {
      addObject("Tuzora signal light bloom", glow_mesh,
                flatMaterial("material.tuzora.signal_glow", {1.0f, 0.62f, 0.18f},
                             {1.0f, 0.52f, 0.10f}, 2.4f, 0.66f),
                {kSignalPosition.x + 0.10f, kSignalPosition.y + 0.30f}, -0.55f);
    }
    for (const PlacedMarker &marker : markers) {
      if (marker.slot == TuzoraBuildSlot::Lamp) {
        addObject("Tuzora lamp glow placed", glow_mesh,
                  flatMaterial("material.tuzora.lamp_glow", {1.0f, 0.60f, 0.18f},
                               {1.0f, 0.44f, 0.10f}, 1.2f, 0.48f),
                  {marker.position.x, marker.position.y + 0.15f}, -0.75f);
        addPixel("Tuzora lamp pole placed", {marker.position.x, marker.position.y - 0.18f},
                 {0.10f, 0.62f}, -0.62f, {0.22f, 0.14f, 0.08f}, {0.08f, 0.03f, 0.01f}, 0.08f);
        addPixel("Tuzora lamp bulb placed", {marker.position.x, marker.position.y + 0.18f},
                 {0.20f, 0.18f}, -0.58f, {1.0f, 0.68f, 0.20f}, {1.0f, 0.42f, 0.08f}, 0.82f);
      }
    }
  }

  void addPlayer() {
    const float face = static_cast<float>(status.facing);
    addPixel("Tuzora player shadow", {player.x, player.y - 0.78f}, {0.72f, 0.11f}, -0.18f,
             {0.02f, 0.025f, 0.03f}, {}, 0.0f, 0.34f);
    addPixel("Tuzora player legs", {player.x - 0.10f, player.y - 0.42f}, {0.16f, 0.62f}, -0.04f,
             {0.08f, 0.12f, 0.17f}, {0.02f, 0.06f, 0.10f}, 0.08f);
    addPixel("Tuzora player far leg", {player.x + 0.16f, player.y - 0.43f}, {0.15f, 0.58f}, -0.05f,
             {0.06f, 0.10f, 0.14f}, {0.01f, 0.04f, 0.08f}, 0.05f);
    addPixel("Tuzora player shorts", {player.x + 0.02f, player.y - 0.12f}, {0.50f, 0.30f}, -0.03f,
             {0.06f, 0.20f, 0.30f}, {0.02f, 0.08f, 0.12f}, 0.10f);
    addPixel("Tuzora player shirt", {player.x, player.y + 0.24f}, {0.56f, 0.62f}, -0.02f,
             {0.05f, 0.58f, 0.74f}, {0.02f, 0.18f, 0.22f}, 0.22f);
    addPixel("Tuzora player shirt sun edge", {player.x - face * 0.20f, player.y + 0.30f},
             {0.08f, 0.52f}, -0.01f, {0.48f, 0.94f, 0.96f}, {0.08f, 0.22f, 0.22f}, 0.14f);
    addPixel("Tuzora player arm", {player.x + face * 0.36f, player.y + 0.16f}, {0.13f, 0.48f},
             -0.01f, {0.16f, 0.11f, 0.08f}, {0.04f, 0.02f, 0.01f}, 0.05f);
    addPixel("Tuzora player head", {player.x + face * 0.04f, player.y + 0.83f}, {0.38f, 0.40f},
             0.0f, {0.38f, 0.24f, 0.16f}, {0.08f, 0.04f, 0.02f}, 0.07f);
    addPixel("Tuzora player hair", {player.x + face * 0.02f, player.y + 1.06f}, {0.45f, 0.18f},
             0.01f, {0.045f, 0.035f, 0.030f}, {0.01f, 0.01f, 0.01f}, 0.03f);
    addPixel("Tuzora player face pixel", {player.x + face * 0.18f, player.y + 0.86f},
             {0.07f, 0.07f}, 0.02f, {0.92f, 0.68f, 0.42f}, {0.18f, 0.08f, 0.03f}, 0.08f);
  }

  void rebuildScene() {
    scene.objects().clear();
    scene.reflectionProbes().clear();
    addBackdrop();
    addAtmosphereDetails();
    addTiles();
    addPickups();
    addSignalAndMarkers();
    addPlayer();
    scene.reflectionProbes().push_back({.name = "Tuzora warm sea probe",
                                        .position = {player.x, -2.0f, player.y},
                                        .influence_radius = 18.0f,
                                        .sky_irradiance = {0.72f, 0.58f, 0.40f},
                                        .ground_irradiance = {0.26f, 0.22f, 0.18f},
                                        .specular_tint = {1.0f, 0.78f, 0.54f},
                                        .intensity = 0.42f});
  }

  [[nodiscard]] Vec2 cameraTarget() const {
    return {std::clamp(player.x + static_cast<float>(status.facing) * 2.2f, 8.0f, 44.0f),
            std::clamp(player.y + 1.0f, 3.2f, 8.8f)};
  }

  [[nodiscard]] TuzoraHudModel hudModel() const {
    TuzoraHudModel model;
    model.title = "TUZORA";
    model.objective = status.signal_lit ? "Signal is alive above the roofs."
                                        : "Light the rooftop signal before night.";
    model.status_line = status.signal_lit                            ? "SIGNAL LIT"
                        : status.outcome == TuzoraOutcome::Nightfall ? "NIGHT ARRIVED"
                                                                     : "SUNSET WINDOW OPEN";
    std::ostringstream resources;
    resources << "shell " << status.shells << "  wood " << status.wood << "  scrap " << status.scrap
              << "  cloth " << status.shade_cloth << "  signal " << status.signal_parts;
    model.resource_line = resources.str();
    std::ostringstream hotbar;
    for (int i = 0; i < 5; ++i) {
      const auto slot = static_cast<TuzoraBuildSlot>(i);
      if (slot == status.selected_slot) {
        hotbar << "[";
      }
      hotbar << (i + 1) << ":" << slotName(slot);
      if (slot == status.selected_slot) {
        hotbar << "]";
      }
      if (i < 4) {
        hotbar << "  ";
      }
    }
    model.hotbar_line = hotbar.str();
    model.prompt_line = status.prompt_line;
    model.event_line = status.event_line.empty() ? "Warm concrete, sea static, friends upstairs."
                                                 : status.event_line;
    model.day_fraction = status.day_fraction;
    model.ended = status.outcome != TuzoraOutcome::Playing;
    model.signal_lit = status.signal_lit;
    return model;
  }
};

Tuzora::Tuzora(TuzoraTuning tuning) : impl_(std::make_unique<Impl>(std::move(tuning))) {}
Tuzora::~Tuzora() = default;
Tuzora::Tuzora(Tuzora &&) noexcept = default;
Tuzora &Tuzora::operator=(Tuzora &&) noexcept = default;

void Tuzora::reset() {
  impl_->reset();
}

void Tuzora::updateFixed(SimCommand command) {
  impl_->updateFixed(command);
}

SimCommand Tuzora::scriptedCommand(const std::uint32_t tick) const {
  return impl_->scriptedCommand(tick);
}

const Scene &Tuzora::scene() const {
  return impl_->scene;
}

const TuzoraStatus &Tuzora::status() const {
  return impl_->status;
}

const CommandReplay &Tuzora::replay() const {
  return impl_->replay;
}

TuzoraHudModel Tuzora::hudModel() const {
  return impl_->hudModel();
}

Vec2 Tuzora::cameraTarget() const {
  return impl_->cameraTarget();
}

std::uint64_t Tuzora::worldHash() const {
  return impl_->status.world_hash;
}

} // namespace aster
