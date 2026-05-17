// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/tube_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = kPi * 2.0f;

void appendIndexQuad(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                     const std::uint32_t c, const std::uint32_t d) {
  mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
}

std::uint32_t appendVertex(CpuMesh &mesh, const Vec3 position, const Vec3 normal, const Vec2 uv) {
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({position, normalize(normal), uv});
  return index;
}

float positiveWallThickness(const TubeMeshSpec &spec) {
  return std::clamp(spec.wall_thickness, 0.0f, spec.outer_radius * 0.92f);
}

} // namespace

CpuMesh makeTubeMesh(const TubeMeshSpec &spec) {
  if (spec.length <= 0.0f || spec.outer_radius <= 0.0f || spec.radial_segments < 6 ||
      spec.length_segments < 1) {
    throw std::invalid_argument(
        "Tube generation requires positive dimensions, radial_segments >= 6, length_segments >= 1.");
  }

  const float wall = positiveWallThickness(spec);
  const float inner_radius = spec.outer_radius - wall;
  CpuMesh mesh;
  const int rings = spec.length_segments + 1;
  const int columns = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(rings * columns * (wall > 0.0f ? 2 : 1)) +
                        static_cast<std::size_t>(spec.radial_segments * 4));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.length_segments * 12));

  const auto vertex_at = [columns](const int x, const int radial) {
    return static_cast<std::uint32_t>(x * columns + radial);
  };

  const float half = spec.length * 0.5f;
  for (int x = 0; x <= spec.length_segments; ++x) {
    const float u = static_cast<float>(x) / static_cast<float>(spec.length_segments);
    const float axial = -half + spec.length * u;
    for (int radial = 0; radial <= spec.radial_segments; ++radial) {
      const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float angle = v * kTau;
      const Vec3 outward{0.0f, std::cos(angle), std::sin(angle)};
      appendVertex(mesh, {axial, outward.y * spec.outer_radius, outward.z * spec.outer_radius},
                   outward, {u, v});
    }
  }

  for (int x = 0; x < spec.length_segments; ++x) {
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      appendIndexQuad(mesh, vertex_at(x, radial), vertex_at(x + 1, radial),
                      vertex_at(x + 1, radial + 1), vertex_at(x, radial + 1));
    }
  }

  if (wall <= 0.0f) {
    return mesh;
  }

  const std::uint32_t inner_start = static_cast<std::uint32_t>(mesh.vertices.size());
  const auto inner_vertex_at = [inner_start, columns](const int x, const int radial) {
    return inner_start + static_cast<std::uint32_t>(x * columns + radial);
  };

  for (int x = 0; x <= spec.length_segments; ++x) {
    const float u = static_cast<float>(x) / static_cast<float>(spec.length_segments);
    const float axial = -half + spec.length * u;
    for (int radial = 0; radial <= spec.radial_segments; ++radial) {
      const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float angle = v * kTau;
      const Vec3 inward{0.0f, -std::cos(angle), -std::sin(angle)};
      appendVertex(mesh, {axial, -inward.y * inner_radius, -inward.z * inner_radius}, inward,
                   {u, v});
    }
  }

  for (int x = 0; x < spec.length_segments; ++x) {
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      appendIndexQuad(mesh, inner_vertex_at(x, radial + 1), inner_vertex_at(x + 1, radial + 1),
                      inner_vertex_at(x + 1, radial), inner_vertex_at(x, radial));
    }
  }

  for (const float side : {-1.0f, 1.0f}) {
    const float axial = side * half;
    const Vec3 cap_normal{side, 0.0f, 0.0f};
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      const float v0 = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float v1 = static_cast<float>(radial + 1) / static_cast<float>(spec.radial_segments);
      const float a0 = v0 * kTau;
      const float a1 = v1 * kTau;
      const Vec3 outer0{axial, std::cos(a0) * spec.outer_radius, std::sin(a0) * spec.outer_radius};
      const Vec3 outer1{axial, std::cos(a1) * spec.outer_radius, std::sin(a1) * spec.outer_radius};
      const Vec3 inner0{axial, std::cos(a0) * inner_radius, std::sin(a0) * inner_radius};
      const Vec3 inner1{axial, std::cos(a1) * inner_radius, std::sin(a1) * inner_radius};
      const std::uint32_t a = appendVertex(mesh, outer0, cap_normal, {0.0f, v0});
      const std::uint32_t b = appendVertex(mesh, outer1, cap_normal, {0.0f, v1});
      const std::uint32_t c = appendVertex(mesh, inner1, cap_normal, {1.0f, v1});
      const std::uint32_t d = appendVertex(mesh, inner0, cap_normal, {1.0f, v0});
      if (side < 0.0f) {
        appendIndexQuad(mesh, b, a, d, c);
      } else {
        appendIndexQuad(mesh, a, b, c, d);
      }
    }
  }

  return mesh;
}

CpuMesh makeCircumferentialBeadMesh(const CircumferentialBeadMeshSpec &spec) {
  if (spec.pipe_radius <= 0.0f || spec.bead_radius <= 0.0f || spec.axial_width <= 0.0f ||
      spec.radial_segments < 6 || spec.bead_segments < 4) {
    throw std::invalid_argument("Circumferential bead generation requires positive dimensions, "
                                "radial_segments >= 6, bead_segments >= 4.");
  }

  CpuMesh mesh;
  const int radial_columns = spec.radial_segments + 1;
  const int bead_columns = spec.bead_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(radial_columns * bead_columns));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.bead_segments * 6));

  for (int radial = 0; radial <= spec.radial_segments; ++radial) {
    const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
    const float theta = v * kTau;
    const Vec3 radial_dir{0.0f, std::cos(theta), std::sin(theta)};
    for (int bead = 0; bead <= spec.bead_segments; ++bead) {
      const float u = static_cast<float>(bead) / static_cast<float>(spec.bead_segments);
      const float phi = u * kTau;
      const float axial = std::cos(phi) * spec.axial_width * 0.5f;
      const float lift = std::max(std::sin(phi), -0.18f) * spec.bead_radius;
      const float radius = spec.pipe_radius + spec.bead_radius * 0.42f + lift;
      const Vec3 normal = normalize(Vec3{std::cos(phi) * 0.58f, radial_dir.y * std::sin(phi),
                                         radial_dir.z * std::sin(phi)} +
                                    radial_dir * 0.34f);
      appendVertex(mesh, {axial, radial_dir.y * radius, radial_dir.z * radius}, normal, {u, v});
    }
  }

  const auto vertex_at = [bead_columns](const int radial, const int bead) {
    return static_cast<std::uint32_t>(radial * bead_columns + bead);
  };
  for (int radial = 0; radial < spec.radial_segments; ++radial) {
    for (int bead = 0; bead < spec.bead_segments; ++bead) {
      appendIndexQuad(mesh, vertex_at(radial, bead), vertex_at(radial + 1, bead),
                      vertex_at(radial + 1, bead + 1), vertex_at(radial, bead + 1));
    }
  }

  return mesh;
}

} // namespace aster
