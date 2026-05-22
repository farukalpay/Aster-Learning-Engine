// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/game_sdk/game_sdk.hpp"
#include "aster/core/belief_extraction.hpp"
#include "aster/core/perceptual_world_runtime.hpp"
#include "aster/core/world_perception_ledger.hpp"
#include "aster/core/world_state.hpp"
#include "aster/systems/animation_system.hpp"
#include "aster/systems/classic_actor_runtime.hpp"
#include "aster/systems/creature_motion.hpp"
#include "aster/systems/equipment_system.hpp"
#include "aster/systems/interaction_system.hpp"
#include "aster/systems/inventory_system.hpp"
#include "aster/systems/item_system.hpp"
#include "aster/systems/light_system.hpp"
#include "aster/systems/mining_system.hpp"
#include "aster/systems/particle_system.hpp"
#include "aster/systems/world_mechanism.hpp"
#include "aster/geometry/cave_system.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/math/vec.hpp"
#include "aster/physics/climb_locomotion.hpp"
#include "aster/physics/physics_world.hpp"
#include "aster/physics/surface_support.hpp"
#include "aster/scene/avatar.hpp"
#include "aster/scene/scene.hpp"
#include "aster/scene/scene_coherence.hpp"
#include "aster/scene/scene_trace.hpp"
#include "aster/ui/hud_layer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct LumenTuning {
  int shard_count = 11;
  int sentinel_count = 0;
  float arena_radius = 7.4f;
  float player_radius = 0.205f;
  float player_height = 0.52f;
  float shard_radius = 0.18f;
  float sentinel_radius = 0.30f;
  float player_speed = 3.0f;
  float run_multiplier = 1.65f;
  float jump_speed = 4.7f;
  float response_rate = 11.0f;
  float sentinel_speed = 0.58f;
  float invulnerability_seconds = 1.1f;
  float playable_radius = 104.0f;
  float recovery_floor_y = -5.0f;
};

struct LumenStatus {
  int score = 0;
  int total_shards = 0;
  int lives = 3;
  int health = 20;
  int max_health = 20;
  float elapsed_seconds = 0.0f;
  bool victory = false;
  bool defeated = false;
};

struct CaveWallLightSample {
  Vec3 position{};
  Vec3 color{1.0f, 0.16f, 0.08f};
  float intensity = 0.0f;
  float source_radius = 1.0f;
  float tunnel_t = 0.0f;
};

struct CaveLightingState {
  float interior = 0.0f;
  float entrance_light = 0.0f;
  float depth = 0.0f;
  float chamber = 0.0f;
  Vec3 entrance_light_position{};
  float wall_light = 0.0f;
  Vec3 wall_light_position{};
  Vec3 wall_light_color{1.0f, 0.16f, 0.08f};
  std::vector<CaveWallLightSample> wall_lights;
};

enum class CaveDebugOverlayLayer : std::uint32_t {
  Collision = 1u << 0u,
  Interactable = 1u << 1u,
  MiningTarget = 1u << 2u,
  SpawnVolume = 1u << 3u,
  CameraObstruction = 1u << 4u,
  Walkable = 1u << 5u,
};

struct LumenAuthoringData {
  sdk::ProjectDocument project;
  sdk::SceneDocument scene;
  sdk::CaveDocument cave;
  bool valid = false;
};

enum class LumenWorldGateVerdict : std::uint32_t {
  Unknown,
  Accepted,
  Quarantined,
};

struct LumenCaveWorldGateReport {
  std::uint64_t seed = 0u;
  std::uint64_t region_id = 0u;
  std::uint64_t probe_trace_hash = 0u;
  LumenWorldGateVerdict verdict = LumenWorldGateVerdict::Unknown;
  bool navigation_valid = false;
  std::size_t checked_steps = 0u;
  std::size_t blocked_steps = 0u;
  bool resource_valid = false;
  std::size_t reachable_resources = 0u;
  std::size_t required_resources = 0u;
  bool encounter_valid = false;
  float encounter_budget = 0.0f;
  std::size_t reachable_encounters = 0u;
  bool perceptual_valid = false;
  float perceptual_salience_score = 0.0f;
  float perceptual_minimum_salience = 0.0f;
  bool perceptual_continuity_valid = false;
  std::uint32_t perceptual_continuity_required_channel_mask = 0u;
  std::uint32_t perceptual_continuity_observed_channel_mask = 0u;
  std::uint32_t perceptual_continuity_missing_channel_mask = 0u;
  float perceptual_continuity_score = 0.0f;
  float perceptual_continuity_minimum_score = 0.0f;
  bool perception_ledger_valid = false;
  std::uint32_t perception_ledger_required_channel_mask = 0u;
  std::uint32_t perception_ledger_observed_channel_mask = 0u;
  std::uint32_t perception_ledger_missing_channel_mask = 0u;
  float perception_ledger_score = 0.0f;
  float perception_ledger_minimum_score = 0.0f;
  std::size_t perception_ledger_cell_count = 0u;
  std::uint64_t nav_report_hash = 0u;
  std::uint64_t resource_probe_hash = 0u;
  std::uint64_t encounter_budget_hash = 0u;
  std::uint64_t perceptual_report_hash = 0u;
  std::uint64_t perceptual_continuity_report_hash = 0u;
  std::uint64_t perception_ledger_hash = 0u;
  std::string diagnostic;
};

struct LumenReactionPackageReport {
  bool accepted = false;
  std::uint32_t required_channel_mask = 0u;
  std::uint32_t observed_channel_mask = 0u;
  std::uint32_t missing_channel_mask = 0u;
  float continuity_score = 0.0f;
  float minimum_score = 0.0f;
  std::uint64_t reaction_package_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t contact_history_hash = 0u;
  std::uint64_t lighting_atmosphere_hash = 0u;
  std::uint64_t wear_continuity_hash = 0u;
  std::uint64_t ai_attention_hash = 0u;
  std::uint64_t streaming_residency_lod_hash = 0u;
  std::uint64_t resource_state_hash = 0u;
  std::uint64_t event_residue_hash = 0u;
  std::uint64_t audio_visual_cue_budget_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;
  std::string diagnostic;
};

struct LumenWorldForensics {
  std::uint64_t epoch = 0u;
  std::uint64_t world_hash = 0u;
  std::uint64_t trace_hash = 0u;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t actor_state_delta_hash = 0u;
  std::size_t actor_delta_count = 0u;
  std::uint64_t sensory_event_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  std::uint64_t render_extraction_hash = 0u;
  std::uint64_t frame_submission_hash = 0u;
  std::uint64_t streaming_region_id = 0u;
  LumenCaveWorldGateReport cave_gate;
  LumenReactionPackageReport coal_mining_reaction;
  WorldPerceptionLedgerReport perception_ledger;
  PerceptualFrameState perceptual_state;
  PerceptualWorldScheduleReport perceptual_schedule;
  BeliefExtractionReport belief_report;
  std::vector<WorldPerceptionObjectTrace> perception_object_traces;
  bool render_extraction_ready = false;
};

class LumenRun {
public:
  explicit LumenRun(LumenTuning tuning = {});
  explicit LumenRun(LumenAuthoringData authoring, LumenTuning tuning = {});

  void reset();
  void update(float dt, Vec2 move_axis, bool run_requested, bool jump_requested);

  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const SceneCoherenceReport &sceneCoherenceReport() const;
  [[nodiscard]] const SceneTraceValidationReport &sceneTraceReport() const;
  [[nodiscard]] const LumenStatus &status() const;
  [[nodiscard]] const LumenWorldForensics &worldForensics() const;
  [[nodiscard]] const LumenCaveWorldGateReport &caveWorldGateReport() const;
  [[nodiscard]] bool caveWorldGateAccepted() const;
  [[nodiscard]] Vec3 playerPosition() const;
  [[nodiscard]] Vec3 playerRenderPosition() const;
  [[nodiscard]] Vec3 prismRelayBasePosition() const;
  [[nodiscard]] Vec3 prismRelayFocusPosition() const;
  void relocatePlayer(Vec3 position, float facing_yaw = 0.0f);
  [[nodiscard]] Vec3 caveFrameReportPosition(float progress_distance) const;
  [[nodiscard]] Vec3 caveFrameReportLookTarget(float progress_distance, float look_ahead) const;
  [[nodiscard]] float caveFrameReportCameraYaw(float progress_distance) const;
  [[nodiscard]] float resolveCameraRadius(Vec3 target, float yaw, float pitch,
                                          float desired_radius) const;
  void updateRenderInterpolation(float alpha);
  void setAvatarPreviewYaw(float yaw);
  void clearAvatarPreviewYaw();
  void setPlayerAvatarVisible(bool visible);
  void setAvatarPointTarget(Vec3 target);
  bool pointAvatarAtRay(Vec3 origin, Vec3 direction, float max_distance = 90.0f);
  void clearAvatarPointTarget();
  void noteRenderExtraction(std::uint64_t extraction_hash, std::uint64_t frame_submission_hash,
                            float frame_cost_ms = 0.0f);
  void updateInteractionFocus(Vec3 ray_origin, Vec3 ray_direction, float dt);
  void interactFocused();
  void secondaryInteractFocused(Vec3 ray_origin, Vec3 ray_direction);
  void activatePrismRelay();
  void openChest();
  void closeChest();
  [[nodiscard]] bool takeChestItem(std::string_view item_id);
  [[nodiscard]] bool takeChestSlot(std::size_t slot_index);
  [[nodiscard]] bool takeSupplyTorch();
  void selectHotbarSlot(std::size_t index);
  [[nodiscard]] bool chestInterfaceOpen() const;
  [[nodiscard]] bool supplyCrateNearby() const;
  [[nodiscard]] int torchCount() const;
  [[nodiscard]] Vec3 supplyCratePosition() const;
  [[nodiscard]] FocusPromptModel focusPromptModel() const;
  [[nodiscard]] HotbarHudModel hotbarHudModel() const;
  [[nodiscard]] ChestContentsHudModel chestContentsHudModel() const;
  [[nodiscard]] std::optional<DynamicPointLight> equippedLight() const;
  [[nodiscard]] std::optional<DynamicPointLight> pondAccentLight() const;
  [[nodiscard]] std::optional<DynamicPointLight> prismRelayLight() const;
  [[nodiscard]] CaveLightingState caveLightingState() const;
  [[nodiscard]] CaveLightingState caveLightingStateAt(Vec3 position) const;
  [[nodiscard]] bool classicGauntletActive() const;
  [[nodiscard]] const AutomapModel &classicGauntletAutomap() const;
  [[nodiscard]] ClassicHudSignalModel classicHudSignals() const;
  [[nodiscard]] TransitionWipeFrame classicTransitionWipe() const;
  [[nodiscard]] Vec3 classicGauntletEntryPosition() const;
  [[nodiscard]] Vec3 classicGauntletLookTarget() const;
  [[nodiscard]] float classicGauntletCameraYaw() const;
  void setCaveDebugOverlayEnabled(bool enabled);
  [[nodiscard]] bool caveDebugOverlayEnabled() const;
  void setCaveDebugOverlayLayerMask(std::uint32_t mask);
  [[nodiscard]] std::uint32_t caveDebugOverlayLayerMask() const;

private:
  struct Shard {
    Vec3 position{};
    bool collected = false;
    std::size_t object_index = 0;
  };

  struct Sentinel {
    float phase = 0.0f;
    float orbit_radius = 1.0f;
    std::size_t object_index = 0;
  };

  enum class DeathSequenceState {
    Alive,
    Falling,
    Blinking,
  };

  struct SceneParticle {
    Vec3 position{};
    Vec3 velocity{};
    float age = 0.0f;
    float lifetime = 1.0f;
    float size = 0.04f;
    std::size_t object_index = 0;
  };

  struct MiningFractureShardVisual {
    std::size_t object_index = 0;
    Vec3 position{};
    Vec3 velocity{};
    Vec3 rotation{};
    Vec3 angular_velocity{};
    float age = 1.0f;
    float lifetime = 1.0f;
    float base_scale = 1.0f;
    bool active = false;
  };

  struct AquaticCreature {
    Vec3 center{};
    Vec2 swim_radius{1.0f, 0.5f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    float phase = 0.0f;
    float speed = 1.0f;
    std::size_t object_index = 0;
  };

  struct CastleBird {
    AvianAgentState state{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    float body_length = 0.54f;
    float wing_span = 0.70f;
    float wing_phase_offset = 0.0f;
    float fear_radius = 3.0f;
    std::size_t object_index = 0;
  };

  struct ChestItemPart {
    std::size_t object_index = 0;
    Vec3 local_position{};
    Vec3 local_rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    float base_emission = 0.0f;
  };

  struct ChestItemVisual {
    std::string item_id;
    Vec3 local_focus_position{};
    float focus_radius = 0.24f;
    bool available = true;
    std::vector<ChestItemPart> parts;
  };

  struct EquippedItemPart {
    std::string item_id;
    std::size_t object_index = 0;
    Vec3 local_position{};
    Vec3 local_rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
  };

  struct PlacedResourceRock {
    std::string id;
    std::string item_id = "stone";
    ItemPlacementShape placement_shape = ItemPlacementShape::Rock;
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec3 rotation{};
    Vec3 scale{0.20f, 0.16f, 0.20f};
    Vec3 collision_half_extents{0.16f, 0.12f, 0.16f};
    std::size_t object_index = 0;
    PhysicsBodyHandle body{};
  };

  struct StaticSceneryBox {
    Vec3 center{};
    Vec3 half_extents{};
  };

  struct TorchParticleVisual {
    std::size_t object_index = 0;
  };

  struct CaveDebugOverlayObject {
    std::size_t object_index = 0;
    std::uint32_t layer = 0u;
    Vec3 visible_scale{1.0f, 1.0f, 1.0f};
    float visible_opacity = 0.26f;
    float visible_emission = 0.28f;
  };

  struct CaveWallFixtureVisual {
    std::size_t backplate = 0;
    std::size_t lens = 0;
    std::array<std::size_t, 5> vertical_guards{};
    std::array<std::size_t, 3> horizontal_guards{};
  };

  struct CaveFloorSupportSurface {
    CaveTunnelProfile tunnel{};
    std::shared_ptr<const TraversableManifold> manifold{};
    std::shared_ptr<const CpuMesh> floor_mesh{};
    std::shared_ptr<const CpuMesh> portal_floor_mesh{};
    float min_normal_y = 0.30f;
  };

  struct AuthoredCaveSection {
    CaveTunnelProfile tunnel{};
    std::vector<CaveWallFixturePlacement> wall_fixtures;
    std::vector<CaveWallFixturePlacement> secondary_wall_fixtures;
    bool contributes_entrance_light = false;
  };

  struct CoalOreNode {
    Vec3 position{};
    Vec3 normal{0.0f, 0.0f, 1.0f};
    Vec3 scale{0.18f, 0.14f, 0.08f};
    float radius = 0.22f;
    int health = 3;
    int max_health = 3;
    int yield_quantity = 2;
    std::size_t object_index = 0;
    bool collected = false;
    float hit_flash = 0.0f;
  };

  struct CaveWebObstacle {
    std::string id = "lumen.cave_web.0";
    Vec3 center{};
    Vec3 normal{0.0f, 0.0f, -1.0f};
    Vec3 side{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float radius_x = 1.0f;
    float radius_y = 1.0f;
    float thickness = 0.42f;
    float slow_scale = 0.18f;
    float hardness = 2.0f;
    std::size_t object_index = 0;
    bool broken = false;
    float hit_flash = 0.0f;
  };

  struct CaveSkitter {
    std::string id = "lumen.cave_skitter.0";
    CaveSkitterAgentState state{};
    int health = 3;
    int max_health = 3;
    std::size_t object_index = 0;
    bool dead = false;
    float hit_flash = 0.0f;
    float bite_flash = 0.0f;
    Vec3 last_hit_normal{0.0f, 1.0f, 0.0f};
  };

  struct ClassicGauntletActorVisual {
    std::string id;
    std::size_t object_index = 0;
    std::size_t eye_object_index = 0;
    std::size_t beacon_object_index = 0;
  };

  std::size_t appendObject(RenderObject object);
  void rebuildScene();
  void rebuildPhysicsWorld();
  void invalidateSceneReports();
  void rebuildSceneCoherenceReport() const;
  void rebuildSceneTraceReport() const;
  [[nodiscard]] SceneCoherenceProblem buildSceneCoherenceProblem() const;
  [[nodiscard]] SceneSymbolicTrace buildSceneSymbolicTrace() const;
  [[nodiscard]] std::vector<SceneTraceRule> sceneTraceRules() const;
  void updateSceneObjects(float animation_dt);
  void resetWorldProof();
  void rebuildCaveWorldGate();
  [[nodiscard]] PerceptualWorldRuntimeOptions perceptualRuntimeOptions(
      std::uint64_t region_id) const;
  [[nodiscard]] WorldPerceptionLedgerReport buildPerceptionLedgerReport(
      std::uint64_t region_id) const;
  [[nodiscard]] std::vector<WorldPerceptionObjectTrace>
  buildPerceptionObjectTraces(const WorldPerceptionLedgerReport &ledger) const;
  [[nodiscard]] PerceptualWorldObservation
  makePerceptualObservation(float dt, Vec2 move_axis, Vec3 previous_player_position) const;
  [[nodiscard]] PerceptualWorldScheduleReport
  buildPerceptualScheduleReport(float frame_cost_ms) const;
  [[nodiscard]] BeliefExtractionReport buildBeliefExtractionReport() const;
  void advancePerceptualRuntime(float dt, Vec2 move_axis, Vec3 previous_player_position);
  void advanceWorldProof(float dt, Vec2 move_axis, bool run_requested, bool jump_requested,
                         Vec3 previous_player_position);
  void updatePlayerPhysics(float dt, Vec2 move_axis, bool run_requested, bool jump_requested);
  void updateFishingVisual();
  void updateAquaticLifeVisual();
  void updateCastleBirds(float dt);
  void updateCastleBirdVisuals();
  void updateCrocodile(float dt);
  void updateCrocodileVisual();
  void updateCaveSkitters(float dt);
  void updateCaveSkitterVisuals(float dt);
  void updateBloodParticles(float dt);
  void updateMiningFractureVisuals(float dt);
  void updateChestInteractionState();
  void updateChestVisuals(float dt);
  void updateEquipmentVisuals(float dt);
  void updatePrismRelay(float dt);
  void updatePrismRelayVisuals(float dt);
  void updateClassicGauntlet(float dt);
  void updateClassicGauntletVisuals(float dt);
  void refreshClassicGauntletAutomap();
  void updateCaveVisuals(float dt);
  void updateCaveDebugOverlayVisibility();
  [[nodiscard]] float caveWebSlowScaleAt(Vec3 position) const;
  [[nodiscard]] bool applyPlayerDamage(int hit_points, Vec3 impact_origin);
  void spawnBloodBurst(Vec3 center, Vec3 impact_origin, float intensity);
  void updateDeathSequence(float dt);
  void updateDeathVisuals();
  void restorePlayerEyeObjects();
  void clearTransientFeedback();
  void triggerPlayerDeath(Vec3 impact_origin);
  void respawnPlayer();
  void enforceWorldBounds();
  void applyCaveWebTraversalGates(PhysicsBody &body);
  [[nodiscard]] bool isSwimmableWater(Vec3 support_position) const;
  [[nodiscard]] TerrainSurfaceSample sampleCaveFloorSupport(const SurfaceSupportQuery &query) const;
  [[nodiscard]] TerrainSurfaceSample sampleWorldSupport(const SurfaceSupportQuery &query) const;
  [[nodiscard]] const AuthoredCaveSection *caveSectionAt(Vec3 position,
                                                         CaveInteriorSample *sample = nullptr) const;
  [[nodiscard]] bool mineFocusedOre(std::size_t ore_index);
  [[nodiscard]] bool mineFocusedCaveWeb(std::size_t web_index);
  [[nodiscard]] bool mineFocusedCaveSkitter(std::size_t skitter_index);
  void recordCoalMiningReaction(std::size_t ore_index, const MiningFeedback &feedback,
                                const CoalOreNode &ore);
  [[nodiscard]] bool placeEquippedResource(Vec3 ray_origin, Vec3 ray_direction);
  [[nodiscard]] bool storeMinedResource(const ItemDefinition &definition, int quantity);
  [[nodiscard]] PhysicsBodyHandle addPlacedRockPhysics(const PlacedResourceRock &rock);
  [[nodiscard]] MiningToolStats activePickaxeStats() const;
  void spawnMiningFractureEffect(Vec3 center, Vec3 normal, Vec3 half_extents,
                                 const Material &material, std::uint32_t seed, int shard_count);
  void collectOverlaps();
  void resolveSentinelImpacts();
  [[nodiscard]] float playerSupportExtent() const;
  [[nodiscard]] Vec3 avatarPosePosition() const;
  [[nodiscard]] Vec3 avatarPosePosition(Vec3 player_position) const;

  LumenTuning tuning_{};
  LumenAuthoringData authoring_{};
  LumenStatus status_{};
  WorldState world_state_{};
  PerceptualWorldRuntime perceptual_runtime_{};
  LumenWorldForensics world_forensics_{};
  std::uint64_t next_world_epoch_ = 1u;
  Scene scene_{};
  TerrainHeightField terrain_{};
  SupportSurfaceSet support_surfaces_{};
  Vec3 player_position_{0.0f, 0.28f, 0.0f};
  Vec3 player_velocity_{};
  AvatarRig player_avatar_{};
  AvatarInstance player_avatar_instance_{};
  AvatarAnimatorState player_avatar_animator_{};
  AvatarPose player_avatar_pose_{};
  float player_facing_yaw_ = 0.0f;
  float player_preview_yaw_ = 0.0f;
  bool player_preview_yaw_enabled_ = false;
  bool player_avatar_visible_ = true;
  float player_avatar_support_extent_ = 0.0f;
  Vec3 player_render_position_{0.0f, 0.28f, 0.0f};
  bool player_avatar_pose_valid_ = false;
  bool player_render_position_valid_ = false;
  Vec3 player_avatar_point_target_{};
  bool player_avatar_point_enabled_ = false;
  float player_mouth_open_ = 0.0f;
  bool player_climbing_ = false;
  float player_climb_blend_ = 0.0f;
  std::vector<Shard> shards_;
  std::vector<Sentinel> sentinels_;
  std::vector<std::size_t> rim_objects_;
  std::vector<std::size_t> scenery_objects_;
  ItemRegistry item_registry_{};
  InventoryContainer chest_inventory_{3u};
  Hotbar hotbar_{6u};
  EquipmentSystem equipment_{};
  InteractionSystem interaction_{};
  ScalarAnimation chest_lid_animation_{};
  DynamicPointLight equipped_light_{};
  ParticleEmitter torch_flame_{8u};
  Vec3 prism_relay_base_{};
  bool prism_relay_active_ = false;
  float prism_relay_charge_ = 0.0f;
  std::size_t prism_relay_core_object_ = 0;
  bool prism_relay_core_valid_ = false;
  std::vector<std::size_t> prism_relay_ring_objects_;
  std::vector<std::size_t> prism_relay_conduit_objects_;
  std::vector<std::size_t> prism_relay_node_objects_;
  Vec3 chest_base_{};
  float chest_yaw_ = 0.0f;
  Vec3 supply_crate_base_{};
  float supply_crate_yaw_ = 0.0f;
  bool chest_open_ = false;
  bool chest_distance_close_armed_ = false;
  std::size_t chest_selected_slot_ = 0;
  std::size_t chest_lid_object_ = 0;
  std::size_t chest_lid_band_object_ = 0;
  std::size_t chest_lock_object_ = 0;
  std::vector<ChestItemVisual> chest_items_;
  std::vector<EquippedItemPart> equipped_item_parts_;
  std::vector<PlacedResourceRock> placed_rocks_;
  std::vector<StaticSceneryBox> scenery_collision_boxes_;
  std::vector<TorchParticleVisual> torch_particle_visuals_;
  std::vector<MiningFractureShardVisual> mining_fracture_shards_;
  std::vector<CoalOreNode> coal_ores_;
  std::vector<CaveWebObstacle> cave_webs_;
  std::vector<CaveSkitter> cave_skitters_;
  std::vector<AuthoredCaveSection> cave_sections_;
  WorldMechanismSystem classic_gauntlet_mechanisms_;
  ClassicActorRuntime classic_gauntlet_actors_;
  AutomapModel classic_gauntlet_automap_;
  TransitionWipe classic_gauntlet_wipe_;
  ClassicHudSignalModel classic_gauntlet_hud_;
  Vec3 classic_gauntlet_entry_{};
  Vec3 classic_gauntlet_plate_{};
  Vec3 classic_gauntlet_door_center_{};
  Vec3 classic_gauntlet_door_side_{1.0f, 0.0f, 0.0f};
  Vec3 classic_gauntlet_lift_base_{};
  Vec3 classic_gauntlet_exit_{};
  std::vector<std::size_t> classic_gauntlet_door_objects_;
  std::vector<std::size_t> classic_gauntlet_signal_objects_;
  std::vector<ClassicGauntletActorVisual> classic_gauntlet_actor_visuals_;
  std::size_t classic_gauntlet_lift_object_ = 0;
  std::size_t classic_gauntlet_plate_object_ = 0;
  std::size_t classic_gauntlet_map_object_ = 0;
  bool classic_gauntlet_active_ = false;
  bool classic_gauntlet_discovered_ = false;
  bool classic_gauntlet_wipe_started_ = false;
  float classic_gauntlet_hurt_seconds_ = 0.0f;
  Vec3 cave_entrance_light_position_{};
  std::vector<std::shared_ptr<const CpuMesh>> cave_collision_meshes_;
  std::vector<CaveFloorSupportSurface> cave_floor_supports_;
  std::vector<std::size_t> cave_exterior_hidden_objects_;
  ViewerCullVolume cave_viewer_cull_volume_{};
  bool cave_debug_overlay_enabled_ = false;
  std::uint32_t cave_debug_overlay_layer_mask_ = 0u;
  std::vector<CaveDebugOverlayObject> cave_debug_overlay_objects_;
  std::size_t focused_cave_web_index_ = 0;
  bool focused_cave_web_valid_ = false;
  MiningState mining_;
  std::uint64_t placed_resource_serial_ = 1u;
  PhysicsWorld physics_;
  mutable SceneCoherenceReport scene_coherence_;
  mutable SceneTraceValidationReport scene_trace_;
  mutable bool scene_coherence_dirty_ = true;
  mutable bool scene_trace_dirty_ = true;
  PhysicsBodyHandle player_body_{};
  PhysicsFluidHandle pond_fluid_{};
  PhysicsFluidHandle inner_pond_fluid_{};
  Vec3 pond_center_{5.58f, 0.025f, 3.52f};
  Vec3 inner_pond_center_{8.60f, 0.10f, -23.65f};
  Vec2 inner_pond_radius_{7.20f, 2.35f};
  Vec3 pond_accent_light_position_{};
  bool pond_accent_light_valid_ = false;
  Vec3 fishing_rod_base_{3.12f, 0.34f, 2.60f};
  Vec3 fishing_rod_tip_{4.86f, 1.08f, 3.50f};
  Vec3 fishing_float_rest_{5.62f, 0.435f, 3.58f};
  float fishing_line_radius_ = 0.006f;
  std::vector<std::size_t> fishing_line_objects_;
  std::vector<AquaticCreature> fish_;
  std::vector<CastleBird> castle_birds_;
  Vec3 castle_bird_flight_center_{0.0f, 3.65f, -15.80f};
  Vec3 castle_bird_flight_half_extents_{6.4f, 1.75f, 7.2f};
  Vec3 castle_bird_nest_position_{-3.80f, 3.60f, -18.10f};
  std::size_t fishing_float_object_ = 0;
  std::size_t pond_water_object_ = 0;
  std::size_t inner_pond_water_object_ = 0;
  ClimbableCylinder climbable_tree_{};
  std::vector<std::size_t> crocodile_objects_;
  AmphibiousPredatorState crocodile_state_{};
  float crocodile_swim_blend_ = 1.0f;
  std::vector<SceneParticle> blood_particles_;
  std::size_t blood_particle_cursor_ = 0;
  std::vector<std::size_t> x_eye_objects_;
  std::size_t left_eye_object_ = 0;
  std::size_t right_eye_object_ = 0;
  bool eye_objects_valid_ = false;
  DeathSequenceState death_state_ = DeathSequenceState::Alive;
  float death_timer_ = 0.0f;
  Vec3 death_origin_{};
  bool pending_defeat_ = false;
  int forced_spawn_lighting_frames_ = 0;
  bool player_grounded_ = false;
  bool player_swimming_ = false;
  float player_swim_blend_ = 0.0f;
  float invulnerability_ = 0.0f;
};

} // namespace aster
