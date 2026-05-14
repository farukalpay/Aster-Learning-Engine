#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
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
  float chamber_spacing = 18.0f;
  float chamber_radius = 2.75f;
  float chamber_vertical_radius = 1.65f;
  float chamber_radius_variation = 0.30f;
  float chamber_jitter = 0.35f;
};

struct VoxelCaveProceduralFrame {
  bool valid = false;
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
  float density = 0.0f;
  float interior = 0.0f;
  float entrance_light = 0.0f;
  float depth = 0.0f;
  float chamber = 0.0f;
  float progress_distance = 0.0f;
  float progress_normalized = 0.0f;
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
  int stream_radius = 2;
  int unload_radius = 4;
  int max_chunk_rebuilds_per_update = 0;
  int forced_rebuild_radius = 0;
  VoxelCaveStructuralSurfaceMode structural_surface_mode =
      VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  Vec3 authored_fixture_light_color{1.0f, 0.16f, 0.08f};
  Vec3 procedural_fixture_light_color{1.0f, 0.16f, 0.08f};
  std::vector<VoxelCaveFixtureLightBand> procedural_fixture_light_bands;
  std::vector<VoxelCaveChamber> chambers;
  std::vector<VoxelCaveTunnel> tunnels;
  std::vector<VoxelCaveSurfaceOpening> surface_openings;
  std::vector<VoxelCaveProceduralField> procedural_fields;
  std::vector<VoxelMaterialProfile> material_profiles;
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
  std::shared_ptr<const CpuMesh> mesh{};
};

struct VoxelChunkSnapshot {
  VoxelChunkCoord coord{};
  Vec3 center{};
  float reveal_age = 0.0f;
  bool newly_revealed = false;
  bool rebuild_pending = false;
  bool collision_dirty = false;
  std::vector<VoxelChunkRenderBatch> batches;
  std::shared_ptr<const CpuMesh> collision_mesh{};
  VoxelChunkSurfaceStats surface_stats{};
};

class VoxelCaveState {
public:
  void configure(VoxelCaveSpec spec);
  void clear();
  void updateStreaming(Vec3 viewer_position, float dt);
  void applyEdit(const VoxelEdit &edit);

  [[nodiscard]] const VoxelCaveSpec &spec() const;
  [[nodiscard]] const std::vector<VoxelEdit> &edits() const;
  void setEdits(std::vector<VoxelEdit> edits);
  [[nodiscard]] const std::vector<VoxelChunkSnapshot> &activeChunks() const;
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
    std::vector<VoxelChunkRenderBatch> batches;
    std::shared_ptr<const CpuMesh> collision_mesh{};
    VoxelChunkSurfaceStats surface_stats{};
  };

  void ensureChunk(VoxelChunkCoord coord);
  void rebuildChunk(ChunkState &chunk);
  [[nodiscard]] Vec3 chunkCenter(VoxelChunkCoord coord) const;
  [[nodiscard]] int chunkDistance(VoxelChunkCoord lhs, VoxelChunkCoord rhs) const;
  [[nodiscard]] bool chunkIntersectsEdit(VoxelChunkCoord coord, const VoxelEdit &edit) const;

  VoxelCaveSpec spec_{};
  std::vector<ChunkState> chunks_;
  std::vector<VoxelEdit> edits_;
  std::vector<VoxelChunkSnapshot> snapshots_;
};

[[nodiscard]] VoxelCaveProceduralFrame
sampleVoxelCaveProceduralFrameAt(const VoxelCaveProceduralField &field, float distance);
[[nodiscard]] VoxelCaveProceduralFrame
closestVoxelCaveProceduralFrame(const VoxelCaveProceduralField &field, Vec3 position);
[[nodiscard]] VoxelCaveInteriorSample sampleVoxelCaveInterior(const VoxelCaveSpec &spec,
                                                              Vec3 position, float density,
                                                              VoxelCaveInteriorProbe probe = {});
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
