// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/procedural_modeling.hpp"

#include "aster/geometry/mesh_modeling.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

constexpr float kEpsilon = 0.000001f;

[[nodiscard]] Vec3 safeNormalize(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > kEpsilon ? normalized : fallback;
}

[[nodiscard]] Vec3 scaled(const Vec3 value, const Vec3 scale) {
  return {value.x * scale.x, value.y * scale.y, value.z * scale.z};
}

[[nodiscard]] Vec3 safeCrossBasis(const Vec3 tangent, const Vec3 up_hint, Vec3 &up) {
  const Vec3 forward = safeNormalize(tangent, {0.0f, 0.0f, 1.0f});
  up = safeNormalize(up_hint, {0.0f, 1.0f, 0.0f});
  Vec3 side = normalize(cross(up, forward));
  if (length(side) <= kEpsilon) {
    side = normalize(cross(std::abs(forward.y) > 0.86f ? Vec3{1.0f, 0.0f, 0.0f}
                                                        : Vec3{0.0f, 1.0f, 0.0f},
                           forward));
  }
  side = safeNormalize(side, {1.0f, 0.0f, 0.0f});
  up = safeNormalize(cross(forward, side), {0.0f, 1.0f, 0.0f});
  return side;
}

void appendIndexQuad(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                     const std::uint32_t c, const std::uint32_t d) {
  appendOrientedQuadIndices(mesh, a, b, c, d,
                            triangleNormal(mesh.vertices[a].position, mesh.vertices[b].position,
                                           mesh.vertices[c].position));
}

[[nodiscard]] Vertex vertex(const Vec3 position, const Vec3 normal, const Vec2 uv) {
  return {.position = position, .normal = safeNormalize(normal, {0.0f, 1.0f, 0.0f}), .uv = uv};
}

} // namespace

std::uint32_t AsterMeshAssembly::beginPart(std::string name) {
  const std::uint32_t index = static_cast<std::uint32_t>(parts_.size());
  parts_.push_back({.name = std::move(name),
                    .first_vertex = static_cast<std::uint32_t>(mesh_.vertices.size()),
                    .first_index = static_cast<std::uint32_t>(mesh_.indices.size())});
  return index;
}

void AsterMeshAssembly::finishPart(const std::uint32_t part_index) {
  if (part_index >= parts_.size()) {
    return;
  }
  MeshAssemblyPart &part = parts_[part_index];
  part.vertex_count = static_cast<std::uint32_t>(mesh_.vertices.size()) - part.first_vertex;
  part.index_count = static_cast<std::uint32_t>(mesh_.indices.size()) - part.first_index;
}

void AsterMeshAssembly::appendPart(std::string name, const CpuMesh &mesh, const Vec3 translation,
                                   const Vec3 scale) {
  const std::uint32_t part = beginPart(std::move(name));
  mergeMesh(mesh_, mesh, translation, scale);
  finishPart(part);
}

CpuMesh AsterMeshAssembly::releaseMesh() {
  CpuMesh out = std::move(mesh_);
  parts_.clear();
  mesh_ = {};
  return out;
}

SweepProfile makeCircularSweepProfile(const int segments, const float radius,
                                       const float vertical_scale) {
  if (segments < 3 || radius <= 0.0f || vertical_scale <= 0.0f) {
    throw std::invalid_argument(
        "Circular sweep profile requires segments >= 3 and positive radii.");
  }
  SweepProfile profile;
  profile.closed = true;
  profile.points.reserve(static_cast<std::size_t>(segments));
  for (int i = 0; i < segments; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(segments);
    const float theta = u * pi() * 2.0f;
    profile.points.push_back({.offset = {std::cos(theta) * radius,
                                         std::sin(theta) * radius * vertical_scale},
                              .uv = {u, 0.0f}});
  }
  return profile;
}

void mergeMesh(CpuMesh &target, const CpuMesh &source, const Vec3 translation, const Vec3 scale) {
  const std::uint32_t base = static_cast<std::uint32_t>(target.vertices.size());
  target.vertices.reserve(target.vertices.size() + source.vertices.size());
  for (Vertex vertex : source.vertices) {
    vertex.position = scaled(vertex.position, scale) + translation;
    vertex.normal = safeNormalize(
        {vertex.normal.x / std::max(std::abs(scale.x), kEpsilon),
         vertex.normal.y / std::max(std::abs(scale.y), kEpsilon),
         vertex.normal.z / std::max(std::abs(scale.z), kEpsilon)},
        {0.0f, 1.0f, 0.0f});
    target.vertices.push_back(vertex);
  }
  target.indices.reserve(target.indices.size() + source.indices.size());
  for (const std::uint32_t index : source.indices) {
    target.indices.push_back(base + index);
  }
}

void appendEllipsoidSection(CpuMesh &mesh, const EllipsoidSectionSpec &spec) {
  if (spec.segments < 3 || spec.rings < 2 || spec.radius.x <= 0.0f || spec.radius.y <= 0.0f ||
      spec.radius.z <= 0.0f || spec.theta_max <= spec.theta_min || spec.phi_max <= spec.phi_min) {
    throw std::invalid_argument(
        "Ellipsoid section requires positive radii, ordered angles, and enough segments.");
  }

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const int columns = spec.segments + 1;
  mesh.vertices.reserve(mesh.vertices.size() +
                        static_cast<std::size_t>((spec.rings + 1) * columns));
  mesh.indices.reserve(mesh.indices.size() +
                       static_cast<std::size_t>(spec.rings * spec.segments * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(spec.rings);
    const float phi = spec.phi_min + (spec.phi_max - spec.phi_min) * v;
    const float horizontal = std::sin(phi);
    for (int segment = 0; segment <= spec.segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
      const float theta = spec.theta_min + (spec.theta_max - spec.theta_min) * u;
      const Vec3 unit{std::cos(theta) * horizontal, std::cos(phi),
                      std::sin(theta) * horizontal};
      const Vec3 position = spec.center + Vec3{unit.x * spec.radius.x, unit.y * spec.radius.y,
                                               unit.z * spec.radius.z};
      const Vec3 normal = safeNormalize({unit.x / spec.radius.x, unit.y / spec.radius.y,
                                         unit.z / spec.radius.z},
                                        {0.0f, 1.0f, 0.0f});
      mesh.vertices.push_back(
          vertex(position, normal,
                 {spec.uv_origin.x + u * spec.uv_scale.x,
                  spec.uv_origin.y + v * spec.uv_scale.y}));
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.segments; ++segment) {
      const std::uint32_t a = base + static_cast<std::uint32_t>(ring * columns + segment);
      const std::uint32_t b = base + static_cast<std::uint32_t>((ring + 1) * columns + segment);
      const std::uint32_t c = base + static_cast<std::uint32_t>((ring + 1) * columns + segment + 1);
      const std::uint32_t d = base + static_cast<std::uint32_t>(ring * columns + segment + 1);
      appendOrientedQuadIndices(mesh, a, b, c, d, mesh.vertices[a].normal + mesh.vertices[c].normal);
    }
  }
}

void appendLathedSurface(CpuMesh &mesh, const LatheSurfaceSpec &spec) {
  if (spec.radial_segments < 3 || spec.profile.size() < 2u || spec.end_angle <= spec.start_angle) {
    throw std::invalid_argument(
        "Lathed surface requires at least two profile points and radial_segments >= 3.");
  }

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const int rows = static_cast<int>(spec.profile.size());
  const int columns = spec.radial_segments + 1;
  mesh.vertices.reserve(mesh.vertices.size() + static_cast<std::size_t>(rows * columns));
  mesh.indices.reserve(mesh.indices.size() +
                       static_cast<std::size_t>((rows - 1) * spec.radial_segments * 6));

  for (int row = 0; row < rows; ++row) {
    const Vec2 current = spec.profile[static_cast<std::size_t>(row)];
    const Vec2 before = spec.profile[static_cast<std::size_t>(std::max(row - 1, 0))];
    const Vec2 after = spec.profile[static_cast<std::size_t>(std::min(row + 1, rows - 1))];
    const Vec2 slope = after - before;
    const float radial_normal = slope.y;
    const float vertical_normal = -slope.x;
    const float v = rows == 1 ? 0.0f : static_cast<float>(row) / static_cast<float>(rows - 1);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = spec.start_angle + (spec.end_angle - spec.start_angle) * u;
      const Vec3 radial{std::cos(theta), 0.0f, std::sin(theta)};
      const Vec3 position = spec.center + radial * current.x + Vec3{0.0f, current.y, 0.0f};
      const Vec3 normal =
          safeNormalize(radial * radial_normal + Vec3{0.0f, vertical_normal, 0.0f}, radial);
      mesh.vertices.push_back(vertex(position, normal, {u * spec.uv_scale.x, v * spec.uv_scale.y}));
    }
  }

  for (int row = 0; row + 1 < rows; ++row) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = base + static_cast<std::uint32_t>(row * columns + segment);
      const std::uint32_t b = base + static_cast<std::uint32_t>((row + 1) * columns + segment);
      const std::uint32_t c = base + static_cast<std::uint32_t>((row + 1) * columns + segment + 1);
      const std::uint32_t d = base + static_cast<std::uint32_t>(row * columns + segment + 1);
      appendOrientedQuadIndices(mesh, a, b, c, d, mesh.vertices[a].normal + mesh.vertices[c].normal);
    }
  }
}

void appendSweptTube(CpuMesh &mesh, const SweptTubeSpec &spec) {
  if (spec.path.points.size() < 2u || spec.profile.points.size() < 3u || spec.radius <= 0.0f) {
    throw std::invalid_argument("Swept tube requires a path, a closed profile, and radius > 0.");
  }

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const int path_count = static_cast<int>(spec.path.points.size());
  const int profile_count = static_cast<int>(spec.profile.points.size());
  const int columns = profile_count + (spec.profile.closed ? 1 : 0);
  mesh.vertices.reserve(mesh.vertices.size() + static_cast<std::size_t>(path_count * columns));
  mesh.indices.reserve(mesh.indices.size() +
                       static_cast<std::size_t>((path_count - 1) * profile_count * 6));

  for (int path_index = 0; path_index < path_count; ++path_index) {
    const SweepPathPoint &point = spec.path.points[static_cast<std::size_t>(path_index)];
    const Vec3 prev = spec.path.points[static_cast<std::size_t>(std::max(path_index - 1, 0))].position;
    const Vec3 next =
        spec.path.points[static_cast<std::size_t>(std::min(path_index + 1, path_count - 1))].position;
    Vec3 up{};
    const Vec3 side = safeCrossBasis(next - prev, point.up, up);
    const float v = path_count == 1 ? 0.0f
                                    : static_cast<float>(path_index) / static_cast<float>(path_count - 1);
    for (int profile_index = 0; profile_index < columns; ++profile_index) {
      const int wrapped = profile_index % profile_count;
      const SweepProfilePoint &profile = spec.profile.points[static_cast<std::size_t>(wrapped)];
      const Vec3 radial = safeNormalize(side * profile.offset.x + up * profile.offset.y, up);
      const Vec3 position =
          point.position + radial * (spec.radius * std::max(point.radius_scale, 0.001f));
      const float u = static_cast<float>(wrapped) / static_cast<float>(profile_count);
      mesh.vertices.push_back(vertex(position, radial, {u, v}));
    }
  }

  for (int path_index = 0; path_index + 1 < path_count; ++path_index) {
    for (int profile_index = 0; profile_index < profile_count; ++profile_index) {
      const std::uint32_t a =
          base + static_cast<std::uint32_t>(path_index * columns + profile_index);
      const std::uint32_t b =
          base + static_cast<std::uint32_t>((path_index + 1) * columns + profile_index);
      const std::uint32_t c =
          base + static_cast<std::uint32_t>((path_index + 1) * columns + profile_index + 1);
      const std::uint32_t d =
          base + static_cast<std::uint32_t>(path_index * columns + profile_index + 1);
      appendIndexQuad(mesh, a, b, c, d);
    }
  }

  if (spec.rebuild_normals) {
    rebuildAngleWeightedNormals(mesh);
  }
}

void appendExtrudedRidge(CpuMesh &mesh, const ExtrudedRidgeSpec &spec) {
  if (spec.spine.size() < 2u || spec.width <= 0.0f || spec.height <= 0.0f) {
    throw std::invalid_argument("Extruded ridge requires at least two spine points and dimensions.");
  }

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const int count = static_cast<int>(spec.spine.size());
  mesh.vertices.reserve(mesh.vertices.size() + static_cast<std::size_t>(count * 3 + 6));
  mesh.indices.reserve(mesh.indices.size() + static_cast<std::size_t>((count - 1) * 12 + 12));

  for (int i = 0; i < count; ++i) {
    const Vec3 prev = spec.spine[static_cast<std::size_t>(std::max(i - 1, 0))];
    const Vec3 next = spec.spine[static_cast<std::size_t>(std::min(i + 1, count - 1))];
    Vec3 up{};
    const Vec3 side = safeCrossBasis(next - prev, spec.up, up);
    const Vec3 center = spec.spine[static_cast<std::size_t>(i)];
    const float v = count == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(count - 1);
    mesh.vertices.push_back(vertex(center - side * (spec.width * 0.5f), -side, {0.0f, v}));
    mesh.vertices.push_back(vertex(center + side * (spec.width * 0.5f), side, {1.0f, v}));
    mesh.vertices.push_back(vertex(center + up * spec.height, up, {0.5f, v}));
  }

  for (int i = 0; i + 1 < count; ++i) {
    const std::uint32_t a0 = base + static_cast<std::uint32_t>(i * 3);
    const std::uint32_t b0 = a0 + 1u;
    const std::uint32_t c0 = a0 + 2u;
    const std::uint32_t a1 = base + static_cast<std::uint32_t>((i + 1) * 3);
    const std::uint32_t b1 = a1 + 1u;
    const std::uint32_t c1 = a1 + 2u;
    appendOrientedQuadIndices(mesh, a0, a1, c1, c0, mesh.vertices[c0].normal);
    appendOrientedQuadIndices(mesh, c0, c1, b1, b0, mesh.vertices[c0].normal);
    appendOrientedQuadIndices(mesh, b0, b1, a1, a0, -mesh.vertices[c0].normal);
  }

  if (spec.cap_ends) {
    appendOrientedTriangleIndices(mesh, base, base + 2u, base + 1u,
                                  -safeNormalize(spec.spine[1] - spec.spine[0],
                                                 {0.0f, 0.0f, 1.0f}));
    const std::uint32_t last = base + static_cast<std::uint32_t>((count - 1) * 3);
    appendOrientedTriangleIndices(mesh, last, last + 1u, last + 2u,
                                  safeNormalize(spec.spine.back() - spec.spine[spec.spine.size() - 2u],
                                                {0.0f, 0.0f, 1.0f}));
  }
  rebuildAngleWeightedNormals(mesh);
}

CpuMesh makeLathedSurface(const LatheSurfaceSpec &spec) {
  CpuMesh mesh;
  appendLathedSurface(mesh, spec);
  return mesh;
}

CpuMesh makeEllipsoidSection(const EllipsoidSectionSpec &spec) {
  CpuMesh mesh;
  appendEllipsoidSection(mesh, spec);
  return mesh;
}

CpuMesh makeSweptTube(const SweptTubeSpec &spec) {
  CpuMesh mesh;
  appendSweptTube(mesh, spec);
  return mesh;
}

CpuMesh makeExtrudedRidge(const ExtrudedRidgeSpec &spec) {
  CpuMesh mesh;
  appendExtrudedRidge(mesh, spec);
  return mesh;
}

} // namespace aster
