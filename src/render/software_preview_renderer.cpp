// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/software_preview_renderer.hpp"

#include "aster/math/color.hpp"
#include "aster/material/procedural_surface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = kPi * 2.0f;

struct Ray {
  Vec3 origin{};
  Vec3 direction{};
};

struct Hit {
  bool valid = false;
  float distance = std::numeric_limits<float>::max();
  Vec3 position{};
  Vec3 local_position{};
  Vec3 normal{};
  Vec3 tangent{1.0f, 0.0f, 0.0f};
  Vec2 uv{};
  Material material{};
  std::uint64_t object_label_hash = 0u;
};

struct TraceTriangle {
  Vec3 a{};
  Vec3 b{};
  Vec3 c{};
  Vec3 la{};
  Vec3 lb{};
  Vec3 lc{};
  Vec3 na{};
  Vec3 nb{};
  Vec3 nc{};
  Vec3 ta{1.0f, 0.0f, 0.0f};
  Vec3 tb{1.0f, 0.0f, 0.0f};
  Vec3 tc{1.0f, 0.0f, 0.0f};
  Vec2 uva{};
  Vec2 uvb{};
  Vec2 uvc{};
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  Vec3 centroid{};
};

struct TraceBvhNode {
  Vec3 bounds_min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  Vec3 bounds_max{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                  -std::numeric_limits<float>::max()};
  std::uint32_t first = 0u;
  std::uint32_t count = 0u;
  std::uint32_t left = 0u;
  std::uint32_t right = 0u;
};

struct PreparedObject {
  RenderObject object{};
  std::vector<TraceTriangle> triangles;
  std::vector<std::uint32_t> triangle_order;
  std::vector<TraceBvhNode> bvh_nodes;
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  std::uint64_t object_label_hash = 0u;
  bool casts_scene_shadow = false;
};

float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

float fractValue(const float value) {
  return value - std::floor(value);
}

float ridge(const float value) {
  return 1.0f - std::abs(value * 2.0f - 1.0f);
}

Vec3 mixVec(const Vec3 a, const Vec3 b, const float t) {
  const float amount = saturate(t);
  return a * (1.0f - amount) + b * amount;
}

float evaluateFogFactor(const AtmosphereSettings &atmosphere, const float distance_to_camera) {
  if (!atmosphere.enabled || atmosphere.fog_strength <= 0.0f) {
    return 0.0f;
  }
  const float range = std::max(atmosphere.fog_end - atmosphere.fog_start, 0.001f);
  const float normalized = std::max((distance_to_camera - atmosphere.fog_start) / range, 0.0f);
  const float power = std::max(atmosphere.fog_power, 0.001f);
  float curve = 0.0f;
  switch (atmosphere.fog_falloff) {
  case AtmosphereFogFalloff::SmoothLinear:
    curve = smoothstep(0.0f, 1.0f, normalized);
    break;
  case AtmosphereFogFalloff::Exponential:
    curve = 1.0f - std::exp(-normalized * power);
    break;
  case AtmosphereFogFalloff::Powered:
    curve = std::pow(saturate(normalized), power);
    break;
  }
  return saturate(curve * std::clamp(atmosphere.fog_strength, 0.0f, 1.0f));
}

Vec3 snapProceduralSamplePosition(const Vec3 world_position, const RenderStyleProfile &style) {
  const float step = std::max(style.procedural_sample_snap, 0.0f);
  if (step <= 0.0001f) {
    return world_position;
  }
  return {std::floor(world_position.x / step + 0.5f) * step,
          std::floor(world_position.y / step + 0.5f) * step,
          std::floor(world_position.z / step + 0.5f) * step};
}

Vec3 applyRenderStylePost(Vec3 color, const RenderStyleProfile &style) {
  const float luma_crush = std::clamp(style.luma_crush, 0.0f, 1.0f);
  if (luma_crush > 0.0001f) {
    const float luma = color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
    const float dark_weight = 1.0f - smoothstep(0.10f, 0.58f, luma);
    color = mixVec(color, color * (0.58f + luma * 0.42f), dark_weight * luma_crush);
  }
  const float steps = std::floor(std::max(style.color_quantization_steps, 0.0f));
  if (steps > 1.0f) {
    color = {std::floor(saturate(color.x) * steps + 0.5f) / steps,
             std::floor(saturate(color.y) * steps + 0.5f) / steps,
             std::floor(saturate(color.z) * steps + 0.5f) / steps};
  }
  return color;
}

std::uint64_t stableLabelHash(const std::string_view text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

float hash31(Vec3 p) {
  p = {fractValue(p.x * 0.1031f), fractValue(p.y * 0.11369f), fractValue(p.z * 0.13787f)};
  const float d = p.x * (p.y + 19.19f) + p.y * (p.z + 19.19f) + p.z * (p.x + 19.19f);
  p = p + Vec3{d, d, d};
  return fractValue((p.x + p.y) * p.z);
}

float valueNoise(const Vec3 p) {
  const Vec3 i{std::floor(p.x), std::floor(p.y), std::floor(p.z)};
  Vec3 f{fractValue(p.x), fractValue(p.y), fractValue(p.z)};
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

float projectedNoise(const Vec3 world_position, Vec3 normal, const float scale, const float salt) {
  normal = normalize(normal);
  if (length(normal) <= 0.0001f) {
    normal = {0.0f, 1.0f, 0.0f};
  }
  Vec3 weights{std::pow(std::abs(normal.x), 4.0f), std::pow(std::abs(normal.y), 4.0f),
               std::pow(std::abs(normal.z), 4.0f)};
  const float weight_sum = std::max(weights.x + weights.y + weights.z, 0.0001f);
  weights = weights / weight_sum;
  const float xy = valueNoise({world_position.x * scale, world_position.y * scale, salt});
  const float xz = valueNoise({world_position.x * scale, world_position.z * scale, salt + 11.7f});
  const float zy = valueNoise({world_position.z * scale, world_position.y * scale, salt + 23.4f});
  return xy * weights.z + xz * weights.y + zy * weights.x;
}

float projectedFbm(const Vec3 world_position, const Vec3 normal, const float scale,
                   const float salt) {
  float sum = 0.0f;
  float amplitude = 0.56f;
  float amplitude_sum = 0.0f;
  float frequency = std::max(scale, 0.001f);
  for (int octave = 0; octave < 4; ++octave) {
    sum += projectedNoise(world_position, normal, frequency,
                          salt + static_cast<float>(octave) * 17.0f) *
           amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.08f;
    amplitude *= 0.52f;
  }
  return amplitude_sum > 0.0f ? sum / amplitude_sum : 0.0f;
}

float luminanceOf(const Vec3 color) {
  return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
}

Vec3 compressLocalRadiance(const Vec3 radiance, const float soft_limit) {
  const float limit = std::max(soft_limit, 0.0001f);
  const float luma = std::max(luminanceOf(radiance), 0.0f);
  if (luma <= limit) {
    return radiance;
  }
  const float overflow = (luma - limit) / limit;
  const float compressed_luma = limit * (1.0f + (1.0f - std::exp(-overflow)) * 0.55f);
  return radiance * (compressed_luma / std::max(luma, 0.0001f));
}

Vec3 applyAtmosphereGrade(Vec3 color, const AtmosphereSettings &atmosphere) {
  const float pre_grade_luma = luminanceOf(color);
  color = mixVec(color, color * atmosphere.shadow_tint,
                 std::clamp(atmosphere.shadow_tint_strength, 0.0f, 1.0f) *
                     (1.0f - smoothstep(0.05f, 0.48f, pre_grade_luma)));
  color = mixVec(color, color * atmosphere.highlight_tint,
                 std::clamp(atmosphere.highlight_tint_strength, 0.0f, 1.0f) *
                     smoothstep(0.52f, 1.45f, pre_grade_luma));
  const float saturation = std::max(atmosphere.saturation, 0.0f);
  if (std::abs(saturation - 1.0f) > 0.0001f) {
    const float luma = luminanceOf(color);
    color = mixVec(Vec3{luma, luma, luma}, color, saturation);
  }
  const float contrast = std::max(atmosphere.contrast, 0.0f);
  if (std::abs(contrast - 1.0f) > 0.0001f) {
    color = (color - Vec3{0.5f, 0.5f, 0.5f}) * contrast + Vec3{0.5f, 0.5f, 0.5f};
  }
  return clamp(color, 0.0f, 1.0f);
}

Vec3 cameraResponseTonemap(Vec3 radiance, const RendererSettings &settings) {
  radiance = clamp(radiance, 0.0f, 64.0f) * std::max(settings.exposure, 0.0f);
  radiance = radiance * Vec3{1.025f, 1.0f, 0.965f};
  const float scene_luma = std::max(luminanceOf(radiance), 0.0f);
  const float toe = 1.0f - smoothstep(0.018f, 0.22f, scene_luma);
  radiance = mixVec(radiance, radiance * 0.72f + Vec3{0.010f, 0.011f, 0.012f}, toe * 0.42f);
  Vec3 mapped = settings.use_aces_tonemap || settings.pipeline.tone_mapper == ToneMapper::FilmicAces
                    ? aces_tonemap(radiance)
                    : reinhard_tonemap(radiance);
  const float mapped_luma = luminanceOf(mapped);
  const float shoulder = smoothstep(0.62f, 0.96f, mapped_luma);
  const Vec3 shoulder_mapped{mapped.x / (1.0f + mapped.x * 0.34f),
                             mapped.y / (1.0f + mapped.y * 0.34f),
                             mapped.z / (1.0f + mapped.z * 0.34f)};
  mapped = mixVec(mapped, shoulder_mapped, shoulder * 0.22f);
  const float local_chroma = std::clamp(0.92f + smoothstep(0.10f, 0.72f, mapped_luma) * 0.12f,
                                        0.88f, 1.04f);
  mapped = mixVec(Vec3{mapped_luma, mapped_luma, mapped_luma}, mapped, local_chroma);
  return clamp(mapped, 0.0f, 1.0f);
}

float maxComponent(const Vec3 value) {
  return std::max({value.x, value.y, value.z});
}

Vec3 orthogonalTangent(Vec3 tangent, const Vec3 normal) {
  tangent = tangent - normal * dot(tangent, normal);
  if (length(tangent) <= 0.0001f) {
    tangent = cross(std::abs(normal.y) < 0.86f ? Vec3{0.0f, 1.0f, 0.0f}
                                               : Vec3{1.0f, 0.0f, 0.0f},
                    normal);
  }
  return length(tangent) > 0.0001f ? normalize(tangent) : Vec3{1.0f, 0.0f, 0.0f};
}

Vec3 materialSamplePosition(const Hit &hit) {
  const MaterialSurfaceProfile profile = resolveMaterialSurfaceProfile(hit.material);
  switch (profile) {
  case MaterialSurfaceProfile::TerrainLayer:
  case MaterialSurfaceProfile::Masonry:
  case MaterialSurfaceProfile::CorrodedMetal:
  case MaterialSurfaceProfile::WeldBead:
  case MaterialSurfaceProfile::ContactShadow:
    return hit.position;
  case MaterialSurfaceProfile::Auto:
  case MaterialSurfaceProfile::Plain:
  case MaterialSurfaceProfile::OrganicFiber:
  case MaterialSurfaceProfile::Liquid:
  case MaterialSurfaceProfile::Foliage:
  case MaterialSurfaceProfile::Resin:
  case MaterialSurfaceProfile::PaintedWood:
  case MaterialSurfaceProfile::Feather:
  case MaterialSurfaceProfile::Scales:
  case MaterialSurfaceProfile::StratifiedRock:
  case MaterialSurfaceProfile::MineralVein:
  case MaterialSurfaceProfile::FilamentWeb:
  case MaterialSurfaceProfile::ChitinShell:
  case MaterialSurfaceProfile::EmissiveLens:
  case MaterialSurfaceProfile::BiologicalIntegument:
    break;
  }
  return hit.local_position;
}

float skyRayleighPhase(const float cos_theta) {
  return 0.75f * (1.0f + cos_theta * cos_theta);
}

float skyMiePhase(const float cos_theta, const float anisotropy) {
  const float g = std::clamp(anisotropy, 0.0f, 0.92f);
  const float g2 = g * g;
  return (1.0f - g2) /
         std::max(std::pow(1.0f + g2 - 2.0f * g * cos_theta, 1.5f), 0.001f);
}

Vec3 environmentRadiance(const Vec3 direction, const RendererSettings &settings,
                         const std::vector<ReflectionProbe> &probes = {}) {
  const Vec3 dir = normalize(direction);
  const Vec3 sun_dir =
      settings.sun_light.enabled && length(settings.sun_light.direction_to_light) > 0.0001f
          ? normalize(settings.sun_light.direction_to_light)
          : Vec3{-0.42f, 0.74f, 0.34f};
  const float up = saturate(dir.y * 0.5f + 0.5f);
  const float sky = smoothstep(-0.08f, 0.58f, dir.y);
  const float horizon = std::exp(-std::abs(dir.y) * 7.5f);
  const float mu = std::clamp(dot(dir, sun_dir), -1.0f, 1.0f);
  const float optical_mass = 1.0f / (saturate(dir.y) + 0.18f);
  const Vec3 lower_air =
      mixVec(Vec3{0.135f, 0.185f, 0.285f},
             settings.atmosphere.enabled ? settings.atmosphere.fog_color * Vec3{0.88f, 0.98f, 1.22f}
                                         : settings.pipeline.clear_color * 1.05f,
             0.42f);
  const Vec3 zenith_air = settings.sky_ambient_color * Vec3{0.18f, 0.36f, 0.86f} +
                          Vec3{0.030f, 0.070f, 0.180f};
  const Vec3 rayleigh = Vec3{0.30f, 0.52f, 0.92f} *
                        (skyRayleighPhase(mu) * (0.09f + 0.14f * sky) /
                         std::sqrt(optical_mass));
  const Vec3 mie = settings.sun_light.color *
                   (skyMiePhase(mu, 0.72f) * (0.013f + 0.006f * horizon) *
                    std::clamp(settings.sun_light.intensity, 0.0f, 8.0f));
  Vec3 color = mixVec(lower_air, zenith_air, sky) + rayleigh + mie;

  const Vec3 cloud_domain{dir.x / std::max(0.28f + up, 0.18f),
                          dir.y * 0.72f + 0.15f,
                          dir.z / std::max(0.28f + up, 0.18f)};
  const float cloud_low = valueNoise(cloud_domain * 2.7f + Vec3{31.0f, 4.0f, -17.0f});
  const float cloud_high =
      valueNoise(cloud_domain * 6.4f + Vec3{-11.0f, 19.0f, 41.0f}) * 0.55f +
      valueNoise(cloud_domain * 12.0f + Vec3{7.0f, -3.0f, 23.0f}) * 0.45f;
  const float cloud_shape =
      smoothstep(0.40f, 0.72f, cloud_low * 0.68f + cloud_high * 0.32f) *
      smoothstep(0.02f, 0.40f, dir.y) * (1.0f - smoothstep(0.78f, 1.0f, dir.y));
  const float cloud_streak =
      smoothstep(0.18f, 0.68f,
                 valueNoise({dir.x * 3.2f + dir.z * 1.4f + 9.0f, dir.y * 2.0f + 4.0f,
                             dir.z * 2.8f - dir.x * 0.8f})) *
      smoothstep(-0.04f, 0.30f, dir.y) * (1.0f - smoothstep(0.58f, 0.94f, dir.y));
  const float silver = std::pow(saturate(mu), 14.0f);
  const Vec3 cloud_color =
      mixVec(Vec3{0.42f, 0.48f, 0.55f}, Vec3{0.82f, 0.86f, 0.90f}, 0.45f + silver * 0.35f);
  color = mixVec(color, cloud_color + settings.sun_light.color * (silver * 0.24f),
                 std::max(cloud_shape * 0.46f, cloud_streak * 0.34f));

  const Vec3 horizon_haze =
      (settings.atmosphere.enabled ? settings.atmosphere.fog_color : lower_air) *
          Vec3{0.96f, 1.04f, 1.18f} +
      settings.sun_light.color * (std::pow(saturate(mu), 5.0f) * 0.12f);
  color = mixVec(color, horizon_haze, horizon * (0.30f + 0.36f * (1.0f - sky)));
  const float aerial_shelf =
      horizon * (0.42f + std::pow(saturate(mu), 3.0f) * 0.24f) *
      (0.74f + valueNoise({dir.x * 5.0f + 17.0f, dir.y * 3.0f, dir.z * 4.0f - 9.0f}) * 0.26f);
  color = color + mixVec(Vec3{0.030f, 0.050f, 0.082f}, horizon_haze, 0.56f) * aerial_shelf * 0.32f;
  if (!probes.empty()) {
    Vec3 probe_sum{};
    float probe_weight = 0.0f;
    for (const ReflectionProbe &probe : probes) {
      const float weight = std::max(probe.intensity, 0.0f);
      const Vec3 probe_light =
          mixVec(probe.ground_irradiance, probe.sky_irradiance, smoothstep(-0.35f, 0.95f, dir.y)) *
          probe.specular_tint;
      probe_sum = probe_sum + probe_light * weight;
      probe_weight += weight;
    }
    if (probe_weight > 0.0001f) {
      color = mixVec(color, probe_sum / probe_weight, std::clamp(probe_weight * 0.20f, 0.0f, 0.52f));
    }
  }
  const float studio_key =
      smoothstep(0.82f, 0.995f, dot(dir, normalize(Vec3{-0.58f, 0.54f, 0.62f})));
  const float studio_rim =
      smoothstep(0.84f, 0.996f, dot(dir, normalize(Vec3{0.72f, 0.34f, -0.58f})));
  const float upper_strip =
      smoothstep(0.62f, 0.92f, dir.y) *
      (1.0f - smoothstep(0.12f, 0.90f, std::abs(dir.x * 0.45f + dir.z * 0.15f)));
  color = color + settings.sun_light.color * (studio_key * 1.10f) +
          Vec3{0.42f, 0.54f, 0.72f} * (studio_rim * 0.42f) +
          Vec3{0.36f, 0.44f, 0.54f} * (upper_strip * 0.22f);
  if (settings.sun_light.enabled && settings.sun_light.intensity > 0.0f) {
    const float sun_dot = std::max(dot(dir, sun_dir), 0.0f);
    const float disk = std::pow(sun_dot, 240.0f);
    const float bloom = std::pow(sun_dot, 18.0f);
    color = color + settings.sun_light.color * settings.sun_light.intensity *
                        (disk * 0.055f + bloom * 0.018f);
  }
  return clamp(color, 0.0f, 8.0f);
}

std::uint8_t toByte(const float value) {
  return static_cast<std::uint8_t>(saturate(value) * 255.0f + 0.5f);
}

float axisValue(Vec3 value, int axis);

Vec3 minVec3(const Vec3 a, const Vec3 b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3 maxVec3(const Vec3 a, const Vec3 b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

void expandBounds(PreparedObject &out, const Vec3 point) {
  out.bounds_min.x = std::min(out.bounds_min.x, point.x);
  out.bounds_min.y = std::min(out.bounds_min.y, point.y);
  out.bounds_min.z = std::min(out.bounds_min.z, point.z);
  out.bounds_max.x = std::max(out.bounds_max.x, point.x);
  out.bounds_max.y = std::max(out.bounds_max.y, point.y);
  out.bounds_max.z = std::max(out.bounds_max.z, point.z);
}

void expandBounds(Vec3 &bounds_min, Vec3 &bounds_max, const Vec3 point) {
  bounds_min = minVec3(bounds_min, point);
  bounds_max = maxVec3(bounds_max, point);
}

void expandBounds(Vec3 &bounds_min, Vec3 &bounds_max, const Vec3 other_min,
                  const Vec3 other_max) {
  bounds_min = minVec3(bounds_min, other_min);
  bounds_max = maxVec3(bounds_max, other_max);
}

std::uint32_t buildPreparedBvhNode(PreparedObject &out, const std::uint32_t first,
                                   const std::uint32_t count) {
  constexpr std::uint32_t kLeafTriangleCount = 6u;
  TraceBvhNode node;
  Vec3 centroid_min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
  Vec3 centroid_max{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()};
  for (std::uint32_t i = first; i < first + count; ++i) {
    const TraceTriangle &triangle = out.triangles[out.triangle_order[i]];
    expandBounds(node.bounds_min, node.bounds_max, triangle.bounds_min, triangle.bounds_max);
    expandBounds(centroid_min, centroid_max, triangle.centroid);
  }

  const std::uint32_t node_index = static_cast<std::uint32_t>(out.bvh_nodes.size());
  out.bvh_nodes.push_back(node);
  const Vec3 centroid_extent = centroid_max - centroid_min;
  int axis = 0;
  if (centroid_extent.y > centroid_extent.x && centroid_extent.y >= centroid_extent.z) {
    axis = 1;
  } else if (centroid_extent.z > centroid_extent.x && centroid_extent.z > centroid_extent.y) {
    axis = 2;
  }
  const float split_extent = axisValue(centroid_extent, axis);
  if (count <= kLeafTriangleCount || split_extent <= 0.0001f) {
    out.bvh_nodes[node_index].first = first;
    out.bvh_nodes[node_index].count = count;
    return node_index;
  }

  const std::uint32_t mid = first + count / 2u;
  std::nth_element(out.triangle_order.begin() + first, out.triangle_order.begin() + mid,
                   out.triangle_order.begin() + first + count,
                   [&](const std::uint32_t lhs, const std::uint32_t rhs) {
                     return axisValue(out.triangles[lhs].centroid, axis) <
                            axisValue(out.triangles[rhs].centroid, axis);
                   });
  const std::uint32_t left = buildPreparedBvhNode(out, first, mid - first);
  const std::uint32_t right = buildPreparedBvhNode(out, mid, first + count - mid);
  out.bvh_nodes[node_index].left = left;
  out.bvh_nodes[node_index].right = right;
  return node_index;
}

void buildPreparedBvh(PreparedObject &out) {
  out.triangle_order.clear();
  out.bvh_nodes.clear();
  if (out.triangles.size() <= 6u) {
    return;
  }
  out.triangle_order.resize(out.triangles.size());
  for (std::uint32_t i = 0u; i < out.triangle_order.size(); ++i) {
    out.triangle_order[i] = i;
  }
  out.bvh_nodes.reserve(out.triangles.size() * 2u);
  buildPreparedBvhNode(out, 0u, static_cast<std::uint32_t>(out.triangle_order.size()));
}

void prepareTriangleMesh(PreparedObject &out, const CpuMesh &mesh) {
  const Mat4 model = out.object.transform.matrix();
  const WorldFromLocal world_from_local{model};
  const MathResult<WorldNormalFromLocal> normal_from_local = worldNormalFromLocal(world_from_local);
  out.bounds_min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
  out.bounds_max = {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()};
  out.triangles.reserve(mesh.indices.size() / 3u);

  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const Vertex &va = mesh.vertices[mesh.indices[i + 0u]];
    const Vertex &vb = mesh.vertices[mesh.indices[i + 1u]];
    const Vertex &vc = mesh.vertices[mesh.indices[i + 2u]];
    const Vec3 a = transformPoint(model, va.position);
    const Vec3 b = transformPoint(model, vb.position);
    const Vec3 c = transformPoint(model, vc.position);
    const Vec3 face = normalize(cross(b - a, c - a));
    if (length(face) <= 0.0001f) {
      continue;
    }
    const auto transformed_normal = [&](const Vec3 normal, const Vec3 fallback) {
      if (length(normal) <= 0.0001f) {
        return fallback;
      }
      if (normal_from_local) {
        const Vec3 transformed =
            transformNormal(Normal{normal}, normal_from_local.value).value;
        return length(transformed) > 0.0001f ? transformed : fallback;
      }
      const Vec3 transformed = normalizeOr(transformVector(model, normal), fallback);
      return length(transformed) > 0.0001f ? transformed : fallback;
    };
    const Vec3 na = transformed_normal(va.normal, face);
    const Vec3 nb = transformed_normal(vb.normal, face);
    const Vec3 nc = transformed_normal(vc.normal, face);
    const auto transformed_tangent = [&](const Vertex &vertex, const Vec3 fallback_normal) {
      Vec3 tangent = transformVector(model, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
      tangent = tangent - fallback_normal * dot(tangent, fallback_normal);
      if (length(tangent) <= 0.0001f) {
        tangent = cross(std::abs(fallback_normal.y) < 0.86f ? Vec3{0.0f, 1.0f, 0.0f}
                                                            : Vec3{1.0f, 0.0f, 0.0f},
                        fallback_normal);
      }
      return length(tangent) > 0.0001f ? normalize(tangent) : Vec3{1.0f, 0.0f, 0.0f};
    };
    const Vec3 safe_na = length(na) > 0.0001f ? na : face;
    const Vec3 safe_nb = length(nb) > 0.0001f ? nb : face;
    const Vec3 safe_nc = length(nc) > 0.0001f ? nc : face;
    const Vec3 bounds_min = minVec3(a, minVec3(b, c));
    const Vec3 bounds_max = maxVec3(a, maxVec3(b, c));
    const Vec3 centroid = (a + b + c) / 3.0f;
    out.triangles.push_back({a,
                             b,
                             c,
                             va.position,
                             vb.position,
                             vc.position,
                             safe_na,
                             safe_nb,
                             safe_nc,
                             transformed_tangent(va, safe_na),
                             transformed_tangent(vb, safe_nb),
                             transformed_tangent(vc, safe_nc),
                             va.uv,
                             vb.uv,
                             vc.uv,
                             bounds_min,
                             bounds_max,
                             centroid});
    expandBounds(out, a);
    expandBounds(out, b);
    expandBounds(out, c);
  }
  buildPreparedBvh(out);
}

const CpuMesh &primitiveMesh(const MeshPrimitive primitive) {
  static const CpuMesh box = makeBox();
  static const CpuMesh sphere = makeUvSphere(64, 32, 1.0f);
  static const CpuMesh plane = makePlane(12.0f);
  static const CpuMesh rock = makeRock(36, 20, 1.0f);
  static const CpuMesh crystal = makeCrystal(8, 1.0f, 1.8f);
  static const CpuMesh ruin = makeRuinBlock();
  static const CpuMesh pillar = makePillar(18, 1.0f, 1.0f);
  switch (primitive) {
  case MeshPrimitive::Box:
    return box;
  case MeshPrimitive::Sphere:
    return sphere;
  case MeshPrimitive::Plane:
    return plane;
  case MeshPrimitive::Rock:
    return rock;
  case MeshPrimitive::Crystal:
    return crystal;
  case MeshPrimitive::RuinBlock:
    return ruin;
  case MeshPrimitive::Pillar:
    return pillar;
  }
  return sphere;
}

std::vector<PreparedObject> prepareScene(const Scene &scene) {
  std::vector<PreparedObject> prepared;
  prepared.reserve(scene.objects().size());
  for (const RenderObject &object : scene.objects()) {
    PreparedObject out;
    out.object = object;
    out.object_label_hash = stableLabelHash(object.name);
    out.casts_scene_shadow =
        renderObjectCastsShadows(object) &&
        resolveMaterialSurfaceProfile(object.material) != MaterialSurfaceProfile::ContactShadow;
    prepareTriangleMesh(out, object.custom_mesh != nullptr ? *object.custom_mesh
                                                           : primitiveMesh(object.primitive));
    prepared.push_back(std::move(out));
  }
  return prepared;
}

float axisValue(const Vec3 value, const int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

bool intersectBounds(const Ray &ray, Vec3 bounds_min, Vec3 bounds_max, float max_distance,
                     float &near_distance);

bool intersectBounds(const Ray &ray, const Vec3 bounds_min, const Vec3 bounds_max,
                     const float max_distance) {
  float near_distance = 0.0f;
  return intersectBounds(ray, bounds_min, bounds_max, max_distance, near_distance);
}

bool intersectBounds(const Ray &ray, const Vec3 bounds_min, const Vec3 bounds_max,
                     const float max_distance, float &near_distance) {
  float t_min = 0.001f;
  float t_max = max_distance;
  for (int axis = 0; axis < 3; ++axis) {
    const float origin = axisValue(ray.origin, axis);
    const float direction = axisValue(ray.direction, axis);
    const float low = axisValue(bounds_min, axis);
    const float high = axisValue(bounds_max, axis);
    if (std::abs(direction) <= 0.000001f) {
      if (origin < low || origin > high) {
        return false;
      }
      continue;
    }
    float near_t = (low - origin) / direction;
    float far_t = (high - origin) / direction;
    if (near_t > far_t) {
      std::swap(near_t, far_t);
    }
    t_min = std::max(t_min, near_t);
    t_max = std::min(t_max, far_t);
    if (t_min > t_max) {
      return false;
    }
  }
  near_distance = t_min;
  return true;
}

bool intersectTriangle(const Ray &ray, const TraceTriangle &triangle, float &t, float &u,
                       float &v) {
  constexpr float kEpsilon = 0.000001f;
  const Vec3 edge1 = triangle.b - triangle.a;
  const Vec3 edge2 = triangle.c - triangle.a;
  const Vec3 p = cross(ray.direction, edge2);
  const float det = dot(edge1, p);
  if (std::abs(det) <= kEpsilon) {
    return false;
  }

  const float inv_det = 1.0f / det;
  const Vec3 s = ray.origin - triangle.a;
  u = inv_det * dot(s, p);
  if (u < 0.0f || u > 1.0f) {
    return false;
  }

  const Vec3 q = cross(s, edge1);
  v = inv_det * dot(ray.direction, q);
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }

  t = inv_det * dot(edge2, q);
  return t > 0.001f;
}

Material applyWorldPerceptualMaterialMemory(const RenderObject &object, const Material &base) {
  if (object.perceptual_primitive.truth_hash == 0u) {
    return base;
  }
  Material material = base;
  const WorldPerceptualSignals &signals = object.perceptual_primitive.signals;
  const float wet =
      std::clamp(signals.material_memory * 0.18f + signals.interaction_residue * 0.14f +
                     signals.light_history * 0.05f +
                     object.perceptual_primitive.neural_irradiance_confidence * 0.05f,
                 0.0f, 0.38f);
  const float residue =
      std::clamp(signals.interaction_residue * 0.22f + signals.contact_field * 0.18f +
                     signals.ecology_pressure * 0.10f,
                 0.0f, 0.45f);
  material.procedural.wetness = std::max(material.procedural.wetness, wet);
  material.procedural.cavity_grime =
      std::clamp(material.procedural.cavity_grime + residue, 0.0f, 1.0f);
  material.ambient_occlusion =
      std::clamp(material.ambient_occlusion * (1.0f - residue * 0.15f), 0.0f, 1.0f);
  material.roughness =
      std::clamp(std::lerp(material.roughness, 0.94f, residue * 0.30f), 0.045f, 1.0f);
  const float neural_warmth = std::clamp(object.perceptual_primitive.neural_irradiance_confidence *
                                            signals.light_history,
                                        0.0f, 1.0f);
  material.base_color.value.x =
      std::clamp(material.base_color.value.x +
                     object.perceptual_primitive.neural_irradiance.x * 0.08f * neural_warmth,
                 0.0f, 1.0f);
  material.base_color.value.y =
      std::clamp(material.base_color.value.y +
                     object.perceptual_primitive.neural_irradiance.y * 0.045f * neural_warmth,
                 0.0f, 1.0f);
  material.base_color.value.z =
      std::clamp(material.base_color.value.z +
                     object.perceptual_primitive.neural_irradiance.z * 0.025f * neural_warmth,
                 0.0f, 1.0f);
  return material;
}

bool intersectPreparedMesh(const Ray &ray, const PreparedObject &prepared, Hit &hit) {
  if (prepared.triangles.empty() ||
      !intersectBounds(ray, prepared.bounds_min, prepared.bounds_max, hit.distance)) {
    return false;
  }

  bool found = false;
  const auto consider_triangle = [&](const TraceTriangle &triangle) {
    float t = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    if (!intersectTriangle(ray, triangle, t, u, v) || t >= hit.distance) {
      return;
    }
    const float w = 1.0f - u - v;
    Vec3 normal = normalize(triangle.na * w + triangle.nb * u + triangle.nc * v);
    if (dot(normal, ray.direction) > 0.0f) {
      normal = normal * -1.0f;
    }
    Vec3 tangent = triangle.ta * w + triangle.tb * u + triangle.tc * v;
    tangent = tangent - normal * dot(tangent, normal);
    if (length(tangent) <= 0.0001f) {
      tangent = cross(std::abs(normal.y) < 0.86f ? Vec3{0.0f, 1.0f, 0.0f}
                                                 : Vec3{1.0f, 0.0f, 0.0f},
                      normal);
    }
    hit.valid = true;
    hit.distance = t;
    hit.position = ray.origin + ray.direction * t;
    hit.local_position = triangle.la * w + triangle.lb * u + triangle.lc * v;
    hit.normal = normal;
    hit.tangent = length(tangent) > 0.0001f ? normalize(tangent) : Vec3{1.0f, 0.0f, 0.0f};
    hit.uv = triangle.uva * w + triangle.uvb * u + triangle.uvc * v;
    hit.material =
        applyWorldPerceptualMaterialMemory(prepared.object, prepared.object.material);
    hit.object_label_hash = prepared.object_label_hash;
    found = true;
  };

  if (prepared.bvh_nodes.empty()) {
    for (const TraceTriangle &triangle : prepared.triangles) {
      consider_triangle(triangle);
    }
    return found;
  }

  std::array<std::uint32_t, 128u> stack{};
  std::size_t stack_size = 0u;
  stack[stack_size++] = 0u;
  while (stack_size > 0u) {
    const TraceBvhNode &node = prepared.bvh_nodes[stack[--stack_size]];
    if (!intersectBounds(ray, node.bounds_min, node.bounds_max, hit.distance)) {
      continue;
    }
    if (node.count > 0u) {
      for (std::uint32_t i = node.first; i < node.first + node.count; ++i) {
        consider_triangle(prepared.triangles[prepared.triangle_order[i]]);
      }
      continue;
    }

    float left_near = 0.0f;
    float right_near = 0.0f;
    const bool left_hit =
        intersectBounds(ray, prepared.bvh_nodes[node.left].bounds_min,
                        prepared.bvh_nodes[node.left].bounds_max, hit.distance, left_near);
    const bool right_hit =
        intersectBounds(ray, prepared.bvh_nodes[node.right].bounds_min,
                        prepared.bvh_nodes[node.right].bounds_max, hit.distance, right_near);
    if (left_hit && right_hit) {
      if (left_near < right_near) {
        stack[stack_size++] = node.right;
        stack[stack_size++] = node.left;
      } else {
        stack[stack_size++] = node.left;
        stack[stack_size++] = node.right;
      }
    } else if (left_hit) {
      stack[stack_size++] = node.left;
    } else if (right_hit) {
      stack[stack_size++] = node.right;
    }
  }
  return found;
}

bool intersectPreparedMeshAny(const Ray &ray, const PreparedObject &prepared,
                              const float max_distance) {
  if (prepared.triangles.empty() ||
      !intersectBounds(ray, prepared.bounds_min, prepared.bounds_max, max_distance)) {
    return false;
  }

  const auto triangle_hits = [&](const TraceTriangle &triangle) {
    float t = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    return intersectTriangle(ray, triangle, t, u, v) && t < max_distance;
  };

  if (prepared.bvh_nodes.empty()) {
    for (const TraceTriangle &triangle : prepared.triangles) {
      if (triangle_hits(triangle)) {
        return true;
      }
    }
    return false;
  }

  std::array<std::uint32_t, 128u> stack{};
  std::size_t stack_size = 0u;
  stack[stack_size++] = 0u;
  while (stack_size > 0u) {
    const TraceBvhNode &node = prepared.bvh_nodes[stack[--stack_size]];
    if (!intersectBounds(ray, node.bounds_min, node.bounds_max, max_distance)) {
      continue;
    }
    if (node.count > 0u) {
      for (std::uint32_t i = node.first; i < node.first + node.count; ++i) {
        if (triangle_hits(prepared.triangles[prepared.triangle_order[i]])) {
          return true;
        }
      }
      continue;
    }
    const TraceBvhNode &left = prepared.bvh_nodes[node.left];
    const TraceBvhNode &right = prepared.bvh_nodes[node.right];
    float left_near = 0.0f;
    float right_near = 0.0f;
    const bool left_hit =
        intersectBounds(ray, left.bounds_min, left.bounds_max, max_distance, left_near);
    const bool right_hit =
        intersectBounds(ray, right.bounds_min, right.bounds_max, max_distance, right_near);
    if (left_hit && right_hit) {
      if (left_near < right_near) {
        stack[stack_size++] = node.right;
        stack[stack_size++] = node.left;
      } else {
        stack[stack_size++] = node.left;
        stack[stack_size++] = node.right;
      }
    } else if (left_hit) {
      stack[stack_size++] = node.left;
    } else if (right_hit) {
      stack[stack_size++] = node.right;
    }
  }
  return false;
}

Hit trace(const Ray &ray, const std::vector<PreparedObject> &scene) {
  Hit closest;
  for (const PreparedObject &prepared : scene) {
    intersectPreparedMesh(ray, prepared, closest);
  }
  return closest;
}

bool traceShadowRay(const Ray &ray, const std::vector<PreparedObject> &scene,
                    const float max_distance) {
  const float capped_distance = std::max(max_distance, 0.02f);
  for (const PreparedObject &prepared : scene) {
    if (!prepared.casts_scene_shadow) {
      continue;
    }
    if (intersectPreparedMeshAny(ray, prepared, capped_distance)) {
      return true;
    }
  }
  return false;
}

Vec2 patternCoordinatesAt(const Vec3 position, const Vec3 normal) {
  const Vec3 abs_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z) {
    return {position.x, position.z};
  }
  if (abs_normal.x >= abs_normal.z) {
    return {position.z, position.y};
  }
  return {position.x, position.y};
}

float courseCellMaskAt(Vec2 uv, const Material &material) {
  uv.x *= std::max(material.pattern_scale.x, 0.001f);
  uv.y *= std::max(material.pattern_scale.y, 0.001f);
  const float row = std::floor(uv.y);
  uv.x += std::fmod(row, 2.0f) * 0.5f;
  const Vec2 cell{fractValue(uv.x), fractValue(uv.y)};
  const float mortar = std::clamp(material.pattern_mortar, 0.001f, 0.45f);
  const float edge_distance =
      std::min(std::min(cell.x, 1.0f - cell.x), std::min(cell.y, 1.0f - cell.y));
  return std::clamp((edge_distance - mortar * 0.45f) / std::max(mortar * 0.55f, 0.001f), 0.0f,
                    1.0f);
}

Vec3 courseCellAlbedo(const Hit &hit) {
  const Vec2 uv = patternCoordinatesAt(hit.position, hit.normal);
  const float cell = courseCellMaskAt(uv, hit.material);
  const float variation =
      valueNoise({std::floor(uv.x * std::max(hit.material.pattern_scale.x, 0.001f)),
                  std::floor(uv.y * std::max(hit.material.pattern_scale.y, 0.001f)), 13.0f});
  const Vec3 brick =
      hit.material.base_color.value * (0.78f + variation * 0.34f) + Vec3{0.035f, 0.024f, 0.014f};
  const Vec3 mortar = hit.material.base_color.value * (0.32f + variation * 0.08f);
  Vec3 color = mixVec(mortar, brick, cell);
  return mixVec(color, color * (0.74f + 0.36f * cell),
                std::clamp(hit.material.pattern_contrast, 0.0f, 1.0f));
}

Vec3 corrodedMetalAlbedo(const Hit &hit) {
  const AsterPipeSurfaceSignals signals =
      sampleAsterPipeSurface({.position = hit.position,
                              .normal = hit.normal,
                              .uv = hit.uv,
                              .detail_scale = hit.material.detail_scale},
                             hit.material.procedural, hit.material.edge_wear,
                             hit.material.pattern_depth);
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 n = normalize(hit.normal);
  const float lower = 1.0f - smoothstep(-0.55f, 0.42f, n.y);
  const float rim = smoothstep(2.44f, 2.60f, std::abs(hit.position.x));
  const float weld = std::max(1.0f - smoothstep(0.035f, 0.34f, std::abs(hit.position.x + 1.55f)),
                              1.0f - smoothstep(0.035f, 0.34f, std::abs(hit.position.x - 1.42f)));
  const Vec3 warp{projectedFbm(hit.position + Vec3{0.37f, 0.0f, -0.11f}, hit.normal,
                               detail * 0.090f, 307.0f) -
                      0.5f,
                  projectedFbm(hit.position + Vec3{-0.17f, 0.29f, 0.0f}, hit.normal,
                               detail * 0.073f, 311.0f) -
                      0.5f,
                  projectedFbm(hit.position + Vec3{0.0f, -0.19f, 0.41f}, hit.normal,
                               detail * 0.081f, 313.0f) -
                      0.5f};
  const Vec3 layered_position = hit.position + warp * 0.34f + hit.normal * (weld * 0.025f);
  const float broad = projectedFbm(layered_position, hit.normal, detail * 0.060f, 307.0f);
  const float medium =
      projectedFbm(layered_position + hit.normal * 0.05f, hit.normal, detail * 0.42f, 331.0f);
  const float fine = ridge(projectedFbm(hit.position, hit.normal, detail * 1.35f, 359.0f));
  const float flake = smoothstep(
      0.55f, 0.92f,
      ridge(projectedFbm(layered_position + Vec3{0.13f, -0.17f, 0.29f}, hit.normal,
                         detail * 0.28f, 887.0f)) *
              0.28f +
          medium * 0.22f + signals.pit_edge * 0.08f);
  const float soot_cloud =
      projectedFbm(layered_position + Vec3{-0.41f, 0.22f, 0.17f}, hit.normal, detail * 0.115f,
                   811.0f);
  const float pitting = smoothstep(0.58f, 0.96f, signals.pit * 0.46f + fine * 0.08f);
  const float upward = smoothstep(0.14f, 0.80f, n.y);
  const float oxide = smoothstep(0.20f, 0.74f,
                                 broad * 0.36f + medium * 0.30f + lower * 0.16f +
                                     weld * 0.12f + rim * 0.14f +
                                     hit.material.pattern_depth * upward * 0.10f +
                                     signals.rust_bloom * 0.42f +
                                     signals.orange_rust * 0.16f + flake * 0.04f);
  const float dark_scale = 0.38f + medium * 0.24f - lower * 0.08f - weld * 0.04f;
  const Vec3 cool_steel = hit.material.base_color.value * Vec3{0.66f, 0.74f, 0.78f} * dark_scale;
  const Vec3 exposed_edge =
      hit.material.base_color.value * Vec3{1.34f, 1.28f, 1.10f} * (0.54f + pitting * 0.20f);
  const Vec3 orange_rust{0.45f, 0.155f, 0.040f};
  const Vec3 dusty_rust{0.54f, 0.245f, 0.075f};
  const Vec3 black_rust{0.040f, 0.035f, 0.030f};
  const Vec3 cool_oxide{0.072f, 0.095f, 0.100f};
  const Vec3 aged_paint{0.34f, 0.325f, 0.275f};
  const float gray_stain =
      projectedFbm(layered_position + Vec3{-0.19f, 0.0f, 0.23f}, hit.normal, detail * 0.18f,
                   733.0f);
  const float rust_tone =
      std::clamp(medium * 0.56f + broad * 0.28f + signals.orange_rust * 0.10f, 0.0f, 1.0f);
  Vec3 rust_layer = mixVec(black_rust, orange_rust, rust_tone);
  rust_layer = mixVec(rust_layer, dusty_rust,
                      smoothstep(0.36f, 0.88f, broad + medium + signals.orange_rust) * 0.34f);
  Vec3 color = mixVec(cool_steel, rust_layer,
                      saturate(oxide * 0.58f + signals.rust_bloom * 0.28f +
                               signals.orange_rust * 0.10f));
  const float dark_mottle = smoothstep(
      0.25f, 0.72f,
      soot_cloud * 0.50f + gray_stain * 0.36f + flake * 0.04f +
          signals.black_oxide * 0.22f + signals.black_scab * 0.46f);
  const float gray_oxide = smoothstep(0.18f, 0.72f,
                                      gray_stain * 0.42f + (1.0f - medium) * 0.20f +
                                          lower * 0.18f + weld * 0.20f + rim * 0.20f +
                                          signals.black_oxide * 0.24f +
                                          signals.weld_scorch * 0.20f +
                                          dark_mottle * 0.15f);
  const float cavity = saturate(signals.cavity_grime * 0.34f + weld * 0.35f + rim * 0.34f +
                                lower * 0.25f + pitting * 0.10f + dark_mottle * 0.20f +
                                signals.rim_soot * 0.26f + signals.weld_scorch * 0.20f);
  color = mixVec(color, aged_paint, signals.paint_remnant * 0.16f * (1.0f - dark_mottle));
  color = mixVec(color, cool_oxide, gray_oxide * 0.78f);
  color = mixVec(color, black_rust,
                 cavity * 0.28f + dark_mottle * 0.40f + signals.black_scab * 0.42f +
                     signals.rim_soot * 0.34f + signals.weld_scorch * 0.26f);
  const float pinhole = smoothstep(0.82f, 0.98f, signals.pit + signals.pit_edge * 0.50f);
  color = mixVec(color, black_rust, pinhole * 0.10f);
  const float scratch_polish =
      smoothstep(0.68f, 0.96f, signals.axial_scratch * (0.64f + medium * 0.36f)) *
      (1.0f - saturate(cavity * 0.55f + signals.black_scab * 0.28f));
  color = mixVec(color, exposed_edge * Vec3{0.92f, 0.96f, 1.02f},
                 scratch_polish * 0.18f + signals.edge_polish * 0.10f);
  color = mixVec(color, exposed_edge,
                 signals.edge_polish * 0.16f + hit.material.edge_wear *
                                                   smoothstep(0.72f, 0.98f, pitting) * 0.18f);
  color = mixVec(color, color * Vec3{0.60f, 0.66f, 0.72f} + Vec3{0.010f, 0.012f, 0.014f},
                 signals.wet_film * 0.12f);
  const float pepper =
      ridge(projectedFbm(layered_position + Vec3{0.21f, 0.31f, -0.14f}, hit.normal,
                         detail * 2.25f, 941.0f));
  color *= 0.76f + medium * 0.13f + pepper * 0.020f - dark_mottle * 0.10f - cavity * 0.04f;
  return clamp(color, 0.0f, 4.0f);
}

Vec3 weldBeadAlbedo(const Hit &hit) {
  const AsterPipeSurfaceSignals signals =
      sampleAsterPipeSurface({.position = hit.position,
                              .normal = hit.normal,
                              .uv = hit.uv,
                              .detail_scale = hit.material.detail_scale},
                             hit.material.procedural, hit.material.edge_wear,
                             hit.material.pattern_depth);
  const float bead_ripple =
      0.5f + 0.5f * std::sin(hit.uv.y * std::max(hit.material.pattern_scale.y, 0.001f) * kTau +
                              projectedFbm(hit.position, hit.normal, hit.material.detail_scale,
                                           401.0f) *
                                  3.8f);
  const float heat_band = saturate(smoothstep(0.05f, 0.58f, ridge(hit.uv.x)) + signals.heat_tint);
  const float contact_grime = 1.0f - smoothstep(0.18f, 0.42f, ridge(hit.uv.x));
  const float slag = ridge(projectedFbm(hit.position + hit.normal * 0.035f, hit.normal,
                                        hit.material.detail_scale * 1.14f, 413.0f));
  const float center = 1.0f - smoothstep(0.0f, 0.48f, std::abs(hit.uv.x - 0.50f));
  const Vec3 weld_metal = Vec3{0.235f, 0.215f, 0.180f} * (0.70f + bead_ripple * 0.20f);
  const Vec3 polished_lip{0.36f, 0.330f, 0.260f};
  const Vec3 straw{0.66f, 0.34f, 0.10f};
  const Vec3 blue_heat{0.08f, 0.13f, 0.25f};
  const Vec3 soot{0.040f, 0.034f, 0.030f};
  const Vec3 rust_dust{0.40f, 0.135f, 0.038f};
  Vec3 color = mixVec(weld_metal, polished_lip, center * (0.055f + bead_ripple * 0.045f));
  color *= 0.82f + bead_ripple * 0.12f;
  color = mixVec(color, straw, heat_band * 0.16f);
  color = mixVec(color, blue_heat, heat_band * (1.0f - bead_ripple) * 0.20f);
  color = mixVec(color, soot,
                 signals.cavity_grime * 0.30f + contact_grime * 0.54f +
                     signals.weld_scorch * 0.34f);
  color = mixVec(color, rust_dust,
                 contact_grime * (0.26f + signals.orange_rust * 0.20f) +
                     signals.weld_slag * 0.16f);
  color = mixVec(color, color * 0.52f + Vec3{0.050f, 0.044f, 0.036f},
                 slag * 0.18f + signals.weld_slag * 0.22f);
  color = mixVec(color, soot, smoothstep(0.70f, 0.96f, slag + signals.black_scab * 0.45f) * 0.28f);
  color = mixVec(color, color * Vec3{0.55f, 0.62f, 0.70f}, signals.wet_film * 0.18f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 biologicalIntegumentPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const float pigment = projectedFbm(hit.position + hit.normal * 0.035f, hit.normal,
                                     detail * 0.36f, 451.0f);
  const float capillary =
      projectedFbm(hit.position + Vec3{0.07f, 0.13f, -0.05f}, hit.normal, detail * 0.82f,
                   467.0f);
  const float pore = ridge(projectedFbm(hit.position, hit.normal, detail * 2.20f, 479.0f));
  const float abrasion =
      ridge(projectedFbm(hit.position + hit.normal * 0.09f, hit.normal, detail * 1.48f, 491.0f));
  const float follicle =
      0.5f + 0.5f * std::sin((hit.position.z * hit.material.pattern_scale.y +
                               hit.position.x * hit.material.pattern_scale.x * 0.32f +
                               hit.uv.x * 7.0f) *
                                  3.35f +
                              pigment * 4.0f);
  const float follicle_mask =
      smoothstep(0.50f, 0.94f, follicle) * smoothstep(0.16f, 0.84f, pore + hit.material.pattern_depth);
  const float low_pelage =
      smoothstep(0.60f, 0.94f, hit.normal.y) *
      smoothstep(0.42f, 0.98f, std::abs(hit.position.z) * 0.35f +
                              std::abs(hit.position.x) * 0.20f);
  const float vascular_weight =
      smoothstep(0.48f, 0.96f, capillary) *
      saturate(0.20f + hit.material.pattern_depth * 1.50f + hit.material.procedural.wetness * 0.75f);
  const float pigment_gain = 0.35f + hit.material.pattern_contrast * 0.65f +
                             hit.material.procedural.macro_variation * 0.20f;

  Vec3 color = mixVec(hit.material.base_color.value * Vec3{0.72f, 0.63f, 0.52f},
                      hit.material.base_color.value * Vec3{0.46f, 0.38f, 0.28f},
                      pigment * pigment_gain);
  color = mixVec(color, {0.58f, 0.23f, 0.17f}, vascular_weight * (0.32f + low_pelage * 0.28f));
  color = mixVec(color, hit.material.base_color.value * Vec3{1.15f, 1.03f, 0.78f},
                 follicle_mask * (0.30f + hit.material.detail_strength * 0.16f));
  color = mixVec(color, color * Vec3{0.72f, 0.68f, 0.60f},
                 smoothstep(0.74f, 0.98f, abrasion) * (0.10f + hit.material.edge_wear * 0.38f));
  return clamp(color * (0.92f + pore * 0.10f), 0.0f, 4.0f);
}

Vec3 terrainPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const float near_detail = 1.0f - smoothstep(3.2f, 11.5f, hit.distance);
  const float mip_detail = 0.28f + near_detail * 0.72f;
  const float broad =
      projectedFbm(hit.position + Vec3{0.15f, 0.0f, -0.09f}, hit.normal, detail * 0.12f, 521.0f);
  const float medium = projectedFbm(hit.position, hit.normal, detail * 0.42f, 523.0f);
  const float fine = ridge(projectedFbm(hit.position, hit.normal, detail * 1.85f, 541.0f));
  const AsterSurfaceCellSample grains =
      asterSurfaceCellular(hit.position + Vec3{0.27f, 0.0f, -0.41f}, detail * 1.18f, 557.0f);
  const float pebble =
      smoothstep(0.58f, 0.95f, fine * 0.48f + (1.0f - grains.distance) * 0.44f + medium * 0.18f) *
      near_detail;
  const float clay_band = projectedFbm(hit.position + Vec3{-0.36f, 0.0f, 0.22f}, hit.normal,
                                       detail * 0.070f, 547.0f);
  const float wind_slab =
      projectedFbm(hit.position + Vec3{1.1f, 0.0f, -0.6f}, hit.normal, detail * 0.028f, 549.0f);
  const Vec3 dry_soil = hit.material.base_color.value * Vec3{1.05f, 0.88f, 0.66f};
  const Vec3 compact_soil = hit.material.base_color.value * Vec3{0.58f, 0.50f, 0.42f};
  const Vec3 clay{0.150f, 0.112f, 0.080f};
  const Vec3 cool_grit{0.095f, 0.092f, 0.082f};
  Vec3 color = mixVec(compact_soil, dry_soil, smoothstep(0.22f, 0.86f, broad));
  color = mixVec(color, clay, smoothstep(0.46f, 0.88f, clay_band) * 0.24f);
  color = mixVec(color, cool_grit, smoothstep(0.56f, 0.92f, medium) * 0.16f);
  color = mixVec(color, compact_soil * 0.86f, smoothstep(0.40f, 0.86f, wind_slab) * 0.08f);
  color = mixVec(color, color * 1.16f + Vec3{0.022f, 0.016f, 0.010f}, pebble * 0.18f);
  color = mixVec(color, color * 0.70f, smoothstep(0.18f, 0.72f, grains.edge_distance) *
                                      (1.0f - grains.distance) * 0.03f * near_detail);
  color *= 0.82f + fine * 0.035f * mip_detail + hit.normal.y * 0.10f;
  return clamp(color, 0.0f, 4.0f);
}

Vec3 fiberPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 p = materialSamplePosition(hit);
  const float along = p.x * 0.96f + p.z * 0.18f;
  const float across = p.y * 0.72f + p.z * 0.30f;
  const float directional_grain =
      0.5f + 0.5f * std::sin((along * hit.material.pattern_scale.x +
                              across * hit.material.pattern_scale.y * 0.07f) *
                                 3.9f +
                             projectedFbm(p, hit.normal, detail * 0.26f, 563.0f) *
                                 3.0f);
  const float fine = ridge(projectedFbm(p, hit.normal, detail * 1.72f, 569.0f));
  if (hit.material.metallic > 0.50f) {
    const float scratch =
        smoothstep(0.54f, 0.96f,
                   ridge(projectedFbm(p + Vec3{0.11f, -0.07f, 0.19f}, hit.normal,
                                      detail * 3.8f, 573.0f)));
    const float brushed =
        0.5f + 0.5f * std::sin((along * hit.material.pattern_scale.x * 2.5f +
                                 projectedFbm(p, hit.normal, detail * 0.18f, 575.0f) * 4.0f) *
                                2.2f);
    Vec3 color = hit.material.base_color.value * (0.74f + fine * 0.12f);
    color = mixVec(color, color * Vec3{1.18f, 1.24f, 1.28f} + Vec3{0.018f, 0.022f, 0.026f},
                   scratch * 0.24f);
    color = mixVec(color, color * Vec3{0.78f, 0.86f, 0.92f},
                   smoothstep(0.62f, 0.96f, brushed) * 0.12f);
    color = mixVec(color, color * Vec3{1.12f, 1.10f, 1.04f},
                   smoothstep(0.22f, 0.70f, directional_grain) * 0.08f);
    return clamp(color, 0.0f, 4.0f);
  }
  const bool warm_fiber = hit.material.base_color.value.x > hit.material.base_color.value.z * 1.25f &&
                          hit.material.base_color.value.y > hit.material.base_color.value.z * 0.82f;
  if (warm_fiber) {
    const float rings =
        0.5f + 0.5f * std::sin((along * 1.65f + across * 0.42f +
                                projectedFbm(p, hit.normal, detail * 0.18f, 571.0f) *
                                    3.8f) *
                               6.2f);
    const float pores = smoothstep(
        0.72f, 0.97f,
        ridge(projectedFbm(p + Vec3{0.17f, 0.04f, -0.11f}, hit.normal,
                           detail * 2.20f, 577.0f)));
    const Vec3 walnut_dark = hit.material.base_color.value * Vec3{0.38f, 0.28f, 0.18f};
    const Vec3 walnut_gold = hit.material.base_color.value * Vec3{1.26f, 1.00f, 0.64f};
    Vec3 color = mixVec(walnut_dark, walnut_gold, smoothstep(0.28f, 0.92f, rings));
    color = mixVec(color, color * 0.46f, pores * 0.26f);
    color = mixVec(color, color * Vec3{1.10f, 0.94f, 0.76f}, directional_grain * 0.12f);
    return clamp(color, 0.0f, 4.0f);
  }
  Vec3 color = mixVec(hit.material.base_color.value * 0.88f,
                      hit.material.base_color.value * Vec3{1.08f, 1.06f, 1.02f},
                      smoothstep(0.18f, 0.92f, directional_grain));
  color = mixVec(color, color * Vec3{0.84f, 0.90f, 0.96f}, fine * 0.08f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 stratifiedPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 p = materialSamplePosition(hit);
  const float layer = 0.5f + 0.5f * std::sin((p.y * hit.material.pattern_scale.y +
                                              p.x * 0.22f + p.z * 0.35f) *
                                                 5.4f +
                                             projectedFbm(p, hit.normal,
                                                          detail * 0.38f, 587.0f) *
                                                 4.6f);
  const float crack = ridge(projectedFbm(p, hit.normal, detail * 1.22f, 593.0f));
  const float wet = std::clamp(hit.material.procedural.wetness, 0.0f, 1.0f);
  Vec3 color = mixVec(hit.material.base_color.value * Vec3{0.58f, 0.62f, 0.60f},
                      hit.material.base_color.value * Vec3{1.20f, 1.12f, 0.96f},
                      smoothstep(0.24f, 0.92f, layer));
  color = mixVec(color, color * 0.70f, smoothstep(0.72f, 0.98f, crack) * 0.08f);
  color = mixVec(color, color * Vec3{0.84f, 0.92f, 1.04f},
                 smoothstep(0.56f, 0.94f, layer) * wet * 0.10f);
  color = mixVec(color, color * Vec3{0.82f, 0.88f, 0.94f} + Vec3{0.010f, 0.013f, 0.015f},
                 wet * 0.10f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 mineralPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 p = materialSamplePosition(hit);
  const float warped =
      projectedFbm(p + Vec3{0.21f, -0.13f, 0.18f}, hit.normal, detail * 0.22f, 607.0f);
  const float vein = ridge(std::sin((p.x * 0.72f + p.z * 1.10f + p.y * 0.26f + warped * 1.9f) *
                                    5.4f) *
                           0.5f +
                       0.5f);
  const float secondary_vein =
      ridge(std::sin((p.x * -0.54f + p.z * 1.34f + p.y * 0.58f +
                      projectedFbm(p, hit.normal, detail * 0.18f, 609.0f) * 1.6f) *
                     9.2f) *
            0.5f +
        0.5f);
  const float polish = projectedFbm(p, hit.normal, detail * 1.44f, 613.0f);
  const float glass_depth =
      projectedFbm(p + hit.normal * 0.09f, hit.normal, detail * 0.62f, 617.0f);
  const float mineral_line =
      smoothstep(0.76f, 0.985f, vein * 0.72f + secondary_vein * 0.42f);
  const float dark_inclusion =
      smoothstep(0.62f, 0.94f,
                 projectedFbm(p + Vec3{0.21f, -0.12f, 0.17f}, hit.normal,
                              detail * 0.72f, 619.0f));
  const Vec3 calcite{0.78f, 0.82f, 0.72f};
  const Vec3 deep_jade{0.030f, 0.115f, 0.085f};
  const Vec3 inner_green{0.075f, 0.300f, 0.210f};
  Vec3 color = mixVec(deep_jade, inner_green,
                      smoothstep(0.28f, 0.88f, glass_depth) * 0.46f +
                          smoothstep(0.38f, 0.88f, vein) * 0.16f);
  color = mixVec(color, calcite, mineral_line * (0.42f + polish * 0.16f));
  color = mixVec(color, hit.material.base_color.value * Vec3{0.38f, 0.55f, 0.48f},
                 dark_inclusion * (0.18f + (1.0f - mineral_line) * 0.20f));
  color = mixVec(color, color * Vec3{0.54f, 0.72f, 0.90f} + Vec3{0.010f, 0.018f, 0.025f},
                 smoothstep(0.42f, 0.90f, glass_depth) * 0.20f);
  color = mixVec(color, color * 1.20f + Vec3{0.018f, 0.024f, 0.032f},
                 smoothstep(0.66f, 0.96f, polish) * 0.12f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 resinPreviewAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 p = materialSamplePosition(hit);
  if (luminanceOf(hit.material.base_color.value) > 0.48f &&
      hit.material.pattern_contrast > 0.55f) {
    const AsterSurfaceCellSample cells =
        asterSurfaceCellular(p + Vec3{0.19f, -0.11f, 0.31f}, detail * 0.62f, 661.0f);
    const float crackle =
        smoothstep(0.52f, 0.95f, 1.0f - cells.edge_distance) * hit.material.pattern_contrast;
    const float glaze =
        projectedFbm(p + hit.normal * 0.10f, hit.normal, detail * 0.54f, 663.0f);
    const float speckle =
        smoothstep(0.72f, 0.985f,
                   valueNoise(p * (detail * 3.4f) + Vec3{17.0f, 29.0f, -5.0f}));
    const Vec3 warm_porcelain = hit.material.base_color.value * Vec3{1.08f, 1.02f, 0.90f};
    const Vec3 fired_clay{0.34f, 0.18f, 0.070f};
    const Vec3 cobalt_mineral{0.055f, 0.092f, 0.160f};
    Vec3 color = mixVec(hit.material.base_color.value * 0.78f, warm_porcelain,
                        smoothstep(0.20f, 0.86f, glaze));
    color = mixVec(color, fired_clay, speckle * 0.42f);
    color = mixVec(color, cobalt_mineral, smoothstep(0.84f, 0.985f, glaze * speckle) * 0.34f);
    color = mixVec(color, Vec3{0.045f, 0.038f, 0.032f}, crackle * 0.12f);
    color = mixVec(color, color * 1.18f + Vec3{0.020f, 0.018f, 0.014f},
                   smoothstep(0.48f, 0.90f, 1.0f - cells.distance) * 0.16f);
    return clamp(color, 0.0f, 4.0f);
  }
  const float cloud = projectedFbm(p + hit.normal * 0.12f, hit.normal,
                                   detail * 0.36f, 631.0f);
  const float inner = projectedFbm(p, hit.normal, detail * 1.10f, 641.0f);
  Vec3 color = hit.material.base_color.value * (0.72f + cloud * 0.34f);
  color = mixVec(color, color * Vec3{0.62f, 0.78f, 0.92f} + Vec3{0.018f, 0.030f, 0.036f},
                 smoothstep(0.58f, 0.96f, inner) * 0.24f);
  color = mixVec(color, hit.material.base_color.value * Vec3{1.20f, 0.94f, 0.70f},
                 smoothstep(0.66f, 0.96f, cloud) * 0.14f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 previewAlbedo(const Hit &hit) {
  switch (resolveMaterialSurfaceProfile(hit.material)) {
  case MaterialSurfaceProfile::Masonry:
    return courseCellAlbedo(hit);
  case MaterialSurfaceProfile::TerrainLayer:
    return terrainPreviewAlbedo(hit);
  case MaterialSurfaceProfile::OrganicFiber:
    return fiberPreviewAlbedo(hit);
  case MaterialSurfaceProfile::StratifiedRock:
    return stratifiedPreviewAlbedo(hit);
  case MaterialSurfaceProfile::MineralVein:
    return mineralPreviewAlbedo(hit);
  case MaterialSurfaceProfile::Resin:
    return resinPreviewAlbedo(hit);
  case MaterialSurfaceProfile::CorrodedMetal:
    return corrodedMetalAlbedo(hit);
  case MaterialSurfaceProfile::WeldBead:
    return weldBeadAlbedo(hit);
  case MaterialSurfaceProfile::BiologicalIntegument:
    return biologicalIntegumentPreviewAlbedo(hit);
  case MaterialSurfaceProfile::Auto:
  case MaterialSurfaceProfile::Plain:
  case MaterialSurfaceProfile::Liquid:
  case MaterialSurfaceProfile::Foliage:
  case MaterialSurfaceProfile::PaintedWood:
  case MaterialSurfaceProfile::Feather:
  case MaterialSurfaceProfile::Scales:
  case MaterialSurfaceProfile::ContactShadow:
  case MaterialSurfaceProfile::FilamentWeb:
  case MaterialSurfaceProfile::ChitinShell:
  case MaterialSurfaceProfile::EmissiveLens:
    break;
  }

  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const Vec3 p = materialSamplePosition(hit);
  const float broad = projectedFbm(p, hit.normal, detail * 0.22f, 19.0f);
  const float fine = projectedFbm(p, hit.normal, detail * 0.84f, 29.0f);
  const float weight = saturate(hit.material.detail_strength + hit.material.pattern_contrast * 0.24f);
  return clamp(hit.material.base_color.value *
                   std::lerp(1.0f, 0.86f + broad * 0.20f + fine * 0.08f, weight),
               0.0f, 4.0f);
}

float surfaceHeightSignal(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const MaterialSurfaceProfile profile = resolveMaterialSurfaceProfile(hit.material);
  switch (profile) {
  case MaterialSurfaceProfile::TerrainLayer: {
    const float near_detail = 1.0f - smoothstep(3.2f, 11.5f, hit.distance);
    const float broad = projectedFbm(hit.position + Vec3{0.15f, 0.0f, -0.09f}, hit.normal,
                                     detail * 0.12f, 521.0f);
    const float medium = projectedFbm(hit.position, hit.normal, detail * 0.42f, 523.0f);
    const float fine = ridge(projectedFbm(hit.position, hit.normal, detail * 1.85f, 541.0f));
    const AsterSurfaceCellSample grains =
        asterSurfaceCellular(hit.position + Vec3{0.27f, 0.0f, -0.41f}, detail * 1.18f, 557.0f);
    const float pebble =
        smoothstep(0.58f, 0.95f,
                   fine * 0.48f + (1.0f - grains.distance) * 0.44f + medium * 0.18f) *
        near_detail;
    return saturate(broad * 0.24f + medium * 0.24f + fine * 0.24f * near_detail +
                    pebble * 0.34f + (1.0f - grains.edge_distance) * 0.10f * near_detail);
  }
  case MaterialSurfaceProfile::OrganicFiber: {
    const Vec3 p = materialSamplePosition(hit);
    const float along = p.x * 0.96f + p.z * 0.18f;
    const float across = p.y * 0.72f + p.z * 0.30f;
    const float grain =
        0.5f + 0.5f * std::sin((along * hit.material.pattern_scale.x +
                                across * hit.material.pattern_scale.y * 0.07f) *
                                   3.9f +
                               projectedFbm(p, hit.normal, detail * 0.26f, 563.0f) *
                                   3.0f);
    const float fine = ridge(projectedFbm(p, hit.normal, detail * 1.72f, 569.0f));
    if (hit.material.metallic > 0.50f) {
      return saturate(grain * 0.12f + fine * 0.34f);
    }
    return saturate(grain * 0.46f + fine * 0.42f);
  }
  case MaterialSurfaceProfile::StratifiedRock: {
    const Vec3 p = materialSamplePosition(hit);
    const float layer =
        0.5f + 0.5f * std::sin((p.y * hit.material.pattern_scale.y +
                                p.x * 0.22f + p.z * 0.35f) *
                                   5.4f +
                               projectedFbm(p, hit.normal, detail * 0.38f, 587.0f) *
                                   4.6f);
    const float crack = ridge(projectedFbm(p, hit.normal, detail * 1.22f, 593.0f));
    return saturate(layer * 0.42f + smoothstep(0.70f, 0.98f, crack) * 0.44f);
  }
  case MaterialSurfaceProfile::MineralVein: {
    const Vec3 p = materialSamplePosition(hit);
    const float vein = ridge(std::sin((p.x * 0.46f + p.z * 0.76f + p.y * 0.22f +
                                       projectedFbm(p, hit.normal, detail * 0.34f,
                                                    607.0f)) *
                                      4.8f) *
                             0.5f +
                         0.5f);
    const float polish = projectedFbm(p, hit.normal, detail * 1.44f, 613.0f);
    return saturate(vein * 0.38f + polish * 0.32f);
  }
  case MaterialSurfaceProfile::Resin: {
    const Vec3 p = materialSamplePosition(hit);
    if (luminanceOf(hit.material.base_color.value) > 0.48f &&
        hit.material.pattern_contrast > 0.55f) {
      const AsterSurfaceCellSample cells =
          asterSurfaceCellular(p + Vec3{0.19f, -0.11f, 0.31f}, detail * 0.62f, 661.0f);
      const float crackle =
          smoothstep(0.52f, 0.95f, 1.0f - cells.edge_distance) * hit.material.pattern_contrast;
      const float glaze =
          projectedFbm(p + hit.normal * 0.10f, hit.normal, detail * 0.54f, 663.0f);
      return saturate(crackle * 0.36f + (1.0f - cells.distance) * 0.16f + glaze * 0.10f);
    }
    const float cloud = projectedFbm(p + hit.normal * 0.12f, hit.normal,
                                     detail * 0.36f, 631.0f);
    const float inner = projectedFbm(p, hit.normal, detail * 1.10f, 641.0f);
    return saturate(cloud * 0.20f + inner * 0.16f);
  }
  case MaterialSurfaceProfile::CorrodedMetal:
  case MaterialSurfaceProfile::WeldBead: {
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = hit.position,
                                .normal = hit.normal,
                                .uv = hit.uv,
                                .detail_scale = hit.material.detail_scale},
                               hit.material.procedural, hit.material.edge_wear,
                               hit.material.pattern_depth);
    return saturate(signals.height * 0.70f + signals.pit * 0.24f +
                    signals.cavity_grime * 0.18f);
  }
  case MaterialSurfaceProfile::Masonry:
  case MaterialSurfaceProfile::Plain:
  case MaterialSurfaceProfile::Auto:
  case MaterialSurfaceProfile::Liquid:
  case MaterialSurfaceProfile::Foliage:
  case MaterialSurfaceProfile::PaintedWood:
  case MaterialSurfaceProfile::Feather:
  case MaterialSurfaceProfile::Scales:
  case MaterialSurfaceProfile::ContactShadow:
  case MaterialSurfaceProfile::FilamentWeb:
  case MaterialSurfaceProfile::ChitinShell:
  case MaterialSurfaceProfile::EmissiveLens:
  case MaterialSurfaceProfile::BiologicalIntegument:
    break;
  }
  return projectedFbm(hit.position, hit.normal, detail * 0.84f, 671.0f);
}

Vec3 perturbNormalFromHeight(const Hit &hit, const Vec3 base_normal, const float strength,
                             const RendererSettings &settings) {
  const Vec3 normal = normalize(base_normal);
  const Vec3 tangent = orthogonalTangent(hit.tangent, normal);
  const Vec3 bitangent = normalize(cross(normal, tangent));
  const float texel_density =
      std::max(hit.material.procedural.physical_texel_density,
               settings.surface_scale.physical_texel_density);
  const float detail_step =
      std::clamp(640.0f / std::max(texel_density, 1.0f), 0.004f, 0.045f);
  const auto offset_hit = [&](const Vec3 offset) {
    Hit sample = hit;
    sample.position = hit.position + offset;
    sample.local_position = hit.local_position + offset;
    return sample;
  };
  const float h_t0 = surfaceHeightSignal(offset_hit(tangent * -detail_step));
  const float h_t1 = surfaceHeightSignal(offset_hit(tangent * detail_step));
  const float h_b0 = surfaceHeightSignal(offset_hit(bitangent * -detail_step));
  const float h_b1 = surfaceHeightSignal(offset_hit(bitangent * detail_step));
  const float coupling = std::max(hit.material.procedural.height_normal_coupling,
                                  settings.surface_scale.height_normal_coupling);
  const float gain = std::clamp(strength * coupling * 1.55f, 0.0f, 2.6f);
  return normalize(normal - tangent * ((h_t1 - h_t0) * gain) -
                   bitangent * ((h_b1 - h_b0) * gain));
}

float effectiveRoughness(const Hit &hit, const RendererSettings &settings) {
  float roughness = std::clamp(hit.material.roughness, 0.045f, 1.0f);
  const MaterialSurfaceProfile profile = resolveMaterialSurfaceProfile(hit.material);
  const AsterPipeSurfaceSignals signals =
      profile == MaterialSurfaceProfile::CorrodedMetal || profile == MaterialSurfaceProfile::WeldBead
          ? sampleAsterPipeSurface({.position = hit.position,
                                    .normal = hit.normal,
                                    .uv = hit.uv,
                                    .detail_scale = hit.material.detail_scale},
                                   hit.material.procedural, hit.material.edge_wear,
                                   hit.material.pattern_depth)
          : AsterPipeSurfaceSignals{};
  const float variation = hit.material.procedural.roughness_variation;
  if (variation > 0.0001f) {
    const float noise = projectedFbm(materialSamplePosition(hit), hit.normal,
                                     std::max(hit.material.detail_scale, 1.0f), 503.0f);
    roughness += (noise - 0.5f) * variation * 0.24f;
  }
  const float height = surfaceHeightSignal(hit);
  const float coupling =
      std::max(hit.material.procedural.roughness_height_coupling,
               settings.surface_scale.roughness_height_coupling);
  if (profile == MaterialSurfaceProfile::TerrainLayer) {
    const float cavity = 1.0f - height;
    roughness = std::lerp(roughness, 0.95f, coupling * 0.14f);
    roughness += (height - 0.50f) * variation * 0.18f;
    roughness = std::lerp(roughness, 0.99f,
                          cavity * hit.material.procedural.height_shading * coupling * 0.10f);
  } else if (profile == MaterialSurfaceProfile::OrganicFiber && hit.material.metallic > 0.50f) {
    roughness += (height - 0.50f) * variation * 0.18f;
    roughness = std::lerp(roughness, 0.18f, hit.material.tangent_anisotropy * 0.08f);
  } else if (profile == MaterialSurfaceProfile::StratifiedRock) {
    roughness = std::lerp(roughness, 0.88f, height * coupling * 0.18f);
    roughness = std::lerp(roughness, 0.48f, hit.material.procedural.wetness * 0.16f);
  } else if (profile == MaterialSurfaceProfile::MineralVein) {
    roughness = std::lerp(roughness, 0.12f, hit.material.coat_strength * 0.18f);
    roughness = std::lerp(roughness, 0.38f, height * coupling * 0.14f);
    roughness += (height - 0.45f) * variation * 0.08f;
  } else if (profile == MaterialSurfaceProfile::Resin) {
    if (luminanceOf(hit.material.base_color.value) > 0.48f &&
        hit.material.pattern_contrast > 0.55f) {
      roughness = std::lerp(roughness, 0.22f, 0.22f + hit.material.coat_strength * 0.28f);
      roughness += (height - 0.50f) * variation * 0.08f;
    } else {
      roughness = std::lerp(roughness, 0.38f, 0.12f + hit.material.coat_strength * 0.14f);
    }
  }
  if (profile == MaterialSurfaceProfile::CorrodedMetal || profile == MaterialSurfaceProfile::WeldBead) {
    const float height_coupling = std::clamp(coupling, 0.0f, 1.50f);
    const float rust_plate = saturate(signals.rust_bloom * 0.54f + signals.orange_rust * 0.24f +
                                      signals.black_scab * 0.42f + signals.cavity_grime * 0.24f);
    roughness = std::lerp(roughness, 0.97f, rust_plate * 0.66f);
    roughness += signals.pit * (0.12f + height_coupling * 0.06f) +
                 signals.weld_slag * (0.08f + height_coupling * 0.04f);
    roughness = std::lerp(roughness, 0.99f, signals.height * height_coupling * 0.18f);
    roughness = std::lerp(roughness, 0.58f, signals.paint_remnant * 0.20f);
    roughness = std::lerp(roughness, 0.46f, signals.axial_scratch * 0.16f);
  }
  roughness = std::lerp(roughness, std::min(roughness, 0.17f), signals.wet_film * 0.72f);
  roughness = std::lerp(roughness, 0.25f, signals.edge_polish * 0.38f);
  return std::clamp(roughness, 0.045f, 1.0f);
}

float effectiveMetallic(const Hit &hit) {
  float metallic = std::clamp(hit.material.metallic, 0.0f, 1.0f);
  const MaterialSurfaceProfile profile = resolveMaterialSurfaceProfile(hit.material);
  if (profile == MaterialSurfaceProfile::CorrodedMetal || profile == MaterialSurfaceProfile::WeldBead) {
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = hit.position,
                                .normal = hit.normal,
                                .uv = hit.uv,
                                .detail_scale = hit.material.detail_scale},
                               hit.material.procedural, hit.material.edge_wear,
                               hit.material.pattern_depth);
    metallic += signals.edge_polish * 0.08f;
    const float rust_plate = saturate(signals.rust_bloom * 0.42f + signals.black_scab * 0.58f +
                                      signals.cavity_grime * 0.28f + signals.pit * 0.18f);
    metallic = std::lerp(metallic, 0.035f, rust_plate * 0.74f);
    metallic = std::lerp(metallic, 0.78f, signals.edge_polish * 0.28f);
    metallic = std::lerp(metallic, 0.62f, signals.axial_scratch * 0.12f);
    metallic -= signals.wet_film * 0.04f;
  }
  return std::clamp(metallic, 0.0f, 1.0f);
}

float effectiveAmbientOcclusion(const Hit &hit) {
  float ao = std::clamp(hit.material.ambient_occlusion, 0.0f, 1.0f);
  const MaterialSurfaceProfile profile = resolveMaterialSurfaceProfile(hit.material);
  if (profile == MaterialSurfaceProfile::CorrodedMetal || profile == MaterialSurfaceProfile::WeldBead) {
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = hit.position,
                                .normal = hit.normal,
                                .uv = hit.uv,
                                .detail_scale = hit.material.detail_scale},
                               hit.material.procedural, hit.material.edge_wear,
                               hit.material.pattern_depth);
    ao *= std::clamp(1.0f - signals.cavity_grime * 0.28f - signals.black_scab * 0.16f -
                         signals.rim_soot * 0.26f - signals.weld_scorch * 0.20f -
                         signals.weld_slag * 0.16f,
                     0.36f, 1.0f);
  } else {
    const float height = surfaceHeightSignal(hit);
    const float cavity = profile == MaterialSurfaceProfile::MineralVein ||
                                 profile == MaterialSurfaceProfile::Resin
                             ? std::abs(height - 0.52f) * 1.45f
                             : 1.0f - height;
    ao *= std::clamp(1.0f - cavity * hit.material.procedural.height_shading * 0.24f,
                     0.58f, 1.0f);
  }
  return ao;
}

float surfaceHeightGradientMagnitude(const Hit &hit, const RendererSettings &settings) {
  const Vec3 normal = normalize(hit.normal);
  const Vec3 tangent = orthogonalTangent(hit.tangent, normal);
  const Vec3 bitangent = normalize(cross(normal, tangent));
  const float texel_density =
      std::max(hit.material.procedural.physical_texel_density,
               settings.surface_scale.physical_texel_density);
  const float detail_step =
      std::clamp(512.0f / std::max(texel_density, 1.0f), 0.004f, 0.052f);
  const auto offset_hit = [&](const Vec3 offset) {
    Hit sample = hit;
    sample.position = hit.position + offset;
    sample.local_position = hit.local_position + offset;
    return sample;
  };
  const float dx = surfaceHeightSignal(offset_hit(tangent * detail_step)) -
                   surfaceHeightSignal(offset_hit(tangent * -detail_step));
  const float dy = surfaceHeightSignal(offset_hit(bitangent * detail_step)) -
                   surfaceHeightSignal(offset_hit(bitangent * -detail_step));
  return std::clamp(std::sqrt(dx * dx + dy * dy) / std::max(detail_step * 2.0f, 0.001f),
                    0.0f, 8.0f);
}

struct SurfaceTruthSample {
  MaterialSurfaceProfile profile = MaterialSurfaceProfile::Plain;
  Vec3 base_color{};
  float height = 0.5f;
  float cavity = 0.0f;
  float normal_variance = 0.0f;
  float normal_strength = 0.0f;
  float roughness = 0.5f;
  float metallic = 0.0f;
  float ambient_occlusion = 1.0f;
  float thickness = 1.0f;
  float transmission = 0.0f;
  float subsurface = 0.0f;
  Vec3 absorption{1.0f, 1.0f, 1.0f};
};

SurfaceTruthSample sampleSurfaceTruth(const Hit &hit, const RendererSettings &settings) {
  SurfaceTruthSample truth;
  truth.profile = resolveMaterialSurfaceProfile(hit.material);
  truth.base_color = previewAlbedo(hit);
  truth.height = surfaceHeightSignal(hit);
  truth.normal_variance = surfaceHeightGradientMagnitude(hit, settings);
  truth.roughness = effectiveRoughness(hit, settings);
  truth.metallic = effectiveMetallic(hit);
  truth.ambient_occlusion = effectiveAmbientOcclusion(hit);
  truth.normal_strength = std::max(hit.material.procedural.micro_normal_strength,
                                   hit.material.detail_strength * 0.16f);

  if (truth.profile == MaterialSurfaceProfile::TerrainLayer) {
    truth.cavity = saturate((1.0f - truth.height) * 0.72f + truth.normal_variance * 0.12f);
    truth.normal_strength *= 0.82f + (1.0f - smoothstep(4.0f, 18.0f, hit.distance)) * 0.78f;
    truth.roughness =
        std::clamp(truth.roughness + truth.cavity * hit.material.procedural.height_shading * 0.06f,
                   0.62f, 1.0f);
    truth.ambient_occlusion =
        std::clamp(truth.ambient_occlusion *
                       (1.0f - truth.cavity * hit.material.procedural.height_shading * 0.10f),
                   0.54f, 1.0f);
  } else if (truth.profile == MaterialSurfaceProfile::StratifiedRock) {
    truth.cavity = saturate((1.0f - truth.height) * 0.50f + truth.normal_variance * 0.08f);
    truth.base_color =
        mixVec(truth.base_color, hit.material.base_color.value * Vec3{0.98f, 1.02f, 1.02f}, 0.18f);
    truth.roughness = std::clamp(truth.roughness + truth.cavity * 0.08f, 0.32f, 0.96f);
    truth.ambient_occlusion = std::clamp(truth.ambient_occlusion * (1.0f - truth.cavity * 0.10f),
                                         0.62f, 1.0f);
  } else if (truth.profile == MaterialSurfaceProfile::MineralVein) {
    const float thin = 1.0f - smoothstep(0.30f, 0.86f, truth.height);
    truth.cavity = saturate(std::abs(truth.height - 0.52f) * 0.34f + truth.normal_variance * 0.035f);
    truth.thickness = std::clamp(0.42f + truth.height * 0.38f + (1.0f - thin) * 0.20f,
                                 0.25f, 1.10f);
    truth.transmission = std::clamp(0.16f + thin * 0.22f + hit.material.coat_strength * 0.30f,
                                    0.0f, 0.62f);
    truth.subsurface = std::clamp(0.14f + hit.material.coat_strength * 0.18f + thin * 0.16f,
                                  0.0f, 0.48f);
    truth.absorption = Vec3{0.10f, 0.58f, 0.36f};
    truth.ambient_occlusion = std::clamp(truth.ambient_occlusion * (1.0f - truth.cavity * 0.08f),
                                         0.70f, 1.0f);
  } else if (truth.profile == MaterialSurfaceProfile::Resin &&
             luminanceOf(hit.material.base_color.value) > 0.48f) {
    truth.cavity = saturate((1.0f - truth.height) * 0.22f + truth.normal_variance * 0.05f);
    truth.thickness = std::clamp(0.66f + truth.height * 0.18f, 0.42f, 1.0f);
    truth.transmission = std::clamp(0.035f + hit.material.coat_strength * 0.06f, 0.0f, 0.15f);
    truth.subsurface = std::clamp(0.075f + hit.material.procedural.height_shading * 0.20f,
                                  0.0f, 0.25f);
    truth.absorption = Vec3{1.0f, 0.82f, 0.60f};
    truth.ambient_occlusion = std::clamp(truth.ambient_occlusion * (1.0f - truth.cavity * 0.18f),
                                         0.66f, 1.0f);
  }
  return truth;
}

struct ContactLightingTerms {
  float diffuse_visibility = 1.0f;
  float direct_visibility = 1.0f;
  float specular_visibility = 1.0f;
  float near_contact_ao = 1.0f;
  float bounce_response = 0.0f;
  Vec3 transmitted_ground_irradiance{};
};

Vec3 nearFieldGroundIrradiance(const Hit &hit, const Vec3 normal,
                               const SurfaceTruthSample &truth,
                               const ContactLightingTerms &contact,
                               const RendererSettings &settings) {
  if (!settings.grounding.enabled || hit.material.render_role == MaterialRenderRole::SupportSurface) {
    return {};
  }
  const float height = std::max(hit.position.y - settings.grounding.reference_y, 0.0f);
  const float proximity = 1.0f - smoothstep(0.05f, 0.96f, height);
  const float underside = smoothstep(-0.30f, 0.36f, -normal.y);
  const float grazing = 1.0f - smoothstep(0.22f, 0.86f, normal.y);
  const float bounce =
      proximity * (0.16f + underside * 0.58f + grazing * 0.20f) *
      (0.72f + contact.bounce_response * 0.34f) * std::clamp(truth.ambient_occlusion, 0.0f, 1.0f);
  const Vec3 bounce_tint = mixVec(truth.base_color, truth.absorption, truth.transmission * 0.32f);
  return settings.ground_ambient_color * bounce_tint *
         (bounce * (0.30f + settings.ambient_strength * 0.96f + truth.subsurface * 0.18f));
}

Vec3 materialFamilyVolumeIrradiance(const Hit &hit, const Vec3 normal, const Vec3 view,
                                    const SurfaceTruthSample &truth,
                                    const RendererSettings &settings) {
  if (truth.profile != MaterialSurfaceProfile::MineralVein &&
      truth.profile != MaterialSurfaceProfile::Resin) {
    return {};
  }
  const float n_dot_v = std::max(dot(normal, view), 0.0f);
  const float edge = std::pow(1.0f - n_dot_v, 2.0f);
  const float thin = 1.0f - smoothstep(0.26f, 0.84f, truth.height);
  const Vec3 sun_dir =
      settings.sun_light.enabled && length(settings.sun_light.direction_to_light) > 0.0001f
          ? normalize(settings.sun_light.direction_to_light)
          : Vec3{-0.45f, 0.78f, 0.30f};
  const float back_scatter = smoothstep(-0.28f, 0.42f, dot(normal, -sun_dir));
  const Vec3 incident =
      settings.sky_ambient_color * (0.18f + edge * 0.16f) +
      settings.sun_light.color * std::clamp(settings.sun_light.intensity, 0.0f, 6.0f) *
          (0.020f + back_scatter * 0.026f);

  if (truth.profile == MaterialSurfaceProfile::MineralVein) {
    const Vec3 jade_absorption = mixVec(truth.base_color, truth.absorption, 0.44f);
    const float strength =
        truth.transmission * (0.24f + thin * 0.56f + edge * 0.74f + back_scatter * 0.36f) *
        std::clamp(truth.ambient_occlusion, 0.0f, 1.0f);
    return incident * jade_absorption * strength *
           std::max(1.0f - truth.roughness * 0.42f, 0.35f) / std::max(truth.thickness, 0.25f);
  }

  if (luminanceOf(hit.material.base_color.value) > 0.48f &&
      hit.material.pattern_contrast > 0.55f) {
    const Vec3 porcelain_warmth{1.0f, 0.86f, 0.66f};
    const float pore = smoothstep(0.38f, 0.92f, thin + hit.material.procedural.height_shading);
    return incident * mixVec(truth.base_color, porcelain_warmth, 0.34f) *
           ((truth.subsurface * 0.14f + pore * 0.020f + edge * 0.020f +
             back_scatter * 0.012f) *
            std::clamp(truth.ambient_occlusion, 0.0f, 1.0f));
  }
  return {};
}

ContactLightingTerms contactLightingTermsFor(const Hit &hit, const SurfaceTruthSample &truth,
                                             const std::vector<PreparedObject> &scene,
                                             const RendererSettings &settings) {
  ContactLightingTerms terms;
  if (!settings.grounding.enabled || !settings.grounding.contact_shadows ||
      settings.grounding.contact_shadow_strength <= 0.0f) {
    return terms;
  }

  const float reference_y = settings.grounding.reference_y;
  if (hit.material.render_role != MaterialRenderRole::SupportSurface && hit.normal.y < 0.62f) {
    const float near_ground = 1.0f - smoothstep(0.05f, 0.68f, hit.position.y - reference_y);
    const float underside = smoothstep(-0.25f, 0.42f, -hit.normal.y);
    terms.near_contact_ao =
        std::clamp(1.0f - near_ground * (0.045f + underside * 0.115f) *
                              settings.grounding.contact_shadow_strength *
                              (1.0f + truth.cavity * 0.18f),
                   0.74f, 1.0f);
    terms.diffuse_visibility = terms.near_contact_ao;
    terms.direct_visibility = std::lerp(1.0f, terms.near_contact_ao, 0.28f);
    terms.specular_visibility = std::lerp(1.0f, terms.near_contact_ao, 0.18f);
    terms.bounce_response = near_ground * (0.22f + underside * 0.58f);
    return terms;
  }
  if (hit.material.render_role != MaterialRenderRole::SupportSurface) {
    return terms;
  }

  float near_wedge = 0.0f;
  float penumbra_shadow = 0.0f;
  Vec3 transmitted_ground{};
  const Vec3 light_dir =
      settings.sun_light.enabled ? normalize(settings.sun_light.direction_to_light)
                                 : Vec3{-0.45f, 0.82f, 0.34f};
  for (const PreparedObject &prepared : scene) {
    const RenderObject &object = prepared.object;
    const bool casts_contact = object.casts_contact_shadow ||
                               (settings.grounding.auto_contact_shadows &&
                                object.auto_contact_shadow);
    if (!casts_contact || object.material.render_role == MaterialRenderRole::SupportSurface ||
        resolveMaterialSurfaceProfile(object.material) == MaterialSurfaceProfile::ContactShadow) {
      continue;
    }
    const Vec3 center = (prepared.bounds_min + prepared.bounds_max) * 0.5f;
    const Vec3 half_extents = (prepared.bounds_max - prepared.bounds_min) * 0.5f;
    const float foot_y = prepared.bounds_min.y;
    const float receiver_delta = std::abs(foot_y - reference_y);
    if (receiver_delta > settings.grounding.contact_shadow_receiver_height) {
      continue;
    }
    const float min_radius = std::max(settings.grounding.contact_shadow_min_radius, 0.001f);
    const float max_radius = std::max(settings.grounding.contact_shadow_max_radius, min_radius);
    const float radius_scale = settings.grounding.contact_shadow_radius_scale *
                               object.contact_shadow_radius_scale;
    const float radius_x = std::clamp(half_extents.x * radius_scale + 0.10f, min_radius, max_radius);
    const float radius_z = std::clamp(half_extents.z * radius_scale + 0.10f, min_radius, max_radius);
    const float contact_radius_x =
        std::clamp(half_extents.x * std::max(radius_scale * 0.62f, 0.44f) + 0.040f,
                   min_radius * 0.50f, max_radius);
    const float contact_radius_z =
        std::clamp(half_extents.z * std::max(radius_scale * 0.62f, 0.44f) + 0.040f,
                   min_radius * 0.50f, max_radius);
    const float caster_height = std::max(center.y - reference_y, 0.0f);
    const float projection = caster_height * 0.18f / std::max(light_dir.y, 0.24f);
    const Vec2 shadow_center{center.x - light_dir.x * projection, center.z - light_dir.z * projection};
    const float contact_dx = (hit.position.x - center.x) / std::max(contact_radius_x, 0.001f);
    const float contact_dz = (hit.position.z - center.z) / std::max(contact_radius_z, 0.001f);
    const float contact_d2 = contact_dx * contact_dx + contact_dz * contact_dz;
    const float dx = (hit.position.x - shadow_center.x) / std::max(radius_x, 0.001f);
    const float dz = (hit.position.z - shadow_center.y) / std::max(radius_z, 0.001f);
    const float d2 = dx * dx + dz * dz;
    const float core = 1.0f - smoothstep(0.00f, 0.34f, contact_d2);
    const float penumbra = 1.0f - smoothstep(0.18f, 1.80f, d2);
    const float receiver_fade =
        1.0f - smoothstep(settings.grounding.contact_shadow_receiver_height * 0.70f,
                           settings.grounding.contact_shadow_receiver_height, receiver_delta);
    const float grain = projectedFbm(hit.position, hit.normal,
                                     settings.grounding.contact_shadow_detail_scale, 701.0f);
    const float material_weight =
        (0.84f + grain * 0.16f) * receiver_fade * object.contact_shadow_strength *
        (1.0f + truth.cavity * 0.20f);
    near_wedge += core * material_weight;
    penumbra_shadow += penumbra * material_weight;

    const MaterialSurfaceProfile caster_profile = resolveMaterialSurfaceProfile(object.material);
    if (caster_profile == MaterialSurfaceProfile::MineralVein) {
      transmitted_ground =
          transmitted_ground +
          Vec3{0.030f, 0.260f, 0.190f} * (core * 0.11f + penumbra * 0.045f) *
              material_weight * std::clamp(object.material.coat_strength, 0.0f, 1.0f);
    } else if (caster_profile == MaterialSurfaceProfile::Resin &&
               luminanceOf(object.material.base_color.value) > 0.48f) {
      transmitted_ground =
          transmitted_ground +
          Vec3{0.300f, 0.220f, 0.140f} * (core * 0.045f + penumbra * 0.022f) *
              material_weight * std::clamp(object.material.coat_strength + 0.30f, 0.0f, 1.0f);
    }
  }

  const float strength = settings.grounding.contact_shadow_strength;
  terms.near_contact_ao =
      std::clamp(1.0f - saturate(near_wedge) * strength * 0.54f, 0.52f, 1.0f);
  const float penumbra_visibility =
      std::clamp(1.0f - saturate(penumbra_shadow) * strength * 0.18f, 0.78f, 1.0f);
  terms.diffuse_visibility =
      std::clamp(terms.near_contact_ao *
                     (1.0f - saturate(penumbra_shadow) * strength * 0.08f),
                 0.46f, 1.0f);
  terms.direct_visibility = std::clamp(terms.near_contact_ao * penumbra_visibility, 0.50f, 1.0f);
  terms.specular_visibility =
      std::clamp(1.0f - saturate(near_wedge) * strength * 0.20f -
                     saturate(penumbra_shadow) * strength * 0.08f,
                 0.68f, 1.0f);
  terms.bounce_response = saturate(penumbra_shadow * 0.28f + near_wedge * 0.16f);
  terms.transmitted_ground_irradiance = transmitted_ground;
  return terms;
}

float directionalShadowVisibility(const Hit &hit, const std::vector<PreparedObject> &scene,
                                  const RendererSettings &settings, const Vec3 light_dir) {
  if (!settings.shadows.enabled || !hit.material.receives_shadows || length(light_dir) <= 0.0001f) {
    return 1.0f;
  }
  const float n_dot_l = dot(normalize(hit.normal), normalize(light_dir));
  if (n_dot_l <= 0.02f) {
    return 1.0f;
  }
  const float max_distance = settings.shadows.max_distance > 0.0f ? settings.shadows.max_distance : 64.0f;
  const Ray shadow_ray{hit.position + normalize(hit.normal) * 0.035f +
                           normalize(light_dir) * settings.shadows.receiver_bias,
                       normalize(light_dir)};
  if (!traceShadowRay(shadow_ray, scene, max_distance)) {
    return 1.0f;
  }
  const float penumbra = std::clamp(settings.shadows.pcf_radius * 0.24f + 0.38f, 0.34f, 0.68f);
  return 1.0f - penumbra;
}

float pointLightVisibility(const Vec3 position, const Light &light,
                           const std::vector<PreparedObject> &scene) {
  const Vec3 to_light = light.position - position;
  const float distance = length(to_light);
  if (distance <= 0.035f) {
    return 1.0f;
  }
  const Vec3 direction = to_light / distance;
  const Ray visibility_ray{position + direction * 0.035f, direction};
  return traceShadowRay(visibility_ray, scene, std::max(distance - 0.08f, 0.02f)) ? 0.18f : 1.0f;
}

Vec3 pointLightRadianceAt(const Vec3 position, const Light &light,
                          const std::vector<PreparedObject> &scene,
                          const bool include_visibility) {
  if (light.intensity <= 0.0f) {
    return {};
  }
  const Vec3 light_vector = light.position - position;
  const float distance_sq = std::max(dot(light_vector, light_vector), 0.0001f);
  const float softened_distance =
      std::max(distance_sq, light.source_radius * light.source_radius + 0.0001f);
  const float visibility = include_visibility ? pointLightVisibility(position, light, scene) : 1.0f;
  return light.color * (light.intensity / softened_distance) * visibility;
}

struct VolumetricLightSample {
  Vec3 radiance{};
  float occlusion = 1.0f;
  float transmittance = 1.0f;
  std::uint64_t source_label_hash = 0u;
};

VolumetricLightSample integrateLocalVolumetricLight(const Ray &ray, const float max_distance,
                                                    const RendererSettings &settings,
                                                    const std::vector<PreparedObject> &scene) {
  (void)scene;
  VolumetricLightSample out;
  if (!settings.atmosphere.enabled || settings.atmosphere.fog_strength <= 0.0f ||
      settings.light_rig.empty()) {
    return out;
  }
  const float integration_distance =
      std::clamp(max_distance > 0.0f ? max_distance : settings.atmosphere.fog_end,
                 0.25f, std::max(settings.atmosphere.fog_end, 0.25f));
  const std::uint32_t steps =
      std::clamp(settings.atmosphere.volumetric_light_steps, 2u, 12u);
  const float step_length = integration_distance / static_cast<float>(steps);
  const float density =
      std::clamp(settings.atmosphere.fog_strength * settings.atmosphere.local_light_scattering,
                 0.0f, 0.85f);
  if (density <= 0.00001f || step_length <= 0.00001f) {
    out.transmittance = 1.0f;
    return out;
  }

  const Vec3 light_reference = ray.origin + ray.direction * (integration_distance * 0.48f);
  const std::vector<Light> active_lights =
      selectRenderLights(settings.light_rig, light_reference, settings.light_policy);
  float accumulated_visibility = 0.0f;
  float visibility_weight = 0.0f;
  float transmittance = 1.0f;
  float strongest_source = 0.0f;
  for (std::uint32_t step = 0u; step < steps; ++step) {
    const float t = (static_cast<float>(step) + 0.5f) * step_length;
    const Vec3 sample_position = ray.origin + ray.direction * t;
    Vec3 scatter{};
    float step_visibility = 0.0f;
    float step_weight = 0.0f;
    for (const Light &light : active_lights) {
      if (light.intensity <= 0.0f) {
        continue;
      }
      const Vec3 to_light = light.position - sample_position;
      const float distance_sq = std::max(dot(to_light, to_light), 0.0001f);
      const float light_distance = std::sqrt(distance_sq);
      const Vec3 light_dir = to_light / std::max(light_distance, 0.0001f);
      const float radius = std::max(light.source_radius, 0.08f);
      const float softened = std::max(distance_sq, radius * radius);
      const float visibility = 1.0f;
      const float cos_theta = dot(-ray.direction, light_dir);
      const float g = std::clamp(settings.atmosphere.phase_anisotropy, -0.65f, 0.65f);
      const float phase = (1.0f - g * g) /
                          std::max(1.0f + g * g - 2.0f * g * cos_theta, 0.08f);
      const float source_core =
          1.0f - smoothstep(radius * radius * 0.18f, radius * radius * 3.8f, distance_sq);
      const float phase_weight = std::clamp(phase * 0.055f, 0.008f, 0.090f);
      const Vec3 medium_color = mixVec(light.color, Vec3{1.0f, 0.78f, 0.58f}, 0.38f);
      const Vec3 radiance =
          medium_color * (light.intensity / softened) * visibility *
          (0.014f + phase_weight +
           source_core * settings.atmosphere.source_glow_strength * 0.10f);
      scatter = scatter + radiance;
      const float weight = std::max(luminanceOf(radiance), 0.0f);
      step_visibility += visibility * weight;
      step_weight += weight;
      if (weight > strongest_source) {
        strongest_source = weight;
        out.source_label_hash = stableLabelHash("software-light-source");
      }
    }
    const float extinction =
        std::clamp(settings.atmosphere.local_light_extinction, 0.0f, 4.0f) * density;
    const float attenuation = std::exp(-extinction * t);
    out.radiance = out.radiance + scatter * (density * step_length * 0.22f * attenuation);
    transmittance *= std::exp(-extinction * step_length);
    if (step_weight > 0.0f) {
      accumulated_visibility += step_visibility;
      visibility_weight += step_weight;
    }
  }
  out.transmittance = std::clamp(transmittance, 0.0f, 1.0f);
  out.occlusion = visibility_weight > 0.0f
                      ? std::clamp(accumulated_visibility / visibility_weight, 0.0f, 1.0f)
                      : 1.0f;
  out.radiance =
      compressLocalRadiance(out.radiance,
                            0.24f + settings.atmosphere.source_glow_strength * 0.028f);
  return out;
}

SoftwareLightingProbePixel lightingProbeForRay(const Ray &ray, const Hit &hit,
                                               const RendererSettings &settings,
                                               const std::vector<PreparedObject> &scene) {
  SoftwareLightingProbePixel out;
  const float max_distance = hit.valid ? hit.distance : std::max(settings.atmosphere.fog_end, 8.0f);
  const VolumetricLightSample volumetric =
      integrateLocalVolumetricLight(ray, max_distance, settings, scene);
  out.volumetric_light_luminance = luminanceOf(volumetric.radiance);
  out.occlusion = volumetric.occlusion;
  out.transmittance = volumetric.transmittance;
  out.source_label_hash = volumetric.source_label_hash;
  if (!hit.valid) {
    return out;
  }
  Vec3 direct{};
  for (const Light &light : selectRenderLights(settings.light_rig, hit.position, settings.light_policy)) {
    direct = direct + pointLightRadianceAt(hit.position, light, scene, true);
  }
  out.direct_light_luminance = luminanceOf(direct);
  const Vec3 emissive =
      hit.material.emission_color.value * hit.material.emission_strength *
      std::max(settings.style.emissive_gain, 0.0f);
  out.emissive_luminance = luminanceOf(emissive);
  if (out.emissive_luminance > 0.001f) {
    out.source_readability_luminance = out.emissive_luminance;
    out.source_label_hash = hit.object_label_hash;
  } else {
    out.source_readability_luminance = out.direct_light_luminance + out.volumetric_light_luminance;
  }
  return out;
}

Vec3 shade(const Hit &hit, const Ray &ray, const RendererSettings &settings,
           const std::vector<PreparedObject> &scene,
           const std::vector<ReflectionProbe> &probes) {
  Hit sample_hit = hit;
  sample_hit.position = snapProceduralSamplePosition(hit.position, settings.style);
  const SurfaceTruthSample truth = sampleSurfaceTruth(sample_hit, settings);
  const Vec3 albedo = truth.base_color;
  const MaterialSurfaceProfile surface_profile = truth.profile;
  Vec3 normal = normalize(hit.normal);
  const float normal_strength = truth.normal_strength;
  if (normal_strength > 0.0001f) {
    if (surface_profile == MaterialSurfaceProfile::CorrodedMetal ||
        surface_profile == MaterialSurfaceProfile::WeldBead) {
      normal = perturbAsterSurfaceNormal({.position = sample_hit.position,
                                          .normal = normal,
                                          .uv = sample_hit.uv,
                                          .detail_scale = hit.material.detail_scale},
                                         hit.material.procedural, {1.0f, 0.0f, 0.0f},
                                         std::clamp(normal_strength, 0.0f, 0.92f));
    } else {
      normal = perturbNormalFromHeight(sample_hit, normal, std::clamp(normal_strength, 0.0f, 0.90f),
                                       settings);
    }
  }

  const Vec3 view = normalize(ray.origin - hit.position);
  const ContactLightingTerms contact = contactLightingTermsFor(sample_hit, truth, scene, settings);
  const float material_ao = truth.ambient_occlusion;
  const float ambient =
      std::max(settings.ambient_strength * material_ao * contact.near_contact_ao,
               settings.ambient_floor);
  const Vec3 diffuse_ibl = environmentRadiance(normal, settings, probes);
  Vec3 color = diffuse_ibl * albedo * ambient +
               albedo * std::max(settings.indirect_albedo_floor, 0.0f);
  if (hit.material.render_role == MaterialRenderRole::SupportSurface) {
    color = color + contact.transmitted_ground_irradiance *
                        (0.62f + settings.ambient_strength * 0.50f);
  }

  const float roughness = truth.roughness;
  const float metallic = truth.metallic;
  color = color + nearFieldGroundIrradiance(sample_hit, normal, truth, contact, settings);
  if (settings.material_debug_view != MaterialDebugView::Beauty) {
    switch (settings.material_debug_view) {
    case MaterialDebugView::BaseColor:
      return gamma_encode(clamp(albedo, 0.0f, 4.0f));
    case MaterialDebugView::Normal:
      return {normal.x * 0.5f + 0.5f, normal.y * 0.5f + 0.5f, normal.z * 0.5f + 0.5f};
    case MaterialDebugView::Roughness:
      return {roughness, roughness, roughness};
    case MaterialDebugView::AmbientOcclusion: {
      const float ao = std::clamp(truth.ambient_occlusion, 0.0f, 1.0f);
      return {ao, ao, ao};
    }
    case MaterialDebugView::Fog: {
      const float fog = evaluateFogFactor(settings.atmosphere, length(ray.origin - hit.position));
      return mixVec({0.02f, 0.025f, 0.03f}, settings.atmosphere.fog_color, fog);
    }
    case MaterialDebugView::Beauty:
      break;
    }
  }
  const float alpha = roughness * roughness;
  const float alpha2 = std::max(alpha * alpha, 0.0005f);
  const float n_dot_v = std::max(dot(normal, view), 0.001f);
  const float k = ((roughness + 1.0f) * (roughness + 1.0f)) * 0.125f;
  const float reflectance =
      std::clamp(hit.material.dielectric_reflectance, 0.0f, 1.0f);
  const float dielectric_f0 = std::clamp(0.16f * reflectance * reflectance, 0.018f, 0.16f);
  const Vec3 f0 = mixVec({dielectric_f0, dielectric_f0, dielectric_f0}, albedo, metallic);
  const float coat_strength = std::clamp(hit.material.coat_strength, 0.0f, 1.0f);
  const float coat_roughness = std::clamp(hit.material.coat_roughness, 0.025f, 1.0f);
  const float coat_alpha = coat_roughness * coat_roughness;
  const float coat_alpha2 = std::max(coat_alpha * coat_alpha, 0.0005f);
  const float coat_k = ((coat_roughness + 1.0f) * (coat_roughness + 1.0f)) * 0.125f;
  const float anisotropy = std::clamp(hit.material.tangent_anisotropy, -0.95f, 0.95f);
  const Vec3 tangent = orthogonalTangent(hit.tangent, normal);
  const Vec3 sheen_color = hit.material.edge_sheen_color;
  const float sheen_strength =
      std::clamp(maxComponent(sheen_color), 0.0f, 2.0f);
  const float sheen_roughness = std::clamp(hit.material.edge_sheen_roughness, 0.0f, 1.0f);
  const Vec3 view_fresnel =
      f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - n_dot_v, 5.0f);
  const float average_fresnel = std::clamp(luminanceOf(view_fresnel), 0.0f, 1.0f);

  const auto add_light = [&](const Vec3 light_dir, const Vec3 radiance, const float visibility) {
    const Vec3 half_vector = normalize(light_dir + view);
    const float n_dot_l = std::max(dot(normal, light_dir), 0.0f);
    const float n_dot_h = std::max(dot(normal, half_vector), 0.0f);
    const float h_dot_v = std::max(dot(half_vector, view), 0.0f);
    const float distribution_denominator = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;
    float distribution =
        alpha2 / std::max(kPi * distribution_denominator * distribution_denominator, 0.001f);
    if (std::abs(anisotropy) > 0.0001f) {
      const Vec3 bitangent = normalize(cross(normal, tangent));
      float alpha_t = std::max(alpha * (1.0f + std::abs(anisotropy) * 0.92f), 0.025f);
      float alpha_b = std::max(alpha * (1.0f - std::abs(anisotropy) * 0.66f), 0.025f);
      if (anisotropy < 0.0f) {
        std::swap(alpha_t, alpha_b);
      }
      const float t_dot_h = dot(tangent, half_vector);
      const float b_dot_h = dot(bitangent, half_vector);
      const float anisotropic_denominator =
          t_dot_h * t_dot_h / (alpha_t * alpha_t) +
          b_dot_h * b_dot_h / (alpha_b * alpha_b) + n_dot_h * n_dot_h;
      distribution =
          1.0f / std::max(kPi * alpha_t * alpha_b * anisotropic_denominator *
                              anisotropic_denominator,
                          0.001f);
    }
    const float geometry_l = n_dot_l / std::max(n_dot_l * (1.0f - k) + k, 0.001f);
    const float geometry_v = n_dot_v / std::max(n_dot_v * (1.0f - k) + k, 0.001f);
    const Vec3 fresnel = f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - h_dot_v, 5.0f);
    const float brdf_denominator = std::max(4.0f * n_dot_l * n_dot_v, 0.04f);
    Vec3 specular = fresnel * (distribution * geometry_l * geometry_v / brdf_denominator);
    if (coat_strength > 0.0001f) {
      const float coat_denominator = n_dot_h * n_dot_h * (coat_alpha2 - 1.0f) + 1.0f;
      const float coat_distribution =
          coat_alpha2 / std::max(kPi * coat_denominator * coat_denominator, 0.001f);
      const float coat_geometry_l =
          n_dot_l / std::max(n_dot_l * (1.0f - coat_k) + coat_k, 0.001f);
      const float coat_geometry_v =
          n_dot_v / std::max(n_dot_v * (1.0f - coat_k) + coat_k, 0.001f);
      const float coat_fresnel = 0.04f + 0.96f * std::pow(1.0f - h_dot_v, 5.0f);
      specular = specular * (1.0f - coat_strength * 0.20f) +
                 Vec3{coat_fresnel, coat_fresnel, coat_fresnel} *
                     (coat_distribution * coat_geometry_l * coat_geometry_v * coat_strength /
                      brdf_denominator);
    }
    const float sheen = sheen_strength <= 0.0001f
                            ? 0.0f
                            : std::pow(1.0f - h_dot_v, 5.0f) *
                                  (0.22f + sheen_roughness * 0.36f) * (1.0f - metallic);
    const float silhouette_specular =
        std::lerp(0.54f, 1.0f, smoothstep(0.025f, 0.18f, n_dot_v));
    specular = specular * silhouette_specular;
    const Vec3 diffuse =
        albedo * ((1.0f - metallic) * (1.0f - average_fresnel) *
                  (1.0f - coat_strength * 0.10f) / kPi);
    color = color + (diffuse + specular + sheen_color * sheen * silhouette_specular) *
                        (radiance * n_dot_l * contact.direct_visibility * visibility);
  };

  if (settings.sun_light.enabled && settings.sun_light.intensity > 0.0f) {
    const Vec3 sun_dir = normalize(settings.sun_light.direction_to_light);
    if (length(sun_dir) > 0.0001f) {
      add_light(sun_dir, settings.sun_light.color * settings.sun_light.intensity,
                directionalShadowVisibility(sample_hit, scene, settings, sun_dir));
    }
  }
  const std::vector<Light> selected_lights =
      selectRenderLights(settings.light_rig, hit.position, settings.light_policy);
  for (const Light &light : selected_lights) {
    if (light.intensity <= 0.0f) {
      continue;
    }
    const Vec3 light_vector = light.position - hit.position;
    const float distance_sq = std::max(dot(light_vector, light_vector), 0.0001f);
    const float softened_distance =
        std::max(distance_sq, light.source_radius * light.source_radius + 0.0001f);
    add_light(normalize(light_vector), light.color * (light.intensity / softened_distance), 1.0f);
  }

  if (settings.reflections.enabled || settings.reflections.fallback_intensity > 0.0f) {
    const Vec3 reflection = normalize(reflect(-view, normal));
    const Vec3 softened_reflection =
        normalize(mixVec(reflection, normal, std::clamp(roughness * 0.42f + 0.05f, 0.0f, 0.72f)));
    const Vec3 env_specular = mixVec(environmentRadiance(reflection, settings, probes),
                                     environmentRadiance(softened_reflection, settings, probes),
                                     std::clamp(roughness * 0.72f, 0.0f, 1.0f));
    const float gloss = std::pow(1.0f - roughness, 1.65f);
    const float probe_gain = std::clamp(settings.reflections.fallback_intensity, 0.0f, 2.0f);
    const float specular_occlusion =
        std::clamp(contact.specular_visibility *
                       std::lerp(0.62f, 1.0f, material_ao),
                   0.0f, 1.0f);
    const Vec3 coat_fresnel =
        Vec3{0.04f, 0.04f, 0.04f} +
        Vec3{0.96f, 0.96f, 0.96f} * std::pow(1.0f - n_dot_v, 5.0f);
    const float silhouette_specular =
        std::lerp(0.48f, 1.0f, smoothstep(0.025f, 0.22f, n_dot_v));
    Vec3 environment_specular = env_specular;
    if (surface_profile == MaterialSurfaceProfile::OrganicFiber && metallic > 0.50f) {
      const float brushed_axis = std::abs(dot(reflection, tangent));
      const float brushed_band =
          smoothstep(0.58f, 0.98f, brushed_axis) *
          (0.55f + smoothstep(0.48f, 0.96f, truth.height) * 0.22f);
      environment_specular =
          environment_specular +
          mixVec(settings.ground_ambient_color, settings.sky_ambient_color,
                 smoothstep(-0.18f, 0.42f, reflection.y)) *
              (brushed_band * hit.material.tangent_anisotropy * 0.18f);
    }
    const float env_brdf_scale =
        std::clamp((0.62f + n_dot_v * 0.38f) * (1.0f - roughness * 0.34f), 0.18f, 1.0f);
    const float env_brdf_bias =
        std::pow(1.0f - n_dot_v, 5.0f) * (0.018f + (1.0f - roughness) * 0.050f);
    color = color + environment_specular *
                        (view_fresnel * (0.08f + gloss * 0.58f) * env_brdf_scale +
                         Vec3{env_brdf_bias, env_brdf_bias, env_brdf_bias} +
                         coat_fresnel * (coat_strength * (0.05f + gloss * 0.22f))) *
                        probe_gain * specular_occlusion * silhouette_specular;
  }

  color = color + materialFamilyVolumeIrradiance(sample_hit, normal, view, truth, settings);

  color = mixVec(color,
                 albedo * std::max(settings.ambient_strength + settings.ambient_floor + 0.14f,
                                   0.18f),
                 std::clamp(settings.style.unlit_mix, 0.0f, 1.0f));
  color = color + hit.material.emission_color.value * hit.material.emission_strength *
                      std::max(settings.style.emissive_gain, 0.0f);
  if (hit.material.render_role != MaterialRenderRole::SupportSurface) {
    const float silhouette_air =
        (1.0f - smoothstep(0.035f, 0.18f, n_dot_v)) *
        std::clamp(0.12f + settings.atmosphere.fog_strength * 0.34f, 0.12f, 0.34f);
    const Vec3 rim_fill =
        mixVec(environmentRadiance(ray.direction, settings, probes),
               environmentRadiance(reflect(-view, normalize(hit.normal)), settings, probes), 0.45f);
    color = mixVec(color, rim_fill, silhouette_air);
  }
  if (hit.material.render_role == MaterialRenderRole::SupportSurface) {
    const float grazing = 1.0f - smoothstep(0.18f, 0.74f, std::abs(dot(normalize(hit.normal), view)));
    const float distance_haze = smoothstep(4.0f, 13.5f, hit.distance);
    const Vec3 horizon_air = environmentRadiance(ray.direction, settings, probes);
    const Vec3 aerial_horizon =
        mixVec(horizon_air,
               settings.sky_ambient_color * 0.88f + settings.atmosphere.fog_color * 0.72f,
               0.44f);
    const float ground_luma = luminanceOf(color);
    const Vec3 contrast_attenuated =
        mixVec(color, Vec3{ground_luma, ground_luma, ground_luma} * 0.92f +
                          aerial_horizon * 0.08f,
               distance_haze * 0.22f);
    color = mixVec(contrast_attenuated, aerial_horizon,
                   distance_haze * (0.42f + grazing * 0.48f) *
                       std::clamp(0.58f + settings.atmosphere.fog_strength * 0.25f,
                                  0.45f, 0.82f));
  }
  if (settings.atmosphere.enabled) {
    const float fog = evaluateFogFactor(settings.atmosphere, length(ray.origin - hit.position));
    const VolumetricLightSample volumetric =
        integrateLocalVolumetricLight(ray, hit.distance, settings, scene);
    const Vec3 display_volume =
        compressLocalRadiance(volumetric.radiance,
                              0.16f + settings.atmosphere.source_glow_strength * 0.022f);
    const Vec3 light_aware_fog = settings.atmosphere.fog_color + display_volume * 0.36f;
    color = mixVec(color + display_volume * 0.34f, light_aware_fog, fog);
  }
  color = cameraResponseTonemap(color, settings);
  color = applyAtmosphereGrade(color, settings.atmosphere);
  color = applyRenderStylePost(color, settings.style);
  return gamma_encode(color);
}

Vec3 skyColor(const Ray &ray, const RendererSettings &settings,
              const std::vector<ReflectionProbe> &probes) {
  const Vec3 base = environmentRadiance(ray.direction, settings, probes);
  Vec3 color = cameraResponseTonemap(base, settings);
  const Vec3 dir = normalize(ray.direction);
  const float altitude = smoothstep(-0.06f, 0.56f, dir.y);
  color = mixVec(color, mixVec(Vec3{0.20f, 0.28f, 0.42f}, Vec3{0.30f, 0.52f, 0.90f}, altitude),
                 0.42f);
  const float high_air_variation =
      valueNoise({dir.x * 4.5f - dir.z * 1.4f + 3.0f, dir.y * 5.0f + 8.0f,
                  dir.z * 3.6f + dir.x * 0.8f});
  color = color + (high_air_variation - 0.5f) * 0.026f *
                      mixVec(Vec3{0.52f, 0.60f, 0.70f}, Vec3{0.22f, 0.38f, 0.62f}, altitude);
  const float cirrus_band =
      smoothstep(-0.08f, 0.24f, dir.y) * (1.0f - smoothstep(0.58f, 0.94f, dir.y));
  const float cirrus_streak =
      0.5f + 0.5f * std::sin((dir.x * 12.0f + dir.z * 4.0f +
                               valueNoise({dir.x * 7.0f, dir.y * 3.0f, dir.z * 7.0f}) * 3.2f) *
                              1.8f);
  color = mixVec(color, Vec3{0.58f, 0.66f, 0.78f},
                 cirrus_band * smoothstep(0.32f, 0.82f, cirrus_streak) * 0.16f);
  const float cloud =
      smoothstep(0.34f, 0.68f,
                 valueNoise({dir.x * 5.8f + dir.z * 2.0f + 13.0f,
                             dir.y * 3.4f + 9.0f,
                             dir.z * 5.2f - dir.x * 1.4f})) *
      smoothstep(-0.04f, 0.24f, dir.y) * (1.0f - smoothstep(0.72f, 0.98f, dir.y));
  const float horizon_glow = std::exp(-std::abs(dir.y) * 8.0f);
  color = mixVec(color, Vec3{0.64f, 0.72f, 0.82f}, cloud * 0.18f);
  color = mixVec(color, Vec3{0.38f, 0.48f, 0.62f} + settings.sun_light.color * 0.08f,
                 horizon_glow * 0.18f);
  color = applyAtmosphereGrade(color, settings.atmosphere);
  color = applyRenderStylePost(color, settings.style);
  return gamma_encode(color);
}

} // namespace

SoftwarePreviewResult renderSoftwarePreviewWithProbe(const Scene &scene,
                                                     const OrbitCamera &camera,
                                                     const SoftwarePreviewOptions &options) {
  if (options.width <= 0 || options.height <= 0) {
    throw std::invalid_argument("Software preview dimensions must be positive.");
  }

  const int samples_per_axis = std::clamp(options.samples_per_axis, 1, 4);
  const float inv_sample_count =
      1.0f / static_cast<float>(samples_per_axis * samples_per_axis);
  const std::vector<PreparedObject> prepared_scene = prepareScene(scene);
  const std::vector<ReflectionProbe> &reflection_probes = scene.reflectionProbes();
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(options.width) *
                                 static_cast<std::size_t>(options.height) * 4u);
  SoftwarePreviewProbeBuffer probe;
  probe.width = options.width;
  probe.height = options.height;
  probe.pixels.resize(static_cast<std::size_t>(options.width) *
                      static_cast<std::size_t>(options.height));
  SoftwareLightingProbeBuffer lighting;
  lighting.width = options.width;
  lighting.height = options.height;
  lighting.pixels.resize(static_cast<std::size_t>(options.width) *
                         static_cast<std::size_t>(options.height));

  for (int y = 0; y < options.height; ++y) {
    for (int x = 0; x < options.width; ++x) {
      Vec3 accumulated{};
      SoftwarePreviewProbePixel probe_pixel;
      probe_pixel.distance = std::numeric_limits<float>::max();
      SoftwareLightingProbePixel lighting_pixel;
      for (int sy = 0; sy < samples_per_axis; ++sy) {
        for (int sx = 0; sx < samples_per_axis; ++sx) {
          const float sample_x =
              static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / samples_per_axis;
          const float sample_y =
              static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / samples_per_axis;
          const CameraRay camera_ray =
              camera.screenRay(ScreenPoint{sample_x, sample_y, 0.0f},
                               Viewport{{}, {static_cast<float>(options.width),
                                             static_cast<float>(options.height)}});
          const Ray ray{camera_ray.origin.value, camera_ray.direction.value};
          const Hit hit = trace(ray, prepared_scene);
          if (hit.valid && (probe_pixel.hit == 0u || hit.distance < probe_pixel.distance)) {
            probe_pixel.hit = 1u;
            probe_pixel.distance = hit.distance;
            probe_pixel.world_position = hit.position;
            probe_pixel.normal = hit.normal;
            probe_pixel.render_role = hit.material.render_role;
            probe_pixel.depth_layer = hit.material.depth_policy.layer;
            probe_pixel.object_label_hash = hit.object_label_hash;
            lighting_pixel = lightingProbeForRay(ray, hit, options.settings, prepared_scene);
          }
          if (!hit.valid && probe_pixel.hit == 0u) {
            lighting_pixel = lightingProbeForRay(ray, hit, options.settings, prepared_scene);
          }
          accumulated = accumulated +
                        (hit.valid ? shade(hit, ray, options.settings, prepared_scene,
                                           reflection_probes)
                                   : skyColor(ray, options.settings, reflection_probes));
        }
      }
      const Vec3 color = accumulated * inv_sample_count;
      const std::size_t base =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(options.width) +
           static_cast<std::size_t>(x)) *
          4u;
      rgba[base + 0u] = toByte(color.x);
      rgba[base + 1u] = toByte(color.y);
      rgba[base + 2u] = toByte(color.z);
      rgba[base + 3u] = 255u;
      if (probe_pixel.hit == 0u) {
        probe_pixel.distance = 0.0f;
      }
      probe.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(options.width) +
                   static_cast<std::size_t>(x)] = probe_pixel;
      lighting.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(options.width) +
                      static_cast<std::size_t>(x)] = lighting_pixel;
    }
  }

  SoftwarePreviewResult result;
  result.framebuffer.replaceRgba8(options.width, options.height, rgba);
  result.probe = std::move(probe);
  result.lighting = std::move(lighting);
  return result;
}

SoftwareFrameBuffer renderSoftwarePreview(const Scene &scene, const OrbitCamera &camera,
                                          const SoftwarePreviewOptions &options) {
  return renderSoftwarePreviewWithProbe(scene, camera, options).framebuffer;
}

} // namespace aster
