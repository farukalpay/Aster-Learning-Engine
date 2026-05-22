// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct NeuralIrradianceVolumeCell {
  Vec3 center{};
  Vec3 feature{};
  float occupancy = 0.0f;
};

struct NeuralIrradianceVolumeDesc {
  std::uint64_t model_hash = 0u;
  Vec3 bounds_min{-1.0f, -1.0f, -1.0f};
  Vec3 bounds_max{1.0f, 1.0f, 1.0f};
  std::vector<NeuralIrradianceVolumeCell> cells;
};

struct NeuralIrradianceQuery {
  Vec3 cell_position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float torch_intensity = 0.0f;
  float fixture_intensity = 0.0f;
  float exposure_age_seconds = 0.0f;
  float wetness = 0.0f;
  float material_memory = 0.0f;
  float occlusion_trust = 0.0f;
  float semantic_lod = 0.0f;
};

struct NeuralIrradianceSample {
  Vec3 diffuse_irradiance{};
  float confidence = 0.0f;
  std::uint64_t sample_hash = 0u;
};

[[nodiscard]] NeuralIrradianceVolumeDesc
makeDefaultNeuralIrradianceVolume(std::uint64_t seed = 0xA57E260212949ull);

[[nodiscard]] NeuralIrradianceSample
evaluateNeuralIrradianceVolume(const NeuralIrradianceVolumeDesc &volume,
                               const NeuralIrradianceQuery &query);

} // namespace aster
