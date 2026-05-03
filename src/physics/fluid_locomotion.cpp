// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/fluid_locomotion.hpp"

#include <algorithm>

namespace aster {

SwimMotionResult buildSwimMotion(const PhysicsFluidSample &fluid, const Vec3 &current_velocity,
                                 const SwimMotionInput &input, SwimMotionSettings settings) {
  settings.activation_submersion = std::clamp(settings.activation_submersion, 0.0f, 1.0f);
  settings.full_swim_submersion =
      std::max(settings.full_swim_submersion, settings.activation_submersion + 0.001f);

  SwimMotionResult result;
  result.desired_velocity = input.desired_velocity;
  if (!fluid.submerged || fluid.submersion < settings.activation_submersion) {
    result.target_vertical_velocity = current_velocity.y;
    return result;
  }

  result.swimming = true;
  result.blend = std::clamp((fluid.submersion - settings.activation_submersion) /
                                (settings.full_swim_submersion - settings.activation_submersion),
                            0.0f, 1.0f);

  const Vec3 horizontal{input.desired_velocity.x, 0.0f, input.desired_velocity.z};
  const Vec3 flow{fluid.flow.x, 0.0f, fluid.flow.z};
  result.desired_velocity = horizontal * std::max(settings.horizontal_speed_scale, 0.0f) +
                            flow * std::clamp(settings.flow_influence, 0.0f, 1.0f);

  const float float_error = fluid.depth_below_surface - std::max(settings.surface_clearance, 0.0f);
  float target_y = std::clamp(float_error * std::max(settings.float_response, 0.0f),
                              -std::max(settings.max_downward_speed, 0.0f),
                              std::max(settings.max_upward_speed, 0.0f));
  if (input.ascend_requested) {
    target_y = std::max(target_y, std::max(settings.ascend_speed, 0.0f));
  }
  result.target_vertical_velocity = target_y;
  return result;
}

} // namespace aster
