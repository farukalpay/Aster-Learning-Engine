// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/pipe_runtime_asset.hpp"

#include "aster/geometry/tube_mesh.hpp"

#include <algorithm>
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

void applyPipeSurfaceAuthoring(CpuMesh &mesh, const AsterPipeAssetSpec &spec) {
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
            ? smoothstep(half - 0.34f, half - 0.02f, std::abs(vertex.position.x))
            : 0.0f;
    const float seam_mask =
        1.0f - smoothstep(0.035f, 0.20f, std::abs(std::sin(theta - 1.92f)));
    const float corrosion =
        hash01(spec.seed ^ static_cast<std::uint32_t>(i * 747796405u)) * 0.62f +
        0.38f * smoothstep(0.12f, 0.92f,
                           std::sin(theta * 4.0f + vertex.position.x * 2.7f) * 0.5f + 0.5f);
    const float chip_depth = rim_mask * corrosion * 0.024f * saturate(spec.rust_strength);
    const float pitting = corrosion * corrosion * 0.010f * saturate(spec.rust_strength);
    vertex.position -= outward * (chip_depth + pitting * (0.35f + seam_mask * 0.50f));
    vertex.normal = normalize(vertex.normal + outward * (0.10f + seam_mask * 0.10f));
    vertex.ambient_occlusion =
        std::clamp(0.92f - corrosion * 0.26f - rim_mask * 0.18f - seam_mask * 0.12f, 0.48f,
                   1.0f);
    vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
  }
}

[[nodiscard]] CpuMesh makeSeamBandMesh(const AsterPipeAssetSpec &spec) {
  CpuMesh mesh;
  const int rows = std::max(4, spec.length_segments);
  constexpr int kColumns = 3;
  const float half = spec.length * 0.5f;
  const float seam_angle = 1.92f;
  const float half_width = 0.025f;
  const float radius = spec.outer_radius + 0.012f;
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
      const float lift = radius + (column == 1 ? 0.010f : 0.0f) + streak;
      const float ao = column == 1 ? 0.62f : 0.76f;
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
  CpuMesh mesh = makeTubeMesh({.length = spec.length,
                               .outer_radius = spec.outer_radius,
                               .wall_thickness = spec.wall_thickness,
                               .radial_segments = spec.radial_segments,
                               .length_segments = spec.length_segments});
  applyPipeSurfaceAuthoring(mesh, spec);
  if (level <= 1) {
    appendMesh(mesh, translated(makeCircumferentialBeadMesh({.pipe_radius = spec.outer_radius,
                                                             .bead_radius = 0.045f,
                                                             .axial_width = 0.18f,
                                                             .radial_segments = spec.radial_segments,
                                                             .bead_segments = 8}),
                                {-1.55f, 0.0f, 0.0f}));
    appendMesh(mesh, translated(makeCircumferentialBeadMesh({.pipe_radius = spec.outer_radius,
                                                             .bead_radius = 0.045f,
                                                             .axial_width = 0.18f,
                                                             .radial_segments = spec.radial_segments,
                                                             .bead_segments = 8}),
                                {1.42f, 0.0f, 0.0f}));
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
      fnv1a64(spec.asset_id + ":pipe_body:bevel:weld:rust:wetness:lod:collision");
  asset.cook_report.dependency_edges = {"pipe_body -> bevel_modifier",
                                        "bevel_modifier -> weld_seam",
                                        "weld_seam -> rust_mask",
                                        "rust_mask -> wetness_flow",
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
  if (asset.cook_report.production_ready) {
    asset.cook_report.diagnostics.push_back(
        "ok: runtime pipe asset has render mesh, LODs, material masks, and collision proxy");
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

  CpuMesh body = makeTubeMesh({.length = spec.length,
                               .outer_radius = spec.outer_radius,
                               .wall_thickness = spec.wall_thickness,
                               .radial_segments = spec.radial_segments,
                               .length_segments = spec.length_segments});
  applyPipeSurfaceAuthoring(body, spec);
  asset.parts.push_back({.name = "beveled chipped hollow pipe body",
                         .material_slot = "pipe.body",
                         .mesh = std::move(body)});

  if (spec.include_longitudinal_seam) {
    asset.parts.push_back({.name = "longitudinal welded seam rust band",
                           .material_slot = "pipe.weld",
                           .mesh = makeSeamBandMesh(spec)});
  }

  if (spec.include_welds) {
    for (const float axial : {-1.55f, 1.42f}) {
      asset.parts.push_back({.name = "circumferential raised weld bead",
                             .material_slot = "pipe.weld",
                             .mesh = translated(makeCircumferentialBeadMesh(
                                                    {.pipe_radius = spec.outer_radius,
                                                     .bead_radius = 0.045f,
                                                     .axial_width = 0.18f,
                                                     .radial_segments = spec.radial_segments,
                                                     .bead_segments = 14}),
                                                {axial, 0.0f, 0.0f})});
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

  for (int i = 0; i < 9; ++i) {
    const float u = static_cast<float>(i) / 8.0f;
    const float axial = -spec.length * 0.45f + u * spec.length * 0.90f;
    const float theta = hash01(spec.seed + static_cast<std::uint32_t>(i * 31)) * kTau;
    const float radius = spec.outer_radius + 0.020f;
    asset.rust_anchors.push_back({.position = {axial, std::cos(theta) * radius,
                                               std::sin(theta) * radius},
                                  .radius = 0.18f + hash01(spec.seed + i * 97u) * 0.22f,
                                  .severity = 0.42f + hash01(spec.seed + i * 53u) * 0.48f});
  }
  for (int i = 0; i < 5; ++i) {
    const float theta = (0.18f + static_cast<float>(i) * 0.13f) * kTau;
    const float axial = -spec.length * 0.35f + static_cast<float>(i) * spec.length * 0.17f;
    const Vec3 root{axial, std::cos(theta) * spec.outer_radius, std::sin(theta) * spec.outer_radius};
    asset.wetness_streaks.push_back({.root = root,
                                     .direction = {1.0f, 0.0f, -0.08f},
                                     .length = 0.42f + 0.10f * static_cast<float>(i),
                                     .strength = spec.wetness_strength});
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
    return makeMaterial({.base_color = {0.36f, 0.32f, 0.27f},
                         .emission_color = {0.06f, 0.025f, 0.008f},
                         .roughness = 0.62f,
                         .metallic = 0.88f,
                         .detail_strength = 0.96f,
                         .detail_scale = 18.0f,
                         .edge_wear = 0.52f,
                         .ambient_occlusion = 0.72f,
                         .surface_profile = MaterialSurfaceProfile::WeldBead,
                         .surface_pattern = SurfacePattern::WeldBead,
                         .pattern_scale = {24.0f, 18.0f},
                         .pattern_depth = 0.36f,
                         .pattern_contrast = 0.95f,
                         .pattern_mortar = 0.035f,
                         .procedural = {.macro_variation = 0.42f,
                                        .micro_normal_strength = 0.48f,
                                        .roughness_variation = 0.48f,
                                        .wetness = 0.08f,
                                        .height_shading = 0.32f}});
  }
  const bool hardware = slot_id == "pipe.hardware";
  return makeMaterial({.base_color = hardware ? Vec3{0.20f, 0.18f, 0.15f}
                                              : Vec3{0.18f, 0.17f, 0.16f},
                       .roughness = hardware ? 0.84f : 0.78f,
                       .metallic = hardware ? 0.74f : 0.82f,
                       .detail_strength = hardware ? 0.78f : 0.92f,
                       .detail_scale = hardware ? 12.0f : 9.6f,
                       .edge_wear = hardware ? 0.36f : 0.28f,
                       .ambient_occlusion = hardware ? 0.64f : 0.70f,
                       .surface_profile = MaterialSurfaceProfile::CorrodedMetal,
                       .surface_pattern = SurfacePattern::WeatheredMetal,
                       .pattern_scale = hardware ? Vec2{5.2f, 8.0f} : Vec2{3.6f, 11.5f},
                       .pattern_depth = hardware ? 0.36f : 0.42f,
                       .pattern_contrast = hardware ? 0.82f : 0.92f,
                       .pattern_mortar = 0.05f,
                       .procedural = {.macro_variation = hardware ? 0.58f : 0.72f,
                                      .micro_normal_strength = hardware ? 0.42f : 0.54f,
                                      .roughness_variation = hardware ? 0.52f : 0.72f,
                                      .wetness = hardware ? 0.02f : 0.04f,
                                      .height_shading = hardware ? 0.30f : 0.40f}});
}

} // namespace aster
