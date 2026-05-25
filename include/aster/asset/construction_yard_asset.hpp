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
  float length = 5.40f;
  float width = 2.45f;
  float chamber_height = 1.48f;
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

struct AsterConstructionSiteSpec {
  std::string asset_id = "asset_graph.lumen_run.modular_construction_site";
  float width = 15.6f;
  float depth = 10.8f;
  float height = 6.0f;
};

struct AsterMobileCraneSpec {
  std::string asset_id = "asset_graph.lumen_run.mobile_crane";
  float boom_length = 10.4f;
  float boom_height = 4.6f;
};

struct AsterHydraulicPressSpec {
  std::string asset_id = "asset_graph.lumen_run.hydraulic_press";
  float width = 2.8f;
  float length = 3.6f;
};

struct AsterMetalBaleSpec {
  std::string asset_id = "asset_graph.lumen_run.metal_bale";
  float fill = 1.0f;
};

struct AsterDeliveryRackSpec {
  std::string asset_id = "asset_graph.lumen_run.bale_delivery_rack";
  int bay_count = 24;
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
[[nodiscard]] AsterConstructionYardAsset
makeAsterModularConstructionSiteAsset(AsterConstructionSiteSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset makeAsterMobileCraneAsset(AsterMobileCraneSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset
makeAsterHydraulicPressAsset(AsterHydraulicPressSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset makeAsterMetalBaleAsset(AsterMetalBaleSpec spec = {});
[[nodiscard]] AsterConstructionYardAsset
makeAsterDeliveryRackAsset(AsterDeliveryRackSpec spec = {});

[[nodiscard]] CpuMesh makeAsterConstructionForkliftMesh(AsterConstructionForkliftSpec spec = {});
[[nodiscard]] CpuMesh makeAsterRecyclerShredderMesh(AsterRecyclerShredderSpec spec = {});
[[nodiscard]] CpuMesh makeAsterPipePalletMesh(AsterPipePalletSpec spec = {});
[[nodiscard]] CpuMesh makeAsterShreddedMetalScrapMesh(AsterScrapShardSpec spec = {});
[[nodiscard]] CpuMesh makeAsterModularConstructionSiteMesh(AsterConstructionSiteSpec spec = {});
[[nodiscard]] CpuMesh makeAsterMobileCraneMesh(AsterMobileCraneSpec spec = {});
[[nodiscard]] CpuMesh makeAsterHydraulicPressMesh(AsterHydraulicPressSpec spec = {});
[[nodiscard]] CpuMesh makeAsterMetalBaleMesh(AsterMetalBaleSpec spec = {});
[[nodiscard]] CpuMesh makeAsterDeliveryRackMesh(AsterDeliveryRackSpec spec = {});

} // namespace aster
