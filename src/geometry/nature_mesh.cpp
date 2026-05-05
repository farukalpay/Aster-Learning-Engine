// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/nature_mesh.hpp"
#include "aster/geometry/water_mesh.hpp"
#include "aster/math/noise.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float smoothstep(const float edge0, const float edge1, const float value) {
  const float t = clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float ringNoise(const float x, const float z) {
  return std::sin(x * 4.17f + z * 1.73f) * 0.50f + std::sin(x * 8.31f - z * 5.19f) * 0.31f +
         std::sin((x + z) * 12.67f) * 0.19f;
}

float contourScale(const float theta, const float irregularity) {
  if (irregularity <= 0.0f) {
    return 1.0f;
  }
  const float profile = std::sin(theta * 1.7f + 0.42f) * 0.46f +
                        std::sin(theta * 2.9f - 1.16f) * 0.32f +
                        std::sin(theta * 5.3f + 2.06f) * 0.22f;
  return std::max(0.55f, 1.0f + profile * irregularity);
}

float directionalRadius(const float fallback, const float negative_radius,
                        const float positive_radius, const float direction) {
  if (direction < 0.0f && negative_radius > 0.0f) {
    return negative_radius;
  }
  if (direction > 0.0f && positive_radius > 0.0f) {
    return positive_radius;
  }
  return fallback;
}

Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return normalize(cross(b - a, c - a));
}

Vec3 cubicPathPoint(const PathRibbonMeshSpec &spec, const float t) {
  const float inv = 1.0f - t;
  return spec.start * (inv * inv * inv) + spec.control * (3.0f * inv * inv * t) +
         spec.control_b * (3.0f * inv * t * t) + spec.end * (t * t * t);
}

Vec3 cubicPathTangent(const PathRibbonMeshSpec &spec, const float t) {
  const float inv = 1.0f - t;
  return normalize((spec.control - spec.start) * (3.0f * inv * inv) +
                   (spec.control_b - spec.control) * (6.0f * inv * t) +
                   (spec.end - spec.control_b) * (3.0f * t * t));
}

float pathEndpointWeight(const float t, const float taper) {
  if (taper <= 0.0f) {
    return 1.0f;
  }
  return smoothstep(0.0f, taper, t) * smoothstep(0.0f, taper, 1.0f - t);
}

void accumulateNormal(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                      const std::uint32_t c) {
  const Vec3 normal =
      faceNormal(mesh.vertices[a].position, mesh.vertices[b].position, mesh.vertices[c].position);
  mesh.vertices[a].normal = mesh.vertices[a].normal + normal;
  mesh.vertices[b].normal = mesh.vertices[b].normal + normal;
  mesh.vertices[c].normal = mesh.vertices[c].normal + normal;
}

void appendTriangle(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec2 uv_a,
                    const Vec2 uv_b, const Vec2 uv_c) {
  const Vec3 normal = faceNormal(a, b, c);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, uv_a});
  mesh.vertices.push_back({b, normal, uv_b});
  mesh.vertices.push_back({c, normal, uv_c});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u});
}

void appendQuad(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                const Vec2 uv_a, const Vec2 uv_b, const Vec2 uv_c, const Vec2 uv_d) {
  const Vec3 normal = faceNormal(a, b, c);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, uv_a});
  mesh.vertices.push_back({b, normal, uv_b});
  mesh.vertices.push_back({c, normal, uv_c});
  mesh.vertices.push_back({d, normal, uv_d});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
}

void appendDoubleSidedQuad(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                           const Vec2 uv_a, const Vec2 uv_b, const Vec2 uv_c, const Vec2 uv_d) {
  appendQuad(mesh, a, b, c, d, uv_a, uv_b, uv_c, uv_d);
  appendQuad(mesh, d, c, b, a, uv_d, uv_c, uv_b, uv_a);
}

void appendDoubleSidedTriangle(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c,
                               const Vec2 uv_a, const Vec2 uv_b, const Vec2 uv_c) {
  appendTriangle(mesh, a, b, c, uv_a, uv_b, uv_c);
  appendTriangle(mesh, c, b, a, uv_c, uv_b, uv_a);
}

void finalizeNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = normalize(vertex.normal);
    if (length(vertex.normal) <= 0.0001f) {
      vertex.normal = {0.0f, 1.0f, 0.0f};
    }
  }
}

void appendEllipsoid(CpuMesh &mesh, const Vec3 center, const Vec3 radius, const int segments,
                     const int rings, const Vec2 uv_origin, const Vec2 uv_scale) {
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (int ring = 0; ring <= rings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(rings);
    const float phi = v * kPi;
    const float horizontal = std::sin(phi);
    for (int segment = 0; segment <= segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(segments);
      const float theta = u * kPi * 2.0f;
      const Vec3 unit{std::cos(theta) * horizontal, std::cos(phi), std::sin(theta) * horizontal};
      const Vec3 position = center + Vec3{unit.x * radius.x, unit.y * radius.y, unit.z * radius.z};
      const Vec3 normal =
          normalize({unit.x / std::max(radius.x, 0.001f), unit.y / std::max(radius.y, 0.001f),
                     unit.z / std::max(radius.z, 0.001f)});
      mesh.vertices.push_back(
          {position, normal, {uv_origin.x + u * uv_scale.x, uv_origin.y + v * uv_scale.y}});
    }
  }

  const int ring_vertices = segments + 1;
  for (int ring = 0; ring < rings; ++ring) {
    for (int segment = 0; segment < segments; ++segment) {
      const std::uint32_t a = base + static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b =
          base + static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c =
          base + static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = base + static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }
}

void segmentBasis(const Vec3 from, const Vec3 to, Vec3 &side, Vec3 &up) {
  const Vec3 forward = normalize(to - from);
  Vec3 reference = std::abs(forward.y) > 0.86f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
  side = normalize(cross(reference, forward));
  if (length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  up = normalize(cross(forward, side));
}

void appendTaperedCylinder(CpuMesh &mesh, const Vec3 from, const Vec3 to, const float start_radius,
                           const float end_radius, const int segments, const Vec2 uv_origin,
                           const Vec2 uv_scale) {
  Vec3 side{};
  Vec3 up{};
  segmentBasis(from, to, side, up);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (int ring = 0; ring < 2; ++ring) {
    const float v = static_cast<float>(ring);
    const Vec3 center = ring == 0 ? from : to;
    const float radius = ring == 0 ? start_radius : end_radius;
    for (int segment = 0; segment <= segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(segments);
      const float theta = u * kPi * 2.0f;
      const Vec3 radial = side * std::cos(theta) + up * std::sin(theta);
      mesh.vertices.push_back({center + radial * radius,
                               normalize(radial),
                               {uv_origin.x + u * uv_scale.x, uv_origin.y + v * uv_scale.y}});
    }
  }

  const int ring_vertices = segments + 1;
  for (int segment = 0; segment < segments; ++segment) {
    const std::uint32_t a = base + static_cast<std::uint32_t>(segment);
    const std::uint32_t b = base + static_cast<std::uint32_t>(ring_vertices + segment);
    const std::uint32_t c = base + static_cast<std::uint32_t>(ring_vertices + segment + 1);
    const std::uint32_t d = base + static_cast<std::uint32_t>(segment + 1);
    mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
  }
}

void appendRadialEdgeSkirt(CpuMesh &mesh, const int radial_segments,
                           const std::uint32_t outer_ring_start, const float depth) {
  if (depth <= 0.0f || radial_segments < 3) {
    return;
  }
  const float outward_run = std::max(depth * 1.65f, 0.001f);
  const std::uint32_t skirt_base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.reserve(mesh.vertices.size() + static_cast<std::size_t>((radial_segments + 1) * 2));
  mesh.indices.reserve(mesh.indices.size() + static_cast<std::size_t>(radial_segments * 6));

  for (int segment = 0; segment <= radial_segments; ++segment) {
    const std::uint32_t source_index = outer_ring_start + static_cast<std::uint32_t>(segment);
    if (source_index >= mesh.vertices.size()) {
      return;
    }
    const Vertex &top = mesh.vertices[source_index];
    Vec3 side_normal = normalize(Vec3{top.position.x, 0.0f, top.position.z});
    if (length(side_normal) <= 0.0001f) {
      side_normal = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 bevel_normal =
        normalize({side_normal.x * depth, outward_run, side_normal.z * depth});
    const Vec3 lower_position =
        top.position + Vec3{side_normal.x * outward_run, -depth, side_normal.z * outward_run};
    mesh.vertices.push_back({top.position, bevel_normal, top.uv});
    mesh.vertices.push_back({lower_position, bevel_normal, top.uv});
  }

  for (int segment = 0; segment < radial_segments; ++segment) {
    const std::uint32_t a = skirt_base + static_cast<std::uint32_t>(segment * 2);
    const std::uint32_t b = a + 1u;
    const std::uint32_t d = skirt_base + static_cast<std::uint32_t>((segment + 1) * 2);
    const std::uint32_t c = d + 1u;
    mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
  }
}

void appendMesh(CpuMesh &mesh, const CpuMesh &segment) {
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.insert(mesh.vertices.end(), segment.vertices.begin(), segment.vertices.end());
  mesh.indices.reserve(mesh.indices.size() + segment.indices.size());
  for (const std::uint32_t index : segment.indices) {
    mesh.indices.push_back(base + index);
  }
}

} // namespace

Vec3 evaluatePathRibbonCenter(const PathRibbonMeshSpec &spec, const float t) {
  return cubicPathPoint(spec, clamp(t, 0.0f, 1.0f));
}

Vec3 evaluatePathRibbonTangent(const PathRibbonMeshSpec &spec, const float t) {
  return cubicPathTangent(spec, clamp(t, 0.0f, 1.0f));
}

CpuMesh makeMoundMesh(MoundMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 3 || spec.radius_x <= 0.0f ||
      spec.radius_z <= 0.0f || spec.height < 0.0f || spec.pond_radius_x <= 0.0f ||
      spec.pond_radius_z <= 0.0f || spec.pond_depth < 0.0f || spec.bank_width <= 0.0f ||
      spec.bank_height < 0.0f || spec.basin_floor_radius < 0.0f ||
      spec.basin_floor_radius >= 1.0f || spec.shore_depth_fraction < 0.0f ||
      spec.shore_depth_fraction > 1.0f || spec.edge_irregularity < 0.0f ||
      spec.edge_skirt_depth < 0.0f) {
    throw std::invalid_argument("Mound mesh requires positive radii and enough segments.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(ring_vertices * (spec.rings + 3)));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * (spec.rings + 1) * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float r = static_cast<float>(ring) / static_cast<float>(spec.rings);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float outer_scale = contourScale(theta, spec.edge_irregularity);
      const float x = std::cos(theta) * spec.radius_x * outer_scale * r;
      const float z = std::sin(theta) * spec.radius_z * outer_scale * r;
      const float pond_scale = contourScale(theta + 1.21f, spec.edge_irregularity * 0.55f);
      const float mound = spec.height * std::pow(std::max(1.0f - r * r, 0.0f), 1.08f);
      const float pond_distance =
          std::sqrt((x * x) / (spec.pond_radius_x * spec.pond_radius_x * pond_scale * pond_scale) +
                    (z * z) / (spec.pond_radius_z * spec.pond_radius_z * pond_scale * pond_scale));
      const float basin_slope = smoothstep(spec.basin_floor_radius, 1.0f, pond_distance);
      const float basin_depth =
          spec.shore_depth_fraction + (1.0f - spec.shore_depth_fraction) * (1.0f - basin_slope);
      const float pond_basin =
          basin_depth * (1.0f - smoothstep(1.0f, 1.0f + spec.bank_width, pond_distance));
      const float bank = smoothstep(0.82f, 1.0f, pond_distance) *
                         (1.0f - smoothstep(1.0f, 1.0f + spec.bank_width, pond_distance));
      const float noise = ringNoise(x, z) * spec.surface_noise * smoothstep(0.18f, 1.0f, r);
      const float shoulder =
          smoothstep(0.76f, 1.0f, pond_distance) *
          (1.0f - smoothstep(1.0f, 1.0f + spec.bank_width * 1.8f, pond_distance));
      const float y = mound - pond_basin * spec.pond_depth + bank * spec.bank_height +
                      shoulder * spec.bank_height * 0.26f + noise;
      mesh.vertices.push_back(
          {{x, y, z}, {}, {x / (spec.radius_x * 2.0f) + 0.5f, z / (spec.radius_z * 2.0f) + 0.5f}});
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
      accumulateNormal(mesh, a, d, b);
      accumulateNormal(mesh, d, c, b);
    }
  }

  finalizeNormals(mesh);
  appendRadialEdgeSkirt(mesh, spec.radial_segments,
                        static_cast<std::uint32_t>(spec.rings * ring_vertices),
                        spec.edge_skirt_depth);
  return mesh;
}

CpuMesh makePondWaterMesh(PondWaterMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 1 || spec.radius_x <= 0.0f ||
      spec.radius_z <= 0.0f || spec.edge_softness < 0.0f || spec.edge_irregularity < 0.0f ||
      spec.shoreline_inset < 0.0f || spec.wave_amplitude < 0.0f || spec.wave_choppiness < 0.0f ||
      spec.wind_speed < 0.0f || spec.wave_components < 1) {
    throw std::invalid_argument("Pond water mesh requires positive radii and enough segments.");
  }

  WaterWaveSpectrum spectrum;
  spectrum.wind_direction = spec.wind_direction;
  spectrum.wind_speed = spec.wind_speed;
  spectrum.amplitude = spec.wave_amplitude;
  spectrum.choppiness = spec.wave_choppiness;
  spectrum.dominant_wavelength = std::max(std::min(spec.radius_x, spec.radius_z) * 0.72f, 0.10f);
  spectrum.min_wavelength = std::max(std::min(spec.radius_x, spec.radius_z) * 0.075f, 0.055f);
  spectrum.component_count = spec.wave_components;
  spectrum.seed = spec.radius_x * 11.17f + spec.radius_z * 3.41f +
                  static_cast<float>(spec.radial_segments) * 0.013f;
  return makeEllipticalWaterMesh({.radial_segments = spec.radial_segments,
                                  .rings = spec.rings,
                                  .radius_x = spec.radius_x,
                                  .radius_z = spec.radius_z,
                                  .edge_softness = spec.edge_softness,
                                  .edge_irregularity = spec.edge_irregularity,
                                  .shoreline_inset = spec.shoreline_inset,
                                  .spectrum = spectrum});
}

CpuMesh makeSubmergedBasinMesh(SubmergedBasinMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 3 || spec.radius_x <= 0.0f ||
      spec.radius_z <= 0.0f || spec.floor_depth <= 0.0f || spec.shore_depth < 0.0f ||
      spec.shore_depth > spec.floor_depth || spec.basin_floor_radius < 0.0f ||
      spec.basin_floor_radius >= 1.0f || spec.edge_irregularity < 0.0f ||
      spec.bottom_noise < 0.0f) {
    throw std::invalid_argument(
        "Submerged basin mesh requires positive radii, ordered depths, and enough segments.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(ring_vertices * (spec.rings + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.rings * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float r = static_cast<float>(ring) / static_cast<float>(spec.rings);
    const float floor_to_shore = smoothstep(spec.basin_floor_radius, 1.0f, r);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float scale = contourScale(theta + 0.37f, spec.edge_irregularity);
      const float x = std::cos(theta) * spec.radius_x * scale * r;
      const float z = std::sin(theta) * spec.radius_z * scale * r;
      const float noise =
          ringNoise(x * 0.72f, z * 0.72f) * spec.bottom_noise * smoothstep(0.08f, 0.92f, r);
      const float depth =
          spec.floor_depth * (1.0f - floor_to_shore) + spec.shore_depth * floor_to_shore;
      mesh.vertices.push_back(
          {{x, -depth + noise, z},
           {},
           {x / (spec.radius_x * 2.0f) + 0.5f, z / (spec.radius_z * 2.0f) + 0.5f}});
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
      accumulateNormal(mesh, a, d, b);
      accumulateNormal(mesh, d, c, b);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

CpuMesh makeTerrainTransitionMesh(TerrainTransitionMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 2 || spec.inner_radius_x <= 0.0f ||
      spec.inner_radius_z <= 0.0f || spec.outer_radius_x <= spec.inner_radius_x ||
      spec.outer_radius_z <= spec.inner_radius_z || spec.inner_height < 0.0f ||
      (spec.outer_radius_z_negative > 0.0f &&
       spec.outer_radius_z_negative <= spec.inner_radius_z) ||
      (spec.outer_radius_z_positive > 0.0f &&
       spec.outer_radius_z_positive <= spec.inner_radius_z) ||
      (spec.outer_radius_x_negative > 0.0f &&
       spec.outer_radius_x_negative <= spec.inner_radius_x) ||
      (spec.outer_radius_x_positive > 0.0f &&
       spec.outer_radius_x_positive <= spec.inner_radius_x) ||
      spec.foundation_lift < 0.0f || spec.surface_noise < 0.0f || spec.edge_irregularity < 0.0f ||
      spec.edge_skirt_depth < 0.0f) {
    throw std::invalid_argument(
        "Terrain transition mesh requires ordered radii and enough segments.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(ring_vertices * (spec.rings + 3)));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * (spec.rings + 1) * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float outer_t = static_cast<float>(ring) / static_cast<float>(spec.rings);
    const float inner_t = 1.0f - outer_t;
    const float eased_inner = smoothstep(0.0f, 1.0f, inner_t);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float inner_scale = contourScale(theta + 0.58f, spec.edge_irregularity * 0.65f);
      const float outer_scale = contourScale(theta - 0.37f, spec.edge_irregularity);
      const float negative_outer_z =
          spec.outer_radius_z_negative > 0.0f ? spec.outer_radius_z_negative : spec.outer_radius_z;
      const float positive_outer_z =
          spec.outer_radius_z_positive > 0.0f ? spec.outer_radius_z_positive : spec.outer_radius_z;
      const float z_side_weight = smoothstep(-0.08f, 0.08f, std::sin(theta));
      const float directional_outer_radius_z =
          negative_outer_z * (1.0f - z_side_weight) + positive_outer_z * z_side_weight;
      const float negative_outer_x =
          spec.outer_radius_x_negative > 0.0f ? spec.outer_radius_x_negative : spec.outer_radius_x;
      const float positive_outer_x =
          spec.outer_radius_x_positive > 0.0f ? spec.outer_radius_x_positive : spec.outer_radius_x;
      const float x_side_weight = smoothstep(-0.08f, 0.08f, std::cos(theta));
      const float directional_outer_radius_x =
          negative_outer_x * (1.0f - x_side_weight) + positive_outer_x * x_side_weight;
      const float radius_x = spec.inner_radius_x * inner_scale * inner_t +
                             directional_outer_radius_x * outer_scale * outer_t;
      const float radius_z = spec.inner_radius_z * inner_scale * inner_t +
                             directional_outer_radius_z * outer_scale * outer_t;
      const float x = std::cos(theta) * radius_x;
      const float z = std::sin(theta) * radius_z;
      const float breakup = ringNoise(x * 0.44f, z * 0.44f);
      const float noise = breakup * spec.surface_noise * inner_t * (1.0f - outer_t * 0.45f);
      const float y = spec.foundation_lift + eased_inner * spec.inner_height + noise;
      mesh.vertices.push_back({{x, y, z}, {}, {u, inner_t}});
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
      accumulateNormal(mesh, a, d, b);
      accumulateNormal(mesh, d, c, b);
    }
  }

  finalizeNormals(mesh);
  appendRadialEdgeSkirt(mesh, spec.radial_segments,
                        static_cast<std::uint32_t>(spec.rings * ring_vertices),
                        spec.edge_skirt_depth);
  return mesh;
}

CpuMesh makeGrassTuftMesh(GrassTuftMeshSpec spec) {
  if (spec.blades < 1 || spec.radius <= 0.0f || spec.min_height <= 0.0f ||
      spec.max_height < spec.min_height || spec.min_width <= 0.0f ||
      spec.max_width < spec.min_width || spec.bend < 0.0f) {
    throw std::invalid_argument("Grass tuft mesh requires positive blade dimensions.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(spec.blades * 8));
  mesh.indices.reserve(static_cast<std::size_t>(spec.blades * 12));

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (int blade = 0; blade < spec.blades; ++blade) {
    const float fill = (static_cast<float>(blade) + 0.5f) / static_cast<float>(spec.blades);
    const float angle = static_cast<float>(blade) * golden_angle;
    const float radial = std::sqrt(fill) * spec.radius;
    const Vec3 root{std::cos(angle) * radial, 0.0f, std::sin(angle) * radial};
    const float height_mix = 0.5f + 0.5f * std::sin(static_cast<float>(blade) * 2.37f);
    const float width_mix = 0.5f + 0.5f * std::sin(static_cast<float>(blade) * 3.11f + 0.4f);
    const float height = spec.min_height + (spec.max_height - spec.min_height) * height_mix;
    const float width = spec.min_width + (spec.max_width - spec.min_width) * width_mix;
    const Vec3 outward = normalize(Vec3{root.x, 0.0f, root.z} + Vec3{0.001f, 0.0f, 0.0f});
    const Vec3 tangent{-std::sin(angle), 0.0f, std::cos(angle)};
    const Vec3 tip =
        root + outward * (spec.bend * (0.45f + fill * 0.55f)) + Vec3{0.0f, height, 0.0f};
    const Vec3 shoulder = root + Vec3{0.0f, height * 0.54f, 0.0f} + outward * (spec.bend * 0.28f);
    const Vec3 base_left = root - tangent * width;
    const Vec3 base_right = root + tangent * width;
    const Vec3 shoulder_left = shoulder - tangent * (width * 0.42f);
    const Vec3 shoulder_right = shoulder + tangent * (width * 0.42f);

    appendQuad(mesh, base_left, base_right, shoulder_right, shoulder_left, {0.0f, 0.0f},
               {1.0f, 0.0f}, {1.0f, 0.58f}, {0.0f, 0.58f});
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const Vec3 upper_normal = faceNormal(shoulder_left, shoulder_right, tip);
    mesh.vertices.push_back({shoulder_left, upper_normal, {0.0f, 0.58f}});
    mesh.vertices.push_back({shoulder_right, upper_normal, {1.0f, 0.58f}});
    mesh.vertices.push_back({tip, upper_normal, {0.5f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u});

    const std::uint32_t back = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({shoulder_right, upper_normal * -1.0f, {1.0f, 0.58f}});
    mesh.vertices.push_back({shoulder_left, upper_normal * -1.0f, {0.0f, 0.58f}});
    mesh.vertices.push_back({tip, upper_normal * -1.0f, {0.5f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(), {back, back + 1u, back + 2u});
  }

  return mesh;
}

CpuMesh makeFishMesh(FishMeshSpec spec) {
  if (spec.body_segments < 6 || spec.body_rings < 4 || spec.body_length <= 0.0f ||
      spec.body_height <= 0.0f || spec.body_width <= 0.0f || spec.tail_length <= 0.0f ||
      spec.fin_span <= 0.0f) {
    throw std::invalid_argument("Fish mesh requires positive dimensions and enough segments.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.body_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.body_rings + 1) * ring_vertices + 18));
  mesh.indices.reserve(static_cast<std::size_t>(spec.body_rings * spec.body_segments * 6 + 24));

  for (int ring = 0; ring <= spec.body_rings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(spec.body_rings);
    const float z = (v - 0.5f) * spec.body_length;
    const float girth = std::sin(v * kPi);
    const float nose_taper = 0.76f + 0.24f * std::sin(v * kPi * 0.72f);
    for (int segment = 0; segment <= spec.body_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.body_segments);
      const float theta = u * kPi * 2.0f;
      const Vec3 normal = normalize({std::cos(theta) / std::max(spec.body_width, 0.001f),
                                     std::sin(theta) / std::max(spec.body_height, 0.001f),
                                     0.20f * std::sin((v - 0.5f) * kPi)});
      mesh.vertices.push_back({{std::cos(theta) * spec.body_width * girth * nose_taper,
                                std::sin(theta) * spec.body_height * girth, z},
                               normal,
                               {u, v}});
    }
  }

  for (int ring = 0; ring < spec.body_rings; ++ring) {
    for (int segment = 0; segment < spec.body_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }

  const float tail_z = -spec.body_length * 0.5f;
  const Vec3 tail_root{0.0f, 0.0f, tail_z};
  appendDoubleSidedQuad(mesh, tail_root + Vec3{0.0f, spec.body_height * 0.40f, 0.0f},
                        tail_root + Vec3{0.0f, -spec.body_height * 0.40f, 0.0f},
                        tail_root + Vec3{0.0f, -spec.fin_span, -spec.tail_length},
                        tail_root + Vec3{0.0f, spec.fin_span, -spec.tail_length}, {0.0f, 0.0f},
                        {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
  appendDoubleSidedQuad(
      mesh, {-spec.body_width * 0.45f, 0.0f, 0.02f}, {-spec.body_width * 1.35f, 0.0f, -0.07f},
      {-spec.body_width * 0.52f, 0.0f, -0.12f}, {-spec.body_width * 0.22f, 0.0f, -0.04f},
      {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
  appendDoubleSidedQuad(
      mesh, {spec.body_width * 0.45f, 0.0f, 0.02f}, {spec.body_width * 0.22f, 0.0f, -0.04f},
      {spec.body_width * 0.52f, 0.0f, -0.12f}, {spec.body_width * 1.35f, 0.0f, -0.07f},
      {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
  return mesh;
}

CpuMesh makeBroadLeafPlantMesh(BroadLeafPlantMeshSpec spec) {
  if (spec.leaf_count < 1 || spec.radius <= 0.0f || spec.min_height <= 0.0f ||
      spec.max_height < spec.min_height || spec.leaf_width <= 0.0f || spec.leaf_length <= 0.0f) {
    throw std::invalid_argument("Broad leaf plant mesh requires positive leaf dimensions.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(spec.leaf_count * 12));
  mesh.indices.reserve(static_cast<std::size_t>(spec.leaf_count * 18));
  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (int leaf = 0; leaf < spec.leaf_count; ++leaf) {
    const float fill = (static_cast<float>(leaf) + 0.5f) / static_cast<float>(spec.leaf_count);
    const float angle = static_cast<float>(leaf) * golden_angle;
    const Vec3 outward{std::cos(angle), 0.0f, std::sin(angle)};
    const Vec3 side{-outward.z, 0.0f, outward.x};
    const float height = spec.min_height + (spec.max_height - spec.min_height) *
                                               (0.35f + 0.65f * std::sin(fill * kPi));
    const Vec3 root = outward * (spec.radius * 0.18f * fill);
    const Vec3 stem_top =
        root + outward * (spec.radius * (0.34f + fill * 0.28f)) + Vec3{0.0f, height * 0.62f, 0.0f};
    const Vec3 tip = stem_top + outward * spec.leaf_length + Vec3{0.0f, height * 0.28f, 0.0f};
    const Vec3 shoulder =
        stem_top + outward * (spec.leaf_length * 0.34f) + Vec3{0.0f, height * 0.08f, 0.0f};

    appendDoubleSidedQuad(mesh, root - side * 0.012f, root + side * 0.012f,
                          stem_top + side * 0.010f, stem_top - side * 0.010f, {0.0f, 0.0f},
                          {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
    appendDoubleSidedQuad(mesh, stem_top - side * spec.leaf_width,
                          stem_top + side * spec.leaf_width, shoulder + side * spec.leaf_width,
                          shoulder - side * spec.leaf_width, {0.0f, 0.0f}, {1.0f, 0.0f},
                          {1.0f, 0.62f}, {0.0f, 0.62f});
    const Vec3 normal =
        faceNormal(shoulder - side * spec.leaf_width, shoulder + side * spec.leaf_width, tip);
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({shoulder - side * spec.leaf_width, normal, {0.0f, 0.62f}});
    mesh.vertices.push_back({shoulder + side * spec.leaf_width, normal, {1.0f, 0.62f}});
    mesh.vertices.push_back({tip, normal, {0.5f, 1.0f}});
    mesh.vertices.push_back({shoulder + side * spec.leaf_width, normal * -1.0f, {1.0f, 0.62f}});
    mesh.vertices.push_back({shoulder - side * spec.leaf_width, normal * -1.0f, {0.0f, 0.62f}});
    mesh.vertices.push_back({tip, normal * -1.0f, {0.5f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(),
                        {base, base + 1u, base + 2u, base + 3u, base + 4u, base + 5u});
  }
  return mesh;
}

CpuMesh makeClimbableTreeTrunkMesh(ClimbableTreeTrunkMeshSpec spec) {
  if (spec.radial_segments < 6 || spec.height_segments < 1 || spec.radius <= 0.0f ||
      spec.height <= 0.0f || spec.root_radius < spec.radius || spec.bark_ridge < 0.0f) {
    throw std::invalid_argument("Climbable tree trunk mesh requires positive dimensions.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.height_segments + 1) * ring_vertices +
                                                 spec.radial_segments * 8));
  mesh.indices.reserve(static_cast<std::size_t>(spec.height_segments * spec.radial_segments * 6 +
                                                spec.radial_segments * 18));

  for (int y = 0; y <= spec.height_segments; ++y) {
    const float v = static_cast<float>(y) / static_cast<float>(spec.height_segments);
    const float trunk_radius = spec.radius * (1.0f - v * 0.18f);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float ridge = std::sin(theta * 5.0f + v * 8.0f) * spec.bark_ridge;
      const float radius = trunk_radius + ridge;
      const Vec3 normal = normalize({std::cos(theta), 0.10f * std::sin(v * kPi), std::sin(theta)});
      mesh.vertices.push_back(
          {{std::cos(theta) * radius, v * spec.height, std::sin(theta) * radius}, normal, {u, v}});
    }
  }

  for (int y = 0; y < spec.height_segments; ++y) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(y * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((y + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((y + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(y * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }

  for (int root = 0; root < 5; ++root) {
    const float angle = static_cast<float>(root) / 5.0f * kPi * 2.0f + 0.18f;
    const Vec3 outward{std::cos(angle), 0.0f, std::sin(angle)};
    const Vec3 side{-outward.z, 0.0f, outward.x};
    const Vec3 a = outward * (spec.radius * 0.42f) + Vec3{0.0f, 0.05f, 0.0f};
    const Vec3 b = outward * spec.root_radius + Vec3{0.0f, 0.0f, 0.0f};
    appendDoubleSidedQuad(mesh, a - side * 0.08f, a + side * 0.08f, b + side * 0.12f,
                          b - side * 0.12f, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
  }
  return mesh;
}

CpuMesh makeTreeCanopyMesh(TreeCanopyMeshSpec spec) {
  if (spec.leaf_clusters < 1 || spec.radius_x <= 0.0f || spec.radius_y <= 0.0f ||
      spec.radius_z <= 0.0f) {
    throw std::invalid_argument("Tree canopy mesh requires positive dimensions.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(spec.leaf_clusters * 8));
  mesh.indices.reserve(static_cast<std::size_t>(spec.leaf_clusters * 12));
  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (int leaf = 0; leaf < spec.leaf_clusters; ++leaf) {
    const float fill = (static_cast<float>(leaf) + 0.5f) / static_cast<float>(spec.leaf_clusters);
    const float theta = static_cast<float>(leaf) * golden_angle;
    const float phi = std::acos(1.0f - 2.0f * fill);
    const Vec3 center{std::cos(theta) * std::sin(phi) * spec.radius_x,
                      std::cos(phi) * spec.radius_y,
                      std::sin(theta) * std::sin(phi) * spec.radius_z};
    const Vec3 outward =
        normalize({center.x / spec.radius_x, center.y / spec.radius_y, center.z / spec.radius_z});
    Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, outward));
    if (length(side) <= 0.0001f) {
      side = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 up = normalize(cross(outward, side));
    const float scale = 0.18f + 0.10f * std::sin(static_cast<float>(leaf) * 1.91f);
    appendDoubleSidedQuad(mesh, center - side * scale - up * (scale * 0.50f),
                          center + side * scale - up * (scale * 0.46f),
                          center + side * (scale * 0.72f) + up * scale,
                          center - side * (scale * 0.68f) + up * (scale * 0.92f), {0.0f, 0.0f},
                          {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});
  }
  return mesh;
}

CpuMesh makeTreasureChestMesh(TreasureChestMeshSpec spec) {
  if (spec.width <= 0.0f || spec.height <= 0.0f || spec.depth <= 0.0f || spec.lid_rounding < 0.0f) {
    throw std::invalid_argument("Treasure chest mesh requires positive dimensions.");
  }

  CpuMesh mesh;
  const float x = spec.width * 0.5f;
  const float z = spec.depth * 0.5f;
  const float body_y = spec.height * 0.50f;
  const float lid_height = std::max(spec.height - body_y, spec.height * 0.18f);
  const int lid_segments = 8;

  appendQuad(mesh, {-x, 0.0f, z}, {x, 0.0f, z}, {x, body_y, z}, {-x, body_y, z}, {0.0f, 0.0f},
             {1.0f, 0.0f}, {1.0f, 0.50f}, {0.0f, 0.50f});
  appendQuad(mesh, {x, 0.0f, -z}, {-x, 0.0f, -z}, {-x, body_y, -z}, {x, body_y, -z}, {0.0f, 0.0f},
             {1.0f, 0.0f}, {1.0f, 0.50f}, {0.0f, 0.50f});
  appendQuad(mesh, {-x, 0.0f, -z}, {-x, 0.0f, z}, {-x, body_y, z}, {-x, body_y, -z}, {0.0f, 0.0f},
             {1.0f, 0.0f}, {1.0f, 0.50f}, {0.0f, 0.50f});
  appendQuad(mesh, {x, 0.0f, z}, {x, 0.0f, -z}, {x, body_y, -z}, {x, body_y, z}, {0.0f, 0.0f},
             {1.0f, 0.0f}, {1.0f, 0.50f}, {0.0f, 0.50f});
  appendQuad(mesh, {-x, 0.0f, -z}, {x, 0.0f, -z}, {x, 0.0f, z}, {-x, 0.0f, z}, {0.0f, 0.0f},
             {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f});

  std::vector<Vec3> arch;
  arch.reserve(static_cast<std::size_t>(lid_segments + 1));
  for (int i = 0; i <= lid_segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(lid_segments);
    const float theta = kPi * t;
    arch.push_back({0.0f, body_y + std::sin(theta) * lid_height, std::cos(theta) * z});
  }

  for (int i = 0; i < lid_segments; ++i) {
    const Vec3 a = arch[static_cast<std::size_t>(i)];
    const Vec3 b = arch[static_cast<std::size_t>(i + 1)];
    appendQuad(mesh, {-x, a.y, a.z}, {x, a.y, a.z}, {x, b.y, b.z}, {-x, b.y, b.z},
               {0.0f, 0.50f + static_cast<float>(i) / lid_segments * 0.50f},
               {1.0f, 0.50f + static_cast<float>(i) / lid_segments * 0.50f},
               {1.0f, 0.50f + static_cast<float>(i + 1) / lid_segments * 0.50f},
               {0.0f, 0.50f + static_cast<float>(i + 1) / lid_segments * 0.50f});
  }

  for (int i = 0; i < lid_segments; ++i) {
    const Vec3 a = arch[static_cast<std::size_t>(i)];
    const Vec3 b = arch[static_cast<std::size_t>(i + 1)];
    appendTriangle(mesh, {-x, body_y, 0.0f}, {-x, a.y, a.z}, {-x, b.y, b.z}, {0.5f, 0.5f},
                   {0.0f, 0.5f}, {0.0f, 1.0f});
    appendTriangle(mesh, {x, body_y, 0.0f}, {x, b.y, b.z}, {x, a.y, a.z}, {0.5f, 0.5f},
                   {1.0f, 1.0f}, {1.0f, 0.5f});
  }

  const float rim_y = body_y * 0.98f;
  appendQuad(mesh, {-x, rim_y, z + 0.010f}, {x, rim_y, z + 0.010f},
             {x, rim_y + spec.height * 0.075f, z + 0.010f},
             {-x, rim_y + spec.height * 0.075f, z + 0.010f}, {0.0f, 0.0f}, {1.0f, 0.0f},
             {1.0f, 1.0f}, {0.0f, 1.0f});
  appendQuad(mesh, {-x * 0.13f, body_y * 0.35f, z + 0.018f},
             {x * 0.13f, body_y * 0.35f, z + 0.018f}, {x * 0.13f, body_y * 0.88f, z + 0.018f},
             {-x * 0.13f, body_y * 0.88f, z + 0.018f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
             {0.0f, 1.0f});
  return mesh;
}

CpuMesh makeSignalSentinelMesh(SignalSentinelMeshSpec spec) {
  if (spec.radial_segments < 5 || spec.fin_count < 3 || spec.height <= 0.0f ||
      spec.body_radius <= 0.0f || spec.fin_span <= 0.0f || spec.fin_height <= 0.0f) {
    throw std::invalid_argument("Signal sentinel mesh requires positive dimensions.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(spec.radial_segments * 28 + spec.fin_count * 8));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * 48 + spec.fin_count * 12));

  const float half_height = spec.height * 0.5f;
  struct Ring {
    float y;
    float radius;
    float v;
  };
  const Ring rings[] = {
      {-half_height * 0.70f, spec.body_radius * 0.56f, 0.10f},
      {-half_height * 0.34f, spec.body_radius * 0.94f, 0.32f},
      {half_height * 0.08f, spec.body_radius * 0.62f, 0.56f},
      {half_height * 0.42f, spec.body_radius * 0.82f, 0.78f},
  };
  constexpr std::size_t ring_count = sizeof(rings) / sizeof(rings[0]);

  const auto ringPoint = [&](const Ring &ring, const int segment) {
    const float angle =
        static_cast<float>(segment) / static_cast<float>(spec.radial_segments) * kPi * 2.0f;
    const float bevel = 1.0f + 0.035f * std::sin(angle * 3.0f);
    return Vec3{std::cos(angle) * ring.radius * bevel, ring.y,
                std::sin(angle) * ring.radius * bevel};
  };

  for (std::size_t r = 0; r + 1u < ring_count; ++r) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const int next = (segment + 1) % spec.radial_segments;
      const float u0 = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float u1 = static_cast<float>(segment + 1) / static_cast<float>(spec.radial_segments);
      appendQuad(mesh, ringPoint(rings[r], segment), ringPoint(rings[r], next),
                 ringPoint(rings[r + 1u], next), ringPoint(rings[r + 1u], segment),
                 {u0, rings[r].v}, {u1, rings[r].v}, {u1, rings[r + 1u].v}, {u0, rings[r + 1u].v});
    }
  }

  const Vec3 top{0.0f, half_height, 0.0f};
  const Vec3 bottom{0.0f, -half_height, 0.0f};
  const Ring &top_ring = rings[ring_count - 1u];
  const Ring &bottom_ring = rings[0];
  for (int segment = 0; segment < spec.radial_segments; ++segment) {
    const int next = (segment + 1) % spec.radial_segments;
    appendTriangle(mesh, top, ringPoint(top_ring, segment), ringPoint(top_ring, next), {0.5f, 1.0f},
                   {0.0f, 0.82f}, {1.0f, 0.82f});
    appendTriangle(mesh, bottom, ringPoint(bottom_ring, next), ringPoint(bottom_ring, segment),
                   {0.5f, 0.0f}, {1.0f, 0.12f}, {0.0f, 0.12f});
  }

  for (int fin = 0; fin < spec.fin_count; ++fin) {
    const float angle = static_cast<float>(fin) / static_cast<float>(spec.fin_count) * kPi * 2.0f +
                        kPi / static_cast<float>(spec.fin_count);
    const Vec3 outward{std::cos(angle), 0.0f, std::sin(angle)};
    const Vec3 side{-outward.z, 0.0f, outward.x};
    const Vec3 root = outward * (spec.body_radius * 0.48f);
    const Vec3 outer = outward * (spec.body_radius + spec.fin_span);
    const float bottom_y = -spec.fin_height * 0.42f;
    const float top_y = spec.fin_height * 0.42f;
    appendDoubleSidedQuad(mesh, root + side * 0.028f + Vec3{0.0f, bottom_y, 0.0f},
                          outer + side * 0.052f + Vec3{0.0f, -spec.fin_height * 0.14f, 0.0f},
                          outer - side * 0.052f + Vec3{0.0f, top_y, 0.0f},
                          root - side * 0.028f + Vec3{0.0f, top_y * 0.74f, 0.0f}, {0.0f, 0.0f},
                          {1.0f, 0.16f}, {1.0f, 1.0f}, {0.0f, 0.84f});
  }

  return mesh;
}

CpuMesh makeBirdMesh(BirdMeshSpec spec) {
  if (spec.body_segments < 8 || spec.body_rings < 4 || spec.body_length <= 0.0f ||
      spec.body_height <= 0.0f || spec.body_width <= 0.0f || spec.head_radius <= 0.0f ||
      spec.beak_length <= 0.0f || spec.wing_span <= 0.0f || spec.wing_chord <= 0.0f ||
      spec.tail_length <= 0.0f || spec.leg_length < 0.0f || spec.crest_height < 0.0f) {
    throw std::invalid_argument("Bird mesh requires positive body, head, wing, and tail sizes.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(
      static_cast<std::size_t>((spec.body_rings + 1) * (spec.body_segments + 1) * 2 + 96));
  mesh.indices.reserve(static_cast<std::size_t>(spec.body_rings * spec.body_segments * 12 + 180));

  appendEllipsoid(mesh, {0.0f, 0.0f, 0.0f},
                  {spec.body_width, spec.body_height, spec.body_length * 0.50f}, spec.body_segments,
                  spec.body_rings, {0.0f, 0.0f}, {0.48f, 0.58f});
  appendEllipsoid(mesh, {0.0f, spec.body_height * 0.48f, spec.body_length * 0.42f},
                  {spec.head_radius * 0.92f, spec.head_radius, spec.head_radius * 1.05f},
                  std::max(spec.body_segments - 4, 8), std::max(spec.body_rings - 2, 4),
                  {0.50f, 0.0f}, {0.28f, 0.38f});

  const Vec3 beak_root{0.0f, spec.body_height * 0.50f, spec.body_length * 0.52f};
  appendTaperedCylinder(mesh, beak_root, beak_root + Vec3{0.0f, -0.012f, spec.beak_length},
                        spec.head_radius * 0.22f, 0.004f, 6, {0.79f, 0.0f}, {0.12f, 0.22f});

  const Vec3 left_root{-spec.body_width * 0.74f, spec.body_height * 0.10f,
                       spec.body_length * 0.08f};
  const Vec3 left_front{-spec.wing_span * 0.62f, spec.body_height * 0.04f,
                        spec.body_length * 0.20f};
  const Vec3 left_tip{-spec.wing_span, -spec.body_height * 0.08f, -spec.body_length * 0.03f};
  const Vec3 left_back{-spec.wing_span * 0.48f, -spec.body_height * 0.03f,
                       -spec.body_length * 0.34f};
  appendDoubleSidedQuad(mesh, left_root, left_front, left_tip, left_back, {0.0f, 0.58f},
                        {0.34f, 0.58f}, {0.48f, 1.0f}, {0.06f, 0.96f});
  appendDoubleSidedQuad(
      mesh, {-left_root.x, left_root.y, left_root.z}, {-left_back.x, left_back.y, left_back.z},
      {-left_tip.x, left_tip.y, left_tip.z}, {-left_front.x, left_front.y, left_front.z},
      {0.50f, 0.58f}, {0.92f, 0.96f}, {0.50f, 1.0f}, {0.64f, 0.58f});

  const float feather_step = spec.wing_span * 0.105f;
  for (int feather = 0; feather < 4; ++feather) {
    const float fill = static_cast<float>(feather) / 3.0f;
    const float inset = feather_step * static_cast<float>(feather);
    const Vec3 left_a = left_back + Vec3{inset * 0.65f, -0.006f, -fill * spec.wing_chord * 0.22f};
    const Vec3 left_b = left_tip + Vec3{inset, -0.010f, -fill * spec.wing_chord * 0.18f};
    const Vec3 left_c = left_b + Vec3{feather_step * 0.52f, -0.014f, -spec.wing_chord * 0.34f};
    appendDoubleSidedTriangle(mesh, left_a, left_b, left_c, {0.10f, 0.76f}, {0.34f, 1.0f},
                              {0.18f, 1.0f});
    appendDoubleSidedTriangle(mesh, {-left_a.x, left_a.y, left_a.z},
                              {-left_c.x, left_c.y, left_c.z}, {-left_b.x, left_b.y, left_b.z},
                              {0.90f, 0.76f}, {0.82f, 1.0f}, {0.66f, 1.0f});
  }

  const Vec3 tail_root{0.0f, -spec.body_height * 0.05f, -spec.body_length * 0.46f};
  for (int feather = -1; feather <= 1; ++feather) {
    const float spread = static_cast<float>(feather) * spec.body_width * 0.46f;
    const Vec3 root_a{spread - spec.body_width * 0.12f, tail_root.y, tail_root.z};
    const Vec3 root_b{spread + spec.body_width * 0.12f, tail_root.y, tail_root.z};
    const Vec3 tip{spread * 1.42f, tail_root.y - spec.body_height * 0.10f,
                   tail_root.z - spec.tail_length};
    appendDoubleSidedTriangle(mesh, root_a, root_b, tip, {0.42f, 0.58f}, {0.58f, 0.58f},
                              {0.50f, 0.92f});
  }

  if (spec.crest_height > 0.001f) {
    const Vec3 crest_base{0.0f, spec.body_height * 0.61f + spec.head_radius * 0.52f,
                          spec.body_length * 0.38f};
    appendDoubleSidedTriangle(mesh, crest_base + Vec3{-spec.head_radius * 0.18f, 0.0f, 0.0f},
                              crest_base + Vec3{spec.head_radius * 0.18f, 0.0f, 0.0f},
                              crest_base + Vec3{0.0f, spec.crest_height, -spec.head_radius * 0.18f},
                              {0.66f, 0.38f}, {0.76f, 0.38f}, {0.71f, 0.56f});
  }

  if (spec.leg_length > 0.001f) {
    for (const float side : {-1.0f, 1.0f}) {
      const Vec3 hip{side * spec.body_width * 0.34f, -spec.body_height * 0.72f,
                     spec.body_length * 0.02f};
      appendTaperedCylinder(mesh, hip, hip + Vec3{0.0f, -spec.leg_length, 0.0f},
                            spec.body_width * 0.055f, spec.body_width * 0.044f, 5, {0.86f, 0.34f},
                            {0.08f, 0.18f});
    }
  }

  return mesh;
}

CpuMesh makeBirdNestMesh(BirdNestMeshSpec spec) {
  if (spec.twig_count < 6 || spec.radial_segments < 4 || spec.outer_radius <= 0.0f ||
      spec.inner_radius <= 0.0f || spec.inner_radius >= spec.outer_radius || spec.depth < 0.0f ||
      spec.twig_radius <= 0.0f || spec.height <= 0.0f) {
    throw std::invalid_argument("Bird nest mesh requires ordered radii and enough twigs.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(spec.twig_count * (spec.radial_segments + 1) * 2));
  mesh.indices.reserve(static_cast<std::size_t>(spec.twig_count * spec.radial_segments * 6));

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (int twig = 0; twig < spec.twig_count; ++twig) {
    const float fill = (static_cast<float>(twig) + 0.5f) / static_cast<float>(spec.twig_count);
    const float angle = static_cast<float>(twig) * golden_angle;
    const float ring_noise =
        fractalNoise({std::cos(angle) * 2.1f, std::sin(angle) * 2.1f, fill * 3.3f}, spec.seed,
                     4) *
            2.0f -
        1.0f;
    const float radius = spec.inner_radius + (spec.outer_radius - spec.inner_radius) *
                                                 (0.28f + 0.72f * std::sqrt(fill));
    const float arc =
        0.46f + 0.20f *
                    (fractalNoise({fill * 4.1f, 2.0f, 0.3f}, spec.seed + 17u, 4) * 2.0f -
                     1.0f);
    const float lift = spec.height * (0.12f + fill * 0.76f);
    const Vec3 from{std::cos(angle - arc) * radius,
                    lift - spec.depth * (0.35f + 0.24f * ring_noise),
                    std::sin(angle - arc) * radius};
    const Vec3 to{std::cos(angle + arc) * (radius + ring_noise * spec.twig_radius * 3.0f),
                  lift - spec.depth * (0.28f - 0.18f * ring_noise),
                  std::sin(angle + arc) * (radius - ring_noise * spec.twig_radius * 2.0f)};
    const float taper =
        0.58f + 0.30f *
                    (fractalNoise({fill * 5.0f, 1.7f, 8.1f}, spec.seed + 31u, 4) * 2.0f -
                     1.0f);
    appendTaperedCylinder(mesh, from, to, spec.twig_radius * (0.82f + fill * 0.42f),
                          spec.twig_radius * std::max(taper, 0.24f), spec.radial_segments,
                          {0.0f, fill}, {1.0f, 0.04f});
  }

  for (int spoke = 0; spoke < std::max(spec.twig_count / 4, 4); ++spoke) {
    const float angle = static_cast<float>(spoke) /
                        static_cast<float>(std::max(spec.twig_count / 4, 4)) * kPi * 2.0f;
    const float inner = spec.inner_radius * (0.54f + 0.10f * std::sin(angle * 3.0f));
    const float outer = spec.outer_radius * (0.90f + 0.06f * std::cos(angle * 2.0f));
    appendTaperedCylinder(
        mesh, {std::cos(angle) * inner, spec.height * 0.08f, std::sin(angle) * inner},
        {std::cos(angle + 0.16f) * outer, spec.height * 0.42f, std::sin(angle + 0.16f) * outer},
        spec.twig_radius * 0.74f, spec.twig_radius * 0.48f, spec.radial_segments, {0.0f, 0.0f},
        {1.0f, 0.08f});
  }

  return mesh;
}

CpuMesh makeAmphibiousPredatorMesh(AmphibiousPredatorMeshSpec spec) {
  if (spec.body_segments < 10 || spec.body_rings < 5 || spec.radial_segments < 5 ||
      spec.scute_count < 1 || spec.body_length <= 0.0f || spec.body_width <= 0.0f ||
      spec.body_height <= 0.0f || spec.head_length <= 0.0f || spec.head_width <= 0.0f ||
      spec.head_height <= 0.0f || spec.snout_length <= 0.0f || spec.snout_width <= 0.0f ||
      spec.snout_height <= 0.0f || spec.tail_length <= 0.0f || spec.tail_tip_width <= 0.0f ||
      spec.leg_length < 0.0f) {
    throw std::invalid_argument(
        "Amphibious predator mesh requires positive body, head, snout, and tail dimensions.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(
      (spec.body_rings + 1) * (spec.body_segments + 1) * 3 + spec.scute_count * 12 + 160));
  mesh.indices.reserve(static_cast<std::size_t>(spec.body_rings * spec.body_segments * 18 +
                                                spec.scute_count * 12 + 240));

  appendEllipsoid(mesh, {0.0f, 0.0f, -spec.body_length * 0.04f},
                  {spec.body_width, spec.body_height, spec.body_length * 0.50f}, spec.body_segments,
                  spec.body_rings, {0.0f, 0.0f}, {0.38f, 0.50f});
  appendEllipsoid(mesh, {0.0f, spec.body_height * 0.08f, spec.body_length * 0.50f},
                  {spec.head_width, spec.head_height, spec.head_length * 0.50f},
                  std::max(spec.body_segments - 6, 10), std::max(spec.body_rings - 3, 5),
                  {0.40f, 0.0f}, {0.24f, 0.30f});
  appendEllipsoid(mesh,
                  {0.0f, spec.body_height * 0.035f,
                   spec.body_length * 0.50f + spec.head_length * 0.42f + spec.snout_length * 0.45f},
                  {spec.snout_width, spec.snout_height, spec.snout_length * 0.52f},
                  std::max(spec.body_segments - 8, 8), std::max(spec.body_rings - 4, 4),
                  {0.64f, 0.0f}, {0.24f, 0.24f});

  const Vec3 tail_root{0.0f, -spec.body_height * 0.02f, -spec.body_length * 0.50f};
  const Vec3 tail_mid{0.0f, -spec.body_height * 0.05f,
                      -spec.body_length * 0.50f - spec.tail_length * 0.54f};
  const Vec3 tail_tip{0.0f, -spec.body_height * 0.02f,
                      -spec.body_length * 0.50f - spec.tail_length};
  appendTaperedCylinder(mesh, tail_root, tail_mid, spec.body_width * 0.58f, spec.body_width * 0.26f,
                        spec.radial_segments, {0.0f, 0.52f}, {0.20f, 0.22f});
  appendTaperedCylinder(mesh, tail_mid, tail_tip, spec.body_width * 0.26f, spec.tail_tip_width,
                        spec.radial_segments, {0.20f, 0.52f}, {0.20f, 0.22f});

  for (int leg = 0; leg < 4; ++leg) {
    const float side = leg < 2 ? -1.0f : 1.0f;
    const float fore = (leg % 2) == 0 ? 0.30f : -0.30f;
    const Vec3 shoulder{side * spec.body_width * 0.72f, -spec.body_height * 0.28f,
                        fore * spec.body_length};
    const Vec3 wrist{side * (spec.body_width * 0.72f + spec.leg_length), -spec.body_height * 0.40f,
                     fore * spec.body_length - 0.035f};
    appendTaperedCylinder(mesh, shoulder, wrist, spec.body_width * 0.095f, spec.body_width * 0.060f,
                          spec.radial_segments, {0.74f, 0.28f}, {0.12f, 0.16f});
    appendEllipsoid(mesh,
                    wrist + Vec3{side * spec.body_width * 0.06f, -spec.body_height * 0.02f, 0.035f},
                    {spec.body_width * 0.12f, spec.body_height * 0.11f, spec.body_width * 0.19f},
                    std::max(spec.radial_segments, 8), 4, {0.86f, 0.28f}, {0.10f, 0.12f});
  }

  const float scute_step = spec.body_length / static_cast<float>(std::max(spec.scute_count - 1, 1));
  for (int scute = 0; scute < spec.scute_count; ++scute) {
    const float fill =
        static_cast<float>(scute) / static_cast<float>(std::max(spec.scute_count - 1, 1));
    const float z = -spec.body_length * 0.46f + scute_step * static_cast<float>(scute);
    const float taper = std::sin(fill * kPi);
    const float half_width = spec.body_width * (0.13f + taper * 0.11f);
    const float root_y = spec.body_height * (0.86f + taper * 0.08f);
    const float plate_height = spec.body_height * (0.055f + taper * 0.035f);
    const float plate_length =
        std::max(scute_step * (0.24f + taper * 0.08f), spec.body_length * 0.018f);
    appendEllipsoid(mesh, {0.0f, root_y, z}, {half_width, plate_height, plate_length},
                    std::max(spec.radial_segments, 8), 4, {0.22f, fill}, {0.14f, 0.045f});
    for (const float side : {-1.0f, 1.0f}) {
      appendEllipsoid(
          mesh,
          {side * half_width * 1.05f, root_y - plate_height * 0.28f, z - plate_length * 0.18f},
          {half_width * 0.38f, plate_height * 0.58f, plate_length * 0.72f},
          std::max(spec.radial_segments - 2, 6), 3, {0.36f, fill}, {0.08f, 0.040f});
    }
  }

  return mesh;
}

CpuMesh makePathRibbonMesh(PathRibbonMeshSpec spec) {
  if (spec.segments < 2 || spec.width <= 0.0f || spec.width_variation < 0.0f ||
      spec.crown_height < 0.0f || spec.surface_noise < 0.0f || spec.endpoint_taper < 0.0f ||
      spec.endpoint_width_floor < 0.0f || spec.endpoint_width_floor > 1.0f) {
    throw std::invalid_argument("Path ribbon mesh requires a positive width and enough segments.");
  }

  CpuMesh mesh;
  constexpr int kAcross = 7;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.segments + 1) * kAcross));
  mesh.indices.reserve(static_cast<std::size_t>(spec.segments * (kAcross - 1) * 6));

  for (int i = 0; i <= spec.segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(spec.segments);
    const Vec3 center = evaluatePathRibbonCenter(spec, t);
    const float endpoint_weight = pathEndpointWeight(t, spec.endpoint_taper);
    const float terminal_width_weight =
        spec.endpoint_taper > 0.0f
            ? spec.endpoint_width_floor + (1.0f - spec.endpoint_width_floor) * endpoint_weight
            : 1.0f;
    const float terminal_surface_weight =
        spec.endpoint_taper > 0.0f ? 0.42f + endpoint_weight * 0.58f : 1.0f;
    Vec3 tangent = evaluatePathRibbonTangent(spec, t);
    if (length(tangent) <= 0.0001f) {
      tangent = {0.0f, 0.0f, 1.0f};
    }
    Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, tangent));
    if (length(side) <= 0.0001f) {
      side = {1.0f, 0.0f, 0.0f};
    }

    for (int across = 0; across < kAcross; ++across) {
      const float u = static_cast<float>(across) / static_cast<float>(kAcross - 1);
      const float width_scale = std::max(
          0.45f, 1.0f + ringNoise(center.x * 0.31f + t, center.z * 0.27f) * spec.width_variation);
      const float edge_breakup =
          ringNoise(center.x + u * 3.0f, center.z - t * 2.0f) * spec.width_variation * 0.18f;
      const float signed_width =
          ((u - 0.5f) * spec.width * width_scale + edge_breakup) * terminal_width_weight;
      const float center_weight = 1.0f - std::abs(u - 0.5f) * 2.0f;
      const float noise = ringNoise(center.x + signed_width * 2.3f, center.z + t * 1.7f) *
                          spec.surface_noise * terminal_surface_weight;
      const Vec3 position =
          center + side * signed_width +
          Vec3{0.0f, (center_weight * spec.crown_height + noise) * terminal_surface_weight, 0.0f};
      mesh.vertices.push_back({position, {}, {u, t}});
    }
  }

  for (int i = 0; i < spec.segments; ++i) {
    for (int across = 0; across < kAcross - 1; ++across) {
      const std::uint32_t a = static_cast<std::uint32_t>(i * kAcross + across);
      const std::uint32_t b = static_cast<std::uint32_t>((i + 1) * kAcross + across);
      const std::uint32_t c = static_cast<std::uint32_t>((i + 1) * kAcross + across + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(i * kAcross + across + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
      accumulateNormal(mesh, a, b, d);
      accumulateNormal(mesh, b, c, d);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

CpuMesh makePathShoulderMesh(PathShoulderMeshSpec spec) {
  if (spec.path.segments < 2 || spec.path.width <= 0.0f || spec.path.endpoint_taper < 0.0f ||
      spec.side_segments < 1 || spec.shoulder_width <= 0.0f || spec.shoulder_height < 0.0f ||
      spec.outer_drop < 0.0f || spec.surface_noise < 0.0f) {
    throw std::invalid_argument(
        "Path shoulder mesh requires positive path and shoulder dimensions.");
  }

  CpuMesh mesh;
  const int across_count = spec.side_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.path.segments + 1) * across_count * 2));
  mesh.indices.reserve(static_cast<std::size_t>(spec.path.segments * spec.side_segments * 12));

  const auto appendSide = [&](const float side_sign) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int i = 0; i <= spec.path.segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(spec.path.segments);
      const Vec3 center = evaluatePathRibbonCenter(spec.path, t);
      const float endpoint_weight = pathEndpointWeight(t, spec.path.endpoint_taper);
      Vec3 tangent = evaluatePathRibbonTangent(spec.path, t);
      if (length(tangent) <= 0.0001f) {
        tangent = {0.0f, 0.0f, 1.0f};
      }
      Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, tangent));
      if (length(side) <= 0.0001f) {
        side = {1.0f, 0.0f, 0.0f};
      }

      for (int across = 0; across <= spec.side_segments; ++across) {
        const float u = static_cast<float>(across) / static_cast<float>(spec.side_segments);
        const float edge_distance = spec.path.width * 0.5f + u * spec.shoulder_width;
        const float ridge_profile = std::sin(u * kPi);
        const float breakup =
            ringNoise(center.x * 0.74f + side_sign * u * 2.6f, center.z * 0.66f + t * 3.1f);
        const float local_height = (ridge_profile * spec.shoulder_height - u * spec.outer_drop +
                                    breakup * spec.surface_noise) *
                                   endpoint_weight;
        const Vec3 position = {center.x, 0.0f, center.z};
        mesh.vertices.push_back(
            {position + side * (side_sign * edge_distance) + Vec3{0.0f, local_height, 0.0f},
             {},
             {u, t}});
      }
    }

    for (int i = 0; i < spec.path.segments; ++i) {
      for (int across = 0; across < spec.side_segments; ++across) {
        const std::uint32_t a = base + static_cast<std::uint32_t>(i * across_count + across);
        const std::uint32_t b = base + static_cast<std::uint32_t>((i + 1) * across_count + across);
        const std::uint32_t c =
            base + static_cast<std::uint32_t>((i + 1) * across_count + across + 1);
        const std::uint32_t d = base + static_cast<std::uint32_t>(i * across_count + across + 1);
        if (side_sign < 0.0f) {
          mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
          accumulateNormal(mesh, a, b, d);
          accumulateNormal(mesh, b, c, d);
        } else {
          mesh.indices.insert(mesh.indices.end(), {a, d, b, b, d, c});
          accumulateNormal(mesh, a, d, b);
          accumulateNormal(mesh, b, d, c);
        }
      }
    }
  };

  appendSide(-1.0f);
  appendSide(1.0f);
  finalizeNormals(mesh);
  return mesh;
}

CpuMesh makePathRouteRibbonMesh(PathRibbonRouteMeshSpec spec) {
  if (spec.segments.empty()) {
    throw std::invalid_argument("Path route ribbon mesh requires at least one segment.");
  }

  CpuMesh mesh;
  for (const PathRibbonMeshSpec &segment : spec.segments) {
    appendMesh(mesh, makePathRibbonMesh(segment));
  }
  return mesh;
}

CpuMesh makePathRouteShoulderMesh(PathShoulderRouteMeshSpec spec) {
  if (spec.segments.empty()) {
    throw std::invalid_argument("Path route shoulder mesh requires at least one segment.");
  }

  CpuMesh mesh;
  for (const PathShoulderMeshSpec &segment : spec.segments) {
    appendMesh(mesh, makePathShoulderMesh(segment));
  }
  return mesh;
}

CpuMesh makePathJunctionMesh(PathJunctionMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 2 || spec.radius_x <= 0.0f ||
      spec.radius_z <= 0.0f || spec.crown_height < 0.0f || spec.surface_noise < 0.0f ||
      spec.edge_irregularity < 0.0f || spec.radius_x_negative < 0.0f ||
      spec.radius_x_positive < 0.0f || spec.radius_z_negative < 0.0f ||
      spec.radius_z_positive < 0.0f || spec.edge_skirt_depth < 0.0f) {
    throw std::invalid_argument("Path junction mesh requires positive radii and enough segments.");
  }

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>((spec.rings + 3) * ring_vertices));
  mesh.indices.reserve(static_cast<std::size_t>((spec.rings + 1) * spec.radial_segments * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float r = static_cast<float>(ring) / static_cast<float>(spec.rings);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float scale = contourScale(theta + 0.84f, spec.edge_irregularity);
      const float cos_theta = std::cos(theta);
      const float sin_theta = std::sin(theta);
      const float radius_x = directionalRadius(spec.radius_x, spec.radius_x_negative,
                                               spec.radius_x_positive, cos_theta);
      const float radius_z = directionalRadius(spec.radius_z, spec.radius_z_negative,
                                               spec.radius_z_positive, sin_theta);
      const float x = cos_theta * radius_x * scale * r;
      const float z = sin_theta * radius_z * scale * r;
      const float crown = std::pow(std::max(1.0f - r * r, 0.0f), 0.72f) * spec.crown_height;
      const float feather = smoothstep(0.04f, 0.42f, 1.0f - r);
      const float noise = ringNoise(x * 1.7f, z * 1.7f) * spec.surface_noise * feather;
      const float uv_radius_x =
          std::max({spec.radius_x, spec.radius_x_negative, spec.radius_x_positive, 0.001f});
      mesh.vertices.push_back(
          {{x, crown + noise, z}, {}, {x / (uv_radius_x * 2.0f) + 0.5f, 1.0f - r}});
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
      accumulateNormal(mesh, a, d, b);
      accumulateNormal(mesh, d, c, b);
    }
  }

  finalizeNormals(mesh);
  appendRadialEdgeSkirt(mesh, spec.radial_segments,
                        static_cast<std::uint32_t>(spec.rings * ring_vertices),
                        spec.edge_skirt_depth);
  return mesh;
}

} // namespace aster
