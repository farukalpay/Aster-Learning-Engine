#pragma once

#include "aster/core/work_budget.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace aster {

enum class VoxelCaveMaterial {
  Air,
  Rock,
  Coal,
  Copper,
  Iron,
};

struct VoxelChunkCoord {
  int x = 0;
  int y = 0;
  int z = 0;

  [[nodiscard]] friend bool operator==(const VoxelChunkCoord lhs, const VoxelChunkCoord rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
  }
};

struct VoxelChunkCoordHash {
  [[nodiscard]] std::size_t operator()(const VoxelChunkCoord coord) const noexcept {
    auto mix = [](std::uint64_t value) {
      value ^= value >> 33u;
      value *= 0xff51afd7ed558ccdull;
      value ^= value >> 33u;
      value *= 0xc4ceb9fe1a85ec53ull;
      value ^= value >> 33u;
      return value;
    };
    std::uint64_t hash = 0x9e3779b97f4a7c15ull;
    hash ^= mix(static_cast<std::uint32_t>(coord.x)) + 0x9e3779b97f4a7c15ull + (hash << 6u) +
            (hash >> 2u);
    hash ^= mix(static_cast<std::uint32_t>(coord.y)) + 0x9e3779b97f4a7c15ull + (hash << 6u) +
            (hash >> 2u);
    hash ^= mix(static_cast<std::uint32_t>(coord.z)) + 0x9e3779b97f4a7c15ull + (hash << 6u) +
            (hash >> 2u);
    return static_cast<std::size_t>(hash);
  }
};

struct VoxelCellCoord {
  int x = 0;
  int y = 0;
  int z = 0;

  [[nodiscard]] friend bool operator==(const VoxelCellCoord lhs, const VoxelCellCoord rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
  }
};

struct VoxelCaveChamber {
  Vec3 center{};
  Vec3 radius{2.4f, 1.7f, 2.4f};
};

struct VoxelCaveTunnel {
  Vec3 from{};
  Vec3 to{};
  float radius = 1.05f;
};

struct VoxelCaveSurfaceOpening {
  Vec3 from{};
  Vec3 to{};
  float radius = 1.05f;
  float surface_margin = 0.25f;
};

struct VoxelCaveProceduralField {
  bool enabled = false;
  std::uint32_t seed = 1u;
  Vec3 start{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float backtrack_distance = 0.0f;
  float tunnel_radius = 1.25f;
  float vertical_radius = 1.05f;
  float radius_variation = 0.16f;
  float wander_frequency = 0.045f;
  float side_wander = 0.85f;
  float vertical_wander = 0.32f;
  float macro_wander_frequency = 0.0f;
  float macro_side_wander = 0.0f;
  float macro_vertical_wander = 0.0f;
  int path_sample_steps = 1;
  float path_sample_spacing = 0.0f;
  float frame_derivative_step = 0.0f;
  float chamber_spacing = 18.0f;
  float chamber_radius = 2.75f;
  float chamber_vertical_radius = 1.65f;
  float chamber_radius_variation = 0.30f;
  float chamber_jitter = 0.35f;
};

struct VoxelCaveSolidPlug {
  bool enabled = true;
  std::uint32_t seed = 1u;
  Vec3 center{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  VoxelCaveMaterial material = VoxelCaveMaterial::Iron;
  float radius = 1.40f;
  float vertical_radius = 1.05f;
  float half_length = 2.80f;
  float edge_feather = 0.18f;
  float surface_noise = 0.08f;
};

struct VoxelCaveProceduralSolidPlugField {
  bool enabled = false;
  std::uint32_t seed = 1u;
  int path_field_index = -1;
  Vec3 start{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  VoxelCaveMaterial material = VoxelCaveMaterial::Iron;
  float first_distance = 14.0f;
  float spacing = 22.0f;
  float radius = 1.45f;
  float vertical_radius = 1.10f;
  float radius_scale = 1.18f;
  float vertical_radius_scale = 1.08f;
  float half_length = 3.60f;
  float edge_feather = 0.18f;
  float surface_noise = 0.08f;
  float radius_variation = 0.08f;
  float length_variation = 0.12f;
  float lateral_jitter = 0.18f;
  float vertical_jitter = 0.08f;
};

struct VoxelCaveProceduralFrame {
  bool valid = false;
  int procedural_field_index = -1;
  Vec3 center{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 side{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float distance = 0.0f;
  float tunnel_radius = 0.0f;
  float vertical_radius = 0.0f;
};

struct VoxelCaveInteriorProbe {
  float density_feather_cells = 1.5f;
  float entrance_light_distance = 8.0f;
  float depth_reference_distance = 72.0f;
};

struct VoxelCaveInteriorSample {
  bool valid = false;
  bool inside = false;
  bool procedural = false;
  int procedural_field_index = -1;
  float density = 0.0f;
  float interior = 0.0f;
  float entrance_light = 0.0f;
  float depth = 0.0f;
  float chamber = 0.0f;
  float progress_distance = 0.0f;
  float progress_normalized = 0.0f;
  float procedural_distance = 0.0f;
  Vec3 center{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 side{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float lateral = 0.0f;
  float vertical = 0.0f;
  float radial_fraction = 0.0f;
  float tunnel_radius = 0.0f;
  float vertical_radius = 0.0f;
};

enum class VoxelCaveFixtureSideMode {
  FixedNegative,
  FixedPositive,
  Alternating,
};

struct VoxelCaveFixtureProfile {
  float start_distance = 4.0f;
  float end_distance = 0.0f;
  float target_spacing = 5.8f;
  int max_count = 8;
  int procedural_behind = 1;
  int procedural_ahead = 2;
  VoxelCaveFixtureSideMode side_mode = VoxelCaveFixtureSideMode::Alternating;
  float wall_inset = 0.14f;
  float mount_height = 0.10f;
  float normal_up_bias = -0.10f;
  float lens_offset = 0.075f;
  float light_offset = 0.18f;
};

struct VoxelCaveFixtureLightBand {
  float start_distance = 0.0f;
  Vec3 color{1.0f, 0.16f, 0.08f};
};

struct VoxelCaveFixturePlacement {
  Vec3 position{};
  Vec3 mount_position{};
  Vec3 lens_position{};
  Vec3 light_position{};
  Vec3 light_color{1.0f, 0.16f, 0.08f};
  Vec3 normal{0.0f, 0.0f, 1.0f};
  Vec3 tangent{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float progress_distance = 0.0f;
  float progress_normalized = 0.0f;
  bool procedural = false;
  float procedural_distance = 0.0f;
  int station_index = 0;
};

struct VoxelCaveFixtureLightProbe {
  float progress_window = 5.8f;
  float minimum_progress_gain = 0.70f;
  float distance_score_weight = 0.55f;
  float progress_score_weight = 1.30f;
};

struct VoxelCaveFixtureLightSample {
  VoxelCaveFixturePlacement placement{};
  float distance = 0.0f;
  float progress_delta = 0.0f;
  float weight = 0.0f;
  float score = 0.0f;
};

enum class VoxelEditOperation {
  Carve,
};

struct VoxelEdit {
  Vec3 center{};
  float radius = 0.55f;
  VoxelEditOperation operation = VoxelEditOperation::Carve;
};

enum class VoxelEditRebuildMode {
  Deferred,
  ImmediateAffected,
};

struct VoxelEditResult {
  bool accepted = false;
  std::vector<VoxelChunkCoord> affected_chunks;
  int rebuilt_chunks = 0;
};

struct VoxelProgressRange {
  bool enabled = false;
  float start_distance = 0.0f;
  float end_distance = 0.0f;
  float feather_distance = 0.0f;
};

struct VoxelMaterialProfile {
  VoxelCaveMaterial material = VoxelCaveMaterial::Coal;
  std::uint32_t seed = 1u;
  std::string display_name;
  std::string visual_material_id;
  int min_tool_tier = 0;
  float hardness = 1.0f;
  std::string resource_item_id;
  int resource_quantity = 1;
  float vein_frequency = 0.18f;
  float vein_radius = 0.18f;
  float rarity = 0.74f;
  float warp_frequency = 0.11f;
  float warp_strength = 0.85f;
  float surface_relief = 0.0f;
  float surface_exposure_threshold = 0.0f;
  float surface_coverage = 0.62f;
  VoxelProgressRange progress_range{};
};

enum class VoxelCaveStructuralSurfaceMode {
  RenderAndCollide,
  CollisionOnly,
  Off,
};

struct VoxelCaveSpec {
  std::uint32_t seed = 1u;
  Vec3 origin{};
  float cell_size = 0.40f;
  int chunk_cells = 20;
  int coarse_proxy_cells = 4;
  int stream_radius = 2;
  int unload_radius = 4;
  int max_chunk_rebuilds_per_update = 0;
  int forced_rebuild_radius = 0;
  int visibility_probe_radius = 1;
  float predictive_lookahead_seconds = 0.45f;
  int path_prefetch_ahead_chunks = 3;
  int path_prefetch_behind_chunks = 1;
  int path_prefetch_radius = 0;
  float path_prefetch_spacing_chunks = 0.85f;
  float chunk_transition_seconds = 0.28f;
  VoxelCaveStructuralSurfaceMode structural_surface_mode =
      VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  Vec3 authored_fixture_light_color{1.0f, 0.16f, 0.08f};
  Vec3 procedural_fixture_light_color{1.0f, 0.16f, 0.08f};
  std::vector<VoxelCaveFixtureLightBand> procedural_fixture_light_bands;
  std::vector<VoxelCaveChamber> chambers;
  std::vector<VoxelCaveTunnel> tunnels;
  std::vector<VoxelCaveSurfaceOpening> surface_openings;
  std::vector<VoxelCaveProceduralField> procedural_fields;
  std::vector<VoxelCaveSolidPlug> solid_plugs;
  std::vector<VoxelCaveProceduralSolidPlugField> procedural_solid_plug_fields;
  std::vector<VoxelMaterialProfile> material_profiles;
};

struct VoxelCaveRebuildPriorityWeights {
  double missing_collision = 12.0;
  double missing_render = 8.0;
  double edit_overlap = 6.0;
  double viewer_distance = 3.0;
  double velocity_alignment = 2.0;
  double dirty_age = 1.0;
};

struct VoxelCaveUpdateOptions {
  bool override_rebuild_budget = false;
  FrameWorkBudget rebuild_budget{};
  FrameBudgetControllerInput budget_feedback{};
  Vec3 viewer_velocity{};
  std::vector<Vec3> visibility_probes;
  VoxelCaveRebuildPriorityWeights priority_weights{};
};

struct VoxelCaveUpdateStats {
  std::uint32_t active_chunks = 0u;
  std::uint32_t dirty_chunks = 0u;
  std::uint32_t queued_rebuilds = 0u;
  std::uint32_t selected_rebuilds = 0u;
  std::uint32_t forced_rebuilds = 0u;
  std::uint32_t rebuilt_chunks = 0u;
  std::uint32_t pending_chunks = 0u;
  std::uint32_t published_chunks = 0u;
  std::uint32_t coarse_visible_chunks = 0u;
  std::uint32_t full_published_chunks = 0u;
  std::uint32_t visibility_probe_chunks = 0u;
  std::uint32_t changed_snapshots = 0u;
  std::uint32_t expired_chunks = 0u;
  BudgetedWorkDiagnostics rebuild_queue{};
  BudgetTelemetry budget_telemetry{};
};

struct VoxelCaveHit {
  bool hit = false;
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float distance = 0.0f;
  VoxelCaveMaterial material = VoxelCaveMaterial::Air;
  VoxelChunkCoord chunk{};
  VoxelCellCoord cell{};
};

enum class VoxelChunkRenderBatchKind {
  StructuralSurface,
  MaterialSurface,
  CoarseProxySurface,
};

enum class VoxelChunkLifecycle {
  Requested,
  CoarseVisible,
  FullBuilding,
  FullPublished,
  Retiring,
};

struct VoxelMaterialSurfaceStats {
  VoxelCaveMaterial material = VoxelCaveMaterial::Rock;
  std::uint32_t vertices = 0u;
  std::uint32_t triangles = 0u;
};

struct VoxelChunkSurfaceStats {
  std::uint32_t active_cells = 0u;
  std::uint32_t surface_vertices = 0u;
  std::uint32_t surface_triangles = 0u;
  std::uint32_t material_batches = 0u;
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  std::vector<VoxelMaterialSurfaceStats> materials;
};

struct VoxelChunkRenderBatch {
  VoxelChunkCoord coord{};
  VoxelChunkRenderBatchKind kind = VoxelChunkRenderBatchKind::StructuralSurface;
  VoxelCaveMaterial material = VoxelCaveMaterial::Rock;
  std::string label;
  float opacity = 1.0f;
  std::shared_ptr<const CpuMesh> mesh{};
};

struct VoxelChunkSnapshot {
  VoxelChunkCoord coord{};
  Vec3 center{};
  Vec3 bounds_center{};
  float bounds_radius = 0.0f;
  float reveal_age = 0.0f;
  bool newly_revealed = false;
  bool rebuild_pending = false;
  bool collision_dirty = false;
  bool coarse_proxy = false;
  VoxelChunkLifecycle lifecycle = VoxelChunkLifecycle::Requested;
  std::vector<VoxelChunkRenderBatch> batches;
  std::shared_ptr<const CpuMesh> collision_mesh{};
  VoxelChunkSurfaceStats surface_stats{};
  std::uint64_t mesh_generation = 0u;
};

class VoxelCaveState {
public:
  void configure(VoxelCaveSpec spec);
  void clear();
  void updateStreaming(Vec3 viewer_position, float dt);
  void updateStreaming(Vec3 viewer_position, float dt, const VoxelCaveUpdateOptions &options);
  [[nodiscard]] VoxelEditResult
  applyEdit(const VoxelEdit &edit,
            VoxelEditRebuildMode rebuild_mode = VoxelEditRebuildMode::Deferred);

  [[nodiscard]] const VoxelCaveSpec &spec() const;
  [[nodiscard]] const std::vector<VoxelEdit> &edits() const;
  void setEdits(std::vector<VoxelEdit> edits);
  [[nodiscard]] const std::vector<VoxelChunkSnapshot> &activeChunks() const;
  [[nodiscard]] const std::vector<VoxelChunkSnapshot> &changedChunks() const;
  [[nodiscard]] const VoxelCaveUpdateStats &lastUpdateStats() const;
  [[nodiscard]] std::optional<VoxelChunkSnapshot> consumeDirtyChunk(VoxelChunkCoord coord);
  [[nodiscard]] VoxelCaveHit raycast(Vec3 origin, Vec3 direction, float max_distance) const;
  [[nodiscard]] VoxelCaveInteriorSample sampleInterior(Vec3 position,
                                                       VoxelCaveInteriorProbe probe = {}) const;
  [[nodiscard]] float densityAt(Vec3 position) const;
  [[nodiscard]] VoxelCaveMaterial materialAt(Vec3 position) const;
  [[nodiscard]] const VoxelMaterialProfile *profileFor(VoxelCaveMaterial material) const;
  [[nodiscard]] VoxelChunkCoord chunkCoordFor(Vec3 position) const;
  [[nodiscard]] VoxelCellCoord cellCoordFor(Vec3 position) const;

private:
  struct ChunkState {
    VoxelChunkCoord coord{};
    Vec3 center{};
    float reveal_age = 0.0f;
    bool newly_revealed = false;
    bool dirty = true;
    bool collision_dirty = true;
    bool coarse_proxy = false;
    VoxelChunkLifecycle lifecycle = VoxelChunkLifecycle::Requested;
    float dirty_age = 0.0f;
    std::uint32_t dirty_frames = 0u;
    std::uint32_t edit_overlaps = 0u;
    float render_transition_age = 0.0f;
    float render_transition_seconds = 0.0f;
    std::vector<VoxelChunkRenderBatch> batches;
    std::vector<VoxelChunkRenderBatch> retiring_batches;
    std::shared_ptr<const CpuMesh> collision_mesh{};
    VoxelChunkSurfaceStats surface_stats{};
    std::uint64_t mesh_generation = 0u;
    std::uint64_t published_generation = 0u;
  };

  void ensureChunk(VoxelChunkCoord coord);
  void rebuildCoarseProxy(ChunkState &chunk);
  void rebuildChunk(ChunkState &chunk);
  void rebuildChunkLookup();
  [[nodiscard]] ChunkState *findChunk(VoxelChunkCoord coord);
  [[nodiscard]] const ChunkState *findChunk(VoxelChunkCoord coord) const;
  [[nodiscard]] VoxelChunkSnapshot snapshotFor(const ChunkState &chunk,
                                               bool collision_dirty) const;
  [[nodiscard]] Vec3 chunkCenter(VoxelChunkCoord coord) const;
  [[nodiscard]] int chunkDistance(VoxelChunkCoord lhs, VoxelChunkCoord rhs) const;
  [[nodiscard]] bool chunkIntersectsEdit(VoxelChunkCoord coord, const VoxelEdit &edit) const;

  VoxelCaveSpec spec_{};
  std::vector<ChunkState> chunks_;
  std::unordered_map<VoxelChunkCoord, std::size_t, VoxelChunkCoordHash> chunk_lookup_;
  std::vector<VoxelEdit> edits_;
  std::vector<VoxelChunkSnapshot> snapshots_;
  std::vector<VoxelChunkSnapshot> changed_snapshots_;
  VoxelCaveUpdateStats last_update_stats_{};
  WorkCostModel rebuild_costs_;
  FrameBudgetController rebuild_budget_controller_{};
};

[[nodiscard]] VoxelCaveProceduralFrame
sampleVoxelCaveProceduralFrameAt(const VoxelCaveProceduralField &field, float distance);
[[nodiscard]] VoxelCaveProceduralFrame
closestVoxelCaveProceduralFrame(const VoxelCaveProceduralField &field, Vec3 position);
[[nodiscard]] VoxelCaveInteriorSample sampleVoxelCaveInterior(const VoxelCaveSpec &spec,
                                                              Vec3 position, float density,
                                                              VoxelCaveInteriorProbe probe = {});
[[nodiscard]] FrameWorkBudget defaultVoxelCaveInteractiveRebuildBudget();
[[nodiscard]] FrameWorkBudget voxelCaveRebuildItemBudget(std::uint32_t max_items);
[[nodiscard]] std::vector<VoxelCaveFixturePlacement>
placeVoxelCavePathFixtures(const VoxelCaveSpec &spec, VoxelCaveFixtureProfile fixture = {});
[[nodiscard]] std::vector<VoxelCaveFixturePlacement>
placeVoxelCaveProceduralFixturesNear(const VoxelCaveSpec &spec, Vec3 viewer,
                                     VoxelCaveFixtureProfile fixture = {});
[[nodiscard]] std::vector<VoxelCaveFixtureLightSample>
rankVoxelCaveFixtureLights(Vec3 viewer, const VoxelCaveInteriorSample &interior,
                           const std::vector<VoxelCaveFixturePlacement> &fixtures,
                           VoxelCaveFixtureLightProbe probe = {});

} // namespace aster
