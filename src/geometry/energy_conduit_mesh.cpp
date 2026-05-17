// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/energy_conduit_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void accumulateNormal(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                      const std::uint32_t c) {
  const Vec3 normal =
      cross(mesh.vertices[b].position - mesh.vertices[a].position,
            mesh.vertices[c].position - mesh.vertices[a].position);
  mesh.vertices[a].normal = mesh.vertices[a].normal + normal;
  mesh.vertices[b].normal = mesh.vertices[b].normal + normal;
  mesh.vertices[c].normal = mesh.vertices[c].normal + normal;
}

void appendTriangle(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                    const std::uint32_t c) {
  mesh.indices.insert(mesh.indices.end(), {a, b, c});
  accumulateNormal(mesh, a, b, c);
}

void appendQuad(CpuMesh &mesh, const Vertex &a, const Vertex &b, const Vertex &c, const Vertex &d,
                const bool double_sided) {
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.insert(mesh.vertices.end(), {a, b, c, d});
  appendTriangle(mesh, base + 0u, base + 1u, base + 2u);
  appendTriangle(mesh, base + 0u, base + 2u, base + 3u);
  if (double_sided) {
    appendTriangle(mesh, base + 2u, base + 1u, base + 0u);
    appendTriangle(mesh, base + 3u, base + 2u, base + 0u);
  }
}

void finalizeNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = normalize(vertex.normal);
    if (length(vertex.normal) <= 0.0001f) {
      vertex.normal = {0.0f, 1.0f, 0.0f};
    }
    vertex.ambient_occlusion = std::clamp(vertex.ambient_occlusion, 0.0f, 1.0f);
  }
}

void appendMesh(CpuMesh &target, const CpuMesh &source) {
  const std::uint32_t base = static_cast<std::uint32_t>(target.vertices.size());
  target.vertices.insert(target.vertices.end(), source.vertices.begin(), source.vertices.end());
  target.indices.reserve(target.indices.size() + source.indices.size());
  for (const std::uint32_t index : source.indices) {
    target.indices.push_back(base + index);
  }
}

Vec3 safeSideFromTangent(const Vec3 tangent) {
  Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, tangent));
  if (length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  return side;
}

Vec3 samplePolyline(const std::vector<Vec3> &points, const float segment_position) {
  const int segment = std::clamp(static_cast<int>(std::floor(segment_position)), 0,
                                 static_cast<int>(points.size()) - 2);
  const float t = std::clamp(segment_position - static_cast<float>(segment), 0.0f, 1.0f);
  return points[static_cast<std::size_t>(segment)] +
         (points[static_cast<std::size_t>(segment) + 1u] -
          points[static_cast<std::size_t>(segment)]) *
             t;
}

} // namespace

CpuMesh makeEnergyConduitRibbonMesh(const EnergyConduitRibbonSpec &spec) {
  CpuMesh mesh;
  if (spec.points.size() < 2u) {
    return mesh;
  }

  const int subdivisions = std::max(spec.subdivisions_per_segment, 1);
  const int segment_count = static_cast<int>(spec.points.size()) - 1;
  const int sample_count = segment_count * subdivisions + 1;
  const float width = std::max(spec.width, 0.001f);
  std::vector<Vec3> centers;
  centers.reserve(static_cast<std::size_t>(sample_count));
  for (int i = 0; i < sample_count; ++i) {
    const float global_t = static_cast<float>(i) / static_cast<float>(sample_count - 1);
    const float segment_position = global_t * static_cast<float>(segment_count);
    Vec3 center = samplePolyline(spec.points, segment_position);
    center.y += std::sin(global_t * kPi) * std::max(spec.crest_height, 0.0f);
    centers.push_back(center);
  }

  for (int i = 0; i + 1 < sample_count; ++i) {
    const Vec3 from = centers[static_cast<std::size_t>(i)];
    const Vec3 to = centers[static_cast<std::size_t>(i + 1)];
    Vec3 tangent = normalize(to - from);
    if (length(tangent) <= 0.0001f) {
      continue;
    }
    const Vec3 side = safeSideFromTangent(tangent) * (width * 0.5f);
    const float u0 = static_cast<float>(i) / static_cast<float>(sample_count - 1);
    const float u1 = static_cast<float>(i + 1) / static_cast<float>(sample_count - 1);
    const float ao0 = 0.72f + 0.24f * std::sin(u0 * kPi);
    const float ao1 = 0.72f + 0.24f * std::sin(u1 * kPi);
    appendQuad(mesh,
               {.position = from - side, .uv = {u0, 0.0f}, .ambient_occlusion = ao0},
               {.position = from + side, .uv = {u0, 1.0f}, .ambient_occlusion = ao0},
               {.position = to + side, .uv = {u1, 1.0f}, .ambient_occlusion = ao1},
               {.position = to - side, .uv = {u1, 0.0f}, .ambient_occlusion = ao1},
               spec.double_sided);
  }

  finalizeNormals(mesh);
  return mesh;
}

CpuMesh makeEnergyConduitRibbonNetworkMesh(const std::vector<EnergyConduitRibbonSpec> &ribbons) {
  CpuMesh network;
  for (const EnergyConduitRibbonSpec &ribbon : ribbons) {
    appendMesh(network, makeEnergyConduitRibbonMesh(ribbon));
  }
  return network;
}

CpuMesh makeEnergyConduitRingMesh(EnergyConduitRingSpec spec) {
  CpuMesh mesh;
  spec.segments = std::max(spec.segments, 8);
  const float radius = std::max(spec.radius, 0.001f);
  const float half_width = std::max(spec.band_width, 0.001f) * 0.5f;
  for (int i = 0; i < spec.segments; ++i) {
    const float t0 = static_cast<float>(i) / static_cast<float>(spec.segments);
    const float t1 = static_cast<float>(i + 1) / static_cast<float>(spec.segments);
    const float a0 = t0 * kPi * 2.0f;
    const float a1 = t1 * kPi * 2.0f;
    const Vec3 radial0{std::cos(a0), 0.0f, std::sin(a0)};
    const Vec3 radial1{std::cos(a1), 0.0f, std::sin(a1)};
    appendQuad(mesh,
               {.position = radial0 * (radius - half_width), .uv = {t0, 0.0f}},
               {.position = radial0 * (radius + half_width), .uv = {t0, 1.0f}},
               {.position = radial1 * (radius + half_width), .uv = {t1, 1.0f}},
               {.position = radial1 * (radius - half_width), .uv = {t1, 0.0f}},
               spec.double_sided);
  }
  finalizeNormals(mesh);
  return mesh;
}

} // namespace aster
