// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/cable_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct StrandFrame {
  Vec3 center{};
  Vec3 radial{};
  Vec3 binormal{};
};

float smoothstep(const float edge0, const float edge1, const float value) {
  const float t = clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

StrandFrame strandFrame(const CableMeshSpec &spec, const int strand, const float v) {
  const float phase =
      (static_cast<float>(strand) / static_cast<float>(spec.strand_count)) * kPi * 2.0f;
  const float theta = phase + v * spec.twist_turns * kPi * 2.0f;
  const float center_radius = spec.radius * spec.strand_center_radius;
  const float local_y = (v - 0.5f) * spec.length;
  const Vec3 radial{std::cos(theta), 0.0f, std::sin(theta)};
  const Vec3 circumferential{-std::sin(theta), 0.0f, std::cos(theta)};
  const Vec3 tangent = normalize(circumferential * (center_radius * spec.twist_turns * kPi * 2.0f) +
                                 Vec3{0.0f, spec.length, 0.0f});
  const Vec3 binormal = normalize(cross(tangent, radial));
  return {radial * center_radius + Vec3{0.0f, local_y, 0.0f}, radial, binormal};
}

float fiberRelief(const CableMeshSpec &spec, const int strand, const float tube_u, const float v) {
  const float strand_phase =
      (static_cast<float>(strand) / static_cast<float>(spec.strand_count)) * kPi * 2.0f;
  const float wrap = tube_u * kPi * 2.0f;
  const float long_fiber =
      std::sin(wrap * 3.0f + v * spec.fiber_twist_turns * kPi * 2.0f + strand_phase);
  const float fine_fiber = std::sin(wrap * 11.0f - v * spec.fiber_twist_turns * kPi * 5.1f);
  return 1.0f + spec.strand_depth * (long_fiber * 0.62f + fine_fiber * 0.18f);
}

void appendStrand(CpuMesh &mesh, const CableMeshSpec &spec, const int strand) {
  const int ring_vertices = spec.radial_segments + 1;
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const float strand_radius = spec.radius * spec.strand_radius;

  for (int y = 0; y <= spec.length_segments; ++y) {
    const float v = static_cast<float>(y) / static_cast<float>(spec.length_segments);
    const StrandFrame frame = strandFrame(spec, strand, v);
    for (int x = 0; x <= spec.radial_segments; ++x) {
      const float tube_u = static_cast<float>(x) / static_cast<float>(spec.radial_segments);
      const float alpha = tube_u * kPi * 2.0f;
      const float relief = fiberRelief(spec, strand, tube_u, v);
      const Vec3 local_normal =
          normalize(frame.radial * std::cos(alpha) + frame.binormal * std::sin(alpha));
      const Vec3 point = frame.center + local_normal * (strand_radius * relief);
      mesh.vertices.push_back({point,
                               local_normal,
                               {(static_cast<float>(strand) + tube_u) /
                                    static_cast<float>(std::max(1, spec.strand_count)),
                                v}});
    }
  }

  for (int y = 0; y < spec.length_segments; ++y) {
    for (int x = 0; x < spec.radial_segments; ++x) {
      const std::uint32_t a = base + static_cast<std::uint32_t>(y * ring_vertices + x);
      const std::uint32_t b = base + static_cast<std::uint32_t>((y + 1) * ring_vertices + x);
      const std::uint32_t c = base + static_cast<std::uint32_t>((y + 1) * ring_vertices + x + 1);
      const std::uint32_t d = base + static_cast<std::uint32_t>(y * ring_vertices + x + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }
}

float braidedSleeveRelief(const CableMeshSpec &spec, const float u, const float v) {
  const float theta = u * kPi * 2.0f;
  const float lanes = static_cast<float>(std::max(2, spec.strand_count));
  const float twist = spec.twist_turns * kPi * 2.0f;
  const float diagonal_a = std::sin(theta * lanes + v * twist);
  const float diagonal_b = std::sin(theta * lanes - v * twist + kPi * 0.42f);
  const float raised_a = smoothstep(0.76f, 0.98f, diagonal_a);
  const float raised_b = smoothstep(0.76f, 0.98f, diagonal_b);
  const float fine = std::sin(theta * lanes * 2.7f + v * spec.fiber_twist_turns * kPi * 2.0f);
  return 1.0f + spec.strand_depth * ((raised_a + raised_b - 0.72f) * 0.42f + fine * 0.10f);
}

void appendBraidedSleeve(CpuMesh &mesh, const CableMeshSpec &spec) {
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(ring_vertices * (spec.length_segments + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.length_segments * 6));

  for (int y = 0; y <= spec.length_segments; ++y) {
    const float v = static_cast<float>(y) / static_cast<float>(spec.length_segments);
    const float local_y = (v - 0.5f) * spec.length;
    for (int x = 0; x <= spec.radial_segments; ++x) {
      const float u = static_cast<float>(x) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float radius = spec.radius * braidedSleeveRelief(spec, u, v);
      const Vec3 radial{std::cos(theta), 0.0f, std::sin(theta)};
      const Vec3 tangent_bias{0.0f,
                              spec.strand_depth * 0.34f *
                                  std::sin(theta * static_cast<float>(spec.strand_count) +
                                           v * spec.twist_turns * kPi * 2.0f),
                              0.0f};
      mesh.vertices.push_back(
          {radial * radius + Vec3{0.0f, local_y, 0.0f}, normalize(radial + tangent_bias), {u, v}});
    }
  }

  for (int y = 0; y < spec.length_segments; ++y) {
    for (int x = 0; x < spec.radial_segments; ++x) {
      const std::uint32_t a = static_cast<std::uint32_t>(y * ring_vertices + x);
      const std::uint32_t b = static_cast<std::uint32_t>((y + 1) * ring_vertices + x);
      const std::uint32_t c = static_cast<std::uint32_t>((y + 1) * ring_vertices + x + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(y * ring_vertices + x + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }
}

void appendSleeveRidgeFamily(CpuMesh &mesh, const CableMeshSpec &spec, const float direction) {
  const int lanes = std::max(2, spec.strand_count);
  const int steps = std::max(2, spec.length_segments);
  const float ridge_half_width = kPi / (static_cast<float>(lanes) * 9.0f);
  const float ridge_radius = spec.radius * (1.0f + spec.strand_depth * 0.55f);
  const float twist = direction * spec.twist_turns * kPi * 2.0f;

  for (int lane = 0; lane < lanes; ++lane) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const float phase = (static_cast<float>(lane) / static_cast<float>(lanes)) * kPi * 2.0f +
                        (direction < 0.0f ? kPi / static_cast<float>(lanes) : 0.0f);
    for (int y = 0; y <= steps; ++y) {
      const float v = static_cast<float>(y) / static_cast<float>(steps);
      const float local_y = (v - 0.5f) * spec.length;
      const float center_theta = phase + (v - 0.5f) * twist;
      for (int side = -1; side <= 1; side += 2) {
        const float theta = center_theta + static_cast<float>(side) * ridge_half_width;
        const Vec3 radial{std::cos(theta), 0.0f, std::sin(theta)};
        mesh.vertices.push_back({radial * ridge_radius + Vec3{0.0f, local_y, 0.0f},
                                 radial,
                                 {static_cast<float>(lane) / static_cast<float>(lanes), v}});
      }
    }

    for (int y = 0; y < steps; ++y) {
      const std::uint32_t a = base + static_cast<std::uint32_t>(y * 2);
      const std::uint32_t b = base + static_cast<std::uint32_t>((y + 1) * 2);
      const std::uint32_t c = base + static_cast<std::uint32_t>((y + 1) * 2 + 1);
      const std::uint32_t d = base + static_cast<std::uint32_t>(y * 2 + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }
}

void appendTwistedStrands(CpuMesh &mesh, const CableMeshSpec &spec) {
  const std::size_t vertices_per_strand =
      static_cast<std::size_t>((spec.radial_segments + 1) * (spec.length_segments + 1));
  const std::size_t indices_per_strand =
      static_cast<std::size_t>(spec.radial_segments * spec.length_segments * 6);
  mesh.vertices.reserve(vertices_per_strand * static_cast<std::size_t>(spec.strand_count));
  mesh.indices.reserve(indices_per_strand * static_cast<std::size_t>(spec.strand_count));

  for (int strand = 0; strand < spec.strand_count; ++strand) {
    appendStrand(mesh, spec, strand);
  }
}

} // namespace

CpuMesh makeCableMesh(CableMeshSpec spec) {
  if (spec.radial_segments < 6 || spec.length_segments < 2 || spec.strand_count < 2 ||
      spec.radius <= 0.0f || spec.length <= 0.0f || spec.strand_depth < 0.0f ||
      spec.strand_radius <= 0.0f || spec.strand_center_radius <= 0.0f ||
      spec.fiber_twist_turns < 0.0f) {
    throw std::invalid_argument(
        "Cable mesh requires multiple strands, positive dimensions and non-negative fiber relief.");
  }

  spec.strand_depth = std::min(spec.strand_depth, 0.24f);
  spec.strand_radius = std::min(spec.strand_radius, 0.48f);
  spec.strand_center_radius = std::min(spec.strand_center_radius, 0.82f);
  spec.twist_turns = std::max(spec.twist_turns, 0.0f);

  CpuMesh mesh;
  switch (spec.construction) {
  case CableConstruction::BraidedSleeve:
    appendBraidedSleeve(mesh, spec);
    appendSleeveRidgeFamily(mesh, spec, 1.0f);
    appendSleeveRidgeFamily(mesh, spec, -1.0f);
    break;
  case CableConstruction::TwistedStrands:
    appendTwistedStrands(mesh, spec);
    break;
  }

  return mesh;
}

} // namespace aster
