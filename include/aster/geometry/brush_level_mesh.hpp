// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <vector>

namespace aster {

enum class BrushVolume {
  Air = 0,
  Solid = 1,
};

struct BrushBox {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  Vec3 rotation{};
  BrushVolume volume = BrushVolume::Solid;
  int time = 0;
  float chamfer = 0.0f;
};

struct BrushLevelMeshOptions {
  float uv_scale = 1.0f;
  float edge_softening = 0.0f;
  float noise_frequency = 0.32f;
  float noise_strength = 0.0f;
  int noise_seed = 1337;
  MeshProcessOptions mesh_options{};
};

[[nodiscard]] CpuMesh buildBrushLevelMesh(const std::vector<BrushBox> &brushes,
                                          BrushLevelMeshOptions options = {});

} // namespace aster
