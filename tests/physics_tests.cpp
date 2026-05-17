// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

namespace {

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

void testPhysicsBroadphaseGeneratedParity() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 1, 1.0f / 120.0f});

  auto add_box = [&](const aster::PhysicsBodyType type, const aster::Vec3 position) {
    aster::PhysicsBodyDesc desc;
    desc.type = type;
    desc.shape = aster::PhysicsShapeType::Box;
    desc.position = position;
    desc.half_extents = {0.5f, 0.5f, 0.5f};
    desc.mass = 1.0f;
    desc.filter = {1u, 1u};
    return world.addBody(desc);
  };
  [[maybe_unused]] const aster::PhysicsBodyHandle dynamic_box =
      add_box(aster::PhysicsBodyType::Dynamic, {0.0f, 0.0f, 0.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle static_overlap =
      add_box(aster::PhysicsBodyType::Static, {0.6f, 0.0f, 0.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle static_far =
      add_box(aster::PhysicsBodyType::Static, {3.0f, 0.0f, 0.0f});

  aster::PhysicsBodyDesc sphere;
  sphere.type = aster::PhysicsBodyType::Dynamic;
  sphere.shape = aster::PhysicsShapeType::Sphere;
  sphere.position = {0.0f, 0.75f, 0.0f};
  sphere.radius = 0.4f;
  sphere.mass = 1.0f;
  sphere.filter = {1u, 1u};
  [[maybe_unused]] const aster::PhysicsBodyHandle dynamic_sphere = world.addBody(sphere);

  world.step(1.0f / 120.0f);
  std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
  for (const aster::PhysicsBroadphasePair &pair : world.broadphasePairs()) {
    pairs.push_back({pair.body_a.index, pair.body_b.index});
  }
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> expected = {
      {0u, 1u}, {0u, 3u}, {1u, 3u}};
  assert(pairs == expected);
}

} // namespace

int main() {
  testContactVolumes();
  testPlacementValidation();
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
  testPhysicsBroadphaseGeneratedParity();
  std::cout << "physics_tests passed.\n";
  return 0;
}
