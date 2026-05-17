#include "aster/geometry/voxel_cave.hpp"

#include "aster/core/profiler.hpp"
#include "aster/geometry/mesh_modeling.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr float kEpsilon = 0.000001f;
constexpr std::uint32_t kVoxelChunkRebuildWorkClass = 1u;

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

  const auto lerp = [](const float a, const float b, const float t) { return a + (b - a) * t; };

  float values[2][2][2]{};
  for (int z = 0; z < 2; ++z) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        values[z][y][x] = hashGrid(ix + x, iy + y, iz + z, seed);
      }
    }
  }

  const float x00 = lerp(values[0][0][0], values[0][0][1], ux);
  const float x10 = lerp(values[0][1][0], values[0][1][1], ux);
  const float x01 = lerp(values[1][0][0], values[1][0][1], ux);
  const float x11 = lerp(values[1][1][0], values[1][1][1], ux);
  return lerp(lerp(x00, x10, uy), lerp(x01, x11, uy), uz);
}

float fbm3(aster::Vec3 point, const std::uint32_t seed, const int octaves = 4) {
  float amplitude = 0.55f;
  float sum = 0.0f;
  float norm = 0.0f;
  for (int i = 0; i < octaves; ++i) {
    sum += valueNoise3(point, seed + static_cast<std::uint32_t>(i) * 997u) * amplitude;
    norm += amplitude;
    point = point * 2.07f + aster::Vec3{13.1f, -7.4f, 5.6f};
    amplitude *= 0.5f;
  }
  return norm > kEpsilon ? sum / norm : 0.0f;
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(std::abs(edge1 - edge0), kEpsilon);
  const float t = aster::clamp((value - edge0) / range, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float capsuleSdf(const aster::Vec3 point, const aster::Vec3 from, const aster::Vec3 to,
                 const float radius) {
  const aster::Vec3 axis = to - from;
  const float len_sq = aster::dot(axis, axis);
  const float t =
      len_sq > kEpsilon ? aster::clamp(aster::dot(point - from, axis) / len_sq, 0.0f, 1.0f) : 0.0f;
  return aster::length(point - (from + axis * t)) - radius;
}

float ellipsoidSdf(const aster::Vec3 point, const aster::VoxelCaveChamber &chamber) {
  const aster::Vec3 radius{std::max(chamber.radius.x, 0.001f), std::max(chamber.radius.y, 0.001f),
                           std::max(chamber.radius.z, 0.001f)};
  const aster::Vec3 scaled{(point.x - chamber.center.x) / radius.x,
                           (point.y - chamber.center.y) / radius.y,
                           (point.z - chamber.center.z) / radius.z};
  const float representative = std::min({radius.x, radius.y, radius.z});
  return (aster::length(scaled) - 1.0f) * representative;
}

aster::Vec3 normalizedOr(const aster::Vec3 value, const aster::Vec3 fallback) {
  const float value_length = aster::length(value);
  if (value_length <= kEpsilon) {
    return fallback;
  }
  return value / value_length;
}

std::uint64_t chunkWorkId(const aster::VoxelChunkCoord coord) {
  return static_cast<std::uint64_t>(aster::VoxelChunkCoordHash{}(coord));
}

aster::Vec3 surfaceBoundsCenter(const aster::VoxelChunkSurfaceStats &stats,
                                const aster::Vec3 fallback) {
  if (stats.surface_vertices == 0u) {
    return fallback;
  }
  return (stats.bounds_min + stats.bounds_max) * 0.5f;
}

float surfaceBoundsRadius(const aster::VoxelChunkSurfaceStats &stats, const aster::Vec3 center) {
  if (stats.surface_vertices == 0u) {
    return 0.0f;
  }
  return aster::length(stats.bounds_max - center);
}

struct ProceduralFieldBasis {
  aster::Vec3 forward{0.0f, 0.0f, -1.0f};
  aster::Vec3 side{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
};

ProceduralFieldBasis basisFromForwardUp(const aster::Vec3 forward, const aster::Vec3 up) {
  ProceduralFieldBasis basis;
  basis.forward = normalizedOr(forward, {0.0f, 0.0f, -1.0f});
  basis.up = normalizedOr(up, {0.0f, 1.0f, 0.0f});
  basis.side = normalizedOr(aster::cross(basis.up, basis.forward), {1.0f, 0.0f, 0.0f});
  basis.up = normalizedOr(aster::cross(basis.forward, basis.side), {0.0f, 1.0f, 0.0f});
  return basis;
}

ProceduralFieldBasis proceduralBasis(const aster::VoxelCaveProceduralField &field) {
  return basisFromForwardUp(field.forward, field.up);
}

aster::Vec3 proceduralCenterAt(const aster::VoxelCaveProceduralField &field,
                               const ProceduralFieldBasis &basis, const float distance) {
  const float frequency = std::max(field.wander_frequency, 0.001f);
  const float domain = distance * frequency;
  float side = (fbm3({domain, 4.70f, -2.10f}, field.seed + 911u, 4) - 0.5f) *
               std::max(field.side_wander, 0.0f) * 2.0f;
  float vertical = (fbm3({-1.60f, domain, 8.20f}, field.seed + 1291u, 4) - 0.5f) *
                   std::max(field.vertical_wander, 0.0f) * 2.0f;
  if (field.macro_wander_frequency > 0.0f) {
    const float macro_domain = distance * std::max(field.macro_wander_frequency, 0.001f);
    side += (fbm3({macro_domain, -8.30f, 6.10f}, field.seed + 3433u, 3) - 0.5f) *
            std::max(field.macro_side_wander, 0.0f) * 2.0f;
    vertical += (fbm3({5.20f, macro_domain, -11.60f}, field.seed + 3541u, 3) - 0.5f) *
                std::max(field.macro_vertical_wander, 0.0f) * 2.0f;
  }
  return field.start + basis.forward * distance + basis.side * side + basis.up * vertical;
}

aster::Vec2 proceduralTunnelRadii(const aster::VoxelCaveProceduralField &field,
                                  const float distance) {
  const float radius_noise =
      fbm3({distance * std::max(field.wander_frequency, 0.001f) + 17.0f, 2.3f, -5.1f},
           field.seed + 1777u, 3);
  const float radius_variation = std::max(field.radius_variation, 0.0f);
  return {std::max(field.tunnel_radius * (1.0f + (radius_noise - 0.5f) * radius_variation * 2.0f),
                   0.12f),
          std::max(field.vertical_radius, 0.12f)};
}

float proceduralFrameDerivativeStep(const aster::VoxelCaveProceduralField &field) {
  if (field.frame_derivative_step > 0.0f) {
    return field.frame_derivative_step;
  }
  return std::max(std::max(field.tunnel_radius, field.vertical_radius) * 0.70f, 0.25f);
}

float proceduralPathSampleSpacing(const aster::VoxelCaveProceduralField &field) {
  if (field.path_sample_spacing > 0.0f) {
    return field.path_sample_spacing;
  }
  return std::max(std::max(field.tunnel_radius, field.vertical_radius) * 0.85f, 0.30f);
}

ProceduralFieldBasis proceduralBasisAt(const aster::VoxelCaveProceduralField &field,
                                       const ProceduralFieldBasis &global_basis,
                                       const float distance) {
  const float step = proceduralFrameDerivativeStep(field);
  const float before_distance = std::max(distance - step, 0.0f);
  const float after_distance = distance + step;
  const aster::Vec3 before = proceduralCenterAt(field, global_basis, before_distance);
  const aster::Vec3 after = proceduralCenterAt(field, global_basis, after_distance);

  ProceduralFieldBasis local;
  local.forward = normalizedOr(after - before, global_basis.forward);
  local.up = field.up - local.forward * aster::dot(field.up, local.forward);
  local.up = normalizedOr(local.up, global_basis.up);
  local.side = normalizedOr(aster::cross(local.up, local.forward), global_basis.side);
  local.up = normalizedOr(aster::cross(local.forward, local.side), global_basis.up);
  return local;
}

float proceduralTunnelSdf(const aster::Vec3 point, const aster::VoxelCaveProceduralField &field,
                          const ProceduralFieldBasis &basis, const float distance) {
  const aster::Vec3 center = proceduralCenterAt(field, basis, distance);
  const ProceduralFieldBasis local_basis = proceduralBasisAt(field, basis, distance);
  const aster::Vec2 radii = proceduralTunnelRadii(field, distance);
  const aster::Vec3 offset = point - center;
  const float lateral = aster::dot(offset, local_basis.side) / radii.x;
  const float vertical = aster::dot(offset, local_basis.up) / radii.y;
  return (std::sqrt(lateral * lateral + vertical * vertical) - 1.0f) * std::min(radii.x, radii.y);
}

float proceduralChamberSdf(const aster::Vec3 point, const aster::VoxelCaveProceduralField &field,
                           const ProceduralFieldBasis &basis, const int chamber_index) {
  if (chamber_index < 0) {
    return std::numeric_limits<float>::max();
  }
  const float spacing = std::max(field.chamber_spacing, 0.001f);
  const float jitter = (hashGrid(chamber_index, 3, 11, field.seed + 2309u) * 2.0f - 1.0f) *
                       aster::clamp(field.chamber_jitter, 0.0f, 0.92f) * spacing;
  const float distance =
      std::max((static_cast<float>(chamber_index) + 0.72f) * spacing + jitter, 0.0f);
  const float radius_noise = hashGrid(chamber_index, 17, 29, field.seed + 3011u);
  const float radius_scale =
      1.0f + (radius_noise - 0.5f) * std::max(field.chamber_radius_variation, 0.0f) * 2.0f;
  const float side_radius = std::max(field.chamber_radius * radius_scale, 0.12f);
  const float up_radius = std::max(field.chamber_vertical_radius * radius_scale, 0.12f);
  const float forward_radius = std::max(side_radius * 1.24f, 0.12f);
  const aster::Vec3 center = proceduralCenterAt(field, basis, distance);
  const ProceduralFieldBasis local_basis = proceduralBasisAt(field, basis, distance);
  const aster::Vec3 offset = point - center;
  const aster::Vec3 scaled{aster::dot(offset, local_basis.side) / side_radius,
                           aster::dot(offset, local_basis.up) / up_radius,
                           aster::dot(offset, local_basis.forward) / forward_radius};
  return (aster::length(scaled) - 1.0f) * std::min({side_radius, up_radius, forward_radius});
}

float proceduralFieldSdf(const aster::Vec3 point, const aster::VoxelCaveProceduralField &field) {
  if (!field.enabled) {
    return std::numeric_limits<float>::max();
  }
  const ProceduralFieldBasis basis = proceduralBasis(field);
  const float projected = aster::dot(point - field.start, basis.forward);
  if (projected < -std::max(field.backtrack_distance, 0.0f)) {
    return std::numeric_limits<float>::max();
  }

  const float base_distance = std::max(projected, 0.0f);
  const int sample_steps = std::max(field.path_sample_steps, 0);
  const float sample_spacing = proceduralPathSampleSpacing(field);
  float density = std::numeric_limits<float>::max();
  for (int sample_offset = -sample_steps; sample_offset <= sample_steps; ++sample_offset) {
    const float candidate_distance =
        std::max(base_distance + static_cast<float>(sample_offset) * sample_spacing, 0.0f);
    density = std::min(density, proceduralTunnelSdf(point, field, basis, candidate_distance));
    if (field.chamber_spacing > 0.0f && field.chamber_radius > 0.0f) {
      const int chamber = static_cast<int>(
          std::floor(candidate_distance / std::max(field.chamber_spacing, 0.001f)));
      for (int offset = -1; offset <= 1; ++offset) {
        density = std::min(density, proceduralChamberSdf(point, field, basis, chamber + offset));
      }
    }
  }
  return density;
}

struct SolidPlugShape {
  bool valid = false;
  std::uint32_t seed = 1u;
  aster::Vec3 center{};
  ProceduralFieldBasis basis{};
  aster::VoxelCaveMaterial material = aster::VoxelCaveMaterial::Iron;
  float radius = 1.0f;
  float vertical_radius = 1.0f;
  float half_length = 1.0f;
  float edge_feather = 0.0f;
  float surface_noise = 0.0f;
};

struct SolidPlugSample {
  bool active = false;
  aster::VoxelCaveMaterial material = aster::VoxelCaveMaterial::Rock;
  float density = -std::numeric_limits<float>::infinity();
  float strength = 0.0f;
};

SolidPlugShape solidPlugShapeFrom(const aster::VoxelCaveSolidPlug &plug) {
  if (!plug.enabled || plug.material == aster::VoxelCaveMaterial::Air || plug.radius <= 0.0f ||
      plug.vertical_radius <= 0.0f || plug.half_length <= 0.0f) {
    return {};
  }
  return {.valid = true,
          .seed = plug.seed,
          .center = plug.center,
          .basis = basisFromForwardUp(plug.forward, plug.up),
          .material = plug.material,
          .radius = std::max(plug.radius, 0.001f),
          .vertical_radius = std::max(plug.vertical_radius, 0.001f),
          .half_length = std::max(plug.half_length, 0.001f),
          .edge_feather = std::max(plug.edge_feather, 0.0f),
          .surface_noise = std::max(plug.surface_noise, 0.0f)};
}

const aster::VoxelCaveProceduralField *
solidPlugPathField(const aster::VoxelCaveSpec &spec,
                   const aster::VoxelCaveProceduralSolidPlugField &field) {
  if (field.path_field_index < 0) {
    return nullptr;
  }
  const std::size_t index = static_cast<std::size_t>(field.path_field_index);
  return index < spec.procedural_fields.size() ? &spec.procedural_fields[index] : nullptr;
}

float solidPlugFieldProjectedDistance(const aster::VoxelCaveSpec &spec,
                                      const aster::VoxelCaveProceduralSolidPlugField &field,
                                      const aster::Vec3 position) {
  if (const aster::VoxelCaveProceduralField *path = solidPlugPathField(spec, field)) {
    const ProceduralFieldBasis basis = proceduralBasis(*path);
    return aster::dot(position - path->start, basis.forward);
  }
  const ProceduralFieldBasis basis = basisFromForwardUp(field.forward, field.up);
  return aster::dot(position - field.start, basis.forward);
}

SolidPlugShape proceduralSolidPlugShapeFrom(
    const aster::VoxelCaveSpec &spec, const aster::VoxelCaveProceduralSolidPlugField &field,
    const int plug_index) {
  if (!field.enabled || field.material == aster::VoxelCaveMaterial::Air || plug_index < 0 ||
      field.spacing <= 0.0f || field.half_length <= 0.0f) {
    return {};
  }

  const float distance =
      std::max(field.first_distance + static_cast<float>(plug_index) * field.spacing, 0.0f);
  ProceduralFieldBasis basis = basisFromForwardUp(field.forward, field.up);
  aster::Vec3 center = field.start + basis.forward * distance;
  float radius = std::max(field.radius, 0.001f);
  float vertical_radius = std::max(field.vertical_radius, 0.001f);

  if (const aster::VoxelCaveProceduralField *path = solidPlugPathField(spec, field)) {
    const aster::VoxelCaveProceduralFrame frame =
        aster::sampleVoxelCaveProceduralFrameAt(*path, distance);
    basis = basisFromForwardUp(frame.forward, frame.up);
    center = frame.center;
    const aster::Vec2 path_radii = proceduralTunnelRadii(*path, distance);
    radius = std::max(path_radii.x * std::max(field.radius_scale, 0.0f), 0.001f);
    vertical_radius =
        std::max(path_radii.y * std::max(field.vertical_radius_scale, 0.0f), 0.001f);
  }

  const float radius_noise = hashGrid(plug_index, 17, 5, field.seed + 401u) * 2.0f - 1.0f;
  const float length_noise = hashGrid(plug_index, 29, 11, field.seed + 503u) * 2.0f - 1.0f;
  const float side_noise = hashGrid(plug_index, 43, 19, field.seed + 607u) * 2.0f - 1.0f;
  const float up_noise = hashGrid(plug_index, 59, 23, field.seed + 709u) * 2.0f - 1.0f;
  const float radius_scale =
      1.0f + radius_noise * std::max(field.radius_variation, 0.0f);
  const float length_scale =
      1.0f + length_noise * std::max(field.length_variation, 0.0f);
  center = center + basis.side * (side_noise * std::max(field.lateral_jitter, 0.0f)) +
           basis.up * (up_noise * std::max(field.vertical_jitter, 0.0f));

  return {.valid = true,
          .seed = field.seed + static_cast<std::uint32_t>(plug_index) * 92821u,
          .center = center,
          .basis = basis,
          .material = field.material,
          .radius = std::max(radius * radius_scale, 0.001f),
          .vertical_radius = std::max(vertical_radius * radius_scale, 0.001f),
          .half_length = std::max(field.half_length * length_scale, 0.001f),
          .edge_feather = std::max(field.edge_feather, 0.0f),
          .surface_noise = std::max(field.surface_noise, 0.0f)};
}

float solidPlugSdf(const aster::Vec3 point, const SolidPlugShape &shape) {
  const aster::Vec3 offset = point - shape.center;
  const float axial = std::abs(aster::dot(offset, shape.basis.forward)) - shape.half_length;
  const float side = aster::dot(offset, shape.basis.side) / std::max(shape.radius, 0.001f);
  const float up = aster::dot(offset, shape.basis.up) / std::max(shape.vertical_radius, 0.001f);
  const float radial =
      (std::sqrt(side * side + up * up) - 1.0f) *
      std::min(std::max(shape.radius, 0.001f), std::max(shape.vertical_radius, 0.001f));
  const float outside_axial = std::max(axial, 0.0f);
  const float outside_radial = std::max(radial, 0.0f);
  float sdf = std::sqrt(outside_axial * outside_axial + outside_radial * outside_radial) +
              std::min(std::max(axial, radial), 0.0f);
  if (shape.surface_noise > 0.0f) {
    const float scale =
        std::max(std::min({shape.radius, shape.vertical_radius, shape.half_length}) * 1.65f,
                 0.001f);
    sdf += (fbm3((point - shape.center) / scale, shape.seed + 811u, 3) - 0.5f) *
           shape.surface_noise;
  }
  return sdf;
}

SolidPlugSample sampleSolidPlugShape(const aster::Vec3 position, const SolidPlugShape &shape) {
  if (!shape.valid) {
    return {};
  }
  const float sdf = solidPlugSdf(position, shape);
  if (sdf > shape.edge_feather) {
    return {};
  }
  const float edge = std::max(shape.edge_feather, 0.001f);
  return {.active = true,
          .material = shape.material,
          .density = -sdf,
          .strength = aster::clamp((-sdf + edge) / edge, 0.0f, 1.0f)};
}

void selectSolidPlugSample(SolidPlugSample &best, const SolidPlugSample candidate) {
  if (!candidate.active) {
    return;
  }
  if (!best.active || candidate.density > best.density) {
    best = candidate;
  }
}

SolidPlugSample sampleSolidPlug(const aster::VoxelCaveSpec &spec, const aster::Vec3 position) {
  SolidPlugSample sample;
  for (const aster::VoxelCaveSolidPlug &plug : spec.solid_plugs) {
    selectSolidPlugSample(sample, sampleSolidPlugShape(position, solidPlugShapeFrom(plug)));
  }
  for (const aster::VoxelCaveProceduralSolidPlugField &field :
       spec.procedural_solid_plug_fields) {
    if (!field.enabled || field.spacing <= 0.0f) {
      continue;
    }
    const float projected = solidPlugFieldProjectedDistance(spec, field, position);
    const float base = (projected - field.first_distance) / std::max(field.spacing, 0.001f);
    const int plug_index = static_cast<int>(std::floor(base));
    for (int offset = -1; offset <= 1; ++offset) {
      selectSolidPlugSample(
          sample, sampleSolidPlugShape(
                      position, proceduralSolidPlugShapeFrom(spec, field, plug_index + offset)));
    }
  }
  return sample;
}

bool insideSurfaceOpening(const aster::VoxelCaveSurfaceOpening &opening,
                          const aster::Vec3 position) {
  if (opening.radius <= 0.0f) {
    return false;
  }
  return capsuleSdf(position, opening.from, opening.to,
                    opening.radius + std::max(opening.surface_margin, 0.0f)) <= 0.0f;
}

bool insideAnySurfaceOpening(const aster::VoxelCaveSpec &spec, const aster::Vec3 position) {
  return std::any_of(spec.surface_openings.begin(), spec.surface_openings.end(),
                     [&](const aster::VoxelCaveSurfaceOpening &opening) {
                       return insideSurfaceOpening(opening, position);
                     });
}

float densityAtSpec(const aster::VoxelCaveSpec &spec, const aster::Vec3 position,
                    const std::vector<aster::VoxelEdit> *edits = nullptr) {
  float density = 1000.0f;
  for (const aster::VoxelCaveChamber &chamber : spec.chambers) {
    density = std::min(density, ellipsoidSdf(position, chamber));
  }
  for (const aster::VoxelCaveTunnel &tunnel : spec.tunnels) {
    density = std::min(density, capsuleSdf(position, tunnel.from, tunnel.to, tunnel.radius));
  }
  for (const aster::VoxelCaveSurfaceOpening &opening : spec.surface_openings) {
    if (opening.radius > 0.0f) {
      density = std::min(density, capsuleSdf(position, opening.from, opening.to, opening.radius));
    }
  }
  for (const aster::VoxelCaveProceduralField &field : spec.procedural_fields) {
    density = std::min(density, proceduralFieldSdf(position, field));
  }
  if (spec.chambers.empty() && spec.tunnels.empty() && spec.procedural_fields.empty()) {
    density =
        capsuleSdf(position, spec.origin, spec.origin + aster::Vec3{0.0f, 0.0f, -12.0f}, 1.2f);
  }
  density += (fbm3((position - spec.origin) * 0.23f, spec.seed + 53u, 3) - 0.5f) * 0.18f;
  const SolidPlugSample solid_plug = sampleSolidPlug(spec, position);
  if (solid_plug.active) {
    density = std::max(density, solid_plug.density);
  }
  if (edits != nullptr) {
    for (const aster::VoxelEdit &edit : *edits) {
      if (edit.operation == aster::VoxelEditOperation::Carve) {
        density = std::min(density, aster::length(position - edit.center) - edit.radius);
      }
    }
  }
  return density;
}

bool usesVeinField(const aster::VoxelMaterialProfile &profile) {
  return profile.material != aster::VoxelCaveMaterial::Air &&
         profile.material != aster::VoxelCaveMaterial::Rock && profile.vein_radius > 0.0f &&
         profile.vein_frequency > 0.0f;
}

float veinSignedDistance(const aster::Vec3 position, const aster::Vec3 origin,
                         const aster::VoxelMaterialProfile &profile) {
  if (!usesVeinField(profile)) {
    return std::numeric_limits<float>::max();
  }

  const aster::Vec3 local = position - origin;
  const float warp_frequency = std::max(profile.warp_frequency, 0.001f);
  const float warp_strength = std::max(profile.warp_strength, 0.0f);
  const aster::Vec3 warp{
      (fbm3(local * warp_frequency + aster::Vec3{17.0f, 0.0f, 0.0f}, profile.seed + 401u, 3) -
       0.5f) *
          warp_strength,
      (fbm3(local * warp_frequency + aster::Vec3{0.0f, 23.0f, 0.0f}, profile.seed + 503u, 3) -
       0.5f) *
          warp_strength,
      (fbm3(local * warp_frequency + aster::Vec3{0.0f, 0.0f, 29.0f}, profile.seed + 607u, 3) -
       0.5f) *
          warp_strength,
  };

  const float frequency = std::max(profile.vein_frequency, 0.001f);
  const aster::Vec3 field = (local + warp) * frequency;
  const int base_x = static_cast<int>(std::floor(field.x));
  const int base_y = static_cast<int>(std::floor(field.y));
  const int base_z = static_cast<int>(std::floor(field.z));
  const float rarity = aster::clamp(profile.rarity, 0.0f, 1.0f);
  const float radius = std::max(profile.vein_radius, 0.001f);
  float best = std::numeric_limits<float>::max();

  for (int z = -1; z <= 1; ++z) {
    for (int y = -1; y <= 1; ++y) {
      for (int x = -1; x <= 1; ++x) {
        const int cx = base_x + x;
        const int cy = base_y + y;
        const int cz = base_z + z;
        if (hashGrid(cx, cy, cz, profile.seed + 13u) < rarity) {
          continue;
        }
        const aster::Vec3 center_field{
            static_cast<float>(cx) + hashGrid(cx, cy, cz, profile.seed + 101u),
            static_cast<float>(cy) + hashGrid(cx, cy, cz, profile.seed + 211u),
            static_cast<float>(cz) + hashGrid(cx, cy, cz, profile.seed + 307u),
        };
        const float dist = aster::length((field - center_field) / frequency) - radius;
        best = std::min(best, dist);
      }
    }
  }
  return best;
}

float veinStrengthAt(const aster::Vec3 position, const aster::Vec3 origin,
                     const aster::VoxelMaterialProfile &profile) {
  const float dist = veinSignedDistance(position, origin, profile);
  if (dist > 0.0f || !std::isfinite(dist)) {
    return 0.0f;
  }
  const float radius = std::max(profile.vein_radius, 0.001f);
  return (radius - dist) / radius;
}

struct VeinMaterialSample {
  const aster::VoxelMaterialProfile *profile = nullptr;
  aster::VoxelCaveMaterial material = aster::VoxelCaveMaterial::Rock;
  float strength = 0.0f;
};

struct SurfaceNetCell {
  bool active = false;
  aster::Vec3 position{};
  aster::Vec3 normal{0.0f, 1.0f, 0.0f};
};

aster::Vertex meshVertex(const SurfaceNetCell &cell) {
  return {cell.position, cell.normal, {cell.position.x * 0.12f, cell.position.z * 0.12f}};
}

void appendSurfaceTriangle(aster::CpuMesh &mesh, const SurfaceNetCell &a, const SurfaceNetCell &b,
                           const SurfaceNetCell &c, aster::Vec3 preferred_normal) {
  aster::appendOrientedTriangle(mesh, meshVertex(a), meshVertex(b), meshVertex(c),
                                preferred_normal);
}

void appendSurfaceQuad(aster::CpuMesh &mesh, const SurfaceNetCell &a, const SurfaceNetCell &b,
                       const SurfaceNetCell &c, const SurfaceNetCell &d,
                       const aster::Vec3 preferred_normal) {
  aster::appendOrientedQuad(mesh, meshVertex(a), meshVertex(b), meshVertex(c), meshVertex(d),
                            preferred_normal);
}

aster::CpuMesh &
meshForMaterial(std::vector<std::pair<aster::VoxelCaveMaterial, aster::CpuMesh>> &meshes,
                const aster::VoxelCaveMaterial material) {
  const auto found = std::find_if(meshes.begin(), meshes.end(), [material](const auto &entry) {
    return entry.first == material;
  });
  if (found != meshes.end()) {
    return found->second;
  }
  meshes.push_back({material, {}});
  return meshes.back().second;
}

aster::SurfaceMeshIndexCache &
cacheForMaterial(std::vector<std::pair<aster::VoxelCaveMaterial, aster::SurfaceMeshIndexCache>>
                     &caches,
                 const aster::VoxelCaveMaterial material) {
  const auto found = std::find_if(caches.begin(), caches.end(), [material](const auto &entry) {
    return entry.first == material;
  });
  if (found != caches.end()) {
    return found->second;
  }
  caches.push_back({material, {}});
  return caches.back().second;
}

const aster::VoxelMaterialProfile *
profileForMaterial(const std::vector<aster::VoxelMaterialProfile> &profiles,
                   const aster::VoxelCaveMaterial material) {
  const auto found =
      std::find_if(profiles.begin(), profiles.end(),
                   [material](const auto &profile) { return profile.material == material; });
  return found == profiles.end() ? nullptr : &*found;
}

std::string materialSurfaceLabel(const std::vector<aster::VoxelMaterialProfile> &profiles,
                                 const aster::VoxelCaveMaterial material) {
  const aster::VoxelMaterialProfile *profile = profileForMaterial(profiles, material);
  if (profile != nullptr && !profile->display_name.empty()) {
    return profile->display_name + " voxel cave surface";
  }
  return "Voxel cave surface";
}

void applyMeshBoundsToStats(aster::VoxelChunkSurfaceStats &stats, const aster::CpuMesh &mesh) {
  const aster::MeshBounds bounds = aster::calculateMeshBounds(mesh);
  if (!bounds.valid) {
    return;
  }
  stats.bounds_min = bounds.min;
  stats.bounds_max = bounds.max;
}

void appendMaterialStats(aster::VoxelChunkSurfaceStats &stats,
                         const aster::VoxelCaveMaterial material, const aster::CpuMesh &mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    return;
  }
  stats.materials.push_back({material, static_cast<std::uint32_t>(mesh.vertices.size()),
                             static_cast<std::uint32_t>(mesh.indices.size() / 3u)});
}

void applyTopologyToStats(aster::VoxelChunkSurfaceStats &stats, const aster::CpuMesh &mesh) {
  const aster::MeshTopologyReport topology = aster::validateMeshTopology(mesh);
  stats.topology_invalid_indices = static_cast<std::uint32_t>(topology.invalid_indices);
  stats.topology_degenerate_triangles = static_cast<std::uint32_t>(topology.degenerate_triangles);
  stats.topology_open_edges = static_cast<std::uint32_t>(topology.open_edges);
  stats.topology_non_manifold_edges = static_cast<std::uint32_t>(topology.non_manifold_edges);
  stats.topology_inconsistent_winding_edges =
      static_cast<std::uint32_t>(topology.inconsistent_winding_edges);
}

SurfaceNetCell caveSurfaceCell(const aster::SurfaceVertex &vertex) {
  return {.active = true, .position = vertex.position, .normal = vertex.normal};
}

std::array<SurfaceNetCell, 4> caveSurfaceCorners(const aster::SurfaceFace &face) {
  return {caveSurfaceCell(face.corners[0]), caveSurfaceCell(face.corners[1]),
          caveSurfaceCell(face.corners[2]), caveSurfaceCell(face.corners[3])};
}

float surfaceExposureFor(const aster::VoxelMaterialProfile &profile, const float strength) {
  const float denominator = std::max(1.0f - profile.surface_exposure_threshold, 0.001f);
  return aster::clamp((strength - profile.surface_exposure_threshold) / denominator, 0.0f, 1.0f);
}

bool hasVisibleSurfaceExposure(const aster::VoxelMaterialProfile &profile, const float strength) {
  return profile.surface_relief > 0.0001f && surfaceExposureFor(profile, strength) > 0.0f;
}

SurfaceNetCell offsetSurfaceCell(SurfaceNetCell cell, const aster::Vec3 offset) {
  cell.position = cell.position + offset;
  return cell;
}

SurfaceNetCell lerpSurfaceCell(const SurfaceNetCell &a, const SurfaceNetCell &b,
                               const SurfaceNetCell &c, const SurfaceNetCell &d, const float u,
                               const float v) {
  const float iu = 1.0f - u;
  const float iv = 1.0f - v;
  SurfaceNetCell cell;
  cell.active = true;
  cell.position =
      a.position * (iu * iv) + b.position * (u * iv) + c.position * (u * v) + d.position * (iu * v);
  cell.normal = aster::normalize(a.normal * (iu * iv) + b.normal * (u * iv) + c.normal * (u * v) +
                                 d.normal * (iu * v));
  if (aster::length(cell.normal) <= 0.0001f) {
    cell.normal = {0.0f, 1.0f, 0.0f};
  }
  return cell;
}

void appendMaterialInterfaceQuad(aster::CpuMesh &rock_mesh,
                                 aster::CpuMesh &material_mesh, const SurfaceNetCell &a,
                                 const SurfaceNetCell &b, const SurfaceNetCell &c,
                                 const SurfaceNetCell &d, const aster::Vec3 preferred_normal,
                                 const aster::VoxelMaterialProfile &profile, const float strength) {
  const float exposure = surfaceExposureFor(profile, strength);
  if (exposure <= 0.0f || profile.surface_relief <= 0.0001f) {
    appendSurfaceQuad(rock_mesh, a, b, c, d, preferred_normal);
    return;
  }

  const float coverage = aster::clamp(profile.surface_coverage, 0.18f, 0.86f);
  const float inset = (1.0f - coverage) * 0.5f;
  const float relief = std::max(profile.surface_relief, 0.0f) * exposure;
  const aster::Vec3 shoulder_offset = preferred_normal * (relief * 0.18f);
  const aster::Vec3 rim_offset = preferred_normal * (relief * 0.38f);

  const SurfaceNetCell ra = offsetSurfaceCell(a, shoulder_offset);
  const SurfaceNetCell rb = offsetSurfaceCell(b, shoulder_offset);
  const SurfaceNetCell rc = offsetSurfaceCell(c, shoulder_offset);
  const SurfaceNetCell rd = offsetSurfaceCell(d, shoulder_offset);
  const SurfaceNetCell i0 =
      offsetSurfaceCell(lerpSurfaceCell(a, b, c, d, inset, inset), rim_offset);
  const SurfaceNetCell i1 =
      offsetSurfaceCell(lerpSurfaceCell(a, b, c, d, 1.0f - inset, inset), rim_offset);
  const SurfaceNetCell i2 =
      offsetSurfaceCell(lerpSurfaceCell(a, b, c, d, 1.0f - inset, 1.0f - inset), rim_offset);
  const SurfaceNetCell i3 =
      offsetSurfaceCell(lerpSurfaceCell(a, b, c, d, inset, 1.0f - inset), rim_offset);

  SurfaceNetCell center = lerpSurfaceCell(a, b, c, d, 0.5f, 0.5f);
  center.position = center.position + preferred_normal * relief;
  center.normal = preferred_normal;

  const auto append_rock_ring = [&](aster::CpuMesh &mesh) {
    appendSurfaceQuad(mesh, ra, rb, i1, i0, preferred_normal);
    appendSurfaceQuad(mesh, rb, rc, i2, i1, preferred_normal);
    appendSurfaceQuad(mesh, rc, rd, i3, i2, preferred_normal);
    appendSurfaceQuad(mesh, rd, ra, i0, i3, preferred_normal);
  };
  const auto append_ore_lens = [&](aster::CpuMesh &mesh) {
    appendSurfaceTriangle(mesh, i0, i1, center, preferred_normal);
    appendSurfaceTriangle(mesh, i1, i2, center, preferred_normal);
    appendSurfaceTriangle(mesh, i2, i3, center, preferred_normal);
    appendSurfaceTriangle(mesh, i3, i0, center, preferred_normal);
  };

  append_rock_ring(rock_mesh);
  append_ore_lens(material_mesh);
}

struct CaveFrameCandidate {
  bool valid = false;
  bool procedural = false;
  int procedural_field_index = -1;
  aster::Vec3 center{};
  aster::Vec3 forward{0.0f, 0.0f, -1.0f};
  aster::Vec3 side{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
  float progress_distance = 0.0f;
  float path_length = 0.0f;
  float procedural_distance = 0.0f;
  float tunnel_radius = 0.0f;
  float vertical_radius = 0.0f;
  float lateral = 0.0f;
  float vertical = 0.0f;
  float radial_fraction = std::numeric_limits<float>::infinity();
};

float authoredPathLength(const aster::VoxelCaveSpec &spec) {
  float total = 0.0f;
  for (const aster::VoxelCaveTunnel &tunnel : spec.tunnels) {
    total += aster::length(tunnel.to - tunnel.from);
  }
  return total;
}

CaveFrameCandidate makeFrameCandidate(const aster::Vec3 position, const aster::Vec3 center,
                                      const ProceduralFieldBasis &basis, const float tunnel_radius,
                                      const float vertical_radius, const float progress_distance,
                                      const float path_length, const bool procedural,
                                      const float procedural_distance = 0.0f,
                                      const int procedural_field_index = -1) {
  const float radius = std::max(tunnel_radius, 0.001f);
  const float height = std::max(vertical_radius, 0.001f);
  const aster::Vec3 offset = position - center;
  const float lateral = aster::dot(offset, basis.side);
  const float vertical = aster::dot(offset, basis.up);
  const float radial_fraction = std::sqrt((lateral * lateral) / (radius * radius) +
                                          (vertical * vertical) / (height * height));
  return {.valid = true,
          .procedural = procedural,
          .procedural_field_index = procedural_field_index,
          .center = center,
          .forward = basis.forward,
          .side = basis.side,
          .up = basis.up,
          .progress_distance = progress_distance,
          .path_length = path_length,
          .procedural_distance = procedural_distance,
          .tunnel_radius = radius,
          .vertical_radius = height,
          .lateral = lateral,
          .vertical = vertical,
          .radial_fraction = radial_fraction};
}

CaveFrameCandidate closestAuthoredFrame(const aster::VoxelCaveSpec &spec,
                                        const aster::Vec3 position) {
  CaveFrameCandidate best;
  const float total_length = authoredPathLength(spec);
  float accumulated = 0.0f;
  for (const aster::VoxelCaveTunnel &tunnel : spec.tunnels) {
    const aster::Vec3 axis = tunnel.to - tunnel.from;
    const float length_sq = aster::dot(axis, axis);
    const float segment_length = std::sqrt(std::max(length_sq, 0.0f));
    if (segment_length <= kEpsilon) {
      continue;
    }
    const float t = aster::clamp(aster::dot(position - tunnel.from, axis) / length_sq, 0.0f, 1.0f);
    const aster::Vec3 center = tunnel.from + axis * t;
    const ProceduralFieldBasis basis = basisFromForwardUp(axis, {0.0f, 1.0f, 0.0f});
    const CaveFrameCandidate candidate =
        makeFrameCandidate(position, center, basis, tunnel.radius, tunnel.radius,
                           accumulated + segment_length * t, total_length, false);
    if (!best.valid || candidate.radial_fraction < best.radial_fraction) {
      best = candidate;
    }
    accumulated += segment_length;
  }
  return best;
}

float fieldStartProgressDistance(const aster::VoxelCaveSpec &spec,
                                 const aster::VoxelCaveProceduralField &field,
                                 const float path_length) {
  const CaveFrameCandidate authored = closestAuthoredFrame(spec, field.start);
  return authored.valid ? authored.progress_distance : path_length;
}

CaveFrameCandidate closestProceduralFrame(const aster::VoxelCaveSpec &spec,
                                          const aster::Vec3 position) {
  CaveFrameCandidate best;
  const float path_length = authoredPathLength(spec);
  for (std::size_t field_index = 0; field_index < spec.procedural_fields.size(); ++field_index) {
    const aster::VoxelCaveProceduralField &field = spec.procedural_fields[field_index];
    const aster::VoxelCaveProceduralFrame frame =
        aster::closestVoxelCaveProceduralFrame(field, position);
    if (!frame.valid) {
      continue;
    }
    const ProceduralFieldBasis basis = basisFromForwardUp(frame.forward, frame.up);
    const float base_progress = fieldStartProgressDistance(spec, field, path_length);
    const CaveFrameCandidate candidate = makeFrameCandidate(
        position, frame.center, basis, frame.tunnel_radius, frame.vertical_radius,
        base_progress + frame.distance, std::max(path_length, base_progress + frame.distance), true,
        frame.distance, static_cast<int>(field_index));
    if (!best.valid || candidate.radial_fraction < best.radial_fraction) {
      best = candidate;
    }
  }
  return best;
}

float chamberWeightAt(const aster::VoxelCaveSpec &spec, const aster::Vec3 position) {
  float weight = 0.0f;
  for (const aster::VoxelCaveChamber &chamber : spec.chambers) {
    const float sdf = ellipsoidSdf(position, chamber);
    if (sdf >= 0.0f) {
      continue;
    }
    const float radius = std::max({chamber.radius.x, chamber.radius.y, chamber.radius.z, 0.001f});
    weight = std::max(weight, aster::clamp(-sdf / radius, 0.0f, 1.0f));
  }
  return weight;
}

CaveFrameCandidate selectVoxelCaveFrame(const aster::VoxelCaveSpec &spec,
                                        const aster::Vec3 position) {
  const CaveFrameCandidate authored = closestAuthoredFrame(spec, position);
  const CaveFrameCandidate procedural = closestProceduralFrame(spec, position);
  if (!authored.valid) {
    return procedural;
  }
  if (!procedural.valid) {
    return authored;
  }
  return procedural.radial_fraction <= authored.radial_fraction ? procedural : authored;
}

float fixtureSideSign(const aster::VoxelCaveFixtureSideMode mode, const int station_index) {
  switch (mode) {
  case aster::VoxelCaveFixtureSideMode::FixedNegative:
    return -1.0f;
  case aster::VoxelCaveFixtureSideMode::FixedPositive:
    return 1.0f;
  case aster::VoxelCaveFixtureSideMode::Alternating:
    return (station_index % 2 == 0) ? -1.0f : 1.0f;
  }
  return -1.0f;
}

aster::Vec3 fixtureLightColor(const aster::VoxelCaveSpec &spec, const CaveFrameCandidate &frame) {
  if (!frame.procedural) {
    return spec.authored_fixture_light_color;
  }

  aster::Vec3 color = spec.procedural_fixture_light_color;
  const float progress_distance = frame.progress_distance;
  float selected_start = -std::numeric_limits<float>::infinity();
  for (const aster::VoxelCaveFixtureLightBand &band : spec.procedural_fixture_light_bands) {
    if (progress_distance >= band.start_distance && band.start_distance >= selected_start) {
      color = band.color;
      selected_start = band.start_distance;
    }
  }
  return color;
}

aster::Vec3 densityInwardNormal(const aster::VoxelCaveSpec &spec, const aster::Vec3 position,
                                const aster::Vec3 fallback) {
  const float e = std::max(spec.cell_size * 0.35f, 0.025f);
  aster::Vec3 gradient{densityAtSpec(spec, position + aster::Vec3{e, 0.0f, 0.0f}) -
                           densityAtSpec(spec, position - aster::Vec3{e, 0.0f, 0.0f}),
                       densityAtSpec(spec, position + aster::Vec3{0.0f, e, 0.0f}) -
                           densityAtSpec(spec, position - aster::Vec3{0.0f, e, 0.0f}),
                       densityAtSpec(spec, position + aster::Vec3{0.0f, 0.0f, e}) -
                           densityAtSpec(spec, position - aster::Vec3{0.0f, 0.0f, e})};
  aster::Vec3 normal = aster::normalize(gradient * -1.0f);
  if (aster::length(normal) <= 0.0001f) {
    normal = fallback;
  }
  return normal;
}

bool projectFixtureToSurface(const aster::VoxelCaveSpec &spec, const CaveFrameCandidate &frame,
                             const aster::VoxelCaveFixtureProfile &fixture, const float side_sign,
                             aster::Vec3 &surface, aster::Vec3 &normal) {
  const aster::Vec3 ray_origin = frame.center + frame.up * std::max(fixture.mount_height, 0.0f);
  aster::Vec3 ray_direction = aster::normalize(frame.side * side_sign);
  if (aster::length(ray_direction) <= 0.0001f) {
    ray_direction = frame.side * side_sign;
  }

  float inside_distance = 0.0f;
  float outside_distance = 0.0f;
  const float max_distance = std::max(frame.tunnel_radius * 1.85f, frame.vertical_radius * 1.35f);
  float previous_density = densityAtSpec(spec, ray_origin);
  bool bracketed = false;
  constexpr int kSurfaceProbeSteps = 18;
  for (int step = 1; step <= kSurfaceProbeSteps; ++step) {
    const float distance =
        max_distance * (static_cast<float>(step) / static_cast<float>(kSurfaceProbeSteps));
    const float density = densityAtSpec(spec, ray_origin + ray_direction * distance);
    if (previous_density <= 0.0f && density > 0.0f) {
      inside_distance =
          max_distance * (static_cast<float>(step - 1) / static_cast<float>(kSurfaceProbeSteps));
      outside_distance = distance;
      bracketed = true;
      break;
    }
    previous_density = density;
  }
  if (!bracketed) {
    return false;
  }

  for (int step = 0; step < 8; ++step) {
    const float mid = (inside_distance + outside_distance) * 0.5f;
    if (densityAtSpec(spec, ray_origin + ray_direction * mid) > 0.0f) {
      outside_distance = mid;
    } else {
      inside_distance = mid;
    }
  }

  surface = ray_origin + ray_direction * ((inside_distance + outside_distance) * 0.5f);
  normal = densityInwardNormal(spec, surface, ray_direction * -1.0f);
  if (aster::dot(normal, ray_direction) > 0.0f) {
    normal = normal * -1.0f;
  }
  return true;
}

aster::VoxelCaveFixturePlacement fixtureFromFrame(const aster::VoxelCaveSpec &spec,
                                                  const CaveFrameCandidate &frame,
                                                  const aster::VoxelCaveFixtureProfile &fixture,
                                                  const int station_index,
                                                  const float progress_normalized) {
  const float side_sign = fixtureSideSign(fixture.side_mode, station_index);
  const float lateral_offset =
      std::max(frame.tunnel_radius - std::max(fixture.wall_inset, 0.0f), 0.10f);
  aster::Vec3 normal =
      aster::normalize(frame.side * -side_sign + frame.up * fixture.normal_up_bias);
  if (aster::length(normal) <= 0.0001f) {
    normal = frame.side * -side_sign;
  }
  aster::Vec3 mount = frame.center + frame.side * (side_sign * lateral_offset) +
                      frame.up * std::max(fixture.mount_height, 0.0f);
  aster::Vec3 surface{};
  if (projectFixtureToSurface(spec, frame, fixture, side_sign, surface, normal)) {
    mount = surface + normal * std::max(fixture.wall_inset, 0.0f);
  }
  const aster::Vec3 lens = mount + normal * std::max(fixture.lens_offset, 0.0f);
  const aster::Vec3 light = lens + normal * std::max(fixture.light_offset, 0.0f);
  return {.position = mount,
          .mount_position = mount,
          .lens_position = lens,
          .light_position = light,
          .light_color = fixtureLightColor(spec, frame),
          .normal = normal,
          .tangent = frame.forward,
          .up = frame.up,
          .progress_distance = frame.progress_distance,
          .progress_normalized = progress_normalized,
          .procedural = frame.procedural,
          .procedural_distance = frame.procedural_distance,
          .station_index = station_index};
}

CaveFrameCandidate authoredFrameAtDistance(const aster::VoxelCaveSpec &spec,
                                           const float target_distance) {
  CaveFrameCandidate frame;
  const float total_length = authoredPathLength(spec);
  float accumulated = 0.0f;
  for (const aster::VoxelCaveTunnel &tunnel : spec.tunnels) {
    const aster::Vec3 axis = tunnel.to - tunnel.from;
    const float segment_length = aster::length(axis);
    if (segment_length <= kEpsilon) {
      continue;
    }
    const float local = aster::clamp((target_distance - accumulated) / segment_length, 0.0f, 1.0f);
    const aster::Vec3 center = tunnel.from + axis * local;
    const ProceduralFieldBasis basis = basisFromForwardUp(axis, {0.0f, 1.0f, 0.0f});
    frame = makeFrameCandidate(center, center, basis, tunnel.radius, tunnel.radius,
                               accumulated + segment_length * local, total_length, false);
    if (target_distance <= accumulated + segment_length) {
      return frame;
    }
    accumulated += segment_length;
  }
  return frame;
}

float progressRangeWeight(const aster::VoxelCaveSpec &spec, const aster::Vec3 position,
                          const aster::VoxelProgressRange &range) {
  if (!range.enabled) {
    return 1.0f;
  }

  const CaveFrameCandidate frame = selectVoxelCaveFrame(spec, position);
  if (!frame.valid) {
    return 0.0f;
  }

  const float feather = std::max(range.feather_distance, 0.0f);
  float weight = smoothstep(range.start_distance - feather, range.start_distance + feather,
                            frame.progress_distance);
  if (range.end_distance > range.start_distance) {
    weight *= 1.0f - smoothstep(range.end_distance - feather, range.end_distance + feather,
                                frame.progress_distance);
  }
  return aster::clamp(weight, 0.0f, 1.0f);
}

VeinMaterialSample sampleVeinMaterial(const aster::VoxelCaveSpec &spec, const aster::Vec3 position,
                                      const std::vector<aster::VoxelMaterialProfile> &profiles) {
  VeinMaterialSample sample;
  for (const aster::VoxelMaterialProfile &profile : profiles) {
    const float progress_weight = progressRangeWeight(spec, position, profile.progress_range);
    if (progress_weight <= 0.0f) {
      continue;
    }
    const float strength = veinStrengthAt(position, spec.origin, profile) * progress_weight;
    if (strength > sample.strength) {
      sample.profile = &profile;
      sample.material = profile.material;
      sample.strength = strength;
    }
  }
  return sample;
}

VeinMaterialSample
sampleSurfaceVeinMaterial(const aster::VoxelCaveSpec &spec, const aster::Vec3 surface_point,
                          aster::Vec3 solid_direction, const float cell_size,
                          const std::vector<aster::VoxelMaterialProfile> &profiles) {
  solid_direction = aster::normalize(solid_direction);
  if (aster::length(solid_direction) <= 0.0001f) {
    solid_direction = {0.0f, -1.0f, 0.0f};
  }

  VeinMaterialSample sample;
  const float plug_depth = std::max(cell_size * 1.6f, 0.10f);
  const float plug_step = std::max(cell_size * 0.5f, 0.05f);
  const int plug_sample_count = std::max(1, static_cast<int>(std::ceil(plug_depth / plug_step)));
  for (int index = 0; index <= plug_sample_count; ++index) {
    const float depth =
        plug_depth * (static_cast<float>(index) / static_cast<float>(plug_sample_count));
    const SolidPlugSample plug_sample = sampleSolidPlug(spec, surface_point + solid_direction * depth);
    if (!plug_sample.active || plug_sample.material == aster::VoxelCaveMaterial::Rock ||
        plug_sample.material == aster::VoxelCaveMaterial::Air) {
      continue;
    }
    const aster::VoxelMaterialProfile *profile = profileForMaterial(profiles, plug_sample.material);
    const float depth_contact =
        1.0f - aster::clamp(depth / std::max(plug_depth, 0.001f), 0.0f, 1.0f);
    const float surface_strength = plug_sample.strength * depth_contact;
    if (profile != nullptr && hasVisibleSurfaceExposure(*profile, surface_strength) &&
        surface_strength >= sample.strength) {
      sample.profile = profile;
      sample.material = plug_sample.material;
      sample.strength = surface_strength;
    }
  }
  for (const aster::VoxelMaterialProfile &profile : profiles) {
    if (!usesVeinField(profile)) {
      continue;
    }
    const float progress_weight = progressRangeWeight(spec, surface_point, profile.progress_range);
    if (progress_weight <= 0.0f) {
      continue;
    }
    const float max_depth = std::max(cell_size, profile.vein_radius * 1.75f);
    const float step = std::max(cell_size * 0.5f, 0.05f);
    const int sample_count = std::max(1, static_cast<int>(std::ceil(max_depth / step)));
    for (int index = 0; index <= sample_count; ++index) {
      const float depth =
          max_depth * (static_cast<float>(index) / static_cast<float>(sample_count));
      const float depth_contact =
          1.0f - aster::clamp(depth / std::max(max_depth, 0.001f), 0.0f, 1.0f);
      const float strength =
          veinStrengthAt(surface_point + solid_direction * depth, spec.origin, profile) *
          progress_weight * depth_contact;
      if (hasVisibleSurfaceExposure(profile, strength) && strength > sample.strength) {
        sample.profile = &profile;
        sample.material = profile.material;
        sample.strength = strength;
      }
    }
  }
  return sample;
}

} // namespace

namespace aster {

VoxelCaveProceduralFrame sampleVoxelCaveProceduralFrameAt(const VoxelCaveProceduralField &field,
                                                          const float distance) {
  if (!field.enabled) {
    return {};
  }
  const ProceduralFieldBasis basis = proceduralBasis(field);
  const float clamped_distance = std::max(distance, 0.0f);
  const ProceduralFieldBasis local_basis = proceduralBasisAt(field, basis, clamped_distance);
  const Vec2 radii = proceduralTunnelRadii(field, clamped_distance);
  return {.valid = true,
          .center = proceduralCenterAt(field, basis, clamped_distance),
          .forward = local_basis.forward,
          .side = local_basis.side,
          .up = local_basis.up,
          .distance = clamped_distance,
          .tunnel_radius = radii.x,
          .vertical_radius = radii.y};
}

VoxelCaveProceduralFrame closestVoxelCaveProceduralFrame(const VoxelCaveProceduralField &field,
                                                         const Vec3 position) {
  if (!field.enabled) {
    return {};
  }
  const ProceduralFieldBasis basis = proceduralBasis(field);
  const float projected = dot(position - field.start, basis.forward);
  if (projected < -std::max(field.backtrack_distance, 0.0f)) {
    return {};
  }
  const int sample_steps = std::max(field.path_sample_steps, 0);
  const float sample_spacing = proceduralPathSampleSpacing(field);
  VoxelCaveProceduralFrame best;
  float best_score = std::numeric_limits<float>::max();
  for (int sample_offset = -sample_steps; sample_offset <= sample_steps; ++sample_offset) {
    const float candidate_distance =
        std::max(projected + static_cast<float>(sample_offset) * sample_spacing, 0.0f);
    VoxelCaveProceduralFrame candidate =
        sampleVoxelCaveProceduralFrameAt(field, candidate_distance);
    if (!candidate.valid) {
      continue;
    }
    const Vec3 offset = position - candidate.center;
    const float radius = std::max(candidate.tunnel_radius, 0.001f);
    const float height = std::max(candidate.vertical_radius, 0.001f);
    const float lateral = dot(offset, candidate.side) / radius;
    const float vertical = dot(offset, candidate.up) / height;
    const float axial =
        std::abs(dot(offset, candidate.forward)) / std::max(sample_spacing, 0.001f);
    const float score = lateral * lateral + vertical * vertical + axial * axial * 0.35f;
    if (!best.valid || score < best_score) {
      best = candidate;
      best_score = score;
    }
  }
  return best;
}

VoxelCaveInteriorSample sampleVoxelCaveInterior(const VoxelCaveSpec &spec, const Vec3 position,
                                                const float density,
                                                const VoxelCaveInteriorProbe probe) {
  VoxelCaveInteriorSample sample;
  sample.density = density;

  const CaveFrameCandidate frame = selectVoxelCaveFrame(spec, position);
  if (!frame.valid) {
    sample.inside = density <= 0.0f;
    sample.interior = sample.inside ? 1.0f : 0.0f;
    return sample;
  }

  const float path_length = std::max(frame.path_length, frame.progress_distance);
  const float density_feather =
      std::max(probe.density_feather_cells, 0.0f) * std::max(spec.cell_size, 0.001f);
  const float interior =
      1.0f - smoothstep(-density_feather * 0.25f, std::max(density_feather, 0.001f), density);
  const float entrance_distance = std::max(probe.entrance_light_distance, 0.001f);
  const float entrance =
      interior * (1.0f - smoothstep(0.0f, entrance_distance, frame.progress_distance));
  const float depth_reference = std::max(probe.depth_reference_distance, 0.001f);

  sample.valid = true;
  sample.inside = density <= 0.0f || interior > 0.10f;
  sample.procedural = frame.procedural;
  sample.procedural_field_index = frame.procedural_field_index;
  sample.interior = aster::clamp(interior, 0.0f, 1.0f);
  sample.entrance_light = aster::clamp(entrance, 0.0f, 1.0f);
  sample.depth =
      aster::clamp(frame.progress_distance / depth_reference, 0.0f, 1.0f) * sample.interior;
  sample.chamber = chamberWeightAt(spec, position);
  sample.progress_distance = frame.progress_distance;
  sample.progress_normalized =
      path_length > 0.001f ? aster::clamp(frame.progress_distance / path_length, 0.0f, 1.0f) : 0.0f;
  sample.procedural_distance = frame.procedural_distance;
  sample.center = frame.center;
  sample.forward = frame.forward;
  sample.side = frame.side;
  sample.up = frame.up;
  sample.lateral = frame.lateral;
  sample.vertical = frame.vertical;
  sample.radial_fraction = frame.radial_fraction;
  sample.tunnel_radius = frame.tunnel_radius;
  sample.vertical_radius = frame.vertical_radius;
  return sample;
}

FrameWorkBudget defaultVoxelCaveInteractiveRebuildBudget() {
  return voxelCaveRebuildItemBudget(1u);
}

FrameWorkBudget voxelCaveRebuildItemBudget(const std::uint32_t max_items) {
  FrameWorkBudget budget;
  budget.max_items = std::max(max_items, 1u);
  budget.class_budgets.push_back(
      {.class_id = kVoxelChunkRebuildWorkClass, .max_items = budget.max_items, .max_seconds = 0.0});
  return budget;
}

std::vector<VoxelCaveFixturePlacement> placeVoxelCavePathFixtures(const VoxelCaveSpec &spec,
                                                                  VoxelCaveFixtureProfile fixture) {
  std::vector<VoxelCaveFixturePlacement> placements;
  const float path_length = authoredPathLength(spec);
  if (path_length <= kEpsilon || fixture.max_count <= 0) {
    return placements;
  }

  const float start = aster::clamp(std::max(fixture.start_distance, 0.0f), 0.0f, path_length);
  const float end = fixture.end_distance > start
                        ? aster::clamp(fixture.end_distance, start, path_length)
                        : path_length;
  if (end <= start + kEpsilon) {
    return placements;
  }

  const float spacing = std::max(fixture.target_spacing, 0.25f);
  const int count = std::clamp(static_cast<int>(std::floor((end - start) / spacing)) + 1, 1,
                               std::max(fixture.max_count, 1));
  placements.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    const float fill =
        count == 1 ? 0.5f : static_cast<float>(index) / static_cast<float>(count - 1);
    const float distance = start + (end - start) * fill;
    const CaveFrameCandidate frame = authoredFrameAtDistance(spec, distance);
    if (!frame.valid) {
      continue;
    }
    const float progress_normalized = path_length > 0.001f ? distance / path_length : 0.0f;
    placements.push_back(fixtureFromFrame(spec, frame, fixture, index, progress_normalized));
  }
  return placements;
}

std::vector<VoxelCaveFixturePlacement>
placeVoxelCaveProceduralFixturesNear(const VoxelCaveSpec &spec, const Vec3 viewer,
                                     VoxelCaveFixtureProfile fixture) {
  std::vector<VoxelCaveFixturePlacement> placements;
  const int behind = std::max(fixture.procedural_behind, 0);
  const int ahead = std::max(fixture.procedural_ahead, 0);
  placements.reserve(spec.procedural_fields.size() * static_cast<std::size_t>(behind + ahead + 1));
  const float spacing = std::max(fixture.target_spacing, 0.25f);
  const float path_length = authoredPathLength(spec);

  for (const VoxelCaveProceduralField &field : spec.procedural_fields) {
    const VoxelCaveProceduralFrame nearest = closestVoxelCaveProceduralFrame(field, viewer);
    if (!nearest.valid) {
      continue;
    }

    const float base_progress = fieldStartProgressDistance(spec, field, path_length);
    const int nearest_index = static_cast<int>(std::floor(nearest.distance / spacing));
    float last_distance = -std::numeric_limits<float>::infinity();
    for (int offset = -behind; offset <= ahead; ++offset) {
      const int station_index = std::max(nearest_index + offset, 0);
      const float station_distance = static_cast<float>(station_index) * spacing;
      if (std::abs(station_distance - last_distance) < 0.001f) {
        continue;
      }
      last_distance = station_distance;

      const VoxelCaveProceduralFrame frame =
          sampleVoxelCaveProceduralFrameAt(field, station_distance);
      if (!frame.valid) {
        continue;
      }
      const ProceduralFieldBasis basis = basisFromForwardUp(frame.forward, frame.up);
      const CaveFrameCandidate candidate = makeFrameCandidate(
          frame.center, frame.center, basis, frame.tunnel_radius, frame.vertical_radius,
          base_progress + frame.distance, std::max(path_length, base_progress + frame.distance),
          true, frame.distance);
      const float progress_normalized =
          candidate.path_length > 0.001f
              ? aster::clamp(candidate.progress_distance / candidate.path_length, 0.0f, 1.0f)
              : 0.0f;
      placements.push_back(
          fixtureFromFrame(spec, candidate, fixture, station_index, progress_normalized));
    }
  }
  return placements;
}

std::vector<VoxelCaveFixtureLightSample>
rankVoxelCaveFixtureLights(const Vec3 viewer, const VoxelCaveInteriorSample &interior,
                           const std::vector<VoxelCaveFixturePlacement> &fixtures,
                           const VoxelCaveFixtureLightProbe probe) {
  std::vector<VoxelCaveFixtureLightSample> ranked;
  ranked.reserve(fixtures.size());
  const float progress_window = std::max(probe.progress_window, 0.001f);
  const float min_gain = aster::clamp(probe.minimum_progress_gain, 0.0f, 1.0f);

  for (const VoxelCaveFixturePlacement &fixture : fixtures) {
    const float distance = aster::length(fixture.light_position - viewer);
    const float progress_delta = std::abs(fixture.progress_distance - interior.progress_distance);
    const float progress_gain =
        1.0f - (1.0f - min_gain) * aster::clamp(progress_delta / progress_window, 0.0f, 1.0f);
    const float weight = aster::clamp(interior.interior * progress_gain, 0.0f, 1.0f);
    ranked.push_back({.placement = fixture,
                      .distance = distance,
                      .progress_delta = progress_delta,
                      .weight = weight,
                      .score = distance * std::max(probe.distance_score_weight, 0.0f) +
                               progress_delta * std::max(probe.progress_score_weight, 0.0f)});
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const VoxelCaveFixtureLightSample &lhs, const VoxelCaveFixtureLightSample &rhs) {
              return lhs.score < rhs.score;
            });
  return ranked;
}

void VoxelCaveState::configure(VoxelCaveSpec spec) {
  if (spec.cell_size <= 0.0f || spec.chunk_cells < 4) {
    throw std::invalid_argument("Voxel cave requires positive cell size and at least four cells.");
  }
  spec.stream_radius = std::max(spec.stream_radius, 1);
  spec.unload_radius = std::max(spec.unload_radius, spec.stream_radius);
  spec.max_chunk_rebuilds_per_update = std::max(spec.max_chunk_rebuilds_per_update, 0);
  spec.forced_rebuild_radius = std::max(spec.forced_rebuild_radius, 0);
  spec.visibility_probe_radius = std::clamp(spec.visibility_probe_radius, 0, spec.unload_radius);
  spec.predictive_lookahead_seconds = std::max(spec.predictive_lookahead_seconds, 0.0f);
  spec.path_prefetch_ahead_chunks =
      std::clamp(spec.path_prefetch_ahead_chunks, 0, spec.unload_radius);
  spec.path_prefetch_behind_chunks =
      std::clamp(spec.path_prefetch_behind_chunks, 0, spec.unload_radius);
  spec.path_prefetch_radius = std::clamp(spec.path_prefetch_radius, 0, spec.unload_radius);
  spec.path_prefetch_spacing_chunks = std::max(spec.path_prefetch_spacing_chunks, 0.10f);
  spec.chunk_transition_seconds = std::max(spec.chunk_transition_seconds, 0.0f);
  spec.coarse_proxy_cells =
      std::clamp(spec.coarse_proxy_cells, 2, std::max(spec.chunk_cells, 2));
  spec_ = std::move(spec);
  clear();
}

void VoxelCaveState::clear() {
  chunks_.clear();
  chunk_lookup_.clear();
  edits_.clear();
  snapshots_.clear();
  changed_snapshots_.clear();
  last_update_stats_ = {};
  rebuild_costs_.clear();
  rebuild_budget_controller_.reset();
}

const VoxelCaveSpec &VoxelCaveState::spec() const {
  return spec_;
}

const std::vector<VoxelEdit> &VoxelCaveState::edits() const {
  return edits_;
}

void VoxelCaveState::setEdits(std::vector<VoxelEdit> edits) {
  edits_ = std::move(edits);
  for (ChunkState &chunk : chunks_) {
    chunk.dirty = true;
    chunk.dirty_age = 0.0f;
    chunk.dirty_frames = 0u;
    chunk.edit_overlaps = 0u;
  }
  changed_snapshots_.clear();
}

const std::vector<VoxelChunkSnapshot> &VoxelCaveState::activeChunks() const {
  return snapshots_;
}

const std::vector<VoxelChunkSnapshot> &VoxelCaveState::changedChunks() const {
  return changed_snapshots_;
}

const VoxelCaveUpdateStats &VoxelCaveState::lastUpdateStats() const {
  return last_update_stats_;
}

std::optional<VoxelChunkSnapshot> VoxelCaveState::consumeDirtyChunk(const VoxelChunkCoord coord) {
  ChunkState *chunk = findChunk(coord);
  if (chunk != nullptr && !chunk->dirty && chunk->collision_dirty) {
    chunk->collision_dirty = false;
    return snapshotFor(*chunk, true);
  }
  return std::nullopt;
}

VoxelChunkCoord VoxelCaveState::chunkCoordFor(const Vec3 position) const {
  const float chunk_size = spec_.cell_size * static_cast<float>(spec_.chunk_cells);
  const Vec3 local = position - spec_.origin;
  return {static_cast<int>(std::floor(local.x / chunk_size)),
          static_cast<int>(std::floor(local.y / chunk_size)),
          static_cast<int>(std::floor(local.z / chunk_size))};
}

VoxelCellCoord VoxelCaveState::cellCoordFor(const Vec3 position) const {
  const Vec3 local = position - spec_.origin;
  return {static_cast<int>(std::floor(local.x / spec_.cell_size)),
          static_cast<int>(std::floor(local.y / spec_.cell_size)),
          static_cast<int>(std::floor(local.z / spec_.cell_size))};
}

Vec3 VoxelCaveState::chunkCenter(const VoxelChunkCoord coord) const {
  const float chunk_size = spec_.cell_size * static_cast<float>(spec_.chunk_cells);
  return spec_.origin + Vec3{(static_cast<float>(coord.x) + 0.5f) * chunk_size,
                             (static_cast<float>(coord.y) + 0.5f) * chunk_size,
                             (static_cast<float>(coord.z) + 0.5f) * chunk_size};
}

int VoxelCaveState::chunkDistance(const VoxelChunkCoord lhs, const VoxelChunkCoord rhs) const {
  return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y), std::abs(lhs.z - rhs.z)});
}

void VoxelCaveState::rebuildChunkLookup() {
  chunk_lookup_.clear();
  chunk_lookup_.reserve(chunks_.size());
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    chunk_lookup_[chunks_[index].coord] = index;
  }
}

VoxelCaveState::ChunkState *VoxelCaveState::findChunk(const VoxelChunkCoord coord) {
  const auto found = chunk_lookup_.find(coord);
  if (found == chunk_lookup_.end() || found->second >= chunks_.size()) {
    return nullptr;
  }
  return &chunks_[found->second];
}

const VoxelCaveState::ChunkState *VoxelCaveState::findChunk(const VoxelChunkCoord coord) const {
  const auto found = chunk_lookup_.find(coord);
  if (found == chunk_lookup_.end() || found->second >= chunks_.size()) {
    return nullptr;
  }
  return &chunks_[found->second];
}

VoxelChunkSnapshot VoxelCaveState::snapshotFor(const ChunkState &chunk,
                                               const bool collision_dirty) const {
  const Vec3 bounds_center = surfaceBoundsCenter(chunk.surface_stats, chunk.center);
  VoxelChunkSnapshot snapshot;
  snapshot.coord = chunk.coord;
  snapshot.center = chunk.center;
  snapshot.bounds_center = bounds_center;
  snapshot.bounds_radius = surfaceBoundsRadius(chunk.surface_stats, bounds_center);
  snapshot.reveal_age = chunk.reveal_age;
  snapshot.newly_revealed = chunk.newly_revealed;
  snapshot.rebuild_pending = chunk.dirty;
  snapshot.collision_dirty = !chunk.dirty && collision_dirty;
  snapshot.coarse_proxy = chunk.coarse_proxy;
  snapshot.lifecycle = chunk.lifecycle;
  const bool transitioning =
      !chunk.retiring_batches.empty() && chunk.render_transition_seconds > 0.0f;
  const float transition =
      transitioning
          ? clamp(chunk.render_transition_age / std::max(chunk.render_transition_seconds, 0.001f),
                  0.0f, 1.0f)
          : 1.0f;
  snapshot.batches.reserve(chunk.retiring_batches.size() + chunk.batches.size());
  for (VoxelChunkRenderBatch batch : chunk.retiring_batches) {
    batch.opacity *= 1.0f - transition;
    if (batch.opacity > 0.003f) {
      snapshot.batches.push_back(std::move(batch));
    }
  }
  for (VoxelChunkRenderBatch batch : chunk.batches) {
    batch.opacity *= transitioning ? transition : 1.0f;
    if (batch.opacity > 0.003f) {
      snapshot.batches.push_back(std::move(batch));
    }
  }
  snapshot.collision_mesh = chunk.collision_mesh;
  snapshot.surface_stats = chunk.surface_stats;
  snapshot.mesh_generation = chunk.mesh_generation;
  return snapshot;
}

void VoxelCaveState::ensureChunk(const VoxelChunkCoord coord) {
  if (findChunk(coord) != nullptr) {
    return;
  }

  ChunkState chunk;
  chunk.coord = coord;
  chunk.center = chunkCenter(coord);
  chunk.newly_revealed = true;
  rebuildCoarseProxy(chunk);
  chunks_.push_back(std::move(chunk));
  chunk_lookup_[coord] = chunks_.size() - 1u;
}

void VoxelCaveState::rebuildCoarseProxy(ChunkState &chunk) {
  ASTER_PROFILE_SCOPE("VoxelCaveState::coarseProxy");
  chunk.batches.clear();
  chunk.retiring_batches.clear();
  chunk.collision_mesh.reset();
  chunk.surface_stats = {};
  chunk.coarse_proxy = false;
  chunk.lifecycle = VoxelChunkLifecycle::Requested;
  chunk.render_transition_age = 0.0f;
  chunk.render_transition_seconds = 0.0f;

  const int n = std::clamp(spec_.coarse_proxy_cells, 2, std::max(spec_.chunk_cells, 2));
  const float chunk_size = spec_.cell_size * static_cast<float>(spec_.chunk_cells);
  const float s = chunk_size / static_cast<float>(n);
  const Vec3 chunk_origin =
      spec_.origin + Vec3{static_cast<float>(chunk.coord.x) * chunk_size,
                          static_cast<float>(chunk.coord.y) * chunk_size,
                          static_cast<float>(chunk.coord.z) * chunk_size};

  SurfaceExtractionSettings settings = spec_.surface_extraction;
  settings.emit_collision_mesh = true;
  settings.emit_channel_meshes = false;
  settings.capture_faces = false;
  SurfaceExtractionResult surface = extractImplicitSurface(
      {.origin = chunk_origin,
       .cell_size = s,
       .cell_count = {n, n, n},
       .settings = settings,
       .density = [this](const Vec3 point) { return densityAt(point); },
       .reject_face = [this](const SurfaceFace &face) {
         return insideAnySurfaceOpening(spec_, face.centroid);
       }});

  CpuMesh proxy_mesh = std::move(surface.collision_mesh);

  if (proxy_mesh.vertices.empty() || proxy_mesh.indices.empty()) {
    return;
  }

  chunk.surface_stats.active_cells = surface.stats.active_cells;
  chunk.surface_stats.surface_vertices = static_cast<std::uint32_t>(proxy_mesh.vertices.size());
  chunk.surface_stats.surface_triangles =
      static_cast<std::uint32_t>(proxy_mesh.indices.size() / 3u);
  applyMeshBoundsToStats(chunk.surface_stats, proxy_mesh);
  applyTopologyToStats(chunk.surface_stats, proxy_mesh);
  appendMaterialStats(chunk.surface_stats, VoxelCaveMaterial::Rock, proxy_mesh);
  chunk.surface_stats.material_batches = 1u;
  ++chunk.mesh_generation;
  chunk.coarse_proxy = true;
  chunk.lifecycle = VoxelChunkLifecycle::CoarseVisible;
  chunk.batches.push_back({chunk.coord, VoxelChunkRenderBatchKind::CoarseProxySurface,
                           VoxelCaveMaterial::Rock, "Cave coarse proxy", 1.0f,
                           std::make_shared<const CpuMesh>(std::move(proxy_mesh))});
}

bool VoxelCaveState::chunkIntersectsEdit(const VoxelChunkCoord coord, const VoxelEdit &edit) const {
  const float chunk_size = spec_.cell_size * static_cast<float>(spec_.chunk_cells);
  const Vec3 center = chunkCenter(coord);
  const Vec3 half{chunk_size * 0.5f, chunk_size * 0.5f, chunk_size * 0.5f};
  const Vec3 closest{clamp(edit.center.x, center.x - half.x, center.x + half.x),
                     clamp(edit.center.y, center.y - half.y, center.y + half.y),
                     clamp(edit.center.z, center.z - half.z, center.z + half.z)};
  return length(closest - edit.center) <= edit.radius + spec_.cell_size;
}

void VoxelCaveState::updateStreaming(const Vec3 viewer_position, const float dt) {
  updateStreaming(viewer_position, dt, {});
}

void VoxelCaveState::updateStreaming(const Vec3 viewer_position, const float dt,
                                     const VoxelCaveUpdateOptions &options) {
  ASTER_PROFILE_SCOPE("VoxelCaveState::streaming");
  last_update_stats_ = {};
  changed_snapshots_.clear();

  const float chunk_size = spec_.cell_size * static_cast<float>(spec_.chunk_cells);
  std::vector<VoxelChunkCoord> retention_centers;
  const auto ensureNeighborhood = [&](const Vec3 position, const int radius,
                                      const bool count_probe_chunks) {
    const VoxelChunkCoord center = chunkCoordFor(position);
    retention_centers.push_back(center);
    const int r = std::clamp(radius, 0, spec_.unload_radius);
    for (int z = -r; z <= r; ++z) {
      for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
          const VoxelChunkCoord coord{center.x + x, center.y + y, center.z + z};
          if (count_probe_chunks && findChunk(coord) == nullptr) {
            ++last_update_stats_.visibility_probe_chunks;
          }
          ensureChunk(coord);
        }
      }
    }
  };

  const VoxelChunkCoord viewer = chunkCoordFor(viewer_position);
  ensureNeighborhood(viewer_position, spec_.stream_radius, false);
  if (length(options.viewer_velocity) > kEpsilon && spec_.predictive_lookahead_seconds > 0.0f) {
    ensureNeighborhood(viewer_position + options.viewer_velocity * spec_.predictive_lookahead_seconds,
                       std::max(1, spec_.visibility_probe_radius), true);
  }
  for (const Vec3 probe : options.visibility_probes) {
    ensureNeighborhood(probe, spec_.visibility_probe_radius, true);
  }
  const VoxelCaveInteriorSample viewer_interior = sampleInterior(viewer_position);
  const bool has_cave_frame = viewer_interior.valid && viewer_interior.inside &&
                              length(viewer_interior.forward) > 0.0001f &&
                              length(viewer_interior.side) > 0.0001f &&
                              length(viewer_interior.up) > 0.0001f;
  if (has_cave_frame) {
    const bool follows_procedural_path =
        viewer_interior.procedural && viewer_interior.procedural_field_index >= 0 &&
        static_cast<std::size_t>(viewer_interior.procedural_field_index) <
            spec_.procedural_fields.size();
    if (follows_procedural_path) {
      const VoxelCaveProceduralField &field =
          spec_.procedural_fields[static_cast<std::size_t>(viewer_interior.procedural_field_index)];
      const int ahead_limit = std::min(spec_.unload_radius, spec_.path_prefetch_ahead_chunks);
      const int behind_limit = std::min(spec_.unload_radius, spec_.path_prefetch_behind_chunks);
      const int probe_radius = std::clamp(spec_.path_prefetch_radius, 0, spec_.unload_radius);
      const float spacing = chunk_size * std::max(spec_.path_prefetch_spacing_chunks, 0.10f);
      for (int step = -behind_limit; step <= ahead_limit; ++step) {
        if (step == 0) {
          continue;
        }
        const float distance =
            std::max(viewer_interior.procedural_distance + static_cast<float>(step) * spacing,
                     0.0f);
        const VoxelCaveProceduralFrame frame =
            sampleVoxelCaveProceduralFrameAt(field, distance);
        if (frame.valid) {
          ensureNeighborhood(frame.center, probe_radius, true);
        }
      }
    } else {
      const int ahead_limit = std::min(spec_.unload_radius, spec_.stream_radius + 1);
      const int lateral_span = std::max(0, spec_.stream_radius - 1);
      for (int ahead = 1; ahead <= ahead_limit; ++ahead) {
        for (int up = -lateral_span; up <= lateral_span; ++up) {
          for (int side = -lateral_span; side <= lateral_span; ++side) {
            const Vec3 probe = viewer_position +
                               normalize(viewer_interior.forward) * (chunk_size * ahead) +
                               normalize(viewer_interior.side) * (chunk_size * 0.70f *
                                                                  static_cast<float>(side)) +
                               normalize(viewer_interior.up) * (chunk_size * 0.54f *
                                                                static_cast<float>(up));
            ensureChunk(chunkCoordFor(probe));
          }
        }
      }
    }
  }

  const std::size_t chunks_before_unload = chunks_.size();
  chunks_.erase(std::remove_if(chunks_.begin(), chunks_.end(),
                               [&](const ChunkState &chunk) {
                                 return std::none_of(
                                     retention_centers.begin(), retention_centers.end(),
                                     [&](const VoxelChunkCoord center) {
                                       return chunkDistance(chunk.coord, center) <=
                                              spec_.unload_radius;
                                     });
                               }),
                chunks_.end());
  if (chunks_.size() != chunks_before_unload) {
    last_update_stats_.expired_chunks =
        static_cast<std::uint32_t>(chunks_before_unload - chunks_.size());
    rebuildChunkLookup();
  }

  const Vec3 motion_direction =
      length(options.viewer_velocity) > kEpsilon ? normalize(options.viewer_velocity) : Vec3{};
  const Vec3 cave_direction = has_cave_frame ? normalize(viewer_interior.forward) : Vec3{};
  const Vec3 priority_direction =
      length(motion_direction) > kEpsilon ? motion_direction : cave_direction;

  BudgetedWorkQueue rebuild_queue;
  rebuild_queue.reserve(chunks_.size());
  std::uint64_t sequence = 0u;
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    ChunkState &chunk = chunks_[index];
    if (!chunk.dirty) {
      continue;
    }
    chunk.dirty_age += std::max(dt, 0.0f);
    ++chunk.dirty_frames;
    ++last_update_stats_.dirty_chunks;

    const bool missing_collision =
        chunk.collision_mesh == nullptr || chunk.collision_mesh->vertices.empty() ||
        chunk.collision_mesh->indices.empty();
    const bool missing_render =
        std::none_of(chunk.batches.begin(), chunk.batches.end(),
                     [](const VoxelChunkRenderBatch &batch) {
                       return batch.mesh != nullptr && !batch.mesh->vertices.empty() &&
                              !batch.mesh->indices.empty();
                     });
    const double distance_chunks =
        static_cast<double>(std::max(length(chunk.center - viewer_position) / chunk_size, 0.0f));
    const double distance_score = 1.0 / (1.0 + distance_chunks);
    const Vec3 to_chunk = chunk.center - viewer_position;
    const double alignment_score =
        length(priority_direction) > kEpsilon && length(to_chunk) > kEpsilon
            ? std::max(0.0f, dot(normalize(to_chunk), priority_direction))
            : 0.0;
    const VoxelCaveRebuildPriorityWeights &weights = options.priority_weights;
    const double priority =
        (missing_collision ? weights.missing_collision : 0.0) +
        (missing_render ? weights.missing_render : 0.0) +
        static_cast<double>(chunk.edit_overlaps) * weights.edit_overlap +
        distance_score * weights.viewer_distance + alignment_score * weights.velocity_alignment +
        static_cast<double>(chunk.dirty_age) * weights.dirty_age;

    const double rebuild_estimate =
        rebuild_costs_.estimate(kVoxelChunkRebuildWorkClass,
                                options.override_rebuild_budget ? 1.0 : 0.0015);
    rebuild_queue.enqueue({.id = chunkWorkId(chunk.coord),
                           .class_id = kVoxelChunkRebuildWorkClass,
                           .priority = priority,
                           .estimated_seconds = rebuild_estimate,
                           .virtual_backlog_frames = chunk.dirty_frames,
                           .sequence = sequence++,
                           .payload_index = index});
  }

  FrameWorkBudget budget = options.override_rebuild_budget ? options.rebuild_budget : FrameWorkBudget{};
  if (!options.override_rebuild_budget && spec_.max_chunk_rebuilds_per_update > 0) {
    budget.max_items = static_cast<std::uint32_t>(spec_.max_chunk_rebuilds_per_update);
    budget.class_budgets.push_back(
        {.class_id = kVoxelChunkRebuildWorkClass,
         .max_items = static_cast<std::uint32_t>(spec_.max_chunk_rebuilds_per_update),
         .max_seconds = 0.0});
  }
  if (!options.override_rebuild_budget) {
    FrameBudgetControllerInput controller_input = options.budget_feedback;
    controller_input.backlog_items =
        std::max(controller_input.backlog_items,
                 static_cast<std::uint32_t>(rebuild_queue.items().size()));
    controller_input.viewer_speed = std::max(controller_input.viewer_speed,
                                             static_cast<double>(length(options.viewer_velocity)));
    budget = rebuild_budget_controller_.nextBudget(controller_input, budget);
    last_update_stats_.budget_telemetry = rebuild_budget_controller_.telemetry();
  } else {
    last_update_stats_.budget_telemetry = {};
    last_update_stats_.budget_telemetry.backlog_items =
        static_cast<std::uint32_t>(rebuild_queue.items().size());
  }
  const BudgetedWorkSelection selected = rebuild_queue.select(budget);
  last_update_stats_.queued_rebuilds =
      static_cast<std::uint32_t>(selected.diagnostics.queued_items);
  last_update_stats_.selected_rebuilds =
      static_cast<std::uint32_t>(selected.diagnostics.selected_items);
  last_update_stats_.rebuild_queue = selected.diagnostics;

  std::vector<bool> rebuild_flags(chunks_.size(), false);
  for (const BudgetedWorkItem &item : selected.selected) {
    if (item.payload_index < rebuild_flags.size()) {
      rebuild_flags[item.payload_index] = true;
    }
  }
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    const ChunkState &chunk = chunks_[index];
    if (!chunk.dirty) {
      continue;
    }
    const bool forced =
        chunkDistance(chunk.coord, viewer) <= std::max(spec_.forced_rebuild_radius, 0);
    if (forced && !rebuild_flags[index]) {
      rebuild_flags[index] = true;
      ++last_update_stats_.forced_rebuilds;
    }
  }

  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    ChunkState &chunk = chunks_[index];
    if (!chunk.dirty || !rebuild_flags[index]) {
      continue;
    }
    chunk.lifecycle = chunk.coarse_proxy ? VoxelChunkLifecycle::CoarseVisible
                                         : VoxelChunkLifecycle::FullBuilding;
    const auto rebuild_start = std::chrono::steady_clock::now();
    rebuildChunk(chunk);
    const auto rebuild_end = std::chrono::steady_clock::now();
    rebuild_costs_.observe({.class_id = kVoxelChunkRebuildWorkClass,
                            .seconds =
                                std::chrono::duration<double>(rebuild_end - rebuild_start).count()});
    chunk.dirty = false;
    chunk.collision_dirty = true;
    chunk.dirty_age = 0.0f;
    chunk.dirty_frames = 0u;
    chunk.edit_overlaps = 0u;
    ++last_update_stats_.rebuilt_chunks;
  }

  {
    ASTER_PROFILE_SCOPE("VoxelCaveState::chunkPublish");
    snapshots_.clear();
    snapshots_.reserve(chunks_.size());
    for (ChunkState &chunk : chunks_) {
      chunk.reveal_age += std::max(dt, 0.0f);
      if (chunk.render_transition_seconds > 0.0f) {
        chunk.render_transition_age += std::max(dt, 0.0f);
        if (chunk.render_transition_age >= chunk.render_transition_seconds) {
          chunk.retiring_batches.clear();
          chunk.render_transition_age = 0.0f;
          chunk.render_transition_seconds = 0.0f;
        }
      }
      const bool has_publishable_mesh =
          chunk.mesh_generation > 0u &&
          ((!chunk.batches.empty() && std::any_of(chunk.batches.begin(), chunk.batches.end(),
                                                  [](const VoxelChunkRenderBatch &batch) {
                                                    return batch.mesh != nullptr &&
                                                           !batch.mesh->vertices.empty() &&
                                                           !batch.mesh->indices.empty();
                                                  })) ||
           (!chunk.retiring_batches.empty() &&
            std::any_of(chunk.retiring_batches.begin(), chunk.retiring_batches.end(),
                        [](const VoxelChunkRenderBatch &batch) {
                          return batch.mesh != nullptr && !batch.mesh->vertices.empty() &&
                                 !batch.mesh->indices.empty();
                        })) ||
           chunk.collision_mesh != nullptr);
      if (!has_publishable_mesh) {
        continue;
      }
      const VoxelChunkSnapshot snapshot = snapshotFor(chunk, chunk.collision_dirty);
      snapshots_.push_back(snapshot);
      ++last_update_stats_.published_chunks;
      if (chunk.coarse_proxy) {
        ++last_update_stats_.coarse_visible_chunks;
      } else {
        ++last_update_stats_.full_published_chunks;
      }
      if (chunk.dirty) {
        ++last_update_stats_.pending_chunks;
      } else if (chunk.published_generation != chunk.mesh_generation || chunk.newly_revealed) {
        changed_snapshots_.push_back(snapshot);
        chunk.published_generation = chunk.mesh_generation;
      }
      chunk.newly_revealed = false;
    }
  }
  last_update_stats_.active_chunks = static_cast<std::uint32_t>(snapshots_.size());
  last_update_stats_.changed_snapshots = static_cast<std::uint32_t>(changed_snapshots_.size());
}

VoxelEditResult VoxelCaveState::applyEdit(const VoxelEdit &edit,
                                          const VoxelEditRebuildMode rebuild_mode) {
  ASTER_PROFILE_SCOPE("VoxelCaveState::applyEdit");
  VoxelEditResult result;
  if (edit.radius <= 0.0f) {
    return result;
  }
  if (edit.operation != VoxelEditOperation::Carve) {
    return result;
  }
  result.accepted = true;
  edits_.push_back(edit);
  for (ChunkState &chunk : chunks_) {
    if (chunkIntersectsEdit(chunk.coord, edit)) {
      chunk.dirty = true;
      chunk.dirty_age = 0.0f;
      chunk.dirty_frames = 0u;
      ++chunk.edit_overlaps;
      result.affected_chunks.push_back(chunk.coord);
      if (rebuild_mode == VoxelEditRebuildMode::ImmediateAffected) {
        rebuildChunk(chunk);
        chunk.dirty = false;
        chunk.collision_dirty = true;
        chunk.dirty_age = 0.0f;
        chunk.dirty_frames = 0u;
        chunk.edit_overlaps = 0u;
        ++result.rebuilt_chunks;
      }
    }
  }
  return result;
}

float VoxelCaveState::densityAt(const Vec3 position) const {
  return densityAtSpec(spec_, position, &edits_);
}

VoxelCaveMaterial VoxelCaveState::materialAt(const Vec3 position) const {
  if (densityAt(position) <= 0.0f) {
    return VoxelCaveMaterial::Air;
  }

  const SolidPlugSample plug_sample = sampleSolidPlug(spec_, position);
  if (plug_sample.active && plug_sample.density >= 0.0f &&
      plug_sample.material != VoxelCaveMaterial::Air) {
    return plug_sample.material;
  }
  const VeinMaterialSample sample = sampleVeinMaterial(spec_, position, spec_.material_profiles);
  return sample.profile != nullptr ? sample.material : VoxelCaveMaterial::Rock;
}

const VoxelMaterialProfile *VoxelCaveState::profileFor(const VoxelCaveMaterial material) const {
  for (const VoxelMaterialProfile &profile : spec_.material_profiles) {
    if (profile.material == material) {
      return &profile;
    }
  }
  if (material == VoxelCaveMaterial::Rock) {
    static const VoxelMaterialProfile rock_profile{
        .material = VoxelCaveMaterial::Rock,
        .seed = 0u,
        .display_name = "Rock",
        .visual_material_id = "rock",
        .min_tool_tier = 0,
        .hardness = 1.0f,
        .resource_item_id = "stone",
        .resource_quantity = 1,
    };
    return &rock_profile;
  }
  return nullptr;
}

VoxelCaveHit VoxelCaveState::raycast(Vec3 origin, Vec3 direction, const float max_distance) const {
  direction = normalize(direction);
  if (length(direction) <= 0.0001f || max_distance <= 0.0f) {
    return {};
  }

  const float step = std::max(spec_.cell_size * 0.33f, 0.05f);
  float previous_density = densityAt(origin);
  for (float distance = step; distance <= max_distance; distance += step) {
    const Vec3 point = origin + direction * distance;
    const float density = densityAt(point);
    if (previous_density <= 0.0f && density > 0.0f) {
      if (insideAnySurfaceOpening(spec_, point)) {
        previous_density = density;
        continue;
      }
      const float e = spec_.cell_size * 0.32f;
      Vec3 normal{densityAt(point + Vec3{e, 0.0f, 0.0f}) - densityAt(point - Vec3{e, 0.0f, 0.0f}),
                  densityAt(point + Vec3{0.0f, e, 0.0f}) - densityAt(point - Vec3{0.0f, e, 0.0f}),
                  densityAt(point + Vec3{0.0f, 0.0f, e}) - densityAt(point - Vec3{0.0f, 0.0f, e})};
      normal = normalize(normal * -1.0f);
      if (length(normal) <= 0.0001f) {
        normal = direction * -1.0f;
      }
      const Vec3 negative_sample = point - normal * spec_.cell_size * 0.65f;
      const Vec3 solid_direction = densityAt(negative_sample) > 0.0f ? normal * -1.0f : normal;
      const VeinMaterialSample vein_sample = sampleSurfaceVeinMaterial(
          spec_, point, solid_direction, spec_.cell_size, spec_.material_profiles);
      return {.hit = true,
              .point = point,
              .normal = normal,
              .distance = distance,
              .material =
                  vein_sample.profile != nullptr ? vein_sample.material : VoxelCaveMaterial::Rock,
              .chunk = chunkCoordFor(point),
              .cell = cellCoordFor(point)};
    }
    previous_density = density;
  }
  return {};
}

VoxelCaveInteriorSample VoxelCaveState::sampleInterior(const Vec3 position,
                                                       const VoxelCaveInteriorProbe probe) const {
  return sampleVoxelCaveInterior(spec_, position, densityAt(position), probe);
}

void VoxelCaveState::rebuildChunk(ChunkState &chunk) {
  ASTER_PROFILE_SCOPE("VoxelCaveState::chunkRebuild");
  std::vector<VoxelChunkRenderBatch> retiring_batches;
  if (chunk.coarse_proxy && spec_.chunk_transition_seconds > 0.0f) {
    retiring_batches = chunk.batches;
  }
  chunk.batches.clear();
  chunk.collision_mesh.reset();
  chunk.surface_stats = {};
  chunk.coarse_proxy = false;
  chunk.lifecycle = VoxelChunkLifecycle::FullBuilding;
  chunk.retiring_batches = std::move(retiring_batches);
  chunk.render_transition_age = 0.0f;
  chunk.render_transition_seconds =
      chunk.retiring_batches.empty() ? 0.0f : spec_.chunk_transition_seconds;
  ++chunk.mesh_generation;

  CpuMesh collision_mesh;
  SurfaceMeshIndexCache collision_cache;
  std::vector<std::pair<VoxelCaveMaterial, CpuMesh>> render_meshes;
  std::vector<std::pair<VoxelCaveMaterial, SurfaceMeshIndexCache>> render_caches;
  const int n = spec_.chunk_cells;
  const float s = spec_.cell_size;
  const VoxelCellCoord base{chunk.coord.x * n, chunk.coord.y * n, chunk.coord.z * n};
  const Vec3 chunk_origin =
      spec_.origin + Vec3{static_cast<float>(base.x) * s, static_cast<float>(base.y) * s,
                          static_cast<float>(base.z) * s};

  SurfaceExtractionResult surface;
  {
    ASTER_PROFILE_SCOPE("VoxelCaveState::chunkSurfaceExtract");
    SurfaceExtractionSettings settings = spec_.surface_extraction;
    settings.emit_collision_mesh = false;
    settings.emit_channel_meshes = false;
    settings.capture_faces = true;
    surface = extractImplicitSurface(
        {.origin = chunk_origin,
         .cell_size = s,
         .cell_count = {n, n, n},
         .settings = settings,
         .density = [this](const Vec3 point) { return densityAt(point); },
         .reject_face = [this](const SurfaceFace &face) {
           return insideAnySurfaceOpening(spec_, face.centroid);
         }});
  }

  ASTER_PROFILE_SCOPE("VoxelCaveState::chunkMeshBuild");
  chunk.surface_stats.active_cells = surface.stats.active_cells;
  for (const SurfaceFace &face : surface.faces) {
    appendSurfaceFace(collision_mesh, face, spec_.surface_extraction.uv_scale, &collision_cache);
    if (face.corner_count == 3) {
      appendSurfaceFace(meshForMaterial(render_meshes, VoxelCaveMaterial::Rock), face,
                        spec_.surface_extraction.uv_scale,
                        &cacheForMaterial(render_caches, VoxelCaveMaterial::Rock));
      continue;
    }
    if (face.corner_count != 4) {
      continue;
    }
    const std::array<SurfaceNetCell, 4> corners = caveSurfaceCorners(face);
    const VeinMaterialSample material_sample = sampleSurfaceVeinMaterial(
        spec_, face.centroid, face.preferred_normal * -1.0f, s, spec_.material_profiles);
    const VoxelCaveMaterial material =
        material_sample.profile != nullptr ? material_sample.material : VoxelCaveMaterial::Rock;
    if (material_sample.profile != nullptr && material != VoxelCaveMaterial::Rock) {
      meshForMaterial(render_meshes, VoxelCaveMaterial::Rock);
      meshForMaterial(render_meshes, material);
      CpuMesh &rock_mesh = meshForMaterial(render_meshes, VoxelCaveMaterial::Rock);
      CpuMesh &material_mesh = meshForMaterial(render_meshes, material);
      appendMaterialInterfaceQuad(rock_mesh, material_mesh, corners[0], corners[1], corners[2],
                                  corners[3], face.preferred_normal, *material_sample.profile,
                                  clamp(material_sample.strength, 0.0f, 1.0f));
      continue;
    }
    appendSurfaceFace(meshForMaterial(render_meshes, material), face, spec_.surface_extraction.uv_scale,
                      &cacheForMaterial(render_caches, material));
  }

  chunk.surface_stats.surface_vertices = static_cast<std::uint32_t>(collision_mesh.vertices.size());
  chunk.surface_stats.surface_triangles =
      static_cast<std::uint32_t>(collision_mesh.indices.size() / 3u);
  applyMeshBoundsToStats(chunk.surface_stats, collision_mesh);
  applyTopologyToStats(chunk.surface_stats, collision_mesh);
  for (const auto &entry : render_meshes) {
    appendMaterialStats(chunk.surface_stats, entry.first, entry.second);
  }
  chunk.surface_stats.material_batches =
      static_cast<std::uint32_t>(chunk.surface_stats.materials.size());

  if (collision_mesh.vertices.empty() || collision_mesh.indices.empty()) {
    chunk.lifecycle = VoxelChunkLifecycle::FullPublished;
    return;
  }

  auto collision_surface = std::make_shared<const CpuMesh>(std::move(collision_mesh));
  const bool structural_collision_enabled =
      spec_.structural_surface_mode == VoxelCaveStructuralSurfaceMode::RenderAndCollide ||
      spec_.structural_surface_mode == VoxelCaveStructuralSurfaceMode::CollisionOnly;
  const bool structural_render_enabled =
      spec_.structural_surface_mode == VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  chunk.collision_mesh = structural_collision_enabled ? collision_surface : nullptr;
  if (!structural_render_enabled) {
    chunk.lifecycle = VoxelChunkLifecycle::FullPublished;
    return;
  }

  for (auto &entry : render_meshes) {
    CpuMesh &surface_mesh = entry.second;
    if (surface_mesh.vertices.empty() || surface_mesh.indices.empty()) {
      continue;
    }
    const VoxelChunkRenderBatchKind kind = entry.first == VoxelCaveMaterial::Rock
                                               ? VoxelChunkRenderBatchKind::StructuralSurface
                                               : VoxelChunkRenderBatchKind::MaterialSurface;
    chunk.batches.push_back({chunk.coord, kind, entry.first,
                             materialSurfaceLabel(spec_.material_profiles, entry.first),
                             1.0f, std::make_shared<const CpuMesh>(std::move(surface_mesh))});
  }
  chunk.lifecycle = VoxelChunkLifecycle::FullPublished;
}

} // namespace aster
