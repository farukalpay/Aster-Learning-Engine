// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_compiler.hpp"
#include "aster/render/render_quality.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class MaterialLabPreviewMode {
  Beauty,
  BaseColor,
  Normal,
  Roughness,
  AmbientOcclusion,
  Fog,
};

enum class MaterialLabMeshTarget {
  Sphere,
  Rock,
  CaveWall,
};

enum class MaterialLabEnvironmentRig {
  StudioNeutral,
  CaveDark,
  ProbeLit,
  Fog,
};

struct MaterialLabTextureAudit {
  std::string role;
  std::string source_path;
  std::string kind;
  std::string color_space;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t mip_count = 0u;
  std::uint64_t byte_cost = 0u;
  bool valid = false;
  std::vector<std::string> diagnostics;
};

struct MaterialLabAudit {
  bool production_ready = false;
  std::uint32_t score = 0u;
  std::uint64_t feature_mask = 0u;
  std::uint64_t shader_variant_key = 0u;
  std::string shader_variant_tag;
  std::uint64_t texture_byte_cost = 0u;
  std::vector<MaterialLabTextureAudit> textures;
  std::vector<std::string> issues;
  std::vector<std::string> mobile_degradations;
  std::vector<std::string> provenance_notes;
};

struct MaterialLabPreviewState {
  MaterialLabPreviewMode mode = MaterialLabPreviewMode::Beauty;
  MaterialLabMeshTarget mesh = MaterialLabMeshTarget::Sphere;
  MaterialLabEnvironmentRig environment = MaterialLabEnvironmentRig::StudioNeutral;
  int width = 144;
  int height = 96;
};

struct MaterialLabPreviewImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba8;
  bool available = false;
  std::string diagnostic;
};

[[nodiscard]] std::string_view materialLabPreviewModeName(MaterialLabPreviewMode mode);
[[nodiscard]] std::string_view materialLabMeshTargetName(MaterialLabMeshTarget target);
[[nodiscard]] std::string_view materialLabEnvironmentRigName(MaterialLabEnvironmentRig rig);

[[nodiscard]] MaterialLabAudit buildMaterialLabAudit(
    const MaterialAsset &asset, const TextureSetValidation &textures,
    const RenderQualityProfile &profile = makeRenderQualityProfile(RenderQualityTier::Production));

[[nodiscard]] MaterialLabPreviewImage renderMaterialLabPreview(
    const MaterialAsset &asset, const MaterialLabPreviewState &state = {});

} // namespace aster
