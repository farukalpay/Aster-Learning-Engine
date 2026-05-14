#include "aster/geometry/voxel_cave.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

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

struct ProceduralFieldBasis {
  aster::Vec3 forward{0.0f, 0.0f, -1.0f};
  aster::Vec3 side{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
};

ProceduralFieldBasis proceduralBasis(const aster::VoxelCaveProceduralField &field) {
  ProceduralFieldBasis basis;
  basis.forward = normalizedOr(field.forward, {0.0f, 0.0f, -1.0f});
  basis.up = normalizedOr(field.up, {0.0f, 1.0f, 0.0f});
  basis.side = normalizedOr(aster::cross(basis.up, basis.forward), {1.0f, 0.0f, 0.0f});
  basis.up = normalizedOr(aster::cross(basis.forward, basis.side), {0.0f, 1.0f, 0.0f});
  return basis;
}

aster::Vec3 proceduralCenterAt(const aster::VoxelCaveProceduralField &field,
                               const ProceduralFieldBasis &basis, const float distance) {
  const float frequency = std::max(field.wander_frequency, 0.001f);
  const float domain = distance * frequency;
  const float side = (fbm3({domain, 4.70f, -2.10f}, field.seed + 911u, 4) - 0.5f) *
                     std::max(field.side_wander, 0.0f) * 2.0f;
  const float vertical = (fbm3({-1.60f, domain, 8.20f}, field.seed + 1291u, 4) - 0.5f) *
                         std::max(field.vertical_wander, 0.0f) * 2.0f;
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

float proceduralTunnelSdf(const aster::Vec3 point, const aster::VoxelCaveProceduralField &field,
                          const ProceduralFieldBasis &basis, const float distance) {
  const aster::Vec3 center = proceduralCenterAt(field, basis, distance);
  const aster::Vec2 radii = proceduralTunnelRadii(field, distance);
  const aster::Vec3 offset = point - center;
  const float lateral = aster::dot(offset, basis.side) / radii.x;
  const float vertical = aster::dot(offset, basis.up) / radii.y;
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
  const aster::Vec3 offset = point - center;
  const aster::Vec3 scaled{aster::dot(offset, basis.side) / side_radius,
                           aster::dot(offset, basis.up) / up_radius,
                           aster::dot(offset, basis.forward) / forward_radius};
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

  const float distance = std::max(projected, 0.0f);
  float density = proceduralTunnelSdf(point, field, basis, distance);
  if (field.chamber_spacing > 0.0f && field.chamber_radius > 0.0f) {
    const int chamber =
        static_cast<int>(std::floor(distance / std::max(field.chamber_spacing, 0.001f)));
    for (int offset = -1; offset <= 1; ++offset) {
      density = std::min(density, proceduralChamberSdf(point, field, basis, chamber + offset));
    }
  }
  return density;
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
  if (edits != nullptr) {
    for (const aster::VoxelEdit &edit : *edits) {
      if (edit.operation == aster::VoxelEditOperation::Carve) {
        density = std::min(density, aster::length(position - edit.center) - edit.radius);
      }
    }
  }
  return density;
}

std::uint32_t appendVertex(aster::CpuMesh &mesh, const aster::Vec3 position,
                           const aster::Vec3 normal, const aster::Vec2 uv) {
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({position, normal, uv});
  return index;
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

void appendSurfaceTriangle(aster::CpuMesh &mesh, const SurfaceNetCell &a, const SurfaceNetCell &b,
                           const SurfaceNetCell &c, aster::Vec3 preferred_normal) {
  preferred_normal = aster::normalize(preferred_normal);
  if (aster::length(preferred_normal) <= 0.0001f) {
    preferred_normal = aster::normalize(a.normal + b.normal + c.normal);
  }
  if (aster::length(preferred_normal) <= 0.0001f) {
    preferred_normal = {0.0f, 1.0f, 0.0f};
  }
  const aster::Vec3 face_normal =
      aster::normalize(aster::cross(b.position - a.position, c.position - a.position));
  if (aster::length(face_normal) <= 0.0001f) {
    return;
  }
  if (aster::dot(face_normal, preferred_normal) < 0.0f) {
    const std::uint32_t ia =
        appendVertex(mesh, a.position, a.normal, {a.position.x * 0.12f, a.position.z * 0.12f});
    const std::uint32_t ib =
        appendVertex(mesh, c.position, c.normal, {c.position.x * 0.12f, c.position.z * 0.12f});
    const std::uint32_t ic =
        appendVertex(mesh, b.position, b.normal, {b.position.x * 0.12f, b.position.z * 0.12f});
    mesh.indices.insert(mesh.indices.end(), {ia, ib, ic});
    return;
  }
  const std::uint32_t ia =
      appendVertex(mesh, a.position, a.normal, {a.position.x * 0.12f, a.position.z * 0.12f});
  const std::uint32_t ib =
      appendVertex(mesh, b.position, b.normal, {b.position.x * 0.12f, b.position.z * 0.12f});
  const std::uint32_t ic =
      appendVertex(mesh, c.position, c.normal, {c.position.x * 0.12f, c.position.z * 0.12f});
  mesh.indices.insert(mesh.indices.end(), {ia, ib, ic});
}

void appendSurfaceQuad(aster::CpuMesh &mesh, const SurfaceNetCell &a, const SurfaceNetCell &b,
                       const SurfaceNetCell &c, const SurfaceNetCell &d,
                       const aster::Vec3 preferred_normal) {
  appendSurfaceTriangle(mesh, a, b, d, preferred_normal);
  appendSurfaceTriangle(mesh, b, c, d, preferred_normal);
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

void appendMaterialInterfaceQuad(aster::CpuMesh &collision_mesh, aster::CpuMesh &rock_mesh,
                                 aster::CpuMesh &material_mesh, const SurfaceNetCell &a,
                                 const SurfaceNetCell &b, const SurfaceNetCell &c,
                                 const SurfaceNetCell &d, const aster::Vec3 preferred_normal,
                                 const aster::VoxelMaterialProfile &profile, const float strength) {
  const float denominator = std::max(1.0f - profile.surface_exposure_threshold, 0.001f);
  const float exposure =
      aster::clamp((strength - profile.surface_exposure_threshold) / denominator, 0.0f, 1.0f);
  if (exposure <= 0.0f || profile.surface_relief <= 0.0001f) {
    appendSurfaceQuad(collision_mesh, a, b, c, d, preferred_normal);
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

  append_rock_ring(collision_mesh);
  append_ore_lens(collision_mesh);
  append_rock_ring(rock_mesh);
  append_ore_lens(material_mesh);
}

struct CaveFrameCandidate {
  bool valid = false;
  bool procedural = false;
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

ProceduralFieldBasis basisFromForwardUp(const aster::Vec3 forward, const aster::Vec3 up) {
  ProceduralFieldBasis basis;
  basis.forward = normalizedOr(forward, {0.0f, 0.0f, -1.0f});
  basis.up = normalizedOr(up, {0.0f, 1.0f, 0.0f});
  basis.side = normalizedOr(aster::cross(basis.up, basis.forward), {1.0f, 0.0f, 0.0f});
  basis.up = normalizedOr(aster::cross(basis.forward, basis.side), {0.0f, 1.0f, 0.0f});
  return basis;
}

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
                                      const float procedural_distance = 0.0f) {
  const float radius = std::max(tunnel_radius, 0.001f);
  const float height = std::max(vertical_radius, 0.001f);
  const aster::Vec3 offset = position - center;
  const float lateral = aster::dot(offset, basis.side);
  const float vertical = aster::dot(offset, basis.up);
  const float radial_fraction = std::sqrt((lateral * lateral) / (radius * radius) +
                                          (vertical * vertical) / (height * height));
  return {.valid = true,
          .procedural = procedural,
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
  for (const aster::VoxelCaveProceduralField &field : spec.procedural_fields) {
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
        frame.distance);
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
    mount = surface;
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
      const float strength =
          veinStrengthAt(surface_point + solid_direction * depth, spec.origin, profile) *
          progress_weight;
      if (strength > sample.strength) {
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
  const Vec2 radii = proceduralTunnelRadii(field, clamped_distance);
  return {.valid = true,
          .center = proceduralCenterAt(field, basis, clamped_distance),
          .forward = basis.forward,
          .side = basis.side,
          .up = basis.up,
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
  return sampleVoxelCaveProceduralFrameAt(field, std::max(projected, 0.0f));
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
  sample.interior = aster::clamp(interior, 0.0f, 1.0f);
  sample.entrance_light = aster::clamp(entrance, 0.0f, 1.0f);
  sample.depth =
      aster::clamp(frame.progress_distance / depth_reference, 0.0f, 1.0f) * sample.interior;
  sample.chamber = chamberWeightAt(spec, position);
  sample.progress_distance = frame.progress_distance;
  sample.progress_normalized =
      path_length > 0.001f ? aster::clamp(frame.progress_distance / path_length, 0.0f, 1.0f) : 0.0f;
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
  spec_ = std::move(spec);
  clear();
}

void VoxelCaveState::clear() {
  chunks_.clear();
  edits_.clear();
  snapshots_.clear();
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
    chunk.collision_dirty = true;
  }
  snapshots_.clear();
}

const std::vector<VoxelChunkSnapshot> &VoxelCaveState::activeChunks() const {
  return snapshots_;
}

std::optional<VoxelChunkSnapshot> VoxelCaveState::consumeDirtyChunk(const VoxelChunkCoord coord) {
  for (ChunkState &chunk : chunks_) {
    if (chunk.coord == coord && chunk.collision_dirty) {
      chunk.collision_dirty = false;
      return VoxelChunkSnapshot{chunk.coord,          chunk.center,         chunk.reveal_age,
                                chunk.newly_revealed, chunk.dirty,          true,
                                chunk.batches,        chunk.collision_mesh, chunk.surface_stats};
    }
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

void VoxelCaveState::ensureChunk(const VoxelChunkCoord coord) {
  const auto found = std::find_if(chunks_.begin(), chunks_.end(), [coord](const ChunkState &chunk) {
    return chunk.coord == coord;
  });
  if (found != chunks_.end()) {
    return;
  }

  ChunkState chunk;
  chunk.coord = coord;
  chunk.center = chunkCenter(coord);
  chunk.newly_revealed = true;
  chunks_.push_back(std::move(chunk));
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
  const VoxelChunkCoord viewer = chunkCoordFor(viewer_position);
  for (int z = -spec_.stream_radius; z <= spec_.stream_radius; ++z) {
    for (int y = -spec_.stream_radius; y <= spec_.stream_radius; ++y) {
      for (int x = -spec_.stream_radius; x <= spec_.stream_radius; ++x) {
        ensureChunk({viewer.x + x, viewer.y + y, viewer.z + z});
      }
    }
  }

  chunks_.erase(std::remove_if(chunks_.begin(), chunks_.end(),
                               [&](const ChunkState &chunk) {
                                 return chunkDistance(chunk.coord, viewer) > spec_.unload_radius;
                               }),
                chunks_.end());

  std::vector<ChunkState *> dirty_chunks;
  dirty_chunks.reserve(chunks_.size());
  for (ChunkState &chunk : chunks_) {
    if (chunk.dirty) {
      dirty_chunks.push_back(&chunk);
    }
  }
  std::sort(dirty_chunks.begin(), dirty_chunks.end(),
            [&](const ChunkState *lhs, const ChunkState *rhs) {
              const int lhs_distance = chunkDistance(lhs->coord, viewer);
              const int rhs_distance = chunkDistance(rhs->coord, viewer);
              if (lhs_distance != rhs_distance) {
                return lhs_distance < rhs_distance;
              }
              return length(lhs->center - viewer_position) < length(rhs->center - viewer_position);
            });

  int remaining_rebuilds = spec_.max_chunk_rebuilds_per_update <= 0
                               ? std::numeric_limits<int>::max()
                               : spec_.max_chunk_rebuilds_per_update;
  for (ChunkState *chunk : dirty_chunks) {
    const bool forced =
        chunkDistance(chunk->coord, viewer) <= std::max(spec_.forced_rebuild_radius, 0);
    if (!forced && remaining_rebuilds <= 0) {
      continue;
    }
    rebuildChunk(*chunk);
    chunk->dirty = false;
    chunk->collision_dirty = true;
    if (!forced && remaining_rebuilds < std::numeric_limits<int>::max()) {
      --remaining_rebuilds;
    }
  }

  snapshots_.clear();
  snapshots_.reserve(chunks_.size());
  for (ChunkState &chunk : chunks_) {
    chunk.reveal_age += std::max(dt, 0.0f);
    const bool published_collision_dirty = !chunk.dirty && chunk.collision_dirty;
    snapshots_.push_back({chunk.coord, chunk.center, chunk.reveal_age, chunk.newly_revealed,
                          chunk.dirty, published_collision_dirty, chunk.batches,
                          chunk.collision_mesh, chunk.surface_stats});
    chunk.newly_revealed = false;
  }
}

void VoxelCaveState::applyEdit(const VoxelEdit &edit) {
  if (edit.radius <= 0.0f) {
    return;
  }
  if (edit.operation != VoxelEditOperation::Carve) {
    return;
  }
  edits_.push_back(edit);
  for (ChunkState &chunk : chunks_) {
    if (chunkIntersectsEdit(chunk.coord, edit)) {
      chunk.dirty = true;
      chunk.collision_dirty = true;
    }
  }
}

float VoxelCaveState::densityAt(const Vec3 position) const {
  return densityAtSpec(spec_, position, &edits_);
}

VoxelCaveMaterial VoxelCaveState::materialAt(const Vec3 position) const {
  if (densityAt(position) <= 0.0f) {
    return VoxelCaveMaterial::Air;
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
  chunk.batches.clear();
  chunk.collision_mesh.reset();
  chunk.surface_stats = {};

  CpuMesh collision_mesh;
  std::vector<std::pair<VoxelCaveMaterial, CpuMesh>> render_meshes;
  const int n = spec_.chunk_cells;
  const float s = spec_.cell_size;
  const VoxelCellCoord base{chunk.coord.x * n, chunk.coord.y * n, chunk.coord.z * n};
  const int cell_grid = n + 1;
  constexpr int cube_offsets[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
  constexpr int cube_edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                     {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

  const auto cellIndex = [cell_grid](const int x, const int y, const int z) {
    return static_cast<std::size_t>((z * cell_grid + y) * cell_grid + x);
  };
  const auto gridPoint = [&](const int x, const int y, const int z) {
    return spec_.origin + Vec3{static_cast<float>(base.x + x) * s,
                               static_cast<float>(base.y + y) * s,
                               static_cast<float>(base.z + z) * s};
  };
  const auto edgeCrossesSurface = [&](const Vec3 a, const Vec3 b) {
    const float da = densityAt(a);
    const float db = densityAt(b);
    return (da > 0.0f) != (db > 0.0f);
  };

  std::vector<SurfaceNetCell> cells(static_cast<std::size_t>(cell_grid * cell_grid * cell_grid));
  for (int z = 0; z <= n; ++z) {
    for (int y = 0; y <= n; ++y) {
      for (int x = 0; x <= n; ++x) {
        std::array<Vec3, 8> p{};
        std::array<float, 8> d{};
        for (int i = 0; i < 8; ++i) {
          const Vec3 cell{static_cast<float>(base.x + x + cube_offsets[i][0]) * s,
                          static_cast<float>(base.y + y + cube_offsets[i][1]) * s,
                          static_cast<float>(base.z + z + cube_offsets[i][2]) * s};
          p[i] = spec_.origin + cell;
          d[i] = densityAt(p[i]);
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
          const bool solid_a = d[a] > 0.0f;
          const bool solid_b = d[b] > 0.0f;
          if (solid_a == solid_b) {
            continue;
          }
          const float denom = d[a] - d[b];
          const float t = std::abs(denom) > kEpsilon ? clamp(d[a] / denom, 0.0f, 1.0f) : 0.5f;
          intersection_sum = intersection_sum + p[a] + (p[b] - p[a]) * t;
          ++intersection_count;
        }
        if (intersection_count <= 0) {
          continue;
        }

        SurfaceNetCell cell;
        cell.active = true;
        cell.position = intersection_sum / static_cast<float>(intersection_count);
        const float e = s * 0.35f;
        Vec3 normal{densityAt(cell.position - Vec3{e, 0.0f, 0.0f}) -
                        densityAt(cell.position + Vec3{e, 0.0f, 0.0f}),
                    densityAt(cell.position - Vec3{0.0f, e, 0.0f}) -
                        densityAt(cell.position + Vec3{0.0f, e, 0.0f}),
                    densityAt(cell.position - Vec3{0.0f, 0.0f, e}) -
                        densityAt(cell.position + Vec3{0.0f, 0.0f, e})};
        normal = normalize(normal);
        cell.normal = length(normal) > 0.0001f ? normal : Vec3{0.0f, 1.0f, 0.0f};
        cells[cellIndex(x, y, z)] = cell;
        ++chunk.surface_stats.active_cells;
      }
    }
  }

  const auto activeCell = [&](const int x, const int y, const int z) -> const SurfaceNetCell * {
    if (x < 0 || y < 0 || z < 0 || x > n || y > n || z > n) {
      return nullptr;
    }
    const SurfaceNetCell &cell = cells[cellIndex(x, y, z)];
    return cell.active ? &cell : nullptr;
  };
  const auto appendFace = [&](const SurfaceNetCell *a, const SurfaceNetCell *b,
                              const SurfaceNetCell *c, const SurfaceNetCell *d) {
    if (a == nullptr || b == nullptr || c == nullptr || d == nullptr) {
      return;
    }
    Vec3 preferred = normalize(a->normal + b->normal + c->normal + d->normal);
    if (length(preferred) <= 0.0001f) {
      preferred = {0.0f, 1.0f, 0.0f};
    }
    const Vec3 centroid = (a->position + b->position + c->position + d->position) * 0.25f;
    if (insideAnySurfaceOpening(spec_, centroid)) {
      return;
    }
    const VeinMaterialSample material_sample =
        sampleSurfaceVeinMaterial(spec_, centroid, preferred * -1.0f, s, spec_.material_profiles);
    const VoxelCaveMaterial material =
        material_sample.profile != nullptr ? material_sample.material : VoxelCaveMaterial::Rock;
    if (material_sample.profile != nullptr && material != VoxelCaveMaterial::Rock) {
      meshForMaterial(render_meshes, VoxelCaveMaterial::Rock);
      meshForMaterial(render_meshes, material);
      CpuMesh &rock_mesh = meshForMaterial(render_meshes, VoxelCaveMaterial::Rock);
      CpuMesh &material_mesh = meshForMaterial(render_meshes, material);
      appendMaterialInterfaceQuad(collision_mesh, rock_mesh, material_mesh, *a, *b, *c, *d,
                                  preferred, *material_sample.profile,
                                  clamp(material_sample.strength, 0.0f, 1.0f));
      return;
    }
    appendSurfaceQuad(collision_mesh, *a, *b, *c, *d, preferred);
    appendSurfaceQuad(meshForMaterial(render_meshes, material), *a, *b, *c, *d, preferred);
  };

  for (int z = 1; z <= n; ++z) {
    for (int y = 1; y <= n; ++y) {
      for (int x = 0; x < n; ++x) {
        if (edgeCrossesSurface(gridPoint(x, y, z), gridPoint(x + 1, y, z))) {
          appendFace(activeCell(x, y - 1, z - 1), activeCell(x, y, z - 1), activeCell(x, y, z),
                     activeCell(x, y - 1, z));
        }
      }
    }
  }
  for (int z = 1; z <= n; ++z) {
    for (int y = 0; y < n; ++y) {
      for (int x = 1; x <= n; ++x) {
        if (edgeCrossesSurface(gridPoint(x, y, z), gridPoint(x, y + 1, z))) {
          appendFace(activeCell(x - 1, y, z - 1), activeCell(x - 1, y, z), activeCell(x, y, z),
                     activeCell(x, y, z - 1));
        }
      }
    }
  }
  for (int z = 0; z < n; ++z) {
    for (int y = 1; y <= n; ++y) {
      for (int x = 1; x <= n; ++x) {
        if (edgeCrossesSurface(gridPoint(x, y, z), gridPoint(x, y, z + 1))) {
          appendFace(activeCell(x - 1, y - 1, z), activeCell(x, y - 1, z), activeCell(x, y, z),
                     activeCell(x - 1, y, z));
        }
      }
    }
  }

  chunk.surface_stats.surface_vertices = static_cast<std::uint32_t>(collision_mesh.vertices.size());
  chunk.surface_stats.surface_triangles =
      static_cast<std::uint32_t>(collision_mesh.indices.size() / 3u);
  if (!collision_mesh.vertices.empty()) {
    chunk.surface_stats.bounds_min = collision_mesh.vertices.front().position;
    chunk.surface_stats.bounds_max = collision_mesh.vertices.front().position;
    for (const Vertex &vertex : collision_mesh.vertices) {
      chunk.surface_stats.bounds_min.x =
          std::min(chunk.surface_stats.bounds_min.x, vertex.position.x);
      chunk.surface_stats.bounds_min.y =
          std::min(chunk.surface_stats.bounds_min.y, vertex.position.y);
      chunk.surface_stats.bounds_min.z =
          std::min(chunk.surface_stats.bounds_min.z, vertex.position.z);
      chunk.surface_stats.bounds_max.x =
          std::max(chunk.surface_stats.bounds_max.x, vertex.position.x);
      chunk.surface_stats.bounds_max.y =
          std::max(chunk.surface_stats.bounds_max.y, vertex.position.y);
      chunk.surface_stats.bounds_max.z =
          std::max(chunk.surface_stats.bounds_max.z, vertex.position.z);
    }
  }
  for (const auto &entry : render_meshes) {
    if (entry.second.vertices.empty() || entry.second.indices.empty()) {
      continue;
    }
    chunk.surface_stats.materials.push_back(
        {entry.first, static_cast<std::uint32_t>(entry.second.vertices.size()),
         static_cast<std::uint32_t>(entry.second.indices.size() / 3u)});
  }
  chunk.surface_stats.material_batches =
      static_cast<std::uint32_t>(chunk.surface_stats.materials.size());

  if (collision_mesh.vertices.empty() || collision_mesh.indices.empty()) {
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
                             std::make_shared<const CpuMesh>(std::move(surface_mesh))});
  }
}

} // namespace aster
