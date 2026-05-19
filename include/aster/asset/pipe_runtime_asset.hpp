// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"
#include "aster/scene/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster {

struct AsterPipeMaterialSlot {
  std::string id;
  std::string label;
  MaterialSurfaceProfile surface_profile = MaterialSurfaceProfile::Plain;
  SurfacePattern surface_pattern = SurfacePattern::None;
};

struct AsterPipeUvIsland {
  std::string id;
  Vec2 min{};
  Vec2 max{};
  float texel_density = 1.0f;
};

struct AsterPipeRustAnchor {
  Vec3 position{};
  float radius = 0.1f;
  float severity = 0.5f;
};

struct AsterPipeWetnessStreakAnchor {
  Vec3 root{};
  Vec3 direction{1.0f, 0.0f, 0.0f};
  float length = 0.5f;
  float strength = 0.5f;
};

struct AsterPipeAssetPart {
  std::string name;
  std::string material_slot;
  CpuMesh mesh;
};

struct AsterPipeLod {
  std::string name;
  CpuMesh mesh;
  float screen_coverage = 1.0f;
};

struct AsterPipeCollisionProxy {
  std::string name;
  Vec3 center{};
  Vec3 half_extents{};
  float radius = 0.0f;
  float length = 0.0f;
  std::size_t triangle_budget = 0u;
  bool covers_render_bounds = false;
};

struct AsterPipeCookReport {
  std::string asset_id;
  bool production_ready = false;
  std::size_t render_vertices = 0u;
  std::size_t render_indices = 0u;
  std::size_t lod_count = 0u;
  std::size_t collision_proxy_count = 0u;
  std::size_t material_slot_count = 0u;
  std::uint64_t modifier_stack_hash = 0u;
  std::vector<std::string> dependency_edges;
  std::vector<std::string> diagnostics;
};

struct AsterPipeAssetSpec {
  std::string asset_id = "asset.pipe.rusted_runtime_section";
  float length = 5.2f;
  float outer_radius = 0.54f;
  float wall_thickness = 0.075f;
  int radial_segments = 96;
  int length_segments = 24;
  int bevel_segments = 4;
  bool include_welds = true;
  bool include_longitudinal_seam = true;
  bool include_flanges = true;
  bool include_bolts = true;
  bool include_chipped_rims = true;
  bool include_lods = true;
  bool include_collision = true;
  int bolt_count_per_flange = 10;
  std::uint32_t seed = 0xA57E2026u;
  float rust_strength = 0.82f;
  float wetness_strength = 0.22f;
};

struct AsterPipeAsset {
  std::vector<AsterPipeAssetPart> parts;
  std::vector<AsterPipeLod> lods;
  std::vector<AsterPipeCollisionProxy> collision_proxies;
  std::vector<AsterPipeMaterialSlot> material_slots;
  std::vector<AsterPipeUvIsland> uv_islands;
  std::vector<AsterPipeRustAnchor> rust_anchors;
  std::vector<AsterPipeWetnessStreakAnchor> wetness_streaks;
  AsterPipeCookReport cook_report;

  [[nodiscard]] CpuMesh mergedRenderMesh() const;
  [[nodiscard]] std::size_t renderVertexCount() const noexcept;
  [[nodiscard]] std::size_t renderIndexCount() const noexcept;
};

[[nodiscard]] AsterPipeAsset makeAsterPipeAsset(AsterPipeAssetSpec spec = {});
[[nodiscard]] CpuMesh makeAsterPipeRenderMesh(AsterPipeAssetSpec spec = {});
[[nodiscard]] Material makeAsterPipeMaterial(const std::string &slot_id);

} // namespace aster
