// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/render_device.hpp"

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/core/profiler.hpp"
#include "aster/math/color.hpp"
#include "aster/material/procedural_surface.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/render/render_conformance.hpp"
#include "aster/render/render_graph_executor.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/framegraph/transient_resource_allocator.hpp"
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
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

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
  std::uint32_t surface_width = 0u;
  std::uint32_t surface_height = 0u;
  std::uint32_t occlusion_width = 0u;
  std::uint32_t occlusion_height = 0u;
  std::uint32_t shadow_atlas_size = 0u;
  std::uint32_t fog_width = 0u;
  std::uint32_t fog_height = 0u;
  std::uint32_t reflection_width = 0u;
  std::uint32_t reflection_height = 0u;
  std::vector<SoftwareShadowCascade> cascades;
  std::vector<std::uint8_t> surface_attributes_rgba8;
  std::vector<std::uint8_t> surface_occlusion_rgba8;
  std::vector<float> shadow_depths;
  std::vector<std::uint8_t> shadow_rgba8;
  std::vector<float> fog_factors;
  std::vector<std::uint8_t> fog_rgba8;
  std::vector<SoftwareProbeSample> probes;
  std::vector<std::uint8_t> reflection_rgba8;
  bool shadow_ready = false;
  bool surface_attributes_ready = false;
  bool surface_occlusion_ready = false;
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

aster::Vec2 hammersley2d(std::uint32_t index, std::uint32_t count);
float fractValue(float value);
float valueNoise(aster::Vec3 p);
float projectedFbm(const aster::Material &material, aster::Vec3 world_position,
                   aster::Vec3 normal, float multiplier, float salt);
float ridge(float value);

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

struct SurfaceBasis {
  aster::Vec3 normal{0.0f, 1.0f, 0.0f};
  aster::Vec3 tangent{1.0f, 0.0f, 0.0f};
  aster::Vec3 bitangent{0.0f, 0.0f, 1.0f};
};

SurfaceBasis makeSurfaceBasis(aster::Vec3 normal) {
  normal = aster::normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  aster::Vec3 tangent =
      std::abs(normal.y) < 0.92f ? aster::cross({0.0f, 1.0f, 0.0f}, normal)
                                 : aster::cross({1.0f, 0.0f, 0.0f}, normal);
  tangent = aster::normalizeOr(tangent, {1.0f, 0.0f, 0.0f});
  const aster::Vec3 bitangent = aster::normalizeOr(aster::cross(normal, tangent),
                                                   {0.0f, 0.0f, 1.0f});
  return {.normal = normal, .tangent = tangent, .bitangent = bitangent};
}

aster::Vec3 sampleSurfaceHemisphere(const SurfaceBasis &basis, const aster::Vec2 sample) {
  const float phi = sample.x * kTau;
  const float cos_theta = std::sqrt(std::clamp(1.0f - sample.y * 0.82f, 0.0f, 1.0f));
  const float sin_theta = std::sqrt(std::clamp(1.0f - cos_theta * cos_theta, 0.0f, 1.0f));
  return aster::normalizeOr(basis.tangent * (std::cos(phi) * sin_theta) +
                                basis.bitangent * (std::sin(phi) * sin_theta) +
                                basis.normal * cos_theta,
                            basis.normal);
}

struct SurfaceOcclusionEval {
  float visibility = 1.0f;
  float cavity = 0.0f;
  float micro_shadow = 0.0f;
  aster::Vec3 bent_normal{0.0f, 1.0f, 0.0f};
};

SurfaceOcclusionEval evaluateSurfaceOcclusion(const aster::Vec3 world_position,
                                              const aster::Vec3 normal,
                                              const float material_ao,
                                              const aster::RendererSettings &settings) {
  SurfaceOcclusionEval eval;
  eval.bent_normal = aster::normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  if (!settings.occlusion.enabled || settings.occlusion.strength <= 0.0f) {
    return eval;
  }
  const float radius = std::max(settings.occlusion.radius, 0.001f);
  const float above_reference = std::max(world_position.y - settings.grounding.reference_y, 0.0f);
  const float contact = 1.0f - smoothstep(0.0f, radius, above_reference);
  const float vertical_receiver =
      (1.0f - smoothstep(0.46f, 0.94f, std::clamp(normal.y, -1.0f, 1.0f))) * 0.44f;
  const float cavity = std::clamp(1.0f - material_ao + settings.occlusion.cavity_bias, 0.0f, 1.0f);
  const float horizon = std::pow(contact, std::max(settings.occlusion.distance_falloff, 0.25f)) *
                        (0.56f + vertical_receiver);
  const SurfaceBasis basis = makeSurfaceBasis(normal);
  const std::uint32_t sample_count = std::clamp(settings.occlusion.sample_count, 4u, 20u);
  const float texel_scale =
      std::clamp(settings.surface_scale.physical_texel_density / 768.0f, 0.42f, 2.25f);
  const float macro_breakup = std::clamp(settings.surface_scale.macro_frequency_breakup, 0.0f, 2.0f);
  const float micro_breakup = std::clamp(settings.surface_scale.micro_frequency_breakup, 0.0f, 2.0f);
  const float jitter = valueNoise(world_position * 0.091f + aster::Vec3{2.7f, 5.1f, 1.3f});
  float sampled_horizon = 0.0f;
  float sampled_micro = 0.0f;
  aster::Vec3 bent_accum = basis.normal * (0.72f + material_ao * 0.18f);
  for (std::uint32_t sample_index = 0u; sample_index < sample_count; ++sample_index) {
    aster::Vec2 xi = hammersley2d(sample_index, sample_count);
    xi.x = fractValue(xi.x + jitter * 0.37f);
    xi.y = fractValue(xi.y + jitter * 0.61f);
    const aster::Vec3 direction = sampleSurfaceHemisphere(basis, xi);
    const float sample_radius = radius * (0.16f + std::sqrt(std::max(xi.y, 0.0f)) * 0.84f);
    const aster::Vec3 probe = world_position + direction * sample_radius;
    const float broad =
        valueNoise(probe * (0.42f + macro_breakup * 0.36f) +
                   aster::Vec3{11.0f, 3.7f, 19.0f});
    const float fine =
        valueNoise(probe * (1.90f + micro_breakup * 2.70f) +
                   basis.normal * (0.21f + texel_scale * 0.13f));
    const float ridged =
        ridge(valueNoise(probe * (3.8f + micro_breakup * 4.2f) +
                         aster::Vec3{0.31f, 0.73f, 1.19f}));
    const float low_angle = 1.0f - std::clamp(aster::dot(direction, basis.normal), 0.0f, 1.0f);
    const float local_cavity = smoothstep(0.56f, 0.94f, broad * 0.34f + fine * 0.34f +
                                                         ridged * 0.32f);
    const float ground_block =
        1.0f - smoothstep(0.0f, radius * 0.82f,
                          std::max(probe.y - settings.grounding.reference_y, 0.0f));
    const float occluder =
        std::max(local_cavity * (0.42f + low_angle * 0.58f),
                 ground_block * contact * (0.30f + vertical_receiver));
    const float distance_weight = 1.0f - smoothstep(radius * 0.20f, radius, sample_radius);
    sampled_horizon += occluder * distance_weight;
    sampled_micro += ridged * local_cavity;
    bent_accum = bent_accum + direction * (1.0f - occluder);
  }
  sampled_horizon /= static_cast<float>(sample_count);
  sampled_micro /= static_cast<float>(sample_count);
  const float micro = std::clamp(cavity * (0.50f + settings.occlusion.micro_shadowing) +
                                     sampled_horizon *
                                         (0.18f + settings.occlusion.thickness * 0.72f) +
                                     sampled_micro * settings.occlusion.micro_shadowing * 0.30f,
                                 0.0f, 1.0f);
  const float raw = horizon * settings.occlusion.contact_hardening + micro;
  eval.visibility = std::clamp(1.0f - raw * settings.occlusion.strength, 0.32f, 1.0f);
  eval.cavity = cavity;
  eval.micro_shadow = micro;
  eval.bent_normal = aster::normalizeOr(bent_accum, basis.normal);
  return eval;
}

float evaluateSpecularOcclusion(const float ao, const float perceptual_roughness,
                                const float n_dot_v) {
  const float clamped_ao = std::clamp(ao, 0.0f, 1.0f);
  const float roughness = std::clamp(perceptual_roughness, 0.045f, 1.0f);
  const float exponent = std::lerp(2.45f, 0.58f, roughness);
  const float visibility =
      std::pow(std::clamp(n_dot_v + clamped_ao, 0.0f, 1.0f), exponent) - 1.0f + clamped_ao;
  return std::clamp(visibility, 0.08f, 1.0f);
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

float radicalInverseVdc(std::uint32_t bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

aster::Vec2 hammersley2d(const std::uint32_t index, const std::uint32_t count) {
  return {static_cast<float>(index) / static_cast<float>(std::max(count, 1u)),
          radicalInverseVdc(index)};
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

  const aster::Vec3 base = material.base_color.value;
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
  aster::Vec3 block = material.base_color.value * (0.82f + broad * 0.22f + fine * 0.12f);
  block = mixVec(block, block * aster::Vec3{1.10f, 1.04f, 0.92f}, material.edge_wear * ridge(fine));
  return mixVec(block, material.base_color.value * 0.36f, line * 0.70f);
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
  aster::Vec3 damp = material.base_color.value * aster::Vec3{0.72f, 0.70f, 0.62f};
  aster::Vec3 mineral = material.base_color.value * aster::Vec3{1.24f, 1.13f, 0.92f};
  aster::Vec3 albedo = mixVec(damp, material.base_color.value, broad * 0.74f);
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
  aster::Vec3 coal = material.base_color.value * (0.54f + sheen * 0.32f);
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
  aster::Vec3 dark = material.base_color.value * aster::Vec3{0.64f, 0.58f, 0.50f};
  aster::Vec3 light = material.base_color.value * aster::Vec3{1.22f, 1.14f, 0.96f};
  return mixVec(dark, light, strand_mask * (0.56f + material.pattern_contrast * 0.28f));
}

aster::Vec3 biologicalIntegumentAlbedo(const aster::Material &material,
                                       const aster::Vec3 world_position,
                                       const aster::Vec3 normal, const aster::Vec2 uv) {
  const float pigment =
      projectedFbm(material, world_position + normal * 0.035f, normal, 0.36f, 451.0f);
  const float capillary =
      projectedFbm(material, world_position + aster::Vec3{0.07f, 0.13f, -0.05f}, normal, 0.82f,
                   467.0f);
  const float pore = ridge(projectedFbm(material, world_position, normal, 2.20f, 479.0f));
  const float abrasion =
      ridge(projectedFbm(material, world_position + normal * 0.09f, normal, 1.48f, 491.0f));
  const float follicle =
      0.5f + 0.5f * std::sin((world_position.z * material.pattern_scale.y +
                               world_position.x * material.pattern_scale.x * 0.32f +
                               uv.x * 7.0f) *
                                  3.35f +
                              pigment * 4.0f);
  const float follicle_mask = smoothstep(0.50f, 0.94f, follicle) *
                              smoothstep(0.16f, 0.84f, pore + material.pattern_depth);
  const float low_pelage = smoothstep(0.60f, 0.94f, normal.y) *
                           smoothstep(0.42f, 0.98f, std::abs(world_position.z) * 0.35f +
                                                       std::abs(world_position.x) * 0.20f);
  const float vascular_weight =
      smoothstep(0.48f, 0.96f, capillary) *
      saturate(0.20f + material.pattern_depth * 1.50f + material.procedural.wetness * 0.75f);
  const float pigment_gain = 0.35f + material.pattern_contrast * 0.65f +
                             material.procedural.macro_variation * 0.20f;

  const aster::Vec3 basal = material.base_color.value * aster::Vec3{0.72f, 0.63f, 0.52f};
  const aster::Vec3 melanin = material.base_color.value * aster::Vec3{0.46f, 0.38f, 0.28f};
  const aster::Vec3 warm_dermis{0.58f, 0.23f, 0.17f};
  const aster::Vec3 guard_hair = material.base_color.value * aster::Vec3{1.15f, 1.03f, 0.78f};
  const aster::Vec3 dust{0.20f, 0.18f, 0.15f};

  aster::Vec3 color = mixVec(basal, melanin, pigment * pigment_gain);
  color = mixVec(color, warm_dermis, vascular_weight * (0.32f + low_pelage * 0.28f));
  color = mixVec(color, guard_hair, follicle_mask * (0.30f + material.detail_strength * 0.16f));
  color = mixVec(color, color * aster::Vec3{0.72f, 0.68f, 0.60f},
                 smoothstep(0.74f, 0.98f, abrasion) * (0.10f + material.edge_wear * 0.38f));
  color = mixVec(color, dust, smoothstep(0.82f, 0.99f, pore) * 0.10f);
  return aster::clamp(color * (0.92f + pore * 0.10f), 0.0f, 4.0f);
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
  const aster::Vec3 shadow = material.base_color.value * aster::Vec3{0.62f, 0.66f, 0.66f};
  const aster::Vec3 silk = material.base_color.value * aster::Vec3{1.22f, 1.24f, 1.16f};
  const aster::Vec3 pearl = material.base_color.value + aster::Vec3{0.08f, 0.09f, 0.075f};
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
  const aster::Vec3 under = material.base_color.value * aster::Vec3{0.44f, 0.34f, 0.30f};
  const aster::Vec3 lacquer = material.base_color.value * aster::Vec3{1.54f, 1.04f, 0.78f};
  const aster::Vec3 warm_mark = material.base_color.value + aster::Vec3{0.070f, 0.020f, 0.010f};
  const aster::Vec3 dark_band = material.base_color.value * aster::Vec3{0.22f, 0.18f, 0.18f};
  const aster::Vec3 cool_sheen = material.base_color.value + aster::Vec3{0.042f, 0.050f, 0.060f};
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
  const aster::Vec3 root = material.base_color.value * aster::Vec3{0.50f, 0.66f, 0.38f};
  const aster::Vec3 mid = material.base_color.value * aster::Vec3{0.82f, 1.08f, 0.58f};
  const aster::Vec3 tip = material.base_color.value * aster::Vec3{1.18f, 1.30f, 0.72f};
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
  const aster::Vec3 deep = material.base_color.value * aster::Vec3{0.55f, 0.88f, 0.96f};
  const aster::Vec3 glint = material.base_color.value + aster::Vec3{0.08f, 0.20f, 0.22f};
  return mixVec(deep, glint, smoothstep(0.45f, 0.94f, wave_a * 0.64f + wave_b * 0.36f));
}

aster::Vec3 amberAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                        const aster::Vec3 normal) {
  const float streak = ridge(projectedFbm(material, world_position, normal, 0.44f, 127.0f));
  const float cloud = projectedFbm(material, world_position, normal, 1.10f, 131.0f);
  const aster::Vec3 honey = material.base_color.value * aster::Vec3{1.30f, 0.96f, 0.56f};
  const aster::Vec3 smoke = material.base_color.value * aster::Vec3{0.62f, 0.42f, 0.28f};
  return mixVec(smoke, honey, smoothstep(0.25f, 0.92f, cloud)) +
         material.emission_color.value * (0.08f + smoothstep(0.70f, 0.98f, streak) * 0.16f);
}

aster::Vec3 corrodedMetalAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                                const aster::Vec3 normal, const aster::Vec2 uv) {
  const aster::AsterPipeSurfaceSignals signals =
      aster::sampleAsterPipeSurface({.position = world_position,
                                     .normal = normal,
                                     .uv = uv,
                                     .detail_scale = material.detail_scale},
                                    material.procedural, material.edge_wear,
                                    material.pattern_depth);
  const aster::Vec3 n = aster::normalize(normal);
  const float lower = 1.0f - smoothstep(-0.55f, 0.42f, n.y);
  const float rim = smoothstep(2.44f, 2.60f, std::abs(world_position.x));
  const float weld = std::max(1.0f - smoothstep(0.035f, 0.34f, std::abs(world_position.x + 1.55f)),
                              1.0f - smoothstep(0.035f, 0.34f, std::abs(world_position.x - 1.42f)));
  const aster::Vec3 warp{
      projectedFbm(material, world_position + aster::Vec3{0.37f, 0.0f, -0.11f}, normal, 0.090f,
                   307.0f) -
          0.5f,
      projectedFbm(material, world_position + aster::Vec3{-0.17f, 0.29f, 0.0f}, normal, 0.073f,
                   311.0f) -
          0.5f,
      projectedFbm(material, world_position + aster::Vec3{0.0f, -0.19f, 0.41f}, normal, 0.081f,
                   313.0f) -
          0.5f};
  const aster::Vec3 layered_position = world_position + warp * 0.34f + normal * (weld * 0.025f);
  const float broad = projectedFbm(material, layered_position, normal, 0.060f, 307.0f);
  const float medium =
      projectedFbm(material, layered_position + normal * 0.05f, normal, 0.42f, 331.0f);
  const float fine = ridge(projectedFbm(material, world_position, normal, 1.35f, 359.0f));
  const float flake = smoothstep(
      0.55f, 0.92f,
      ridge(projectedFbm(material, layered_position + aster::Vec3{0.13f, -0.17f, 0.29f}, normal,
                         0.28f, 887.0f)) *
              0.28f +
          medium * 0.22f + signals.pit_edge * 0.08f);
  const float soot_cloud = projectedFbm(
      material, layered_position + aster::Vec3{-0.41f, 0.22f, 0.17f}, normal, 0.115f, 811.0f);
  const float pitting = smoothstep(0.58f, 0.96f, signals.pit * 0.46f + fine * 0.08f);
  const float upward = smoothstep(0.14f, 0.80f, n.y);
  const float oxide = smoothstep(0.20f, 0.74f,
                                 broad * 0.36f + medium * 0.30f + lower * 0.16f +
                                     weld * 0.12f + rim * 0.14f +
                                     material.pattern_depth * upward * 0.10f +
                                     signals.rust_bloom * 0.42f +
                                     signals.orange_rust * 0.16f + flake * 0.04f);
  const float dark_scale = 0.38f + medium * 0.24f - lower * 0.08f - weld * 0.04f;
  const aster::Vec3 cool_steel =
      material.base_color.value * aster::Vec3{0.66f, 0.74f, 0.78f} * dark_scale;
  const aster::Vec3 exposed_edge =
      material.base_color.value * aster::Vec3{1.34f, 1.28f, 1.10f} * (0.54f + pitting * 0.20f);
  const aster::Vec3 orange_rust{0.45f, 0.155f, 0.040f};
  const aster::Vec3 dusty_rust{0.54f, 0.245f, 0.075f};
  const aster::Vec3 black_rust{0.040f, 0.035f, 0.030f};
  const aster::Vec3 cool_oxide{0.072f, 0.095f, 0.100f};
  const aster::Vec3 aged_paint{0.34f, 0.325f, 0.275f};
  const float gray_stain =
      projectedFbm(material, layered_position + aster::Vec3{-0.19f, 0.0f, 0.23f}, normal, 0.18f,
                   733.0f);
  const float rust_tone =
      std::clamp(medium * 0.56f + broad * 0.28f + signals.orange_rust * 0.10f, 0.0f,
                 1.0f);
  aster::Vec3 rust_layer = mixVec(black_rust, orange_rust, rust_tone);
  rust_layer =
      mixVec(rust_layer, dusty_rust,
             smoothstep(0.36f, 0.88f, broad + medium + signals.orange_rust) * 0.34f);
  aster::Vec3 color =
      mixVec(cool_steel, rust_layer,
             aster::saturate(oxide * 0.58f + signals.rust_bloom * 0.28f +
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
  const float cavity = aster::saturate(signals.cavity_grime * 0.34f + weld * 0.35f +
                                       rim * 0.34f + lower * 0.25f + pitting * 0.10f +
                                       dark_mottle * 0.20f + signals.rim_soot * 0.26f +
                                       signals.weld_scorch * 0.20f);
  color = mixVec(color, aged_paint, signals.paint_remnant * 0.16f * (1.0f - dark_mottle));
  color = mixVec(color, cool_oxide, gray_oxide * 0.78f);
  color = mixVec(color, black_rust,
                 cavity * 0.28f + dark_mottle * 0.40f + signals.black_scab * 0.42f +
                     signals.rim_soot * 0.34f + signals.weld_scorch * 0.26f);
  const float pinhole = smoothstep(0.82f, 0.98f, signals.pit + signals.pit_edge * 0.50f);
  color = mixVec(color, black_rust, pinhole * 0.10f);
  const float scratch_polish =
      smoothstep(0.68f, 0.96f, signals.axial_scratch * (0.64f + medium * 0.36f)) *
      (1.0f - aster::saturate(cavity * 0.55f + signals.black_scab * 0.28f));
  color = mixVec(color, exposed_edge * aster::Vec3{0.92f, 0.96f, 1.02f},
                 scratch_polish * 0.18f + signals.edge_polish * 0.10f);
  color = mixVec(color, exposed_edge,
                 signals.edge_polish * 0.16f +
                     material.edge_wear * smoothstep(0.72f, 0.98f, pitting) * 0.18f);
  color = mixVec(color, color * aster::Vec3{0.60f, 0.66f, 0.72f} +
                            aster::Vec3{0.010f, 0.012f, 0.014f},
                 signals.wet_film * 0.12f);
  const float pepper =
      ridge(projectedFbm(material, layered_position + aster::Vec3{0.21f, 0.31f, -0.14f}, normal,
                         2.25f, 941.0f));
  color *= 0.76f + medium * 0.13f + pepper * 0.020f - dark_mottle * 0.10f - cavity * 0.04f;
  return aster::clamp(color, 0.0f, 4.0f);
}

aster::Vec3 weldBeadAlbedo(const aster::Material &material, const aster::Vec3 world_position,
                           const aster::Vec3 normal, const aster::Vec2 uv) {
  const aster::AsterPipeSurfaceSignals signals =
      aster::sampleAsterPipeSurface({.position = world_position,
                                     .normal = normal,
                                     .uv = uv,
                                     .detail_scale = material.detail_scale},
                                    material.procedural, material.edge_wear,
                                    material.pattern_depth);
  const float bead_ripple =
      0.5f + 0.5f * std::sin(uv.y * std::max(material.pattern_scale.y, 0.001f) * kTau +
                              projectedFbm(material, world_position, normal, 1.0f, 401.0f) * 3.8f);
  const float heat_band = saturate(smoothstep(0.05f, 0.58f, ridge(uv.x)) + signals.heat_tint);
  const float contact_grime = 1.0f - smoothstep(0.18f, 0.42f, ridge(uv.x));
  const float slag =
      ridge(projectedFbm(material, world_position + normal * 0.035f, normal, 1.14f, 413.0f));
  const float center = 1.0f - smoothstep(0.0f, 0.48f, std::abs(uv.x - 0.50f));
  const aster::Vec3 weld_metal = aster::Vec3{0.235f, 0.215f, 0.180f} *
                                 (0.70f + bead_ripple * 0.20f);
  const aster::Vec3 polished_lip{0.36f, 0.330f, 0.260f};
  const aster::Vec3 straw{0.66f, 0.34f, 0.10f};
  const aster::Vec3 blue_heat{0.08f, 0.13f, 0.25f};
  const aster::Vec3 soot{0.040f, 0.034f, 0.030f};
  const aster::Vec3 rust_dust{0.40f, 0.135f, 0.038f};
  aster::Vec3 color =
      mixVec(weld_metal, polished_lip, center * (0.055f + bead_ripple * 0.045f));
  color *= 0.82f + bead_ripple * 0.12f;
  color = mixVec(color, straw, heat_band * 0.16f);
  color = mixVec(color, blue_heat, heat_band * (1.0f - bead_ripple) * 0.20f);
  color = mixVec(color, soot,
                 signals.cavity_grime * 0.30f + contact_grime * 0.54f +
                     signals.weld_scorch * 0.34f);
  color = mixVec(color, rust_dust,
                 contact_grime * (0.26f + signals.orange_rust * 0.20f) +
                     signals.weld_slag * 0.16f);
  color = mixVec(color, color * 0.52f + aster::Vec3{0.050f, 0.044f, 0.036f},
                 slag * 0.18f + signals.weld_slag * 0.22f);
  color = mixVec(color, soot,
                 smoothstep(0.70f, 0.96f, slag + signals.black_scab * 0.45f) * 0.28f);
  color = mixVec(color, color * aster::Vec3{0.55f, 0.62f, 0.70f}, signals.wet_film * 0.18f);
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
                                    const aster::Vec3 world_position, const aster::Vec3 normal,
                                    const aster::RendererSettings &settings) {
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
  const float macro_gain =
      1.0f + std::max(material.procedural.macro_variation, 0.0f) * 0.34f +
      std::clamp(settings.surface_scale.macro_frequency_breakup, 0.0f, 2.0f) * 0.10f;
  const float micro_gain =
      1.0f + std::clamp(settings.surface_scale.micro_frequency_breakup, 0.0f, 2.0f) * 0.16f;
  const float profile_id = static_cast<float>(aster::materialSurfaceProfileId(profile));
  const aster::Vec3 p =
      world_position * (0.55f * detail * macro_gain * micro_gain) +
      aster::Vec3{profile_id * 0.23f, 0.0f, profile_id * 0.41f};
  const aster::Vec3 bump{valueNoise(p + aster::Vec3{11.1f, 2.3f, 0.7f}) - 0.5f,
                         (valueNoise(p + aster::Vec3{5.7f, 13.1f, 3.2f}) - 0.5f) * 0.35f,
                         valueNoise(p + aster::Vec3{2.9f, 7.4f, 17.6f}) - 0.5f};
  if (profile == aster::MaterialSurfaceProfile::CorrodedMetal ||
      profile == aster::MaterialSurfaceProfile::WeldBead) {
    return aster::perturbAsterSurfaceNormal({.position = world_position,
                                             .normal = normal,
                                             .uv = {},
                                             .detail_scale = material.detail_scale},
                                            material.procedural, {1.0f, 0.0f, 0.0f}, strength);
  }
  const aster::Vec3 adjusted = aster::normalize(normal + bump * strength);
  return aster::length(adjusted) > 0.0001f ? adjusted : normal;
}

aster::Vec3 applyScalePresentationNormal(const aster::Material &material,
                                         const aster::Vec3 world_position,
                                         const aster::Vec3 normal,
                                         const aster::RendererSettings &settings) {
  const aster::MaterialSurfaceProfile profile = aster::resolveMaterialSurfaceProfile(material);
  if (profile == aster::MaterialSurfaceProfile::ContactShadow ||
      profile == aster::MaterialSurfaceProfile::Liquid ||
      settings.surface_scale.height_normal_coupling <= 0.0001f) {
    return normal;
  }

  const float coupling = std::clamp(settings.surface_scale.height_normal_coupling, 0.0f, 1.5f);
  const float density =
      std::clamp(settings.surface_scale.physical_texel_density / 768.0f, 0.45f, 2.25f);
  const float macro_breakup = std::clamp(settings.surface_scale.macro_frequency_breakup, 0.0f, 2.0f);
  const float micro_breakup = std::clamp(settings.surface_scale.micro_frequency_breakup, 0.0f, 2.0f);
  const float surface_gain =
      std::clamp(material.pattern_depth * 0.55f + material.edge_wear * 0.42f +
                     material.procedural.height_shading * 0.68f + material.detail_strength * 0.16f,
                 0.0f, 1.0f);
  if (surface_gain <= 0.0001f) {
    return normal;
  }

  const SurfaceBasis basis = makeSurfaceBasis(normal);
  const float step =
      (0.018f / std::sqrt(std::max(material.detail_scale, 0.001f) + 1.0f)) /
      std::sqrt(density);
  const float profile_id = static_cast<float>(aster::materialSurfaceProfileId(profile));
  const auto height_at = [&](const aster::Vec3 position) {
    const float secondary =
        projectedFbm(material, position, basis.normal, 0.12f + macro_breakup * 0.035f,
                     719.0f + profile_id);
    const float edge_energy =
        ridge(projectedFbm(material, position + basis.normal * 0.027f, basis.normal,
                           0.76f + micro_breakup * 0.18f, 733.0f + profile_id));
    const float micro =
        projectedFbm(material, position - basis.normal * 0.019f, basis.normal,
                     1.72f + micro_breakup * 0.42f, 751.0f + profile_id);
    return secondary * 0.42f + edge_energy * 0.34f + micro * 0.24f;
  };
  const float center = height_at(world_position);
  const float dx = height_at(world_position + basis.tangent * step) - center;
  const float dy = height_at(world_position + basis.bitangent * step) - center;
  const float gain = coupling * surface_gain * (5.0f + density * 1.4f);
  const aster::Vec3 adjusted =
      aster::normalizeOr(basis.normal - basis.tangent * dx * gain -
                             basis.bitangent * dy * gain,
                         basis.normal);
  return adjusted;
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
    return aster::clamp(fiber, 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::BiologicalIntegument:
    return applyProceduralLayer(material, world_position, normal,
                                biologicalIntegumentAlbedo(material, world_position, normal, uv));
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
    const aster::Vec3 dark = material.base_color.value * aster::Vec3{0.62f, 0.48f, 0.34f};
    const aster::Vec3 warm = material.base_color.value * aster::Vec3{1.18f, 0.96f, 0.68f};
    return aster::clamp(mixVec(dark, warm, smoothstep(0.18f, 0.92f, rings)), 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::Feather: {
    const float central = 1.0f - smoothstep(0.025f, 0.18f, std::abs(uv.x - 0.5f));
    const float barb =
        0.5f + 0.5f * std::sin((uv.y * material.pattern_scale.y + uv.x * 3.0f) * kTau);
    aster::Vec3 feather = mixVec(material.base_color.value * 0.72f, material.base_color.value * 1.22f,
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
        mixVec(material.base_color.value * aster::Vec3{0.66f, 0.82f, 0.76f},
               material.base_color.value * aster::Vec3{1.22f, 1.02f, 0.68f}, hue_shift);
    return aster::clamp(mixVec(material.base_color.value * 0.62f, scale_color, shell), 0.0f, 4.0f);
  }
  case aster::MaterialSurfaceProfile::CorrodedMetal:
    return applyProceduralLayer(material, world_position, normal,
                                corrodedMetalAlbedo(material, world_position, normal, uv));
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

float sampleRuntimeHeight(const aster::RuntimeTexture &texture, const aster::Vec2 uv) {
  return std::clamp(aster::sampleRuntimeTextureChannel(texture, uv, 0u), 0.0f, 1.0f);
}

aster::Vec3 applyRuntimeHeightNormal(const aster::RuntimeTextureSet *textures, aster::Vec3 normal,
                                     const aster::Vec4 tangent_handedness,
                                     const aster::Vec2 uv,
                                     const aster::RendererSettings &settings) {
  const aster::RuntimeTexture *height_texture = textureForRole(textures, "height");
  const float coupling = std::clamp(settings.surface_scale.height_normal_coupling, 0.0f, 1.5f);
  if (!hasAuthoredRuntimeTexture(height_texture) || coupling <= 0.0001f) {
    return normal;
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
  const float texel_u = 1.0f / static_cast<float>(std::max(height_texture->width, 1u));
  const float texel_v = 1.0f / static_cast<float>(std::max(height_texture->height, 1u));
  const float left = sampleRuntimeHeight(*height_texture, {uv.x - texel_u, uv.y});
  const float right = sampleRuntimeHeight(*height_texture, {uv.x + texel_u, uv.y});
  const float down = sampleRuntimeHeight(*height_texture, {uv.x, uv.y - texel_v});
  const float up = sampleRuntimeHeight(*height_texture, {uv.x, uv.y + texel_v});
  const float texel_density =
      std::clamp(settings.surface_scale.physical_texel_density / 512.0f, 0.25f, 2.25f);
  const float breakup =
      0.65f + settings.surface_scale.micro_frequency_breakup * 0.25f +
      settings.surface_scale.macro_frequency_breakup * 0.10f;
  const float strength = coupling * texel_density * breakup * 3.0f;
  const aster::Vec3 tangent_space =
      aster::normalizeOr(aster::Vec3{-(right - left) * strength, -(up - down) * strength, 1.0f},
                         aster::Vec3{0.0f, 0.0f, 1.0f});
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
  float height = 0.5f;
  aster::Vec3 emissive{};
};

void applyHeightResponse(RuntimeMaterialSample &sample, const float height,
                         const float roughness_coupling, const float response_strength) {
  const float strength = std::clamp(response_strength, 0.0f, 1.5f);
  const float coupling = std::clamp(roughness_coupling * strength, 0.0f, 1.75f);
  if (coupling <= 0.0001f) {
    sample.height = height;
    return;
  }
  const float cavity = 1.0f - smoothstep(0.22f, 0.78f, height);
  const float crest = smoothstep(0.54f, 0.96f, height);
  sample.height = height;
  sample.albedo *= 0.92f + height * 0.14f - cavity * 0.055f * strength;
  sample.roughness = std::clamp(sample.roughness + cavity * coupling * 0.16f -
                                    crest * coupling * 0.045f,
                                0.045f, 1.0f);
  sample.ambient_occlusion *=
      std::clamp(1.0f - cavity * (0.14f + coupling * 0.08f), 0.46f, 1.0f);
  sample.wetness = std::max(sample.wetness, cavity * coupling * 0.085f);
}

float sampleProceduralHeightSignal(const aster::Material &material,
                                   const aster::Vec3 world_position,
                                   const aster::Vec3 normal, const aster::Vec2 uv) {
  const aster::MaterialSurfaceProfile profile = aster::resolveMaterialSurfaceProfile(material);
  if (profile == aster::MaterialSurfaceProfile::ContactShadow ||
      profile == aster::MaterialSurfaceProfile::Liquid) {
    return 0.5f;
  }
  if (profile == aster::MaterialSurfaceProfile::CorrodedMetal ||
      profile == aster::MaterialSurfaceProfile::WeldBead) {
    const aster::AsterPipeSurfaceSignals signals =
        aster::sampleAsterPipeSurface({.position = world_position,
                                       .normal = normal,
                                       .uv = uv,
                                       .detail_scale = material.detail_scale},
                                      material.procedural, material.edge_wear,
                                      material.pattern_depth);
    return std::clamp(0.42f + signals.height * 0.58f + signals.pit * 0.22f +
                          signals.cavity_grime * 0.16f + signals.weld_slag * 0.18f +
                          signals.rim_soot * 0.12f,
                      0.0f, 1.0f);
  }

  const float active_height =
      std::max({std::abs(material.pattern_depth), material.procedural.height_shading,
                material.procedural.roughness_height_coupling, material.detail_strength * 0.35f});
  if (active_height <= 0.0001f) {
    return 0.5f;
  }
  const float profile_id = static_cast<float>(aster::materialSurfaceProfileId(profile));
  const float macro =
      projectedFbm(material, world_position, normal, 0.10f, 811.0f + profile_id);
  const float mid =
      ridge(projectedFbm(material, world_position + normal * 0.041f, normal, 0.62f,
                         829.0f + profile_id));
  const float fine =
      projectedFbm(material, world_position - normal * 0.023f, normal, 1.48f,
                   853.0f + profile_id);
  const float upward = smoothstep(0.12f, 0.86f, aster::normalizeOr(normal, {0.0f, 1.0f, 0.0f}).y);
  return std::clamp(0.5f + (macro - 0.5f) * 0.36f + (mid - 0.5f) * 0.30f +
                        (fine - 0.5f) * 0.18f + upward * material.procedural.height_shading * 0.18f,
                    0.0f, 1.0f);
}

RuntimeMaterialSample sampleRuntimeMaterial(const aster::Material &material,
                                            const aster::RuntimeTextureSet *textures,
                                            const aster::Vec3 world_position,
                                            const aster::Vec3 normal, const aster::Vec2 uv,
                                            const aster::RendererSettings &settings,
                                            const double frame_seconds) {
  RuntimeMaterialSample sample;
  sample.albedo = materialAlbedo(material, world_position, normal, uv, frame_seconds);
  sample.roughness = std::clamp(material.roughness, 0.045f, 1.0f);
  sample.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
  sample.ambient_occlusion = std::clamp(material.ambient_occlusion, 0.0f, 1.0f);
  sample.opacity = std::clamp(material.opacity, 0.0f, 1.0f);
  sample.wetness = std::clamp(material.procedural.wetness, 0.0f, 1.0f);
  sample.emissive = material.emission_color.value * material.emission_strength;

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
    const float height = sampleRuntimeHeight(*height_texture, uv);
    const float roughness_coupling =
        std::clamp(settings.surface_scale.roughness_height_coupling +
                       material.procedural.roughness_height_coupling * 0.35f,
                   0.0f, 1.75f);
    applyHeightResponse(sample, height, roughness_coupling, 1.0f);
  } else {
    const float procedural_height =
        sampleProceduralHeightSignal(material, world_position, normal, uv);
    const float procedural_coupling =
        std::clamp(settings.surface_scale.roughness_height_coupling * 0.72f +
                       material.procedural.roughness_height_coupling * 0.48f,
                   0.0f, 1.75f);
    applyHeightResponse(sample, procedural_height, procedural_coupling, 0.72f);
  }
  if (const aster::RuntimeTexture *wetness_texture = textureForRole(textures, "wetness");
      hasAuthoredRuntimeTexture(wetness_texture)) {
    sample.wetness =
        std::max(sample.wetness, aster::sampleRuntimeTextureChannel(*wetness_texture, uv, 0u));
  }
  const aster::MaterialSurfaceProfile profile = aster::resolveMaterialSurfaceProfile(material);
  if (profile == aster::MaterialSurfaceProfile::CorrodedMetal ||
      profile == aster::MaterialSurfaceProfile::WeldBead) {
    const aster::AsterPipeSurfaceSignals signals =
        aster::sampleAsterPipeSurface({.position = world_position,
                                       .normal = normal,
                                       .uv = uv,
                                       .detail_scale = material.detail_scale},
                                      material.procedural, material.edge_wear,
                                      material.pattern_depth);
    const float rust_plate = aster::saturate(signals.rust_bloom * 0.54f +
                                             signals.orange_rust * 0.24f +
                                             signals.black_scab * 0.42f +
                                             signals.cavity_grime * 0.24f);
    const float height_coupling =
        std::clamp(material.procedural.roughness_height_coupling, 0.0f, 1.50f);
    sample.height = std::max(sample.height, signals.height);
    sample.roughness = std::lerp(sample.roughness, 0.97f, rust_plate * 0.66f);
    sample.roughness = std::clamp(sample.roughness +
                                      signals.pit * (0.12f + height_coupling * 0.06f) +
                                      signals.weld_slag * (0.08f + height_coupling * 0.04f),
                                  0.045f, 1.0f);
    sample.roughness =
        std::lerp(sample.roughness, 0.99f, signals.height * height_coupling * 0.18f);
    sample.roughness = std::lerp(sample.roughness, 0.58f, signals.paint_remnant * 0.20f);
    sample.roughness = std::lerp(sample.roughness, 0.46f, signals.axial_scratch * 0.16f);
    sample.roughness = std::lerp(sample.roughness, 0.25f, signals.edge_polish * 0.38f);
    const float corrosion_plate =
        aster::saturate(signals.rust_bloom * 0.42f + signals.black_scab * 0.58f +
                        signals.cavity_grime * 0.28f + signals.pit * 0.18f);
    sample.metallic = std::lerp(sample.metallic, 0.035f, corrosion_plate * 0.74f);
    sample.metallic = std::lerp(sample.metallic, 0.78f, signals.edge_polish * 0.28f);
    sample.metallic = std::lerp(sample.metallic, 0.62f, signals.axial_scratch * 0.12f);
    sample.metallic = std::clamp(sample.metallic - signals.wet_film * 0.04f, 0.0f, 1.0f);
    sample.wetness = std::max(sample.wetness, signals.wet_film);
    sample.ambient_occlusion *= std::clamp(1.0f - signals.cavity_grime * 0.30f -
                                               signals.pit * 0.10f -
                                               signals.black_scab * 0.16f -
                                               signals.rim_soot * 0.26f -
                                               signals.weld_scorch * 0.20f -
                                               signals.weld_slag * 0.16f,
                                           0.45f, 1.0f);
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
    normal = proceduralSurfaceNormal(material, material_sample_position, normal, settings);
  }
  normal = applyRuntimeNormalMap(textures, normal, tangent_handedness, uv);
  normal = applyRuntimeHeightNormal(textures, normal, tangent_handedness, uv, settings);
  normal = applyScalePresentationNormal(material, material_sample_position, normal, settings);

  const RuntimeMaterialSample material_sample =
      sampleRuntimeMaterial(material, textures, material_sample_position, normal, uv, settings,
                            frame_seconds);
  const aster::Vec3 albedo = material_sample.albedo;
  const SurfaceOcclusionEval surface_occlusion =
      evaluateSurfaceOcclusion(world_position, normal, material_sample.ambient_occlusion, settings);
  const float sky_factor = saturate(surface_occlusion.bent_normal.y * 0.5f + 0.5f);
  const aster::Vec3 ambient_color =
      mixVec(settings.ground_ambient_color, settings.sky_ambient_color, sky_factor);
  const float geometry_ao =
      std::clamp(material_sample.ambient_occlusion * std::clamp(vertex_ao, 0.0f, 1.0f) *
                     surface_occlusion.visibility,
                 0.0f, 1.0f);
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
  const float specular_visibility =
      evaluateSpecularOcclusion(geometry_ao, perceptual_roughness, n_dot_v);

  if (settings.reflections.enabled && settings.reflections.fallback_intensity > 0.0f) {
    const aster::Vec3 incident = -view;
    const aster::Vec3 reflection =
        aster::normalizeOr(incident - normal * (2.0f * aster::dot(incident, normal)), normal);
    const float probe_mix = smoothstep(0.22f, 0.96f, perceptual_roughness);
    const aster::Vec3 dominant_reflection =
        aster::normalizeOr(mixVec(reflection, surface_occlusion.bent_normal,
                                  probe_mix * (0.26f + surface_occlusion.cavity * 0.18f)),
                            reflection);
    const aster::Vec3 env_color =
        mixVec(softwareReflectionEnvironment(frame_resources, world_position, dominant_reflection,
                                             settings),
               ambient_color, probe_mix * 0.34f);
    const aster::Vec3 fresnel =
        f0 + (aster::Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - n_dot_v, 5.0f);
    const float roughness_visibility =
        std::pow(1.0f - perceptual_roughness * 0.62f, 2.0f) *
        (0.72f + (1.0f - probe_mix) * 0.28f);
    const float material_visibility = 0.24f + metallic * 0.76f;
    color += env_color * fresnel *
             (roughness_visibility * material_visibility * specular_visibility *
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
    const aster::Vec3 specular =
        fresnel * (distribution * geometry_l * geometry_v * specular_visibility);
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

  if (!settings.occlusion.enabled && settings.grounding.enabled &&
      settings.grounding.surface_occlusion_strength > 0.0f) {
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
      sampleRuntimeMaterial(material, textures, world_position, normal, uv, settings,
                            frame_seconds);
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

void applySoftwarePresentationGrade(std::vector<std::uint8_t> &pixels, const int width,
                                    const int height,
                                    const aster::PresentationLensSettings &presentation) {
  const float vignette = std::clamp(presentation.vignette_strength, 0.0f, 0.65f);
  const float shoulder = std::clamp(presentation.shoulder_strength, 0.0f, 1.0f);
  const float composition = std::clamp(presentation.composition_weight, 0.0f, 1.2f);
  const float scale_reference = std::clamp(presentation.scale_reference_m / 1.8f, 0.35f, 3.0f);
  const float detail_gain = composition * (0.020f + scale_reference * 0.018f);
  const float grain_strength = composition * std::clamp(scale_reference, 0.4f, 1.6f) * 0.0035f;
  if ((vignette <= 0.0001f && shoulder <= 0.0001f && detail_gain <= 0.0001f &&
       grain_strength <= 0.0001f) ||
      width <= 0 || height <= 0) {
    return;
  }
  const std::vector<std::uint8_t> source = pixels;
  const float inv_width = 1.0f / static_cast<float>(std::max(width, 1));
  const float inv_height = 1.0f / static_cast<float>(std::max(height, 1));
  for (int y = 0; y < height; ++y) {
    const float ny = ((static_cast<float>(y) + 0.5f) * inv_height) * 2.0f - 1.0f;
    for (int x = 0; x < width; ++x) {
      const float nx = ((static_cast<float>(x) + 0.5f) * inv_width) * 2.0f - 1.0f;
      const float radial = std::sqrt(nx * nx + ny * ny);
      const float vignette_gain = 1.0f - smoothstep(0.42f, 1.18f, radial) * vignette;
      const std::size_t base = static_cast<std::size_t>(y * width + x) * 4u;
      const float luma = byteLuma(source, base);
      float local_average = luma;
      if (x > 0 && x + 1 < width && y > 0 && y + 1 < height) {
        local_average =
            (byteLuma(source, static_cast<std::size_t>(y * width + x - 1) * 4u) +
             byteLuma(source, static_cast<std::size_t>(y * width + x + 1) * 4u) +
             byteLuma(source, static_cast<std::size_t>((y - 1) * width + x) * 4u) +
             byteLuma(source, static_cast<std::size_t>((y + 1) * width + x) * 4u)) *
            0.25f;
      }
      const float local_detail = std::clamp(luma - local_average, -0.22f, 0.22f);
      const float grain =
          fractValue(52.9829189f *
                     fractValue(0.06711056f * static_cast<float>(x) +
                                0.00583715f * static_cast<float>(y) +
                                presentation.focal_length_mm * 0.00017f)) -
          0.5f;
      const float shoulder_weight = smoothstep(0.62f, 1.0f, luma) * shoulder;
      for (std::size_t channel = 0u; channel < 3u; ++channel) {
        float value = static_cast<float>(source[base + channel]) / 255.0f;
        value *= vignette_gain;
        value += local_detail * detail_gain;
        value += grain * grain_strength * (1.0f - smoothstep(0.04f, 0.84f, luma));
        value = std::lerp(value, value / (1.0f + value * 0.42f), shoulder_weight);
        pixels[base + channel] = static_cast<std::uint8_t>(
            std::clamp(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0l, 255l));
      }
    }
  }
}

void applySoftwarePostProcess(aster::SoftwareFrameBuffer &framebuffer,
                              const aster::RendererSettings &settings) {
  if (framebuffer.empty() ||
      (!settings.post.bloom && !settings.post.fxaa &&
       settings.presentation.vignette_strength <= 0.0001f &&
       settings.presentation.shoulder_strength <= 0.0001f)) {
    return;
  }
  std::vector<std::uint8_t> pixels(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  applySoftwareBloom(pixels, framebuffer.width(), framebuffer.height(), settings.post);
  applySoftwareFxaa(pixels, framebuffer.width(), framebuffer.height(), settings.post);
  applySoftwarePresentationGrade(pixels, framebuffer.width(), framebuffer.height(),
                                 settings.presentation);
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
  shadow.material.base_color.value = {0.0f, 0.0f, 0.0f};
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
  const aster::ShadowAtlasContract contract = aster::makeShadowAtlasContract(camera, settings);
  if (contract.atlas_size == 0u || contract.cascade_count == 0u) {
    return;
  }
  resources.shadow_atlas_size = contract.atlas_size;
  resources.shadow_depths.assign(static_cast<std::size_t>(contract.atlas_size) * contract.atlas_size,
                                 std::numeric_limits<float>::infinity());
  resources.shadow_rgba8.assign(
      static_cast<std::size_t>(contract.atlas_size) * contract.atlas_size * 4u, 255u);
  resources.cascades.clear();
  resources.cascades.reserve(contract.cascade_count);

  for (std::uint32_t cascade = 0u; cascade < contract.cascade_count; ++cascade) {
    const aster::ShadowCascadeContract &source = contract.cascades[cascade];
    resources.cascades.push_back({.center = source.center,
                                  .right = source.right,
                                  .up = source.up,
                                  .forward = source.forward,
                                  .radius = source.radius,
                                  .tile_x = source.tile_x,
                                  .tile_y = source.tile_y,
                                  .tile_width = source.tile_width,
                                  .tile_height = source.tile_height});
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
              static_cast<std::size_t>(atlas_y) * contract.atlas_size +
              static_cast<std::size_t>(atlas_x);
          resources.shadow_depths[index] = std::min(resources.shadow_depths[index], caster_depth);
        }
      }
    }
  }

  for (std::uint32_t y = 0u; y < contract.atlas_size; ++y) {
    for (std::uint32_t x = 0u; x < contract.atlas_size; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y) * contract.atlas_size + x;
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

void buildSoftwareSurfaceAttributes(const aster::Scene &scene, const aster::OrbitCamera &camera,
                                    const aster::RendererSettings &settings,
                                    SoftwareFrameResources &resources) {
  resources.surface_width = std::max(resources.frame_width, 1u);
  resources.surface_height = std::max(resources.frame_height, 1u);
  resources.surface_attributes_rgba8.assign(static_cast<std::size_t>(resources.surface_width) *
                                                resources.surface_height * 4u,
                                            0u);
  const float texel_density = std::max(settings.surface_scale.physical_texel_density, 1.0f);
  const float macro_breakup = std::clamp(settings.surface_scale.macro_frequency_breakup, 0.0f, 2.0f);
  const float micro_breakup = std::clamp(settings.surface_scale.micro_frequency_breakup, 0.0f, 2.0f);
  const float coupling = std::clamp(settings.surface_scale.height_normal_coupling, 0.0f, 1.5f);
  const float roughness_coupling =
      std::clamp(settings.surface_scale.roughness_height_coupling, 0.0f, 1.75f);
  const std::uint32_t surface_samples =
      std::clamp(settings.occlusion.sample_count, 4u, 24u);
  for (std::uint32_t y = 0u; y < resources.surface_height; ++y) {
    const float fy = (static_cast<float>(y) + 0.5f) /
                     static_cast<float>(std::max(resources.surface_height, 1u));
    for (std::uint32_t x = 0u; x < resources.surface_width; ++x) {
      const float fx = (static_cast<float>(x) + 0.5f) /
                       static_cast<float>(std::max(resources.surface_width, 1u));
      float macro = 0.0f;
      float micro = 0.0f;
      float directional = 0.0f;
      for (std::uint32_t sample = 0u; sample < surface_samples; ++sample) {
        const aster::Vec2 h = hammersley2d(sample, surface_samples);
        const float angle = h.x * kTau + texel_density * 0.00013f;
        const float radius = std::sqrt(h.y) * 0.018f * (1.0f + macro_breakup);
        const aster::Vec2 offset{std::cos(angle) * radius, std::sin(angle) * radius};
        const aster::Vec3 sample_position{
            (fx + offset.x) * (5.0f + macro_breakup * 8.0f),
            (fy + offset.y) * (4.0f + macro_breakup * 6.0f),
            texel_density * 0.00031f + static_cast<float>(sample) * 0.071f};
        macro += valueNoise(sample_position);
        micro += valueNoise(sample_position * (7.0f + micro_breakup * 8.0f) +
                            aster::Vec3{0.13f, 0.37f, 1.91f});
        directional += ridge(valueNoise(sample_position * (2.4f + micro_breakup) +
                                        aster::Vec3{2.1f, 0.5f, 0.7f}));
      }
      const float inv_samples = 1.0f / static_cast<float>(surface_samples);
      macro *= inv_samples;
      micro *= inv_samples;
      directional *= inv_samples;
      const float normal_x = 0.5f + (macro - 0.5f) * 0.22f * coupling;
      const float normal_y = 0.74f + (directional - 0.5f) * 0.18f * coupling;
      const float roughness =
          std::clamp(0.50f + macro_breakup * 0.10f +
                         directional * (0.10f + roughness_coupling * 0.035f) -
                         micro * (0.13f - roughness_coupling * 0.020f) +
                         (1.0f - macro) * roughness_coupling * 0.045f,
                     0.05f, 0.96f);
      const float ao =
          std::clamp(0.84f - macro * 0.12f - micro * (0.07f + roughness_coupling * 0.025f) -
                         directional * 0.06f,
                     0.36f, 1.0f);
      const std::size_t base =
          (static_cast<std::size_t>(y) * resources.surface_width + x) * 4u;
      resources.surface_attributes_rgba8[base + 0u] = debugByte(normal_x);
      resources.surface_attributes_rgba8[base + 1u] = debugByte(normal_y);
      resources.surface_attributes_rgba8[base + 2u] = debugByte(roughness);
      resources.surface_attributes_rgba8[base + 3u] = debugByte(ao);
    }
  }

  const aster::Vec3 camera_position = camera.position();
  const float view_radius = std::max(aster::length(camera_position - camera.target), 0.001f);
  for (const aster::RenderObject &object : scene.objects()) {
    if (isContactShadowUtility(object)) {
      continue;
    }
    const LocalBounds bounds = objectLocalBounds(object);
    const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
    const float sx = (object.transform.position.x - camera.target.x) / view_radius;
    const float sy = (object.transform.position.z - camera.target.z) / view_radius;
    const float center_x = (0.5f + sx * 0.32f) * static_cast<float>(resources.surface_width);
    const float center_y = (0.58f - sy * 0.26f) * static_cast<float>(resources.surface_height);
    const float radius_x = std::max(half_extents.x * 0.18f, 0.035f) *
                           static_cast<float>(resources.surface_width);
    const float radius_y = std::max(half_extents.y * 0.18f, 0.035f) *
                           static_cast<float>(resources.surface_height);
    const int min_x = std::max(0, static_cast<int>(std::floor(center_x - radius_x)));
    const int max_x =
        std::min(static_cast<int>(resources.surface_width) - 1,
                 static_cast<int>(std::ceil(center_x + radius_x)));
    const int min_y = std::max(0, static_cast<int>(std::floor(center_y - radius_y)));
    const int max_y =
        std::min(static_cast<int>(resources.surface_height) - 1,
                 static_cast<int>(std::ceil(center_y + radius_y)));
    for (int py = min_y; py <= max_y; ++py) {
      for (int px = min_x; px <= max_x; ++px) {
        const float dx = (static_cast<float>(px) + 0.5f - center_x) / std::max(radius_x, 0.001f);
        const float dy = (static_cast<float>(py) + 0.5f - center_y) / std::max(radius_y, 0.001f);
        const float falloff = 1.0f - smoothstep(0.15f, 1.0f, dx * dx + dy * dy);
        if (falloff <= 0.0f) {
          continue;
        }
        const std::size_t base =
            (static_cast<std::size_t>(py) * resources.surface_width +
             static_cast<std::size_t>(px)) *
            4u;
        const float authored_density =
            object.material.procedural.physical_texel_density > 0.0f
                ? object.material.procedural.physical_texel_density
                : settings.surface_scale.physical_texel_density;
        const float density_mark =
            std::clamp(authored_density / std::max(settings.surface_scale.physical_texel_density, 1.0f),
                       0.25f, 2.0f);
        float material_roughness = object.material.roughness;
        float material_ao_drop = object.material.procedural.cavity_grime * 0.26f;
        float normal_lift = falloff * 0.08f;
        const aster::MaterialSurfaceProfile profile =
            aster::resolveMaterialSurfaceProfile(object.material);
        if (profile == aster::MaterialSurfaceProfile::CorrodedMetal ||
            profile == aster::MaterialSurfaceProfile::WeldBead) {
          const aster::Vec3 signal_position =
              object.transform.position +
              aster::Vec3{dx * std::max(half_extents.x, 0.001f), 0.0f,
                          dy * std::max(half_extents.y, 0.001f)};
          const aster::AsterPipeSurfaceSignals signals =
              aster::sampleAsterPipeSurface({.position = signal_position,
                                             .normal = {0.0f, 1.0f, 0.0f},
                                             .uv = {0.5f + dx * 0.5f, 0.5f + dy * 0.5f},
                                             .detail_scale = object.material.detail_scale},
                                            object.material.procedural,
                                            object.material.edge_wear,
                                            object.material.pattern_depth);
          const float corrosion = std::clamp(signals.rust_bloom * 0.45f +
                                                 signals.black_scab * 0.35f +
                                                 signals.cavity_grime * 0.24f +
                                                 signals.weld_slag * 0.20f,
                                             0.0f, 1.0f);
          material_roughness =
              std::clamp(std::lerp(material_roughness, 0.96f, corrosion * 0.72f) -
                             signals.edge_polish * 0.18f,
                         0.045f, 1.0f);
          material_ao_drop += corrosion * 0.20f + signals.pit * 0.12f + signals.rim_soot * 0.18f;
          normal_lift += (signals.height - 0.5f) * 0.18f * coupling;
        }
        const float roughness =
            std::clamp(static_cast<float>(resources.surface_attributes_rgba8[base + 2u]) / 255.0f +
                           (material_roughness - 0.55f) * 0.35f,
                       0.0f, 1.0f);
        const float ao =
            std::clamp(static_cast<float>(resources.surface_attributes_rgba8[base + 3u]) / 255.0f -
                           falloff * (0.10f + material_ao_drop),
                       0.20f, 1.0f);
        resources.surface_attributes_rgba8[base + 0u] =
            debugByte(0.5f + (density_mark - 1.0f) * 0.12f + normal_lift);
        resources.surface_attributes_rgba8[base + 1u] = debugByte(0.78f + falloff * 0.10f + normal_lift * 0.35f);
        resources.surface_attributes_rgba8[base + 2u] = debugByte(roughness);
        resources.surface_attributes_rgba8[base + 3u] = debugByte(ao);
      }
    }
  }
  resources.surface_attributes_ready = true;
}

void buildSoftwareSurfaceOcclusion(const aster::Scene &scene, const aster::OrbitCamera &camera,
                                   const aster::RendererSettings &settings,
                                   SoftwareFrameResources &resources) {
  if (!resources.surface_attributes_ready) {
    buildSoftwareSurfaceAttributes(scene, camera, settings, resources);
  }
  resources.occlusion_width = std::max(resources.frame_width, 1u);
  resources.occlusion_height = std::max(resources.frame_height, 1u);
  resources.surface_occlusion_rgba8.assign(static_cast<std::size_t>(resources.occlusion_width) *
                                               resources.occlusion_height * 4u,
                                           255u);
  const float strength =
      settings.occlusion.enabled ? std::clamp(settings.occlusion.strength, 0.0f, 1.0f) : 0.0f;
  const std::uint32_t occlusion_samples =
      std::clamp(settings.occlusion.sample_count, 4u, 24u);
  for (std::uint32_t y = 0u; y < resources.occlusion_height; ++y) {
    const float fy = (static_cast<float>(y) + 0.5f) /
                     static_cast<float>(std::max(resources.occlusion_height, 1u));
    for (std::uint32_t x = 0u; x < resources.occlusion_width; ++x) {
      const float fx = (static_cast<float>(x) + 0.5f) /
                       static_cast<float>(std::max(resources.occlusion_width, 1u));
      const std::size_t attr_base =
          (static_cast<std::size_t>(
               std::min(resources.surface_height - 1u,
                        static_cast<std::uint32_t>(fy * static_cast<float>(resources.surface_height)))) *
               resources.surface_width +
           std::min(resources.surface_width - 1u,
                    static_cast<std::uint32_t>(fx * static_cast<float>(resources.surface_width)))) *
          4u;
      const float packed_ao =
          resources.surface_attributes_rgba8.empty()
              ? 1.0f
              : static_cast<float>(resources.surface_attributes_rgba8[attr_base + 3u]) / 255.0f;
      const float floor_contact = 1.0f - smoothstep(0.52f, 0.98f, fy);
      const float horizon = std::pow(floor_contact, std::max(settings.occlusion.distance_falloff, 0.25f));
      float micro_average = 0.0f;
      for (std::uint32_t sample = 0u; sample < occlusion_samples; ++sample) {
        const aster::Vec2 h = hammersley2d(sample, occlusion_samples);
        const float angle = h.x * kTau;
        const float radius = std::sqrt(h.y) * 0.012f * (1.0f + settings.occlusion.radius);
        micro_average += valueNoise({(fx + std::cos(angle) * radius) *
                                         (32.0f + settings.surface_scale.micro_frequency_breakup * 24.0f),
                                     (fy + std::sin(angle) * radius) *
                                         (24.0f + settings.surface_scale.micro_frequency_breakup * 18.0f),
                                     static_cast<float>(sample) * 0.23f});
      }
      const float micro =
          micro_average / static_cast<float>(occlusion_samples) *
          settings.occlusion.micro_shadowing;
      const float occlusion =
          std::clamp(1.0f - (1.0f - packed_ao + horizon * settings.occlusion.contact_hardening +
                             micro) *
                                strength,
                     0.28f, 1.0f);
      const std::size_t base =
          (static_cast<std::size_t>(y) * resources.occlusion_width + x) * 4u;
      resources.surface_occlusion_rgba8[base + 0u] = debugByte(occlusion);
      resources.surface_occlusion_rgba8[base + 1u] = debugByte(occlusion);
      resources.surface_occlusion_rgba8[base + 2u] = debugByte(occlusion);
      resources.surface_occlusion_rgba8[base + 3u] = 255u;
    }
  }

  const aster::Vec3 camera_position = camera.position();
  const float view_radius = std::max(aster::length(camera_position - camera.target), 0.001f);
  for (const aster::RenderObject &object : scene.objects()) {
    if (isContactShadowUtility(object) || !object.material.receives_shadows) {
      continue;
    }
    const LocalBounds bounds = objectLocalBounds(object);
    const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
    const float sx = (object.transform.position.x - camera.target.x) / view_radius;
    const float sy = (object.transform.position.z - camera.target.z) / view_radius;
    const float center_x = (0.5f + sx * 0.32f) * static_cast<float>(resources.occlusion_width);
    const float center_y = (0.60f - sy * 0.26f) * static_cast<float>(resources.occlusion_height);
    const float radius_x = std::max(half_extents.x * 0.24f, 0.045f) *
                           static_cast<float>(resources.occlusion_width);
    const float radius_y = std::max(half_extents.y * 0.24f, 0.045f) *
                           static_cast<float>(resources.occlusion_height);
    const int min_x = std::max(0, static_cast<int>(std::floor(center_x - radius_x)));
    const int max_x =
        std::min(static_cast<int>(resources.occlusion_width) - 1,
                 static_cast<int>(std::ceil(center_x + radius_x)));
    const int min_y = std::max(0, static_cast<int>(std::floor(center_y - radius_y)));
    const int max_y =
        std::min(static_cast<int>(resources.occlusion_height) - 1,
                 static_cast<int>(std::ceil(center_y + radius_y)));
    const float contact_weight =
        std::clamp(settings.occlusion.strength * (0.22f + settings.occlusion.contact_hardening), 0.0f,
                   0.80f);
    for (int py = min_y; py <= max_y; ++py) {
      for (int px = min_x; px <= max_x; ++px) {
        const float dx = (static_cast<float>(px) + 0.5f - center_x) / std::max(radius_x, 0.001f);
        const float dy = (static_cast<float>(py) + 0.5f - center_y) / std::max(radius_y, 0.001f);
        const float falloff = 1.0f - smoothstep(0.10f, 1.0f, dx * dx + dy * dy);
        if (falloff <= 0.0f) {
          continue;
        }
        const std::size_t base =
            (static_cast<std::size_t>(py) * resources.occlusion_width +
             static_cast<std::size_t>(px)) *
            4u;
        const float current =
            static_cast<float>(resources.surface_occlusion_rgba8[base + 0u]) / 255.0f;
        const float value =
            std::clamp(current * (1.0f - falloff * contact_weight), 0.18f, 1.0f);
        resources.surface_occlusion_rgba8[base + 0u] = debugByte(value);
        resources.surface_occlusion_rgba8[base + 1u] = debugByte(value);
        resources.surface_occlusion_rgba8[base + 2u] = debugByte(value);
      }
    }
  }
  resources.surface_occlusion_ready = true;
}

void buildSoftwareVolumetricFog(const aster::OrbitCamera &camera,
                                const aster::RendererSettings &settings,
                                SoftwareFrameResources &resources) {
  const aster::FogVolumeContract contract =
      aster::makeFogVolumeContract(settings, resources.frame_width, resources.frame_height);
  if (contract.width == 0u || contract.height == 0u) {
    return;
  }
  resources.fog_width = contract.width;
  resources.fog_height = contract.height;
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
  const aster::ReflectionProbeAtlasContract contract =
      aster::makeReflectionProbeAtlasContract(scene, settings);
  if (contract.width == 0u || contract.height == 0u || contract.probe_count == 0u) {
    return;
  }
  resources.reflection_width = contract.width;
  resources.reflection_height = contract.height;
  resources.probes.clear();
  resources.probes.reserve(contract.probe_count);
  resources.reflection_rgba8.assign(static_cast<std::size_t>(resources.reflection_width) *
                                        resources.reflection_height * 4u,
                                    0u);

  for (std::uint32_t probe_index = 0u; probe_index < contract.probe_count; ++probe_index) {
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
      for (std::uint32_t y = 0u; y < contract.face_size; ++y) {
        const float vertical =
            (static_cast<float>(y) + 0.5f) / static_cast<float>(contract.face_size);
        for (std::uint32_t x = 0u; x < contract.face_size; ++x) {
          const float horizontal =
              (static_cast<float>(x) + 0.5f) / static_cast<float>(contract.face_size);
          const float edge = 1.0f - smoothstep(0.42f, 0.72f,
                                               std::abs(horizontal - 0.5f) +
                                                   std::abs(vertical - 0.5f));
          const aster::Vec3 color = face_color * (0.72f + edge * 0.28f);
          const std::uint32_t atlas_x = (probe_index * 6u + face) * contract.face_size + x;
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

ProjectedVertex projectVertex(
    const aster::Vertex &vertex, const aster::Mat4 &model,
    const aster::Mat4 &model_view_projection,
    const aster::MathResult<aster::WorldNormalFromLocal> &normal_from_local,
    const float tangent_handedness_scale, const int width, const int height,
    const float normal_offset) {
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

  const aster::Vec3 world_normal =
      normal_from_local
          ? aster::transformNormal(aster::Normal{vertex.normal}, normal_from_local.value).value
          : aster::normalizeOr(aster::transformVector(model, vertex.normal), {0.0f, 1.0f, 0.0f});
  aster::Vec3 world_tangent =
      aster::transformVector(model, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
  world_tangent = world_tangent - world_normal * aster::dot(world_normal, world_tangent);
  world_tangent = aster::normalizeOr(world_tangent, {1.0f, 0.0f, 0.0f});
  return {
      .valid = true,
      .world_position = aster::transformPoint(model, local_position),
      .normal = world_normal,
      .tangent = {world_tangent.x, world_tangent.y, world_tangent.z,
                  vertex.tangent.w * tangent_handedness_scale},
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
  const aster::WorldFromLocal world_from_local{model};
  const aster::MathResult<aster::WorldNormalFromLocal> normal_from_local =
      aster::worldNormalFromLocal(world_from_local);
  const float tangent_handedness_scale =
      aster::determinant(aster::upperLeftMat3(model)) < 0.0f ? -1.0f : 1.0f;
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
                      normal_from_local, tangent_handedness_scale, framebuffer.width(),
                      framebuffer.height(), depth_policy.normal_offset);
    const ProjectedVertex b =
        projectVertex(mesh.vertices[mesh.indices[i + 1u]], model, model_view_projection,
                      normal_from_local, tangent_handedness_scale, framebuffer.width(),
                      framebuffer.height(), depth_policy.normal_offset);
    const ProjectedVertex c =
        projectVertex(mesh.vertices[mesh.indices[i + 2u]], model, model_view_projection,
                      normal_from_local, tangent_handedness_scale, framebuffer.width(),
                      framebuffer.height(), depth_policy.normal_offset);
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

void RenderWorld::rebuild(const Scene &scene) {
  scene_.rebuild(scene);
}

const RenderScene &RenderWorld::scene() const noexcept {
  return scene_;
}

FixedRenderGraph RenderGraphCompiler::compileDefault(const bool ui_overlay_enabled,
                                                     const bool capture_enabled) {
  const auto graph_start = std::chrono::steady_clock::now();
  const FixedRenderGraph graph = makeFixedRenderGraph(ui_overlay_enabled, capture_enabled);
  const auto graph_end = std::chrono::steady_clock::now();
  last_compile_seconds_ = std::chrono::duration<double>(graph_end - graph_start).count();
  return graph;
}

double RenderGraphCompiler::lastCompileSeconds() const noexcept {
  return last_compile_seconds_;
}

std::size_t FrameExecutor::execute(const FixedRenderGraph &graph,
                                   const RenderGraphPassCallback &callback) const {
  return executeFixedRenderGraph(graph, callback);
}

GpuFrameProfiler::GpuFrameProfiler(const bool supported,
                                   const double timestamp_period_nanoseconds)
    : supported_(supported), timestamp_period_nanoseconds_(timestamp_period_nanoseconds) {}

void GpuFrameProfiler::beginFrame(const std::uint32_t slot_count) {
  samples_.clear();
  samples_.reserve(slot_count);
}

void GpuFrameProfiler::recordUnavailable(const std::uint32_t slot_count) {
  beginFrame(slot_count);
}

void GpuFrameProfiler::recordSample(const std::uint32_t slot, const std::uint64_t ticks,
                                    const bool available) {
  samples_.push_back({.slot = slot,
                      .ticks = ticks,
                      .nanoseconds = static_cast<double>(ticks) * timestamp_period_nanoseconds_,
                      .available = available});
}

bool GpuFrameProfiler::supported() const noexcept {
  return supported_;
}

const std::vector<rhi::TimestampQueryResult> &GpuFrameProfiler::samples() const noexcept {
  return samples_;
}

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

std::string_view backendFeatureProofKindName(const BackendFeatureProofKind kind) {
  switch (kind) {
  case BackendFeatureProofKind::GraphResource:
    return "graph-resource";
  case BackendFeatureProofKind::Capture:
    return "capture";
  case BackendFeatureProofKind::TextureSampling:
    return "texture-sampling";
  case BackendFeatureProofKind::Instancing:
    return "instancing";
  case BackendFeatureProofKind::GpuTimestamps:
    return "gpu-timestamps";
  case BackendFeatureProofKind::HdrRenderTarget:
    return "hdr-render-target";
  case BackendFeatureProofKind::Msaa:
    return "msaa";
  case BackendFeatureProofKind::Presentation:
    return "presentation";
  }
  return "graph-resource";
}

std::string_view backendFeatureProofStatusName(const BackendFeatureProofStatus status) {
  switch (status) {
  case BackendFeatureProofStatus::NotAdvertised:
    return "not-advertised";
  case BackendFeatureProofStatus::NotExercised:
    return "not-exercised";
  case BackendFeatureProofStatus::Proven:
    return "proven";
  case BackendFeatureProofStatus::MissingProof:
    return "missing-proof";
  case BackendFeatureProofStatus::Unsupported:
    return "unsupported";
  }
  return "missing-proof";
}

std::string_view frameDebuggerTimelineEventKindName(
    const FrameDebuggerTimelineEventKind kind) {
  switch (kind) {
  case FrameDebuggerTimelineEventKind::Visibility:
    return "visibility";
  case FrameDebuggerTimelineEventKind::MaterialBinding:
    return "material-binding";
  case FrameDebuggerTimelineEventKind::LightCluster:
    return "light-cluster";
  case FrameDebuggerTimelineEventKind::Shadow:
    return "shadow";
  case FrameDebuggerTimelineEventKind::SurfaceOcclusion:
    return "surface-occlusion";
  case FrameDebuggerTimelineEventKind::Fog:
    return "fog";
  case FrameDebuggerTimelineEventKind::Probe:
    return "probe";
  case FrameDebuggerTimelineEventKind::PassOutput:
    return "pass-output";
  case FrameDebuggerTimelineEventKind::Overdraw:
    return "overdraw";
  case FrameDebuggerTimelineEventKind::Fallback:
    return "fallback";
  }
  return "visibility";
}

std::string_view frameResourceProvenanceKindName(const FrameResourceProvenanceKind kind) {
  switch (kind) {
  case FrameResourceProvenanceKind::GraphResource:
    return "graph-resource";
  case FrameResourceProvenanceKind::MaterialTexture:
    return "material-texture";
  }
  return "graph-resource";
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
  const std::size_t active_light_count =
      static_cast<std::size_t>(std::count_if(lights.begin(), lights.end(), [&](const Light &light) {
        return light.intensity > visible_policy.min_intensity;
      }));
  grid.visible_lights = selectRenderLights(lights, camera.target, visible_policy);
  grid.fallback_used = grid.visible_lights.size() < active_light_count;

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

ClusteredLightFrameData buildClusteredLightFrameData(const ClusteredLightGrid &grid) {
  ClusteredLightFrameData data;
  data.cluster_count_x = grid.cluster_count_x;
  data.cluster_count_y = grid.cluster_count_y;
  data.cluster_count_z = grid.cluster_count_z;
  data.cluster_offsets = grid.cluster_offsets;
  data.visible_lights.reserve(grid.visible_lights.size());
  data.light_indices.reserve(grid.assignments.size());
  data.overflowed = grid.overflowed;
  data.fallback_used = grid.fallback_used;

  const auto append_hash = [](std::uint64_t &hash, const std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
  };
  const auto quantized = [](const float value) {
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(std::llround(std::clamp(value, -65536.0f, 65536.0f) * 4096.0f)));
  };

  std::uint64_t light_hash = 1469598103934665603ull;
  append_hash(light_hash, grid.visible_lights.size());
  for (const Light &light : grid.visible_lights) {
    data.visible_lights.push_back({.position = light.position,
                                   .radius = light.source_radius,
                                   .color = light.color,
                                   .intensity = light.intensity});
    append_hash(light_hash, quantized(light.position.x));
    append_hash(light_hash, quantized(light.position.y));
    append_hash(light_hash, quantized(light.position.z));
    append_hash(light_hash, quantized(light.source_radius));
    append_hash(light_hash, quantized(light.color.x));
    append_hash(light_hash, quantized(light.color.y));
    append_hash(light_hash, quantized(light.color.z));
    append_hash(light_hash, quantized(light.intensity));
  }
  data.visible_lights_hash = light_hash;

  std::uint64_t assignment_hash = 1469598103934665603ull;
  append_hash(assignment_hash, grid.cluster_count_x);
  append_hash(assignment_hash, grid.cluster_count_y);
  append_hash(assignment_hash, grid.cluster_count_z);
  append_hash(assignment_hash, grid.cluster_offsets.size());
  for (const std::uint32_t offset : grid.cluster_offsets) {
    append_hash(assignment_hash, offset);
  }
  for (const ClusteredLightAssignment &assignment : grid.assignments) {
    data.light_indices.push_back(assignment.light_index);
    append_hash(assignment_hash, assignment.cluster_index);
    append_hash(assignment_hash, assignment.light_index);
  }
  data.assignments_hash = assignment_hash;
  return data;
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
      renderGraphResourceBit(RenderGraphResource::SurfaceAttributes) |
      renderGraphResourceBit(RenderGraphResource::LightClusters) |
      renderGraphResourceBit(RenderGraphResource::ShadowAtlas) |
      renderGraphResourceBit(RenderGraphResource::SurfaceOcclusion) |
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
          .projection_convention = defaultProjectionConvention(),
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

std::uint32_t imageFormatBytesPerPixel(const rhi::ImageFormat format) {
  switch (format) {
  case rhi::ImageFormat::Rgba16Float:
    return 8u;
  case rhi::ImageFormat::Depth32Float:
  case rhi::ImageFormat::Rgba8Unorm:
  case rhi::ImageFormat::Rgba8Srgb:
  case rhi::ImageFormat::Bgra8Unorm:
  case rhi::ImageFormat::Bgra8Srgb:
    return 4u;
  case rhi::ImageFormat::Rg8Unorm:
    return 2u;
  case rhi::ImageFormat::R8Unorm:
    return 1u;
  default:
    return 4u;
  }
}

const framegraph::CompiledResource *
compiledResourceForHandle(const FixedRenderGraph &graph, const framegraph::ResourceHandle handle) {
  if (handle.index < graph.resources.size() && graph.resources[handle.index].handle == handle) {
    return &graph.resources[handle.index];
  }
  const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
                                  [handle](const framegraph::CompiledResource &resource) {
                                    return resource.handle == handle;
                                  });
  return found == graph.resources.end() ? nullptr : &*found;
}

rhi::ImageExtent resourceCostExtent(const RenderGraphResource resource,
                                    const framegraph::ResourceDesc &desc,
                                    const RendererSettings &settings,
                                    const std::uint32_t frame_width,
                                    const std::uint32_t frame_height) {
  if (desc.extent.width > 1u || desc.extent.height > 1u) {
    return desc.extent;
  }
  switch (resource) {
  case RenderGraphResource::ShadowAtlas: {
    const std::uint32_t atlas = std::max(settings.shadows.atlas_size, 1u);
    return {atlas, atlas, 1u};
  }
  case RenderGraphResource::SurfaceAttributes:
  case RenderGraphResource::SurfaceOcclusion:
    return {std::max(frame_width, 1u), std::max(frame_height, 1u), 1u};
  case RenderGraphResource::VolumetricFog:
    return {std::max(frame_width / 4u, 1u), std::max(frame_height / 4u, 1u), 1u};
  case RenderGraphResource::ReflectionProbes: {
    const std::uint32_t face = std::max(settings.reflections.probe_resolution, 1u);
    return {face * 6u, face, 1u};
  }
  case RenderGraphResource::LightClusters:
    return {1u, 1u, 1u};
  default:
    return {std::max(frame_width, 1u), std::max(frame_height, 1u), 1u};
  }
}

std::uint64_t estimatedResourceBytes(const RenderGraphResource resource,
                                     const framegraph::ResourceDesc &desc,
                                     const RendererSettings &settings,
                                     const std::uint32_t frame_width,
                                     const std::uint32_t frame_height) {
  if (desc.kind == framegraph::ResourceKind::Buffer) {
    return desc.byte_size;
  }
  const rhi::ImageExtent extent =
      resourceCostExtent(resource, desc, settings, frame_width, frame_height);
  return static_cast<std::uint64_t>(std::max(extent.width, 1u)) *
         static_cast<std::uint64_t>(std::max(extent.height, 1u)) *
         static_cast<std::uint64_t>(std::max(extent.depth, 1u)) *
         imageFormatBytesPerPixel(desc.format);
}

std::uint64_t estimatePassBandwidthBytes(const FixedRenderGraph &graph,
                                         const framegraph::CompiledPass &pass,
                                         const RendererSettings &settings,
                                         const std::uint32_t frame_width,
                                         const std::uint32_t frame_height) {
  std::uint64_t bytes = 0u;
  const auto add_resource = [&](const framegraph::ResourceHandle handle) {
    const framegraph::CompiledResource *resource = compiledResourceForHandle(graph, handle);
    if (resource == nullptr) {
      return;
    }
    bytes += estimatedResourceBytes(renderGraphResourceFromName(resource->name), resource->desc,
                                    settings, frame_width, frame_height);
  };
  for (const framegraph::ResourceHandle read : pass.reads) {
    add_resource(read);
  }
  for (const framegraph::ResourceHandle write : pass.writes) {
    add_resource(write);
  }
  return bytes;
}

rhi::ImageExtent renderTargetExtentForPass(const FixedRenderGraph &graph,
                                           const framegraph::CompiledPass &pass,
                                           const RendererSettings &settings,
                                           const std::uint32_t frame_width,
                                           const std::uint32_t frame_height) {
  for (const framegraph::ResourceHandle write : pass.writes) {
    const framegraph::CompiledResource *resource = compiledResourceForHandle(graph, write);
    if (resource != nullptr && resource->desc.kind == framegraph::ResourceKind::Image) {
      return resourceCostExtent(renderGraphResourceFromName(resource->name), resource->desc,
                                settings, frame_width, frame_height);
    }
  }
  return {std::max(frame_width, 1u), std::max(frame_height, 1u), 1u};
}

bool realPresentationMode(const rhi::PresentationMode presentation) {
  return presentation == rhi::PresentationMode::SoftwareFramebuffer ||
         presentation == rhi::PresentationMode::MetalLayer ||
         presentation == rhi::PresentationMode::D3D12Swapchain;
}

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

void appendProjectionConventionDiagnostics(
    const aster::OrbitCamera &camera, const aster::RenderBackendCapabilities &capabilities,
    std::vector<aster::FrameDiagnosticEvent> &events) {
  const aster::ProjectionConvention camera_convention =
      aster::projectionConventionFromPolicy(camera.projection_policy);
  const aster::ProjectionConvention backend_convention = capabilities.projection_convention;
  if (camera_convention.handedness != backend_convention.handedness ||
      camera_convention.depth_range != backend_convention.depth_range ||
      camera_convention.depth_direction != backend_convention.depth_direction) {
    events.push_back({.kind = aster::FrameDiagnosticKind::ProjectionConventionMismatch,
                      .severity = aster::FrameDiagnosticSeverity::Warning,
                      .pass = "projection-contract",
                      .label = capabilities.name,
                      .message =
                          "Camera projection handedness/depth policy differs from backend.",
                      .value = static_cast<std::uint64_t>(capabilities.kind)});
  }
  if (camera_convention.viewport_origin != backend_convention.viewport_origin ||
      camera_convention.y_flip != backend_convention.y_flip) {
    events.push_back({.kind = aster::FrameDiagnosticKind::ViewportOriginMismatch,
                      .severity = aster::FrameDiagnosticSeverity::Warning,
                      .pass = "projection-contract",
                      .label = capabilities.name,
                      .message = "Camera viewport origin policy differs from backend.",
                      .value = static_cast<std::uint64_t>(capabilities.kind)});
  }
  if (camera_convention.matrix_storage != backend_convention.matrix_storage ||
      camera_convention.vector_convention != backend_convention.vector_convention ||
      backend_convention.matrix_storage != aster::MatrixStorageOrder::ColumnMajor ||
      backend_convention.vector_convention != aster::VectorConvention::ColumnVector) {
    events.push_back({.kind = aster::FrameDiagnosticKind::BackendProjectionDrift,
                      .severity = aster::FrameDiagnosticSeverity::Warning,
                      .pass = "projection-contract",
                      .label = capabilities.name,
                      .message =
                          "Camera/backend matrix storage or vector convention has drifted.",
                      .value = static_cast<std::uint64_t>(capabilities.kind)});
  }
}

void appendRenderMathContractReportDiagnostics(
    const aster::RenderMathContractReport &report,
    std::vector<aster::FrameDiagnosticEvent> &events) {
  if (report.valid) {
    return;
  }
  events.push_back({.kind = aster::FrameDiagnosticKind::MathContract,
                    .severity = aster::FrameDiagnosticSeverity::Error,
                    .pass = "math-contract",
                    .label = "canonical-render-contract",
                    .message =
                        "Canonical render math contract failed; inspect FrameForensics.math_contract.",
                    .value = report.issue_count});
}

std::uint64_t appendEvidenceText(std::uint64_t hash, std::string_view value);
void appendUnique(std::vector<std::string> &values, std::string value);
std::string materialAssetIdFor(const aster::RenderObject &object);
std::string textureRoleFate(const aster::MaterialRuntimeResource *runtime_material,
                            const aster::RenderBackendCapabilities &capabilities,
                            std::string_view role);

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

bool authoredTextureRole(const aster::MaterialRuntimeResource *resource,
                         const std::string_view role) {
  if (resource == nullptr) {
    return false;
  }
  const aster::RuntimeTexture *texture = resource->texture_set.find(role);
  return texture != nullptr && texture->valid && !texture->fallback;
}

bool materialDeclaresTextureRole(const aster::MaterialRuntimeResource *resource,
                                 const std::string_view role) {
  if (resource == nullptr) {
    return false;
  }
  for (const auto &[declared_role, slot] : resource->compiled.asset.textures) {
    (void)slot;
    if (aster::canonicalMaterialTextureRole(declared_role) == role) {
      return true;
    }
  }
  return false;
}

bool materialWantsNormalMap(const aster::MaterialRuntimeResource *resource) {
  if (resource == nullptr) {
    return false;
  }
  const aster::MaterialFeatureSet features = aster::materialFeatureSet(resource->compiled.asset);
  return features.normal_map;
}

std::string textureFallbackReason(const aster::MaterialRuntimeResource *resource,
                                  const aster::RuntimeTexture *texture,
                                  const std::string_view role) {
  if (texture == nullptr) {
    return "runtime texture role is absent from the material resource";
  }
  if (!texture->valid) {
    return "runtime texture failed validation";
  }
  if (!texture->fallback) {
    return {};
  }
  if (materialDeclaresTextureRole(resource, role)) {
    if (!texture->source_path.empty()) {
      return "authored texture source was unavailable: " + texture->source_path.generic_string();
    }
    return "authored texture source degraded to fallback";
  }
  return "role uses the renderer fallback texture";
}

std::string textureBackendDegradation(const aster::RenderBackendCapabilities &capabilities,
                                      const aster::RuntimeTexture *texture) {
  if (texture == nullptr || !texture->valid) {
    return "texture resource is not valid for backend binding";
  }
  if (!capabilities.supports_texture_sampling && !texture->fallback) {
    return std::string(aster::renderBackendKindName(capabilities.kind)) +
           " cannot sample authored material textures";
  }
  return {};
}

std::uint32_t expectedMipCount(const aster::RuntimeTexture &texture) {
  const std::uint32_t width =
      texture.mips.empty() ? texture.width : std::max(texture.mips.front().width, 1u);
  const std::uint32_t height =
      texture.mips.empty() ? texture.height : std::max(texture.mips.front().height, 1u);
  return aster::textureMipCount(std::max(width, 1u), std::max(height, 1u));
}

bool meshUv0LooksUnset(const aster::CpuMesh *mesh) {
  if (mesh == nullptr || mesh->vertices.empty()) {
    return false;
  }
  return std::all_of(mesh->vertices.begin(), mesh->vertices.end(), [](const aster::Vertex &vertex) {
    return std::abs(vertex.uv.x) <= 0.000001f && std::abs(vertex.uv.y) <= 0.000001f;
  });
}

bool meshHasInvalidTangentBasis(const aster::CpuMesh *mesh) {
  if (mesh == nullptr || mesh->vertices.empty()) {
    return false;
  }
  return std::any_of(mesh->vertices.begin(), mesh->vertices.end(), [](const aster::Vertex &vertex) {
    const aster::Vec3 tangent{vertex.tangent.x, vertex.tangent.y, vertex.tangent.z};
    return !aster::allFinite(tangent) || aster::length(tangent) <= 0.0001f ||
           !std::isfinite(vertex.tangent.w);
  });
}

void appendAssetIssue(std::vector<std::string> &issues, std::string issue) {
  appendUnique(issues, std::move(issue));
}

std::uint64_t assetFrameTraceHash(const aster::AssetFrameTrace &trace) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, trace.object_index);
  hash = appendEvidenceText(hash, trace.object_name);
  hash = appendEvidenceText(hash, trace.source_asset_id);
  hash = appendEvidenceText(hash, trace.source_graph_guid);
  hash = appendEvidenceText(hash, trace.source_graph_node);
  hash = appendEvidenceText(hash, trace.source_path);
  hash = appendEvidenceText(hash, trace.source_node);
  hash = appendEvidenceText(hash, trace.source_mesh);
  hash = appendEvidenceText(hash, trace.material_slot);
  hash = appendEvidenceText(hash, trace.shader_variant_key);
  hash = appendEvidenceText(hash, trace.pipeline_cache_key);
  hash = appendEvidenceText(hash, trace.procedural_capability_status);
  for (const std::string &issue : trace.issues) {
    hash = appendEvidenceText(hash, issue);
  }
  for (const std::string &role : trace.texture_roles) {
    hash = appendEvidenceText(hash, role);
  }
  for (const std::string &degradation : trace.backend_degradations) {
    hash = appendEvidenceText(hash, degradation);
  }
  return hash;
}

std::uint64_t materialNativePipelineKey(const aster::RenderObject &object,
                                        const aster::MaterialRuntimeResource *resource,
                                        const aster::RendererSettings &settings,
                                        const aster::RenderBackendCapabilities &capabilities) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto append = [&hash](const std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
  };
  append(static_cast<std::uint64_t>(capabilities.kind));
  append(static_cast<std::uint64_t>(aster::classifyMaterialRenderQueue(object.material)));
  append(static_cast<std::uint64_t>(object.material.alpha_mode));
  append(static_cast<std::uint64_t>(object.material.depth_write));
  append(static_cast<std::uint64_t>(object.material.depth_policy.layer));
  append(static_cast<std::uint64_t>(object.material.cull_mode));
  append(object.material.receives_shadows ? 1u : 0u);
  append(settings.shadows.enabled && object.material.receives_shadows ? 1u : 0u);
  append(settings.atmosphere.enabled && settings.atmosphere.fog_strength > 0.0f ? 1u : 0u);
  append(settings.reflections.enabled ? 1u : 0u);
  append(authoredTextureRole(resource, "normal") ? 1u : 0u);
  append(authoredTextureRole(resource, "height") ? 1u : 0u);
  append(authoredTextureRole(resource, "wetness") ? 1u : 0u);
  append(authoredTextureRole(resource, "emissive") ? 1u : 0u);
  append(authoredTextureRole(resource, "opacity") ? 1u : 0u);
  append(authoredTextureRole(resource, "orm") ? 1u : 0u);
  append(authoredTextureRole(resource, "roughness") ? 1u : 0u);
  append(authoredTextureRole(resource, "ao") ? 1u : 0u);
  append(object.material.shader_variant_key);
  append(aster::materialPermutationKey(object.material, resource != nullptr));
  return hash;
}

void appendMaterialPipelineKeyTraces(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                                     const aster::RendererSettings &settings,
                                     const aster::MaterialResourceLibrary *library,
                                     const aster::RenderBackendCapabilities &capabilities,
                                     std::vector<aster::rhi::PipelineStateTrace> &pipelines) {
  const std::uint64_t layout_hash = textureDescriptorLayoutHash();
  std::unordered_set<std::uint64_t> seen;
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
      const aster::MaterialRuntimeResource *resource =
          library == nullptr
              ? nullptr
              : library->findForMaterialIds(object.material_asset_id, object.material.asset_id);
      const std::uint64_t key = materialNativePipelineKey(object, resource, settings, capabilities);
      if (!seen.insert(key).second) {
        continue;
      }
      pipelines.push_back({.label = "material:" +
                                    (object.name.empty() ? std::string("unnamed") : object.name),
                           .cache_key = key,
                           .descriptor_layout_hash = layout_hash});
    }
  }
}

std::uint64_t estimatedBytesFor(const aster::framegraph::ResourceDesc &desc) {
  if (desc.kind == aster::framegraph::ResourceKind::Buffer) {
    return std::max<std::uint64_t>(desc.byte_size, desc.stride);
  }
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

aster::RendererDebugView debugViewForResource(const aster::RenderGraphResource resource) {
  switch (resource) {
  case aster::RenderGraphResource::ShadowAtlas:
    return aster::RendererDebugView::ShadowMask;
  case aster::RenderGraphResource::SurfaceAttributes:
    return aster::RendererDebugView::SurfaceAttributes;
  case aster::RenderGraphResource::SurfaceOcclusion:
    return aster::RendererDebugView::SurfaceOcclusion;
  case aster::RenderGraphResource::VolumetricFog:
    return aster::RendererDebugView::Fog;
  case aster::RenderGraphResource::ReflectionProbes:
    return aster::RendererDebugView::ReflectionProbe;
  default:
    return aster::RendererDebugView::FinalColor;
  }
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
    if (declaration != nullptr) {
      for (const aster::RenderGraphResourceBindingDesc &output : declaration->outputs) {
        const bool backend_has_output =
            (capabilities.graph_resource_mask & aster::renderGraphResourceBit(output.resource)) !=
            0u;
        if (!backend_has_output) {
          forensics.events.push_back(
              {.kind = aster::FrameDiagnosticKind::CapabilityMismatch,
               .severity = aster::FrameDiagnosticSeverity::Info,
               .pass = pass.name,
               .label = std::string(aster::renderGraphResourceName(output.resource)),
               .message =
                   "Backend does not advertise the render graph output resource for this pass.",
               .value = pass_index});
        }
      }
    }
    if (declaration != nullptr &&
        declaration->debug_capture != aster::RenderGraphDebugCapturePolicy::Disabled) {
      for (const aster::RenderGraphResourceBindingDesc &output : declaration->outputs) {
        forensics.captures.push_back(
            {.pass = semantic,
             .view = debugViewForResource(output.resource),
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
  const aster::framegraph::TransientResourceAllocationPlan allocation_plan =
      aster::framegraph::TransientResourceAllocator{}.allocate(graph);
  forensics.rhi_trace.memory.transient_bytes = allocation_plan.stats.transient_bytes;
  forensics.rhi_trace.memory.aliased_bytes_saved = allocation_plan.stats.aliased_bytes_saved;
  for (const aster::framegraph::TransientResourcePhysicalAllocation &allocation :
       allocation_plan.physical_allocations) {
    aster::rhi::TransientAllocationTrace trace;
    trace.label = "transient-allocation:" + std::to_string(allocation.id);
    trace.physical_allocation_id = allocation.id;
    trace.first_pass = allocation.first_pass;
    trace.last_pass = allocation.last_pass;
    trace.byte_size = allocation.byte_size;
    for (const aster::framegraph::ResourceHandle handle : allocation.resources) {
      if (const aster::framegraph::CompiledResource *resource = compiledResourceFor(graph, handle)) {
        trace.resources.push_back(resource->name);
      }
    }
    forensics.rhi_trace.transient_allocations.push_back(std::move(trace));
  }
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
    } else if (capture.resource == aster::RenderGraphResource::SurfaceAttributes &&
               resources.surface_attributes_ready) {
      updateCapturePayload(capture, resources.surface_width, resources.surface_height,
                           resources.surface_attributes_rgba8);
    } else if (capture.resource == aster::RenderGraphResource::SurfaceOcclusion &&
               resources.surface_occlusion_ready) {
      updateCapturePayload(capture, resources.occlusion_width, resources.occlusion_height,
                           resources.surface_occlusion_rgba8);
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

void appendFramebufferCapturePayloads(const aster::SoftwareFrameBuffer &framebuffer,
                                      aster::FrameForensics &forensics) {
  if (framebuffer.empty()) {
    return;
  }
  const std::vector<std::uint8_t> final_rgba(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  for (aster::FrameDebugCapture &capture : forensics.captures) {
    if (!capture.available &&
        (capture.resource == aster::RenderGraphResource::CaptureReadback ||
         capture.resource == aster::RenderGraphResource::SceneColor)) {
      updateCapturePayload(capture, static_cast<std::uint32_t>(std::max(framebuffer.width(), 0)),
                           static_cast<std::uint32_t>(std::max(framebuffer.height(), 0)),
                           final_rgba);
    }
  }
}

std::uint64_t proofHash(const std::uint64_t seed, const std::uint64_t value) {
  return appendEvidenceValue(seed, value);
}

bool hasAvailableCapture(const aster::FrameForensics &forensics,
                         const aster::RenderGraphResource resource) {
  return std::any_of(forensics.captures.begin(), forensics.captures.end(),
                     [resource](const aster::FrameDebugCapture &capture) {
                       return capture.resource == resource && capture.available &&
                              capture.content_hash != 0u;
                     });
}

bool hasResourceTrace(const aster::FrameForensics &forensics,
                      const aster::RenderGraphResource resource, const bool write) {
  return std::any_of(forensics.resource_traces.begin(), forensics.resource_traces.end(),
                     [resource, write](const aster::FrameResourceTrace &trace) {
                       return trace.resource == resource && trace.write == write;
                     });
}

bool hasReadAfterWriteEvidence(const aster::FrameForensics &forensics,
                               const aster::RenderGraphResource resource) {
  bool saw_write = false;
  for (const aster::FrameResourceTrace &trace : forensics.resource_traces) {
    if (trace.resource != resource) {
      continue;
    }
    if (trace.write) {
      saw_write = true;
    } else if (saw_write) {
      return true;
    }
  }
  return false;
}

bool requiresNativeCaptureProof(const aster::RenderGraphResource resource) {
  return resource == aster::RenderGraphResource::ShadowAtlas ||
         resource == aster::RenderGraphResource::SurfaceOcclusion ||
         resource == aster::RenderGraphResource::VolumetricFog ||
         resource == aster::RenderGraphResource::ReflectionProbes ||
         resource == aster::RenderGraphResource::CaptureReadback;
}

void appendProof(aster::FrameForensics &forensics, const aster::BackendFeatureProofKind kind,
                 const aster::BackendFeatureProofStatus status, std::string feature,
                 std::string label, std::string message,
                 const aster::RenderGraphResource resource = aster::RenderGraphResource::SceneColor,
                 const aster::RenderGraphPass pass = aster::RenderGraphPass::SceneColorDepth,
                 const std::uint32_t advertised = 0u, const std::uint32_t native = 0u,
                 const std::uint64_t evidence_hash = 0u) {
  forensics.backend_feature_proofs.push_back({.kind = kind,
                                              .status = status,
                                              .resource = resource,
                                              .pass = pass,
                                              .feature = std::move(feature),
                                              .label = std::move(label),
                                              .message = std::move(message),
                                              .advertised = advertised,
                                              .native = native,
                                              .evidence_hash = evidence_hash});
}

void appendPassArtifacts(aster::FrameForensics &forensics) {
  forensics.pass_artifacts.clear();
  for (const aster::FrameDebugCapture &capture : forensics.captures) {
    forensics.pass_artifacts.push_back({.pass = capture.pass,
                                        .resource = capture.resource,
                                        .label = capture.label,
                                        .kind = "debug-capture",
                                        .width = capture.width,
                                        .height = capture.height,
                                        .content_hash = capture.content_hash,
                                        .available = capture.available});
  }
  for (const aster::rhi::PipelineStateTrace &pipeline : forensics.rhi_trace.pipelines) {
    forensics.pass_artifacts.push_back({.pass = aster::renderGraphPassFromName(pipeline.label),
                                        .label = pipeline.label,
                                        .kind = "pipeline-state",
                                        .content_hash = pipeline.cache_key,
                                        .available = pipeline.cache_key != 0u});
  }
}

bool graphResourceExercised(const aster::RenderGraphResource resource,
                            const aster::RendererSettings &settings) {
  switch (resource) {
  case aster::RenderGraphResource::ShadowAtlas:
    return settings.shadows.enabled;
  case aster::RenderGraphResource::SurfaceOcclusion:
    return settings.occlusion.enabled && settings.occlusion.strength > 0.0f;
  case aster::RenderGraphResource::VolumetricFog:
    return settings.atmosphere.enabled && settings.atmosphere.fog_strength > 0.0f;
  case aster::RenderGraphResource::ReflectionProbes:
    return settings.reflections.enabled && settings.reflections.static_local_probes;
  default:
    return true;
  }
}

void appendResourceCertification(const aster::FixedRenderGraph &graph,
                                 const aster::RenderBackendCapabilities &capabilities,
                                 const aster::RendererSettings &settings,
                                 aster::FrameForensics &forensics) {
  for (const aster::framegraph::CompiledResource &resource_node : graph.resources) {
    const aster::RenderGraphResource resource =
        aster::renderGraphResourceFromName(resource_node.name);
    const bool advertised =
        (capabilities.graph_resource_mask & aster::renderGraphResourceBit(resource)) != 0u;
    if (!advertised) {
      appendProof(forensics, aster::BackendFeatureProofKind::GraphResource,
                  aster::BackendFeatureProofStatus::NotAdvertised,
                  std::string(aster::renderGraphResourceName(resource)), resource_node.name,
                  "Backend does not advertise this graph resource.", resource,
                  aster::RenderGraphPass::SceneColorDepth, 0u, 0u);
      continue;
    }
    if (!graphResourceExercised(resource, settings)) {
      appendProof(forensics, aster::BackendFeatureProofKind::GraphResource,
                  aster::BackendFeatureProofStatus::NotExercised,
                  std::string(aster::renderGraphResourceName(resource)), resource_node.name,
                  "Advertised graph resource was not exercised by this frame's settings.",
                  resource, aster::RenderGraphPass::SceneColorDepth, 1u, 0u);
      continue;
    }

    const bool write_trace =
        resource_node.desc.lifetime == aster::framegraph::ResourceLifetime::Imported ||
        hasResourceTrace(forensics, resource, true);
    const bool capture_ok = !requiresNativeCaptureProof(resource) || hasAvailableCapture(forensics, resource);
    const bool sampling_ok =
        resource == aster::RenderGraphResource::ShadowAtlas ||
                resource == aster::RenderGraphResource::SurfaceOcclusion ||
                resource == aster::RenderGraphResource::VolumetricFog ||
                resource == aster::RenderGraphResource::ReflectionProbes
            ? hasReadAfterWriteEvidence(forensics, resource)
            : true;
    const bool proven = write_trace && capture_ok && sampling_ok;
    appendProof(forensics, aster::BackendFeatureProofKind::GraphResource,
                proven ? aster::BackendFeatureProofStatus::Proven
                       : aster::BackendFeatureProofStatus::MissingProof,
                std::string(aster::renderGraphResourceName(resource)), resource_node.name,
                proven ? "Advertised graph resource produced required frame evidence."
                       : "Advertised graph resource is missing write, capture, or sampling proof.",
                resource, aster::RenderGraphPass::SceneColorDepth, 1u, proven ? 1u : 0u,
                proofHash(resource_node.desc.usage, hasAvailableCapture(forensics, resource) ? 1u : 0u));
  }
}

void appendCapabilityCertification(const aster::RenderBackendCapabilities &capabilities,
                                   const aster::RendererSettings &settings,
                                   const aster::FrameStats &stats,
                                   aster::FrameForensics &forensics) {
  const bool capture_proven = hasAvailableCapture(forensics, aster::RenderGraphResource::CaptureReadback) ||
                              hasAvailableCapture(forensics, aster::RenderGraphResource::SceneColor);
  appendProof(forensics, aster::BackendFeatureProofKind::Capture,
              capabilities.supports_capture
                  ? (capture_proven ? aster::BackendFeatureProofStatus::Proven
                                    : aster::BackendFeatureProofStatus::MissingProof)
                  : aster::BackendFeatureProofStatus::NotAdvertised,
              "capture", "frame-capture",
              capture_proven ? "Frame capture payload is available."
                             : "No frame capture payload was produced for this frame.",
              aster::RenderGraphResource::CaptureReadback, aster::RenderGraphPass::Capture,
              capabilities.supports_capture ? 1u : 0u, capture_proven ? 1u : 0u);

  const bool texture_exercised =
      std::any_of(forensics.material_bindings.begin(), forensics.material_bindings.end(),
                  [](const aster::MaterialBindingTrace &binding) {
                    return binding.valid && binding.bound && !binding.fallback;
                  });
  appendProof(forensics, aster::BackendFeatureProofKind::TextureSampling,
              capabilities.supports_texture_sampling
                  ? (texture_exercised ? aster::BackendFeatureProofStatus::Proven
                                       : aster::BackendFeatureProofStatus::NotExercised)
                  : aster::BackendFeatureProofStatus::NotAdvertised,
              "texture-sampling", "material-bindings",
              texture_exercised ? "At least one authored material texture was bound."
                                : "This frame did not exercise authored texture sampling.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::Opaque,
              capabilities.supports_texture_sampling ? 1u : 0u, texture_exercised ? 1u : 0u);

  const bool instancing_exercised = stats.instance_groups > 0u && stats.visible_objects > 1u;
  appendProof(forensics, aster::BackendFeatureProofKind::Instancing,
              capabilities.supports_instancing
                  ? (instancing_exercised ? aster::BackendFeatureProofStatus::Proven
                                          : aster::BackendFeatureProofStatus::NotExercised)
                  : aster::BackendFeatureProofStatus::NotAdvertised,
              "instancing", "draw-groups",
              instancing_exercised ? "Frame contains grouped draw instances."
                                   : "This frame did not require backend instancing.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::Opaque,
              capabilities.supports_instancing ? 1u : 0u, instancing_exercised ? 1u : 0u);

  const bool timestamp_proven =
      std::any_of(forensics.timestamp_samples.begin(), forensics.timestamp_samples.end(),
                  [](const aster::rhi::TimestampQueryResult &sample) {
                    return sample.available;
                  });
  appendProof(forensics, aster::BackendFeatureProofKind::GpuTimestamps,
              capabilities.supports_gpu_timestamps
                  ? (timestamp_proven ? aster::BackendFeatureProofStatus::Proven
                                      : aster::BackendFeatureProofStatus::MissingProof)
                  : aster::BackendFeatureProofStatus::Unsupported,
              "gpu-timestamps", "timestamp-query",
              timestamp_proven ? "GPU timestamp samples were resolved."
                               : "GPU timestamp queries are not available for this backend.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::SceneColorDepth,
              capabilities.supports_gpu_timestamps ? 1u : 0u, timestamp_proven ? 1u : 0u);

  const bool hdr_graph =
      std::any_of(forensics.rhi_trace.pipelines.begin(), forensics.rhi_trace.pipelines.end(),
                  [](const aster::rhi::PipelineStateTrace &pipeline) {
                    return pipeline.cache_key != 0u;
                  });
  appendProof(forensics, aster::BackendFeatureProofKind::HdrRenderTarget,
              capabilities.capability_table.hdr_render_targets
                  ? (settings.post.hdr_scene_color && hdr_graph
                         ? aster::BackendFeatureProofStatus::Proven
                         : aster::BackendFeatureProofStatus::NotExercised)
                  : aster::BackendFeatureProofStatus::Unsupported,
              "hdr-render-target", "scene-color",
              capabilities.capability_table.hdr_render_targets
                  ? "HDR scene-color contract is represented in the frame graph."
                  : "Backend does not advertise native HDR render targets.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::SceneColorDepth,
              capabilities.capability_table.hdr_render_targets ? 1u : 0u,
              settings.post.hdr_scene_color && hdr_graph ? 1u : 0u);

  appendProof(forensics, aster::BackendFeatureProofKind::Msaa,
              capabilities.capability_table.msaa ? aster::BackendFeatureProofStatus::MissingProof
                                                 : aster::BackendFeatureProofStatus::Unsupported,
              "msaa", "sample-count",
              capabilities.capability_table.msaa
                  ? "MSAA is advertised but no resolve proof exists yet."
                  : "Backend does not advertise MSAA.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::SceneColorDepth,
              capabilities.capability_table.msaa ? 1u : 0u, 0u);

  const bool presentation_surface_available =
      realPresentationMode(capabilities.capability_table.presentation);
  const bool waits_for_explicit_d3d12_present =
      capabilities.kind == aster::RenderBackendKind::D3D12 &&
      capabilities.capability_table.presentation == aster::rhi::PresentationMode::D3D12Swapchain;
  appendProof(forensics, aster::BackendFeatureProofKind::Presentation,
              presentation_surface_available
                  ? (waits_for_explicit_d3d12_present
                         ? aster::BackendFeatureProofStatus::NotExercised
                         : aster::BackendFeatureProofStatus::Proven)
                  : aster::BackendFeatureProofStatus::Unsupported,
              "presentation", "presentation-mode",
              presentation_surface_available
                  ? (waits_for_explicit_d3d12_present
                         ? "D3D12 swapchain is bound; explicit present proof is pending."
                         : "Backend reports a real presentation surface.")
                  : "Backend has no swapchain/window presentation proof; offscreen readback "
                    "does not count as presentation.",
              aster::RenderGraphResource::SceneColor, aster::RenderGraphPass::UiComposite,
              presentation_surface_available ? 1u : 0u,
              presentation_surface_available && !waits_for_explicit_d3d12_present ? 1u : 0u);
}

void certifyBackendFrame(const aster::FixedRenderGraph &graph,
                         const aster::RenderBackendCapabilities &capabilities,
                         const aster::RendererSettings &settings,
                         const aster::FrameStats &stats,
                         aster::FrameForensics &forensics) {
  forensics.backend_feature_proofs.clear();
  forensics.rhi_validation_events.clear();
  forensics.timestamp_samples.clear();

  const aster::rhi::ResourceLifetimeValidationReport validation =
      aster::rhi::ResourceLifetimeValidator{}.validateFrameGraph(graph);
  forensics.rhi_validation_events = validation.events;
  for (const aster::rhi::ResourceLifetimeValidationEvent &event : validation.events) {
    forensics.events.push_back(
        {.kind = aster::FrameDiagnosticKind::ResourceLifetimeHazard,
         .severity = event.severity == aster::rhi::ResourceLifetimeValidationSeverity::Error
                         ? aster::FrameDiagnosticSeverity::Error
                         : aster::FrameDiagnosticSeverity::Warning,
         .pass = event.pass,
         .label = event.resource,
         .message = event.message,
         .value = event.resource_id});
  }

  if (capabilities.supports_gpu_timestamps && !forensics.rhi_trace.timestamps.empty()) {
    forensics.timestamp_samples = forensics.rhi_trace.timestamps;
  } else if (!capabilities.supports_gpu_timestamps) {
    aster::GpuFrameProfiler profiler(false);
    profiler.recordUnavailable(static_cast<std::uint32_t>(graph.passes.size()));
    forensics.timestamp_samples = profiler.samples();
    forensics.rhi_trace.timestamps = forensics.timestamp_samples;
    forensics.events.push_back({.kind = aster::FrameDiagnosticKind::BackendFallback,
                                .severity = aster::FrameDiagnosticSeverity::Info,
                                .pass = "frame",
                                .label = "gpu-timestamps",
                                .message =
                                    "Backend does not advertise GPU timestamp queries."});
  }

  appendPassArtifacts(forensics);
  appendResourceCertification(graph, capabilities, settings, forensics);
  appendCapabilityCertification(capabilities, settings, stats, forensics);

  forensics.certification = {.backend = capabilities.kind,
                             .valid = true,
                             .proof_count = forensics.backend_feature_proofs.size(),
                             .validation_error_count = validation.errorCount(),
                             .math_contract_error_count =
                                 forensics.math_contract.valid
                                     ? 0u
                                     : std::max<std::size_t>(forensics.math_contract.issue_count,
                                                             1u)};
  for (const aster::BackendFeatureProof &proof : forensics.backend_feature_proofs) {
    if (proof.status == aster::BackendFeatureProofStatus::Proven) {
      ++forensics.certification.proven_count;
    } else if (proof.status == aster::BackendFeatureProofStatus::MissingProof) {
      ++forensics.certification.missing_proof_count;
    }
  }
  forensics.certification.valid =
      forensics.certification.missing_proof_count == 0u &&
      forensics.certification.validation_error_count == 0u &&
      forensics.certification.math_contract_error_count == 0u;
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
        const std::string degradation = textureBackendDegradation(capabilities, texture);
        traces.push_back({.object_name = object.name,
                          .material_asset_id = object.material_asset_id.empty()
                                                   ? object.material.asset_id
                                                   : object.material_asset_id,
                          .role = std::string(role),
                          .source_path = texture == nullptr ? std::string()
                                                            : texture->source_path.generic_string(),
                          .texture_kind =
                              texture == nullptr
                                  ? std::string()
                                  : std::string(aster::textureKindName(texture->kind)),
                          .color_space =
                              texture == nullptr
                                  ? std::string()
                                  : std::string(aster::textureColorSpaceName(texture->color_space)),
                          .fallback_reason = textureFallbackReason(runtime_material, texture, role),
                          .backend_degradation = degradation,
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

void appendAssetFrameTraces(const aster::Scene &scene,
                            const aster::MaterialResourceLibrary *library,
                            const aster::RenderBackendCapabilities &capabilities,
                            aster::FrameForensics &forensics) {
  forensics.asset_traces.clear();
  forensics.asset_traces.reserve(scene.objects().size());

  for (std::size_t object_index = 0u; object_index < scene.objects().size(); ++object_index) {
    const aster::RenderObject &object = scene.objects()[object_index];
    const aster::MaterialRuntimeResource *runtime_material =
        library == nullptr ? nullptr : library->findForMaterialIds(object.material_asset_id,
                                                                   object.material.asset_id);
    const aster::RenderObjectAssetProvenance &provenance = object.asset_provenance;
    aster::AssetFrameTrace trace;
    trace.object_name = objectDiagnosticLabel(object, object_index);
    trace.object_index = object_index;
    trace.source_asset_id =
        provenance.source_asset_id.empty() ? materialAssetIdFor(object) : provenance.source_asset_id;
    if (!provenance.source_path.empty()) {
      trace.source_path = provenance.source_path.generic_string();
    } else if (runtime_material != nullptr &&
               !runtime_material->compiled.asset.source_path.empty()) {
      trace.source_path = runtime_material->compiled.asset.source_path.generic_string();
    }
    trace.source_node = provenance.source_node;
    trace.source_mesh = provenance.source_mesh;
    trace.material_slot = provenance.material_slot.empty() && runtime_material != nullptr
                              ? runtime_material->compiled.asset.name
                              : provenance.material_slot;
    trace.source_graph_guid = object.material.procedural_graph_guid;
    trace.source_graph_node = object.material.procedural_graph_node;
    trace.procedural_capability_status = object.material.procedural_capability_status;
    trace.shader_variant_key = std::to_string(object.material.shader_variant_key);
    const aster::CompiledMaterial compiled_trace =
        aster::compileMaterialForRendering(object.material, runtime_material != nullptr,
                                           materialAssetIdFor(object));
    trace.pipeline_cache_key = std::to_string(compiled_trace.permutation_key);
    if (!trace.source_graph_guid.empty()) {
      if (trace.source_asset_id.empty() || trace.source_asset_id == materialAssetIdFor(object)) {
        trace.source_asset_id = trace.source_graph_guid;
      }
      if (trace.source_node.empty()) {
        trace.source_node = trace.source_graph_node;
      }
      if (trace.procedural_capability_status.find("unsupported") != std::string::npos) {
        appendAssetIssue(trace.issues, "procedural:node-unsupported-by-runtime");
      } else {
        appendUnique(trace.backend_degradations,
                     std::string(aster::renderBackendKindName(capabilities.kind)) +
                         ":procedural-material-reference-path");
      }
    }

    const bool material_has_texture_declarations =
        runtime_material != nullptr && !runtime_material->compiled.asset.textures.empty();
    const bool normal_map_expected = materialWantsNormalMap(runtime_material);
    if (object.custom_mesh != nullptr && material_has_texture_declarations) {
      if (!provenance.uv0_present) {
        appendAssetIssue(trace.issues, "mesh:uv0-missing-required-by-material-textures");
      } else if (meshUv0LooksUnset(object.custom_mesh.get())) {
        appendAssetIssue(trace.issues, "mesh:uv0-all-zero-or-unset-for-textured-material");
      }
    }
    if (provenance.degenerate_triangles > 0u) {
      appendAssetIssue(trace.issues,
                       "mesh:degenerate-triangles-dropped=" +
                           std::to_string(provenance.degenerate_triangles));
    }
    if (provenance.invalid_normals > 0u) {
      appendAssetIssue(trace.issues,
                       "mesh:invalid-normals-rebuilt=" +
                           std::to_string(provenance.invalid_normals));
    }
    if (normal_map_expected) {
      if (!authoredTextureRole(runtime_material, "normal")) {
        appendAssetIssue(trace.issues, "texture:normal:fallback-source-missing");
        appendUnique(trace.backend_degradations,
                     std::string(aster::renderBackendKindName(capabilities.kind)) +
                         ":normal-map-degraded-to-vertex-normal");
      }
      if ((!provenance.authored_tangent_basis && provenance.generated_tangents == 0u) ||
          meshHasInvalidTangentBasis(object.custom_mesh.get())) {
        appendAssetIssue(trace.issues, "mesh:tangent-basis-missing-for-normal-map");
      } else if (!provenance.authored_tangent_basis && provenance.generated_tangents > 0u) {
        appendAssetIssue(trace.issues,
                         "mesh:tangent-basis-generated-by-importer=" +
                             std::to_string(provenance.generated_tangents));
      }
      const aster::Vec3 scale = object.transform.scale;
      if (scale.x * scale.y * scale.z < 0.0f) {
        appendAssetIssue(trace.issues, "mesh:tangent-basis-flipped-by-negative-scale");
      }
    }

    if (runtime_material != nullptr) {
      for (const auto &[declared_role, slot] : runtime_material->compiled.asset.textures) {
        (void)slot;
        const std::string canonical_role(
            aster::canonicalMaterialTextureRole(declared_role));
        if (aster::textureKindForRole(canonical_role) == aster::TextureKind::Unknown) {
          appendAssetIssue(trace.issues,
                           "texture:" + declared_role + ":unknown-role-not-bound");
        }
      }
    }

    for (const std::string_view role : aster::materialRuntimeTextureRoles()) {
      const aster::RuntimeTexture *texture =
          runtime_material == nullptr ? nullptr : runtime_material->texture_set.find(role);
      trace.texture_roles.push_back(textureRoleFate(runtime_material, capabilities, role));
      if (!materialDeclaresTextureRole(runtime_material, role)) {
        continue;
      }
      if (texture == nullptr || !texture->valid || texture->fallback) {
        appendAssetIssue(trace.issues,
                         "texture:" + std::string(role) + ":fallback-source-missing");
        continue;
      }
      const std::uint32_t expected_mips = expectedMipCount(*texture);
      const std::uint32_t actual_mips = static_cast<std::uint32_t>(texture->mips.size());
      if (actual_mips < expected_mips) {
        appendAssetIssue(trace.issues,
                         "texture:" + std::string(role) + ":mip-chain-incomplete expected=" +
                             std::to_string(expected_mips) + " actual=" +
                             std::to_string(actual_mips));
      }
      const aster::TextureColorSpace expected_color_space =
          aster::defaultTextureColorSpace(texture->kind);
      if (texture->color_space != expected_color_space) {
        appendAssetIssue(trace.issues,
                         "texture:" + std::string(role) + ":color-space-" +
                             std::string(aster::textureColorSpaceName(texture->color_space)) +
                             "-expected-" +
                             std::string(aster::textureColorSpaceName(expected_color_space)));
      }
      const std::string degradation = textureBackendDegradation(capabilities, texture);
      if (!degradation.empty()) {
        appendUnique(trace.backend_degradations, degradation);
      }
    }

    trace.trace_hash = assetFrameTraceHash(trace);
    for (const std::string &issue : trace.issues) {
      const aster::FrameDiagnosticKind kind =
          issue.rfind("texture:", 0u) == 0u
              ? aster::FrameDiagnosticKind::TextureRoleDegraded
              : (issue.rfind("mesh:", 0u) == 0u
                     ? aster::FrameDiagnosticKind::MeshAttributeDegraded
                     : aster::FrameDiagnosticKind::AssetProvenanceWarning);
      forensics.events.push_back({.kind = kind,
                                  .severity = aster::FrameDiagnosticSeverity::Warning,
                                  .pass = "asset-frame-trace",
                                  .label = trace.object_name,
                                  .message = issue,
                                  .value = object_index});
    }
    forensics.asset_traces.push_back(std::move(trace));
  }
}

std::uint64_t appendEvidenceText(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash = appendEvidenceValue(hash, static_cast<std::uint8_t>(c));
  }
  return appendEvidenceValue(hash, value.size());
}

void appendUnique(std::vector<std::string> &values, std::string value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

std::string materialAssetIdFor(const aster::RenderObject &object) {
  if (!object.material_asset_id.empty()) {
    return object.material_asset_id;
  }
  return object.material.asset_id;
}

aster::RenderGraphPass graphPassFor(const aster::FrameRenderPass pass) {
  return pass == aster::FrameRenderPass::Transparent ? aster::RenderGraphPass::Transparent
                                                     : aster::RenderGraphPass::Opaque;
}

std::string textureRoleFate(const aster::MaterialRuntimeResource *runtime_material,
                            const aster::RenderBackendCapabilities &capabilities,
                            const std::string_view role) {
  const aster::RuntimeTexture *texture =
      runtime_material == nullptr ? nullptr : runtime_material->texture_set.find(role);
  if (texture == nullptr) {
    return std::string(role) + ":missing";
  }
  if (!texture->valid) {
    return std::string(role) + ":invalid";
  }
  if (texture->fallback) {
    return std::string(role) + ":fallback";
  }
  return std::string(role) + (capabilities.supports_texture_sampling ? ":bound" : ":unsupported");
}

std::uint64_t objectFateHash(const aster::ObjectRenderFateTrace &fate) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, fate.object_index);
  hash = appendEvidenceValue(hash, fate.visible ? 1u : 0u);
  hash = appendEvidenceText(hash, fate.object_name);
  hash = appendEvidenceText(hash, fate.mesh_key);
  hash = appendEvidenceText(hash, fate.material_key);
  hash = appendEvidenceText(hash, fate.material_asset_id);
  hash = appendEvidenceText(hash, fate.shader_variant_key);
  hash = appendEvidenceText(hash, fate.pipeline_tag);
  hash = appendEvidenceText(hash, fate.source_graph_guid);
  hash = appendEvidenceText(hash, fate.source_graph_node);
  hash = appendEvidenceText(hash, fate.pipeline_cache_key);
  hash = appendEvidenceText(hash, fate.procedural_capability_status);
  hash = appendEvidenceText(hash, fate.asset_source_path);
  hash = appendEvidenceText(hash, fate.asset_source_node);
  hash = appendEvidenceText(hash, fate.asset_source_mesh);
  hash = appendEvidenceText(hash, fate.asset_material_slot);
  for (const std::string &value : fate.asset_issues) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.backend_degradations) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.texture_roles) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.pass_list) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.resource_transitions) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.capture_labels) {
    hash = appendEvidenceText(hash, value);
  }
  for (const std::string &value : fate.feature_proofs) {
    hash = appendEvidenceText(hash, value);
  }
  return appendEvidenceText(hash, fate.final_contribution);
}

std::uint64_t surfacePresentationHash(const aster::SurfacePresentationTrace &trace) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, trace.object_index);
  hash = appendEvidenceText(hash, trace.object_name);
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(
                                       std::lround(trace.physical_texel_density * 10.0f)));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(
                                       std::lround(trace.height_normal_coupling * 1000.0f)));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(
                                       std::lround(trace.roughness_height_coupling * 1000.0f)));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(
                                       std::lround(trace.cavity_strength * 1000.0f)));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(
                                       std::lround(trace.contact_hardening * 1000.0f)));
  hash = appendEvidenceValue(hash, trace.surface_occlusion_enabled ? 1u : 0u);
  return appendEvidenceValue(hash, trace.contact_shadow_receiver ? 1u : 0u);
}

void appendSurfacePresentationTraces(const aster::Scene &scene,
                                     const aster::RendererSettings &settings,
                                     aster::FrameForensics &forensics) {
  forensics.surface_traces.clear();
  forensics.surface_traces.reserve(scene.objects().size());
  for (std::size_t object_index = 0u; object_index < scene.objects().size(); ++object_index) {
    const aster::RenderObject &object = scene.objects()[object_index];
    if (isContactShadowUtility(object)) {
      continue;
    }
    const aster::ProceduralSurfaceLayer &surface = object.material.procedural;
    aster::SurfacePresentationTrace trace;
    trace.object_name = objectDiagnosticLabel(object, object_index);
    trace.object_index = object_index;
    trace.physical_texel_density = surface.physical_texel_density > 0.0f
                                       ? surface.physical_texel_density
                                       : settings.surface_scale.physical_texel_density;
    trace.height_normal_coupling = surface.height_normal_coupling > 0.0f
                                       ? surface.height_normal_coupling
                                       : settings.surface_scale.height_normal_coupling;
    trace.roughness_height_coupling = surface.roughness_height_coupling > 0.0f
                                          ? surface.roughness_height_coupling
                                          : settings.surface_scale.roughness_height_coupling;
    trace.cavity_strength = std::clamp(surface.cavity_grime + object.material.edge_wear * 0.35f,
                                       0.0f, 1.0f);
    trace.contact_hardening = settings.occlusion.contact_hardening;
    trace.scale_reference_m = settings.presentation.scale_reference_m;
    trace.surface_occlusion_enabled = settings.occlusion.enabled;
    trace.contact_shadow_receiver = object.material.receives_shadows &&
                                    settings.grounding.enabled &&
                                    settings.grounding.contact_shadows;
    trace.trace_hash = surfacePresentationHash(trace);
    if (settings.occlusion.enabled &&
        trace.physical_texel_density < settings.surface_scale.physical_texel_density * 0.50f) {
      forensics.events.push_back(
          {.kind = aster::FrameDiagnosticKind::SurfacePresentationWarning,
           .severity = aster::FrameDiagnosticSeverity::Warning,
           .pass = "surface-presentation",
           .label = trace.object_name,
           .message = "Object surface texel density is below the presentation scale policy.",
           .value = object_index});
    }
    forensics.surface_traces.push_back(std::move(trace));
  }
}

void appendObjectRenderFateTraces(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                                  const aster::RendererSettings &settings,
                                  const aster::MaterialResourceLibrary *library,
                                  const aster::RenderBackendCapabilities &capabilities,
                                  aster::FrameForensics &forensics) {
  forensics.object_fates.clear();
  forensics.object_fates.reserve(scene.objects().size());
  for (std::size_t object_index = 0u; object_index < scene.objects().size(); ++object_index) {
    const aster::RenderObject &object = scene.objects()[object_index];
    const std::string material_asset_id = materialAssetIdFor(object);
    const aster::MaterialRuntimeResource *runtime_material =
        library == nullptr ? nullptr : library->findForMaterialIds(object.material_asset_id,
                                                                   object.material.asset_id);
    const aster::CompiledMaterial compiled =
        aster::compileMaterialForRendering(object.material, runtime_material != nullptr,
                                           material_asset_id);
    aster::ObjectRenderFateTrace fate;
    fate.object_name = objectDiagnosticLabel(object, object_index);
    fate.object_index = object_index;
    fate.mesh_key = std::to_string(aster::renderMeshIdForObject(object).value);
    fate.material_key = std::to_string(aster::renderMaterialKeyForObject(object).value);
    fate.material_asset_id = material_asset_id;
    fate.shader_variant_key = std::to_string(object.material.shader_variant_key);
    fate.pipeline_tag = compiled.pipeline_tag;
    fate.source_graph_guid = object.material.procedural_graph_guid;
    fate.source_graph_node = object.material.procedural_graph_node;
    fate.pipeline_cache_key = std::to_string(compiled.permutation_key);
    fate.procedural_capability_status = object.material.procedural_capability_status;
    const auto asset_trace =
        std::find_if(forensics.asset_traces.begin(), forensics.asset_traces.end(),
                     [object_index](const aster::AssetFrameTrace &trace) {
                       return trace.object_index == object_index;
                     });
    if (asset_trace != forensics.asset_traces.end()) {
      fate.asset_source_path = asset_trace->source_path;
      fate.asset_source_node = asset_trace->source_node;
      fate.asset_source_mesh = asset_trace->source_mesh;
      fate.asset_material_slot = asset_trace->material_slot;
      fate.source_graph_guid = asset_trace->source_graph_guid;
      fate.source_graph_node = asset_trace->source_graph_node;
      fate.pipeline_cache_key = asset_trace->pipeline_cache_key;
      fate.procedural_capability_status = asset_trace->procedural_capability_status;
      fate.asset_issues = asset_trace->issues;
      fate.backend_degradations = asset_trace->backend_degradations;
    }
    for (const std::string_view role : aster::materialRuntimeTextureRoles()) {
      fate.texture_roles.push_back(textureRoleFate(runtime_material, capabilities, role));
    }
    for (const aster::FrameRenderDrawGroup &group : plan.groups) {
      for (std::size_t i = 0u; i < group.instance_count; ++i) {
        const std::size_t plan_index = group.first_instance + i;
        if (plan_index >= plan.instances.size()) {
          break;
        }
        const aster::FrameRenderInstance &instance = plan.instances[plan_index];
        if (instance.object_index != object_index) {
          continue;
        }
        fate.visible = true;
        appendUnique(fate.pass_list,
                     std::string(aster::renderGraphPassName(graphPassFor(group.pass))));
      }
    }
    if (settings.shadows.enabled && object.material.receives_shadows &&
        aster::renderObjectCastsShadows(object)) {
      appendUnique(fate.pass_list, std::string(aster::renderGraphPassName(
                                       aster::RenderGraphPass::ShadowAtlas)));
    }
    for (const aster::FrameResourceTrace &trace : forensics.resource_traces) {
      const std::string label = trace.pass_name + ":" + trace.resource_name + ":" +
                                (trace.write ? "write" : "read");
      appendUnique(fate.resource_transitions, label);
    }
    fate.final_contribution = fate.visible ? "planned" : "culled";
    fate.contribution_hash = objectFateHash(fate);
    forensics.object_fates.push_back(std::move(fate));
  }
}

void finalizeObjectRenderFates(aster::FrameForensics &forensics) {
  std::vector<std::string> capture_labels;
  for (const aster::FrameDebugCapture &capture : forensics.captures) {
    capture_labels.push_back(capture.label + (capture.available ? ":available" : ":missing"));
  }
  std::vector<std::string> proof_labels;
  for (const aster::BackendFeatureProof &proof : forensics.backend_feature_proofs) {
    proof_labels.push_back(proof.label + ":" +
                           std::string(aster::backendFeatureProofStatusName(proof.status)));
  }
  for (aster::ObjectRenderFateTrace &fate : forensics.object_fates) {
    fate.capture_labels = capture_labels;
    fate.feature_proofs = proof_labels;
    if (!fate.visible) {
      fate.final_contribution = "culled";
    } else if (std::any_of(forensics.captures.begin(), forensics.captures.end(),
                           [](const aster::FrameDebugCapture &capture) {
                             return capture.available &&
                                    (capture.resource == aster::RenderGraphResource::SceneColor ||
                                     capture.resource ==
                                         aster::RenderGraphResource::CaptureReadback);
                           })) {
      fate.final_contribution = "contributed";
    } else {
      fate.final_contribution = "planned-no-final-capture";
    }
    fate.contribution_hash = objectFateHash(fate);
  }
}

bool containsString(const std::vector<std::string> &values, const std::string_view needle) {
  return std::any_of(values.begin(), values.end(), [needle](const std::string &value) {
    return value == needle;
  });
}

const aster::FramePassStats *passStatsFor(const aster::FrameForensics &forensics,
                                          const aster::RenderGraphPass pass) {
  const auto found = std::find_if(forensics.passes.begin(), forensics.passes.end(),
                                  [pass](const aster::FramePassStats &stats) {
                                    return stats.pass == pass;
                                  });
  return found == forensics.passes.end() ? nullptr : &*found;
}

void enrichFramePassCostMap(aster::FrameForensics &forensics, const aster::FixedRenderGraph &graph,
                            const aster::RendererSettings &settings,
                            const std::uint32_t frame_width,
                            const std::uint32_t frame_height,
                            const MaterialFrameSummary &material_summary) {
  for (std::size_t pass_index = 0u; pass_index < forensics.passes.size(); ++pass_index) {
    aster::FramePassStats &stats = forensics.passes[pass_index];
    const auto compiled = std::find_if(
        graph.passes.begin(), graph.passes.end(),
        [&stats](const aster::framegraph::CompiledPass &pass) { return pass.name == stats.name; });
    if (stats.cpu_build_seconds == 0.0) {
      stats.cpu_build_seconds = stats.encode_seconds;
    }
    if (pass_index < forensics.timestamp_samples.size() &&
        forensics.timestamp_samples[pass_index].available) {
      stats.gpu_execution_seconds =
          forensics.timestamp_samples[pass_index].nanoseconds / 1000000000.0;
    }
    if (compiled != graph.passes.end()) {
      const aster::rhi::ImageExtent target =
          renderTargetExtentForPass(graph, *compiled, settings, frame_width, frame_height);
      stats.render_target_width = target.width;
      stats.render_target_height = target.height;
      stats.estimated_bandwidth_bytes =
          estimatePassBandwidthBytes(graph, *compiled, settings, frame_width, frame_height);
      stats.descriptor_heap_pressure = std::accumulate(
          compiled->descriptor_requirements.begin(), compiled->descriptor_requirements.end(),
          std::size_t{0u},
          [](const std::size_t total,
             const aster::framegraph::DescriptorRequirement &requirement) {
            return total + std::max<std::uint32_t>(requirement.count, 1u);
          });
    }
    if (stats.pass == aster::RenderGraphPass::Opaque) {
      stats.pipeline_cache_hits = material_summary.material_variant_cache_hits;
      stats.pipeline_cache_misses = material_summary.material_variant_cache_misses;
      stats.descriptor_heap_pressure +=
          material_summary.material_permutations *
          std::max<std::uint32_t>(1u, settings.reflections.enabled ? 2u : 1u);
    }
  }
}

aster::RenderGraphResource firstOutputResourceForPass(const aster::RenderGraphPass pass) {
  const aster::RenderGraphPassDeclaration *declaration = aster::defaultRenderPassDeclaration(pass);
  if (declaration != nullptr && !declaration->outputs.empty()) {
    return declaration->outputs.front().resource;
  }
  return aster::RenderGraphResource::SceneColor;
}

std::string firstFallbackReasonForObject(const aster::FrameForensics &forensics,
                                         const aster::ObjectRenderFateTrace &fate) {
  for (const aster::MaterialBindingTrace &binding : forensics.material_bindings) {
    if (binding.object_name != fate.object_name) {
      continue;
    }
    if (!binding.fallback_reason.empty()) {
      return binding.role + ":" + binding.fallback_reason;
    }
    if (!binding.backend_degradation.empty()) {
      return binding.role + ":" + binding.backend_degradation;
    }
  }
  if (!fate.asset_issues.empty()) {
    return fate.asset_issues.front();
  }
  if (!fate.backend_degradations.empty()) {
    return fate.backend_degradations.front();
  }
  return "none";
}

bool hasAvailableCaptureFor(const aster::FrameForensics &forensics,
                            const aster::RenderGraphResource resource) {
  return std::any_of(forensics.captures.begin(), forensics.captures.end(),
                     [resource](const aster::FrameDebugCapture &capture) {
                       return capture.resource == resource && capture.available;
                     });
}

std::uint64_t timelineEventHash(const aster::FrameDebuggerTimelineEvent &event) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, event.sequence);
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(event.kind));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(event.pass));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(event.resource));
  hash = appendEvidenceValue(hash, event.object_index);
  hash = appendEvidenceText(hash, event.object_name);
  hash = appendEvidenceText(hash, event.label);
  hash = appendEvidenceText(hash, event.evidence);
  hash = appendEvidenceValue(hash, event.estimated_bandwidth_bytes);
  hash = appendEvidenceValue(hash, event.render_target_width);
  hash = appendEvidenceValue(hash, event.render_target_height);
  hash = appendEvidenceValue(hash, event.draw_count);
  hash = appendEvidenceValue(hash, event.material_variant_count);
  hash = appendEvidenceValue(hash, event.descriptor_heap_pressure);
  hash = appendEvidenceValue(hash, event.pipeline_cache_hits);
  hash = appendEvidenceValue(hash, event.pipeline_cache_misses);
  return appendEvidenceText(hash, event.fallback_reason);
}

void appendTimelineEvent(std::vector<aster::FrameDebuggerTimelineEvent> &timeline,
                         aster::FrameDebuggerTimelineEvent event) {
  event.sequence = timeline.size();
  event.evidence_hash = timelineEventHash(event);
  timeline.push_back(std::move(event));
}

void rebuildFrameDebuggerTimeline(aster::FrameForensics &forensics) {
  forensics.debug_timeline.clear();
  forensics.debug_timeline.reserve(forensics.passes.size() + forensics.object_fates.size() * 7u +
                                   forensics.material_bindings.size() +
                                   forensics.object_clusters.size());

  for (const aster::FramePassStats &pass : forensics.passes) {
    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::PassOutput,
                         .pass = pass.pass,
                         .resource = firstOutputResourceForPass(pass.pass),
                         .label = pass.name,
                         .evidence = "draws=" + std::to_string(pass.draw_calls) +
                                     " encode_ms=" +
                                     std::to_string(pass.encode_seconds * 1000.0) +
                                     " gpu_ms=" +
                                     std::to_string(pass.gpu_execution_seconds * 1000.0) +
                                     " bandwidth=" +
                                     std::to_string(pass.estimated_bandwidth_bytes),
                         .cpu_build_seconds = pass.cpu_build_seconds,
                         .gpu_execution_seconds = pass.gpu_execution_seconds,
                         .estimated_bandwidth_bytes = pass.estimated_bandwidth_bytes,
                         .render_target_width = pass.render_target_width,
                         .render_target_height = pass.render_target_height,
                         .draw_count = pass.draw_calls,
                         .material_variant_count = pass.material_permutations,
                         .descriptor_heap_pressure = pass.descriptor_heap_pressure,
                         .pipeline_cache_hits = pass.pipeline_cache_hits,
                         .pipeline_cache_misses = pass.pipeline_cache_misses});
  }

  for (const aster::MeshVisibilityTrace &visibility : forensics.mesh_visibility) {
    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Visibility,
                         .pass = visibility.pass,
                         .resource = aster::RenderGraphResource::SceneColor,
                         .object_name = visibility.object_name,
                         .object_index = visibility.object_index,
                         .label = visibility.visible ? "visible" : "culled",
                         .evidence = visibility.reason});
  }

  for (const aster::MaterialBindingTrace &binding : forensics.material_bindings) {
    const std::string fallback =
        !binding.fallback_reason.empty()
            ? binding.fallback_reason
            : (!binding.backend_degradation.empty() ? binding.backend_degradation : "none");
    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::MaterialBinding,
                         .pass = aster::RenderGraphPass::Opaque,
                         .resource = aster::RenderGraphResource::SceneColor,
                         .object_name = binding.object_name,
                         .label = binding.role,
                         .evidence = (binding.bound ? "bound" : "unbound") +
                                     std::string(binding.fallback ? ":fallback" : ":authored"),
                         .fallback_reason = fallback});
  }

  for (const aster::ObjectClusterMembershipTrace &cluster : forensics.object_clusters) {
    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::LightCluster,
                         .pass = aster::RenderGraphPass::LightCull,
                         .resource = aster::RenderGraphResource::LightClusters,
                         .object_name = cluster.object_name,
                         .object_index = cluster.object_index,
                         .label = "cluster:" + std::to_string(cluster.cluster_index),
                         .evidence = cluster.visible ? "visible-object" : "not-visible"});
  }

  for (const aster::ObjectRenderFateTrace &fate : forensics.object_fates) {
    const bool shadow = containsString(fate.pass_list, "shadow-atlas");
    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Shadow,
                         .pass = aster::RenderGraphPass::ShadowAtlas,
                         .resource = aster::RenderGraphResource::ShadowAtlas,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = shadow ? "shadow-linked" : "shadow-not-linked",
                         .evidence = hasAvailableCaptureFor(
                                         forensics, aster::RenderGraphResource::ShadowAtlas)
                                         ? "shadow-capture-available"
                                         : "shadow-capture-missing"});

    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::SurfaceOcclusion,
                         .pass = aster::RenderGraphPass::SurfaceOcclusion,
                         .resource = aster::RenderGraphResource::SurfaceOcclusion,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = "surface-occlusion-resource",
                         .evidence = hasAvailableCaptureFor(
                                         forensics, aster::RenderGraphResource::SurfaceOcclusion)
                                         ? "surface-occlusion-capture-available"
                                         : "surface-occlusion-capture-missing"});

    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Fog,
                         .pass = aster::RenderGraphPass::VolumetricFog,
                         .resource = aster::RenderGraphResource::VolumetricFog,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = "fog-resource",
                         .evidence = hasAvailableCaptureFor(
                                         forensics, aster::RenderGraphResource::VolumetricFog)
                                         ? "fog-capture-available"
                                         : "fog-capture-missing"});

    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Probe,
                         .pass = aster::RenderGraphPass::ReflectionProbe,
                         .resource = aster::RenderGraphResource::ReflectionProbes,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = "reflection-probe-resource",
                         .evidence = hasAvailableCaptureFor(
                                         forensics, aster::RenderGraphResource::ReflectionProbes)
                                         ? "probe-capture-available"
                                         : "probe-capture-missing"});

    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Overdraw,
                         .pass = fate.visible ? aster::RenderGraphPass::Opaque
                                              : aster::RenderGraphPass::SceneColorDepth,
                         .resource = aster::RenderGraphResource::SceneColor,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = "estimated-overdraw",
                         .evidence = "layers=" +
                                     std::to_string(std::max<std::size_t>(
                                         fate.visible ? fate.pass_list.size() : 0u, 1u))});

    appendTimelineEvent(forensics.debug_timeline,
                        {.kind = aster::FrameDebuggerTimelineEventKind::Fallback,
                         .pass = fate.visible ? aster::RenderGraphPass::Opaque
                                              : aster::RenderGraphPass::SceneColorDepth,
                         .resource = aster::RenderGraphResource::SceneColor,
                         .object_name = fate.object_name,
                         .object_index = fate.object_index,
                         .label = "fallback-reason",
                         .evidence = fate.final_contribution,
                         .fallback_reason = firstFallbackReasonForObject(forensics, fate)});
  }
}

std::uint64_t resourceProvenanceHash(const aster::FrameResourceProvenance &provenance) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(provenance.kind));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(provenance.resource));
  hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(provenance.producer_pass));
  hash = appendEvidenceText(hash, provenance.resource_name);
  hash = appendEvidenceText(hash, provenance.producer_node);
  hash = appendEvidenceText(hash, provenance.material_asset_id);
  hash = appendEvidenceText(hash, provenance.material_graph_guid);
  hash = appendEvidenceText(hash, provenance.material_graph_node);
  hash = appendEvidenceText(hash, provenance.cook_report);
  hash = appendEvidenceText(hash, provenance.texture_role);
  hash = appendEvidenceText(hash, provenance.source_path);
  hash = appendEvidenceText(hash, provenance.asset_hash);
  hash = appendEvidenceText(hash, provenance.shader_variant_key);
  hash = appendEvidenceText(hash, provenance.backend_fallback);
  for (const std::string &upstream : provenance.upstream) {
    hash = appendEvidenceText(hash, upstream);
  }
  return hash;
}

std::string proofStatusForResource(const aster::FrameForensics &forensics,
                                   const aster::RenderGraphResource resource) {
  for (const aster::BackendFeatureProof &proof : forensics.backend_feature_proofs) {
    if (proof.kind == aster::BackendFeatureProofKind::GraphResource &&
        proof.resource == resource) {
      return std::string(aster::backendFeatureProofStatusName(proof.status)) + ":" +
             proof.message;
    }
  }
  return {};
}

const aster::AssetFrameTrace *assetTraceForObject(const aster::FrameForensics &forensics,
                                                  const std::string_view object_name) {
  const auto found = std::find_if(forensics.asset_traces.begin(), forensics.asset_traces.end(),
                                  [object_name](const aster::AssetFrameTrace &trace) {
                                    return trace.object_name == object_name;
                                  });
  return found == forensics.asset_traces.end() ? nullptr : &*found;
}

const aster::ObjectRenderFateTrace *fateForObject(const aster::FrameForensics &forensics,
                                                  const std::string_view object_name) {
  const auto found = std::find_if(forensics.object_fates.begin(), forensics.object_fates.end(),
                                  [object_name](const aster::ObjectRenderFateTrace &trace) {
                                    return trace.object_name == object_name;
                                  });
  return found == forensics.object_fates.end() ? nullptr : &*found;
}

void rebuildResourceProvenance(const aster::FixedRenderGraph &graph,
                               aster::FrameForensics &forensics) {
  forensics.resource_provenance.clear();
  forensics.resource_provenance.reserve(graph.resources.size() +
                                        forensics.material_bindings.size());

  for (const aster::framegraph::CompiledResource &resource_node : graph.resources) {
    aster::FrameResourceProvenance provenance;
    provenance.kind = aster::FrameResourceProvenanceKind::GraphResource;
    provenance.resource = aster::renderGraphResourceFromName(resource_node.name);
    provenance.resource_name = resource_node.name;
    provenance.cook_report = "compiled-frame-graph";
    provenance.backend_fallback = proofStatusForResource(forensics, provenance.resource);
    for (const aster::framegraph::CompiledPass &pass : graph.passes) {
      if (std::find(pass.writes.begin(), pass.writes.end(), resource_node.handle) !=
          pass.writes.end()) {
        provenance.producer_pass = aster::renderGraphPassFromName(pass.name);
        provenance.producer_node = pass.name;
        for (const aster::framegraph::ResourceHandle input : pass.reads) {
          if (const aster::framegraph::CompiledResource *upstream =
                  compiledResourceFor(graph, input)) {
            appendUnique(provenance.upstream, upstream->name);
          }
        }
        break;
      }
    }
    provenance.asset_hash = std::to_string(resource_node.physical_allocation_id);
    provenance.provenance_hash = resourceProvenanceHash(provenance);
    forensics.resource_provenance.push_back(std::move(provenance));
  }

  for (const aster::MaterialBindingTrace &binding : forensics.material_bindings) {
    const aster::AssetFrameTrace *asset_trace = assetTraceForObject(forensics, binding.object_name);
    const aster::ObjectRenderFateTrace *fate = fateForObject(forensics, binding.object_name);
    aster::FrameResourceProvenance provenance;
    provenance.kind = aster::FrameResourceProvenanceKind::MaterialTexture;
    provenance.resource = aster::RenderGraphResource::SceneColor;
    provenance.producer_pass = fate != nullptr && containsString(fate->pass_list, "transparent")
                                   ? aster::RenderGraphPass::Transparent
                                   : aster::RenderGraphPass::Opaque;
    provenance.resource_name = std::string(aster::renderGraphResourceName(provenance.resource));
    provenance.producer_node = "material-binding:" + binding.object_name + ":" + binding.role;
    provenance.material_asset_id = binding.material_asset_id;
    provenance.texture_role = binding.role;
    provenance.source_path = binding.source_path;
    provenance.backend_fallback =
        !binding.fallback_reason.empty()
            ? binding.fallback_reason
            : (!binding.backend_degradation.empty() ? binding.backend_degradation : "none");
    if (asset_trace != nullptr) {
      provenance.material_graph_guid = asset_trace->source_graph_guid;
      provenance.material_graph_node = asset_trace->source_graph_node;
      provenance.cook_report =
          asset_trace->source_path.empty() ? "runtime-frame-trace" : asset_trace->source_path;
      provenance.asset_hash = std::to_string(asset_trace->trace_hash);
      provenance.shader_variant_key = asset_trace->shader_variant_key;
      if (!asset_trace->material_slot.empty()) {
        appendUnique(provenance.upstream, "material-slot:" + asset_trace->material_slot);
      }
      if (!asset_trace->source_mesh.empty()) {
        appendUnique(provenance.upstream, "mesh:" + asset_trace->source_mesh);
      }
      for (const std::string &issue : asset_trace->issues) {
        appendUnique(provenance.upstream, "issue:" + issue);
      }
    } else {
      provenance.cook_report = "runtime-material-library";
    }
    provenance.provenance_hash = resourceProvenanceHash(provenance);
    forensics.resource_provenance.push_back(std::move(provenance));
  }
}

std::string aggregateAssetHash(const aster::FrameForensics &forensics) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const aster::AssetFrameTrace &trace : forensics.asset_traces) {
    hash = appendEvidenceValue(hash, trace.trace_hash);
  }
  return hash == 1469598103934665603ull ? std::string() : std::to_string(hash);
}

std::string firstShaderVariantKey(const aster::FrameForensics &forensics) {
  for (const aster::ObjectRenderFateTrace &fate : forensics.object_fates) {
    if (!fate.shader_variant_key.empty() && fate.shader_variant_key != "0") {
      return fate.shader_variant_key;
    }
  }
  for (const aster::AssetFrameTrace &trace : forensics.asset_traces) {
    if (!trace.shader_variant_key.empty() && trace.shader_variant_key != "0") {
      return trace.shader_variant_key;
    }
  }
  return "none";
}

std::string backendDifferenceForCapture(const aster::FrameForensics &forensics,
                                        const aster::RenderGraphResource resource) {
  const std::string proof = proofStatusForResource(forensics, resource);
  if (!proof.empty()) {
    return proof;
  }
  return "single-backend-candidate";
}

void rebuildRegressionGallery(const aster::RenderBackendCapabilities &capabilities,
                              aster::FrameForensics &forensics) {
  forensics.regression_gallery.clear();
  forensics.regression_gallery.reserve(forensics.captures.size());
  const std::string asset_hash = aggregateAssetHash(forensics);
  const std::string shader_key = firstShaderVariantKey(forensics);
  for (const aster::FrameDebugCapture &capture : forensics.captures) {
    const aster::FramePassStats *pass_stats = passStatsFor(forensics, capture.pass);
    forensics.regression_gallery.push_back(
        {.label = capture.label,
         .backend = capabilities.kind,
         .pass = capture.pass,
         .resource = capture.resource,
         .width = capture.width,
         .height = capture.height,
         .image_hash = capture.content_hash,
         .diff_hash = 0u,
         .mean_abs_error = 0.0,
         .differing_pixel_ratio = 0.0,
         .image_diff_status = capture.available ? "baseline-required" : "capture-missing",
         .backend_difference = backendDifferenceForCapture(forensics, capture.resource),
         .pass_encode_seconds = pass_stats == nullptr ? 0.0 : pass_stats->encode_seconds,
         .asset_hash = asset_hash,
         .shader_variant_key = shader_key,
         .available = capture.available});
  }
}

void rebuildFrameEvidenceProducts(const aster::FixedRenderGraph &graph,
                                  const aster::RenderBackendCapabilities &capabilities,
                                  aster::FrameForensics &forensics) {
  rebuildFrameDebuggerTimeline(forensics);
  rebuildResourceProvenance(graph, forensics);
  rebuildRegressionGallery(capabilities, forensics);
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

RenderMathContractReport certifyRenderMathContract(
    const Scene &scene, const OrbitCamera &camera, const RenderBackendCapabilities &capabilities,
    const MaterialResourceLibrary *library) {
  RenderMathContractReport report;
  report.object_count = scene.objects().size();

  const ProjectionConvention canonical = defaultProjectionConvention();
  const ProjectionConvention camera_convention =
      projectionConventionFromPolicy(camera.projection_policy);
  const ProjectionConvention backend_convention = capabilities.projection_convention;

  const auto core_matches = [](const ProjectionConvention lhs,
                               const ProjectionConvention rhs) {
    return lhs.handedness == rhs.handedness && lhs.depth_range == rhs.depth_range &&
           lhs.depth_direction == rhs.depth_direction &&
           lhs.viewport_origin == rhs.viewport_origin && lhs.y_flip == rhs.y_flip &&
           lhs.matrix_storage == rhs.matrix_storage &&
           lhs.vector_convention == rhs.vector_convention;
  };
  const auto depth_is_canonical = [canonical](const ProjectionConvention convention) {
    return convention.handedness == canonical.handedness &&
           convention.depth_range == canonical.depth_range &&
           convention.depth_direction == canonical.depth_direction;
  };
  const auto viewport_is_canonical = [canonical](const ProjectionConvention convention) {
    return convention.viewport_origin == canonical.viewport_origin &&
           convention.y_flip == canonical.y_flip;
  };
  const auto matrix_is_canonical = [canonical](const ProjectionConvention convention) {
    return convention.matrix_storage == canonical.matrix_storage &&
           convention.vector_convention == canonical.vector_convention;
  };
  const auto texture_color_expected = [](const std::string_view role,
                                         const TextureColorSpace color_space) {
    const bool srgb_role = role == "albedo" || role == "base_color" || role == "emissive";
    return srgb_role ? color_space == TextureColorSpace::SRGB
                     : color_space == TextureColorSpace::Linear;
  };
  auto add_issue = [&report](std::string issue) {
    report.issues.push_back(std::move(issue));
  };

  report.backend_canonical = core_matches(backend_convention, canonical);
  report.camera_canonical = core_matches(camera_convention, canonical);
  report.camera_matches_backend = core_matches(camera_convention, backend_convention);
  report.depth_contract_canonical =
      depth_is_canonical(backend_convention) && depth_is_canonical(camera_convention);
  report.viewport_contract_canonical =
      viewport_is_canonical(backend_convention) && viewport_is_canonical(camera_convention);
  report.matrix_contract_canonical =
      matrix_is_canonical(backend_convention) && matrix_is_canonical(camera_convention);

  if (!report.backend_canonical) {
    add_issue("backend projection convention is not the canonical Aster render contract");
  }
  if (!report.camera_canonical) {
    add_issue("camera projection convention is not the canonical Aster render contract");
  }
  if (!report.camera_matches_backend) {
    add_issue("camera projection convention does not match the active backend");
  }
  if (!report.depth_contract_canonical) {
    add_issue("depth contract must be right-handed zero-to-one reverse-Z");
  }
  if (!report.viewport_contract_canonical) {
    add_issue("viewport contract must use top-left origin with backend Y flip");
  }
  if (!report.matrix_contract_canonical) {
    add_issue("matrix contract must use column-major storage and column-vector transforms");
  }

  std::unordered_set<std::string> inspected_materials;
  for (std::size_t i = 0; i < scene.objects().size(); ++i) {
    const RenderObject &object = scene.objects()[i];
    const std::string label = object.name.empty() ? ("object:" + std::to_string(i)) : object.name;
    if (!allFinite(object.transform.position) || !allFinite(object.transform.scale) ||
        !finiteQuat(object.transform.rotation)) {
      ++report.non_finite_world_matrices;
      add_issue("non-finite world transform: " + label);
      continue;
    }
    const Vec3 scale = object.transform.scale;
    if (std::abs(scale.x) <= 0.000001f || std::abs(scale.y) <= 0.000001f ||
        std::abs(scale.z) <= 0.000001f) {
      ++report.singular_normal_matrices;
      add_issue("singular normal matrix from zero scale: " + label);
    }
    if (scale.x * scale.y * scale.z < 0.0f) {
      ++report.negative_tangent_flips;
    }
    if (!allFinite(object.material.base_color.value) ||
        !allFinite(object.material.emission_color.value)) {
      ++report.color_space_violations;
      add_issue("runtime material color is non-finite: " + label);
    }

    const std::string material_id =
        !object.material_asset_id.empty() ? object.material_asset_id : object.material.asset_id;
    if (library == nullptr || material_id.empty() ||
        !inspected_materials.insert(material_id).second) {
      continue;
    }
    const MaterialRuntimeResource *resource =
        library->findForMaterialIds(object.material_asset_id, object.material.asset_id);
    if (resource == nullptr) {
      continue;
    }
    for (const auto &[role, texture] : resource->texture_set.textures) {
      if (!texture.valid) {
        continue;
      }
      ++report.texture_count;
      if (!texture_color_expected(role, texture.color_space)) {
        ++report.color_space_violations;
        add_issue("texture role/color-space boundary mismatch: " + material_id + ":" + role);
      }
      if (role == "normal") {
        ++report.normal_texture_count;
        if (texture.normal_convention != TextureNormalConvention::OpenGlYUp) {
          ++report.normal_convention_violations;
          add_issue("normal map convention must be OpenGL/Y-up: " + material_id);
        }
      }
    }
  }

  report.tangent_handedness_traced = true;
  report.normal_map_convention_valid = report.normal_convention_violations == 0u;
  report.color_space_boundary_valid = report.color_space_violations == 0u;
  report.issue_count = report.issues.size();
  report.valid = report.backend_canonical && report.camera_canonical &&
                 report.camera_matches_backend && report.depth_contract_canonical &&
                 report.viewport_contract_canonical && report.matrix_contract_canonical &&
                 report.non_finite_world_matrices == 0u &&
                 report.singular_normal_matrices == 0u &&
                 report.normal_map_convention_valid && report.color_space_boundary_valid;

  std::uint64_t hash = 1469598103934665603ull;
  hash = appendEvidenceValue(hash, report.valid ? 1u : 0u);
  hash = appendEvidenceValue(hash, report.backend_canonical ? 1u : 0u);
  hash = appendEvidenceValue(hash, report.camera_canonical ? 1u : 0u);
  hash = appendEvidenceValue(hash, report.camera_matches_backend ? 1u : 0u);
  hash = appendEvidenceValue(hash, report.object_count);
  hash = appendEvidenceValue(hash, report.non_finite_world_matrices);
  hash = appendEvidenceValue(hash, report.singular_normal_matrices);
  hash = appendEvidenceValue(hash, report.negative_tangent_flips);
  hash = appendEvidenceValue(hash, report.texture_count);
  hash = appendEvidenceValue(hash, report.normal_texture_count);
  hash = appendEvidenceValue(hash, report.normal_convention_violations);
  hash = appendEvidenceValue(hash, report.color_space_violations);
  for (const std::string &issue : report.issues) {
    hash = appendEvidenceText(hash, issue);
  }
  report.contract_hash = hash;
  return report;
}

std::string_view asterRenderProofSignalName(const AsterRenderProofSignal signal) {
  switch (signal) {
  case AsterRenderProofSignal::PassProvenance:
    return "pass-provenance";
  case AsterRenderProofSignal::DescriptorPressure:
    return "descriptor-pressure";
  case AsterRenderProofSignal::PipelineCache:
    return "pipeline-cache";
  case AsterRenderProofSignal::ResourceLifetime:
    return "resource-lifetime";
  case AsterRenderProofSignal::MathContract:
    return "math-contract";
  case AsterRenderProofSignal::BackendFallback:
    return "backend-fallback";
  case AsterRenderProofSignal::AssetProvenance:
    return "asset-provenance";
  case AsterRenderProofSignal::VisualRegression:
    return "visual-regression";
  case AsterRenderProofSignal::ClusteredLighting:
    return "clustered-lighting";
  case AsterRenderProofSignal::SurfaceFidelity:
    return "surface-fidelity";
  }
  return "pass-provenance";
}

AsterRenderProofSummary summarizeAsterRenderProof(const FrameForensics &forensics) {
  AsterRenderProofSummary summary;
  const auto append_row = [&summary](const AsterRenderProofSignal signal, std::string label,
                                     std::string evidence, const std::size_t count,
                                     const bool ready) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(signal));
    hash = appendEvidenceText(hash, label);
    hash = appendEvidenceText(hash, evidence);
    hash = appendEvidenceValue(hash, static_cast<std::uint64_t>(count));
    hash = appendEvidenceValue(hash, ready ? 1u : 0u);
    summary.rows.push_back({.signal = signal,
                            .label = std::move(label),
                            .evidence = std::move(evidence),
                            .count = count,
                            .ready = ready,
                            .hash = hash});
    if (ready) {
      ++summary.ready_signals;
    } else {
      ++summary.blocked_signals;
      summary.diagnostics.push_back(
          std::string(asterRenderProofSignalName(signal)) + ": proof signal has no usable evidence");
    }
  };

  for (const FramePassStats &pass : forensics.passes) {
    summary.descriptor_pressure += pass.descriptor_heap_pressure;
    summary.pipeline_cache_hits += pass.pipeline_cache_hits;
    summary.pipeline_cache_misses += pass.pipeline_cache_misses;
  }
  const std::size_t pipeline_trace_count = forensics.rhi_trace.pipelines.size();
  const std::size_t pipeline_fate_count =
      static_cast<std::size_t>(std::count_if(
          forensics.object_fates.begin(), forensics.object_fates.end(),
          [](const ObjectRenderFateTrace &fate) { return !fate.pipeline_cache_key.empty(); }));
  summary.backend_fallbacks =
      static_cast<std::size_t>(std::count_if(
          forensics.events.begin(), forensics.events.end(), [](const FrameDiagnosticEvent &event) {
            return event.kind == FrameDiagnosticKind::BackendFallback ||
                   event.kind == FrameDiagnosticKind::MaterialVariantFallback ||
                   event.kind == FrameDiagnosticKind::CapabilityMismatch ||
                   event.kind == FrameDiagnosticKind::ClusteredLightingFallback ||
                   event.kind == FrameDiagnosticKind::TextureRoleDegraded ||
                   event.kind == FrameDiagnosticKind::MeshAttributeDegraded;
          }));
  summary.backend_fallbacks +=
      static_cast<std::size_t>(std::count_if(
          forensics.material_bindings.begin(), forensics.material_bindings.end(),
          [](const MaterialBindingTrace &binding) {
            return binding.fallback || !binding.fallback_reason.empty() ||
                   !binding.backend_degradation.empty();
          }));
  summary.backend_fallbacks +=
      static_cast<std::size_t>(std::count_if(
          forensics.backend_feature_proofs.begin(), forensics.backend_feature_proofs.end(),
          [](const BackendFeatureProof &proof) {
            return proof.status == BackendFeatureProofStatus::MissingProof ||
                   proof.status == BackendFeatureProofStatus::Unsupported;
          }));

  const bool pass_ready = !forensics.passes.empty() &&
                          std::any_of(forensics.resource_provenance.begin(),
                                      forensics.resource_provenance.end(),
                                      [](const FrameResourceProvenance &provenance) {
                                        return provenance.kind ==
                                                   FrameResourceProvenanceKind::GraphResource &&
                                               provenance.provenance_hash != 0u;
                                      });
  append_row(AsterRenderProofSignal::PassProvenance, "FrameForensics pass provenance",
             std::to_string(forensics.passes.size()) + " passes, " +
                 std::to_string(forensics.resource_provenance.size()) + " provenance records",
             forensics.resource_provenance.size(), pass_ready);

  append_row(AsterRenderProofSignal::DescriptorPressure, "descriptor pressure map",
             std::to_string(summary.descriptor_pressure) +
                 " descriptor slots across frame graph passes",
             summary.descriptor_pressure, !forensics.passes.empty());

  const std::size_t pipeline_count = summary.pipeline_cache_hits +
                                     summary.pipeline_cache_misses + pipeline_trace_count +
                                     pipeline_fate_count;
  append_row(AsterRenderProofSignal::PipelineCache, "pipeline cache evidence",
             std::to_string(summary.pipeline_cache_hits) + " hits, " +
                 std::to_string(summary.pipeline_cache_misses) + " misses, " +
                 std::to_string(pipeline_trace_count) + " RHI pipeline traces",
             pipeline_count, pipeline_count > 0u);

  const bool lifetime_errors =
      std::any_of(forensics.rhi_validation_events.begin(), forensics.rhi_validation_events.end(),
                  [](const rhi::ResourceLifetimeValidationEvent &event) {
                    return event.severity == rhi::ResourceLifetimeValidationSeverity::Error;
                  }) ||
      forensics.certification.validation_error_count > 0u;
  append_row(AsterRenderProofSignal::ResourceLifetime, "resource lifetime audit",
             std::to_string(forensics.resource_traces.size()) + " resource transitions, " +
                 std::to_string(forensics.rhi_validation_events.size()) + " validation events",
             forensics.resource_traces.size() + forensics.rhi_validation_events.size(),
             !forensics.resource_traces.empty() && !lifetime_errors);

  const bool math_contract_ready =
      forensics.math_contract.valid && forensics.math_contract.contract_hash != 0u;
  append_row(AsterRenderProofSignal::MathContract, "canonical render math contract",
             std::to_string(forensics.math_contract.issue_count) + " issues, " +
                 std::to_string(forensics.math_contract.negative_tangent_flips) +
                 " tangent handedness flips traced, " +
                 std::to_string(forensics.math_contract.texture_count) +
                 " texture boundaries checked",
             forensics.math_contract.issue_count, math_contract_ready);
  if (!math_contract_ready) {
    for (const std::string &issue : forensics.math_contract.issues) {
      summary.diagnostics.push_back("math contract: " + issue);
    }
  }

  const bool fallback_details_ready =
      summary.backend_fallbacks == 0u ||
      std::any_of(forensics.debug_timeline.begin(), forensics.debug_timeline.end(),
                  [](const FrameDebuggerTimelineEvent &event) {
                    return event.kind == FrameDebuggerTimelineEventKind::Fallback &&
                           (!event.fallback_reason.empty() || !event.evidence.empty());
                  }) ||
      std::any_of(forensics.events.begin(), forensics.events.end(),
                  [](const FrameDiagnosticEvent &event) { return !event.message.empty(); });
  append_row(AsterRenderProofSignal::BackendFallback, "backend fallback reasons",
             std::to_string(summary.backend_fallbacks) + " fallback or degradation records",
             summary.backend_fallbacks, fallback_details_ready);
  if (summary.backend_fallbacks > 0u) {
    summary.diagnostics.push_back("backend fallbacks captured: " +
                                  std::to_string(summary.backend_fallbacks));
  }

  const std::size_t material_provenance_count =
      static_cast<std::size_t>(std::count_if(
          forensics.resource_provenance.begin(), forensics.resource_provenance.end(),
          [](const FrameResourceProvenance &provenance) {
            return provenance.kind == FrameResourceProvenanceKind::MaterialTexture;
          }));
  append_row(AsterRenderProofSignal::AssetProvenance, "asset provenance chain",
             std::to_string(forensics.asset_traces.size()) + " object asset traces, " +
                 std::to_string(material_provenance_count) + " material texture records",
             forensics.asset_traces.size() + material_provenance_count,
             !forensics.asset_traces.empty() || material_provenance_count > 0u);

  const std::size_t available_regressions =
      static_cast<std::size_t>(std::count_if(
          forensics.regression_gallery.begin(), forensics.regression_gallery.end(),
          [](const FrameRegressionGalleryEntry &entry) {
            return entry.available && entry.image_hash != 0u;
          }));
  append_row(AsterRenderProofSignal::VisualRegression, "visual regression gallery",
             std::to_string(available_regressions) + " available capture artifacts",
             available_regressions, available_regressions > 0u);

  const std::size_t cluster_count =
      static_cast<std::size_t>(forensics.clustered_lights.cluster_count_x) *
      static_cast<std::size_t>(forensics.clustered_lights.cluster_count_y) *
      static_cast<std::size_t>(forensics.clustered_lights.cluster_count_z);
  const bool cluster_ready = cluster_count > 0u &&
                             !forensics.clustered_lights.cluster_offsets.empty() &&
                             forensics.clustered_lights.assignments_hash != 0u;
  append_row(AsterRenderProofSignal::ClusteredLighting, "cluster and froxel light proof",
             std::to_string(cluster_count) + " clusters, " +
                 std::to_string(forensics.clustered_lights.light_indices.size()) +
                 " light references",
             cluster_count, cluster_ready);

  const std::size_t surface_count =
      static_cast<std::size_t>(std::count_if(
          forensics.surface_traces.begin(), forensics.surface_traces.end(),
          [](const SurfacePresentationTrace &trace) {
            return trace.physical_texel_density > 0.0f &&
                   (trace.height_normal_coupling > 0.0f ||
                    trace.roughness_height_coupling > 0.0f ||
                    trace.cavity_strength > 0.0f ||
                    trace.contact_shadow_receiver);
          }));
  append_row(AsterRenderProofSignal::SurfaceFidelity, "surface fidelity signal",
             std::to_string(surface_count) + " objects with scale/contact/cavity evidence",
             surface_count, surface_count > 0u);

  summary.production_trace_ready = summary.blocked_signals == 0u;
  return summary;
}

void FrameDebugger::appendGraphForensics(const FixedRenderGraph &graph,
                                         const RenderBackendCapabilities &capabilities,
                                         const int framebuffer_width,
                                         const int framebuffer_height,
                                         FrameForensics &forensics) const {
  appendFrameGraphForensics(graph, capabilities, framebuffer_width, framebuffer_height, forensics);
}

const CpuMesh &RenderDevice::meshForPrimitive(const MeshPrimitive primitive) const {
  return asset_cache_.meshForPrimitive(primitive);
}

const CpuMesh &RenderDevice::meshForObject(const RenderObject &object) {
  return asset_cache_.meshForObject(object);
}

void RenderDevice::syncDynamicMeshes(const Scene &scene, const bool immediate_eviction) {
  asset_cache_.syncDynamicMeshes(scene, resource_registry_, immediate_eviction);
}

void RenderDevice::initialize() {
  ASTER_PROFILE_SCOPE("RenderDevice::initialize");
  frame_graph_ = makeDefaultFrameGraph(true, true);
  render_graph_ = graph_compiler_.compileDefault(true, true);
  compiled_frame_graph_ = render_graph_;
  asset_cache_.initializeBuiltins();

  const char *force_null = std::getenv("ASTER_FORCE_NULL_RENDERER");
  const char *force_software = std::getenv("ASTER_FORCE_SOFTWARE_RENDERER");
  const bool force_null_enabled = force_null != nullptr && *force_null != '\0';
  const bool force_software_enabled = force_software != nullptr && *force_software != '\0';
  if (force_null_enabled && !force_software_enabled) {
    native_backend_ = createNullRenderBackend();
    if (native_backend_ != nullptr && !native_backend_->initialize()) {
      native_backend_.reset();
    }
  } else if (!force_software_enabled) {
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

bool RenderDevice::bindWindow(const NativeWindowSurface &surface) {
  return native_backend_ != nullptr && native_backend_->bindWindow(surface);
}

void RenderDevice::prepareScene(const Scene &scene) {
  ASTER_PROFILE_SCOPE("RenderDevice::prepareScene");
  render_world_.rebuild(scene);
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
  render_world_.rebuild(scene);
  const FrameRenderPlan plan =
      buildFrameRenderPlan(render_world_.scene(), camera, settings.line_of_sight_fade,
                           framebuffer_width, framebuffer_height);
  const RenderBackendCapabilities active_capabilities =
      native_backend_ != nullptr ? native_backend_->capabilities() : softwareCapabilities();
  last_forensics_.math_contract =
      certifyRenderMathContract(scene, camera, active_capabilities, material_library_.get());
  const ClusteredLightGrid clustered_lights =
      buildClusteredLightGrid(settings.light_rig, camera, framebuffer_width, framebuffer_height,
                              settings.clustered_lighting);
  last_forensics_.clustered_lights = buildClusteredLightFrameData(clustered_lights);
  last_forensics_.evidence = {.render_ir_hash = render_world_.scene().ir().content_hash,
                              .visibility_plan_hash = framePlanEvidenceHash(plan),
                              .backend_kind =
                                  static_cast<std::uint32_t>(active_capabilities.kind),
                              .draw_signature_count =
                                  static_cast<std::uint32_t>(plan.groups.size())};
  const bool detailed_forensics = settings.forensics.detailed_traces;
  const bool capture_forensics = settings.forensics.capture_payloads;
  const bool certify_forensics = settings.forensics.backend_certification;
  const bool graph_forensics =
      detailed_forensics || capture_forensics || certify_forensics;
  MaterialFrameSummary material_summary =
      analyzeMaterialFrame(scene, plan, active_capabilities, material_artifact_cache_,
                           previous_transparent_order_);
  previous_transparent_order_ = material_summary.transparent_order;
  if (graph_forensics) {
    frame_debugger_.appendGraphForensics(render_graph_, active_capabilities, framebuffer_width,
                                         framebuffer_height, last_forensics_);
  }
  if (detailed_forensics) {
    appendMaterialBindingTraces(scene, plan, material_library_.get(), active_capabilities,
                                last_forensics_.material_bindings);
    appendMaterialPipelineKeyTraces(scene, plan, settings, material_library_.get(),
                                    active_capabilities, last_forensics_.rhi_trace.pipelines);
    appendObjectDebuggerTraces(scene, plan, camera, settings, framebuffer_width,
                               framebuffer_height, last_forensics_);
    appendAssetFrameTraces(scene, material_library_.get(), active_capabilities,
                           last_forensics_);
    appendObjectRenderFateTraces(scene, plan, settings, material_library_.get(),
                                 active_capabilities, last_forensics_);
    appendSurfacePresentationTraces(scene, settings, last_forensics_);
    last_forensics_.events.insert(last_forensics_.events.end(),
                                  std::make_move_iterator(material_summary.events.begin()),
                                  std::make_move_iterator(material_summary.events.end()));
    appendRenderMathContractDiagnostics(scene, last_forensics_.events);
    appendProjectionConventionDiagnostics(camera, active_capabilities, last_forensics_.events);
    appendRenderMathContractReportDiagnostics(last_forensics_.math_contract,
                                              last_forensics_.events);
    appendMathDiagnosticsToFrame(last_forensics_.events);
  }
  stats.visible_objects = plan.diagnostics.visible_objects;
  stats.culled_objects = plan.diagnostics.culled_objects;
  stats.instance_groups = plan.diagnostics.instance_groups;
  stats.lod_culled_objects = plan.diagnostics.lod_culled_objects;
  stats.visibility_hint_objects = plan.diagnostics.visibility_hint_objects;
  stats.dynamic_mesh_objects = plan.diagnostics.dynamic_mesh_objects;
  stats.dynamic_mesh_cache_entries = asset_cache_.cachedMeshCount();
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
  stats.graph_compile_seconds = graph_compiler_.lastCompileSeconds();
  stats.backend_kind_value =
      static_cast<std::uint32_t>(backendCapabilities().kind);

  SoftwareFrameBuffer &framebuffer = activeFrameBuffer();
  if (native_backend_ != nullptr) {
    PreparedRenderMeshes meshes{
        .box = &asset_cache_.box(),
        .sphere = &asset_cache_.sphere(),
        .plane = &asset_cache_.plane(),
        .contact_shadow_plane = &asset_cache_.contactShadowPlane(),
        .rock = &asset_cache_.rock(),
        .crystal = &asset_cache_.crystal(),
        .ruin_block = &asset_cache_.ruinBlock(),
        .pillar = &asset_cache_.pillar(),
        .custom_meshes = &asset_cache_.customMeshes(),
        .custom_mesh_resources = &asset_cache_.customMeshResources(),
    };
    framebuffer.resize(framebuffer_width, framebuffer_height);
    framebuffer.clearTransparent();
    const FrameExecutionContext context{.scene = scene,
                                        .plan = plan,
                                        .camera = camera,
                                        .settings = settings,
                                        .graph = render_graph_,
                                        .meshes = meshes,
                                        .clustered_lights = &last_forensics_.clustered_lights,
                                        .framebuffer_width = framebuffer_width,
                                        .framebuffer_height = framebuffer_height,
                                        .frame_seconds = frame_seconds,
                                        .material_library = material_library_.get(),
                                        .forensics = &last_forensics_};
    FrameStats native_stats = native_backend_->render(context);
    native_stats.visible_objects = plan.diagnostics.visible_objects;
    native_stats.culled_objects = plan.diagnostics.culled_objects;
    native_stats.instance_groups = plan.diagnostics.instance_groups;
    native_stats.lod_culled_objects = plan.diagnostics.lod_culled_objects;
    native_stats.visibility_hint_objects = plan.diagnostics.visibility_hint_objects;
    native_stats.dynamic_mesh_objects = plan.diagnostics.dynamic_mesh_objects;
    native_stats.dynamic_mesh_cache_entries = asset_cache_.cachedMeshCount();
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
    native_stats.graph_compile_seconds = graph_compiler_.lastCompileSeconds();
	    if (capture_forensics && native_backend_->capabilities().supports_capture) {
	      appendFramebufferCapturePayloads(framebuffer, last_forensics_);
	    }
	    if (certify_forensics) {
	      certifyBackendFrame(render_graph_, native_backend_->capabilities(), settings, native_stats,
	                          last_forensics_);
	    }
	    enrichFramePassCostMap(last_forensics_, render_graph_, settings,
	                           static_cast<std::uint32_t>(framebuffer_width),
	                           static_cast<std::uint32_t>(framebuffer_height), material_summary);
	    if (detailed_forensics) {
	      appendMathDiagnosticsToFrame(last_forensics_.events);
	      finalizeObjectRenderFates(last_forensics_);
	    }
    if (graph_forensics || detailed_forensics || capture_forensics) {
      rebuildFrameEvidenceProducts(render_graph_, native_backend_->capabilities(), last_forensics_);
    }
    native_stats.timestamp_query_slots = last_forensics_.timestamp_samples.size();
    native_stats.resource_lifetime_warnings +=
        last_forensics_.certification.validation_error_count;
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
                              ? asset_cache_.contactShadowPlane()
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

  (void)frame_executor_.execute(render_graph_, [&](const RenderGraphPassInvocation &invocation) {
    const auto pass_start = std::chrono::steady_clock::now();
    const std::size_t draws_before = stats.draw_calls;
    switch (invocation.semantic) {
    case RenderGraphPass::LightCull:
      break;
    case RenderGraphPass::ShadowAtlas:
      buildSoftwareShadowAtlas(scene, camera, settings, software_resources);
      break;
    case RenderGraphPass::SurfaceOcclusion:
      buildSoftwareSurfaceOcclusion(scene, camera, settings, software_resources);
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
      buildSoftwareSurfaceAttributes(scene, camera, settings, software_resources);
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
  if (capture_forensics) {
    appendSoftwareCapturePayloads(software_resources, framebuffer, last_forensics_);
  }
  const auto encode_end = std::chrono::steady_clock::now();
  stats.render_encode_seconds = std::chrono::duration<double>(encode_end - encode_start).count();
  stats.backend_feature_mask = softwareCapabilities().graph_resource_mask;
  stats.backend_kind_value = static_cast<std::uint32_t>(RenderBackendKind::SoftwareReference);
  const rhi::ResourceRegistryStats registry_stats = resource_registry_.stats();
  stats.registry_live_resources = registry_stats.live_resources;
  stats.registry_retired_resources = registry_stats.retired_resources;
  stats.resource_lifetime_warnings = registry_stats.retired_resources;
  stats.graph_compile_seconds = graph_compiler_.lastCompileSeconds();
	  if (certify_forensics) {
	    certifyBackendFrame(render_graph_, softwareCapabilities(), settings, stats, last_forensics_);
	  }
	  enrichFramePassCostMap(last_forensics_, render_graph_, settings,
	                         static_cast<std::uint32_t>(framebuffer_width),
	                         static_cast<std::uint32_t>(framebuffer_height), material_summary);
	  if (detailed_forensics) {
	    appendMathDiagnosticsToFrame(last_forensics_.events);
	    finalizeObjectRenderFates(last_forensics_);
	  }
  if (graph_forensics || detailed_forensics || capture_forensics) {
    rebuildFrameEvidenceProducts(render_graph_, softwareCapabilities(), last_forensics_);
  }
  stats.timestamp_query_slots = last_forensics_.timestamp_samples.size();
  stats.resource_lifetime_warnings += last_forensics_.certification.validation_error_count;

  return stats;
}

const char *RenderDevice::backendName() const {
  if (native_backend_ != nullptr) {
    return native_backend_->backendName();
  }
  return "Aster Learning Software Rasterizer";
}

RendererPresentResult RenderDevice::present(const NativeWindowSurface &surface,
                                            const RendererPresentDesc &desc) {
  if (native_backend_ != nullptr) {
    RendererPresentResult result = native_backend_->present(surface, desc);
    if (result.presented) {
      auto proof = std::find_if(
          last_forensics_.backend_feature_proofs.begin(),
          last_forensics_.backend_feature_proofs.end(),
          [](const BackendFeatureProof &candidate) {
            return candidate.kind == BackendFeatureProofKind::Presentation;
          });
      const std::uint64_t evidence_hash =
          appendEvidenceValue(1469598103934665603ull, result.frame_index);
      if (proof == last_forensics_.backend_feature_proofs.end()) {
        last_forensics_.backend_feature_proofs.push_back(
            {.kind = BackendFeatureProofKind::Presentation,
             .status = BackendFeatureProofStatus::Proven,
             .resource = RenderGraphResource::SceneColor,
             .pass = RenderGraphPass::UiComposite,
             .feature = "presentation",
             .label = "swapchain-present",
             .message = "Backend presented the frame through a native swapchain.",
             .advertised = 1u,
             .native = 1u,
             .evidence_hash = evidence_hash});
      } else {
        proof->status = BackendFeatureProofStatus::Proven;
        proof->label = "swapchain-present";
        proof->message = "Backend presented the frame through a native swapchain.";
        proof->advertised = 1u;
        proof->native = 1u;
        proof->evidence_hash = evidence_hash;
      }
      last_forensics_.certification.proof_count = last_forensics_.backend_feature_proofs.size();
      last_forensics_.certification.proven_count =
          std::count_if(last_forensics_.backend_feature_proofs.begin(),
                        last_forensics_.backend_feature_proofs.end(),
                        [](const BackendFeatureProof &candidate) {
                          return candidate.status == BackendFeatureProofStatus::Proven;
                        });
      last_forensics_.certification.missing_proof_count =
          std::count_if(last_forensics_.backend_feature_proofs.begin(),
                        last_forensics_.backend_feature_proofs.end(),
                        [](const BackendFeatureProof &candidate) {
                          return candidate.status == BackendFeatureProofStatus::MissingProof;
                        });
      last_forensics_.certification.valid =
          last_forensics_.certification.missing_proof_count == 0u &&
          last_forensics_.certification.validation_error_count == 0u &&
          last_forensics_.certification.math_contract_error_count == 0u;
      rebuildFrameEvidenceProducts(render_graph_, backendCapabilities(), last_forensics_);
    }
    return result;
  }
  RendererPresentResult result;
  result.backend = RenderBackendKind::SoftwareReference;
  result.presentation = rhi::PresentationMode::SoftwareFramebuffer;
  result.width = static_cast<std::uint32_t>(std::max(surface.width, 0));
  result.height = static_cast<std::uint32_t>(std::max(surface.height, 0));
  return result;
}

RenderBackendCapabilities RenderDevice::backendCapabilities() const {
  if (native_backend_ != nullptr) {
    return native_backend_->capabilities();
  }
  return softwareCapabilities();
}

RendererPresentationStatus RenderDevice::presentationStatus() const {
  if (native_backend_ != nullptr) {
    return native_backend_->presentationStatus();
  }
  return {.backend = RenderBackendKind::SoftwareReference,
          .presentation = rhi::PresentationMode::SoftwareFramebuffer,
          .native_present_supported = false,
          .bound_window = false};
}

const FixedRenderGraph &RenderDevice::renderGraph() const {
  return render_graph_;
}

const FrameForensics &RenderDevice::lastFrameForensics() const {
  return last_forensics_;
}

void RenderDevice::stampLastFrameCausalTrace(const std::uint64_t world_trace_hash,
                                             const std::uint64_t simulation_tick,
                                             const std::uint64_t extraction_hash,
                                             const std::uint64_t asset_lineage_hash,
                                             const std::uint64_t world_transition_hash,
                                             const std::uint64_t actor_state_delta_hash,
                                             const std::uint64_t sensory_event_hash,
                                             const std::uint64_t visibility_set_hash,
                                             const std::uint64_t encounter_budget_hash,
                                             const bool navigation_valid,
                                             const std::uint64_t streaming_region_id,
                                             const float perceptual_salience_score,
                                             const bool perceptual_continuity_accepted,
                                             const std::uint32_t perceptual_continuity_required_channel_mask,
                                             const std::uint32_t perceptual_continuity_observed_channel_mask,
                                             const std::uint32_t perceptual_continuity_missing_channel_mask,
                                             const float perceptual_continuity_score,
                                             const float perceptual_continuity_minimum_score,
                                             const std::uint64_t reaction_package_hash,
                                             const std::uint64_t material_memory_hash,
                                             const std::uint64_t lighting_atmosphere_hash,
                                             const std::uint64_t ai_attention_hash,
                                             const std::uint64_t streaming_residency_lod_hash,
                                             const std::uint64_t resource_state_hash,
                                             const std::uint64_t event_residue_hash,
                                             const std::uint64_t readability_audit_hash) {
  last_forensics_.world_trace_hash = world_trace_hash;
  last_forensics_.simulation_tick = simulation_tick;
  last_forensics_.extraction_hash = extraction_hash;
  last_forensics_.asset_lineage_hash = asset_lineage_hash;
  last_forensics_.world_transition_linked = world_transition_hash != 0u;
  last_forensics_.world_transition_hash = world_transition_hash;
  last_forensics_.actor_state_delta_hash = actor_state_delta_hash;
  last_forensics_.sensory_event_hash = sensory_event_hash;
  last_forensics_.visibility_set_hash = visibility_set_hash;
  last_forensics_.encounter_budget_hash = encounter_budget_hash;
  last_forensics_.navigation_valid = navigation_valid;
  last_forensics_.streaming_region_id = streaming_region_id;
  last_forensics_.perceptual_salience_score = perceptual_salience_score;
  last_forensics_.perceptual_continuity_accepted = perceptual_continuity_accepted;
  last_forensics_.perceptual_continuity_required_channel_mask =
      perceptual_continuity_required_channel_mask;
  last_forensics_.perceptual_continuity_observed_channel_mask =
      perceptual_continuity_observed_channel_mask;
  last_forensics_.perceptual_continuity_missing_channel_mask =
      perceptual_continuity_missing_channel_mask;
  last_forensics_.perceptual_continuity_score = perceptual_continuity_score;
  last_forensics_.perceptual_continuity_minimum_score = perceptual_continuity_minimum_score;
  last_forensics_.reaction_package_hash = reaction_package_hash;
  last_forensics_.material_memory_hash = material_memory_hash;
  last_forensics_.lighting_atmosphere_hash = lighting_atmosphere_hash;
  last_forensics_.ai_attention_hash = ai_attention_hash;
  last_forensics_.streaming_residency_lod_hash = streaming_residency_lod_hash;
  last_forensics_.resource_state_hash = resource_state_hash;
  last_forensics_.event_residue_hash = event_residue_hash;
  last_forensics_.readability_audit_hash = readability_audit_hash;
}

void RenderDevice::stampLastFramePerceptionLedger(
    const WorldPerceptionLedgerReport &ledger,
    std::vector<WorldPerceptionObjectTrace> object_traces) {
  last_forensics_.perception_ledger_hash = ledger.ledger_hash;
  last_forensics_.perception_ledger_cell_count = ledger.cell_count;
  last_forensics_.perception_ledger_score = ledger.score;
  last_forensics_.perception_ledger_accepted = ledger.accepted;
  last_forensics_.perception_object_traces = std::move(object_traces);
}

const std::shared_ptr<const MaterialResourceLibrary> &RenderDevice::materialResourceLibrary()
    const noexcept {
  return material_library_;
}

} // namespace aster
