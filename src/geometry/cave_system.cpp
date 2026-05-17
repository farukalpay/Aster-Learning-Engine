// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/cave_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;

std::uint32_t mixBits(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

float hashGrid(const int x, const int y, const int z, const std::uint32_t seed) {
  std::uint32_t h = seed ^ 0x9e3779b9u;
  h ^= mixBits(static_cast<std::uint32_t>(x) + 0x85ebca6bu);
  h ^= mixBits(static_cast<std::uint32_t>(y) + 0xc2b2ae35u);
  h ^= mixBits(static_cast<std::uint32_t>(z) + 0x27d4eb2fu);
  h = mixBits(h);
  return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

float valueNoise3(const aster::Vec3 point, const std::uint32_t seed) {
  const int ix = static_cast<int>(std::floor(point.x));
  const int iy = static_cast<int>(std::floor(point.y));
  const int iz = static_cast<int>(std::floor(point.z));
  const float fx = point.x - static_cast<float>(ix);
  const float fy = point.y - static_cast<float>(iy);
  const float fz = point.z - static_cast<float>(iz);
  const float ux = fx * fx * (3.0f - 2.0f * fx);
  const float uy = fy * fy * (3.0f - 2.0f * fy);
  const float uz = fz * fz * (3.0f - 2.0f * fz);

  float values[2][2][2]{};
  for (int z = 0; z < 2; ++z) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        values[z][y][x] = hashGrid(ix + x, iy + y, iz + z, seed);
      }
    }
  }

  const auto lerp = [](const float a, const float b, const float t) { return a + (b - a) * t; };
  const float x00 = lerp(values[0][0][0], values[0][0][1], ux);
  const float x10 = lerp(values[0][1][0], values[0][1][1], ux);
  const float x01 = lerp(values[1][0][0], values[1][0][1], ux);
  const float x11 = lerp(values[1][1][0], values[1][1][1], ux);
  const float y0 = lerp(x00, x10, uy);
  const float y1 = lerp(x01, x11, uy);
  return lerp(y0, y1, uz);
}

float fbm3(aster::Vec3 point, const std::uint32_t seed, const int octaves = 5) {
  float amplitude = 0.56f;
  float sum = 0.0f;
  float norm = 0.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += valueNoise3(point, seed + static_cast<std::uint32_t>(octave) * 1013u) * amplitude;
    norm += amplitude;
    point = point * 2.03f + aster::Vec3{11.7f, -4.3f, 6.1f};
    amplitude *= 0.5f;
  }
  return norm > kEpsilon ? sum / norm : 0.0f;
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(std::abs(edge1 - edge0), kEpsilon);
  const float t = aster::clamp((value - edge0) / range, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

aster::Vec3 faceNormal(const aster::Vec3 a, const aster::Vec3 b, const aster::Vec3 c) {
  return aster::normalize(aster::cross(b - a, c - a));
}

void accumulateNormal(aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                      const std::uint32_t c) {
  const aster::Vec3 normal =
      faceNormal(mesh.vertices[a].position, mesh.vertices[b].position, mesh.vertices[c].position);
  mesh.vertices[a].normal = mesh.vertices[a].normal + normal;
  mesh.vertices[b].normal = mesh.vertices[b].normal + normal;
  mesh.vertices[c].normal = mesh.vertices[c].normal + normal;
}

void finalizeNormals(aster::CpuMesh &mesh) {
  for (aster::Vertex &vertex : mesh.vertices) {
    vertex.normal = aster::normalize(vertex.normal);
    if (aster::length(vertex.normal) <= 0.0001f) {
      vertex.normal = {0.0f, 1.0f, 0.0f};
    }
  }
}

void appendIndexedQuad(aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                       const std::uint32_t c, const std::uint32_t d) {
  mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
  accumulateNormal(mesh, a, b, d);
  accumulateNormal(mesh, b, c, d);
}

void appendOrientedTriangle(aster::CpuMesh &mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c,
                            const aster::Vec3 preferred_normal) {
  const aster::Vec3 normal =
      faceNormal(mesh.vertices[a].position, mesh.vertices[b].position, mesh.vertices[c].position);
  if (aster::length(normal) > 0.0001f && aster::dot(normal, preferred_normal) < 0.0f) {
    std::swap(b, c);
  }
  mesh.indices.insert(mesh.indices.end(), {a, b, c});
  accumulateNormal(mesh, a, b, c);
}

void appendOrientedQuad(aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                        const std::uint32_t c, const std::uint32_t d,
                        const aster::Vec3 preferred_normal) {
  appendOrientedTriangle(mesh, a, b, d, preferred_normal);
  appendOrientedTriangle(mesh, b, c, d, preferred_normal);
}

aster::Vec3 cubicPoint(const aster::CaveTunnelProfile &profile, const float t) {
  const float u = 1.0f - t;
  return profile.start * (u * u * u) + profile.control * (3.0f * u * u * t) +
         profile.control_b * (3.0f * u * t * t) + profile.end * (t * t * t);
}

aster::Vec3 cubicTangent(const aster::CaveTunnelProfile &profile, const float t) {
  const float u = 1.0f - t;
  return aster::normalize((profile.control - profile.start) * (3.0f * u * u) +
                          (profile.control_b - profile.control) * (6.0f * u * t) +
                          (profile.end - profile.control_b) * (3.0f * t * t));
}

float chamberWeight(const aster::CaveTunnelProfile &profile, const float t) {
  const float falloff = std::max(profile.chamber_falloff, 0.001f);
  const float distance = std::abs(t - aster::clamp(profile.chamber_t, 0.0f, 1.0f));
  return 1.0f - smoothstep(0.0f, falloff, distance);
}

float tunnelWidthScale(const aster::CaveTunnelProfile &profile, const float t) {
  return 1.0f + chamberWeight(profile, t) * std::max(profile.chamber_width_scale - 1.0f, 0.0f);
}

float tunnelHeightScale(const aster::CaveTunnelProfile &profile, const float t) {
  return 1.0f + chamberWeight(profile, t) * std::max(profile.chamber_height_scale - 1.0f, 0.0f);
}

float tunnelPathLength(const aster::CaveTunnelProfile &profile, const float start_t,
                       const float end_t) {
  const int samples = std::max(profile.length_segments, 8);
  const float a = aster::clamp(std::min(start_t, end_t), 0.0f, 1.0f);
  const float b = aster::clamp(std::max(start_t, end_t), a, 1.0f);
  float length = 0.0f;
  aster::Vec3 previous = cubicPoint(profile, a);
  for (int i = 1; i <= samples; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(samples);
    const float t = a + (b - a) * u;
    const aster::Vec3 next = cubicPoint(profile, t);
    length += aster::length(next - previous);
    previous = next;
  }
  return length;
}

void tunnelBasis(const aster::CaveTunnelProfile &profile, const float t, aster::Vec3 &side,
                 aster::Vec3 &up) {
  aster::Vec3 tangent = cubicTangent(profile, t);
  if (aster::length(tangent) <= 0.0001f) {
    tangent = {0.0f, 0.0f, -1.0f};
  }
  side = aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, tangent));
  if (aster::length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  up = aster::normalize(aster::cross(tangent, side));
  if (aster::length(up) <= 0.0001f) {
    up = {0.0f, 1.0f, 0.0f};
  }
}

struct TunnelFrameSample {
  float t = 0.0f;
  aster::Vec3 floor_center{};
  aster::Vec3 tangent{0.0f, 0.0f, -1.0f};
  aster::Vec3 side{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
  float distance_sq = 0.0f;
  float half_width = 0.0f;
  float height = 0.0f;
  float floor_half_width = 0.0f;
};

TunnelFrameSample closestTunnelFrame(const aster::CaveTunnelProfile &profile,
                                     const aster::Vec3 position) {
  const int samples = std::max(profile.length_segments, 16);
  float best_distance_sq = std::numeric_limits<float>::infinity();
  float best_t = 0.0f;
  aster::Vec3 best_point = profile.start;

  for (int segment = 0; segment < samples; ++segment) {
    const float t0 = static_cast<float>(segment) / static_cast<float>(samples);
    const float t1 = static_cast<float>(segment + 1) / static_cast<float>(samples);
    const aster::Vec3 a = cubicPoint(profile, t0);
    const aster::Vec3 b = cubicPoint(profile, t1);
    const aster::Vec3 ab = b - a;
    const float len_sq = aster::dot(ab, ab);
    const float u =
        len_sq > kEpsilon ? aster::clamp(aster::dot(position - a, ab) / len_sq, 0.0f, 1.0f) : 0.0f;
    const aster::Vec3 closest = a + ab * u;
    const float distance_sq = aster::dot(position - closest, position - closest);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_t = t0 + (t1 - t0) * u;
      best_point = closest;
    }
  }

  TunnelFrameSample frame;
  frame.t = aster::clamp(best_t, 0.0f, 1.0f);
  frame.floor_center = best_point;
  frame.tangent = cubicTangent(profile, frame.t);
  tunnelBasis(profile, frame.t, frame.side, frame.up);
  frame.distance_sq = best_distance_sq;
  frame.half_width = profile.half_width * tunnelWidthScale(profile, frame.t);
  frame.height = profile.wall_height * tunnelHeightScale(profile, frame.t);
  frame.floor_half_width = profile.floor_width * tunnelWidthScale(profile, frame.t) * 0.5f;
  return frame;
}

float ringRadiusNoise(const aster::CaveTunnelProfile &profile, const aster::Vec3 center,
                      const int ring, const int radial) {
  const float n = fbm3(center * 0.33f + aster::Vec3{static_cast<float>(ring) * 0.17f,
                                                    static_cast<float>(radial) * 0.11f, 4.3f},
                       profile.seed + 17u, 4);
  return aster::clamp(1.0f + (n * 2.0f - 1.0f) * std::max(profile.wall_noise, 0.0f), 0.68f, 1.36f);
}

aster::CpuMesh makeTunnelChunk(const aster::CaveTunnelProfile &profile, const int first_segment,
                               const int last_segment) {
  const int radial_segments = std::max(profile.radial_segments, 8);
  const int segment_count = std::max(last_segment - first_segment, 1);
  const int rings = segment_count + 1;
  const int ring_vertices = radial_segments + 1;
  const int total_segments = std::max(profile.length_segments, 2);

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(rings * ring_vertices));
  mesh.indices.reserve(static_cast<std::size_t>(segment_count * radial_segments * 6));

  for (int ring = 0; ring < rings; ++ring) {
    const int segment = first_segment + ring;
    const float t = static_cast<float>(segment) / static_cast<float>(total_segments);
    const aster::Vec3 floor_center = cubicPoint(profile, aster::clamp(t, 0.0f, 1.0f));
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(profile, t, side, up);
    const float height_scale = tunnelHeightScale(profile, t);
    const aster::Vec3 ring_center =
        floor_center + up * (profile.wall_height * height_scale * 0.55f);
    for (int radial = 0; radial <= radial_segments; ++radial) {
      const float u = static_cast<float>(radial) / static_cast<float>(radial_segments);
      const float theta = u * kPi * 2.0f;
      const float horizontal_radius = profile.half_width * tunnelWidthScale(profile, t) *
                                      ringRadiusNoise(profile, floor_center, segment, radial);
      const float sin_theta = std::sin(theta);
      const float vertical_radius = sin_theta >= 0.0f ? profile.wall_height * height_scale * 0.66f
                                                      : profile.wall_height * height_scale * 0.55f;
      const aster::Vec3 radial_vector =
          side * (std::cos(theta) * horizontal_radius) + up * (sin_theta * vertical_radius);
      const aster::Vec3 position = ring_center + radial_vector;
      const aster::Vec3 inward = aster::normalize(radial_vector * -1.0f);
      mesh.vertices.push_back(
          {position,
           aster::length(inward) > 0.0f ? inward : aster::Vec3{0.0f, 1.0f, 0.0f},
           {u * 2.0f, t * 6.0f}});
    }
  }

  for (int ring = 0; ring < segment_count; ++ring) {
    for (int radial = 0; radial < radial_segments; ++radial) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + radial);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + radial);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }

  return mesh;
}

void appendMesh(aster::CpuMesh &target, const aster::CpuMesh &source) {
  const std::uint32_t base = static_cast<std::uint32_t>(target.vertices.size());
  target.vertices.insert(target.vertices.end(), source.vertices.begin(), source.vertices.end());
  target.indices.reserve(target.indices.size() + source.indices.size());
  for (const std::uint32_t index : source.indices) {
    target.indices.push_back(base + index);
  }
}

aster::CpuMesh makeTunnelEndCap(const aster::CaveTunnelProfile &profile) {
  const int radial_segments = std::max(profile.radial_segments, 8);
  const int segment = std::max(profile.length_segments, 2);
  constexpr float t = 1.0f;
  const aster::Vec3 floor_center = cubicPoint(profile, t);
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(profile, t, side, up);
  const aster::Vec3 tangent = cubicTangent(profile, t);
  const float height_scale = tunnelHeightScale(profile, t);
  const aster::Vec3 ring_center = floor_center + up * (profile.wall_height * height_scale * 0.55f);

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(radial_segments + 2));
  mesh.indices.reserve(static_cast<std::size_t>(radial_segments * 3));
  mesh.vertices.push_back(
      {ring_center - tangent * 0.10f, aster::normalize(tangent * -1.0f), {0.5f, 0.5f}});

  for (int radial = 0; radial <= radial_segments; ++radial) {
    const float u = static_cast<float>(radial) / static_cast<float>(radial_segments);
    const float theta = u * kPi * 2.0f;
    const float horizontal_radius = profile.half_width * tunnelWidthScale(profile, t) *
                                    ringRadiusNoise(profile, floor_center, segment, radial);
    const float sin_theta = std::sin(theta);
    const float vertical_radius = sin_theta >= 0.0f ? profile.wall_height * height_scale * 0.66f
                                                    : profile.wall_height * height_scale * 0.55f;
    const aster::Vec3 radial_vector =
        side * (std::cos(theta) * horizontal_radius) + up * (sin_theta * vertical_radius);
    mesh.vertices.push_back({ring_center + radial_vector,
                             aster::normalize(tangent * -1.0f + radial_vector * -0.08f),
                             {0.5f + std::cos(theta) * 0.5f, 0.5f + sin_theta * 0.5f}});
  }

  for (int radial = 0; radial < radial_segments; ++radial) {
    const std::uint32_t a = static_cast<std::uint32_t>(1 + radial);
    const std::uint32_t b = static_cast<std::uint32_t>(1 + radial + 1);
    mesh.indices.insert(mesh.indices.end(), {0u, b, a});
  }

  return mesh;
}

aster::CpuMesh makeCaveFloorMesh(const aster::CaveTunnelProfile &profile) {
  const int segments = std::max(profile.length_segments, 2);
  constexpr int columns = 9;
  constexpr float offsets[columns] = {-1.0f, -0.74f, -0.48f, -0.22f, 0.0f,
                                      0.22f, 0.48f,  0.74f,  1.0f};
  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * columns));
  mesh.indices.reserve(static_cast<std::size_t>(segments * (columns - 1) * 6));

  for (int segment = 0; segment <= segments; ++segment) {
    const float t = static_cast<float>(segment) / static_cast<float>(segments);
    const aster::Vec3 center = cubicPoint(profile, t);
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(profile, t, side, up);
    const float width_noise =
        fbm3(center * 0.21f + aster::Vec3{2.0f, 7.0f, -3.0f}, profile.seed + 41u, 4);
    const float width =
        profile.floor_width * tunnelWidthScale(profile, t) * (0.92f + width_noise * 0.16f);
    for (int column = 0; column < columns; ++column) {
      const float offset = offsets[column];
      const float crown = (1.0f - std::abs(offset)) * profile.floor_crown;
      const float edge_raise =
          smoothstep(0.66f, 1.0f, std::abs(offset)) * std::max(profile.floor_edge_raise, 0.0f);
      const float gravel =
          (fbm3(center * 0.75f + side * offset * 2.3f, profile.seed + 73u, 3) - 0.5f) * 0.018f;
      mesh.vertices.push_back(
          {center + side * (offset * width * 0.5f) + up * (crown + edge_raise + gravel),
           {},
           {(offset + 1.0f) * 0.5f, t * 5.0f}});
    }
  }

  for (int segment = 0; segment < segments; ++segment) {
    for (int column = 0; column + 1 < columns; ++column) {
      const std::uint32_t a = static_cast<std::uint32_t>(segment * columns + column);
      const std::uint32_t b = static_cast<std::uint32_t>((segment + 1) * columns + column);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      appendIndexedQuad(mesh, a, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalMesh(const aster::CavePortalProfile &profile) {
  const int segments = std::max(profile.arch_segments, 6);
  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * 4));
  mesh.indices.reserve(static_cast<std::size_t>(segments * 24 + 12));

  const float z = -profile.depth * 0.24f;
  const float back_z = -profile.depth * std::max(profile.inner_lining_depth, 0.20f);

  for (int i = 0; i <= segments; ++i) {
    const float fill = static_cast<float>(i) / static_cast<float>(segments);
    const float angle = kPi - fill * kPi;
    const float inner_breakup = 1.0f + std::sin(fill * kPi * 5.0f + 0.73f) * 0.035f +
                                std::sin(fill * kPi * 9.0f + 1.91f) * 0.020f;
    const float outer_breakup = 1.0f + std::sin(fill * kPi * 4.0f + 1.31f) * 0.060f +
                                std::sin(fill * kPi * 7.0f + 2.47f) * 0.035f;
    const float inner_x = std::cos(angle) * profile.inner_half_width * inner_breakup;
    const float inner_y = profile.ground_lip + std::sin(angle) * profile.inner_height *
                                                   (0.98f + std::sin(fill * kPi * 3.0f) * 0.025f);
    const float lining_breakup =
        1.0f + std::sin(fill * kPi * 6.0f + 0.37f) * std::max(profile.lining_breakup, 0.0f);
    const float outer_x = std::cos(angle) * profile.outer_half_width * outer_breakup;
    const float outer_y =
        profile.ground_lip + std::sin(angle) * profile.outer_height *
                                 (0.95f + std::sin(fill * kPi * 4.7f + 0.41f) * 0.040f);
    const float outer_lining_breakup = 1.0f + (lining_breakup - 1.0f) * 0.35f;
    mesh.vertices.push_back(
        {{outer_x, outer_y, z}, {}, {fill, outer_y / std::max(profile.outer_height, 0.001f)}});
    mesh.vertices.push_back({{inner_x, inner_y, z + profile.depth * 0.075f},
                             {},
                             {fill, inner_y / std::max(profile.inner_height, 0.001f)}});
    mesh.vertices.push_back(
        {{inner_x * lining_breakup, inner_y * (0.98f + (lining_breakup - 1.0f) * 0.35f), back_z},
         {},
         {fill, 1.0f + inner_y / std::max(profile.inner_height, 0.001f)}});
    mesh.vertices.push_back({{outer_x * outer_lining_breakup,
                              outer_y * (0.99f + (outer_lining_breakup - 1.0f) * 0.20f), back_z},
                             {},
                             {fill, 1.0f + outer_y / std::max(profile.outer_height, 0.001f)}});
  }

  for (int i = 0; i < segments; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(segments);
    const float angle = kPi - fill * kPi;
    const float sin_angle = std::sin(angle);
    const float cos_angle = std::cos(angle);
    const aster::Vec3 outer_normal =
        aster::normalize({cos_angle / std::max(profile.outer_half_width, 0.001f),
                          sin_angle / std::max(profile.outer_height, 0.001f), 0.0f});
    const aster::Vec3 inner_normal =
        aster::normalize({-cos_angle / std::max(profile.inner_half_width, 0.001f),
                          -sin_angle / std::max(profile.inner_height, 0.001f), 0.0f});
    const std::uint32_t outer_a = static_cast<std::uint32_t>(i * 4);
    const std::uint32_t inner_a = outer_a + 1u;
    const std::uint32_t back_a = outer_a + 2u;
    const std::uint32_t outer_back_a = outer_a + 3u;
    const std::uint32_t outer_b = static_cast<std::uint32_t>((i + 1) * 4);
    const std::uint32_t inner_b = outer_b + 1u;
    const std::uint32_t back_b = outer_b + 2u;
    const std::uint32_t outer_back_b = outer_b + 3u;
    appendOrientedQuad(mesh, outer_a, outer_b, inner_b, inner_a, {0.0f, 0.0f, 1.0f});
    appendOrientedQuad(mesh, inner_a, inner_b, back_b, back_a, inner_normal);
    appendOrientedQuad(mesh, outer_b, outer_a, outer_back_a, outer_back_b, outer_normal);
    appendOrientedQuad(mesh, outer_back_a, outer_back_b, back_b, back_a, {0.0f, 0.0f, -1.0f});
  }

  appendOrientedQuad(mesh, 0u, 1u, 2u, 3u, {-1.0f, 0.0f, 0.0f});
  const std::uint32_t end_outer = static_cast<std::uint32_t>(segments * 4);
  appendOrientedQuad(mesh, end_outer, end_outer + 3u, end_outer + 2u, end_outer + 1u,
                     {1.0f, 0.0f, 0.0f});
  finalizeNormals(mesh);
  return mesh;
}

aster::Vec3 portalArchBackPoint(const aster::CaveTunnelProfile &tunnel,
                                const aster::CavePortalProfile &portal, const float fill) {
  const float angle = kPi - fill * kPi;
  const float inner_breakup = 1.0f + std::sin(fill * kPi * 5.0f + 0.73f) * 0.035f +
                              std::sin(fill * kPi * 9.0f + 1.91f) * 0.020f;
  const float lining_breakup =
      1.0f + std::sin(fill * kPi * 6.0f + 0.37f) * std::max(portal.lining_breakup, 0.0f);
  const float inner_x = std::cos(angle) * portal.inner_half_width * inner_breakup * lining_breakup;
  const float inner_y = (portal.ground_lip + std::sin(angle) * portal.inner_height *
                                                 (0.98f + std::sin(fill * kPi * 3.0f) * 0.025f)) *
                        (0.98f + (lining_breakup - 1.0f) * 0.35f);
  const float back_depth = portal.depth * std::max(portal.inner_lining_depth, 0.20f);

  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(tunnel, 0.0f, side, up);
  const aster::Vec3 tangent = cubicTangent(tunnel, 0.0f);
  return tunnel.start + side * inner_x + up * inner_y + tangent * back_depth;
}

aster::Vec3 tunnelArchPoint(const aster::CaveTunnelProfile &profile, const float t,
                            const float fill) {
  const int segment =
      std::clamp(static_cast<int>(std::round(t * static_cast<float>(profile.length_segments))), 0,
                 std::max(profile.length_segments, 2));
  const aster::Vec3 floor_center = cubicPoint(profile, t);
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(profile, t, side, up);
  const float height_scale = tunnelHeightScale(profile, t);
  const aster::Vec3 ring_center = floor_center + up * (profile.wall_height * height_scale * 0.55f);
  const float angle = kPi - fill * kPi;
  const float horizontal_radius =
      profile.half_width * tunnelWidthScale(profile, t) *
      ringRadiusNoise(profile, floor_center, segment, static_cast<int>(fill * 1000.0f));
  const float sin_angle = std::sin(angle);
  const float vertical_radius = sin_angle >= 0.0f ? profile.wall_height * height_scale * 0.66f
                                                  : profile.wall_height * height_scale * 0.55f;
  return ring_center + side * (std::cos(angle) * horizontal_radius) +
         up * (sin_angle * vertical_radius);
}

aster::Vec3 portalFloorBackPoint(const aster::CaveTunnelProfile &tunnel,
                                 const aster::CavePortalProfile &portal, const float lateral) {
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(tunnel, 0.0f, side, up);
  const aster::Vec3 tangent = cubicTangent(tunnel, 0.0f);
  const float back_depth = portal.depth * std::max(portal.inner_lining_depth, 0.20f);
  const float edge = std::abs(lateral);
  const float crown = (1.0f - edge) * 0.018f;
  const float width = portal.inner_half_width * (1.02f - edge * 0.035f);
  return tunnel.start + side * (lateral * width) + up * (portal.ground_lip + crown) +
         tangent * back_depth;
}

aster::Vec3 portalFloorFrontPoint(const aster::CaveTunnelProfile &tunnel,
                                  const aster::CavePortalProfile &portal, const float lateral) {
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(tunnel, 0.0f, side, up);
  const aster::Vec3 tangent = cubicTangent(tunnel, 0.0f);
  const aster::CaveTerrainPortalCut terrain_cut =
      aster::makeCaveTerrainPortalCut(tunnel, portal, 0.0f);
  const float edge = std::abs(lateral);
  const float crown = (1.0f - edge) * 0.012f;
  const float width = portal.outer_half_width * (1.0f - edge * 0.025f);
  return tunnel.start + side * (lateral * width) + up * (portal.ground_lip + crown) -
         tangent * terrain_cut.radius_forward_negative;
}

aster::Vec3 tunnelFloorPoint(const aster::CaveTunnelProfile &profile, const float t,
                             const float lateral) {
  const aster::Vec3 center = cubicPoint(profile, t);
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(profile, t, side, up);
  const float edge = std::abs(lateral);
  const float width = profile.floor_width * tunnelWidthScale(profile, t) * 0.5f;
  const float crown = (1.0f - edge) * profile.floor_crown;
  const float edge_raise = smoothstep(0.66f, 1.0f, edge) * std::max(profile.floor_edge_raise, 0.0f);
  return center + side * (lateral * width) + up * (crown + edge_raise + 0.010f);
}

aster::CpuMesh makePortalThroatMesh(const aster::CaveTunnelProfile &tunnel,
                                    const aster::CavePortalProfile &portal) {
  const int segments = std::max({portal.arch_segments, tunnel.radial_segments, 8});
  const float first_t =
      std::clamp(3.0f / static_cast<float>(std::max(tunnel.length_segments, 2)), 0.025f, 0.090f);
  const float second_t = std::clamp(8.0f / static_cast<float>(std::max(tunnel.length_segments, 2)),
                                    first_t + 0.025f, 0.155f);

  aster::CpuMesh mesh;
  constexpr int floor_columns = 9;
  constexpr float floor_offsets[floor_columns] = {-1.0f, -0.76f, -0.52f, -0.26f, 0.0f,
                                                  0.26f, 0.52f,  0.76f,  1.0f};
  constexpr int floor_rows = 4;
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * 3 + floor_columns * floor_rows));
  mesh.indices.reserve(
      static_cast<std::size_t>(segments * 12 + (floor_rows - 1) * (floor_columns - 1) * 6));

  for (int i = 0; i <= segments; ++i) {
    const float fill = static_cast<float>(i) / static_cast<float>(segments);
    const aster::Vec3 portal_point = portalArchBackPoint(tunnel, portal, fill);
    const aster::Vec3 first_point = tunnelArchPoint(tunnel, first_t, fill);
    const aster::Vec3 second_point = tunnelArchPoint(tunnel, second_t, fill);
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(tunnel, first_t, side, up);
    const aster::Vec3 center =
        cubicPoint(tunnel, first_t) +
        up * (tunnel.wall_height * tunnelHeightScale(tunnel, first_t) * 0.55f);
    const aster::Vec3 inward = aster::normalize(center - first_point);
    const aster::Vec3 normal =
        aster::length(inward) > 0.0001f ? inward : aster::Vec3{0.0f, 1.0f, 0.0f};
    mesh.vertices.push_back({portal_point, normal, {fill, 0.0f}});
    mesh.vertices.push_back({first_point, normal, {fill, 0.55f}});
    mesh.vertices.push_back({second_point, normal, {fill, 1.0f}});
  }

  for (int i = 0; i < segments; ++i) {
    const std::uint32_t a = static_cast<std::uint32_t>(i * 3);
    const std::uint32_t b = static_cast<std::uint32_t>((i + 1) * 3);
    appendOrientedQuad(mesh, a, b, b + 1u, a + 1u,
                       aster::normalize(mesh.vertices[a].normal + mesh.vertices[b].normal));
    appendOrientedQuad(
        mesh, a + 1u, b + 1u, b + 2u, a + 2u,
        aster::normalize(mesh.vertices[a + 1u].normal + mesh.vertices[b + 1u].normal));
  }

  const std::uint32_t floor_base = static_cast<std::uint32_t>(mesh.vertices.size());
  const float floor_t[floor_rows] = {0.0f, 0.0f, first_t, second_t};
  for (int row = 0; row < floor_rows; ++row) {
    const float t = floor_t[row];
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(tunnel, t, side, up);
    for (int column = 0; column < floor_columns; ++column) {
      const float lateral = floor_offsets[column];
      const aster::Vec3 position = row == 0
                                       ? portalFloorFrontPoint(tunnel, portal, lateral)
                                       : (row == 1 ? portalFloorBackPoint(tunnel, portal, lateral)
                                                   : tunnelFloorPoint(tunnel, t, lateral));
      mesh.vertices.push_back({position, up, {(lateral + 1.0f) * 0.5f, t * 5.0f}});
    }
  }

  for (int row = 0; row + 1 < floor_rows; ++row) {
    const float t = floor_t[row];
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(tunnel, t, side, up);
    for (int column = 0; column + 1 < floor_columns; ++column) {
      const std::uint32_t a = floor_base + static_cast<std::uint32_t>(row * floor_columns + column);
      const std::uint32_t b =
          floor_base + static_cast<std::uint32_t>((row + 1) * floor_columns + column);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      appendOrientedQuad(mesh, a, b, c, d, up);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalSealMesh(const aster::CaveTunnelProfile &tunnel,
                                  const aster::CavePortalProfile &portal) {
  const aster::CaveTerrainPortalCut cut = aster::makeCaveTerrainPortalCut(tunnel, portal, 0.035f);
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(tunnel, 0.0f, side, up);
  const aster::Vec3 forward = cubicTangent(tunnel, 0.0f);
  const float front = std::max(cut.radius_forward_negative, 0.05f);
  const float back = std::max(cut.radius_forward_positive, 0.05f);
  const float side_negative = std::max(cut.radius_side_negative, portal.outer_half_width);
  const float side_positive = std::max(cut.radius_side_positive, portal.outer_half_width);
  const float floor_y = portal.ground_lip - 0.030f;

  constexpr int rows = 8;
  constexpr int columns = 12;
  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((rows + 1) * (columns + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(rows * columns * 6));

  const auto sideExtentAt = [&](const float f) {
    const float forward_extent = f < 0.0f ? front : back;
    const float normalized_forward = std::abs(f) / std::max(forward_extent, 0.001f);
    const float ellipse_width =
        std::sqrt(std::max(1.0f - normalized_forward * normalized_forward, 0.0f));
    return std::pair<float, float>{side_negative * ellipse_width, side_positive * ellipse_width};
  };
  const auto pointAt = [&](const float f, const float lateral, const float y) {
    const float max_side = std::max(std::max(side_negative, side_positive), 0.001f);
    const float crown = (1.0f - std::min(std::abs(lateral) / max_side, 1.0f)) * 0.018f;
    return tunnel.start + forward * f + side * lateral + up * (y + crown);
  };

  for (int row = 0; row <= rows; ++row) {
    const float row_fill = static_cast<float>(row) / static_cast<float>(rows);
    const float f = -front + (front + back) * row_fill;
    const auto [left_extent, right_extent] = sideExtentAt(f);
    for (int column = 0; column <= columns; ++column) {
      const float column_fill = static_cast<float>(column) / static_cast<float>(columns);
      const float lateral = -left_extent + (left_extent + right_extent) * column_fill;
      mesh.vertices.push_back({pointAt(f, lateral, floor_y), up, {column_fill, row_fill}});
    }
  }

  const auto topIndex = [&](const int row, const int column) {
    return static_cast<std::uint32_t>(row * (columns + 1) + column);
  };
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      appendOrientedQuad(mesh, topIndex(row, column), topIndex(row + 1, column),
                         topIndex(row + 1, column + 1), topIndex(row, column + 1), up);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalBlendMesh(const aster::CavePortalProfile &profile) {
  const int arch_segments = std::max(profile.arch_segments, 8);
  constexpr int rings = 7;
  const int columns = arch_segments + 1;

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((rings + 1) * columns));
  mesh.indices.reserve(static_cast<std::size_t>(rings * arch_segments * 6));

  for (int ring = 0; ring <= rings; ++ring) {
    const float ring_fill = static_cast<float>(ring) / static_cast<float>(rings);
    const float soften = ring_fill * ring_fill * (3.0f - 2.0f * ring_fill);
    for (int column = 0; column <= arch_segments; ++column) {
      const float fill = static_cast<float>(column) / static_cast<float>(arch_segments);
      const float angle = kPi - fill * kPi;
      const float shoulder = 0.5f + 0.5f * std::sin(fill * kPi * 2.0f + 0.35f) +
                             (fbm3({fill * 7.0f, ring_fill * 3.0f, 1.0f}, 919u, 3) - 0.5f) * 0.20f;
      const float x_radius =
          profile.outer_half_width * (1.0f + soften * (0.24f + shoulder * 0.08f));
      const float y_radius = profile.outer_height * (1.0f + soften * (0.18f + shoulder * 0.05f));
      const float x = std::cos(angle) * x_radius;
      const float y = profile.ground_lip + std::sin(angle) * y_radius +
                      smoothstep(0.0f, 1.0f, ring_fill) * 0.18f;
      const float z = -profile.depth * (0.22f + soften * 0.34f);
      const float breakup = (fbm3({x * 0.82f, y * 0.64f, z * 0.78f}, 1237u, 4) - 0.5f) *
                            std::max(profile.lining_breakup, 0.0f) * (0.16f + soften * 0.42f);
      mesh.vertices.push_back(
          {{x * (1.0f + breakup), y * (1.0f + breakup * 0.20f), z}, {}, {fill, ring_fill}});
    }
  }

  for (int ring = 0; ring < rings; ++ring) {
    for (int column = 0; column < arch_segments; ++column) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * columns + column);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * columns + column);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      appendIndexedQuad(mesh, a, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalShellMesh(const aster::CavePortalProfile &profile) {
  const int arch_segments = std::max(profile.arch_segments, 12);
  constexpr int depth_segments = 7;
  const int columns = arch_segments + 1;
  const float front_z = -profile.depth * 0.04f;
  const float back_z = -std::max(profile.inner_lining_depth * 1.28f, profile.depth * 1.58f);

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((depth_segments + 1) * columns));
  mesh.indices.reserve(static_cast<std::size_t>(depth_segments * arch_segments * 6));

  for (int depth = 0; depth <= depth_segments; ++depth) {
    const float depth_fill = static_cast<float>(depth) / static_cast<float>(depth_segments);
    const float ease = depth_fill * depth_fill * (3.0f - 2.0f * depth_fill);
    const float z = front_z + (back_z - front_z) * ease;
    for (int column = 0; column <= arch_segments; ++column) {
      const float fill = static_cast<float>(column) / static_cast<float>(arch_segments);
      const float angle = kPi - fill * kPi;
      const float crown = std::sin(angle);
      const float lateral = std::cos(angle);
      const float width = profile.inner_half_width * (1.02f + ease * 0.20f);
      const float height = profile.inner_height * (1.03f + ease * 0.10f);
      const float breakup = (fbm3({fill * 5.6f, depth_fill * 3.8f, 0.70f}, 2203u, 3) - 0.5f) *
                            std::max(profile.lining_breakup, 0.0f) * (0.22f + ease * 0.28f);
      const float x = lateral * width * (1.0f + breakup);
      const float y =
          profile.ground_lip + crown * height * (1.0f + breakup * 0.18f) + ease * 0.030f;
      mesh.vertices.push_back({{x, y, z}, {}, {fill, depth_fill}});
    }
  }

  for (int depth = 0; depth < depth_segments; ++depth) {
    for (int column = 0; column < arch_segments; ++column) {
      const std::uint32_t a = static_cast<std::uint32_t>(depth * columns + column);
      const std::uint32_t b = static_cast<std::uint32_t>((depth + 1) * columns + column);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      appendIndexedQuad(mesh, a, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalFormationMesh(const aster::CavePortalProfile &profile) {
  const int arch_segments = std::max(profile.arch_segments, 12);
  constexpr int radial_segments = 4;
  constexpr int depth_segments = 8;
  const int columns = arch_segments + 1;
  const int rings = radial_segments + 1;

  const float inner_width = std::max(profile.inner_half_width * 0.98f, 0.12f);
  const float inner_height = std::max(profile.inner_height * 0.98f, 0.12f);
  const float outer_width = std::max(profile.outer_half_width * 1.28f, inner_width + 0.36f);
  const float outer_height = std::max(profile.outer_height * 1.26f, inner_height + 0.48f);
  const float front_z = profile.depth * 0.14f;
  const float back_z = -std::max(profile.inner_lining_depth * 0.98f, profile.depth * 1.32f);

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((depth_segments + 1) * rings * columns));
  mesh.indices.reserve(
      static_cast<std::size_t>(depth_segments * radial_segments * arch_segments * 6));

  for (int depth = 0; depth <= depth_segments; ++depth) {
    const float depth_fill = static_cast<float>(depth) / static_cast<float>(depth_segments);
    const float depth_ease = depth_fill * depth_fill * (3.0f - 2.0f * depth_fill);
    const float z = front_z + (back_z - front_z) * depth_ease;
    for (int ring = 0; ring <= radial_segments; ++ring) {
      const float ring_fill = static_cast<float>(ring) / static_cast<float>(radial_segments);
      const float ring_ease = ring_fill * ring_fill * (3.0f - 2.0f * ring_fill);
      for (int column = 0; column <= arch_segments; ++column) {
        const float fill = static_cast<float>(column) / static_cast<float>(arch_segments);
        const float angle = kPi - fill * kPi;
        const float crown = std::max(std::sin(angle), 0.0f);
        const float lateral = std::cos(angle);
        const float shoulder = smoothstep(0.18f, 0.86f, crown);
        const float side_weight = 1.0f - shoulder;
        const float breakup =
            (fbm3({fill * 7.4f, ring_fill * 4.8f, depth_fill * 5.6f}, 2897u, 4) - 0.5f) *
            std::max(profile.lining_breakup, 0.0f);
        const float weather = 1.0f + breakup * (0.45f + ring_ease * 0.85f + depth_ease * 0.38f);
        const float width = (inner_width + (outer_width - inner_width) * ring_ease) *
                            (1.0f + side_weight * ring_ease * 0.10f) * weather;
        const float height = (inner_height + (outer_height - inner_height) * ring_ease) *
                             (1.0f + shoulder * ring_ease * 0.07f) * (1.0f + breakup * 0.22f);
        const float skirt = side_weight * ring_ease * (0.34f + depth_ease * 0.12f);
        const float x = lateral * width;
        const float y = profile.ground_lip + crown * height - skirt;
        aster::Vertex vertex;
        vertex.position = {x, y, z};
        vertex.uv = {fill, ring_fill * 0.65f + depth_fill * 0.35f};
        vertex.ambient_occlusion = 1.0f - ring_fill * 0.24f;
        mesh.vertices.push_back(vertex);
      }
    }
  }

  const auto vertex_index = [&](const int depth, const int ring, const int column) {
    return static_cast<std::uint32_t>((depth * rings + ring) * columns + column);
  };
  for (int depth = 0; depth < depth_segments; ++depth) {
    for (int ring = 0; ring < radial_segments; ++ring) {
      for (int column = 0; column < arch_segments; ++column) {
        const std::uint32_t a = vertex_index(depth, ring, column);
        const std::uint32_t b = vertex_index(depth + 1, ring, column);
        const std::uint32_t c = vertex_index(depth + 1, ring, column + 1);
        const std::uint32_t d = vertex_index(depth, ring, column + 1);
        appendIndexedQuad(mesh, a, b, c, d);
      }
    }
  }
  for (int depth = 0; depth < depth_segments; ++depth) {
    for (int column = 0; column < arch_segments; ++column) {
      const std::uint32_t a = vertex_index(depth, radial_segments, column);
      const std::uint32_t b = vertex_index(depth, radial_segments, column + 1);
      const std::uint32_t c = vertex_index(depth + 1, radial_segments, column + 1);
      const std::uint32_t d = vertex_index(depth + 1, radial_segments, column);
      appendIndexedQuad(mesh, a, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

aster::CpuMesh makePortalFloorMesh(const aster::CaveTunnelProfile &tunnel,
                                   const aster::CavePortalProfile &portal) {
  constexpr int rows = 12;
  constexpr int columns = 9;
  constexpr float offsets[columns] = {-1.0f, -0.76f, -0.50f, -0.24f, 0.0f,
                                      0.24f, 0.50f,  0.76f,  1.0f};
  const aster::Vec3 start = cubicPoint(tunnel, 0.0f);
  aster::Vec3 side{};
  aster::Vec3 up{};
  tunnelBasis(tunnel, 0.0f, side, up);
  aster::Vec3 horizontal_tangent{cubicTangent(tunnel, 0.0f).x, 0.0f, cubicTangent(tunnel, 0.0f).z};
  if (aster::length(horizontal_tangent) <= 0.0001f) {
    horizontal_tangent = {0.0f, 0.0f, -1.0f};
  }
  horizontal_tangent = aster::normalize(horizontal_tangent);

  const float outside_depth = std::max(portal.depth * 1.05f, 0.35f);
  const float inside_depth = std::max(std::max(portal.inner_lining_depth, portal.depth * 1.55f),
                                      tunnel.floor_width * 0.82f);
  const float max_inside_t = 0.095f;
  const float outside_half_width =
      std::max(portal.outer_half_width * 0.98f, tunnel.floor_width * 0.58f);

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((rows + 1) * columns));
  mesh.indices.reserve(static_cast<std::size_t>(rows * (columns - 1) * 6));

  for (int row = 0; row <= rows; ++row) {
    const float row_fill = static_cast<float>(row) / static_cast<float>(rows);
    const float along = -outside_depth + (outside_depth + inside_depth) * row_fill;
    const float inside_fill = aster::clamp(along / std::max(inside_depth, 0.001f), 0.0f, 1.0f);
    const float t = inside_fill * max_inside_t;
    const aster::Vec3 tunnel_center = cubicPoint(tunnel, t);
    const aster::Vec3 center = along < 0.0f ? start + horizontal_tangent * along : tunnel_center;
    aster::Vec3 row_side{};
    aster::Vec3 row_up{};
    tunnelBasis(tunnel, t, row_side, row_up);
    const float tunnel_half_width = tunnel.floor_width * tunnelWidthScale(tunnel, t) * 0.5f;
    const float outside_blend = smoothstep(-outside_depth, 0.0f, along);
    const float half_width =
        along < 0.0f ? outside_half_width + (tunnel_half_width - outside_half_width) * outside_blend
                     : tunnel_half_width;

    for (int column = 0; column < columns; ++column) {
      const float offset = offsets[column];
      const float crown = (1.0f - std::abs(offset)) * tunnel.floor_crown;
      const float edge_raise =
          smoothstep(0.66f, 1.0f, std::abs(offset)) * std::max(tunnel.floor_edge_raise, 0.0f);
      const float gravel =
          (fbm3(center * 0.68f + row_side * offset * 2.1f, tunnel.seed + 911u, 3) - 0.5f) * 0.012f;
      mesh.vertices.push_back({center + row_side * (offset * half_width) +
                                   row_up * (0.018f + crown + edge_raise + gravel),
                               {},
                               {(offset + 1.0f) * 0.5f, row_fill * 1.35f}});
    }
  }

  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column + 1 < columns; ++column) {
      const std::uint32_t a = static_cast<std::uint32_t>(row * columns + column);
      const std::uint32_t b = static_cast<std::uint32_t>((row + 1) * columns + column);
      const std::uint32_t c = b + 1u;
      const std::uint32_t d = a + 1u;
      appendIndexedQuad(mesh, a, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

std::vector<aster::CaveOreNodePlacement> makeOreNodes(const aster::CaveTunnelProfile &tunnel,
                                                      const aster::CaveOreVeinProfile &vein) {
  struct Candidate {
    float score = 0.0f;
    aster::CaveOreNodePlacement placement{};
  };

  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(std::max(vein.candidates, 0)));
  const int count = std::max(vein.candidates, 0);
  for (int i = 0; i < count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(std::max(count, 1));
    const float jitter =
        hashGrid(i, 5, 19, vein.seed) * 0.65f / static_cast<float>(std::max(count, 1));
    const float t_min = aster::clamp(std::max(tunnel.ore_start_t, 0.08f), 0.0f, 0.90f);
    const float t = aster::clamp(t_min + (fill + jitter) * (0.94f - t_min), t_min, 0.94f);
    const aster::Vec3 center = cubicPoint(tunnel, t);
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(tunnel, t, side, up);
    const float side_sign = hashGrid(i, 3, 7, vein.seed + 11u) < 0.5f ? -1.0f : 1.0f;
    const float vertical = -0.22f + hashGrid(i, 13, 23, vein.seed + 29u) * 0.72f;
    const float radial_length = std::max(std::sqrt(1.0f + vertical * vertical), 0.001f);
    const float side_component = side_sign / radial_length;
    const float up_component = vertical / radial_length;
    const float width_scale = tunnelWidthScale(tunnel, t);
    const float height_scale = tunnelHeightScale(tunnel, t);
    const float horizontal_radius =
        tunnel.half_width * width_scale * (0.98f + hashGrid(i, 17, 31, vein.seed + 37u) * 0.08f);
    const float vertical_radius = tunnel.wall_height * height_scale *
                                  (up_component >= 0.0f ? 0.66f : 0.55f);
    const aster::Vec3 ring_center = center + up * (tunnel.wall_height * height_scale * 0.55f);
    const aster::Vec3 surface_point = ring_center + side * (side_component * horizontal_radius) +
                                      up * (up_component * vertical_radius);
    aster::Vec3 inward = aster::normalize(side * (-side_component / std::max(horizontal_radius, 0.001f)) +
                                          up * (-up_component / std::max(vertical_radius, 0.001f)));
    if (aster::length(inward) <= 0.0001f) {
      inward = aster::normalize(ring_center - surface_point);
    }
    const aster::Vec3 field_point = surface_point;
    const float field_a =
        std::abs(fbm3(field_point * vein.field_frequency_a, vein.seed + 101u, 5) - 0.5f) * 2.0f;
    const float field_b =
        std::abs(fbm3(field_point * vein.field_frequency_b, vein.seed + 211u, 5) - 0.5f) * 2.0f;
    const float score = field_a / std::max(vein.intersection_threshold_a, 0.001f) +
                        field_b / std::max(vein.intersection_threshold_b, 0.001f);
    const float strength = 1.0f - aster::clamp(score * 0.30f, 0.0f, 0.78f);
    const float radius = 0.24f + strength * 0.16f;
    const aster::Vec3 position =
        surface_point + inward * (std::max(vein.wall_inset, 0.0f) + radius * 0.24f);
    candidates.push_back(
        {score,
         {position,
          inward,
          {radius * (1.28f + strength * 0.35f), radius * (0.88f + strength * 0.22f),
           radius * (0.70f + strength * 0.18f)},
          radius}});
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &lhs, const Candidate &rhs) { return lhs.score < rhs.score; });

  std::vector<aster::CaveOreNodePlacement> nodes;
  nodes.reserve(static_cast<std::size_t>(std::max(vein.max_nodes, 0)));
  for (const Candidate &candidate : candidates) {
    bool spaced = true;
    for (const aster::CaveOreNodePlacement &node : nodes) {
      if (aster::length(candidate.placement.position - node.position) <
          std::max(vein.min_spacing, 0.0f)) {
        spaced = false;
        break;
      }
    }
    if (!spaced) {
      continue;
    }
    nodes.push_back(candidate.placement);
    if (static_cast<int>(nodes.size()) >= std::max(vein.max_nodes, 0)) {
      break;
    }
  }
  return nodes;
}

std::vector<aster::CaveFeaturePlacement>
makeCaveFeatures(const aster::CaveTunnelProfile &tunnel, const aster::CaveFeatureProfile &profile) {
  struct Candidate {
    float score = 0.0f;
    aster::CaveFeaturePlacement placement{};
  };

  const int count = std::max(profile.candidates, 0);
  const float start_t = aster::clamp(std::min(profile.start_t, profile.end_t), 0.0f, 0.98f);
  const float end_t = aster::clamp(std::max(profile.start_t, profile.end_t), start_t, 0.99f);
  const float ceiling_fraction = aster::clamp(profile.ceiling_fraction, 0.0f, 1.0f);
  const float column_fraction = aster::clamp(profile.column_fraction, 0.0f, 1.0f);
  const float shelf_fraction = aster::clamp(profile.shelf_fraction, 0.0f, 1.0f);

  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(std::max(count, 1));
    const float jitter =
        (hashGrid(i, 29, 7, profile.seed) - 0.5f) * 0.72f / static_cast<float>(std::max(count, 1));
    const float t = aster::clamp(start_t + (fill + jitter) * (end_t - start_t), start_t, end_t);
    const aster::Vec3 center = cubicPoint(tunnel, t);
    aster::Vec3 side{};
    aster::Vec3 up{};
    tunnelBasis(tunnel, t, side, up);
    const aster::Vec3 tangent = cubicTangent(tunnel, t);
    const float side_sign = hashGrid(i, 5, 11, profile.seed + 17u) < 0.5f ? -1.0f : 1.0f;
    const float width = tunnel.half_width * tunnelWidthScale(tunnel, t);
    const float height = tunnel.wall_height * tunnelHeightScale(tunnel, t);
    const float lateral =
        side_sign * width * (0.18f + hashGrid(i, 13, 3, profile.seed + 31u) * 0.62f);
    const float mineral = hashGrid(i, 23, 19, profile.seed + 47u);
    const float variant = hashGrid(i, 37, 41, profile.seed + 59u);

    aster::CaveFeaturePlacement placement;
    placement.tangent = tangent;
    placement.t = t;
    placement.mineral = mineral;
    if (variant < ceiling_fraction) {
      const float length_scale = 0.52f + hashGrid(i, 43, 17, profile.seed + 71u) * 0.72f;
      placement.kind = aster::CaveFeatureKind::Stalactite;
      placement.normal = up * -1.0f;
      placement.position = center + side * lateral + up * (height * 1.12f);
      placement.scale = {0.12f + length_scale * 0.055f, 0.34f + length_scale * 0.52f,
                         0.12f + length_scale * 0.052f};
      placement.radius = placement.scale.x;
    } else if (variant < ceiling_fraction + column_fraction) {
      const float column_height = 0.72f + hashGrid(i, 31, 53, profile.seed + 83u) * 0.64f;
      placement.kind = aster::CaveFeatureKind::Column;
      placement.normal = up;
      placement.position = center + side * (lateral * 0.58f) + up * (height * 0.48f);
      placement.scale = {0.18f + column_height * 0.042f, height * (0.40f + column_height * 0.12f),
                         0.18f + column_height * 0.040f};
      placement.radius = placement.scale.x;
    } else if (variant < ceiling_fraction + column_fraction + shelf_fraction) {
      const float shelf_scale = 0.65f + hashGrid(i, 61, 2, profile.seed + 97u) * 0.55f;
      placement.kind = aster::CaveFeatureKind::WallShelf;
      placement.normal = aster::normalize(side * -side_sign + up * 0.16f);
      placement.position =
          center + side * (side_sign * (width - std::max(profile.wall_inset, 0.0f))) +
          up * (height * (0.30f + hashGrid(i, 71, 11, profile.seed + 101u) * 0.42f));
      placement.scale = {0.36f * shelf_scale, 0.10f + shelf_scale * 0.045f,
                         0.20f + shelf_scale * 0.18f};
      placement.radius = placement.scale.x;
    } else {
      const float length_scale = 0.42f + hashGrid(i, 83, 67, profile.seed + 113u) * 0.68f;
      placement.kind = aster::CaveFeatureKind::Stalagmite;
      placement.normal = up;
      placement.position = center + side * lateral + up * 0.06f;
      placement.scale = {0.14f + length_scale * 0.070f, 0.32f + length_scale * 0.58f,
                         0.14f + length_scale * 0.066f};
      placement.radius = placement.scale.x;
    }

    const float chamber = chamberWeight(tunnel, t);
    const float score =
        fbm3(placement.position * 0.44f + aster::Vec3{5.0f, 11.0f, -3.0f}, profile.seed + 127u, 4) +
        chamber * 0.34f + mineral * std::max(profile.mineral_fraction, 0.0f) * 0.22f;
    candidates.push_back({score, placement});
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &lhs, const Candidate &rhs) { return lhs.score > rhs.score; });

  std::vector<aster::CaveFeaturePlacement> features;
  features.reserve(static_cast<std::size_t>(std::max(profile.max_features, 0)));
  for (const Candidate &candidate : candidates) {
    bool spaced = true;
    for (const aster::CaveFeaturePlacement &feature : features) {
      if (aster::length(candidate.placement.position - feature.position) <
          std::max(profile.min_spacing, 0.0f)) {
        spaced = false;
        break;
      }
    }
    if (!spaced) {
      continue;
    }
    features.push_back(candidate.placement);
    if (static_cast<int>(features.size()) >= std::max(profile.max_features, 0)) {
      break;
    }
  }
  return features;
}

} // namespace

namespace aster {

Vec3 evaluateCaveTunnelCenter(const CaveTunnelProfile &profile, const float t) {
  return cubicPoint(profile, clamp(t, 0.0f, 1.0f));
}

Vec3 evaluateCaveTunnelTangent(const CaveTunnelProfile &profile, const float t) {
  return cubicTangent(profile, clamp(t, 0.0f, 1.0f));
}

float estimateCaveTunnelLength(const CaveTunnelProfile &profile, const float start_t,
                               const float end_t) {
  return tunnelPathLength(profile, start_t, end_t);
}

CaveTunnelFrame sampleCaveTunnelFrame(const CaveTunnelProfile &profile, const float t) {
  const float clamped_t = clamp(t, 0.0f, 1.0f);
  CaveTunnelFrame frame;
  frame.t = clamped_t;
  frame.floor_center = cubicPoint(profile, clamped_t);
  frame.tangent = cubicTangent(profile, clamped_t);
  tunnelBasis(profile, clamped_t, frame.side, frame.up);
  frame.half_width = profile.half_width * tunnelWidthScale(profile, clamped_t);
  frame.height = profile.wall_height * tunnelHeightScale(profile, clamped_t);
  frame.floor_half_width = profile.floor_width * tunnelWidthScale(profile, clamped_t) * 0.5f;
  return frame;
}

CaveTunnelFrame sampleCaveTunnelFrameAtDistance(const CaveTunnelProfile &profile,
                                                const float distance) {
  const float target_distance = std::max(distance, 0.0f);
  if (target_distance <= 0.0f) {
    return sampleCaveTunnelFrame(profile, 0.0f);
  }

  const int samples = std::max(profile.length_segments, 8);
  Vec3 previous = cubicPoint(profile, 0.0f);
  float accumulated = 0.0f;
  for (int i = 1; i <= samples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(samples);
    const Vec3 next = cubicPoint(profile, t);
    const float segment_length = length(next - previous);
    if (accumulated + segment_length >= target_distance) {
      const float segment_t =
          segment_length > 0.0001f ? (target_distance - accumulated) / segment_length : 0.0f;
      return sampleCaveTunnelFrame(
          profile, (static_cast<float>(i - 1) + clamp(segment_t, 0.0f, 1.0f)) /
                       static_cast<float>(samples));
    }
    accumulated += segment_length;
    previous = next;
  }

  return sampleCaveTunnelFrame(profile, 1.0f);
}

CaveInteriorSample sampleCaveInteriorVolume(const CaveTunnelProfile &profile, const Vec3 position) {
  const TunnelFrameSample frame = closestTunnelFrame(profile, position);
  const Vec3 offset = position - frame.floor_center;
  const float lateral = std::abs(dot(offset, frame.side));
  const float vertical = dot(offset, frame.up);
  const float interior_start = clamp(profile.visible_wall_start_t + 0.035f, 0.0f, 0.96f);
  const float entry_weight = smoothstep(interior_start, interior_start + 0.14f, frame.t);
  const float lateral_weight =
      1.0f - smoothstep(frame.half_width * 0.82f, frame.half_width * 1.22f, lateral);
  const float floor_weight = smoothstep(-0.24f, 0.12f, vertical);
  const float roof_weight = 1.0f - smoothstep(frame.height * 1.28f, frame.height * 1.62f, vertical);
  const float interior =
      clamp(entry_weight * lateral_weight * floor_weight * roof_weight, 0.0f, 1.0f);
  const float entrance_light =
      clamp((1.0f - smoothstep(interior_start, interior_start + 0.38f, frame.t)) * lateral_weight *
                floor_weight * roof_weight,
            0.0f, 1.0f);
  const float depth =
      clamp((frame.t - interior_start) / std::max(1.0f - interior_start, 0.001f), 0.0f, 1.0f);
  return {interior,
          entrance_light,
          frame.t,
          lateral,
          vertical,
          depth,
          chamberWeight(profile, frame.t),
          frame.half_width,
          frame.height};
}

CaveTraversalConstraint constrainCaveTraversalPosition(const CaveTunnelProfile &profile,
                                                       const Vec3 position,
                                                       const float actor_radius) {
  const TunnelFrameSample frame = closestTunnelFrame(profile, position);
  const Vec3 offset = position - frame.floor_center;
  const float signed_lateral = dot(offset, frame.side);
  const float vertical = dot(offset, frame.up);
  const float actor = std::max(actor_radius, 0.0f);
  const float collision_start = clamp(profile.collision_start_t, 0.0f, 0.96f);
  const float end_tolerance = std::max(actor * 0.85f, 0.20f);
  const float side_limit = std::max(frame.floor_half_width - actor * 0.34f, actor * 1.15f);
  const float lateral_margin =
      std::max(frame.half_width, actor + std::max(profile.floor_edge_raise, 0.0f) + 0.38f);
  const bool near_traversal_volume = frame.t >= collision_start - 0.035f && vertical >= -0.62f &&
                                     vertical <= frame.height * 1.38f &&
                                     std::abs(signed_lateral) <= side_limit + lateral_margin;

  Vec3 corrected = position;
  bool constrained = false;
  if (near_traversal_volume && std::abs(signed_lateral) > side_limit) {
    corrected =
        corrected - frame.side * (signed_lateral - std::copysign(side_limit, signed_lateral));
    constrained = true;
  }

  const Vec3 end_center = cubicPoint(profile, 1.0f);
  const Vec3 end_tangent = cubicTangent(profile, 1.0f);
  const float beyond_end = dot(corrected - end_center, end_tangent);
  const bool near_end_volume = frame.t >= 0.965f && vertical >= -0.62f &&
                               vertical <= frame.height * 1.42f &&
                               std::abs(signed_lateral) <= side_limit + lateral_margin;
  if (profile.end_constraint_enabled && near_end_volume && beyond_end > -end_tolerance) {
    corrected = corrected - end_tangent * (beyond_end + end_tolerance);
    constrained = true;
  }

  if (!constrained) {
    return {};
  }
  return {.active = true,
          .corrected_position = corrected,
          .correction = corrected - position,
          .tunnel_t = frame.t,
          .side_limit = side_limit,
          .lateral = std::abs(signed_lateral)};
}

CaveViewConstraint constrainCaveViewSegment(const CaveTunnelProfile &profile, const Vec3 target,
                                            const Vec3 desired_camera,
                                            const CaveViewConstraintProbe probe) {
  const CaveInteriorSample target_sample = sampleCaveInteriorVolume(profile, target);
  if (target_sample.interior <= std::max(probe.interior_threshold, 0.0f)) {
    return {};
  }

  const Vec3 offset = desired_camera - target;
  const float desired_radius = length(offset);
  if (desired_radius <= std::max(probe.minimum_radius, 0.0f)) {
    return {};
  }

  const Vec3 direction = offset / desired_radius;
  const int sample_count = std::clamp(probe.samples, 3, 96);
  const float min_radius = std::max(probe.minimum_radius, 0.0f);
  const float backtrack_limit =
      target_sample.tunnel_t - std::max(probe.backtrack_tolerance_t, 0.0f);

  float allowed_radius = desired_radius;
  bool clipped = false;
  for (int i = 1; i <= sample_count; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(sample_count);
    const float radius = min_radius + (desired_radius - min_radius) * u;
    const Vec3 point = target + direction * radius;
    const CaveInteriorSample point_sample = sampleCaveInteriorVolume(profile, point);
    const bool inside_same_visibility_cell =
        point_sample.interior > std::max(probe.interior_threshold, 0.0f) &&
        point_sample.tunnel_t >= backtrack_limit;
    if (!inside_same_visibility_cell) {
      allowed_radius =
          min_radius + (desired_radius - min_radius) * (static_cast<float>(std::max(i - 1, 0)) /
                                                        static_cast<float>(sample_count));
      clipped = true;
      break;
    }
  }

  if (!clipped || allowed_radius >= desired_radius - 0.001f) {
    return {};
  }
  allowed_radius = std::max(allowed_radius, min_radius);
  return {
      .active = true, .radius = allowed_radius, .position = target + direction * allowed_radius};
}

std::vector<CaveWallFixturePlacement> placeCaveWallFixtures(const CaveTunnelProfile &profile,
                                                            const CaveWallFixtureProfile fixture) {
  const float start_t = clamp(std::min(fixture.start_t, fixture.end_t), 0.0f, 0.98f);
  const float end_t = clamp(std::max(fixture.start_t, fixture.end_t), start_t, 0.99f);
  const float interval_length = tunnelPathLength(profile, start_t, end_t);
  const float target_spacing = std::max(fixture.target_spacing, 0.25f);
  const int count = std::clamp(static_cast<int>(std::floor(interval_length / target_spacing)) + 1,
                               1, std::max(fixture.max_count, 1));
  const float side_sign = fixture.wall_side < 0.0f ? -1.0f : 1.0f;

  std::vector<CaveWallFixturePlacement> placements;
  placements.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float fill = count == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(count - 1);
    const float t = start_t + (end_t - start_t) * fill;
    const Vec3 center = cubicPoint(profile, t);
    Vec3 side{};
    Vec3 up{};
    tunnelBasis(profile, t, side, up);
    const Vec3 tangent = cubicTangent(profile, t);
    const float half_width = profile.half_width * tunnelWidthScale(profile, t);
    const Vec3 normal = normalize(side * -side_sign + up * std::min(fixture.normal_up_bias, 0.20f));
    const float inset_half_width =
        std::max(half_width - std::max(fixture.wall_inset, 0.0f), 0.1f);
    const Vec3 mount_position = center + side * (side_sign * inset_half_width) +
                                up * std::max(fixture.mount_height, 0.0f);
    const Vec3 lens_position = mount_position + normal * std::max(fixture.lens_offset, 0.0f);
    const Vec3 light_position =
        lens_position + normal * std::max(fixture.light_offset, fixture.lens_offset);
    placements.push_back({mount_position,
                          mount_position,
                          lens_position,
                          light_position,
                          {1.0f, 0.16f, 0.08f},
                          normal,
                          tangent,
                          up,
                          t});
  }
  return placements;
}

CaveTerrainCoverFit
fitCaveTunnelToTerrainCover(const CaveTunnelProfile &profile,
                            const std::function<CaveTerrainCoverSample(Vec2)> &height_sampler,
                            const CaveTerrainCoverProbe probe) {
  CaveTerrainCoverFit fit{.tunnel = profile};
  if (!height_sampler || probe.samples <= 0 || probe.required_consecutive_samples <= 0) {
    return fit;
  }

  const float min_t = clamp(std::min(probe.min_t, probe.max_t), 0.0f, 0.98f);
  const float max_t = clamp(std::max(probe.min_t, probe.max_t), min_t, 0.99f);
  int covered_run = 0;
  float run_start_t = min_t;
  for (int sample = 0; sample <= probe.samples; ++sample) {
    const float u = static_cast<float>(sample) / static_cast<float>(probe.samples);
    const float t = min_t + (max_t - min_t) * u;
    Vec3 side{};
    Vec3 up{};
    tunnelBasis(profile, t, side, up);
    const float height_scale = tunnelHeightScale(profile, t);
    const Vec3 roof = cubicPoint(profile, t) + up * (profile.wall_height * height_scale * 1.21f);
    const CaveTerrainCoverSample terrain = height_sampler({roof.x, roof.z});
    const bool covered = terrain.valid && terrain.height >= roof.y + probe.roof_clearance;
    if (covered) {
      if (covered_run == 0) {
        run_start_t = t;
      }
      ++covered_run;
      if (covered_run >= probe.required_consecutive_samples) {
        fit.cover_found = true;
        fit.first_covered_t = run_start_t;
        const float start_t = std::max(profile.visible_wall_start_t, run_start_t);
        fit.tunnel.visible_wall_start_t = start_t;
        fit.tunnel.collision_start_t = profile.collision_start_t;
        return fit;
      }
    } else {
      covered_run = 0;
    }
  }
  fit.first_covered_t = max_t;
  return fit;
}

CaveTerrainPortalCut makeCaveTerrainPortalCut(const CaveTunnelProfile &tunnel,
                                              const CavePortalProfile &portal,
                                              const float padding) {
  Vec3 tangent = evaluateCaveTunnelTangent(tunnel, 0.0f);
  Vec2 forward{tangent.x, tangent.z};
  if (length(forward) <= 0.0001f) {
    forward = {0.0f, -1.0f};
  }
  forward = normalize(forward);

  const Vec2 start{tunnel.start.x, tunnel.start.z};
  const float pad = std::max(padding, 0.0f);
  const float side_radius = std::max(portal.outer_half_width, tunnel.half_width) + pad;
  const float outside_depth = portal.depth * 0.75f + pad;
  const float inside_depth = std::max(portal.depth * 1.35f, tunnel.half_width * 1.20f) + pad;
  const Vec2 center = start + forward * ((inside_depth - outside_depth) * 0.5f);

  return {.center = center,
          .forward = forward,
          .radius = {side_radius, (outside_depth + inside_depth) * 0.5f},
          .radius_side_negative = side_radius,
          .radius_side_positive = side_radius,
          .radius_forward_negative = outside_depth,
          .radius_forward_positive = inside_depth};
}

CaveComplex buildCaveComplex(const CaveComplexSpec &spec) {
  if (spec.tunnel.length_segments < 2 || spec.tunnel.radial_segments < 8 ||
      spec.tunnel.half_width <= 0.0f || spec.tunnel.wall_height <= 0.0f ||
      spec.tunnel.floor_width <= 0.0f) {
    throw std::invalid_argument("Cave tunnel requires positive dimensions and enough segments.");
  }

  CaveComplex complex;
  complex.portal_mesh = makePortalMesh(spec.portal);
  complex.portal_blend_mesh = makePortalBlendMesh(spec.portal);
  complex.portal_shell_mesh = makePortalShellMesh(spec.portal);
  complex.portal_formation_mesh = makePortalFormationMesh(spec.portal);
  complex.portal_seal_mesh = makePortalSealMesh(spec.tunnel, spec.portal);
  complex.entrance_throat_mesh = makePortalThroatMesh(spec.tunnel, spec.portal);
  complex.portal_floor_mesh = makePortalFloorMesh(spec.tunnel, spec.portal);
  complex.floor_mesh = makeCaveFloorMesh(spec.tunnel);
  const int first_collision_segment =
      std::clamp(static_cast<int>(std::floor(clamp(spec.tunnel.collision_start_t, 0.0f, 0.95f) *
                                             static_cast<float>(spec.tunnel.length_segments))),
                 0, spec.tunnel.length_segments - 1);
  const int last_collision_segment =
      std::clamp(static_cast<int>(std::ceil(clamp(spec.tunnel.collision_end_t, 0.05f, 1.0f) *
                                            static_cast<float>(spec.tunnel.length_segments))),
                 first_collision_segment + 1, spec.tunnel.length_segments);
  complex.collision_mesh =
      makeTunnelChunk(spec.tunnel, first_collision_segment, last_collision_segment);
  if (spec.tunnel.end_constraint_enabled && last_collision_segment >= spec.tunnel.length_segments) {
    appendMesh(complex.collision_mesh, makeTunnelEndCap(spec.tunnel));
  }

  constexpr int chunk_segments = 14;
  const int first_visible_segment =
      std::clamp(static_cast<int>(std::floor(clamp(spec.tunnel.visible_wall_start_t, 0.0f, 0.95f) *
                                             static_cast<float>(spec.tunnel.length_segments))),
                 0, spec.tunnel.length_segments - 1);
  for (int start = first_visible_segment; start < spec.tunnel.length_segments;
       start += chunk_segments) {
    const int end = std::min(start + chunk_segments, spec.tunnel.length_segments);
    CpuMesh chunk = makeTunnelChunk(spec.tunnel, start, end);
    if (spec.tunnel.end_constraint_enabled && end == spec.tunnel.length_segments) {
      appendMesh(chunk, makeTunnelEndCap(spec.tunnel));
    }
    complex.tunnel_chunks.push_back(std::move(chunk));
  }

  complex.ore_nodes = makeOreNodes(spec.tunnel, spec.ore);
  complex.features = makeCaveFeatures(spec.tunnel, spec.features);

  const float chest_t =
      clamp(spec.tunnel.chest_t, std::max(spec.tunnel.visible_wall_start_t + 0.12f, 0.0f), 0.92f);
  Vec3 side{};
  Vec3 up{};
  tunnelBasis(spec.tunnel, chest_t, side, up);
  const Vec3 tangent = evaluateCaveTunnelTangent(spec.tunnel, chest_t);
  const float floor_half_width =
      spec.tunnel.floor_width * tunnelWidthScale(spec.tunnel, chest_t) * 0.5f;
  const float chest_side_offset =
      std::clamp(floor_half_width - 0.58f, floor_half_width * 0.52f, floor_half_width * 0.86f);
  complex.chest_position =
      evaluateCaveTunnelCenter(spec.tunnel, chest_t) + side * -chest_side_offset + up * 0.06f;
  complex.chest_yaw = std::atan2(-tangent.x, -tangent.z);
  return complex;
}

} // namespace aster
