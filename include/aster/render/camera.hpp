// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"

#include <array>
#include <cstddef>

namespace aster {

using CameraRay = WorldRay;

enum class CameraProjectionMode {
  Perspective,
  Orthographic,
};

enum class FrustumPlane {
  Left,
  Right,
  Bottom,
  Top,
  Near,
  Far,
};

struct CameraFrustum {
  std::array<Plane3, 6> planes{};

  [[nodiscard]] const Plane3 &operator[](const FrustumPlane plane) const {
    return planes[static_cast<std::size_t>(plane)];
  }
};

struct Camera {
  Vec3 eye{0.0f, 0.0f, 1.0f};
  Vec3 target{0.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  CameraProjectionMode projection_mode = CameraProjectionMode::Perspective;
  ProjectionPolicy projection_policy = defaultProjectionPolicy();
  float vertical_fov = radians(54.0f);
  float orthographic_height = 8.0f;
  float near_plane = 0.05f;
  float far_plane = 120.0f;
  Vec2 jitter{};

  [[nodiscard]] WorldToView viewMatrix() const;
  [[nodiscard]] ViewToClip projectionMatrix(float aspect_ratio) const;
  [[nodiscard]] WorldToClip viewProjectionMatrix(float aspect_ratio) const;
  [[nodiscard]] CameraRay screenRay(ScreenPoint pointer, Viewport viewport) const;
  [[nodiscard]] WorldPoint unproject(ScreenPoint window, Viewport viewport) const;
  [[nodiscard]] CameraFrustum frustum(float aspect_ratio) const;
};

class OrbitCamera {
public:
  void orbit(float yaw_delta, float pitch_delta);
  void zoom(float radius_delta);

  [[nodiscard]] Vec3 position() const;
  [[nodiscard]] Camera camera() const;
  [[nodiscard]] CameraRay screenRay(ScreenPoint pointer, Viewport viewport) const;
  [[nodiscard]] WorldToView viewMatrix() const;
  [[nodiscard]] ViewToClip projectionMatrix(float aspect_ratio) const;
  [[nodiscard]] WorldToClip viewProjectionMatrix(float aspect_ratio) const;
  [[nodiscard]] CameraFrustum frustum(float aspect_ratio) const;

  Vec3 target{0.0f, 0.9f, 0.0f};
  float yaw = radians(36.0f);
  float pitch = radians(18.0f);
  float radius = 6.2f;
  float vertical_fov = radians(54.0f);
  float near_plane = 0.05f;
  float far_plane = 120.0f;
  ProjectionPolicy projection_policy = defaultProjectionPolicy();
  Vec2 jitter{};
};

} // namespace aster
