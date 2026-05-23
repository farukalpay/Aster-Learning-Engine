// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/render/mesh.hpp"

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

namespace aster {

enum class AssetMeshFormat {
  Auto,
  Obj,
  Ply,
  Stl,
  Fbx,
  Gltf,
  Usd,
  Alembic,
};

enum class AssetMeshAxis {
  PositiveX,
  PositiveY,
  PositiveZ,
  NegativeX,
  NegativeY,
  NegativeZ,
};

struct AssetMeshImportOptions {
  AssetMeshFormat format = AssetMeshFormat::Auto;
  float unit_scale = 1.0f;
  AssetMeshAxis source_up = AssetMeshAxis::PositiveY;
  AssetMeshAxis source_forward = AssetMeshAxis::PositiveZ;
  AssetMeshAxis target_up = AssetMeshAxis::PositiveY;
  AssetMeshAxis target_forward = AssetMeshAxis::PositiveZ;
  bool flip_winding = false;
  bool rebuild_missing_normals = true;
};

struct AssetMeshSourceFacet {
  std::string object;
  std::string group;
  std::string material;
  std::size_t first_triangle = 0u;
  std::size_t triangle_count = 0u;
};

struct AssetMeshSourceSummary {
  std::vector<std::string> objects;
  std::vector<std::string> groups;
  std::vector<std::string> materials;
  std::vector<AssetMeshSourceFacet> facets;
};

struct AssetMeshIoReport {
  std::filesystem::path path;
  AssetMeshFormat format = AssetMeshFormat::Auto;
  std::string stable_source_hash;
  std::size_t vertices = 0u;
  std::size_t indices = 0u;
  bool ok = false;
  AssetMeshSourceSummary source;
  std::vector<std::string> diagnostics;
};

struct AssetMeshImportResult {
  CpuMesh mesh;
  AssetMeshIoReport report;
};

[[nodiscard]] std::string_view assetMeshFormatName(AssetMeshFormat format);
[[nodiscard]] AssetMeshFormat assetMeshFormatFromPath(const std::filesystem::path &path);
[[nodiscard]] std::string_view assetMeshAxisName(AssetMeshAxis axis);
[[nodiscard]] AssetMeshImportResult importMeshAsset(const std::filesystem::path &path,
                                                    AssetMeshFormat format = AssetMeshFormat::Auto);
[[nodiscard]] AssetMeshImportResult importMeshAsset(const std::filesystem::path &path,
                                                    AssetMeshImportOptions options);
[[nodiscard]] AssetMeshIoReport exportMeshAssetObj(const CpuMesh &mesh,
                                                   const std::filesystem::path &path);
[[nodiscard]] AssetMeshIoReport exportMeshAssetPly(const CpuMesh &mesh,
                                                   const std::filesystem::path &path);
[[nodiscard]] AssetMeshIoReport exportMeshAssetStl(const CpuMesh &mesh,
                                                   const std::filesystem::path &path);

} // namespace aster
