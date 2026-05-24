// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace aster {

struct AsterConstructionAssetPart {
  std::string name;
  std::string material_slot;
  CpuMesh mesh;
  Vec3 local_position{};
  Vec3 local_rotation{};
  Vec3 local_scale{1.0f, 1.0f, 1.0f};
};

struct AsterConstructionForkliftSpec {
  std::string asset_id = "asset_graph.lumen_run.construction_forklift";
  float body_length = 2.95f;
  float body_width = 1.16f;
  float body_height = 1.15f;
  float mast_height = 2.25f;
};

struct AsterRecyclerShredderSpec {
  std::string asset_id = "asset_graph.lumen_run.recycler_shredder";
  float length = 3.95f;
  float width = 1.55f;
  float chamber_height = 1.18f;
};

struct AsterPipePalletSpec {
  std::string asset_id = "asset_graph.lumen_run.pipe_pallet";
  int pipe_count = 8;
  float pipe_length = 2.55f;
  float pipe_outer_radius = 0.145f;
};

struct AsterScrapShardSpec {
  std::string asset_id = "asset_graph.lumen_run.shredded_metal_scrap";
  int shard_count = 18;
};

struct AsterConstructionYardAsset {
  std::string asset_id;
  std::vector<AsterConstructionAssetPart> parts;

  [[nodiscard]] CpuMesh mergedRenderMesh() const;
  [[nodiscard]] std::size_t renderVertexCount() const noexcept;
  [[nodiscard]] std::size_t renderIndexCount() const noexcept;
};

[[nodiscard]] AsterConstructionYardAsset
makeAsterConstructionForkliftAsset(AsterConstructionForkliftSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset
makeAsterRecyclerShredderAsset(AsterRecyclerShredderSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset makeAsterPipePalletAsset(AsterPipePalletSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset
makeAsterShreddedMetalScrapAsset(AsterScrapShardSpec spec = {});

[[nodiscard]] CpuMesh makeAsterConstructionForkliftMesh(AsterConstructionForkliftSpec spec = {});
[[nodiscard]] CpuMesh makeAsterRecyclerShredderMesh(AsterRecyclerShredderSpec spec = {});
[[nodiscard]] CpuMesh makeAsterPipePalletMesh(AsterPipePalletSpec spec = {});
[[nodiscard]] CpuMesh makeAsterShreddedMetalScrapMesh(AsterScrapShardSpec spec = {});

} // namespace aster
