// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/material/procedural_surface.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace aster {
namespace {

[[nodiscard]] float fractValue(const float value) {
  return value - std::floor(value);
}

[[nodiscard]] float ridge(const float value) {
  return 1.0f - std::abs(value * 2.0f - 1.0f);
}

[[nodiscard]] float hash31(Vec3 p) {
  p = {fractValue(p.x * 0.1031f), fractValue(p.y * 0.11369f), fractValue(p.z * 0.13787f)};
  const float d = p.x * (p.y + 19.19f) + p.y * (p.z + 19.19f) + p.z * (p.x + 19.19f);
  p = p + Vec3{d, d, d};
  return fractValue((p.x + p.y) * p.z);
}

[[nodiscard]] Vec3 safeNormal(Vec3 normal) {
  normal = normalize(normal);
  return length(normal) > 0.0001f ? normal : Vec3{0.0f, 1.0f, 0.0f};
}

[[nodiscard]] float projectedNoise(const Vec3 position, Vec3 normal, const float scale,
                                   const float salt) {
  normal = safeNormal(normal);
  Vec3 weights{std::pow(std::abs(normal.x), 4.0f), std::pow(std::abs(normal.y), 4.0f),
               std::pow(std::abs(normal.z), 4.0f)};
  const float weight_sum = std::max(weights.x + weights.y + weights.z, 0.0001f);
  weights = weights / weight_sum;
  const float xy = asterSurfaceValueNoise({position.x * scale, position.y * scale, salt});
  const float xz =
      asterSurfaceValueNoise({position.x * scale, position.z * scale, salt + 11.7f});
  const float zy =
      asterSurfaceValueNoise({position.z * scale, position.y * scale, salt + 23.4f});
  return xy * weights.z + xz * weights.y + zy * weights.x;
}

[[nodiscard]] Vec3 orthogonalTangent(Vec3 normal, Vec3 tangent_hint) {
  normal = safeNormal(normal);
  tangent_hint = tangent_hint - normal * dot(normal, tangent_hint);
  if (length(tangent_hint) > 0.0001f) {
    return normalize(tangent_hint);
  }
  return std::abs(normal.y) < 0.92f ? normalize(cross({0.0f, 1.0f, 0.0f}, normal))
                                    : normalize(cross({1.0f, 0.0f, 0.0f}, normal));
}

} // namespace

float asterSurfaceSmoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

float asterSurfaceValueNoise(const Vec3 position) {
  const Vec3 i{std::floor(position.x), std::floor(position.y), std::floor(position.z)};
  Vec3 f{fractValue(position.x), fractValue(position.y), fractValue(position.z)};
  f = f * f * (Vec3{3.0f, 3.0f, 3.0f} - f * 2.0f);

  const float n000 = hash31(i + Vec3{0.0f, 0.0f, 0.0f});
  const float n100 = hash31(i + Vec3{1.0f, 0.0f, 0.0f});
  const float n010 = hash31(i + Vec3{0.0f, 1.0f, 0.0f});
  const float n110 = hash31(i + Vec3{1.0f, 1.0f, 0.0f});
  const float n001 = hash31(i + Vec3{0.0f, 0.0f, 1.0f});
  const float n101 = hash31(i + Vec3{1.0f, 0.0f, 1.0f});
  const float n011 = hash31(i + Vec3{0.0f, 1.0f, 1.0f});
  const float n111 = hash31(i + Vec3{1.0f, 1.0f, 1.0f});

  const float nx00 = std::lerp(n000, n100, f.x);
  const float nx10 = std::lerp(n010, n110, f.x);
  const float nx01 = std::lerp(n001, n101, f.x);
  const float nx11 = std::lerp(n011, n111, f.x);
  return std::lerp(std::lerp(nx00, nx10, f.y), std::lerp(nx01, nx11, f.y), f.z);
}

float asterSurfaceProjectedFbm(const Vec3 position, const Vec3 normal, const float scale,
                               const float salt, const int octaves) {
  float sum = 0.0f;
  float amplitude = 0.56f;
  float amplitude_sum = 0.0f;
  float frequency = std::max(scale, 0.001f);
  for (int octave = 0; octave < std::max(octaves, 1); ++octave) {
    sum += projectedNoise(position, normal, frequency, salt + static_cast<float>(octave) * 17.0f) *
           amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.08f;
    amplitude *= 0.52f;
  }
  return amplitude_sum > 0.0f ? sum / amplitude_sum : 0.0f;
}

float asterSurfaceRidgedFbm(const Vec3 position, const Vec3 normal, const float scale,
                            const float salt, const int octaves) {
  float sum = 0.0f;
  float amplitude = 0.58f;
  float amplitude_sum = 0.0f;
  float frequency = std::max(scale, 0.001f);
  for (int octave = 0; octave < std::max(octaves, 1); ++octave) {
    sum += ridge(projectedNoise(position, normal, frequency,
                                salt + static_cast<float>(octave) * 23.0f)) *
           amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.18f;
    amplitude *= 0.50f;
  }
  return amplitude_sum > 0.0f ? sum / amplitude_sum : 0.0f;
}

AsterSurfaceCellSample asterSurfaceCellular(const Vec3 position, const float scale,
                                            const float salt) {
  const Vec3 p = position * std::max(scale, 0.001f) + Vec3{salt * 0.17f, salt * 0.07f, salt * 0.11f};
  const Vec3 base{std::floor(p.x), std::floor(p.y), std::floor(p.z)};
  float nearest = 1000.0f;
  float second = 1000.0f;
  float cell_id = 0.0f;
  for (int z = -1; z <= 1; ++z) {
    for (int y = -1; y <= 1; ++y) {
      for (int x = -1; x <= 1; ++x) {
        const Vec3 cell = base + Vec3{static_cast<float>(x), static_cast<float>(y),
                                      static_cast<float>(z)};
        const Vec3 jitter{hash31(cell + Vec3{1.7f, salt, 0.0f}),
                          hash31(cell + Vec3{0.0f, 3.1f, salt}),
                          hash31(cell + Vec3{salt, 0.0f, 5.3f})};
        const Vec3 feature = cell + jitter;
        const float d = length(p - feature);
        if (d < nearest) {
          second = nearest;
          nearest = d;
          cell_id = hash31(cell + Vec3{salt, salt * 0.5f, 9.1f});
        } else if (d < second) {
          second = d;
        }
      }
    }
  }
  const float edge_distance = std::max(second - nearest, 0.0f);
  return {.cell_id = cell_id,
          .distance = saturate(nearest),
          .edge_distance = saturate(edge_distance),
          .pitting = asterSurfaceSmoothstep(0.04f, 0.42f, 1.0f - nearest) *
                     (1.0f - asterSurfaceSmoothstep(0.08f, 0.62f, edge_distance))};
}

Vec3 asterSurfaceTriplanarWeights(Vec3 normal, const float sharpness) {
  normal = safeNormal(normal);
  Vec3 weights{std::pow(std::abs(normal.x), std::max(sharpness, 0.25f)),
               std::pow(std::abs(normal.y), std::max(sharpness, 0.25f)),
               std::pow(std::abs(normal.z), std::max(sharpness, 0.25f))};
  const float sum = std::max(weights.x + weights.y + weights.z, 0.0001f);
  return weights / sum;
}

AsterPipeSurfaceSignals sampleAsterPipeSurface(const AsterSurfaceDomain &domain,
                                               const ProceduralSurfaceLayer &layer,
                                               const float edge_wear,
                                               const float pattern_depth) {
  const float detail = std::max(domain.detail_scale, 0.001f);
  const Vec3 normal = safeNormal(domain.normal);
  const float oxide_layering = saturate(std::max(layer.oxide_layering, pattern_depth));
  const float pitting_density = std::max(layer.pitting_density, 0.0f);
  const float cavity_gain = saturate(layer.cavity_grime);
  const float wet_gain = saturate(layer.wetness + layer.wet_streaks * 0.12f);
  const float macro_breakup = std::clamp(layer.macro_frequency_breakup, 0.0f, 1.75f);
  const float micro_breakup = std::clamp(layer.micro_frequency_breakup, 0.0f, 1.75f);
  const float texel_density =
      std::clamp(layer.physical_texel_density / 768.0f, 0.55f, 1.85f);
  const float macro_detail = detail * (0.92f + macro_breakup * 0.36f);
  const float micro_detail = detail * (0.92f + micro_breakup * 0.42f) * std::sqrt(texel_density);

  const float broad =
      asterSurfaceProjectedFbm(domain.position, normal, macro_detail * 0.13f, 307.0f);
  const float bloom =
      asterSurfaceProjectedFbm(domain.position + Vec3{0.31f, -0.17f, 0.23f}, normal,
                               macro_detail * 0.052f, 617.0f, 5);
  const float smoky =
      asterSurfaceProjectedFbm(domain.position + Vec3{-0.29f, 0.21f, -0.11f}, normal,
                               macro_detail * 0.086f, 653.0f, 5);
  const float fine =
      asterSurfaceProjectedFbm(domain.position + normal * 0.053f, normal, micro_detail * 0.76f,
                               331.0f);
  const float ridged =
      asterSurfaceRidgedFbm(domain.position, normal, micro_detail * 1.42f, 359.0f);
  const AsterSurfaceCellSample cells =
      asterSurfaceCellular(domain.position + normal * 0.037f,
                           micro_detail * (0.42f + pitting_density),
                           283.0f);
  const float upward = asterSurfaceSmoothstep(0.10f, 0.84f, normal.y);
  const float axial_wave =
      0.5f + 0.5f * std::sin((domain.position.x * (2.7f + detail * 0.10f) + fine * 4.2f) +
                              domain.uv.y * 13.0f);
  const float circumferential =
      0.5f + 0.5f * std::sin((domain.uv.y * 15.0f + domain.position.x * 0.65f + broad * 3.0f));
  const auto axial_band = [](const float x, const float center, const float inner,
                             const float outer) {
    return 1.0f - asterSurfaceSmoothstep(inner, outer, std::abs(x - center));
  };
  const float weld_zone =
      std::max(axial_band(domain.position.x, -1.55f, 0.05f, 0.34f),
               axial_band(domain.position.x, 1.42f, 0.05f, 0.34f));
  const float rim_zone = asterSurfaceSmoothstep(2.44f, 2.60f, std::abs(domain.position.x));

  AsterPipeSurfaceSignals out;
  out.broad_oxide =
      saturate(broad * 0.54f + ridged * 0.20f + upward * oxide_layering * 0.34f);
  out.fine_oxide = saturate(fine * 0.70f + ridged * 0.30f);
  out.pit = saturate(cells.pitting * (0.10f + pitting_density * 0.14f) +
                     ridged * (0.05f + pitting_density * 0.035f) +
                     fine * std::min(layer.pitting_depth * 5.0f, 0.10f));
  out.pit_edge = saturate((1.0f - cells.edge_distance) * out.pit * 0.34f +
                          ridged * 0.075f + fine * 0.025f);
  out.cavity_grime = saturate(cavity_gain *
                                  (0.20f + ridged * 0.42f + out.pit_edge * 0.55f +
                                   out.pit * 0.24f) +
                              (1.0f - upward) * cavity_gain * 0.18f);
  out.edge_polish = saturate(edge_wear * (0.12f + asterSurfaceRidgedFbm(domain.position, normal,
                                               micro_detail * 1.9f, 389.0f) *
                                                0.22f) *
                             (0.20f + layer.edge_polish * 0.40f));
  out.wet_film =
      saturate(wet_gain * (0.18f + asterSurfaceSmoothstep(0.55f, 0.98f, axial_wave) * 0.62f) *
               (0.65f + out.cavity_grime * 0.35f));
  out.axial_scratch =
      saturate(layer.axial_scratches *
               asterSurfaceSmoothstep(0.52f, 0.96f, circumferential * 0.62f + ridged * 0.38f));
  out.heat_tint = saturate(layer.weld_heat_tint *
                           asterSurfaceSmoothstep(0.26f, 0.90f, fine * 0.55f + axial_wave * 0.45f));
  out.rust_bloom = saturate(layer.rust_bloom *
                            asterSurfaceSmoothstep(0.28f, 0.78f,
                                                   bloom * 0.54f + broad * 0.30f +
                                                       out.broad_oxide * 0.18f +
                                                       (1.0f - upward) * 0.10f));
  out.weld_scorch =
      saturate(weld_zone * (0.28f + layer.weld_heat_tint * 0.42f + smoky * 0.30f));
  out.rim_soot =
      saturate(rim_zone * layer.rim_soot * (0.32f + out.cavity_grime * 0.36f + smoky * 0.32f));
  out.weld_slag =
      saturate(weld_zone * layer.weld_slag * (0.22f + ridged * 0.38f + out.pit_edge * 0.16f));
  out.paint_remnant =
      saturate(layer.paint_remnant *
               asterSurfaceSmoothstep(0.63f, 0.92f,
                                      asterSurfaceRidgedFbm(domain.position + Vec3{0.12f, 0.0f, 0.37f},
                                                            normal, macro_detail * 0.38f, 691.0f) *
                                              0.54f +
                                          (1.0f - out.broad_oxide) * 0.22f + upward * 0.10f));
  out.black_scab =
      saturate(layer.black_scab *
               asterSurfaceSmoothstep(0.38f, 0.84f,
                                      smoky * 0.46f + out.cavity_grime * 0.30f +
                                          out.weld_scorch * 0.34f + out.rim_soot * 0.42f +
                                          (1.0f - upward) * 0.16f));
  out.black_oxide = asterSurfaceSmoothstep(0.22f, 0.76f,
                                           out.broad_oxide * (0.26f + out.cavity_grime * 0.62f) +
                                               out.pit * 0.16f + out.pit_edge * 0.08f +
                                               out.black_scab * 0.48f);
  out.orange_rust = asterSurfaceSmoothstep(0.25f, 0.80f,
                                           out.broad_oxide * (0.20f + fine * 0.30f) +
                                               out.rust_bloom * 0.48f + out.pit_edge * 0.06f +
                                               ridged * 0.10f - out.wet_film * 0.10f);
  out.height = saturate(layer.height_shading *
                        (0.04f + out.pit * 0.16f + ridged * 0.09f +
                         out.cavity_grime * 0.12f + out.rust_bloom * 0.10f +
                         out.black_scab * 0.13f + out.weld_scorch * 0.09f +
                         out.weld_slag * 0.08f + out.rim_soot * 0.08f +
                         out.axial_scratch * 0.055f));
  return out;
}

Vec3 perturbAsterSurfaceNormal(const AsterSurfaceDomain &domain,
                               const ProceduralSurfaceLayer &layer, const Vec3 tangent_hint,
                               const float strength) {
  const Vec3 normal = safeNormal(domain.normal);
  const Vec3 tangent = orthogonalTangent(normal, tangent_hint);
  const Vec3 bitangent = normalize(cross(normal, tangent));
  const float detail = std::max(domain.detail_scale, 0.001f);
  const float density = std::clamp(layer.physical_texel_density / 768.0f, 0.55f, 1.85f);
  const float coupling = std::clamp(layer.height_normal_coupling, 0.25f, 1.50f);
  const float step = (0.012f / std::sqrt(detail + 1.0f)) / std::sqrt(density);
  const auto height_at = [&](const Vec3 position) {
    const AsterPipeSurfaceSignals signals = sampleAsterPipeSurface(
        {.position = position, .normal = normal, .uv = domain.uv, .detail_scale = detail}, layer,
        layer.edge_polish, layer.height_shading);
    return signals.height + signals.pit * layer.pitting_depth * 1.45f +
           signals.axial_scratch * 0.070f + signals.rust_bloom * 0.050f +
           signals.black_scab * 0.095f + signals.weld_scorch * 0.075f +
           signals.weld_slag * 0.065f + signals.rim_soot * 0.070f -
           signals.cavity_grime * 0.13f;
  };
  const float center = height_at(domain.position);
  const float dx = height_at(domain.position + tangent * step) - center;
  const float dy = height_at(domain.position + bitangent * step) - center;
  const float gradient_gain = 8.2f + coupling * 5.4f;
  const Vec3 adjusted =
      normalize(normal - tangent * (dx * strength * gradient_gain) -
                bitangent * (dy * strength * gradient_gain));
  return length(adjusted) > 0.0001f ? adjusted : normal;
}

} // namespace aster
