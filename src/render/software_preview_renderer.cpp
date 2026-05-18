// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/software_preview_renderer.hpp"

#include "aster/math/color.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
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
  Vec3 normal{};
  Vec2 uv{};
  Material material{};
};

struct TraceTriangle {
  Vec3 a{};
  Vec3 b{};
  Vec3 c{};
  Vec3 na{};
  Vec3 nb{};
  Vec3 nc{};
  Vec2 uva{};
  Vec2 uvb{};
  Vec2 uvc{};
};

struct PreparedObject {
  RenderObject object{};
  std::vector<TraceTriangle> triangles;
  Vec3 bounds_min{};
  Vec3 bounds_max{};
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

std::uint8_t toByte(const float value) {
  return static_cast<std::uint8_t>(saturate(value) * 255.0f + 0.5f);
}

void expandBounds(PreparedObject &out, const Vec3 point) {
  out.bounds_min.x = std::min(out.bounds_min.x, point.x);
  out.bounds_min.y = std::min(out.bounds_min.y, point.y);
  out.bounds_min.z = std::min(out.bounds_min.z, point.z);
  out.bounds_max.x = std::max(out.bounds_max.x, point.x);
  out.bounds_max.y = std::max(out.bounds_max.y, point.y);
  out.bounds_max.z = std::max(out.bounds_max.z, point.z);
}

void prepareTriangleMesh(PreparedObject &out, const CpuMesh &mesh) {
  const Mat4 model = out.object.transform.matrix();
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
    const Vec3 na = normalize(transformVector(model, va.normal));
    const Vec3 nb = normalize(transformVector(model, vb.normal));
    const Vec3 nc = normalize(transformVector(model, vc.normal));
    out.triangles.push_back({a,
                             b,
                             c,
                             length(na) > 0.0001f ? na : face,
                             length(nb) > 0.0001f ? nb : face,
                             length(nc) > 0.0001f ? nc : face,
                             va.uv,
                             vb.uv,
                             vc.uv});
    expandBounds(out, a);
    expandBounds(out, b);
    expandBounds(out, c);
  }
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

bool intersectBounds(const Ray &ray, const Vec3 bounds_min, const Vec3 bounds_max,
                     const float max_distance) {
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

bool intersectPreparedMesh(const Ray &ray, const PreparedObject &prepared, Hit &hit) {
  if (prepared.triangles.empty() ||
      !intersectBounds(ray, prepared.bounds_min, prepared.bounds_max, hit.distance)) {
    return false;
  }

  bool found = false;
  for (const TraceTriangle &triangle : prepared.triangles) {
    float t = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    if (!intersectTriangle(ray, triangle, t, u, v) || t >= hit.distance) {
      continue;
    }
    const float w = 1.0f - u - v;
    Vec3 normal = normalize(triangle.na * w + triangle.nb * u + triangle.nc * v);
    if (dot(normal, ray.direction) > 0.0f) {
      normal = normal * -1.0f;
    }
    hit.valid = true;
    hit.distance = t;
    hit.position = ray.origin + ray.direction * t;
    hit.normal = normal;
    hit.uv = triangle.uva * w + triangle.uvb * u + triangle.uvc * v;
    hit.material = prepared.object.material;
    found = true;
  }
  return found;
}

Hit trace(const Ray &ray, const std::vector<PreparedObject> &scene) {
  Hit closest;
  for (const PreparedObject &prepared : scene) {
    intersectPreparedMesh(ray, prepared, closest);
  }
  return closest;
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
      hit.material.base_color * (0.78f + variation * 0.34f) + Vec3{0.035f, 0.024f, 0.014f};
  const Vec3 mortar = hit.material.base_color * (0.32f + variation * 0.08f);
  Vec3 color = mixVec(mortar, brick, cell);
  return mixVec(color, color * (0.74f + 0.36f * cell),
                std::clamp(hit.material.pattern_contrast, 0.0f, 1.0f));
}

Vec3 corrodedMetalAlbedo(const Hit &hit) {
  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const float broad = projectedFbm(hit.position, hit.normal, detail * 0.16f, 307.0f);
  const float fine = projectedFbm(hit.position + hit.normal * 0.05f, hit.normal, detail * 0.74f,
                                  331.0f);
  const float pitting = ridge(projectedFbm(hit.position, hit.normal, detail * 1.38f, 359.0f));
  const float upward = smoothstep(0.14f, 0.80f, hit.normal.y);
  const float oxide =
      smoothstep(0.42f, 0.95f,
                 broad * 0.48f + pitting * 0.34f + upward * hit.material.pattern_depth * 0.42f);
  const float dark_scale = 0.60f + fine * 0.22f;
  const Vec3 cool_steel = hit.material.base_color * Vec3{0.82f, 0.88f, 0.92f} * dark_scale;
  const Vec3 exposed_edge =
      hit.material.base_color * Vec3{1.18f, 1.16f, 1.06f} * (0.66f + pitting * 0.16f);
  const Vec3 orange_rust{0.42f, 0.15f, 0.045f};
  const Vec3 black_rust{0.085f, 0.060f, 0.043f};
  Vec3 color = mixVec(cool_steel, mixVec(black_rust, orange_rust, fine), oxide);
  color = mixVec(color, exposed_edge, hit.material.edge_wear * smoothstep(0.72f, 0.98f, pitting));
  return clamp(color, 0.0f, 4.0f);
}

Vec3 weldBeadAlbedo(const Hit &hit) {
  const float bead_ripple =
      0.5f + 0.5f * std::sin(hit.uv.y * std::max(hit.material.pattern_scale.y, 0.001f) * kTau +
                              projectedFbm(hit.position, hit.normal, hit.material.detail_scale,
                                           401.0f) *
                                  3.8f);
  const float heat_band = smoothstep(0.05f, 0.58f, ridge(hit.uv.x));
  const Vec3 base = hit.material.base_color * (0.76f + bead_ripple * 0.28f);
  const Vec3 straw{0.72f, 0.38f, 0.12f};
  const Vec3 blue_heat{0.11f, 0.16f, 0.30f};
  Vec3 color = mixVec(base, straw, heat_band * 0.24f);
  color = mixVec(color, blue_heat, heat_band * (1.0f - bead_ripple) * 0.16f);
  return clamp(color, 0.0f, 4.0f);
}

Vec3 previewAlbedo(const Hit &hit) {
  switch (resolveMaterialSurfaceProfile(hit.material)) {
  case MaterialSurfaceProfile::Masonry:
    return courseCellAlbedo(hit);
  case MaterialSurfaceProfile::CorrodedMetal:
    return corrodedMetalAlbedo(hit);
  case MaterialSurfaceProfile::WeldBead:
    return weldBeadAlbedo(hit);
  case MaterialSurfaceProfile::Auto:
  case MaterialSurfaceProfile::Plain:
  case MaterialSurfaceProfile::OrganicFiber:
  case MaterialSurfaceProfile::TerrainLayer:
  case MaterialSurfaceProfile::Liquid:
  case MaterialSurfaceProfile::Foliage:
  case MaterialSurfaceProfile::Resin:
  case MaterialSurfaceProfile::PaintedWood:
  case MaterialSurfaceProfile::Feather:
  case MaterialSurfaceProfile::Scales:
  case MaterialSurfaceProfile::StratifiedRock:
  case MaterialSurfaceProfile::MineralVein:
  case MaterialSurfaceProfile::ContactShadow:
  case MaterialSurfaceProfile::FilamentWeb:
  case MaterialSurfaceProfile::ChitinShell:
  case MaterialSurfaceProfile::EmissiveLens:
    break;
  }

  const float detail = std::max(hit.material.detail_scale, 0.001f);
  const float broad = projectedFbm(hit.position, hit.normal, detail * 0.22f, 19.0f);
  const float fine = projectedFbm(hit.position, hit.normal, detail * 0.84f, 29.0f);
  const float weight = saturate(hit.material.detail_strength + hit.material.pattern_contrast * 0.24f);
  return clamp(hit.material.base_color.value *
                   std::lerp(1.0f, 0.86f + broad * 0.20f + fine * 0.08f, weight),
               0.0f, 4.0f);
}

float effectiveRoughness(const Hit &hit) {
  const float variation = hit.material.procedural.roughness_variation;
  if (variation <= 0.0001f) {
    return std::clamp(hit.material.roughness, 0.045f, 1.0f);
  }
  const float noise = projectedFbm(hit.position, hit.normal, std::max(hit.material.detail_scale, 1.0f),
                                   503.0f);
  return std::clamp(hit.material.roughness + (noise - 0.5f) * variation * 0.24f, 0.045f, 1.0f);
}

Vec3 shade(const Hit &hit, const Ray &ray, const RendererSettings &settings) {
  Hit sample_hit = hit;
  sample_hit.position = snapProceduralSamplePosition(hit.position, settings.style);
  const Vec3 albedo = previewAlbedo(sample_hit);
  Vec3 normal = normalize(hit.normal);
  const float normal_strength = std::max(hit.material.procedural.micro_normal_strength,
                                         hit.material.detail_strength * 0.16f);
  if (normal_strength > 0.0001f) {
    const Vec3 bump{
        projectedFbm(sample_hit.position, normal, hit.material.detail_scale * 1.10f, 607.0f) -
            0.5f,
        projectedFbm(sample_hit.position, normal, hit.material.detail_scale * 1.35f, 619.0f) -
            0.5f,
        projectedFbm(sample_hit.position, normal, hit.material.detail_scale * 1.58f, 631.0f) -
            0.5f};
    normal = normalize(normal + bump * std::clamp(normal_strength, 0.0f, 0.90f));
  }

  const Vec3 view = normalize(ray.origin - hit.position);
  const float sky = saturate(normal.y * 0.5f + 0.5f);
  const float ambient = std::max(settings.ambient_strength * hit.material.ambient_occlusion,
                                 settings.ambient_floor);
  Vec3 color = mixVec(settings.ground_ambient_color, settings.sky_ambient_color, sky) * albedo *
                   ambient +
               albedo * 0.025f;

  const float metallic = std::clamp(hit.material.metallic, 0.0f, 1.0f);
  const float roughness = effectiveRoughness(sample_hit);
  const float alpha = roughness * roughness;
  const float alpha2 = std::max(alpha * alpha, 0.0005f);
  const float n_dot_v = std::max(dot(normal, view), 0.001f);
  const float k = ((roughness + 1.0f) * (roughness + 1.0f)) * 0.125f;
  const Vec3 f0 = mixVec({0.04f, 0.04f, 0.04f}, albedo, metallic);

  const auto add_light = [&](const Vec3 light_dir, const Vec3 radiance) {
    const Vec3 half_vector = normalize(light_dir + view);
    const float n_dot_l = std::max(dot(normal, light_dir), 0.0f);
    const float n_dot_h = std::max(dot(normal, half_vector), 0.0f);
    const float h_dot_v = std::max(dot(half_vector, view), 0.0f);
    const float distribution_denominator = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;
    const float distribution =
        alpha2 / std::max(kPi * distribution_denominator * distribution_denominator, 0.001f);
    const float geometry_l = n_dot_l / std::max(n_dot_l * (1.0f - k) + k, 0.001f);
    const float geometry_v = n_dot_v / std::max(n_dot_v * (1.0f - k) + k, 0.001f);
    const Vec3 fresnel = f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * std::pow(1.0f - h_dot_v, 5.0f);
    const Vec3 specular = fresnel * (distribution * geometry_l * geometry_v);
    const Vec3 diffuse = albedo * ((1.0f - metallic) * 0.82f);
    color = color + (diffuse + specular) * (radiance * n_dot_l);
  };

  if (settings.sun_light.enabled && settings.sun_light.intensity > 0.0f) {
    const Vec3 sun_dir = normalize(settings.sun_light.direction_to_light);
    if (length(sun_dir) > 0.0001f) {
      add_light(sun_dir, settings.sun_light.color * settings.sun_light.intensity);
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
    add_light(normalize(light_vector), light.color * (light.intensity / softened_distance));
  }

  color = mixVec(color,
                 albedo * std::max(settings.ambient_strength + settings.ambient_floor + 0.14f,
                                   0.18f),
                 std::clamp(settings.style.unlit_mix, 0.0f, 1.0f));
  color = color + hit.material.emission_color * hit.material.emission_strength *
                      std::max(settings.style.emissive_gain, 0.0f);
  if (settings.atmosphere.enabled) {
    const float fog = evaluateFogFactor(settings.atmosphere, length(ray.origin - hit.position));
    color = mixVec(color, settings.atmosphere.fog_color, fog);
  }
  color = aces_tonemap(color * settings.exposure);
  color = applyRenderStylePost(color, settings.style);
  return gamma_encode(color);
}

Vec3 skyColor(const Ray &ray, const RendererSettings &settings) {
  const float t = saturate(ray.direction.y * 0.5f + 0.5f);
  const Vec3 base = mixVec(settings.pipeline.clear_color * 0.72f,
                           settings.sky_ambient_color * 0.32f + Vec3{0.015f, 0.020f, 0.030f}, t);
  Vec3 color = aces_tonemap(base * settings.exposure);
  color = applyRenderStylePost(color, settings.style);
  return gamma_encode(color);
}

} // namespace

SoftwareFrameBuffer renderSoftwarePreview(const Scene &scene, const OrbitCamera &camera,
                                          const SoftwarePreviewOptions &options) {
  if (options.width <= 0 || options.height <= 0) {
    throw std::invalid_argument("Software preview dimensions must be positive.");
  }

  const int samples_per_axis = std::clamp(options.samples_per_axis, 1, 4);
  const float inv_sample_count =
      1.0f / static_cast<float>(samples_per_axis * samples_per_axis);
  const std::vector<PreparedObject> prepared_scene = prepareScene(scene);
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(options.width) *
                                 static_cast<std::size_t>(options.height) * 4u);

  for (int y = 0; y < options.height; ++y) {
    for (int x = 0; x < options.width; ++x) {
      Vec3 accumulated{};
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
          const Ray ray{camera_ray.origin, camera_ray.direction};
          const Hit hit = trace(ray, prepared_scene);
          accumulated = accumulated + (hit.valid ? shade(hit, ray, options.settings)
                                                 : skyColor(ray, options.settings));
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
    }
  }

  SoftwareFrameBuffer framebuffer;
  framebuffer.replaceRgba8(options.width, options.height, rgba);
  return framebuffer;
}

} // namespace aster
