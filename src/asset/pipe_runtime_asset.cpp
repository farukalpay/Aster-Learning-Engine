// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/pipe_runtime_asset.hpp"

#include "aster/geometry/tube_mesh.hpp"
#include "aster/material/procedural_surface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = kPi * 2.0f;

[[nodiscard]] float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) {
  const float t = saturate((value - edge0) / std::max(edge1 - edge0, 0.0001f));
  return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] float hash01(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

[[nodiscard]] std::uint64_t fnv1a64(const std::string &value) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : value) {
    hash ^= static_cast<std::uint8_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

void appendIndexQuad(CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                     const std::uint32_t c, const std::uint32_t d) {
  mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
}

std::uint32_t appendVertex(CpuMesh &mesh, const Vec3 position, const Vec3 normal, const Vec2 uv,
                           const float ao = 1.0f) {
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({position, normalize(normal), uv, {1.0f, 0.0f, 0.0f, 1.0f}, ao});
  return index;
}

void appendMesh(CpuMesh &target, const CpuMesh &source, const Vec3 offset = {}) {
  const std::uint32_t base = static_cast<std::uint32_t>(target.vertices.size());
  target.vertices.reserve(target.vertices.size() + source.vertices.size());
  target.indices.reserve(target.indices.size() + source.indices.size());
  for (Vertex vertex : source.vertices) {
    vertex.position += offset;
    target.vertices.push_back(vertex);
  }
  for (const std::uint32_t index : source.indices) {
    target.indices.push_back(base + index);
  }
}

[[nodiscard]] CpuMesh translated(CpuMesh mesh, const Vec3 offset) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.position += offset;
  }
  return mesh;
}

struct BoundsBuilder {
  Vec3 min{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::infinity()};
  Vec3 max{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
           -std::numeric_limits<float>::infinity()};
  bool valid = false;

  void add(const Vec3 point) {
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
    valid = true;
  }

  [[nodiscard]] Vec3 center() const {
    return (min + max) * 0.5f;
  }

  [[nodiscard]] Vec3 halfExtents() const {
    return (max - min) * 0.5f;
  }
};

[[nodiscard]] BoundsBuilder boundsFor(const std::vector<AsterPipeAssetPart> &parts) {
  BoundsBuilder bounds;
  for (const AsterPipeAssetPart &part : parts) {
    for (const Vertex &vertex : part.mesh.vertices) {
      bounds.add(vertex.position);
    }
  }
  return bounds;
}

[[nodiscard]] ProceduralSurfaceLayer pipeSurfaceLayerFromSpec(const AsterPipeAssetSpec &spec) {
  return {.macro_variation = 0.72f,
          .micro_normal_strength = 0.60f,
          .roughness_variation = 0.74f,
          .physical_texel_density = 1024.0f,
          .height_normal_coupling = 0.92f,
          .roughness_height_coupling = 0.74f,
          .macro_frequency_breakup = 0.48f,
          .micro_frequency_breakup = 0.66f,
          .wetness = spec.wetness_strength,
          .height_shading = 0.46f,
          .pitting_density = spec.pitting_density,
          .pitting_depth = spec.pitting_depth,
          .oxide_layering = spec.oxide_layering * spec.rust_strength,
          .cavity_grime = spec.cavity_grime_strength,
          .edge_polish = spec.edge_polish_strength,
          .weld_heat_tint = spec.weld_heat_tint_strength,
          .axial_scratches = spec.axial_scratch_strength,
          .wet_streaks = static_cast<float>(std::max(spec.wet_streak_count, 0)) / 7.0f,
          .rust_bloom = spec.rust_bloom_strength,
          .black_scab = spec.black_scab_strength,
          .paint_remnant = spec.paint_remnant_strength,
          .weld_slag = spec.weld_slag_strength,
          .rim_soot = spec.rim_soot_strength};
}

[[nodiscard]] float axialBandMask(const float x, const float center, const float inner,
                                  const float outer) {
  return 1.0f - smoothstep(inner, outer, std::abs(x - center));
}

AsterPipeSurfaceMask summarizeSurfaceMask(const AsterPipeAssetPart &part,
                                          const AsterPipeAssetSpec &spec) {
  AsterPipeSurfaceMask mask;
  mask.id = "mask." + part.material_slot + "." + part.name;
  std::replace(mask.id.begin(), mask.id.end(), ' ', '-');
  mask.target_part = part.name;
  if (part.mesh.vertices.empty()) {
    return mask;
  }

  const ProceduralSurfaceLayer layer = pipeSurfaceLayerFromSpec(spec);
  float pit = 0.0f;
  float cavity = 0.0f;
  float wet = 0.0f;
  float edge = 0.0f;
  float oxide = 0.0f;
  for (const Vertex &vertex : part.mesh.vertices) {
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = vertex.position,
                                .normal = vertex.normal,
                                .uv = vertex.uv,
                                .detail_scale = part.material_slot == "pipe.hardware" ? 13.5f
                                                                                       : 11.25f},
                               layer, spec.edge_polish_strength, spec.oxide_layering);
    pit += signals.pit > 0.08f ? 1.0f : 0.0f;
    cavity += signals.cavity_grime > 0.34f ? 1.0f : 0.0f;
    wet += signals.wet_film > 0.30f ? 1.0f : 0.0f;
    edge += signals.edge_polish > 0.38f ? 1.0f : 0.0f;
    oxide += signals.broad_oxide > 0.38f ? 1.0f : 0.0f;
  }
  const float count = static_cast<float>(part.mesh.vertices.size());
  mask.pitting_coverage = pit / count;
  mask.cavity_coverage = cavity / count;
  mask.wetness_coverage = wet / count;
  mask.edge_wear_coverage = edge / count;
  mask.oxide_coverage = oxide / count;
  return mask;
}

void applyPipeSurfaceAuthoring(CpuMesh &mesh, const AsterPipeAssetSpec &spec) {
  const ProceduralSurfaceLayer layer = pipeSurfaceLayerFromSpec(spec);
  const float half = spec.length * 0.5f;
  for (std::size_t i = 0u; i < mesh.vertices.size(); ++i) {
    Vertex &vertex = mesh.vertices[i];
    const float radial = std::sqrt(vertex.position.y * vertex.position.y +
                                   vertex.position.z * vertex.position.z);
    if (radial <= 0.0001f) {
      continue;
    }
    const float theta = std::atan2(vertex.position.z, vertex.position.y);
    const Vec3 outward{0.0f, vertex.position.y / radial, vertex.position.z / radial};
    const float rim_mask =
        spec.include_chipped_rims
            ? smoothstep(half - 0.22f, half - 0.018f, std::abs(vertex.position.x))
            : 0.0f;
    const float seam_mask =
        1.0f - smoothstep(0.035f, 0.20f, std::abs(std::sin(theta - 1.92f)));
    const float weld_mask = std::max(axialBandMask(vertex.position.x, -1.55f, 0.035f, 0.22f),
                                     axialBandMask(vertex.position.x, 1.42f, 0.035f, 0.22f));
    const float flange_mask =
        std::max(axialBandMask(vertex.position.x, -half + 0.22f, 0.025f, 0.24f),
                 axialBandMask(vertex.position.x, half - 0.22f, 0.025f, 0.24f));
    const AsterPipeSurfaceSignals signals =
        sampleAsterPipeSurface({.position = vertex.position,
                                .normal = vertex.normal,
                                .uv = vertex.uv,
                                .detail_scale = 11.25f},
                               layer, spec.edge_polish_strength, spec.oxide_layering);
    const float hash = hash01(spec.seed ^ static_cast<std::uint32_t>(i * 747796405u));
    const float corrosion =
        saturate(signals.broad_oxide * 0.44f + signals.pit * 0.32f + hash * 0.18f +
                 seam_mask * 0.18f + weld_mask * 0.20f + flange_mask * 0.12f);
    const float cavity = saturate(signals.cavity_grime + seam_mask * 0.24f + weld_mask * 0.34f +
                                  flange_mask * 0.20f);
    const float rim_wave = 0.5f + 0.5f * std::sin(theta * 9.0f + signals.broad_oxide * 2.8f);
    const float chip_depth =
        rim_mask * (0.0020f + signals.pit * 0.0055f + rim_wave * 0.0020f) *
        saturate(spec.rust_strength);
    const float pitting =
        (signals.pit * 0.18f + signals.pit_edge * 0.06f) * spec.pitting_depth *
        saturate(spec.rust_strength);
    const Vec3 scratch_tangent{0.0f, -outward.z, outward.y};
    vertex.position -= outward * (chip_depth + pitting * (0.30f + cavity * 0.42f));
    vertex.normal = normalize(vertex.normal + outward * (0.08f + seam_mask * 0.10f) +
                              scratch_tangent * ((signals.axial_scratch - 0.5f) * 0.035f));
    if (rim_mask > 0.0f && spec.rim_normal_feather > 0.0f) {
      const float side = vertex.position.x >= 0.0f ? 1.0f : -1.0f;
      const Vec3 cap_axis{side, 0.0f, 0.0f};
      const float inner_radius = std::max(spec.outer_radius - spec.wall_thickness, 0.0f);
      const float radial_mid = (spec.outer_radius + inner_radius) * 0.5f;
      const Vec3 rim_radial = radial >= radial_mid ? outward : outward * -1.0f;
      const float cap_weight = smoothstep(0.18f, 0.82f, std::abs(vertex.normal.x));
      const float feather = saturate(rim_mask * spec.rim_normal_feather * (0.22f + cap_weight));
      const Vec3 rounded_normal = normalize(rim_radial * 0.78f + cap_axis * 0.32f);
      vertex.normal = normalize(vertex.normal * (1.0f - feather) + rounded_normal * feather);
    }
    vertex.ambient_occlusion =
        std::clamp(0.95f - corrosion * 0.13f - cavity * 0.18f - rim_mask * 0.07f -
                       seam_mask * 0.08f - weld_mask * 0.07f,
                   0.52f, 1.0f);
    vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
  }
}

[[nodiscard]] CpuMesh makeAsterPipeBodyMesh(const AsterPipeAssetSpec &spec) {
  const float wall = std::clamp(spec.wall_thickness, 0.0f, spec.outer_radius * 0.86f);
  const float inner_radius = spec.outer_radius - wall;
  const float bevel = std::min({wall * 0.42f, spec.outer_radius * 0.070f, spec.length * 0.018f});
  if (wall <= 0.0f || bevel <= 0.001f) {
    return makeTubeMesh({.length = spec.length,
                         .outer_radius = spec.outer_radius,
                         .wall_thickness = spec.wall_thickness,
                         .radial_segments = spec.radial_segments,
                         .length_segments = spec.length_segments});
  }

  CpuMesh mesh;
  const float half = spec.length * 0.5f;
  const int radial_columns = spec.radial_segments + 1;
  const int axial_columns = spec.length_segments + 1;
  const float main_length = std::max(spec.length - bevel * 2.0f, spec.length * 0.72f);
  mesh.vertices.reserve(static_cast<std::size_t>(radial_columns * axial_columns * 2 +
                                                 radial_columns * 12));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.length_segments * 12 +
                                                spec.radial_segments * 60));

  const auto outer_at = [radial_columns](const int axial, const int radial) {
    return static_cast<std::uint32_t>(axial * radial_columns + radial);
  };
  for (int axial = 0; axial <= spec.length_segments; ++axial) {
    const float u = static_cast<float>(axial) / static_cast<float>(spec.length_segments);
    const float x = -main_length * 0.5f + main_length * u;
    for (int radial = 0; radial <= spec.radial_segments; ++radial) {
      const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float theta = v * kTau;
      const Vec3 outward{0.0f, std::cos(theta), std::sin(theta)};
      appendVertex(mesh, {x, outward.y * spec.outer_radius, outward.z * spec.outer_radius},
                   outward, {u, v});
    }
  }
  for (int axial = 0; axial < spec.length_segments; ++axial) {
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      appendIndexQuad(mesh, outer_at(axial, radial), outer_at(axial + 1, radial),
                      outer_at(axial + 1, radial + 1), outer_at(axial, radial + 1));
    }
  }

  const std::uint32_t inner_start = static_cast<std::uint32_t>(mesh.vertices.size());
  const auto inner_at = [inner_start, radial_columns](const int axial, const int radial) {
    return inner_start + static_cast<std::uint32_t>(axial * radial_columns + radial);
  };
  for (int axial = 0; axial <= spec.length_segments; ++axial) {
    const float u = static_cast<float>(axial) / static_cast<float>(spec.length_segments);
    const float x = -main_length * 0.5f + main_length * u;
    for (int radial = 0; radial <= spec.radial_segments; ++radial) {
      const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float theta = v * kTau;
      const Vec3 inward{0.0f, -std::cos(theta), -std::sin(theta)};
      appendVertex(mesh, {x, -inward.y * inner_radius, -inward.z * inner_radius}, inward,
                   {u, v}, 0.76f);
    }
  }
  for (int axial = 0; axial < spec.length_segments; ++axial) {
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      appendIndexQuad(mesh, inner_at(axial, radial + 1), inner_at(axial + 1, radial + 1),
                      inner_at(axial + 1, radial), inner_at(axial, radial));
    }
  }

  struct RimProfile {
    float axial = 0.0f;
    float radius = 0.0f;
    float axis_weight = 0.0f;
    float radial_sign = 1.0f;
    float ao = 1.0f;
  };
  const std::array<RimProfile, 6> profile = {{
      {half - bevel, spec.outer_radius, 0.05f, 1.0f, 0.90f},
      {half - bevel * 0.44f, spec.outer_radius - bevel * 0.11f, 0.34f, 1.0f, 0.84f},
      {half, spec.outer_radius - bevel * 0.58f, 0.92f, 1.0f, 0.78f},
      {half, inner_radius + bevel * 0.58f, 0.92f, -1.0f, 0.62f},
      {half - bevel * 0.44f, inner_radius + bevel * 0.11f, 0.34f, -1.0f, 0.66f},
      {half - bevel, inner_radius, 0.05f, -1.0f, 0.70f},
  }};
  constexpr int kRimProfileColumns = 6;
  for (const float side : {-1.0f, 1.0f}) {
    const std::uint32_t start = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto rim_at = [start](const int radial, const int profile_index) {
      return start + static_cast<std::uint32_t>(radial * kRimProfileColumns + profile_index);
    };
    for (int radial = 0; radial <= spec.radial_segments; ++radial) {
      const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
      const float theta = v * kTau;
      const Vec3 outward{0.0f, std::cos(theta), std::sin(theta)};
      const Vec3 axis{side, 0.0f, 0.0f};
      for (int p = 0; p < kRimProfileColumns; ++p) {
        const RimProfile &point = profile[static_cast<std::size_t>(p)];
        const Vec3 radial_normal = outward * point.radial_sign;
        const Vec3 normal =
            normalize(radial_normal * (1.0f - point.axis_weight) + axis * point.axis_weight);
        const float x = side * point.axial;
        appendVertex(mesh, {x, outward.y * point.radius, outward.z * point.radius}, normal,
                     {static_cast<float>(p) / static_cast<float>(kRimProfileColumns - 1), v},
                     point.ao);
      }
    }
    for (int radial = 0; radial < spec.radial_segments; ++radial) {
      for (int p = 0; p + 1 < kRimProfileColumns; ++p) {
        if (side > 0.0f) {
          appendIndexQuad(mesh, rim_at(radial, p), rim_at(radial + 1, p),
                          rim_at(radial + 1, p + 1), rim_at(radial, p + 1));
        } else {
          appendIndexQuad(mesh, rim_at(radial + 1, p), rim_at(radial, p),
                          rim_at(radial, p + 1), rim_at(radial + 1, p + 1));
        }
      }
    }
  }

  return mesh;
}

[[nodiscard]] CpuMesh makeSeamBandMesh(const AsterPipeAssetSpec &spec) {
  CpuMesh mesh;
  const int rows = std::max(4, spec.length_segments);
  constexpr int kColumns = 3;
  const float half = spec.length * 0.5f;
  const float seam_angle = 4.38f;
  const float half_width = 0.018f;
  const float radius = spec.outer_radius + std::max(spec.attachment_clearance * 0.45f, 0.002f);
  mesh.vertices.reserve(static_cast<std::size_t>((rows + 1) * kColumns));
  mesh.indices.reserve(static_cast<std::size_t>(rows * (kColumns - 1) * 6));
  for (int row = 0; row <= rows; ++row) {
    const float u = static_cast<float>(row) / static_cast<float>(rows);
    const float x = -half + spec.length * u;
    const float streak =
        std::sin((u * 7.0f + hash01(spec.seed + 19u)) * kTau) * 0.004f * spec.rust_strength;
    for (int column = 0; column < kColumns; ++column) {
      const float side = static_cast<float>(column - 1);
      const float theta = seam_angle + side * half_width;
      const Vec3 normal{0.0f, std::cos(theta), std::sin(theta)};
      const float center_lift = column == 1 ? spec.seam_inset_depth * 0.22f : 0.0f;
      const float lift = radius + center_lift + streak;
      const float ao = column == 1 ? 0.78f : 0.86f;
      appendVertex(mesh, {x, normal.y * lift, normal.z * lift}, normal,
                   {u, 0.46f + side * 0.04f}, ao);
    }
  }
  const auto at = [](const int row, const int column) {
    return static_cast<std::uint32_t>(row * kColumns + column);
  };
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column + 1 < kColumns; ++column) {
      appendIndexQuad(mesh, at(row, column), at(row + 1, column), at(row + 1, column + 1),
                      at(row, column + 1));
    }
  }
  return mesh;
}

[[nodiscard]] CpuMesh makeWeldContactBandMesh(const AsterPipeAssetSpec &spec, const float axial) {
  struct ProfilePoint {
    float x = 0.0f;
    float lift = 0.0f;
    float ao = 1.0f;
  };
  const float bead_radius = 0.032f;
  const float half_width = 0.064f;
  const float skirt = std::max(spec.weld_contact_skirt_width, 0.0f);
  const float clearance = std::max(spec.attachment_clearance, 0.002f);
  const std::array<ProfilePoint, 9> profile = {{
      {-half_width - skirt, clearance * 0.38f, 0.58f},
      {-half_width - skirt * 0.42f, clearance * 0.62f, 0.54f},
      {-half_width, bead_radius * 0.34f, 0.62f},
      {-half_width * 0.46f, bead_radius * 0.82f, 0.76f},
      {0.0f, bead_radius * 1.12f, 0.82f},
      {half_width * 0.46f, bead_radius * 0.82f, 0.76f},
      {half_width, bead_radius * 0.34f, 0.62f},
      {half_width + skirt * 0.42f, clearance * 0.62f, 0.54f},
      {half_width + skirt, clearance * 0.38f, 0.58f},
  }};

  CpuMesh mesh;
  const int radial_columns = spec.radial_segments + 1;
  const int profile_columns = static_cast<int>(profile.size());
  mesh.vertices.reserve(static_cast<std::size_t>(radial_columns * profile_columns));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * (profile_columns - 1) * 6));

  for (int radial = 0; radial <= spec.radial_segments; ++radial) {
    const float v = static_cast<float>(radial) / static_cast<float>(spec.radial_segments);
    const float theta = v * kTau;
    const Vec3 radial_dir{0.0f, std::cos(theta), std::sin(theta)};
    const float bead_ripple = 0.5f + 0.5f * std::sin(theta * 42.0f + axial * 2.7f +
                                                     std::sin(theta * 7.0f) * 1.15f);
    const float bead_wander = std::sin(theta * 15.0f + axial * 3.3f) * 0.0045f;
    for (int p = 0; p < profile_columns; ++p) {
      const ProfilePoint &point = profile[static_cast<std::size_t>(p)];
      const ProfilePoint &prev = profile[static_cast<std::size_t>(std::max(p - 1, 0))];
      const ProfilePoint &next =
          profile[static_cast<std::size_t>(std::min(p + 1, profile_columns - 1))];
      const float center_weight =
          1.0f - smoothstep(0.12f, half_width + skirt * 0.35f, std::abs(point.x));
      const float dx = std::max(next.x - prev.x, 0.001f);
      const float slope = (next.lift - prev.lift) / dx;
      const float local_lift =
          point.lift + center_weight * (0.0030f + bead_ripple * 0.0080f);
      const float radius = spec.outer_radius + local_lift;
      const float local_x = axial + point.x + center_weight * bead_wander;
      const Vec3 axial_slope{std::clamp(-slope * 0.38f, -0.44f, 0.44f), 0.0f, 0.0f};
      const Vec3 ripple_slope{std::sin(theta * 42.0f) * center_weight * 0.12f, 0.0f, 0.0f};
      const Vec3 normal =
          normalize(radial_dir * (0.90f + local_lift * 0.85f) + axial_slope + ripple_slope);
      appendVertex(mesh, {local_x, radial_dir.y * radius, radial_dir.z * radius}, normal,
                   {static_cast<float>(p) / static_cast<float>(profile_columns - 1), v},
                   point.ao);
    }
  }

  const auto at = [](const int radial, const int profile_index) {
    return static_cast<std::uint32_t>(radial * 9 + profile_index);
  };
  for (int radial = 0; radial < spec.radial_segments; ++radial) {
    for (int p = 0; p + 1 < profile_columns; ++p) {
      appendIndexQuad(mesh, at(radial, p), at(radial + 1, p), at(radial + 1, p + 1),
                      at(radial, p + 1));
    }
  }
  return mesh;
}

void appendBoltHead(CpuMesh &mesh, const Vec3 center, const float half_length,
                    const float radius, const int sides, const float uv_u) {
  const std::uint32_t side_start = static_cast<std::uint32_t>(mesh.vertices.size());
  for (int x = 0; x <= 1; ++x) {
    const float side = x == 0 ? -1.0f : 1.0f;
    for (int i = 0; i <= sides; ++i) {
      const float v = static_cast<float>(i) / static_cast<float>(sides);
      const float theta = v * kTau;
      const Vec3 radial{0.0f, std::cos(theta), std::sin(theta)};
      appendVertex(mesh, center + Vec3{side * half_length, radial.y * radius, radial.z * radius},
                   radial, {uv_u + side * 0.01f, v}, 0.82f);
    }
  }
  const auto side_at = [side_start, sides](const int x, const int i) {
    return side_start + static_cast<std::uint32_t>(x * (sides + 1) + i);
  };
  for (int i = 0; i < sides; ++i) {
    appendIndexQuad(mesh, side_at(0, i), side_at(1, i), side_at(1, i + 1), side_at(0, i + 1));
  }
  for (int x = 0; x <= 1; ++x) {
    const float side = x == 0 ? -1.0f : 1.0f;
    const Vec3 normal{side, 0.0f, 0.0f};
    const std::uint32_t center_index =
        appendVertex(mesh, center + Vec3{side * half_length, 0.0f, 0.0f}, normal, {uv_u, 0.5f});
    for (int i = 0; i < sides; ++i) {
      if (side < 0.0f) {
        mesh.indices.insert(mesh.indices.end(), {center_index, side_at(x, i + 1), side_at(x, i)});
      } else {
        mesh.indices.insert(mesh.indices.end(), {center_index, side_at(x, i), side_at(x, i + 1)});
      }
    }
  }
}

[[nodiscard]] CpuMesh makeBoltRingMesh(const AsterPipeAssetSpec &spec, const float axial,
                                       const float direction) {
  CpuMesh mesh;
  const int bolt_count = std::max(4, spec.bolt_count_per_flange);
  const float bolt_circle = spec.outer_radius + 0.18f;
  const float bolt_radius = 0.035f;
  const float half_length = 0.045f;
  for (int i = 0; i < bolt_count; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(bolt_count);
    const float theta = u * kTau + 0.12f;
    const Vec3 center{axial + direction * 0.095f, std::cos(theta) * bolt_circle,
                      std::sin(theta) * bolt_circle};
    appendBoltHead(mesh, center, half_length, bolt_radius, 6, u);
  }
  return mesh;
}

[[nodiscard]] CpuMesh makePipeLodMesh(AsterPipeAssetSpec spec, const int level) {
  const float divisor = level == 0 ? 1.0f : (level == 1 ? 2.0f : 4.0f);
  spec.radial_segments = std::max(16, static_cast<int>(static_cast<float>(spec.radial_segments) /
                                                       divisor));
  spec.length_segments = std::max(4, static_cast<int>(static_cast<float>(spec.length_segments) /
                                                      divisor));
  CpuMesh mesh = makeAsterPipeBodyMesh(spec);
  applyPipeSurfaceAuthoring(mesh, spec);
  if (level <= 1) {
    appendMesh(mesh, makeWeldContactBandMesh(spec, -1.55f));
    appendMesh(mesh, makeWeldContactBandMesh(spec, 1.42f));
  }
  return mesh;
}

void populateCookReport(AsterPipeAsset &asset, const AsterPipeAssetSpec &spec) {
  asset.cook_report.asset_id = spec.asset_id;
  asset.cook_report.production_ready = true;
  asset.cook_report.render_vertices = asset.renderVertexCount();
  asset.cook_report.render_indices = asset.renderIndexCount();
  asset.cook_report.lod_count = asset.lods.size();
  asset.cook_report.collision_proxy_count = asset.collision_proxies.size();
  asset.cook_report.material_slot_count = asset.material_slots.size();
  asset.cook_report.modifier_stack_hash =
      fnv1a64(spec.asset_id +
              ":pipe_body:soft_rim:contact_skirt:seam_inset:depth_bias:weld:voronoi_pitting:oxide:cavity:edge:wet_film:scratch:lod:collision");
  asset.cook_report.dependency_edges = {"pipe_body -> bevel_modifier",
                                        "bevel_modifier -> soft_rim_normals",
                                        "weld_seam -> contact_skirt",
                                        "contact_skirt -> depth_bias_policy",
                                        "weld_seam -> cavity_occlusion",
                                        "longitudinal_seam -> seam_inset",
                                        "rust_mask -> oxide_layer",
                                        "voronoi_pitting -> rust_mask",
                                        "edge_angle_wear -> edge_polish",
                                        "wet_film -> wetness_flow",
                                        "axial_scratch -> normal_height",
                                        "pipe_body -> lod_generator",
                                        "pipe_body -> collision_proxy"};
  if (asset.lods.size() < 3u) {
    asset.cook_report.production_ready = false;
    asset.cook_report.diagnostics.push_back("error: rusted pipe asset requires three runtime LODs");
  }
  if (asset.collision_proxies.empty()) {
    asset.cook_report.production_ready = false;
    asset.cook_report.diagnostics.push_back("error: rusted pipe asset requires collision proxy");
  }
  if (asset.renderVertexCount() == 0u || asset.renderIndexCount() == 0u) {
    asset.cook_report.production_ready = false;
    asset.cook_report.diagnostics.push_back("error: rusted pipe render mesh is empty");
  }
  if (asset.surface_masks.empty()) {
    asset.cook_report.production_ready = false;
    asset.cook_report.diagnostics.push_back("error: rusted pipe asset requires authored surface masks");
  }
  if (asset.cook_report.production_ready) {
    asset.cook_report.diagnostics.push_back(
        "ok: runtime pipe asset has render mesh, LODs, layered corrosion masks, and collision proxy");
  }
}

} // namespace

CpuMesh AsterPipeAsset::mergedRenderMesh() const {
  CpuMesh merged;
  for (const AsterPipeAssetPart &part : parts) {
    appendMesh(merged, part.mesh);
  }
  return merged;
}

std::size_t AsterPipeAsset::renderVertexCount() const noexcept {
  std::size_t count = 0u;
  for (const AsterPipeAssetPart &part : parts) {
    count += part.mesh.vertices.size();
  }
  return count;
}

std::size_t AsterPipeAsset::renderIndexCount() const noexcept {
  std::size_t count = 0u;
  for (const AsterPipeAssetPart &part : parts) {
    count += part.mesh.indices.size();
  }
  return count;
}

AsterPipeAsset makeAsterPipeAsset(AsterPipeAssetSpec spec) {
  if (spec.length <= 0.0f || spec.outer_radius <= 0.0f || spec.wall_thickness < 0.0f ||
      spec.radial_segments < 12 || spec.length_segments < 2) {
    throw std::invalid_argument(
        "Aster pipe asset requires positive dimensions, radial_segments >= 12, length_segments >= 2.");
  }

  AsterPipeAsset asset;
  asset.material_slots = {
      {.id = "pipe.body",
       .label = "Corroded pipe body",
       .surface_profile = MaterialSurfaceProfile::CorrodedMetal,
       .surface_pattern = SurfacePattern::WeatheredMetal},
      {.id = "pipe.weld",
       .label = "Raised weld seams",
       .surface_profile = MaterialSurfaceProfile::WeldBead,
       .surface_pattern = SurfacePattern::WeldBead},
      {.id = "pipe.hardware",
       .label = "Oxidized flange hardware",
       .surface_profile = MaterialSurfaceProfile::CorrodedMetal,
       .surface_pattern = SurfacePattern::WeatheredMetal},
  };
  asset.uv_islands = {{.id = "outer-cylinder", .min = {0.0f, 0.0f}, .max = {0.62f, 1.0f},
                       .texel_density = 5.2f},
                      {.id = "inner-wall", .min = {0.62f, 0.0f}, .max = {0.78f, 1.0f},
                       .texel_density = 4.6f},
                      {.id = "weld-and-flange", .min = {0.78f, 0.0f}, .max = {1.0f, 1.0f},
                       .texel_density = 6.4f}};

  CpuMesh body = makeAsterPipeBodyMesh(spec);
  applyPipeSurfaceAuthoring(body, spec);
  asset.parts.push_back({.name = "beveled chipped hollow pipe body",
                         .material_slot = "pipe.body",
                         .mesh = std::move(body)});

  if (spec.include_longitudinal_seam) {
    asset.parts.push_back({.name = "longitudinal welded seam rust band",
                           .material_slot = "pipe.body",
                           .mesh = makeSeamBandMesh(spec)});
  }

  if (spec.include_welds) {
    for (const float axial : {-1.55f, 1.42f}) {
      asset.parts.push_back({.name = "circumferential raised weld bead with contact skirt",
                             .material_slot = "pipe.weld",
                             .mesh = makeWeldContactBandMesh(spec, axial)});
    }
  }

  if (spec.include_flanges) {
    const float half = spec.length * 0.5f;
    for (const float side : {-1.0f, 1.0f}) {
      asset.parts.push_back({.name = "beveled bolt flange ring",
                             .material_slot = "pipe.hardware",
                             .mesh = translated(makeCircumferentialBeadMesh(
                                                    {.pipe_radius = spec.outer_radius + 0.035f,
                                                     .bead_radius = 0.090f,
                                                     .axial_width = 0.26f,
                                                     .radial_segments = spec.radial_segments,
                                                     .bead_segments = 12}),
                                                {side * (half - 0.22f), 0.0f, 0.0f})});
      if (spec.include_bolts) {
        asset.parts.push_back({.name = "hex bolt head placeholders",
                               .material_slot = "pipe.hardware",
                               .mesh = makeBoltRingMesh(spec, side * (half - 0.22f), side)});
      }
    }
  }

  for (int i = 0; i < 11; ++i) {
    const float u = static_cast<float>(i) / 10.0f;
    const float axial = -spec.length * 0.45f + u * spec.length * 0.90f;
    const float theta = hash01(spec.seed + static_cast<std::uint32_t>(i * 31)) * kTau;
    const float radius = spec.outer_radius + 0.020f;
    asset.rust_anchors.push_back({.position = {axial, std::cos(theta) * radius,
                                               std::sin(theta) * radius},
                                  .radius = 0.18f + hash01(spec.seed + i * 97u) * 0.22f,
                                  .severity = 0.42f + hash01(spec.seed + i * 53u) * 0.48f});
  }
  const int streak_count = std::max(1, spec.wet_streak_count);
  for (int i = 0; i < streak_count; ++i) {
    const float u = streak_count == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(streak_count - 1);
    const float theta = (0.12f + u * 0.72f + hash01(spec.seed + i * 61u) * 0.045f) * kTau;
    const float axial = -spec.length * 0.38f + u * spec.length * 0.76f;
    const Vec3 root{axial, std::cos(theta) * spec.outer_radius, std::sin(theta) * spec.outer_radius};
    asset.wetness_streaks.push_back({.root = root,
                                     .direction = {1.0f, 0.0f, -0.08f - 0.035f * u},
                                     .length = 0.34f + 0.42f * u,
                                     .strength = spec.wetness_strength * (0.72f + 0.28f * u)});
  }

  asset.surface_masks.reserve(asset.parts.size());
  for (const AsterPipeAssetPart &part : asset.parts) {
    asset.surface_masks.push_back(summarizeSurfaceMask(part, spec));
  }

  if (spec.include_lods) {
    for (int level = 0; level < 3; ++level) {
      asset.lods.push_back({.name = "pipe.lod" + std::to_string(level),
                            .mesh = makePipeLodMesh(spec, level),
                            .screen_coverage = level == 0 ? 1.0f : (level == 1 ? 0.42f : 0.16f)});
    }
  }

  if (spec.include_collision) {
    const BoundsBuilder render_bounds = boundsFor(asset.parts);
    const Vec3 center = render_bounds.valid ? render_bounds.center() : Vec3{};
    const Vec3 half_extents = render_bounds.valid ? render_bounds.halfExtents()
                                                  : Vec3{spec.length * 0.5f, spec.outer_radius,
                                                         spec.outer_radius};
    asset.collision_proxies.push_back({.name = "pipe.collision.runtime-bounds",
                                       .center = center,
                                       .half_extents = half_extents + Vec3{0.04f, 0.04f, 0.04f},
                                       .radius = spec.outer_radius + 0.24f,
                                       .length = spec.length,
                                       .triangle_budget = 48u,
                                       .covers_render_bounds = true});
  }

  populateCookReport(asset, spec);
  return asset;
}

CpuMesh makeAsterPipeRenderMesh(const AsterPipeAssetSpec spec) {
  return makeAsterPipeAsset(spec).mergedRenderMesh();
}

Material makeAsterPipeMaterial(const std::string &slot_id) {
  if (slot_id == "pipe.weld") {
    return makeMaterial({.base_color = {0.155f, 0.135f, 0.105f},
                         .emission_color = {0.06f, 0.025f, 0.008f},
                         .roughness = 0.80f,
                         .metallic = 0.58f,
                         .detail_strength = 1.16f,
                         .detail_scale = 24.0f,
                         .edge_wear = 0.18f,
                         .ambient_occlusion = 0.52f,
                         .surface_profile = MaterialSurfaceProfile::WeldBead,
                         .surface_pattern = SurfacePattern::WeldBead,
                         .pattern_scale = {24.0f, 18.0f},
                         .pattern_depth = 0.36f,
                         .pattern_contrast = 0.95f,
                         .pattern_mortar = 0.035f,
                         .depth_policy = {.layer = RenderDepthLayer::SurfaceAttachment,
                                          .constant_bias = 0.00008f,
                                          .slope_bias = 0.00012f,
                                          .normal_offset = 0.0025f},
                         .procedural = {.macro_variation = 0.42f,
                                        .micro_normal_strength = 0.62f,
                                        .roughness_variation = 0.72f,
                                        .physical_texel_density = 1024.0f,
                                        .height_normal_coupling = 0.88f,
                                        .roughness_height_coupling = 0.68f,
                                        .macro_frequency_breakup = 0.44f,
                                        .micro_frequency_breakup = 0.72f,
                                        .wetness = 0.08f,
                                        .height_shading = 0.58f,
                                        .pitting_density = 0.74f,
                                        .pitting_depth = 0.007f,
                                        .oxide_layering = 0.76f,
                                        .cavity_grime = 0.92f,
                                        .edge_polish = 0.28f,
                                        .weld_heat_tint = 0.70f,
                                        .axial_scratches = 0.38f,
                                        .wet_streaks = 0.18f,
                                        .rust_bloom = 0.58f,
                                        .black_scab = 0.92f,
                                        .paint_remnant = 0.02f,
                                        .weld_slag = 1.0f,
                                        .rim_soot = 0.24f}});
  }
  const bool hardware = slot_id == "pipe.hardware";
  return makeMaterial({.base_color = hardware ? LinearRgb{0.20f, 0.18f, 0.15f}
                                              : LinearRgb{0.30f, 0.135f, 0.060f},
                       .roughness = hardware ? 0.84f : 0.72f,
                       .metallic = hardware ? 0.74f : 0.48f,
                       .detail_strength = hardware ? 0.78f : 1.02f,
                       .detail_scale = hardware ? 12.0f : 12.4f,
                       .edge_wear = hardware ? 0.36f : 0.36f,
                       .ambient_occlusion = hardware ? 0.64f : 0.48f,
                       .surface_profile = MaterialSurfaceProfile::CorrodedMetal,
                       .surface_pattern = SurfacePattern::WeatheredMetal,
                       .pattern_scale = hardware ? Vec2{5.2f, 8.0f} : Vec2{5.8f, 17.0f},
                       .pattern_depth = hardware ? 0.36f : 0.52f,
                       .pattern_contrast = hardware ? 0.82f : 0.94f,
                       .pattern_mortar = 0.05f,
                       .depth_policy = hardware ? RenderDepthPolicy{.layer = RenderDepthLayer::SurfaceAttachment,
                                                                     .constant_bias = 0.00006f,
                                                                     .slope_bias = 0.00008f,
                                                                     .normal_offset = 0.0015f}
                                                : RenderDepthPolicy{},
                       .procedural = {.macro_variation = hardware ? 0.58f : 0.90f,
                                      .micro_normal_strength = hardware ? 0.12f : 0.46f,
                                      .roughness_variation = hardware ? 0.58f : 0.92f,
                                      .physical_texel_density = hardware ? 768.0f : 1024.0f,
                                      .height_normal_coupling = hardware ? 0.34f : 0.92f,
                                      .roughness_height_coupling = hardware ? 0.42f : 0.74f,
                                      .macro_frequency_breakup = hardware ? 0.24f : 0.48f,
                                      .micro_frequency_breakup = hardware ? 0.28f : 0.66f,
                                      .wetness = hardware ? 0.03f : 0.07f,
                                      .height_shading = hardware ? 0.10f : 0.36f,
                                      .pitting_density = hardware ? 0.70f : 0.96f,
                                      .pitting_depth = hardware ? 0.003f : 0.0055f,
                                      .oxide_layering = hardware ? 0.38f : 0.94f,
                                      .cavity_grime = hardware ? 0.46f : 0.82f,
                                      .edge_polish = hardware ? 0.42f : 0.18f,
                                      .weld_heat_tint = hardware ? 0.12f : 0.20f,
                                      .axial_scratches = hardware ? 0.36f : 0.82f,
                                      .wet_streaks = hardware ? 0.08f : 0.70f,
                                      .rust_bloom = hardware ? 0.22f : 0.82f,
                                      .black_scab = hardware ? 0.28f : 0.88f,
                                      .paint_remnant = hardware ? 0.02f : 0.06f,
                                      .weld_slag = hardware ? 0.08f : 0.36f,
                                      .rim_soot = hardware ? 0.10f : 0.92f}});
}

} // namespace aster
