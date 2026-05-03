// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/asset/scene_asset_importer.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/game/animation_system.hpp"
#include "aster/game/creature_motion.hpp"
#include "aster/game/equipment_system.hpp"
#include "aster/game/interaction_system.hpp"
#include "aster/game/inventory_system.hpp"
#include "aster/game/item_system.hpp"
#include "aster/game/light_system.hpp"
#include "aster/game/lumen_run.hpp"
#include "aster/game/particle_system.hpp"
#include "aster/game/player_motion.hpp"
#include "aster/game/third_person_camera.hpp"
#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/brush_level_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/castle_course.hpp"
#include "aster/geometry/cave_system.hpp"
#include "aster/geometry/mesh_projection.hpp"
#include "aster/geometry/nature_mesh.hpp"
#include "aster/geometry/stroke_mesh.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/geometry/terrain_sculpt.hpp"
#include "aster/geometry/voxel_structure.hpp"
#include "aster/geometry/water_mesh.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/math/color.hpp"
#include "aster/math/mat4.hpp"
#include "aster/math/transform.hpp"
#include "aster/net/net_message.hpp"
#include "aster/net/node_router.hpp"
#include "aster/physics/climb_locomotion.hpp"
#include "aster/physics/contact_query.hpp"
#include "aster/physics/fluid_locomotion.hpp"
#include "aster/physics/physics_world.hpp"
#include "aster/physics/placement_validation.hpp"
#include "aster/physics/surface_support.hpp"
#include "aster/physics/terrain_contact.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/mesh.hpp"
#include "aster/scene/avatar.hpp"
#include "aster/scene/scene.hpp"
#include "aster/scene/scene_coherence.hpp"
#include "aster/scene/scene_trace.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace {

void expectNear(const float actual, const float expected, const float tolerance) {
  assert(std::abs(actual - expected) <= tolerance);
}

void testVectorMath() {
  const aster::Vec3 x{1.0f, 0.0f, 0.0f};
  const aster::Vec3 y{0.0f, 1.0f, 0.0f};
  const aster::Vec3 z = aster::cross(x, y);
  expectNear(z.x, 0.0f, 0.0001f);
  expectNear(z.y, 0.0f, 0.0001f);
  expectNear(z.z, 1.0f, 0.0001f);
  expectNear(aster::length(aster::normalize({3.0f, 4.0f, 0.0f})), 1.0f, 0.0001f);
}

void testMatrixComposition() {
  const aster::Mat4 transform =
      aster::translation({2.0f, 3.0f, 4.0f}) * aster::scale({2.0f, 2.0f, 2.0f});
  expectNear(transform.m[0], 2.0f, 0.0001f);
  expectNear(transform.m[5], 2.0f, 0.0001f);
  expectNear(transform.m[10], 2.0f, 0.0001f);
  expectNear(transform.m[12], 2.0f, 0.0001f);
  expectNear(transform.m[13], 3.0f, 0.0001f);
  expectNear(transform.m[14], 4.0f, 0.0001f);
}

void testTransformContract() {
  aster::Transform transform;
  transform.position = {2.0f, 3.0f, 4.0f};
  transform.scale = {2.0f, 2.0f, 2.0f};

  const aster::Mat4 matrix = transform.matrix();
  expectNear(matrix.m[0], 2.0f, 0.0001f);
  expectNear(matrix.m[5], 2.0f, 0.0001f);
  expectNear(matrix.m[10], 2.0f, 0.0001f);
  expectNear(matrix.m[12], 2.0f, 0.0001f);
  expectNear(matrix.m[13], 3.0f, 0.0001f);
  expectNear(matrix.m[14], 4.0f, 0.0001f);
}

void testColorPipeline() {
  const aster::Vec3 mapped = aster::aces_tonemap({2.0f, 1.0f, 0.25f});
  assert(mapped.x <= 1.0f && mapped.x >= 0.0f);
  assert(mapped.y <= 1.0f && mapped.y >= 0.0f);
  assert(mapped.z <= 1.0f && mapped.z >= 0.0f);
  const aster::Vec3 reinhard = aster::reinhard_tonemap({1.0f, 3.0f, 0.0f});
  expectNear(reinhard.x, 0.5f, 0.0001f);
  expectNear(reinhard.y, 0.75f, 0.0001f);
  expectNear(reinhard.z, 0.0f, 0.0001f);
}

void testFixedTimestep() {
  aster::FixedTimestep timestep({1.0 / 60.0, 1.0 / 20.0, 4});
  assert(timestep.advance(1.0) == 3u);
  assert(timestep.accumulatorSeconds() < 0.0001);
  assert(timestep.advance(0.001) == 0u);
  assert(timestep.interpolationAlpha() > 0.0);
  assert(timestep.advance(1.0 / 60.0) == 1u);
  timestep.reset();
  assert(timestep.accumulatorSeconds() == 0.0);
}

void testFrameTimeStats() {
  aster::FrameTimeStats stats;
  assert(stats.empty());
  stats.addSample(0.010);
  stats.addSample(0.020);
  stats.addSample(0.030);
  stats.addSample(-1.0);

  const aster::FrameTimeSummary summary = stats.summarize(0.016);
  assert(summary.samples == 3u);
  expectNear(summary.min_seconds, 0.010, 0.000001);
  expectNear(summary.mean_seconds, 0.020, 0.000001);
  expectNear(summary.p95_seconds, 0.030, 0.000001);
  expectNear(summary.max_seconds, 0.030, 0.000001);
  assert(summary.budget_seconds.has_value());
  assert(summary.over_budget == 2u);
}

void testGameplayItemInteractionSystems() {
  aster::ItemRegistry registry;
  registry.add({.id = "torch",
                .display_name = "Torch",
                .short_label = "TRC",
                .type = aster::ItemType::LightTool,
                .tint = {1.0f, 0.45f, 0.12f},
                .creates_light = true,
                .creates_fire_particles = true});
  registry.add({.id = "pickaxe",
                .display_name = "Pickaxe",
                .short_label = "PCK",
                .type = aster::ItemType::Tool,
                .tint = {0.5f, 0.48f, 0.42f}});
  assert(registry.contains("torch"));

  aster::InventoryContainer chest(2u);
  assert(chest.addItem(*registry.find("torch"), 1).has_value());
  assert(chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(!chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(chest.removeItem("torch", 1));

  aster::Hotbar hotbar(3u);
  const std::optional<std::size_t> slot = hotbar.addItem(*registry.find("torch"), 1);
  assert(slot.has_value());
  assert(hotbar.select(*slot));
  aster::EquipmentSystem equipment;
  equipment.equipFromHotbar(hotbar);
  assert(equipment.isEquipped("torch"));

  aster::InteractionSystem interactions;
  interactions.update({{.id = "item:torch",
                        .kind = aster::InteractionTargetKind::Item,
                        .action_label = "Take",
                        .subject_label = "Torch",
                        .position = {0.0f, 0.0f, 3.0f},
                        .radius = 0.35f,
                        .max_distance = 6.0f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:torch");

  aster::ScalarAnimation animation;
  animation.setTarget(1.0f);
  animation.update(1.0f / 15.0f);
  assert(animation.value() > 0.0f);
  aster::FlickerLightSpec light_spec;
  light_spec.source_radius = 0.70f;
  const aster::DynamicPointLight light =
      aster::evaluateFlickerLight(light_spec, {1.0f, 2.0f, 3.0f}, 0.25f);
  assert(light.active);
  assert(light.intensity > 0.0f);
  expectNear(light.source_radius, 0.70f, 0.0001f);

  aster::ParticleEmitter emitter(3u);
  emitter.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, 0.5f, {.max_particles = 3u});
  assert(emitter.particles().size() == 3u);
  assert(emitter.particles().front().active);

  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 1.0f});
  aster::AvatarPose pose;
  pose.position = {1.0f, 2.0f, 3.0f};
  pose.facing_yaw = aster::radians(35.0f);
  pose.gait_phase = 0.8f;
  pose.stride_amplitude = 0.35f;
  const std::optional<aster::Transform> socket = aster::resolveAvatarAttachmentSocket(
      rig, pose, {.part_name = "right paw", .local_position = {0.0f, -0.10f, 0.20f}});
  assert(socket.has_value());
  assert(aster::length(socket->position - pose.position) > 0.10f);
  assert(!aster::resolveAvatarAttachmentSocket(rig, pose, {.part_name = "missing"}).has_value());
}

void testMeshGeneration() {
  const aster::CpuMesh sphere = aster::makeUvSphere(16, 8, 1.0f);
  assert(!sphere.vertices.empty());
  assert(!sphere.indices.empty());
  assert(sphere.indices.size() == 16u * 8u * 6u);

  const aster::CpuMesh plane = aster::makePlane(2.0f);
  assert(plane.vertices.size() == 4u);
  assert(plane.indices.size() == 6u);

  const aster::CpuMesh box = aster::makeBox();
  assert(box.vertices.size() == 36u);
  assert(box.indices.size() == 36u);

  const aster::CpuMesh rock = aster::makeRock(8, 5, 1.0f);
  assert(!rock.vertices.empty());
  assert(!rock.indices.empty());

  const aster::CpuMesh crystal = aster::makeCrystal(6, 1.0f, 1.8f);
  assert(crystal.indices.size() == 6u * 12u);

  const aster::CpuMesh block = aster::makeRuinBlock();
  assert(block.indices.size() == 36u);
  for (const aster::Vertex &vertex : block.vertices) {
    assert(aster::dot(vertex.position, vertex.normal) > 0.0f);
  }

  const aster::CpuMesh pillar = aster::makePillar(8, 1.0f, 1.0f);
  assert(!pillar.vertices.empty());
  assert(!pillar.indices.empty());

  const aster::CpuMesh cable = aster::makeCableMesh();
  assert(!cable.vertices.empty());
  assert(!cable.indices.empty());

  const aster::CpuMesh mound = aster::makeMoundMesh();
  assert(!mound.vertices.empty());
  assert(!mound.indices.empty());
  const aster::MoundMeshSpec basin_spec{.radial_segments = 48,
                                        .rings = 12,
                                        .radius_x = 3.2f,
                                        .radius_z = 2.1f,
                                        .height = 0.42f,
                                        .pond_radius_x = 1.8f,
                                        .pond_radius_z = 1.0f,
                                        .pond_depth = 0.72f,
                                        .bank_width = 0.42f,
                                        .bank_height = 0.24f,
                                        .basin_floor_radius = 0.48f,
                                        .shore_depth_fraction = 0.12f};
  const aster::CpuMesh basin = aster::makeMoundMesh(basin_spec);
  float basin_floor_y = std::numeric_limits<float>::max();
  float shore_lip_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : basin.vertices) {
    const float dx = vertex.position.x / basin_spec.pond_radius_x;
    const float dz = vertex.position.z / basin_spec.pond_radius_z;
    const float pond_distance = std::sqrt(dx * dx + dz * dz);
    if (pond_distance < basin_spec.basin_floor_radius * 0.55f) {
      basin_floor_y = std::min(basin_floor_y, vertex.position.y);
    }
    if (pond_distance > 0.92f && pond_distance < 1.12f) {
      shore_lip_y = std::max(shore_lip_y, vertex.position.y);
    }
  }
  assert(shore_lip_y > basin_floor_y + basin_spec.pond_depth * 0.35f);

  const aster::CpuMesh pond = aster::makePondWaterMesh();
  assert(!pond.vertices.empty());
  assert(!pond.indices.empty());
  const aster::SubmergedBasinMeshSpec submerged_spec{.floor_depth = 0.42f,
                                                     .shore_depth = 0.09f,
                                                     .basin_floor_radius = 0.40f,
                                                     .bottom_noise = 0.0f};
  const aster::CpuMesh submerged_basin = aster::makeSubmergedBasinMesh(submerged_spec);
  assert(!submerged_basin.vertices.empty());
  assert(!submerged_basin.indices.empty());
  float submerged_floor_y = std::numeric_limits<float>::max();
  float submerged_shore_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : submerged_basin.vertices) {
    const float dx = vertex.position.x / submerged_spec.radius_x;
    const float dz = vertex.position.z / submerged_spec.radius_z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance < submerged_spec.basin_floor_radius * 0.50f) {
      submerged_floor_y = std::min(submerged_floor_y, vertex.position.y);
    }
    if (distance > 0.94f) {
      submerged_shore_y = std::max(submerged_shore_y, vertex.position.y);
    }
  }
  assert(submerged_shore_y > submerged_floor_y + 0.25f);
  const aster::CpuMesh spectral_water =
      aster::makeEllipticalWaterMesh({.radial_segments = 32,
                                      .rings = 6,
                                      .radius_x = 2.0f,
                                      .radius_z = 1.0f,
                                      .shoreline_inset = 0.08f,
                                      .spectrum = {.amplitude = 0.024f, .component_count = 10}});
  assert(!spectral_water.vertices.empty());
  assert(!spectral_water.indices.empty());
  const aster::Vec3 water_normal = aster::spectralWaterNormal(
      {0.25f, -0.15f}, {.amplitude = 0.020f, .dominant_wavelength = 0.8f, .component_count = 8});
  assert(aster::length(water_normal) > 0.99f);

  const aster::CpuMesh terrain_transition = aster::makeTerrainTransitionMesh();
  assert(!terrain_transition.vertices.empty());
  assert(!terrain_transition.indices.empty());
  bool transition_has_edge_skirt = false;
  for (const aster::Vertex &vertex : terrain_transition.vertices) {
    transition_has_edge_skirt = transition_has_edge_skirt || vertex.position.y < 0.0f;
  }
  assert(transition_has_edge_skirt);
  const aster::TerrainTransitionMeshSpec directional_transition_spec{
      .radial_segments = 32,
      .rings = 4,
      .inner_radius_x = 1.4f,
      .inner_radius_z = 1.0f,
      .outer_radius_x = 2.2f,
      .outer_radius_z = 2.1f,
      .outer_radius_z_negative = 1.42f,
      .outer_radius_z_positive = 2.1f,
      .outer_radius_x_negative = 2.75f,
      .outer_radius_x_positive = 2.2f,
      .edge_irregularity = 0.0f,
      .edge_skirt_depth = 0.0f};
  const aster::CpuMesh directional_transition =
      aster::makeTerrainTransitionMesh(directional_transition_spec);
  float directional_min_x = std::numeric_limits<float>::max();
  float directional_max_x = std::numeric_limits<float>::lowest();
  float directional_min_z = std::numeric_limits<float>::max();
  float directional_max_z = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : directional_transition.vertices) {
    directional_min_x = std::min(directional_min_x, vertex.position.x);
    directional_max_x = std::max(directional_max_x, vertex.position.x);
    directional_min_z = std::min(directional_min_z, vertex.position.z);
    directional_max_z = std::max(directional_max_z, vertex.position.z);
  }
  assert(-directional_min_x > directional_max_x + 0.45f);
  assert(-directional_min_x > 2.70f && -directional_min_x < 2.80f);
  assert(directional_max_x > 2.15f && directional_max_x < 2.25f);
  assert(directional_max_z > -directional_min_z + 0.55f);
  assert(directional_max_z > 2.05f && directional_max_z < 2.15f);
  assert(-directional_min_z > 1.37f && -directional_min_z < 1.47f);

  const aster::CpuMesh grass = aster::makeGrassTuftMesh();
  assert(!grass.vertices.empty());
  assert(!grass.indices.empty());
  const aster::CpuMesh fish = aster::makeFishMesh();
  assert(!fish.vertices.empty());
  assert(!fish.indices.empty());
  const aster::CpuMesh predator = aster::makeAmphibiousPredatorMesh();
  assert(!predator.vertices.empty());
  assert(!predator.indices.empty());
  const aster::CpuMesh broad_leaf = aster::makeBroadLeafPlantMesh();
  assert(!broad_leaf.vertices.empty());
  assert(!broad_leaf.indices.empty());
  const aster::CpuMesh tree_trunk = aster::makeClimbableTreeTrunkMesh();
  assert(!tree_trunk.vertices.empty());
  assert(!tree_trunk.indices.empty());
  const aster::CpuMesh tree_canopy = aster::makeTreeCanopyMesh();
  assert(!tree_canopy.vertices.empty());
  assert(!tree_canopy.indices.empty());
  const aster::CpuMesh chest = aster::makeTreasureChestMesh();
  assert(!chest.vertices.empty());
  assert(!chest.indices.empty());
  const aster::CpuMesh sentinel = aster::makeSignalSentinelMesh();
  assert(!sentinel.vertices.empty());
  assert(!sentinel.indices.empty());
  const aster::CpuMesh underpass = aster::makeGothicUnderpassMesh();
  assert(!underpass.vertices.empty());
  assert(!underpass.indices.empty());
  const aster::GothicUnderpassMeshSpec underpass_spec;
  const std::vector<aster::ArchitecturalCollisionBox> underpass_collision =
      aster::makeGothicUnderpassCollisionBoxes(underpass_spec);
  assert(underpass_collision.size() == 3u);
  const auto underpassContains = [&](const aster::Vec3 point) {
    for (const aster::ArchitecturalCollisionBox &box : underpass_collision) {
      if (std::abs(point.x - box.center.x) <= box.half_extents.x &&
          std::abs(point.y - box.center.y) <= box.half_extents.y &&
          std::abs(point.z - box.center.z) <= box.half_extents.z) {
        return true;
      }
    }
    return false;
  };
  assert(!underpassContains({0.0f, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(
      underpassContains({underpass_spec.half_width, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(underpassContains({0.0f, underpass_spec.passage_height + 0.20f, 0.0f}));

  const aster::CpuMesh path = aster::makePathRibbonMesh();
  assert(!path.vertices.empty());
  assert(!path.indices.empty());
  const aster::CpuMesh tapered_path = aster::makePathRibbonMesh({.segments = 8,
                                                                 .width = 0.8f,
                                                                 .width_variation = 0.0f,
                                                                 .crown_height = 0.0f,
                                                                 .surface_noise = 0.0f,
                                                                 .endpoint_taper = 0.25f,
                                                                 .start = {-1.0f, 0.0f, 0.0f},
                                                                 .control = {-0.4f, 0.0f, 0.0f},
                                                                 .control_b = {0.4f, 0.0f, 0.0f},
                                                                 .end = {1.0f, 0.0f, 0.0f}});
  float tapered_start_min = std::numeric_limits<float>::max();
  float tapered_start_max = std::numeric_limits<float>::lowest();
  float tapered_mid_min = std::numeric_limits<float>::max();
  float tapered_mid_max = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : tapered_path.vertices) {
    if (std::abs(vertex.uv.y) < 0.0001f) {
      tapered_start_min = std::min(tapered_start_min, vertex.position.z);
      tapered_start_max = std::max(tapered_start_max, vertex.position.z);
    }
    if (std::abs(vertex.uv.y - 0.5f) < 0.0001f) {
      tapered_mid_min = std::min(tapered_mid_min, vertex.position.z);
      tapered_mid_max = std::max(tapered_mid_max, vertex.position.z);
    }
  }
  const float tapered_start_span = tapered_start_max - tapered_start_min;
  const float tapered_mid_span = tapered_mid_max - tapered_mid_min;
  assert(tapered_start_span > tapered_mid_span * 0.55f);
  assert(tapered_start_span < tapered_mid_span);
  const aster::CpuMesh path_shoulders =
      aster::makePathShoulderMesh({.path = {.segments = 8,
                                            .width = 0.8f,
                                            .endpoint_taper = 0.25f,
                                            .start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 0.0f},
                                            .control_b = {0.4f, 0.0f, 0.0f},
                                            .end = {1.0f, 0.0f, 0.0f}},
                                   .side_segments = 3,
                                   .shoulder_width = 0.35f,
                                   .shoulder_height = 0.12f});
  assert(!path_shoulders.vertices.empty());
  assert(!path_shoulders.indices.empty());
  float shoulder_min_y = std::numeric_limits<float>::max();
  float shoulder_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : path_shoulders.vertices) {
    shoulder_min_y = std::min(shoulder_min_y, vertex.position.y);
    shoulder_max_y = std::max(shoulder_max_y, vertex.position.y);
  }
  assert(shoulder_max_y > shoulder_min_y + 0.05f);
  aster::StrokePath stroke_path;
  stroke_path.points = {{{-0.5f, 0.0f}, 0.05f}, {{0.0f, 0.35f}, 0.04f}, {{0.5f, 0.0f}, 0.05f}};
  const aster::CpuMesh stroke = aster::makeStrokeRibbonMesh({{stroke_path}, 0.02f});
  assert(stroke.vertices.size() == 8u);
  assert(stroke.indices.size() == 12u);
  const aster::PathRibbonMeshSpec path_spec{.start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 1.0f},
                                            .control_b = {0.8f, 0.0f, -0.6f},
                                            .end = {1.0f, 0.0f, 0.0f}};
  const aster::Vec3 path_start = aster::evaluatePathRibbonCenter(path_spec, 0.0f);
  const aster::Vec3 path_end = aster::evaluatePathRibbonCenter(path_spec, 1.0f);
  assert(std::abs(path_start.x + 1.0f) < 0.001f);
  assert(std::abs(path_end.x - 1.0f) < 0.001f);
  assert(aster::length(aster::evaluatePathRibbonTangent(path_spec, 0.5f)) > 0.9f);
  const std::vector<aster::PathRibbonMeshSpec> route_segments = {{.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {-2.0f, 0.0f, 0.0f},
                                                                  .control = {-1.5f, 0.0f, 1.0f},
                                                                  .control_b = {-0.8f, 0.0f, 1.0f},
                                                                  .end = {0.0f, 0.0f, 0.6f}},
                                                                 {.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {0.0f, 0.0f, 0.6f},
                                                                  .control = {0.9f, 0.0f, 0.2f},
                                                                  .control_b = {1.4f, 0.0f, -0.5f},
                                                                  .end = {2.0f, 0.0f, 0.0f}}};
  const aster::CpuMesh route_path = aster::makePathRouteRibbonMesh({.segments = route_segments});
  assert(route_path.vertices.size() > tapered_path.vertices.size());
  assert(route_path.indices.size() > tapered_path.indices.size());
  aster::PathShoulderRouteMeshSpec route_shoulder_spec;
  for (const aster::PathRibbonMeshSpec &segment : route_segments) {
    route_shoulder_spec.segments.push_back({.path = segment, .side_segments = 2});
  }
  const aster::CpuMesh route_shoulders = aster::makePathRouteShoulderMesh(route_shoulder_spec);
  assert(!route_shoulders.vertices.empty());
  assert(!route_shoulders.indices.empty());

  const aster::TerrainHeightField terrain =
      aster::makeProceduralTerrain({.grid_size = 17, .square_size = 1.0f});
  const aster::TerrainSurfaceSample terrain_sample = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(terrain_sample.valid);
  assert(aster::length(terrain_sample.normal) > 0.99f);
  const aster::CpuMesh terrain_mesh = aster::makeTerrainMesh(terrain);
  assert(terrain_mesh.vertices.size() == 17u * 17u);
  assert(terrain_mesh.indices.size() == 16u * 16u * 6u);
  const auto assert_upward_winding = [](const aster::CpuMesh &mesh) {
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
      const aster::Vertex &a = mesh.vertices[mesh.indices[i]];
      const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
      const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
      assert(aster::cross(b.position - a.position, c.position - a.position).y > 0.0f);
    }
  };
  assert_upward_winding(terrain_mesh);
  aster::TerrainMeshBuildOptions smooth_terrain_options;
  smooth_terrain_options.subdivisions_per_square = 2;
  smooth_terrain_options.smooth_visual_surface = true;
  const aster::CpuMesh smooth_terrain_mesh =
      aster::makeTerrainMesh(terrain, smooth_terrain_options);
  assert(smooth_terrain_mesh.vertices.size() == 33u * 33u);
  assert(smooth_terrain_mesh.indices.size() == 32u * 32u * 6u);
  assert_upward_winding(smooth_terrain_mesh);

  aster::TerrainMeshBuildOptions clipped_terrain_options;
  clipped_terrain_options.clip_boxes.push_back({{-0.55f, -0.55f}, {0.55f, 0.55f}});
  const aster::CpuMesh clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, clipped_terrain_options);
  assert(clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
  aster::TerrainMeshBuildOptions oriented_clip_options;
  oriented_clip_options.clip_oriented_ellipses.push_back({.center = {0.0f, 0.0f},
                                                          .forward = {1.0f, 0.0f},
                                                          .radius = {1.2f, 1.8f},
                                                          .radius_forward_negative = 0.8f,
                                                          .radius_forward_positive = 2.4f});
  const aster::CpuMesh oriented_clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, oriented_clip_options);
  assert(oriented_clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
}

void testTerrainSculpting() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 25;
  terrain.square_size = 1.0f;
  terrain.origin = {-12.0f, -12.0f};
  terrain.heights.assign(static_cast<std::size_t>(terrain.grid_size * terrain.grid_size), 0.0f);

  aster::applyTerrainMountainBrush(terrain, {.seed = 11u,
                                             .center = {0.0f, 0.0f},
                                             .radius = {7.0f, 7.0f},
                                             .height = 3.0f,
                                             .shoulder_height = 0.4f,
                                             .surface_noise = 0.0f,
                                             .ridge_frequency = 0.12f,
                                             .inner_plateau = 0.12f});
  assert(aster::sampleTerrain(terrain, {0.0f, 0.0f}).height > 2.3f);
  assert(std::abs(aster::sampleTerrain(terrain, {11.0f, 11.0f}).height) < 0.001f);

  aster::deformTerrainAlongPath(terrain, {.path = {.samples = 24,
                                                   .start = {-6.0f, 0.5f, -4.0f},
                                                   .control = {-2.0f, 0.9f, -1.5f},
                                                   .control_b = {2.0f, 1.4f, 1.5f},
                                                   .end = {6.0f, 1.8f, 4.0f}},
                                          .half_width = 0.55f,
                                          .shoulder_width = 0.75f,
                                          .crown_height = 0.0f,
                                          .surface_noise = 0.0f,
                                          .seed = 17u});
  const aster::TerrainSurfaceSample path_mid = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(path_mid.valid);
  assert(path_mid.height > 0.9f);
  assert(path_mid.height < 1.7f);

  aster::TerrainHeightField route_terrain;
  route_terrain.grid_size = 25;
  route_terrain.square_size = 1.0f;
  route_terrain.origin = {-12.0f, -12.0f};
  route_terrain.heights.assign(
      static_cast<std::size_t>(route_terrain.grid_size * route_terrain.grid_size), 0.0f);
  aster::deformTerrainAlongRoute(route_terrain,
                                 {.route = {.segments = {{.samples = 16,
                                                          .start = {-8.0f, 0.4f, 5.0f},
                                                          .control = {-5.2f, 0.5f, 7.2f},
                                                          .control_b = {-2.7f, 0.6f, 6.4f},
                                                          .end = {0.0f, 0.7f, 5.0f}},
                                                         {.samples = 16,
                                                          .start = {0.0f, 0.7f, 5.0f},
                                                          .control = {2.8f, 0.8f, 3.1f},
                                                          .control_b = {5.2f, 0.9f, 3.4f},
                                                          .end = {8.0f, 1.0f, 5.0f}}}},
                                  .half_width = 0.5f,
                                  .shoulder_width = 0.6f,
                                  .crown_height = 0.0f,
                                  .surface_noise = 0.0f,
                                  .seed = 31u});
  const aster::TerrainSurfaceSample route_join = aster::sampleTerrain(route_terrain, {0.0f, 5.0f});
  assert(route_join.valid);
  assert(route_join.height > 0.62f);
  assert(std::abs(aster::sampleTerrain(route_terrain, {11.0f, -11.0f}).height) < 0.001f);

  aster::sculptTerrainPortalShelf(terrain, {.seed = 23u,
                                            .entrance = {2.0f, 1.25f, 2.0f},
                                            .inward = {0.0f, 0.0f, -1.0f},
                                            .floor_height = 1.25f,
                                            .half_width = 1.2f,
                                            .front_depth = 1.0f,
                                            .back_depth = 1.5f,
                                            .feather = 0.6f,
                                            .side_berm_height = 0.3f,
                                            .rear_cover_height = 0.4f,
                                            .surface_noise = 0.0f});
  const aster::TerrainSurfaceSample portal_floor = aster::sampleTerrain(terrain, {2.0f, 2.0f});
  assert(portal_floor.valid);
  assert(std::abs(portal_floor.height - 1.25f) < 0.08f);
}

void testCaveInteriorVolume() {
  const aster::CaveTunnelProfile tunnel{.seed = 7u,
                                        .start = {0.0f, 0.0f, 0.0f},
                                        .control = {0.2f, -0.5f, -3.0f},
                                        .control_b = {1.1f, -1.2f, -6.0f},
                                        .end = {0.4f, -1.6f, -9.0f},
                                        .length_segments = 32,
                                        .radial_segments = 12,
                                        .half_width = 1.0f,
                                        .wall_height = 1.5f,
                                        .floor_width = 1.1f,
                                        .floor_crown = 0.02f,
                                        .wall_noise = 0.0f,
                                        .visible_wall_start_t = 0.20f,
                                        .collision_start_t = 0.18f,
                                        .ore_start_t = 0.32f,
                                        .chest_t = 0.62f,
                                        .chamber_t = 0.64f,
                                        .chamber_falloff = 0.20f,
                                        .chamber_width_scale = 1.7f,
                                        .chamber_height_scale = 1.25f};
  const aster::CaveInteriorSample outside_under =
      aster::sampleCaveInteriorVolume(tunnel, {0.0f, -1.0f, -1.0f});
  assert(outside_under.interior < 0.001f);

  const aster::Vec3 inside_floor = aster::evaluateCaveTunnelCenter(tunnel, 0.58f);
  const aster::CaveInteriorSample inside =
      aster::sampleCaveInteriorVolume(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f});
  assert(inside.interior > 0.5f);
  assert(inside.depth > 0.0f);
  assert(inside.half_width > tunnel.half_width);

  const aster::CaveComplex complex =
      aster::buildCaveComplex({.tunnel = tunnel,
                               .portal = {.arch_segments = 8},
                               .ore = {.seed = 13u, .candidates = 16, .max_nodes = 4}});
  assert(!complex.tunnel_chunks.empty());
  assert(!complex.collision_mesh.vertices.empty());
  assert(!complex.collision_mesh.indices.empty());
  assert(complex.portal_mesh.vertices.size() > 18u);
  assert(!complex.portal_floor_mesh.vertices.empty());
  assert(!complex.portal_floor_mesh.indices.empty());
  aster::Vec3 portal_floor_center{};
  for (const aster::Vertex &vertex : complex.portal_floor_mesh.vertices) {
    portal_floor_center = portal_floor_center + vertex.position;
    assert(vertex.normal.y > 0.20f);
  }
  portal_floor_center =
      portal_floor_center / static_cast<float>(complex.portal_floor_mesh.vertices.size());
  const aster::TerrainSurfaceSample portal_floor_support = aster::sampleMeshSupport(
      complex.portal_floor_mesh, {},
      aster::SurfaceSupportQuery{{portal_floor_center.x, portal_floor_center.z}}, 0.36f);
  assert(portal_floor_support.valid);
  assert(complex.chest_position.y < -0.4f);
  const aster::Vec3 chamber_center = aster::evaluateCaveTunnelCenter(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_tangent = aster::evaluateCaveTunnelTangent(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_side =
      aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, chamber_tangent));
  const aster::CaveTraversalConstraint side_constraint = aster::constrainCaveTraversalPosition(
      tunnel, chamber_center + chamber_side * 2.4f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(side_constraint.active);
  assert(aster::length(side_constraint.correction) > 0.0f);
  const aster::Vec3 end = aster::evaluateCaveTunnelCenter(tunnel, 1.0f);
  const aster::Vec3 end_tangent = aster::evaluateCaveTunnelTangent(tunnel, 1.0f);
  const aster::CaveTraversalConstraint end_constraint = aster::constrainCaveTraversalPosition(
      tunnel, end + end_tangent * 0.45f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(end_constraint.active);
  assert(aster::dot(end_constraint.corrected_position - end, end_tangent) < 0.0f);
  const aster::CaveViewConstraint view_constraint =
      aster::constrainCaveViewSegment(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f},
                                      tunnel.start + aster::Vec3{0.0f, 1.0f, 1.2f},
                                      {.samples = 18,
                                       .minimum_radius = 0.65f,
                                       .interior_threshold = 0.045f,
                                       .backtrack_tolerance_t = 0.12f});
  assert(view_constraint.active);
  assert(view_constraint.radius > 0.0f);
  const std::vector<aster::CaveWallFixturePlacement> fixtures =
      aster::placeCaveWallFixtures(tunnel, {.start_t = 0.28f,
                                            .end_t = 0.70f,
                                            .target_spacing = 2.5f,
                                            .max_count = 3,
                                            .wall_side = -1.0f,
                                            .mount_height = 1.0f,
                                            .wall_inset = 0.12f});
  assert(!fixtures.empty());
  for (const aster::CaveWallFixturePlacement &fixture : fixtures) {
    const aster::CaveInteriorSample fixture_sample =
        aster::sampleCaveInteriorVolume(tunnel, fixture.position);
    assert(fixture_sample.tunnel_t >= 0.24f);
    assert(aster::length(fixture.normal) > 0.90f);
  }
  const aster::CaveTerrainPortalCut portal_cut =
      aster::makeCaveTerrainPortalCut(tunnel, {.arch_segments = 8});
  assert(portal_cut.radius_forward_positive > portal_cut.radius_forward_negative);
  const aster::CaveTerrainCoverFit uncovered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel, [](const aster::Vec2) { return aster::CaveTerrainCoverSample{true, -10.0f}; },
      {.samples = 24,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(!uncovered_fit.cover_found);
  assert(uncovered_fit.tunnel.visible_wall_start_t == tunnel.visible_wall_start_t);
  const aster::CaveTerrainCoverFit covered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel,
      [](const aster::Vec2 position) {
        return aster::CaveTerrainCoverSample{true, position.y < -4.0f ? 10.0f : -10.0f};
      },
      {.samples = 48,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(covered_fit.cover_found);
  assert(covered_fit.tunnel.visible_wall_start_t > tunnel.visible_wall_start_t);
  assert(covered_fit.tunnel.collision_start_t == tunnel.collision_start_t);
  for (const aster::CaveOreNodePlacement &ore : complex.ore_nodes) {
    const aster::CaveInteriorSample ore_sample =
        aster::sampleCaveInteriorVolume(tunnel, ore.position);
    assert(ore_sample.tunnel_t >= tunnel.ore_start_t - 0.08f);
  }
}

void testMeshProcessingPipeline() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  for (aster::Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());

  aster::MeshDiagnostics diagnostics;
  const aster::CpuMesh prepared = aster::prepareMeshForRendering(std::move(mesh), {}, &diagnostics);
  assert(prepared.indices.size() == 6u);
  assert(diagnostics.degenerate_triangles == 1u);
  assert(diagnostics.invalid_normals == 4u);
  assert(diagnostics.generated_tangents == 6u);
  for (const aster::Vertex &vertex : prepared.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    assert(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
  }
}

void testMeshProcessingRejectsInvalidIndices() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  mesh.indices.front() = 99u;

  bool rejected = false;
  try {
    (void)aster::prepareMeshForRendering(std::move(mesh));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
}

void testMeshDrapingRaisesEmbeddedVertices() {
  aster::CpuMesh path = aster::makePathRibbonMesh({.segments = 3,
                                                   .width = 0.3f,
                                                   .crown_height = 0.0f,
                                                   .surface_noise = 0.0f,
                                                   .start = {-1.0f, -0.2f, 0.0f},
                                                   .control = {-0.4f, -0.2f, 0.0f},
                                                   .control_b = {0.4f, -0.2f, 0.0f},
                                                   .end = {1.0f, -0.2f, 0.0f}});
  const aster::CpuMesh draped = aster::drapeMeshToSurface(
      std::move(path), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.35f}; },
      {.surface_offset = 0.02f, .raise_only = true});
  for (const aster::Vertex &vertex : draped.vertices) {
    assert(vertex.position.y >= 0.369f);
  }

  aster::CpuMesh raised_path = aster::makePathRibbonMesh({.segments = 3,
                                                          .width = 0.3f,
                                                          .crown_height = 0.0f,
                                                          .surface_noise = 0.0f,
                                                          .start = {-1.0f, 0.8f, 0.0f},
                                                          .control = {-0.4f, 0.8f, 0.0f},
                                                          .control_b = {0.4f, 0.8f, 0.0f},
                                                          .end = {1.0f, 0.8f, 0.0f}});
  const aster::CpuMesh projected = aster::drapeMeshToSurface(
      std::move(raised_path),
      [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.20f}; },
      {.surface_offset = 0.01f, .raise_only = false});
  for (const aster::Vertex &vertex : projected.vertices) {
    expectNear(vertex.position.y, 0.21f, 0.0001f);
  }

  aster::CpuMesh relief = aster::makePathShoulderMesh({.path = {.segments = 4,
                                                                .width = 0.4f,
                                                                .start = {-1.0f, 0.0f, 0.0f},
                                                                .control = {-0.4f, 0.0f, 0.0f},
                                                                .control_b = {0.4f, 0.0f, 0.0f},
                                                                .end = {1.0f, 0.0f, 0.0f}},
                                                       .shoulder_height = 0.16f});
  const aster::CpuMesh preserved = aster::drapeMeshToSurface(
      std::move(relief), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.45f}; },
      {.surface_offset = 0.02f,
       .raise_only = false,
       .preserve_vertical_offset = true,
       .reference_y = 0.0f});
  float preserved_min_y = std::numeric_limits<float>::max();
  float preserved_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : preserved.vertices) {
    preserved_min_y = std::min(preserved_min_y, vertex.position.y);
    preserved_max_y = std::max(preserved_max_y, vertex.position.y);
  }
  assert(preserved_max_y > preserved_min_y + 0.05f);
}

void testSceneAssetNormalTangentImport() {
  const std::filesystem::path project_root =
      std::filesystem::path(__FILE__).parent_path().parent_path();
  const std::filesystem::path sample =
      project_root / "assets/sample_scene/normal_tangent_probe/normal_tangent_probe.scene";
  assert(std::filesystem::exists(sample));

  const aster::SceneAsset asset =
      aster::importSceneAsset(sample, {{}, true, 1.0f, aster::AssetOriginPolicy::CenterOnGround});
  assert(asset.material_slots.size() == 2u);
  assert(asset.mesh_chunks.size() == 1u);
  assert(asset.mesh_chunks.front().material_slot == 1u);
  assert(asset.material_slots[1].has_base_color_texture);
  assert(asset.material_slots[1].has_metallic_roughness_texture);
  assert(asset.material_slots[1].has_normal_texture);
  assert(asset.material_slots[1].has_occlusion_texture);
  assert(asset.material_slots[1].material.double_sided);
  assert(!asset.mesh_chunks.front().mesh.vertices.empty());
  assert(asset.mesh_chunks.front().mesh.indices.size() == 23322u);
  assert(asset.mesh_chunks.front().diagnostics.generated_tangents == 23322u);

  float min_y = std::numeric_limits<float>::max();
  float max_radius = 0.0f;
  for (const aster::Vertex &vertex : asset.mesh_chunks.front().mesh.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    min_y = std::min(min_y, vertex.position.y);
    max_radius = std::max(max_radius, aster::length({vertex.position.x, 0.0f, vertex.position.z}));
  }
  expectNear(min_y, 0.0f, 0.001f);
  assert(max_radius > 0.1f);
}

void testBrushLevelMeshBuild() {
  const aster::CpuMesh mesh = aster::buildBrushLevelMesh(
      {
          {{0.0f, 1.0f, 0.0f}, {1.2f, 1.0f, 0.2f}, {}, aster::BrushVolume::Solid, 0, 0.08f},
          {{0.0f, 0.6f, 0.0f}, {0.3f, 0.6f, 0.4f}, {}, aster::BrushVolume::Air, 1, 0.0f},
      },
      {.uv_scale = 0.5f,
       .edge_softening = 0.04f,
       .noise_frequency = 0.5f,
       .noise_strength = 0.01f});

  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() > 36u);

  bool found_lower_door_triangle = false;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const aster::Vertex &a = mesh.vertices[mesh.indices[i + 0u]];
    const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.22f && centroid.y < 0.48f && std::abs(centroid.z) < 0.18f) {
      found_lower_door_triangle = true;
      break;
    }
  }
  assert(!found_lower_door_triangle);
}

void testArchitecturalMeshBuild() {
  const aster::CpuMesh gatehouse = aster::makeGatehouseMesh({.wall_half_width = 1.2f,
                                                             .wall_height = 1.8f,
                                                             .tower_half_width = 0.34f,
                                                             .tower_height = 2.2f,
                                                             .depth = 0.55f,
                                                             .door_half_width = 0.32f,
                                                             .door_height = 1.0f,
                                                             .parapet_blocks = 4});
  assert(!gatehouse.vertices.empty());
  assert(!gatehouse.indices.empty());

  bool found_open_gate = true;
  for (std::size_t i = 0; i < gatehouse.indices.size(); i += 3u) {
    const aster::Vertex &a = gatehouse.vertices[gatehouse.indices[i + 0u]];
    const aster::Vertex &b = gatehouse.vertices[gatehouse.indices[i + 1u]];
    const aster::Vertex &c = gatehouse.vertices[gatehouse.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.20f && centroid.y < 0.82f && std::abs(centroid.z) < 0.20f) {
      found_open_gate = false;
      break;
    }
  }
  assert(found_open_gate);

  const aster::CpuMesh ruined_castle = aster::makeRuinedCastleCourseMesh(
      {.half_width = 2.4f, .wall_height = 1.7f, .tower_height = 2.4f, .parapet_blocks = 7});
  assert(!ruined_castle.vertices.empty());
  assert(!ruined_castle.indices.empty());
}

void testVoxelStructureBuild() {
  std::vector<aster::VoxelCell> cells;
  aster::appendVoxelBox(cells, {0, 0, 0}, {2, 1, 1}, 3);
  const aster::VoxelStructure structure = aster::buildVoxelStructure(cells);
  assert(structure.mesh_batches.size() == 1u);
  assert(structure.mesh_batches.front().channel == 3u);
  assert(structure.mesh_batches.front().mesh.indices.size() == 10u * 6u);
  assert(structure.collision_boxes.size() == 1u);
  expectNear(structure.collision_boxes.front().center.x, 1.0f, 0.0001f);
  expectNear(structure.collision_boxes.front().half_extents.x, 1.0f, 0.0001f);

  std::vector<aster::VoxelCell> corner_cells;
  corner_cells.push_back({{0, 0, 0}, 1});
  corner_cells.push_back({{1, 0, 1}, 1});
  corner_cells.push_back({{0, 1, 1}, 1});
  const aster::VoxelStructure corner_structure = aster::buildVoxelStructure(corner_cells);
  bool found_occluded_vertex = false;
  for (const aster::VoxelMeshBatch &batch : corner_structure.mesh_batches) {
    for (const aster::Vertex &vertex : batch.mesh.vertices) {
      assert(vertex.ambient_occlusion >= 0.0f && vertex.ambient_occlusion <= 1.0f);
      found_occluded_vertex = found_occluded_vertex || vertex.ambient_occlusion < 0.70f;
    }
  }
  assert(found_occluded_vertex);
}

void testCastleCourseBuild() {
  const aster::CastleCourse course = aster::buildCastleCourse();
  assert(!course.structure.mesh_batches.empty());
  assert(!course.structure.collision_boxes.empty());
  assert(!course.box_elements.empty());
  assert(!course.ground_zones.empty());
  bool found_foundation = false;
  for (const aster::VoxelMeshBatch &batch : course.structure.mesh_batches) {
    found_foundation =
        found_foundation ||
        batch.channel == static_cast<std::uint32_t>(aster::CastleCourseChannel::Foundation);
  }
  assert(found_foundation);
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    min_x = std::min(min_x, box.center.x - box.half_extents.x);
    max_x = std::max(max_x, box.center.x + box.half_extents.x);
    min_z = std::min(min_z, box.center.z - box.half_extents.z);
    max_z = std::max(max_z, box.center.z + box.half_extents.z);
  }
  assert(max_x - min_x > 42.0f);
  assert(max_z - min_z > 34.0f);
  const aster::CastleCourseGroundZone &ground = course.ground_zones.front();
  assert(ground.half_extents.x > 20.0f);
  assert(ground.half_extents.y > 17.0f);
  assert(std::abs(ground.center.x) < 0.001f);
  const aster::Vec3 underpass_clearance{0.0f, 0.60f, -0.18f};
  bool underpass_blocked = false;
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    underpass_blocked =
        underpass_blocked || (std::abs(underpass_clearance.x - box.center.x) < box.half_extents.x &&
                              std::abs(underpass_clearance.y - box.center.y) < box.half_extents.y &&
                              std::abs(underpass_clearance.z - box.center.z) < box.half_extents.z);
  }
  assert(!underpass_blocked);
}

void testSceneContract() {
  const aster::Scene scene = aster::Scene::makeShowcase();
  assert(scene.objects().size() >= 3u);
  assert(scene.objects().front().primitive == aster::MeshPrimitive::Plane);
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::CourseCells);
  assert(scene.objects().front().material.pattern_depth > 0.0f);
}

const aster::SceneCoherenceContribution *findContribution(const aster::SceneCoherenceReport &report,
                                                          const aster::SceneCoherenceTerm term) {
  for (const aster::SceneCoherenceContribution &contribution : report.contributions) {
    if (contribution.term == term) {
      return &contribution;
    }
  }
  return nullptr;
}

void testSceneCoherenceEnergy() {
  aster::SceneCoherenceProblem coherent;
  coherent.visual.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.collision.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.navigation.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.routes.push_back(
      {"walkable", {{0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}}, 0.05f, 1.0f, 0.25f});
  coherent.solid_volumes.push_back({"wall", {3.0f, 0.0f, 0.0f}, {0.2f, 1.0f, 1.0f}});
  coherent.fluid_volumes.push_back({"water", {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
  coherent.ecological_samples.push_back({"aquatic", {0.2f, 0.0f, 0.0f}, 0.05f, 1.0f});
  coherent.fluid_interaction_segments.push_back(
      {"line", {-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 1.0f});
  coherent.affordance_samples.push_back(
      {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1.0f});
  coherent.material_fields.push_back(
      {"blend",
       {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, 1.0f}, {{0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}, 1.0f}},
       1.0f});
  coherent.visibility_probes.push_back({"camera",
                                        {0.0f, 0.0f, 0.0f},
                                        {{3.0f, 0.0f, 0.0f}},
                                        {{"room", {-0.5f, 0.0f, 0.0f}, {1.5f, 1.0f, 1.0f}}},
                                        {{"wall", {1.5f, 0.0f, 0.0f}, {0.1f, 1.0f, 1.0f}}}});
  coherent.light_samples.push_back(
      {{0.0f, 1.0f, 0.0f}, {0.6f, 0.5f, 0.4f}, {0.6f, 0.5f, 0.4f}, 1.0f});

  const aster::SceneCoherenceReport coherent_report = aster::evaluateSceneCoherence(coherent);
  assert(coherent_report.energy < 0.0001f);
  assert(aster::contains(coherent.fluid_volumes.front(), {0.0f, 0.0f, 0.0f}));
  assert(aster::segmentIntersectsVolume({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},
                                        coherent.fluid_volumes.front()));

  aster::SceneCoherenceProblem broken = coherent;
  broken.collision.samples.front().position = {2.0f, 0.0f, 0.0f};
  broken.ecological_samples.front().position = {4.0f, 0.0f, 0.0f};
  broken.fluid_interaction_segments.front().from = {3.0f, 0.0f, 0.0f};
  broken.fluid_interaction_segments.front().to = {4.0f, 0.0f, 0.0f};
  broken.visibility_probes.front().blockers.clear();
  const aster::SceneCoherenceReport broken_report = aster::evaluateSceneCoherence(broken);
  assert(broken_report.energy > coherent_report.energy);
  const aster::SceneCoherenceContribution *representation =
      findContribution(broken_report, aster::SceneCoherenceTerm::RepresentationCollision);
  const aster::SceneCoherenceContribution *fluid =
      findContribution(broken_report, aster::SceneCoherenceTerm::FluidContainment);
  const aster::SceneCoherenceContribution *visibility =
      findContribution(broken_report, aster::SceneCoherenceTerm::VisibilityLeak);
  assert(representation != nullptr && representation->raw_value > 0.0f);
  assert(fluid != nullptr && fluid->raw_value > 0.0f);
  assert(visibility != nullptr && visibility->raw_value > 0.0f);
}

void testSceneTraceValidation() {
  const std::vector<aster::SceneTraceRule> rules{
      aster::forbidTraceSymbol("camera", "CameraInsideWall"),
      aster::requireTraceSymbolWithin("path", "PathVisible", "PathContinues", 2u),
      aster::requireTraceSymbolSameFrame("water", "FishVisible", "FishInsideWater"),
      aster::forbidTraceSymbolSameFrame("water-surface", "FishVisible", "FishOnSurface")};

  aster::SceneSymbolicTrace valid;
  valid.frames.push_back({0.0, {"PathVisible", "PathContinues", "Walkable"}});
  valid.frames.push_back({0.1, {"FishVisible", "FishInsideWater"}});
  const aster::SceneTraceValidationReport valid_report = aster::validateSceneTrace(valid, rules);
  assert(valid_report.valid);
  assert(valid_report.defect_scale == aster::SceneTraceDefectScale::None);

  aster::SceneSymbolicTrace broken;
  broken.frames.push_back({0.0, {"PathVisible", "Walkable"}});
  broken.frames.push_back({0.1, {"Walkable"}});
  broken.frames.push_back({0.2, {"CameraInsideWall"}});
  broken.frames.push_back({0.3, {"FishVisible", "FishOnSurface"}});
  const aster::SceneTraceValidationReport broken_report = aster::validateSceneTrace(broken, rules);
  assert(!broken_report.valid);
  assert(broken_report.rank_proxy >= 1u);
  assert(broken_report.defect_scale != aster::SceneTraceDefectScale::None);

  const aster::SceneTraceSeparatorProfile profile =
      aster::estimateSceneTraceSeparatorProfile({valid}, {broken}, rules, 8u);
  assert(profile.separated);
  assert(profile.rank_proxy == 1u);
  assert(!profile.separating_rules.empty());
}

void testLumenSceneCoherenceReport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::SceneCoherenceReport &report = run.sceneCoherenceReport();
  assert(!report.contributions.empty());
  assert(report.energy >= 0.0f);
  assert(findContribution(report, aster::SceneCoherenceTerm::FluidContainment) != nullptr);
  assert(findContribution(report, aster::SceneCoherenceTerm::RouteCollision) != nullptr);
  const aster::SceneTraceValidationReport &trace_report = run.sceneTraceReport();
  assert(trace_report.trace_length > 0u);
  assert(!trace_report.rules.empty());
  assert(trace_report.valid);
}

void testLumenCameraCollisionCanBeatComfortRadius() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0, .playable_radius = 86.0f});
  const float radius = run.resolveCameraRadius({0.0f, 1.0f, 85.0f}, 0.0f, 0.0f, 6.0f);
  assert(radius < 1.0f);
}

void testLumenInnerPondSeamHasSupport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const auto &objects = run.scene().objects();

  const auto planar_half_extents = [](const aster::RenderObject &object) {
    aster::Vec2 half_extents{};
    if (object.custom_mesh == nullptr) {
      return half_extents;
    }
    for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
      half_extents.x =
          std::max(half_extents.x, std::abs(vertex.position.x * object.transform.scale.x));
      half_extents.y =
          std::max(half_extents.y, std::abs(vertex.position.z * object.transform.scale.z));
    }
    return half_extents;
  };

  const aster::RenderObject *largest_grass_surface = nullptr;
  float largest_grass_area = 0.0f;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::GrassSoil) {
      continue;
    }
    const aster::Vec2 half_extents = planar_half_extents(object);
    const float area = half_extents.x * half_extents.y;
    if (area > largest_grass_area) {
      largest_grass_area = area;
      largest_grass_surface = &object;
    }
  }
  assert(largest_grass_surface != nullptr);

  const aster::Vec2 seam_center{largest_grass_surface->transform.position.x,
                                largest_grass_surface->transform.position.z};
  const aster::RenderObject *seam_transition = nullptr;
  float closest_transition_distance = std::numeric_limits<float>::max();
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::TerrainBlend) {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - seam_center.x,
                            object.transform.position.z - seam_center.y};
    const float distance = aster::length(delta);
    if (distance < closest_transition_distance) {
      closest_transition_distance = distance;
      seam_transition = &object;
    }
  }
  assert(seam_transition != nullptr);
  const aster::Vec2 seam_radius = planar_half_extents(*seam_transition);
  assert(seam_radius.x > 1.0f && seam_radius.y > 1.0f);

  std::vector<const aster::RenderObject *> support_surfaces;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr) {
      continue;
    }
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
      support_surfaces.push_back(&object);
      break;
    default:
      break;
    }
  }
  assert(!support_surfaces.empty());

  constexpr int angular_samples = 64;
  const float radial_samples[] = {0.84f, 0.95f, 1.06f};
  for (const float radial : radial_samples) {
    for (int i = 0; i < angular_samples; ++i) {
      const float angle =
          static_cast<float>(i) / static_cast<float>(angular_samples) * aster::radians(360.0f);
      const aster::Vec2 point{seam_center.x + std::cos(angle) * seam_radius.x * radial,
                              seam_center.y + std::sin(angle) * seam_radius.y * radial};
      aster::TerrainSurfaceSample best;
      for (const aster::RenderObject *surface : support_surfaces) {
        const aster::TerrainSurfaceSample sample = aster::sampleMeshSupport(
            *surface->custom_mesh, surface->transform, aster::SurfaceSupportQuery{point}, 0.25f);
        if (sample.valid && (!best.valid || sample.height > best.height)) {
          best = sample;
        }
      }
      assert(best.valid);
    }
  }
}

void testLumenSupportSurfacesRenderOpaque() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  bool saw_support_surface = false;

  for (const aster::RenderObject &object : run.scene().objects()) {
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
    case aster::SurfacePattern::SoilPath:
    case aster::SurfacePattern::LayeredTerrain:
      saw_support_surface = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(!aster::isMaterialTranslucent(object.material));
      assert(aster::isDoubleSidedMaterial(object.material));
      break;
    default:
      break;
    }
  }

  assert(saw_support_surface);
}

void testLumenCaveVisualContracts() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  bool saw_cave_threshold = false;
  bool saw_cave_floor = false;
  bool saw_cave_chunk = false;
  bool saw_coal_ore = false;
  bool saw_wall_light_lens = false;

  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Walkable cave entrance threshold") {
      saw_cave_threshold = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);

      aster::Vec3 center{};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        center = center + vertex.position;
      }
      center = center / static_cast<float>(object.custom_mesh->vertices.size());
      const aster::TerrainSurfaceSample support =
          aster::sampleMeshSupport(*object.custom_mesh, object.transform,
                                   aster::SurfaceSupportQuery{{center.x, center.z}}, 0.36f);
      assert(support.valid);
    }
    if (object.name == "Walkable packed cave floor") {
      saw_cave_floor = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
    }
    if (object.name == "Chunked procedural cave interior") {
      saw_cave_chunk = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.viewer_cull_volume.enabled);
      assert(object.viewer_cull_volume.outside == aster::FaceCullMode::Back);
      assert(object.viewer_cull_volume.inside == aster::FaceCullMode::Back);
    }
    if (object.name == "Coal ore vein node") {
      saw_coal_ore = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.surface_pattern == aster::SurfacePattern::CoalVein);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Industrial caged wall sconce frosted glass") {
      saw_wall_light_lens = true;
      assert(object.material.surface_pattern == aster::SurfacePattern::AmberResin);
      assert(object.material.emission_strength > 0.5f);
      assert(!object.camera_occlusion_fade);
    }
  }

  assert(saw_cave_threshold);
  assert(saw_cave_floor);
  assert(saw_cave_chunk);
  assert(saw_coal_ore);
  assert(saw_wall_light_lens);
}

void testLumenPondWallLightIsMountedOutsideWater() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::RenderObject *inner_water = nullptr;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Deep inner castle pond water") {
      inner_water = &object;
      break;
    }
  }
  assert(inner_water != nullptr);
  assert(inner_water->custom_mesh != nullptr);

  aster::Vec2 water_radius{};
  for (const aster::Vertex &vertex : inner_water->custom_mesh->vertices) {
    water_radius.x =
        std::max(water_radius.x, std::abs(vertex.position.x * inner_water->transform.scale.x));
    water_radius.y =
        std::max(water_radius.y, std::abs(vertex.position.z * inner_water->transform.scale.z));
  }
  assert(water_radius.x > 1.0f && water_radius.y > 1.0f);

  const aster::Vec2 water_center{inner_water->transform.position.x,
                                 inner_water->transform.position.z};
  bool saw_wall_fixture = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name != "Industrial caged wall sconce frosted glass") {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - water_center.x,
                            object.transform.position.z - water_center.y};
    if (std::abs(delta.x) > water_radius.x + 4.0f || std::abs(delta.y) > water_radius.y + 4.0f) {
      continue;
    }
    saw_wall_fixture = true;
    const float normalized_footprint =
        std::sqrt((delta.x * delta.x) / (water_radius.x * water_radius.x) +
                  (delta.y * delta.y) / (water_radius.y * water_radius.y));
    assert(normalized_footprint > 1.18f);
    assert(object.transform.position.y > inner_water->transform.position.y + 0.70f);
  }
  assert(saw_wall_fixture);
}

void testControlScheme() {
  aster::ControlScheme controls;
  controls.addCommand("camera.orbit.left");
  controls.bind("camera.orbit.left", {aster::ControlDevice::Keyboard, 263});

  aster::ControlState state;
  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(state.justPressed("camera.orbit.left"));

  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(!state.justPressed("camera.orbit.left"));

  state.update(controls, {{}, {}, {}});
  assert(!state.pressed("camera.orbit.left"));
  assert(state.justReleased("camera.orbit.left"));
}

void testPlayerMotionPlan() {
  const aster::PlayerMovePlan plan = aster::buildPlayerMovePlan(
      {.walk_speed = 2.0f, .run_multiplier = 1.5f, .response_rate = 10.0f, .jump_speed = 5.0f},
      {{2.0f, 0.0f}, true, true});
  expectNear(plan.target_speed, 3.0f, 0.0001f);
  expectNear(aster::length({plan.input.desired_velocity.x, 0.0f, plan.input.desired_velocity.z}),
             3.0f, 0.0001f);
  assert(plan.input.jump_requested);
  expectNear(plan.character_settings.jump_speed, 5.0f, 0.0001f);
}

void testSwimMotionPlan() {
  const aster::PhysicsFluidSample fluid{true, 0.75f, 0.40f, 0.24f, {0.20f, 0.0f, 0.0f}};
  const aster::SwimMotionResult swim =
      aster::buildSwimMotion(fluid, {0.0f, -1.0f, 0.0f}, {{2.0f, 0.0f, 0.0f}, true},
                             {.activation_submersion = 0.25f,
                              .full_swim_submersion = 0.75f,
                              .horizontal_speed_scale = 0.5f,
                              .flow_influence = 0.5f,
                              .surface_clearance = 0.10f,
                              .float_response = 4.0f,
                              .max_upward_speed = 1.0f,
                              .max_downward_speed = 0.4f,
                              .ascend_speed = 1.2f});
  assert(swim.swimming);
  expectNear(swim.blend, 1.0f, 0.0001f);
  assert(swim.desired_velocity.x > 1.0f);
  assert(swim.target_vertical_velocity >= 1.2f);
}

void testClimbMotionPlan() {
  const aster::ClimbableCylinder tree{{0.0f, 0.0f, 0.0f}, 0.40f, 3.0f};
  const aster::ClimbSurfaceSample sample =
      aster::sampleClimbableCylinder(tree, {0.55f, 1.2f, 0.0f},
                                     {.capture_distance = 0.22f,
                                      .character_clearance = 0.16f,
                                      .stick_response = 10.0f,
                                      .tangent_speed_scale = 0.5f,
                                      .ascend_speed = 1.4f,
                                      .hold_vertical_speed = 0.1f,
                                      .max_correction = 0.12f});
  assert(sample.climbable);
  const aster::ClimbMotionResult climb = aster::buildClimbMotion(
      sample,
      {.desired_velocity = {0.0f, 0.0f, 1.0f}, .engage_requested = true, .ascend_requested = true},
      {.capture_distance = 0.22f,
       .character_clearance = 0.16f,
       .stick_response = 10.0f,
       .tangent_speed_scale = 0.5f,
       .ascend_speed = 1.4f,
       .hold_vertical_speed = 0.1f,
       .max_correction = 0.12f});
  assert(climb.climbing);
  assert(climb.desired_velocity.y >= 1.4f);
  assert(aster::length(climb.position_correction) > 0.0f);
}

void testAmphibiousPredatorMotion() {
  aster::AmphibiousPredatorState state;
  state.position = {-0.4f, 0.0f, 0.0f};
  const aster::AmphibiousPredatorUpdate update =
      aster::updateAmphibiousPredator(state,
                                      {.water_center = {0.0f, 0.0f, 0.0f},
                                       .water_radius = {2.0f, 1.0f},
                                       .water_surface_y = 0.2f,
                                       .body_height = 0.2f,
                                       .swim_speed = 0.5f,
                                       .shore_speed = 0.3f,
                                       .pursue_speed = 1.0f,
                                       .aggression = 0.5f,
                                       .notice_radius = 3.0f,
                                       .water_pursuit_margin = 0.1f,
                                       .strike_radius = 0.5f,
                                       .strike_cooldown = 1.0f},
                                      {0.0f, 0.2f, 0.0f}, 0.2f);
  assert(update.mode == aster::AmphibiousMotionMode::Pursue ||
         update.mode == aster::AmphibiousMotionMode::Strike);
  assert(aster::length(update.position - aster::Vec3{-0.4f, 0.0f, 0.0f}) > 0.0f);
}

void testAvatarRigSceneBinding() {
  aster::Material fur;
  fur.surface_pattern = aster::SurfacePattern::FurFibers;
  aster::Material face;
  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 0.7f,
                                                               .fur_material = fur,
                                                               .muzzle_material = face,
                                                               .eye_material = face,
                                                               .nose_material = face});
  assert(rig.parts.size() >= 12u);
  bool has_pointing_finger = false;
  for (const aster::AvatarPart &part : rig.parts) {
    has_pointing_finger = has_pointing_finger || part.role == aster::AvatarPartRole::PointingFinger;
  }
  assert(has_pointing_finger);
  const aster::AvatarBounds bounds = aster::avatarLocalBounds(rig);
  assert(bounds.min.y < -0.35f);
  assert(aster::avatarGroundSupportExtent(rig) > 0.35f);

  aster::Scene scene;
  const aster::AvatarInstance instance =
      aster::appendAvatar(scene, rig, {.position = {1.0f, 0.5f, -2.0f}});
  assert(instance.object_indices.size() == rig.parts.size());
  assert(scene.objects().size() == rig.parts.size());
  aster::applyAvatarPose(scene, rig, instance,
                         {.position = {1.0f, 0.5f, -2.0f},
                          .facing_yaw = aster::radians(45.0f),
                          .gait_phase = 1.0f,
                          .stride_amplitude = 0.5f,
                          .vertical_bob = 0.01f,
                          .head_yaw_offset = 0.35f,
                          .mouth_open = 1.0f});
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::FurFibers);
  assert(scene.objects().front().transform.position.y > 0.0f);
  bool saw_head_turn = false;
  bool saw_open_mouth = false;
  for (std::size_t i = 0; i < rig.parts.size(); ++i) {
    if (rig.parts[i].joint == aster::AvatarJoint::Head) {
      const std::size_t object_index = instance.object_indices[i];
      saw_head_turn = saw_head_turn || scene.objects()[object_index].transform.rotation.y > 1.0f;
    }
    if (rig.parts[i].joint == aster::AvatarJoint::Mouth) {
      const std::size_t object_index = instance.object_indices[i];
      saw_open_mouth = scene.objects()[object_index].transform.scale.y >
                       rig.parts[i].local_transform.scale.y * 3.0f;
    }
  }
  assert(saw_head_turn);
  assert(saw_open_mouth);

  aster::AvatarAnimatorState animator;
  const aster::AvatarPose pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .head_yaw_offset = aster::radians(30.0f),
                                   .mouth_open = 1.0f},
                                  1.0f / 60.0f);
  assert(animator.initialized);
  assert(pose.stride_amplitude > 0.0f);
  assert(pose.head_yaw_offset > 0.0f);
  assert(pose.mouth_open > 0.0f);
  const aster::AvatarPose point_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {},
                                   .desired_facing_yaw = 0.0f,
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .has_attention_target = true,
                                   .attention_target = {1.0f, 1.1f, 2.0f},
                                   .pointing_enabled = true},
                                  1.0f / 30.0f);
  assert(point_pose.point_blend > 0.0f);
  assert(std::abs(point_pose.point_yaw_offset) > 0.001f);
  const aster::AvatarPose swim_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .swim_blend = 1.0f},
                                  1.0f / 30.0f);
  assert(swim_pose.swim_blend > 0.0f);
}

void testThirdPersonFollowController() {
  aster::ThirdPersonFollowState state;
  const aster::ThirdPersonFollowPose pose =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = true,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {40.0f, -12.0f},
                                      .focus_target = {0.0f, 1.0f, 0.0f},
                                      .fallback_yaw = aster::radians(38.0f),
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(pose.active);
  assert(std::abs(pose.camera_yaw - aster::radians(38.0f)) > 0.001f);
  assert(pose.camera_pitch > aster::radians(28.0f));
  expectNear(pose.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose shifted =
      aster::updateThirdPersonFollow(state, {.target_response = 12.0f},
                                     {.active = true,
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(shifted.camera_target.x > 0.0f);
  assert(shifted.camera_target.x < 10.0f);
  const aster::ThirdPersonFollowPose snapped = aster::updateThirdPersonFollow(
      state, {.target_response = 12.0f, .teleport_snap_distance = 4.0f},
      {.active = true,
       .focus_target = {40.0f, 1.0f, 0.0f},
       .fallback_yaw = 0.0f,
       .fallback_pitch = aster::radians(28.0f)},
      1.0f / 60.0f);
  expectNear(snapped.camera_target.x, 40.0f, 0.0001f);
  expectNear(snapped.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose released =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = false,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {100.0f, 0.0f},
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(!released.active);
  expectNear(released.camera_yaw, snapped.camera_yaw, 0.0001f);

  const aster::Vec2 forward = aster::cameraRelativeMoveAxis({0.0f, 1.0f}, 0.0f);
  expectNear(forward.x, 0.0f, 0.0001f);
  expectNear(forward.y, -1.0f, 0.0001f);
  const aster::Vec2 rotated_forward =
      aster::cameraRelativeMoveAxis({0.0f, 1.0f}, aster::radians(90.0f));
  expectNear(rotated_forward.x, -1.0f, 0.0001f);
  expectNear(rotated_forward.y, 0.0f, 0.0001f);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = 0.0f;
  camera.radius = 4.0f;
  const aster::CameraRay ray = camera.screenRay({400.0f, 300.0f}, {800.0f, 600.0f});
  assert(aster::length(ray.direction) > 0.99f);
  assert(ray.direction.z < -0.99f);
}

void testContactVolumes() {
  const aster::VerticalCapsuleContactVolume lower{{0.0f, 0.40f, 0.0f}, 0.22f, 0.28f};
  const aster::VerticalCapsuleContactVolume same_level{{0.34f, 0.42f, 0.0f}, 0.22f, 0.30f};
  assert(aster::overlaps(lower, same_level));

  const aster::VerticalCapsuleContactVolume overhead{{0.10f, 2.20f, 0.0f}, 0.22f, 0.22f};
  assert(!aster::overlaps(lower, overhead));
}

void testPlacementValidation() {
  aster::PlacementValidator validator;
  validator.addForbiddenAabb(aster::makePlacementAabb({0.0f, 0.5f, 0.0f}, {1.0f, 0.5f, 1.0f}));
  assert(validator.rejectsPoint({0.2f, 0.4f, 0.2f}));
  assert(validator.allowsPoint({0.2f, 1.4f, 0.2f}));
  assert(validator.rejectsFootprint({{-0.2f, -0.2f}, {0.2f, 0.2f}}));
  assert(validator.allowsAabb(aster::makePlacementAabb({0.0f, 1.6f, 0.0f}, {0.2f, 0.1f, 0.2f})));

  validator.addForbiddenEllipse({{3.0f, 0.0f}, {0.5f, 0.75f}});
  assert(validator.rejectsFootprint({{2.8f, -0.1f}, {3.2f, 0.1f}}));
  assert(validator.allowsTriangleFootprint({4.0f, 0.0f, 0.0f}, {4.2f, 0.0f, 0.0f},
                                           {4.0f, 0.0f, 0.2f}));

  const aster::PlacementOrientedEllipse oriented_ellipse{.center = {0.0f, 4.0f},
                                                         .forward = {1.0f, 0.0f},
                                                         .radius = {0.75f, 1.0f},
                                                         .radius_forward_negative = 0.45f,
                                                         .radius_forward_positive = 2.0f};
  validator.addForbiddenOrientedEllipse(oriented_ellipse);
  assert(validator.rejectsPoint({1.5f, 0.0f, 4.0f}));
  assert(validator.allowsPoint({-0.75f, 0.0f, 4.0f}));
  assert(validator.rejectsFootprint({{1.85f, 3.95f}, {2.05f, 4.05f}}));
  expectNear(aster::normalizedDistance(oriented_ellipse, {1.0f, 4.0f}), 0.5f, 0.0001f);

  const aster::PlacementEllipse ellipse{{0.0f, 0.0f}, {2.0f, 1.0f}};
  assert(aster::contains(ellipse, aster::Vec2{1.0f, 0.0f}));
  assert(!aster::contains(ellipse, aster::Vec2{2.2f, 0.0f}));
  assert(aster::contains(ellipse,
                         aster::makePlacementFootprintFromBounds({-0.5f, -0.25f}, {0.5f, 0.25f})));
  const aster::PlacementFootprint edge_overlap =
      aster::makePlacementFootprintFromBounds({1.8f, -0.20f}, {2.2f, 0.20f});
  assert(aster::intersects(ellipse, edge_overlap));
  assert(!aster::contains(ellipse, edge_overlap));
  expectNear(aster::normalizedDistance(ellipse, {1.0f, 0.0f}), 0.5f, 0.0001f);
  const aster::PlacementEllipseBand shoreline_band{ellipse, 0.82f, 1.08f};
  assert(!aster::contains(shoreline_band, aster::Vec2{0.5f, 0.0f}));
  assert(aster::contains(shoreline_band, aster::Vec2{1.8f, 0.0f}));
  assert(!aster::contains(shoreline_band, aster::Vec2{2.4f, 0.0f}));
}

void testNetworkFrameCodec() {
  aster::net::NetMessage message;
  message.channel = 42;
  message.sequence = 9;
  message.source_node = 11;
  message.target_node = 12;
  message.payload = aster::net::bytesFromText("hello");

  std::vector<std::uint8_t> frame = aster::net::encodeFrame(message);
  aster::net::NetMessage decoded;
  assert(aster::net::decodeNextFrame(frame, decoded) == aster::net::FrameDecodeResult::Ready);
  assert(frame.empty());
  assert(decoded.channel == message.channel);
  assert(decoded.sequence == message.sequence);
  assert(decoded.source_node == message.source_node);
  assert(decoded.target_node == message.target_node);
  assert(aster::net::textFromBytes(decoded.payload) == "hello");

  std::vector<std::uint8_t> partial = aster::net::encodeFrame(message);
  partial.pop_back();
  assert(aster::net::decodeNextFrame(partial, decoded) ==
         aster::net::FrameDecodeResult::Incomplete);
}

void testNodeRouter() {
  aster::net::NodeRouter router;
  int delivered = 0;
  const auto token = router.subscribe(7, [&](const aster::net::NetMessage &message) {
    delivered += message.sequence == 5 ? 1 : 0;
  });

  aster::net::NetMessage message;
  message.channel = 7;
  message.sequence = 5;
  router.enqueue(message);
  assert(router.drainAll() == 1u);
  assert(delivered == 1);
  assert(router.stats().delivered == 1u);
  assert(router.unsubscribe(token));
  assert(!router.route(message));
  assert(router.stats().dropped == 1u);
}

void testPhysicsWorldGravityAndStaticContact() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle floor =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, -0.25f, 0.0f},
                     {4.0f, 0.25f, 4.0f}});
  const aster::PhysicsBodyHandle ball = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {0.0f, 2.0f, 0.0f},
                                                       {0.5f, 0.5f, 0.5f},
                                                       0.5f,
                                                       1.0f,
                                                       {0.45f, 0.0f}});

  for (int i = 0; i < 150; ++i) {
    world.step(1.0f / 60.0f);
  }

  const aster::PhysicsBody &body = world.body(ball);
  expectNear(body.position.y, 0.5f, 0.035f);
  assert(!world.contacts().empty());
}

void testPhysicsDistanceConstraint() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  const aster::PhysicsBodyHandle body = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {3.0f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f});
  [[maybe_unused]] const aster::PhysicsConstraintHandle constraint = world.addDistanceConstraint(
      {body, {0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 0.0f, aster::DistanceConstraintMode::Maximum});
  world.step(1.0f / 60.0f);
  assert(aster::length(world.body(body).position) <= 1.001f);
}

void testPhysicsQueriesAndDynamicContact() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle wall =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, 0.0f, -2.0f},
                     {0.7f, 0.7f, 0.12f}});
  const aster::PhysicsBodyHandle left = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {-0.22f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f,
                                                       1.0f,
                                                       {0.35f, 0.0f}});
  const aster::PhysicsBodyHandle right = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                        aster::PhysicsShapeType::Sphere,
                                                        {0.22f, 0.0f, 0.0f},
                                                        {0.25f, 0.25f, 0.25f},
                                                        0.25f,
                                                        1.0f,
                                                        {0.35f, 0.0f}});

  world.step(1.0f / 60.0f);
  assert(aster::length(world.body(left).position - world.body(right).position) >= 0.49f);

  aster::PhysicsRayHit ray_hit;
  assert(world.raycast({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, 3.0f}, ray_hit));
  assert(aster::samePhysicsHandle(ray_hit.body, wall));

  aster::PhysicsShapeCastHit cast_hit;
  assert(world.castSphere({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -3.0f}, 0.25f}, cast_hit));
  assert(aster::samePhysicsHandle(cast_hit.body, wall));

  const std::vector<aster::PhysicsOverlapHit> overlaps =
      world.overlapSphere({0.0f, 0.0f, 0.0f}, 0.7f);
  assert(overlaps.size() >= 2u);
}

void testPhysicsStaticTriangleMeshContact() {
  aster::CpuMesh wall_mesh;
  wall_mesh.vertices = {{{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                        {{0.0f, 1.2f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                        {{0.0f, 1.2f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                        {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}};
  wall_mesh.indices = {0, 1, 2, 0, 2, 3};

  aster::PhysicsWorld wall_world;
  wall_world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc wall_desc;
  wall_desc.type = aster::PhysicsBodyType::Static;
  wall_desc.shape = aster::PhysicsShapeType::TriangleMesh;
  wall_desc.mesh = std::make_shared<const aster::CpuMesh>(wall_mesh);
  [[maybe_unused]] const aster::PhysicsBodyHandle wall = wall_world.addBody(wall_desc);

  aster::PhysicsShapeCastHit center_sweep_hit;
  assert(
      wall_world.castSphere({{-1.0f, 0.60f, 0.0f}, {2.0f, 0.0f, 0.0f}, 0.25f}, center_sweep_hit));
  assert(aster::samePhysicsHandle(center_sweep_hit.body, wall));
  assert(center_sweep_hit.distance > 0.70f && center_sweep_hit.distance < 0.80f);

  aster::PhysicsShapeCastHit edge_sweep_hit;
  assert(wall_world.castSphere({{-1.0f, 0.60f, 1.12f}, {2.0f, 0.0f, 0.0f}, 0.25f}, edge_sweep_hit));
  assert(aster::samePhysicsHandle(edge_sweep_hit.body, wall));

  aster::PhysicsBodyDesc capsule_desc;
  capsule_desc.type = aster::PhysicsBodyType::Dynamic;
  capsule_desc.shape = aster::PhysicsShapeType::Capsule;
  capsule_desc.position = {-0.10f, 0.60f, 0.0f};
  capsule_desc.half_extents = {0.20f, 0.32f, 0.20f};
  capsule_desc.radius = 0.20f;
  capsule_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle capsule = wall_world.addBody(capsule_desc);
  wall_world.step(1.0f / 60.0f);
  assert(wall_world.body(capsule).position.x < -0.19f);

  aster::CpuMesh corridor_mesh;
  corridor_mesh.vertices = {{{-0.55f, 0.0f, -1.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                            {{-0.55f, 1.2f, -1.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                            {{-0.55f, 1.2f, 1.2f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                            {{-0.55f, 0.0f, 1.2f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                            {{0.55f, 0.0f, -1.2f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                            {{0.55f, 1.2f, -1.2f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                            {{0.55f, 1.2f, 1.2f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                            {{0.55f, 0.0f, 1.2f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}};
  corridor_mesh.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7};

  aster::PhysicsWorld corridor_world;
  corridor_world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc corridor_desc;
  corridor_desc.type = aster::PhysicsBodyType::Static;
  corridor_desc.shape = aster::PhysicsShapeType::TriangleMesh;
  corridor_desc.mesh = std::make_shared<const aster::CpuMesh>(corridor_mesh);
  [[maybe_unused]] const aster::PhysicsBodyHandle corridor = corridor_world.addBody(corridor_desc);

  capsule_desc.position = {0.0f, 0.60f, -1.0f};
  const aster::PhysicsBodyHandle walker = corridor_world.addBody(capsule_desc);
  corridor_world.setVelocity(walker, {0.0f, 0.0f, 1.4f});
  for (int i = 0; i < 80; ++i) {
    corridor_world.step(1.0f / 60.0f);
  }
  assert(corridor_world.body(walker).position.z > 0.60f);
  expectNear(corridor_world.body(walker).position.y, 0.60f, 0.001f);
}

void testPhysicsCharacterController() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle floor =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, -0.2f, 0.0f},
                     {4.0f, 0.2f, 4.0f}});

  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.55f, 0.0f};
  character_desc.half_extents = {0.25f, 0.30f, 0.25f};
  character_desc.radius = 0.25f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  for (int i = 0; i < 80; ++i) {
    (void)world.moveCharacter(character, {{1.6f, 0.0f, 0.0f}, false}, {}, 1.0f / 60.0f);
    world.step(1.0f / 60.0f);
  }

  aster::CharacterMoveResult state =
      world.moveCharacter(character, {{0.0f, 0.0f, 0.0f}, false}, {}, 1.0f / 60.0f);
  assert(state.grounded);
  assert(world.body(character).position.x > 0.4f);
}

void testTerrainCharacterContact() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.5f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::CharacterControllerSettings controller;
  controller.jump_speed = 4.0f;
  const aster::CharacterMoveResult state = aster::moveCharacterOnTerrain(
      world, character, terrain, {{0.0f, 0.0f, 0.0f}, true},
      {.controller = controller, .support_extent_y = 0.5f}, 1.0f / 60.0f);
  assert(state.grounded);
  expectNear(world.body(character).velocity.y, 4.0f, 0.0001f);
}

void testTerrainCharacterRaisesToSurface() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f};

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.50f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::CharacterControllerSettings controller;
  controller.ground_probe_distance = 0.5f;
  const aster::CharacterMoveResult state = aster::moveCharacterOnTerrain(
      world, character, terrain, {{0.0f, 0.0f, 0.0f}, false},
      {.controller = controller, .support_extent_y = 0.5f}, 1.0f / 60.0f);
  assert(state.grounded);
  expectNear(world.body(character).position.y, 0.85f, 0.0001f);
}

void testMeshSupportSurface() {
  aster::SupportSurfaceSet support;
  support.addMesh({std::make_shared<const aster::CpuMesh>(aster::makePlane(2.0f)),
                   {{0.0f, 0.24f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}},
                   0.5f});
  support.addBox({{1.0f, 0.50f, 1.0f}, {0.25f, 0.25f, 0.25f}});
  const aster::TerrainSurfaceSample sample = support.sample(aster::Vec2{0.0f, 0.0f});
  assert(sample.valid);
  expectNear(sample.height, 0.24f, 0.0001f);
  assert(sample.normal.y > 0.9f);
  const aster::TerrainSurfaceSample box_sample = support.sample(aster::Vec2{1.0f, 1.0f});
  assert(box_sample.valid);
  expectNear(box_sample.height, 0.75f, 0.0001f);
  const aster::TerrainSurfaceSample blocked_overhead =
      support.sample({{1.0f, 1.0f}, 0.10f, 0.12f, 2.0f});
  assert(!blocked_overhead.valid);
  const aster::TerrainSurfaceSample near_top = support.sample({{1.0f, 1.0f}, 0.66f, 0.12f, 2.0f});
  assert(near_top.valid);
  expectNear(near_top.height, 0.75f, 0.0001f);
}

void testSupportSurfaceTerrainPlacementFilter() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  aster::PlacementValidator validator;
  validator.addForbiddenAabb(aster::makePlacementAabb({0.0f, 0.0f, 0.0f}, {0.4f, 0.2f, 0.4f}));

  aster::SupportSurfaceSet support;
  support.setTerrain(&terrain);
  support.setTerrainPlacementValidator(validator);
  assert(!support.sample(aster::Vec2{0.0f, 0.0f}).valid);

  support.addBox({{0.0f, 0.20f, 0.0f}, {0.3f, 0.1f, 0.3f}});
  const aster::TerrainSurfaceSample raised = support.sample(aster::Vec2{0.0f, 0.0f});
  assert(raised.valid);
  expectNear(raised.height, 0.30f, 0.0001f);
}

void testSupportSurfaceOrientedTerrainPlacementFilter() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 5;
  terrain.square_size = 1.0f;
  terrain.origin = {-2.0f, -2.0f};
  terrain.heights.assign(25u, 2.0f);

  aster::PlacementValidator validator;
  validator.addForbiddenOrientedEllipse({.center = {0.0f, 0.0f},
                                         .forward = {1.0f, 0.0f},
                                         .radius = {0.65f, 1.0f},
                                         .radius_forward_negative = 0.50f,
                                         .radius_forward_positive = 1.80f});

  aster::SupportSurfaceSet support;
  support.setTerrain(&terrain);
  support.setTerrainPlacementValidator(validator);
  assert(!support.sample(aster::Vec2{1.0f, 0.0f}).valid);

  support.addBox({{1.0f, 0.20f, 0.0f}, {0.30f, 0.10f, 0.30f}});
  const aster::TerrainSurfaceSample cave_floor = support.sample(aster::Vec2{1.0f, 0.0f});
  assert(cave_floor.valid);
  expectNear(cave_floor.height, 0.30f, 0.0001f);

  const aster::TerrainSurfaceSample outside_cover = support.sample(aster::Vec2{-0.90f, 0.0f});
  assert(outside_cover.valid);
  expectNear(outside_cover.height, 2.0f, 0.0001f);
}

void testSurfaceStepAssist() {
  aster::SupportSurfaceSet support;
  support.addBox({{0.0f, 0.0f, 0.0f}, {0.45f, 0.05f, 0.45f}});
  support.addBox({{0.55f, 0.16f, 0.0f}, {0.30f, 0.16f, 0.45f}});

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.25f, 0.55f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  const auto sampler = [&](const aster::Vec3 position) {
    return support.sample({{position.x, position.z}, position.y, 0.40f, 0.20f});
  };
  const aster::CharacterMoveResult assisted =
      aster::applySurfaceStepAssist(world, character, sampler, {1.0f, 0.0f, 0.0f},
                                    {.support_extent_y = 0.50f,
                                     .probe_distance = 0.32f,
                                     .max_step_up = 0.35f,
                                     .max_step_down = 0.05f,
                                     .min_horizontal_speed = 0.1f,
                                     .skin_width = 0.0f});
  assert(assisted.grounded);
  expectNear(world.body(character).position.y, 0.82f, 0.0001f);
}

void testContinuousHorizontalCollisionBlocksFastSweep() {
  constexpr std::uint32_t world_layer = 1u << 0u;
  constexpr std::uint32_t character_layer = 1u << 1u;

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {-1.0f, 0.0f, 0.0f};
  character_desc.half_extents = {0.25f, 0.40f, 0.25f};
  character_desc.radius = 0.25f;
  character_desc.filter = {character_layer, world_layer};
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::PhysicsBodyDesc wall_desc;
  wall_desc.type = aster::PhysicsBodyType::Static;
  wall_desc.shape = aster::PhysicsShapeType::Box;
  wall_desc.position = {0.0f, 0.0f, 0.0f};
  wall_desc.half_extents = {0.10f, 1.0f, 1.0f};
  wall_desc.filter = {world_layer, character_layer};
  [[maybe_unused]] const aster::PhysicsBodyHandle wall = world.addBody(wall_desc);

  aster::PhysicsBody &body = world.body(character);
  body.previous_position = {-1.0f, 0.0f, 0.0f};
  body.position = {1.0f, 0.0f, 0.0f};
  body.velocity = {12.0f, 0.0f, 0.0f};

  const bool resolved = aster::resolveContinuousHorizontalCollision(
      world, character, {.radius = 0.25f, .collision_mask = world_layer});
  assert(resolved);
  assert(world.body(character).position.x < -0.30f);
  assert(world.body(character).velocity.x <= 0.0001f);
}

void testPhysicsFluidVolumeDragAndBuoyancy() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  const aster::PhysicsBodyHandle ball = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {0.0f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f,
                                                       1.0f,
                                                       {0.20f, 0.0f}});
  [[maybe_unused]] const aster::PhysicsFluidHandle water = world.addFluidVolume(
      {{0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 2.0f}, 0.4f, 1.25f, 5.0f, {0.0f, 0.0f, 0.0f}});
  world.setVelocity(ball, {3.0f, -2.0f, 0.0f});
  world.step(1.0f / 30.0f);

  const aster::PhysicsBody &body = world.body(ball);
  assert(body.velocity.x < 3.0f);
  assert(body.velocity.y > -2.0f);
  const aster::PhysicsFluidSample sample = world.sampleFluid(ball);
  assert(sample.submerged);
  expectNear(sample.surface_y, 0.4f, 0.0001f);
  assert(sample.depth_below_surface > 0.0f);
}

} // namespace

int main() {
  testVectorMath();
  testMatrixComposition();
  testTransformContract();
  testColorPipeline();
  testFixedTimestep();
  testFrameTimeStats();
  testGameplayItemInteractionSystems();
  testMeshGeneration();
  testTerrainSculpting();
  testCaveInteriorVolume();
  testMeshProcessingPipeline();
  testMeshProcessingRejectsInvalidIndices();
  testMeshDrapingRaisesEmbeddedVertices();
  testSceneAssetNormalTangentImport();
  testBrushLevelMeshBuild();
  testArchitecturalMeshBuild();
  testVoxelStructureBuild();
  testCastleCourseBuild();
  testSceneContract();
  testSceneCoherenceEnergy();
  testSceneTraceValidation();
  testLumenSceneCoherenceReport();
  testLumenCameraCollisionCanBeatComfortRadius();
  testLumenInnerPondSeamHasSupport();
  testLumenSupportSurfacesRenderOpaque();
  testLumenCaveVisualContracts();
  testLumenPondWallLightIsMountedOutsideWater();
  testControlScheme();
  testPlayerMotionPlan();
  testSwimMotionPlan();
  testClimbMotionPlan();
  testAmphibiousPredatorMotion();
  testAvatarRigSceneBinding();
  testThirdPersonFollowController();
  testContactVolumes();
  testPlacementValidation();
  testNetworkFrameCodec();
  testNodeRouter();
  testPhysicsWorldGravityAndStaticContact();
  testPhysicsDistanceConstraint();
  testPhysicsQueriesAndDynamicContact();
  testPhysicsStaticTriangleMeshContact();
  testPhysicsCharacterController();
  testTerrainCharacterContact();
  testTerrainCharacterRaisesToSurface();
  testMeshSupportSurface();
  testSupportSurfaceTerrainPlacementFilter();
  testSupportSurfaceOrientedTerrainPlacementFilter();
  testSurfaceStepAssist();
  testContinuousHorizontalCollisionBlocksFastSweep();
  testPhysicsFluidVolumeDragAndBuoyancy();
  std::cout << "Aster tests passed.\n";
  return 0;
}
