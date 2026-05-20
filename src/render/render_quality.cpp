// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_quality.hpp"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <string_view>
#include <utility>

namespace aster {
namespace {

bool hasTextureRole(const MaterialAsset &asset, const std::string_view role) {
  return asset.textures.find(std::string(role)) != asset.textures.end();
}

bool hasAnyTextureRole(const MaterialAsset &asset,
                       const std::initializer_list<std::string_view> roles) {
  return std::any_of(roles.begin(), roles.end(), [&asset](const std::string_view role) {
    return hasTextureRole(asset, role);
  });
}

bool featureEnabled(const MaterialAsset &asset, const std::string_view feature) {
  const auto found = asset.explicit_features.find(std::string(feature));
  return found != asset.explicit_features.end() && found->second;
}

bool hasFloatParamAbove(const MaterialAsset &asset, const std::string_view key,
                        const float threshold) {
  const auto found = asset.params.find(std::string(key));
  return found != asset.params.end() && found->second > threshold;
}

bool mapHasAnyKey(const std::map<std::string, std::string> &values,
                  const std::initializer_list<std::string_view> keys) {
  return std::any_of(keys.begin(), keys.end(), [&values](const std::string_view key) {
    return values.find(std::string(key)) != values.end();
  });
}

bool assetHasAnyMetadata(const MaterialAsset &asset,
                         const std::initializer_list<std::string_view> keys) {
  return mapHasAnyKey(asset.authoring, keys) || mapHasAnyKey(asset.quality_profile, keys);
}

void addIssue(MaterialQualityReport &report, const RenderQualityIssueSeverity severity,
              std::string category, std::string message) {
  report.issues.push_back(
      {.severity = severity, .category = std::move(category), .message = std::move(message)});
}

std::uint32_t issuePenalty(const RenderQualityIssueSeverity severity) {
  switch (severity) {
  case RenderQualityIssueSeverity::Info:
    return 3u;
  case RenderQualityIssueSeverity::Warning:
    return 9u;
  case RenderQualityIssueSeverity::Error:
    return 26u;
  }
  return 9u;
}

bool hasError(const MaterialQualityReport &report) {
  return std::any_of(report.issues.begin(), report.issues.end(), [](const RenderQualityIssue &issue) {
    return issue.severity == RenderQualityIssueSeverity::Error;
  });
}

} // namespace

std::string_view renderQualityTierName(const RenderQualityTier tier) {
  switch (tier) {
  case RenderQualityTier::Prototype:
    return "prototype";
  case RenderQualityTier::Production:
    return "production";
  case RenderQualityTier::Cinematic:
    return "cinematic";
  }
  return "production";
}

std::string_view shadowTechniqueName(const ShadowTechnique technique) {
  switch (technique) {
  case ShadowTechnique::Disabled:
    return "disabled";
  case ShadowTechnique::ContactOnly:
    return "contact-only";
  case ShadowTechnique::CascadedDirectional:
    return "cascaded-directional";
  }
  return "disabled";
}

std::string_view reflectionProbeModeName(const ReflectionProbeMode mode) {
  switch (mode) {
  case ReflectionProbeMode::Disabled:
    return "disabled";
  case ReflectionProbeMode::StaticLocal:
    return "static-local";
  case ReflectionProbeMode::StreamingLocal:
    return "streaming-local";
  }
  return "disabled";
}

RenderQualityProfile makeRenderQualityProfile(const RenderQualityTier tier) {
  RenderQualityProfile profile;
  profile.tier = tier;
  switch (tier) {
  case RenderQualityTier::Prototype:
    profile.surface_fidelity.require_energy_conserving_bsdf = false;
    profile.surface_fidelity.require_tangent_basis_policy = false;
    profile.surface_fidelity.require_physical_texel_density = false;
    profile.surface_fidelity.require_height_normal_coupling = false;
    profile.surface_fidelity.require_frequency_breakup = false;
    profile.surface_fidelity.minimum_area_light_radius = 0.06f;
    profile.materials.require_normal = false;
    profile.materials.require_roughness_or_orm = false;
    profile.textures.minimum_dimension = 128u;
    profile.textures.require_mip_chain = false;
    profile.textures.require_compressed_runtime_textures = false;
    profile.shadows = {.technique = ShadowTechnique::ContactOnly,
                       .directional_cascades = 0u,
                       .map_size = 512u,
                       .max_distance = 18.0f,
                       .normal_bias = 0.016f,
                       .receiver_bias = 0.018f,
                       .softness = 0.42f};
    profile.environment.ambient_strength = 0.28f;
    profile.post.bloom_intensity = 0.0f;
    profile.occlusion = {.enabled = true,
                         .mode = RendererOcclusionMode::SurfaceCavity,
                         .radius = 0.62f,
                         .thickness = 0.12f,
                         .strength = 0.18f,
                         .sample_count = 4u,
                         .contact_hardening = 0.12f};
    profile.surface_scale = {.physical_texel_density = 128.0f,
                             .macro_frequency_breakup = 0.12f,
                             .micro_frequency_breakup = 0.16f,
                             .height_normal_coupling = 0.22f,
                             .roughness_height_coupling = 0.18f};
    profile.fog.enabled = true;
    profile.fog.density = 0.08f;
    profile.reflections.mode = ReflectionProbeMode::Disabled;
    break;
  case RenderQualityTier::Production:
    profile.surface_fidelity.minimum_area_light_radius = 0.18f;
    profile.surface_fidelity.minimum_physical_texel_density = 384.0f;
    profile.shadows = {.technique = ShadowTechnique::CascadedDirectional,
                       .directional_cascades = 3u,
                       .map_size = 2048u,
                       .max_distance = 72.0f,
                       .normal_bias = 0.010f,
                       .receiver_bias = 0.012f,
                       .softness = 0.30f};
    profile.environment.ambient_strength = 0.22f;
    profile.environment.ambient_floor = 0.014f;
    profile.post.tone_mapper = ToneMapper::PbrNeutral;
    profile.post.bloom_threshold = 2.2f;
    profile.post.bloom_intensity = 0.12f;
    profile.occlusion = {.enabled = true,
                         .mode = RendererOcclusionMode::Hybrid,
                         .radius = 1.12f,
                         .thickness = 0.18f,
                         .strength = 0.38f,
                         .sample_count = 12u,
                         .contact_hardening = 0.30f};
    profile.presentation = {.focal_length_mm = 46.0f,
                            .camera_height_m = 1.55f,
                            .scale_reference_m = 1.80f,
                            .composition_weight = 0.60f,
                            .vignette_strength = 0.08f};
    profile.surface_scale = {.physical_texel_density = 512.0f,
                             .macro_frequency_breakup = 0.34f,
                             .micro_frequency_breakup = 0.48f,
                             .height_normal_coupling = 0.84f,
                             .roughness_height_coupling = 0.58f};
    profile.fog.enabled = true;
    profile.fog.volumetric = false;
    profile.fog.start = 7.0f;
    profile.fog.end = 28.0f;
    profile.fog.density = 0.14f;
    profile.reflections = {.mode = ReflectionProbeMode::StaticLocal,
                           .probe_resolution = 256u,
                           .max_active_probes = 16u,
                           .update_budget_per_frame = 1u,
                           .influence_radius = 10.0f};
    break;
  case RenderQualityTier::Cinematic:
    profile.surface_fidelity.minimum_area_light_radius = 0.36f;
    profile.surface_fidelity.minimum_physical_texel_density = 768.0f;
    profile.textures.minimum_dimension = 1024u;
    profile.materials.require_occlusion = true;
    profile.shadows = {.technique = ShadowTechnique::CascadedDirectional,
                       .directional_cascades = 4u,
                       .map_size = 4096u,
                       .max_distance = 120.0f,
                       .normal_bias = 0.008f,
                       .receiver_bias = 0.010f,
                       .softness = 0.24f};
    profile.environment.ambient_strength = 0.20f;
    profile.environment.ambient_floor = 0.018f;
    profile.post.tone_mapper = ToneMapper::FilmicAces;
    profile.post.exposure = 1.08f;
    profile.post.saturation = 1.04f;
    profile.post.contrast = 1.05f;
    profile.post.bloom_threshold = 1.9f;
    profile.post.bloom_intensity = 0.22f;
    profile.occlusion = {.enabled = true,
                         .mode = RendererOcclusionMode::Hybrid,
                         .radius = 1.45f,
                         .thickness = 0.24f,
                         .strength = 0.48f,
                         .sample_count = 20u,
                         .contact_hardening = 0.42f};
    profile.presentation = {.focal_length_mm = 54.0f,
                            .camera_height_m = 1.50f,
                            .scale_reference_m = 1.80f,
                            .composition_weight = 0.66f,
                            .vignette_strength = 0.12f};
    profile.surface_scale = {.physical_texel_density = 1024.0f,
                             .macro_frequency_breakup = 0.46f,
                             .micro_frequency_breakup = 0.62f,
                             .height_normal_coupling = 0.92f,
                             .roughness_height_coupling = 0.72f};
    profile.fog.enabled = true;
    profile.fog.volumetric = true;
    profile.fog.start = 5.5f;
    profile.fog.end = 34.0f;
    profile.fog.density = 0.20f;
    profile.fog.froxel_depth_slices = 64u;
    profile.reflections = {.mode = ReflectionProbeMode::StreamingLocal,
                           .probe_resolution = 512u,
                           .max_active_probes = 32u,
                           .update_budget_per_frame = 2u,
                           .influence_radius = 14.0f};
    break;
  }
  profile.asset_pipeline.runtime_compression = profile.textures.runtime_compression;
  return profile;
}

void applyRenderQualityProfile(RendererSettings &settings, const RenderQualityProfile &profile) {
  settings.exposure = profile.post.exposure;
  settings.ambient_strength = profile.environment.ambient_strength;
  settings.ambient_floor = profile.environment.ambient_floor;
  settings.sky_ambient_color = profile.environment.sky_ambient;
  settings.ground_ambient_color = profile.environment.ground_ambient;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = profile.environment.sun_direction_to_light;
  settings.sun_light.color = profile.environment.sun_color;
  settings.sun_light.intensity = profile.environment.sun_intensity;
  settings.pipeline.tone_mapper = profile.post.tone_mapper;
  settings.post.hdr_scene_color = true;
  settings.post.bloom = profile.post.bloom_intensity > 0.0f;
  settings.post.fxaa = profile.tier != RenderQualityTier::Prototype;
  settings.post.bloom_threshold = profile.post.bloom_threshold;
  settings.post.bloom_intensity = profile.post.bloom_intensity;
  settings.post.color_grade_saturation = profile.post.saturation;
  settings.post.color_grade_contrast = profile.post.contrast;
  settings.shadows.enabled = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.shadows.cascaded_directional =
      profile.shadows.technique == ShadowTechnique::CascadedDirectional;
  settings.shadows.directional_cascades = profile.shadows.directional_cascades;
  settings.shadows.atlas_size = profile.shadows.map_size;
  settings.shadows.max_distance = profile.shadows.max_distance;
  settings.shadows.receiver_bias = profile.shadows.receiver_bias;
  settings.shadows.normal_bias = profile.shadows.normal_bias;
  settings.shadows.pcf_radius = profile.shadows.softness;
  settings.reflections.enabled = profile.reflections.mode != ReflectionProbeMode::Disabled;
  settings.reflections.static_local_probes = profile.reflections.mode == ReflectionProbeMode::StaticLocal;
  settings.reflections.probe_resolution = profile.reflections.probe_resolution;
  settings.reflections.max_active_probes = profile.reflections.max_active_probes;
  settings.reflections.fallback_intensity = profile.reflections.mode == ReflectionProbeMode::Disabled
                                                ? 0.0f
                                                : profile.environment.ambient_strength;
  settings.occlusion.enabled = profile.occlusion.enabled;
  settings.occlusion.mode = profile.occlusion.mode;
  settings.occlusion.radius = profile.occlusion.radius;
  settings.occlusion.thickness = profile.occlusion.thickness;
  settings.occlusion.strength = profile.occlusion.strength;
  settings.occlusion.sample_count = profile.occlusion.sample_count;
  settings.occlusion.contact_hardening = profile.occlusion.contact_hardening;
  settings.occlusion.micro_shadowing = profile.surface_scale.micro_frequency_breakup * 0.36f;
  settings.presentation.focal_length_mm = profile.presentation.focal_length_mm;
  settings.presentation.camera_height_m = profile.presentation.camera_height_m;
  settings.presentation.scale_reference_m = profile.presentation.scale_reference_m;
  settings.presentation.composition_weight = profile.presentation.composition_weight;
  settings.presentation.vignette_strength = profile.presentation.vignette_strength;
  settings.surface_scale.physical_texel_density = profile.surface_scale.physical_texel_density;
  settings.surface_scale.macro_frequency_breakup = profile.surface_scale.macro_frequency_breakup;
  settings.surface_scale.micro_frequency_breakup = profile.surface_scale.micro_frequency_breakup;
  settings.surface_scale.height_normal_coupling = profile.surface_scale.height_normal_coupling;
  settings.surface_scale.roughness_height_coupling =
      profile.surface_scale.roughness_height_coupling;
  settings.grounding.enabled = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.contact_shadows = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.auto_contact_shadows = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.contact_shadow_strength = profile.shadows.softness;
  settings.grounding.contact_shadow_receiver_bias = profile.shadows.receiver_bias;
  settings.grounding.surface_occlusion_strength = profile.occlusion.strength;
  settings.grounding.surface_occlusion_mix = 0.38f + profile.occlusion.contact_hardening * 0.22f;
  settings.grounding.surface_occlusion_height = profile.occlusion.radius;
  settings.atmosphere.enabled = profile.fog.enabled;
  settings.atmosphere.fog_color = profile.fog.color;
  settings.atmosphere.fog_start = profile.fog.start;
  settings.atmosphere.fog_end = profile.fog.end;
  settings.atmosphere.fog_strength = profile.fog.density;
  settings.atmosphere.saturation = profile.post.saturation;
  settings.atmosphere.contrast = profile.post.contrast;
  settings.clustered_lighting.enabled = profile.tier != RenderQualityTier::Prototype;
  settings.clustered_lighting.cluster_count_x =
      profile.tier == RenderQualityTier::Cinematic ? 16u : 8u;
  settings.clustered_lighting.cluster_count_y =
      profile.tier == RenderQualityTier::Cinematic ? 8u : 4u;
  settings.clustered_lighting.cluster_count_z =
      profile.tier == RenderQualityTier::Cinematic ? 16u : 8u;
  settings.clustered_lighting.max_visible_lights =
      profile.tier == RenderQualityTier::Cinematic ? kRenderLightUniformCapacity : 32u;
  settings.clustered_lighting.max_lights_per_cluster =
      profile.tier == RenderQualityTier::Cinematic ? 12u : 8u;
  settings.light_policy.max_point_lights =
      settings.clustered_lighting.enabled ? settings.clustered_lighting.max_visible_lights
                                          : kDefaultRenderLightBudget;
  if (profile.surface_fidelity.require_area_light_response) {
    for (Light &light : settings.light_rig) {
      light.source_radius =
          std::max(light.source_radius, profile.surface_fidelity.minimum_area_light_radius);
    }
  }
}

TextureImportOptions textureImportOptionsForQuality(const RenderQualityProfile &profile,
                                                    const bool require_existing_files) {
  return {.require_existing_files = require_existing_files,
          .generate_mips = profile.textures.require_mip_chain ||
                           profile.asset_pipeline.bake_texture_mips,
          .compression = profile.asset_pipeline.runtime_compression};
}

MaterialQualityReport evaluateMaterialQuality(const MaterialAsset &asset,
                                              const TextureSetValidation &textures,
                                              const RenderQualityProfile &profile) {
  MaterialQualityReport report;
  const bool lit = asset.shading_model == MaterialShadingModel::LitPBR;
  if (lit && profile.materials.require_albedo && !hasTextureRole(asset, "albedo")) {
    addIssue(report, RenderQualityIssueSeverity::Error, "material", "Lit PBR material has no albedo texture.");
  }
  if (lit && profile.materials.require_normal && !hasTextureRole(asset, "normal")) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "material", "Lit PBR material has no normal texture.");
  }
  if (lit && profile.materials.require_roughness_or_orm &&
      !hasAnyTextureRole(asset, {"roughness", "orm", "metallic_roughness"})) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "material",
             "Lit PBR material has no roughness, ORM, or metallic-roughness texture.");
  }
  if (lit && profile.materials.require_occlusion && !hasAnyTextureRole(asset, {"ao", "occlusion", "orm"})) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "material", "Material has no ambient-occlusion source.");
  }
  if (lit && profile.materials.require_shadow_receiver && !asset.receives_shadows) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "lighting", "Lit material opts out of shadow receiving.");
  }
  if (const auto roughness = asset.params.find("roughness"); roughness != asset.params.end()) {
    if (roughness->second < profile.materials.minimum_roughness ||
        roughness->second > profile.materials.maximum_roughness) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "material",
               "Roughness is outside the profile's stable PBR range.");
    }
  }
  if (lit && profile.surface_fidelity.require_energy_conserving_bsdf &&
      !assetHasAnyMetadata(
          asset, {"bsdf", "brdf", "surface_model", "lighting_model", "energy_conservation"})) {
    addIssue(report, RenderQualityIssueSeverity::Info, "surface-fidelity",
             "Material uses LitPBR inputs but does not name its BSDF or "
             "energy-conservation contract.");
  }
  if (lit && profile.surface_fidelity.require_environment_response) {
    if (profile.reflections.mode == ReflectionProbeMode::Disabled) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
               "Quality profile disables reflection probes, so wet/specular response "
               "has no IBL path.");
    } else if (!mapHasAnyKey(asset.preview, {"environment", "ibl", "reflection_probe"})) {
      addIssue(report, RenderQualityIssueSeverity::Info, "surface-fidelity",
               "Material preview metadata does not declare an environment or "
               "reflection-probe rig.");
    }
  }
  if (profile.surface_fidelity.require_area_light_response &&
      profile.surface_fidelity.minimum_area_light_radius <= 0.0f) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
             "Quality profile has no nonzero area-light radius for "
             "specular/soft-light response.");
  }
  if (profile.surface_fidelity.require_shadow_filtering) {
    if (profile.shadows.technique == ShadowTechnique::Disabled) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
               "Quality profile disables shadow filtering.");
    } else if (profile.shadows.softness <= 0.0f) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
               "Quality profile enables shadows without a filtering/softness radius.");
    }
  }
  if (lit && profile.surface_fidelity.require_tangent_basis_policy &&
      (hasTextureRole(asset, "normal") || featureEnabled(asset, "normal_map")) &&
      !assetHasAnyMetadata(asset, {"tangent_basis", "tangent_space", "normal_convention"})) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
             "Normal-mapped material does not declare its tangent-space policy.");
  }
  const bool displacement_like =
      hasTextureRole(asset, "height") || featureEnabled(asset, "parallax") ||
      featureEnabled(asset, "triplanar") ||
      hasFloatParamAbove(asset, "height_shading", 0.0f) ||
      hasFloatParamAbove(asset, "micro_normal_strength", 0.0f);
  if (lit && profile.surface_fidelity.require_temporal_stability_budget && displacement_like &&
      !assetHasAnyMetadata(asset, {"temporal_stability", "motion_aliasing_budget",
                                  "mip_bias_policy", "parallax_lod_policy"})) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "surface-fidelity",
             "Height/detail material has no temporal stability or motion-aliasing budget.");
  }
  if (profile.surface_fidelity.require_artist_preview && asset.preview.empty()) {
    addIssue(report, RenderQualityIssueSeverity::Info, "surface-fidelity",
             "Material has no artist-facing preview rig metadata.");
  }
  const auto param_or = [&asset](const std::string_view key, const float fallback) {
    const auto found = asset.params.find(std::string(key));
    return found == asset.params.end() ? fallback : found->second;
  };
  const float texel_density =
      param_or("physical_texel_density", param_or("texel_density", 0.0f));
  if (lit && profile.surface_fidelity.require_physical_texel_density &&
      texel_density < profile.surface_fidelity.minimum_physical_texel_density &&
      !assetHasAnyMetadata(asset, {"physical_texel_density", "texel_density", "meters_per_texel"})) {
    addIssue(report, RenderQualityIssueSeverity::Warning, "surface-scale",
             "Material does not declare a physical texel-density policy for scale-readable detail.");
  }
  if (lit && displacement_like && profile.surface_fidelity.require_height_normal_coupling) {
    const float height_normal = param_or("height_normal_coupling", 0.0f);
    const float roughness_height = param_or("roughness_height_coupling", 0.0f);
    if ((height_normal <= 0.0f || roughness_height <= 0.0f) &&
        !assetHasAnyMetadata(asset, {"height_normal_coupling", "roughness_height_coupling",
                                    "height_roughness_response"})) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "surface-stack",
               "Height, normal, and roughness response are not authored as a coupled surface stack.");
    }
  }
  if (lit && profile.surface_fidelity.require_frequency_breakup) {
    const float macro_breakup = param_or("macro_frequency_breakup", 0.0f);
    const float micro_breakup = param_or("micro_frequency_breakup", 0.0f);
    if ((macro_breakup <= 0.0f || micro_breakup <= 0.0f) &&
        !assetHasAnyMetadata(asset, {"macro_frequency_breakup", "micro_frequency_breakup",
                                    "surface_frequency_breakup"})) {
      addIssue(report, RenderQualityIssueSeverity::Info, "surface-stack",
               "Material has no macro/micro frequency breakup metadata for non-repeating detail.");
    }
  }

  for (const std::string &diagnostic : textures.diagnostics) {
    addIssue(report, textures.ok ? RenderQualityIssueSeverity::Warning
                                 : RenderQualityIssueSeverity::Error,
             "texture", diagnostic);
  }
  for (const TextureAssetMetadata &texture : textures.textures) {
    if (!texture.valid) {
      addIssue(report, RenderQualityIssueSeverity::Error, "texture",
               "Texture metadata is invalid for " + texture.source_path.string() + ".");
      continue;
    }
    if (profile.textures.minimum_dimension > 0u &&
        (texture.width > 0u || texture.height > 0u) &&
        std::max(texture.width, texture.height) < profile.textures.minimum_dimension) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "texture",
               "Texture is below the profile minimum dimension: " + texture.source_path.string() + ".");
    }
    if (profile.textures.require_mip_chain && texture.mips.size() <= 1u &&
        std::max(texture.width, texture.height) > 1u) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "texture",
               "Texture has no mip chain: " + texture.source_path.string() + ".");
    }
    if (profile.textures.require_compressed_runtime_textures &&
        texture.compression != profile.textures.runtime_compression) {
      addIssue(report, RenderQualityIssueSeverity::Warning, "texture",
               "Texture is not in the runtime compression target: " + texture.source_path.string() + ".");
    }
  }

  std::uint32_t penalty = 0u;
  for (const RenderQualityIssue &issue : report.issues) {
    penalty += issuePenalty(issue.severity);
  }
  report.score = penalty >= 100u ? 0u : 100u - penalty;
  report.production_ready = !hasError(report) && report.score >= 70u;
  return report;
}

} // namespace aster
