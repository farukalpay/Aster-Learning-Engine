// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class MaterialShadingModel {
  LitPBR,
  Unlit,
  Emissive,
};

enum class MaterialBlendMode {
  Opaque,
  Masked,
  Blend,
};

enum class MaterialAssetCullMode {
  None,
  Back,
  Front,
};

enum class MaterialDiagnosticSeverity {
  Warning,
  Error,
};

struct MaterialDiagnostic {
  MaterialDiagnosticSeverity severity = MaterialDiagnosticSeverity::Error;
  std::filesystem::path source_path;
  std::size_t line = 0u;
  std::size_t column = 0u;
  std::string message;
};

struct MaterialTextureSlot {
  std::string role;
  std::filesystem::path uri;
  bool srgb = false;
  bool required = true;
};

struct MaterialLayerExpression {
  std::string name;
  std::string operation;
  std::vector<std::string> arguments;
  std::string raw;
};

struct MaterialFeatureSet {
  bool textured = false;
  bool normal_map = false;
  bool orm_texture = false;
  bool emissive = false;
  bool height = false;
  bool parallax = false;
  bool triplanar = false;
  bool decal_receiver = false;
  bool fog = true;
  bool shadow = true;
  bool alpha_clip = false;
  bool alpha_blend = false;
  bool double_sided = false;
  bool instancing = true;
};

struct MaterialAsset {
  std::uint32_t schema_version = 1u;
  std::string id;
  std::string name;
  MaterialShadingModel shading_model = MaterialShadingModel::LitPBR;
  MaterialSurfaceProfile surface_profile = MaterialSurfaceProfile::Auto;
  MaterialBlendMode blend_mode = MaterialBlendMode::Opaque;
  MaterialAssetCullMode cull_mode = MaterialAssetCullMode::Back;
  bool receives_decals = false;
  bool receives_shadows = true;
  std::map<std::string, MaterialTextureSlot> textures;
  std::map<std::string, float> params;
  std::map<std::string, bool> explicit_features;
  std::vector<MaterialLayerExpression> layers;
  std::filesystem::path source_path;
};

struct MaterialAssetLoadResult {
  MaterialAsset value;
  std::vector<MaterialDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const;
};

[[nodiscard]] std::string_view materialShadingModelName(MaterialShadingModel value);
[[nodiscard]] std::string_view materialBlendModeName(MaterialBlendMode value);
[[nodiscard]] std::string_view materialAssetCullModeName(MaterialAssetCullMode value);

[[nodiscard]] MaterialAssetLoadResult parseMaterialAsset(std::string_view source,
                                                        std::filesystem::path source_path = {});
[[nodiscard]] MaterialAssetLoadResult loadMaterialAsset(const std::filesystem::path &path);
[[nodiscard]] std::vector<MaterialDiagnostic> validateMaterialAsset(const MaterialAsset &asset);
[[nodiscard]] MaterialFeatureSet materialFeatureSet(const MaterialAsset &asset);
[[nodiscard]] std::uint64_t materialFeatureMask(const MaterialFeatureSet &features);
[[nodiscard]] Material resolveMaterialAssetFallback(const MaterialAsset &asset);

} // namespace aster
