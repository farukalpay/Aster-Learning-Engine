// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/neural_irradiance_volume.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace aster {
namespace {

constexpr std::uint64_t kNivSeed = 0xA57E260212949ull;

[[nodiscard]] float clamp01(const float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

[[nodiscard]] float hashUnit(std::uint64_t seed, const std::uint64_t value) {
  const std::uint64_t mixed = hashCombine64(seed, value);
  return static_cast<float>((mixed >> 40u) & 0xffffffu) / static_cast<float>(0xffffffu);
}

[[nodiscard]] float signedWeight(std::uint64_t seed, const std::uint64_t layer,
                                 const std::uint64_t row, const std::uint64_t column) {
  const float unit =
      hashUnit(hashCombine64(hashCombine64(seed, layer), row), column);
  return unit * 2.0f - 1.0f;
}

[[nodiscard]] float stableFloatHashInput(const float value) {
  return std::isfinite(value) ? value : 0.0f;
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kNivSeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(
                       std::bit_cast<std::uint32_t>(stableFloatHashInput(value))));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const Vec3 value) {
  return mix(hash, stableHash64(value));
}

[[nodiscard]] Vec3 normalizedPosition(const NeuralIrradianceVolumeDesc &volume,
                                      const Vec3 position) {
  const Vec3 extent = volume.bounds_max - volume.bounds_min;
  return {extent.x == 0.0f ? 0.5f : clamp01((position.x - volume.bounds_min.x) / extent.x),
          extent.y == 0.0f ? 0.5f : clamp01((position.y - volume.bounds_min.y) / extent.y),
          extent.z == 0.0f ? 0.5f : clamp01((position.z - volume.bounds_min.z) / extent.z)};
}

[[nodiscard]] Vec3 featureAt(const NeuralIrradianceVolumeDesc &volume, const Vec3 position,
                             float &occupancy) {
  if (volume.cells.empty()) {
    occupancy = 0.0f;
    return {};
  }
  Vec3 feature{};
  float total_weight = 0.0f;
  occupancy = 0.0f;
  for (const NeuralIrradianceVolumeCell &cell : volume.cells) {
    const float dist = length(position - cell.center);
    const float weight = 1.0f / (0.08f + dist * dist);
    feature = feature + cell.feature * weight;
    occupancy += clamp01(cell.occupancy) * weight;
    total_weight += weight;
  }
  if (total_weight <= 0.0f) {
    occupancy = 0.0f;
    return {};
  }
  occupancy = clamp01(occupancy / total_weight);
  return feature / total_weight;
}

[[nodiscard]] float mlpActivation(const float value) {
  return std::tanh(value);
}

[[nodiscard]] std::array<float, 8>
evaluateHiddenLayer(const std::array<float, 16> &input, const std::uint64_t model_hash) {
  std::array<float, 8> hidden{};
  for (std::size_t row = 0; row < hidden.size(); ++row) {
    float sum = signedWeight(model_hash, 0u, row, 31u) * 0.18f;
    for (std::size_t column = 0; column < input.size(); ++column) {
      sum += input[column] *
             signedWeight(model_hash, 1u, row, static_cast<std::uint64_t>(column)) *
             (column < 6u ? 0.24f : 0.34f);
    }
    hidden[row] = mlpActivation(sum);
  }
  return hidden;
}

[[nodiscard]] float evaluateOutputNode(const std::array<float, 8> &hidden,
                                       const std::array<float, 16> &input,
                                       const std::uint64_t model_hash,
                                       const std::uint64_t node) {
  float sum = signedWeight(model_hash, 2u, node, 19u) * 0.14f;
  for (std::size_t i = 0; i < hidden.size(); ++i) {
    sum += hidden[i] *
           signedWeight(model_hash, 3u, node, static_cast<std::uint64_t>(i)) * 0.42f;
  }
  sum += input[6] * signedWeight(model_hash, 4u, node, 6u) * 0.30f;
  sum += input[7] * signedWeight(model_hash, 4u, node, 7u) * 0.24f;
  sum += input[10] * signedWeight(model_hash, 4u, node, 10u) * 0.20f;
  return 0.5f + 0.5f * mlpActivation(sum);
}

} // namespace

NeuralIrradianceVolumeDesc makeDefaultNeuralIrradianceVolume(const std::uint64_t seed) {
  NeuralIrradianceVolumeDesc volume;
  volume.model_hash = hashCombine64(kNivSeed, seed);
  volume.bounds_min = {-1.0f, -1.0f, -1.0f};
  volume.bounds_max = {1.0f, 1.0f, 1.0f};
  volume.cells.reserve(8u);
  for (int z = 0; z < 2; ++z) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        const std::uint64_t index = static_cast<std::uint64_t>(x + y * 2 + z * 4);
        const std::uint64_t cell_seed = hashCombine64(volume.model_hash, index);
        volume.cells.push_back(
            {.center = {-0.75f + static_cast<float>(x) * 1.50f,
                        -0.75f + static_cast<float>(y) * 1.50f,
                        -0.75f + static_cast<float>(z) * 1.50f},
             .feature = {0.18f + hashUnit(cell_seed, 11u) * 0.72f,
                         0.18f + hashUnit(cell_seed, 23u) * 0.72f,
                         0.18f + hashUnit(cell_seed, 37u) * 0.72f},
             .occupancy = 0.45f + hashUnit(cell_seed, 41u) * 0.45f});
      }
    }
  }
  return volume;
}

NeuralIrradianceSample
evaluateNeuralIrradianceVolume(const NeuralIrradianceVolumeDesc &volume,
                               const NeuralIrradianceQuery &query) {
  const std::uint64_t model_hash = volume.model_hash == 0u ? kNivSeed : volume.model_hash;
  const Vec3 normalized_position = normalizedPosition(volume, query.cell_position);
  const Vec3 normal = normalizeOr(query.normal, {0.0f, 1.0f, 0.0f});
  float occupancy = 0.0f;
  const Vec3 feature = featureAt(volume, normalized_position, occupancy);
  const float torch = clamp01(query.torch_intensity);
  const float fixture = clamp01(query.fixture_intensity);
  const float exposure = clamp01(query.exposure_age_seconds / 24.0f);
  const float wetness = clamp01(query.wetness);
  const float material = clamp01(query.material_memory);
  const float occlusion = clamp01(query.occlusion_trust);
  const float semantic_lod = clamp01(query.semantic_lod);
  const std::array<float, 16> input{normalized_position.x,
                                    normalized_position.y,
                                    normalized_position.z,
                                    normal.x * 0.5f + 0.5f,
                                    normal.y * 0.5f + 0.5f,
                                    normal.z * 0.5f + 0.5f,
                                    torch,
                                    fixture,
                                    exposure,
                                    wetness,
                                    material,
                                    occlusion,
                                    semantic_lod,
                                    feature.x,
                                    feature.y,
                                    feature.z};
  const std::array<float, 8> hidden = evaluateHiddenLayer(input, model_hash);
  const Vec3 neural = {evaluateOutputNode(hidden, input, model_hash, 0u),
                       evaluateOutputNode(hidden, input, model_hash, 1u),
                       evaluateOutputNode(hidden, input, model_hash, 2u)};
  const float local_light = clamp01(torch * 0.72f + fixture * 0.46f);
  const float wet_gain = 1.0f + wetness * 0.22f;
  const float trust = 0.38f + occlusion * 0.44f + semantic_lod * 0.18f;
  NeuralIrradianceSample sample;
  sample.diffuse_irradiance = {
      clamp01((neural.x * 0.20f + torch * 0.62f + fixture * 0.40f + material * 0.08f) *
              wet_gain),
      clamp01((neural.y * 0.16f + torch * 0.34f + fixture * 0.26f + exposure * 0.06f) *
              wet_gain),
      clamp01((neural.z * 0.12f + torch * 0.14f + fixture * 0.12f + wetness * 0.08f) *
              wet_gain)};
  sample.confidence =
      clamp01((0.22f + occupancy * 0.26f + trust * 0.42f + local_light * 0.10f) *
              (1.0f - wetness * 0.08f));
  std::uint64_t hash = hashCombine64(model_hash, 0x260212949ull);
  hash = mix(hash, normalized_position);
  hash = mix(hash, normal);
  hash = mix(hash, torch);
  hash = mix(hash, fixture);
  hash = mix(hash, exposure);
  hash = mix(hash, wetness);
  hash = mix(hash, material);
  hash = mix(hash, occlusion);
  hash = mix(hash, semantic_lod);
  hash = mix(hash, feature);
  hash = mix(hash, sample.diffuse_irradiance);
  hash = mix(hash, sample.confidence);
  sample.sample_hash = hash;
  return sample;
}

} // namespace aster
