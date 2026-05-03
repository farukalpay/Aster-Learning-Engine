// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/terrain_sculpt.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr float kEpsilon = 0.000001f;

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(std::abs(edge1 - edge0), kEpsilon);
  const float t = aster::clamp((value - edge0) / range, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

std::uint32_t mixBits(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

float hashGrid(const int x, const int z, const std::uint32_t seed) {
  std::uint32_t h = seed ^ 0x9e3779b9u;
  h ^= mixBits(static_cast<std::uint32_t>(x) + 0x85ebca6bu);
  h ^= mixBits(static_cast<std::uint32_t>(z) + 0xc2b2ae35u);
  h = mixBits(h);
  return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

float valueNoise2(const float x, const float z, const std::uint32_t seed) {
  const int ix = static_cast<int>(std::floor(x));
  const int iz = static_cast<int>(std::floor(z));
  const float fx = x - static_cast<float>(ix);
  const float fz = z - static_cast<float>(iz);
  const float ux = fx * fx * (3.0f - 2.0f * fx);
  const float uz = fz * fz * (3.0f - 2.0f * fz);
  const float a = hashGrid(ix, iz, seed);
  const float b = hashGrid(ix + 1, iz, seed);
  const float c = hashGrid(ix, iz + 1, seed);
  const float d = hashGrid(ix + 1, iz + 1, seed);
  const float x0 = a + (b - a) * ux;
  const float x1 = c + (d - c) * ux;
  return x0 + (x1 - x0) * uz;
}

float fbm2(float x, float z, const std::uint32_t seed, const int octaves = 5) {
  float amplitude = 0.56f;
  float sum = 0.0f;
  float norm = 0.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += valueNoise2(x, z, seed + static_cast<std::uint32_t>(octave) * 1619u) * amplitude;
    norm += amplitude;
    x = x * 2.03f + 9.7f;
    z = z * 2.03f - 4.1f;
    amplitude *= 0.5f;
  }
  return norm > kEpsilon ? sum / norm : 0.0f;
}

float ridged2(const float x, const float z, const std::uint32_t seed) {
  return 1.0f - std::abs(fbm2(x, z, seed, 5) * 2.0f - 1.0f);
}

int heightIndex(const aster::TerrainHeightField &terrain, const int x, const int z) {
  return z * terrain.grid_size + x;
}

bool validTerrain(const aster::TerrainHeightField &terrain) {
  return terrain.grid_size >= 2 && terrain.square_size > 0.0f &&
         terrain.heights.size() == static_cast<std::size_t>(terrain.grid_size * terrain.grid_size);
}

aster::Vec2 gridWorldPosition(const aster::TerrainHeightField &terrain, const int x, const int z) {
  return {terrain.origin.x + static_cast<float>(x) * terrain.square_size,
          terrain.origin.y + static_cast<float>(z) * terrain.square_size};
}

float ellipseMetric(const aster::Vec2 point, const aster::Vec2 center, const aster::Vec2 radius) {
  const float rx = std::max(std::abs(radius.x), kEpsilon);
  const float rz = std::max(std::abs(radius.y), kEpsilon);
  const float nx = (point.x - center.x) / rx;
  const float nz = (point.y - center.y) / rz;
  return std::sqrt(nx * nx + nz * nz);
}

struct PathSample {
  aster::Vec3 position{};
  aster::Vec3 tangent{0.0f, 0.0f, 1.0f};
};

std::vector<PathSample> samplePath(const aster::TerrainSplinePath &path) {
  const int samples = std::max(path.samples, 2);
  std::vector<PathSample> out;
  out.reserve(static_cast<std::size_t>(samples + 1));
  for (int i = 0; i <= samples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(samples);
    aster::Vec3 tangent = aster::evaluateTerrainSplineTangent(path, t);
    if (aster::length(tangent) <= 0.0001f) {
      tangent = {0.0f, 0.0f, 1.0f};
    }
    out.push_back({aster::evaluateTerrainSplinePath(path, t), tangent});
  }
  return out;
}

struct PathFrame {
  float distance = 0.0f;
  float target_height = 0.0f;
  float t = 0.0f;
};

PathFrame closestPathFrame(const aster::Vec2 point, const std::vector<PathSample> &samples) {
  PathFrame best;
  best.distance = std::numeric_limits<float>::infinity();
  if (samples.empty()) {
    return best;
  }
  best.target_height = samples.front().position.y;

  for (std::size_t i = 0; i + 1u < samples.size(); ++i) {
    const aster::Vec3 a = samples[i].position;
    const aster::Vec3 b = samples[i + 1u].position;
    const aster::Vec2 ab{b.x - a.x, b.z - a.z};
    const float segment_len_sq = aster::dot(ab, ab);
    const float u = segment_len_sq > kEpsilon
                        ? aster::clamp(aster::dot(aster::Vec2{point.x - a.x, point.y - a.z}, ab) /
                                           segment_len_sq,
                                       0.0f, 1.0f)
                        : 0.0f;
    const aster::Vec2 closest{a.x + ab.x * u, a.z + ab.y * u};
    const float distance = aster::length(point - closest);
    if (distance < best.distance) {
      best.distance = distance;
      best.target_height = a.y + (b.y - a.y) * u;
      best.t = (static_cast<float>(i) + u) / static_cast<float>(samples.size() - 1u);
    }
  }
  return best;
}

PathFrame closestRouteFrame(const aster::Vec2 point,
                            const std::vector<std::vector<PathSample>> &segments) {
  PathFrame best;
  best.distance = std::numeric_limits<float>::infinity();
  const float segment_count = static_cast<float>(std::max<std::size_t>(segments.size(), 1u));

  for (std::size_t segment_index = 0; segment_index < segments.size(); ++segment_index) {
    PathFrame frame = closestPathFrame(point, segments[segment_index]);
    if (frame.distance < best.distance) {
      frame.t = (static_cast<float>(segment_index) + frame.t) / segment_count;
      best = frame;
    }
  }

  return best;
}

void deformTerrainWithPathSamples(aster::TerrainHeightField &terrain,
                                  const std::vector<std::vector<PathSample>> &path_samples,
                                  const float half_width, const float shoulder_width,
                                  const float crown_height, const float surface_noise,
                                  const float max_raise, const float max_lower,
                                  const std::uint32_t seed) {
  const float outer_width = half_width + shoulder_width;
  for (int z = 0; z < terrain.grid_size; ++z) {
    for (int x = 0; x < terrain.grid_size; ++x) {
      const aster::Vec2 world = gridWorldPosition(terrain, x, z);
      const PathFrame frame = closestRouteFrame(world, path_samples);
      if (!std::isfinite(frame.distance) || frame.distance > outer_width) {
        continue;
      }

      const float core_weight = 1.0f - smoothstep(half_width, outer_width, frame.distance);
      const float center_weight = 1.0f - smoothstep(0.0f, half_width, frame.distance);
      const float weather = (fbm2(world.x * 0.92f + frame.t * 3.7f,
                                  world.y * 0.92f - frame.t * 2.3f, seed + 311u, 3) -
                             0.5f) *
                            surface_noise * core_weight;
      const float target_height = frame.target_height + crown_height * center_weight + weather;
      float &height = terrain.heights[heightIndex(terrain, x, z)];
      const float raise_limit = std::max(max_raise, 0.0f);
      const float lower_limit = std::max(max_lower, 0.0f);
      const float limited_delta = aster::clamp(target_height - height, -lower_limit, raise_limit);
      height = height + limited_delta * core_weight;
    }
  }
}

} // namespace

namespace aster {

Vec3 evaluateTerrainSplinePath(const TerrainSplinePath &path, const float t_in) {
  const float t = clamp(t_in, 0.0f, 1.0f);
  const float u = 1.0f - t;
  return path.start * (u * u * u) + path.control * (3.0f * u * u * t) +
         path.control_b * (3.0f * u * t * t) + path.end * (t * t * t);
}

Vec3 evaluateTerrainSplineTangent(const TerrainSplinePath &path, const float t_in) {
  const float t = clamp(t_in, 0.0f, 1.0f);
  const float u = 1.0f - t;
  return normalize((path.control - path.start) * (3.0f * u * u) +
                   (path.control_b - path.control) * (6.0f * u * t) +
                   (path.end - path.control_b) * (3.0f * t * t));
}

void applyTerrainMountainBrush(TerrainHeightField &terrain, const TerrainMountainBrushSpec &spec) {
  if (!validTerrain(terrain)) {
    throw std::invalid_argument("Mountain terrain brush requires a complete height field.");
  }
  if (spec.radius.x <= 0.0f || spec.radius.y <= 0.0f) {
    throw std::invalid_argument("Mountain terrain brush requires positive radii.");
  }

  for (int z = 0; z < terrain.grid_size; ++z) {
    for (int x = 0; x < terrain.grid_size; ++x) {
      const Vec2 world = gridWorldPosition(terrain, x, z);
      const float metric = ellipseMetric(world, spec.center, spec.radius);
      if (metric >= 1.0f) {
        continue;
      }

      const float body = 1.0f - smoothstep(std::max(spec.inner_plateau, 0.0f), 1.0f, metric);
      const float shoulder = 1.0f - smoothstep(0.72f, 1.0f, metric);
      const float ridge =
          ridged2(world.x * spec.ridge_frequency, world.y * spec.ridge_frequency, spec.seed + 97u);
      const float weather = (fbm2(world.x * spec.ridge_frequency * 3.1f,
                                  world.y * spec.ridge_frequency * 3.1f, spec.seed + 211u, 4) -
                             0.5f) *
                            spec.surface_noise;
      const float lift = spec.height * body * (0.82f + ridge * 0.26f) +
                         spec.shoulder_height * shoulder + weather * shoulder;
      terrain.heights[heightIndex(terrain, x, z)] += std::max(lift, 0.0f);
    }
  }
}

void deformTerrainAlongPath(TerrainHeightField &terrain, const TerrainPathDeformationSpec &spec) {
  if (!validTerrain(terrain)) {
    throw std::invalid_argument("Path terrain deformation requires a complete height field.");
  }
  if (spec.half_width <= 0.0f || spec.shoulder_width < 0.0f) {
    throw std::invalid_argument("Path terrain deformation requires positive width.");
  }

  deformTerrainWithPathSamples(terrain, {samplePath(spec.path)}, spec.half_width,
                               spec.shoulder_width, spec.crown_height, spec.surface_noise,
                               spec.max_raise, spec.max_lower, spec.seed);
}

void deformTerrainAlongRoute(TerrainHeightField &terrain, const TerrainRouteDeformationSpec &spec) {
  if (!validTerrain(terrain)) {
    throw std::invalid_argument("Route terrain deformation requires a complete height field.");
  }
  if (spec.route.segments.empty()) {
    throw std::invalid_argument("Route terrain deformation requires at least one path segment.");
  }
  if (spec.half_width <= 0.0f || spec.shoulder_width < 0.0f) {
    throw std::invalid_argument("Route terrain deformation requires positive width.");
  }

  std::vector<std::vector<PathSample>> path_samples;
  path_samples.reserve(spec.route.segments.size());
  for (const TerrainSplinePath &segment : spec.route.segments) {
    path_samples.push_back(samplePath(segment));
  }

  deformTerrainWithPathSamples(terrain, path_samples, spec.half_width, spec.shoulder_width,
                               spec.crown_height, spec.surface_noise, spec.max_raise,
                               spec.max_lower, spec.seed);
}

void sculptTerrainPortalShelf(TerrainHeightField &terrain, const TerrainPortalShelfSpec &spec) {
  if (!validTerrain(terrain)) {
    throw std::invalid_argument("Portal terrain shelf requires a complete height field.");
  }
  if (spec.half_width <= 0.0f || spec.front_depth <= 0.0f || spec.back_depth <= 0.0f ||
      spec.feather < 0.0f) {
    throw std::invalid_argument("Portal terrain shelf requires positive dimensions.");
  }

  Vec2 inward = normalize(Vec2{spec.inward.x, spec.inward.z});
  if (length(inward) <= 0.0001f) {
    inward = {0.0f, -1.0f};
  }
  const Vec2 side{-inward.y, inward.x};
  const Vec2 entrance{spec.entrance.x, spec.entrance.z};
  const float outer_side = spec.half_width + spec.feather;
  const float outer_front = spec.front_depth + spec.feather;
  const float outer_back = spec.back_depth + spec.feather;

  for (int z = 0; z < terrain.grid_size; ++z) {
    for (int x = 0; x < terrain.grid_size; ++x) {
      const Vec2 world = gridWorldPosition(terrain, x, z);
      const Vec2 offset = world - entrance;
      const float forward = dot(offset, inward);
      const float lateral = std::abs(dot(offset, side));
      if (lateral > outer_side || forward < -outer_front || forward > outer_back) {
        continue;
      }

      const float lateral_weight = 1.0f - smoothstep(spec.half_width, outer_side, lateral);
      const float forward_limit = forward < 0.0f ? spec.front_depth : spec.back_depth;
      const float forward_weight =
          1.0f - smoothstep(forward_limit, forward_limit + spec.feather, std::abs(forward));
      const float shelf_weight = lateral_weight * forward_weight;
      float &height = terrain.heights[heightIndex(terrain, x, z)];
      if (shelf_weight > 0.0f) {
        height = height + (spec.floor_height - height) * shelf_weight;
      }

      const float side_band = 1.0f - smoothstep(spec.half_width * 0.78f, outer_side, lateral);
      const float side_outer =
          smoothstep(spec.half_width * 0.48f, spec.half_width * 1.10f, lateral);
      const float depth_band =
          smoothstep(-spec.front_depth * 0.42f, spec.back_depth * 0.62f, forward) *
          (1.0f - smoothstep(spec.back_depth * 0.74f, outer_back, forward));
      const float rear_band = smoothstep(0.0f, spec.back_depth * 0.72f, forward) *
                              (1.0f - smoothstep(spec.back_depth, outer_back, forward)) *
                              (1.0f - smoothstep(spec.half_width * 1.65f, outer_side, lateral));
      const float weather =
          (fbm2(world.x * 0.68f, world.y * 0.68f, spec.seed + 419u, 4) - 0.5f) * spec.surface_noise;
      const float berm_target =
          spec.floor_height + side_band * side_outer * depth_band * spec.side_berm_height +
          rear_band * spec.rear_cover_height + weather * std::max(depth_band, rear_band);
      height = std::max(height, berm_target);
    }
  }
}

} // namespace aster
