// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/material/material_lab.hpp"

#include "aster/render/software_preview_renderer.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <span>
#include <system_error>

namespace aster {
namespace {

[[nodiscard]] std::uint64_t fileByteCost(const std::filesystem::path &path) {
  std::error_code error;
  const std::uint64_t size = std::filesystem::file_size(path, error);
  return error ? 0u : size;
}

[[nodiscard]] std::string_view textureCompressionName(const TextureCompression compression) {
  switch (compression) {
  case TextureCompression::None:
    return "none";
  case TextureCompression::Ktx2Basis:
    return "ktx2-basis";
  }
  return "none";
}

[[nodiscard]] MeshPrimitive previewPrimitive(const MaterialLabMeshTarget target) {
  switch (target) {
  case MaterialLabMeshTarget::Rock:
    return MeshPrimitive::Rock;
  case MaterialLabMeshTarget::CaveWall:
    return MeshPrimitive::Plane;
  case MaterialLabMeshTarget::Sphere:
  default:
    return MeshPrimitive::Sphere;
  }
}

[[nodiscard]] OrbitCamera previewCamera(const MaterialLabMeshTarget target) {
  OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = radians(42.0f);
  camera.pitch = radians(18.0f);
  camera.radius = target == MaterialLabMeshTarget::CaveWall ? 3.3f : 4.0f;
  camera.vertical_fov = radians(38.0f);
  return camera;
}

[[nodiscard]] MaterialDebugView debugViewFor(const MaterialLabPreviewMode mode) {
  switch (mode) {
  case MaterialLabPreviewMode::BaseColor:
    return MaterialDebugView::BaseColor;
  case MaterialLabPreviewMode::Normal:
    return MaterialDebugView::Normal;
  case MaterialLabPreviewMode::Roughness:
    return MaterialDebugView::Roughness;
  case MaterialLabPreviewMode::AmbientOcclusion:
    return MaterialDebugView::AmbientOcclusion;
  case MaterialLabPreviewMode::Fog:
    return MaterialDebugView::Fog;
  case MaterialLabPreviewMode::Beauty:
  default:
    return MaterialDebugView::Beauty;
  }
}

[[nodiscard]] RendererSettings previewSettings(const MaterialLabPreviewState &state) {
  RendererSettings settings;
  settings.material_debug_view = debugViewFor(state.mode);
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = state.mode != MaterialLabPreviewMode::BaseColor;
  settings.ambient_strength = 0.26f;
  settings.ambient_floor = 0.02f;
  settings.exposure = 1.05f;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.45f, 0.78f, 0.30f};
  settings.sun_light.color = {1.0f, 0.84f, 0.66f};
  settings.sun_light.intensity = 2.2f;
  settings.pipeline.clear_color = {0.028f, 0.032f, 0.038f};
  settings.atmosphere.enabled = state.mode == MaterialLabPreviewMode::Fog ||
                                state.environment == MaterialLabEnvironmentRig::Fog ||
                                state.environment == MaterialLabEnvironmentRig::CaveDark;
  settings.atmosphere.fog_color = state.environment == MaterialLabEnvironmentRig::CaveDark
                                      ? Vec3{0.055f, 0.066f, 0.075f}
                                      : Vec3{0.08f, 0.086f, 0.094f};
  settings.atmosphere.fog_start = state.environment == MaterialLabEnvironmentRig::Fog ? 2.2f : 4.0f;
  settings.atmosphere.fog_end = state.environment == MaterialLabEnvironmentRig::Fog ? 7.0f : 10.0f;
  settings.atmosphere.fog_strength =
      state.mode == MaterialLabPreviewMode::Fog || state.environment == MaterialLabEnvironmentRig::Fog
          ? 0.48f
          : 0.10f;
  settings.light_rig = {
      Light{{-2.4f, 2.8f, 2.2f}, {12.0f, 9.5f, 6.0f}, 1.0f, 0.6f},
      Light{{2.6f, 1.4f, 1.8f}, {4.8f, 6.0f, 8.5f}, 1.0f, 0.8f},
      Light{{0.0f, 2.5f, -2.5f}, {5.0f, 4.2f, 3.0f}, 1.0f, 1.0f},
      Light{{0.0f, 0.8f, 2.5f}, {2.0f, 2.1f, 2.4f}, 1.0f, 1.2f},
  };
  if (state.environment == MaterialLabEnvironmentRig::CaveDark) {
    settings.exposure = 1.18f;
    settings.ambient_strength = 0.14f;
    settings.sun_light.intensity = 1.15f;
    settings.pipeline.clear_color = {0.012f, 0.015f, 0.019f};
  } else if (state.environment == MaterialLabEnvironmentRig::ProbeLit) {
    settings.reflections.enabled = true;
    settings.reflections.static_local_probes = true;
    settings.reflections.fallback_intensity = 0.38f;
    settings.ambient_strength = 0.34f;
    settings.sun_light.intensity = 1.65f;
  }
  return settings;
}

[[nodiscard]] Scene previewScene(const MaterialAsset &asset, const MaterialLabMeshTarget target) {
  const CompiledMaterialAsset compiled = compileMaterialAssetForRendering(asset);
  Scene scene;
  RenderObject object;
  object.name = "Material Lab Preview";
  object.material = compiled.fallback_material.material;
  object.material_asset_id = asset.id;
  object.asset_provenance.source_asset_id =
      asset.procedural_graph_guid.empty() ? asset.id : asset.procedural_graph_guid;
  object.asset_provenance.source_node = asset.procedural_graph_node;
  object.asset_provenance.material_slot = asset.id;
  object.primitive = previewPrimitive(target);
  if (target == MaterialLabMeshTarget::CaveWall) {
    object.transform.scale = {1.8f, 1.0f, 1.8f};
  }
  scene.objects().push_back(object);
  return scene;
}

void appendMobileDegradations(const MaterialAsset &asset, MaterialLabAudit &audit) {
  const auto append_if_true = [&](const std::string_view key, const std::string_view message) {
    const auto found = asset.quality_profile.find(std::string(key));
    if (found != asset.quality_profile.end() && found->second == "true") {
      audit.mobile_degradations.emplace_back(message);
    }
  };
  append_if_true("mobile_drop_parallax", "mobile preview drops parallax");
  append_if_true("mobile_drop_height", "mobile preview drops height response");
  if (const auto max_texture = asset.quality_profile.find("mobile_max_texture_size");
      max_texture != asset.quality_profile.end()) {
    audit.mobile_degradations.push_back("mobile max texture size " + max_texture->second);
  }
  if (audit.mobile_degradations.empty()) {
    audit.mobile_degradations.emplace_back("no mobile quality delta declared");
  }
}

} // namespace

std::string_view materialLabPreviewModeName(const MaterialLabPreviewMode mode) {
  switch (mode) {
  case MaterialLabPreviewMode::Beauty:
    return "beauty";
  case MaterialLabPreviewMode::BaseColor:
    return "base-color";
  case MaterialLabPreviewMode::Normal:
    return "normal";
  case MaterialLabPreviewMode::Roughness:
    return "roughness";
  case MaterialLabPreviewMode::AmbientOcclusion:
    return "ao";
  case MaterialLabPreviewMode::Fog:
    return "fog";
  }
  return "beauty";
}

std::string_view materialLabMeshTargetName(const MaterialLabMeshTarget target) {
  switch (target) {
  case MaterialLabMeshTarget::Sphere:
    return "sphere";
  case MaterialLabMeshTarget::Rock:
    return "rock";
  case MaterialLabMeshTarget::CaveWall:
    return "cave-wall";
  }
  return "sphere";
}

std::string_view materialLabEnvironmentRigName(const MaterialLabEnvironmentRig rig) {
  switch (rig) {
  case MaterialLabEnvironmentRig::StudioNeutral:
    return "studio-neutral";
  case MaterialLabEnvironmentRig::CaveDark:
    return "cave-dark";
  case MaterialLabEnvironmentRig::ProbeLit:
    return "probe-lit";
  case MaterialLabEnvironmentRig::Fog:
    return "fog";
  }
  return "studio-neutral";
}

MaterialLabAudit buildMaterialLabAudit(const MaterialAsset &asset,
                                       const TextureSetValidation &textures,
                                       const RenderQualityProfile &profile) {
  MaterialLabAudit audit;
  const CompiledMaterialAsset compiled = compileMaterialAssetForRendering(asset);
  const MaterialQualityReport quality = evaluateMaterialQuality(asset, textures, profile);
  audit.production_ready = quality.production_ready;
  audit.score = quality.score;
  audit.feature_mask = materialFeatureMask(materialFeatureSet(asset));
  audit.shader_variant_key = compiled.variant.stable_hash;
  audit.shader_variant_tag = compiled.variant.tag;

  auto role = asset.textures.begin();
  for (const TextureAssetMetadata &texture : textures.textures) {
    MaterialLabTextureAudit row;
    row.role = role == asset.textures.end() ? std::string(textureKindName(texture.kind))
                                            : role->first;
    row.source_path = texture.source_path.generic_string();
    row.kind = textureKindName(texture.kind);
    row.color_space = textureColorSpaceName(texture.color_space);
    row.width = texture.width;
    row.height = texture.height;
    row.mip_count = static_cast<std::uint32_t>(texture.mips.size());
    row.byte_cost = fileByteCost(texture.source_path);
    row.valid = texture.valid;
    row.diagnostics = texture.diagnostics;
    if (texture.compression != TextureCompression::Ktx2Basis) {
      row.diagnostics.push_back("warning: compression is " +
                                std::string(textureCompressionName(texture.compression)));
    }
    audit.texture_byte_cost += row.byte_cost;
    audit.textures.push_back(std::move(row));
    if (role != asset.textures.end()) {
      ++role;
    }
  }

  for (const RenderQualityIssue &issue : quality.issues) {
    std::string severity = "warning";
    if (issue.severity == RenderQualityIssueSeverity::Error) {
      severity = "error";
    } else if (issue.severity == RenderQualityIssueSeverity::Info) {
      severity = "info";
    }
    const std::string entry = severity + ": " + issue.message;
    if (issue.category == "surface-fidelity") {
      audit.surface_fidelity.push_back(entry);
    } else {
      audit.issues.push_back(severity + ": " + issue.category + ": " + issue.message);
    }
  }
  if (const auto roughness = asset.params.find("roughness"); roughness != asset.params.end() &&
                                                 (roughness->second < 0.04f ||
                                                  roughness->second > 0.98f)) {
    audit.issues.push_back("warning: roughness sits outside the stable PBR range");
  }
  if (const auto normal = asset.params.find("micro_normal_strength");
      normal != asset.params.end() && normal->second > 1.0f) {
    audit.issues.push_back("warning: normal strength may alias under grazing light");
  }
  if (const auto height = asset.params.find("height_shading");
      height != asset.params.end() && height->second > 0.65f) {
    audit.issues.push_back("warning: height response may alias in motion");
  }
  audit.provenance_notes.push_back("roughness histogram unavailable: no decoded texture pixels");
  audit.provenance_notes.push_back("compression artifact preview unavailable: no native transcode path");
  if (audit.surface_fidelity.empty()) {
    audit.surface_fidelity.emplace_back("surface contract has no open fidelity warnings");
  }
  appendMobileDegradations(asset, audit);
  return audit;
}

MaterialLabPreviewImage renderMaterialLabPreview(const MaterialAsset &asset,
                                                 const MaterialLabPreviewState &state) {
  MaterialLabPreviewImage image;
  image.width = std::max(state.width, 1);
  image.height = std::max(state.height, 1);
  try {
    const SoftwareFrameBuffer framebuffer =
        renderSoftwarePreview(previewScene(asset, state.mesh), previewCamera(state.mesh),
                              {.width = image.width,
                               .height = image.height,
                               .samples_per_axis = 1,
                               .frame_seconds = 0.0,
                               .settings = previewSettings(state)});
    const std::span<const std::uint8_t> pixels = framebuffer.rgba8();
    image.rgba8.assign(pixels.begin(), pixels.end());
    image.available = !image.rgba8.empty();
  } catch (const std::exception &error) {
    image.available = false;
    image.diagnostic = error.what();
  }
  return image;
}

} // namespace aster
