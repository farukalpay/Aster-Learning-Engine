// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_quality.hpp"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace aster {
namespace {

bool hasTextureRole(const MaterialAsset &asset, const std::string_view role) {
  return asset.textures.find(std::string(role)) != asset.textures.end();
}

bool hasAnyTextureRole(const MaterialAsset &asset, const std::initializer_list<std::string_view> roles) {
  return std::any_of(roles.begin(), roles.end(), [&asset](const std::string_view role) {
    return hasTextureRole(asset, role);
  });
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
    profile.fog.enabled = true;
    profile.fog.density = 0.08f;
    profile.reflections.mode = ReflectionProbeMode::Disabled;
    break;
  case RenderQualityTier::Production:
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
  settings.grounding.enabled = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.contact_shadows = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.auto_contact_shadows = profile.shadows.technique != ShadowTechnique::Disabled;
  settings.grounding.contact_shadow_strength = profile.shadows.softness;
  settings.grounding.contact_shadow_receiver_bias = profile.shadows.receiver_bias;
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
