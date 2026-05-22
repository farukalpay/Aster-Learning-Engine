// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster::sdk {

constexpr std::uint32_t kAuthoringSchemaVersion = 1u;

using AssetId = std::string;
using EntityId = std::string;

enum class DiagnosticSeverity {
  Warning,
  Error,
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  std::filesystem::path source;
  std::string path;
  std::string message;
};

template <typename T> struct LoadResult {
  T value{};
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const {
    for (const Diagnostic &diagnostic : diagnostics) {
      if (diagnostic.severity == DiagnosticSeverity::Error) {
        return false;
      }
    }
    return true;
  }
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct LinearRgb {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct EmissionColor {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Transform {
  Vec3 translation{};
  Vec3 rotation{};
  Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct GameplayTag {
  std::string value;
};

enum class AssetKind {
  Unknown,
  Scene,
  Prefab,
  Cave,
  Material,
  Item,
  ActionGraph,
  InputMap,
  Ui,
  Mesh,
  Texture,
  AssetGraph,
};

struct ProjectAssetRef {
  AssetId id;
  AssetKind kind = AssetKind::Unknown;
  std::filesystem::path path;
};

struct ProjectDocument {
  std::uint32_t schema_version = 0u;
  std::string name;
  AssetId startup_scene;
  std::vector<ProjectAssetRef> assets;
};

struct TransformComponent {
  Transform local{};
};

struct MeshRendererComponent {
  AssetId mesh;
  AssetId material;
  bool visible = true;
};

struct PerceptualPlacementBindingComponent {
  AssetId template_ref;
  std::string cell_anchor;
  std::string residency_role;
  std::string local_cause;
  std::string traversal_role;
  std::string visual_occlusion_role;
  std::string acoustic_role;
  float material_half_life_override = 0.0f;
  float streaming_cost_override = 0.0f;
};

enum class ColliderShape {
  Box,
  Sphere,
  Capsule,
  Mesh,
};

struct ColliderComponent {
  ColliderShape shape = ColliderShape::Box;
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  float height = 1.0f;
  bool trigger = false;
  std::string layer;
  std::string mask;
};

enum class LightKind {
  Point,
  Directional,
  Spot,
};

struct LightComponent {
  LightKind kind = LightKind::Point;
  Vec3 color{1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
  float radius = 1.0f;
};

struct InteractableComponent {
  std::vector<GameplayTag> tags;
  AssetId action_graph;
  std::string prompt_action;
  std::string prompt_subject;
  float radius = 0.35f;
  float max_distance = 8.0f;
};

struct InventorySlot {
  AssetId item;
  int quantity = 0;
};

struct InventoryComponent {
  int slot_count = 0;
  std::vector<InventorySlot> slots;
};

struct CameraComponent {
  bool primary = false;
  float vertical_fov_degrees = 60.0f;
  float near_plane = 0.01f;
  float far_plane = 1000.0f;
};

struct CaveSceneComponent {
  AssetId cave;
  std::string section;
};

struct FixtureComponent {
  std::string archetype;
  std::string socket;
};

struct OreNodeComponent {
  AssetId resource_item;
  int health = 1;
  int yield_quantity = 1;
  float radius = 0.25f;
};

struct TorchSocketComponent {
  Vec3 color{1.0f, 0.5f, 0.2f};
  float intensity = 1.0f;
  float radius = 1.0f;
};

struct SpawnPointComponent {
  std::string spawn_kind;
  float radius = 0.35f;
  Vec3 half_extents{0.35f, 0.35f, 0.35f};
};

struct MiningComponent {
  AssetId resource_item;
  int health = 1;
  int yield_quantity = 1;
  float radius = 0.35f;
};

struct CaveDebugComponent {
  std::vector<GameplayTag> layers;
  Vec3 color{1.0f, 1.0f, 1.0f};
};

struct ComponentSet {
  std::optional<TransformComponent> transform;
  std::optional<MeshRendererComponent> mesh_renderer;
  std::optional<PerceptualPlacementBindingComponent> perceptual_binding;
  std::optional<ColliderComponent> collider;
  std::optional<LightComponent> light;
  std::optional<InteractableComponent> interactable;
  std::optional<InventoryComponent> inventory;
  std::optional<CameraComponent> camera;
  std::optional<CaveSceneComponent> cave_scene;
  std::optional<FixtureComponent> fixture;
  std::optional<OreNodeComponent> ore_node;
  std::optional<TorchSocketComponent> torch_socket;
  std::optional<SpawnPointComponent> spawn_point;
  std::optional<MiningComponent> mining;
  std::optional<CaveDebugComponent> cave_debug;
};

struct EntityDefinition {
  EntityId id;
  std::string name;
  EntityId parent;
  ComponentSet components;
};

struct SceneDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  std::vector<EntityDefinition> entities;
};

struct PrefabDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  std::vector<EntityDefinition> entities;
};

struct CaveSeedRecord {
  std::string id;
  std::uint32_t value = 0u;
};

struct CaveTunnelProfileDocument {
  std::uint32_t seed = 1u;
  Vec3 start{};
  Vec3 control{};
  Vec3 control_b{};
  Vec3 end{};
  bool floor_relative = false;
  int length_segments = 64;
  int radial_segments = 18;
  float half_width = 1.05f;
  float wall_height = 1.65f;
  float floor_width = 1.22f;
  float floor_crown = 0.025f;
  float floor_edge_raise = 0.055f;
  float wall_noise = 0.18f;
  float visible_wall_start_t = 0.12f;
  float collision_start_t = 0.10f;
  float collision_end_t = 1.0f;
  float ore_start_t = 0.24f;
  float chest_t = 0.56f;
  float chamber_t = 0.62f;
  float chamber_falloff = 0.22f;
  float chamber_width_scale = 1.55f;
  float chamber_height_scale = 1.22f;
  bool end_constraint_enabled = true;
};

struct CavePortalProfileDocument {
  int arch_segments = 18;
  float inner_half_width = 0.92f;
  float inner_height = 1.30f;
  float outer_half_width = 1.78f;
  float outer_height = 2.20f;
  float depth = 0.86f;
  float ground_lip = 0.16f;
  float inner_lining_depth = 1.25f;
  float lining_breakup = 0.045f;
};

struct CaveOreVeinProfileDocument {
  std::uint32_t seed = 1u;
  int candidates = 72;
  int max_nodes = 14;
  float field_frequency_a = 0.48f;
  float field_frequency_b = 0.91f;
  float intersection_threshold_a = 0.26f;
  float intersection_threshold_b = 0.22f;
  float wall_inset = 0.075f;
  float min_spacing = 1.08f;
};

struct CaveFeatureProfileDocument {
  std::uint32_t seed = 1u;
  int candidates = 96;
  int max_features = 34;
  float start_t = 0.16f;
  float end_t = 0.94f;
  float min_spacing = 0.82f;
  float wall_inset = 0.18f;
  float ceiling_fraction = 0.36f;
  float column_fraction = 0.10f;
  float shelf_fraction = 0.16f;
  float mineral_fraction = 0.18f;
};

struct CaveWallFixtureProfileDocument {
  std::string id;
  float start_t = 0.24f;
  float end_t = 0.58f;
  float target_spacing = 5.5f;
  int max_count = 4;
  float wall_side = 1.0f;
  float mount_height = 1.08f;
  float wall_inset = 0.14f;
  float normal_up_bias = -0.10f;
  float lens_offset = 0.075f;
  float light_offset = 0.18f;
  Vec3 light_color{1.0f, 0.78f, 0.56f};
};

struct CaveSectionDocument {
  std::string id;
  std::string archetype;
  bool terrain_cover_fit = false;
  bool derive_from_previous = false;
  bool contributes_entrance_light = false;
  CaveTunnelProfileDocument tunnel;
  CavePortalProfileDocument portal;
  CaveOreVeinProfileDocument ore;
  CaveFeatureProfileDocument features;
  std::vector<CaveWallFixtureProfileDocument> fixtures;
};

struct CavePlacementDocument {
  std::string id;
  std::string kind;
  std::string archetype;
  AssetId prefab;
  std::string section;
  Vec3 position{};
  Vec3 rotation{};
  Vec3 scale{1.0f, 1.0f, 1.0f};
  bool floor_relative = false;
};

struct CaveRouteValidationDocument {
  std::string id;
  std::vector<Vec3> points;
  float max_segment_length = 1.5f;
  float support_tolerance = 0.25f;
};

struct CaveVolumeValidationDocument {
  std::string id;
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
};

struct CaveCameraValidationDocument {
  std::string id;
  Vec3 camera{};
  Vec3 target{};
  float min_clearance = 0.25f;
};

struct CaveProbeAgentDocument {
  std::string id;
  std::uint32_t seed = 1u;
  int step_count = 16;
  float step_length = 1.25f;
  std::string entry_anchor;
};

struct CaveWorldProbeDocument {
  std::string id;
  std::string kind;
  Vec3 position{};
  float radius = 1.0f;
  int minimum_count = 1;
  float minimum_budget = 0.0f;
  float maximum_budget = 1.0f;
};

struct CavePerceptualBudgetDocument {
  std::string id;
  float minimum_salience = 0.50f;
};

struct CaveReactionPackageDocument {
  std::string id;
  AssetId action;
  float minimum_score = 0.0f;
  std::vector<std::string> required_channels;
};

struct CavePerceptualContinuityBudgetDocument {
  std::string id;
  float minimum_score = 0.50f;
  std::vector<std::string> required_channels;
  std::vector<CaveReactionPackageDocument> reaction_packages;
};

struct CavePerceptionLedgerCellDocument {
  std::string id;
  float minimum_score = 0.0f;
  std::vector<std::string> required_channels;
};

struct CavePerceptionLedgerDocument {
  std::string id;
  float minimum_score = 0.50f;
  std::vector<std::string> required_channels;
  std::vector<CavePerceptionLedgerCellDocument> cells;
};

struct CavePerceptualRuntimeDocument {
  std::string id;
  float exposure_horizon_seconds = 47.0f;
  float minimum_continuity_score = 0.62f;
  float minimum_occlusion_trust = 0.45f;
  float minimum_lighting_believability = 0.45f;
  float minimum_player_readable_cause = 0.45f;
};

struct CavePerceptualCausalityGraphDocument {
  std::string id;
  float minimum_decision_impact = 0.50f;
  std::vector<std::string> required_causal_edges;
  std::vector<std::string> required_decision_channels;
  std::vector<std::string> required_player_readable_causes;
};

struct CaveBeliefContractDocument {
  std::string id;
  float minimum_score = 0.70f;
  std::vector<std::string> required_checks;
};

struct CavePerceptualWorldSchedulerReport {
  bool accepted = false;
  std::string scheduler_hash;
  std::string memory_residue_hash;
  std::string threat_signal_hash;
  std::string material_age_hash;
  std::string interaction_debt_hash;
  std::string perceptual_priority_hash;
  std::string streaming_budget_hash;
  float memory_residue = 0.0f;
  float threat_signal = 0.0f;
  float material_age = 0.0f;
  float interaction_debt = 0.0f;
  float perceptual_priority = 0.0f;
  float streaming_budget = 0.0f;
  float belief_stability = 0.0f;
  float decision_impact_score = 0.0f;
  std::string diagnostic;
};

struct CaveBeliefFindingReport {
  std::string kind;
  std::string severity;
  std::string subject;
  float score = 0.0f;
  float threshold = 0.0f;
  std::string evidence_hash;
  std::string source;
  std::string message;
};

struct CaveBeliefFalsenessReport {
  bool accepted = false;
  float score = 0.0f;
  float minimum_score = 0.70f;
  std::string world_transition_hash;
  std::string extraction_hash;
  std::string belief_contract_hash;
  std::string readability_audit_hash;
  std::string perceptual_scheduler_hash;
  float decision_impact_score = 0.0f;
  std::vector<CaveBeliefFindingReport> findings;
};

struct CaveWorldGateReportDocument {
  std::uint32_t schema_version = 0u;
  std::string kind;
  AssetId id;
  std::string verdict;
  std::string region_id;
  std::string world_transition_hash;
  std::string extraction_hash;
  std::string belief_contract_hash;
  bool navigation_valid = false;
  bool perceptual_runtime_accepted = false;
  CavePerceptualWorldSchedulerReport perceptual_world_scheduler;
  CaveBeliefFalsenessReport falseness_report;
};

struct CaveValidationDocument {
  std::vector<CaveRouteValidationDocument> walkable_routes;
  std::vector<CaveVolumeValidationDocument> spawn_volumes;
  std::vector<CaveVolumeValidationDocument> collision_volumes;
  std::vector<CaveCameraValidationDocument> camera_probes;
  std::optional<CaveProbeAgentDocument> probe_agent;
  std::vector<CaveWorldProbeDocument> resource_probes;
  std::vector<CaveWorldProbeDocument> encounter_probes;
  std::optional<CavePerceptualBudgetDocument> perceptual_budget;
  std::optional<CavePerceptualContinuityBudgetDocument> perceptual_continuity_budget;
  std::optional<CavePerceptionLedgerDocument> perception_ledger;
  std::optional<CavePerceptualRuntimeDocument> perceptual_runtime;
  std::optional<CavePerceptualCausalityGraphDocument> perceptual_causality_graph;
  std::optional<CaveBeliefContractDocument> belief_contract;
};

struct CaveDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  std::vector<CaveSeedRecord> seeds;
  std::vector<AssetId> required_assets;
  std::vector<CaveSectionDocument> sections;
  std::vector<CavePlacementDocument> placements;
  CaveValidationDocument validation;
};

struct MaterialDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  LinearRgb base_color{1.0f, 1.0f, 1.0f};
  EmissionColor emission_color{};
  float roughness = 0.55f;
  float metallic = 0.0f;
  float emission_strength = 0.0f;
  float opacity = 1.0f;
  bool double_sided = false;
  std::string alpha_mode = "opaque";
  std::string depth_write = "auto";
  std::string surface_pattern = "none";
  std::map<std::string, std::filesystem::path> texture_slots;
  std::map<std::string, std::string> compiler_hints;
  std::vector<GameplayTag> tags;
};

struct ItemDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string display_name;
  std::string short_label;
  bool stackable = false;
  int max_stack = 1;
  AssetId icon;
  AssetId pickup_prefab;
  AssetId use_action_graph;
  std::vector<std::string> equipment_slots;
  std::vector<GameplayTag> tags;
  std::map<std::string, std::string> metadata;
};

struct ActionNode {
  std::string id;
  std::string type;
  std::map<std::string, std::string> parameters;
  std::vector<GameplayTag> tags;
};

struct ActionReactionContractDocument {
  std::string id;
  float minimum_score = 0.0f;
  std::vector<std::string> required_channels;
  std::vector<std::string> required_events;
};

struct ActionGraphDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  std::vector<ActionNode> nodes;
  std::vector<ActionReactionContractDocument> reaction_contracts;
};

struct InputBindingDocument {
  std::string command;
  std::string device;
  std::string key;
  std::string button;
  float scale = 1.0f;
  float deadzone = 0.0f;
  std::vector<GameplayTag> tags;
};

struct InputMapDocument {
  std::uint32_t schema_version = 0u;
  AssetId id;
  std::string name;
  std::vector<InputBindingDocument> bindings;
};

struct EntityInstance {
  EntityDefinition definition;
  AssetId source_asset;
};

struct InstantiateResult {
  std::size_t created_entities = 0u;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const {
    for (const Diagnostic &diagnostic : diagnostics) {
      if (diagnostic.severity == DiagnosticSeverity::Error) {
        return false;
      }
    }
    return true;
  }
};

class World {
public:
  void clear();
  [[nodiscard]] InstantiateResult instantiate(const SceneDocument &scene);
  [[nodiscard]] InstantiateResult instantiate(const PrefabDocument &prefab);

  [[nodiscard]] const EntityInstance *findEntity(std::string_view id) const;
  [[nodiscard]] const std::vector<EntityInstance> &entities() const;

private:
  [[nodiscard]] InstantiateResult instantiateEntities(const AssetId &source_asset,
                                                      const std::vector<EntityDefinition> &entities);

  std::vector<EntityInstance> entities_;
};

struct ActionContext {
  EntityId actor;
  EntityId target;
  std::string input;
};

struct ActionEvent {
  std::string node_id;
  std::string type;
  EntityId actor;
  EntityId target;
  std::map<std::string, std::string> parameters;
  std::vector<GameplayTag> tags;
  std::uint64_t deterministic_stamp = 0u;
};

struct ActionExecution {
  std::vector<ActionEvent> events;
  std::vector<Diagnostic> diagnostics;
  std::uint64_t contract_stamp = 0u;

  [[nodiscard]] bool ok() const {
    for (const Diagnostic &diagnostic : diagnostics) {
      if (diagnostic.severity == DiagnosticSeverity::Error) {
        return false;
      }
    }
    return true;
  }
};

class ActionGraphRuntime {
public:
  [[nodiscard]] ActionExecution execute(const ActionGraphDocument &graph,
                                        const ActionContext &context) const;
};

[[nodiscard]] AssetKind parseAssetKind(std::string_view value);
[[nodiscard]] std::string_view assetKindName(AssetKind kind);

[[nodiscard]] LoadResult<ProjectDocument> parseProjectDocument(std::string_view source,
                                                               std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<SceneDocument> parseSceneDocument(std::string_view source,
                                                           std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<PrefabDocument> parsePrefabDocument(std::string_view source,
                                                             std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<CaveDocument> parseCaveDocument(std::string_view source,
                                                         std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<MaterialDocument>
parseMaterialDocument(std::string_view source, std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<ItemDocument> parseItemDocument(std::string_view source,
                                                         std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<ActionGraphDocument>
parseActionGraphDocument(std::string_view source, std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<InputMapDocument>
parseInputMapDocument(std::string_view source, std::filesystem::path source_path = {});
[[nodiscard]] LoadResult<CaveWorldGateReportDocument>
parseCaveWorldGateReportDocument(std::string_view source,
                                 std::filesystem::path source_path = {});

[[nodiscard]] LoadResult<ProjectDocument> loadProjectDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<SceneDocument> loadSceneDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<PrefabDocument> loadPrefabDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<CaveDocument> loadCaveDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<MaterialDocument> loadMaterialDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<ItemDocument> loadItemDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<ActionGraphDocument>
loadActionGraphDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<InputMapDocument> loadInputMapDocument(const std::filesystem::path &path);
[[nodiscard]] LoadResult<CaveWorldGateReportDocument>
loadCaveWorldGateReportDocument(const std::filesystem::path &path);

[[nodiscard]] std::uint64_t actionGraphContractStamp(const ActionGraphDocument &graph);
[[nodiscard]] std::uint64_t inputMapContractStamp(const InputMapDocument &input_map);

[[nodiscard]] std::vector<Diagnostic>
validateCaveDocument(const CaveDocument &cave, const ProjectDocument *project = nullptr,
                     const SceneDocument *scene = nullptr,
                     std::filesystem::path source_path = {});

} // namespace aster::sdk

#include "aster/game_sdk/agent_authoring.hpp"
