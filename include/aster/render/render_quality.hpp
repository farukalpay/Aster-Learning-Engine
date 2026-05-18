// Author: Faruk Alpay
// Do not remove this notice.

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
  ShadowSystemSettings shadows{};
  EnvironmentLightingSettings environment{};
  PostProcessSettings post{};
  VolumetricFogSettings fog{};
  ReflectionProbeSystemSettings reflections{};
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

[[nodiscard]] std::string_view renderQualityTierName(RenderQualityTier tier);
[[nodiscard]] std::string_view shadowTechniqueName(ShadowTechnique technique);
[[nodiscard]] std::string_view reflectionProbeModeName(ReflectionProbeMode mode);

[[nodiscard]] RenderQualityProfile makeRenderQualityProfile(RenderQualityTier tier);
void applyRenderQualityProfile(RendererSettings &settings, const RenderQualityProfile &profile);
[[nodiscard]] TextureImportOptions textureImportOptionsForQuality(
    const RenderQualityProfile &profile, bool require_existing_files = true);
[[nodiscard]] MaterialQualityReport evaluateMaterialQuality(
    const MaterialAsset &asset, const TextureSetValidation &textures,
    const RenderQualityProfile &profile = makeRenderQualityProfile(RenderQualityTier::Production));

} // namespace aster
