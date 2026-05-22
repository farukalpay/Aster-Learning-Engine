// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/geometry/traversable_manifold.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;

struct RingVertex {
  Vertex vertex{};
  TraversableManifoldVertexRole role = TraversableManifoldVertexRole::Floor;
  bool support = false;
  bool collision = false;
  float lateral = 0.0f;
};

[[nodiscard]] Vec3 normalizedOr(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > kEpsilon ? normalized : fallback;
}

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(std::abs(edge1 - edge0), kEpsilon);
  const float t = clamp((value - edge0) / range, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] float floorOffsetAt(const TraversableManifoldStation &station,
                                  const float normalized_lateral) {
  const float edge = std::abs(normalized_lateral);
  const float crown = (1.0f - edge) * std::max(station.floor_crown, 0.0f);
  const float edge_raise =
      smoothstep(0.66f, 1.0f, edge) * std::max(station.floor_edge_raise, 0.0f);
  return crown + edge_raise;
}

[[nodiscard]] TraversableManifoldStation sanitizeStation(TraversableManifoldStation station) {
  station.tangent = normalizedOr(station.tangent, {0.0f, 0.0f, -1.0f});
  station.side = normalizedOr(station.side, normalizedOr(cross({0.0f, 1.0f, 0.0f}, station.tangent),
                                                         {1.0f, 0.0f, 0.0f}));
  station.up = normalizedOr(station.up, normalizedOr(cross(station.tangent, station.side),
                                                     {0.0f, 1.0f, 0.0f}));
  station.floor_half_width = std::max(station.floor_half_width, 0.05f);
  station.wall_half_width = std::max(station.wall_half_width, station.floor_half_width + 0.08f);
  station.height = std::max(station.height, 0.20f);
  return station;
}

[[nodiscard]] std::vector<RingVertex>
makeRing(const TraversableManifoldStation &source, const TraversableManifoldBuildSettings &settings) {
  const TraversableManifoldStation station = sanitizeStation(source);
  const int floor_columns = std::max(settings.floor_columns | 1, 3);
  const int wall_columns = std::max(settings.wall_columns, 6);
  std::vector<RingVertex> ring;
  ring.reserve(static_cast<std::size_t>(floor_columns + wall_columns));

  for (int column = 0; column < floor_columns; ++column) {
    const float fill = static_cast<float>(column) / static_cast<float>(floor_columns - 1);
    const float lateral_normalized = fill * 2.0f - 1.0f;
    const float lateral = lateral_normalized * station.floor_half_width;
    const float edge_inset = std::max(settings.support_edge_inset, 0.0f);
    const bool support =
        std::abs(lateral) <= std::max(station.floor_half_width - edge_inset, 0.01f);
    RingVertex sample;
    sample.role = TraversableManifoldVertexRole::Floor;
    sample.support = support;
    sample.collision = false;
    sample.lateral = lateral;
    sample.vertex.position =
        station.floor_center + station.side * lateral +
        station.up * floorOffsetAt(station, lateral_normalized);
    sample.vertex.normal = station.up;
    sample.vertex.uv = {fill * std::max(settings.uv_scale_u, 0.001f),
                        station.t * std::max(settings.uv_scale_v, 0.001f)};
    sample.vertex.ambient_occlusion = 0.90f;
    ring.push_back(sample);
  }

  const float shoulder_width = std::max(settings.shoulder_width, 0.02f);
  const float shoulder_lateral =
      std::min(station.floor_half_width + shoulder_width, station.wall_half_width * 0.94f);
  const float cos_limit =
      clamp(shoulder_lateral / std::max(station.wall_half_width, kEpsilon), 0.05f, 0.985f);
  const float right_angle = kPi * 2.0f - std::acos(cos_limit);
  const float left_angle = kPi + std::acos(cos_limit) + kPi * 2.0f;
  const Vec3 ring_center = station.floor_center + station.up * (station.height * 0.55f);
  const float floor_edge_y =
      floorOffsetAt(station, 1.0f) + std::max(settings.lower_wall_lift, 0.0f);

  for (int column = 0; column < wall_columns; ++column) {
    const float fill = static_cast<float>(column) / static_cast<float>(wall_columns - 1);
    const float theta = right_angle + (left_angle - right_angle) * fill;
    const float wrapped_theta = std::fmod(theta, kPi * 2.0f);
    const float sin_theta = std::sin(wrapped_theta);
    const float cos_theta = std::cos(wrapped_theta);
    const float vertical_radius = station.height * (sin_theta >= 0.0f ? 0.66f : 0.55f);
    Vec3 position = ring_center + station.side * (cos_theta * station.wall_half_width) +
                    station.up * (sin_theta * vertical_radius);
    const float vertical = dot(position - station.floor_center, station.up);
    if (vertical < floor_edge_y) {
      position = position + station.up * (floor_edge_y - vertical);
    }
    const Vec3 inward = normalizedOr(station.floor_center + station.up * (station.height * 0.48f) -
                                         position,
                                     station.up);
    const float lateral = dot(position - station.floor_center, station.side);
    const float vertical_after_lift = dot(position - station.floor_center, station.up);

    RingVertex sample;
    sample.role = sin_theta > 0.78f ? TraversableManifoldVertexRole::Ceiling
                                    : (std::abs(lateral) <= station.floor_half_width + shoulder_width
                                           ? TraversableManifoldVertexRole::Shoulder
                                           : TraversableManifoldVertexRole::Wall);
    sample.support = false;
    sample.collision =
        inward.y < std::max(settings.collision_max_walkable_normal_y, 0.0f) &&
        vertical_after_lift >= floor_edge_y + std::max(settings.collision_floor_clearance, 0.0f);
    sample.lateral = lateral;
    sample.vertex.position = position;
    sample.vertex.normal = inward;
    sample.vertex.uv = {fill * std::max(settings.uv_scale_u, 0.001f),
                        station.t * std::max(settings.uv_scale_v, 0.001f)};
    sample.vertex.ambient_occlusion =
        sample.role == TraversableManifoldVertexRole::Ceiling ? 0.62f : 0.74f;
    ring.push_back(sample);
  }

  return ring;
}

void appendGridQuad(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                    const std::uint32_t c, const std::uint32_t d, const Vec3 preferred_normal) {
  appendOrientedQuadIndices(mesh, a, b, c, d, preferred_normal, QuadTriangulationMode::Beauty);
}

void appendOrientedTriangle(CpuMesh &mesh, std::uint32_t a, std::uint32_t b,
                            std::uint32_t c, const Vec3 preferred_normal) {
  const Vec3 pa = mesh.vertices[a].position;
  const Vec3 pb = mesh.vertices[b].position;
  const Vec3 pc = mesh.vertices[c].position;
  const Vec3 face = normalizedOr(cross(pb - pa, pc - pa), preferred_normal);
  if (dot(face, preferred_normal) < 0.0f) {
    std::swap(b, c);
  }
  mesh.indices.insert(mesh.indices.end(), {a, b, c});
}

void appendCap(CpuMesh &mesh, const std::vector<RingVertex> &ring,
               const std::size_t base_vertex, const Vec3 preferred_normal) {
  if (ring.size() < 4u) {
    return;
  }
  Vec3 center{};
  for (const RingVertex &sample : ring) {
    center = center + sample.vertex.position;
  }
  center = center / static_cast<float>(ring.size());
  Vertex cap_center;
  cap_center.position = center;
  cap_center.normal = preferred_normal;
  cap_center.uv = {0.5f, 0.5f};
  cap_center.ambient_occlusion = 0.72f;
  const std::uint32_t center_index = appendMeshVertex(mesh, cap_center);
  for (std::size_t column = 0u; column < ring.size(); ++column) {
    const std::size_t next_column = (column + 1u) % ring.size();
    appendOrientedTriangle(mesh, center_index,
                           static_cast<std::uint32_t>(base_vertex + column),
                           static_cast<std::uint32_t>(base_vertex + next_column),
                           preferred_normal);
  }
}

[[nodiscard]] CpuMesh makeStructuralChunk(
    const std::vector<std::vector<RingVertex>> &rings, const std::size_t first,
    const std::size_t last, const bool cap_start = false, const bool cap_end = false) {
  CpuMesh mesh;
  if (rings.empty() || first >= rings.size() || last <= first) {
    return mesh;
  }
  const std::size_t ring_vertices = rings[first].size();
  if (ring_vertices < 4u) {
    return mesh;
  }
  const std::size_t ring_count = last - first + 1u;
  mesh.vertices.reserve(ring_count * ring_vertices);
  mesh.indices.reserve((ring_count - 1u) * ring_vertices * 6u);
  for (std::size_t ring = first; ring <= last; ++ring) {
    for (const RingVertex &sample : rings[ring]) {
      mesh.vertices.push_back(sample.vertex);
    }
  }
  const auto index_at = [&](const std::size_t ring, const std::size_t column) {
    return static_cast<std::uint32_t>((ring - first) * ring_vertices + column);
  };
  for (std::size_t ring = first; ring < last; ++ring) {
    for (std::size_t column = 0; column < ring_vertices; ++column) {
      const std::size_t next_column = (column + 1u) % ring_vertices;
      const Vec3 preferred =
          normalizedOr(rings[ring][column].vertex.normal + rings[ring][next_column].vertex.normal +
                           rings[ring + 1u][column].vertex.normal +
                           rings[ring + 1u][next_column].vertex.normal,
                       {0.0f, 1.0f, 0.0f});
      appendGridQuad(mesh, index_at(ring, column), index_at(ring + 1u, column),
                     index_at(ring + 1u, next_column), index_at(ring, next_column), preferred);
    }
  }
  if (cap_start) {
    const Vec3 cap_normal = normalizedOr(rings[first + 1u][0u].vertex.position -
                                             rings[first][0u].vertex.position,
                                         {0.0f, 0.0f, -1.0f});
    appendCap(mesh, rings[first], 0u, cap_normal);
  }
  if (cap_end) {
    const std::size_t local_ring = last - first;
    const Vec3 cap_normal = normalizedOr(rings[last - 1u][0u].vertex.position -
                                             rings[last][0u].vertex.position,
                                         {0.0f, 0.0f, 1.0f});
    appendCap(mesh, rings[last], local_ring * ring_vertices, cap_normal);
  }
  rebuildAngleWeightedNormals(mesh);
  return mesh;
}

[[nodiscard]] CpuMesh makeRoleMesh(const std::vector<std::vector<RingVertex>> &rings,
                                   const bool support_mesh) {
  CpuMesh mesh;
  if (rings.size() < 2u || rings.front().empty()) {
    return mesh;
  }
  const std::size_t ring_vertices = rings.front().size();
  std::vector<std::vector<std::uint32_t>> remap(
      rings.size(), std::vector<std::uint32_t>(ring_vertices, std::numeric_limits<std::uint32_t>::max()));
  for (std::size_t ring = 0; ring < rings.size(); ++ring) {
    for (std::size_t column = 0; column < ring_vertices; ++column) {
      const RingVertex &sample = rings[ring][column];
      const bool include = support_mesh ? sample.support : sample.collision;
      if (!include) {
        continue;
      }
      remap[ring][column] = appendMeshVertex(mesh, sample.vertex);
    }
  }
  for (std::size_t ring = 0; ring + 1u < rings.size(); ++ring) {
    for (std::size_t column = 0; column < ring_vertices; ++column) {
      const std::size_t next_column = (column + 1u) % ring_vertices;
      const std::uint32_t a = remap[ring][column];
      const std::uint32_t b = remap[ring + 1u][column];
      const std::uint32_t c = remap[ring + 1u][next_column];
      const std::uint32_t d = remap[ring][next_column];
      if (a == std::numeric_limits<std::uint32_t>::max() ||
          b == std::numeric_limits<std::uint32_t>::max() ||
          c == std::numeric_limits<std::uint32_t>::max() ||
          d == std::numeric_limits<std::uint32_t>::max()) {
        continue;
      }
      const Vec3 preferred =
          support_mesh ? Vec3{0.0f, 1.0f, 0.0f}
                       : normalizedOr(mesh.vertices[a].normal + mesh.vertices[b].normal +
                                          mesh.vertices[c].normal + mesh.vertices[d].normal,
                                      {0.0f, 1.0f, 0.0f});
      appendGridQuad(mesh, a, b, c, d, preferred);
    }
  }
  rebuildAngleWeightedNormals(mesh, support_mesh ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f});
  return mesh;
}

[[nodiscard]] TraversableManifoldStation interpolateStations(
    const TraversableManifoldStation &a, const TraversableManifoldStation &b, const float fill) {
  TraversableManifoldStation out;
  out.t = a.t + (b.t - a.t) * fill;
  out.floor_center = a.floor_center + (b.floor_center - a.floor_center) * fill;
  out.tangent = normalizedOr(a.tangent + (b.tangent - a.tangent) * fill, a.tangent);
  out.side = normalizedOr(a.side + (b.side - a.side) * fill, a.side);
  out.up = normalizedOr(a.up + (b.up - a.up) * fill, a.up);
  out.floor_half_width = a.floor_half_width + (b.floor_half_width - a.floor_half_width) * fill;
  out.wall_half_width = a.wall_half_width + (b.wall_half_width - a.wall_half_width) * fill;
  out.height = a.height + (b.height - a.height) * fill;
  out.floor_crown = a.floor_crown + (b.floor_crown - a.floor_crown) * fill;
  out.floor_edge_raise = a.floor_edge_raise + (b.floor_edge_raise - a.floor_edge_raise) * fill;
  return sanitizeStation(out);
}

[[nodiscard]] TraversableManifoldStation closestStationOnManifold(
    const std::vector<TraversableManifoldStation> &stations, const Vec3 position) {
  if (stations.empty()) {
    return {};
  }
  if (stations.size() == 1u) {
    return sanitizeStation(stations.front());
  }
  float best_distance_sq = std::numeric_limits<float>::infinity();
  TraversableManifoldStation best = stations.front();
  for (std::size_t i = 0; i + 1u < stations.size(); ++i) {
    const TraversableManifoldStation a = sanitizeStation(stations[i]);
    const TraversableManifoldStation b = sanitizeStation(stations[i + 1u]);
    const Vec3 ab = b.floor_center - a.floor_center;
    const float len_sq = dot(ab, ab);
    const float fill = len_sq > kEpsilon ? clamp(dot(position - a.floor_center, ab) / len_sq,
                                                 0.0f, 1.0f)
                                         : 0.0f;
    const Vec3 point = a.floor_center + ab * fill;
    const float distance_sq = dot(position - point, position - point);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best = interpolateStations(a, b, fill);
    }
  }
  return best;
}

void accumulateDiagnostics(TraversableManifold &manifold,
                           const std::vector<std::vector<RingVertex>> &rings) {
  manifold.diagnostics.station_count = manifold.stations.size();
  manifold.diagnostics.structural_vertices = manifold.structural_mesh.vertices.size();
  manifold.diagnostics.structural_triangles = manifold.structural_mesh.indices.size() / 3u;
  manifold.diagnostics.support_vertices = manifold.support_mesh.vertices.size();
  manifold.diagnostics.support_triangles = manifold.support_mesh.indices.size() / 3u;
  manifold.diagnostics.collision_vertices = manifold.collision_mesh.vertices.size();
  manifold.diagnostics.collision_triangles = manifold.collision_mesh.indices.size() / 3u;
  manifold.diagnostics.structural_open_edges =
      validateMeshTopology(manifold.structural_mesh).open_edges;
  manifold.diagnostics.support_open_edges = validateMeshTopology(manifold.support_mesh).open_edges;
  manifold.diagnostics.collision_open_edges =
      validateMeshTopology(manifold.collision_mesh).open_edges;

  float max_support_deviation = 0.0f;
  float max_floor_wall_gap = 0.0f;
  std::size_t seam_gap_samples = 0u;
  for (const std::vector<RingVertex> &ring : rings) {
    if (ring.size() < 4u) {
      continue;
    }
    std::size_t floor_count = 0u;
    while (floor_count < ring.size() &&
           ring[floor_count].role == TraversableManifoldVertexRole::Floor) {
      max_support_deviation =
          std::max(max_support_deviation,
                   std::abs(dot(ring[floor_count].vertex.normal, {0.0f, 1.0f, 0.0f}) - 1.0f));
      ++floor_count;
    }
    if (floor_count == 0u || floor_count >= ring.size()) {
      continue;
    }
    const RingVertex &left_floor = ring.front();
    const RingVertex &right_floor = ring[floor_count - 1u];
    const RingVertex &right_wall = ring[floor_count];
    const RingVertex &left_wall = ring.back();
    const float left_gap = length(left_floor.vertex.position - left_wall.vertex.position);
    const float right_gap = length(right_floor.vertex.position - right_wall.vertex.position);
    max_floor_wall_gap = std::max({max_floor_wall_gap, left_gap, right_gap});
    if (left_gap > 0.55f || right_gap > 0.55f) {
      ++seam_gap_samples;
    }
  }
  manifold.diagnostics.max_support_deviation = max_support_deviation;
  manifold.diagnostics.max_floor_wall_gap = max_floor_wall_gap;
  manifold.diagnostics.seam_gap_samples = seam_gap_samples;
}

} // namespace

TraversableManifold
buildTraversableManifold(const std::vector<TraversableManifoldStation> &stations,
                         TraversableManifoldBuildSettings settings) {
  TraversableManifold manifold;
  if (stations.size() < 2u) {
    return manifold;
  }

  manifold.stations.reserve(stations.size());
  std::vector<std::vector<RingVertex>> rings;
  rings.reserve(stations.size());
  for (TraversableManifoldStation station : stations) {
    station = sanitizeStation(station);
    manifold.stations.push_back(station);
    rings.push_back(makeRing(station, settings));
  }

  manifold.structural_mesh =
      makeStructuralChunk(rings, 0u, rings.size() - 1u, false, settings.emit_end_caps);
  manifold.support_mesh = makeRoleMesh(rings, true);
  manifold.collision_mesh = makeRoleMesh(rings, false);

  const int chunk_segments = std::max(settings.chunk_segments, 1);
  for (std::size_t first = 0u; first + 1u < rings.size();) {
    const std::size_t last =
        std::min(first + static_cast<std::size_t>(chunk_segments), rings.size() - 1u);
    manifold.structural_chunks.push_back(makeStructuralChunk(
        rings, first, last, false, settings.emit_end_caps && last == rings.size() - 1u));
    first = last;
  }

  accumulateDiagnostics(manifold, rings);
  return manifold;
}

TraversableManifoldSupportSample
sampleTraversableManifoldSupport(const TraversableManifold &manifold,
                                 TraversableManifoldSupportQuery query) {
  TraversableManifoldSupportSample sample;
  if (manifold.stations.empty()) {
    return sample;
  }
  const TraversableManifoldStation station =
      closestStationOnManifold(manifold.stations, query.position);
  const Vec3 offset = query.position - station.floor_center;
  const float lateral = dot(offset, station.side);
  const float vertical = dot(offset, station.up);
  const float lateral_abs = std::abs(lateral);
  const float actor_radius = std::max(query.actor_radius, 0.0f);
  const float walkable_half_width = std::max(station.floor_half_width - actor_radius * 0.72f,
                                             station.floor_half_width * 0.48f);
  const float lateral_normalized =
      clamp(lateral / std::max(station.floor_half_width, kEpsilon), -1.0f, 1.0f);
  const float floor_offset = floorOffsetAt(station, lateral_normalized);
  const Vec3 floor_position = station.floor_center + station.side * lateral + station.up * floor_offset;
  const float world_height = floor_position.y;
  const float ground_distance = query.position.y - world_height;
  const bool vertical_in_range =
      ground_distance <= std::max(query.max_above, 0.0f) &&
      -ground_distance <= std::max(query.max_below, 0.0f);
  const bool inside_width = lateral_abs <= station.wall_half_width + actor_radius * 0.50f;
  const bool inside_height = vertical >= -std::max(query.max_below, 0.0f) &&
                             vertical <= station.height * 1.55f + std::max(query.max_above, 0.0f);

  sample.valid = vertical_in_range && inside_width && inside_height;
  sample.inside_envelope = inside_width && inside_height;
  sample.walkable = sample.valid && lateral_abs <= walkable_half_width + actor_radius * 0.30f;
  sample.tunnel_t = station.t;
  sample.height = world_height;
  sample.ground_distance = ground_distance;
  sample.lateral = lateral;
  sample.half_width = walkable_half_width;
  sample.lateral_clearance = walkable_half_width - lateral_abs;
  sample.lateral_penetration = std::max(lateral_abs - walkable_half_width, 0.0f);
  sample.floor_position = floor_position;
  sample.tangent = station.tangent;
  sample.side = station.side;
  sample.up = station.up;
  if (sample.lateral_penetration > 0.0f) {
    const float side_sign = lateral >= 0.0f ? 1.0f : -1.0f;
    sample.obstacle_normal = station.side * -side_sign;
    sample.depenetration = station.side * (-side_sign * sample.lateral_penetration);
  }
  return sample;
}

Vec3 projectVelocityOnTraversableManifold(const TraversableManifoldSupportSample &sample,
                                          const Vec3 desired_velocity,
                                          TraversableVelocityProjectionSettings settings) {
  if (!sample.inside_envelope || length(desired_velocity) <= kEpsilon) {
    return desired_velocity;
  }
  const Vec3 horizontal{desired_velocity.x, 0.0f, desired_velocity.z};
  if (length(horizontal) <= kEpsilon) {
    return desired_velocity;
  }
  const Vec3 tangent_horizontal =
      normalizedOr({sample.tangent.x, 0.0f, sample.tangent.z}, {0.0f, 0.0f, -1.0f});
  const Vec3 side_horizontal =
      normalizedOr({sample.side.x, 0.0f, sample.side.z}, {1.0f, 0.0f, 0.0f});
  const float tangent_speed = dot(horizontal, tangent_horizontal);
  float side_speed = dot(horizontal, side_horizontal);
  const float pressure =
      std::abs(sample.lateral) / std::max(sample.half_width + std::max(sample.lateral_penetration, 0.0f),
                                          kEpsilon);
  const float slow_start = clamp(settings.edge_slow_start, 0.0f, 1.0f);
  const float edge_stop = std::max(settings.edge_stop, slow_start + 0.001f);
  if (pressure > slow_start && side_speed * sample.lateral > 0.0f) {
    const float edge_fill = clamp((pressure - slow_start) / (edge_stop - slow_start), 0.0f, 1.0f);
    const float scale = 1.0f + (std::max(settings.minimum_lateral_scale, 0.0f) - 1.0f) * edge_fill;
    side_speed *= scale;
  }
  const float tangent_allowance =
      std::max(std::abs(tangent_speed) * std::max(settings.maximum_lateral_fraction_of_forward, 0.0f),
               0.12f);
  side_speed = clamp(side_speed, -tangent_allowance, tangent_allowance);
  const Vec3 projected = tangent_horizontal * tangent_speed + side_horizontal * side_speed;
  return {projected.x, desired_velocity.y, projected.z};
}

} // namespace aster
