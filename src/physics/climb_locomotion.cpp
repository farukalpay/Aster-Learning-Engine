// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/climb_locomotion.hpp"

#include <algorithm>

namespace aster {

ClimbSurfaceSample sampleClimbableCylinder(const ClimbableCylinder &cylinder,
                                           const Vec3 character_position,
                                           ClimbMotionSettings settings) {
  ClimbSurfaceSample sample;
  if (cylinder.radius <= 0.0f || cylinder.height <= 0.0f) {
    return sample;
  }

  const float capture = std::max(settings.capture_distance, 0.0f);
  const Vec2 planar_delta{character_position.x - cylinder.base.x,
                          character_position.z - cylinder.base.z};
  const float planar_distance = length(planar_delta);
  if (planar_distance > cylinder.radius + capture) {
    return sample;
  }

  const float local_y = character_position.y - cylinder.base.y;
  if (local_y < -capture || local_y > cylinder.height + capture) {
    return sample;
  }

  Vec2 normal2 = planar_distance > 0.0001f ? planar_delta / planar_distance : Vec2{1.0f, 0.0f};
  sample.climbable = true;
  sample.outward_normal = {normal2.x, 0.0f, normal2.y};
  sample.nearest_point = {
      cylinder.base.x + normal2.x * cylinder.radius,
      std::clamp(character_position.y, cylinder.base.y, cylinder.base.y + cylinder.height),
      cylinder.base.z + normal2.y * cylinder.radius};
  sample.height_fraction =
      std::clamp((sample.nearest_point.y - cylinder.base.y) / cylinder.height, 0.0f, 1.0f);
  sample.clearance = planar_distance - cylinder.radius;
  return sample;
}

ClimbMotionResult buildClimbMotion(const ClimbSurfaceSample &surface, const ClimbMotionInput &input,
                                   ClimbMotionSettings settings) {
  ClimbMotionResult result;
  result.desired_velocity = input.desired_velocity;
  if (!surface.climbable || !input.engage_requested) {
    return result;
  }

  settings.character_clearance = std::max(settings.character_clearance, 0.0f);
  settings.max_correction = std::max(settings.max_correction, 0.0f);

  const Vec3 horizontal{input.desired_velocity.x, 0.0f, input.desired_velocity.z};
  Vec3 tangent = horizontal - surface.outward_normal * dot(horizontal, surface.outward_normal);
  const float tangent_speed = length(tangent);
  if (tangent_speed > 0.0001f) {
    tangent = tangent / tangent_speed;
  }

  const float target_clearance = settings.character_clearance;
  const float radial_error = target_clearance - surface.clearance;
  Vec3 correction = surface.outward_normal *
                    std::clamp(radial_error, -settings.max_correction, settings.max_correction);
  if (surface.height_fraction <= 0.01f && correction.y < 0.0f) {
    correction.y = 0.0f;
  }

  const float vertical = input.ascend_requested ? std::max(settings.ascend_speed, 0.0f)
                                                : std::max(settings.hold_vertical_speed, 0.0f);
  result.climbing = true;
  result.blend = 1.0f;
  result.position_correction = correction;
  result.desired_velocity =
      tangent * (tangent_speed * std::max(settings.tangent_speed_scale, 0.0f)) +
      Vec3{0.0f, vertical, 0.0f};
  return result;
}

} // namespace aster
