// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/transform.hpp"
#include "aster/math/vec.hpp"
#include "aster/core/world_perceptual_primitive.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct CpuMesh;
struct RenderObject;

enum class RenderPerceptualTruthMode : std::uint32_t {
  Compatibility = 0,
  Warn = 1,
  Strict = 2,
};

enum class MeshPrimitive {
  Box,
  Sphere,
  Plane,
  Rock,
  Crystal,
  RuinBlock,
  Pillar,
};

enum class SurfacePattern {
  None,
  CourseCells,
  FiberStrands,
  GrassSoil,
  WaterSurface,
  SoilPath,
  TerrainBlend,
  FurFibers,
  LayeredTerrain,
  WeatheredStone,
  PaintedWood,
  Foliage,
  IridescentScales,
  AmberResin,
  FeatherVanes,
  TwigNest,
  ReptileScales,
  CaveRock,
  CoalVein,
  ContactShadow,
  CaveWeb,
  CaveSkitterChitin,
  CaveSkitterEye,
  WeatheredMetal,
  WeldBead,
  BiologicalIntegument,
};

enum class MaterialSurfaceProfile : std::uint32_t {
  Auto = 0,
  Plain = 1,
  Masonry = 2,
  OrganicFiber = 3,
  TerrainLayer = 4,
  Liquid = 5,
  Foliage = 6,
  Resin = 7,
  PaintedWood = 8,
  Feather = 9,
  Scales = 10,
  StratifiedRock = 11,
  MineralVein = 12,
  ContactShadow = 13,
  FilamentWeb = 14,
  ChitinShell = 15,
  EmissiveLens = 16,
  CorrodedMetal = 17,
  WeldBead = 18,
  BiologicalIntegument = 19,
};

enum class FaceCullMode {
  None,
  Back,
  Front,
};

enum class MaterialRenderRole {
  Surface,
  SupportSurface,
};

enum class MaterialAlphaMode {
  Opaque,
  Masked,
  DitheredCoverage,
  Blend,
};

enum class MaterialDepthWrite {
  Auto,
  Enabled,
  Disabled,
};

enum class CameraOcclusionPolicy {
  Fade,
  Solid,
};

enum class MaterialRenderQueue {
  Opaque,
  Masked,
  Translucent,
};

struct ProceduralSurfaceLayer {
  float macro_variation = 0.0f;
  float micro_normal_strength = 0.0f;
  float roughness_variation = 0.0f;
  float physical_texel_density = 512.0f;
  float height_normal_coupling = 0.0f;
  float roughness_height_coupling = 0.0f;
  float macro_frequency_breakup = 0.0f;
  float micro_frequency_breakup = 0.0f;
  float wetness = 0.0f;
  float height_shading = 0.0f;
  float pitting_density = 0.0f;
  float pitting_depth = 0.0f;
  float oxide_layering = 0.0f;
  float cavity_grime = 0.0f;
  float edge_polish = 0.0f;
  float weld_heat_tint = 0.0f;
  float axial_scratches = 0.0f;
  float wet_streaks = 0.0f;
  float rust_bloom = 0.0f;
  float black_scab = 0.0f;
  float paint_remnant = 0.0f;
  float weld_slag = 0.0f;
  float rim_soot = 0.0f;
};

struct ViewerCullVolume {
  bool enabled = false;
  Vec3 center{};
  Vec3 half_extents{};
  FaceCullMode outside = FaceCullMode::Back;
  FaceCullMode inside = FaceCullMode::Front;
};

enum class RenderVisibilityClass : std::uint32_t {
  General,
  CaveCell,
  CavePortal,
  DynamicVoxel,
  Creature,
};

struct RenderVisibilityHint {
  RenderVisibilityClass visibility_class = RenderVisibilityClass::General;
  Vec3 cell{};
  float portal_depth = 0.0f;
};

struct RenderLodPolicy {
  float max_distance = 0.0f;
  float min_projected_radius = 0.0f;
};

enum class RenderDepthLayer : std::uint32_t {
  BaseSurface = 0,
  SurfaceAttachment = 1,
  Decal = 2,
  ContactShadow = 3,
  DebugOverlay = 4,
};

struct RenderDepthPolicy {
  RenderDepthLayer layer = RenderDepthLayer::BaseSurface;
  float constant_bias = 0.0f;
  float slope_bias = 0.0f;
  float normal_offset = 0.0f;
};

struct DynamicMeshResourceKey {
  std::uint64_t id = 0u;
  std::uint64_t generation = 0u;

  [[nodiscard]] bool valid() const noexcept {
    return id != 0u;
  }

  [[nodiscard]] friend bool operator==(const DynamicMeshResourceKey lhs,
                                       const DynamicMeshResourceKey rhs) noexcept {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
};

struct DynamicMeshResourceKeyHash {
  [[nodiscard]] std::size_t operator()(const DynamicMeshResourceKey key) const noexcept {
    std::uint64_t hash = key.id ^ 0x9e3779b97f4a7c15ull;
    hash ^= key.generation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33u;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33u;
    return static_cast<std::size_t>(hash);
  }
};

struct RenderObjectAssetProvenance {
  std::string source_asset_id;
  std::filesystem::path source_path;
  std::string source_node;
  std::string source_mesh;
  std::string material_slot;
  std::uint32_t uv_channel = 0u;
  bool uv0_present = true;
  bool authored_tangent_basis = true;
  std::size_t degenerate_triangles = 0u;
  std::size_t invalid_normals = 0u;
  std::size_t generated_tangents = 0u;
};

struct Material {
  std::string asset_id;
  LinearRgb base_color{1.0f, 1.0f, 1.0f};
  EmissionColor emission_color{0.0f, 0.0f, 0.0f};
  float roughness = 0.55f;
  float metallic = 0.0f;
  float dielectric_reflectance = 0.5f;
  float coat_strength = 0.0f;
  float coat_roughness = 0.03f;
  float tangent_anisotropy = 0.0f;
  Vec3 edge_sheen_color{};
  float edge_sheen_roughness = 0.0f;
  float emission_strength = 0.0f;
  float detail_strength = 0.0f;
  float detail_scale = 1.0f;
  float edge_wear = 0.0f;
  float ambient_occlusion = 1.0f;
  MaterialSurfaceProfile surface_profile = MaterialSurfaceProfile::Auto;
  SurfacePattern surface_pattern = SurfacePattern::None;
  Vec2 pattern_scale{1.0f, 1.0f};
  float pattern_depth = 0.0f;
  float pattern_contrast = 0.0f;
  float pattern_mortar = 0.08f;
  float opacity = 1.0f;
  bool double_sided = false;
  FaceCullMode cull_mode = FaceCullMode::Back;
  MaterialRenderRole render_role = MaterialRenderRole::Surface;
  MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
  MaterialDepthWrite depth_write = MaterialDepthWrite::Auto;
  RenderDepthPolicy depth_policy{};
  CameraOcclusionPolicy camera_occlusion = CameraOcclusionPolicy::Fade;
  ProceduralSurfaceLayer procedural{};
  bool receives_shadows = true;
  std::uint64_t compiled_permutation_key = 0u;
  std::uint32_t compiled_permutation_flags = 0u;
  std::uint64_t shader_variant_key = 0u;
  std::string procedural_graph_guid;
  std::string procedural_graph_node;
  std::string procedural_capability_status;
  std::uint64_t procedural_pipeline_key = 0u;
};

struct MaterialDesc {
  LinearRgb base_color{1.0f, 1.0f, 1.0f};
  EmissionColor emission_color{0.0f, 0.0f, 0.0f};
  float roughness = 0.55f;
  float metallic = 0.0f;
  float dielectric_reflectance = 0.5f;
  float coat_strength = 0.0f;
  float coat_roughness = 0.03f;
  float tangent_anisotropy = 0.0f;
  Vec3 edge_sheen_color{};
  float edge_sheen_roughness = 0.0f;
  float emission_strength = 0.0f;
  float detail_strength = 0.0f;
  float detail_scale = 1.0f;
  float edge_wear = 0.0f;
  float ambient_occlusion = 1.0f;
  MaterialSurfaceProfile surface_profile = MaterialSurfaceProfile::Auto;
  SurfacePattern surface_pattern = SurfacePattern::None;
  Vec2 pattern_scale{1.0f, 1.0f};
  float pattern_depth = 0.0f;
  float pattern_contrast = 0.0f;
  float pattern_mortar = 0.08f;
  float opacity = 1.0f;
  bool double_sided = false;
  FaceCullMode cull_mode = FaceCullMode::Back;
  MaterialRenderRole render_role = MaterialRenderRole::Surface;
  MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
  MaterialDepthWrite depth_write = MaterialDepthWrite::Auto;
  RenderDepthPolicy depth_policy{};
  CameraOcclusionPolicy camera_occlusion = CameraOcclusionPolicy::Fade;
  ProceduralSurfaceLayer procedural{};
  bool receives_shadows = true;
  std::string procedural_graph_guid;
  std::string procedural_graph_node;
  std::string procedural_capability_status;
  std::uint64_t procedural_pipeline_key = 0u;
};

[[nodiscard]] Material makeMaterial(const MaterialDesc &desc);
[[nodiscard]] Material makeSupportSurfaceMaterial(Material material);
[[nodiscard]] MaterialSurfaceProfile surfaceProfileForPattern(SurfacePattern pattern);
[[nodiscard]] MaterialSurfaceProfile resolveMaterialSurfaceProfile(const Material &material);
[[nodiscard]] std::uint32_t materialSurfaceProfileId(MaterialSurfaceProfile profile);
[[nodiscard]] std::string_view materialSurfaceProfileName(MaterialSurfaceProfile profile);
[[nodiscard]] MaterialRenderQueue classifyMaterialRenderQueue(const Material &material);
[[nodiscard]] bool materialWritesDepth(const Material &material);
[[nodiscard]] bool allowsCameraOcclusionFade(const Material &material);
[[nodiscard]] bool isMaterialTranslucent(const Material &material);
[[nodiscard]] bool isDoubleSidedMaterial(const Material &material);
[[nodiscard]] bool renderObjectCastsShadows(const RenderObject &object);

struct RenderObject {
  std::string name;
  std::string material_asset_id;
  MeshPrimitive primitive = MeshPrimitive::Sphere;
  Transform transform{};
  Material material{};
  float spin_rate = 0.0f;
  std::shared_ptr<const CpuMesh> custom_mesh{};
  bool camera_occlusion_fade = true;
  bool auto_contact_shadow = true;
  bool casts_contact_shadow = false;
  bool casts_shadows = true;
  float contact_shadow_strength = 1.0f;
  float contact_shadow_radius_scale = 1.0f;
  ViewerCullVolume viewer_cull_volume{};
  RenderVisibilityHint visibility_hint{};
  RenderLodPolicy lod{};
  DynamicMeshResourceKey dynamic_mesh{};
  RenderObjectAssetProvenance asset_provenance{};
  RenderPerceptualTruthMode perceptual_truth_mode = RenderPerceptualTruthMode::Compatibility;
  WorldPerceptualPrimitive perceptual_primitive{};
};

struct ReflectionProbe {
  std::string name;
  Vec3 position{};
  float influence_radius = 8.0f;
  Vec3 sky_irradiance{0.42f, 0.47f, 0.52f};
  Vec3 ground_irradiance{0.20f, 0.17f, 0.13f};
  Vec3 specular_tint{1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
  std::filesystem::path cubemap_asset;
};

class Scene {
public:
  [[nodiscard]] const std::vector<RenderObject> &objects() const {
    return objects_;
  }

  [[nodiscard]] std::vector<RenderObject> &objects() {
    return objects_;
  }

  [[nodiscard]] const std::vector<ReflectionProbe> &reflectionProbes() const {
    return reflection_probes_;
  }

  [[nodiscard]] std::vector<ReflectionProbe> &reflectionProbes() {
    return reflection_probes_;
  }

private:
  std::vector<RenderObject> objects_;
  std::vector<ReflectionProbe> reflection_probes_;
};

} // namespace aster
