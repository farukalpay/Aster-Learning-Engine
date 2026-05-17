// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/implicit_surface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace aster {
namespace {

constexpr float kSurfaceEpsilon = 0.000001f;

[[nodiscard]] Vec3 normalizedOr(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > kSurfaceEpsilon ? normalized : fallback;
}

[[nodiscard]] float capsuleSdf(const Vec3 point, const Vec3 from, const Vec3 to,
                               const float radius) {
  const Vec3 axis = to - from;
  const float len_sq = dot(axis, axis);
  const float t = len_sq > kSurfaceEpsilon
                      ? clamp(dot(point - from, axis) / len_sq, 0.0f, 1.0f)
                      : 0.0f;
  return length(point - (from + axis * t)) - radius;
}

[[nodiscard]] float ellipsoidSdf(const Vec3 point, const Vec3 center, const Vec3 radii) {
  const Vec3 radius{std::max(radii.x, 0.001f), std::max(radii.y, 0.001f),
                    std::max(radii.z, 0.001f)};
  const Vec3 scaled{(point.x - center.x) / radius.x, (point.y - center.y) / radius.y,
                    (point.z - center.z) / radius.z};
  return (length(scaled) - 1.0f) * std::min({radius.x, radius.y, radius.z});
}

[[nodiscard]] SurfaceMeshBatch &meshBatchFor(std::vector<SurfaceMeshBatch> &batches,
                                             const std::uint32_t channel) {
  const auto found = std::find_if(batches.begin(), batches.end(), [channel](const auto &batch) {
    return batch.channel == channel;
  });
  if (found != batches.end()) {
    return *found;
  }
  batches.push_back({channel, {}});
  return batches.back();
}

[[nodiscard]] std::uint64_t surfaceVertexKey(const int x, const int y, const int z) {
  constexpr std::uint64_t mask = (1ull << 21u) - 1ull;
  return ((static_cast<std::uint64_t>(x + 1) & mask) << 0u) |
         ((static_cast<std::uint64_t>(y + 1) & mask) << 21u) |
         ((static_cast<std::uint64_t>(z + 1) & mask) << 42u);
}

[[nodiscard]] SurfaceFace makeSurfaceFace(const std::array<const SurfaceVertex *, 4> &corners,
                                          const int active_count) {
  SurfaceFace face;
  face.corner_count = active_count;
  for (int i = 0; i < active_count; ++i) {
    face.corners[static_cast<std::size_t>(i)] = *corners[static_cast<std::size_t>(i)];
    face.corner_keys[static_cast<std::size_t>(i)] = corners[static_cast<std::size_t>(i)]->key;
    face.preferred_normal = face.preferred_normal + corners[static_cast<std::size_t>(i)]->normal;
    face.centroid = face.centroid + corners[static_cast<std::size_t>(i)]->position;
  }
  face.preferred_normal = normalizedOr(face.preferred_normal, {0.0f, 1.0f, 0.0f});
  face.centroid = face.centroid / static_cast<float>(std::max(active_count, 1));
  return face;
}

void appendExtractedFace(SurfaceExtractionResult &result, SurfaceFace face,
                         const SurfaceExtractionSpec &spec,
                         SurfaceMeshIndexCache &collision_cache,
                         std::unordered_map<std::uint32_t, SurfaceMeshIndexCache> &channel_caches) {
  if (spec.channel) {
    face.channel = spec.channel(face, spec.cell_size);
  }
  if (spec.reject_face && spec.reject_face(face)) {
    ++result.stats.rejected_faces;
    return;
  }
  if (face.corner_count < 3) {
    ++result.stats.degenerate_faces;
    return;
  }

  const std::size_t before_indices = result.collision_mesh.indices.size();
  if (spec.settings.capture_faces) {
    result.faces.push_back(face);
  }
  if (spec.settings.emit_collision_mesh) {
    appendSurfaceFace(result.collision_mesh, face, spec.settings.uv_scale, &collision_cache);
  }
  if (spec.settings.emit_channel_meshes) {
    SurfaceMeshIndexCache &cache = channel_caches[face.channel.channel];
    appendSurfaceFace(meshBatchFor(result.mesh_batches, face.channel.channel).mesh, face,
                      spec.settings.uv_scale, &cache);
  }
  const bool emitted_collision =
      !spec.settings.emit_collision_mesh || result.collision_mesh.indices.size() > before_indices;
  if (!emitted_collision) {
    ++result.stats.degenerate_faces;
    return;
  }
  ++result.stats.faces;
  if (face.corner_count == 3) {
    ++result.stats.partial_faces;
  }
}

void finalizeStats(SurfaceExtractionResult &result) {
  result.stats.bounds = calculateMeshBounds(result.collision_mesh);
  result.stats.topology = validateMeshTopology(result.collision_mesh);
  if (!result.collision_mesh.vertices.empty() || !result.collision_mesh.indices.empty()) {
    result.stats.surface_vertices = static_cast<std::uint32_t>(result.collision_mesh.vertices.size());
    result.stats.surface_triangles =
        static_cast<std::uint32_t>(result.collision_mesh.indices.size() / 3u);
  } else {
    std::uint32_t vertices = 0u;
    std::uint32_t triangles = 0u;
    for (const SurfaceMeshBatch &batch : result.mesh_batches) {
      vertices += static_cast<std::uint32_t>(batch.mesh.vertices.size());
      triangles += static_cast<std::uint32_t>(batch.mesh.indices.size() / 3u);
    }
    result.stats.surface_vertices = vertices;
    result.stats.surface_triangles = triangles;
  }

  for (const SurfaceMeshBatch &batch : result.mesh_batches) {
    if (batch.mesh.vertices.empty() || batch.mesh.indices.empty()) {
      continue;
    }
    result.stats.batches.push_back({batch.channel,
                                    static_cast<std::uint32_t>(batch.mesh.vertices.size()),
                                    static_cast<std::uint32_t>(batch.mesh.indices.size() / 3u)});
  }
  result.stats.material_batches = static_cast<std::uint32_t>(result.stats.batches.size());
}

} // namespace

ImplicitSurfaceField::ImplicitSurfaceField(Sampler sampler) : sampler_(std::move(sampler)) {}

float ImplicitSurfaceField::sample(const Vec3 point) const {
  if (!sampler_) {
    return std::numeric_limits<float>::max();
  }
  return sampler_(point);
}

ImplicitSurfaceField::operator bool() const {
  return static_cast<bool>(sampler_);
}

ImplicitSurfaceField makeCapsuleField(const Vec3 from, const Vec3 to, const float radius) {
  return ImplicitSurfaceField([from, to, radius](const Vec3 point) {
    return capsuleSdf(point, from, to, std::max(radius, 0.0f));
  });
}

ImplicitSurfaceField makeEllipsoidField(const Vec3 center, const Vec3 radii) {
  return ImplicitSurfaceField([center, radii](const Vec3 point) {
    return ellipsoidSdf(point, center, radii);
  });
}

ImplicitSurfaceField makePointRadiusField(const Vec3 center, const float radius) {
  return ImplicitSurfaceField([center, radius](const Vec3 point) {
    return length(point - center) - std::max(radius, 0.0f);
  });
}

ImplicitSurfaceField makeOffsetField(ImplicitSurfaceField field, const float offset) {
  return ImplicitSurfaceField([field = std::move(field), offset](const Vec3 point) {
    return field.sample(point) - offset;
  });
}

ImplicitSurfaceField combineImplicitFields(std::vector<ImplicitSurfaceField> fields,
                                           const ImplicitBooleanOperation operation) {
  return ImplicitSurfaceField([fields = std::move(fields), operation](const Vec3 point) {
    if (fields.empty()) {
      return std::numeric_limits<float>::max();
    }
    float value = fields.front().sample(point);
    switch (operation) {
    case ImplicitBooleanOperation::Union:
      for (const ImplicitSurfaceField &field : fields) {
        value = std::min(value, field.sample(point));
      }
      return value;
    case ImplicitBooleanOperation::Intersection:
      for (const ImplicitSurfaceField &field : fields) {
        value = std::max(value, field.sample(point));
      }
      return value;
    case ImplicitBooleanOperation::Difference:
      for (std::size_t i = 1u; i < fields.size(); ++i) {
        value = std::max(value, -fields[i].sample(point));
      }
      return value;
    }
    return value;
  });
}

Vertex surfaceVertexForMesh(const SurfaceVertex vertex, const float uv_scale) {
  return {vertex.position, vertex.normal, {vertex.position.x * uv_scale, vertex.position.z * uv_scale}};
}

void appendSurfaceFace(CpuMesh &mesh, const SurfaceFace &face, const float uv_scale,
                       SurfaceMeshIndexCache *cache) {
  const auto appendCorner = [&](const int corner) {
    const SurfaceVertex &surface_vertex = face.corners[static_cast<std::size_t>(corner)];
    const std::uint64_t key = face.corner_keys[static_cast<std::size_t>(corner)];
    if (cache != nullptr && key != 0u) {
      const auto found = cache->vertices.find(key);
      if (found != cache->vertices.end()) {
        return found->second;
      }
    }
    const std::uint32_t index = appendMeshVertex(mesh, surfaceVertexForMesh(surface_vertex, uv_scale));
    if (cache != nullptr && key != 0u) {
      cache->vertices[key] = index;
    }
    return index;
  };

  if (face.corner_count == 3) {
    const std::uint32_t a = appendCorner(0);
    const std::uint32_t b = appendCorner(1);
    const std::uint32_t c = appendCorner(2);
    appendOrientedTriangleIndices(mesh, a, b, c, face.preferred_normal);
    return;
  }
  if (face.corner_count == 4) {
    const std::uint32_t a = appendCorner(0);
    const std::uint32_t b = appendCorner(1);
    const std::uint32_t c = appendCorner(2);
    const std::uint32_t d = appendCorner(3);
    appendOrientedQuadIndices(mesh, a, b, c, d, face.preferred_normal);
  }
}

namespace {

[[nodiscard]] float boundaryComponent(const Vec3 point, const SurfaceBoundaryAxis axis) {
  switch (axis) {
  case SurfaceBoundaryAxis::X:
    return point.x;
  case SurfaceBoundaryAxis::Y:
    return point.y;
  case SurfaceBoundaryAxis::Z:
    return point.z;
  }
  return point.x;
}

[[nodiscard]] bool sameBoundaryPoint(const Vec3 lhs, const Vec3 rhs, const float tolerance) {
  return std::abs(lhs.x - rhs.x) <= tolerance && std::abs(lhs.y - rhs.y) <= tolerance &&
         std::abs(lhs.z - rhs.z) <= tolerance;
}

[[nodiscard]] float pointDelta(const Vec3 lhs, const Vec3 rhs) {
  return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y), std::abs(lhs.z - rhs.z)});
}

} // namespace

bool SurfaceBoundaryCompatibilityReport::compatible() const {
  return unmatched_lhs_vertices == 0u && unmatched_rhs_vertices == 0u;
}

SurfaceBoundarySignature collectSurfaceBoundary(const CpuMesh &mesh, const SurfaceBoundaryAxis axis,
                                                const float value, const float tolerance) {
  SurfaceBoundarySignature signature;
  signature.axis = axis;
  signature.value = value;
  signature.tolerance = std::max(tolerance, 0.0f);

  for (const Vertex &vertex : mesh.vertices) {
    if (std::abs(boundaryComponent(vertex.position, axis) - value) > signature.tolerance) {
      continue;
    }
    const bool already_present =
        std::any_of(signature.vertices.begin(), signature.vertices.end(), [&](const Vec3 point) {
          return sameBoundaryPoint(point, vertex.position, signature.tolerance);
        });
    if (!already_present) {
      signature.vertices.push_back(vertex.position);
    }
  }

  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }
    const bool touches =
        std::abs(boundaryComponent(mesh.vertices[ia].position, axis) - value) <=
            signature.tolerance ||
        std::abs(boundaryComponent(mesh.vertices[ib].position, axis) - value) <=
            signature.tolerance ||
        std::abs(boundaryComponent(mesh.vertices[ic].position, axis) - value) <=
            signature.tolerance;
    if (touches) {
      ++signature.triangles_touching;
    }
  }

  std::sort(signature.vertices.begin(), signature.vertices.end(), [](const Vec3 lhs, const Vec3 rhs) {
    if (lhs.x != rhs.x) {
      return lhs.x < rhs.x;
    }
    if (lhs.y != rhs.y) {
      return lhs.y < rhs.y;
    }
    return lhs.z < rhs.z;
  });
  CpuMesh boundary_mesh;
  boundary_mesh.vertices.reserve(signature.vertices.size());
  for (const Vec3 point : signature.vertices) {
    boundary_mesh.vertices.push_back({point, {0.0f, 1.0f, 0.0f}, {}});
  }
  signature.bounds = calculateMeshBounds(boundary_mesh);
  return signature;
}

SurfaceBoundaryCompatibilityReport
compareSurfaceBoundaries(const SurfaceBoundarySignature &lhs, const SurfaceBoundarySignature &rhs,
                         const float tolerance) {
  const float match_tolerance =
      std::max({tolerance, lhs.tolerance, rhs.tolerance, kSurfaceEpsilon});
  SurfaceBoundaryCompatibilityReport report;
  report.lhs_vertices = lhs.vertices.size();
  report.rhs_vertices = rhs.vertices.size();

  std::vector<bool> rhs_matched(rhs.vertices.size(), false);
  for (const Vec3 lhs_point : lhs.vertices) {
    bool matched = false;
    float best_delta = std::numeric_limits<float>::infinity();
    std::size_t best_index = 0u;
    for (std::size_t rhs_index = 0; rhs_index < rhs.vertices.size(); ++rhs_index) {
      const float delta = pointDelta(lhs_point, rhs.vertices[rhs_index]);
      if (delta < best_delta) {
        best_delta = delta;
        best_index = rhs_index;
      }
    }
    if (best_delta <= match_tolerance && best_index < rhs_matched.size()) {
      rhs_matched[best_index] = true;
      matched = true;
      report.max_delta = std::max(report.max_delta, best_delta);
    }
    if (!matched) {
      ++report.unmatched_lhs_vertices;
    }
  }

  for (const bool matched : rhs_matched) {
    if (!matched) {
      ++report.unmatched_rhs_vertices;
    }
  }
  return report;
}

SurfaceExtractionResult extractImplicitSurface(const SurfaceExtractionSpec &spec) {
  if (!spec.density) {
    throw std::invalid_argument("Surface extraction requires a density sampler.");
  }
  if (spec.cell_size <= 0.0f) {
    throw std::invalid_argument("Surface extraction requires positive cell size.");
  }
  const int nx = spec.cell_count.x;
  const int ny = spec.cell_count.y;
  const int nz = spec.cell_count.z;
  if (nx <= 0 || ny <= 0 || nz <= 0) {
    throw std::invalid_argument("Surface extraction requires positive cell counts.");
  }

  SurfaceExtractionResult result;
  SurfaceMeshIndexCache collision_cache;
  std::unordered_map<std::uint32_t, SurfaceMeshIndexCache> channel_caches;
  const int cell_grid_x = nx + 1;
  const int cell_grid_y = ny + 1;
  const int cell_grid_z = nz + 1;
  const int density_grid_x = nx + 2;
  const int density_grid_y = ny + 2;
  const int density_grid_z = nz + 2;
  constexpr int cube_offsets[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
  constexpr int cube_edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                     {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

  const auto cellIndex = [cell_grid_x, cell_grid_y](const int x, const int y, const int z) {
    return static_cast<std::size_t>((z * cell_grid_y + y) * cell_grid_x + x);
  };
  const auto densityIndex = [density_grid_x, density_grid_y](const int x, const int y,
                                                             const int z) {
    return static_cast<std::size_t>((z * density_grid_y + y) * density_grid_x + x);
  };
  const auto gridPoint = [&](const int x, const int y, const int z) {
    return spec.origin + Vec3{static_cast<float>(x) * spec.cell_size,
                              static_cast<float>(y) * spec.cell_size,
                              static_cast<float>(z) * spec.cell_size};
  };

  std::vector<float> density_samples(
      static_cast<std::size_t>(density_grid_x * density_grid_y * density_grid_z), 0.0f);
  for (int z = 0; z < density_grid_z; ++z) {
    for (int y = 0; y < density_grid_y; ++y) {
      for (int x = 0; x < density_grid_x; ++x) {
        density_samples[densityIndex(x, y, z)] = spec.density(gridPoint(x, y, z));
      }
    }
  }
  const auto densitySample = [&](const int x, const int y, const int z) {
    return density_samples[densityIndex(x, y, z)];
  };
  const auto edgeCrossesSurface = [&](const int ax, const int ay, const int az, const int bx,
                                      const int by, const int bz) {
    const float da = densitySample(ax, ay, az);
    const float db = densitySample(bx, by, bz);
    return (da > 0.0f) != (db > 0.0f);
  };

  std::vector<SurfaceVertex> cells(
      static_cast<std::size_t>(cell_grid_x * cell_grid_y * cell_grid_z));
  std::vector<bool> active_cells(cells.size(), false);
  for (int z = 0; z <= nz; ++z) {
    for (int y = 0; y <= ny; ++y) {
      for (int x = 0; x <= nx; ++x) {
        std::array<Vec3, 8> p{};
        std::array<float, 8> d{};
        for (int i = 0; i < 8; ++i) {
          p[static_cast<std::size_t>(i)] =
              gridPoint(x + cube_offsets[i][0], y + cube_offsets[i][1], z + cube_offsets[i][2]);
          d[static_cast<std::size_t>(i)] =
              densitySample(x + cube_offsets[i][0], y + cube_offsets[i][1],
                            z + cube_offsets[i][2]);
        }
        const bool all_solid =
            std::all_of(d.begin(), d.end(), [](const float value) { return value > 0.0f; });
        const bool all_air =
            std::all_of(d.begin(), d.end(), [](const float value) { return value <= 0.0f; });
        if (all_solid || all_air) {
          continue;
        }

        Vec3 intersection_sum{};
        int intersection_count = 0;
        for (const auto &edge : cube_edges) {
          const int a = edge[0];
          const int b = edge[1];
          const bool solid_a = d[static_cast<std::size_t>(a)] > 0.0f;
          const bool solid_b = d[static_cast<std::size_t>(b)] > 0.0f;
          if (solid_a == solid_b) {
            continue;
          }
          const float denom = d[static_cast<std::size_t>(a)] - d[static_cast<std::size_t>(b)];
          const float t =
              std::abs(denom) > kSurfaceEpsilon
                  ? clamp(d[static_cast<std::size_t>(a)] / denom, 0.0f, 1.0f)
                  : 0.5f;
          intersection_sum =
              intersection_sum + p[static_cast<std::size_t>(a)] +
              (p[static_cast<std::size_t>(b)] - p[static_cast<std::size_t>(a)]) * t;
          ++intersection_count;
        }
        if (intersection_count <= 0) {
          continue;
        }

        SurfaceVertex cell;
        cell.position = intersection_sum / static_cast<float>(intersection_count);
        cell.grid_coord = {x, y, z};
        cell.key = surfaceVertexKey(x, y, z);
        const float e = spec.cell_size * std::max(spec.settings.normal_probe_scale, 0.01f);
        Vec3 normal{spec.density(cell.position - Vec3{e, 0.0f, 0.0f}) -
                        spec.density(cell.position + Vec3{e, 0.0f, 0.0f}),
                    spec.density(cell.position - Vec3{0.0f, e, 0.0f}) -
                        spec.density(cell.position + Vec3{0.0f, e, 0.0f}),
                    spec.density(cell.position - Vec3{0.0f, 0.0f, e}) -
                        spec.density(cell.position + Vec3{0.0f, 0.0f, e})};
        cell.normal = normalizedOr(normal, {0.0f, 1.0f, 0.0f});
        const std::size_t index = cellIndex(x, y, z);
        cells[index] = cell;
        active_cells[index] = true;
        ++result.stats.active_cells;
      }
    }
  }

  const auto activeCell = [&](const int x, const int y, const int z) -> const SurfaceVertex * {
    if (x < 0 || y < 0 || z < 0 || x > nx || y > ny || z > nz) {
      return nullptr;
    }
    const std::size_t index = cellIndex(x, y, z);
    return active_cells[index] ? &cells[index] : nullptr;
  };
  const auto appendFace = [&](const SurfaceVertex *a, const SurfaceVertex *b,
                              const SurfaceVertex *c, const SurfaceVertex *d) {
    std::array<const SurfaceVertex *, 4> corners{a, b, c, d};
    std::array<const SurfaceVertex *, 4> active{};
    int active_count = 0;
    for (const SurfaceVertex *cell : corners) {
      if (cell != nullptr) {
        active[static_cast<std::size_t>(active_count++)] = cell;
      }
    }
    if (active_count < 3) {
      return;
    }
    if (active_count < 4 && !spec.settings.emit_partial_faces) {
      ++result.stats.partial_faces;
      return;
    }
    appendExtractedFace(result, makeSurfaceFace(active, active_count), spec, collision_cache,
                        channel_caches);
  };

  for (int z = 1; z <= nz; ++z) {
    for (int y = 1; y <= ny; ++y) {
      for (int x = 0; x <= nx; ++x) {
        if (edgeCrossesSurface(x, y, z, x + 1, y, z)) {
          appendFace(activeCell(x, y - 1, z - 1), activeCell(x, y, z - 1), activeCell(x, y, z),
                     activeCell(x, y - 1, z));
        }
      }
    }
  }
  for (int z = 1; z <= nz; ++z) {
    for (int y = 0; y <= ny; ++y) {
      for (int x = 1; x <= nx; ++x) {
        if (edgeCrossesSurface(x, y, z, x, y + 1, z)) {
          appendFace(activeCell(x - 1, y, z - 1), activeCell(x - 1, y, z), activeCell(x, y, z),
                     activeCell(x, y, z - 1));
        }
      }
    }
  }
  for (int z = 0; z <= nz; ++z) {
    for (int y = 1; y <= ny; ++y) {
      for (int x = 1; x <= nx; ++x) {
        if (edgeCrossesSurface(x, y, z, x, y, z + 1)) {
          appendFace(activeCell(x - 1, y - 1, z), activeCell(x, y - 1, z), activeCell(x, y, z),
                     activeCell(x - 1, y, z));
        }
      }
    }
  }

  finalizeStats(result);
  return result;
}

} // namespace aster
