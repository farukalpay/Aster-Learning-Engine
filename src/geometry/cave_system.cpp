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
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * 3));
  mesh.indices.reserve(static_cast<std::size_t>(segments * 12));

  const aster::Vec3 normal{0.0f, 0.0f, 1.0f};
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
    mesh.vertices.push_back(
        {{outer_x, outer_y, z}, normal, {fill, outer_y / std::max(profile.outer_height, 0.001f)}});
    mesh.vertices.push_back({{inner_x, inner_y, z + profile.depth * 0.075f},
                             normal,
                             {fill, inner_y / std::max(profile.inner_height, 0.001f)}});
    mesh.vertices.push_back(
        {{inner_x * lining_breakup, inner_y * (0.98f + (lining_breakup - 1.0f) * 0.35f), back_z},
         {0.0f, 0.0f, -1.0f},
         {fill, 1.0f + inner_y / std::max(profile.inner_height, 0.001f)}});
  }

  for (int i = 0; i < segments; ++i) {
    const std::uint32_t outer_a = static_cast<std::uint32_t>(i * 3);
    const std::uint32_t inner_a = outer_a + 1u;
    const std::uint32_t back_a = outer_a + 2u;
    const std::uint32_t outer_b = static_cast<std::uint32_t>((i + 1) * 3);
    const std::uint32_t inner_b = outer_b + 1u;
    const std::uint32_t back_b = outer_b + 2u;
    mesh.indices.insert(mesh.indices.end(), {outer_a, outer_b, inner_a, outer_b, inner_b, inner_a});
    mesh.indices.insert(mesh.indices.end(), {inner_a, inner_b, back_a, inner_b, back_b, back_a});
  }

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
    const aster::Vec3 radial = aster::normalize(side * side_sign + up * vertical);
    const float width_scale = tunnelWidthScale(tunnel, t);
    const float height_scale = tunnelHeightScale(tunnel, t);
    const float wall_radius =
        tunnel.half_width * width_scale * (0.96f + hashGrid(i, 17, 31, vein.seed + 37u) * 0.18f);
    const aster::Vec3 position =
        center + up * (tunnel.wall_height * height_scale * (0.54f + vertical * 0.24f)) +
        radial * (wall_radius - std::max(vein.wall_inset, 0.0f));
    const aster::Vec3 field_point = position;
    const float field_a =
        std::abs(fbm3(field_point * vein.field_frequency_a, vein.seed + 101u, 5) - 0.5f) * 2.0f;
    const float field_b =
        std::abs(fbm3(field_point * vein.field_frequency_b, vein.seed + 211u, 5) - 0.5f) * 2.0f;
    const float score = field_a / std::max(vein.intersection_threshold_a, 0.001f) +
                        field_b / std::max(vein.intersection_threshold_b, 0.001f);
    const float strength = 1.0f - aster::clamp(score * 0.30f, 0.0f, 0.78f);
    const float radius = 0.24f + strength * 0.16f;
    candidates.push_back(
        {score,
         {position,
          radial * -1.0f,
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

} // namespace

namespace aster {

Vec3 evaluateCaveTunnelCenter(const CaveTunnelProfile &profile, const float t) {
  return cubicPoint(profile, clamp(t, 0.0f, 1.0f));
}

Vec3 evaluateCaveTunnelTangent(const CaveTunnelProfile &profile, const float t) {
  return cubicTangent(profile, clamp(t, 0.0f, 1.0f));
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
  if (near_end_volume && beyond_end > -end_tolerance) {
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
    const float half_width =
        profile.half_width * tunnelWidthScale(profile, t) - std::max(fixture.wall_inset, 0.0f);
    const Vec3 normal = normalize(side * -side_sign + up * -0.10f);
    const Vec3 position = center + side * (side_sign * std::max(half_width, 0.1f)) +
                          up * std::max(fixture.mount_height, 0.0f);
    placements.push_back({position, normal, tangent, up, t});
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
  complex.portal_floor_mesh = makePortalFloorMesh(spec.tunnel, spec.portal);
  complex.floor_mesh = makeCaveFloorMesh(spec.tunnel);
  const int first_collision_segment =
      std::clamp(static_cast<int>(std::floor(clamp(spec.tunnel.collision_start_t, 0.0f, 0.95f) *
                                             static_cast<float>(spec.tunnel.length_segments))),
                 0, spec.tunnel.length_segments - 1);
  complex.collision_mesh =
      makeTunnelChunk(spec.tunnel, first_collision_segment, spec.tunnel.length_segments);
  appendMesh(complex.collision_mesh, makeTunnelEndCap(spec.tunnel));

  constexpr int chunk_segments = 14;
  const int first_visible_segment =
      std::clamp(static_cast<int>(std::floor(clamp(spec.tunnel.visible_wall_start_t, 0.0f, 0.95f) *
                                             static_cast<float>(spec.tunnel.length_segments))),
                 0, spec.tunnel.length_segments - 1);
  for (int start = first_visible_segment; start < spec.tunnel.length_segments;
       start += chunk_segments) {
    const int end = std::min(start + chunk_segments, spec.tunnel.length_segments);
    CpuMesh chunk = makeTunnelChunk(spec.tunnel, start, end);
    if (end == spec.tunnel.length_segments) {
      appendMesh(chunk, makeTunnelEndCap(spec.tunnel));
    }
    complex.tunnel_chunks.push_back(std::move(chunk));
  }

  complex.ore_nodes = makeOreNodes(spec.tunnel, spec.ore);

  const float chest_t =
      clamp(spec.tunnel.chest_t, std::max(spec.tunnel.visible_wall_start_t + 0.12f, 0.0f), 0.92f);
  Vec3 side{};
  Vec3 up{};
  tunnelBasis(spec.tunnel, chest_t, side, up);
  const Vec3 tangent = evaluateCaveTunnelTangent(spec.tunnel, chest_t);
  complex.chest_position =
      evaluateCaveTunnelCenter(spec.tunnel, chest_t) + side * -0.38f + up * 0.06f;
  complex.chest_yaw = std::atan2(-tangent.x, -tangent.z);
  return complex;
}

} // namespace aster
