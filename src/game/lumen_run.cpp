// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/lumen_run.hpp"
#include "aster/core/profiler.hpp"
#include "aster/game/player_motion.hpp"
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

namespace aster {

LumenRun::LumenRun(LumenTuning tuning) : tuning_(tuning) {
  reset();
}

void LumenRun::reset() {
  ASTER_PROFILE_SCOPE("LumenRun::reset");
  status_ = {};
  status_.lives = 3;
  status_.max_health = kPlayerMaxHealth;
  status_.health = status_.max_health;
  status_.total_shards = tuning_.shard_count;
  terrain_ = makeProceduralTerrain({.grid_size = 289,
                                    .square_size = 0.74f,
                                    .central_flat_radius = tuning_.arena_radius * 1.06f,
                                    .transition_width = 10.0f,
                                    .hill_height = 1.18f,
                                    .mountain_height = 3.10f});
  sculptLumenCaveTerrain(terrain_);
  const TerrainSurfaceSample start_ground = sampleTerrain(terrain_, {0.0f, 0.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  prism_relay_base_ = prismRelayBasePlacement();
  prism_relay_active_ = false;
  prism_relay_charge_ = kPrismRelayIdleCharge;
  player_velocity_ = {};
  player_avatar_instance_ = {};
  player_facing_yaw_ = 0.0f;
  player_avatar_pose_ = {};
  player_preview_yaw_ = 0.0f;
  player_preview_yaw_enabled_ = false;
  player_avatar_animator_ = {};
  player_avatar_support_extent_ = 0.0f;
  player_render_position_ = player_position_;
  player_avatar_pose_valid_ = false;
  player_render_position_valid_ = false;
  player_avatar_point_target_ = {};
  player_avatar_point_enabled_ = false;
  player_mouth_open_ = 0.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  invulnerability_ = 0.0f;
  death_state_ = DeathSequenceState::Alive;
  death_timer_ = 0.0f;
  death_origin_ = {};
  pending_defeat_ = false;
  item_registry_ = makeLumenRunItemRegistry();
  chest_inventory_ = InventoryContainer(3u);
  hotbar_ = Hotbar(6u);
  equipment_.clear();
  for (const char *item_id : {"pickaxe", "torch", "fishing_rod"}) {
    if (const ItemDefinition *definition = item_registry_.find(item_id)) {
      (void)chest_inventory_.addItem(*definition, 1);
    }
  }
  interaction_.clear();
  chest_lid_animation_.snap(0.0f);
  chest_open_ = false;
  chest_selected_slot_ = 0u;
  supply_crate_base_ = {};
  supply_crate_yaw_ = 0.0f;
  equipped_light_ = {};
  prism_relay_core_object_ = 0;
  prism_relay_core_valid_ = false;
  prism_relay_ring_objects_.clear();
  prism_relay_conduit_objects_.clear();
  prism_relay_node_objects_.clear();
  torch_flame_.reset();
  crocodile_state_ = {};
  crocodile_state_.position = inner_pond_center_ + Vec3{-2.80f, 0.24f, 0.18f};
  crocodile_state_.facing_yaw = radians(78.0f);
  crocodile_swim_blend_ = 1.0f;

  shards_.clear();
  sentinels_.clear();
  rim_objects_.clear();
  scenery_objects_.clear();
  fishing_line_objects_.clear();
  fish_.clear();
  castle_birds_.clear();
  crocodile_objects_.clear();
  blood_particles_.clear();
  blood_particle_cursor_ = 0;
  chest_items_.clear();
  equipped_item_parts_.clear();
  placed_rocks_.clear();
  scenery_collision_boxes_.clear();
  torch_particle_visuals_.clear();
  mining_fracture_shards_.clear();
  coal_ores_.clear();
  cave_webs_.clear();
  cave_skitters_.clear();
  cave_sections_.clear();
  cave_collision_meshes_.clear();
  cave_viewer_cull_volume_ = {};
  focused_cave_web_index_ = 0;
  focused_cave_web_valid_ = false;
  mining_.reset();
  placed_resource_serial_ = 1u;
  x_eye_objects_.clear();
  eye_objects_valid_ = false;
  const Vec2 climbable_tree_planar{-22.0f, 15.8f};
  const TerrainSurfaceSample climbable_tree_ground = sampleTerrain(terrain_, climbable_tree_planar);
  climbable_tree_ = {{climbable_tree_planar.x,
                      (climbable_tree_ground.valid ? climbable_tree_ground.height : 0.0f) + 0.03f,
                      climbable_tree_planar.y},
                     0.46f,
                     3.70f};
  player_grounded_ = false;

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  const auto shard_overlaps_reserved_composition = [](const Vec3 position) {
    return overlapsFishingComposition(position) || overlapsParkourChestComposition(position);
  };
  const auto relocate_shard = [&](Shard &shard, const int seed) {
    Vec2 planar = normalize(Vec2{shard.position.x, shard.position.z});
    if (length(planar) <= 0.0001f) {
      planar = {1.0f, 0.0f};
    }
    const Vec2 rotated{-planar.y, planar.x};
    for (int attempt = 0; attempt < 8; ++attempt) {
      const float side_bias = 0.54f + static_cast<float>(attempt) * 0.11f;
      const float radius_scale = 0.64f + static_cast<float>((seed + attempt) % 3) * 0.07f;
      const Vec2 relocated =
          normalize(planar * 0.48f + rotated * side_bias) * (tuning_.arena_radius * radius_scale);
      shard.position.x = relocated.x;
      shard.position.z = relocated.y;
      if (!shard_overlaps_reserved_composition(shard.position)) {
        return;
      }
      planar = normalize(Vec2{std::cos(static_cast<float>(seed + attempt + 1) * golden_angle),
                              std::sin(static_cast<float>(seed + attempt + 1) * golden_angle)});
    }
  };
  for (int i = 0; i < tuning_.shard_count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(tuning_.shard_count);
    const float radius = std::sqrt(fill) * tuning_.arena_radius * 0.82f;
    const float angle = static_cast<float>(i) * golden_angle;
    Shard shard;
    shard.position = {std::cos(angle) * radius, tuning_.shard_radius * 1.8f,
                      std::sin(angle) * radius};
    if (shard_overlaps_reserved_composition(shard.position)) {
      relocate_shard(shard, i);
    }
    shards_.push_back(shard);
  }

  for (int i = 0; i < tuning_.sentinel_count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(tuning_.sentinel_count);
    Sentinel sentinel;
    sentinel.phase = fill * kPi * 2.0f;
    sentinel.orbit_radius = tuning_.arena_radius * (0.38f + 0.34f * fill);
    sentinels_.push_back(sentinel);
  }

  rebuildScene();
}

void LumenRun::update(const float dt, Vec2 move_axis, const bool run_requested,
                      const bool jump_requested) {
  ASTER_PROFILE_SCOPE("LumenRun::update");
  if (status_.victory || (status_.defeated && death_state_ == DeathSequenceState::Alive)) {
    return;
  }

  const float step = std::clamp(dt, 0.0f, 0.05f);
  status_.elapsed_seconds += step;
  invulnerability_ = std::max(0.0f, invulnerability_ - step);
  updateBloodParticles(step);
  updateMiningFractureVisuals(step);

  if (death_state_ != DeathSequenceState::Alive) {
    updateDeathSequence(step);
    updateSceneObjects(step);
    return;
  }

  if (length(move_axis) > 1.0f) {
    move_axis = normalize(move_axis);
  }
  if (player_avatar_point_enabled_ &&
      (length(move_axis) > 0.001f || run_requested || jump_requested)) {
    clearAvatarPointTarget();
  }

  updatePlayerPhysics(step, move_axis, run_requested, jump_requested);
  enforceWorldBounds();
  updateChestInteractionState();
  updatePrismRelay(step);
  updateCrocodile(step);
  updateCaveSkitters(step);
  updateCastleBirds(step);

  collectOverlaps();
  resolveSentinelImpacts();
  updateSceneObjects(step);
}

const Scene &LumenRun::scene() const {
  return scene_;
}

const SceneCoherenceReport &LumenRun::sceneCoherenceReport() const {
  if (scene_coherence_dirty_) {
    rebuildSceneCoherenceReport();
  }
  return scene_coherence_;
}

const SceneTraceValidationReport &LumenRun::sceneTraceReport() const {
  if (scene_trace_dirty_) {
    rebuildSceneTraceReport();
  }
  return scene_trace_;
}

const LumenStatus &LumenRun::status() const {
  return status_;
}

Vec3 LumenRun::playerPosition() const {
  return player_position_;
}

Vec3 LumenRun::playerRenderPosition() const {
  return player_render_position_valid_ ? player_render_position_ : player_position_;
}

Vec3 LumenRun::prismRelayBasePosition() const {
  return prism_relay_base_;
}

Vec3 LumenRun::prismRelayFocusPosition() const {
  return prism_relay_base_ + Vec3{0.0f, kPrismRelayCoreHeight, 0.0f};
}

Vec3 LumenRun::caveFrameReportPosition(const float progress_distance) const {
  if (cave_sections_.empty()) {
    return player_position_;
  }
  const AuthoredCaveSection &section =
      cave_sections_.size() > 1u ? cave_sections_[1u] : cave_sections_.front();
  const CaveTunnelFrame frame =
      sampleCaveTunnelFrameAtDistance(section.tunnel, std::max(progress_distance, 0.0f));
  return frame.floor_center + frame.up * playerSupportExtent();
}

void LumenRun::relocatePlayer(Vec3 position, const float facing_yaw) {
  const TerrainSurfaceSample ground =
      sampleWorldSupport({{position.x, position.z}, position.y, 0.64f, 6.0f});
  if (ground.valid) {
    position.y = ground.height + playerSupportExtent();
  }
  player_position_ = position;
  player_velocity_ = {};
  player_facing_yaw_ = facing_yaw;
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  updateSceneObjects(1.0f / 60.0f);
}

void LumenRun::updateRenderInterpolation(const float alpha) {
  if (death_state_ != DeathSequenceState::Alive || player_preview_yaw_enabled_ ||
      !player_avatar_pose_valid_ || !physics_.valid(player_body_) ||
      player_avatar_instance_.object_indices.empty()) {
    player_render_position_ = player_position_;
    player_render_position_valid_ = true;
    return;
  }

  const PhysicsBody &body = physics_.body(player_body_);
  player_render_position_ =
      body.previous_position + (body.position - body.previous_position) * clamp(alpha, 0.0f, 1.0f);
  player_render_position_valid_ = true;

  AvatarPose render_pose = player_avatar_pose_;
  render_pose.position = avatarPosePosition(player_render_position_);
  applyAvatarPose(scene_, player_avatar_, player_avatar_instance_, render_pose);
}

float LumenRun::resolveCameraRadius(const Vec3 target, const float yaw, const float pitch,
                                    const float desired_radius) const {
  if (desired_radius <= 0.0f) {
    return desired_radius;
  }
  CaveInteriorSample cave_target{};
  const AuthoredCaveSection *cave_section = caveSectionAt(target, &cave_target);
  const bool cave_visibility_camera = cave_target.interior > 0.045f;
  const float minimum_radius =
      cave_visibility_camera
          ? std::max(0.82f, std::max(player_avatar_support_extent_, 0.0f) * 1.05f)
          : std::max(1.85f, std::max(player_avatar_support_extent_, 0.0f) * 1.80f);
  constexpr float collision_minimum_radius = 0.55f;
  constexpr float cast_radius = 0.18f;
  constexpr float hit_clearance = 0.32f;
  constexpr float surface_clearance = 0.42f;
  constexpr float search_step = 0.32f;
  const float cp = std::cos(pitch);
  const auto offset_for_radius = [&](const float radius) {
    return Vec3{radius * cp * std::sin(yaw), radius * std::sin(pitch), radius * cp * std::cos(yaw)};
  };
  const Vec3 desired_offset = offset_for_radius(desired_radius);
  PhysicsShapeCastHit hit;
  PhysicsSphereCast cast;
  cast.origin = target;
  cast.displacement = desired_offset;
  cast.radius = cast_radius;
  cast.filter.collides_with = kPhysicsLayerWorld;
  cast.filter.ignore_body = player_body_;
  float resolved_radius = desired_radius;
  float lower_bound = minimum_radius;
  if (physics_.castSphere(cast, hit)) {
    lower_bound = cave_visibility_camera ? minimum_radius : collision_minimum_radius;
    resolved_radius = std::clamp(hit.distance - hit_clearance, lower_bound, desired_radius);
  }
  if (cave_section != nullptr && cave_visibility_camera) {
    const CaveViewConstraint view_constraint =
        constrainCaveViewSegment(cave_section->tunnel, target,
                                 target + offset_for_radius(resolved_radius),
                                 {.samples = 36,
                                  .minimum_radius = minimum_radius,
                                  .interior_threshold = 0.045f,
                                  .backtrack_tolerance_t = 0.18f});
    if (view_constraint.active) {
      resolved_radius = std::clamp(view_constraint.radius, minimum_radius, resolved_radius);
    }
  }

  const auto camera_has_support_obstruction = [&](const float radius) {
    const Vec3 camera_position = target + offset_for_radius(radius);
    const TerrainSurfaceSample sample =
        support_surfaces_.sample({{camera_position.x, camera_position.z},
                                  camera_position.y,
                                  std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::infinity()});
    return sample.valid && camera_position.y < sample.height + surface_clearance;
  };

  if (!camera_has_support_obstruction(resolved_radius)) {
    return resolved_radius;
  }
  for (float candidate = resolved_radius - search_step; candidate >= lower_bound;
       candidate -= search_step) {
    if (!camera_has_support_obstruction(candidate)) {
      return candidate;
    }
  }
  return lower_bound;
}

void LumenRun::setAvatarPreviewYaw(const float yaw) {
  player_preview_yaw_ = yaw;
  player_preview_yaw_enabled_ = true;
  if (!scene_.objects().empty() && !player_avatar_instance_.object_indices.empty()) {
    updateSceneObjects(1.0f / 60.0f);
  }
}

void LumenRun::clearAvatarPreviewYaw() {
  player_preview_yaw_enabled_ = false;
}

void LumenRun::setAvatarPointTarget(const Vec3 target) {
  player_avatar_point_target_ = target;
  player_avatar_point_enabled_ = true;
}

bool LumenRun::pointAvatarAtRay(const Vec3 origin, const Vec3 direction, const float max_distance) {
  const Vec3 ray_direction = normalize(direction);
  if (length(ray_direction) <= 0.0001f || max_distance <= 0.0f) {
    return false;
  }

  bool found = false;
  float best_distance = max_distance;
  Vec3 best_target{};

  constexpr int support_steps = 128;
  const float step_size = max_distance / static_cast<float>(support_steps);
  bool previous_valid = false;
  float previous_signed_height = 0.0f;
  float previous_t = 0.0f;
  for (int step = 1; step <= support_steps; ++step) {
    const float t = step_size * static_cast<float>(step);
    const Vec3 probe = origin + ray_direction * t;
    const TerrainSurfaceSample sample = support_surfaces_.sample(Vec2{probe.x, probe.z});
    if (!sample.valid) {
      previous_valid = false;
      continue;
    }
    const float signed_height = probe.y - sample.height;
    if (previous_valid && previous_signed_height >= 0.0f && signed_height <= 0.0f) {
      float lo = previous_t;
      float hi = t;
      TerrainSurfaceSample hit_sample = sample;
      for (int refine = 0; refine < 8; ++refine) {
        const float mid = (lo + hi) * 0.5f;
        const Vec3 mid_probe = origin + ray_direction * mid;
        const TerrainSurfaceSample mid_sample =
            support_surfaces_.sample(Vec2{mid_probe.x, mid_probe.z});
        if (!mid_sample.valid) {
          lo = mid;
          continue;
        }
        const float mid_signed_height = mid_probe.y - mid_sample.height;
        if (mid_signed_height > 0.0f) {
          lo = mid;
        } else {
          hi = mid;
          hit_sample = mid_sample;
        }
      }
      const Vec3 hit_probe = origin + ray_direction * hi;
      best_distance = hi;
      best_target = {hit_probe.x, hit_sample.height, hit_probe.z};
      found = true;
      break;
    }
    previous_valid = true;
    previous_signed_height = signed_height;
    previous_t = t;
  }

  PhysicsRayHit physics_hit;
  PhysicsRay ray;
  ray.origin = origin;
  ray.direction = ray_direction;
  ray.max_distance = max_distance;
  ray.filter.collides_with = kPhysicsLayerWorld;
  ray.filter.ignore_body = player_body_;
  if (physics_.raycast(ray, physics_hit) &&
      (!found || physics_hit.distance < best_distance - 0.02f)) {
    best_target = physics_hit.point;
    found = true;
  }

  if (!found) {
    return false;
  }
  setAvatarPointTarget(best_target);
  return true;
}

void LumenRun::clearAvatarPointTarget() {
  player_avatar_point_enabled_ = false;
}

void LumenRun::updateInteractionFocus(const Vec3 ray_origin, const Vec3 ray_direction,
                                      const float dt) {
  std::vector<InteractionTarget> targets;
  targets.reserve(3u + coal_ores_.size() + cave_webs_.size() + cave_skitters_.size());
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  const bool player_near_chest = length(player_position_ - chest_focus) < kChestInteractionDistance;
  std::string action = "Open";
  std::string subject = "Chest";
  if (chest_open_) {
    action = "Take";
    subject = "Items";
    if (const ItemStack *stack = chest_inventory_.slot(chest_selected_slot_);
        stack != nullptr && !stack->empty()) {
      if (const ItemDefinition *definition = item_registry_.find(stack->item_id)) {
        subject = definition->display_name;
      }
    }
  }
  targets.push_back({.id = chest_open_ ? "chest:contents" : "chest:parkour",
                     .kind = InteractionTargetKind::Container,
                     .action_label = action,
                     .subject_label = subject,
                     .position = chest_focus,
                     .radius = 0.82f,
                     .max_distance = 14.0f,
                     .proximity_distance = kChestInteractionDistance,
                     .enabled = player_near_chest});

  const Vec3 relay_focus = prismRelayFocusPosition();
  const bool player_near_relay =
      length(player_position_ - relay_focus) <= kPrismRelayInteractionDistance;
  targets.push_back({.id = "relay:prism",
                     .kind = InteractionTargetKind::Item,
                     .action_label = prism_relay_active_ ? "Tune" : "Ignite",
                     .subject_label = "Prism Relay",
                     .position = relay_focus,
                     .radius = 0.92f,
                     .max_distance = 14.0f,
                     .proximity_distance = kPrismRelayInteractionDistance,
                     .enabled = player_near_relay});

  focused_cave_web_index_ = 0;
  focused_cave_web_valid_ = false;
  const float ray_length = length(ray_direction);
  const Vec3 ray_direction_unit = ray_length > 0.0001f ? ray_direction / ray_length : Vec3{};
  const auto webRayHit = [&](const CaveWebObstacle &web) -> std::optional<std::pair<float, Vec3>> {
    if (web.broken || ray_length <= 0.0001f) {
      return std::nullopt;
    }
    const float denom = dot(ray_direction_unit, web.normal);
    if (std::abs(denom) <= 0.0001f) {
      return std::nullopt;
    }
    const float t = dot(web.center - ray_origin, web.normal) / denom;
    if (t < 0.0f || t > kCaveWebInteractionDistance + 1.0f) {
      return std::nullopt;
    }
    const Vec3 hit = ray_origin + ray_direction_unit * t;
    const Vec3 offset = hit - web.center;
    const float x = dot(offset, web.side) / std::max(web.radius_x, 0.001f);
    const float y = dot(offset, web.up) / std::max(web.radius_y, 0.001f);
    if (x * x + y * y > 1.0f) {
      return std::nullopt;
    }
    return std::make_pair(t, hit);
  };
  const auto rayOccludedByWeb = [&](const Vec3 target, const float target_distance) {
    if (ray_length <= 0.0001f) {
      return false;
    }
    for (const CaveWebObstacle &web : cave_webs_) {
      if (web.broken) {
        continue;
      }
      const std::optional<std::pair<float, Vec3>> hit = webRayHit(web);
      if (!hit.has_value() || hit->first <= 0.02f) {
        continue;
      }
      const Vec3 to_target = target - ray_origin;
      if (dot(normalize(to_target), ray_direction_unit) <= 0.92f) {
        continue;
      }

      const Vec3 web_offset = target - web.center;
      const float web_plane = dot(web_offset, web.normal);
      const float web_x = dot(web_offset, web.side) / std::max(web.radius_x, 0.001f);
      const float web_y = dot(web_offset, web.up) / std::max(web.radius_y, 0.001f);
      const bool inside_web_span = web_x * web_x + web_y * web_y <= 1.08f;
      const float attached_depth = std::max(web.thickness * 1.8f, 0.20f);
      const bool attached_to_web =
          inside_web_span && std::abs(web_plane) <= attached_depth &&
          hit->first <= target_distance + attached_depth;
      const bool behind_web = hit->first < std::max(target_distance - 0.02f, 0.0f);
      if (behind_web || attached_to_web) {
        return true;
      }
    }
    return false;
  };
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  const bool pickaxe_equipped =
      equipped_definition != nullptr && equipped_definition->has_mining_tool;
  for (std::size_t i = 0; i < coal_ores_.size(); ++i) {
    const CoalOreNode &ore = coal_ores_[i];
    if (ore.collected) {
      continue;
    }
    const float distance = length(player_position_ - ore.position);
    targets.push_back({.id = "ore:" + std::to_string(i),
                       .kind = InteractionTargetKind::Item,
                       .action_label = pickaxe_equipped ? "Mine" : "Need",
                       .subject_label = pickaxe_equipped ? "Coal Ore" : "Pickaxe",
                       .position = ore.position,
                       .radius = std::max(ore.radius, 0.28f),
                       .max_distance = 14.0f,
                       .enabled = distance <= kOreInteractionDistance});
  }
  float nearest_web_distance = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < cave_webs_.size(); ++i) {
    const CaveWebObstacle &web = cave_webs_[i];
    if (web.broken) {
      continue;
    }
    const float distance = length(player_position_ - web.center);
    if (distance < nearest_web_distance && distance <= kCaveWebInteractionDistance) {
      nearest_web_distance = distance;
      focused_cave_web_index_ = i;
      focused_cave_web_valid_ = true;
    }
    const std::optional<std::pair<float, Vec3>> web_hit = webRayHit(web);
    targets.push_back({.id = "web:" + std::to_string(i),
                       .kind = InteractionTargetKind::Item,
                       .shape = web_hit.has_value() ? InteractionTargetShape::ExplicitHit
                                                     : InteractionTargetShape::Sphere,
                       .action_label = pickaxe_equipped ? "Cut" : "Need",
                       .subject_label = pickaxe_equipped ? "Spider Web" : "Pickaxe",
                       .position = web_hit.has_value() ? web_hit->second : web.center,
                       .radius = std::max(std::max(web.radius_x, web.radius_y) * 0.56f, 0.42f),
                       .max_distance = 14.0f,
                       .proximity_distance = kCaveWebInteractionDistance,
                       .hit_distance = web_hit.has_value() ? web_hit->first : 0.0f,
                       .evidence_strength = web_hit.has_value() ? 2.0f : 1.0f,
                       .enabled = distance <= kCaveWebInteractionDistance});
  }
  for (std::size_t i = 0; i < cave_skitters_.size(); ++i) {
    const CaveSkitter &skitter = cave_skitters_[i];
    if (skitter.dead || skitter.state.dead) {
      continue;
    }
    const float distance = length(player_position_ - skitter.state.position);
    const Vec3 skitter_focus = skitter.state.position + Vec3{0.0f, 0.10f, 0.0f};
    const float ray_distance = length(skitter_focus - ray_origin);
    targets.push_back({.id = "skitter:" + std::to_string(i),
                       .kind = InteractionTargetKind::Item,
                       .action_label = pickaxe_equipped ? "Strike" : "Need",
                       .subject_label = pickaxe_equipped ? "Cave Skitter" : "Pickaxe",
                       .position = skitter_focus,
                       .radius = 0.48f,
                       .max_distance = 14.0f,
                       .proximity_distance = kCaveSkitterInteractionDistance,
                       .occluded = rayOccludedByWeb(skitter_focus, ray_distance),
                       .enabled = distance <= kCaveSkitterInteractionDistance});
  }

  interaction_.update(targets, ray_origin, ray_direction, player_position_, dt);
}

void LumenRun::interactFocused() {
  const InteractionFocus &focus = interaction_.focus();
  if (!focus.visible) {
    return;
  }

  if (focus.kind == InteractionTargetKind::Container && focus.target_id == "chest:parkour") {
    openChest();
    setAvatarPointTarget(chest_base_ + Vec3{0.0f, 0.42f, 0.0f});
    return;
  }
  if (focus.kind == InteractionTargetKind::Container && focus.target_id == "chest:contents") {
    if (takeChestSlot(chest_selected_slot_)) {
      setAvatarPointTarget(focus.position);
    }
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.target_id == "relay:prism") {
    activatePrismRelay();
    setAvatarPointTarget(prismRelayFocusPosition());
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.target_id.rfind("ore:", 0u) == 0u) {
    (void)mineFocusedOre(focus.target_id);
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.target_id.rfind("web:", 0u) == 0u) {
    (void)mineFocusedCaveWeb(focus.target_id);
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.target_id.rfind("skitter:", 0u) == 0u) {
    (void)mineFocusedCaveSkitter(focus.target_id);
  }
}

void LumenRun::secondaryInteractFocused(const Vec3 ray_origin, const Vec3 ray_direction) {
  const InteractionFocus &focus = interaction_.focus();
  if (focus.visible && focus.kind == InteractionTargetKind::Container &&
      focus.target_id == "chest:parkour") {
    interactFocused();
    return;
  }
  if (placeEquippedResource(ray_origin, ray_direction)) {
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.target_id.rfind("ore:", 0u) == 0u) {
    (void)mineFocusedOre(focus.target_id);
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.target_id.rfind("web:", 0u) == 0u) {
    (void)mineFocusedCaveWeb(focus.target_id);
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.target_id.rfind("skitter:", 0u) == 0u) {
    (void)mineFocusedCaveSkitter(focus.target_id);
  }
}

void LumenRun::openChest() {
  chest_open_ = true;
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  chest_distance_close_armed_ = length(player_position_ - chest_focus) <= kChestAutoCloseDistance;
  chest_lid_animation_.setTarget(1.0f);
  for (std::size_t i = 0; i < chest_inventory_.slotCount(); ++i) {
    const ItemStack *stack = chest_inventory_.slot(i);
    if (stack != nullptr && !stack->empty()) {
      chest_selected_slot_ = i;
      break;
    }
  }
}

void LumenRun::closeChest() {
  chest_open_ = false;
  chest_distance_close_armed_ = false;
  chest_lid_animation_.setTarget(0.0f);
}

bool LumenRun::takeChestItem(const std::string_view item_id) {
  const ItemDefinition *definition = item_registry_.find(item_id);
  if (definition == nullptr) {
    return false;
  }
  const std::optional<std::size_t> slot = hotbar_.addItem(*definition, 1);
  if (!slot.has_value()) {
    return false;
  }
  (void)chest_inventory_.removeItem(item_id, 1);
  for (ChestItemVisual &item : chest_items_) {
    if (item.item_id == item_id && item.available) {
      item.available = false;
      break;
    }
  }
  if (hotbar_.selectedStack() == nullptr || hotbar_.selectedStack()->empty()) {
    (void)hotbar_.select(*slot);
  }
  equipment_.equipFromHotbar(hotbar_);
  for (std::size_t i = 0; i < chest_inventory_.slotCount(); ++i) {
    const ItemStack *stack = chest_inventory_.slot(i);
    if (stack != nullptr && !stack->empty()) {
      chest_selected_slot_ = i;
      return true;
    }
  }
  return true;
}

bool LumenRun::takeChestSlot(const std::size_t slot_index) {
  const ItemStack *stack = chest_inventory_.slot(slot_index);
  if (stack == nullptr || stack->empty()) {
    return false;
  }
  chest_selected_slot_ = slot_index;
  return takeChestItem(stack->item_id);
}

bool LumenRun::takeSupplyTorch() {
  if (!supplyCrateNearby()) {
    return false;
  }
  const ItemDefinition *definition = item_registry_.find("torch");
  if (definition == nullptr) {
    return false;
  }
  const std::optional<std::size_t> slot = hotbar_.addItem(*definition, 1);
  if (!slot.has_value()) {
    return false;
  }
  if (equipment_.isEquipped("torch")) {
    equipment_.equipFromHotbar(hotbar_);
  }
  return true;
}

void LumenRun::selectHotbarSlot(const std::size_t index) {
  if (hotbar_.select(index)) {
    equipment_.equipFromHotbar(hotbar_);
  }
}

bool LumenRun::chestInterfaceOpen() const {
  return chest_open_ && chest_lid_animation_.value() > 0.35f;
}

bool LumenRun::supplyCrateNearby() const {
  const Vec3 supply_focus = supply_crate_base_ + Vec3{0.0f, 0.42f, 0.0f};
  return length(player_position_ - supply_focus) <= kSupplyCrateInteractionDistance;
}

int LumenRun::torchCount() const {
  int count = 0;
  for (const ItemStack &stack : hotbar_.slots()) {
    if (stack.item_id == "torch") {
      count += std::max(stack.quantity, 0);
    }
  }
  return count;
}

Vec3 LumenRun::supplyCratePosition() const {
  return supply_crate_base_;
}

FocusPromptModel LumenRun::focusPromptModel() const {
  const InteractionFocus &focus = interaction_.focus();
  return {.visible = focus.visible,
          .alpha = focus.strength,
          .key = "E",
          .action = focus.action_label,
          .subject = focus.subject_label};
}

HotbarHudModel LumenRun::hotbarHudModel() const {
  HotbarHudModel model;
  model.visible = true;
  const std::vector<ItemStack> &slots = hotbar_.slots();
  model.slots.reserve(slots.size());
  for (std::size_t i = 0; i < slots.size(); ++i) {
    HotbarSlotModel slot_model;
    slot_model.key = std::to_string(i + 1u);
    slot_model.selected = i == hotbar_.selectedIndex();
    if (const ItemDefinition *definition = item_registry_.find(slots[i].item_id)) {
      slot_model.filled = !slots[i].empty();
      slot_model.label = definition->short_label;
      slot_model.tint = definition->tint;
      if (slots[i].quantity > 1) {
        slot_model.quantity = std::to_string(slots[i].quantity);
      }
    }
    model.slots.push_back(std::move(slot_model));
  }
  return model;
}

ChestContentsHudModel LumenRun::chestContentsHudModel() const {
  ChestContentsHudModel model;
  model.visible = chestInterfaceOpen();
  model.title = "Chest";
  const std::vector<ItemStack> &slots = chest_inventory_.slots();
  model.slots.reserve(slots.size());
  for (std::size_t i = 0; i < slots.size(); ++i) {
    ChestContentsSlotModel slot_model;
    slot_model.selected = i == chest_selected_slot_;
    if (const ItemDefinition *definition = item_registry_.find(slots[i].item_id)) {
      slot_model.filled = !slots[i].empty();
      slot_model.label = definition->short_label;
      slot_model.tint = definition->tint;
      if (slots[i].quantity > 1) {
        slot_model.quantity = std::to_string(slots[i].quantity);
      }
    }
    model.slots.push_back(std::move(slot_model));
  }
  return model;
}

std::optional<DynamicPointLight> LumenRun::equippedLight() const {
  return equipped_light_.active ? std::optional<DynamicPointLight>{equipped_light_} : std::nullopt;
}

std::optional<DynamicPointLight> LumenRun::pondAccentLight() const {
  if (!pond_accent_light_valid_) {
    return std::nullopt;
  }
  return evaluateFlickerLight({.color = {1.0f, 0.48f, 0.20f},
                               .intensity = 1.18f,
                               .amplitude = 0.035f,
                               .speed = 3.2f,
                               .source_radius = 1.65f},
                              pond_accent_light_position_, status_.elapsed_seconds);
}

std::optional<DynamicPointLight> LumenRun::prismRelayLight() const {
  if (prism_relay_charge_ <= 0.05f) {
    return std::nullopt;
  }
  return evaluateFlickerLight({.color = {0.46f, 0.86f, 1.0f},
                              .intensity = 2.85f * clamp(prism_relay_charge_, 0.0f, 1.15f),
                               .amplitude = 0.045f,
                               .speed = 2.4f,
                               .source_radius = 4.8f},
                              prismRelayFocusPosition(), status_.elapsed_seconds);
}

CaveLightingState LumenRun::caveLightingState() const {
  return caveLightingStateAt(player_position_);
}

const LumenRun::AuthoredCaveSection *
LumenRun::caveSectionAt(const Vec3 position, CaveInteriorSample *sample) const {
  const AuthoredCaveSection *best_section = nullptr;
  CaveInteriorSample best_sample{};
  for (const AuthoredCaveSection &section : cave_sections_) {
    const CaveInteriorSample candidate = sampleCaveInteriorVolume(section.tunnel, position);
    if (best_section == nullptr || candidate.interior > best_sample.interior) {
      best_section = &section;
      best_sample = candidate;
    }
  }
  if (sample != nullptr) {
    *sample = best_sample;
  }
  return best_section;
}

CaveLightingState LumenRun::caveLightingStateAt(const Vec3 position) const {
  if (cave_sections_.empty()) {
    return {};
  }
  CaveInteriorSample best_sample{};
  (void)caveSectionAt(position, &best_sample);

  struct LightCandidate {
    CaveWallLightSample sample{};
    float distance = 0.0f;
    float score = 0.0f;
  };
  std::vector<LightCandidate> candidates;
  candidates.reserve(24u);

  const auto append_authored_fixtures = [&](const CaveInteriorSample &section_sample,
                                            const std::vector<CaveWallFixturePlacement> &fixtures,
                                            const float path_length) {
    constexpr float kProgressWindow = 0.18f;
    constexpr float kMinimumProgressGain = 0.62f;
    for (const CaveWallFixturePlacement &fixture : fixtures) {
      const float distance = length(fixture.light_position - position);
      const float progress_delta = std::abs(fixture.t - section_sample.tunnel_t);
      const float progress_gain = 1.0f - (1.0f - kMinimumProgressGain) *
                                             clamp(progress_delta / kProgressWindow, 0.0f, 1.0f);
      const float weight = clamp(section_sample.interior * progress_gain, 0.0f, 1.0f);
      candidates.push_back(
          {.sample = {.position = fixture.light_position,
                      .color = fixture.light_color,
                      .intensity = kAuthoredCaveFixtureIntensity * weight,
                      .source_radius = kAuthoredCaveFixtureSourceRadius,
                      .tunnel_t = fixture.t},
           .distance = distance,
           .score = distance * 0.55f + progress_delta * std::max(path_length, 1.0f) * 0.10f});
    }
  };
  float entrance_light = 0.0f;
  for (const AuthoredCaveSection &section : cave_sections_) {
    const CaveInteriorSample section_sample = sampleCaveInteriorVolume(section.tunnel, position);
    const float path_length = estimateCaveTunnelLength(section.tunnel);
    if (section.contributes_entrance_light) {
      entrance_light = std::max(entrance_light, section_sample.entrance_light);
    }
    append_authored_fixtures(section_sample, section.wall_fixtures, path_length);
    append_authored_fixtures(section_sample, section.secondary_wall_fixtures, path_length);
  }

  std::sort(
      candidates.begin(), candidates.end(),
      [](const LightCandidate &lhs, const LightCandidate &rhs) { return lhs.score < rhs.score; });

  Vec3 wall_light_position = cave_entrance_light_position_;
  Vec3 wall_light_color = kCaveIndustrialWarmLight;
  std::vector<CaveWallLightSample> wall_lights;
  wall_lights.reserve(candidates.size());
  for (const LightCandidate &candidate : candidates) {
    if (wall_lights.empty()) {
      wall_light_position = candidate.sample.position;
      wall_light_color = candidate.sample.color;
    }
    wall_lights.push_back(candidate.sample);
  }

  const float interior = best_sample.interior;
  float wall_light = entrance_light * 0.20f;
  for (const LightCandidate &candidate : candidates) {
    const float distance_sq =
        std::max(dot(candidate.sample.position - position, candidate.sample.position - position),
                 0.0001f);
    const float softened =
        std::max(distance_sq, candidate.sample.source_radius * candidate.sample.source_radius);
    wall_light = std::max(wall_light, candidate.sample.intensity / softened * 0.035f);
  }
  wall_light = clamp(wall_light, 0.0f, 1.0f);
  return {.interior = interior,
          .entrance_light = entrance_light,
          .depth = best_sample.depth * best_sample.interior,
          .chamber = best_sample.chamber * best_sample.interior,
          .entrance_light_position = cave_entrance_light_position_,
          .wall_light = wall_light,
          .wall_light_position = wall_light_position,
          .wall_light_color = wall_light_color,
          .wall_lights = std::move(wall_lights)};
}

std::size_t LumenRun::appendObject(RenderObject object) {
  scene_.objects().push_back(std::move(object));
  return scene_.objects().size() - 1;
}

void LumenRun::rebuildScene() {
  ASTER_PROFILE_SCOPE("LumenRun::rebuildScene");
  scene_.objects().clear();
  pond_accent_light_valid_ = false;
  cave_sections_.clear();
  cave_entrance_light_position_ = {};
  cave_collision_meshes_.clear();
  cave_webs_.clear();
  scenery_collision_boxes_.clear();
  mining_fracture_shards_.clear();
  prism_relay_core_object_ = 0;
  prism_relay_core_valid_ = false;
  prism_relay_ring_objects_.clear();
  prism_relay_conduit_objects_.clear();
  prism_relay_node_objects_.clear();
  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  const TerrainMeshClipEllipse bird_grove_clip = birdGroveClip(castle_bird_nest_position_);
  const std::vector<TerrainMeshClipEllipse> terrain_holes =
      terrainSurfaceHoles(pond_center_, inner_pond_center_, inner_pond_radius_, bird_grove_clip);
  const std::vector<TerrainMeshClipOrientedEllipse> terrain_portal_clips = {
      cavePortalTerrainClip()};
  const std::vector<TerrainMeshClipEllipse> wetland_clips =
      plazaWetlandClips(pond_center_, inner_pond_center_, inner_pond_radius_);
  const std::vector<TerrainMeshClipEllipse> stone_floor_holes =
      hardscapeOcclusionClips(pond_center_, inner_pond_center_, inner_pond_radius_);
  std::vector<PlacementFootprint> stone_floor_footprints;
  stone_floor_footprints.reserve(course.ground_zones.size());
  for (const CastleCourseGroundZone &zone : course.ground_zones) {
    stone_floor_footprints.push_back(translatedGroundZoneFootprint(zone, castle_origin));
  }
  const PlacementValidator terrain_placement =
      terrainPlacementValidator(stone_floor_footprints, terrain_holes, terrain_portal_clips);
  support_surfaces_.clear();
  support_surfaces_.setTerrain(&terrain_);
  support_surfaces_.setTerrainPlacementValidator(terrain_placement);
  SupportSurfaceSet decorative_ground_surfaces;
  decorative_ground_surfaces.setTerrain(&terrain_);
  decorative_ground_surfaces.setTerrainPlacementValidator(terrain_placement);

  const Material stone_plaza = makeSupportSurfaceMaterial(
      material({0.40f, 0.39f, 0.34f}, {0.0f, 0.0f, 0.0f}, 0.9f, 0.0f, 0.0f, 0.92f, 4.8f, 0.28f,
               0.74f, SurfacePattern::CourseCells, {1.45f, 1.45f}, 0.050f, 0.56f, 0.050f));
  const Material brick_facade =
      material({0.50f, 0.34f, 0.25f}, {0.0f, 0.0f, 0.0f}, 0.91f, 0.0f, 0.0f, 0.92f, 7.2f, 0.46f,
               0.58f, SurfacePattern::WeatheredStone, {5.4f, 8.2f}, 0.100f, 0.86f, 0.055f);
  const Material dark_masonry =
      material({0.31f, 0.325f, 0.305f}, {0.0f, 0.0f, 0.0f}, 0.94f, 0.0f, 0.0f, 0.76f, 6.2f, 0.40f,
               0.62f, SurfacePattern::WeatheredStone, {4.6f, 6.6f}, 0.070f, 0.74f, 0.070f);
  const Material warm_window = material({0.74f, 0.58f, 0.36f}, {1.0f, 0.66f, 0.32f}, 0.5f, 0.0f,
                                        0.18f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material weathered_iron = material({0.24f, 0.23f, 0.22f}, {0.0f, 0.0f, 0.0f}, 0.56f, 0.62f,
                                           0.0f, 0.34f, 8.6f, 0.18f, 0.88f);
  const Material amber_lamp = material({0.92f, 0.68f, 0.38f}, {1.0f, 0.68f, 0.30f}, 0.16f, 0.02f,
                                       0.44f, 0.08f, 5.0f, 0.04f, 1.0f);
  const Material amber_shard =
      material({0.98f, 0.55f, 0.18f}, {1.0f, 0.46f, 0.12f}, 0.13f, 0.0f, 0.36f, 0.24f, 8.2f, 0.05f,
               0.96f, SurfacePattern::AmberResin, {6.8f, 10.5f}, 0.035f, 0.92f, 0.04f);
  const Material sentinel_material =
      material({0.13f, 0.145f, 0.13f}, {0.82f, 0.24f, 0.08f}, 0.52f, 0.36f, 0.18f, 0.46f, 10.0f,
               0.24f, 0.84f, SurfacePattern::FiberStrands, {4.0f, 9.0f}, 0.018f, 0.42f, 0.05f);
  Material grass_soil = makeSupportSurfaceMaterial(
      material({0.30f, 0.44f, 0.20f}, {0.0f, 0.0f, 0.0f}, 0.90f, 0.0f, 0.0f, 0.92f, 12.0f, 0.16f,
               0.96f, SurfacePattern::GrassSoil, {6.2f, 7.4f}, 0.095f, 0.88f, 0.08f));
  grass_soil.procedural = {.macro_variation = 0.62f,
                           .micro_normal_strength = 0.36f,
                           .roughness_variation = 0.18f,
                           .height_shading = 0.18f};
  Material terrain_transition = makeSupportSurfaceMaterial(
      material({0.42f, 0.39f, 0.30f}, {0.0f, 0.0f, 0.0f}, 0.94f, 0.0f, 0.0f, 0.72f, 8.6f, 0.08f,
               0.94f, SurfacePattern::TerrainBlend, {1.65f, 1.65f}, 0.030f, 0.66f, 0.08f));
  Material hardscape_dust = makeSupportSurfaceMaterial(
      material({0.46f, 0.45f, 0.36f}, {0.0f, 0.0f, 0.0f}, 0.96f, 0.0f, 0.0f, 0.56f, 8.4f, 0.06f,
               0.96f, SurfacePattern::TerrainBlend, {3.40f, 4.10f}, 0.024f, 0.62f, 0.08f));
  Material hardscape_substrate = hardscape_dust;
  hardscape_substrate.base_color = {0.49f, 0.51f, 0.40f};
  hardscape_substrate.opacity = 1.0f;
  hardscape_substrate.pattern_contrast = 0.30f;
  Material grass_blades =
      material({0.19f, 0.39f, 0.13f}, {0.0f, 0.0f, 0.0f}, 0.92f, 0.0f, 0.0f, 0.52f, 9.8f, 0.05f,
               0.95f, SurfacePattern::Foliage, {8.8f, 12.6f}, 0.026f, 0.62f, 0.08f);
  grass_blades.double_sided = true;
  grass_blades.alpha_mode = MaterialAlphaMode::Masked;
  grass_blades.camera_occlusion = CameraOcclusionPolicy::Solid;
  grass_blades.procedural = {.macro_variation = 0.42f,
                             .micro_normal_strength = 0.28f,
                             .roughness_variation = 0.12f,
                             .height_shading = 0.14f};
  Material exotic_leaf =
      material({0.15f, 0.50f, 0.28f}, {0.015f, 0.040f, 0.016f}, 0.68f, 0.0f, 0.025f, 0.36f, 8.8f,
               0.02f, 0.96f, SurfacePattern::Foliage, {8.6f, 12.2f}, 0.026f, 0.56f, 0.06f);
  exotic_leaf.double_sided = true;
  exotic_leaf.alpha_mode = MaterialAlphaMode::Masked;
  exotic_leaf.camera_occlusion = CameraOcclusionPolicy::Solid;
  exotic_leaf.procedural = {.macro_variation = 0.26f,
                            .micro_normal_strength = 0.20f,
                            .roughness_variation = 0.10f,
                            .height_shading = 0.08f};
  const Material exotic_flower =
      material({0.72f, 0.26f, 0.52f}, {0.16f, 0.035f, 0.10f}, 0.58f, 0.0f, 0.08f, 0.18f, 6.2f,
               0.02f, 0.92f, SurfacePattern::FiberStrands, {5.0f, 7.2f}, 0.012f, 0.36f, 0.05f);
  const Material fish_scale_a = material(
      {0.14f, 0.54f, 0.60f}, {0.015f, 0.055f, 0.055f}, 0.42f, 0.0f, 0.020f, 0.20f, 9.2f, 0.03f,
      0.94f, SurfacePattern::IridescentScales, {13.5f, 17.0f}, 0.014f, 0.30f, 0.04f);
  const Material fish_scale_b = material(
      {0.78f, 0.36f, 0.16f}, {0.06f, 0.025f, 0.010f}, 0.44f, 0.0f, 0.018f, 0.18f, 8.0f, 0.03f,
      0.94f, SurfacePattern::IridescentScales, {12.0f, 15.5f}, 0.013f, 0.28f, 0.04f);
  Material pond_water =
      material({0.055f, 0.54f, 0.68f}, {0.025f, 0.12f, 0.13f}, 0.10f, 0.0f, 0.050f, 0.34f, 7.0f,
               0.0f, 0.98f, SurfacePattern::WaterSurface, {5.8f, 6.6f}, 0.045f, 0.86f, 0.04f);
  pond_water.opacity = 0.78f;
  pond_water.double_sided = true;
  pond_water.alpha_mode = MaterialAlphaMode::Blend;
  pond_water.depth_write = MaterialDepthWrite::Disabled;
  pond_water.camera_occlusion = CameraOcclusionPolicy::Solid;
  pond_water.procedural = {.macro_variation = 0.20f,
                           .micro_normal_strength = 0.0f,
                           .roughness_variation = 0.16f,
                           .wetness = 0.42f};
  Material pond_stone =
      material({0.54f, 0.49f, 0.40f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.58f, 6.4f, 0.16f,
               0.92f, SurfacePattern::WeatheredStone, {3.4f, 4.8f}, 0.040f, 0.54f, 0.07f);
  pond_stone.procedural = {.macro_variation = 0.22f,
                           .micro_normal_strength = 0.22f,
                           .roughness_variation = 0.12f,
                           .wetness = 0.06f,
                           .height_shading = 0.10f};
  Material soil_path = makeSupportSurfaceMaterial(
      material({0.60f, 0.43f, 0.24f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.0f, 0.0f, 0.86f, 7.4f, 0.10f,
               0.95f, SurfacePattern::SoilPath, {6.4f, 9.0f}, 0.040f, 0.88f, 0.05f));
  soil_path.procedural = {.macro_variation = 0.36f,
                          .micro_normal_strength = 0.24f,
                          .roughness_variation = 0.14f,
                          .height_shading = 0.12f};
  const Material fishing_line_material = material({0.50f, 0.52f, 0.46f}, {0.02f, 0.02f, 0.015f},
                                                  0.56f, 0.0f, 0.02f, 0.08f, 10.0f, 0.02f, 0.96f);
  const Material fishing_rod_material = material({0.16f, 0.105f, 0.055f}, {0.0f, 0.0f, 0.0f}, 0.54f,
                                                 0.04f, 0.0f, 0.30f, 9.0f, 0.12f, 0.92f);
  const Material tree_bark =
      material({0.30f, 0.18f, 0.095f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.02f, 0.0f, 0.66f, 10.5f, 0.18f,
               0.88f, SurfacePattern::PaintedWood, {3.2f, 12.0f}, 0.042f, 0.72f, 0.06f);
  Material layered_terrain = makeSupportSurfaceMaterial(
      material({0.185f, 0.268f, 0.160f}, {0.0f, 0.0f, 0.0f}, 0.95f, 0.0f, 0.0f, 0.84f, 4.8f, 0.09f,
               0.91f, SurfacePattern::LayeredTerrain, {0.54f, 0.62f}, 0.048f, 1.0f, 0.08f));
  layered_terrain.procedural = {.macro_variation = 0.60f,
                                .micro_normal_strength = 0.36f,
                                .roughness_variation = 0.18f,
                                .height_shading = 0.24f};
  Material mountain_stone =
      material({0.245f, 0.255f, 0.235f}, {0.0f, 0.0f, 0.0f}, 0.96f, 0.0f, 0.0f, 0.90f, 7.6f, 0.30f,
               0.70f, SurfacePattern::WeatheredStone, {5.8f, 9.2f}, 0.092f, 0.88f, 0.070f);
  mountain_stone.procedural = {.macro_variation = 0.42f,
                               .micro_normal_strength = 0.34f,
                               .roughness_variation = 0.14f,
                               .height_shading = 0.16f};
  Material cave_mouth_stone = mountain_stone;
  cave_mouth_stone.base_color = {0.315f, 0.318f, 0.286f};
  cave_mouth_stone.surface_pattern = SurfacePattern::CaveRock;
  cave_mouth_stone.pattern_scale = {3.6f, 6.2f};
  cave_mouth_stone.pattern_depth = 0.132f;
  cave_mouth_stone.pattern_contrast = 1.06f;
  cave_mouth_stone.detail_strength = 0.94f;
  cave_mouth_stone.detail_scale = 11.4f;
  cave_mouth_stone.edge_wear = 0.32f;
  cave_mouth_stone.ambient_occlusion = 0.62f;
  cave_mouth_stone.double_sided = true;
  cave_mouth_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_mouth_stone.procedural = {.macro_variation = 0.58f,
                                 .micro_normal_strength = 0.48f,
                                 .roughness_variation = 0.18f,
                                 .wetness = 0.06f,
                                 .height_shading = 0.22f};
  Material cave_wall =
      material({0.082f, 0.078f, 0.067f}, {0.012f, 0.009f, 0.005f}, 0.98f, 0.0f, 0.012f, 0.96f, 8.8f,
               0.30f, 0.60f, SurfacePattern::CaveRock, {2.4f, 4.2f}, 0.120f, 0.94f, 0.060f);
  cave_wall.cull_mode = FaceCullMode::Back;
  cave_wall.double_sided = true;
  cave_wall.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_wall.procedural = {.macro_variation = 0.64f,
                          .micro_normal_strength = 0.54f,
                          .roughness_variation = 0.18f,
                          .wetness = 0.10f,
                          .height_shading = 0.24f};
  Material cave_entrance_wall = cave_wall;
  cave_entrance_wall.base_color = {0.190f, 0.172f, 0.132f};
  cave_entrance_wall.emission_color = {0.052f, 0.032f, 0.014f};
  cave_entrance_wall.emission_strength = 0.046f;
  cave_entrance_wall.ambient_occlusion = 0.54f;
  cave_entrance_wall.procedural.wetness = 0.12f;
  Material cave_floor = makeSupportSurfaceMaterial(cave_wall);
  cave_floor.base_color = {0.130f, 0.114f, 0.088f};
  cave_floor.pattern_scale = {3.2f, 4.4f};
  cave_floor.pattern_depth = 0.085f;
  cave_floor.ambient_occlusion = 0.48f;
  cave_floor.procedural.wetness = 0.18f;
  Material cave_talus = makeSupportSurfaceMaterial(
      material({0.155f, 0.150f, 0.128f}, {0.010f, 0.008f, 0.005f}, 0.94f, 0.0f, 0.008f, 0.82f, 8.6f,
               0.30f, 0.62f, SurfacePattern::CaveRock, {2.8f, 5.4f}, 0.052f, 0.72f, 0.055f));
  cave_talus.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_talus.procedural = {.macro_variation = 0.34f,
                           .micro_normal_strength = 0.28f,
                           .roughness_variation = 0.14f,
                           .wetness = 0.08f,
                           .height_shading = 0.14f};
  Material cave_calcite =
      material({0.46f, 0.39f, 0.30f}, {0.035f, 0.024f, 0.014f}, 0.88f, 0.0f, 0.018f, 0.82f, 13.0f,
               0.34f, 0.58f, SurfacePattern::CaveRock, {3.0f, 9.0f}, 0.095f, 0.88f, 0.050f);
  cave_calcite.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_mineral_glow =
      material({0.72f, 0.46f, 0.24f}, {0.86f, 0.44f, 0.14f}, 0.28f, 0.0f, 0.30f, 0.36f, 12.0f,
               0.04f, 0.92f, SurfacePattern::AmberResin, {7.0f, 11.0f}, 0.020f, 0.64f, 0.04f);
  cave_mineral_glow.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material coal_ore_material =
      material({0.090f, 0.080f, 0.066f}, {0.170f, 0.105f, 0.040f}, 0.52f, 0.12f, 0.115f, 0.74f,
               18.0f, 0.22f, 0.94f, SurfacePattern::CoalVein, {7.4f, 12.0f}, 0.135f, 0.98f, 0.045f);
  coal_ore_material.opacity = 1.0f;
  coal_ore_material.emission_strength = 0.16f;
  coal_ore_material.edge_wear = 0.30f;
  coal_ore_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material ironstone_ore_material =
      material({0.245f, 0.155f, 0.105f}, {0.42f, 0.19f, 0.075f}, 0.78f, 0.08f, 0.040f, 0.82f,
               14.0f, 0.28f, 0.86f, SurfacePattern::CaveRock, {3.8f, 6.8f}, 0.180f, 0.94f,
               0.055f);
  ironstone_ore_material.opacity = 1.0f;
  ironstone_ore_material.edge_wear = 0.38f;
  ironstone_ore_material.emission_strength = 0.075f;
  ironstone_ore_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  ironstone_ore_material.cull_mode = FaceCullMode::Back;
  Material cave_web_material =
      material({0.78f, 0.80f, 0.76f}, {0.070f, 0.078f, 0.070f}, 0.34f, 0.0f, 0.060f, 0.76f,
               15.0f, 0.02f, 0.82f, SurfacePattern::CaveWeb, {4.0f, 18.0f}, 0.018f, 0.72f,
               0.035f,
               {.macro_variation = 0.18f,
                .micro_normal_strength = 0.16f,
                .roughness_variation = 0.18f,
                .height_shading = 0.04f});
  cave_web_material.opacity = 0.86f;
  cave_web_material.double_sided = true;
  cave_web_material.cull_mode = FaceCullMode::None;
  cave_web_material.alpha_mode = MaterialAlphaMode::Blend;
  cave_web_material.depth_write = MaterialDepthWrite::Enabled;
  cave_web_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material cave_skitter_material =
      material({0.115f, 0.058f, 0.042f}, {0.105f, 0.020f, 0.010f}, 0.34f, 0.06f, 0.070f, 0.82f,
               24.0f, 0.12f, 0.84f, SurfacePattern::CaveSkitterChitin, {28.0f, 34.0f}, 0.070f,
               0.92f, 0.035f,
               {.macro_variation = 0.24f,
                .micro_normal_strength = 0.38f,
                .roughness_variation = 0.24f,
                .height_shading = 0.16f});
  cave_skitter_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  cave_skitter_material.opacity = 1.0f;
  cave_skitter_material.alpha_mode = MaterialAlphaMode::Opaque;
  cave_skitter_material.depth_write = MaterialDepthWrite::Enabled;
  cave_skitter_material.double_sided = false;
  cave_skitter_material.cull_mode = FaceCullMode::Back;
  Material industrial_light_metal = material(
      {0.070f, 0.064f, 0.055f}, {0.004f, 0.003f, 0.002f}, 0.52f, 0.68f, 0.004f, 0.54f, 12.0f, 0.16f,
      0.78f, SurfacePattern::WeatheredStone, {4.2f, 6.4f}, 0.024f, 0.56f, 0.05f);
  industrial_light_metal.cull_mode = FaceCullMode::Back;
  Material industrial_red_lens =
      material({0.92f, 0.56f, 0.30f}, {1.0f, 0.42f, 0.18f}, 0.34f, 0.0f, 0.34f, 0.26f, 6.0f, 0.02f,
               0.88f, SurfacePattern::AmberResin, {4.4f, 7.2f}, 0.010f, 0.50f, 0.04f);
  industrial_red_lens.double_sided = true;
  industrial_red_lens.alpha_mode = MaterialAlphaMode::Blend;
  industrial_red_lens.depth_write = MaterialDepthWrite::Disabled;
  applyIndustrialLensColor(industrial_red_lens, kCaveIndustrialWarmLight);
  Material teddy_fur =
      material({0.39f, 0.255f, 0.155f}, {0.0f, 0.0f, 0.0f}, 0.99f, 0.0f, 0.0f, 1.08f, 18.0f, 0.035f,
               0.86f, SurfacePattern::FurFibers, {9.5f, 18.0f}, 0.060f, 0.92f, 0.08f);
  teddy_fur.procedural = {.macro_variation = 0.24f,
                          .micro_normal_strength = 0.30f,
                          .roughness_variation = 0.16f,
                          .height_shading = 0.08f};
  Material teddy_muzzle =
      material({0.70f, 0.55f, 0.37f}, {0.0f, 0.0f, 0.0f}, 0.97f, 0.0f, 0.0f, 0.70f, 12.0f, 0.02f,
               0.86f, SurfacePattern::FurFibers, {6.2f, 10.5f}, 0.026f, 0.58f, 0.08f);
  teddy_muzzle.procedural = {.macro_variation = 0.16f,
                             .micro_normal_strength = 0.20f,
                             .roughness_variation = 0.12f,
                             .height_shading = 0.06f};
  const Material teddy_eye = material({0.018f, 0.014f, 0.012f}, {0.030f, 0.022f, 0.014f}, 0.18f,
                                      0.0f, 0.03f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material teddy_nose = material({0.050f, 0.030f, 0.020f}, {0.018f, 0.010f, 0.006f}, 0.34f,
                                       0.0f, 0.02f, 0.04f, 3.5f, 0.0f, 1.0f);
  Material sign_wood =
      material({0.48f, 0.31f, 0.16f}, {0.0f, 0.0f, 0.0f}, 0.86f, 0.0f, 0.0f, 0.72f, 8.2f, 0.62f,
               0.72f, SurfacePattern::PaintedWood, {2.2f, 7.6f}, 0.036f, 0.86f, 0.08f);
  sign_wood.double_sided = true;
  Material sign_ink =
      material({0.055f, 0.038f, 0.020f}, {0.0f, 0.0f, 0.0f}, 0.78f, 0.0f, 0.0f, 0.18f, 5.0f, 0.12f,
               0.98f, SurfacePattern::FiberStrands, {3.4f, 8.2f}, 0.004f, 0.34f, 0.08f);
  sign_ink.double_sided = true;
  const Material marsh_soil = makeSupportSurfaceMaterial(
      material({0.22f, 0.25f, 0.16f}, {0.0f, 0.0f, 0.0f}, 0.97f, 0.0f, 0.0f, 0.86f, 9.2f, 0.18f,
               0.94f, SurfacePattern::GrassSoil, {6.8f, 8.6f}, 0.070f, 0.78f, 0.08f));
  Material wetland_blades =
      material({0.20f, 0.31f, 0.18f}, {0.0f, 0.0f, 0.0f}, 0.92f, 0.0f, 0.0f, 0.58f, 9.5f, 0.08f,
               0.95f, SurfacePattern::Foliage, {8.5f, 11.0f}, 0.024f, 0.58f, 0.08f);
  wetland_blades.double_sided = true;
  wetland_blades.alpha_mode = MaterialAlphaMode::Masked;
  wetland_blades.camera_occlusion = CameraOcclusionPolicy::Solid;
  const Material crocodile_hide =
      material({0.11f, 0.23f, 0.115f}, {0.010f, 0.020f, 0.006f}, 0.72f, 0.0f, 0.018f, 0.68f, 15.0f,
               0.18f, 0.86f, SurfacePattern::ReptileScales, {22.0f, 30.0f}, 0.048f, 0.92f, 0.030f);
  const Material crocodile_belly =
      material({0.42f, 0.39f, 0.23f}, {0.010f, 0.008f, 0.003f}, 0.78f, 0.0f, 0.0f, 0.38f, 8.0f,
               0.04f, 0.92f, SurfacePattern::FiberStrands, {4.0f, 9.0f}, 0.014f, 0.36f, 0.06f);
  const Material crocodile_eye = material({0.70f, 0.54f, 0.12f}, {0.32f, 0.20f, 0.04f}, 0.32f, 0.0f,
                                          0.10f, 0.0f, 1.0f, 0.0f, 1.0f);
  const Material blood_material =
      material({0.48f, 0.018f, 0.010f}, {0.10f, 0.0f, 0.0f}, 0.44f, 0.0f, 0.10f, 0.20f, 9.0f, 0.03f,
               0.96f, SurfacePattern::WaterSurface, {2.5f, 3.0f}, 0.012f, 0.26f, 0.05f);
  Material x_eye_material =
      material({0.010f, 0.006f, 0.004f}, {0.12f, 0.010f, 0.004f}, 0.62f, 0.0f, 0.08f);
  x_eye_material.double_sided = true;
  Material emerald_feathers =
      material({0.10f, 0.48f, 0.36f}, {0.018f, 0.060f, 0.046f}, 0.46f, 0.0f, 0.025f, 0.44f, 12.0f,
               0.04f, 0.92f, SurfacePattern::FeatherVanes, {5.6f, 13.0f}, 0.030f, 0.62f, 0.05f);
  emerald_feathers.double_sided = true;
  emerald_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material crimson_feathers =
      material({0.54f, 0.12f, 0.18f}, {0.045f, 0.010f, 0.018f}, 0.50f, 0.0f, 0.030f, 0.40f, 11.5f,
               0.03f, 0.93f, SurfacePattern::FeatherVanes, {5.2f, 12.4f}, 0.028f, 0.58f, 0.05f);
  crimson_feathers.double_sided = true;
  crimson_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material ink_white_feathers =
      material({0.12f, 0.11f, 0.105f}, {0.020f, 0.018f, 0.016f}, 0.54f, 0.0f, 0.018f, 0.46f, 13.5f,
               0.02f, 0.94f, SurfacePattern::FeatherVanes, {6.6f, 15.5f}, 0.026f, 0.66f, 0.05f);
  ink_white_feathers.double_sided = true;
  ink_white_feathers.alpha_mode = MaterialAlphaMode::Masked;
  Material gold_blue_feathers =
      material({0.78f, 0.48f, 0.13f}, {0.058f, 0.032f, 0.012f}, 0.44f, 0.0f, 0.026f, 0.38f, 10.8f,
               0.03f, 0.92f, SurfacePattern::FeatherVanes, {4.8f, 11.2f}, 0.024f, 0.56f, 0.05f);
  gold_blue_feathers.double_sided = true;
  gold_blue_feathers.alpha_mode = MaterialAlphaMode::Masked;
  const Material nest_twig_material =
      material({0.35f, 0.22f, 0.12f}, {0.0f, 0.0f, 0.0f}, 0.84f, 0.0f, 0.0f, 0.66f, 14.0f, 0.18f,
               0.86f, SurfacePattern::TwigNest, {8.0f, 14.0f}, 0.040f, 0.82f, 0.06f);
  auto appendScenery = [&](const char *name, const MeshPrimitive primitive, const Vec3 position,
                           const Vec3 scale, const Vec3 rotation, const Material &scenery_material,
                           const float spin_rate = 0.0f) {
    RenderObject object;
    object.name = name;
    object.primitive = primitive;
    object.transform.position = position;
    object.transform.scale = scale;
    object.transform.rotation = rotation;
    object.material = scenery_material;
    object.camera_occlusion_fade = allowsCameraOcclusionFade(scenery_material);
    object.spin_rate = spin_rate;
    const std::size_t index = appendObject(object);
    scenery_objects_.push_back(index);
    return index;
  };

  auto appendGeneratedScenery = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                                    const Vec3 position, const Vec3 scale, const Vec3 rotation,
                                    const Material &scenery_material,
                                    const float spin_rate = 0.0f) {
    RenderObject object;
    object.name = name;
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = mesh;
    object.transform.position = position;
    object.transform.scale = scale;
    object.transform.rotation = rotation;
    object.material = scenery_material;
    object.camera_occlusion_fade = allowsCameraOcclusionFade(scenery_material);
    object.spin_rate = spin_rate;
    const std::size_t index = appendObject(object);
    scenery_objects_.push_back(index);
    return index;
  };

  const auto keepCameraSolid = [&](const std::size_t object_index) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].camera_occlusion_fade = false;
    }
    return object_index;
  };

  const auto enableContactShadow = [&](const std::size_t object_index, const float strength,
                                       const float radius_scale = 1.0f) {
    if (object_index < scene_.objects().size()) {
      RenderObject &object = scene_.objects()[object_index];
      object.auto_contact_shadow = true;
      object.casts_contact_shadow = true;
      object.contact_shadow_strength = strength;
      object.contact_shadow_radius_scale = radius_scale;
    }
    return object_index;
  };

  const auto appendStructuralScenery = [&](const char *name, const MeshPrimitive primitive,
                                           const Vec3 position, const Vec3 scale,
                                           const Vec3 rotation, const Material &scenery_material) {
    return keepCameraSolid(
        appendScenery(name, primitive, position, scale, rotation, scenery_material));
  };

  const auto appendGeneratedStructuralScenery =
      [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh, const Vec3 position,
          const Vec3 scale, const Vec3 rotation, const Material &scenery_material) {
        return keepCameraSolid(
            appendGeneratedScenery(name, mesh, position, scale, rotation, scenery_material));
      };

  auto appendBeam = [&](const char *name, const Vec3 from, const Vec3 to, const float radius,
                        const Material &beam_material) {
    const float beam_length = std::max(length(to - from), 0.05f);
    return appendScenery(name, MeshPrimitive::Box, (from + to) * 0.5f,
                         {radius, beam_length, radius}, segmentRotation(from, to), beam_material);
  };

  Material prism_statue_stone = dark_masonry;
  prism_statue_stone.base_color = mixColor(prism_statue_stone.base_color, {0.18f, 0.205f, 0.20f},
                                           0.34f);
  prism_statue_stone.roughness = 0.88f;
  prism_statue_stone.metallic = 0.0f;
  prism_statue_stone.edge_wear = 0.38f;
  prism_statue_stone.ambient_occlusion = 0.78f;
  prism_statue_stone.procedural = {.macro_variation = 0.30f,
                                   .micro_normal_strength = 0.34f,
                                   .roughness_variation = 0.16f,
                                   .height_shading = 0.14f};
  prism_statue_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_statue_trim = pond_stone;
  prism_statue_trim.base_color = mixColor(prism_statue_trim.base_color, {0.26f, 0.30f, 0.30f},
                                          0.36f);
  prism_statue_trim.roughness = 0.82f;
  prism_statue_trim.edge_wear = 0.44f;
  prism_statue_trim.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_inlay = weathered_iron;
  prism_inlay.base_color = mixColor(prism_inlay.base_color, {0.08f, 0.12f, 0.13f}, 0.46f);
  prism_inlay.emission_color = {0.10f, 0.36f, 0.42f};
  prism_inlay.emission_strength = 0.08f;
  prism_inlay.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_core_material = amber_shard;
  prism_core_material.base_color = {0.34f, 0.78f, 1.0f};
  prism_core_material.emission_color = {0.30f, 0.78f, 1.0f};
  prism_core_material.emission_strength = 0.30f;
  prism_core_material.surface_pattern = SurfacePattern::AmberResin;
  prism_core_material.pattern_scale = {8.0f, 14.0f};
  prism_core_material.pattern_depth = 0.060f;
  prism_core_material.pattern_contrast = 0.95f;
  prism_core_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material prism_conduit_material = prism_core_material;
  prism_conduit_material.base_color = {0.16f, 0.56f, 0.92f};
  prism_conduit_material.emission_color = {0.26f, 0.82f, 1.0f};
  prism_conduit_material.emission_strength = 0.46f;
  prism_conduit_material.opacity = 0.18f;
  prism_conduit_material.double_sided = true;
  prism_conduit_material.alpha_mode = MaterialAlphaMode::Blend;
  prism_conduit_material.depth_write = MaterialDepthWrite::Disabled;
  prism_conduit_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  prism_conduit_material.surface_pattern = SurfacePattern::FiberStrands;
  prism_conduit_material.pattern_scale = {10.0f, 18.0f};
  prism_conduit_material.pattern_depth = 0.030f;
  prism_conduit_material.procedural = {.macro_variation = 0.38f,
                                       .micro_normal_strength = 0.18f,
                                       .roughness_variation = 0.12f,
                                       .height_shading = 0.08f};

  const auto appendRelayStatuePart = [&](const char *name, const MeshPrimitive primitive,
                                         const Vec3 position, const Vec3 scale,
                                         const Vec3 rotation, const Material &part_material,
                                         const bool collision = false,
                                         const float shadow = 0.22f) {
    const std::size_t index =
        enableContactShadow(appendStructuralScenery(name, primitive, position, scale, rotation,
                                                    part_material),
                            shadow, 0.86f);
    if (collision) {
      scenery_collision_boxes_.push_back({position, scale * 0.50f});
      support_surfaces_.addBox({position, scale * 0.50f});
    }
    return index;
  };
  const Vec3 relay_core = prismRelayFocusPosition();
  appendRelayStatuePart("Prism relay statue square plinth", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 0.11f, 0.0f},
                        {1.45f, 0.22f, 1.45f}, {}, prism_statue_trim, true, 0.32f);
  appendRelayStatuePart("Prism relay statue octagonal riser", MeshPrimitive::Pillar,
                        prism_relay_base_ + Vec3{0.0f, 0.44f, 0.0f},
                        {0.78f, 0.58f, 0.78f}, {}, prism_statue_stone, true, 0.30f);
  appendRelayStatuePart("Prism relay statue engraved inlay", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 0.75f, 0.0f},
                        {0.92f, 0.045f, 0.92f}, {}, prism_inlay, false, 0.18f);
  appendRelayStatuePart("Prism relay statue torso", MeshPrimitive::Pillar,
                        prism_relay_base_ + Vec3{0.0f, 1.18f, 0.0f},
                        {0.34f, 0.82f, 0.28f}, {}, prism_statue_stone, false, 0.24f);
  appendRelayStatuePart("Prism relay statue shoulders", MeshPrimitive::Box,
                        prism_relay_base_ + Vec3{0.0f, 1.54f, 0.02f},
                        {0.82f, 0.16f, 0.28f}, {}, prism_statue_trim, false, 0.22f);
  appendRelayStatuePart("Prism relay statue head", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{0.0f, 1.83f, 0.0f},
                        {0.18f, 0.22f, 0.18f}, {}, prism_statue_stone, false, 0.20f);
  enableContactShadow(appendBeam("Prism relay statue left raised arm",
                                 prism_relay_base_ + Vec3{-0.34f, 1.50f, 0.02f},
                                 prism_relay_base_ + Vec3{-0.18f, 2.06f, 0.06f}, 0.060f,
                                 prism_statue_trim),
                      0.18f, 0.70f);
  enableContactShadow(appendBeam("Prism relay statue right raised arm",
                                 prism_relay_base_ + Vec3{0.34f, 1.50f, 0.02f},
                                 prism_relay_base_ + Vec3{0.18f, 2.06f, 0.06f}, 0.060f,
                                 prism_statue_trim),
                      0.18f, 0.70f);
  appendRelayStatuePart("Prism relay statue left crystal hand", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{-0.18f, 2.07f, 0.06f},
                        {0.075f, 0.075f, 0.075f}, {}, prism_inlay, false, 0.12f);
  appendRelayStatuePart("Prism relay statue right crystal hand", MeshPrimitive::Sphere,
                        prism_relay_base_ + Vec3{0.18f, 2.07f, 0.06f},
                        {0.075f, 0.075f, 0.075f}, {}, prism_inlay, false, 0.12f);
  prism_relay_core_object_ =
      appendScenery("Aster prism relay refractor core", MeshPrimitive::Crystal, relay_core,
                    {0.24f, 0.52f, 0.24f}, {0.0f, status_.elapsed_seconds, 0.0f},
                    prism_core_material, 0.36f);
  prism_relay_core_valid_ = true;
  prism_relay_ring_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay equatorial field ring",
      makeSharedMesh(
          makeEnergyConduitRingMesh({.radius = 0.56f, .band_width = 0.030f, .segments = 72})),
      relay_core, {1.0f, 1.0f, 1.0f}, {}, prism_conduit_material, 0.18f));
  prism_relay_ring_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay vertical field ring",
      makeSharedMesh(
          makeEnergyConduitRingMesh({.radius = 0.68f, .band_width = 0.026f, .segments = 72})),
      relay_core, {1.0f, 1.0f, 1.0f}, {radians(90.0f), 0.0f, radians(12.0f)},
      prism_conduit_material, -0.14f));

  std::vector<EnergyConduitRibbonSpec> relay_conduits;
  relay_conduits.reserve(3u);
  const auto addRelayConduit = [&](const Vec3 anchor, const float crest, const float width) {
    Vec3 planar_direction{anchor.x - relay_core.x, 0.0f, anchor.z - relay_core.z};
    if (length(planar_direction) <= 0.0001f) {
      planar_direction = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 start =
        relay_core + normalize(planar_direction) * 0.76f + Vec3{0.0f, -0.08f, 0.0f};
    const Vec3 midpoint = (start + anchor) * 0.5f + Vec3{0.0f, crest, 0.0f};
    relay_conduits.push_back({.points = {start, midpoint, anchor},
                              .width = width,
                              .crest_height = crest * 0.20f,
                              .subdivisions_per_segment = 24});
  };
  const Vec3 relay_left_anchor = prism_relay_base_ + Vec3{-0.92f, 0.78f, 0.26f};
  const Vec3 relay_right_anchor = prism_relay_base_ + Vec3{0.92f, 0.78f, 0.26f};
  const Vec3 relay_rear_anchor = prism_relay_base_ + Vec3{0.0f, 0.88f, -1.02f};
  addRelayConduit(relay_left_anchor, 0.36f, 0.070f);
  addRelayConduit(relay_right_anchor, 0.36f, 0.070f);
  addRelayConduit(relay_rear_anchor, 0.44f, 0.064f);
  prism_relay_conduit_objects_.push_back(appendGeneratedScenery(
      "Aster prism relay conduit network",
      makeSharedMesh(makeEnergyConduitRibbonNetworkMesh(relay_conduits)),
      {}, {1.0f, 1.0f, 1.0f}, {}, prism_conduit_material));
  for (const Vec3 node : {relay_left_anchor, relay_right_anchor, relay_rear_anchor}) {
    prism_relay_node_objects_.push_back(appendScenery("Aster prism relay endpoint node",
                                                     MeshPrimitive::Sphere, node,
                                                     {0.095f, 0.095f, 0.095f}, {},
                                                     prism_core_material,
                                                     0.22f));
  }

  for (PlacedResourceRock &rock : placed_rocks_) {
    const ItemDefinition *definition = item_registry_.find(rock.item_id);
    if (rock.id.empty()) {
      rock.id = placedResourceId(placed_resource_serial_++);
    }
    if (definition != nullptr) {
      rock.placement_shape = definition->placement_shape;
    }
    const std::size_t index =
        keepCameraSolid(appendScenery("Placed cave resource rock",
                                      placementPrimitiveFor(rock.placement_shape), rock.position,
                                      rock.scale, rock.rotation,
                                      lumenPlacedResourceMaterial(definition)));
    rock.object_index = index;
    rock.body = {};
    if (index < scene_.objects().size()) {
      RenderObject &object = scene_.objects()[index];
      object.casts_contact_shadow = true;
      object.contact_shadow_strength = 0.18f;
      object.contact_shadow_radius_scale = 0.78f;
    }
    support_surfaces_.addBox({rock.position, rock.collision_half_extents});
  }

  const auto appendIndustrialWallLight = [&](const CaveWallFixturePlacement &placement,
                                             const Material &metal,
                                             const Material &lens) -> CaveWallFixtureVisual {
    CaveWallFixtureVisual visual;
    const Vec3 normal =
        length(placement.normal) > 0.0001f ? normalize(placement.normal) : Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 up =
        length(placement.up) > 0.0001f ? normalize(placement.up) : Vec3{0.0f, 1.0f, 0.0f};
    Vec3 right = normalize(cross(up, normal));
    if (length(right) <= 0.0001f) {
      right = {1.0f, 0.0f, 0.0f};
    }
    const float yaw = std::atan2(normal.x, normal.z);
    const Vec3 rotation{0.0f, yaw, 0.0f};
    const Vec3 base =
        length(placement.mount_position) > 0.0001f ? placement.mount_position : placement.position;
    visual.backplate = keepCameraSolid(appendScenery("Industrial red cave wall light backplate",
                                                     MeshPrimitive::Box, base + normal * 0.018f,
                                                     {0.48f, 0.30f, 0.036f}, rotation, metal));
    visual.lens = keepCameraSolid(appendScenery("Industrial red cave wall light glowing lens",
                                                MeshPrimitive::Box, placement.lens_position,
                                                {0.36f, 0.19f, 0.032f}, rotation, lens));
    if (visual.lens < scene_.objects().size()) {
      applyIndustrialLensColor(scene_.objects()[visual.lens].material, placement.light_color);
    }

    constexpr std::array<float, 5> kVerticalOffsets{-0.30f, -0.15f, 0.0f, 0.15f, 0.30f};
    for (std::size_t i = 0; i < visual.vertical_guards.size(); ++i) {
      const Vec3 rib_center = base + normal * 0.105f + right * kVerticalOffsets[i];
      visual.vertical_guards[i] = keepCameraSolid(
          appendBeam("Industrial red cave wall light vertical guard", rib_center - up * 0.235f,
                     rib_center + up * 0.235f, 0.018f, metal));
    }
    constexpr std::array<float, 3> kHorizontalOffsets{-0.17f, 0.0f, 0.17f};
    for (std::size_t i = 0; i < visual.horizontal_guards.size(); ++i) {
      const Vec3 rib_center = base + normal * 0.125f + up * kHorizontalOffsets[i];
      visual.horizontal_guards[i] = keepCameraSolid(
          appendBeam("Industrial red cave wall light horizontal guard", rib_center - right * 0.385f,
                     rib_center + right * 0.385f, 0.016f, metal));
    }
    return visual;
  };

  auto appendGeneratedBeam = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                                 const Vec3 from, const Vec3 to, const float radius,
                                 const Material &beam_material) {
    const float beam_length = std::max(length(to - from), 0.05f);
    return appendGeneratedScenery(name, mesh, (from + to) * 0.5f, {radius, beam_length, radius},
                                  segmentRotation(from, to), beam_material);
  };

  const auto appendGroundUnderlay = [&](const char *name,
                                        const std::shared_ptr<const CpuMesh> &mesh,
                                        const Vec3 position, const Material &underlay_material) {
    appendGeneratedScenery(name, mesh, position, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                           underlay_material);
    const Transform transform{position, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    support_surfaces_.addMesh({mesh, transform, 0.12f});
    decorative_ground_surfaces.addMesh({mesh, transform, 0.12f});
  };

  const std::size_t terrain_index = appendGeneratedScenery(
      "Surrounding heightfield terrain",
      terrainMesh(terrain_, terrain_holes, terrainPlacementClips(terrain_placement),
                  terrain_portal_clips),
      {0.0f, -0.012f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, layered_terrain);
  scene_.objects()[terrain_index].camera_occlusion_fade = false;

  for (const CastleCourseGroundZone &zone : course.ground_zones) {
    RenderObject floor;
    floor.name = "Castle interior stone floor";
    floor.primitive = MeshPrimitive::Plane;
    floor.transform.position = {castle_origin.x + zone.center.x, 0.010f,
                                castle_origin.z + zone.center.y};
    floor.custom_mesh =
        plazaDeckMesh(zone.half_extents * 2.0f, floor.transform.position, stone_floor_holes);
    floor.camera_occlusion_fade = false;
    floor.material = stone_plaza;
    appendObject(floor);
    support_surfaces_.addMesh({floor.custom_mesh, floor.transform, 0.50f});
    decorative_ground_surfaces.addMesh({floor.custom_mesh, floor.transform, 0.50f});
  }

  player_avatar_ = makePlushHumanoidAvatar({.height = tuning_.player_height * 1.24f,
                                            .fur_material = teddy_fur,
                                            .muzzle_material = teddy_muzzle,
                                            .eye_material = teddy_eye,
                                            .nose_material = teddy_nose});
  player_avatar_support_extent_ = avatarGroundSupportExtent(player_avatar_);
  player_avatar_instance_ = appendAvatar(
      scene_, player_avatar_, {.position = avatarPosePosition(), .facing_yaw = player_facing_yaw_});
  bool found_left_eye = false;
  bool found_right_eye = false;
  for (std::size_t i = 0;
       i < player_avatar_.parts.size() && i < player_avatar_instance_.object_indices.size(); ++i) {
    if (player_avatar_.parts[i].name == "left bead eye") {
      left_eye_object_ = player_avatar_instance_.object_indices[i];
      found_left_eye = true;
    } else if (player_avatar_.parts[i].name == "right bead eye") {
      right_eye_object_ = player_avatar_instance_.object_indices[i];
      found_right_eye = true;
    }
  }
  eye_objects_valid_ = found_left_eye && found_right_eye;
  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].camera_occlusion_fade = false;
      enableContactShadow(object_index, 0.42f, 0.92f);
    }
  }

  for (Shard &shard : shards_) {
    RenderObject object;
    object.name = "Veined amber shard";
    object.primitive = MeshPrimitive::Crystal;
    object.transform.scale = {tuning_.shard_radius * 0.82f, tuning_.shard_radius * 1.55f,
                              tuning_.shard_radius * 0.82f};
    object.material = amber_shard;
    shard.object_index = appendObject(object);
    enableContactShadow(shard.object_index, 0.38f, 0.72f);
  }

  for (Sentinel &sentinel : sentinels_) {
    RenderObject object;
    object.name = "Floodlight ward sentinel";
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = signalSentinelMesh();
    object.transform.scale = {tuning_.sentinel_radius * 1.35f, tuning_.sentinel_radius * 1.35f,
                              tuning_.sentinel_radius * 1.35f};
    object.material = sentinel_material;
    sentinel.object_index = appendObject(object);
    enableContactShadow(sentinel.object_index, 0.34f, 0.82f);
  }

  const auto castleMaterial = [&](const CastleCourseChannel channel) -> const Material & {
    switch (channel) {
    case CastleCourseChannel::Foundation:
      return stone_plaza;
    case CastleCourseChannel::WarmStone:
      return brick_facade;
    case CastleCourseChannel::CoolStone:
      return dark_masonry;
    case CastleCourseChannel::TrimStone:
      return pond_stone;
    case CastleCourseChannel::Wood:
      return sign_wood;
    case CastleCourseChannel::Iron:
      return weathered_iron;
    case CastleCourseChannel::WarmLight:
      return warm_window;
    }
    return brick_facade;
  };

  for (const VoxelMeshBatch &batch : course.structure.mesh_batches) {
    const CastleCourseChannel channel = static_cast<CastleCourseChannel>(batch.channel);
    const std::shared_ptr<const CpuMesh> mesh = makeSharedMesh(batch.mesh);
    appendGeneratedStructuralScenery(castleChannelName(channel), mesh, castle_origin,
                                     {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                     castleMaterial(channel));
    support_surfaces_.addMesh(
        {mesh, {castle_origin, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.55f});
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    appendStructuralScenery(castleChannelName(element.channel), MeshPrimitive::Box,
                            castle_origin + element.center, element.half_extents * 2.0f,
                            {0.0f, 0.0f, 0.0f}, castleMaterial(element.channel));
    if (element.support || element.collidable) {
      support_surfaces_.addBox({castle_origin + element.center, element.half_extents});
    }
  }
  appendGeneratedScenery("Jump sign text", parkourSignStrokeMesh(),
                         castle_origin + Vec3{0.0f, 3.34f, 3.43f}, {0.84f, 0.84f, 0.84f},
                         {0.0f, 0.0f, 0.0f}, sign_ink);

  const Vec3 cave_entrance = caveEntrancePlanar();
  const TerrainSurfaceSample cave_ground =
      sampleTerrain(terrain_, {cave_entrance.x, cave_entrance.z});
  const float cave_floor_y = (cave_ground.valid ? cave_ground.height : 0.0f) + 0.030f;
  const CaveComplexSpec cave_spec = lumenTerrainCoveredCaveComplexSpec(terrain_, cave_floor_y);
  CaveComplex cave_complex = buildCaveComplex(cave_spec);
  const CaveComplexSpec deep_cave_spec = lumenDeepCaveComplexSpec(cave_spec.tunnel);
  CaveComplex deep_cave_complex = buildCaveComplex(deep_cave_spec);
  const LumenCaveWebPlacement web_placement = lumenCaveWebPlacement(cave_floor_y);
  CaveWebObstacle cave_web;
  cave_web.id = "cave_web:0";
  cave_web.center = web_placement.center;
  cave_web.normal =
      length(web_placement.normal) > 0.0001f ? normalize(web_placement.normal)
                                             : Vec3{0.0f, 0.0f, -1.0f};
  cave_web.side = length(web_placement.side) > 0.0001f ? normalize(web_placement.side)
                                                       : Vec3{1.0f, 0.0f, 0.0f};
  cave_web.up =
      length(web_placement.up) > 0.0001f ? normalize(web_placement.up) : Vec3{0.0f, 1.0f, 0.0f};
  cave_web.radius_x = web_placement.radius_x;
  cave_web.radius_y = web_placement.radius_y;
  cave_web.thickness = web_placement.thickness;
  cave_web.slow_scale = kCaveWebSlowScale;
  cave_web.hardness = 2.0f;
  cave_web.object_index = keepCameraSolid(appendGeneratedScenery(
      "Oval cave spider web span",
      makeSharedMesh(makeCaveWebMesh({.center = cave_web.center,
                                      .normal = cave_web.normal,
                                      .side = cave_web.side,
                                      .up = cave_web.up,
                                      .radius_x = cave_web.radius_x,
                                      .radius_y = cave_web.radius_y,
                                      .radial_strands = 26,
                                      .ring_strands = 8,
                                      .ring_segments = 112,
                                      .strand_width =
                                          std::max(0.010f, std::min(cave_web.radius_x,
                                                                    cave_web.radius_y) *
                                                            0.010f),
                                      .anchor_width_scale = 1.30f,
                                      .sag = cave_web.radius_y * 0.065f,
                                      .irregularity = 0.055f,
                                      .seed = kLumenCaveSeed + 7331u,
                                      .double_sided = true})),
      {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_web_material));
  if (cave_web.object_index < scene_.objects().size()) {
    scene_.objects()[cave_web.object_index].auto_contact_shadow = false;
  }
  cave_webs_.push_back(cave_web);
  const std::shared_ptr<const CpuMesh> cave_skitter_mesh =
      makeSharedMesh(makeCaveSkitterMesh({.body_segments = 34,
                                          .body_rings = 11,
                                          .leg_segments = 10,
                                          .body_length = 0.30f,
                                          .body_width = 0.17f,
                                          .body_height = 0.095f,
                                          .abdomen_length = 0.36f,
                                          .abdomen_width = 0.22f,
                                          .abdomen_height = 0.14f,
                                          .leg_span = 0.42f,
                                          .leg_lift = 0.050f,
                                          .fang_length = 0.070f,
                                          .eye_radius = 0.020f}));
  for (int skitter_index = 0; skitter_index < kCaveSkitterEncounterCount; ++skitter_index) {
    const float angle =
        (static_cast<float>(skitter_index) / static_cast<float>(kCaveSkitterEncounterCount)) *
            kPi * 2.0f +
        kPi * 0.18f;
    CaveSkitter skitter;
    skitter.id = "cave_skitter:" + std::to_string(skitter_index);
    skitter.max_health = kCaveSkitterMaxHealth;
    skitter.health = skitter.max_health;
    skitter.state.home_offset = {std::cos(angle) * cave_web.radius_x * 0.38f,
                                 std::sin(angle) * cave_web.radius_y * 0.30f};
    skitter.state.position = cave_web.center + cave_web.side * skitter.state.home_offset.x +
                             cave_web.up * skitter.state.home_offset.y -
                             cave_web.normal * std::max(cave_web.thickness * 0.64f, 0.12f);
    skitter.state.facing_yaw = std::atan2(cave_web.normal.x, cave_web.normal.z);
    skitter.state.temperament =
        0.22f + 0.26f * static_cast<float>(skitter_index) +
        0.08f * std::sin(static_cast<float>(skitter_index) * 1.73f);
    skitter.object_index = appendGeneratedScenery("Cave skitter arachnid", cave_skitter_mesh,
                                                  skitter.state.position, {1.0f, 1.0f, 1.0f},
                                                  {0.0f, skitter.state.facing_yaw, 0.0f},
                                                  cave_skitter_material);
    enableContactShadow(skitter.object_index, 0.42f, 0.82f);
  cave_skitters_.push_back(skitter);
  }
  cave_sections_.push_back({.tunnel = cave_spec.tunnel,
                            .wall_fixtures =
                                placeCaveWallFixtures(cave_spec.tunnel,
                                                      lumenCaveWallFixtureProfile()),
                            .secondary_wall_fixtures = placeCaveWallFixtures(
                                cave_spec.tunnel, lumenCaveSecondaryWallFixtureProfile()),
                            .contributes_entrance_light = true});
  cave_sections_.push_back(
      {.tunnel = deep_cave_spec.tunnel,
       .wall_fixtures =
           placeCaveWallFixtures(deep_cave_spec.tunnel, lumenDeepCaveWallFixtureProfile()),
       .secondary_wall_fixtures = placeCaveWallFixtures(deep_cave_spec.tunnel,
                                                        lumenDeepCaveSecondaryWallFixtureProfile()),
       .contributes_entrance_light = false});
  cave_entrance_light_position_ = caveEntranceLightPosition(cave_floor_y);
  cave_viewer_cull_volume_ = viewerCullVolumeForMesh(cave_complex.collision_mesh, 0.20f,
                                                     FaceCullMode::Back, FaceCullMode::Back);
  cave_collision_meshes_.push_back(makeSharedMesh(std::move(cave_complex.collision_mesh)));
  cave_collision_meshes_.push_back(makeSharedMesh(std::move(deep_cave_complex.collision_mesh)));

  chest_base_ = cave_complex.chest_position;
  chest_yaw_ = cave_complex.chest_yaw;
  const auto chestPartPosition = [&](const Vec3 local_offset) {
    return chest_base_ + rotateYaw(local_offset, chest_yaw_);
  };
  const Material item_iron = weathered_iron;
  Material torch_flame_material = amber_lamp;
  torch_flame_material.base_color = {1.0f, 0.42f, 0.10f};
  torch_flame_material.emission_color = {1.0f, 0.34f, 0.08f};
  torch_flame_material.emission_strength = 0.78f;
  torch_flame_material.opacity = 0.94f;
  torch_flame_material.alpha_mode = MaterialAlphaMode::Blend;
  torch_flame_material.depth_write = MaterialDepthWrite::Disabled;
  torch_flame_material.camera_occlusion = CameraOcclusionPolicy::Solid;
  Material dark_chest_interior =
      material({0.030f, 0.020f, 0.014f}, {0.020f, 0.010f, 0.004f}, 0.72f, 0.0f, 0.02f);

  const std::size_t parkour_chest_index = appendScenery(
      "Parkour starter chest base", MeshPrimitive::Box, chestPartPosition({0.0f, 0.19f, 0.0f}),
      {0.86f, 0.34f, 0.58f}, {0.0f, chest_yaw_, 0.0f}, sign_wood);
  enableContactShadow(parkour_chest_index, 0.58f, 0.88f);
  appendScenery("Chest dark open interior", MeshPrimitive::Box,
                chestPartPosition({0.0f, 0.39f, 0.0f}), {0.76f, 0.065f, 0.48f},
                {0.0f, chest_yaw_, 0.0f}, dark_chest_interior);
  chest_lid_object_ = appendScenery("Animated chest lid", MeshPrimitive::Box,
                                    chestPartPosition({0.0f, 0.49f, 0.0f}), {0.88f, 0.17f, 0.62f},
                                    {0.0f, chest_yaw_, 0.0f}, sign_wood);
  chest_lid_band_object_ = appendScenery(
      "Chest moving iron band front", MeshPrimitive::Box, chestPartPosition({0.0f, 0.44f, 0.326f}),
      {0.72f, 0.050f, 0.028f}, {0.0f, chest_yaw_, 0.0f}, weathered_iron);
  chest_lock_object_ =
      appendScenery("Chest moving blackened iron lock", MeshPrimitive::Box,
                    chestPartPosition({0.0f, 0.36f, 0.354f}), {0.105f, 0.14f, 0.030f},
                    {0.0f, chest_yaw_, 0.0f}, weathered_iron);
  appendScenery("Chest blackened iron band front", MeshPrimitive::Box,
                chestPartPosition({0.0f, 0.25f, 0.294f}), {0.72f, 0.052f, 0.030f},
                {0.0f, chest_yaw_, 0.0f}, weathered_iron);

  const auto appendChestItemPart = [&](ChestItemVisual &item, const char *name,
                                       const MeshPrimitive primitive, const Vec3 local_position,
                                       const Vec3 scale, const Vec3 local_rotation,
                                       const Material &part_material) {
    const std::size_t index = appendScenery(
        name, primitive, chestPartPosition(local_position), scale,
        {local_rotation.x, chest_yaw_ + local_rotation.y, local_rotation.z}, part_material);
    item.parts.push_back(
        {index, local_position, local_rotation, scale, part_material.emission_strength});
    return index;
  };
  chest_items_.push_back({"pickaxe", {-0.23f, 0.58f, 0.02f}, 0.34f, true, {}});
  ChestItemVisual &pickaxe_item = chest_items_.back();
  appendChestItemPart(pickaxe_item, "Chest pickaxe handle", MeshPrimitive::Box,
                      {-0.27f, 0.57f, -0.01f}, {0.040f, 0.46f, 0.040f},
                      segmentRotation({-0.45f, 0.47f, -0.14f}, {-0.09f, 0.67f, 0.14f}), sign_wood);
  appendChestItemPart(pickaxe_item, "Chest pickaxe head", MeshPrimitive::Box,
                      {-0.07f, 0.69f, 0.17f}, {0.38f, 0.060f, 0.065f},
                      {radians(4.0f), radians(54.0f), radians(-7.0f)}, item_iron);

  chest_items_.push_back({"torch", {0.08f, 0.61f, 0.02f}, 0.34f, true, {}});
  ChestItemVisual &torch_item = chest_items_.back();
  appendChestItemPart(torch_item, "Chest torch wrapped handle", MeshPrimitive::Box,
                      {0.05f, 0.57f, -0.04f}, {0.048f, 0.43f, 0.048f},
                      segmentRotation({-0.02f, 0.46f, -0.17f}, {0.14f, 0.68f, 0.13f}), sign_wood);
  appendChestItemPart(torch_item, "Chest torch ember head", MeshPrimitive::Sphere,
                      {0.16f, 0.71f, 0.16f}, {0.075f, 0.095f, 0.075f}, {0.0f, 0.0f, 0.0f},
                      torch_flame_material);

  chest_items_.push_back({"fishing_rod", {0.34f, 0.58f, 0.00f}, 0.36f, true, {}});
  ChestItemVisual &rod_item = chest_items_.back();
  appendChestItemPart(rod_item, "Chest fishing rod blank", MeshPrimitive::Box,
                      {0.31f, 0.58f, -0.01f}, {0.024f, 0.72f, 0.024f},
                      segmentRotation({0.05f, 0.47f, -0.21f}, {0.58f, 0.70f, 0.20f}),
                      fishing_rod_material);
  appendChestItemPart(rod_item, "Chest fishing reel", MeshPrimitive::Sphere, {0.18f, 0.55f, -0.11f},
                      {0.100f, 0.100f, 0.055f}, {0.0f, 0.0f, 0.0f}, item_iron);

  const auto appendEquippedPart = [&](const std::string &item_id, const char *name,
                                      const MeshPrimitive primitive, const Vec3 local_position,
                                      const Vec3 scale, const Vec3 local_rotation,
                                      const Material &part_material) {
    const std::size_t index =
        appendScenery(name, primitive, {0.0f, -20.0f, 0.0f}, {0.001f, 0.001f, 0.001f},
                      local_rotation, part_material);
    equipped_item_parts_.push_back({item_id, index, local_position, local_rotation, scale});
  };
  appendEquippedPart("pickaxe", "Equipped pickaxe handle", MeshPrimitive::Box, {0.0f, 0.17f, 0.0f},
                     {0.032f, 0.38f, 0.032f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendEquippedPart("pickaxe", "Equipped pickaxe head", MeshPrimitive::Box, {0.0f, 0.36f, 0.0f},
                     {0.31f, 0.052f, 0.056f}, {0.0f, radians(90.0f), 0.0f}, item_iron);
  appendEquippedPart("torch", "Equipped torch wrapped handle", MeshPrimitive::Box,
                     {0.0f, 0.16f, 0.0f}, {0.040f, 0.34f, 0.040f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendEquippedPart("torch", "Equipped torch ember core", MeshPrimitive::Crystal,
                     {0.0f, 0.34f, 0.0f}, {0.086f, 0.126f, 0.086f},
                     {radians(0.0f), radians(0.0f), radians(0.0f)}, torch_flame_material);
  appendEquippedPart("fishing_rod", "Equipped fishing rod blank", MeshPrimitive::Box,
                     {0.0f, 0.30f, 0.0f}, {0.016f, 0.68f, 0.016f},
                     {radians(-5.0f), 0.0f, radians(7.0f)}, fishing_rod_material);
  appendEquippedPart("fishing_rod", "Equipped fishing reel", MeshPrimitive::Sphere,
                     {0.048f, 0.12f, 0.0f}, {0.070f, 0.070f, 0.038f}, {0.0f, 0.0f, 0.0f},
                     item_iron);
  for (std::size_t i = 0; i < 8u; ++i) {
    const std::size_t index =
        appendScenery("Equipped torch fire particle", MeshPrimitive::Crystal, {0.0f, -20.0f, 0.0f},
                      {0.001f, 0.001f, 0.001f}, {}, torch_flame_material);
    torch_particle_visuals_.push_back({index});
  }
  for (std::size_t i = 0; i < kMiningFractureShardVisualPoolSize; ++i) {
    const std::size_t index =
        appendScenery("Mesh-cut mining fragment", MeshPrimitive::Rock, {0.0f, -24.0f, 0.0f},
                      {0.001f, 0.001f, 0.001f}, {}, cave_talus);
    scene_.objects()[index].camera_occlusion_fade = false;
    mining_fracture_shards_.push_back({.object_index = index});
  }

  support_surfaces_.addBox({chest_base_ + Vec3{0.0f, 0.25f, 0.0f}, {0.40f, 0.25f, 0.30f}});
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    appendGeneratedStructuralScenery(portal.name, gothicUnderpassMesh(), portal.position,
                                     portal.scale, {0.0f, 0.0f, 0.0f}, dark_masonry);
  }

  support_surfaces_.addMesh(
      {terrainTransitionMesh(), {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.35f});
  decorative_ground_surfaces.addMesh(
      {terrainTransitionMesh(), {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.35f});
  appendGroundUnderlay("Courtyard pond hardscape underlay", pondHardscapeUnderlayMesh(),
                       {pond_center_.x, -0.042f, pond_center_.z}, hardscape_substrate);
  support_surfaces_.addMesh(
      {moundMesh(), {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  decorative_ground_surfaces.addMesh(
      {moundMesh(), {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  support_surfaces_.addMesh({pondHardscapeSubstrateMesh(),
                             {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                             0.18f});
  decorative_ground_surfaces.addMesh({pondHardscapeSubstrateMesh(),
                                      {pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                                      0.18f});
  support_surfaces_.addMesh({innerPondTransitionMesh(),
                             {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                             0.35f});
  decorative_ground_surfaces.addMesh({innerPondTransitionMesh(),
                                      {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                                      0.35f});
  appendGroundUnderlay("Inner pond hardscape underlay", innerPondHardscapeUnderlayMesh(),
                       {inner_pond_center_.x, -0.044f, inner_pond_center_.z}, hardscape_substrate);
  support_surfaces_.addMesh(
      {innerPondMoundMesh(), {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  decorative_ground_surfaces.addMesh(
      {innerPondMoundMesh(), {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  support_surfaces_.addMesh({innerPondHardscapeSubstrateMesh(),
                             {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                             0.18f});
  decorative_ground_surfaces.addMesh({innerPondHardscapeSubstrateMesh(),
                                      {inner_pond_center_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                                      0.18f});

  const Vec3 bird_grove_center{bird_grove_clip.center.x, 0.0f, bird_grove_clip.center.y};
  appendGeneratedScenery("Bird grove feathered transition", birdGroveTransitionMesh(),
                         bird_grove_center, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         terrain_transition);
  support_surfaces_.addMesh({birdGroveTransitionMesh(),
                             {bird_grove_center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                             0.32f});
  decorative_ground_surfaces.addMesh({birdGroveTransitionMesh(),
                                      {bird_grove_center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                                      0.32f});
  appendGeneratedScenery("Bird grove grass island", birdGroveGroundMesh(), bird_grove_center,
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, grass_soil);
  support_surfaces_.addMesh(
      {birdGroveGroundMesh(), {bird_grove_center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  decorative_ground_surfaces.addMesh(
      {birdGroveGroundMesh(), {bird_grove_center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, 0.25f});
  const TerrainSurfaceSample bird_tree_ground =
      decorative_ground_surfaces.sample(Vec2{bird_grove_center.x, bird_grove_center.z});
  const Vec3 bird_tree_base{bird_grove_center.x,
                            (bird_tree_ground.valid ? bird_tree_ground.height : 0.0f) + 0.010f,
                            bird_grove_center.z};
  castle_bird_nest_position_ = bird_tree_base + Vec3{0.0f, 3.56f, 0.0f};
  castle_bird_flight_center_ = {0.0f, castle_bird_nest_position_.y + 0.32f, -15.80f};

  const auto groundDrapeSurface = [&decorative_ground_surfaces](const Vec2 position) {
    return decorative_ground_surfaces.sample(
        {.world_position = position, .reference_y = 0.08f, .max_above = 0.54f, .max_below = 1.20f});
  };
  const MeshSurfaceSampler terrainOnlyDrapeSurface = [this](const Vec2 position) {
    return sampleTerrain(terrain_, position);
  };
  const GrassSurfaceSampler decorativeGrassSurface = groundDrapeSurface;
  const GrassSurfaceSampler terrainGrassSurface = terrainOnlyDrapeSurface;
  const auto appendPathShouldersOn = [&](const char *name, const PathRibbonMeshSpec &path_spec,
                                         const float width, const float height,
                                         const MeshSurfaceSampler &surface_sampler,
                                         const float surface_offset = 0.006f) {
    const std::shared_ptr<const CpuMesh> shoulder_mesh = makeSharedMesh(drapeMeshToSurface(
        makePathShoulderMesh(pathShoulderSpec(path_spec, width, height)), surface_sampler,
        {.surface_offset = surface_offset,
         .raise_only = false,
         .preserve_vertical_offset = true,
         .reference_y = 0.0f}));
    appendGeneratedScenery(name, shoulder_mesh, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f}, grass_soil);
    support_surfaces_.addMesh({shoulder_mesh, {}, 0.32f});
  };
  const auto appendPathShoulders = [&](const char *name, const PathRibbonMeshSpec &path_spec,
                                       const float width, const float height) {
    appendPathShouldersOn(name, path_spec, width, height, groundDrapeSurface);
  };
  const auto appendPathRouteShouldersOn =
      [&](const char *name, const std::vector<PathRibbonMeshSpec> &path_specs, const float width,
          const float height, const MeshSurfaceSampler &surface_sampler,
          const float surface_offset = 0.006f) {
        PathShoulderRouteMeshSpec route_spec;
        route_spec.segments.reserve(path_specs.size());
        for (const PathRibbonMeshSpec &path_spec : path_specs) {
          route_spec.segments.push_back(pathShoulderSpec(path_spec, width, height));
        }
        const std::shared_ptr<const CpuMesh> shoulder_mesh = makeSharedMesh(
            drapeMeshToSurface(makePathRouteShoulderMesh(std::move(route_spec)), surface_sampler,
                               {.surface_offset = surface_offset,
                                .raise_only = false,
                                .preserve_vertical_offset = true,
                                .reference_y = 0.0f}));
        appendGeneratedScenery(name, shoulder_mesh, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                               {0.0f, 0.0f, 0.0f}, grass_soil);
        support_surfaces_.addMesh({shoulder_mesh, {}, 0.32f});
      };
  const auto appendSoilPathOn = [&](const char *name, CpuMesh path_mesh,
                                    const MeshSurfaceSampler &surface_sampler,
                                    const float surface_offset = 0.012f) {
    const std::shared_ptr<const CpuMesh> draped_path =
        makeSharedMesh(drapeMeshToSurface(std::move(path_mesh), surface_sampler,
                                          {.surface_offset = surface_offset, .raise_only = false}));
    appendGeneratedScenery(name, draped_path, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f}, soil_path);
    support_surfaces_.addMesh({draped_path, {}, 0.40f});
    return draped_path;
  };
  const auto appendSoilPath = [&](const char *name, CpuMesh path_mesh) {
    return appendSoilPathOn(name, std::move(path_mesh), groundDrapeSurface);
  };
  const auto appendGrassFieldOn = [&](const char *name, GrassFieldScatterSpec scatter_spec,
                                      const Material &field_material,
                                      const GrassSurfaceSampler &surface_sampler) {
    const std::vector<GrassBladeAnchor> anchors =
        scatterGrassFieldAnchors(scatter_spec, surface_sampler);
    CpuMesh field_mesh = makeGrassFieldMesh(
        anchors, {.blade_segments = 4,
                  .double_sided = true,
                  .root_ao = 0.58f,
                  .mid_ao = 0.84f,
                  .tip_ao = 1.0f,
                  .width_taper = 1.22f,
                  .lateral_sway = 0.17f});
    if (field_mesh.vertices.empty() || field_mesh.indices.empty()) {
      return scene_.objects().size();
    }
    return appendGeneratedScenery(name, makeSharedMesh(std::move(field_mesh)), {0.0f, 0.0f, 0.0f},
                                  {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, field_material);
  };
  const auto appendGrassField = [&](const char *name, GrassFieldScatterSpec scatter_spec,
                                    const Material &field_material) {
    return appendGrassFieldOn(name, std::move(scatter_spec), field_material, decorativeGrassSurface);
  };

  const std::vector<PathRibbonMeshSpec> cave_path_specs = caveApproachPathSpecs();
  const std::vector<PathRibbonMeshSpec> abandoned_house_path_specs =
      abandonedGothicHousePathSpecs();
  appendPathRouteShouldersOn("Mounded exterior verge to cave", cave_path_specs, 0.56f, 0.088f,
                             terrainOnlyDrapeSurface, 0.034f);
  appendSoilPathOn("Curving exterior soil path to cave",
                   makePathRouteRibbonMesh({.segments = cave_path_specs}), terrainOnlyDrapeSurface,
                   0.074f);
  appendPathRouteShouldersOn("Mounded exterior verge to gothic stone house",
                             abandoned_house_path_specs, 0.46f, 0.076f, terrainOnlyDrapeSurface,
                             0.030f);
  appendSoilPathOn("Right branch soil path to gothic stone house",
                   makePathRouteRibbonMesh({.segments = abandoned_house_path_specs}),
                   terrainOnlyDrapeSurface, 0.072f);
  std::vector<PathRibbonMeshSpec> central_grass_paths = cave_path_specs;
  central_grass_paths.insert(central_grass_paths.end(), abandoned_house_path_specs.begin(),
                             abandoned_house_path_specs.end());
  central_grass_paths.push_back(castleUnderpassPathSpec());
  central_grass_paths.push_back(castleNestPathSpec());
  central_grass_paths.push_back(naturePathSpec(wetland_clips.front()));
  appendGrassFieldOn(
      "Engine central terrain grass field",
      {.min = {-14.5f, -12.5f},
       .max = {15.5f, 14.5f},
       .seed = kLumenCaveSeed + 2401u,
       .target_blades = 3400,
       .max_blades = 3400,
       .min_spacing = 0.058f,
       .surface_offset = 0.018f,
       .min_height = 0.14f,
       .max_height = 0.38f,
       .min_width = 0.009f,
       .max_width = 0.024f,
       .max_bend = 0.104f,
       .max_lean = 0.046f,
       .density_noise_scale = 0.66f,
       .density_noise_contrast = 0.36f,
       .min_surface_normal_y = 0.54f,
       .preferred_surface_normal_y = 0.86f,
       .accepts_position =
           [central_grass_paths, terrain_placement, this](const Vec2 position) {
             const TerrainSurfaceSample sample = sampleTerrain(terrain_, position);
             if (!sample.valid ||
                 terrain_placement.rejectsPoint({position.x, sample.height, position.y})) {
               return false;
             }
             return length(position) < 18.0f &&
                    distanceToPathRoute(central_grass_paths, position) > 0.62f;
           }},
      grass_blades, terrainGrassSurface);

  const auto appendGothicStoneHouse = [&]() {
    if (abandoned_house_path_specs.empty()) {
      return;
    }

    const PathRibbonMeshSpec &approach = abandoned_house_path_specs.back();
    Vec3 forward = evaluatePathRibbonTangent(approach, 1.0f);
    forward.y = 0.0f;
    if (length(forward) <= 0.0001f) {
      forward = {0.0f, 0.0f, 1.0f};
    }
    forward = normalize(forward);
    Vec3 side = normalize(cross({0.0f, 1.0f, 0.0f}, forward));
    if (length(side) <= 0.0001f) {
      side = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 doorstep = evaluatePathRibbonCenter(approach, 1.0f);
    Vec3 base = doorstep + forward * 3.05f;
    const TerrainSurfaceSample ground = terrainOnlyDrapeSurface({base.x, base.z});
    base.y = (ground.valid ? ground.height : doorstep.y) + 0.035f;
    const float yaw = std::atan2(forward.x, forward.z);
    const Vec3 house_rotation{0.0f, yaw, 0.0f};

    Material gothic_stone = cave_mouth_stone;
    gothic_stone.base_color = mixColor(gothic_stone.base_color, {0.22f, 0.23f, 0.21f}, 0.46f);
    gothic_stone.edge_wear = 0.34f;
    gothic_stone.ambient_occlusion = 0.86f;
    gothic_stone.camera_occlusion = CameraOcclusionPolicy::Solid;
    Material gothic_dark_stone = gothic_stone;
    gothic_dark_stone.base_color = mixColor(gothic_dark_stone.base_color, {0.10f, 0.105f, 0.10f},
                                            0.36f);
    gothic_dark_stone.pattern_depth = 0.12f;
    Material gothic_timber = sign_wood;
    gothic_timber.base_color = mixColor(gothic_timber.base_color, {0.18f, 0.10f, 0.055f}, 0.42f);
    gothic_timber.surface_pattern = SurfacePattern::PaintedWood;
    gothic_timber.pattern_scale = {5.4f, 13.0f};
    gothic_timber.pattern_depth = 0.050f;
    gothic_timber.detail_strength = 0.58f;
    gothic_timber.ambient_occlusion = 0.84f;
    gothic_timber.camera_occlusion = CameraOcclusionPolicy::Solid;
    Material warm_glass = warm_window;
    warm_glass.emission_strength = 0.42f;
    warm_glass.opacity = 0.72f;
    warm_glass.alpha_mode = MaterialAlphaMode::Blend;
    warm_glass.depth_write = MaterialDepthWrite::Disabled;
    warm_glass.camera_occlusion = CameraOcclusionPolicy::Solid;

    const auto local = [&](const float x, const float y, const float z) {
      return base + side * x + Vec3{0.0f, y, 0.0f} + forward * z;
    };
    const auto worldHalfExtents = [&](const Vec3 scale) {
      const Vec3 half = scale * 0.5f;
      return Vec3{std::abs(side.x) * half.x + std::abs(forward.x) * half.z,
                  half.y,
                  std::abs(side.z) * half.x + std::abs(forward.z) * half.z};
    };
    const auto addBoxSupport = [&](const Vec3 center, const Vec3 scale) {
      support_surfaces_.addBox({center, worldHalfExtents(scale)});
    };
    const auto addBoxCollision = [&](const Vec3 center, const Vec3 scale) {
      scenery_collision_boxes_.push_back({center, worldHalfExtents(scale)});
    };
    const auto appendHouseBox = [&](const char *name, const Vec3 center, const Vec3 scale,
                                    const Vec3 rotation, const Material &house_material,
                                    const bool support, const bool collision) {
      const std::size_t index =
          enableContactShadow(appendStructuralScenery(name, MeshPrimitive::Box, center, scale,
                                                      rotation, house_material),
                              0.22f, 0.86f);
      if (support) {
        addBoxSupport(center, scale);
      }
      if (collision) {
        addBoxCollision(center, scale);
      }
      return index;
    };
    const auto appendHouseStone = [&](const char *name, const MeshPrimitive primitive,
                                      const Vec3 center, const Vec3 scale, const Vec3 rotation,
                                      const bool collision) {
      const std::size_t index =
          enableContactShadow(appendStructuralScenery(name, primitive, center, scale, rotation,
                                                      gothic_stone),
                              0.20f, 0.82f);
      if (collision) {
        addBoxCollision(center, scale);
      }
      return index;
    };
    const auto appendHouseBeam = [&](const char *name, const Vec3 from, const Vec3 to,
                                     const float radius) {
      const std::size_t index = enableContactShadow(appendBeam(name, from, to, radius, gothic_timber),
                                                    0.18f, 0.74f);
      return index;
    };

    appendHouseBox("Gothic stone house ground floor slabs", local(0.0f, 0.06f, 0.0f),
                   {5.30f, 0.12f, 5.80f}, house_rotation, gothic_dark_stone, true, true);
    appendHouseBox("Gothic stone house second floor slabs", local(0.0f, 1.48f, 0.18f),
                   {4.86f, 0.12f, 5.12f}, house_rotation, gothic_dark_stone, true, true);

    appendHouseBox("Gothic house rear wall lower", local(0.0f, 0.82f, 2.74f),
                   {5.34f, 1.52f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house left wall lower", local(-2.66f, 0.78f, 0.10f),
                   {0.28f, 1.46f, 5.20f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house right wall lower", local(2.66f, 0.78f, 0.10f),
                   {0.28f, 1.46f, 5.20f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house front wall lower left", local(-1.82f, 0.86f, -2.76f),
                   {1.42f, 1.56f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house front wall lower right", local(1.82f, 0.86f, -2.76f),
                   {1.42f, 1.56f, 0.28f}, house_rotation, gothic_stone, false, true);
    appendHouseStone("Gothic pointed doorway left jamb", MeshPrimitive::Pillar,
                     local(-0.66f, 0.86f, -2.86f), {0.22f, 1.55f, 0.22f}, house_rotation, true);
    appendHouseStone("Gothic pointed doorway right jamb", MeshPrimitive::Pillar,
                     local(0.66f, 0.86f, -2.86f), {0.22f, 1.55f, 0.22f}, house_rotation, true);
    appendHouseStone("Gothic pointed doorway keystone", MeshPrimitive::RuinBlock,
                     local(0.0f, 1.64f, -2.86f), {0.84f, 0.30f, 0.28f},
                     {0.0f, yaw, radians(45.0f)}, false);

    appendHouseBox("Gothic house rear wall upper", local(0.0f, 2.14f, 2.62f),
                   {5.04f, 1.20f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house left wall upper", local(-2.48f, 2.12f, 0.12f),
                   {0.24f, 1.16f, 4.82f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house right wall upper", local(2.48f, 2.12f, 0.12f),
                   {0.24f, 1.16f, 4.82f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house upper front left", local(-1.54f, 2.14f, -2.56f),
                   {1.38f, 1.18f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseBox("Gothic house upper front right", local(1.54f, 2.14f, -2.56f),
                   {1.38f, 1.18f, 0.24f}, house_rotation, gothic_stone, false, true);
    appendHouseStone("Gothic upper pointed window stone peak", MeshPrimitive::RuinBlock,
                     local(0.0f, 2.48f, -2.66f), {0.72f, 0.24f, 0.20f},
                     {0.0f, yaw, radians(45.0f)}, false);

    for (const float x : {-1.95f, 1.95f}) {
      appendHouseStone("Gothic stone side buttress", MeshPrimitive::RuinBlock,
                       local(x, 0.78f, -2.18f), {0.34f, 1.35f, 0.40f}, house_rotation, true);
      appendHouseStone("Gothic stone rear buttress", MeshPrimitive::RuinBlock,
                       local(x, 0.82f, 2.96f), {0.36f, 1.42f, 0.38f}, house_rotation, true);
      appendHouseBeam("Gothic vertical timber brace", local(x, 0.30f, -2.58f),
                      local(x, 2.68f, -2.58f), 0.060f);
    }

    appendHouseBeam("Gothic interior stair rail", local(1.86f, 0.42f, -1.82f),
                    local(0.76f, 1.68f, 0.80f), 0.044f);
    for (int i = 0; i < 7; ++i) {
      const float t = static_cast<float>(i) / 6.0f;
      const Vec3 step_center =
          local(1.88f - t * 1.12f, 0.22f + t * 1.08f, -1.82f + t * 2.62f);
      const Vec3 step_scale{0.78f, 0.12f, 0.42f};
      appendHouseBox("Gothic stone stair tread", step_center, step_scale, house_rotation,
                     gothic_dark_stone, true, true);
    }

    appendHouseBox("Gothic left slate roof plane", local(-1.12f, 3.02f, 0.0f),
                   {3.16f, 0.16f, 5.94f}, {0.0f, yaw, radians(-24.0f)}, gothic_dark_stone,
                   false, false);
    appendHouseBox("Gothic right slate roof plane", local(1.12f, 3.02f, 0.0f),
                   {3.16f, 0.16f, 5.94f}, {0.0f, yaw, radians(24.0f)}, gothic_dark_stone,
                   false, false);
    appendHouseBeam("Gothic roof ridge beam", local(0.0f, 3.38f, -2.82f),
                    local(0.0f, 3.30f, 2.88f), 0.070f);
    for (const float z : {-2.10f, -0.70f, 0.70f, 2.10f}) {
      appendHouseBeam("Gothic left roof spar", local(-2.50f, 2.58f, z),
                      local(0.0f, 3.36f, z), 0.052f);
      appendHouseBeam("Gothic right roof spar", local(2.50f, 2.58f, z),
                      local(0.0f, 3.36f, z), 0.052f);
    }

    for (const Vec3 window : {Vec3{-2.72f, 1.05f, -0.92f}, Vec3{2.72f, 1.02f, 1.02f},
                             Vec3{0.0f, 2.16f, -2.72f}, Vec3{0.0f, 2.20f, 2.76f}}) {
      appendHouseBox("Gothic narrow amber window", local(window.x, window.y, window.z),
                     {0.08f, 0.62f, 0.42f}, house_rotation, warm_glass, false, false);
    }
  };
  appendGothicStoneHouse();

  const Vec3 cave_inward = caveInwardDirection();
  const float cave_yaw = std::atan2(-cave_inward.x, -cave_inward.z);
  supply_crate_base_ = cave_entrance + Vec3{-1.34f, cave_floor_y + 0.28f, 1.18f};
  if (const TerrainSurfaceSample supply_ground =
          groundDrapeSurface({supply_crate_base_.x, supply_crate_base_.z});
      supply_ground.valid) {
    supply_crate_base_.y = supply_ground.height + 0.28f;
  }
  supply_crate_yaw_ = cave_yaw + radians(8.0f);
  appendGroundUnderlay("Cave mouth fractured talus apron",
                       makeSharedMesh(makePathJunctionMesh({.radial_segments = 128,
                                                            .rings = 7,
                                                            .radius_x = 2.25f,
                                                            .radius_z = 2.10f,
                                                            .crown_height = 0.038f,
                                                            .surface_noise = 0.026f,
                                                            .edge_irregularity = 0.18f,
                                                            .radius_x_negative = 2.60f,
                                                            .radius_x_positive = 2.85f,
                                                            .radius_z_negative = 1.58f,
                                                            .radius_z_positive = 3.85f,
                                                            .edge_skirt_depth = 0.045f})),
                       cave_entrance + Vec3{0.08f, cave_floor_y + 0.060f, 0.36f}, cave_talus);
  const std::shared_ptr<const CpuMesh> cave_portal_seal_mesh =
      makeSharedMesh(std::move(cave_complex.portal_seal_mesh));
  appendGeneratedStructuralScenery("Opaque cave portal terrain seal", cave_portal_seal_mesh,
                                   {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                   cave_talus);
  support_surfaces_.addMesh({cave_portal_seal_mesh, {}, 0.42f});
  decorative_ground_surfaces.addMesh({cave_portal_seal_mesh, {}, 0.42f});
  const std::shared_ptr<const CpuMesh> cave_portal_mesh =
      makeSharedMesh(std::move(cave_complex.portal_mesh));
  appendGeneratedStructuralScenery(
      "Natural embedded cave mouth", cave_portal_mesh,
      {cave_entrance.x, cave_floor_y + 0.015f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_portal_blend_mesh =
      makeSharedMesh(std::move(cave_complex.portal_blend_mesh));
  appendGeneratedStructuralScenery(
      "Smooth terrain blended cave overhang", cave_portal_blend_mesh,
      {cave_entrance.x, cave_floor_y - 0.010f, cave_entrance.z + 0.10f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_portal_shell_mesh =
      makeSharedMesh(std::move(cave_complex.portal_shell_mesh));
  appendGeneratedStructuralScenery(
      "Opaque recessed cave mouth liner", cave_portal_shell_mesh,
      {cave_entrance.x, cave_floor_y + 0.015f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_entrance_wall);
  const std::shared_ptr<const CpuMesh> cave_portal_formation_mesh =
      makeSharedMesh(std::move(cave_complex.portal_formation_mesh));
  appendGeneratedStructuralScenery(
      "Continuous procedural cave mouth formation", cave_portal_formation_mesh,
      {cave_entrance.x, cave_floor_y + 0.010f, cave_entrance.z + 0.05f}, {1.0f, 1.0f, 1.0f},
      {0.0f, cave_yaw, 0.0f}, cave_mouth_stone);
  const std::shared_ptr<const CpuMesh> cave_entrance_throat_mesh =
      makeSharedMesh(std::move(cave_complex.entrance_throat_mesh));
  appendGeneratedStructuralScenery("Sealed cave entrance throat", cave_entrance_throat_mesh,
                                   {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                                   cave_entrance_wall);
  support_surfaces_.addMesh({cave_entrance_throat_mesh, {}, 0.20f});
  const std::shared_ptr<const CpuMesh> cave_portal_floor_mesh =
      makeSharedMesh(std::move(cave_complex.portal_floor_mesh));
  appendGeneratedScenery("Walkable cave entrance threshold", cave_portal_floor_mesh,
                         {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({cave_portal_floor_mesh, {}, 0.46f});
  decorative_ground_surfaces.addMesh({cave_portal_floor_mesh, {}, 0.46f});
  const std::shared_ptr<const CpuMesh> cave_floor_mesh =
      makeSharedMesh(std::move(cave_complex.floor_mesh));
  appendGeneratedScenery("Walkable packed cave floor", cave_floor_mesh, {0.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({cave_floor_mesh, {}, 0.36f});
  decorative_ground_surfaces.addMesh({cave_floor_mesh, {}, 0.36f});
  const std::shared_ptr<const CpuMesh> deep_cave_floor_mesh =
      makeSharedMesh(std::move(deep_cave_complex.floor_mesh));
  appendGeneratedScenery("Walkable deep cave floor", deep_cave_floor_mesh, {0.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, cave_floor);
  support_surfaces_.addMesh({deep_cave_floor_mesh, {}, 0.36f});
  decorative_ground_surfaces.addMesh({deep_cave_floor_mesh, {}, 0.36f});
  std::size_t cave_chunk_index = 0;
  for (CpuMesh &chunk : cave_complex.tunnel_chunks) {
    const Material &chunk_material = cave_chunk_index < 2u ? cave_entrance_wall : cave_wall;
    const std::size_t chunk_index = appendGeneratedStructuralScenery(
        "Authored cave interior", makeSharedMesh(std::move(chunk)), {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, chunk_material);
    scene_.objects()[chunk_index].viewer_cull_volume = cave_viewer_cull_volume_;
    ++cave_chunk_index;
  }
  for (CpuMesh &chunk : deep_cave_complex.tunnel_chunks) {
    appendGeneratedStructuralScenery("Authored deep cave interior", makeSharedMesh(std::move(chunk)),
                                     {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                     {0.0f, 0.0f, 0.0f}, cave_wall);
  }
  for (const AuthoredCaveSection &section : cave_sections_) {
    for (const CaveWallFixturePlacement &fixture : section.wall_fixtures) {
      appendIndustrialWallLight(fixture, industrial_light_metal, industrial_red_lens);
    }
    for (const CaveWallFixturePlacement &fixture : section.secondary_wall_fixtures) {
      appendIndustrialWallLight(fixture, industrial_light_metal, industrial_red_lens);
    }
  }

  const auto appendCaveFeatures = [&](const std::vector<CaveFeaturePlacement> &features,
                                      const CaveFeatureProfile &profile) {
    const float mineral_cutoff = 1.0f - std::clamp(profile.mineral_fraction, 0.0f, 1.0f);
    for (const CaveFeaturePlacement &feature : features) {
      const bool mineral_accent = feature.mineral >= mineral_cutoff;
      const Material &feature_material = mineral_accent ? cave_mouth_stone : cave_calcite;
      constexpr MeshPrimitive primitive = MeshPrimitive::Rock;
      Vec3 position = feature.position;
      Vec3 rotation{};
      Vec3 scale = feature.scale;
      const char *name = "Cave calcite formation";
      switch (feature.kind) {
      case CaveFeatureKind::Stalactite:
        name = mineral_accent ? "Cave iron-stained stalactite" : "Cave calcite stalactite";
        position = feature.position + feature.normal * (feature.scale.y * 0.88f);
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        break;
      case CaveFeatureKind::Stalagmite:
        name = mineral_accent ? "Cave iron-stained stalagmite" : "Cave calcite stalagmite";
        position = feature.position + feature.normal * (feature.scale.y * 0.88f);
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        break;
      case CaveFeatureKind::Column:
        name = mineral_accent ? "Cave iron-stained column" : "Cave calcite column";
        rotation = segmentRotation(feature.position, feature.position + feature.normal);
        scale.x *= 1.12f;
        scale.z *= 1.12f;
        break;
      case CaveFeatureKind::WallShelf:
        name = mineral_accent ? "Cave iron-stained wall shelf" : "Cave flowstone wall shelf";
        rotation = {0.0f, std::atan2(feature.normal.x, feature.normal.z), 0.0f};
        break;
      }
      keepCameraSolid(appendScenery(name, primitive, position, scale, rotation, feature_material));
    }
  };
  appendCaveFeatures(cave_complex.features, cave_spec.features);
  appendCaveFeatures(deep_cave_complex.features, deep_cave_spec.features);

  Vec3 sign_base = {cave_entrance.x - 1.78f, cave_floor_y + 0.72f, cave_entrance.z + 1.32f};
  if (const TerrainSurfaceSample sign_ground = groundDrapeSurface({sign_base.x, sign_base.z});
      sign_ground.valid) {
    sign_base.y = sign_ground.height + 0.76f;
  }
  appendScenery("Cave sign left post", MeshPrimitive::Box, sign_base + Vec3{-0.40f, -0.25f, 0.0f},
                {0.060f, 0.74f, 0.060f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendScenery("Cave sign right post", MeshPrimitive::Box, sign_base + Vec3{0.40f, -0.25f, 0.0f},
                {0.060f, 0.74f, 0.060f}, {0.0f, 0.0f, 0.0f}, sign_wood);
  appendScenery("Weathered Cave sign board", MeshPrimitive::Box, sign_base, {1.08f, 0.34f, 0.060f},
                {0.0f, 0.0f, 0.0f}, sign_wood);
  appendGeneratedScenery("Cave sign handwritten text", caveSignStrokeMesh(),
                         sign_base + Vec3{0.0f, -0.004f, 0.037f}, {0.82f, 0.82f, 0.82f},
                         {0.0f, 0.0f, 0.0f}, sign_ink);

  for (int i = 0; i < 7; ++i) {
    const float side = (i % 2 == 0) ? -1.0f : 1.0f;
    const float depth = 0.24f + static_cast<float>(i / 2) * 0.46f;
    const float spread = 1.78f + 0.22f * std::sin(static_cast<float>(i) * 1.7f);
    Vec3 rock_position = cave_entrance + Vec3{side * spread, cave_floor_y + 0.16f, 0.74f - depth};
    const TerrainSurfaceSample rock_ground =
        groundDrapeSurface(Vec2{rock_position.x, rock_position.z});
    if (rock_ground.valid) {
      rock_position.y = rock_ground.height + 0.17f;
    }
    appendScenery("Cave mouth fallen stone", MeshPrimitive::Rock, rock_position,
                  {0.19f + 0.045f * std::sin(static_cast<float>(i) * 0.8f),
                   0.12f + 0.030f * std::cos(static_cast<float>(i) * 1.1f),
                   0.18f + 0.040f * std::sin(static_cast<float>(i) * 1.3f)},
                  {0.0f, static_cast<float>(i) * 0.61f, 0.0f}, cave_mouth_stone);
  }

  const Vec2 cave_route_min = pathRouteMin(cave_path_specs) - Vec2{1.58f, 1.58f};
  const Vec2 cave_route_max = pathRouteMax(cave_path_specs) + Vec2{1.58f, 1.58f};
  const Vec2 cave_entrance_planar{cave_entrance.x, cave_entrance.z};
  const auto outsideCavePortal = [cave_entrance_planar](const Vec2 position) {
    const float dx = (position.x - cave_entrance_planar.x) / 2.18f;
    const float dz = (position.y - cave_entrance_planar.y) / 1.92f;
    return dx * dx + dz * dz > 1.0f && position.y > cave_entrance_planar.y - 0.82f;
  };
  appendGrassFieldOn(
      "Engine cave approach grass field",
      {.min = cave_route_min,
       .max = cave_route_max,
       .seed = kLumenCaveSeed + 2701u,
       .target_blades = 1780,
       .max_blades = 1780,
       .min_spacing = 0.066f,
       .surface_offset = 0.020f,
       .min_height = 0.18f,
       .max_height = 0.54f,
       .min_width = 0.010f,
       .max_width = 0.032f,
       .max_bend = 0.140f,
       .max_lean = 0.066f,
       .density_noise_scale = 0.66f,
       .density_noise_contrast = 0.42f,
       .min_surface_normal_y = 0.50f,
       .preferred_surface_normal_y = 0.83f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             const float route_distance = distanceToPathRoute(cave_path_specs, position);
             return outsideCavePortal(position) &&
                    route_distance > kCaveApproachPathWidth * 0.70f &&
                    route_distance < kCaveApproachPathWidth * 2.28f;
           }},
      grass_blades, terrainGrassSurface);
  appendGrassFieldOn(
      "Engine cave mouth grass field",
      {.min = {cave_entrance.x - 5.10f, cave_entrance.z - 1.05f},
       .max = {cave_entrance.x + 5.10f, cave_entrance.z + 5.20f},
       .seed = kLumenCaveSeed + 2809u,
       .target_blades = 1180,
       .max_blades = 1180,
       .min_spacing = 0.058f,
       .surface_offset = 0.022f,
       .min_height = 0.20f,
       .max_height = 0.66f,
       .min_width = 0.011f,
       .max_width = 0.034f,
       .max_bend = 0.160f,
       .max_lean = 0.072f,
       .density_noise_scale = 0.86f,
       .density_noise_contrast = 0.46f,
       .min_surface_normal_y = 0.48f,
       .preferred_surface_normal_y = 0.82f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             return outsideCavePortal(position) &&
                    distanceToPathRoute(cave_path_specs, position) >
                        kCaveApproachPathWidth * 0.76f;
           }},
      grass_blades, terrainGrassSurface);
  const std::vector<GroundDetailAnchor> cave_ground_details = scatterGroundDetailAnchors(
      {.min = {cave_entrance.x - 5.35f, cave_entrance.z - 1.20f},
       .max = {cave_entrance.x + 5.35f, cave_entrance.z + 5.45f},
       .seed = kLumenCaveSeed + 2927u,
       .target_details = 260,
       .max_details = 260,
       .min_spacing = 0.165f,
       .surface_offset = 0.026f,
       .min_radius = 0.035f,
       .max_radius = 0.135f,
       .twig_fraction = 0.22f,
       .leaf_fraction = 0.32f,
       .density_noise_scale = 0.50f,
       .density_noise_contrast = 0.34f,
       .min_surface_normal_y = 0.42f,
       .preferred_surface_normal_y = 0.80f,
       .accepts_position =
           [cave_path_specs, outsideCavePortal](const Vec2 position) {
             return outsideCavePortal(position) &&
                    distanceToPathRoute(cave_path_specs, position) >
                        kCaveApproachPathWidth * 0.62f;
           }},
      terrainGrassSurface);
  appendGeneratedScenery("Engine cave mouth procedural ground detail",
                         makeSharedMesh(makeGroundDetailMesh(
                             cave_ground_details,
                             {.pebble_segments = 8,
                              .double_sided_litter = true,
                              .pebble_height = 0.46f,
                              .leaf_curl = 0.024f,
                              .twig_height = 0.022f})),
                         {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, cave_talus);

  coal_ores_.clear();
  coal_ores_.reserve(cave_complex.ore_nodes.size() + deep_cave_complex.ore_nodes.size());
  const auto appendCaveOreNodes = [&](const std::vector<CaveOreNodePlacement> &ore_nodes) {
    for (const CaveOreNodePlacement &ore : ore_nodes) {
      const float yaw = std::atan2(ore.normal.x, ore.normal.z);
      const Vec3 ore_position = ore.position + ore.normal * std::max(ore.radius * 0.16f, 0.035f);
      const Vec3 ore_scale = ore.scale * 0.92f;
      const std::size_t index =
          keepCameraSolid(appendScenery("Coal ore vein node", MeshPrimitive::Rock, ore_position,
                                        ore_scale, {0.0f, yaw, 0.0f}, coal_ore_material));
      coal_ores_.push_back({ore_position, ore.normal, ore_scale, ore.radius, kCoalOreMaxHealth,
                            kCoalOreMaxHealth, 2, index, false, 0.0f});
    }
  };
  appendCaveOreNodes(cave_complex.ore_nodes);
  appendCaveOreNodes(deep_cave_complex.ore_nodes);

  const PathRibbonMeshSpec underpass_path_spec = castleUnderpassPathSpec();
  appendPathShoulders("Mounded verge through underpass", underpass_path_spec, 0.34f, 0.052f);
  appendSoilPath("Packed soil path through underpass", *castleUnderpassPathMesh());
  {
    CpuMesh portal_blend = *castlePathPortalBlendMesh();
    const Vec3 portal = castlePathPortalPoint();
    for (Vertex &vertex : portal_blend.vertices) {
      vertex.position = vertex.position + portal;
    }
    const std::shared_ptr<const CpuMesh> draped_portal_blend =
        makeSharedMesh(drapeMeshToSurface(std::move(portal_blend), groundDrapeSurface,
                                          {.surface_offset = 0.024f, .raise_only = false}));
    appendGeneratedScenery("Packed soil path portal blend", draped_portal_blend, {0.0f, 0.0f, 0.0f},
                           {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, soil_path);
    support_surfaces_.addMesh({draped_portal_blend, {}, 0.42f});
  }

  const PathRibbonMeshSpec nest_path_spec = castleNestPathSpec();
  appendPathShoulders("Mounded verge to bird nest", nest_path_spec, 0.42f, 0.086f);
  appendSoilPath("Packed soil path to bird nest", *castleNestPathMesh());
  const GeneratedSceneryBundle bird_tree_bundle = assembleGeneratedScenery(
      {.root = {.position = bird_tree_base},
       .meshes = {GeneratedSceneryMeshPart{.name = "Bird nest tree trunk",
                                           .mesh = climbableTreeTrunkMesh(),
                                           .transform = {.rotation = {0.0f, radians(-10.0f), 0.0f},
                                                         .scale = {0.70f, 0.92f, 0.70f}},
                                           .material = tree_bark},
                  GeneratedSceneryMeshPart{.name = "Bird nest tree crown",
                                           .mesh = treeCanopyMesh(),
                                           .transform = {.position = {0.0f, 3.12f, 0.0f},
                                                         .rotation = {0.0f, radians(16.0f), 0.0f},
                                                         .scale = {0.92f, 0.78f, 0.90f}},
                                           .material = exotic_leaf}},
       .sockets = {GeneratedScenerySocket{
           .name = "nest", .transform = {.position = {0.0f, 3.56f, 0.0f}}, .radius = 0.42f}}});
  aster::appendGeneratedScenery(scene_, bird_tree_bundle, &scenery_objects_);
  appendGeneratedBeam("Bird nest support branch", fishingRodMesh(),
                      bird_tree_base + Vec3{-0.48f, 2.72f, 0.18f},
                      castle_bird_nest_position_ + Vec3{-0.08f, -0.08f, 0.02f}, 0.035f, tree_bark);
  appendGeneratedBeam("Bird nest crossing branch", fishingRodMesh(),
                      bird_tree_base + Vec3{0.52f, 2.78f, -0.14f},
                      castle_bird_nest_position_ + Vec3{0.10f, -0.09f, -0.04f}, 0.030f, tree_bark);
  appendGeneratedScenery("Interwoven branch bird nest", castleBirdNestMesh(),
                         castle_bird_nest_position_, {1.0f, 1.0f, 1.0f},
                         {0.0f, radians(-18.0f), 0.0f}, nest_twig_material);

  castle_birds_.clear();
  const auto appendBird = [&](const char *name, const std::shared_ptr<const CpuMesh> &mesh,
                              const Material &bird_material, const Vec3 home_offset,
                              const Vec3 start_offset, const Vec3 scale, const float temperament,
                              const float fear_radius, const float wing_phase_offset) {
    CastleBird bird;
    bird.state.home_position = castle_bird_nest_position_ + home_offset;
    bird.state.position = castle_bird_flight_center_ + start_offset;
    bird.state.velocity =
        normalize(Vec3{-start_offset.z, 0.24f + temperament * 0.18f, start_offset.x}) *
        (1.1f + temperament * 0.55f);
    bird.state.temperament = temperament;
    bird.scale = scale;
    bird.wing_phase_offset = wing_phase_offset;
    bird.fear_radius = fear_radius;
    bird.body_length = scale.z * 0.52f;
    bird.wing_span = scale.x * 0.72f;
    bird.object_index =
        appendGeneratedScenery(name, mesh, bird.state.position, scale, {}, bird_material);
    if (bird.object_index < scene_.objects().size()) {
      scene_.objects()[bird.object_index].casts_contact_shadow = false;
      scene_.objects()[bird.object_index].auto_contact_shadow = false;
    }
    castle_birds_.push_back(bird);
  };
  appendBird("Schalow's turaco", crestedBirdMesh(), emerald_feathers, {-0.34f, 0.24f, 0.08f},
             {-2.8f, 0.30f, 2.4f}, {0.62f, 0.62f, 0.62f}, 0.24f, 3.2f, 0.4f);
  appendBird("Ruspoli's turaco", crestedBirdMesh(), crimson_feathers, {0.22f, 0.32f, -0.10f},
             {1.7f, 0.52f, -2.8f}, {0.60f, 0.62f, 0.60f}, 0.58f, 3.7f, 1.9f);
  appendBird("Shaft-tailed whydah", longTailBirdMesh(), ink_white_feathers, {0.06f, 0.42f, 0.38f},
             {3.5f, 0.78f, 1.0f}, {0.48f, 0.50f, 0.60f}, 0.82f, 4.1f, 3.2f);
  appendBird("Curl-crested aracari", compactBirdMesh(), gold_blue_feathers, {-0.12f, 0.18f, -0.36f},
             {-0.8f, 0.18f, -3.3f}, {0.56f, 0.58f, 0.56f}, 0.43f, 3.5f, 4.6f);

  for (int side = -1; side <= 1; side += 2) {
    const float x = static_cast<float>(side) * 2.45f;
    appendScenery("Amber lamp post", MeshPrimitive::Box, {x, 0.72f, -4.15f}, {0.11f, 1.26f, 0.11f},
                  {0.0f, 0.0f, 0.0f}, weathered_iron);
    appendScenery("Amber lamp core", MeshPrimitive::Crystal, {x, 1.52f, -4.15f},
                  {0.10f, 0.22f, 0.10f}, {0.0f, 0.0f, 0.0f}, amber_lamp, 0.35f);
  }

  const PathRibbonMeshSpec pond_path_spec = naturePathSpec(wetland_clips.front());
  const std::vector<PathRibbonMeshSpec> pond_path_route{pond_path_spec};
  appendPathShoulders("Mounded verge to pond", pond_path_spec, 0.48f, 0.078f);
  appendSoilPath("Packed soil path to pond", makePathRibbonMesh(pond_path_spec));

  for (int i = 0; i < 30; ++i) {
    const float t = (static_cast<float>(i) + 0.30f) / 30.0f;
    if (t < 0.13f) {
      continue;
    }
    const float side_sign = i % 2 == 0 ? -1.0f : 1.0f;
    const float stagger = 0.34f + 0.13f * std::sin(static_cast<float>(i) * 1.77f);
    Vec3 path_position = naturePathPoint(pond_path_spec, t) +
                         naturePathSide(pond_path_spec, t) * (side_sign * stagger);
    const TerrainSurfaceSample path_ground =
        groundDrapeSurface(Vec2{path_position.x, path_position.z});
    if (path_ground.valid) {
      path_position.y = path_ground.height + 0.014f;
    }
    appendGeneratedScenery("Path edge grass tuft", grassTuftMesh(),
                           path_position + Vec3{0.0f, 0.010f, 0.0f},
                           {0.58f, 0.70f + 0.20f * std::sin(static_cast<float>(i) * 0.91f), 0.58f},
                           {0.0f, static_cast<float>(i) * 0.83f, 0.0f}, grass_blades);
  }

  appendGeneratedScenery("Feathered terrain transition", terrainTransitionMesh(),
                         pond_center_ + Vec3{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, terrain_transition);
  appendGeneratedScenery("Moss and soil pond mound", moundMesh(), pond_center_, {1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 0.0f}, grass_soil);
  appendGeneratedScenery("Courtyard pond hardscape substrate", pondHardscapeSubstrateMesh(),
                         pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, hardscape_substrate);
  appendGeneratedScenery("Courtyard pond hardscape soil apron", pondHardscapeApronMesh(),
                         pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, hardscape_dust);
  appendGeneratedScenery("Submerged courtyard pond silt bed", pondBasinMesh(),
                         {pond_center_.x, kCourtyardPondSurfaceY, pond_center_.z},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  pond_water_object_ = appendGeneratedScenery("Wind-rippled courtyard pond", pondWaterMesh(),
                                              {pond_center_.x, 0.405f, pond_center_.z},
                                              {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, pond_water);
  if (pond_water_object_ < scene_.objects().size()) {
    scene_.objects()[pond_water_object_].casts_contact_shadow = false;
    scene_.objects()[pond_water_object_].auto_contact_shadow = false;
  }
  appendGrassField(
      "Engine pond bank grass field",
      {.min = {pond_center_.x - 2.90f, pond_center_.z - 2.12f},
       .max = {pond_center_.x + 2.90f, pond_center_.z + 2.12f},
       .seed = kLumenCaveSeed + 2303u,
       .target_blades = 620,
       .max_blades = 620,
       .min_spacing = 0.060f,
       .surface_offset = 0.020f,
       .min_height = 0.18f,
       .max_height = 0.52f,
       .min_width = 0.010f,
       .max_width = 0.028f,
       .max_bend = 0.115f,
       .max_lean = 0.052f,
       .density_noise_scale = 0.84f,
       .density_noise_contrast = 0.34f,
       .min_surface_normal_y = 0.52f,
       .preferred_surface_normal_y = 0.84f,
       .accepts_position =
           [pond_path_spec, pond_path_route, this](const Vec2 position) {
             const float dx = (position.x - pond_center_.x) / std::max(kCourtyardPondRadius.x, 0.001f);
             const float dz = (position.y - pond_center_.z) / std::max(kCourtyardPondRadius.y, 0.001f);
             const float normalized = std::sqrt(dx * dx + dz * dz);
             return normalized > 0.93f && normalized < 1.34f &&
                    distanceToPathRoute(pond_path_route, position) > pond_path_spec.width * 0.62f;
           }},
      grass_blades);

  for (int i = 0; i < 26; ++i) {
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const float fill = (static_cast<float>(i) + 0.5f) / 26.0f;
    const float ring = 0.62f + std::sqrt(fill) * 0.34f;
    Vec3 tuft_position{pond_center_.x + std::cos(angle) * 2.20f * ring, 0.0f,
                       pond_center_.z + std::sin(angle) * 1.46f * ring};
    const TerrainSurfaceSample tuft_ground =
        groundDrapeSurface(Vec2{tuft_position.x, tuft_position.z});
    if (!tuft_ground.valid) {
      continue;
    }
    tuft_position.y = tuft_ground.height + 0.018f;
    appendGeneratedScenery("Pond bank grass tuft", grassTuftMesh(), tuft_position,
                           {0.78f + fill * 0.38f, 0.86f + fill * 0.28f, 0.78f + fill * 0.38f},
                           {0.0f, angle, 0.0f}, grass_blades);
  }

  for (int i = 0; i < 14; ++i) {
    const float angle = (static_cast<float>(i) / 14.0f) * kPi * 2.0f;
    const float radius_x = 1.72f + 0.18f * std::sin(static_cast<float>(i) * 1.7f);
    const float radius_z = 1.25f + 0.12f * std::cos(static_cast<float>(i) * 1.1f);
    appendScenery("Pond edge stone", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * radius_x, 0.23f,
                   pond_center_.z + std::sin(angle) * radius_z},
                  {0.13f + 0.035f * std::sin(static_cast<float>(i) * 0.9f),
                   0.070f + 0.025f * std::cos(static_cast<float>(i) * 1.4f),
                   0.10f + 0.025f * std::sin(static_cast<float>(i) * 1.2f)},
                  {0.0f, angle, 0.0f}, pond_stone);
  }

  for (int i = 0; i < 9; ++i) {
    const float angle = static_cast<float>(i) * 0.73f + 0.35f;
    const float radius = 2.02f + 0.25f * std::sin(static_cast<float>(i) * 1.21f);
    appendScenery("Raised bank soil clump", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * radius, 0.30f,
                   pond_center_.z + std::sin(angle) * radius * 0.64f},
                  {0.18f, 0.080f, 0.14f}, {0.0f, angle, 0.0f}, soil_path);
  }

  for (int i = 0; i < 6; ++i) {
    const float angle = -2.55f + static_cast<float>(i) * 0.42f;
    appendScenery("Visible shoreline stone", MeshPrimitive::Rock,
                  {pond_center_.x + std::cos(angle) * (1.62f + 0.08f * static_cast<float>(i)),
                   0.34f,
                   pond_center_.z + std::sin(angle) * (1.14f + 0.05f * static_cast<float>(i))},
                  {0.18f, 0.075f, 0.14f}, {0.0f, angle, 0.0f}, pond_stone);
  }

  for (int i = 0; i < 7; ++i) {
    const float angle = -0.92f + static_cast<float>(i) * 0.58f;
    const Vec3 plant_position{pond_center_.x + std::cos(angle) * (2.42f + 0.18f * (i % 2)),
                              0.24f + 0.025f * std::sin(static_cast<float>(i) * 1.3f),
                              pond_center_.z + std::sin(angle) * (1.78f + 0.12f * (i % 3))};
    if (insideTerrainClipEllipse({plant_position.x, plant_position.z}, wetland_clips.front())) {
      continue;
    }
    Vec3 grounded_plant_position = plant_position;
    const TerrainSurfaceSample plant_ground =
        groundDrapeSurface(Vec2{plant_position.x, plant_position.z});
    if (plant_ground.valid) {
      grounded_plant_position.y = plant_ground.height + 0.022f;
    }
    appendGeneratedScenery(
        "Exotic fishing-bank broadleaf", broadLeafPlantMesh(), grounded_plant_position,
        {0.82f + 0.08f * static_cast<float>(i % 3), 0.92f + 0.10f * static_cast<float>(i % 2),
         0.82f + 0.08f * static_cast<float>(i % 3)},
        {0.0f, angle + 0.35f, 0.0f}, exotic_leaf);
    if ((i % 2) == 0) {
      appendScenery("Exotic fishing-bank flower", MeshPrimitive::Sphere,
                    grounded_plant_position + Vec3{0.0f, 0.58f, 0.0f}, {0.055f, 0.046f, 0.055f},
                    {0.0f, angle, 0.0f}, exotic_flower);
    }
  }

  const Vec3 fishing_tree_base = pond_center_ + Vec3{-3.35f, 0.08f, 1.88f};
  appendGeneratedScenery("Small wetland biome tree trunk", climbableTreeTrunkMesh(),
                         fishing_tree_base, {0.36f, 0.54f, 0.36f}, {0.0f, radians(18.0f), 0.0f},
                         fishing_rod_material);
  appendGeneratedScenery("Small wetland biome tree canopy", treeCanopyMesh(),
                         fishing_tree_base + Vec3{0.0f, 2.05f, 0.0f}, {0.62f, 0.56f, 0.62f},
                         {0.0f, radians(18.0f), 0.0f}, exotic_leaf);

  appendGeneratedScenery("Giant climbable exotic tree trunk", climbableTreeTrunkMesh(),
                         climbable_tree_.base, {1.0f, 1.0f, 1.0f}, {0.0f, radians(-14.0f), 0.0f},
                         tree_bark);
  appendGeneratedScenery("Giant climbable exotic tree crown", treeCanopyMesh(),
                         climbable_tree_.base + Vec3{0.0f, 3.48f, 0.0f}, {1.30f, 1.10f, 1.22f},
                         {0.0f, radians(22.0f), 0.0f}, exotic_leaf);
  appendGeneratedScenery("Giant climbable exotic upper crown", treeCanopyMesh(),
                         climbable_tree_.base + Vec3{0.28f, 4.20f, -0.18f}, {0.82f, 0.74f, 0.82f},
                         {0.0f, radians(-28.0f), 0.0f}, exotic_leaf);
  for (int i = 0; i < 5; ++i) {
    const float angle = static_cast<float>(i) / 5.0f * kPi * 2.0f;
    Vec3 root_leaf_position =
        climbable_tree_.base + Vec3{std::cos(angle) * 1.05f, 0.0f, std::sin(angle) * 0.94f};
    const TerrainSurfaceSample root_leaf_ground =
        groundDrapeSurface(Vec2{root_leaf_position.x, root_leaf_position.z});
    if (root_leaf_ground.valid) {
      root_leaf_position.y = root_leaf_ground.height + 0.020f;
    } else {
      root_leaf_position.y += 0.06f;
    }
    appendGeneratedScenery("Climb tree root broadleaf", broadLeafPlantMesh(), root_leaf_position,
                           {0.74f, 0.82f, 0.74f}, {0.0f, angle, 0.0f}, exotic_leaf);
  }

  fish_.clear();
  const auto appendFish = [&](const char *name, const Vec3 center, const Vec2 swim_radius,
                              const Vec3 scale, const float phase, const float speed,
                              const Material &fish_material) {
    AquaticCreature fish;
    fish.center = center;
    fish.swim_radius = swim_radius;
    fish.scale = scale;
    fish.phase = phase;
    fish.speed = speed;
    fish.object_index = appendGeneratedScenery(name, fishMesh(), center, scale, {}, fish_material);
    if (fish.object_index < scene_.objects().size()) {
      scene_.objects()[fish.object_index].casts_contact_shadow = false;
      scene_.objects()[fish.object_index].auto_contact_shadow = false;
    }
    fish_.push_back(fish);
  };
  appendFish("Exotic teal pond fish", {pond_center_.x - 0.34f, 0.345f, pond_center_.z - 0.08f},
             {1.36f, 0.72f}, {0.92f, 0.92f, 0.92f}, 0.20f, 0.92f, fish_scale_a);
  appendFish("Exotic ember pond fish", {pond_center_.x + 0.48f, 0.330f, pond_center_.z + 0.18f},
             {1.12f, 0.58f}, {0.78f, 0.78f, 0.78f}, 2.24f, 1.18f, fish_scale_b);

  appendGeneratedScenery("Inner castle wetland transition", innerPondTransitionMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         terrain_transition);
  appendGeneratedScenery("Deep inner castle pond basin", innerPondMoundMesh(), inner_pond_center_,
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  appendGeneratedScenery("Inner pond hardscape substrate", innerPondHardscapeSubstrateMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         hardscape_substrate);
  appendGeneratedScenery("Inner pond hardscape soil apron", innerPondHardscapeApronMesh(),
                         inner_pond_center_, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},
                         hardscape_dust);
  appendGeneratedScenery("Submerged inner pond silt bed", innerPondBasinMesh(),
                         {inner_pond_center_.x, kInnerPondSurfaceY, inner_pond_center_.z},
                         {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, marsh_soil);
  inner_pond_water_object_ =
      appendGeneratedScenery("Deep inner castle pond water", innerPondWaterMesh(),
                             {inner_pond_center_.x, 0.430f, inner_pond_center_.z},
                             {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, pond_water);
  if (inner_pond_water_object_ < scene_.objects().size()) {
    scene_.objects()[inner_pond_water_object_].casts_contact_shadow = false;
    scene_.objects()[inner_pond_water_object_].auto_contact_shadow = false;
  }
  const auto findWallMountedFixture =
      [&](const Vec3 anchor, const float mount_height) -> std::optional<CaveWallFixturePlacement> {
    struct Candidate {
      float score = std::numeric_limits<float>::max();
      CaveWallFixturePlacement placement{};
    };
    std::optional<Candidate> best;
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const auto considerFace = [&](const Vec3 normal, const float span, const Vec3 center,
                                  const Vec3 half) {
      if (span < 1.20f) {
        return;
      }

      Vec3 point{};
      if (std::abs(normal.x) > 0.5f) {
        point.x = center.x + normal.x * half.x;
        point.y = std::clamp(mount_height, center.y - half.y + 0.42f, center.y + half.y - 0.42f);
        point.z = std::clamp(anchor.z, center.z - half.z + 0.24f, center.z + half.z - 0.24f);
      } else {
        point.x = std::clamp(anchor.x, center.x - half.x + 0.24f, center.x + half.x - 0.24f);
        point.y = std::clamp(mount_height, center.y - half.y + 0.42f, center.y + half.y - 0.42f);
        point.z = center.z + normal.z * half.z;
      }

      if (dot(anchor - point, normal) <= 0.0f) {
        return;
      }

      const Vec2 planar_delta{anchor.x - point.x, anchor.z - point.z};
      const float score = length(planar_delta) + std::abs(anchor.y - point.y) * 0.18f;
      if (best.has_value() && best->score <= score) {
        return;
      }

      Vec3 tangent = normalize(cross(up, normal));
      if (length(tangent) <= 0.0001f) {
        tangent = {1.0f, 0.0f, 0.0f};
      }
      const Vec3 mount_position = point;
      const Vec3 lens_position = mount_position + normal * 0.075f;
      const Vec3 light_position = lens_position + normal * 0.18f;
      best = Candidate{score,
                       {.position = mount_position,
                        .mount_position = mount_position,
                        .lens_position = lens_position,
                        .light_position = light_position,
                        .light_color = kCaveIndustrialWarmLight,
                        .normal = normal,
                        .tangent = tangent,
                        .up = up,
                        .t = 0.0f}};
    };

    for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
      if (box.half_extents.y < 0.90f) {
        continue;
      }
      const Vec3 center = castle_origin + box.center;
      const Vec3 half = box.half_extents;
      if (center.y + half.y < mount_height - 0.35f) {
        continue;
      }
      considerFace({1.0f, 0.0f, 0.0f}, half.z * 2.0f, center, half);
      considerFace({-1.0f, 0.0f, 0.0f}, half.z * 2.0f, center, half);
      considerFace({0.0f, 0.0f, 1.0f}, half.x * 2.0f, center, half);
      considerFace({0.0f, 0.0f, -1.0f}, half.x * 2.0f, center, half);
    }
    return best.has_value() ? std::optional<CaveWallFixturePlacement>{best->placement}
                            : std::nullopt;
  };

  const Vec3 pond_wall_anchor =
      inner_pond_center_ +
      Vec3{inner_pond_radius_.x * 0.12f, kInnerPondSurfaceY + 1.34f, -inner_pond_radius_.y - 2.0f};
  if (const std::optional<CaveWallFixturePlacement> pond_fixture =
          findWallMountedFixture(pond_wall_anchor, pond_wall_anchor.y)) {
    appendIndustrialWallLight(*pond_fixture, industrial_light_metal, industrial_red_lens);
    pond_accent_light_position_ = pond_fixture->light_position + pond_fixture->up * 0.20f;
    pond_accent_light_valid_ = true;
  }

  const PlacementEllipseBand inner_pond_reed_band{
      {{inner_pond_center_.x, inner_pond_center_.z}, inner_pond_radius_}, 1.015f, 1.125f};
  for (int i = 0; i < 38; ++i) {
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f))) + 0.18f;
    if (std::sin(angle) < -0.72f) {
      continue;
    }
    const float fill = (static_cast<float>(i) + 0.5f) / 38.0f;
    const float radius = 1.018f + std::sqrt(fill) * 0.095f;
    Vec3 reed_position{inner_pond_center_.x + std::cos(angle) * inner_pond_radius_.x * radius, 0.0f,
                       inner_pond_center_.z + std::sin(angle) * inner_pond_radius_.y * radius};
    if (!contains(inner_pond_reed_band, Vec2{reed_position.x, reed_position.z})) {
      continue;
    }
    const TerrainSurfaceSample reed_ground =
        groundDrapeSurface(Vec2{reed_position.x, reed_position.z});
    if (!reed_ground.valid) {
      continue;
    }
    reed_position.y = reed_ground.height + 0.018f;
    appendGeneratedScenery("Inner pond sawgrass clump", grassTuftMesh(), reed_position,
                           {0.58f + fill * 0.22f, 0.70f + fill * 0.20f, 0.58f + fill * 0.22f},
                           {0.0f, angle, 0.0f}, wetland_blades);
  }

  crocodile_objects_.clear();
  const auto appendCrocodilePart = [&](const char *name, const MeshPrimitive primitive,
                                       const Vec3 scale, const Material &part_material) {
    const std::size_t index =
        appendScenery(name, primitive, crocodile_state_.position, scale, {}, part_material);
    if (index < scene_.objects().size()) {
      scene_.objects()[index].auto_contact_shadow = false;
    }
    crocodile_objects_.push_back(index);
  };
  const auto appendGeneratedCrocodilePart = [&](const char *name,
                                                const std::shared_ptr<const CpuMesh> &mesh,
                                                const Vec3 scale, const Material &part_material) {
    const std::size_t index =
        appendGeneratedScenery(name, mesh, crocodile_state_.position, scale, {}, part_material);
    if (index < scene_.objects().size()) {
      scene_.objects()[index].auto_contact_shadow = false;
    }
    crocodile_objects_.push_back(index);
  };
  appendGeneratedCrocodilePart("Crocodile armored body", crocodileMesh(), {1.0f, 1.0f, 1.0f},
                               crocodile_hide);
  appendCrocodilePart("Crocodile jaw", MeshPrimitive::Box, {0.17f, 0.030f, 0.30f}, crocodile_belly);
  appendCrocodilePart("Crocodile left eye", MeshPrimitive::Sphere, {0.035f, 0.032f, 0.035f},
                      crocodile_eye);
  appendCrocodilePart("Crocodile right eye", MeshPrimitive::Sphere, {0.035f, 0.032f, 0.035f},
                      crocodile_eye);

  x_eye_objects_.clear();
  for (int i = 0; i < 2; ++i) {
    x_eye_objects_.push_back(appendGeneratedScenery("Player X eye overlay", eyeCrossMesh(),
                                                    {0.0f, -20.0f, 0.0f}, {0.001f, 0.001f, 0.001f},
                                                    {}, x_eye_material));
  }

  blood_particles_.clear();
  blood_particle_cursor_ = 0;
  for (int i = 0; i < 24; ++i) {
    blood_particles_.push_back(
        {{},
         {},
         0.0f,
         0.0f,
         0.030f,
         appendScenery("Blood particle", MeshPrimitive::Sphere, {0.0f, -20.0f, 0.0f},
                       {0.001f, 0.001f, 0.001f}, {}, blood_material)});
  }

  appendGeneratedBeam("Fishing rod blank", fishingRodMesh(), fishing_rod_base_, fishing_rod_tip_,
                      0.024f, fishing_rod_material);
  appendGeneratedBeam("Fishing rod fork support left", fishingRodMesh(),
                      fishing_rod_base_ + Vec3{0.16f, -0.18f, 0.03f},
                      fishing_rod_base_ + Vec3{0.30f, 0.30f, 0.08f}, 0.018f, fishing_rod_material);
  appendGeneratedBeam("Fishing rod fork support right", fishingRodMesh(),
                      fishing_rod_base_ + Vec3{0.34f, -0.18f, 0.14f},
                      fishing_rod_base_ + Vec3{0.30f, 0.30f, 0.08f}, 0.018f, fishing_rod_material);
  appendScenery("Fishing rod ground rest", MeshPrimitive::Rock,
                fishing_rod_base_ + Vec3{0.06f, -0.15f, 0.02f}, {0.16f, 0.055f, 0.12f},
                {0.0f, 0.65f, 0.0f}, pond_stone);
  appendBeam("Fishing cork handle", fishing_rod_base_ + Vec3{-0.20f, -0.08f, 0.08f},
             fishing_rod_base_ + Vec3{0.12f, 0.06f, -0.04f}, 0.052f,
             material({0.48f, 0.30f, 0.17f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.0f, 0.0f, 0.22f, 8.0f,
                      0.10f, 0.90f));
  appendScenery("Fishing reel body", MeshPrimitive::Sphere,
                fishing_rod_base_ + Vec3{0.16f, 0.02f, -0.07f}, {0.095f, 0.095f, 0.045f},
                {0.0f, 0.0f, 0.0f}, weathered_iron);

  fishing_line_objects_.clear();
  for (int i = 0; i < kFishingLineSegmentCount; ++i) {
    const std::size_t index =
        appendGeneratedScenery("Fine fishing line segment", fishingLineMesh(), fishing_rod_tip_,
                               {fishing_line_radius_, 0.20f, fishing_line_radius_},
                               {0.0f, 0.0f, 0.0f}, fishing_line_material);
    fishing_line_objects_.push_back(index);
  }
  fishing_float_object_ =
      appendScenery("Small fishing float", MeshPrimitive::Sphere, fishing_float_rest_,
                    {0.055f, 0.075f, 0.055f}, {0.0f, 0.0f, 0.0f},
                    material({0.86f, 0.78f, 0.52f}, {0.18f, 0.06f, 0.02f}, 0.34f, 0.0f, 0.04f,
                             0.08f, 6.0f, 0.04f, 1.0f));

  rebuildPhysicsWorld();
  updateSceneObjects(1.0f / 60.0f);
  invalidateSceneReports();
}

void LumenRun::rebuildPhysicsWorld() {
  physics_.clear();
  physics_.setSettings({{0.0f, -11.2f, 0.0f}, 8, 1.0f / 120.0f});

  PhysicsBodyDesc player_body;
  player_body.type = PhysicsBodyType::Dynamic;
  player_body.shape = PhysicsShapeType::Capsule;
  player_body.position = player_position_;
  player_body.half_extents = {tuning_.player_radius,
                              tuning_.player_height * 0.5f - tuning_.player_radius * 0.92f,
                              tuning_.player_radius};
  player_body.radius = tuning_.player_radius * 0.92f;
  player_body.mass = 1.0f;
  player_body.material = {0.34f, 0.02f};
  player_body.linear_damping = 0.045f;
  player_body.filter = {kPhysicsLayerPlayer, kPhysicsLayerWorld};
  player_body.allow_sleep = false;
  player_body_ = physics_.addBody(player_body);

  const float arena_extent = std::max(tuning_.playable_radius, tuning_.arena_radius * 5.8f);
  PhysicsBodyDesc floor_body;
  floor_body.type = PhysicsBodyType::Static;
  floor_body.shape = PhysicsShapeType::Box;
  floor_body.position = {0.0f, -0.66f, 0.0f};
  floor_body.half_extents = {arena_extent, 0.20f, arena_extent};
  floor_body.material = {0.82f, 0.01f};
  floor_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
  [[maybe_unused]] const PhysicsBodyHandle floor_handle = physics_.addBody(floor_body);
  pond_fluid_ = physics_.addFluidVolume(
      {pond_center_ + Vec3{0.0f, kCourtyardPondCenterOffsetY, 0.0f},
       {kCourtyardPondRadius.x, kCourtyardPondHalfDepth, kCourtyardPondRadius.y},
       kCourtyardPondSurfaceY,
       1.18f,
       6.0f,
       {0.06f, 0.0f, 0.02f},
       kPhysicsLayerPlayer});
  inner_pond_fluid_ =
      physics_.addFluidVolume({inner_pond_center_ + Vec3{0.0f, kInnerPondCenterOffsetY, 0.0f},
                               {inner_pond_radius_.x, kInnerPondHalfDepth, inner_pond_radius_.y},
                               kInnerPondSurfaceY,
                               1.22f,
                               6.8f,
                               {0.045f, 0.0f, -0.025f},
                               kPhysicsLayerPlayer});

  const auto addStaticBox = [&](const Vec3 position, const Vec3 half_extents) {
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
      return;
    }
    PhysicsBodyDesc obstacle_body;
    obstacle_body.type = PhysicsBodyType::Static;
    obstacle_body.shape = PhysicsShapeType::Box;
    obstacle_body.position = position;
    obstacle_body.half_extents = half_extents;
    obstacle_body.material = {0.68f, 0.0f};
    obstacle_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
    [[maybe_unused]] const PhysicsBodyHandle obstacle_handle = physics_.addBody(obstacle_body);
  };

  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
    addStaticBox(castle_origin + box.center, box.half_extents);
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    if (element.collidable) {
      addStaticBox(castle_origin + element.center, element.half_extents);
    }
  }
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    for (const ArchitecturalCollisionBox &box : makeGothicUnderpassCollisionBoxes()) {
      addStaticBox({portal.position.x + box.center.x * portal.scale.x,
                    portal.position.y + box.center.y * portal.scale.y,
                    portal.position.z + box.center.z * portal.scale.z},
                   {box.half_extents.x * portal.scale.x, box.half_extents.y * portal.scale.y,
                    box.half_extents.z * portal.scale.z});
    }
  }
  const float boundary = std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  addStaticBox({0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f});
  addStaticBox({0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f});
  addStaticBox({boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addStaticBox({-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addStaticBox(chest_base_ + Vec3{0.0f, 0.23f, 0.0f}, {0.30f, 0.23f, 0.22f});
  addStaticBox(climbable_tree_.base + Vec3{0.0f, climbable_tree_.height * 0.5f, 0.0f},
               {climbable_tree_.radius * 0.82f, climbable_tree_.height * 0.5f,
                climbable_tree_.radius * 0.82f});
  for (const StaticSceneryBox &box : scenery_collision_boxes_) {
    addStaticBox(box.center, box.half_extents);
  }
  for (PlacedResourceRock &rock : placed_rocks_) {
    rock.body = addPlacedRockPhysics(rock);
  }
  for (const std::shared_ptr<const CpuMesh> &cave_collision_mesh : cave_collision_meshes_) {
    if (cave_collision_mesh == nullptr) {
      continue;
    }
    PhysicsBodyDesc cave_body;
    cave_body.type = PhysicsBodyType::Static;
    cave_body.shape = PhysicsShapeType::TriangleMesh;
    cave_body.mesh = cave_collision_mesh;
    cave_body.mesh_transform = {};
    cave_body.material = {0.74f, 0.0f};
    cave_body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
    [[maybe_unused]] const PhysicsBodyHandle cave_handle = physics_.addBody(cave_body);
  }
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      addStaticBox(ore.position, ore.scale * 0.54f);
    }
  }
}

SceneCoherenceProblem LumenRun::buildSceneCoherenceProblem() const {
  SceneCoherenceProblem problem;
  problem.visual.label = "lumen visual navigation representation";
  problem.collision.label = "lumen physical navigation representation";
  problem.navigation.label = "lumen navigable route representation";

  const auto addSurfaceSample = [&](const char *label, const Vec3 position, const float radius) {
    const SceneCoherenceSample sample{label, position, radius, 1.0f};
    problem.visual.samples.push_back(sample);
    problem.collision.samples.push_back(sample);
    problem.navigation.samples.push_back(sample);
  };

  constexpr int route_sample_count = 18;
  constexpr float route_clearance = 0.18f;
  constexpr float route_max_segment_length = 1.45f;
  constexpr float route_support_tolerance = 0.24f;
  SceneMaterialContinuityField route_material;
  route_material.label = "lumen route material field";
  route_material.neighbor_radius = route_max_segment_length * 1.15f;

  const auto addRoute = [&](const char *label, const PathRibbonMeshSpec &spec) {
    SceneCoherenceRoute route;
    route.label = label;
    route.clearance = route_clearance;
    route.max_segment_length = route_max_segment_length;
    route.support_tolerance = route_support_tolerance;
    route.points.reserve(route_sample_count);
    for (int i = 0; i < route_sample_count; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(route_sample_count - 1);
      Vec3 point = evaluatePathRibbonCenter(spec, t);
      const TerrainSurfaceSample ground = support_surfaces_.sample(Vec2{point.x, point.z});
      if (ground.valid) {
        point.y = ground.height;
      }
      route.points.push_back(point);
      addSurfaceSample(label, point, spec.width * 0.5f);
      const Vec3 tangent = evaluatePathRibbonTangent(spec, t);
      problem.affordance_samples.push_back({point, tangent, tangent, 1.0f});
      route_material.samples.push_back({point, {1.0f}, 1.0f});
    }
    problem.routes.push_back(std::move(route));
  };

  addRoute("underpass path", castleUnderpassPathSpec());
  addRoute("nest path", castleNestPathSpec());
  addRoute("pond path",
           naturePathSpec(
               plazaWetlandClips(pond_center_, inner_pond_center_, inner_pond_radius_).front()));
  int cave_route_index = 0;
  for (const PathRibbonMeshSpec &segment : caveApproachPathSpecs()) {
    const std::string route_label = "cave path " + std::to_string(++cave_route_index);
    addRoute(route_label.c_str(), segment);
  }
  for (std::size_t section_index = 0; section_index < cave_sections_.size(); ++section_index) {
    const CaveTunnelProfile &tunnel = cave_sections_[section_index].tunnel;
    const std::string label =
        section_index == 0u ? "cave passage" : "deep cave passage " + std::to_string(section_index);
    addRoute(label.c_str(), {.segments = 64,
                             .width = tunnel.floor_width,
                             .width_variation = 0.0f,
                             .crown_height = tunnel.floor_crown,
                             .surface_noise = 0.0f,
                             .endpoint_taper = 0.08f,
                             .start = tunnel.start,
                             .control = tunnel.control,
                             .control_b = tunnel.control_b,
                             .end = tunnel.end});
  }
  problem.material_fields.push_back(std::move(route_material));

  const auto addSolid = [&](const char *label, const Vec3 center, const Vec3 half_extents) {
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
      return;
    }
    problem.solid_volumes.push_back({label, center, half_extents});
  };

  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
    addSolid("castle voxel collision", castle_origin + box.center, box.half_extents);
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    if (element.collidable) {
      addSolid("castle authored collision", castle_origin + element.center, element.half_extents);
    }
  }
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    for (const ArchitecturalCollisionBox &box : makeGothicUnderpassCollisionBoxes()) {
      addSolid("underpass collision",
               {portal.position.x + box.center.x * portal.scale.x,
                portal.position.y + box.center.y * portal.scale.y,
                portal.position.z + box.center.z * portal.scale.z},
               {box.half_extents.x * portal.scale.x, box.half_extents.y * portal.scale.y,
                box.half_extents.z * portal.scale.z});
    }
  }

  const float boundary = std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  addSolid("playable north boundary", {0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f});
  addSolid("playable south boundary", {0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f});
  addSolid("playable east boundary", {boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addSolid("playable west boundary", {-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addSolid("cave chest collision", chest_base_ + Vec3{0.0f, 0.25f, 0.0f}, {0.40f, 0.25f, 0.30f});
  addSolid("climbable trunk collision",
           climbable_tree_.base + Vec3{0.0f, climbable_tree_.height * 0.5f, 0.0f},
           {climbable_tree_.radius * 0.82f, climbable_tree_.height * 0.5f,
            climbable_tree_.radius * 0.82f});
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      addSolid("coal ore collision", ore.position, ore.scale * 0.54f);
    }
  }

  problem.fluid_volumes.push_back(
      {"courtyard pond volume",
       pond_center_ + Vec3{0.0f, kCourtyardPondCenterOffsetY, 0.0f},
       {kCourtyardPondRadius.x, kCourtyardPondHalfDepth, kCourtyardPondRadius.y}});
  problem.fluid_volumes.push_back(
      {"inner pond volume",
       inner_pond_center_ + Vec3{0.0f, kInnerPondCenterOffsetY, 0.0f},
       {inner_pond_radius_.x, kInnerPondHalfDepth, inner_pond_radius_.y}});

  for (const AquaticCreature &fish : fish_) {
    Vec3 sample_position = fish.center;
    if (fish.object_index < scene_.objects().size()) {
      sample_position = scene_.objects()[fish.object_index].transform.position;
    }
    problem.ecological_samples.push_back({"aquatic creature", sample_position, 0.06f, 1.0f});
  }
  if (fishing_float_object_ < scene_.objects().size()) {
    problem.ecological_samples.push_back(
        {"floating rig contact", scene_.objects()[fishing_float_object_].transform.position, 0.04f,
         1.0f});
  }
  problem.fluid_interaction_segments.push_back(
      {"fishing line", fishing_rod_tip_,
       fishing_float_object_ < scene_.objects().size()
           ? scene_.objects()[fishing_float_object_].transform.position
           : fishing_float_rest_,
       1.0f});

  SceneVisibilityProbe camera_probe;
  camera_probe.label = "playable camera visibility";
  camera_probe.camera = player_position_ + Vec3{0.0f, 1.05f, 0.0f};
  camera_probe.allowed_regions.push_back(
      {"playable interior", {0.0f, 1.0f, 0.0f}, {boundary, 3.0f, boundary}});
  camera_probe.blockers.push_back(
      {"north boundary blocker", {0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f}});
  camera_probe.blockers.push_back(
      {"south boundary blocker", {0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f}});
  camera_probe.blockers.push_back(
      {"east boundary blocker", {boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary}});
  camera_probe.blockers.push_back(
      {"west boundary blocker", {-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary}});
  camera_probe.visible_points = {{0.0f, 1.0f, boundary + 2.0f},
                                 {0.0f, 1.0f, -boundary - 2.0f},
                                 {boundary + 2.0f, 1.0f, 0.0f},
                                 {-boundary - 2.0f, 1.0f, 0.0f}};
  problem.visibility_probes.push_back(std::move(camera_probe));

  return problem;
}

std::vector<SceneTraceRule> LumenRun::sceneTraceRules() const {
  return {
      forbidTraceSymbol("no-wall-leak.camera-inside", kTraceCameraInsideWall),
      forbidTraceSymbol("no-wall-leak.outside-visible", kTraceOutsideVisible),
      forbidTraceSymbol("collision.no-mesh-penetration", kTraceMeshPenetration),
      forbidTraceSymbol("path.no-local-cut", kTracePathCut),
      requireTraceSymbolWithin("path.continuity", kTracePathVisible, kTracePathContinues,
                               kSceneTracePathContinuityHorizon),
      requireTraceSymbolSameFrame("water.fish-embedding", kTraceFishVisible, kTraceFishInsideWater),
      forbidTraceSymbol("water.no-fish-misembedded", kTraceFishMisembedded),
      forbidTraceSymbol("water.no-fish-on-surface", kTraceFishOnSurface),
      forbidTraceSymbol("props.fishing-support-on-shore", kTraceFishingSupportWet),
      requireTraceSymbolSameFrame("affordance.reward-reachable", kTraceRewardVisible,
                                  kTraceRewardReachable),
      forbidTraceSymbol("affordance.no-false-reward", kTraceFalseRewardAffordance),
      requireTraceSymbolSameFrame("affordance.threat-readable", kTraceThreatVisible,
                                  kTraceThreatReadable),
  };
}

SceneSymbolicTrace LumenRun::buildSceneSymbolicTrace() const {
  const SceneCoherenceProblem problem = buildSceneCoherenceProblem();
  SceneSymbolicTrace trace;

  const auto pushFrame = [&trace]() -> SceneTraceFrame & {
    SceneTraceFrame frame;
    frame.time_seconds = static_cast<double>(trace.frames.size()) / kSceneTraceCheckpointRate;
    trace.frames.push_back(std::move(frame));
    return trace.frames.back();
  };

  const auto supported = [this](const Vec3 point, const float tolerance) {
    const TerrainSurfaceSample support = support_surfaces_.sample(Vec2{point.x, point.z});
    return support.valid && std::abs(support.height - point.y) <= tolerance;
  };

  for (const SceneCoherenceRoute &route : problem.routes) {
    if (route.points.size() < 2u) {
      continue;
    }
    for (std::size_t i = 0u; i + 1u < route.points.size(); ++i) {
      const Vec3 point = route.points[i];
      const Vec3 next = route.points[i + 1u];
      const float support_tolerance =
          route.support_tolerance > 0.0f ? route.support_tolerance : 0.28f;
      const bool point_supported = supported(point, support_tolerance);
      const bool next_supported = supported(next, support_tolerance);
      const bool blocked = insideAnySceneVolume(problem.solid_volumes, point);
      const bool next_blocked = insideAnySceneVolume(problem.solid_volumes, next);
      const bool continuous =
          (route.max_segment_length <= 0.0f ||
           length(next - point) <= route.max_segment_length + support_tolerance) &&
          point_supported && next_supported && !blocked && !next_blocked;

      SceneTraceFrame &frame = pushFrame();
      addTraceSymbol(frame, kTracePathVisible);
      if (blocked || !point_supported) {
        addTraceSymbol(frame, kTraceBlocked);
      } else {
        addTraceSymbol(frame, kTraceWalkable);
      }
      if (blocked) {
        addTraceSymbol(frame, kTraceMeshPenetration);
      }
      addTraceSymbol(frame, continuous ? kTracePathContinues : kTracePathCut);
    }
  }

  for (const SceneVisibilityProbe &probe : problem.visibility_probes) {
    SceneTraceFrame &frame = pushFrame();
    if (insideAnySceneVolume(problem.solid_volumes, probe.camera)) {
      addTraceSymbol(frame, kTraceCameraInsideWall);
    }
    for (const Vec3 point : probe.visible_points) {
      const bool allowed = insideAnySceneVolume(probe.allowed_regions, point);
      const bool blocked = blockedByAnySceneVolume(probe.blockers, probe.camera, point);
      if (!allowed && !blocked) {
        addTraceSymbol(frame, kTraceOutsideVisible);
      }
    }
  }

  const auto fluidSurfaceForPoint = [this](const Vec3 point, float &surface_y) {
    if (insideEllipticalFootprint(point, pond_center_, kCourtyardPondRadius,
                                  kSceneTraceWaterFootprintScale)) {
      surface_y = kCourtyardPondSurfaceY;
      return true;
    }
    if (insideEllipticalFootprint(point, inner_pond_center_, inner_pond_radius_,
                                  kSceneTraceWaterFootprintScale)) {
      surface_y = kInnerPondSurfaceY;
      return true;
    }
    return false;
  };

  for (const AquaticCreature &fish : fish_) {
    Vec3 position = fish.center;
    if (fish.object_index < scene_.objects().size()) {
      position = scene_.objects()[fish.object_index].transform.position;
    }
    const bool inside_fluid = insideAnySceneVolume(problem.fluid_volumes, position);
    float surface_y = 0.0f;
    const bool has_surface = fluidSurfaceForPoint(position, surface_y);

    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceFishVisible);
    addTraceSymbol(frame, inside_fluid ? kTraceFishInsideWater : kTraceFishMisembedded);
    if (!inside_fluid && has_surface &&
        std::abs(position.y - surface_y) <= kSceneTraceFishSurfaceTolerance) {
      addTraceSymbol(frame, kTraceFishOnSurface);
    }
  }

  {
    const bool support_in_water =
        insideEllipticalFootprint(fishing_rod_base_, pond_center_, kCourtyardPondRadius, 1.0f) ||
        insideEllipticalFootprint(fishing_rod_base_, inner_pond_center_, inner_pond_radius_, 1.0f);
    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, support_in_water ? kTraceFishingSupportWet : kTraceFishingSupportDry);
  }

  for (const Shard &shard : shards_) {
    if (shard.collected) {
      continue;
    }
    const TerrainSurfaceSample support =
        support_surfaces_.sample(Vec2{shard.position.x, shard.position.z});
    const bool reachable =
        support.valid && !insideAnySceneVolume(problem.solid_volumes, shard.position);

    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceRewardVisible);
    addTraceSymbol(frame, reachable ? kTraceRewardReachable : kTraceFalseRewardAffordance);
  }

  for (const Sentinel &sentinel : sentinels_) {
    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceThreatVisible);
    if (sentinel.object_index < scene_.objects().size()) {
      addTraceSymbol(frame, kTraceThreatReadable);
    }
  }

  return trace;
}

void LumenRun::invalidateSceneReports() {
  scene_coherence_dirty_ = true;
  scene_trace_dirty_ = true;
}

void LumenRun::rebuildSceneCoherenceReport() const {
  scene_coherence_ = evaluateSceneCoherence(buildSceneCoherenceProblem());
  scene_coherence_dirty_ = false;
}

void LumenRun::rebuildSceneTraceReport() const {
  scene_trace_ = validateSceneTrace(buildSceneSymbolicTrace(), sceneTraceRules());
  scene_trace_dirty_ = false;
}

void LumenRun::applyCaveWebTraversalGates(PhysicsBody &body) {
  for (const CaveWebObstacle &web : cave_webs_) {
    if (web.broken) {
      continue;
    }
    const Vec3 normal =
        length(web.normal) > 0.0001f ? normalize(web.normal) : Vec3{0.0f, 0.0f, -1.0f};
    const Vec3 side =
        length(web.side) > 0.0001f ? normalize(web.side) : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 up = length(web.up) > 0.0001f ? normalize(web.up) : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 offset = body.position - web.center;
    const float radius_x = std::max(web.radius_x + tuning_.player_radius * 0.80f, 0.001f);
    const float radius_y = std::max(web.radius_y + playerSupportExtent() * 0.38f, 0.001f);
    const float x = dot(offset, side) / radius_x;
    const float y = dot(offset, up) / radius_y;
    if (x * x + y * y > 1.0f) {
      continue;
    }

    const float approach = dot(offset, normal);
    const float clearance =
        tuning_.player_radius + std::max(web.thickness * 0.50f, tuning_.player_radius * 0.25f);
    if (approach <= -clearance) {
      continue;
    }
    body.position = body.position - normal * (approach + clearance);
    const float forward_speed = dot(body.velocity, normal);
    if (forward_speed > 0.0f) {
      body.velocity = body.velocity - normal * forward_speed;
    }
    physics_.wakeBody(player_body_);
  }
}

void LumenRun::updatePlayerPhysics(const float dt, const Vec2 move_axis, const bool run_requested,
                                   const bool jump_requested) {
  ASTER_PROFILE_SCOPE("LumenRun::updatePlayerPhysics");
  if (!physics_.valid(player_body_)) {
    return;
  }

  const PlayerMovePlan move_plan = buildPlayerMovePlan(
      {.walk_speed = tuning_.player_speed,
       .run_multiplier = tuning_.run_multiplier,
       .response_rate = tuning_.response_rate,
       .ground_probe_distance = 0.18f,
       .max_slope_angle = radians(52.0f),
       .jump_speed = tuning_.jump_speed},
      {.move_axis = move_axis, .run_requested = run_requested, .jump_requested = jump_requested});

  CharacterMoveInput character_input = move_plan.input;
  const float cave_web_slow = caveWebSlowScaleAt(physics_.body(player_body_).position);
  if (cave_web_slow < 0.999f) {
    character_input.desired_velocity.x *= cave_web_slow;
    character_input.desired_velocity.z *= cave_web_slow;
    PhysicsBody &body = physics_.body(player_body_);
    const float damp_response = 1.0f - std::exp(-16.0f * std::max(dt, 0.0f));
    const float velocity_scale = 1.0f - (1.0f - cave_web_slow) * damp_response;
    body.velocity.x *= velocity_scale;
    body.velocity.z *= velocity_scale;
    physics_.wakeBody(player_body_);
  }
  const bool was_climbing = player_climbing_;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  const PhysicsFluidSample fluid_sample = physics_.sampleFluid(player_body_);
  const SwimMotionResult swim_motion = buildSwimMotion(
      fluid_sample, physics_.body(player_body_).velocity,
      {.desired_velocity = character_input.desired_velocity, .ascend_requested = jump_requested},
      {.activation_submersion = 0.22f,
       .full_swim_submersion = 0.62f,
       .horizontal_speed_scale = 0.64f,
       .flow_influence = 0.34f,
       .surface_clearance = 0.038f,
       .float_response = 8.0f,
       .max_upward_speed = 1.22f,
       .max_downward_speed = 0.48f,
       .ascend_speed = 1.35f});
  player_swimming_ = swim_motion.swimming;
  player_swim_blend_ = swim_motion.blend;
  if (swim_motion.swimming) {
    character_input.desired_velocity = swim_motion.desired_velocity;
    character_input.jump_requested = false;
    PhysicsBody &body = physics_.body(player_body_);
    const float response = 1.0f - std::exp(-9.0f * std::max(dt, 0.0f));
    body.velocity.y += (swim_motion.target_vertical_velocity - body.velocity.y) * response;
    physics_.wakeBody(player_body_);
  }

  ClimbMotionResult climb_motion;
  if (!swim_motion.swimming) {
    const ClimbSurfaceSample climb_surface =
        sampleClimbableCylinder(climbable_tree_, physics_.body(player_body_).position,
                                {.capture_distance = 0.42f,
                                 .character_clearance = tuning_.player_radius * 1.22f,
                                 .stick_response = 12.0f,
                                 .tangent_speed_scale = 0.46f,
                                 .ascend_speed = 1.62f,
                                 .hold_vertical_speed = 0.16f,
                                 .max_correction = 0.16f});
    climb_motion = buildClimbMotion(
        climb_surface,
        {.desired_velocity = character_input.desired_velocity,
         .engage_requested = jump_requested || (was_climbing && length(move_axis) > 0.05f),
         .ascend_requested = jump_requested},
        {.capture_distance = 0.42f,
         .character_clearance = tuning_.player_radius * 1.22f,
         .stick_response = 12.0f,
         .tangent_speed_scale = 0.46f,
         .ascend_speed = 1.62f,
         .hold_vertical_speed = 0.16f,
         .max_correction = 0.16f});
    if (climb_motion.climbing) {
      PhysicsBody &body = physics_.body(player_body_);
      body.position = body.position + climb_motion.position_correction;
      body.velocity = climb_motion.desired_velocity;
      character_input.desired_velocity = climb_motion.desired_velocity;
      character_input.jump_requested = false;
      player_climbing_ = true;
      player_climb_blend_ = climb_motion.blend;
      physics_.wakeBody(player_body_);
    }
  }

  const TerrainSurfaceQuerySampler surface_sampler = [this](const Vec3 support_position) {
    if (isSwimmableWater(support_position)) {
      return TerrainSurfaceSample{};
    }
    return sampleWorldSupport(
        {{support_position.x, support_position.z}, support_position.y, 0.22f, 1.35f});
  };
  const CharacterMoveResult character_state =
      moveCharacterOnSurface(physics_, player_body_, surface_sampler, character_input,
                             {.controller = move_plan.character_settings,
                              .support_extent_y = playerSupportExtent(),
                              .support_radius = tuning_.player_radius,
                              .support_sample_count = 10},
                             dt);
  player_grounded_ = character_state.grounded && !player_swimming_ && !player_climbing_;
  if (!player_swimming_ && !player_climbing_) {
    const CharacterMoveResult step_assist = applySurfaceStepAssist(
        physics_, player_body_, surface_sampler, character_input.desired_velocity,
        {.support_extent_y = playerSupportExtent(),
         .support_radius = tuning_.player_radius,
         .support_sample_count = 10,
         .probe_distance = tuning_.player_radius * 1.65f,
         .max_step_up = 0.34f,
         .max_step_down = 0.12f,
         .min_horizontal_speed = 0.20f,
         .skin_width = 0.010f});
    player_grounded_ = player_grounded_ || step_assist.grounded;
  }

  physics_.step(dt);
  [[maybe_unused]] const bool horizontal_collision_resolved = resolveContinuousHorizontalCollision(
      physics_, player_body_,
      {.radius = tuning_.player_radius, .collision_mask = kPhysicsLayerWorld});
  if (player_climbing_ && physics_.valid(player_body_)) {
    PhysicsBody &body = physics_.body(player_body_);
    body.velocity.y = std::max(body.velocity.y, climb_motion.desired_velocity.y);
  }
  if (!player_swimming_ && !player_climbing_) {
    const CharacterMoveResult terrain_contact =
        solveSurfaceContact(physics_, player_body_, surface_sampler,
                            {.support_extent_y = playerSupportExtent(),
                             .skin_width = 0.010f,
                             .max_correction_distance = 2.40f,
                             .support_radius = tuning_.player_radius,
                             .support_sample_count = 10});
    player_grounded_ = player_grounded_ || terrain_contact.grounded;
  }
  if (!player_swimming_ && !player_climbing_ && physics_.valid(player_body_)) {
    PhysicsBody &body = physics_.body(player_body_);
    CaveTraversalConstraint cave_constraint{};
    for (const AuthoredCaveSection &section : cave_sections_) {
      const CaveTraversalConstraint candidate =
          constrainCaveTraversalPosition(section.tunnel, body.position, tuning_.player_radius);
      if (candidate.active &&
          (!cave_constraint.active ||
           length(candidate.correction) > length(cave_constraint.correction))) {
        cave_constraint = candidate;
      }
    }
    if (cave_constraint.active && length(cave_constraint.correction) > 0.0001f) {
      const Vec3 correction_direction = normalize(cave_constraint.correction);
      const float outward_speed = dot(body.velocity, correction_direction);
      body.position = cave_constraint.corrected_position;
      if (outward_speed < 0.0f) {
        body.velocity = body.velocity - correction_direction * outward_speed;
      }
      physics_.wakeBody(player_body_);
    }
    applyCaveWebTraversalGates(body);
  }

  player_position_ = physics_.body(player_body_).position;
  player_velocity_ = physics_.body(player_body_).velocity;
  player_mouth_open_ =
      player_swimming_
          ? 0.32f
          : (player_climbing_ ? 0.18f
                              : ((!player_grounded_ || player_velocity_.y > 0.65f) ? 1.0f : 0.0f));
  if (length(move_axis) > 0.001f) {
    player_facing_yaw_ = std::atan2(move_axis.x, move_axis.y);
  }
}

void LumenRun::updateSceneObjects(const float animation_dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateSceneObjects");
  auto &objects = scene_.objects();

  const bool preview = player_preview_yaw_enabled_;
  const AvatarPose player_pose = updateAvatarAnimator(
      player_avatar_animator_, {.max_stride_amplitude = 0.62f, .vertical_bob_amplitude = 0.017f},
      {.position = avatarPosePosition(),
       .velocity = preview ? Vec3{} : player_velocity_,
       .desired_facing_yaw = preview ? player_preview_yaw_ : player_facing_yaw_,
       .has_facing_target = true,
       .max_planar_speed = tuning_.player_speed * tuning_.run_multiplier,
       .head_yaw_offset = 0.0f,
       .head_pitch_offset = 0.0f,
       .has_attention_target = player_avatar_point_enabled_ && !preview,
       .attention_target = player_avatar_point_target_,
       .pointing_enabled = player_avatar_point_enabled_ && !preview,
       .mouth_open = preview ? 0.0f : player_mouth_open_,
       .swim_blend = preview ? 0.0f : player_swim_blend_,
       .climb_blend = preview ? 0.0f : player_climb_blend_},
      animation_dt);
  player_avatar_pose_ = player_pose;
  player_avatar_pose_valid_ = true;
  player_render_position_ = player_position_;
  player_render_position_valid_ = true;
  applyAvatarPose(scene_, player_avatar_, player_avatar_instance_, player_pose);
  if (death_state_ != DeathSequenceState::Alive) {
    updateDeathVisuals();
  } else {
    restorePlayerEyeObjects();
  }
  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index < objects.size() &&
        objects[object_index].material.surface_pattern == SurfacePattern::FurFibers) {
      objects[object_index].material.emission_strength = invulnerability_ > 0.0f ? 0.12f : 0.0f;
    }
  }

  const float pulse = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 4.2f);
  for (Shard &shard : shards_) {
    RenderObject &object = objects[shard.object_index];
    if (shard.collected) {
      object.transform.position = {0.0f, -20.0f, 0.0f};
      object.transform.scale = {0.001f, 0.001f, 0.001f};
      object.material.emission_strength = 0.0f;
      continue;
    }
    object.transform.position = shard.position + Vec3{0.0f, pulse * 0.05f, 0.0f};
    object.transform.rotation = {0.0f, status_.elapsed_seconds * 0.7f + shard.position.x, 0.0f};
    object.transform.scale = {tuning_.shard_radius * 0.82f, tuning_.shard_radius * 1.55f,
                              tuning_.shard_radius * 0.82f};
    object.material.emission_strength = 0.30f + pulse * 0.22f;
  }

  for (Sentinel &sentinel : sentinels_) {
    const float angle = sentinel.phase + status_.elapsed_seconds * tuning_.sentinel_speed;
    const float weave = std::sin(angle * 1.7f) * 0.55f;
    RenderObject &object = objects[sentinel.object_index];
    object.transform.position = {
        std::cos(angle) * sentinel.orbit_radius,
        tuning_.sentinel_radius * 1.42f,
        std::sin(angle + weave) * sentinel.orbit_radius,
    };
    object.transform.rotation = {0.06f * std::sin(angle * 1.3f), -angle, 0.04f * std::cos(angle)};
    object.material.emission_strength = 0.18f + 0.08f * std::sin(status_.elapsed_seconds * 5.0f);
  }

  for (std::size_t i = 0; i < rim_objects_.size(); ++i) {
    RenderObject &object = objects[rim_objects_[i]];
    const float phase = status_.elapsed_seconds * 2.0f + static_cast<float>(i) * 0.31f;
    object.material.emission_strength = 0.02f + 0.03f * (0.5f + 0.5f * std::sin(phase));
  }

  updatePrismRelayVisuals(animation_dt);
  updateChestVisuals(animation_dt);
  updateEquipmentVisuals(animation_dt);
  updateCaveVisuals(animation_dt);
  updateCaveSkitterVisuals(animation_dt);
  updateFishingVisual();
  updateCastleBirdVisuals();
  updateCrocodileVisual();
}

void LumenRun::updateChestInteractionState() {
  ASTER_PROFILE_SCOPE("LumenRun::updateChestInteractionState");
  if (!chest_open_) {
    return;
  }
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  const float distance = length(player_position_ - chest_focus);
  if (!chest_distance_close_armed_ && distance <= kChestInteractionDistance) {
    chest_distance_close_armed_ = true;
  }
  if (chest_distance_close_armed_ && distance > kChestAutoCloseDistance) {
    closeChest();
  }
}

void LumenRun::updateChestVisuals(const float dt) {
  chest_lid_animation_.update(dt);
  auto &objects = scene_.objects();
  const auto hideObject = [&](const std::size_t index) {
    if (index >= objects.size()) {
      return;
    }
    objects[index].transform.position = {0.0f, -20.0f, 0.0f};
    objects[index].transform.scale = {0.001f, 0.001f, 0.001f};
    objects[index].material.emission_strength = 0.0f;
  };
  const auto placeLidPart = [&](const std::size_t index, const Vec3 closed_local) {
    if (index >= objects.size()) {
      return;
    }
    const float open = easeOutCubic(chest_lid_animation_.value());
    const float angle = open * radians(58.0f);
    const Vec3 hinge{0.0f, 0.39f, -0.25f};
    const Vec3 opened_local = hinge + rotateX(closed_local - hinge, -angle);
    RenderObject &object = objects[index];
    object.transform.position = chest_base_ + rotateYaw(opened_local, chest_yaw_);
    object.transform.rotation = {-angle, chest_yaw_, 0.0f};
  };

  placeLidPart(chest_lid_object_, {0.0f, 0.49f, 0.0f});
  placeLidPart(chest_lid_band_object_, {0.0f, 0.44f, 0.326f});
  placeLidPart(chest_lock_object_, {0.0f, 0.36f, 0.354f});

  const float open_visibility = easeOutCubic(chest_lid_animation_.value());
  const InteractionFocus &focus = interaction_.focus();
  for (ChestItemVisual &item : chest_items_) {
    const bool visible = item.available && open_visibility > 0.01f;
    const bool highlighted = focus.visible && focus.target_id == "item:" + item.item_id;
    for (const ChestItemPart &part : item.parts) {
      if (!visible) {
        hideObject(part.object_index);
        continue;
      }
      if (part.object_index >= objects.size()) {
        continue;
      }
      RenderObject &object = objects[part.object_index];
      const float lift =
          std::sin(status_.elapsed_seconds * 3.2f + static_cast<float>(part.object_index) * 0.17f) *
          0.010f * open_visibility;
      object.transform.position =
          chest_base_ + rotateYaw(part.local_position + Vec3{0.0f, lift, 0.0f}, chest_yaw_);
      object.transform.rotation = {part.local_rotation.x, chest_yaw_ + part.local_rotation.y,
                                   part.local_rotation.z};
      object.transform.scale = part.scale * open_visibility;
      object.material.emission_strength = part.base_emission + (highlighted ? 0.16f : 0.0f);
    }
  }
}

void LumenRun::updateEquipmentVisuals(const float dt) {
  auto &objects = scene_.objects();
  const auto hideObject = [&](const std::size_t index) {
    if (index >= objects.size()) {
      return;
    }
    objects[index].transform.position = {0.0f, -20.0f, 0.0f};
    objects[index].transform.scale = {0.001f, 0.001f, 0.001f};
  };

  const std::string equipped_id =
      equipment_.hasEquippedItem() ? equipment_.equipped().item_id : std::string{};
  Transform hand_socket{avatarPosePosition() + rotateYaw({0.27f, 0.33f, 0.11f}, player_facing_yaw_),
                        {0.0f, player_facing_yaw_, 0.0f},
                        {1.0f, 1.0f, 1.0f}};
  if (const std::optional<Transform> resolved_socket = resolveAvatarAttachmentSocket(
          player_avatar_, player_avatar_pose_, plushRightHandCarrySocket())) {
    hand_socket = *resolved_socket;
  }
  const float stride = std::clamp(player_avatar_pose_.stride_amplitude, 0.0f, 1.0f);
  const float carry_swing = std::sin(player_avatar_pose_.gait_phase) * stride;
  const Vec3 carry_rotation{radians(-12.0f) + carry_swing * 0.08f,
                            player_facing_yaw_ + radians(5.0f),
                            radians(-10.0f) + carry_swing * 0.06f};
  for (const EquippedItemPart &part : equipped_item_parts_) {
    if (part.item_id != equipped_id) {
      hideObject(part.object_index);
      continue;
    }
    if (part.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[part.object_index];
    object.transform.position =
        hand_socket.position + rotateEuler(part.local_position, carry_rotation);
    object.transform.rotation = carry_rotation + part.local_rotation;
    object.transform.scale = part.scale;
  }

  const bool torch_equipped = equipment_.isEquipped("torch");
  const Vec3 flame_position =
      hand_socket.position + rotateEuler({0.0f, 0.39f, 0.0f}, carry_rotation);
  equipped_light_ = torch_equipped ? evaluateFlickerLight({.color = {1.0f, 0.50f, 0.20f},
                                                           .intensity = 5.8f,
                                                           .amplitude = 0.22f,
                                                           .speed = 14.0f,
                                                           .source_radius = 0.82f},
                                                          flame_position, status_.elapsed_seconds)
                                   : DynamicPointLight{};

  if (!torch_equipped) {
    torch_flame_.reset();
    for (const TorchParticleVisual &visual : torch_particle_visuals_) {
      hideObject(visual.object_index);
    }
    return;
  }

  torch_flame_.update(dt, flame_position, status_.elapsed_seconds,
                      {.max_particles = torch_particle_visuals_.size(),
                       .lifetime = 0.48f,
                       .rise_speed = 0.48f,
                       .swirl_radius = 0.045f,
                       .base_size = 0.040f,
                       .hot_tint = {1.0f, 0.46f, 0.08f},
                       .cool_tint = {0.55f, 0.07f, 0.018f}});
  const std::vector<Particle> &particles = torch_flame_.particles();
  for (std::size_t i = 0; i < torch_particle_visuals_.size(); ++i) {
    const std::size_t object_index = torch_particle_visuals_[i].object_index;
    if (object_index >= objects.size()) {
      continue;
    }
    if (i >= particles.size() || !particles[i].active) {
      hideObject(object_index);
      continue;
    }
    const Particle &particle = particles[i];
    RenderObject &object = objects[object_index];
    object.transform.position = particle.position;
    object.transform.rotation = {0.0f, status_.elapsed_seconds * 1.6f + static_cast<float>(i),
                                 0.0f};
    object.transform.scale = {particle.size, particle.size * 1.55f, particle.size};
    object.material.base_color = particle.tint;
    object.material.emission_color = particle.tint;
    object.material.emission_strength = 0.66f;
  }
}

void LumenRun::activatePrismRelay() {
  prism_relay_active_ = true;
  prism_relay_charge_ =
      std::max(prism_relay_charge_, std::min(kPrismRelayIgnitedCharge + kPrismRelayRetuneKick,
                                             kPrismRelayIgnitedCharge * 1.15f));
}

void LumenRun::updatePrismRelay(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updatePrismRelay");
  const float target = prism_relay_active_ ? kPrismRelayIgnitedCharge : kPrismRelayIdleCharge;
  const float response = prism_relay_active_ ? 2.7f : 1.4f;
  const float blend = clamp(dt * response, 0.0f, 1.0f);
  prism_relay_charge_ = std::lerp(prism_relay_charge_, target, blend);
}

void LumenRun::updatePrismRelayVisuals(const float dt) {
  (void)dt;
  auto &objects = scene_.objects();
  const float charge = clamp(prism_relay_charge_, 0.0f, 1.15f);
  const float field = clamp((charge - 0.05f) / 0.95f, 0.0f, 1.0f);
  const float pulse = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 3.35f);
  const Vec3 relay_core = prismRelayFocusPosition();

  if (prism_relay_core_valid_ && prism_relay_core_object_ < objects.size()) {
    RenderObject &core = objects[prism_relay_core_object_];
    core.transform.position = relay_core + Vec3{0.0f, 0.035f * field * pulse, 0.0f};
    core.transform.rotation = {0.045f * std::sin(status_.elapsed_seconds * 1.7f),
                               status_.elapsed_seconds * (0.32f + field * 0.22f),
                               0.050f * std::cos(status_.elapsed_seconds * 1.3f)};
    const float breathe = 1.0f + field * (0.026f + 0.018f * pulse);
    core.transform.scale = {0.24f * breathe, 0.52f * breathe, 0.24f * breathe};
    core.material.emission_strength = 0.10f + charge * (0.56f + 0.20f * pulse);
  }

  for (std::size_t i = 0; i < prism_relay_ring_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_ring_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &ring = objects[object_index];
    const float direction = i == 0u ? 1.0f : -1.0f;
    ring.transform.position = relay_core;
    ring.transform.rotation =
        i == 0u
            ? Vec3{0.08f * field * std::sin(status_.elapsed_seconds * 1.1f),
                   status_.elapsed_seconds * 0.24f * direction,
                   0.05f * field * std::cos(status_.elapsed_seconds * 1.6f)}
            : Vec3{radians(90.0f) + 0.07f * field * std::sin(status_.elapsed_seconds * 1.4f),
                   status_.elapsed_seconds * 0.18f * direction,
                   radians(12.0f) + 0.04f * field * std::cos(status_.elapsed_seconds * 1.9f)};
    const float ring_breathe = 1.0f + field * (0.035f + 0.018f * pulse);
    ring.transform.scale = {ring_breathe, ring_breathe, ring_breathe};
    ring.material.opacity = 0.08f + field * 0.34f;
    ring.material.emission_strength = 0.04f + charge * (0.42f + 0.16f * pulse);
  }

  for (std::size_t i = 0; i < prism_relay_conduit_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_conduit_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &conduit = objects[object_index];
    const float phase = pulse + 0.5f * std::sin(status_.elapsed_seconds * 2.2f +
                                                static_cast<float>(i) * 1.37f);
    conduit.material.opacity = 0.045f + field * (0.22f + 0.08f * phase);
    conduit.material.emission_strength = 0.035f + charge * (0.46f + 0.18f * phase);
  }

  for (std::size_t i = 0; i < prism_relay_node_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_node_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &node = objects[object_index];
    const float phase = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 2.6f +
                                               static_cast<float>(i) * 0.91f);
    const float scale = 0.11f + field * (0.035f + 0.018f * phase);
    node.transform.scale = {scale, scale, scale};
    node.material.emission_strength = 0.08f + charge * (0.38f + 0.12f * phase);
  }
}

void LumenRun::updateCaveVisuals(const float dt) {
  auto &objects = scene_.objects();
  for (CoalOreNode &ore : coal_ores_) {
    if (ore.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[ore.object_index];
    if (ore.collected) {
      object.transform.position = {0.0f, -20.0f, 0.0f};
      object.transform.scale = {0.001f, 0.001f, 0.001f};
      object.material.emission_strength = 0.0f;
      continue;
    }
    ore.hit_flash = std::max(0.0f, ore.hit_flash - dt * 3.8f);
    const float damage = 1.0f - static_cast<float>(std::max(ore.health, 0)) /
                                    static_cast<float>(std::max(ore.max_health, 1));
    const float pulse = ore.hit_flash * 0.06f + damage * 0.035f;
    object.transform.position = ore.position + ore.normal * (ore.hit_flash * 0.018f);
    object.transform.scale = ore.scale * (1.0f + pulse);
    object.material.emission_strength = 0.055f + ore.hit_flash * 0.120f + damage * 0.030f;
    object.material.edge_wear = 0.18f + damage * 0.38f;
  }
  for (CaveWebObstacle &web : cave_webs_) {
    if (web.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[web.object_index];
    if (web.broken) {
      hideRenderObject(object);
      continue;
    }
    web.hit_flash = std::max(0.0f, web.hit_flash - dt * 4.4f);
    object.material.emission_strength = 0.035f + web.hit_flash * 0.18f;
    object.material.opacity = 0.74f + web.hit_flash * 0.10f;
  }
}

float LumenRun::caveWebSlowScaleAt(const Vec3 position) const {
  float slow_scale = 1.0f;
  for (const CaveWebObstacle &web : cave_webs_) {
    if (web.broken) {
      continue;
    }
    const Vec3 offset = position - web.center;
    const float plane_distance = std::abs(dot(offset, web.normal));
    if (plane_distance > std::max(web.thickness, 0.01f)) {
      continue;
    }
    const float radius_x = std::max(web.radius_x, 0.001f);
    const float radius_y = std::max(web.radius_y, 0.001f);
    const float x = dot(offset, web.side) / radius_x;
    const float y = dot(offset, web.up) / radius_y;
    if (x * x + y * y <= 1.0f) {
      slow_scale = std::min(slow_scale, std::clamp(web.slow_scale, 0.05f, 1.0f));
    }
  }
  return slow_scale;
}

TerrainSurfaceSample LumenRun::sampleWorldSupport(const SurfaceSupportQuery &query) const {
  return support_surfaces_.sample(query);
}

void LumenRun::updateFishingVisual() {
  auto &objects = scene_.objects();
  if (fishing_line_objects_.empty() || fishing_float_object_ >= objects.size() ||
      pond_water_object_ >= objects.size()) {
    return;
  }

  RenderObject &water = objects[pond_water_object_];
  water.transform.position.y = 0.405f + std::sin(status_.elapsed_seconds * 1.4f) * 0.009f;
  if (inner_pond_water_object_ < objects.size()) {
    RenderObject &inner_water = objects[inner_pond_water_object_];
    inner_water.transform.position.y =
        0.430f + std::sin(status_.elapsed_seconds * 1.15f + 0.60f) * 0.012f;
  }

  const auto updateRig = [&](const std::vector<std::size_t> &line_objects,
                             const std::size_t float_object, const Vec3 rod_tip,
                             const Vec3 float_rest, const float phase_offset,
                             const float sag_base) {
    if (line_objects.empty() || float_object >= objects.size()) {
      return;
    }
    const Vec3 float_position =
        float_rest + Vec3{std::sin(status_.elapsed_seconds * 1.6f + phase_offset) * 0.026f,
                          std::sin(status_.elapsed_seconds * 2.2f + phase_offset) * 0.018f,
                          std::cos(status_.elapsed_seconds * 1.3f + phase_offset) * 0.020f};
    RenderObject &bobber = objects[float_object];
    bobber.transform.position = float_position;
    bobber.transform.rotation = {0.05f * std::sin(status_.elapsed_seconds * 1.7f + phase_offset),
                                 status_.elapsed_seconds * 0.18f,
                                 0.04f * std::cos(status_.elapsed_seconds * 1.3f + phase_offset)};

    const float sag = sag_base + 0.018f * std::sin(status_.elapsed_seconds * 1.1f + phase_offset);
    const float side_sway = std::sin(status_.elapsed_seconds * 0.8f + phase_offset) * 0.018f;
    Vec3 previous = rod_tip;
    for (std::size_t i = 0; i < line_objects.size(); ++i) {
      const float t1 = static_cast<float>(i + 1u) / static_cast<float>(line_objects.size());
      const Vec3 next = lineCurvePoint(rod_tip, float_position, t1, sag, side_sway);
      const Vec3 midpoint = (previous + next) * 0.5f;
      const float segment_length = std::max(length(next - previous), 0.035f);

      RenderObject &line = objects[line_objects[i]];
      line.transform.position = midpoint;
      line.transform.rotation = segmentRotation(previous, next);
      line.transform.scale = {fishing_line_radius_, segment_length, fishing_line_radius_};
      previous = next;
    }
  };

  updateRig(fishing_line_objects_, fishing_float_object_, fishing_rod_tip_, fishing_float_rest_,
            0.0f, 0.12f);
  updateAquaticLifeVisual();
}

void LumenRun::updateAquaticLifeVisual() {
  auto &objects = scene_.objects();
  for (std::size_t i = 0; i < fish_.size(); ++i) {
    AquaticCreature &fish = fish_[i];
    if (fish.object_index >= objects.size()) {
      continue;
    }
    const float phase = fish.phase + status_.elapsed_seconds * fish.speed;
    const float wobble = std::sin(phase * 2.4f + static_cast<float>(i)) * 0.035f;
    const Vec3 position{fish.center.x + std::cos(phase) * fish.swim_radius.x,
                        fish.center.y + wobble,
                        fish.center.z + std::sin(phase * 0.86f) * fish.swim_radius.y};
    const Vec3 ahead{fish.center.x + std::cos(phase + 0.08f) * fish.swim_radius.x, fish.center.y,
                     fish.center.z + std::sin((phase + 0.08f) * 0.86f) * fish.swim_radius.y};
    RenderObject &object = objects[fish.object_index];
    object.transform.position = position;
    object.transform.rotation = {0.02f * std::sin(phase * 1.7f),
                                 std::atan2(ahead.x - position.x, ahead.z - position.z),
                                 0.10f * std::sin(phase * 2.1f)};
    object.transform.scale = fish.scale * (0.92f + 0.08f * std::sin(phase * 1.3f));
  }
}

void LumenRun::updateCastleBirds(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCastleBirds");
  if (castle_birds_.empty()) {
    return;
  }

  std::vector<AvianAgentState> flock;
  flock.reserve(castle_birds_.size());
  for (const CastleBird &bird : castle_birds_) {
    flock.push_back(bird.state);
  }

  for (CastleBird &bird : castle_birds_) {
    const AvianAgentUpdate update =
        updateAvianAgent(bird.state,
                         {.flight_center = castle_bird_flight_center_,
                          .flight_half_extents = castle_bird_flight_half_extents_,
                          .nest_position = castle_bird_nest_position_,
                          .max_speed = 1.75f + bird.state.temperament * 0.74f,
                          .max_force = 4.8f + bird.state.temperament * 1.7f,
                          .fear_radius = bird.fear_radius,
                          .nest_return_radius = 7.0f,
                          .landing_radius = 1.15f,
                          .perch_height = 0.32f,
                          .wander_strength = 0.46f + bird.state.temperament * 0.22f},
                         flock, player_position_, dt);
    bird.state.mode = update.mode;
  }
}

void LumenRun::updateCastleBirdVisuals() {
  auto &objects = scene_.objects();
  for (CastleBird &bird : castle_birds_) {
    if (bird.object_index >= objects.size()) {
      continue;
    }
    const float flap = std::sin(bird.state.wing_phase + bird.wing_phase_offset);
    const float cruise = 1.0f - bird.state.landing_blend;
    RenderObject &object = objects[bird.object_index];
    object.transform.position = bird.state.position;
    object.transform.rotation = {bird.state.pitch + flap * 0.055f * cruise, bird.state.facing_yaw,
                                 bird.state.roll + flap * 0.16f * cruise};
    object.transform.scale =
        bird.scale * (0.96f + cruise * 0.04f * std::sin(bird.state.wing_phase * 0.57f));
    object.transform.scale.y *= 1.0f - bird.state.landing_blend * 0.12f;
  }
}

void LumenRun::updateCrocodile(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCrocodile");
  if (death_state_ != DeathSequenceState::Alive) {
    return;
  }
  const AmphibiousPredatorUpdate update =
      updateAmphibiousPredator(crocodile_state_,
                               {.water_center = inner_pond_center_,
                                .water_radius = inner_pond_radius_,
                                .water_surface_y = 0.430f,
                                .body_height = 0.16f,
                                .swim_submergence = 0.04f,
                                .swim_speed = 0.82f,
                                .shore_speed = 0.42f,
                                .pursue_speed = 1.08f,
                                .aggression = 0.54f,
                                .notice_radius = 4.65f,
                                .water_pursuit_margin = 0.26f,
                                .patrol_min_radius = 0.36f,
                                .patrol_max_radius = 0.84f,
                                .shore_radius = 1.0f,
                                .strike_radius = tuning_.player_radius + 0.34f,
                                .strike_cooldown = 1.85f},
                               player_position_, dt);
  crocodile_swim_blend_ = update.swim_blend;
  if (update.strike && invulnerability_ <= 0.0f) {
    triggerPlayerDeath(update.position);
  }
}

void LumenRun::updateCaveSkitters(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCaveSkitters");
  if (death_state_ != DeathSequenceState::Alive || cave_skitters_.empty() || cave_webs_.empty()) {
    return;
  }

  const CaveWebObstacle *web = &cave_webs_.front();
  const Vec3 player_web_offset = player_position_ - web->center;
  const float player_plane_distance = std::abs(dot(player_web_offset, web->normal));
  const float player_web_x = dot(player_web_offset, web->side) / std::max(web->radius_x, 0.001f);
  const float player_web_y = dot(player_web_offset, web->up) / std::max(web->radius_y, 0.001f);
  const bool player_inside_web_volume =
      player_plane_distance <= std::max(web->thickness * 1.15f, 0.01f) &&
      player_web_x * player_web_x + player_web_y * player_web_y <= 1.0f;
  bool skitters_awake = web->broken || player_inside_web_volume;
  for (const CaveSkitter &skitter : cave_skitters_) {
    skitters_awake = skitters_awake || skitter.hit_flash > 0.0f || skitter.health < skitter.max_health;
  }

  std::vector<CaveSkitterAgentState> states;
  states.reserve(cave_skitters_.size());
  for (CaveSkitter &skitter : cave_skitters_) {
    skitter.state.dead = skitter.dead;
    states.push_back(skitter.state);
  }

  const std::vector<CaveSkitterAgentUpdate> updates = updateCaveSkitterGroup(
      states,
      {.web = {.center = web->center,
               .normal = web->normal,
               .side = web->side,
               .up = web->up,
               .radius_x = web->radius_x,
               .radius_y = web->radius_y,
               .thickness = web->thickness},
       .max_speed = 1.05f,
       .max_force = 5.4f,
       .patrol_speed_scale = 0.54f,
       .aggro_radius = skitters_awake ? 3.15f : 0.0f,
       .strike_radius = skitters_awake ? tuning_.player_radius + 0.34f : 0.0f,
       .bite_cooldown = 1.40f,
       .separation_radius = 0.34f,
       .cohesion_radius = 0.95f,
       .retreat_seconds = 0.36f,
       .surface_offset = -std::max(web->thickness * 0.64f, 0.12f),
       .depth_wander = std::max(web->thickness * 0.12f, 0.035f)},
      player_position_, dt);

  for (std::size_t i = 0; i < cave_skitters_.size(); ++i) {
    CaveSkitter &skitter = cave_skitters_[i];
    skitter.state = states[i];
    if (skitters_awake && i < updates.size() && updates[i].bite) {
      skitter.bite_flash = 1.0f;
      (void)applyPlayerDamage(kCaveSkitterBiteDamage, skitter.state.position);
    }
  }
}

void LumenRun::updateCrocodileVisual() {
  auto &objects = scene_.objects();
  if (crocodile_objects_.size() < kCrocodilePartCount) {
    return;
  }

  const Vec3 base = crocodile_state_.position;
  const float yaw = crocodile_state_.facing_yaw;
  const float swim = std::clamp(crocodile_swim_blend_, 0.0f, 1.0f);
  const float wave = std::sin(status_.elapsed_seconds * 4.1f + crocodile_state_.phase) * swim;
  const auto setPart = [&](const std::size_t part, const Vec3 offset, const Vec3 rotation,
                           const Vec3 scale) {
    if (part >= crocodile_objects_.size() || crocodile_objects_[part] >= objects.size()) {
      return;
    }
    RenderObject &object = objects[crocodile_objects_[part]];
    object.transform.position = base + rotateYaw(offset, yaw);
    object.transform.rotation = {rotation.x, yaw + rotation.y, rotation.z};
    object.transform.scale = scale;
  };

  setPart(0, {0.0f, 0.00f + wave * 0.018f, 0.0f}, {0.035f * wave, 0.0f, 0.020f * wave},
          {1.0f, 1.0f, 1.0f});
  const float jaw_open =
      crocodile_state_.mode == AmphibiousMotionMode::Strike ? 0.055f : 0.018f * swim;
  setPart(1, {0.0f, -0.064f - jaw_open, 0.86f}, {-0.14f - jaw_open * 3.2f, 0.0f, 0.0f},
          {0.17f, 0.030f, 0.30f});
  setPart(2, {-0.074f, 0.088f, 0.635f}, {0.0f, -0.035f, 0.0f}, {0.024f, 0.020f, 0.026f});
  setPart(3, {0.074f, 0.088f, 0.635f}, {0.0f, 0.035f, 0.0f}, {0.024f, 0.020f, 0.026f});
}

void LumenRun::updateCaveSkitterVisuals(const float dt) {
  auto &objects = scene_.objects();
  for (CaveSkitter &skitter : cave_skitters_) {
    if (skitter.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[skitter.object_index];
    if (skitter.dead || skitter.state.dead) {
      hideRenderObject(object);
      continue;
    }

    skitter.hit_flash = std::max(0.0f, skitter.hit_flash - dt * 4.8f);
    skitter.bite_flash = std::max(0.0f, skitter.bite_flash - dt * 5.6f);
    const float stride = length(skitter.state.velocity);
    const float gait = std::sin(skitter.state.phase * (8.0f + stride * 2.8f));
    const float alert =
        skitter.state.mode == CaveSkitterMotionMode::Pursue ||
                skitter.state.mode == CaveSkitterMotionMode::Strike
            ? 1.0f
            : 0.0f;
    const float damage = 1.0f - static_cast<float>(std::max(skitter.health, 0)) /
                                    static_cast<float>(std::max(skitter.max_health, 1));
    const Vec3 hit_recoil = skitter.last_hit_normal * (-skitter.hit_flash * 0.055f);
    object.transform.position =
        skitter.state.position + hit_recoil +
        Vec3{0.0f, 0.012f * gait - skitter.hit_flash * 0.010f, 0.0f};
    object.transform.rotation = {0.030f * gait,
                                 skitter.state.facing_yaw,
                                 0.040f * std::sin(skitter.state.phase * 5.4f) +
                                     skitter.hit_flash * 0.080f};
    const float scale = 1.0f + skitter.bite_flash * 0.06f;
    object.transform.scale = {scale * (1.0f + skitter.hit_flash * 0.035f),
                              scale * (1.0f - damage * 0.05f - skitter.hit_flash * 0.12f),
                              scale * (1.0f + skitter.hit_flash * 0.080f)};
    object.material.emission_strength =
        0.020f + alert * 0.035f + skitter.hit_flash * 0.18f + skitter.bite_flash * 0.12f;
    object.material.edge_wear = 0.10f + damage * 0.24f;
    object.material.pattern_depth = 0.060f + damage * 0.055f + skitter.hit_flash * 0.035f;
    object.material.pattern_contrast = 0.88f + damage * 0.16f + skitter.hit_flash * 0.14f;
  }
}

void LumenRun::updateBloodParticles(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateBloodParticles");
  auto &objects = scene_.objects();
  for (SceneParticle &particle : blood_particles_) {
    if (particle.object_index >= objects.size()) {
      continue;
    }
    if (particle.age >= particle.lifetime) {
      objects[particle.object_index].transform.position = {0.0f, -20.0f, 0.0f};
      objects[particle.object_index].transform.scale = {0.001f, 0.001f, 0.001f};
      continue;
    }
    particle.age += dt;
    particle.velocity.y -= 5.6f * dt;
    particle.position = particle.position + particle.velocity * dt;
    const float life =
        std::clamp(1.0f - particle.age / std::max(particle.lifetime, 0.001f), 0.0f, 1.0f);
    RenderObject &object = objects[particle.object_index];
    object.transform.position = particle.position;
    object.transform.scale = {particle.size * (0.55f + life * 0.45f),
                              particle.size * (0.38f + life * 0.62f),
                              particle.size * (0.55f + life * 0.45f)};
    object.material.emission_strength = 0.02f + life * 0.08f;
  }
}

void LumenRun::updateMiningFractureVisuals(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateMiningFractureVisuals");
  auto &objects = scene_.objects();
  for (MiningFractureShardVisual &shard : mining_fracture_shards_) {
    if (shard.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[shard.object_index];
    if (!shard.active) {
      hideRenderObject(object);
      continue;
    }

    shard.age += std::max(dt, 0.0f);
    if (shard.age >= shard.lifetime) {
      shard.active = false;
      hideRenderObject(object);
      continue;
    }

    shard.velocity.y -= 3.9f * std::max(dt, 0.0f);
    shard.position = shard.position + shard.velocity * std::max(dt, 0.0f);
    shard.rotation = shard.rotation + shard.angular_velocity * std::max(dt, 0.0f);
    const float life =
        std::clamp(1.0f - shard.age / std::max(shard.lifetime, 0.001f), 0.0f, 1.0f);
    const float scale = shard.base_scale * (0.18f + 0.82f * life);
    object.transform.position = shard.position;
    object.transform.rotation = shard.rotation;
    object.transform.scale = {scale, scale, scale};
    object.material.emission_strength = std::max(object.material.emission_strength, 0.035f * life);
  }
}

void LumenRun::spawnBloodBurst(const Vec3 center, const Vec3 impact_origin, const float intensity) {
  if (blood_particles_.empty()) {
    return;
  }
  const std::size_t count = std::clamp(
      static_cast<std::size_t>(std::ceil(std::max(intensity, 0.0f) * 8.0f)), std::size_t{3},
      blood_particles_.size());
  const Vec3 away_base = length(center - impact_origin) > 0.0001f
                             ? normalize(center - impact_origin)
                             : Vec3{0.0f, 0.45f, 1.0f};
  for (std::size_t emitted = 0; emitted < count; ++emitted) {
    const std::size_t particle_index = (blood_particle_cursor_ + emitted) % blood_particles_.size();
    SceneParticle &particle = blood_particles_[particle_index];
    const float serial = static_cast<float>(blood_particle_cursor_ + emitted);
    const float angle = serial * (kPi * (3.0f - std::sqrt(5.0f)));
    const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
    const float ring = 0.22f + 0.32f * (static_cast<float>(emitted % 5u) / 4.0f);
    particle.position = center + Vec3{0.0f, 0.08f, 0.0f};
    particle.velocity = radial * ring + away_base * (0.20f + 0.10f * intensity) +
                        Vec3{0.0f, 0.52f + 0.28f * std::sin(angle * 1.7f), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.42f + 0.18f * (static_cast<float>(emitted % 4u) / 3.0f);
    particle.size = 0.016f + 0.014f * std::clamp(intensity, 0.0f, 1.5f);
  }
  blood_particle_cursor_ += count;
}

void LumenRun::triggerPlayerDeath(const Vec3 impact_origin) {
  if (death_state_ != DeathSequenceState::Alive) {
    return;
  }
  status_.health = 0;
  --status_.lives;
  pending_defeat_ = status_.lives <= 0;
  invulnerability_ = tuning_.invulnerability_seconds + 1.2f;
  death_state_ = DeathSequenceState::Falling;
  death_timer_ = 0.0f;
  death_origin_ = player_position_;
  player_velocity_ = {};
  player_mouth_open_ = 1.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  clearAvatarPointTarget();
  if (physics_.valid(player_body_)) {
    physics_.setVelocity(player_body_, {});
  }

  for (std::size_t i = 0; i < blood_particles_.size(); ++i) {
    SceneParticle &particle = blood_particles_[i];
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const float ring = 0.35f + 0.45f * (static_cast<float>(i % 7u) / 6.0f);
    const Vec3 away = normalize(player_position_ - impact_origin);
    const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
    particle.position = player_position_ + Vec3{0.0f, 0.16f, 0.0f};
    particle.velocity =
        radial * ring + away * 0.42f + Vec3{0.0f, 1.2f + 0.35f * std::sin(angle * 1.7f), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.75f + 0.28f * (static_cast<float>(i % 5u) / 4.0f);
    particle.size = 0.025f + 0.018f * (static_cast<float>(i % 4u) / 3.0f);
  }
}

bool LumenRun::applyPlayerDamage(const int hit_points, const Vec3 impact_origin) {
  if (hit_points <= 0 || invulnerability_ > 0.0f || death_state_ != DeathSequenceState::Alive ||
      status_.defeated) {
    return false;
  }
  status_.max_health = std::max(status_.max_health, kPlayerMaxHealth);
  status_.health = std::clamp(status_.health - hit_points, 0, status_.max_health);
  if (status_.health <= 0) {
    triggerPlayerDeath(impact_origin);
    return true;
  }
  invulnerability_ = std::max(invulnerability_, 0.62f);
  player_mouth_open_ = 1.0f;
  for (std::size_t i = 0; i < blood_particles_.size(); i += 3u) {
    SceneParticle &particle = blood_particles_[i];
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const Vec3 away = length(player_position_ - impact_origin) > 0.0001f
                          ? normalize(player_position_ - impact_origin)
                          : Vec3{std::cos(angle), 0.0f, std::sin(angle)};
    particle.position = player_position_ + Vec3{0.0f, 0.18f, 0.0f};
    particle.velocity = away * (0.36f + 0.10f * static_cast<float>(i % 4u)) +
                        Vec3{0.0f, 0.62f + 0.08f * std::sin(angle), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.42f + 0.08f * static_cast<float>(i % 3u);
    particle.size = 0.018f + 0.006f * static_cast<float>(i % 2u);
  }
  return true;
}

void LumenRun::updateDeathSequence(const float dt) {
  death_timer_ += dt;
  const float fall_duration = 0.82f;
  if (death_state_ == DeathSequenceState::Falling && death_timer_ >= fall_duration) {
    death_state_ = DeathSequenceState::Blinking;
    death_timer_ = 0.0f;
    return;
  }
  if (death_state_ == DeathSequenceState::Blinking && death_timer_ >= 1.15f) {
    if (pending_defeat_) {
      status_.defeated = true;
      death_state_ = DeathSequenceState::Alive;
      return;
    }
    respawnPlayer();
  }
}

void LumenRun::updateDeathVisuals() {
  auto &objects = scene_.objects();
  const float fall_t = death_state_ == DeathSequenceState::Falling
                           ? std::clamp(death_timer_ / 0.82f, 0.0f, 1.0f)
                           : 1.0f;

  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[object_index];
    object.transform.position.y -= fall_t * 0.22f;
    object.transform.rotation.x += fall_t * 1.42f;
    object.transform.rotation.z += fall_t * 0.62f;
  }

  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < objects.size()) {
      objects[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }

  if (!eye_objects_valid_ || left_eye_object_ >= objects.size() ||
      right_eye_object_ >= objects.size()) {
    return;
  }

  const auto apply_eye_cross = [&](const std::size_t eye_index, const std::size_t overlay_slot) {
    if (overlay_slot >= x_eye_objects_.size() || x_eye_objects_[overlay_slot] >= objects.size()) {
      return;
    }
    const RenderObject &eye = objects[eye_index];
    RenderObject &overlay = objects[x_eye_objects_[overlay_slot]];
    const Vec3 forward = rotateEuler({0.0f, 0.0f, 1.0f}, eye.transform.rotation);
    const float radius = std::max({std::abs(eye.transform.scale.x), std::abs(eye.transform.scale.y),
                                   std::abs(eye.transform.scale.z)});
    overlay.transform.position = eye.transform.position + forward * (radius * 1.08f + 0.006f);
    overlay.transform.rotation = eye.transform.rotation;
    overlay.transform.scale = {std::max(radius * 3.35f, 0.044f), std::max(radius * 3.35f, 0.044f),
                               0.010f};
    overlay.camera_occlusion_fade = false;
  };
  apply_eye_cross(left_eye_object_, 0u);
  apply_eye_cross(right_eye_object_, 1u);
}

void LumenRun::restorePlayerEyeObjects() {
  auto &objects = scene_.objects();
  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < objects.size()) {
      objects[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }
  if (!eye_objects_valid_) {
    return;
  }

  const auto restore_eye = [&](const std::size_t object_index) {
    if (object_index >= objects.size()) {
      return;
    }
    for (std::size_t i = 0;
         i < player_avatar_.parts.size() && i < player_avatar_instance_.object_indices.size();
         ++i) {
      if (player_avatar_instance_.object_indices[i] != object_index) {
        continue;
      }
      const AvatarPart &part = player_avatar_.parts[i];
      RenderObject &eye = objects[object_index];
      eye.primitive = part.primitive;
      eye.custom_mesh.reset();
      eye.material = part.material;
      eye.camera_occlusion_fade = false;
      return;
    }
  };
  restore_eye(left_eye_object_);
  restore_eye(right_eye_object_);
}

void LumenRun::respawnPlayer() {
  const TerrainSurfaceSample start_ground =
      sampleWorldSupport({{0.0f, 0.0f}, playerSupportExtent(), 0.30f, 2.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  player_velocity_ = {};
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_mouth_open_ = 0.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  status_.health = status_.max_health;
  clearAvatarPointTarget();
  invulnerability_ = tuning_.invulnerability_seconds;
  death_state_ = DeathSequenceState::Alive;
  death_timer_ = 0.0f;
  pending_defeat_ = false;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }
}

void LumenRun::enforceWorldBounds() {
  const Vec2 planar{player_position_.x, player_position_.z};
  const bool invalid_position = !std::isfinite(player_position_.x) ||
                                !std::isfinite(player_position_.y) ||
                                !std::isfinite(player_position_.z);
  CaveInteriorSample cave_sample{};
  const bool inside_authored_cave =
      !invalid_position && caveSectionAt(player_position_, &cave_sample) != nullptr &&
      cave_sample.interior > 0.08f;
  const bool outside_radius =
      !inside_authored_cave &&
      length(planar) > std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  const bool below_recovery_floor =
      !inside_authored_cave && player_position_.y < tuning_.recovery_floor_y;
  if (!invalid_position && !outside_radius && !below_recovery_floor) {
    return;
  }

  const TerrainSurfaceSample start_ground =
      sampleWorldSupport({{0.0f, 0.0f}, playerSupportExtent(), 0.30f, 2.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  player_velocity_ = {};
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  clearAvatarPointTarget();
  invulnerability_ = std::max(invulnerability_, 0.35f);
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
}

bool LumenRun::isSwimmableWater(const Vec3 support_position) const {
  const float support_ceiling = playerSupportExtent() * 1.20f;
  const auto in_water = [&](const Vec3 center, const Vec2 radius, const float surface_y) {
    if (support_position.y > surface_y + support_ceiling) {
      return false;
    }
    return insideEllipticalFootprint(support_position, center, radius, 0.92f);
  };

  return in_water(pond_center_, kCourtyardPondRadius, kCourtyardPondSurfaceY) ||
         in_water(inner_pond_center_, inner_pond_radius_, kInnerPondSurfaceY);
}

PhysicsBodyHandle LumenRun::addPlacedRockPhysics(const PlacedResourceRock &rock) {
  if (rock.collision_half_extents.x <= 0.0f || rock.collision_half_extents.y <= 0.0f ||
      rock.collision_half_extents.z <= 0.0f) {
    return {};
  }
  PhysicsBodyDesc body;
  body.type = PhysicsBodyType::Static;
  body.shape = PhysicsShapeType::Box;
  body.position = rock.position;
  body.half_extents = rock.collision_half_extents;
  body.material = {0.76f, 0.0f};
  body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
  return physics_.addBody(body);
}

bool LumenRun::storeMinedResource(const ItemDefinition &definition, const int quantity) {
  if (quantity <= 0) {
    return true;
  }
  return hotbar_.addItem(definition, quantity).has_value();
}

MiningToolStats LumenRun::activePickaxeStats() const {
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *definition = item_registry_.find(equipped.item_id);
  if (definition != nullptr && definition->has_mining_tool) {
    return definition->mining_tool;
  }
  return starterPickaxeStats();
}

void LumenRun::spawnMiningFractureEffect(const Vec3 center, Vec3 normal, Vec3 half_extents,
                                         const Material &material, const std::uint32_t seed,
                                         const int shard_count) {
  if (mining_fracture_shards_.empty()) {
    return;
  }
  normal = length(normal) > 0.0001f ? normalize(normal) : Vec3{0.0f, 1.0f, 0.0f};
  half_extents = {std::max(half_extents.x, 0.035f), std::max(half_extents.y, 0.035f),
                  std::max(half_extents.z, 0.035f)};

  Vec3 axis_y = std::abs(normal.y) < 0.94f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  Vec3 axis_x = normalize(cross(axis_y, normal));
  if (length(axis_x) <= 0.0001f) {
    axis_x = {1.0f, 0.0f, 0.0f};
  }
  axis_y = normalize(cross(normal, axis_x));

  const int requested_shards =
      std::clamp(shard_count, 2, static_cast<int>(mining_fracture_shards_.size()));
  MeshCutImpactSpec cut;
  cut.volume = {.center = center,
                .half_extents = half_extents,
                .axis_x = axis_x,
                .axis_y = axis_y,
                .axis_z = normal};
  cut.impact_point = center + normal * half_extents.z;
  cut.impact_normal = normal;
  cut.seed = seed;
  cut.plane_count =
      std::clamp(static_cast<int>(std::ceil(std::log2(static_cast<float>(requested_shards)))),
                 2, 5);
  cut.angular_spread = 0.54f;
  cut.offset_spread = 0.18f;
  cut.minimum_fragment_area = 0.00004f;

  MeshCutResult cut_result = buildImpactMeshCut(cut);
  std::vector<MeshCutFragment> fragments = std::move(cut_result.fragments);
  if (fragments.empty()) {
    return;
  }
  if (fragments.size() > static_cast<std::size_t>(requested_shards)) {
    std::sort(fragments.begin(), fragments.end(),
              [](const MeshCutFragment &lhs, const MeshCutFragment &rhs) {
                return lhs.surface_area > rhs.surface_area;
              });
    fragments.resize(static_cast<std::size_t>(requested_shards));
  }
  float total_area = 0.0f;
  for (const MeshCutFragment &fragment : fragments) {
    total_area += std::max(fragment.surface_area, 0.0f);
  }

  const auto acquireSlot = [&]() -> MiningFractureShardVisual * {
    for (MiningFractureShardVisual &visual : mining_fracture_shards_) {
      if (!visual.active) {
        return &visual;
      }
    }
    return &*std::max_element(mining_fracture_shards_.begin(), mining_fracture_shards_.end(),
                              [](const MiningFractureShardVisual &lhs,
                                 const MiningFractureShardVisual &rhs) {
                                return lhs.age < rhs.age;
                              });
  };

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (std::size_t i = 0; i < fragments.size(); ++i) {
    MiningFractureShardVisual *visual = acquireSlot();
    if (visual == nullptr || visual->object_index >= scene_.objects().size()) {
      continue;
    }
    CpuMesh local_mesh = std::move(fragments[i].mesh);
    for (Vertex &vertex : local_mesh.vertices) {
      vertex.position = vertex.position - fragments[i].centroid;
    }

    const float fill =
        fragments.size() <= 1u ? 0.0f
                               : static_cast<float>(i) / static_cast<float>(fragments.size() - 1u);
    const float area_fraction =
        total_area > 0.0001f ? fragments[i].surface_area / total_area : 0.0f;
    const float volume_factor = std::clamp(1.0f - area_fraction, 0.35f, 1.0f);
    const Vec3 tangent_kick =
        axis_x * std::cos(golden_angle * static_cast<float>(i)) +
        axis_y * std::sin(golden_angle * static_cast<float>(i));

    visual->active = true;
    visual->position = fragments[i].centroid;
    visual->velocity = fragments[i].impulse_direction * (0.78f + volume_factor * 0.72f) +
                       tangent_kick * (0.18f + fill * 0.18f) + normal * 0.18f;
    visual->rotation = {fill * 0.7f, fill * 1.3f, fill * 0.5f};
    visual->angular_velocity =
        {1.8f + fill * 2.0f, 2.4f + volume_factor * 2.3f, 1.2f + fill * 1.7f};
    visual->age = 0.0f;
    visual->lifetime = 0.68f + volume_factor * 0.32f;
    visual->base_scale = 1.0f;

    RenderObject &object = scene_.objects()[visual->object_index];
    object.name = "Mesh-cut mining fragment";
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = makeSharedMesh(std::move(local_mesh));
    object.transform.position = visual->position;
    object.transform.rotation = visual->rotation;
    object.transform.scale = {1.0f, 1.0f, 1.0f};
    object.material = material;
    object.material.edge_wear = std::max(object.material.edge_wear, 0.34f + fill * 0.18f);
    object.material.pattern_depth = std::max(object.material.pattern_depth, 0.16f);
    object.material.pattern_contrast = std::max(object.material.pattern_contrast, 0.30f);
    object.material.emission_color = object.material.emission_strength > 0.0f
                                         ? object.material.emission_color
                                         : Vec3{0.54f, 0.38f, 0.24f};
    object.material.emission_strength =
        std::max(object.material.emission_strength, 0.070f + volume_factor * 0.035f);
    object.camera_occlusion_fade = false;
    object.casts_contact_shadow = false;
  }
}

bool LumenRun::placeEquippedResource(const Vec3 ray_origin, Vec3 ray_direction) {
  const ItemStack *selected = hotbar_.selectedStack();
  if (selected == nullptr || selected->empty()) {
    return false;
  }
  const ItemDefinition *definition = item_registry_.find(selected->item_id);
  if (definition == nullptr || !definition->placeable ||
      selected->quantity < std::max(definition->placement_cost, 1)) {
    return false;
  }

  ray_direction = normalize(ray_direction);
  if (length(ray_direction) <= 0.0001f) {
    return false;
  }

  const float reach = std::max(definition->placement_reach, 0.1f);
  Vec3 hit_point{};
  Vec3 hit_normal{0.0f, 1.0f, 0.0f};
  float hit_distance = std::numeric_limits<float>::infinity();
  bool hit = false;
  const auto considerHit = [&](const Vec3 point, Vec3 normal, const float distance) {
    if (distance < 0.0f || distance > reach || (hit && distance >= hit_distance - 0.025f)) {
      return;
    }
    if (length(normal) <= 0.0001f) {
      normal = {0.0f, 1.0f, 0.0f};
    }
    hit_point = point;
    hit_normal = normalize(normal);
    hit_distance = distance;
    hit = true;
  };

  PhysicsRayHit physics_hit;
  PhysicsRay ray;
  ray.origin = ray_origin;
  ray.direction = ray_direction;
  ray.max_distance = reach;
  ray.filter.collides_with = kPhysicsLayerWorld;
  ray.filter.ignore_body = player_body_;
  if (physics_.raycast(ray, physics_hit)) {
    considerHit(physics_hit.point, physics_hit.normal, physics_hit.distance);
  }

  constexpr int kSurfaceRaySteps = 48;
  bool previous_valid = false;
  float previous_signed_height = 0.0f;
  float previous_t = 0.0f;
  TerrainSurfaceSample previous_sample;
  for (int i = 1; i <= kSurfaceRaySteps; ++i) {
    const float t = reach * static_cast<float>(i) / static_cast<float>(kSurfaceRaySteps);
    const Vec3 probe = ray_origin + ray_direction * t;
    const TerrainSurfaceSample sample =
        sampleWorldSupport({{probe.x, probe.z}, probe.y, 0.34f, 2.0f});
    if (!sample.valid) {
      previous_valid = false;
      continue;
    }
    const float signed_height = probe.y - sample.height;
    if (previous_valid && previous_signed_height >= 0.0f && signed_height <= 0.0f) {
      float lo = previous_t;
      float hi = t;
      TerrainSurfaceSample hit_sample = sample;
      for (int step = 0; step < 8; ++step) {
        const float mid = (lo + hi) * 0.5f;
        const Vec3 mid_probe = ray_origin + ray_direction * mid;
        const TerrainSurfaceSample mid_sample =
            sampleWorldSupport({{mid_probe.x, mid_probe.z}, mid_probe.y, 0.34f, 2.0f});
        if (!mid_sample.valid) {
          lo = mid;
          continue;
        }
        const float mid_signed_height = mid_probe.y - mid_sample.height;
        if (mid_signed_height > 0.0f) {
          lo = mid;
        } else {
          hi = mid;
          hit_sample = mid_sample;
        }
      }
      const Vec3 surface_probe = ray_origin + ray_direction * hi;
      considerHit({surface_probe.x, hit_sample.height, surface_probe.z}, hit_sample.normal, hi);
      break;
    }
    previous_valid = true;
    previous_signed_height = signed_height;
    previous_t = t;
    previous_sample = sample;
  }
  (void)previous_sample;

  if (!hit) {
    return false;
  }

  const Vec3 half_extents = {std::max(definition->placement_collision_half_extents.x, 0.04f),
                             std::max(definition->placement_collision_half_extents.y, 0.04f),
                             std::max(definition->placement_collision_half_extents.z, 0.04f)};
  const float normal_clearance = std::max({half_extents.x, half_extents.y, half_extents.z});
  const Vec3 center = hit_point + hit_normal * (normal_clearance + 0.018f);
  const float player_clearance =
      tuning_.player_radius + std::max({half_extents.x, half_extents.z}) + 0.10f;
  if (length(center - player_position_) < player_clearance) {
    return false;
  }

  const float yaw_seed =
      std::sin(center.x * 12.9898f + center.y * 37.719f + center.z * 78.233f) * 43758.5453f;
  const float yaw = (yaw_seed - std::floor(yaw_seed)) * kPi * 2.0f;
  PlacedResourceRock rock;
  rock.id = placedResourceId(placed_resource_serial_++);
  rock.item_id = definition->id;
  rock.placement_shape = definition->placement_shape;
  rock.position = center;
  rock.normal = hit_normal;
  rock.rotation = {0.0f, yaw, 0.0f};
  rock.scale = definition->placement_scale;
  rock.collision_half_extents = half_extents;

  RenderObject object;
  object.name = "Placed cave resource rock";
  object.primitive = placementPrimitiveFor(definition->placement_shape);
  object.transform.position = rock.position;
  object.transform.rotation = rock.rotation;
  object.transform.scale = rock.scale;
  object.material = lumenPlacedResourceMaterial(definition);
  object.camera_occlusion_fade = false;
  object.casts_contact_shadow = true;
  object.contact_shadow_strength = 0.18f;
  object.contact_shadow_radius_scale = 0.78f;
  rock.object_index = appendObject(object);
  scenery_objects_.push_back(rock.object_index);
  support_surfaces_.addBox({rock.position, rock.collision_half_extents});
  rock.body = addPlacedRockPhysics(rock);
  placed_rocks_.push_back(rock);

  (void)hotbar_.removeSelectedItem(std::max(definition->placement_cost, 1));
  equipment_.equipFromHotbar(hotbar_);
  setAvatarPointTarget(hit_point);
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedCaveWeb(const std::string_view target_id) {
  constexpr std::string_view prefix = "web:";
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (target_id.substr(0u, prefix.size()) != prefix || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }
  std::size_t web_index = 0u;
  const std::string_view index_text = target_id.substr(prefix.size());
  const char *begin = index_text.data();
  const char *end = begin + index_text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, web_index);
  if (parsed.ec != std::errc{} || parsed.ptr != end || web_index >= cave_webs_.size()) {
    return false;
  }

  CaveWebObstacle &web = cave_webs_[web_index];
  if (web.broken || length(player_position_ - web.center) > kCaveWebInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  Vec3 hit_normal = player_position_ - web.center;
  hit_normal = length(hit_normal) > 0.0001f ? normalize(hit_normal) : web.normal;
  const MineableHit web_hit{.hit = true,
                            .target_key = web.id.empty()
                                              ? "cave_web:" + std::to_string(web_index)
                                              : web.id,
                            .point = web.center,
                            .normal = hit_normal,
                            .material = VoxelCaveMaterial::Rock};
  const MineableAttempt mine_attempt{.now_seconds = status_.elapsed_seconds,
                                     .hit = web_hit,
                                     .tool = tool,
                                     .material_hardness = std::max(web.hardness, 0.1f),
                                     .resource_item_id = {},
                                     .resource_quantity = 0,
                                     .carve_surface = false};
  const MiningFeedback feedback = mining_.tryMine(mine_attempt);
  if (!feedback.accepted) {
    return false;
  }

  web.hit_flash = 1.0f;
  setAvatarPointTarget(feedback.impact_point);
  Material fracture_material =
      web.object_index < scene_.objects().size() ? scene_.objects()[web.object_index].material
                                                 : Material{};
  fracture_material.surface_pattern = SurfacePattern::CaveWeb;
  fracture_material.opacity = std::max(fracture_material.opacity, 0.62f);
  const Vec3 fracture_extent{std::max(web.radius_x * 0.22f, 0.16f),
                             std::max(web.radius_y * 0.12f, 0.10f), 0.035f};
  if (!feedback.carved) {
    fracture_material.emission_strength = 0.08f + feedback.crack_fraction * 0.10f;
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal, fracture_extent,
                              fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8719u),
                              4 + static_cast<int>(std::ceil(feedback.crack_fraction * 5.0f)));
    return true;
  }

  web.broken = true;
  if (web.object_index < scene_.objects().size()) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {std::max(web.radius_x * 0.42f, 0.22f),
                               std::max(web.radius_y * 0.20f, 0.14f), 0.045f},
                              fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8729u), 14);
    hideRenderObject(scene_.objects()[web.object_index]);
  }
  for (CaveSkitter &skitter : cave_skitters_) {
    if (skitter.dead || skitter.state.dead) {
      continue;
    }
    const Vec3 skitter_center = skitter.state.position + Vec3{0.0f, 0.08f, 0.0f};
    spawnBloodBurst(skitter_center, feedback.impact_point, 1.0f);
    if (skitter.object_index < scene_.objects().size()) {
      Material skitter_burst_material = scene_.objects()[skitter.object_index].material;
      skitter_burst_material.base_color = {0.42f, 0.018f, 0.010f};
      skitter_burst_material.emission_color = {0.12f, 0.0f, 0.0f};
      skitter_burst_material.emission_strength =
          std::max(skitter_burst_material.emission_strength, 0.10f);
      spawnMiningFractureEffect(skitter_center, feedback.impact_normal, {0.22f, 0.12f, 0.24f},
                                skitter_burst_material,
                                fractureSeedFor(skitter_center, kLumenCaveSeed + 8837u), 8);
      hideRenderObject(scene_.objects()[skitter.object_index]);
    }
    skitter.dead = true;
    skitter.state.dead = true;
    skitter.state.velocity = {};
    skitter.health = 0;
  }
  focused_cave_web_valid_ = false;
  interaction_.clear();
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedCaveSkitter(const std::string_view target_id) {
  constexpr std::string_view prefix = "skitter:";
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (target_id.substr(0u, prefix.size()) != prefix || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }
  std::size_t skitter_index = 0u;
  const std::string_view index_text = target_id.substr(prefix.size());
  const char *begin = index_text.data();
  const char *end = begin + index_text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, skitter_index);
  if (parsed.ec != std::errc{} || parsed.ptr != end || skitter_index >= cave_skitters_.size()) {
    return false;
  }

  CaveSkitter &skitter = cave_skitters_[skitter_index];
  if (skitter.dead || skitter.state.dead ||
      length(player_position_ - skitter.state.position) > kCaveSkitterInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  Vec3 hit_normal = player_position_ - skitter.state.position;
  hit_normal = length(hit_normal) > 0.0001f ? normalize(hit_normal) : Vec3{0.0f, 1.0f, 0.0f};
  const MineableHit skitter_hit{.hit = true,
                                .target_key = skitter.id.empty()
                                                  ? "cave_skitter:" + std::to_string(skitter_index)
                                                  : skitter.id,
                                .point = skitter.state.position + Vec3{0.0f, 0.08f, 0.0f},
                                .normal = hit_normal,
                                .material = VoxelCaveMaterial::Rock};
  const MiningFeedback feedback =
      mining_.tryMine({.now_seconds = status_.elapsed_seconds,
                       .hit = skitter_hit,
                       .tool = tool,
                       .material_hardness = static_cast<float>(std::max(skitter.max_health, 1)),
                       .resource_item_id = {},
                       .resource_quantity = 0,
                       .carve_surface = false});
  if (!feedback.accepted) {
    return false;
  }

  skitter.hit_flash = 1.0f;
  skitter.last_hit_normal = feedback.impact_normal;
  skitter.state.flinch_seconds = 0.36f;
  setAvatarPointTarget(feedback.impact_point);
  skitter.health = std::max(
      feedback.carved ? 0 : 1,
      static_cast<int>(std::ceil((1.0f - feedback.crack_fraction) *
                                 static_cast<float>(std::max(skitter.max_health, 1)))));
  const Material fracture_material =
      skitter.object_index < scene_.objects().size() ? scene_.objects()[skitter.object_index].material
                                                     : Material{};
  if (!feedback.carved) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {0.24f, 0.16f, 0.26f}, fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8819u),
                              8 + static_cast<int>(std::ceil(feedback.crack_fraction * 7.0f)));
    return true;
  }

  skitter.dead = true;
  skitter.state.dead = true;
  skitter.state.velocity = {};
  spawnBloodBurst(skitter.state.position + Vec3{0.0f, 0.08f, 0.0f}, feedback.impact_point, 1.0f);
  if (skitter.object_index < scene_.objects().size()) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {0.24f, 0.14f, 0.28f}, fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8829u), 18);
    hideRenderObject(scene_.objects()[skitter.object_index]);
  }
  interaction_.clear();
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedOre(const std::string_view target_id) {
  constexpr std::string_view prefix = "ore:";
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (target_id.substr(0u, prefix.size()) != prefix || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }
  std::size_t ore_index = 0u;
  const std::string_view index_text = target_id.substr(prefix.size());
  const char *begin = index_text.data();
  const char *end = begin + index_text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, ore_index);
  if (parsed.ec != std::errc{} || parsed.ptr != end || ore_index >= coal_ores_.size()) {
    return false;
  }

  CoalOreNode &ore = coal_ores_[ore_index];
  if (ore.collected || length(player_position_ - ore.position) > kOreInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  MineableHit ore_hit;
  ore_hit.hit = true;
  ore_hit.target_key = "ore:" + std::to_string(ore_index);
  ore_hit.point = ore.position;
  ore_hit.normal = length(ore.normal) > 0.0001f ? normalize(ore.normal) : Vec3{0.0f, 1.0f, 0.0f};
  ore_hit.material = VoxelCaveMaterial::Coal;
  MiningFeedback feedback = mining_.tryMine({.now_seconds = status_.elapsed_seconds,
                                             .hit = ore_hit,
                                             .tool = tool,
                                             .material_hardness =
                                                 static_cast<float>(std::max(ore.max_health, 1)),
                                             .resource_item_id = "coal",
                                             .resource_quantity = ore.yield_quantity});
  if (!feedback.accepted) {
    return false;
  }

  ore.hit_flash = 1.0f;
  setAvatarPointTarget(feedback.impact_point);
  ore.health = std::max(
      feedback.carved ? 0 : 1,
      static_cast<int>(std::ceil((1.0f - feedback.crack_fraction) *
                                 static_cast<float>(std::max(ore.max_health, 1)))));
  if (!feedback.carved) {
    return true;
  }

  const ItemDefinition *coal = item_registry_.find("coal");
  if (coal == nullptr || !storeMinedResource(*coal, feedback.resource_quantity)) {
    ore.health = 1;
    return false;
  }

  if (ore.object_index < scene_.objects().size()) {
    const Material fracture_material = scene_.objects()[ore.object_index].material;
    spawnMiningFractureEffect(ore.position, ore.normal, ore.scale * 0.52f, fracture_material,
                              fractureSeedFor(ore.position, kLumenCaveSeed + 9001u), 12);
    hideRenderObject(scene_.objects()[ore.object_index]);
  }
  ore.collected = true;
  equipment_.equipFromHotbar(hotbar_);
  return true;
}

void LumenRun::collectOverlaps() {
  ASTER_PROFILE_SCOPE("LumenRun::collectOverlaps");
  for (Shard &shard : shards_) {
    if (shard.collected) {
      continue;
    }
    if (distanceOnArena(player_position_, shard.position) <=
        tuning_.player_radius + tuning_.shard_radius) {
      shard.collected = true;
      ++status_.score;
    }
  }
  status_.victory = status_.score >= status_.total_shards;
}

void LumenRun::resolveSentinelImpacts() {
  ASTER_PROFILE_SCOPE("LumenRun::resolveSentinelImpacts");
  if (invulnerability_ > 0.0f) {
    return;
  }

  const auto &objects = scene_.objects();
  const VerticalCapsuleContactVolume player_volume{
      player_position_,
      tuning_.player_radius,
      std::max(tuning_.player_height * 0.5f - tuning_.player_radius, 0.0f),
  };
  for (const Sentinel &sentinel : sentinels_) {
    const Vec3 sentinel_position = objects[sentinel.object_index].transform.position;
    const VerticalCapsuleContactVolume sentinel_volume{
        sentinel_position,
        tuning_.sentinel_radius * 0.82f,
        tuning_.sentinel_radius * 1.30f,
    };
    if (overlaps(player_volume, sentinel_volume)) {
      triggerPlayerDeath(sentinel_position);
      return;
    }
  }
}

float LumenRun::playerSupportExtent() const {
  return tuning_.player_height * 0.5f;
}

Vec3 LumenRun::avatarPosePosition() const {
  return avatarPosePosition(player_position_);
}

Vec3 LumenRun::avatarPosePosition(const Vec3 player_position) const {
  const float visual_offset = std::max(player_avatar_support_extent_ - playerSupportExtent(), 0.0f);
  return player_position + Vec3{0.0f, visual_offset, 0.0f};
}

} // namespace aster
