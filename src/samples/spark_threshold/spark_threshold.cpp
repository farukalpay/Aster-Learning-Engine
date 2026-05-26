// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/spark_threshold/spark_threshold.hpp"

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
#include <string>
#include <utility>
#include <vector>

namespace aster {
namespace {

constexpr float kFixedStepSeconds = 1.0f / 60.0f;

struct SparkNodeState {
  SparkThresholdNode kind = SparkThresholdNode::Mine;
  Vec3 position{};
  const char *name = "";
  const char *prompt = "";
  const char *route_label = "";
  int overload_cost = 1;
  bool active = false;
};

struct SparkScriptPlan {
  std::vector<SparkThresholdNode> nodes;
  bool exit_after_nodes = true;
};

[[nodiscard]] SparkScriptPlan scriptPlan(const SparkThresholdScriptRoute route) {
  switch (route) {
  case SparkThresholdScriptRoute::Resonance:
    return {{{SparkThresholdNode::Mine, SparkThresholdNode::Castle, SparkThresholdNode::Scrap}},
            true};
  case SparkThresholdScriptRoute::Escape:
    return {{{SparkThresholdNode::Mine, SparkThresholdNode::Castle}}, true};
  case SparkThresholdScriptRoute::Overload:
    return {{{SparkThresholdNode::Scrap, SparkThresholdNode::Mine, SparkThresholdNode::Castle}},
            true};
  }
  return {};
}

[[nodiscard]] const char *nodeName(const SparkThresholdNode node) {
  switch (node) {
  case SparkThresholdNode::Mine:
    return "Maden Rezonatoru";
  case SparkThresholdNode::Scrap:
    return "Hurda Dinamosu";
  case SparkThresholdNode::Castle:
    return "Kale Prizmasi";
  }
  return "";
}

[[nodiscard]] Material sparkMaterial(const std::string &asset_id, const LinearRgb base,
                                     const EmissionColor emission,
                                     const float emission_strength,
                                     const MaterialSurfaceProfile profile,
                                     const SurfacePattern pattern) {
  MaterialDesc desc;
  desc.base_color = base;
  desc.emission_color = emission;
  desc.emission_strength = emission_strength;
  desc.roughness = profile == MaterialSurfaceProfile::EmissiveLens ? 0.24f : 0.74f;
  desc.metallic = profile == MaterialSurfaceProfile::CorrodedMetal ? 0.38f : 0.04f;
  desc.surface_profile = profile;
  desc.surface_pattern = pattern;
  desc.detail_strength = 0.48f;
  desc.detail_scale = 5.2f;
  desc.pattern_scale = {4.6f, 3.8f};
  desc.pattern_depth = 0.18f;
  desc.pattern_contrast = 0.42f;
  desc.edge_wear = 0.24f;
  desc.ambient_occlusion = 0.86f;
  desc.procedural.macro_variation = 0.42f;
  desc.procedural.micro_normal_strength = 0.46f;
  desc.procedural.roughness_variation = 0.28f;
  desc.procedural.physical_texel_density = 920.0f;
  desc.procedural.height_normal_coupling = 0.88f;
  desc.procedural.roughness_height_coupling = 0.72f;
  desc.procedural.macro_frequency_breakup = 0.58f;
  desc.procedural.micro_frequency_breakup = 0.78f;
  desc.procedural.height_shading = 0.30f;
  desc.procedural.pitting_density = profile == MaterialSurfaceProfile::CorrodedMetal ? 0.60f : 0.22f;
  desc.procedural.pitting_depth = 0.012f;
  desc.procedural.oxide_layering = profile == MaterialSurfaceProfile::CorrodedMetal ? 0.52f : 0.10f;
  desc.procedural.cavity_grime = 0.34f;
  desc.procedural.edge_polish = 0.26f;
  desc.procedural.wetness = pattern == SurfacePattern::CaveRock ? 0.34f : 0.04f;
  desc.receives_shadows = true;
  Material material = makeMaterial(desc);
  material.asset_id = asset_id;
  return material;
}

[[nodiscard]] Material translucentSparkMaterial(const std::string &asset_id,
                                                const LinearRgb base,
                                                const EmissionColor emission,
                                                const float emission_strength,
                                                const float opacity) {
  Material material = sparkMaterial(asset_id, base, emission, emission_strength,
                                    MaterialSurfaceProfile::EmissiveLens, SurfacePattern::None);
  material.opacity = opacity;
  material.double_sided = true;
  material.cull_mode = FaceCullMode::None;
  material.alpha_mode = MaterialAlphaMode::Blend;
  material.depth_write = MaterialDepthWrite::Disabled;
  material.depth_policy.layer = RenderDepthLayer::SurfaceAttachment;
  material.depth_policy.constant_bias = -0.004f;
  material.receives_shadows = false;
  return material;
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedMesh(CpuMesh mesh) {
  return std::make_shared<const CpuMesh>(std::move(mesh));
}

[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedRing(const float radius,
                                                            const float band_width,
                                                            const int segments = 72) {
  EnergyConduitRingSpec spec;
  spec.radius = radius;
  spec.band_width = band_width;
  spec.segments = segments;
  return makeSharedMesh(makeEnergyConduitRingMesh(spec));
}

[[nodiscard]] CpuMesh makeOperatorMesh() {
  CpuMesh mesh;
  mesh.vertices = {
      {{0.00f, 0.0f, 0.58f}, {0.0f, 1.0f, 0.0f}, {0.50f, 1.00f}},
      {{-0.30f, 0.0f, 0.22f}, {0.0f, 1.0f, 0.0f}, {0.20f, 0.68f}},
      {{-0.46f, 0.0f, -0.24f}, {0.0f, 1.0f, 0.0f}, {0.06f, 0.26f}},
      {{0.00f, 0.0f, -0.52f}, {0.0f, 1.0f, 0.0f}, {0.50f, 0.02f}},
      {{0.46f, 0.0f, -0.24f}, {0.0f, 1.0f, 0.0f}, {0.94f, 0.26f}},
      {{0.30f, 0.0f, 0.22f}, {0.0f, 1.0f, 0.0f}, {0.80f, 0.68f}},
      {{0.00f, 0.0f, 0.05f}, {0.0f, 1.0f, 0.0f}, {0.50f, 0.52f}},
  };
  mesh.indices = {6u, 0u, 1u, 6u, 1u, 2u, 6u, 2u, 3u,
                  6u, 3u, 4u, 6u, 4u, 5u, 6u, 5u, 0u};
  return mesh;
}

[[nodiscard]] CpuMesh makeShardMesh() {
  CpuMesh mesh;
  mesh.vertices = {
      {{0.0f, 0.46f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}},
      {{-0.32f, 0.0f, -0.22f}, {-0.4f, 0.4f, -0.4f}, {0.0f, 0.0f}},
      {{0.32f, 0.0f, -0.22f}, {0.4f, 0.4f, -0.4f}, {1.0f, 0.0f}},
      {{0.0f, 0.0f, 0.36f}, {0.0f, 0.4f, 0.6f}, {0.5f, 0.25f}},
      {{0.0f, -0.38f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.0f}},
  };
  mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u, 0u, 3u, 1u,
                  4u, 2u, 1u, 4u, 3u, 2u, 4u, 1u, 3u};
  return mesh;
}

[[nodiscard]] CpuMesh makeVerticalRingMesh(const float radius, const float band_width,
                                           const int segments) {
  CpuMesh mesh;
  const float inner = std::max(radius - band_width * 0.5f, 0.01f);
  const float outer = radius + band_width * 0.5f;
  for (int i = 0; i < segments; ++i) {
    const float a = static_cast<float>(i) / static_cast<float>(segments) * 6.28318530718f;
    const float c = std::cos(a);
    const float s = std::sin(a);
    mesh.vertices.push_back({{c * outer, s * outer, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
    mesh.vertices.push_back({{c * inner, s * inner, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
  }
  for (int i = 0; i < segments; ++i) {
    const std::uint32_t a = static_cast<std::uint32_t>(i * 2);
    const std::uint32_t b = static_cast<std::uint32_t>(((i + 1) % segments) * 2);
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(a + 1u);
    mesh.indices.push_back(a + 1u);
    mesh.indices.push_back(b);
    mesh.indices.push_back(b + 1u);
  }
  return mesh;
}

[[nodiscard]] float planarDistance(const Vec3 lhs, const Vec3 rhs) {
  const float dx = lhs.x - rhs.x;
  const float dz = lhs.z - rhs.z;
  return std::sqrt(dx * dx + dz * dz);
}

[[nodiscard]] Vec2 planarDirectionTo(const Vec3 from, const Vec3 to) {
  Vec2 axis{to.x - from.x, to.z - from.z};
  const float len = length(axis);
  if (len > 0.001f) {
    axis = axis / len;
  }
  return axis;
}

} // namespace

struct SparkThreshold::Impl {
  explicit Impl(SparkThresholdTuning tuning_in)
      : tuning(std::move(tuning_in)),
        ring_mesh(makeSharedRing(0.54f, 0.070f, 80)),
        gate_ring_mesh(makeSharedRing(0.96f, 0.095f, 96)),
        vertical_gate_ring_mesh(makeSharedMesh(makeVerticalRingMesh(1.18f, 0.13f, 112))),
        operator_mesh(makeSharedMesh(makeOperatorMesh())),
        shard_mesh(makeSharedMesh(makeShardMesh())) {
    reset();
  }

  SparkThresholdTuning tuning;
  Scene scene;
  CommandReplay replay;
  DeterministicRandomStream rng{0u};
  SparkThresholdStatus status;
  std::vector<SparkNodeState> nodes;
  std::vector<SparkThresholdNode> route_order;
  Vec3 player_position{0.0f, 0.32f, -8.0f};
  Vec3 player_velocity{};
  float player_yaw = 0.0f;
  std::uint32_t command_sequence = 0u;
  std::shared_ptr<const CpuMesh> ring_mesh;
  std::shared_ptr<const CpuMesh> gate_ring_mesh;
  std::shared_ptr<const CpuMesh> vertical_gate_ring_mesh;
  std::shared_ptr<const CpuMesh> operator_mesh;
  std::shared_ptr<const CpuMesh> shard_mesh;

  void reset() {
    rng.reset(tuning.seed);
    replay.clear();
    command_sequence = 0u;
    route_order.clear();
    player_position = {0.0f, 0.32f, -8.0f};
    player_velocity = {};
    player_yaw = 0.0f;
    nodes = {
        {SparkThresholdNode::Mine, {-6.4f, 0.30f, -2.2f}, "Maden Rezonatoru",
         "Maden izini dengele", "maden", 1, false},
        {SparkThresholdNode::Scrap, {6.8f, 0.30f, -1.2f}, "Hurda Dinamosu",
         "Hurda gucunu bas", "hurda", 2, false},
        {SparkThresholdNode::Castle, {0.0f, 0.30f, 6.4f}, "Kale Prizmasi",
         "Kale prizmasini cevir", "kale", 1, false},
    };
    status = {};
    status.title_screen = tuning.title_screen;
    status.time_remaining_seconds = static_cast<float>(tuning.time_limit_ticks) * kFixedStepSeconds;
    refreshStatusFlags();
    status.world_hash = computeWorldHash();
    rebuildScene();
  }

  void startRun() {
    if (!status.title_screen) {
      return;
    }
    status.title_screen = false;
    status.outcome = SparkThresholdOutcome::Playing;
    status.world_hash = computeWorldHash();
    rebuildScene();
  }

  [[nodiscard]] bool playing() const {
    return !status.title_screen && status.outcome == SparkThresholdOutcome::Playing;
  }

  void refreshStatusFlags() {
    status.activated_nodes = 0;
    status.mine_active = false;
    status.scrap_active = false;
    status.castle_active = false;
    status.overload = 0;
    for (const SparkNodeState &node : nodes) {
      if (!node.active) {
        continue;
      }
      ++status.activated_nodes;
      status.overload += node.overload_cost;
      if (node.kind == SparkThresholdNode::Mine) {
        status.mine_active = true;
      } else if (node.kind == SparkThresholdNode::Scrap) {
        status.scrap_active = true;
      } else if (node.kind == SparkThresholdNode::Castle) {
        status.castle_active = true;
      }
    }
    status.final_gate_live = status.activated_nodes >= 2;
    status.has_first_node = !route_order.empty();
    status.first_node = route_order.empty() ? SparkThresholdNode::Mine : route_order.front();
  }

  [[nodiscard]] SparkNodeState *nodeFor(const SparkThresholdNode kind) {
    for (SparkNodeState &node : nodes) {
      if (node.kind == kind) {
        return &node;
      }
    }
    return nullptr;
  }

  [[nodiscard]] const SparkNodeState *nodeFor(const SparkThresholdNode kind) const {
    for (const SparkNodeState &node : nodes) {
      if (node.kind == kind) {
        return &node;
      }
    }
    return nullptr;
  }

  [[nodiscard]] std::optional<SparkThresholdNode> focusedNode() const {
    float best_distance = tuning.interact_radius;
    std::optional<SparkThresholdNode> best;
    for (const SparkNodeState &node : nodes) {
      if (node.active) {
        continue;
      }
      const float distance = planarDistance(player_position, node.position);
      if (distance <= best_distance) {
        best_distance = distance;
        best = node.kind;
      }
    }
    return best;
  }

  [[nodiscard]] bool atFinalGate() const {
    return planarDistance(player_position, finalGatePosition()) <= tuning.gate_radius;
  }

  [[nodiscard]] Vec3 finalGatePosition() const {
    return {0.0f, 0.32f, 10.6f};
  }

  void activateNode(const SparkThresholdNode kind) {
    SparkNodeState *node = nodeFor(kind);
    if (node == nullptr || node->active) {
      return;
    }
    node->active = true;
    route_order.push_back(kind);
    refreshStatusFlags();
  }

  void finishRun() {
    if (!status.final_gate_live) {
      return;
    }
    if (!route_order.empty() && route_order.front() == SparkThresholdNode::Scrap) {
      status.outcome = SparkThresholdOutcome::OverloadEnding;
      status.ending_key = "asiri-yuk";
    } else if (status.activated_nodes >= status.total_nodes) {
      status.outcome = SparkThresholdOutcome::ResonanceEnding;
      status.ending_key = "rezonans";
    } else {
      status.outcome = SparkThresholdOutcome::EscapeEnding;
      status.ending_key = "kacis";
    }
  }

  void interact() {
    if (status.title_screen) {
      startRun();
      return;
    }
    if (!playing()) {
      return;
    }
    if (status.final_gate_live && atFinalGate()) {
      finishRun();
      return;
    }
    if (const std::optional<SparkThresholdNode> focus = focusedNode(); focus.has_value()) {
      activateNode(*focus);
    }
  }

  void updateFixed(SimCommand command) {
    command.tick = status.tick;
    command.sequence = command_sequence++;
    if (status.title_screen) {
      if (command.pressed(SimCommandButton::Interact) || command.pressed(SimCommandButton::Primary)) {
        startRun();
      }
      return;
    }
    if (status.outcome != SparkThresholdOutcome::Playing) {
      return;
    }

    replay.record(command);
    if (command.pressed(SimCommandButton::Interact) || command.pressed(SimCommandButton::Primary)) {
      interact();
    }
    if (status.outcome == SparkThresholdOutcome::Playing) {
      const Vec2 axis = simCommandMoveAxis(command);
      const float speed =
          command.pressed(SimCommandButton::Run) ? tuning.run_speed : tuning.walk_speed;
      Vec3 move{axis.x, 0.0f, axis.y};
      if (length(move) > 1.0f) {
        move = normalize(move);
      }
      if (length(move) > 0.001f) {
        player_yaw = std::atan2(move.x, move.z);
      }
      player_velocity = move * speed;
      player_position = player_position + player_velocity * kFixedStepSeconds;
      player_position.x = std::clamp(player_position.x, -10.8f, 10.8f);
      player_position.z = std::clamp(player_position.z, -9.8f, 12.4f);

      ++status.tick;
      status.elapsed_seconds = static_cast<float>(status.tick) * kFixedStepSeconds;
      status.time_remaining_seconds =
          std::max(0.0f, static_cast<float>(tuning.time_limit_ticks) * kFixedStepSeconds -
                             status.elapsed_seconds);
      if (status.tick >= static_cast<std::uint32_t>(std::max(tuning.time_limit_ticks, 1))) {
        status.outcome = SparkThresholdOutcome::Defeated;
        status.ending_key = "kayip";
      }
    }
    refreshStatusFlags();
    status.replay_checksum = replay.checksum();
    status.world_hash = computeWorldHash();
    rebuildScene();
  }

  [[nodiscard]] Vec3 nextScriptTarget(const SparkThresholdScriptRoute route) const {
    const SparkScriptPlan plan = scriptPlan(route);
    for (const SparkThresholdNode node_kind : plan.nodes) {
      const SparkNodeState *node = nodeFor(node_kind);
      if (node != nullptr && !node->active) {
        return node->position;
      }
    }
    return finalGatePosition();
  }

  [[nodiscard]] SimCommand scriptedCommand(const SparkThresholdScriptRoute route) const {
    SimCommand command;
    command.tick = status.tick;
    if (status.title_screen) {
      command.set(SimCommandButton::Interact, true);
      return command;
    }
    if (status.outcome != SparkThresholdOutcome::Playing) {
      return command;
    }
    const Vec3 target = nextScriptTarget(route);
    const float distance = planarDistance(player_position, target);
    const bool target_is_gate = status.final_gate_live && planarDistance(target, finalGatePosition()) < 0.05f;
    const float threshold = target_is_gate ? tuning.gate_radius * 0.62f : tuning.interact_radius * 0.58f;
    if (distance <= threshold) {
      command.set(SimCommandButton::Interact, true);
      return command;
    }
    const Vec2 axis = planarDirectionTo(player_position, target);
    command.strafe = static_cast<std::int16_t>(std::clamp(axis.x, -1.0f, 1.0f) * 32767.0f);
    command.forward = static_cast<std::int16_t>(std::clamp(axis.y, -1.0f, 1.0f) * 32767.0f);
    command.set(SimCommandButton::Run, true);
    return command;
  }

  [[nodiscard]] std::uint64_t computeWorldHash() const {
    std::uint64_t hash = 0xA57E5F4A7E2026ull;
    hash = hashCombine64(hash, tuning.seed);
    hash = hashCombine64(hash, status.tick);
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.outcome));
    hash = hashCombine64(hash, status.title_screen ? 1u : 0u);
    hash = hashCombine64(hash, stableHash64(player_position));
    hash = hashCombine64(hash, stableHash64(player_velocity));
    hash = hashCombine64(hash, stableHash64(player_yaw));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.health));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.activated_nodes));
    hash = hashCombine64(hash, static_cast<std::uint64_t>(status.overload));
    for (const SparkNodeState &node : nodes) {
      hash = hashCombine64(hash, static_cast<std::uint64_t>(node.kind));
      hash = hashCombine64(hash, node.active ? 1u : 0u);
    }
    for (const SparkThresholdNode node : route_order) {
      hash = hashCombine64(hash, static_cast<std::uint64_t>(node));
    }
    return hashCombine64(hash, replay.checksum());
  }

  void addObject(std::string name, const MeshPrimitive primitive, const Vec3 position,
                 const Vec3 scale, const Material &material, const bool contact_shadow = false) {
    RenderObject object;
    object.name = std::move(name);
    object.primitive = primitive;
    object.transform.position = position;
    object.transform.scale = scale;
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = primitive != MeshPrimitive::Plane;
    object.casts_contact_shadow = contact_shadow;
    object.contact_shadow_strength = contact_shadow ? 0.42f : 0.0f;
    object.contact_shadow_radius_scale = 0.78f;
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
    object.contact_shadow_strength = contact_shadow ? 0.36f : 0.0f;
    object.auto_contact_shadow = contact_shadow;
    scene.objects().push_back(std::move(object));
  }

  void addSegment(std::string name, const Vec3 from, const Vec3 to, const float width,
                  const float height, const Material &material, const float y) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float segment_length = std::max(std::sqrt(dx * dx + dz * dz), 0.001f);
    RenderObject object;
    object.name = std::move(name);
    object.primitive = MeshPrimitive::Box;
    object.transform.position = {(from.x + to.x) * 0.5f, y, (from.z + to.z) * 0.5f};
    object.transform.rotation = quatFromEulerXyz({0.0f, std::atan2(dx, dz), 0.0f});
    object.transform.scale = {width, height, segment_length};
    object.material = material;
    object.material_asset_id = material.asset_id;
    object.casts_shadows = false;
    object.auto_contact_shadow = false;
    scene.objects().push_back(std::move(object));
  }

  [[nodiscard]] float pulse(const float rate, const float phase = 0.0f) const {
    return 0.5f + 0.5f * std::sin(static_cast<float>(status.tick) * rate + phase);
  }

  [[nodiscard]] bool nodeActive(const SparkThresholdNode kind) const {
    const SparkNodeState *node = nodeFor(kind);
    return node != nullptr && node->active;
  }

  void addZonePlatforms(const Material &basalt, const Material &scrap, const Material &stone,
                        const Material &neon_dim) {
    addObject("Neon mine gallery wet basalt floor", MeshPrimitive::Box, {-6.4f, 0.02f, -2.2f},
              {5.2f, 0.055f, 4.4f}, basalt);
    addObject("Scrap energy yard oxidized deck", MeshPrimitive::Box, {6.8f, 0.02f, -1.2f},
              {5.4f, 0.060f, 4.8f}, scrap);
    addObject("Ancient castle threshold stone dais", MeshPrimitive::Box, {0.0f, 0.02f, 6.4f},
              {5.6f, 0.065f, 4.4f}, stone);
    addObject("Central guide path basalt spine", MeshPrimitive::Box, {0.0f, 0.035f, -2.0f},
              {1.15f, 0.035f, 12.0f}, basalt);
    for (int i = 0; i < 5; ++i) {
      const float offset = (static_cast<float>(i) - 2.0f) * 0.82f;
      addObject("Mine floor cyan fissure strip", MeshPrimitive::Box,
                {-6.4f + offset, 0.092f, -2.2f}, {0.050f, 0.018f, 3.70f}, neon_dim);
      addObject("Scrap floor amber heat seam", MeshPrimitive::Box,
                {6.8f, 0.096f, -1.2f + offset}, {4.10f, 0.018f, 0.050f}, neon_dim);
      addObject("Castle dais violet mortar line", MeshPrimitive::Box,
                {offset, 0.098f, 6.4f}, {0.045f, 0.018f, 3.55f}, neon_dim);
    }
    addSegment("Dormant route conduit mine to gate", {-6.4f, 0.0f, -2.2f}, {0.0f, 0.0f, 10.6f},
               0.16f, 0.024f, neon_dim, 0.095f);
    addSegment("Dormant route conduit scrap to gate", {6.8f, 0.0f, -1.2f}, {0.0f, 0.0f, 10.6f},
               0.16f, 0.024f, neon_dim, 0.096f);
    addSegment("Dormant route conduit castle to gate", {0.0f, 0.0f, 6.4f}, {0.0f, 0.0f, 10.6f},
               0.16f, 0.024f, neon_dim, 0.097f);
  }

  void addNodeVisual(const SparkNodeState &node, Material active_material, Material inactive_material) {
    const bool active = node.active;
    Material lens = active ? active_material : inactive_material;
    if (active) {
      lens.emission_strength += 0.7f + pulse(0.17f, static_cast<float>(node.overload_cost)) * 0.45f;
    }
    const char *base_name = node.kind == SparkThresholdNode::Mine
                                ? "Mine Energy Node"
                                : (node.kind == SparkThresholdNode::Scrap ? "Scrap Dynamo"
                                                                          : "Castle Prism");
    addObject(std::string(base_name) + " Pedestal", MeshPrimitive::Pillar,
              {node.position.x, 0.25f, node.position.z}, {0.46f, 0.46f, 0.46f},
              inactive_material, true);
    addMeshObject(std::string(base_name) + " Ring", ring_mesh,
                  {node.position.x, 0.56f, node.position.z}, 0.0f,
                  active ? Vec3{1.30f, 1.0f, 1.30f} : Vec3{1.08f, 1.0f, 1.08f}, lens);
    addMeshObject(std::string(base_name) + " Lens", shard_mesh,
                  {node.position.x, 0.80f, node.position.z}, pulse(0.055f) * 6.2831f,
                  active ? Vec3{0.86f, 1.10f, 0.86f} : Vec3{0.64f, 0.82f, 0.64f}, lens,
                  true);
  }

  void rebuildScene() {
    scene.objects().clear();
    scene.reflectionProbes().clear();

    Material basalt = sparkMaterial("material.spark.wet_basalt", {0.035f, 0.040f, 0.048f},
                                    {0.02f, 0.09f, 0.12f}, 0.05f,
                                    MaterialSurfaceProfile::StratifiedRock,
                                    SurfacePattern::CaveRock);
    Material scrap = sparkMaterial("material.spark.oxidized_metal", {0.18f, 0.082f, 0.046f},
                                   {0.20f, 0.06f, 0.02f}, 0.10f,
                                   MaterialSurfaceProfile::CorrodedMetal,
                                   SurfacePattern::WeatheredMetal);
    Material stone = sparkMaterial("material.spark.castle_stone", {0.17f, 0.16f, 0.145f},
                                   {0.06f, 0.04f, 0.10f}, 0.06f,
                                   MaterialSurfaceProfile::Masonry,
                                   SurfacePattern::WeatheredStone);
    Material neon = translucentSparkMaterial("material.spark.neon_conduit", {0.020f, 0.18f, 0.18f},
                                             {0.06f, 0.92f, 1.00f}, 1.18f, 0.86f);
    Material amber = translucentSparkMaterial("material.spark.broken_lens", {0.28f, 0.12f, 0.025f},
                                              {1.00f, 0.55f, 0.10f}, 1.46f, 0.92f);
    Material dim = translucentSparkMaterial("material.spark.neon_conduit", {0.024f, 0.070f, 0.075f},
                                            {0.04f, 0.20f, 0.24f}, 0.18f, 0.46f);
    Material player = sparkMaterial("material.spark.neon_conduit", {0.030f, 0.090f, 0.16f},
                                    {0.10f, 0.62f, 1.00f}, 0.82f,
                                    MaterialSurfaceProfile::EmissiveLens, SurfacePattern::AmberResin);
    Material gate = status.final_gate_live ? neon : dim;
    gate.emission_strength += status.final_gate_live ? 0.95f + pulse(0.12f) * 0.55f : 0.0f;

    addObject("Spark Threshold dark terrain foundation", MeshPrimitive::Box, {0.0f, -0.035f, 1.0f},
              {23.0f, 0.055f, 24.0f}, basalt);
    addZonePlatforms(basalt, scrap, stone, dim);

    for (int i = 0; i < 5; ++i) {
      const float offset = static_cast<float>(i) - 2.0f;
      addObject("Mine ribbed basalt arch", MeshPrimitive::Pillar,
                {-8.7f + offset * 0.86f, 0.62f, -4.35f}, {0.16f, 0.90f, 0.16f},
                basalt, true);
      addObject("Mine hanging cyan shard", MeshPrimitive::Crystal,
                {-6.4f + offset * 0.70f, 1.00f, -3.75f}, {0.18f, 0.34f, 0.18f},
                neon, true);
      addObject("Scrap yard pressure column", MeshPrimitive::Box,
                {8.9f, 0.48f, -3.1f + offset * 0.82f}, {0.24f, 0.72f, 0.24f},
                scrap, true);
      addObject("Scrap yard amber breaker light", MeshPrimitive::Box,
                {8.58f, 0.86f, -3.1f + offset * 0.82f}, {0.08f, 0.08f, 0.22f},
                amber);
      addObject("Castle weathered threshold pillar", MeshPrimitive::Pillar,
                {-2.3f + offset * 1.15f, 0.58f, 8.55f}, {0.18f, 0.82f, 0.18f},
                stone, true);
      addObject("Castle blue rune inset", MeshPrimitive::Box,
                {-2.3f + offset * 1.15f, 0.98f, 8.35f}, {0.16f, 0.035f, 0.035f},
                neon);
    }

    for (const SparkNodeState &node : nodes) {
      addNodeVisual(node, node.kind == SparkThresholdNode::Scrap ? amber : neon,
                    node.kind == SparkThresholdNode::Castle ? stone : dim);
    }

    if (nodeActive(SparkThresholdNode::Mine)) {
      addSegment("Routed mine resonance conduit", {-6.4f, 0.0f, -2.2f}, {0.0f, 0.0f, 10.6f},
                 0.23f, 0.035f, neon, 0.132f);
    }
    if (nodeActive(SparkThresholdNode::Scrap)) {
      Material hot = amber;
      hot.emission_strength += 0.70f;
      addSegment("Routed scrap overload conduit", {6.8f, 0.0f, -1.2f}, {0.0f, 0.0f, 10.6f},
                 0.26f, 0.040f, hot, 0.136f);
    }
    if (nodeActive(SparkThresholdNode::Castle)) {
      addSegment("Routed castle prism conduit", {0.0f, 0.0f, 6.4f}, {0.0f, 0.0f, 10.6f},
                 0.22f, 0.034f, neon, 0.134f);
    }

    addObject("Final Threshold Gate Base", MeshPrimitive::Box, {0.0f, 0.16f, 10.6f},
              {2.4f, 0.18f, 0.92f}, status.final_gate_live ? stone : basalt, true);
    addMeshObject(status.final_gate_live ? "Final Threshold Gate Ring" : "Dormant Final Gate Ring",
                  vertical_gate_ring_mesh, {0.0f, 1.18f, 10.52f}, 0.0f,
                  status.final_gate_live ? Vec3{1.36f, 1.36f, 1.36f} : Vec3{1.05f, 1.05f, 1.05f},
                  gate);
    addObject("Final Threshold left stone cheek", MeshPrimitive::Pillar, {-1.55f, 0.78f, 10.72f},
              {0.22f, 0.90f, 0.22f}, stone, true);
    addObject("Final Threshold right stone cheek", MeshPrimitive::Pillar, {1.55f, 0.78f, 10.72f},
              {0.22f, 0.90f, 0.22f}, stone, true);
    addObject("Final Threshold crown lens", MeshPrimitive::Crystal, {0.0f, 2.42f, 10.55f},
              {0.28f, 0.38f, 0.28f}, status.final_gate_live ? gate : dim);

    addMeshObject("Spark Threshold Operator", operator_mesh,
                  {player_position.x, player_position.y + 0.16f, player_position.z}, player_yaw,
                  {1.08f, 1.0f, 1.08f}, player, true);
    addObject("Operator chest guide light", MeshPrimitive::Sphere,
              {player_position.x, player_position.y + 0.46f, player_position.z + 0.02f},
              {0.16f, 0.16f, 0.16f}, neon);

    if (status.outcome == SparkThresholdOutcome::OverloadEnding) {
      Material flare = amber;
      flare.emission_strength += 2.4f;
      addMeshObject("Overload ending amber shock ring", vertical_gate_ring_mesh,
                    {0.0f, 1.18f, 10.45f}, 0.0f, {2.25f, 2.25f, 2.25f}, flare);
    } else if (status.outcome == SparkThresholdOutcome::ResonanceEnding) {
      Material harmony = neon;
      harmony.emission_strength += 2.0f;
      addMeshObject("Resonance ending blue harmony ring", vertical_gate_ring_mesh,
                    {0.0f, 1.18f, 10.45f}, 0.0f, {2.12f, 2.12f, 2.12f}, harmony);
    }

    scene.reflectionProbes().push_back({"Spark Threshold wet mine probe", {-4.0f, 2.0f, -1.0f},
                                        8.0f, {0.18f, 0.32f, 0.38f},
                                        {0.08f, 0.06f, 0.05f}, {0.75f, 0.95f, 1.0f}, 1.05f,
                                        {}});
    scene.reflectionProbes().push_back({"Spark Threshold castle probe", {0.0f, 2.0f, 7.2f},
                                        7.0f, {0.22f, 0.18f, 0.32f},
                                        {0.12f, 0.08f, 0.06f}, {1.0f, 0.86f, 0.65f}, 0.92f,
                                        {}});
  }

  [[nodiscard]] SparkThresholdGuideStep guideStep() const {
    if (status.title_screen) {
      return {"Baslangic", "E / Space ile Kivilcim Esigi'ne gir.",
              "Farkli enerji siralari farkli sonlar acar.", false};
    }
    if (status.outcome != SparkThresholdOutcome::Playing) {
      return {"Yol tamamlandi", "R ile yeniden baslatip baska bir path deneyebilirsin.",
              "Hurdayi ilk secmek riski yukseltir.", true};
    }
    if (status.elapsed_seconds < 18.0f && status.activated_nodes == 0) {
      return {"Hareket", "WASD ile kos, Shift ile hizlan, E ile yakin dugumu bagla.",
              "Ilk secim sonun tonunu belirler.", false};
    }
    if (status.activated_nodes < 2) {
      return {"Enerji izi", "En az iki dugum bagla: maden, hurda avlusu veya kale prizmasi.",
              "Dengeli bir rota icin hurdayi sona birak.", false};
    }
    if (!status.final_gate_live || !atFinalGate()) {
      return {"Esik acik", "Kale kapisindaki halka yandi. Oraya git ve E ile finali sec.",
              status.activated_nodes == 2 ? "Iki dugum Kacis sonunu acar."
                                          : "Uc dengeli dugum Rezonans sonunu acar.",
              false};
    }
    return {"Final", "E ile esigi tetikle.", "Sectigin sira finali belirleyecek.", false};
  }

  [[nodiscard]] SparkThresholdHudModel hudModel() const {
    SparkThresholdHudModel model;
    model.title = "Kivilcim Esigi";
    model.subtitle = "Neon maden, hurda enerji avlusu ve eski kale arasinda kisa bir esik kosusu.";
    model.title_screen = status.title_screen;
    model.playing = playing();
    model.ended = status.outcome != SparkThresholdOutcome::Playing;
    model.outcome = status.outcome;
    model.time_fraction =
        tuning.time_limit_ticks <= 0
            ? 0.0f
            : std::clamp(status.time_remaining_seconds /
                             (static_cast<float>(tuning.time_limit_ticks) * kFixedStepSeconds),
                         0.0f, 1.0f);
    model.node_fraction = static_cast<float>(status.activated_nodes) /
                          static_cast<float>(std::max(status.total_nodes, 1));
    const SparkThresholdGuideStep guide = guideStep();
    model.guide_title = guide.title;
    model.guide_body = guide.body;
    model.guide_hint = guide.hint;
    model.objective = status.final_gate_live
                          ? "Esik canli: final halkasina git."
                          : "En az iki enerji dugumunu bagla.";
    model.status_line = "Enerji " + std::to_string(status.activated_nodes) + "/" +
                        std::to_string(status.total_nodes) + "  Asiri yuk " +
                        std::to_string(status.overload);
    if (status.has_first_node) {
      model.route_hint = std::string("Ilk iz: ") + nodeName(status.first_node);
    } else {
      model.route_hint = "Ilk iz henuz secilmedi.";
    }
    if (status.final_gate_live && atFinalGate()) {
      model.prompt_line = "E: Esigi tetikle";
    } else if (const std::optional<SparkThresholdNode> focus = focusedNode(); focus.has_value()) {
      model.prompt_line = std::string("E: ") + nodeName(*focus) + " bagla";
    } else {
      model.prompt_line = "Yakindaki isikli dugume yaklas.";
    }

    switch (status.outcome) {
    case SparkThresholdOutcome::Playing:
      break;
    case SparkThresholdOutcome::ResonanceEnding:
      model.ending_title = "Rezonans";
      model.ending_body = "Uc iz dengelendi. Esik sessizce acildi ve vadi isigi geri dondu.";
      break;
    case SparkThresholdOutcome::EscapeEnding:
      model.ending_title = "Kacis";
      model.ending_body = "Yeterli enerjiyi aldin ve gecitten ciktin. Geride hala karanlik odalar var.";
      break;
    case SparkThresholdOutcome::OverloadEnding:
      model.ending_title = "Asiri Yuk";
      model.ending_body = "Hurda dinamosu one gecti. Esik acildi ama arkanda amber bir firtina kaldi.";
      break;
    case SparkThresholdOutcome::Defeated:
      model.ending_title = "Kayip";
      model.ending_body = "Esik sogudu. R ile yeniden baslatip farkli bir sira dene.";
      break;
    }
    return model;
  }
};

SparkThreshold::SparkThreshold(SparkThresholdTuning tuning)
    : impl_(std::make_unique<Impl>(std::move(tuning))) {}

SparkThreshold::~SparkThreshold() = default;

SparkThreshold::SparkThreshold(SparkThreshold &&) noexcept = default;

SparkThreshold &SparkThreshold::operator=(SparkThreshold &&) noexcept = default;

void SparkThreshold::reset() {
  impl_->reset();
}

void SparkThreshold::startRun() {
  impl_->startRun();
}

void SparkThreshold::updateFixed(const SimCommand command) {
  impl_->updateFixed(command);
}

SimCommand SparkThreshold::scriptedCommand(const SparkThresholdScriptRoute route) const {
  return impl_->scriptedCommand(route);
}

const Scene &SparkThreshold::scene() const {
  return impl_->scene;
}

const SparkThresholdStatus &SparkThreshold::status() const {
  return impl_->status;
}

const CommandReplay &SparkThreshold::replay() const {
  return impl_->replay;
}

SparkThresholdHudModel SparkThreshold::hudModel() const {
  return impl_->hudModel();
}

SparkThresholdGuideStep SparkThreshold::guideStep() const {
  return impl_->guideStep();
}

Vec3 SparkThreshold::playerPosition() const {
  return impl_->player_position;
}

Vec3 SparkThreshold::cameraTarget() const {
  return impl_->player_position + Vec3{0.0f, 0.48f, 0.0f};
}

std::uint64_t SparkThreshold::worldHash() const {
  return impl_->status.world_hash;
}

} // namespace aster
