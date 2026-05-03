// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"

namespace aster {

struct CameraRay {
  Vec3 origin{};
  Vec3 direction{0.0f, 0.0f, -1.0f};
};

class OrbitCamera {
public:
  void orbit(float yaw_delta, float pitch_delta);
  void zoom(float radius_delta);

  [[nodiscard]] Vec3 position() const;
  [[nodiscard]] CameraRay screenRay(Vec2 pointer, Vec2 viewport_size) const;
  [[nodiscard]] Mat4 viewMatrix() const;
  [[nodiscard]] Mat4 projectionMatrix(float aspect_ratio) const;

  Vec3 target{0.0f, 0.9f, 0.0f};
  float yaw = radians(36.0f);
  float pitch = radians(18.0f);
  float radius = 6.2f;
  float vertical_fov = radians(54.0f);
  float near_plane = 0.05f;
  float far_plane = 120.0f;
};

} // namespace aster
