// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_device.hpp"

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/core/profiler.hpp"
#include "aster/math/color.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/render/render_graph_executor.hpp"
#include "aster/scene/scene.hpp"
#include "native_render_backend.hpp"
#include "render_backend_common.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr int kSphereSegments = 48;
constexpr int kSphereRings = 24;
constexpr int kRockSegments = 28;
constexpr int kRockRings = 16;
constexpr int kCrystalSides = 6;
constexpr int kPillarSides = 10;
constexpr float kPlaneSize = 12.0f;
constexpr float kContactShadowPlaneSize = 2.0f;
constexpr float kTau = 6.28318530718f;

struct Vec4f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;
};

struct LocalBounds {
  aster::Vec3 min{};
  aster::Vec3 max{};
};

struct ProjectedVertex {
  bool valid = false;
  aster::Vec3 world_position{};
  aster::Vec3 normal{};
  aster::Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  aster::Vec2 uv{};
  float ambient_occlusion = 1.0f;
  float x = 0.0f;
  float y = 0.0f;
  float depth = 1.0f;
};

struct SoftwareShadowCascade {
  aster::Vec3 center{};
  aster::Vec3 right{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
  aster::Vec3 forward{0.0f, -1.0f, 0.0f};
  float radius = 1.0f;
  std::uint32_t tile_x = 0u;
  std::uint32_t tile_y = 0u;
  std::uint32_t tile_width = 0u;
  std::uint32_t tile_height = 0u;
};

struct SoftwareProbeSample {
  aster::Vec3 position{};
  float influence_radius = 1.0f;
  aster::Vec3 sky_irradiance{};
  aster::Vec3 ground_irradiance{};
  aster::Vec3 specular_tint{1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
};

struct SoftwareFrameResources {
  std::uint32_t frame_width = 0u;
  std::uint32_t frame_height = 0u;
  std::uint32_t shadow_atlas_size = 0u;
  std::uint32_t fog_width = 0u;
  std::uint32_t fog_height = 0u;
  std::uint32_t reflection_width = 0u;
  std::uint32_t reflection_height = 0u;
  std::vector<SoftwareShadowCascade> cascades;
  std::vector<float> shadow_depths;
  std::vector<std::uint8_t> shadow_rgba8;
  std::vector<float> fog_factors;
  std::vector<std::uint8_t> fog_rgba8;
  std::vector<SoftwareProbeSample> probes;
  std::vector<std::uint8_t> reflection_rgba8;
  bool shadow_ready = false;
  bool fog_ready = false;
  bool reflection_ready = false;
};

float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

aster::Vec3 mixVec(const aster::Vec3 a, const aster::Vec3 b, const float t) {
  const float amount = saturate(t);
  return a * (1.0f - amount) + b * amount;
}

std::uint8_t debugByte(const float value) {
  return static_cast<std::uint8_t>(
      std::clamp(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0l, 255l));
}

std::uint64_t hashDebugBytes(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const std::uint8_t byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

float evaluateFogFactor(const aster::AtmosphereSettings &atmosphere,
                        const float distance_to_camera) {
  if (!atmosphere.enabled || atmosphere.fog_strength <= 0.0f) {
    return 0.0f;
  }
  const float range = std::max(atmosphere.fog_end - atmosphere.fog_start, 0.001f);
  const float normalized = std::max((distance_to_camera - atmosphere.fog_start) / range, 0.0f);
  const float power = std::max(atmosphere.fog_power, 0.001f);
  float curve = 0.0f;
  switch (atmosphere.fog_falloff) {
  case aster::AtmosphereFogFalloff::SmoothLinear:
    curve = smoothstep(0.0f, 1.0f, normalized);
    break;
  case aster::AtmosphereFogFalloff::Exponential:
    curve = 1.0f - std::exp(-normalized * power);
    break;
  case aster::AtmosphereFogFalloff::Powered:
    curve = std::pow(saturate(normalized), power);
    break;
  }
  return saturate(curve * std::clamp(atmosphere.fog_strength, 0.0f, 1.0f));
}

float sampleSoftwareShadowVisibility(const SoftwareFrameResources *resources,
                                     const aster::Vec3 world_position,
                                     const aster::Vec3 normal,
                                     const aster::RendererShadowSettings &settings) {
  if (resources == nullptr || !resources->shadow_ready || resources->shadow_depths.empty() ||
      resources->shadow_atlas_size == 0u || resources->cascades.empty()) {
    return 1.0f;
  }
  const SoftwareShadowCascade *selected = nullptr;
  const float distance_to_center = aster::length(world_position - resources->cascades.front().center);
  for (const SoftwareShadowCascade &cascade : resources->cascades) {
    if (distance_to_center <= cascade.radius || selected == nullptr) {
      selected = &cascade;
      if (distance_to_center <= cascade.radius) {
        break;
      }
    }
  }
  if (selected == nullptr || selected->tile_width == 0u || selected->tile_height == 0u) {
    return 1.0f;
  }

  const aster::Vec3 receiver =
      world_position + aster::normalizeOr(normal, {0.0f, 1.0f, 0.0f}) *
                           std::max(settings.normal_bias, 0.0f);
  const aster::Vec3 offset = receiver - selected->center;
  const float u = aster::dot(offset, selected->right) / (selected->radius * 2.0f) + 0.5f;
  const float v = aster::dot(offset, selected->up) / (selected->radius * 2.0f) + 0.5f;
  if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
    return 1.0f;
  }

  const int local_x = static_cast<int>(std::floor(u * static_cast<float>(selected->tile_width)));
  const int local_y = static_cast<int>(std::floor(v * static_cast<float>(selected->tile_height)));
  const int pcf_radius =
      std::max(0, static_cast<int>(std::lround(std::clamp(settings.pcf_radius, 0.0f, 4.0f))));
  const float receiver_depth = aster::dot(receiver, selected->forward);
  const float receiver_bias = std::max(settings.receiver_bias, 0.0f) +
                              std::max(settings.normal_bias, 0.0f) * 0.5f;
  int samples = 0;
  int occluded = 0;
  for (int oy = -pcf_radius; oy <= pcf_radius; ++oy) {
    const int y = local_y + oy;
    if (y < 0 || y >= static_cast<int>(selected->tile_height)) {
      continue;
    }
    for (int ox = -pcf_radius; ox <= pcf_radius; ++ox) {
      const int x = local_x + ox;
      if (x < 0 || x >= static_cast<int>(selected->tile_width)) {
        continue;
      }
      const std::uint32_t atlas_x = selected->tile_x + static_cast<std::uint32_t>(x);
      const std::uint32_t atlas_y = selected->tile_y + static_cast<std::uint32_t>(y);
      const std::size_t index = static_cast<std::size_t>(atlas_y) * resources->shadow_atlas_size +
                                static_cast<std::size_t>(atlas_x);
      if (index >= resources->shadow_depths.size()) {
        continue;
      }
      const float caster_depth = resources->shadow_depths[index];
      if (!std::isfinite(caster_depth)) {
        continue;
      }
      ++samples;
      if (receiver_depth > caster_depth + receiver_bias) {
        ++occluded;
      }
    }
  }
  if (samples == 0) {
    return 1.0f;
  }
  const float occlusion = static_cast<float>(occluded) / static_cast<float>(samples);
  return 1.0f - occlusion * 0.72f;
}

aster::Vec3 softwareReflectionEnvironment(const SoftwareFrameResources *resources,
                                          const aster::Vec3 world_position,
                                          const aster::Vec3 reflection,
                                          const aster::RendererSettings &settings) {
  const float env_sky = saturate(reflection.y * 0.5f + 0.5f);
  const aster::Vec3 fallback_env =
      mixVec(settings.ground_ambient_color, settings.sky_ambient_color, env_sky);
  if (resources == nullptr || !resources->reflection_ready || resources->probes.empty() ||
      !settings.reflections.static_local_probes) {
    return fallback_env;
  }

  aster::Vec3 accumulated{};
  float total_weight = 0.0f;
  for (const SoftwareProbeSample &probe : resources->probes) {
    const float radius = std::max(probe.influence_radius, 0.001f);
    const float distance = aster::length(world_position - probe.position);
    const float weight = 1.0f - smoothstep(radius * 0.72f, radius, distance);
    if (weight <= 0.0001f) {
      continue;
    }
    const aster::Vec3 probe_env =
        mixVec(probe.ground_irradiance, probe.sky_irradiance, env_sky) * probe.specular_tint *
        std::clamp(probe.intensity, 0.0f, 4.0f);
    accumulated = accumulated + probe_env * weight;
    total_weight += weight;
  }
  if (total_weight <= 0.0001f) {
    return fallback_env;
  }
  return mixVec(fallback_env, accumulated / total_weight, saturate(total_weight));
}

aster::Vec3 snapProceduralSamplePosition(const aster::Vec3 world_position,
                                         const aster::RenderStyleProfile &style) {
  const float step = std::max(style.procedural_sample_snap, 0.0f);
  if (step <= 0.0001f) {
    return world_position;
  }
  return {std::floor(world_position.x / step + 0.5f) * step,
          std::floor(world_position.y / step + 0.5f) * step,
          std::floor(world_position.z / step + 0.5f) * step};
}

aster::Vec3 applyRenderStylePost(aster::Vec3 color, const aster::RenderStyleProfile &style) {
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

std::string normalizeStyleName(const std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const char ch : value) {
    if (ch == '_' || ch == ' ') {
      out.push_back('-');
    } else {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  return out;
}

aster::MeshProcessOptions renderMeshOptions() {
  aster::MeshProcessOptions options;
  options.generate_tangents = false;
  return options;
}

Vec4f transformPoint4(const aster::Mat4 &matrix, const aster::Vec3 point) {
  return {
      matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
      matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
      matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14],
      matrix.m[3] * point.x + matrix.m[7] * point.y + matrix.m[11] * point.z + matrix.m[15],
  };
}

float fractValue(const float value) {
  return value - std::floor(value);
}

float hash31(aster::Vec3 p) {
  p = {fractValue(p.x * 0.1031f), fractValue(p.y * 0.11369f), fractValue(p.z * 0.13787f)};
  const float d = p.x * (p.y + 19.19f) + p.y * (p.z + 19.19f) + p.z * (p.x + 19.19f);
  p = p + aster::Vec3{d, d, d};
  return fractValue((p.x + p.y) * p.z);
}

float valueNoise(const aster::Vec3 p) {
  const aster::Vec3 i{std::floor(p.x), std::floor(p.y), std::floor(p.z)};
  aster::Vec3 f{fractValue(p.x), fractValue(p.y), fractValue(p.z)};
  f = f * f * (aster::Vec3{3.0f, 3.0f, 3.0f} - f * 2.0f);

  const float n000 = hash31(i + aster::Vec3{0.0f, 0.0f, 0.0f});
  const float n100 = hash31(i + aster::Vec3{1.0f, 0.0f, 0.0f});
  const float n010 = hash31(i + aster::Vec3{0.0f, 1.0f, 0.0f});
  const float n110 = hash31(i + aster::Vec3{1.0f, 1.0f, 0.0f});
  const float n001 = hash31(i + aster::Vec3{0.0f, 0.0f, 1.0f});
  const float n101 = hash31(i + aster::Vec3{1.0f, 0.0f, 1.0f});
  const float n011 = hash31(i + aster::Vec3{0.0f, 1.0f, 1.0f});
  const float n111 = hash31(i + aster::Vec3{1.0f, 1.0f, 1.0f});

  const float nx00 = std::lerp(n000, n100, f.x);
  const float nx10 = std::lerp(n010, n110, f.x);
  const float nx01 = std::lerp(n001, n101, f.x);
  const float nx11 = std::lerp(n011, n111, f.x);
  const float nxy0 = std::lerp(nx00, nx10, f.y);
  const float nxy1 = std::lerp(nx01, nx11, f.y);
  return std::lerp(nxy0, nxy1, f.z);
}

bool isTerrainProfile(const aster::MaterialSurfaceProfile profile) {
  return profile == aster::MaterialSurfaceProfile::TerrainLayer;
}

float projectedNoise(const aster::Vec3 world_position, aster::Vec3 normal, const float scale,
                     const float salt) {
  normal = aster::normalize(normal);
  if (aster::length(normal) <= 0.0001f) {
    normal = {0.0f, 1.0f, 0.0f};
  }
  aster::Vec3 weights{std::pow(std::abs(normal.x), 4.0f), std::pow(std::abs(normal.y), 4.0f),
                      std::pow(std::abs(normal.z), 4.0f)};
  const float weight_sum = std::max(weights.x + weights.y + weights.z, 0.0001f);
  weights = weights / weight_sum;
  const float xy = valueNoise({world_position.x * scale, world_position.y * scale, salt});
  const float xz = valueNoise({world_position.x * scale, world_position.z * scale, salt + 11.7f});
  const float zy = valueNoise({world_position.z * scale, world_position.y * scale, salt + 23.4f});
  return xy * weights.z + xz * weights.y + zy * weights.x;
}

float terrainFbm(const aster::Vec3 world_position, const aster::Vec3 normal, const float scale,
                 const float salt) {
  float sum = 0.0f;
  float amplitude = 0.56f;
  float amplitude_sum = 0.0f;
  float frequency = scale;
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

aster::Vec3 terrainLayeredAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                                 const aster::Vec3 normal) {
  const float pattern_id = static_cast<float>(
      aster::materialSurfaceProfileId(aster::resolveMaterialSurfaceProfile(material)));
  const float detail_scale = std::max(material.detail_scale, 0.001f);
  const float macro_scale = 0.018f + std::sqrt(detail_scale) * 0.011f;
  const float mid_scale = 0.055f + detail_scale * 0.018f;
  const float fine_scale = 0.38f + detail_scale * 0.055f;
  const float macro = terrainFbm(world_position, normal, macro_scale, pattern_id * 3.7f);
  const float mid = terrainFbm(world_position, normal, mid_scale, pattern_id * 5.1f + 9.0f);
  const float fine = terrainFbm(world_position, normal, fine_scale, pattern_id * 7.9f + 2.0f);
  const float ridge = 1.0f - std::abs(terrainFbm(world_position, normal, mid_scale * 1.85f,
                                                 pattern_id * 2.4f + 31.0f) *
                                          2.0f -
                                      1.0f);
  const float up = saturate(aster::normalize(normal).y);
  const float slope = 1.0f - smoothstep(0.54f, 0.92f, up);
  const float altitude = smoothstep(-0.20f, 2.80f, world_position.y + (macro - 0.5f) * 0.65f);
  const float soil_weight = saturate(slope * 0.46f + (0.54f - mid) * 0.46f + ridge * 0.10f);
  const float rock_weight =
      saturate(smoothstep(0.70f, 1.08f, slope + ridge * 0.14f + altitude * 0.04f)) * 0.52f;
  const float grass_weight =
      saturate(0.70f + (macro - 0.45f) * 0.28f + (fine - 0.50f) * 0.12f - soil_weight * 0.36f -
               rock_weight * 0.58f + material.pattern_depth * 1.10f);

  const aster::Vec3 base = material.base_color;
  const aster::Vec3 grass =
      mixVec(base * aster::Vec3{0.78f, 1.04f, 0.58f}, {0.18f, 0.28f, 0.12f}, 0.24f);
  const aster::Vec3 dry_grass =
      mixVec(base * aster::Vec3{0.98f, 0.90f, 0.62f}, {0.34f, 0.30f, 0.18f}, 0.26f);
  const aster::Vec3 soil =
      mixVec(base * aster::Vec3{1.02f, 0.76f, 0.52f}, {0.30f, 0.23f, 0.16f}, 0.30f);
  const aster::Vec3 rock =
      mixVec(base * aster::Vec3{0.94f, 0.94f, 0.86f}, {0.44f, 0.42f, 0.36f}, 0.32f);
  aster::Vec3 albedo = mixVec(soil, dry_grass, saturate(grass_weight * 0.30f + macro * 0.18f));
  albedo = mixVec(albedo, grass, grass_weight);
  albedo = mixVec(albedo, rock, rock_weight);

  const float fiber = projectedNoise(world_position + aster::Vec3{fine * 1.7f, 0.0f, macro}, normal,
                                     fine_scale * 2.35f, pattern_id + 43.0f);
  const float pebble =
      projectedNoise(world_position, normal, fine_scale * 3.8f, pattern_id + 67.0f);
  albedo = albedo * (0.92f + fine * 0.10f + fiber * grass_weight * 0.05f);
  albedo = mixVec(albedo, albedo * 0.82f, saturate(pebble * soil_weight * 0.08f + slope * 0.05f));
  return aster::clamp(albedo, 0.0f, 4.0f);
}

aster::Vec3 patternSamplePosition(const aster::Material &material, const aster::Vec3 world_position,
                                  const aster::Vec3 normal, const double frame_seconds) {
  const float pattern_id = static_cast<float>(
      aster::materialSurfaceProfileId(aster::resolveMaterialSurfaceProfile(material)));
  const float scale = std::max(material.detail_scale, 0.001f);
  return world_position * scale + normal * 0.73f +
         aster::Vec3{pattern_id * 0.17f, pattern_id * 0.31f,
                     static_cast<float>(frame_seconds) * 0.015f};
}

float projectedFbm(const aster::Material &material, const aster::Vec3 world_position,
                   const aster::Vec3 normal, const float multiplier, const float salt) {
  const float scale = std::max(material.detail_scale, 0.001f) * multiplier;
  return terrainFbm(world_position, normal, scale, salt);
}

float ridge(const float value) {
  return 1.0f - std::abs(value * 2.0f - 1.0f);
}

aster::Vec3 structuredStoneAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                                  const aster::Vec3 normal) {
  const float mortar = std::max(material.pattern_mortar, 0.005f);
  const aster::Vec2 structural_uv = std::abs(normal.y) > 0.55f
                                        ? aster::Vec2{world_position.x, world_position.z}
                                        : aster::Vec2{world_position.x, world_position.y};
  const aster::Vec2 scaled{structural_uv.x * std::max(material.pattern_scale.x, 0.001f),
                           structural_uv.y * std::max(material.pattern_scale.y, 0.001f)};
  const aster::Vec2 cell{fractValue(scaled.x), fractValue(scaled.y)};
  const float edge = std::min(std::min(cell.x, 1.0f - cell.x), std::min(cell.y, 1.0f - cell.y));
  const float line = 1.0f - smoothstep(mortar, mortar + 0.035f, edge);
  const float broad = projectedFbm(material, world_position, normal, 0.11f, 19.0f);
  const float fine = projectedFbm(material, world_position, normal, 0.84f, 29.0f);
  aster::Vec3 block = material.base_color * (0.82f + broad * 0.22f + fine * 0.12f);
  block = mixVec(block, block * aster::Vec3{1.10f, 1.04f, 0.92f}, material.edge_wear * ridge(fine));
  return mixVec(block, material.base_color * 0.36f, line * 0.70f);
}

aster::Vec3 stratifiedRockAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                           const aster::Vec3 normal) {
  const float strata =
      0.5f + 0.5f * std::sin((world_position.y * material.pattern_scale.y +
                              world_position.z * 0.33f + world_position.x * 0.18f) *
                             1.75f);
  const float broad = projectedFbm(material, world_position, normal, 0.10f, 41.0f);
  const float fine = projectedFbm(material, world_position, normal, 0.88f, 53.0f);
  const float crack = ridge(projectedFbm(material, world_position, normal, 0.42f, 67.0f));
  aster::Vec3 damp = material.base_color * aster::Vec3{0.72f, 0.70f, 0.62f};
  aster::Vec3 mineral = material.base_color * aster::Vec3{1.24f, 1.13f, 0.92f};
  aster::Vec3 albedo = mixVec(damp, material.base_color, broad * 0.74f);
  albedo =
      mixVec(albedo, mineral,
             saturate(strata * 0.18f + ridge(fine) * 0.12f) * saturate(material.pattern_contrast));
  albedo = mixVec(albedo, albedo * 0.50f,
                  smoothstep(0.68f, 0.96f, crack) * (0.35f + material.pattern_depth * 1.8f));
  return aster::clamp(albedo * (0.82f + fine * 0.22f), 0.0f, 4.0f);
}

aster::Vec3 mineralVeinAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                           const aster::Vec3 normal) {
  const float vein_a = ridge(projectedFbm(material, world_position, normal, 0.32f, 71.0f));
  const float vein_b =
      ridge(projectedFbm(material, world_position + normal * 0.17f, normal, 0.76f, 89.0f));
  const float vein = smoothstep(0.72f, 0.96f, vein_a * vein_b + material.pattern_depth * 0.52f);
  const float sheen =
      smoothstep(0.50f, 0.95f, projectedFbm(material, world_position, normal, 1.55f, 97.0f));
  aster::Vec3 coal = material.base_color * (0.54f + sheen * 0.32f);
  const aster::Vec3 warm_vein =
      mixVec({0.22f, 0.15f, 0.075f}, material.emission_color.value + material.base_color.value,
             0.35f);
  return mixVec(coal, warm_vein, vein * 0.78f);
}

aster::Vec3 organicFiberAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                               const aster::Vec3 normal, const aster::Vec2 uv, const float salt) {
  const float flow =
      world_position.y * material.pattern_scale.y +
      (world_position.x + world_position.z * 0.38f) * material.pattern_scale.x * 0.18f;
  const float noise = projectedFbm(material, world_position, normal, 0.70f, salt);
  const float strand = 0.5f + 0.5f * std::sin(flow * 0.46f + noise * 5.3f + uv.x * kTau);
  const float strand_mask = smoothstep(0.28f, 0.92f, strand);
  aster::Vec3 dark = material.base_color * aster::Vec3{0.64f, 0.58f, 0.50f};
  aster::Vec3 light = material.base_color * aster::Vec3{1.22f, 1.14f, 0.96f};
  return mixVec(dark, light, strand_mask * (0.56f + material.pattern_contrast * 0.28f));
}

aster::Vec3 filamentWebAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                          const aster::Vec3 normal, const aster::Vec2 uv) {
  const float along = uv.x * std::max(material.pattern_scale.y, 0.001f);
  const float across = std::abs(uv.y - 0.5f) * 2.0f;
  const float fiber =
      ridge(0.5f + 0.5f * std::sin(along * 0.72f +
                                   projectedFbm(material, world_position, normal, 0.88f, 211.0f) *
                                       4.6f));
  const float core = 1.0f - smoothstep(0.18f, 1.0f, across);
  const float dust = projectedFbm(material, world_position, normal, 1.42f, 227.0f);
  const float glint = smoothstep(0.58f, 0.98f, fiber * core);
  const aster::Vec3 shadow = material.base_color * aster::Vec3{0.62f, 0.66f, 0.66f};
  const aster::Vec3 silk = material.base_color * aster::Vec3{1.22f, 1.24f, 1.16f};
  const aster::Vec3 pearl = material.base_color + aster::Vec3{0.08f, 0.09f, 0.075f};
  return aster::clamp(mixVec(mixVec(shadow, silk, core * 0.72f + dust * 0.16f), pearl, glint),
                      0.0f, 4.0f);
}

aster::Vec3 emissiveLensAlbedo(const aster::Material &material,
                               const aster::Vec3 world_position, const aster::Vec3 normal);

aster::Vec3 chitinShellAlbedo(const aster::Material &material,
                              const aster::Vec3 world_position, const aster::Vec3 normal,
                              const aster::Vec2 uv) {
  if (uv.x > 0.90f && uv.y < 0.28f) {
    return emissiveLensAlbedo(material, world_position, normal);
  }
  const float texel_x = std::floor(uv.x * std::max(material.pattern_scale.x, 1.0f));
  const float texel_y = std::floor(uv.y * std::max(material.pattern_scale.y, 1.0f));
  const float texel = fractValue(std::sin(texel_x * 12.9898f + texel_y * 78.233f) * 43758.5453f);
  const float ridge_a = ridge(projectedFbm(material, world_position, normal, 1.35f, 241.0f));
  const float band =
      0.5f + 0.5f * std::sin((texel_y * 0.92f + std::floor(texel_x * 0.25f)) * 1.73f);
  const float shell = smoothstep(0.20f, 0.92f, ridge_a * 0.56f + band * 0.30f + texel * 0.14f);
  const float center_spot =
      (1.0f - smoothstep(0.04f, 0.18f, std::abs(uv.x - 0.50f))) *
      smoothstep(0.10f, 0.54f, uv.y) * (1.0f - smoothstep(0.78f, 0.98f, uv.y));
  const float leg_band = smoothstep(0.62f, 0.96f, ridge(0.5f + 0.5f * std::sin(texel_y * 2.45f)));
  const float oil = projectedFbm(material, world_position + normal * 0.05f, normal, 0.78f, 263.0f);
  const aster::Vec3 under = material.base_color * aster::Vec3{0.44f, 0.34f, 0.30f};
  const aster::Vec3 lacquer = material.base_color * aster::Vec3{1.54f, 1.04f, 0.78f};
  const aster::Vec3 warm_mark = material.base_color + aster::Vec3{0.070f, 0.020f, 0.010f};
  const aster::Vec3 dark_band = material.base_color * aster::Vec3{0.22f, 0.18f, 0.18f};
  const aster::Vec3 cool_sheen = material.base_color + aster::Vec3{0.042f, 0.050f, 0.060f};
  aster::Vec3 albedo = mixVec(under, lacquer, shell);
  albedo = mixVec(albedo, warm_mark, center_spot * 0.58f);
  albedo = mixVec(albedo, dark_band, leg_band * (0.18f + texel * 0.12f));
  albedo = mixVec(albedo, cool_sheen, oil * 0.26f);
  return aster::clamp(albedo, 0.0f, 4.0f);
}

aster::Vec3 emissiveLensAlbedo(const aster::Material &material,
                               const aster::Vec3 world_position, const aster::Vec3 normal) {
  const float wet = smoothstep(0.45f, 0.96f,
                               projectedFbm(material, world_position, normal, 1.9f, 283.0f));
  return aster::clamp(material.base_color.value * (0.62f + wet * 0.44f) +
                          material.emission_color.value * (0.28f + wet * 0.34f),
                      0.0f, 4.0f);
}

aster::Vec3 foliageAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                          const aster::Vec3 normal, const aster::Vec2 uv) {
  const float blade_height = saturate(uv.y);
  const float root_weight = 1.0f - smoothstep(0.04f, 0.36f, blade_height);
  const float tip_weight = smoothstep(0.48f, 1.0f, blade_height);
  const float central_vein = 1.0f - smoothstep(0.025f, 0.22f, std::abs(uv.x - 0.5f));
  const float strand =
      0.5f + 0.5f * std::sin((uv.y * material.pattern_scale.y + uv.x * 2.0f) * kTau);
  const float mottling = projectedFbm(material, world_position, normal, 0.64f, 109.0f);
  const float fiber = ridge(projectedFbm(material, world_position, normal, 1.22f, 113.0f));
  const aster::Vec3 root = material.base_color * aster::Vec3{0.50f, 0.66f, 0.38f};
  const aster::Vec3 mid = material.base_color * aster::Vec3{0.82f, 1.08f, 0.58f};
  const aster::Vec3 tip = material.base_color * aster::Vec3{1.18f, 1.30f, 0.72f};
  aster::Vec3 blade = mixVec(root, mid, smoothstep(0.02f, 0.72f, blade_height));
  blade = mixVec(blade, tip, tip_weight * (0.42f + mottling * 0.28f));
  blade = blade * (0.88f + mottling * 0.18f + fiber * 0.08f + strand * 0.05f);
  blade = mixVec(blade, blade * aster::Vec3{1.26f, 1.34f, 0.88f}, central_vein * 0.18f);
  return aster::clamp(mixVec(blade, blade * 0.72f, root_weight * 0.34f), 0.0f, 4.0f);
}

aster::Vec3 liquidAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                         const aster::Vec2 uv, const double frame_seconds) {
  const float time = static_cast<float>(frame_seconds);
  const float wave_a =
      0.5f +
      0.5f * std::sin((uv.x * material.pattern_scale.x + uv.y * material.pattern_scale.y) * 5.4f +
                      time * 1.35f);
  const float wave_b =
      valueNoise({world_position.x * 0.58f + time * 0.22f, world_position.y * 0.12f,
                  world_position.z * 0.72f - time * 0.17f});
  const aster::Vec3 deep = material.base_color * aster::Vec3{0.55f, 0.88f, 0.96f};
  const aster::Vec3 glint = material.base_color + aster::Vec3{0.08f, 0.20f, 0.22f};
  return mixVec(deep, glint, smoothstep(0.45f, 0.94f, wave_a * 0.64f + wave_b * 0.36f));
}

aster::Vec3 amberAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                        const aster::Vec3 normal) {
  const float streak = ridge(projectedFbm(material, world_position, normal, 0.44f, 127.0f));
  const float cloud = projectedFbm(material, world_position, normal, 1.10f, 131.0f);
  const aster::Vec3 honey = material.base_color * aster::Vec3{1.30f, 0.96f, 0.56f};
  const aster::Vec3 smoke = material.base_color * aster::Vec3{0.62f, 0.42f, 0.28f};
  return mixVec(smoke, honey, smoothstep(0.25f, 0.92f, cloud)) +
         material.emission_color * (0.08f + smoothstep(0.70f, 0.98f, streak) * 0.16f);
}

aster::Vec3 corrodedMetalAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                                const aster::Vec3 normal) {
  const float detail = std::max(material.detail_scale, 0.001f);
  const float broad = projectedFbm(material, world_position, normal, 0.16f, 307.0f);
  const float fine = projectedFbm(material, world_position + normal * 0.05f, normal, 0.74f, 331.0f);
  const float pitting = ridge(projectedFbm(material, world_position, normal, 1.38f, 359.0f));
  const float upward = smoothstep(0.14f, 0.80f, normal.y);
  const float oxide =
      smoothstep(0.42f, 0.95f,
                 broad * 0.48f + pitting * 0.34f + upward * material.pattern_depth * 0.42f);
  const aster::Vec3 cool_steel =
      material.base_color * aster::Vec3{0.82f, 0.88f, 0.92f} * (0.60f + fine * 0.22f);
  const aster::Vec3 exposed =
      material.base_color * aster::Vec3{1.18f, 1.16f, 1.06f} * (0.66f + pitting * 0.16f);
  const aster::Vec3 orange_rust{0.42f, 0.15f, 0.045f};
  const aster::Vec3 black_rust{0.085f, 0.060f, 0.043f};
  aster::Vec3 color = mixVec(cool_steel, mixVec(black_rust, orange_rust, fine), oxide);
  color = mixVec(color, exposed, material.edge_wear * smoothstep(0.72f, 0.98f, pitting));
  return aster::clamp(color * (0.98f + detail * 0.002f), 0.0f, 4.0f);
}

aster::Vec3 weldBeadAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                           const aster::Vec3 normal, const aster::Vec2 uv) {
  const float bead_ripple =
      0.5f + 0.5f * std::sin(uv.y * std::max(material.pattern_scale.y, 0.001f) * kTau +
                              projectedFbm(material, world_position, normal, 1.0f, 401.0f) * 3.8f);
  const float heat_band = smoothstep(0.05f, 0.58f, ridge(uv.x));
  const aster::Vec3 base = material.base_color * (0.76f + bead_ripple * 0.28f);
  const aster::Vec3 straw{0.72f, 0.38f, 0.12f};
  const aster::Vec3 blue_heat{0.11f, 0.16f, 0.30f};
  aster::Vec3 color = mixVec(base, straw, heat_band * 0.24f);
  color = mixVec(color, blue_heat, heat_band * (1.0f - bead_ripple) * 0.16f);
  return aster::clamp(color, 0.0f, 4.0f);
}

aster::Vec3 pbrNeutralTonemap(aster::Vec3 color) {
  constexpr float start_compression = 0.76f;
  constexpr float desaturation = 0.15f;

  const float x = std::min(color.x, std::min(color.y, color.z));
  const float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
  color = color - aster::Vec3{offset, offset, offset};

  const float peak = std::max(color.x, std::max(color.y, color.z));
  if (peak < start_compression) {
    return color;
  }

  constexpr float d = 1.0f - start_compression;
  const float new_peak = 1.0f - d * d / (peak + d - start_compression);
  color = color * (new_peak / std::max(peak, 0.0001f));
  const float g = 1.0f - 1.0f / (desaturation * (peak - new_peak) + 1.0f);
  return mixVec(color, {new_peak, new_peak, new_peak}, g);
}

int toneMapperUniform(const aster::RendererSettings &settings) {
  if (settings.use_aces_tonemap) {
    return 0;
  }
  switch (settings.pipeline.tone_mapper) {
  case aster::ToneMapper::FilmicAces:
    return 0;
  case aster::ToneMapper::PbrNeutral:
    return 1;
  case aster::ToneMapper::Reinhard:
    return 2;
  }
  return 1;
}

aster::Vec3 toneMapColor(const aster::Vec3 color, const int tone_mapper) {
  if (tone_mapper == 0) {
    return aster::aces_tonemap(color);
  }
  if (tone_mapper == 2) {
    return aster::reinhard_tonemap(color);
  }
  return pbrNeutralTonemap(color);
}

float luminance(const aster::Vec3 color) {
  return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
}

aster::Vec3 applyVisualGrade(aster::Vec3 color, const aster::AtmosphereSettings &atmosphere) {
  const float pre_grade_luma = luminance(color);
  color = mixVec(color, color * atmosphere.shadow_tint,
                 std::clamp(atmosphere.shadow_tint_strength, 0.0f, 1.0f) *
                     (1.0f - smoothstep(0.05f, 0.48f, pre_grade_luma)));
  color = mixVec(color, color * atmosphere.highlight_tint,
                 std::clamp(atmosphere.highlight_tint_strength, 0.0f, 1.0f) *
                     smoothstep(0.52f, 1.45f, pre_grade_luma));
  const float post_grade_luma = luminance(color);
  color = mixVec({post_grade_luma, post_grade_luma, post_grade_luma}, color,
                 std::clamp(atmosphere.saturation, 0.0f, 2.0f));
  color = (color - aster::Vec3{0.5f, 0.5f, 0.5f}) * std::max(atmosphere.contrast, 0.0f) +
          aster::Vec3{0.5f, 0.5f, 0.5f};
  return color;
}

aster::Vec3 applyProceduralLayer(const aster::Material &material, const aster::Vec3 world_position,
                                 const aster::Vec3 normal, const aster::Vec3 color) {
  const float macro_strength = std::max(material.procedural.macro_variation, 0.0f);
  const float wetness = std::clamp(material.procedural.wetness, 0.0f, 1.0f);
  const float height_shading = std::max(material.procedural.height_shading, 0.0f);
  if (macro_strength <= 0.0001f && wetness <= 0.0001f && height_shading <= 0.0001f) {
    return color;
  }

  const float detail = std::max(material.detail_scale, 0.001f);
  const float broad =
      valueNoise(world_position * (0.10f + detail * 0.014f) + aster::Vec3{23.7f, 0.0f, -11.3f});
  const float fine = valueNoise(world_position * (0.48f + detail * 0.036f) + normal * 0.31f);
  const float surface_lift = smoothstep(0.18f, 0.86f, normal.y);
  const float variation = 1.0f + (broad - 0.5f) * macro_strength * 0.28f +
                          (fine - 0.5f) * macro_strength * 0.12f +
                          surface_lift * height_shading * 0.045f;
  const aster::Vec3 damp_color =
      mixVec(color * 0.70f, color * aster::Vec3{0.80f, 0.86f, 0.92f}, surface_lift * 0.45f);
  return aster::clamp(mixVec(color * variation, damp_color, wetness * (0.18f + fine * 0.18f)), 0.0f,
                      4.0f);
}

aster::Vec3 proceduralSurfaceNormal(const aster::Material &material,
                                    const aster::Vec3 world_position, const aster::Vec3 normal) {
  const aster::MaterialSurfaceProfile profile = aster::resolveMaterialSurfaceProfile(material);
  if (profile == aster::MaterialSurfaceProfile::ContactShadow ||
      profile == aster::MaterialSurfaceProfile::Liquid) {
    return normal;
  }

  const float terrain_gain = isTerrainProfile(profile) ? 0.44f : 0.28f;
  const float inferred_strength = material.detail_strength * terrain_gain;
  const float explicit_strength = std::max(material.procedural.micro_normal_strength, 0.0f);
  const float strength = std::clamp(std::max(inferred_strength, explicit_strength), 0.0f, 1.20f);
  if (strength <= 0.0001f) {
    return normal;
  }

  const float detail = std::max(material.detail_scale, 0.001f);
  const float macro_gain = 1.0f + std::max(material.procedural.macro_variation, 0.0f) * 0.34f;
  const float profile_id = static_cast<float>(aster::materialSurfaceProfileId(profile));
  const aster::Vec3 p =
      world_position * (0.55f * detail * macro_gain) +
      aster::Vec3{profile_id * 0.23f, 0.0f, profile_id * 0.41f};
  const aster::Vec3 bump{valueNoise(p + aster::Vec3{11.1f, 2.3f, 0.7f}) - 0.5f,
                         (valueNoise(p + aster::Vec3{5.7f, 13.1f, 3.2f}) - 0.5f) * 0.35f,
                         valueNoise(p + aster::Vec3{2.9f, 7.4f, 17.6f}) - 0.5f};
  const aster::Vec3 adjusted = aster::normalize(normal + bump * strength);
  return aster::length(adjusted) > 0.0001f ? adjusted : normal;
}

aster::Vec3 materialAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                           const aster::Vec3 normal, const aster::Vec2 uv,
                           const double frame_seconds) {
  const aster::MaterialSurfaceProfile profile = aster::resolveMaterialSurfaceProfile(material);
  if (profile == aster::MaterialSurfaceProfile::ContactShadow) {
    return {0.0f, 0.0f, 0.0f};
  }

  const float pattern_id = static_cast<float>(aster::materialSurfaceProfileId(profile));
  const aster::Vec3 sample_position =
      patternSamplePosition(material, world_position, normal, frame_seconds);
  const float broad = valueNoise(sample_position * 0.37f);
  const float fine = valueNoise(sample_position * 1.91f);
  const float procedural_weight =
      profile == aster::MaterialSurfaceProfile::Plain
          ? saturate(material.detail_strength + material.procedural.macro_variation * 0.22f)
          : saturate(0.16f + material.detail_strength + material.pattern_contrast * 0.55f +
                     std::abs(material.pattern_depth) * 1.35f +
                     material.procedural.macro_variation * 0.24f);
  if (isTerrainProfile(profile)) {
    return applyProceduralLayer(material, world_position, normal,
                                terrainLayeredAlbedo(material, world_position, normal));
  }
  switch (profile) {
  case aster::MaterialSurfaceProfile::Masonry:
    return applyProceduralLayer(
        material, world_position, normal,
        aster::clamp(structuredStoneAlbedo(material, world_position, normal), 0.0f, 4.0f));
  case aster::MaterialSurfaceProfile::StratifiedRock:
    return applyProceduralLayer(material, world_position, normal,
                                stratifiedRockAlbedo(material, world_position, normal));
  case aster::MaterialSurfaceProfile::MineralVein:
    return applyProceduralLayer(material, world_position, normal,
                                mineralVeinAlbedo(material, world_position, normal));
  case aster::MaterialSurfaceProfile::Liquid:
    return applyProceduralLayer(material, world_position, normal,
                                liquidAlbedo(material, world_position, uv, frame_seconds));
  case aster::MaterialSurfaceProfile::OrganicFiber: {
    const aster::Vec3 fiber =
        organicFiberAlbedo(material, world_position, normal, uv, pattern_id + 151.0f);
    const aster::Vec3 twig_tint = material.surface_pattern == aster::SurfacePattern::TwigNest
                                      ? aster::Vec3{0.90f, 0.78f, 0.58f}
                                      : aster::Vec3{1.0f, 1.0f, 1.0f};
    return aster::clamp(fiber * twig_tint, 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::FilamentWeb:
    return applyProceduralLayer(material, world_position, normal,
                                filamentWebAlbedo(material, world_position, normal, uv));
  case aster::MaterialSurfaceProfile::ChitinShell:
    return applyProceduralLayer(material, world_position, normal,
                                chitinShellAlbedo(material, world_position, normal, uv));
  case aster::MaterialSurfaceProfile::EmissiveLens:
    return emissiveLensAlbedo(material, world_position, normal);
  case aster::MaterialSurfaceProfile::Foliage:
    return applyProceduralLayer(
        material, world_position, normal,
        aster::clamp(foliageAlbedo(material, world_position, normal, uv), 0.0f, 4.0f));
  case aster::MaterialSurfaceProfile::Resin:
    return aster::clamp(amberAlbedo(material, world_position, normal), 0.0f, 4.0f);
  case aster::MaterialSurfaceProfile::PaintedWood: {
    const float grain = ridge(projectedFbm(material, world_position, normal, 0.52f, 167.0f));
    const float rings =
        0.5f + 0.5f * std::sin((world_position.y * material.pattern_scale.y +
                                world_position.x * 0.28f + world_position.z * 0.19f) *
                                   2.2f +
                               grain * 3.4f);
    const aster::Vec3 dark = material.base_color * aster::Vec3{0.62f, 0.48f, 0.34f};
    const aster::Vec3 warm = material.base_color * aster::Vec3{1.18f, 0.96f, 0.68f};
    return aster::clamp(mixVec(dark, warm, smoothstep(0.18f, 0.92f, rings)), 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::Feather: {
    const float central = 1.0f - smoothstep(0.025f, 0.18f, std::abs(uv.x - 0.5f));
    const float barb =
        0.5f + 0.5f * std::sin((uv.y * material.pattern_scale.y + uv.x * 3.0f) * kTau);
    aster::Vec3 feather = mixVec(material.base_color * 0.72f, material.base_color * 1.22f,
                                 smoothstep(0.26f, 0.88f, barb));
    return aster::clamp(mixVec(feather, feather * 1.35f, central * 0.34f), 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::Scales: {
    const aster::Vec2 scaled{uv.x * std::max(material.pattern_scale.x, 0.001f),
                             uv.y * std::max(material.pattern_scale.y, 0.001f)};
    const aster::Vec2 cell{fractValue(scaled.x), fractValue(scaled.y)};
    const float shell = smoothstep(0.18f, 0.50f, 1.0f - length(cell - aster::Vec2{0.5f, 0.5f}));
    const float hue_shift = projectedFbm(material, world_position, normal, 1.10f, 181.0f);
    aster::Vec3 scale_color =
        mixVec(material.base_color * aster::Vec3{0.66f, 0.82f, 0.76f},
               material.base_color * aster::Vec3{1.22f, 1.02f, 0.68f}, hue_shift);
    return aster::clamp(mixVec(material.base_color * 0.62f, scale_color, shell), 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::CorrodedMetal:
    return applyProceduralLayer(material, world_position, normal,
                                corrodedMetalAlbedo(material, world_position, normal));
  case aster::MaterialSurfaceProfile::WeldBead:
    return applyProceduralLayer(material, world_position, normal,
                                weldBeadAlbedo(material, world_position, normal, uv));
  case aster::MaterialSurfaceProfile::Auto:
  case aster::MaterialSurfaceProfile::Plain:
  case aster::MaterialSurfaceProfile::TerrainLayer:
  case aster::MaterialSurfaceProfile::ContactShadow:
    break;
  }
  const float uv_wave = std::sin((uv.x * std::max(material.pattern_scale.x, 0.001f) +
                                  uv.y * std::max(material.pattern_scale.y, 0.001f)) *
                                 6.2831853f);
  const aster::Vec3 macro_position =
      world_position * 0.22f + aster::Vec3{pattern_id * 0.41f, 0.0f, pattern_id * 0.29f};
  const float macro_patch = valueNoise(macro_position);
  const float macro_stain = valueNoise(macro_position * 0.43f + aster::Vec3{2.7f, 0.0f, -1.9f});
  const float upward_surface = smoothstep(0.50f, 0.92f, normal.y);
  const float macro_weight =
      saturate((material.detail_strength * 0.20f + material.pattern_contrast * 0.12f) *
               (0.45f + upward_surface * 0.55f));
  const float variation = 0.82f + broad * 0.24f + fine * 0.10f + uv_wave * 0.04f +
                          (macro_patch * 0.18f - macro_stain * 0.08f) * macro_weight;
  return applyProceduralLayer(
      material, world_position, normal,
      aster::clamp(material.base_color.value * std::lerp(1.0f, variation, procedural_weight), 0.0f,
                   4.0f));
}

const aster::RuntimeTexture *textureForRole(const aster::RuntimeTextureSet *textures,
                                            const std::string_view role) {
  return textures == nullptr ? nullptr : textures->find(role);
}

bool hasAuthoredRuntimeTexture(const aster::RuntimeTexture *texture) {
  return texture != nullptr && texture->valid && !texture->fallback;
}

aster::Vec3 applyRuntimeNormalMap(const aster::RuntimeTextureSet *textures, aster::Vec3 normal,
                                  const aster::Vec4 tangent_handedness,
                                  const aster::Vec2 uv) {
  const aster::RuntimeTexture *normal_texture = textureForRole(textures, "normal");
  if (!hasAuthoredRuntimeTexture(normal_texture)) {
    return normal;
  }
  const aster::Vec4 encoded = aster::sampleRuntimeTexture(*normal_texture, uv);
  aster::Vec3 tangent_space{encoded.x * 2.0f - 1.0f, encoded.y * 2.0f - 1.0f,
                            encoded.z * 2.0f - 1.0f};
  if (normal_texture->normal_convention == aster::TextureNormalConvention::DirectXInvertedY) {
    tangent_space.y = -tangent_space.y;
  }
  normal = aster::normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  aster::Vec3 tangent{tangent_handedness.x, tangent_handedness.y, tangent_handedness.z};
  tangent = tangent - normal * aster::dot(normal, tangent);
  if (aster::length(tangent) <= 0.0001f) {
    tangent = std::abs(normal.y) < 0.92f ? aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, normal))
                                         : aster::normalize(aster::cross({1.0f, 0.0f, 0.0f}, normal));
  } else {
    tangent = aster::normalize(tangent);
  }
  const float handedness = tangent_handedness.w < 0.0f ? -1.0f : 1.0f;
  const aster::Vec3 bitangent = aster::normalize(aster::cross(normal, tangent)) * handedness;
  return aster::normalizeOr(tangent * tangent_space.x + bitangent * tangent_space.y +
                                normal * std::max(tangent_space.z, 0.0f),
                            normal);
}

struct RuntimeMaterialSample {
  aster::Vec3 albedo{};
  float roughness = 0.55f;
  float metallic = 0.0f;
  float ambient_occlusion = 1.0f;
  float opacity = 1.0f;
  float wetness = 0.0f;
  aster::Vec3 emissive{};
};

RuntimeMaterialSample sampleRuntimeMaterial(const aster::Material &material,
                                            const aster::RuntimeTextureSet *textures,
                                            const aster::Vec3 world_position,
                                            const aster::Vec3 normal, const aster::Vec2 uv,
                                            const double frame_seconds) {
  RuntimeMaterialSample sample;
  sample.albedo = materialAlbedo(material, world_position, normal, uv, frame_seconds);
  sample.roughness = std::clamp(material.roughness, 0.045f, 1.0f);
  sample.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
  sample.ambient_occlusion = std::clamp(material.ambient_occlusion, 0.0f, 1.0f);
  sample.opacity = std::clamp(material.opacity, 0.0f, 1.0f);
  sample.wetness = std::clamp(material.procedural.wetness, 0.0f, 1.0f);
  sample.emissive = material.emission_color * material.emission_strength;

  if (const aster::RuntimeTexture *albedo_texture = textureForRole(textures, "albedo")) {
    sample.albedo = sample.albedo * aster::sampleRuntimeTextureRgb(*albedo_texture, uv);
  }
  if (const aster::RuntimeTexture *orm_texture = textureForRole(textures, "orm");
      hasAuthoredRuntimeTexture(orm_texture)) {
    const aster::Vec4 orm = aster::sampleRuntimeTexture(*orm_texture, uv);
    sample.ambient_occlusion *= std::clamp(orm.x, 0.0f, 1.0f);
    sample.roughness = std::clamp(orm.y, 0.045f, 1.0f);
    sample.metallic = std::clamp(orm.z, 0.0f, 1.0f);
  }
  if (const aster::RuntimeTexture *roughness_texture = textureForRole(textures, "roughness");
      hasAuthoredRuntimeTexture(roughness_texture)) {
    sample.roughness =
        std::clamp(aster::sampleRuntimeTextureChannel(*roughness_texture, uv, 0u), 0.045f, 1.0f);
  }
  if (const aster::RuntimeTexture *metallic_texture = textureForRole(textures, "metallic");
      hasAuthoredRuntimeTexture(metallic_texture)) {
    sample.metallic =
        std::clamp(aster::sampleRuntimeTextureChannel(*metallic_texture, uv, 0u), 0.0f, 1.0f);
  }
  if (const aster::RuntimeTexture *ao_texture = textureForRole(textures, "ao");
      hasAuthoredRuntimeTexture(ao_texture)) {
    sample.ambient_occlusion *=
        std::clamp(aster::sampleRuntimeTextureChannel(*ao_texture, uv, 0u), 0.0f, 1.0f);
  }
  if (const aster::RuntimeTexture *height_texture = textureForRole(textures, "height");
      hasAuthoredRuntimeTexture(height_texture)) {
    const float height = aster::sampleRuntimeTextureChannel(*height_texture, uv, 0u);
    sample.albedo *= 0.88f + height * 0.22f;
  }
  if (const aster::RuntimeTexture *wetness_texture = textureForRole(textures, "wetness");
      hasAuthoredRuntimeTexture(wetness_texture)) {
    sample.wetness =
        std::max(sample.wetness, aster::sampleRuntimeTextureChannel(*wetness_texture, uv, 0u));
  }
  if (sample.wetness > 0.0001f) {
    const float wet = std::clamp(sample.wetness, 0.0f, 1.0f);
    sample.roughness = std::lerp(sample.roughness, std::min(sample.roughness, 0.18f), wet * 0.72f);
    sample.albedo = mixVec(sample.albedo, sample.albedo * 0.54f + aster::Vec3{0.015f, 0.022f, 0.030f},
                           wet * 0.38f);
  }
  if (const aster::RuntimeTexture *emissive_texture = textureForRole(textures, "emissive");
      hasAuthoredRuntimeTexture(emissive_texture)) {
    sample.emissive += aster::sampleRuntimeTextureRgb(*emissive_texture, uv) *
                       std::max(material.emission_strength, 1.0f);
  }
  if (const aster::RuntimeTexture *opacity_texture = textureForRole(textures, "opacity");
      hasAuthoredRuntimeTexture(opacity_texture)) {
    sample.opacity *=
        std::clamp(aster::sampleRuntimeTextureChannel(*opacity_texture, uv, 0u), 0.0f, 1.0f);
  }
  sample.albedo = aster::clamp(sample.albedo, 0.0f, 4.0f);
  return sample;
}

aster::Vec3 shadeVertex(const aster::Material &material, const aster::Vec3 world_position,
                        aster::Vec3 normal, const aster::Vec4 tangent_handedness,
                        const aster::Vec2 uv, const float vertex_ao,
                        const aster::Vec3 camera_position, const aster::RendererSettings &settings,
                        const double frame_seconds, const aster::RuntimeTextureSet *textures,
                        const SoftwareFrameResources *frame_resources) {
  normal = aster::normalize(normal);
  if (aster::length(normal) <= 0.0001f) {
    normal = {0.0f, 1.0f, 0.0f};
  }
  const aster::Vec3 material_sample_position =
      snapProceduralSamplePosition(world_position, settings.style);
  if (settings.procedural_surface_normals) {
    normal = proceduralSurfaceNormal(material, material_sample_position, normal);
  }
  normal = applyRuntimeNormalMap(textures, normal, tangent_handedness, uv);

  const RuntimeMaterialSample material_sample =
      sampleRuntimeMaterial(material, textures, material_sample_position, normal, uv, frame_seconds);
  const aster::Vec3 albedo = material_sample.albedo;
  const float sky_factor = saturate(normal.y * 0.5f + 0.5f);
  const aster::Vec3 ambient_color =
      mixVec(settings.ground_ambient_color, settings.sky_ambient_color, sky_factor);
  const float geometry_ao =
      std::clamp(material_sample.ambient_occlusion * std::clamp(vertex_ao, 0.0f, 1.0f), 0.0f,
                 1.0f);
  const float ambient_level =
      std::max(settings.ambient_strength * geometry_ao, settings.ambient_floor);
  aster::Vec3 color =
      ambient_color * albedo * ambient_level +
      albedo * std::max(settings.indirect_albedo_floor, 0.0f);

  const aster::Vec3 view = aster::normalize(camera_position - world_position);
  const float metallic = material_sample.metallic;
  const float perceptual_roughness = material_sample.roughness;
  const float n_dot_v = std::max(aster::dot(normal, view), 0.001f);
  const aster::Vec3 f0 = mixVec({0.04f, 0.04f, 0.04f}, albedo, metallic);
  const float alpha = perceptual_roughness * perceptual_roughness;
  const float alpha2 = std::max(alpha * alpha, 0.0005f);
  const float k = ((perceptual_roughness + 1.0f) * (perceptual_roughness + 1.0f)) * 0.125f;

  if (settings.reflections.enabled && settings.reflections.fallback_intensity > 0.0f) {
    const aster::Vec3 incident = -view;
    const aster::Vec3 reflection =
        aster::normalizeOr(incident - normal * (2.0f * aster::dot(incident, normal)), normal);
    const aster::Vec3 env_color =
        softwareReflectionEnvironment(frame_resources, world_position, reflection, settings);
    const aster::Vec3 fresnel =
        f0 + (aster::Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - n_dot_v, 5.0f);
    const float roughness_visibility = std::pow(1.0f - perceptual_roughness * 0.72f, 2.0f);
    const float material_visibility = 0.24f + metallic * 0.76f;
    color += env_color * fresnel *
             (roughness_visibility * material_visibility *
              std::clamp(settings.reflections.fallback_intensity, 0.0f, 2.0f));
  }

  const auto add_light = [&](const aster::Vec3 light_dir, const aster::Vec3 radiance) {
    const aster::Vec3 half_vector = aster::normalize(light_dir + view);
    const float n_dot_l = std::max(aster::dot(normal, light_dir), 0.0f);
    const float n_dot_h = std::max(aster::dot(normal, half_vector), 0.0f);
    const float h_dot_v = std::max(aster::dot(half_vector, view), 0.0f);
    const float distribution_denominator = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;
    const float distribution =
        alpha2 /
        std::max(3.14159265f * distribution_denominator * distribution_denominator, 0.001f);
    const float geometry_l = n_dot_l / std::max(n_dot_l * (1.0f - k) + k, 0.001f);
    const float geometry_v = n_dot_v / std::max(n_dot_v * (1.0f - k) + k, 0.001f);
    const aster::Vec3 fresnel =
        f0 + (aster::Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - h_dot_v, 5.0f);
    const aster::Vec3 specular = fresnel * (distribution * geometry_l * geometry_v);
    const aster::Vec3 diffuse = albedo * ((1.0f - metallic) * 0.82f);
    color = color + (diffuse + specular) * (radiance * n_dot_l);
  };

  if (settings.sun_light.enabled && settings.sun_light.intensity > 0.0f) {
    const aster::Vec3 sun_dir = aster::normalize(settings.sun_light.direction_to_light);
    if (aster::length(sun_dir) > 0.0001f) {
      const float shadow_visibility =
          material.receives_shadows
              ? sampleSoftwareShadowVisibility(frame_resources, world_position, normal,
                                               settings.shadows)
              : 1.0f;
      add_light(sun_dir,
                settings.sun_light.color * settings.sun_light.intensity * shadow_visibility);
    }
  }

  const std::vector<aster::Light> selected_lights =
      aster::selectRenderLights(settings.light_rig, world_position, settings.light_policy);
  for (const aster::Light &light : selected_lights) {
    const aster::Vec3 light_vector = light.position - world_position;
    const float distance_sq = std::max(aster::dot(light_vector, light_vector), 0.0001f);
    const aster::Vec3 light_dir = aster::normalize(light_vector);
    const float softened_distance =
        std::max(distance_sq, light.source_radius * light.source_radius + 0.0001f);
    const aster::Vec3 radiance = light.color * (light.intensity / softened_distance);
    add_light(light_dir, radiance);
  }

  color = mixVec(color,
                 albedo * std::max(settings.ambient_strength + settings.ambient_floor + 0.14f,
                                   0.18f),
                 std::clamp(settings.style.unlit_mix, 0.0f, 1.0f));

  if (settings.grounding.enabled && settings.grounding.surface_occlusion_strength > 0.0f) {
    const float height = std::max(settings.grounding.surface_occlusion_height, 0.001f);
    const float above_reference = std::max(world_position.y - settings.grounding.reference_y, 0.0f);
    const float proximity = 1.0f - smoothstep(0.0f, height, above_reference);
    const float vertical_receiver = (1.0f - smoothstep(0.48f, 0.92f, normal.y)) * 0.35f;
    const float raw = 1.0f - std::clamp(proximity * vertical_receiver *
                                            settings.grounding.surface_occlusion_strength,
                                        0.0f, 0.70f);
    color = color * mixVec({1.0f, 1.0f, 1.0f},
                           {std::max(raw, settings.grounding.surface_occlusion_min),
                            std::max(raw, settings.grounding.surface_occlusion_min),
                            std::max(raw, settings.grounding.surface_occlusion_min)},
                           settings.grounding.surface_occlusion_mix);
  }

  if (settings.atmosphere.enabled) {
    const float fog = evaluateFogFactor(settings.atmosphere,
                                        aster::length(camera_position - world_position));
    color = mixVec(color, settings.atmosphere.fog_color, fog);
  }

  color = applyVisualGrade(color, settings.atmosphere);
  color = color + material_sample.emissive * std::max(settings.style.emissive_gain, 0.0f);
  color = color * settings.exposure;
  color = toneMapColor(color, toneMapperUniform(settings));
  color = applyRenderStylePost(color, settings.style);
  return aster::gamma_encode(aster::clamp(color, 0.0f, 1.0f));
}

aster::FrameColor shadedFrameColor(const aster::Material &material,
                                   const aster::Vec3 world_position, const aster::Vec3 normal,
                                   const aster::Vec4 tangent_handedness, const aster::Vec2 uv,
                                   const float vertex_ao,
                                   const aster::Vec3 camera_position,
                                   const aster::RendererSettings &settings,
                                   const double frame_seconds, const float opacity,
                                   const aster::RuntimeTextureSet *textures,
                                   const SoftwareFrameResources *frame_resources) {
  const RuntimeMaterialSample material_sample =
      sampleRuntimeMaterial(material, textures, world_position, normal, uv, frame_seconds);
  return aster::frameColor(shadeVertex(material, world_position, normal, tangent_handedness, uv,
                                       vertex_ao, camera_position, settings, frame_seconds,
                                       textures, frame_resources),
                           opacity * material_sample.opacity);
}

float byteLuma(const std::vector<std::uint8_t> &pixels, const std::size_t base) {
  const float r = static_cast<float>(pixels[base + 0u]) / 255.0f;
  const float g = static_cast<float>(pixels[base + 1u]) / 255.0f;
  const float b = static_cast<float>(pixels[base + 2u]) / 255.0f;
  return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

void applySoftwareBloom(std::vector<std::uint8_t> &pixels, const int width, const int height,
                        const aster::RendererPostSettings &post) {
  if (!post.bloom || post.bloom_intensity <= 0.0f || width <= 2 || height <= 2) {
    return;
  }
  const float threshold = std::clamp(post.bloom_threshold / 4.0f, 0.55f, 0.98f);
  std::vector<float> bright(static_cast<std::size_t>(width) * height * 3u, 0.0f);
  std::vector<float> blur(bright.size(), 0.0f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y * width + x);
      const std::size_t base = pixel * 4u;
      const float luma = byteLuma(pixels, base);
      const float weight = std::clamp((luma - threshold) / std::max(1.0f - threshold, 0.001f),
                                      0.0f, 1.0f);
      bright[pixel * 3u + 0u] = static_cast<float>(pixels[base + 0u]) / 255.0f * weight;
      bright[pixel * 3u + 1u] = static_cast<float>(pixels[base + 1u]) / 255.0f * weight;
      bright[pixel * 3u + 2u] = static_cast<float>(pixels[base + 2u]) / 255.0f * weight;
    }
  }
  for (int y = 1; y + 1 < height; ++y) {
    for (int x = 1; x + 1 < width; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y * width + x);
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          const float kernel = (ox == 0 && oy == 0) ? 0.25f : ((ox == 0 || oy == 0) ? 0.125f : 0.0625f);
          const std::size_t source = static_cast<std::size_t>((y + oy) * width + (x + ox)) * 3u;
          blur[pixel * 3u + 0u] += bright[source + 0u] * kernel;
          blur[pixel * 3u + 1u] += bright[source + 1u] * kernel;
          blur[pixel * 3u + 2u] += bright[source + 2u] * kernel;
        }
      }
    }
  }
  const float intensity = std::clamp(post.bloom_intensity, 0.0f, 2.0f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y * width + x);
      const std::size_t base = pixel * 4u;
      for (std::size_t channel = 0u; channel < 3u; ++channel) {
        const float value = static_cast<float>(pixels[base + channel]) / 255.0f +
                            blur[pixel * 3u + channel] * intensity;
        pixels[base + channel] = static_cast<std::uint8_t>(
            std::clamp(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0l, 255l));
      }
    }
  }
}

void applySoftwareFxaa(std::vector<std::uint8_t> &pixels, const int width, const int height,
                       const aster::RendererPostSettings &post) {
  if (!post.fxaa || width <= 2 || height <= 2) {
    return;
  }
  const std::vector<std::uint8_t> source = pixels;
  for (int y = 1; y + 1 < height; ++y) {
    for (int x = 1; x + 1 < width; ++x) {
      const std::size_t center = static_cast<std::size_t>(y * width + x) * 4u;
      const float luma_center = byteLuma(source, center);
      const float luma_left = byteLuma(source, static_cast<std::size_t>(y * width + x - 1) * 4u);
      const float luma_right = byteLuma(source, static_cast<std::size_t>(y * width + x + 1) * 4u);
      const float luma_up = byteLuma(source, static_cast<std::size_t>((y - 1) * width + x) * 4u);
      const float luma_down = byteLuma(source, static_cast<std::size_t>((y + 1) * width + x) * 4u);
      const float contrast = std::max({luma_center, luma_left, luma_right, luma_up, luma_down}) -
                             std::min({luma_center, luma_left, luma_right, luma_up, luma_down});
      if (contrast < 0.075f) {
        continue;
      }
      for (std::size_t channel = 0u; channel < 3u; ++channel) {
        const float center_value = static_cast<float>(source[center + channel]);
        const float average =
            (static_cast<float>(source[(static_cast<std::size_t>(y * width + x - 1) * 4u) + channel]) +
             static_cast<float>(source[(static_cast<std::size_t>(y * width + x + 1) * 4u) + channel]) +
             static_cast<float>(source[(static_cast<std::size_t>((y - 1) * width + x) * 4u) + channel]) +
             static_cast<float>(source[(static_cast<std::size_t>((y + 1) * width + x) * 4u) + channel])) *
            0.25f;
        pixels[center + channel] =
            static_cast<std::uint8_t>(std::clamp(std::lround(center_value * 0.55f + average * 0.45f),
                                                 0l, 255l));
      }
    }
  }
}

void applySoftwarePostProcess(aster::SoftwareFrameBuffer &framebuffer,
                              const aster::RendererSettings &settings) {
  if (framebuffer.empty() || (!settings.post.bloom && !settings.post.fxaa)) {
    return;
  }
  std::vector<std::uint8_t> pixels(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  applySoftwareBloom(pixels, framebuffer.width(), framebuffer.height(), settings.post);
  applySoftwareFxaa(pixels, framebuffer.width(), framebuffer.height(), settings.post);
  framebuffer.replaceRgba8(framebuffer.width(), framebuffer.height(), pixels);
}

bool containsViewerCullVolume(const aster::ViewerCullVolume &volume, const aster::Vec3 point) {
  if (!volume.enabled || volume.half_extents.x <= 0.0f || volume.half_extents.y <= 0.0f ||
      volume.half_extents.z <= 0.0f) {
    return false;
  }
  const aster::Vec3 delta = point - volume.center;
  return std::abs(delta.x) <= volume.half_extents.x && std::abs(delta.y) <= volume.half_extents.y &&
         std::abs(delta.z) <= volume.half_extents.z;
}

aster::FaceCullMode objectCullMode(const aster::RenderObject &object,
                                   const aster::Vec3 camera_position,
                                   const bool pipeline_back_face_culling) {
  if (!pipeline_back_face_culling || aster::isDoubleSidedMaterial(object.material)) {
    return aster::FaceCullMode::None;
  }
  if (containsViewerCullVolume(object.viewer_cull_volume, camera_position)) {
    return object.viewer_cull_volume.inside;
  }
  if (object.viewer_cull_volume.enabled) {
    return object.viewer_cull_volume.outside;
  }
  return object.material.cull_mode;
}

bool isContactShadowUtility(const aster::RenderObject &object) {
  return aster::resolveMaterialSurfaceProfile(object.material) ==
         aster::MaterialSurfaceProfile::ContactShadow;
}

LocalBounds primitiveLocalBounds(const aster::MeshPrimitive primitive) {
  switch (primitive) {
  case aster::MeshPrimitive::Box:
    return {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  case aster::MeshPrimitive::Sphere:
  case aster::MeshPrimitive::Rock:
    return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
  case aster::MeshPrimitive::Crystal:
    return {{-1.0f, -0.9f, -0.9f}, {1.0f, 0.9f, 0.9f}};
  case aster::MeshPrimitive::RuinBlock:
    return {{-0.56f, -0.54f, -0.55f}, {0.56f, 0.54f, 0.55f}};
  case aster::MeshPrimitive::Pillar:
    return {{-1.18f, -0.5f, -1.18f}, {1.18f, 0.5f, 1.18f}};
  case aster::MeshPrimitive::Plane:
    return {{-6.0f, 0.0f, -6.0f}, {6.0f, 0.0f, 6.0f}};
  }
  return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
}

LocalBounds customMeshLocalBounds(const aster::CpuMesh &mesh) {
  if (mesh.vertices.empty()) {
    return {};
  }
  LocalBounds bounds{mesh.vertices.front().position, mesh.vertices.front().position};
  for (const aster::Vertex &vertex : mesh.vertices) {
    bounds.min.x = std::min(bounds.min.x, vertex.position.x);
    bounds.min.y = std::min(bounds.min.y, vertex.position.y);
    bounds.min.z = std::min(bounds.min.z, vertex.position.z);
    bounds.max.x = std::max(bounds.max.x, vertex.position.x);
    bounds.max.y = std::max(bounds.max.y, vertex.position.y);
    bounds.max.z = std::max(bounds.max.z, vertex.position.z);
  }
  return bounds;
}

LocalBounds objectLocalBounds(const aster::RenderObject &object) {
  return object.custom_mesh != nullptr ? customMeshLocalBounds(*object.custom_mesh)
                                       : primitiveLocalBounds(object.primitive);
}

float localMaxAbs(const float min_value, const float max_value) {
  return std::max(std::abs(min_value), std::abs(max_value));
}

float objectFootY(const aster::RenderObject &object, const LocalBounds &bounds) {
  return object.transform.position.y + bounds.min.y * object.transform.scale.y;
}

aster::Vec2 objectContactHalfExtents(const aster::RenderObject &object, const LocalBounds &bounds) {
  return {localMaxAbs(bounds.min.x, bounds.max.x) * std::abs(object.transform.scale.x),
          localMaxAbs(bounds.min.z, bounds.max.z) * std::abs(object.transform.scale.z)};
}

bool canCastContactShadow(const aster::RenderObject &object,
                          const aster::GroundingSettings &grounding) {
  const bool requested =
      object.casts_contact_shadow || (grounding.auto_contact_shadows && object.auto_contact_shadow);
  if (!grounding.enabled || !grounding.contact_shadows || !requested) {
    return false;
  }
  if (object.material.render_role == aster::MaterialRenderRole::SupportSurface) {
    return false;
  }
  if (aster::classifyMaterialRenderQueue(object.material) != aster::MaterialRenderQueue::Opaque ||
      isContactShadowUtility(object)) {
    return false;
  }
  if (object.custom_mesh == nullptr && object.primitive == aster::MeshPrimitive::Plane) {
    return false;
  }

  const LocalBounds bounds = objectLocalBounds(object);
  const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  if (raw_radius < min_radius || raw_radius > max_radius) {
    return false;
  }

  const float receiver_delta = std::abs(objectFootY(object, bounds) - grounding.reference_y);
  return receiver_delta <= std::max(grounding.contact_shadow_receiver_height, 0.0f);
}

aster::RenderObject contactShadowObjectFor(const aster::RenderObject &object,
                                           const aster::GroundingSettings &grounding) {
  const LocalBounds bounds = objectLocalBounds(object);
  const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  const float radius = std::clamp(raw_radius, min_radius, max_radius);
  const float footprint_x = std::clamp(half_extents.x * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float footprint_z = std::clamp(half_extents.y * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float foot_y = objectFootY(object, bounds);
  const float receiver_delta = std::abs(foot_y - grounding.reference_y);
  const float fade = 1.0f - smoothstep(grounding.contact_shadow_receiver_height * 0.72f,
                                       grounding.contact_shadow_receiver_height, receiver_delta);

  aster::RenderObject shadow;
  shadow.name = "Procedural contact shadow";
  shadow.primitive = aster::MeshPrimitive::Plane;
  shadow.transform.position = {object.transform.position.x,
                               foot_y + grounding.contact_shadow_receiver_bias,
                               object.transform.position.z};
  shadow.transform.rotation =
      aster::quatFromEulerXyz({0.0f, aster::eulerXyz(object.transform.rotation).y, 0.0f});
  shadow.transform.scale = {footprint_x, 1.0f, footprint_z};
  shadow.material.base_color = {0.0f, 0.0f, 0.0f};
  shadow.material.roughness = 1.0f;
  shadow.material.opacity = std::clamp(
      grounding.contact_shadow_strength * object.contact_shadow_strength * fade, 0.0f, 0.85f);
  shadow.material.surface_profile = aster::MaterialSurfaceProfile::ContactShadow;
  shadow.material.surface_pattern = aster::SurfacePattern::ContactShadow;
  shadow.material.alpha_mode = aster::MaterialAlphaMode::Blend;
  shadow.material.depth_write = aster::MaterialDepthWrite::Disabled;
  shadow.material.depth_policy = {.layer = aster::RenderDepthLayer::ContactShadow,
                                  .constant_bias = 0.00008f,
                                  .slope_bias = 0.00012f};
  shadow.material.camera_occlusion = aster::CameraOcclusionPolicy::Solid;
  shadow.material.double_sided = true;
  shadow.material.receives_shadows = false;
  shadow.casts_contact_shadow = false;
  shadow.casts_shadows = false;
  shadow.camera_occlusion_fade = false;
  return shadow;
}

void buildSoftwareShadowAtlas(const aster::Scene &scene, const aster::OrbitCamera &camera,
                              const aster::RendererSettings &settings,
                              SoftwareFrameResources &resources) {
  if (!settings.shadows.enabled || !settings.sun_light.enabled ||
      settings.sun_light.intensity <= 0.0f) {
    return;
  }
  const aster::Vec3 sun_dir =
      aster::normalizeOr(settings.sun_light.direction_to_light, {0.0f, 1.0f, 0.0f});
  const aster::Vec3 forward = -sun_dir;
  const aster::Vec3 basis_seed = std::abs(forward.y) < 0.92f ? aster::Vec3{0.0f, 1.0f, 0.0f}
                                                              : aster::Vec3{1.0f, 0.0f, 0.0f};
  const aster::Vec3 right = aster::normalizeOr(aster::cross(basis_seed, forward),
                                               {1.0f, 0.0f, 0.0f});
  const aster::Vec3 up = aster::normalizeOr(aster::cross(forward, right), {0.0f, 1.0f, 0.0f});

  const std::uint32_t cascade_count =
      settings.shadows.cascaded_directional
          ? std::clamp(settings.shadows.directional_cascades, 1u, 4u)
          : 1u;
  const std::uint32_t atlas_size = std::clamp(settings.shadows.atlas_size, 32u, 2048u);
  const std::uint32_t base_tile_width = std::max(atlas_size / cascade_count, 1u);
  resources.shadow_atlas_size = atlas_size;
  resources.shadow_depths.assign(static_cast<std::size_t>(atlas_size) * atlas_size,
                                 std::numeric_limits<float>::infinity());
  resources.shadow_rgba8.assign(static_cast<std::size_t>(atlas_size) * atlas_size * 4u, 255u);
  resources.cascades.clear();
  resources.cascades.reserve(cascade_count);

  const float max_distance = std::max(settings.shadows.max_distance, 1.0f);
  for (std::uint32_t cascade = 0u; cascade < cascade_count; ++cascade) {
    const std::uint32_t tile_x = cascade * base_tile_width;
    const std::uint32_t tile_width =
        cascade + 1u == cascade_count ? atlas_size - tile_x : base_tile_width;
    const float radius =
        max_distance * (static_cast<float>(cascade + 1u) / static_cast<float>(cascade_count));
    resources.cascades.push_back({.center = camera.target,
                                  .right = right,
                                  .up = up,
                                  .forward = forward,
                                  .radius = std::max(radius, 0.001f),
                                  .tile_x = tile_x,
                                  .tile_y = 0u,
                                  .tile_width = tile_width,
                                  .tile_height = atlas_size});
  }

  for (const aster::RenderObject &object : scene.objects()) {
    if (!aster::renderObjectCastsShadows(object) ||
        (object.custom_mesh != nullptr &&
         (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty()))) {
      continue;
    }
    const LocalBounds bounds = objectLocalBounds(object);
    const aster::Vec3 half_extents{
        localMaxAbs(bounds.min.x, bounds.max.x) * std::abs(object.transform.scale.x),
        localMaxAbs(bounds.min.y, bounds.max.y) * std::abs(object.transform.scale.y),
        localMaxAbs(bounds.min.z, bounds.max.z) * std::abs(object.transform.scale.z)};
    const float caster_radius =
        std::max({half_extents.x, half_extents.y, half_extents.z, 0.025f});
    const aster::Vec3 caster_center = object.transform.position;
    for (const SoftwareShadowCascade &cascade : resources.cascades) {
      const aster::Vec3 offset = caster_center - cascade.center;
      const float u = aster::dot(offset, cascade.right) / (cascade.radius * 2.0f) + 0.5f;
      const float v = aster::dot(offset, cascade.up) / (cascade.radius * 2.0f) + 0.5f;
      const float radius_u = caster_radius / (cascade.radius * 2.0f);
      if (u + radius_u < 0.0f || u - radius_u > 1.0f || v + radius_u < 0.0f ||
          v - radius_u > 1.0f) {
        continue;
      }
      const int min_x = std::max(0, static_cast<int>(
                                        std::floor((u - radius_u) *
                                                   static_cast<float>(cascade.tile_width))));
      const int max_x =
          std::min(static_cast<int>(cascade.tile_width) - 1,
                   static_cast<int>(std::ceil((u + radius_u) *
                                              static_cast<float>(cascade.tile_width))));
      const int min_y = std::max(0, static_cast<int>(
                                        std::floor((v - radius_u) *
                                                   static_cast<float>(cascade.tile_height))));
      const int max_y =
          std::min(static_cast<int>(cascade.tile_height) - 1,
                   static_cast<int>(std::ceil((v + radius_u) *
                                              static_cast<float>(cascade.tile_height))));
      const float caster_depth = aster::dot(caster_center, cascade.forward) - caster_radius;
      for (int y = min_y; y <= max_y; ++y) {
        const float py = (static_cast<float>(y) + 0.5f) /
                         static_cast<float>(std::max(cascade.tile_height, 1u));
        for (int x = min_x; x <= max_x; ++x) {
          const float px = (static_cast<float>(x) + 0.5f) /
                           static_cast<float>(std::max(cascade.tile_width, 1u));
          const float dx = (px - u) / std::max(radius_u, 0.0001f);
          const float dy = (py - v) / std::max(radius_u, 0.0001f);
          if (dx * dx + dy * dy > 1.0f) {
            continue;
          }
          const std::uint32_t atlas_x = cascade.tile_x + static_cast<std::uint32_t>(x);
          const std::uint32_t atlas_y = cascade.tile_y + static_cast<std::uint32_t>(y);
          const std::size_t index =
              static_cast<std::size_t>(atlas_y) * atlas_size + static_cast<std::size_t>(atlas_x);
          resources.shadow_depths[index] = std::min(resources.shadow_depths[index], caster_depth);
        }
      }
    }
  }

  for (std::uint32_t y = 0u; y < atlas_size; ++y) {
    for (std::uint32_t x = 0u; x < atlas_size; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y) * atlas_size + x;
      const bool occupied = std::isfinite(resources.shadow_depths[pixel]);
      const float checker = ((x / 8u + y / 8u) & 1u) == 0u ? 0.04f : 0.0f;
      const float value = occupied ? 0.18f + checker : 0.86f + checker;
      const std::size_t base = pixel * 4u;
      resources.shadow_rgba8[base + 0u] = debugByte(value);
      resources.shadow_rgba8[base + 1u] = debugByte(occupied ? value * 0.72f : value);
      resources.shadow_rgba8[base + 2u] = debugByte(occupied ? value * 0.52f : value);
      resources.shadow_rgba8[base + 3u] = 255u;
    }
  }
  resources.shadow_ready = true;
}

void buildSoftwareVolumetricFog(const aster::OrbitCamera &camera,
                                const aster::RendererSettings &settings,
                                SoftwareFrameResources &resources) {
  if (!settings.atmosphere.enabled || settings.atmosphere.fog_strength <= 0.0f ||
      resources.frame_width == 0u || resources.frame_height == 0u) {
    return;
  }
  resources.fog_width = std::max(resources.frame_width / 4u, 1u);
  resources.fog_height = std::max(resources.frame_height / 4u, 1u);
  resources.fog_factors.assign(static_cast<std::size_t>(resources.fog_width) *
                                   resources.fog_height,
                               0.0f);
  resources.fog_rgba8.assign(static_cast<std::size_t>(resources.fog_width) *
                                 resources.fog_height * 4u,
                             0u);
  const float camera_distance = aster::length(camera.position() - camera.target);
  for (std::uint32_t y = 0u; y < resources.fog_height; ++y) {
    const float fy = (static_cast<float>(y) + 0.5f) /
                     static_cast<float>(std::max(resources.fog_height, 1u));
    for (std::uint32_t x = 0u; x < resources.fog_width; ++x) {
      const float fx = (static_cast<float>(x) + 0.5f) /
                       static_cast<float>(std::max(resources.fog_width, 1u));
      const float vignette = std::sqrt((fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f));
      const float distance = camera_distance + settings.atmosphere.fog_start +
                             (settings.atmosphere.fog_end - settings.atmosphere.fog_start) *
                                 (0.18f + fy * 0.82f + vignette * 0.24f);
      const float fog = evaluateFogFactor(settings.atmosphere, distance);
      const std::size_t pixel = static_cast<std::size_t>(y) * resources.fog_width + x;
      resources.fog_factors[pixel] = fog;
      const std::size_t base = pixel * 4u;
      resources.fog_rgba8[base + 0u] = debugByte(settings.atmosphere.fog_color.x * fog);
      resources.fog_rgba8[base + 1u] = debugByte(settings.atmosphere.fog_color.y * fog);
      resources.fog_rgba8[base + 2u] = debugByte(settings.atmosphere.fog_color.z * fog);
      resources.fog_rgba8[base + 3u] = debugByte(fog);
    }
  }
  resources.fog_ready = true;
}

void buildSoftwareReflectionProbeAtlas(const aster::Scene &scene,
                                       const aster::RendererSettings &settings,
                                       SoftwareFrameResources &resources) {
  if (!settings.reflections.enabled || !settings.reflections.static_local_probes ||
      scene.reflectionProbes().empty()) {
    return;
  }
  const std::uint32_t max_probes =
      settings.reflections.max_active_probes == 0u
          ? static_cast<std::uint32_t>(scene.reflectionProbes().size())
          : settings.reflections.max_active_probes;
  const std::uint32_t probe_count = std::min<std::uint32_t>(
      static_cast<std::uint32_t>(scene.reflectionProbes().size()), std::max(max_probes, 1u));
  const std::uint32_t face_size = std::clamp(settings.reflections.probe_resolution, 8u, 256u);
  resources.reflection_width = face_size * 6u * probe_count;
  resources.reflection_height = face_size;
  resources.probes.clear();
  resources.probes.reserve(probe_count);
  resources.reflection_rgba8.assign(static_cast<std::size_t>(resources.reflection_width) *
                                        resources.reflection_height * 4u,
                                    0u);

  for (std::uint32_t probe_index = 0u; probe_index < probe_count; ++probe_index) {
    const aster::ReflectionProbe &probe = scene.reflectionProbes()[probe_index];
    resources.probes.push_back({.position = probe.position,
                                .influence_radius = std::max(probe.influence_radius, 0.001f),
                                .sky_irradiance = probe.sky_irradiance,
                                .ground_irradiance = probe.ground_irradiance,
                                .specular_tint = probe.specular_tint,
                                .intensity = probe.intensity});
    for (std::uint32_t face = 0u; face < 6u; ++face) {
      const float face_sky = static_cast<float>(face) / 5.0f;
      const aster::Vec3 face_color =
          mixVec(probe.ground_irradiance, probe.sky_irradiance, face_sky) *
          probe.specular_tint * std::clamp(probe.intensity, 0.0f, 4.0f);
      for (std::uint32_t y = 0u; y < face_size; ++y) {
        const float vertical = (static_cast<float>(y) + 0.5f) / static_cast<float>(face_size);
        for (std::uint32_t x = 0u; x < face_size; ++x) {
          const float horizontal = (static_cast<float>(x) + 0.5f) / static_cast<float>(face_size);
          const float edge = 1.0f - smoothstep(0.42f, 0.72f,
                                               std::abs(horizontal - 0.5f) +
                                                   std::abs(vertical - 0.5f));
          const aster::Vec3 color = face_color * (0.72f + edge * 0.28f);
          const std::uint32_t atlas_x = (probe_index * 6u + face) * face_size + x;
          const std::uint32_t atlas_y = y;
          const std::size_t base =
              (static_cast<std::size_t>(atlas_y) * resources.reflection_width + atlas_x) * 4u;
          resources.reflection_rgba8[base + 0u] = debugByte(color.x);
          resources.reflection_rgba8[base + 1u] = debugByte(color.y);
          resources.reflection_rgba8[base + 2u] = debugByte(color.z);
          resources.reflection_rgba8[base + 3u] = 255u;
        }
      }
    }
  }
  resources.reflection_ready = true;
}

ProjectedVertex projectVertex(const aster::Vertex &vertex, const aster::Mat4 &model,
                              const aster::Mat4 &model_view_projection, const int width,
                              const int height, const float normal_offset) {
  const aster::Vec3 local_position = vertex.position + vertex.normal * normal_offset;
  const Vec4f clip = transformPoint4(model_view_projection, local_position);
  if (clip.w <= 0.0001f) {
    return {};
  }

  const float inv_w = 1.0f / clip.w;
  const float ndc_x = clip.x * inv_w;
  const float ndc_y = clip.y * inv_w;
  const float ndc_z = clip.z * inv_w;
  if (ndc_z < 0.0f || ndc_z > 1.0f || ndc_x < -2.0f || ndc_x > 2.0f || ndc_y < -2.0f ||
      ndc_y > 2.0f) {
    return {};
  }

  const aster::Vec3 world_tangent =
      aster::transformVector(model, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
  return {
      .valid = true,
      .world_position = aster::transformPoint(model, local_position),
      .normal = aster::normalize(aster::transformVector(model, vertex.normal)),
      .tangent = {world_tangent.x, world_tangent.y, world_tangent.z, vertex.tangent.w},
      .uv = vertex.uv,
      .ambient_occlusion = vertex.ambient_occlusion,
      .x = (ndc_x * 0.5f + 0.5f) * static_cast<float>(width),
      .y = (1.0f - (ndc_y * 0.5f + 0.5f)) * static_cast<float>(height),
      .depth = ndc_z,
  };
}

bool shouldCullTriangle(const ProjectedVertex &a, const ProjectedVertex &b,
                        const ProjectedVertex &c, const aster::Vec3 camera_position,
                        const aster::FaceCullMode cull_mode) {
  if (cull_mode == aster::FaceCullMode::None) {
    return false;
  }
  const aster::Vec3 face_normal = aster::normalize(
      aster::cross(b.world_position - a.world_position, c.world_position - a.world_position));
  const aster::Vec3 centroid = (a.world_position + b.world_position + c.world_position) / 3.0f;
  const bool front_facing = aster::dot(face_normal, camera_position - centroid) >= 0.0f;
  return (cull_mode == aster::FaceCullMode::Back && !front_facing) ||
         (cull_mode == aster::FaceCullMode::Front && front_facing);
}

void drawMesh(aster::SoftwareFrameBuffer &framebuffer, const aster::CpuMesh &mesh,
              const aster::RenderObject &object, const aster::OrbitCamera &camera,
              const aster::RendererSettings &settings, const double frame_seconds,
              const float opacity, std::size_t &draw_calls,
              const aster::MaterialResourceLibrary *material_library,
              const SoftwareFrameResources *frame_resources) {
  if (mesh.vertices.empty() || mesh.indices.empty() || opacity <= 0.003f) {
    return;
  }
  const aster::MaterialRuntimeResource *runtime_material =
      material_library == nullptr
          ? nullptr
          : material_library->findForMaterialIds(object.material_asset_id, object.material.asset_id);
  const aster::Material &material =
      runtime_material == nullptr ? object.material : runtime_material->fallback_material;
  const aster::RuntimeTextureSet *runtime_textures =
      runtime_material == nullptr ? nullptr : &runtime_material->texture_set;

  const aster::Vec3 camera_position = camera.position();
  const aster::Mat4 model =
      settings.animate_scene && object.spin_rate != 0.0f
          ? object.transform.matrix() *
                aster::rotation_y(static_cast<float>(frame_seconds) * object.spin_rate)
          : object.transform.matrix();
  const float aspect_ratio = static_cast<float>(std::max(framebuffer.width(), 1)) /
                             static_cast<float>(std::max(framebuffer.height(), 1));
  const aster::Mat4 model_view_projection =
      camera.projectionMatrix(aspect_ratio).value * camera.viewMatrix().value * model;
  const aster::FaceCullMode cull_mode =
      objectCullMode(object, camera_position, settings.pipeline.back_face_culling);
  const bool alpha_blend =
      opacity < 0.999f || aster::classifyMaterialRenderQueue(material) ==
                              aster::MaterialRenderQueue::Translucent;
  const bool depth_write = aster::materialWritesDepth(material) && !alpha_blend;
  const aster::RenderDepthPolicy depth_policy = material.depth_policy;

  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const ProjectedVertex a =
        projectVertex(mesh.vertices[mesh.indices[i + 0u]], model, model_view_projection,
                      framebuffer.width(), framebuffer.height(), depth_policy.normal_offset);
    const ProjectedVertex b =
        projectVertex(mesh.vertices[mesh.indices[i + 1u]], model, model_view_projection,
                      framebuffer.width(), framebuffer.height(), depth_policy.normal_offset);
    const ProjectedVertex c =
        projectVertex(mesh.vertices[mesh.indices[i + 2u]], model, model_view_projection,
                      framebuffer.width(), framebuffer.height(), depth_policy.normal_offset);
    if (!a.valid || !b.valid || !c.valid ||
        shouldCullTriangle(a, b, c, camera_position, cull_mode)) {
      continue;
    }

    const aster::FrameVertex fa{
        .x = a.x,
        .y = a.y,
        .depth = a.depth,
        .color = shadedFrameColor(material, a.world_position, a.normal, a.tangent, a.uv,
                                  a.ambient_occlusion, camera_position, settings, frame_seconds,
                                  opacity, runtime_textures, frame_resources),
    };
    const aster::FrameVertex fb{
        .x = b.x,
        .y = b.y,
        .depth = b.depth,
        .color = shadedFrameColor(material, b.world_position, b.normal, b.tangent, b.uv,
                                  b.ambient_occlusion, camera_position, settings, frame_seconds,
                                  opacity, runtime_textures, frame_resources),
    };
    const aster::FrameVertex fc{
        .x = c.x,
        .y = c.y,
        .depth = c.depth,
        .color = shadedFrameColor(material, c.world_position, c.normal, c.tangent, c.uv,
                                  c.ambient_occlusion, camera_position, settings, frame_seconds,
                                  opacity, runtime_textures, frame_resources),
    };
    framebuffer.drawTriangle(fa, fb, fc, settings.pipeline.depth_test, depth_write, alpha_blend,
                             depth_policy.constant_bias, depth_policy.slope_bias);
  }
  ++draw_calls;
}

} // namespace

namespace aster {

RenderDevice::RenderDevice() = default;
RenderDevice::~RenderDevice() = default;

std::string_view renderBackendKindName(const RenderBackendKind kind) {
  switch (kind) {
  case RenderBackendKind::SoftwareReference:
    return "software-reference";
  case RenderBackendKind::Metal:
    return "metal";
  case RenderBackendKind::D3D12:
    return "d3d12";
  case RenderBackendKind::Null:
    return "null";
  case RenderBackendKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::string_view renderStylePresetName(const RenderStylePreset preset) {
  switch (preset) {
  case RenderStylePreset::Neutral:
    return "neutral";
  case RenderStylePreset::RetroHorrorReadable:
    return "retro-horror";
  }
  return "neutral";
}

std::optional<RenderStylePreset> parseRenderStylePreset(const std::string_view value) {
  const std::string normalized = normalizeStyleName(value);
  if (normalized.empty() || normalized == "neutral" || normalized == "none" ||
      normalized == "default") {
    return RenderStylePreset::Neutral;
  }
  if (normalized == "retro" || normalized == "retro-horror" ||
      normalized == "retro-horror-readable" || normalized == "ps1-horror") {
    return RenderStylePreset::RetroHorrorReadable;
  }
  return std::nullopt;
}

RenderStyleProfile makeRenderStyleProfile(const RenderStylePreset preset) {
  RenderStyleProfile profile;
  profile.preset = preset;
  switch (preset) {
  case RenderStylePreset::Neutral:
    break;
  case RenderStylePreset::RetroHorrorReadable:
    profile.unlit_mix = 0.14f;
    profile.emissive_gain = 2.35f;
    profile.luma_crush = 0.14f;
    profile.color_quantization_steps = 48.0f;
    profile.procedural_sample_snap = 0.0f;
    break;
  }
  return profile;
}

void applyRenderStyleProfile(RendererSettings &settings, const RenderStyleProfile &profile) {
  settings.style = profile;
  if (profile.preset == RenderStylePreset::Neutral) {
    settings.atmosphere.fog_falloff = AtmosphereFogFalloff::SmoothLinear;
    settings.atmosphere.fog_power = 1.0f;
    return;
  }

  settings.pipeline.clear_color = {0.018f, 0.003f, 0.002f};
  settings.ambient_strength = std::min(settings.ambient_strength, 0.30f);
  settings.ambient_floor = std::min(settings.ambient_floor, 0.055f);
  settings.sun_light.intensity = std::min(settings.sun_light.intensity, 0.85f);
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.145f, 0.022f, 0.014f};
  settings.atmosphere.fog_start = 1.85f;
  settings.atmosphere.fog_end = 16.0f;
  settings.atmosphere.fog_strength = 0.52f;
  settings.atmosphere.fog_falloff = AtmosphereFogFalloff::Exponential;
  settings.atmosphere.fog_power = 1.45f;
  settings.atmosphere.saturation = 1.08f;
  settings.atmosphere.contrast = 1.18f;
  settings.atmosphere.shadow_tint = {1.10f, 0.44f, 0.34f};
  settings.atmosphere.shadow_tint_strength = 0.12f;
  settings.atmosphere.highlight_tint = {1.16f, 0.66f, 0.42f};
  settings.atmosphere.highlight_tint_strength = 0.08f;
}

LightRig defaultLightRig() {
  return {
      Light{{-5.8f, 7.2f, 3.8f}, {29.0f, 27.0f, 23.0f}, 1.0f},
      Light{{4.7f, 3.6f, 2.0f}, {11.0f, 12.0f, 13.5f}, 1.0f},
      Light{{0.2f, 3.8f, -8.8f}, {14.0f, 10.5f, 7.8f}, 1.0f},
      Light{{0.0f, 2.0f, 4.8f}, {7.0f, 6.7f, 7.2f}, 1.0f},
  };
}

std::vector<Light> selectRenderLights(const LightRig &lights, const Vec3 reference_position,
                                      const RenderLightPolicy &policy) {
  std::vector<Light> active;
  active.reserve(lights.size());
  for (const Light &light : lights) {
    if (light.intensity > policy.min_intensity) {
      active.push_back(light);
    }
  }

  const std::size_t budget =
      std::min({active.size(), policy.max_point_lights, kRenderLightUniformCapacity});
  if (active.size() <= budget) {
    return active;
  }
  if (budget == 0u) {
    active.clear();
    return active;
  }
  if (policy.distance_weighted) {
    const auto score = [reference_position](const Light &light) {
      const Vec3 delta = light.position - reference_position;
      const float distance_sq =
          std::max(dot(delta, delta), light.source_radius * light.source_radius + 0.0001f);
      const float energy =
          light.color.x * 0.2126f + light.color.y * 0.7152f + light.color.z * 0.0722f;
      return std::max(energy, 0.0f) * std::max(light.intensity, 0.0f) / distance_sq;
    };
    std::partial_sort(active.begin(), active.begin() + static_cast<std::ptrdiff_t>(budget),
                      active.end(), [&](const Light &lhs, const Light &rhs) {
                        return score(lhs) > score(rhs);
                      });
  }
  active.resize(budget);
  return active;
}

ClusteredLightGrid buildClusteredLightGrid(const LightRig &lights, const OrbitCamera &camera,
                                           const int framebuffer_width,
                                           const int framebuffer_height,
                                           const ClusteredLightPolicy &policy) {
  ClusteredLightGrid grid;
  if (!policy.enabled || policy.mode == ClusteredLightCullingMode::Disabled ||
      framebuffer_width <= 0 || framebuffer_height <= 0) {
    return grid;
  }

  grid.cluster_count_x = std::max(policy.cluster_count_x, 1u);
  grid.cluster_count_y = std::max(policy.cluster_count_y, 1u);
  grid.cluster_count_z = std::max(policy.cluster_count_z, 1u);
  const std::uint32_t cluster_count =
      grid.cluster_count_x * grid.cluster_count_y * grid.cluster_count_z;
  grid.cluster_offsets.assign(static_cast<std::size_t>(cluster_count) + 1u, 0u);

  RenderLightPolicy visible_policy;
  visible_policy.max_point_lights =
      std::min<std::size_t>(std::max<std::size_t>(policy.max_visible_lights, 1u),
                            kRenderLightUniformCapacity);
  visible_policy.distance_weighted = true;
  visible_policy.min_intensity = 0.0f;
  grid.visible_lights = selectRenderLights(lights, camera.target, visible_policy);
  grid.fallback_used = grid.visible_lights.size() < lights.size();

  const float aspect = static_cast<float>(std::max(framebuffer_width, 1)) /
                       static_cast<float>(std::max(framebuffer_height, 1));
  const Mat4 view_projection = camera.viewProjectionMatrix(aspect).value;
  const Vec3 eye = camera.position();
  const Vec3 forward = normalizeOr(camera.target - eye, {0.0f, 0.0f, -1.0f});
  const float near_plane = std::max(camera.near_plane, 0.001f);
  const float far_plane = std::max(camera.far_plane, near_plane + 0.001f);
  const std::uint32_t max_lights_per_cluster = std::max(policy.max_lights_per_cluster, 1u);
  std::vector<std::uint32_t> per_cluster_count(cluster_count, 0u);

  const auto cluster_index_for = [&](const Light &light) -> std::uint32_t {
    const Vec4 clip =
        view_projection * Vec4{light.position.x, light.position.y, light.position.z, 1.0f};
    const float inv_w = std::abs(clip.w) > 0.000001f ? 1.0f / clip.w : 1.0f;
    const float ndc_x = std::clamp(clip.x * inv_w, -1.0f, 1.0f);
    const float ndc_y = std::clamp(clip.y * inv_w, -1.0f, 1.0f);
    const float depth = std::clamp(dot(light.position - eye, forward), near_plane, far_plane);
    const float depth_t = (depth - near_plane) / std::max(far_plane - near_plane, 0.001f);
    const std::uint32_t x = std::min(
        static_cast<std::uint32_t>(((ndc_x * 0.5f) + 0.5f) * grid.cluster_count_x),
        grid.cluster_count_x - 1u);
    const std::uint32_t y = std::min(
        static_cast<std::uint32_t>((1.0f - ((ndc_y * 0.5f) + 0.5f)) * grid.cluster_count_y),
        grid.cluster_count_y - 1u);
    const std::uint32_t z =
        std::min(static_cast<std::uint32_t>(depth_t * grid.cluster_count_z),
                 grid.cluster_count_z - 1u);
    return (z * grid.cluster_count_y + y) * grid.cluster_count_x + x;
  };

  for (std::uint32_t light_index = 0u;
       light_index < static_cast<std::uint32_t>(grid.visible_lights.size()); ++light_index) {
    const std::uint32_t cluster_index = cluster_index_for(grid.visible_lights[light_index]);
    if (per_cluster_count[cluster_index] >= max_lights_per_cluster) {
      grid.overflowed = true;
      if (policy.fallback == ClusteredLightFallbackPolicy::DisableOverflow) {
        continue;
      }
    }
    ++per_cluster_count[cluster_index];
    grid.assignments.push_back({.cluster_index = cluster_index, .light_index = light_index});
  }

  std::sort(grid.assignments.begin(), grid.assignments.end(),
            [](const ClusteredLightAssignment &lhs, const ClusteredLightAssignment &rhs) {
              if (lhs.cluster_index != rhs.cluster_index) {
                return lhs.cluster_index < rhs.cluster_index;
              }
              return lhs.light_index < rhs.light_index;
            });
  std::fill(grid.cluster_offsets.begin(), grid.cluster_offsets.end(), 0u);
  for (const ClusteredLightAssignment &assignment : grid.assignments) {
    ++grid.cluster_offsets[static_cast<std::size_t>(assignment.cluster_index) + 1u];
  }
  for (std::size_t i = 1u; i < grid.cluster_offsets.size(); ++i) {
    grid.cluster_offsets[i] += grid.cluster_offsets[i - 1u];
  }
  return grid;
}

namespace {

rhi::DeviceCapabilities softwareCapabilityTable() {
  rhi::DeviceCapabilities table;
  table.backend = rhi::BackendKind::SoftwareReference;
  table.shader_materials = true;
  table.texture_sampling = true;
  table.instancing = false;
  table.capture = true;
  table.ui_composite = true;
  table.gpu_timestamps = false;
  table.storage_buffers = false;
  table.texture_arrays = false;
  table.shadow_maps = true;
  table.debug_markers = false;
  table.hdr_render_targets = true;
  table.msaa = false;
  table.color_format_mask = rhi::imageFormatCapabilityBit(rhi::ImageFormat::Bgra8Unorm) |
                            rhi::imageFormatCapabilityBit(rhi::ImageFormat::Rgba8Unorm) |
                            rhi::imageFormatCapabilityBit(rhi::ImageFormat::Rgba16Float);
  table.depth_format_mask = rhi::imageFormatCapabilityBit(rhi::ImageFormat::Depth32Float);
  table.sample_count_mask = rhi::sampleCountCapabilityBit(1u);
  table.blend_mode_mask = rhi::blendModeCapabilityBit(rhi::BlendMode::Opaque) |
                          rhi::blendModeCapabilityBit(rhi::BlendMode::AlphaBlend);
  table.shader_model = rhi::ShaderModel::SoftwareReference;
  table.presentation = rhi::PresentationMode::SoftwareFramebuffer;
  table.limits.max_color_attachments = 1u;
  table.limits.max_sampled_textures_per_material =
      static_cast<std::uint32_t>(aster::materialRuntimeTextureRoles().size());
  table.limits.max_samplers_per_material = 1u;
  table.limits.max_uniform_buffers_per_stage = 1u;
  table.limits.max_bind_groups = 1u;
  table.limits.max_vertex_attributes = 5u;
  table.limits.max_texture_dimension_2d = 16384u;
  table.limits.max_dynamic_uniform_bytes = 64u * 1024u;
  return table;
}

RenderBackendCapabilities softwareCapabilities() {
  const std::uint32_t graph_resources =
      renderGraphResourceBit(RenderGraphResource::SceneColor) |
      renderGraphResourceBit(RenderGraphResource::SceneDepth) |
      renderGraphResourceBit(RenderGraphResource::LightClusters) |
      renderGraphResourceBit(RenderGraphResource::ShadowAtlas) |
      renderGraphResourceBit(RenderGraphResource::VolumetricFog) |
      renderGraphResourceBit(RenderGraphResource::ReflectionProbes) |
      renderGraphResourceBit(RenderGraphResource::UiOverlay) |
      renderGraphResourceBit(RenderGraphResource::CaptureReadback);
  return {.kind = RenderBackendKind::SoftwareReference,
          .name = "Aster Learning Software Rasterizer",
          .gpu = false,
          .supports_shader_materials = true,
          .supports_texture_sampling = true,
          .supports_instancing = false,
          .supports_capture = true,
          .supports_ui_composite = true,
          .supports_gpu_timestamps = false,
          .graph_resource_mask = graph_resources,
          .capability_table = softwareCapabilityTable()};
}

struct MaterialFrameSummary {
  std::size_t pipeline_switches = 0u;
  std::size_t material_permutations = 0u;
  std::size_t material_variant_cache_hits = 0u;
  std::size_t material_variant_cache_misses = 0u;
  std::vector<std::size_t> transparent_order;
  std::vector<FrameDiagnosticEvent> events;
};

std::uint64_t appendEvidenceValue(std::uint64_t hash, const std::uint64_t value) {
  constexpr std::uint64_t kPrime = 1099511628211ull;
  hash ^= value;
  hash *= kPrime;
  return hash;
}

std::uint64_t framePlanEvidenceHash(const FrameRenderPlan &plan) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, plan.source_ir_hash);
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(plan.instances.size()));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(plan.groups.size()));
  for (const FrameRenderDrawGroup &group : plan.groups) {
    hash = appendEvidenceValue(hash, group.signature.key.value);
    hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(group.first_instance));
    hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(group.instance_count));
  }
  return hash;
}

const char *mathOperationName(const aster::MathDiagnosticOperation operation) {
  switch (operation) {
  case aster::MathDiagnosticOperation::Normalize:
    return "normalize";
  case aster::MathDiagnosticOperation::NormalizeFallback:
    return "normalize-fallback";
  case aster::MathDiagnosticOperation::PlaneConstruction:
    return "plane-construction";
  case aster::MathDiagnosticOperation::Projection:
    return "projection";
  case aster::MathDiagnosticOperation::Unprojection:
    return "unprojection";
  case aster::MathDiagnosticOperation::MatrixInverse:
    return "matrix-inverse";
  case aster::MathDiagnosticOperation::MatrixDecomposition:
    return "matrix-decomposition";
  case aster::MathDiagnosticOperation::GeometryQuery:
    return "geometry-query";
  case aster::MathDiagnosticOperation::RobustPredicate:
    return "robust-predicate";
  case aster::MathDiagnosticOperation::AuthoringMeasure:
    return "authoring-measure";
  case aster::MathDiagnosticOperation::Unknown:
  default:
    return "unknown";
  }
}

aster::FrameDiagnosticKind frameDiagnosticKindForMath(
    const aster::MathDiagnosticEvent &event) {
  if (event.operation == aster::MathDiagnosticOperation::RobustPredicate) {
    return aster::FrameDiagnosticKind::PredicateUncertainty;
  }
  return aster::FrameDiagnosticKind::MathContract;
}

void appendMathDiagnosticsToFrame(std::vector<aster::FrameDiagnosticEvent> &events) {
  const std::size_t count = aster::mathDiagnosticCount();
  for (std::size_t i = 0; i < count; ++i) {
    const aster::MathDiagnosticEvent event = aster::mathDiagnosticAt(i);
    events.push_back({.kind = frameDiagnosticKindForMath(event),
                      .severity = aster::FrameDiagnosticSeverity::Warning,
                      .pass = "math",
                      .label = mathOperationName(event.operation),
                      .message = event.message == nullptr ? "math contract diagnostic"
                                                          : event.message,
                      .value = static_cast<std::uint64_t>(event.error)});
  }
  if (count > 0u) {
    aster::clearMathDiagnostics();
  }
}

bool finiteQuat(const aster::Quat value) {
  return aster::isFiniteScalar(value.x) && aster::isFiniteScalar(value.y) &&
         aster::isFiniteScalar(value.z) && aster::isFiniteScalar(value.w);
}

std::string objectDiagnosticLabel(const aster::RenderObject &object, const std::size_t index) {
  return object.name.empty() ? ("object:" + std::to_string(index)) : object.name;
}

void appendRenderMathContractDiagnostics(const aster::Scene &scene,
                                         std::vector<aster::FrameDiagnosticEvent> &events) {
  for (std::size_t i = 0; i < scene.objects().size(); ++i) {
    const aster::RenderObject &object = scene.objects()[i];
    const std::string label = objectDiagnosticLabel(object, i);
    if (!aster::allFinite(object.transform.position) || !aster::allFinite(object.transform.scale) ||
        !finiteQuat(object.transform.rotation)) {
      events.push_back({.kind = aster::FrameDiagnosticKind::NonFiniteWorldMatrix,
                        .severity = aster::FrameDiagnosticSeverity::Error,
                        .pass = "scene-contract",
                        .label = label,
                        .message = "Render object transform contains non-finite values.",
                        .value = i});
      continue;
    }
    const aster::Vec3 scale = object.transform.scale;
    if (std::abs(scale.x) <= 0.000001f || std::abs(scale.y) <= 0.000001f ||
        std::abs(scale.z) <= 0.000001f) {
      events.push_back({.kind = aster::FrameDiagnosticKind::SingularNormalMatrix,
                        .severity = aster::FrameDiagnosticSeverity::Warning,
                        .pass = "scene-contract",
                        .label = label,
                        .message = "Render object scale makes its normal matrix singular.",
                        .value = i});
    }
    if (scale.x * scale.y * scale.z < 0.0f) {
      events.push_back({.kind = aster::FrameDiagnosticKind::NegativeScaleTangentFlip,
                        .severity = aster::FrameDiagnosticSeverity::Info,
                        .pass = "scene-contract",
                        .label = label,
                        .message = "Negative transform scale flips tangent-space handedness.",
                        .value = i});
    }
  }
}

const aster::framegraph::CompiledResource *
compiledResourceFor(const aster::FixedRenderGraph &graph, const aster::framegraph::ResourceHandle handle) {
  const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
                                  [handle](const aster::framegraph::CompiledResource &resource) {
                                    return resource.handle == handle;
                                  });
  return found == graph.resources.end() ? nullptr : &*found;
}

std::uint64_t textureDescriptorLayoutHash() {
  aster::rhi::PipelineLayoutDesc layout;
  std::uint32_t binding = 0u;
  for (const std::string_view role : aster::materialRuntimeTextureRoles()) {
    (void)role;
    layout.descriptor_ranges.push_back({.kind = aster::rhi::DescriptorRangeKind::SampledImage,
                                        .binding = binding++,
                                        .count = 1u,
                                        .stage_mask = 1u});
  }
  layout.descriptor_ranges.push_back({.kind = aster::rhi::DescriptorRangeKind::Sampler,
                                      .binding = binding,
                                      .count = 1u,
                                      .stage_mask = 1u});
  layout.descriptor_range_count = static_cast<std::uint32_t>(layout.descriptor_ranges.size());
  return aster::rhi::descriptorLayoutHash(layout);
}

std::uint64_t estimatedBytesFor(const aster::framegraph::ResourceDesc &desc) {
  std::uint64_t bytes_per_pixel = 4u;
  switch (desc.format) {
  case aster::rhi::ImageFormat::Rgba16Float:
    bytes_per_pixel = 8u;
    break;
  case aster::rhi::ImageFormat::Depth32Float:
  case aster::rhi::ImageFormat::Rgba8Unorm:
  case aster::rhi::ImageFormat::Rgba8Srgb:
  case aster::rhi::ImageFormat::Bgra8Unorm:
  case aster::rhi::ImageFormat::Bgra8Srgb:
    bytes_per_pixel = 4u;
    break;
  case aster::rhi::ImageFormat::Rg8Unorm:
    bytes_per_pixel = 2u;
    break;
  case aster::rhi::ImageFormat::R8Unorm:
    bytes_per_pixel = 1u;
    break;
  default:
    bytes_per_pixel = 4u;
    break;
  }
  const std::uint64_t width = std::max(desc.extent.width, 1u);
  const std::uint64_t height = std::max(desc.extent.height, 1u);
  const std::uint64_t depth = std::max(desc.extent.depth, 1u);
  return width * height * depth * bytes_per_pixel;
}

void appendFrameGraphForensics(const aster::FixedRenderGraph &graph,
                               const aster::RenderBackendCapabilities &capabilities,
                               const int framebuffer_width,
                               const int framebuffer_height,
                               aster::FrameForensics &forensics) {
  for (std::size_t pass_index = 0u; pass_index < graph.passes.size(); ++pass_index) {
    const aster::framegraph::CompiledPass &pass = graph.passes[pass_index];
    const aster::RenderGraphPass semantic = aster::renderGraphPassFromName(pass.name);
    const aster::RenderGraphPassDeclaration *declaration =
        aster::defaultRenderPassDeclaration(semantic);
    bool backend_has_declared_outputs = false;
    if (declaration != nullptr) {
      backend_has_declared_outputs =
          std::all_of(declaration->outputs.begin(), declaration->outputs.end(),
                      [&capabilities](const aster::RenderGraphResourceBindingDesc &output) {
                        return (capabilities.graph_resource_mask &
                                aster::renderGraphResourceBit(output.resource)) != 0u;
                      });
    }
    if (declaration != nullptr && !declaration->produces_backend_work &&
        !backend_has_declared_outputs) {
      forensics.events.push_back(
          {.kind = aster::FrameDiagnosticKind::CapabilityMismatch,
           .severity = aster::FrameDiagnosticSeverity::Info,
           .pass = pass.name,
           .label = aster::renderGraphExecutorKeyName(declaration->executor).data(),
           .message =
               "Pass is declared as a resource producer/consumer, but its backend workload is not wired yet.",
           .value = pass_index});
    }
    if (declaration != nullptr &&
        declaration->debug_capture != aster::RenderGraphDebugCapturePolicy::Disabled) {
      for (const aster::RenderGraphResourceBindingDesc &output : declaration->outputs) {
        forensics.captures.push_back(
            {.pass = semantic,
             .view = output.resource == aster::RenderGraphResource::ShadowAtlas
                         ? aster::RendererDebugView::ShadowMask
                         : (output.resource == aster::RenderGraphResource::VolumetricFog
                                ? aster::RendererDebugView::Fog
                                : (output.resource == aster::RenderGraphResource::ReflectionProbes
                                       ? aster::RendererDebugView::ReflectionProbe
                                       : aster::RendererDebugView::FinalColor)),
             .resource = output.resource,
             .label = pass.name + ":" +
                      std::string(aster::renderGraphResourceName(output.resource)),
             .width = static_cast<std::uint32_t>(std::max(framebuffer_width, 0)),
             .height = static_cast<std::uint32_t>(std::max(framebuffer_height, 0)),
             .row_stride_bytes =
                 static_cast<std::uint32_t>(std::max(framebuffer_width, 0) * 4),
             .available = false});
      }
    }

    if (!pass.descriptor_requirements.empty()) {
      aster::rhi::DescriptorLayoutTrace layout;
      layout.label = pass.name;
      for (const aster::framegraph::DescriptorRequirement &requirement :
           pass.descriptor_requirements) {
        layout.ranges.push_back({.kind = requirement.kind,
                                 .binding = requirement.binding,
                                 .count = requirement.count,
                                 .stage_mask = 1u});
      }
      aster::rhi::PipelineLayoutDesc layout_desc;
      layout_desc.descriptor_ranges = layout.ranges;
      layout_desc.descriptor_range_count =
          static_cast<std::uint32_t>(layout_desc.descriptor_ranges.size());
      layout.layout_hash = aster::rhi::descriptorLayoutHash(layout_desc);
      forensics.rhi_trace.descriptor_layouts.push_back(layout);
      aster::rhi::GraphicsPipelineDesc pipeline;
      pipeline.render_pass = pass.pipeline_compatibility;
      pipeline.pipeline_cache_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
      forensics.rhi_trace.pipelines.push_back({.label = pass.name,
                                               .cache_key = pipeline.pipeline_cache_key,
                                               .descriptor_layout_hash = layout.layout_hash});
    }

    forensics.rhi_trace.queue_submits.push_back(
        {.label = pass.name,
         .queue = declaration == nullptr ? aster::rhi::QueueKind::Graphics : declaration->queue,
         .command_buffer_count = 1u,
         .signal_fence_value = pass_index + 1u});
  }

  for (const aster::framegraph::GraphBarrier &barrier : graph.barriers) {
    const aster::framegraph::CompiledPass *pass =
        barrier.pass_index < graph.passes.size() ? &graph.passes[barrier.pass_index] : nullptr;
    const aster::framegraph::CompiledResource *resource =
        compiledResourceFor(graph, barrier.resource);
    const std::string pass_name = pass == nullptr ? "unknown" : pass->name;
    const std::string resource_name = resource == nullptr ? "unknown" : resource->name;
    const aster::RenderGraphPass semantic = aster::renderGraphPassFromName(pass_name);
    const bool writes = pass != nullptr &&
                        std::find(pass->writes.begin(), pass->writes.end(), barrier.resource) !=
                            pass->writes.end();
    forensics.resource_traces.push_back(
        {.pass = semantic,
         .resource = aster::renderGraphResourceFromName(resource_name),
         .pass_name = pass_name,
         .resource_name = resource_name,
         .before = barrier.before,
         .after = barrier.after,
         .queue = barrier.expanded.destination_queue,
         .write = writes});
    forensics.rhi_trace.transitions.push_back(
        {.pass = pass_name, .resource = resource_name, .barrier = barrier.expanded});
  }

  for (const aster::framegraph::CompiledResource &resource : graph.resources) {
    const std::uint64_t bytes = estimatedBytesFor(resource.desc);
    if (resource.desc.lifetime == aster::framegraph::ResourceLifetime::Transient) {
      forensics.rhi_trace.memory.transient_bytes += bytes;
    }
    forensics.rhi_trace.memory.resident_bytes += bytes;
  }
  forensics.rhi_trace.memory.budget_bytes = forensics.rhi_trace.memory.resident_bytes;
}

void updateCapturePayload(aster::FrameDebugCapture &capture, const std::uint32_t width,
                          const std::uint32_t height,
                          const std::vector<std::uint8_t> &rgba8) {
  capture.width = width;
  capture.height = height;
  capture.row_stride_bytes = width * 4u;
  capture.rgba8 = rgba8;
  capture.content_hash = hashDebugBytes(capture.rgba8);
  capture.available = !capture.rgba8.empty() && capture.content_hash != 0u;
}

void appendSoftwareCapturePayloads(const SoftwareFrameResources &resources,
                                   const aster::SoftwareFrameBuffer &framebuffer,
                                   aster::FrameForensics &forensics) {
  std::vector<std::uint8_t> final_rgba(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  for (aster::FrameDebugCapture &capture : forensics.captures) {
    if (capture.resource == aster::RenderGraphResource::ShadowAtlas && resources.shadow_ready) {
      updateCapturePayload(capture, resources.shadow_atlas_size, resources.shadow_atlas_size,
                           resources.shadow_rgba8);
    } else if (capture.resource == aster::RenderGraphResource::VolumetricFog &&
               resources.fog_ready) {
      updateCapturePayload(capture, resources.fog_width, resources.fog_height,
                           resources.fog_rgba8);
    } else if (capture.resource == aster::RenderGraphResource::ReflectionProbes &&
               resources.reflection_ready) {
      updateCapturePayload(capture, resources.reflection_width, resources.reflection_height,
                           resources.reflection_rgba8);
    } else if ((capture.pass == aster::RenderGraphPass::UiComposite &&
                capture.resource == aster::RenderGraphResource::SceneColor) ||
               capture.resource == aster::RenderGraphResource::CaptureReadback) {
      updateCapturePayload(capture, static_cast<std::uint32_t>(std::max(framebuffer.width(), 0)),
                           static_cast<std::uint32_t>(std::max(framebuffer.height(), 0)),
                           final_rgba);
    }
  }
}

void appendMaterialBindingTraces(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                                 const aster::MaterialResourceLibrary *library,
                                 const aster::RenderBackendCapabilities &capabilities,
                                 std::vector<aster::MaterialBindingTrace> &traces) {
  const std::uint64_t layout_hash = textureDescriptorLayoutHash();
  for (const aster::FrameRenderDrawGroup &group : plan.groups) {
    for (std::size_t i = 0u; i < group.instance_count; ++i) {
      const std::size_t plan_index = group.first_instance + i;
      if (plan_index >= plan.instances.size()) {
        break;
      }
      const aster::FrameRenderInstance &instance = plan.instances[plan_index];
      if (instance.object_index >= scene.objects().size()) {
        continue;
      }
      const aster::RenderObject &object = scene.objects()[instance.object_index];
      const aster::MaterialRuntimeResource *runtime_material =
          library == nullptr
              ? nullptr
              : library->findForMaterialIds(object.material_asset_id, object.material.asset_id);
      for (const std::string_view role : aster::materialRuntimeTextureRoles()) {
        const aster::RuntimeTexture *texture =
            runtime_material == nullptr ? nullptr : runtime_material->texture_set.find(role);
        traces.push_back({.object_name = object.name,
                          .material_asset_id = object.material_asset_id.empty()
                                                   ? object.material.asset_id
                                                   : object.material_asset_id,
                          .role = std::string(role),
                          .valid = texture != nullptr && texture->valid,
                          .fallback = texture == nullptr || texture->fallback,
                          .bound = capabilities.supports_texture_sampling && texture != nullptr &&
                                   texture->valid,
                          .width = texture == nullptr ? 0u : texture->width,
                          .height = texture == nullptr ? 0u : texture->height,
                          .mip_count = texture == nullptr
                                           ? 0u
                                           : static_cast<std::uint32_t>(texture->mips.size()),
                          .descriptor_layout_hash = layout_hash});
      }
    }
  }
}

std::uint32_t objectClusterIndex(const aster::Vec3 position, const aster::OrbitCamera &camera,
                                 const int framebuffer_width, const int framebuffer_height,
                                 const aster::ClusteredLightPolicy &policy) {
  const std::uint32_t cluster_count_x = std::max(policy.cluster_count_x, 1u);
  const std::uint32_t cluster_count_y = std::max(policy.cluster_count_y, 1u);
  const std::uint32_t cluster_count_z = std::max(policy.cluster_count_z, 1u);
  const float aspect = static_cast<float>(std::max(framebuffer_width, 1)) /
                       static_cast<float>(std::max(framebuffer_height, 1));
  const aster::Mat4 view_projection = camera.viewProjectionMatrix(aspect).value;
  const aster::Vec4 clip =
      view_projection * aster::Vec4{position.x, position.y, position.z, 1.0f};
  const float inv_w = std::abs(clip.w) > 0.000001f ? 1.0f / clip.w : 1.0f;
  const float ndc_x = std::clamp(clip.x * inv_w, -1.0f, 1.0f);
  const float ndc_y = std::clamp(clip.y * inv_w, -1.0f, 1.0f);
  const aster::Vec3 eye = camera.position();
  const aster::Vec3 forward = aster::normalizeOr(camera.target - eye, {0.0f, 0.0f, -1.0f});
  const float near_plane = std::max(camera.near_plane, 0.001f);
  const float far_plane = std::max(camera.far_plane, near_plane + 0.001f);
  const float depth = std::clamp(aster::dot(position - eye, forward), near_plane, far_plane);
  const float depth_t = (depth - near_plane) / std::max(far_plane - near_plane, 0.001f);
  const std::uint32_t x =
      std::min(static_cast<std::uint32_t>(((ndc_x * 0.5f) + 0.5f) * cluster_count_x),
               cluster_count_x - 1u);
  const std::uint32_t y =
      std::min(static_cast<std::uint32_t>((1.0f - ((ndc_y * 0.5f) + 0.5f)) *
                                          cluster_count_y),
               cluster_count_y - 1u);
  const std::uint32_t z =
      std::min(static_cast<std::uint32_t>(depth_t * cluster_count_z), cluster_count_z - 1u);
  return (z * cluster_count_y + y) * cluster_count_x + x;
}

void appendObjectDebuggerTraces(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                                const aster::OrbitCamera &camera,
                                const aster::RendererSettings &settings,
                                const int framebuffer_width, const int framebuffer_height,
                                aster::FrameForensics &forensics) {
  std::vector<bool> visible(scene.objects().size(), false);
  std::vector<float> opacity(scene.objects().size(), 0.0f);
  std::vector<aster::RenderGraphPass> pass(scene.objects().size(), aster::RenderGraphPass::Opaque);
  for (const aster::FrameRenderDrawGroup &group : plan.groups) {
    for (std::size_t i = 0u; i < group.instance_count; ++i) {
      const std::size_t instance_index = group.first_instance + i;
      if (instance_index >= plan.instances.size()) {
        continue;
      }
      const aster::FrameRenderInstance &instance = plan.instances[instance_index];
      if (instance.object_index >= scene.objects().size()) {
        continue;
      }
      visible[instance.object_index] = true;
      opacity[instance.object_index] = instance.opacity;
      pass[instance.object_index] = group.pass == aster::FrameRenderPass::Transparent
                                        ? aster::RenderGraphPass::Transparent
                                        : aster::RenderGraphPass::Opaque;
    }
  }

  const aster::Vec3 camera_position = camera.position();
  for (std::size_t object_index = 0u; object_index < scene.objects().size(); ++object_index) {
    const aster::RenderObject &object = scene.objects()[object_index];
    std::string reason = "visible";
    if (!visible[object_index]) {
      if (object.custom_mesh != nullptr &&
          (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty())) {
        reason = "empty-custom-mesh";
      } else if (object.lod.max_distance > 0.0f &&
                 aster::length(object.transform.position - camera_position) >
                     object.lod.max_distance) {
        reason = "lod-max-distance";
      } else if (object.lod.min_projected_radius > 0.0f) {
        reason = "lod-projected-radius-or-frustum";
      } else {
        reason = "planner-cull-or-frustum";
      }
    }
    forensics.mesh_visibility.push_back(
        {.object_name = objectDiagnosticLabel(object, object_index),
         .object_index = object_index,
         .visible = visible[object_index],
         .pass = pass[object_index],
         .opacity = visible[object_index] ? opacity[object_index] : 0.0f,
         .reason = reason});

    if (settings.clustered_lighting.enabled &&
        settings.clustered_lighting.mode != aster::ClusteredLightCullingMode::Disabled &&
        framebuffer_width > 0 && framebuffer_height > 0 && visible[object_index]) {
      forensics.object_clusters.push_back(
          {.object_name = objectDiagnosticLabel(object, object_index),
           .object_index = object_index,
           .cluster_index = objectClusterIndex(object.transform.position, camera,
                                               framebuffer_width, framebuffer_height,
                                               settings.clustered_lighting),
           .visible = true});
    }
  }
}

std::string shaderVariantTagFor(const Material &material) {
  if (!material.asset_id.empty()) {
    return material.asset_id;
  }
  if (material.shader_variant_key != 0u) {
    return "shader:" + std::to_string(material.shader_variant_key);
  }
  return {};
}

MaterialFrameSummary analyzeMaterialFrame(
    const Scene &scene, const FrameRenderPlan &plan, const RenderBackendCapabilities &capabilities,
    std::unordered_map<std::uint64_t, MaterialPermutationArtifact> &artifact_cache,
    const std::vector<std::size_t> &previous_transparent_order) {
  MaterialFrameSummary summary;
  std::unordered_set<std::uint64_t> unique_permutations;
  std::unordered_set<std::uint64_t> reported_fallbacks;
  std::string previous_pipeline;
  bool has_previous_pipeline = false;

  for (const FrameRenderDrawGroup &group : plan.groups) {
    if (group.instance_count == 0u || group.first_instance >= plan.instances.size()) {
      continue;
    }
    const FrameRenderInstance &first_instance = plan.instances[group.first_instance];
    if (first_instance.object_index >= scene.objects().size()) {
      continue;
    }
    const RenderObject &first_object = scene.objects()[first_instance.object_index];
    const bool has_texture_dependencies =
        !first_object.material.asset_id.empty() || first_object.material.shader_variant_key != 0u;
    const CompiledMaterial compiled =
        compileMaterialForRendering(first_object.material, has_texture_dependencies,
                                    first_object.material.asset_id);
    if (!has_previous_pipeline || compiled.pipeline_tag != previous_pipeline) {
      if (has_previous_pipeline) {
        ++summary.pipeline_switches;
      }
      previous_pipeline = compiled.pipeline_tag;
      has_previous_pipeline = true;
    }

    for (std::size_t i = 0; i < group.instance_count; ++i) {
      const std::size_t plan_index = group.first_instance + i;
      if (plan_index >= plan.instances.size()) {
        break;
      }
      const FrameRenderInstance &instance = plan.instances[plan_index];
      if (instance.object_index >= scene.objects().size()) {
        continue;
      }
      if (group.pass == FrameRenderPass::Transparent) {
        summary.transparent_order.push_back(instance.object_index);
      }
      const RenderObject &object = scene.objects()[instance.object_index];
      const bool object_has_texture_dependencies =
          !object.material.asset_id.empty() || object.material.shader_variant_key != 0u;
      const CompiledMaterial object_compiled =
          compileMaterialForRendering(object.material, object_has_texture_dependencies,
                                      object.material.asset_id);
      unique_permutations.insert(object_compiled.permutation_key);
      const bool shader_variant =
          (object_compiled.permutation_flags &
           materialPermutationFlagBit(MaterialPermutationFlag::ShaderVariant)) != 0u;
      const bool fallback = shader_variant && !capabilities.supports_shader_materials;
      if (fallback && reported_fallbacks.insert(object_compiled.permutation_key).second) {
        summary.events.push_back(
            {.kind = FrameDiagnosticKind::MaterialVariantFallback,
             .severity = FrameDiagnosticSeverity::Warning,
             .pass = group.pass == FrameRenderPass::Transparent ? "transparent" : "opaque",
             .label = object.name.empty() ? object.material.asset_id : object.name,
             .message = "Backend does not support shader materials for this material variant.",
             .value = object_compiled.permutation_key});
      }
      const MaterialPermutationArtifact artifact = materialPermutationArtifactFor(
          object_compiled, capabilities.name, shaderVariantTagFor(object.material),
          fallback ? "backend-missing-shader-materials" : "");
      if (artifact_cache.contains(artifact.permutation_key)) {
        ++summary.material_variant_cache_hits;
      } else {
        artifact_cache.emplace(artifact.permutation_key, artifact);
        ++summary.material_variant_cache_misses;
      }
    }
  }

  summary.material_permutations = unique_permutations.size();
  if (!previous_transparent_order.empty() &&
      previous_transparent_order != summary.transparent_order) {
    summary.events.push_back({.kind = FrameDiagnosticKind::TranslucentSortChanged,
                              .severity = FrameDiagnosticSeverity::Info,
                              .pass = "transparent",
                              .label = "transparent-order",
                              .message = "Transparent draw order changed from the previous frame.",
                              .value = summary.transparent_order.size()});
  }
  return summary;
}

} // namespace

const CpuMesh &RenderDevice::meshForPrimitive(const MeshPrimitive primitive) const {
  switch (primitive) {
  case MeshPrimitive::Box:
    return box_;
  case MeshPrimitive::Sphere:
    return sphere_;
  case MeshPrimitive::Plane:
    return plane_;
  case MeshPrimitive::Rock:
    return rock_;
  case MeshPrimitive::Crystal:
    return crystal_;
  case MeshPrimitive::RuinBlock:
    return ruin_block_;
  case MeshPrimitive::Pillar:
    return pillar_;
  }
  return sphere_;
}

const CpuMesh &RenderDevice::meshForObject(const RenderObject &object) {
  if (object.custom_mesh == nullptr) {
    return meshForPrimitive(object.primitive);
  }

  if (object.dynamic_mesh.valid()) {
    auto it = custom_mesh_resource_cache_.find(object.dynamic_mesh);
    if (it == custom_mesh_resource_cache_.end()) {
      it = custom_mesh_resource_cache_
               .emplace(object.dynamic_mesh,
                        prepareMeshForRendering(*object.custom_mesh, renderMeshOptions()))
               .first;
    }
    return it->second;
  }

  const CpuMesh *source = object.custom_mesh.get();
  auto it = custom_mesh_cache_.find(source);
  if (it == custom_mesh_cache_.end()) {
    it = custom_mesh_cache_.emplace(source, prepareMeshForRendering(*source, renderMeshOptions()))
             .first;
  }
  return it->second;
}

void RenderDevice::syncDynamicMeshes(const Scene &scene, const bool immediate_eviction) {
  ASTER_PROFILE_SCOPE("RenderDevice::syncDynamicMeshes");
  if (immediate_eviction) {
    for (const auto &[mesh, handle] : custom_mesh_resource_handles_) {
      (void)mesh;
      resource_registry_.destroy(handle);
    }
    for (const auto &[resource, handle] : dynamic_mesh_resource_handles_) {
      (void)resource;
      resource_registry_.destroy(handle);
    }
    custom_mesh_cache_.clear();
    custom_mesh_last_seen_.clear();
    custom_mesh_resource_cache_.clear();
    custom_mesh_resource_last_seen_.clear();
    custom_mesh_resource_handles_.clear();
    dynamic_mesh_resource_handles_.clear();
    resource_registry_.clearRetired();
  }

  ++mesh_cache_frame_;
  std::unordered_set<const CpuMesh *> live_meshes;
  std::unordered_set<DynamicMeshResourceKey, DynamicMeshResourceKeyHash> live_resources;
  live_meshes.reserve(scene.objects().size());
  live_resources.reserve(scene.objects().size());
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh == nullptr) {
      continue;
    }
    if (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty()) {
      continue;
    }
    if (object.dynamic_mesh.valid()) {
      live_resources.insert(object.dynamic_mesh);
      custom_mesh_resource_last_seen_[object.dynamic_mesh] = mesh_cache_frame_;
    } else {
      live_meshes.insert(object.custom_mesh.get());
      custom_mesh_last_seen_[object.custom_mesh.get()] = mesh_cache_frame_;
    }
  }

  constexpr std::uint64_t kRetiredMeshGraceFrames = 3u;
  for (auto it = custom_mesh_cache_.begin(); it != custom_mesh_cache_.end();) {
    const auto seen = custom_mesh_last_seen_.find(it->first);
    const bool live = live_meshes.contains(it->first);
    const bool expired =
        immediate_eviction || seen == custom_mesh_last_seen_.end() ||
        mesh_cache_frame_ > seen->second + kRetiredMeshGraceFrames;
    if (live || !expired) {
      ++it;
    } else {
      if (const auto handle = custom_mesh_resource_handles_.find(it->first);
          handle != custom_mesh_resource_handles_.end()) {
        resource_registry_.destroy(handle->second);
        custom_mesh_resource_handles_.erase(handle);
      }
      custom_mesh_last_seen_.erase(it->first);
      it = custom_mesh_cache_.erase(it);
    }
  }

  for (auto it = custom_mesh_resource_cache_.begin(); it != custom_mesh_resource_cache_.end();) {
    const auto seen = custom_mesh_resource_last_seen_.find(it->first);
    const bool live = live_resources.contains(it->first);
    const bool expired =
        immediate_eviction || seen == custom_mesh_resource_last_seen_.end() ||
        mesh_cache_frame_ > seen->second + kRetiredMeshGraceFrames;
    if (live || !expired) {
      ++it;
    } else {
      if (const auto handle = dynamic_mesh_resource_handles_.find(it->first);
          handle != dynamic_mesh_resource_handles_.end()) {
        resource_registry_.destroy(handle->second);
        dynamic_mesh_resource_handles_.erase(handle);
      }
      custom_mesh_resource_last_seen_.erase(it->first);
      it = custom_mesh_resource_cache_.erase(it);
    }
  }

  for (const CpuMesh *mesh : live_meshes) {
    if (!custom_mesh_cache_.contains(mesh)) {
      custom_mesh_cache_.emplace(mesh, prepareMeshForRendering(*mesh, renderMeshOptions()));
    }
    if (!custom_mesh_resource_handles_.contains(mesh)) {
      const auto &prepared = custom_mesh_cache_.at(mesh);
      custom_mesh_resource_handles_.emplace(
          mesh, resource_registry_.createBuffer(
                    {.byte_size = prepared.vertices.size() * sizeof(Vertex) +
                                  prepared.indices.size() * sizeof(std::uint32_t),
                     .usage = rhi::bufferUsageBit(rhi::BufferUsage::Vertex) |
                              rhi::bufferUsageBit(rhi::BufferUsage::Index),
                     .debug_label = "custom-mesh"}));
    }
  }
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh == nullptr || object.custom_mesh->vertices.empty() ||
        object.custom_mesh->indices.empty()) {
      continue;
    }
    if (object.dynamic_mesh.valid() &&
        !custom_mesh_resource_cache_.contains(object.dynamic_mesh)) {
      custom_mesh_resource_cache_.emplace(
          object.dynamic_mesh, prepareMeshForRendering(*object.custom_mesh, renderMeshOptions()));
    }
    if (object.dynamic_mesh.valid() &&
        !dynamic_mesh_resource_handles_.contains(object.dynamic_mesh)) {
      const auto &prepared = custom_mesh_resource_cache_.at(object.dynamic_mesh);
      dynamic_mesh_resource_handles_.emplace(
          object.dynamic_mesh,
          resource_registry_.createBuffer(
              {.byte_size = prepared.vertices.size() * sizeof(Vertex) +
                            prepared.indices.size() * sizeof(std::uint32_t),
               .usage = rhi::bufferUsageBit(rhi::BufferUsage::Vertex) |
                        rhi::bufferUsageBit(rhi::BufferUsage::Index),
               .debug_label = "dynamic-mesh"}));
    }
  }
  resource_registry_.clearRetired();
}

void RenderDevice::initialize() {
  ASTER_PROFILE_SCOPE("RenderDevice::initialize");
  const auto graph_start = std::chrono::steady_clock::now();
  frame_graph_ = makeDefaultFrameGraph(true, true);
  compiled_frame_graph_ = framegraph::compileFrameGraph(frame_graph_);
  render_graph_ = compiled_frame_graph_;
  const auto graph_end = std::chrono::steady_clock::now();
  graph_compile_seconds_ = std::chrono::duration<double>(graph_end - graph_start).count();
  const MeshProcessOptions mesh_options = renderMeshOptions();
  box_ = prepareMeshForRendering(makeBox(), mesh_options);
  sphere_ =
      prepareMeshForRendering(makeUvSphere(kSphereSegments, kSphereRings, 1.0f), mesh_options);
  plane_ = prepareMeshForRendering(makePlane(kPlaneSize), mesh_options);
  contact_shadow_plane_ = prepareMeshForRendering(makePlane(kContactShadowPlaneSize), mesh_options);
  rock_ = prepareMeshForRendering(makeRock(kRockSegments, kRockRings, 1.0f), mesh_options);
  crystal_ = prepareMeshForRendering(makeCrystal(kCrystalSides, 1.0f, 1.8f), mesh_options);
  ruin_block_ = prepareMeshForRendering(makeRuinBlock(), mesh_options);
  pillar_ = prepareMeshForRendering(makePillar(kPillarSides, 1.0f, 1.0f), mesh_options);

  const char *force_null = std::getenv("ASTER_FORCE_NULL_RENDERER");
  const char *force_software = std::getenv("ASTER_FORCE_SOFTWARE_RENDERER");
  if (force_null != nullptr && *force_null != '\0') {
    native_backend_ = createNullRenderBackend();
    if (native_backend_ != nullptr && !native_backend_->initialize()) {
      native_backend_.reset();
    }
  } else if (force_software == nullptr || *force_software == '\0') {
    native_backend_ = createNativeRenderBackend();
    if (native_backend_ != nullptr && !native_backend_->initialize()) {
      native_backend_.reset();
    }
  }
}

void RenderDevice::setMaterialResourceLibrary(
    std::shared_ptr<const MaterialResourceLibrary> library) {
  material_library_ = std::move(library);
}

void RenderDevice::prepareScene(const Scene &scene) {
  ASTER_PROFILE_SCOPE("RenderDevice::prepareScene");
  render_scene_.rebuild(scene);
  syncDynamicMeshes(scene, true);
}

FrameStats RenderDevice::render(const Scene &scene, const OrbitCamera &camera,
                                const RendererSettings &settings, const int framebuffer_width,
                                const int framebuffer_height, const double frame_seconds) {
  ASTER_PROFILE_SCOPE("RenderDevice::render");
  last_forensics_ = {};
  FrameStats stats;
  stats.frame_seconds = frame_seconds;
  stats.framebuffer_width = framebuffer_width;
  stats.framebuffer_height = framebuffer_height;
  stats.graph_passes = render_graph_.passes.size();
  stats.graph_resources = render_graph_.resources.size();
  stats.graph_barriers = render_graph_.barriers.size();
  stats.graph_transient_resources = render_graph_.transient_resource_count;
  const rhi::ResourceRegistryStats initial_registry_stats = resource_registry_.stats();
  stats.registry_live_resources = initial_registry_stats.live_resources;
  stats.registry_retired_resources = initial_registry_stats.retired_resources;

  if (framebuffer_width <= 0 || framebuffer_height <= 0) {
    return stats;
  }

  syncDynamicMeshes(scene, false);
  render_scene_.rebuild(scene);
  const FrameRenderPlan plan =
      buildFrameRenderPlan(render_scene_, camera, settings.line_of_sight_fade, framebuffer_width,
                           framebuffer_height);
  const RenderBackendCapabilities active_capabilities =
      native_backend_ != nullptr ? native_backend_->capabilities() : softwareCapabilities();
  const ClusteredLightGrid clustered_lights =
      buildClusteredLightGrid(settings.light_rig, camera, framebuffer_width, framebuffer_height,
                              settings.clustered_lighting);
  last_forensics_.evidence = {.render_ir_hash = render_scene_.ir().content_hash,
                              .visibility_plan_hash = framePlanEvidenceHash(plan),
                              .backend_kind =
                                  static_cast<std::uint32_t>(active_capabilities.kind),
                              .draw_signature_count =
                                  static_cast<std::uint32_t>(plan.groups.size())};
  MaterialFrameSummary material_summary =
      analyzeMaterialFrame(scene, plan, active_capabilities, material_artifact_cache_,
                           previous_transparent_order_);
  previous_transparent_order_ = material_summary.transparent_order;
  appendFrameGraphForensics(render_graph_, active_capabilities, framebuffer_width,
                            framebuffer_height, last_forensics_);
  appendMaterialBindingTraces(scene, plan, material_library_.get(), active_capabilities,
                              last_forensics_.material_bindings);
  appendObjectDebuggerTraces(scene, plan, camera, settings, framebuffer_width, framebuffer_height,
                             last_forensics_);
  last_forensics_.events.insert(last_forensics_.events.end(),
                                std::make_move_iterator(material_summary.events.begin()),
                                std::make_move_iterator(material_summary.events.end()));
  appendRenderMathContractDiagnostics(scene, last_forensics_.events);
  appendMathDiagnosticsToFrame(last_forensics_.events);
  stats.visible_objects = plan.diagnostics.visible_objects;
  stats.culled_objects = plan.diagnostics.culled_objects;
  stats.instance_groups = plan.diagnostics.instance_groups;
  stats.lod_culled_objects = plan.diagnostics.lod_culled_objects;
  stats.visibility_hint_objects = plan.diagnostics.visibility_hint_objects;
  stats.dynamic_mesh_objects = plan.diagnostics.dynamic_mesh_objects;
  stats.dynamic_mesh_cache_entries =
      custom_mesh_cache_.size() + custom_mesh_resource_cache_.size();
  stats.pipeline_switches = material_summary.pipeline_switches;
  stats.material_permutations = material_summary.material_permutations;
  stats.material_variant_cache_hits = material_summary.material_variant_cache_hits;
  stats.material_variant_cache_misses = material_summary.material_variant_cache_misses;
  stats.active_point_lights =
      settings.clustered_lighting.enabled
          ? clustered_lights.visible_lights.size()
          : selectRenderLights(settings.light_rig, camera.target, settings.light_policy).size();
  stats.clustered_light_clusters =
      static_cast<std::size_t>(clustered_lights.cluster_count_x) * clustered_lights.cluster_count_y *
      clustered_lights.cluster_count_z;
  stats.clustered_light_assignments = clustered_lights.assignments.size();
  if (clustered_lights.fallback_used || clustered_lights.overflowed) {
    last_forensics_.events.push_back(
        {.kind = FrameDiagnosticKind::ClusteredLightingFallback,
         .severity = FrameDiagnosticSeverity::Info,
         .pass = "light-cull",
         .label = "clustered-forward-v1",
         .message = clustered_lights.overflowed
                        ? "Clustered lighting exceeded the per-cluster light budget."
                        : "Clustered lighting clamped visible lights to the backend contract.",
         .value = clustered_lights.assignments.size()});
  }
  stats.rust_plan_seconds = plan.diagnostics.rust_plan_seconds;
  stats.graph_compile_seconds = graph_compile_seconds_;
  stats.backend_kind_value =
      static_cast<std::uint32_t>(backendCapabilities().kind);

  SoftwareFrameBuffer &framebuffer = activeFrameBuffer();
  if (native_backend_ != nullptr) {
    PreparedRenderMeshes meshes{
        .box = &box_,
        .sphere = &sphere_,
        .plane = &plane_,
        .contact_shadow_plane = &contact_shadow_plane_,
        .rock = &rock_,
        .crystal = &crystal_,
        .ruin_block = &ruin_block_,
        .pillar = &pillar_,
        .custom_meshes = &custom_mesh_cache_,
        .custom_mesh_resources = &custom_mesh_resource_cache_,
    };
    framebuffer.resize(framebuffer_width, framebuffer_height);
    framebuffer.clearTransparent();
    FrameStats native_stats =
        native_backend_->render(scene, plan, camera, settings, render_graph_, meshes,
                                framebuffer_width, framebuffer_height, frame_seconds,
                                material_library_.get(), &last_forensics_);
    native_stats.visible_objects = plan.diagnostics.visible_objects;
    native_stats.culled_objects = plan.diagnostics.culled_objects;
    native_stats.instance_groups = plan.diagnostics.instance_groups;
    native_stats.lod_culled_objects = plan.diagnostics.lod_culled_objects;
    native_stats.visibility_hint_objects = plan.diagnostics.visibility_hint_objects;
    native_stats.dynamic_mesh_objects = plan.diagnostics.dynamic_mesh_objects;
    native_stats.dynamic_mesh_cache_entries =
        custom_mesh_cache_.size() + custom_mesh_resource_cache_.size();
    native_stats.pipeline_switches = material_summary.pipeline_switches;
    native_stats.material_permutations = material_summary.material_permutations;
    native_stats.material_variant_cache_hits = material_summary.material_variant_cache_hits;
    native_stats.material_variant_cache_misses = material_summary.material_variant_cache_misses;
    native_stats.active_point_lights = stats.active_point_lights;
    native_stats.clustered_light_clusters = stats.clustered_light_clusters;
    native_stats.clustered_light_assignments = stats.clustered_light_assignments;
    native_stats.rust_plan_seconds = plan.diagnostics.rust_plan_seconds;
    native_stats.graph_passes = render_graph_.passes.size();
    native_stats.graph_resources = render_graph_.resources.size();
    native_stats.graph_barriers = render_graph_.barriers.size();
    native_stats.graph_transient_resources = render_graph_.transient_resource_count;
    const rhi::ResourceRegistryStats registry_stats = resource_registry_.stats();
    native_stats.registry_live_resources = registry_stats.live_resources;
    native_stats.registry_retired_resources = registry_stats.retired_resources;
    native_stats.resource_lifetime_warnings = registry_stats.retired_resources;
    native_stats.backend_feature_mask = native_backend_->capabilities().graph_resource_mask;
    native_stats.backend_kind_value =
        static_cast<std::uint32_t>(native_backend_->capabilities().kind);
    native_stats.graph_compile_seconds = graph_compile_seconds_;
    return native_stats;
  }

  clearNativeFrame();
  framebuffer.resize(framebuffer_width, framebuffer_height);
  framebuffer.clear(settings.pipeline.clear_color);
  SoftwareFrameResources software_resources;
  software_resources.frame_width = static_cast<std::uint32_t>(framebuffer_width);
  software_resources.frame_height = static_cast<std::uint32_t>(framebuffer_height);

  const auto encode_start = std::chrono::steady_clock::now();

  const auto draw_object = [&](const RenderObject &object, const float opacity) {
    if (object.custom_mesh != nullptr &&
        (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty())) {
      return;
    }
    const CpuMesh &mesh = isContactShadowUtility(object) && object.custom_mesh == nullptr
                              ? contact_shadow_plane_
                              : meshForObject(object);
    drawMesh(framebuffer, mesh, object, camera, settings, frame_seconds, opacity, stats.draw_calls,
             material_library_.get(), &software_resources);
  };

  const auto draw_planned_group = [&](const FrameRenderDrawGroup &group) {
    for (std::size_t i = 0; i < group.instance_count; ++i) {
      const FrameRenderInstance &instance = plan.instances[group.first_instance + i];
      if (instance.object_index < scene.objects().size()) {
        draw_object(scene.objects()[instance.object_index], instance.opacity);
      }
    }
  };

  const auto draw_contact_shadows = [&] {
    for (const FrameRenderInstance &instance : plan.instances) {
      if (instance.object_index >= scene.objects().size()) {
        continue;
      }
      const RenderObject &object = scene.objects()[instance.object_index];
      if (canCastContactShadow(object, settings.grounding)) {
        const RenderObject shadow = contactShadowObjectFor(object, settings.grounding);
        draw_object(shadow, shadow.material.opacity);
      }
    }
  };

  (void)executeFixedRenderGraph(render_graph_, [&](const RenderGraphPassInvocation &invocation) {
    const auto pass_start = std::chrono::steady_clock::now();
    const std::size_t draws_before = stats.draw_calls;
    switch (invocation.semantic) {
    case RenderGraphPass::LightCull:
      break;
    case RenderGraphPass::ShadowAtlas:
      buildSoftwareShadowAtlas(scene, camera, settings, software_resources);
      break;
    case RenderGraphPass::SceneLighting:
      break;
    case RenderGraphPass::VolumetricFog:
      buildSoftwareVolumetricFog(camera, settings, software_resources);
      break;
    case RenderGraphPass::ReflectionProbe:
      buildSoftwareReflectionProbeAtlas(scene, settings, software_resources);
      break;
    case RenderGraphPass::Opaque:
      for (const FrameRenderDrawGroup &group : plan.groups) {
        if (group.pass == FrameRenderPass::Opaque) {
          draw_planned_group(group);
        }
      }
      break;
    case RenderGraphPass::ContactShadow:
      draw_contact_shadows();
      break;
    case RenderGraphPass::Transparent:
      for (const FrameRenderDrawGroup &group : plan.groups) {
        if (group.pass == FrameRenderPass::Transparent) {
          draw_planned_group(group);
        }
      }
      break;
    case RenderGraphPass::SceneColorDepth:
    case RenderGraphPass::UiComposite:
    case RenderGraphPass::Capture:
      break;
    }
    const auto pass_end = std::chrono::steady_clock::now();
    last_forensics_.passes.push_back(
        {.pass = invocation.semantic,
         .name = invocation.pass == nullptr ? std::string(renderGraphPassName(invocation.semantic))
                                            : invocation.pass->name,
         .draw_calls = stats.draw_calls - draws_before,
         .pipeline_switches = invocation.semantic == RenderGraphPass::Opaque
                                  ? material_summary.pipeline_switches
                                  : 0u,
         .material_permutations =
             invocation.semantic == RenderGraphPass::Opaque
                 ? material_summary.material_permutations
                 : (invocation.semantic == RenderGraphPass::LightCull
                        ? clustered_lights.assignments.size()
                        : 0u),
         .encode_seconds = std::chrono::duration<double>(pass_end - pass_start).count()});
  });
  applySoftwarePostProcess(framebuffer, settings);
  appendSoftwareCapturePayloads(software_resources, framebuffer, last_forensics_);
  const auto encode_end = std::chrono::steady_clock::now();
  stats.render_encode_seconds = std::chrono::duration<double>(encode_end - encode_start).count();
  stats.backend_feature_mask = softwareCapabilities().graph_resource_mask;
  stats.backend_kind_value = static_cast<std::uint32_t>(RenderBackendKind::SoftwareReference);
  const rhi::ResourceRegistryStats registry_stats = resource_registry_.stats();
  stats.registry_live_resources = registry_stats.live_resources;
  stats.registry_retired_resources = registry_stats.retired_resources;
  stats.resource_lifetime_warnings = registry_stats.retired_resources;
  stats.graph_compile_seconds = graph_compile_seconds_;

  return stats;
}

const char *RenderDevice::backendName() const {
  if (native_backend_ != nullptr) {
    return native_backend_->backendName();
  }
  return "Aster Learning Software Rasterizer";
}

RenderBackendCapabilities RenderDevice::backendCapabilities() const {
  if (native_backend_ != nullptr) {
    return native_backend_->capabilities();
  }
  return softwareCapabilities();
}

const FixedRenderGraph &RenderDevice::renderGraph() const {
  return render_graph_;
}

const FrameForensics &RenderDevice::lastFrameForensics() const {
  return last_forensics_;
}

const std::shared_ptr<const MaterialResourceLibrary> &RenderDevice::materialResourceLibrary()
    const noexcept {
  return material_library_;
}

} // namespace aster
