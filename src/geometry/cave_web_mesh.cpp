// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/cave_web_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = kPi * 2.0f;

float hash01(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float signedHash(std::uint32_t value) {
  return hash01(value) * 2.0f - 1.0f;
}

Vec3 safeNormalizeOr(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > 0.0001f ? normalized : fallback;
}

struct WebBasis {
  Vec3 normal{};
  Vec3 side{};
  Vec3 up{};
};

WebBasis orthonormalWebBasis(const CaveWebMeshSpec &spec) {
  WebBasis basis;
  basis.normal = safeNormalizeOr(spec.normal, {0.0f, 0.0f, -1.0f});
  basis.side = spec.side - basis.normal * dot(spec.side, basis.normal);
  basis.side = safeNormalizeOr(basis.side, std::abs(basis.normal.y) > 0.86f
                                               ? Vec3{1.0f, 0.0f, 0.0f}
                                               : normalize(cross({0.0f, 1.0f, 0.0f}, basis.normal)));
  basis.up = spec.up - basis.normal * dot(spec.up, basis.normal) -
             basis.side * dot(spec.up, basis.side);
  basis.up = safeNormalizeOr(basis.up, normalize(cross(basis.normal, basis.side)));
  if (length(basis.up) <= 0.0001f) {
    basis.up = {0.0f, 1.0f, 0.0f};
  }
  basis.side = safeNormalizeOr(cross(basis.up, basis.normal), basis.side);
  return basis;
}

Vec3 ellipsePoint(const CaveWebMeshSpec &spec, const WebBasis &basis, const float angle,
                  const float radius_fraction, const float phase, const float strand_bias) {
  const float shaped_fraction = clamp(radius_fraction, 0.0f, 1.0f);
  const float rx = std::max(spec.radius_x, 0.001f);
  const float ry = std::max(spec.radius_y, 0.001f);
  const float contour = 1.0f + spec.irregularity *
                                   (std::sin(angle * 2.7f + phase) * 0.45f +
                                    std::sin(angle * 5.1f - phase * 0.7f) * 0.30f +
                                    strand_bias * 0.25f);
  const float clamped_contour = clamp(contour, 0.82f, 1.0f);
  const float x = std::cos(angle) * rx * shaped_fraction * clamped_contour;
  const float droop = -std::max(spec.sag, 0.0f) * shaped_fraction * shaped_fraction *
                      (0.18f + 0.82f * clamp(std::sin(angle) * 0.5f + 0.5f, 0.0f, 1.0f));
  const float normalized_x = clamp(std::abs(x) / rx, 0.0f, 1.0f);
  const float y_limit = ry * std::sqrt(std::max(0.0f, 1.0f - normalized_x * normalized_x)) * 0.985f;
  const float y =
      clamp(std::sin(angle) * ry * shaped_fraction * clamped_contour + droop, -y_limit, y_limit);
  const float depth =
      std::sin(angle * 3.0f + phase) * spec.strand_width * 0.65f * shaped_fraction;
  return spec.center + basis.side * x + basis.up * (y + droop) + basis.normal * depth;
}

void appendRibbonSegment(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 basis_normal,
                         const float width_a, const float width_b, const float u0,
                         const float u1, const bool reverse) {
  const Vec3 tangent = safeNormalizeOr(b - a, {1.0f, 0.0f, 0.0f});
  Vec3 side = normalize(cross(tangent, basis_normal));
  if (length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  const Vec3 normal = safeNormalizeOr(cross(side, tangent), basis_normal);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  Vertex v0{a - side * width_a, normal, {u0, 0.0f}};
  Vertex v1{a + side * width_a, normal, {u0, 1.0f}};
  Vertex v2{b + side * width_b, normal, {u1, 1.0f}};
  Vertex v3{b - side * width_b, normal, {u1, 0.0f}};
  v0.ambient_occlusion = 0.86f;
  v1.ambient_occlusion = 0.96f;
  v2.ambient_occlusion = 0.96f;
  v3.ambient_occlusion = 0.86f;
  mesh.vertices.insert(mesh.vertices.end(), {v0, v1, v2, v3});
  if (reverse) {
    mesh.indices.insert(mesh.indices.end(), {base, base + 2u, base + 1u, base, base + 3u,
                                             base + 2u});
  } else {
    mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u,
                                             base + 3u});
  }
}

void appendRibbon(CpuMesh &mesh, const std::vector<Vec3> &points, const Vec3 normal,
                  const float width, const bool double_sided) {
  if (points.size() < 2u || width <= 0.0f) {
    return;
  }
  float accumulated = 0.0f;
  for (std::size_t i = 1; i < points.size(); ++i) {
    const float segment_length = length(points[i] - points[i - 1]);
    const float u0 = accumulated;
    const float u1 = accumulated + segment_length;
    const float taper_a =
        0.72f + 0.28f * std::sin(static_cast<float>(i) * 1.91f + segment_length * 0.73f);
    const float taper_b =
        0.74f + 0.26f * std::sin(static_cast<float>(i) * 2.23f + segment_length * 0.51f);
    appendRibbonSegment(mesh, points[i - 1], points[i], normal, width * taper_a, width * taper_b,
                        u0, u1, false);
    if (double_sided) {
      appendRibbonSegment(mesh, points[i], points[i - 1], normal * -1.0f, width * taper_b,
                          width * taper_a, u1, u0, true);
    }
    accumulated = u1;
  }
}

} // namespace

CpuMesh makeCaveWebMesh(CaveWebMeshSpec spec) {
  spec.radius_x = std::max(spec.radius_x, 0.05f);
  spec.radius_y = std::max(spec.radius_y, 0.05f);
  spec.radial_strands = std::max(spec.radial_strands, 6);
  spec.ring_strands = std::max(spec.ring_strands, 2);
  spec.ring_segments = std::max(spec.ring_segments, spec.radial_strands * 4);
  spec.strand_width = std::max(spec.strand_width, 0.002f);
  const WebBasis basis = orthonormalWebBasis(spec);
  const float phase = hash01(spec.seed ^ 0x5197f2u) * kTau;

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.radial_strands + spec.ring_strands) *
                                                 spec.ring_segments * 8));
  mesh.indices.reserve(mesh.vertices.capacity() * 3u / 2u);

  std::vector<Vec3> strand_points;
  for (int strand = 0; strand < spec.radial_strands; ++strand) {
    const float t = static_cast<float>(strand) / static_cast<float>(spec.radial_strands);
    const float angle =
        t * kTau + signedHash(spec.seed + static_cast<std::uint32_t>(strand * 41)) * 0.040f;
    const float bias = signedHash(spec.seed + static_cast<std::uint32_t>(strand * 97 + 13));
    strand_points.clear();
    constexpr int kRadialSegments = 9;
    for (int step = 0; step <= kRadialSegments; ++step) {
      const float r = static_cast<float>(step) / static_cast<float>(kRadialSegments);
      const float bend =
          signedHash(spec.seed + static_cast<std::uint32_t>(strand * 157 + step * 29)) *
          spec.irregularity * (1.0f - std::abs(r * 2.0f - 1.0f));
      strand_points.push_back(
          ellipsePoint(spec, basis, angle + bend, r, phase + bias * 0.75f, bias));
    }
    appendRibbon(mesh, strand_points, basis.normal, spec.strand_width * spec.anchor_width_scale,
                 spec.double_sided);
  }

  for (int ring = 1; ring <= spec.ring_strands; ++ring) {
    const float ring_t = static_cast<float>(ring) / static_cast<float>(spec.ring_strands + 1);
    const float r = 0.14f + ring_t * 0.78f;
    const float ring_width = spec.strand_width * (0.72f + ring_t * 0.36f);
    strand_points.clear();
    strand_points.reserve(static_cast<std::size_t>(spec.ring_segments + 1));
    const float ring_phase =
        phase + signedHash(spec.seed + static_cast<std::uint32_t>(ring * 211)) * 0.90f;
    for (int segment = 0; segment <= spec.ring_segments; ++segment) {
      const float t = static_cast<float>(segment) / static_cast<float>(spec.ring_segments);
      const float angle = t * kTau + ring_t * 0.38f + std::sin(t * kTau * 2.0f + ring_phase) *
                                                    spec.irregularity * 0.18f;
      const float bias = signedHash(spec.seed + static_cast<std::uint32_t>(ring * 409 + segment));
      strand_points.push_back(ellipsePoint(spec, basis, angle, r, ring_phase, bias));
    }
    appendRibbon(mesh, strand_points, basis.normal, ring_width, spec.double_sided);
  }

  const int droop_count = std::max(3, spec.radial_strands / 5);
  for (int strand = 0; strand < droop_count; ++strand) {
    const float t = (static_cast<float>(strand) + 0.5f) / static_cast<float>(droop_count);
    const float angle = kPi * (0.12f + t * 0.76f);
    const float bias = signedHash(spec.seed + static_cast<std::uint32_t>(strand * 503 + 19));
    strand_points.clear();
    constexpr int kDroopSegments = 8;
    for (int step = 0; step <= kDroopSegments; ++step) {
      const float r = 0.24f + static_cast<float>(step) / static_cast<float>(kDroopSegments) * 0.68f;
      const Vec3 point = ellipsePoint(spec, basis, angle + bias * 0.05f, r, phase, bias) +
                         basis.up * (-spec.sag * 0.55f *
                                     std::sin(static_cast<float>(step) /
                                              static_cast<float>(kDroopSegments) * kPi));
      strand_points.push_back(point);
    }
    appendRibbon(mesh, strand_points, basis.normal, spec.strand_width * 0.72f,
                 spec.double_sided);
  }

  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = safeNormalizeOr(vertex.normal, basis.normal);
    vertex.tangent = {basis.side.x, basis.side.y, basis.side.z, 1.0f};
  }
  return mesh;
}

} // namespace aster
