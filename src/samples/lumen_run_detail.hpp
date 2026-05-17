// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/samples/lumen_run.hpp"
#include "aster/core/profiler.hpp"
#include "aster/systems/player_motion.hpp"
#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/castle_course.hpp"
#include "aster/geometry/cave_system.hpp"
#include "aster/geometry/cave_web_mesh.hpp"
#include "aster/geometry/energy_conduit_mesh.hpp"
#include "aster/geometry/generated_scenery.hpp"
#include "aster/geometry/mesh_cut.hpp"
#include "aster/geometry/mesh_projection.hpp"
#include "aster/geometry/nature_mesh.hpp"
#include "aster/geometry/stroke_mesh.hpp"
#include "aster/geometry/terrain_sculpt.hpp"
#include "aster/physics/contact_query.hpp"
#include "aster/physics/fluid_locomotion.hpp"
#include "aster/physics/placement_validation.hpp"
#include "aster/physics/terrain_contact.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// Source-only Lumen Run helpers. Kept out of public headers so sample-specific
// content does not leak into reusable engine modules.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::uint32_t kPhysicsLayerWorld = 1u << 0u;
constexpr std::uint32_t kPhysicsLayerPlayer = 1u << 1u;
constexpr int kFishingLineSegmentCount = 7;
constexpr std::size_t kCrocodilePartCount = 4;
constexpr aster::Vec3 kCastleOrigin{0.0f, 0.0f, -6.80f};
constexpr aster::Vec2 kCourtyardPondRadius{2.24f, 1.56f};
constexpr float kCourtyardPondSurfaceY = 0.405f;
constexpr float kCourtyardPondHalfDepth = 1.15f;
constexpr float kCourtyardPondCenterOffsetY = -0.06f;
constexpr float kInnerPondSurfaceY = 0.430f;
constexpr float kInnerPondHalfDepth = 1.60f;
constexpr float kInnerPondCenterOffsetY = -0.08f;
constexpr float kParkourChestPlacementClearance = 3.25f;
constexpr float kChestInteractionDistance = 2.55f;
constexpr float kChestAutoCloseDistance = 3.20f;
constexpr float kSupplyCrateInteractionDistance = 2.85f;
constexpr double kSceneTraceCheckpointRate = 30.0;
constexpr std::size_t kSceneTracePathContinuityHorizon = 3u;
constexpr float kSceneTraceWaterFootprintScale = 0.98f;
constexpr float kSceneTraceFishSurfaceTolerance = 0.06f;
constexpr const char *kTraceWalkable = "Walkable";
constexpr const char *kTraceBlocked = "Blocked";
constexpr const char *kTracePathVisible = "PathVisible";
constexpr const char *kTracePathContinues = "PathContinues";
constexpr const char *kTracePathCut = "PathCut";
constexpr const char *kTraceCameraInsideWall = "CameraInsideWall";
constexpr const char *kTraceOutsideVisible = "OutsideVisible";
constexpr const char *kTraceFishVisible = "FishVisible";
constexpr const char *kTraceFishInsideWater = "FishInsideWater";
constexpr const char *kTraceFishOnSurface = "FishOnSurface";
constexpr const char *kTraceFishMisembedded = "FishMisembedded";
constexpr const char *kTraceFishingSupportDry = "FishingSupportDry";
constexpr const char *kTraceFishingSupportWet = "FishingSupportWet";
constexpr const char *kTraceRewardVisible = "RewardVisible";
constexpr const char *kTraceRewardReachable = "RewardReachable";
constexpr const char *kTraceFalseRewardAffordance = "FalseRewardAffordance";
constexpr const char *kTraceThreatVisible = "ThreatVisible";
constexpr const char *kTraceThreatReadable = "ThreatReadable";
constexpr const char *kTraceMeshPenetration = "MeshPenetration";
constexpr std::uint32_t kLumenCaveSeed = 0x9f2a51u;
constexpr aster::Vec3 kCaveEntrancePlanar{31.0f, 0.0f, -59.0f};
constexpr aster::Vec3 kCaveInwardPlanar{-0.16f, 0.0f, -1.0f};
constexpr float kCaveApproachPathWidth = 0.74f;
constexpr std::size_t kMiningFractureShardVisualPoolSize = 28u;
constexpr aster::Vec3 kCaveIndustrialWarmLight{1.0f, 0.42f, 0.24f};
constexpr float kAuthoredCaveFixtureIntensity = 42.0f;
constexpr float kAuthoredCaveFixtureSourceRadius = 2.15f;
constexpr float kOreInteractionDistance = 2.35f;
constexpr float kCaveWebInteractionDistance = 3.10f;
constexpr float kCaveWebSlowScale = 0.18f;
constexpr int kCaveSkitterEncounterCount = 3;
constexpr float kCaveSkitterInteractionDistance = 2.85f;
constexpr int kCaveSkitterMaxHealth = 3;
constexpr int kPlayerMaxHealth = 20;
constexpr int kCaveSkitterBiteDamage = 4;
constexpr int kCoalOreMaxHealth = 3;
constexpr aster::Vec3 kPrismRelayCastleLocal{-8.4f, 0.035f, -14.2f};
constexpr float kPrismRelayInteractionDistance = 3.25f;
constexpr float kPrismRelayCoreHeight = 2.28f;
constexpr float kPrismRelayIdleCharge = 0.18f;
constexpr float kPrismRelayIgnitedCharge = 1.0f;
constexpr float kPrismRelayRetuneKick = 0.16f;

struct UnderpassPortalPlacement {
  const char *name = "";
  aster::Vec3 position{};
  aster::Vec3 scale{1.0f, 1.0f, 1.0f};
};

float distanceOnArena(const aster::Vec3 lhs, const aster::Vec3 rhs) {
  const aster::Vec2 delta{lhs.x - rhs.x, lhs.z - rhs.z};
  return aster::length(delta);
}

float normalizedEllipseDistanceSquared(const aster::Vec2 point, const aster::Vec3 center,
                                       const aster::Vec2 radius) {
  const float radius_x = std::max(radius.x, 0.001f);
  const float radius_z = std::max(radius.y, 0.001f);
  const float dx = (point.x - center.x) / radius_x;
  const float dz = (point.y - center.z) / radius_z;
  return dx * dx + dz * dz;
}

bool insideEllipticalFootprint(const aster::Vec3 position, const aster::Vec3 center,
                               const aster::Vec2 radius, const float edge_scale) {
  const float scale = std::clamp(edge_scale, 0.0f, 1.0f);
  return normalizedEllipseDistanceSquared({position.x, position.z}, center, radius * scale) <= 1.0f;
}

float directionalClipRadius(const float fallback, const float negative_radius,
                            const float positive_radius, const float offset) {
  if (offset < 0.0f && negative_radius > 0.0f) {
    return negative_radius;
  }
  if (offset > 0.0f && positive_radius > 0.0f) {
    return positive_radius;
  }
  return fallback;
}

bool insideTerrainClipEllipse(const aster::Vec2 point, const aster::TerrainMeshClipEllipse &clip) {
  if (clip.radius.x <= 0.0f || clip.radius.y <= 0.0f) {
    return false;
  }
  const float offset_x = point.x - clip.center.x;
  const float offset_z = point.y - clip.center.y;
  const float radius_x = directionalClipRadius(clip.radius.x, clip.radius_x_negative,
                                               clip.radius_x_positive, offset_x);
  const float radius_z = directionalClipRadius(clip.radius.y, clip.radius_z_negative,
                                               clip.radius_z_positive, offset_z);
  const float x = offset_x / std::max(radius_x, 0.001f);
  const float z = offset_z / std::max(radius_z, 0.001f);
  return x * x + z * z <= 1.0f;
}

bool insideAnySceneVolume(const std::vector<aster::SceneCoherenceVolume> &volumes,
                          const aster::Vec3 point) {
  for (const aster::SceneCoherenceVolume &volume : volumes) {
    if (aster::contains(volume, point)) {
      return true;
    }
  }
  return false;
}

bool blockedByAnySceneVolume(const std::vector<aster::SceneCoherenceVolume> &volumes,
                             const aster::Vec3 from, const aster::Vec3 to) {
  for (const aster::SceneCoherenceVolume &volume : volumes) {
    if (aster::segmentIntersectsVolume(from, to, volume)) {
      return true;
    }
  }
  return false;
}

aster::Vec3 segmentRotation(const aster::Vec3 from, const aster::Vec3 to) {
  const aster::Vec3 direction = aster::normalize(to - from);
  const float pitch = std::acos(aster::clamp(direction.y, -1.0f, 1.0f));
  const float yaw = std::atan2(direction.x, direction.z);
  return {pitch, yaw, 0.0f};
}

aster::Vec3 rotateYaw(const aster::Vec3 value, const float yaw) {
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

aster::Vec3 rotateX(const aster::Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

aster::Vec3 rotateZ(const aster::Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

aster::Vec3 rotateEuler(const aster::Vec3 value, const aster::Vec3 rotation) {
  return rotateZ(rotateYaw(rotateX(value, rotation.x), rotation.y), rotation.z);
}

aster::Vec3 mixColor(const aster::Vec3 a, const aster::Vec3 b, const float t) {
  const float amount = aster::clamp(t, 0.0f, 1.0f);
  return a + (b - a) * amount;
}

aster::Vec3 saturatedColor(const aster::Vec3 color) {
  return {aster::clamp(color.x, 0.0f, 1.0f), aster::clamp(color.y, 0.0f, 1.0f),
          aster::clamp(color.z, 0.0f, 1.0f)};
}

void applyIndustrialLensColor(aster::Material &lens_material, const aster::Vec3 color) {
  const aster::Vec3 saturated = saturatedColor(color);
  lens_material.base_color = mixColor(saturated * 0.42f, {0.08f, 0.035f, 0.025f}, 0.18f);
  lens_material.emission_color = saturated;
  lens_material.emission_strength = std::max(lens_material.emission_strength, 0.82f);
}

std::string placedResourceId(const std::uint64_t serial) {
  return "placed_resource:" + std::to_string(serial);
}

std::uint32_t fractureSeedFor(const aster::Vec3 position, const std::uint32_t salt) {
  const auto quantize = [](const float value) {
    const auto scaled = static_cast<std::int32_t>(std::floor(value * 127.0f));
    return static_cast<std::uint32_t>(scaled);
  };
  std::uint32_t seed = salt ^ 0x9e3779b9u;
  seed ^= quantize(position.x) + 0x85ebca6bu + (seed << 6u) + (seed >> 2u);
  seed ^= quantize(position.y) + 0xc2b2ae35u + (seed << 6u) + (seed >> 2u);
  seed ^= quantize(position.z) + 0x27d4eb2fu + (seed << 6u) + (seed >> 2u);
  return seed == 0u ? 1u : seed;
}

void hideRenderObject(aster::RenderObject &object) {
  object.transform.position = {0.0f, -24.0f, 0.0f};
  object.transform.scale = {0.001f, 0.001f, 0.001f};
  object.custom_mesh.reset();
  object.dynamic_mesh = {};
  object.visibility_hint = {};
  object.lod = {};
  object.material.emission_strength = 0.0f;
}

aster::MeshPrimitive placementPrimitiveFor(const aster::ItemPlacementShape shape) {
  (void)shape;
  return aster::MeshPrimitive::Rock;
}

aster::Material material(const aster::Vec3 base, const aster::Vec3 emission, const float roughness,
                         const float metallic, const float glow, const float detail = 0.0f,
                         const float detail_scale = 1.0f, const float wear = 0.0f,
                         const float ambient_occlusion = 1.0f,
                         const aster::SurfacePattern pattern = aster::SurfacePattern::None,
                         const aster::Vec2 pattern_scale = {1.0f, 1.0f},
                         const float pattern_depth = 0.0f, const float pattern_contrast = 0.0f,
                         const float pattern_mortar = 0.08f,
                         const aster::ProceduralSurfaceLayer procedural = {}) {
  return aster::makeMaterial({.base_color = base,
                              .emission_color = emission,
                              .roughness = roughness,
                              .metallic = metallic,
                              .emission_strength = glow,
                              .detail_strength = detail,
                              .detail_scale = detail_scale,
                              .edge_wear = wear,
                              .ambient_occlusion = ambient_occlusion,
                              .surface_pattern = pattern,
                              .pattern_scale = pattern_scale,
                              .pattern_depth = pattern_depth,
                              .pattern_contrast = pattern_contrast,
                              .pattern_mortar = pattern_mortar,
                              .procedural = procedural});
}

aster::Material lumenPlacedResourceMaterial(const aster::ItemDefinition *definition) {
  aster::Material placed_stone =
      material({0.245f, 0.255f, 0.235f}, {0.0f, 0.0f, 0.0f}, 0.96f, 0.0f, 0.0f, 0.86f,
               8.8f, 0.28f, 0.72f, aster::SurfacePattern::CaveRock, {3.2f, 5.0f},
               0.096f, 0.86f, 0.060f,
               {.macro_variation = 0.28f,
                .micro_normal_strength = 0.46f,
                .roughness_variation = 0.26f,
                .height_shading = 0.28f});
  if (definition != nullptr) {
    placed_stone.base_color = mixColor(placed_stone.base_color, definition->tint, 0.26f);
  }
  placed_stone.camera_occlusion = aster::CameraOcclusionPolicy::Solid;
  placed_stone.cull_mode = aster::FaceCullMode::Back;
  return placed_stone;
}

std::shared_ptr<const aster::CpuMesh> makeSharedMesh(aster::CpuMesh mesh) {
  return std::make_shared<const aster::CpuMesh>(std::move(mesh));
}

aster::ViewerCullVolume viewerCullVolumeForMesh(const aster::CpuMesh &mesh, const float padding,
                                                const aster::FaceCullMode outside,
                                                const aster::FaceCullMode inside) {
  aster::Vec3 min_bounds{std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::infinity()};
  aster::Vec3 max_bounds{-std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()};
  for (const aster::Vertex &vertex : mesh.vertices) {
    min_bounds.x = std::min(min_bounds.x, vertex.position.x);
    min_bounds.y = std::min(min_bounds.y, vertex.position.y);
    min_bounds.z = std::min(min_bounds.z, vertex.position.z);
    max_bounds.x = std::max(max_bounds.x, vertex.position.x);
    max_bounds.y = std::max(max_bounds.y, vertex.position.y);
    max_bounds.z = std::max(max_bounds.z, vertex.position.z);
  }
  if (mesh.vertices.empty()) {
    return {};
  }
  const aster::Vec3 pad{std::max(padding, 0.0f), std::max(padding, 0.0f), std::max(padding, 0.0f)};
  return {.enabled = true,
          .center = (min_bounds + max_bounds) * 0.5f,
          .half_extents = (max_bounds - min_bounds) * 0.5f + pad,
          .outside = outside,
          .inside = inside};
}

aster::ItemRegistry makeLumenRunItemRegistry() {
  aster::ItemRegistry registry;
  registry.add({.id = "pickaxe",
                .display_name = "Pickaxe",
                .short_label = "PCK",
                .type = aster::ItemType::Tool,
                .tint = {0.50f, 0.48f, 0.42f},
                .world_scale = {1.0f, 1.0f, 1.0f},
                .hand_scale = {1.0f, 1.0f, 1.0f},
                .has_mining_tool = true,
                .mining_tool = aster::starterPickaxeStats()});
  registry.add({.id = "torch",
                .display_name = "Torch",
                .short_label = "TRC",
                .type = aster::ItemType::LightTool,
                .tint = {0.98f, 0.52f, 0.16f},
                .world_scale = {1.0f, 1.0f, 1.0f},
                .hand_scale = {1.0f, 1.0f, 1.0f},
                .stackable = true,
                .max_stack = 99,
                .creates_light = true,
                .creates_fire_particles = true});
  registry.add({.id = "fishing_rod",
                .display_name = "Fishing Rod",
                .short_label = "ROD",
                .type = aster::ItemType::Tool,
                .tint = {0.36f, 0.22f, 0.12f},
                .world_scale = {1.0f, 1.0f, 1.0f},
                .hand_scale = {1.0f, 1.0f, 1.0f}});
  registry.add({.id = "coal",
                .display_name = "Coal",
                .short_label = "COA",
                .type = aster::ItemType::Resource,
                .tint = {0.08f, 0.075f, 0.070f},
                .world_scale = {1.0f, 1.0f, 1.0f},
                .hand_scale = {1.0f, 1.0f, 1.0f},
                .stackable = true,
                .max_stack = 24});
  registry.add({.id = "iron_ore",
                .display_name = "Ironstone",
                .short_label = "IRO",
                .type = aster::ItemType::Resource,
                .tint = {0.46f, 0.26f, 0.16f},
                .world_scale = {0.34f, 0.26f, 0.32f},
                .hand_scale = {0.86f, 0.86f, 0.86f},
                .stackable = true,
                .max_stack = 64});
  registry.add({.id = "copper_ore",
                .display_name = "Copper",
                .short_label = "COP",
                .type = aster::ItemType::Resource,
                .tint = {0.72f, 0.36f, 0.16f},
                .world_scale = {0.32f, 0.24f, 0.30f},
                .hand_scale = {0.84f, 0.84f, 0.84f},
                .stackable = true,
                .max_stack = 64});
  registry.add({.id = "stone",
                .display_name = "Rock",
                .short_label = "RCK",
                .type = aster::ItemType::Resource,
                .tint = {0.32f, 0.31f, 0.29f},
                .world_scale = {0.30f, 0.24f, 0.30f},
                .hand_scale = {0.82f, 0.82f, 0.82f},
                .stackable = true,
                .max_stack = 48,
                .placeable = true,
                .placement_cost = 1,
                .placement_reach = 4.4f,
                .placement_scale = {0.22f, 0.16f, 0.20f},
                .placement_collision_half_extents = {0.16f, 0.12f, 0.15f}});
  return registry;
}

const aster::CastleCourse &castleCourse() {
  static const aster::CastleCourse course = aster::buildCastleCourse();
  return course;
}

aster::Vec3 castleOrigin() {
  return kCastleOrigin;
}

aster::Vec3 parkourChestBase() {
  return castleOrigin() + aster::Vec3{3.08f, 0.035f, 5.16f};
}

aster::Vec3 prismRelayBasePlacement() {
  return castleOrigin() + kPrismRelayCastleLocal;
}

aster::AvatarAttachmentSocket plushRightHandCarrySocket() {
  return {.part_name = "right paw",
          .local_position = {0.04f, -0.10f, 0.28f},
          .local_rotation = {aster::radians(-24.0f), 0.0f, aster::radians(-8.0f)}};
}

std::array<UnderpassPortalPlacement, 2> gothicUnderpassPortals() {
  return {{{"Gothic parkour underpass entrance", {0.02f, 0.018f, -5.18f}, {1.0f, 1.0f, 1.0f}},
           {"Gothic parkour underpass exit", {0.14f, 0.018f, -9.36f}, {0.92f, 1.0f, 0.92f}}}};
}

aster::Vec3 ellipseBankPoint(const aster::TerrainMeshClipEllipse &ellipse,
                             const aster::Vec3 approach, const float clearance) {
  aster::Vec2 direction{approach.x - ellipse.center.x, approach.z - ellipse.center.y};
  if (aster::length(direction) <= 0.0001f) {
    direction = {1.0f, 0.0f};
  }
  direction = aster::normalize(direction);
  const float radius = 1.0f / std::sqrt((direction.x * direction.x) /
                                            std::max(ellipse.radius.x * ellipse.radius.x, 0.0001f) +
                                        (direction.y * direction.y) /
                                            std::max(ellipse.radius.y * ellipse.radius.y, 0.0001f));
  const aster::Vec2 point = ellipse.center + direction * (radius + std::max(clearance, 0.0f));
  return {point.x, approach.y, point.y};
}

aster::PlacementFootprint translatedGroundZoneFootprint(const aster::CastleCourseGroundZone &zone,
                                                        const aster::Vec3 origin) {
  return aster::makePlacementFootprint({origin.x + zone.center.x, origin.z + zone.center.y},
                                       zone.half_extents);
}

aster::PlacementValidator terrainPlacementValidator(
    const std::vector<aster::PlacementFootprint> &floor_footprints,
    const std::vector<aster::TerrainMeshClipEllipse> &terrain_holes,
    const std::vector<aster::TerrainMeshClipOrientedEllipse> &oriented_terrain_holes) {
  aster::PlacementValidator validator;
  for (const aster::PlacementFootprint &footprint : floor_footprints) {
    validator.addForbiddenFootprint(footprint);
  }
  for (const aster::TerrainMeshClipEllipse &clip : terrain_holes) {
    validator.addForbiddenEllipse({clip.center, clip.radius});
  }
  for (const aster::TerrainMeshClipOrientedEllipse &clip : oriented_terrain_holes) {
    validator.addForbiddenOrientedEllipse(
        {.center = clip.center,
         .forward = clip.forward,
         .radius = clip.radius,
         .radius_side_negative = clip.radius_side_negative,
         .radius_side_positive = clip.radius_side_positive,
         .radius_forward_negative = clip.radius_forward_negative,
         .radius_forward_positive = clip.radius_forward_positive});
  }
  return validator;
}

std::vector<aster::TerrainMeshClipBox>
terrainPlacementClips(const aster::PlacementValidator &validator) {
  std::vector<aster::TerrainMeshClipBox> clips;
  clips.reserve(validator.forbiddenAabbs().size() + validator.forbiddenFootprints().size());
  for (const aster::PlacementAabb &volume : validator.forbiddenAabbs()) {
    const aster::PlacementFootprint footprint = aster::projectFootprint(volume);
    clips.push_back({footprint.min, footprint.max});
  }
  for (const aster::PlacementFootprint &footprint : validator.forbiddenFootprints()) {
    clips.push_back({footprint.min, footprint.max});
  }
  return clips;
}

const char *castleChannelName(const aster::CastleCourseChannel channel) {
  switch (channel) {
  case aster::CastleCourseChannel::Foundation:
    return "Castle foundation";
  case aster::CastleCourseChannel::WarmStone:
    return "Castle warm stone";
  case aster::CastleCourseChannel::CoolStone:
    return "Castle cool stone";
  case aster::CastleCourseChannel::TrimStone:
    return "Castle trim stone";
  case aster::CastleCourseChannel::Wood:
    return "Castle wood";
  case aster::CastleCourseChannel::Iron:
    return "Castle iron";
  case aster::CastleCourseChannel::WarmLight:
    return "Castle warm light";
  }
  return "Castle section";
}

aster::StrokePath strokePath(std::initializer_list<aster::Vec2> points, const float width) {
  aster::StrokePath path;
  path.points.reserve(points.size());
  for (const aster::Vec2 point : points) {
    path.points.push_back({point, width});
  }
  return path;
}

void appendSignLetter(std::vector<aster::StrokePath> &paths, const char letter, const float x,
                      const float y, const float w, const float h, const float width) {
  const float top = y + h;
  const float mid = y + h * 0.54f;
  const float lower_top = y + h * 0.72f;
  switch (letter) {
  case 'C':
    paths.push_back(strokePath({{x + w * 0.88f, top - h * 0.08f},
                                {x + w * 0.52f, top + h * 0.02f},
                                {x + w * 0.12f, top - h * 0.18f},
                                {x + w * 0.04f, y + h * 0.48f},
                                {x + w * 0.14f, y + h * 0.12f},
                                {x + w * 0.56f, y - h * 0.02f},
                                {x + w * 0.92f, y + h * 0.08f}},
                               width));
    break;
  case 'J':
    paths.push_back(strokePath({{x + w * 0.12f, top},
                                {x + w * 0.92f, top},
                                {x + w * 0.84f, y + h * 0.18f},
                                {x + w * 0.52f, y},
                                {x + w * 0.16f, y + h * 0.12f}},
                               width));
    break;
  case 'A':
    paths.push_back(
        strokePath({{x + w * 0.06f, y}, {x + w * 0.50f, top}, {x + w * 0.94f, y}}, width));
    paths.push_back(strokePath({{x + w * 0.24f, y + h * 0.42f}, {x + w * 0.76f, y + h * 0.42f}},
                               width * 0.84f));
    break;
  case 'V':
    paths.push_back(
        strokePath({{x + w * 0.06f, top}, {x + w * 0.48f, y}, {x + w * 0.94f, top}}, width));
    break;
  case 'E':
    paths.push_back(strokePath({{x + w * 0.10f, y}, {x + w * 0.10f, top}}, width));
    paths.push_back(strokePath({{x + w * 0.12f, top}, {x + w * 0.92f, top}}, width));
    paths.push_back(strokePath({{x + w * 0.12f, y + h * 0.52f}, {x + w * 0.76f, y + h * 0.52f}},
                               width * 0.88f));
    paths.push_back(strokePath({{x + w * 0.12f, y}, {x + w * 0.90f, y}}, width));
    break;
  case 'U':
    paths.push_back(strokePath({{x + w * 0.08f, top},
                                {x + w * 0.08f, y + h * 0.18f},
                                {x + w * 0.50f, y},
                                {x + w * 0.92f, y + h * 0.18f},
                                {x + w * 0.92f, top}},
                               width));
    break;
  case 'u':
    paths.push_back(strokePath({{x + w * 0.08f, lower_top},
                                {x + w * 0.08f, y + h * 0.16f},
                                {x + w * 0.50f, y},
                                {x + w * 0.92f, y + h * 0.16f},
                                {x + w * 0.92f, lower_top}},
                               width));
    break;
  case 'a':
    paths.push_back(strokePath({{x + w * 0.82f, y + h * 0.04f},
                                {x + w * 0.78f, lower_top},
                                {x + w * 0.42f, lower_top + h * 0.04f},
                                {x + w * 0.12f, y + h * 0.45f},
                                {x + w * 0.20f, y + h * 0.10f},
                                {x + w * 0.58f, y + h * 0.02f},
                                {x + w * 0.84f, y + h * 0.26f}},
                               width));
    paths.push_back(strokePath({{x + w * 0.78f, y + h * 0.42f}, {x + w * 0.92f, y + h * 0.03f}},
                               width * 0.86f));
    break;
  case 'v':
    paths.push_back(strokePath({{x + w * 0.05f, lower_top},
                                {x + w * 0.42f, y + h * 0.02f},
                                {x + w * 0.92f, lower_top + h * 0.03f}},
                               width));
    break;
  case 'e':
    paths.push_back(strokePath({{x + w * 0.86f, y + h * 0.40f},
                                {x + w * 0.24f, y + h * 0.40f},
                                {x + w * 0.16f, y + h * 0.18f},
                                {x + w * 0.44f, y + h * 0.02f},
                                {x + w * 0.86f, y + h * 0.12f},
                                {x + w * 0.92f, y + h * 0.50f},
                                {x + w * 0.52f, lower_top},
                                {x + w * 0.16f, y + h * 0.54f}},
                               width));
    break;
  case 'M':
    paths.push_back(strokePath({{x + w * 0.06f, y},
                                {x + w * 0.06f, top},
                                {x + w * 0.50f, mid},
                                {x + w * 0.94f, top},
                                {x + w * 0.94f, y}},
                               width));
    break;
  case 'm':
    paths.push_back(strokePath({{x + w * 0.06f, y},
                                {x + w * 0.06f, lower_top},
                                {x + w * 0.34f, y + h * 0.42f},
                                {x + w * 0.54f, lower_top},
                                {x + w * 0.74f, y + h * 0.42f},
                                {x + w * 0.94f, lower_top},
                                {x + w * 0.94f, y}},
                               width));
    break;
  case 'P':
    paths.push_back(strokePath({{x, y}, {x + w * 0.02f, top}}, width));
    paths.push_back(strokePath({{x + w * 0.02f, top},
                                {x + w * 0.82f, top + h * 0.02f},
                                {x + w * 0.92f, mid + h * 0.08f},
                                {x + w * 0.18f, mid}},
                               width));
    break;
  case 'p':
    paths.push_back(
        strokePath({{x + w * 0.08f, y - h * 0.16f}, {x + w * 0.08f, lower_top}}, width));
    paths.push_back(strokePath({{x + w * 0.08f, lower_top},
                                {x + w * 0.78f, lower_top + h * 0.02f},
                                {x + w * 0.92f, y + h * 0.42f},
                                {x + w * 0.18f, y + h * 0.32f}},
                               width));
    break;
  case '!':
    paths.push_back(strokePath({{x + w * 0.50f, top}, {x + w * 0.48f, y + h * 0.28f}}, width));
    paths.push_back(strokePath({{x + w * 0.44f, y + h * 0.02f}, {x + w * 0.58f, y + h * 0.02f}},
                               width * 1.10f));
    break;
  default:
    break;
  }
}

std::shared_ptr<const aster::CpuMesh> parkourSignStrokeMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh = [] {
    std::vector<aster::StrokePath> paths;
    struct Glyph {
      char letter;
      float width;
    };
    constexpr Glyph text[] = {{'J', 0.24f}, {'u', 0.28f}, {'m', 0.36f}, {'p', 0.28f}, {'!', 0.11f}};
    constexpr float h = 0.38f;
    constexpr float spacing = 0.085f;
    constexpr float stroke_width = 0.034f;
    float total_width = -spacing;
    for (const Glyph glyph : text) {
      total_width += glyph.width + spacing;
    }
    float x = total_width * -0.5f;
    for (const Glyph glyph : text) {
      appendSignLetter(paths, glyph.letter, x, -0.17f, glyph.width, h, stroke_width);
      x += glyph.width + spacing;
    }
    return makeSharedMesh(aster::makeStrokeRibbonMesh({std::move(paths), 0.0f}));
  }();
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> caveSignStrokeMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh = [] {
    std::vector<aster::StrokePath> paths;
    struct Glyph {
      char letter;
      float width;
    };
    constexpr Glyph text[] = {{'C', 0.24f}, {'A', 0.23f}, {'V', 0.23f}, {'E', 0.22f}};
    constexpr float h = 0.30f;
    constexpr float spacing = 0.048f;
    constexpr float stroke_width = 0.026f;
    float total_width = -spacing;
    for (const Glyph glyph : text) {
      total_width += glyph.width + spacing;
    }
    float x = total_width * -0.5f;
    for (const Glyph glyph : text) {
      appendSignLetter(paths, glyph.letter, x, -0.14f, glyph.width, h, stroke_width);
      x += glyph.width + spacing;
    }
    return makeSharedMesh(aster::makeStrokeRibbonMesh({std::move(paths), 0.0f}));
  }();
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> moundMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeMoundMesh({.radial_segments = 128,
                                           .rings = 28,
                                           .radius_x = 3.92f,
                                           .radius_z = 2.78f,
                                           .height = 0.34f,
                                           .pond_radius_x = 2.08f,
                                           .pond_radius_z = 1.44f,
                                           .pond_depth = 0.58f,
                                           .bank_width = 0.64f,
                                           .bank_height = 0.20f,
                                           .basin_floor_radius = 0.50f,
                                           .shore_depth_fraction = 0.16f,
                                           .surface_noise = 0.034f,
                                           .edge_irregularity = 0.15f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> terrainTransitionMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 144,
                                                       .rings = 12,
                                                       .inner_radius_x = 3.74f,
                                                       .inner_radius_z = 2.60f,
                                                       .outer_radius_x = 4.32f,
                                                       .outer_radius_z = 3.06f,
                                                       .inner_height = 0.034f,
                                                       .foundation_lift = 0.006f,
                                                       .surface_noise = 0.008f,
                                                       .edge_irregularity = 0.16f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> pondHardscapeApronMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 144,
                                                       .rings = 10,
                                                       .inner_radius_x = 3.98f,
                                                       .inner_radius_z = 2.76f,
                                                       .outer_radius_x = 5.36f,
                                                       .outer_radius_z = 3.88f,
                                                       .inner_height = 0.020f,
                                                       .foundation_lift = 0.016f,
                                                       .surface_noise = 0.012f,
                                                       .edge_irregularity = 0.28f,
                                                       .edge_skirt_depth = 0.0f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> pondHardscapeSubstrateMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 144,
                                                       .rings = 8,
                                                       .inner_radius_x = 3.58f,
                                                       .inner_radius_z = 2.46f,
                                                       .outer_radius_x = 5.46f,
                                                       .outer_radius_z = 3.90f,
                                                       .inner_height = 0.020f,
                                                       .foundation_lift = 0.014f,
                                                       .surface_noise = 0.010f,
                                                       .edge_irregularity = 0.22f,
                                                       .edge_skirt_depth = 0.0f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> pondHardscapeUnderlayMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePathJunctionMesh({.radial_segments = 144,
                                                  .rings = 10,
                                                  .radius_x = 5.78f,
                                                  .radius_z = 4.16f,
                                                  .crown_height = 0.006f,
                                                  .surface_noise = 0.004f,
                                                  .edge_irregularity = 0.06f,
                                                  .edge_skirt_depth = 0.16f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> pondWaterMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePondWaterMesh({.radial_segments = 144,
                                               .rings = 18,
                                               .radius_x = kCourtyardPondRadius.x,
                                               .radius_z = kCourtyardPondRadius.y,
                                               .edge_softness = 0.025f,
                                               .edge_irregularity = 0.045f,
                                               .shoreline_inset = 0.0f,
                                               .wave_amplitude = 0.014f,
                                               .wave_choppiness = 0.10f,
                                               .wind_speed = 5.8f,
                                               .wind_direction = {0.82f, 0.38f},
                                               .wave_components = 20}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> pondBasinMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeSubmergedBasinMesh({.radial_segments = 144,
                                                    .rings = 18,
                                                    .radius_x = 2.02f,
                                                    .radius_z = 1.40f,
                                                    .floor_depth = 0.24f,
                                                    .shore_depth = 0.08f,
                                                    .basin_floor_radius = 0.44f,
                                                    .edge_irregularity = 0.060f,
                                                    .bottom_noise = 0.018f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondMoundMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeMoundMesh({.radial_segments = 160,
                                           .rings = 34,
                                           .radius_x = 9.05f,
                                           .radius_z = 3.45f,
                                           .height = 0.62f,
                                           .pond_radius_x = 7.10f,
                                           .pond_radius_z = 2.35f,
                                           .pond_depth = 1.30f,
                                           .bank_width = 0.72f,
                                           .bank_height = 0.38f,
                                           .basin_floor_radius = 0.50f,
                                           .shore_depth_fraction = 0.12f,
                                           .surface_noise = 0.042f,
                                           .edge_irregularity = 0.18f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondTransitionMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 160,
                                                       .rings = 14,
                                                       .inner_radius_x = 8.48f,
                                                       .inner_radius_z = 3.08f,
                                                       .outer_radius_x = 9.90f,
                                                       .outer_radius_z = 4.10f,
                                                       .outer_radius_z_negative = 3.16f,
                                                       .outer_radius_z_positive = 4.10f,
                                                       .outer_radius_x_negative = 10.70f,
                                                       .outer_radius_x_positive = 9.90f,
                                                       .inner_height = 0.095f,
                                                       .foundation_lift = 0.012f,
                                                       .surface_noise = 0.016f,
                                                       .edge_irregularity = 0.17f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondHardscapeApronMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 176,
                                                       .rings = 12,
                                                       .inner_radius_x = 9.42f,
                                                       .inner_radius_z = 3.66f,
                                                       .outer_radius_x = 12.06f,
                                                       .outer_radius_z = 5.28f,
                                                       .outer_radius_z_negative = 3.74f,
                                                       .outer_radius_z_positive = 5.28f,
                                                       .outer_radius_x_negative = 13.35f,
                                                       .outer_radius_x_positive = 12.06f,
                                                       .inner_height = 0.026f,
                                                       .foundation_lift = 0.018f,
                                                       .surface_noise = 0.020f,
                                                       .edge_irregularity = 0.30f,
                                                       .edge_skirt_depth = 0.0f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondHardscapeSubstrateMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 176,
                                                       .rings = 10,
                                                       .inner_radius_x = 8.64f,
                                                       .inner_radius_z = 3.22f,
                                                       .outer_radius_x = 12.18f,
                                                       .outer_radius_z = 5.18f,
                                                       .outer_radius_z_negative = 3.52f,
                                                       .outer_radius_z_positive = 5.18f,
                                                       .outer_radius_x_negative = 13.35f,
                                                       .outer_radius_x_positive = 12.18f,
                                                       .inner_height = 0.026f,
                                                       .foundation_lift = 0.016f,
                                                       .surface_noise = 0.014f,
                                                       .edge_irregularity = 0.24f,
                                                       .edge_skirt_depth = 0.0f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondHardscapeUnderlayMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePathJunctionMesh({.radial_segments = 192,
                                                  .rings = 12,
                                                  .radius_x = 12.38f,
                                                  .radius_z = 5.34f,
                                                  .crown_height = 0.006f,
                                                  .surface_noise = 0.004f,
                                                  .edge_irregularity = 0.05f,
                                                  .radius_x_negative = 13.70f,
                                                  .radius_x_positive = 12.70f,
                                                  .radius_z_negative = 3.60f,
                                                  .radius_z_positive = 5.54f,
                                                  .edge_skirt_depth = 0.18f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondWaterMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePondWaterMesh({.radial_segments = 192,
                                               .rings = 24,
                                               .radius_x = 7.34f,
                                               .radius_z = 2.52f,
                                               .edge_softness = 0.035f,
                                               .edge_irregularity = 0.055f,
                                               .shoreline_inset = 0.030f,
                                               .wave_amplitude = 0.022f,
                                               .wave_choppiness = 0.14f,
                                               .wind_speed = 7.8f,
                                               .wind_direction = {0.58f, -0.42f},
                                               .wave_components = 24}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> innerPondBasinMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeSubmergedBasinMesh({.radial_segments = 192,
                                                    .rings = 24,
                                                    .radius_x = 7.06f,
                                                    .radius_z = 2.28f,
                                                    .floor_depth = 0.32f,
                                                    .shore_depth = 0.10f,
                                                    .basin_floor_radius = 0.46f,
                                                    .edge_irregularity = 0.070f,
                                                    .bottom_noise = 0.034f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> fishMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeFishMesh({.body_segments = 20,
                                          .body_rings = 9,
                                          .body_length = 0.48f,
                                          .body_height = 0.15f,
                                          .body_width = 0.10f,
                                          .tail_length = 0.18f,
                                          .fin_span = 0.13f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> crocodileMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeAmphibiousPredatorMesh({.body_segments = 30,
                                                        .body_rings = 11,
                                                        .radial_segments = 10,
                                                        .scute_count = 12,
                                                        .body_length = 0.98f,
                                                        .body_width = 0.24f,
                                                        .body_height = 0.115f,
                                                        .head_length = 0.32f,
                                                        .head_width = 0.19f,
                                                        .head_height = 0.105f,
                                                        .snout_length = 0.46f,
                                                        .snout_width = 0.14f,
                                                        .snout_height = 0.054f,
                                                        .tail_length = 0.92f,
                                                        .tail_tip_width = 0.042f,
                                                        .leg_length = 0.29f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> broadLeafPlantMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeBroadLeafPlantMesh({.leaf_count = 11,
                                                    .radius = 0.30f,
                                                    .min_height = 0.24f,
                                                    .max_height = 0.62f,
                                                    .leaf_width = 0.095f,
                                                    .leaf_length = 0.36f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> climbableTreeTrunkMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeClimbableTreeTrunkMesh({.radial_segments = 20,
                                                        .height_segments = 10,
                                                        .radius = 0.38f,
                                                        .height = 3.75f,
                                                        .root_radius = 0.88f,
                                                        .bark_ridge = 0.040f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> treeCanopyMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTreeCanopyMesh(
          {.leaf_clusters = 54, .radius_x = 1.54f, .radius_y = 0.94f, .radius_z = 1.34f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> birdGroveGroundMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeMoundMesh({.radial_segments = 96,
                                           .rings = 18,
                                           .radius_x = 2.82f,
                                           .radius_z = 2.04f,
                                           .height = 0.18f,
                                           .pond_radius_x = 0.34f,
                                           .pond_radius_z = 0.26f,
                                           .pond_depth = 0.0f,
                                           .bank_width = 0.58f,
                                           .bank_height = 0.08f,
                                           .basin_floor_radius = 0.42f,
                                           .shore_depth_fraction = 0.0f,
                                           .surface_noise = 0.026f,
                                           .edge_irregularity = 0.12f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> birdGroveTransitionMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeTerrainTransitionMesh({.radial_segments = 128,
                                                       .rings = 10,
                                                       .inner_radius_x = 2.12f,
                                                       .inner_radius_z = 1.48f,
                                                       .outer_radius_x = 3.10f,
                                                       .outer_radius_z = 2.24f,
                                                       .inner_height = 0.034f,
                                                       .foundation_lift = 0.008f,
                                                       .surface_noise = 0.014f,
                                                       .edge_irregularity = 0.16f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> gothicUnderpassMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeGothicUnderpassMesh({.half_width = 1.26f,
                                                     .height = 1.92f,
                                                     .depth = 1.62f,
                                                     .passage_half_width = 0.54f,
                                                     .passage_height = 1.24f,
                                                     .buttress_width = 0.18f,
                                                     .trim_depth = 0.12f,
                                                     .arch_steps = 6,
                                                     .parapet_blocks = 5}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> signalSentinelMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeSignalSentinelMesh({.radial_segments = 9,
                                                    .fin_count = 5,
                                                    .height = 1.92f,
                                                    .body_radius = 0.36f,
                                                    .fin_span = 0.50f,
                                                    .fin_height = 0.82f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> compactBirdMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeBirdMesh({.body_segments = 18,
                                          .body_rings = 8,
                                          .body_length = 0.46f,
                                          .body_height = 0.17f,
                                          .body_width = 0.13f,
                                          .head_radius = 0.095f,
                                          .beak_length = 0.080f,
                                          .wing_span = 0.58f,
                                          .wing_chord = 0.21f,
                                          .tail_length = 0.18f,
                                          .crest_height = 0.0f,
                                          .leg_length = 0.075f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> crestedBirdMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeBirdMesh({.body_segments = 20,
                                          .body_rings = 9,
                                          .body_length = 0.54f,
                                          .body_height = 0.20f,
                                          .body_width = 0.16f,
                                          .head_radius = 0.12f,
                                          .beak_length = 0.10f,
                                          .wing_span = 0.72f,
                                          .wing_chord = 0.25f,
                                          .tail_length = 0.26f,
                                          .crest_height = 0.13f,
                                          .leg_length = 0.09f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> longTailBirdMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeBirdMesh({.body_segments = 20,
                                          .body_rings = 9,
                                          .body_length = 0.50f,
                                          .body_height = 0.18f,
                                          .body_width = 0.14f,
                                          .head_radius = 0.105f,
                                          .beak_length = 0.095f,
                                          .wing_span = 0.66f,
                                          .wing_chord = 0.22f,
                                          .tail_length = 0.42f,
                                          .crest_height = 0.045f,
                                          .leg_length = 0.075f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> castleBirdNestMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeBirdNestMesh({.twig_count = 64,
                                              .radial_segments = 7,
                                              .outer_radius = 0.64f,
                                              .inner_radius = 0.26f,
                                              .depth = 0.20f,
                                              .twig_radius = 0.018f,
                                              .height = 0.30f,
                                              .seed = 137}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> eyeCrossMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh = [] {
    std::vector<aster::StrokePath> paths;
    paths.push_back(strokePath({{-0.46f, -0.42f}, {0.46f, 0.42f}}, 0.18f));
    paths.push_back(strokePath({{-0.46f, 0.42f}, {0.46f, -0.42f}}, 0.18f));
    return makeSharedMesh(aster::makeStrokeRibbonMesh({std::move(paths), 0.0f}));
  }();
  return mesh;
}

aster::Vec3 castlePathPortalPoint() {
  return {0.02f, 0.050f, -4.72f};
}

aster::Vec3 castleExteriorGateTrailhead() {
  return {0.0f, 0.075f, 8.95f};
}

aster::PathRibbonMeshSpec castleUnderpassPathSpec() {
  const aster::Vec3 portal = castlePathPortalPoint();
  return {.segments = 42,
          .width = 0.60f,
          .width_variation = 0.16f,
          .crown_height = 0.020f,
          .surface_noise = 0.010f,
          .endpoint_taper = 0.14f,
          .start = portal,
          .control = {-0.42f, 0.060f, -5.92f},
          .control_b = {0.38f, 0.062f, -8.10f},
          .end = {0.15f, 0.060f, -9.78f}};
}

std::shared_ptr<const aster::CpuMesh> castleUnderpassPathMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePathRibbonMesh(castleUnderpassPathSpec()));
  return mesh;
}

aster::PathRibbonMeshSpec castleNestPathSpec() {
  return {.segments = 48,
          .width = 0.62f,
          .width_variation = 0.14f,
          .crown_height = 0.025f,
          .surface_noise = 0.012f,
          .endpoint_taper = 0.16f,
          .start = castleUnderpassPathSpec().end,
          .control = {-1.22f, 0.070f, -12.18f},
          .control_b = {-3.72f, 0.075f, -15.48f},
          .end = {-3.85f, 0.082f, -18.10f}};
}

std::shared_ptr<const aster::CpuMesh> castleNestPathMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePathRibbonMesh(castleNestPathSpec()));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> castlePathPortalBlendMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makePathJunctionMesh({.radial_segments = 64,
                                                  .rings = 6,
                                                  .radius_x = 0.74f,
                                                  .radius_z = 0.54f,
                                                  .crown_height = 0.026f,
                                                  .surface_noise = 0.008f,
                                                  .edge_irregularity = 0.10f}));
  return mesh;
}

aster::PathShoulderMeshSpec pathShoulderSpec(aster::PathRibbonMeshSpec path,
                                             const float shoulder_width,
                                             const float shoulder_height) {
  return {.path = path,
          .side_segments = 4,
          .shoulder_width = shoulder_width,
          .shoulder_height = shoulder_height,
          .outer_drop = shoulder_height * 0.22f,
          .surface_noise = shoulder_height * 0.18f};
}

aster::PathRibbonMeshSpec naturePathSpec(const aster::TerrainMeshClipEllipse &pond_clip) {
  const aster::Vec3 designed_bank_approach{4.15f, 0.070f, 2.58f};
  constexpr float path_width = 0.86f;
  return {.segments = 56,
          .width = path_width,
          .width_variation = 0.20f,
          .crown_height = 0.038f,
          .surface_noise = 0.018f,
          .endpoint_taper = 0.18f,
          .start = castlePathPortalPoint(),
          .control = {-0.76f, 0.055f, -2.72f},
          .control_b = {4.88f, 0.062f, -0.08f},
          .end = ellipseBankPoint(pond_clip, designed_bank_approach, path_width * 0.56f)};
}

aster::Vec3 caveEntrancePlanar() {
  return kCaveEntrancePlanar;
}

aster::Vec3 caveInwardDirection() {
  return aster::normalize(kCaveInwardPlanar);
}

aster::PathRibbonMeshSpec caveApproachPathSegment(const int segments, const aster::Vec3 start,
                                                  const aster::Vec3 control,
                                                  const aster::Vec3 control_b,
                                                  const aster::Vec3 end) {
  return {.segments = segments,
          .width = kCaveApproachPathWidth,
          .width_variation = 0.36f,
          .crown_height = 0.030f,
          .surface_noise = 0.026f,
          .endpoint_taper = 0.0f,
          .endpoint_width_floor = 1.0f,
          .start = start,
          .control = control,
          .control_b = control_b,
          .end = end};
}

std::vector<aster::PathRibbonMeshSpec> caveApproachPathSpecs() {
  const aster::Vec3 entrance = caveEntrancePlanar();
  const aster::Vec3 gate = castleExteriorGateTrailhead();
  const aster::Vec3 outside_turn{11.8f, 0.082f, 12.55f};
  const aster::Vec3 lower_slope{30.6f, 0.096f, 6.35f};
  const aster::Vec3 upper_slope{49.2f, 0.112f, -22.4f};
  const aster::Vec3 cave_lip{entrance.x, 0.120f, entrance.z + 1.22f};

  return {
      caveApproachPathSegment(20, gate, {2.3f, 0.078f, 9.65f}, {6.6f, 0.082f, 12.8f}, outside_turn),
      caveApproachPathSegment(26, outside_turn, {16.8f, 0.087f, 16.2f}, {25.4f, 0.094f, 10.9f},
                              lower_slope),
      caveApproachPathSegment(34, lower_slope, {40.8f, 0.105f, 1.8f}, {54.6f, 0.114f, -9.4f},
                              upper_slope),
      caveApproachPathSegment(34, upper_slope, {57.4f, 0.122f, -36.2f}, {42.1f, 0.126f, -54.7f},
                              cave_lip)};
}

std::vector<aster::PathRibbonMeshSpec> abandonedGothicHousePathSpecs() {
  const std::vector<aster::PathRibbonMeshSpec> cave_path = caveApproachPathSpecs();
  const aster::Vec3 branch =
      cave_path.size() > 1u ? aster::evaluatePathRibbonCenter(cave_path[1], 0.38f)
                            : aster::Vec3{18.5f, 0.09f, 13.8f};
  const aster::Vec3 hedge_turn{24.8f, 0.104f, 22.6f};
  const aster::Vec3 rise{34.2f, 0.116f, 28.8f};
  const aster::Vec3 doorstep{43.6f, 0.128f, 31.4f};

  return {caveApproachPathSegment(18, branch, {18.9f, 0.096f, 18.4f},
                                  {21.6f, 0.102f, 21.8f}, hedge_turn),
          caveApproachPathSegment(22, hedge_turn, {28.0f, 0.108f, 25.9f},
                                  {31.5f, 0.114f, 29.2f}, rise),
          caveApproachPathSegment(22, rise, {38.0f, 0.122f, 31.2f},
                                  {41.1f, 0.126f, 31.7f}, doorstep)};
}

aster::TerrainMountainBrushSpec lumenCaveMountainBrushSpec() {
  const aster::Vec3 entrance = caveEntrancePlanar();
  const aster::Vec3 inward = caveInwardDirection();
  return {.seed = kLumenCaveSeed + 313u,
          .center = {entrance.x + inward.x * 10.4f, entrance.z + inward.z * 10.4f},
          .radius = {32.0f, 38.0f},
          .height = 5.35f,
          .shoulder_height = 1.08f,
          .surface_noise = 0.36f,
          .ridge_frequency = 0.064f,
          .inner_plateau = 0.0f};
}

float sampledTerrainHeightOrZero(const aster::TerrainHeightField &terrain,
                                 const aster::Vec3 position) {
  const aster::TerrainSurfaceSample sample =
      aster::sampleTerrain(terrain, {position.x, position.z});
  return sample.valid ? sample.height : 0.0f;
}

aster::TerrainPortalShelfSpec lumenCavePortalShelfSpec(const float floor_y) {
  return {.seed = kLumenCaveSeed + 509u,
          .entrance = {caveEntrancePlanar().x, floor_y, caveEntrancePlanar().z},
          .inward = caveInwardDirection(),
          .floor_height = floor_y,
          .half_width = 1.66f,
          .front_depth = 1.12f,
          .back_depth = 4.65f,
          .feather = 2.10f,
          .side_berm_height = 0.36f,
          .rear_cover_height = 2.75f,
          .surface_noise = 0.30f};
}

aster::TerrainSplineRoute lumenCaveTerrainRoute(const aster::TerrainHeightField &terrain,
                                                const float floor_y) {
  const std::vector<aster::PathRibbonMeshSpec> paths = caveApproachPathSpecs();
  const auto path_height = [&terrain](const aster::Vec3 point, const float offset) {
    return sampledTerrainHeightOrZero(terrain, point) + offset;
  };
  aster::TerrainSplineRoute route;
  route.segments.reserve(paths.size());
  for (std::size_t index = 0; index < paths.size(); ++index) {
    const aster::PathRibbonMeshSpec &path = paths[index];
    const bool final_segment = index + 1u == paths.size();
    route.segments.push_back(
        {.samples = std::max(path.segments, 8),
         .start = {path.start.x, path_height(path.start, 0.018f), path.start.z},
         .control = {path.control.x, path_height(path.control, 0.020f), path.control.z},
         .control_b = {path.control_b.x, path_height(path.control_b, 0.022f), path.control_b.z},
         .end = {path.end.x, final_segment ? floor_y + 0.018f : path_height(path.end, 0.018f),
                 path.end.z}});
  }
  return route;
}

void sculptLumenCaveTerrain(aster::TerrainHeightField &terrain) {
  aster::applyTerrainMountainBrush(terrain, lumenCaveMountainBrushSpec());
  const aster::Vec3 entrance = caveEntrancePlanar();
  const float floor_y = sampledTerrainHeightOrZero(terrain, entrance) - 0.12f;
  const aster::TerrainPortalShelfSpec shelf = lumenCavePortalShelfSpec(floor_y);
  aster::sculptTerrainPortalShelf(terrain, shelf);
  aster::deformTerrainAlongRoute(terrain, {.route = lumenCaveTerrainRoute(terrain, floor_y),
                                           .half_width = kCaveApproachPathWidth * 0.54f,
                                           .shoulder_width = 0.92f,
                                           .crown_height = 0.030f,
                                           .surface_noise = 0.010f,
                                           .max_raise = 0.18f,
                                           .max_lower = 0.22f,
                                           .seed = kLumenCaveSeed + 617u});
  aster::sculptTerrainPortalShelf(terrain, shelf);
}

aster::CaveTunnelProfile lumenCaveTunnelProfile(const float floor_y) {
  const aster::Vec3 entrance = caveEntrancePlanar();
  return {.seed = kLumenCaveSeed,
          .start = {entrance.x, floor_y + 0.030f, entrance.z},
          .control = {entrance.x - 1.05f, floor_y - 1.45f, entrance.z - 7.20f},
          .control_b = {entrance.x + 3.65f, floor_y - 4.85f, entrance.z - 19.20f},
          .end = {entrance.x - 0.86f, floor_y - 6.35f, entrance.z - 30.20f},
          .length_segments = 112,
          .radial_segments = 36,
          .half_width = 1.76f,
          .wall_height = 2.58f,
          .floor_width = 2.52f,
          .floor_crown = 0.030f,
          .floor_edge_raise = 0.075f,
          .wall_noise = 0.30f,
          .visible_wall_start_t = 0.0f,
          .collision_start_t = 0.0f,
          .collision_end_t = 0.965f,
          .ore_start_t = 0.24f,
          .chest_t = 0.78f,
          .chamber_t = 0.74f,
          .chamber_falloff = 0.25f,
          .chamber_width_scale = 2.55f,
          .chamber_height_scale = 1.62f,
          .end_constraint_enabled = false};
}

aster::CaveComplexSpec lumenCaveComplexSpec(const float floor_y) {
  aster::CaveComplexSpec spec;
  spec.tunnel = lumenCaveTunnelProfile(floor_y);
  spec.portal = {.arch_segments = 48,
                 .inner_half_width = 1.48f,
                 .inner_height = 1.78f,
                 .outer_half_width = 2.28f,
                 .outer_height = 2.78f,
                 .depth = 1.54f,
                 .ground_lip = 0.08f,
                 .inner_lining_depth = 2.20f,
                 .lining_breakup = 0.052f};
  spec.ore = {.seed = kLumenCaveSeed + 211u,
              .candidates = 132,
              .max_nodes = 20,
              .field_frequency_a = 0.38f,
              .field_frequency_b = 0.76f,
              .intersection_threshold_a = 0.27f,
              .intersection_threshold_b = 0.25f,
              .wall_inset = 0.040f,
              .min_spacing = 0.98f};
  spec.features = {.seed = kLumenCaveSeed + 421u,
                   .candidates = 168,
                   .max_features = 38,
                   .start_t = 0.06f,
                   .end_t = 0.92f,
                   .min_spacing = 0.78f,
                   .wall_inset = 0.12f,
                   .ceiling_fraction = 0.30f,
                   .column_fraction = 0.06f,
                   .shelf_fraction = 0.22f,
                   .mineral_fraction = 0.28f};
  return spec;
}

aster::CaveComplexSpec lumenTerrainCoveredCaveComplexSpec(const aster::TerrainHeightField &terrain,
                                                          const float floor_y) {
  aster::CaveComplexSpec spec = lumenCaveComplexSpec(floor_y);
  const aster::CaveTerrainCoverFit cover_fit = aster::fitCaveTunnelToTerrainCover(
      spec.tunnel,
      [&terrain](const aster::Vec2 position) {
        const aster::TerrainSurfaceSample sample = aster::sampleTerrain(terrain, position);
        return aster::CaveTerrainCoverSample{sample.valid, sample.valid ? sample.height : 0.0f};
      },
      {.samples = 96,
       .required_consecutive_samples = 3,
       .min_t = 0.0f,
       .max_t = 0.52f,
       .roof_clearance = 0.24f});
  if (cover_fit.cover_found) {
    spec.tunnel = cover_fit.tunnel;
    spec.tunnel.visible_wall_start_t =
        aster::clamp(cover_fit.first_covered_t - 0.018f, 0.0f, 0.40f);
  }
  spec.tunnel.visible_wall_start_t = std::min(spec.tunnel.visible_wall_start_t, 0.035f);
  spec.features.start_t =
      std::max(spec.features.start_t,
               aster::clamp(spec.tunnel.visible_wall_start_t + 0.18f, 0.0f, spec.features.end_t));
  return spec;
}

aster::Vec3 caveEntranceLightPosition(const float floor_y) {
  const aster::Vec3 entrance = caveEntrancePlanar();
  return {entrance.x, floor_y + 1.10f, entrance.z + 1.38f};
}

aster::CaveWallFixtureProfile lumenCaveWallFixtureProfile() {
  return {.start_t = 0.28f,
          .end_t = 0.72f,
          .target_spacing = 4.6f,
          .max_count = 5,
          .wall_side = -1.0f,
          .mount_height = 1.22f,
          .wall_inset = 0.24f,
          .lens_offset = 0.095f,
          .light_offset = 0.22f};
}

aster::CaveWallFixtureProfile lumenCaveSecondaryWallFixtureProfile() {
  aster::CaveWallFixtureProfile profile = lumenCaveWallFixtureProfile();
  profile.start_t = 0.34f;
  profile.end_t = 0.66f;
  profile.target_spacing = 6.2f;
  profile.max_count = 3;
  profile.wall_side = 1.0f;
  return profile;
}

aster::Vec3 caveTunnelBodyCenter(const aster::CaveTunnelFrame &frame) {
  const float radius = std::min(frame.half_width * 0.92f, frame.height * 0.52f);
  return frame.floor_center + frame.up * radius;
}

float caveTunnelBodyRadius(const aster::CaveTunnelFrame &frame) {
  return std::min(frame.half_width * 0.92f, frame.height * 0.52f);
}

aster::CaveTunnelProfile lumenDeepCaveTunnelProfile(const aster::CaveTunnelProfile &entry) {
  const aster::CaveTunnelFrame gate = sampleCaveTunnelFrame(entry, 1.0f);
  const aster::Vec3 forward =
      aster::length(gate.tangent) > 0.0001f ? aster::normalize(gate.tangent)
                                            : aster::Vec3{0.0f, 0.0f, -1.0f};
  const aster::Vec3 side = aster::length(gate.side) > 0.0001f
                               ? aster::normalize(gate.side)
                               : aster::Vec3{1.0f, 0.0f, 0.0f};
  const aster::Vec3 up = aster::length(gate.up) > 0.0001f
                             ? aster::normalize(gate.up)
                             : aster::Vec3{0.0f, 1.0f, 0.0f};
  const float body_radius = std::max(caveTunnelBodyRadius(gate), 0.80f);
  const float entry_length = estimateCaveTunnelLength(entry);
  const float span = std::max(entry_length * 1.35f, body_radius * 30.0f);
  const aster::Vec3 start = gate.floor_center;
  return {.seed = kLumenCaveSeed + 6121u,
          .start = start,
          .control = start + forward * (span * 0.30f) + side * (body_radius * 1.25f) -
                     up * (body_radius * 0.55f),
          .control_b = start + forward * (span * 0.68f) - side * (body_radius * 2.10f) -
                       up * (body_radius * 2.25f),
          .end = start + forward * span + side * (body_radius * 0.70f) -
                 up * (body_radius * 3.20f),
          .length_segments = std::max(entry.length_segments + entry.length_segments / 2, 144),
          .radial_segments = entry.radial_segments,
          .half_width = entry.half_width * 1.04f,
          .wall_height = entry.wall_height * 1.02f,
          .floor_width = entry.floor_width * 1.04f,
          .floor_crown = entry.floor_crown,
          .floor_edge_raise = entry.floor_edge_raise,
          .wall_noise = entry.wall_noise,
          .visible_wall_start_t = 0.0f,
          .collision_start_t = 0.0f,
          .collision_end_t = 1.0f,
          .ore_start_t = 0.10f,
          .chest_t = 0.54f,
          .chamber_t = 0.52f,
          .chamber_falloff = 0.24f,
          .chamber_width_scale = 2.10f,
          .chamber_height_scale = 1.42f,
          .end_constraint_enabled = true};
}

aster::CaveComplexSpec lumenDeepCaveComplexSpec(const aster::CaveTunnelProfile &entry) {
  aster::CaveComplexSpec spec;
  spec.tunnel = lumenDeepCaveTunnelProfile(entry);
  spec.ore = {.seed = kLumenCaveSeed + 6211u,
              .candidates = 160,
              .max_nodes = 18,
              .field_frequency_a = 0.40f,
              .field_frequency_b = 0.82f,
              .intersection_threshold_a = 0.28f,
              .intersection_threshold_b = 0.24f,
              .wall_inset = 0.045f,
              .min_spacing = 1.05f};
  spec.features = {.seed = kLumenCaveSeed + 6421u,
                   .candidates = 210,
                   .max_features = 54,
                   .start_t = 0.04f,
                   .end_t = 0.96f,
                   .min_spacing = 0.86f,
                   .wall_inset = 0.13f,
                   .ceiling_fraction = 0.34f,
                   .column_fraction = 0.08f,
                   .shelf_fraction = 0.22f,
                   .mineral_fraction = 0.30f};
  return spec;
}

aster::CaveWallFixtureProfile lumenDeepCaveWallFixtureProfile() {
  aster::CaveWallFixtureProfile profile = lumenCaveWallFixtureProfile();
  profile.start_t = 0.08f;
  profile.end_t = 0.94f;
  profile.target_spacing = 5.2f;
  profile.max_count = 9;
  profile.wall_side = -1.0f;
  return profile;
}

aster::CaveWallFixtureProfile lumenDeepCaveSecondaryWallFixtureProfile() {
  aster::CaveWallFixtureProfile profile = lumenDeepCaveWallFixtureProfile();
  profile.start_t = 0.16f;
  profile.end_t = 0.88f;
  profile.target_spacing = 7.0f;
  profile.max_count = 6;
  profile.wall_side = 1.0f;
  return profile;
}

struct LumenCaveWebPlacement {
  aster::Vec3 center{};
  aster::Vec3 normal{0.0f, 0.0f, -1.0f};
  aster::Vec3 side{1.0f, 0.0f, 0.0f};
  aster::Vec3 up{0.0f, 1.0f, 0.0f};
  float radius_x = 1.0f;
  float radius_y = 1.0f;
  float thickness = 0.42f;
};

LumenCaveWebPlacement lumenCaveWebPlacement(const float floor_y) {
  const aster::CaveTunnelProfile tunnel = lumenCaveTunnelProfile(floor_y);
  const aster::CaveTunnelFrame end_frame = sampleCaveTunnelFrame(tunnel, 1.0f);
  const float end_radius = caveTunnelBodyRadius(end_frame);
  return {.center = caveTunnelBodyCenter(end_frame) + end_frame.tangent * 0.75f,
          .normal = end_frame.tangent,
          .side = end_frame.side,
          .up = end_frame.up,
          .radius_x = std::max(end_frame.half_width * 1.08f, end_radius * 1.18f),
          .radius_y = std::max(end_frame.height * 0.58f, end_radius * 1.04f),
          .thickness = std::max(0.34f, end_radius * 0.30f)};
}

std::shared_ptr<const aster::CpuMesh> grassTuftMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeGrassTuftMesh({.blades = 18,
                                               .radius = 0.22f,
                                               .min_height = 0.18f,
                                               .max_height = 0.40f,
                                               .min_width = 0.016f,
                                               .max_width = 0.034f,
                                               .bend = 0.070f}));
  return mesh;
}

aster::Vec3 naturePathPoint(aster::PathRibbonMeshSpec spec, const float t) {
  spec.start.y += 0.040f;
  spec.control.y += 0.040f;
  spec.control_b.y += 0.040f;
  spec.end.y += 0.040f;
  return aster::evaluatePathRibbonCenter(spec, t);
}

aster::Vec3 naturePathSide(const aster::PathRibbonMeshSpec &spec, const float t) {
  aster::Vec3 tangent = aster::evaluatePathRibbonTangent(spec, t);
  if (aster::length(tangent) <= 0.0001f) {
    tangent = {0.0f, 0.0f, 1.0f};
  }
  return aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, tangent));
}

float distanceToSegment(const aster::Vec2 point, const aster::Vec2 a, const aster::Vec2 b) {
  const aster::Vec2 segment = b - a;
  const float len_sq = std::max(aster::dot(segment, segment), 0.000001f);
  const float t = aster::clamp(aster::dot(point - a, segment) / len_sq, 0.0f, 1.0f);
  return aster::length(point - (a + segment * t));
}

float distanceToPathRoute(const std::vector<aster::PathRibbonMeshSpec> &path_specs,
                          const aster::Vec2 point) {
  float distance = std::numeric_limits<float>::max();
  for (const aster::PathRibbonMeshSpec &path : path_specs) {
    const int samples = std::max(path.segments, 12);
    aster::Vec3 previous = aster::evaluatePathRibbonCenter(path, 0.0f);
    for (int sample = 1; sample <= samples; ++sample) {
      const float t = static_cast<float>(sample) / static_cast<float>(samples);
      const aster::Vec3 current = aster::evaluatePathRibbonCenter(path, t);
      distance = std::min(distance, distanceToSegment(point, {previous.x, previous.z},
                                                     {current.x, current.z}));
      previous = current;
    }
  }
  return distance;
}

aster::Vec2 pathRouteMin(const std::vector<aster::PathRibbonMeshSpec> &path_specs) {
  aster::Vec2 min_bounds{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  for (const aster::PathRibbonMeshSpec &path : path_specs) {
    const aster::Vec3 points[] = {path.start, path.control, path.control_b, path.end};
    for (const aster::Vec3 point : points) {
      min_bounds.x = std::min(min_bounds.x, point.x);
      min_bounds.y = std::min(min_bounds.y, point.z);
    }
  }
  return min_bounds;
}

aster::Vec2 pathRouteMax(const std::vector<aster::PathRibbonMeshSpec> &path_specs) {
  aster::Vec2 max_bounds{std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest()};
  for (const aster::PathRibbonMeshSpec &path : path_specs) {
    const aster::Vec3 points[] = {path.start, path.control, path.control_b, path.end};
    for (const aster::Vec3 point : points) {
      max_bounds.x = std::max(max_bounds.x, point.x);
      max_bounds.y = std::max(max_bounds.y, point.z);
    }
  }
  return max_bounds;
}

std::shared_ptr<const aster::CpuMesh> fishingRodMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeCableMesh({.construction = aster::CableConstruction::BraidedSleeve,
                                           .radial_segments = 16,
                                           .length_segments = 12,
                                           .strand_count = 6,
                                           .radius = 1.0f,
                                           .length = 1.0f,
                                           .strand_depth = 0.006f,
                                           .twist_turns = 0.035f,
                                           .strand_radius = 0.10f,
                                           .strand_center_radius = 0.18f,
                                           .fiber_twist_turns = 0.12f}));
  return mesh;
}

std::shared_ptr<const aster::CpuMesh> fishingLineMesh() {
  static const std::shared_ptr<const aster::CpuMesh> mesh =
      makeSharedMesh(aster::makeCableMesh({.construction = aster::CableConstruction::BraidedSleeve,
                                           .radial_segments = 12,
                                           .length_segments = 10,
                                           .strand_count = 6,
                                           .radius = 1.0f,
                                           .length = 1.0f,
                                           .strand_depth = 0.025f,
                                           .twist_turns = 0.10f,
                                           .strand_radius = 0.18f,
                                           .strand_center_radius = 0.28f,
                                           .fiber_twist_turns = 0.8f}));
  return mesh;
}

std::vector<aster::TerrainMeshClipEllipse> terrainWaterClips(const aster::Vec3 pond_center,
                                                             const aster::Vec3 inner_pond_center,
                                                             const aster::Vec2 inner_pond_radius) {
  return {{{pond_center.x, pond_center.z}, {5.48f, 3.92f}},
          {{inner_pond_center.x, inner_pond_center.z},
           {inner_pond_radius.x + 5.18f, inner_pond_radius.y + 2.98f},
           inner_pond_radius.x + 6.15f,
           inner_pond_radius.x + 5.18f,
           inner_pond_radius.y + 1.18f,
           inner_pond_radius.y + 2.98f}};
}

aster::TerrainMeshClipEllipse birdGroveClip(const aster::Vec3 nest_position) {
  return {{nest_position.x, nest_position.z}, {2.34f, 1.62f}};
}

std::vector<aster::TerrainMeshClipEllipse> plazaWetlandClips(const aster::Vec3 pond_center,
                                                             const aster::Vec3 inner_pond_center,
                                                             const aster::Vec2 inner_pond_radius) {
  return {{{pond_center.x, pond_center.z}, {2.48f, 1.72f}},
          {{inner_pond_center.x, inner_pond_center.z},
           {inner_pond_radius.x + 0.28f, inner_pond_radius.y + 0.18f}}};
}

std::vector<aster::TerrainMeshClipEllipse>
hardscapeOcclusionClips(const aster::Vec3 pond_center, const aster::Vec3 inner_pond_center,
                        const aster::Vec2 inner_pond_radius) {
  return terrainWaterClips(pond_center, inner_pond_center, inner_pond_radius);
}

std::vector<aster::TerrainMeshClipEllipse>
terrainSurfaceHoles(const aster::Vec3 pond_center, const aster::Vec3 inner_pond_center,
                    const aster::Vec2 inner_pond_radius,
                    const aster::TerrainMeshClipEllipse &bird_grove_clip) {
  std::vector<aster::TerrainMeshClipEllipse> clips =
      hardscapeOcclusionClips(pond_center, inner_pond_center, inner_pond_radius);
  clips.push_back(bird_grove_clip);
  return clips;
}

aster::TerrainMeshClipOrientedEllipse cavePortalTerrainClip() {
  const aster::CaveComplexSpec cave_spec = lumenCaveComplexSpec(0.0f);
  const aster::CaveTerrainPortalCut cut =
      aster::makeCaveTerrainPortalCut(cave_spec.tunnel, cave_spec.portal, 0.050f);
  return {.center = cut.center,
          .forward = cut.forward,
          .radius = cut.radius,
          .radius_side_negative = cut.radius_side_negative,
          .radius_side_positive = cut.radius_side_positive,
          .radius_forward_negative = cut.radius_forward_negative,
          .radius_forward_positive = cut.radius_forward_positive};
}

std::shared_ptr<const aster::CpuMesh>
terrainMesh(const aster::TerrainHeightField &terrain,
            const std::vector<aster::TerrainMeshClipEllipse> &clip_ellipses,
            const std::vector<aster::TerrainMeshClipBox> &clip_boxes,
            const std::vector<aster::TerrainMeshClipOrientedEllipse> &oriented_clip_ellipses = {}) {
  aster::TerrainMeshBuildOptions options;
  options.subdivisions_per_square = 2;
  options.smooth_visual_surface = true;
  options.clip_ellipses = clip_ellipses;
  options.clip_oriented_ellipses = oriented_clip_ellipses;
  options.clip_boxes = clip_boxes;
  return makeSharedMesh(aster::makeTerrainMesh(terrain, options));
}

bool insideClipLocal(const aster::Vec2 local_point, const aster::Vec3 world_center,
                     const aster::TerrainMeshClipEllipse &clip) {
  return insideTerrainClipEllipse({world_center.x + local_point.x, world_center.z + local_point.y},
                                  clip);
}

aster::Vec2 clipBoundaryPointLocal(const aster::Vec2 from, const aster::Vec2 to,
                                   const aster::Vec3 world_center,
                                   const aster::TerrainMeshClipEllipse &clip) {
  const bool from_inside = insideClipLocal(from, world_center, clip);
  float low = 0.0f;
  float high = 1.0f;
  for (int step = 0; step < 24; ++step) {
    const float mid = (low + high) * 0.5f;
    const aster::Vec2 point = from + (to - from) * mid;
    if (insideClipLocal(point, world_center, clip) == from_inside) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return from + (to - from) * ((low + high) * 0.5f);
}

std::vector<aster::Vec2> clipPolygonOutsideEllipse(const std::vector<aster::Vec2> &polygon,
                                                   const aster::Vec3 world_center,
                                                   const aster::TerrainMeshClipEllipse &clip) {
  if (polygon.empty()) {
    return {};
  }
  std::vector<aster::Vec2> clipped;
  clipped.reserve(polygon.size() + 2);
  aster::Vec2 previous = polygon.back();
  bool previous_keep = !insideClipLocal(previous, world_center, clip);
  for (const aster::Vec2 current : polygon) {
    const bool current_keep = !insideClipLocal(current, world_center, clip);
    if (current_keep != previous_keep) {
      clipped.push_back(clipBoundaryPointLocal(previous, current, world_center, clip));
    }
    if (current_keep) {
      clipped.push_back(current);
    }
    previous = current;
    previous_keep = current_keep;
  }
  return clipped;
}

std::shared_ptr<const aster::CpuMesh>
plazaDeckMesh(const aster::Vec2 size, const aster::Vec3 world_center,
              const std::vector<aster::TerrainMeshClipEllipse> &clip_ellipses) {
  constexpr float kDeckCellSize = 0.21f;
  const int columns = std::max(1, static_cast<int>(std::ceil(size.x / kDeckCellSize)));
  const int rows = std::max(1, static_cast<int>(std::ceil(size.y / kDeckCellSize)));
  const float cell_width = size.x / static_cast<float>(columns);
  const float cell_depth = size.y / static_cast<float>(rows);
  const float half_width = size.x * 0.5f;
  const float half_depth = size.y * 0.5f;

  aster::CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(columns * rows * 4));
  mesh.indices.reserve(static_cast<std::size_t>(columns * rows * 6));

  const auto appendPolygon = [&](const std::vector<aster::Vec2> &polygon) {
    if (polygon.size() < 3) {
      return;
    }
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const aster::Vec2 point : polygon) {
      mesh.vertices.push_back({{point.x, 0.0f, point.y},
                               {0.0f, 1.0f, 0.0f},
                               {point.x / cell_width, point.y / cell_depth}});
    }
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
      mesh.indices.insert(mesh.indices.end(), {base, base + static_cast<std::uint32_t>(i + 1),
                                               base + static_cast<std::uint32_t>(i)});
    }
  };

  for (int row = 0; row < rows; ++row) {
    const float z0 = -half_depth + static_cast<float>(row) * cell_depth;
    const float z1 = z0 + cell_depth;
    for (int column = 0; column < columns; ++column) {
      const float x0 = -half_width + static_cast<float>(column) * cell_width;
      const float x1 = x0 + cell_width;
      std::vector<aster::Vec2> polygon{{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
      for (const aster::TerrainMeshClipEllipse &clip : clip_ellipses) {
        polygon = clipPolygonOutsideEllipse(polygon, world_center, clip);
        if (polygon.empty()) {
          break;
        }
      }
      appendPolygon(polygon);
    }
  }

  return makeSharedMesh(mesh);
}

aster::Vec3 lineCurvePoint(const aster::Vec3 start, const aster::Vec3 end, const float t,
                           const float sag, const float side_sway) {
  const aster::Vec3 chord = end - start;
  aster::Vec3 side = aster::normalize(aster::cross(chord, {0.0f, 1.0f, 0.0f}));
  if (aster::length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  const float arc = std::sin(t * kPi);
  return start + chord * t + aster::Vec3{0.0f, -sag * arc, 0.0f} + side * (side_sway * arc);
}

bool overlapsFishingComposition(const aster::Vec3 position) {
  const aster::Vec2 p{position.x, position.z};
  constexpr aster::Vec2 pond_center{5.58f, 3.52f};
  constexpr aster::Vec2 rod_base{3.12f, 2.60f};
  constexpr aster::Vec2 rod_tip{4.86f, 3.50f};
  const aster::Vec2 rod = rod_tip - rod_base;
  const float rod_len_sq = std::max(aster::dot(rod, rod), 0.0001f);
  const float t = aster::clamp(aster::dot(p - rod_base, rod) / rod_len_sq, 0.0f, 1.0f);
  const aster::Vec2 closest = rod_base + rod * t;
  const float pond_distance =
      std::sqrt((p.x - pond_center.x) * (p.x - pond_center.x) / (4.20f * 4.20f) +
                (p.y - pond_center.y) * (p.y - pond_center.y) / (2.95f * 2.95f));
  return pond_distance < 1.18f || aster::length(p - closest) < 0.72f;
}

bool overlapsParkourChestComposition(const aster::Vec3 position) {
  const aster::Vec3 chest = parkourChestBase();
  const aster::Vec2 delta{position.x - chest.x, position.z - chest.z};
  return aster::length(delta) < kParkourChestPlacementClearance;
}

} // namespace

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
