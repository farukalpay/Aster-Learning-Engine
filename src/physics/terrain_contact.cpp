// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/terrain_contact.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr float kEpsilon = 0.000001f;

aster::Vec3 bodySupportPosition(const aster::PhysicsBody &body, const float support_extent_y) {
  return {body.position.x, body.position.y - support_extent_y, body.position.z};
}

float bodySupportRadius(const aster::PhysicsBody &body, const float configured_radius) {
  if (configured_radius > 0.0f) {
    return configured_radius;
  }
  if (body.shape == aster::PhysicsShapeType::Sphere ||
      body.shape == aster::PhysicsShapeType::Capsule) {
    return body.radius;
  }
  return std::min(body.half_extents.x, body.half_extents.z);
}

float supportHeight(const aster::TerrainSurfaceSample &sample, const float support_extent_y) {
  return sample.height + support_extent_y;
}

aster::TerrainSurfaceSample higherSurfaceSample(const aster::TerrainSurfaceSample lhs,
                                                const aster::TerrainSurfaceSample rhs) {
  if (!lhs.valid) {
    return rhs;
  }
  if (!rhs.valid) {
    return lhs;
  }
  return rhs.height > lhs.height ? rhs : lhs;
}

aster::TerrainSurfaceSample
sampleSupportFootprint(const aster::TerrainSurfaceQuerySampler &surface_sampler,
                       const aster::Vec3 support_position, const float support_radius,
                       const int support_sample_count) {
  aster::TerrainSurfaceSample best = surface_sampler(support_position);
  const float radius = std::max(support_radius, 0.0f);
  const int sample_count = std::clamp(support_sample_count, 0, 16);
  if (radius <= kEpsilon || sample_count <= 0) {
    return best;
  }

  constexpr float kPi = 3.14159265358979323846f;
  const float effective_radius = std::max(radius * 0.86f, 0.0f);
  for (int sample_index = 0; sample_index < sample_count; ++sample_index) {
    const float angle =
        (static_cast<float>(sample_index) / static_cast<float>(sample_count)) * kPi * 2.0f;
    const aster::Vec3 offset{std::cos(angle) * effective_radius, 0.0f,
                             std::sin(angle) * effective_radius};
    best = higherSurfaceSample(best, surface_sampler(support_position + offset));
  }

  const float half_radius = effective_radius * 0.50f;
  for (int sample_index = 0; sample_index < sample_count; sample_index += 2) {
    const float angle =
        ((static_cast<float>(sample_index) + 0.5f) / static_cast<float>(sample_count)) * kPi * 2.0f;
    const aster::Vec3 offset{std::cos(angle) * half_radius, 0.0f, std::sin(angle) * half_radius};
    best = higherSurfaceSample(best, surface_sampler(support_position + offset));
  }

  return best;
}

bool terrainGrounded(const aster::TerrainSurfaceSample &sample, const aster::PhysicsBody &body,
                     const float support_extent_y,
                     const aster::CharacterControllerSettings &settings, float &ground_distance) {
  if (!sample.valid) {
    ground_distance = 0.0f;
    return false;
  }
  ground_distance = body.position.y - supportHeight(sample, support_extent_y);
  const float slope_limit = std::cos(std::max(settings.max_slope_angle, 0.0f));
  return sample.normal.y >= slope_limit &&
         ground_distance <= std::max(settings.ground_probe_distance, 0.0f);
}

} // namespace

namespace aster {

CharacterMoveResult moveCharacterOnTerrain(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                           const TerrainHeightField &terrain,
                                           const CharacterMoveInput &input,
                                           const TerrainCharacterSettings &settings,
                                           const float dt) {
  return moveCharacterOnSurface(
      world, handle,
      [&](const Vec3 position) { return sampleTerrain(terrain, {position.x, position.z}); }, input,
      settings, dt);
}

CharacterMoveResult moveCharacterOnSurface(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                           const TerrainSurfaceSampler &surface_sampler,
                                           const CharacterMoveInput &input,
                                           const TerrainCharacterSettings &settings,
                                           const float dt) {
  if (!surface_sampler) {
    throw std::invalid_argument("Terrain character surface sampler is empty.");
  }
  return moveCharacterOnSurface(
      world, handle, [&](const Vec3 position) { return surface_sampler({position.x, position.z}); },
      input, settings, dt);
}

CharacterMoveResult moveCharacterOnSurface(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                           const TerrainSurfaceQuerySampler &surface_sampler,
                                           const CharacterMoveInput &input,
                                           const TerrainCharacterSettings &settings,
                                           const float dt) {
  if (!world.valid(handle)) {
    throw std::invalid_argument("Terrain character body handle is invalid.");
  }
  if (!surface_sampler) {
    throw std::invalid_argument("Terrain character surface sampler is empty.");
  }
  PhysicsBody &character = world.body(handle);
  if (character.type != PhysicsBodyType::Dynamic) {
    throw std::invalid_argument("Terrain character controller requires a dynamic body.");
  }

  CharacterControllerSettings controller = settings.controller;
  controller.ground_probe_distance = std::max(controller.ground_probe_distance, 0.0f);
  const float support_extent_y = std::max(settings.support_extent_y, 0.0f);
  const TerrainSurfaceSample sample = sampleSupportFootprint(
      surface_sampler, bodySupportPosition(character, support_extent_y),
      bodySupportRadius(character, settings.support_radius), settings.support_sample_count);

  float ground_distance = 0.0f;
  const bool grounded =
      terrainGrounded(sample, character, support_extent_y, controller, ground_distance);
  if (grounded && (ground_distance < 0.0f || character.velocity.y <= 0.0f)) {
    character.position.y = supportHeight(sample, support_extent_y);
    ground_distance = 0.0f;
  }

  const Vec3 desired_horizontal{input.desired_velocity.x, 0.0f, input.desired_velocity.z};
  Vec3 horizontal{character.velocity.x, 0.0f, character.velocity.z};
  const bool wants_motion = length(desired_horizontal) > kEpsilon;
  const float response_rate =
      wants_motion ? (grounded ? controller.acceleration : controller.air_acceleration)
                   : controller.braking;
  const float response = 1.0f - std::exp(-std::max(response_rate, 0.0f) * std::max(dt, 0.0f));
  horizontal = horizontal + (desired_horizontal - horizontal) * response;
  character.velocity.x = horizontal.x;
  character.velocity.z = horizontal.z;

  if (grounded && character.velocity.y < 0.0f) {
    character.velocity.y = 0.0f;
  }
  if (grounded && input.jump_requested && controller.jump_speed > 0.0f) {
    character.velocity.y = controller.jump_speed;
  }

  world.wakeBody(handle);
  return {grounded, sample.valid ? sample.normal : Vec3{0.0f, 1.0f, 0.0f}, character.velocity,
          ground_distance};
}

CharacterMoveResult solveTerrainContact(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                        const TerrainHeightField &terrain,
                                        TerrainContactSettings settings) {
  return solveSurfaceContact(
      world, handle,
      [&](const Vec3 position) { return sampleTerrain(terrain, {position.x, position.z}); },
      settings);
}

CharacterMoveResult solveSurfaceContact(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                        const TerrainSurfaceSampler &surface_sampler,
                                        TerrainContactSettings settings) {
  if (!surface_sampler) {
    return {};
  }
  return solveSurfaceContact(
      world, handle, [&](const Vec3 position) { return surface_sampler({position.x, position.z}); },
      settings);
}

CharacterMoveResult solveSurfaceContact(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                        const TerrainSurfaceQuerySampler &surface_sampler,
                                        TerrainContactSettings settings) {
  if (!world.valid(handle)) {
    return {};
  }
  if (!surface_sampler) {
    return {};
  }

  PhysicsBody &body = world.body(handle);
  const float support_extent_y = std::max(settings.support_extent_y, 0.0f);
  const TerrainSurfaceSample sample = sampleSupportFootprint(
      surface_sampler, bodySupportPosition(body, support_extent_y),
      bodySupportRadius(body, settings.support_radius), settings.support_sample_count);
  if (!sample.valid) {
    return {};
  }

  const float floor_y =
      supportHeight(sample, support_extent_y) + std::max(settings.skin_width, 0.0f);
  const float ground_distance = body.position.y - supportHeight(sample, support_extent_y);
  const float correction_distance = floor_y - body.position.y;
  bool corrected = false;
  if (body.position.y < floor_y &&
      correction_distance <= std::max(settings.max_correction_distance, 0.0f)) {
    body.position.y = floor_y;
    corrected = true;
    if (body.velocity.y < 0.0f) {
      body.velocity.y = 0.0f;
    }
    world.wakeBody(handle);
  }

  const bool grounded =
      corrected || (std::abs(ground_distance) <= std::max(settings.skin_width, 0.0f) * 2.0f &&
                    sample.normal.y > 0.0f);
  return {grounded, sample.normal, body.velocity, ground_distance};
}

CharacterMoveResult applySurfaceStepAssist(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                           const TerrainSurfaceQuerySampler &surface_sampler,
                                           const Vec3 desired_velocity,
                                           SurfaceStepAssistSettings settings) {
  if (!world.valid(handle) || !surface_sampler) {
    return {};
  }

  PhysicsBody &body = world.body(handle);
  const Vec3 horizontal{desired_velocity.x, 0.0f, desired_velocity.z};
  const float horizontal_speed = length(horizontal);
  if (horizontal_speed < std::max(settings.min_horizontal_speed, 0.0f)) {
    return {};
  }

  const float support_extent_y = std::max(settings.support_extent_y, 0.0f);
  const Vec3 support_position = bodySupportPosition(body, support_extent_y);
  const Vec3 direction = horizontal / horizontal_speed;
  const Vec3 probe_position =
      support_position + direction * std::max(settings.probe_distance, 0.0f);
  const TerrainSurfaceSample sample = sampleSupportFootprint(
      surface_sampler, {probe_position.x, support_position.y, probe_position.z},
      bodySupportRadius(body, settings.support_radius), settings.support_sample_count);
  if (!sample.valid || sample.normal.y <= 0.0f) {
    return {};
  }

  const float current_floor = support_position.y;
  const float delta = sample.height - current_floor;
  if (delta > std::max(settings.max_step_up, 0.0f) ||
      delta < -std::max(settings.max_step_down, 0.0f)) {
    return {};
  }

  const float target_y =
      supportHeight(sample, support_extent_y) + std::max(settings.skin_width, 0.0f);
  if (target_y <= body.position.y) {
    return {};
  }

  body.position.y = target_y;
  if (body.velocity.y < 0.0f) {
    body.velocity.y = 0.0f;
  }
  world.wakeBody(handle);
  return {true, sample.normal, body.velocity, delta};
}

bool resolveContinuousHorizontalCollision(PhysicsWorld &world, const PhysicsBodyHandle handle,
                                          ContinuousCharacterCollisionSettings settings) {
  if (!world.valid(handle)) {
    return false;
  }

  PhysicsBody &body = world.body(handle);
  const float radius = bodySupportRadius(body, settings.radius);
  if (radius <= kEpsilon) {
    return false;
  }

  const Vec3 swept_delta = body.position - body.previous_position;
  const Vec3 horizontal_delta{swept_delta.x, 0.0f, swept_delta.z};
  const float horizontal_distance = length(horizontal_delta);
  const float minimum_displacement =
      radius * std::max(settings.minimum_displacement_fraction, 0.0f);
  if (horizontal_distance <= minimum_displacement) {
    return false;
  }

  PhysicsSphereCast sweep;
  sweep.origin = {body.previous_position.x, body.position.y, body.previous_position.z};
  sweep.displacement = horizontal_delta;
  sweep.radius = radius * std::clamp(settings.sweep_radius_scale, 0.0f, 1.0f);
  sweep.filter.collides_with = settings.collision_mask;
  sweep.filter.ignore_body = handle;

  PhysicsShapeCastHit hit;
  if (!world.castSphere(sweep, hit) || hit.fraction >= 1.0f) {
    return false;
  }

  const float skin =
      std::min(radius * std::max(settings.skin_width_fraction, 0.0f), horizontal_distance);
  const float safe_fraction = std::max((hit.distance - skin) / horizontal_distance, 0.0f);
  body.position.x = sweep.origin.x + horizontal_delta.x * safe_fraction;
  body.position.z = sweep.origin.z + horizontal_delta.z * safe_fraction;

  const float inward_speed = dot(body.velocity, hit.normal);
  if (inward_speed < 0.0f) {
    body.velocity = body.velocity - hit.normal * inward_speed;
  }
  world.wakeBody(handle);
  return true;
}

} // namespace aster
