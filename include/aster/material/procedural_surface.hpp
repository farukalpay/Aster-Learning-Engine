// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"
#include "aster/scene/scene.hpp"

namespace aster {

struct AsterSurfaceDomain {
  Vec3 position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  Vec2 uv{};
  float detail_scale = 1.0f;
};

struct AsterSurfaceCellSample {
  float cell_id = 0.0f;
  float distance = 1.0f;
  float edge_distance = 1.0f;
  float pitting = 0.0f;
};

struct AsterPipeSurfaceSignals {
  float broad_oxide = 0.0f;
  float fine_oxide = 0.0f;
  float orange_rust = 0.0f;
  float black_oxide = 0.0f;
  float pit = 0.0f;
  float pit_edge = 0.0f;
  float cavity_grime = 0.0f;
  float edge_polish = 0.0f;
  float wet_film = 0.0f;
  float axial_scratch = 0.0f;
  float heat_tint = 0.0f;
  float rust_bloom = 0.0f;
  float black_scab = 0.0f;
  float paint_remnant = 0.0f;
  float weld_scorch = 0.0f;
  float weld_slag = 0.0f;
  float rim_soot = 0.0f;
  float height = 0.0f;
};

[[nodiscard]] float asterSurfaceSmoothstep(float edge0, float edge1, float value);
[[nodiscard]] float asterSurfaceValueNoise(Vec3 position);
[[nodiscard]] float asterSurfaceProjectedFbm(Vec3 position, Vec3 normal, float scale, float salt,
                                             int octaves = 4);
[[nodiscard]] float asterSurfaceRidgedFbm(Vec3 position, Vec3 normal, float scale, float salt,
                                          int octaves = 4);
[[nodiscard]] AsterSurfaceCellSample asterSurfaceCellular(Vec3 position, float scale, float salt);
[[nodiscard]] Vec3 asterSurfaceTriplanarWeights(Vec3 normal, float sharpness = 4.0f);
[[nodiscard]] AsterPipeSurfaceSignals
sampleAsterPipeSurface(const AsterSurfaceDomain &domain, const ProceduralSurfaceLayer &layer,
                       float edge_wear, float pattern_depth);
[[nodiscard]] Vec3 perturbAsterSurfaceNormal(const AsterSurfaceDomain &domain,
                                             const ProceduralSurfaceLayer &layer,
                                             Vec3 tangent_hint, float strength);

} // namespace aster
