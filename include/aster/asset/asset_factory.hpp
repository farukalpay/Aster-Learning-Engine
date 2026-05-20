// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_modifier_stack.hpp"
#include "aster/asset/pipe_runtime_asset.hpp"
#include "aster/material/procedural_surface.hpp"
#include "aster/physics/physics_world.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class AsterAssetFactoryStageKind {
  SourceGeometry,
  ModifierStack,
  SurfaceContract,
  LodRecipe,
  PhysicsProxy,
  QualityGate,
  Package,
};

enum class AsterAssetFactoryPhysicsProxyKind {
  BoundsBox,
  RadialCapsule,
  TriangleMesh,
};

enum class AsterPipeFactoryVariant {
  ReferenceSilhouette,
  IndustrialHardware,
};

enum class AsterAssetFactoryDiagnosticSeverity {
  Info,
  Warning,
  Error,
};

struct AsterAssetFactorySignalRule {
  std::string id;
  float minimum_average = 0.0f;
  float minimum_coverage = 0.0f;
  float weight = 1.0f;
};

struct AsterAssetFactorySignalSample {
  std::string id;
  float average = 0.0f;
  float coverage = 0.0f;
  float maximum = 0.0f;
};

struct AsterAssetFactorySurfaceCoverage {
  std::string contract_id;
  std::size_t sample_count = 0u;
  std::vector<AsterAssetFactorySignalSample> signals;
  std::vector<std::string> claimed_signals;
  std::vector<std::string> rejected_signals;
  std::vector<std::string> diagnostics;
  std::uint32_t quality_score = 100u;
  bool passed = true;
};

struct AsterAssetFactorySurfaceContract {
  std::string id;
  std::string label;
  std::string material_slot;
  ProceduralSurfaceLayer layer{};
  float detail_scale = 1.0f;
  float min_physical_texel_density = 768.0f;
  float min_height_normal_coupling = 0.50f;
  float min_roughness_height_coupling = 0.40f;
  std::vector<AsterAssetFactorySignalRule> required_signals;
  std::vector<AsterAssetFactorySignalRule> forbidden_signals;
};

struct AsterAssetFactoryPhysicsProxy {
  std::string id;
  std::string label;
  AsterAssetFactoryPhysicsProxyKind kind = AsterAssetFactoryPhysicsProxyKind::BoundsBox;
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  float length = 1.0f;
  std::size_t triangle_budget = 0u;
  bool covers_render_bounds = false;
  bool query_enabled = true;
  PhysicsMaterial material{};
  PhysicsCollisionFilter filter{};
};

struct AsterAssetFactoryStage {
  std::string id;
  AsterAssetFactoryStageKind kind = AsterAssetFactoryStageKind::SourceGeometry;
  std::string label;
  std::vector<std::string> depends_on;
  AssetModifierStack modifier_stack;
  std::string surface_contract_id;
  std::string physics_proxy_id;
  std::uint32_t minimum_quality = 60u;
  bool enabled = true;
  std::vector<std::string> creative_variant_tags;
};

struct AsterAssetFactoryStageReport {
  std::string id;
  AsterAssetFactoryStageKind kind = AsterAssetFactoryStageKind::SourceGeometry;
  bool executed = false;
  bool passed = true;
  std::uint32_t quality_score = 100u;
  std::size_t input_vertices = 0u;
  std::size_t output_vertices = 0u;
  std::size_t input_indices = 0u;
  std::size_t output_indices = 0u;
  std::vector<std::string> diagnostics;
};

struct AsterAssetFactoryRecipe {
  std::string asset_id;
  std::string label;
  std::string source_provenance_id;
  CpuMesh source_mesh;
  Material material{};
  std::vector<CpuMesh> authored_lods;
  std::vector<AsterAssetFactoryStage> stages;
  std::vector<AsterAssetFactorySurfaceContract> surface_contracts;
  std::vector<AsterAssetFactoryPhysicsProxy> physics_proxies;
  std::vector<std::string> visual_brief_claims;
  std::vector<std::string> visual_brief_rejections;
  std::vector<std::string> dependency_edges;
  std::vector<std::string> creative_variant_tags;
};

struct AsterAssetFactoryBuildResult {
  std::string asset_id;
  std::string stable_recipe_hash;
  CpuMesh mesh;
  Material material{};
  std::vector<CpuMesh> lods;
  std::vector<PhysicsBodyDesc> physics_bodies;
  std::vector<AsterAssetFactoryStageReport> stage_reports;
  std::vector<AsterAssetFactorySurfaceCoverage> surface_coverages;
  std::vector<std::string> dependency_edges;
  std::vector<std::string> visual_brief_claims;
  std::vector<std::string> visual_brief_rejections;
  std::vector<std::string> diagnostics;
  std::uint32_t quality_score = 0u;
  bool production_ready = false;
};

struct AsterAssetFactoryQualityDiagnostic {
  AsterAssetFactoryDiagnosticSeverity severity = AsterAssetFactoryDiagnosticSeverity::Info;
  std::string category;
  std::string stage_id;
  std::string message;
};

struct AsterAssetFactoryLodSummary {
  std::uint32_t level = 0u;
  std::size_t vertices = 0u;
  std::size_t indices = 0u;
  float triangle_ratio = 1.0f;
  float recommended_screen_coverage = 1.0f;
  bool generated_by_factory = true;
};

struct AsterAssetFactoryPhysicsProxySummary {
  std::string id;
  std::string shape;
  Vec3 center{};
  Vec3 half_extents{};
  float radius = 0.0f;
  float length = 0.0f;
  std::size_t triangle_budget = 0u;
  float friction = 0.0f;
  bool covers_render_bounds = false;
  bool query_enabled = true;
};

struct AsterAssetFactoryVisualBriefRow {
  std::string signal;
  std::string status;
  std::string source;
  float weight = 1.0f;
};

struct AsterAssetFactoryRecipeAudit {
  std::string asset_id;
  std::string stable_recipe_hash;
  std::vector<std::string> stage_order;
  std::vector<std::string> missing_dependencies;
  std::vector<std::string> surface_contract_ids;
  std::vector<std::string> physics_proxy_ids;
  std::vector<AsterAssetFactoryLodSummary> lod_summary;
  std::vector<AsterAssetFactoryVisualBriefRow> visual_brief_rows;
  std::vector<AsterAssetFactoryQualityDiagnostic> diagnostics;
  std::uint32_t quality_score = 0u;
  bool production_ready = false;
};

[[nodiscard]] std::string_view
asterAssetFactoryDiagnosticSeverityName(AsterAssetFactoryDiagnosticSeverity severity) noexcept;
[[nodiscard]] std::string_view
asterAssetFactoryStageKindName(AsterAssetFactoryStageKind kind) noexcept;
[[nodiscard]] std::string_view
asterAssetFactoryPhysicsProxyKindName(AsterAssetFactoryPhysicsProxyKind kind) noexcept;
[[nodiscard]] std::vector<AsterAssetFactoryQualityDiagnostic>
validateAsterAssetFactoryRecipe(const AsterAssetFactoryRecipe &recipe);
[[nodiscard]] std::vector<AsterAssetFactoryLodSummary>
summarizeAsterAssetFactoryLods(const AsterAssetFactoryBuildResult &result);
[[nodiscard]] std::vector<AsterAssetFactoryPhysicsProxySummary>
summarizeAsterAssetFactoryPhysicsProxies(const AsterAssetFactoryRecipe &recipe);
[[nodiscard]] std::vector<AsterAssetFactoryVisualBriefRow>
makeAsterAssetFactoryVisualBriefRows(const AsterAssetFactoryRecipe &recipe,
                                     const AsterAssetFactoryBuildResult &result);
[[nodiscard]] AsterAssetFactoryRecipeAudit
auditAsterAssetFactoryBuild(const AsterAssetFactoryRecipe &recipe,
                            const AsterAssetFactoryBuildResult &result);
[[nodiscard]] std::vector<std::string>
describeAsterAssetFactoryAudit(const AsterAssetFactoryRecipeAudit &audit);
[[nodiscard]] std::string stableAsterAssetFactoryRecipeHash(
    const AsterAssetFactoryRecipe &recipe);
[[nodiscard]] PhysicsBodyDesc
asterAssetFactoryPhysicsBodyDesc(const AsterAssetFactoryPhysicsProxy &proxy,
                                 std::shared_ptr<const CpuMesh> mesh = {});
[[nodiscard]] AsterAssetFactoryBuildResult
buildAsterAssetFactoryRecipe(const AsterAssetFactoryRecipe &recipe);
[[nodiscard]] AsterAssetFactoryRecipe
makeAsterPipeFactoryRecipe(AsterPipeAssetSpec spec = {},
                           AsterPipeFactoryVariant variant =
                               AsterPipeFactoryVariant::ReferenceSilhouette);

} // namespace aster
