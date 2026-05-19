// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/mesh.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace aster {

enum class AssetMeshFormat {
  Auto,
  Obj,
  Ply,
  Stl,
};

struct AssetMeshIoReport {
  std::filesystem::path path;
  AssetMeshFormat format = AssetMeshFormat::Auto;
  std::string stable_source_hash;
  std::size_t vertices = 0u;
  std::size_t indices = 0u;
  bool ok = false;
  std::vector<std::string> diagnostics;
};

struct AssetMeshImportResult {
  CpuMesh mesh;
  AssetMeshIoReport report;
};

[[nodiscard]] std::string_view assetMeshFormatName(AssetMeshFormat format);
[[nodiscard]] AssetMeshFormat assetMeshFormatFromPath(const std::filesystem::path &path);
[[nodiscard]] AssetMeshImportResult importMeshAsset(const std::filesystem::path &path,
                                                    AssetMeshFormat format = AssetMeshFormat::Auto);
[[nodiscard]] AssetMeshIoReport exportMeshAssetObj(const CpuMesh &mesh,
                                                   const std::filesystem::path &path);

} // namespace aster
