// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/mesh_authoring.hpp"
#include "aster/math/transform.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

enum class AssetModifierKind {
  Transform,
  Triangulate,
  Weld,
  RecalculateNormals,
  Mirror,
  Inset,
  Extrude,
  Bevel,
  Array,
  Solidify,
  Displace,
  WeightedNormal,
  Smooth,
  Decimate,
};

struct AssetModifierDesc {
  std::string id;
  AssetModifierKind kind = AssetModifierKind::Transform;
  Transform transform{};
  MeshMirrorAxis mirror_axis = MeshMirrorAxis::X;
  float amount = 0.0f;
  float epsilon = 0.0001f;
  bool enabled = true;
  std::uint32_t seed = 1u;
  std::uint32_t count = 1u;
  std::vector<std::string> creative_variant_tags;
};

struct AssetModifierStack {
  std::string asset_id;
  std::string source_provenance_id;
  std::vector<AssetModifierDesc> modifiers;
};

struct AssetModifierStackReport {
  std::string stable_provenance_id;
  std::size_t input_vertices = 0u;
  std::size_t input_indices = 0u;
  std::size_t output_vertices = 0u;
  std::size_t output_indices = 0u;
  std::uint32_t quality_score = 100u;
  std::vector<MeshAuthoringReport> operator_reports;
  std::vector<std::string> diagnostics;
  std::vector<std::string> degradation_reasons;
  std::vector<std::string> creative_variant_tags;
};

struct AssetModifierStackResult {
  CpuMesh mesh;
  AssetModifierStackReport report;
};

[[nodiscard]] std::string_view assetModifierKindName(AssetModifierKind kind);
[[nodiscard]] std::string stableAssetModifierStackId(const AssetModifierStack &stack);
[[nodiscard]] AssetModifierStackResult applyAssetModifierStack(const CpuMesh &source,
                                                               const AssetModifierStack &stack);

} // namespace aster
