// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/render/render_device.hpp"
#include "aster/texture/texture_importer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

enum class RenderQualityTier {
  Prototype,
  Production,
  Cinematic,
};

enum class RenderQualityIssueSeverity {
  Info,
  Warning,
  Error,
};

enum class ShadowTechnique {
  Disabled,
  ContactOnly,
  CascadedDirectional,
};

enum class ReflectionProbeMode {
  Disabled,
  StaticLocal,
  StreamingLocal,
};

struct PbrMaterialQualityPolicy {
  bool require_albedo = true;
  bool require_normal = true;
  bool require_roughness_or_orm = true;
  bool require_occlusion = false;
  bool require_shadow_receiver = true;
  float minimum_roughness = 0.04f;
  float maximum_roughness = 0.98f;
};

struct TextureQualityPolicy {
  std::uint32_t minimum_dimension = 512u;
  bool require_mip_chain = true;
  bool require_compressed_runtime_textures = true;
  TextureCompression runtime_compression = TextureCompression::Ktx2Basis;
};

struct SurfaceFidelityPolicy {
  bool require_energy_conserving_bsdf = true;
  bool require_environment_response = true;
  bool require_area_light_response = true;
  bool require_shadow_filtering = true;
  bool require_tangent_basis_policy = true;
  bool require_temporal_stability_budget = true;
  bool require_artist_preview = true;
  bool require_physical_texel_density = true;
  bool require_height_normal_coupling = true;
  bool require_frequency_breakup = true;
  float minimum_area_light_radius = 0.18f;
  float minimum_physical_texel_density = 256.0f;
};

struct ShadowSystemSettings {
  ShadowTechnique technique = ShadowTechnique::ContactOnly;
  std::uint32_t directional_cascades = 0u;
  std::uint32_t map_size = 1024u;
  float max_distance = 32.0f;
  float normal_bias = 0.012f;
  float receiver_bias = 0.014f;
  float softness = 0.34f;
};

struct EnvironmentLightingSettings {
  Vec3 sky_ambient{0.42f, 0.47f, 0.52f};
  Vec3 ground_ambient{0.20f, 0.17f, 0.13f};
  Vec3 sun_direction_to_light{-0.42f, 0.82f, 0.38f};
  Vec3 sun_color{1.0f, 0.86f, 0.62f};
  float sun_intensity = 1.0f;
  float ambient_strength = 0.22f;
  float ambient_floor = 0.012f;
};

struct PostProcessSettings {
  ToneMapper tone_mapper = ToneMapper::PbrNeutral;
  float exposure = 1.05f;
  float saturation = 1.0f;
  float contrast = 1.0f;
  float bloom_threshold = 2.6f;
  float bloom_intensity = 0.0f;
};

struct VolumetricFogSettings {
  bool enabled = false;
  bool volumetric = false;
  Vec3 color{0.12f, 0.14f, 0.15f};
  float start = 8.0f;
  float end = 24.0f;
  float density = 0.0f;
  std::uint32_t froxel_depth_slices = 0u;
};

struct ReflectionProbeSystemSettings {
  ReflectionProbeMode mode = ReflectionProbeMode::Disabled;
  std::uint32_t probe_resolution = 128u;
  std::uint32_t max_active_probes = 0u;
  std::uint32_t update_budget_per_frame = 0u;
  float influence_radius = 8.0f;
};

struct SurfaceOcclusionQualitySettings {
  bool enabled = true;
  RendererOcclusionMode mode = RendererOcclusionMode::Hybrid;
  float radius = 1.10f;
  float thickness = 0.18f;
  float strength = 0.36f;
  std::uint32_t sample_count = 12u;
  float contact_hardening = 0.28f;
};

struct PresentationQualitySettings {
  float focal_length_mm = 46.0f;
  float camera_height_m = 1.55f;
  float scale_reference_m = 1.80f;
  float composition_weight = 0.58f;
  float vignette_strength = 0.08f;
};

struct SurfaceScaleQualitySettings {
  float physical_texel_density = 512.0f;
  float macro_frequency_breakup = 0.32f;
  float micro_frequency_breakup = 0.46f;
  float height_normal_coupling = 0.82f;
  float roughness_height_coupling = 0.56f;
};

struct RenderAssetPipelinePolicy {
  bool validate_materials = true;
  bool bake_texture_mips = true;
  bool pack_orm_textures = true;
  bool preserve_source_metadata = true;
  TextureCompression runtime_compression = TextureCompression::Ktx2Basis;
};

struct RenderQualityProfile {
  RenderQualityTier tier = RenderQualityTier::Production;
  PbrMaterialQualityPolicy materials{};
  TextureQualityPolicy textures{};
  SurfaceFidelityPolicy surface_fidelity{};
  ShadowSystemSettings shadows{};
  EnvironmentLightingSettings environment{};
  PostProcessSettings post{};
  VolumetricFogSettings fog{};
  ReflectionProbeSystemSettings reflections{};
  SurfaceOcclusionQualitySettings occlusion{};
  PresentationQualitySettings presentation{};
  SurfaceScaleQualitySettings surface_scale{};
  RenderAssetPipelinePolicy asset_pipeline{};
};

struct RenderQualityIssue {
  RenderQualityIssueSeverity severity = RenderQualityIssueSeverity::Warning;
  std::string category;
  std::string message;
};

struct MaterialQualityReport {
  bool production_ready = false;
  std::uint32_t score = 0u;
  std::vector<RenderQualityIssue> issues;
};

enum class AsterMaterialSignalKind : std::uint32_t {
  ShaderVariant,
  TypedBindingLayout,
  RustWetness,
  CavityEdgeWear,
  NormalHeightCoupling,
  TextureRoleProof,
  PreviewReadiness,
  SurfaceFidelity,
};

struct AsterMaterialSignalRow {
  AsterMaterialSignalKind kind = AsterMaterialSignalKind::ShaderVariant;
  std::string label;
  std::string evidence;
  float strength = 0.0f;
  bool ready = false;
};

struct AsterMaterialSignalSummary {
  bool image_proof_ready = false;
  std::uint32_t score = 0u;
  std::size_t ready_signals = 0u;
  std::size_t blocked_signals = 0u;
  std::vector<AsterMaterialSignalRow> rows;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string_view renderQualityTierName(RenderQualityTier tier);
[[nodiscard]] std::string_view shadowTechniqueName(ShadowTechnique technique);
[[nodiscard]] std::string_view reflectionProbeModeName(ReflectionProbeMode mode);
[[nodiscard]] std::string_view asterMaterialSignalKindName(AsterMaterialSignalKind kind);

[[nodiscard]] RenderQualityProfile makeRenderQualityProfile(RenderQualityTier tier);
void applyRenderQualityProfile(RendererSettings &settings, const RenderQualityProfile &profile);
[[nodiscard]] TextureImportOptions textureImportOptionsForQuality(
    const RenderQualityProfile &profile, bool require_existing_files = true);
[[nodiscard]] MaterialQualityReport evaluateMaterialQuality(
    const MaterialAsset &asset, const TextureSetValidation &textures,
    const RenderQualityProfile &profile = makeRenderQualityProfile(RenderQualityTier::Production));
[[nodiscard]] AsterMaterialSignalSummary summarizeAsterMaterialSignals(
    const MaterialAsset &asset, const TextureSetValidation &textures,
    const RenderQualityProfile &profile = makeRenderQualityProfile(RenderQualityTier::Production));

} // namespace aster
