// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

void OrbitCamera::orbit(const float yaw_delta, const float pitch_delta) {
  yaw += yaw_delta;
  pitch = std::clamp(pitch + pitch_delta, radians(-80.0f), radians(80.0f));
}

void OrbitCamera::zoom(const float radius_delta) {
  radius = std::clamp(radius + radius_delta, 1.8f, 24.0f);
}

Vec3 OrbitCamera::position() const {
  const float cp = std::cos(pitch);
  return {
      target.x + radius * cp * std::sin(yaw),
      target.y + radius * std::sin(pitch),
      target.z + radius * cp * std::cos(yaw),
  };
}

CameraRay OrbitCamera::screenRay(const Vec2 pointer, const Vec2 viewport_size) const {
  const float width = std::max(viewport_size.x, 1.0f);
  const float height = std::max(viewport_size.y, 1.0f);
  const float aspect = width / height;
  const float ndc_x = pointer.x / width * 2.0f - 1.0f;
  const float ndc_y = 1.0f - pointer.y / height * 2.0f;
  const float tan_half_fov = std::tan(vertical_fov * 0.5f);

  const Vec3 origin = position();
  const Vec3 forward = normalize(target - origin);
  Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  if (length(right) <= 0.0001f) {
    right = {1.0f, 0.0f, 0.0f};
  }
  const Vec3 up = normalize(cross(right, forward));
  return {origin, normalize(forward + right * (ndc_x * aspect * tan_half_fov) +
                            up * (ndc_y * tan_half_fov))};
}

Mat4 OrbitCamera::viewMatrix() const {
  return look_at(position(), target, {0.0f, 1.0f, 0.0f});
}

Mat4 OrbitCamera::projectionMatrix(const float aspect_ratio) const {
  return perspective(vertical_fov, aspect_ratio, near_plane, far_plane);
}

} // namespace aster
