// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace aster {
namespace {

Plane3 normalizePlane(const Plane3 plane) {
  const float len = length(plane.normal);
  if (len <= 0.000001f) {
    return plane;
  }
  return {plane.normal / len, plane.distance / len};
}

Plane3 planeFromRows(const float row0, const float row1, const float row2, const float row3) {
  return normalizePlane({{row0, row1, row2}, row3});
}

float rowValue(const Mat4 &matrix, const int row, const int column) {
  return at(matrix, row, column);
}

Plane3 combineRows(const Mat4 &matrix, const int row_a, const float scale_a, const int row_b,
                   const float scale_b) {
  return planeFromRows(rowValue(matrix, row_a, 0) * scale_a +
                           rowValue(matrix, row_b, 0) * scale_b,
                       rowValue(matrix, row_a, 1) * scale_a +
                           rowValue(matrix, row_b, 1) * scale_b,
                       rowValue(matrix, row_a, 2) * scale_a +
                           rowValue(matrix, row_b, 2) * scale_b,
                       rowValue(matrix, row_a, 3) * scale_a +
                           rowValue(matrix, row_b, 3) * scale_b);
}

} // namespace

WorldToView Camera::viewMatrix() const {
  const MathResult<Mat4> view = lookAt(eye, target, up, projection_policy.handedness);
  return WorldToView{view ? view.value : identity()};
}

ViewToClip Camera::projectionMatrix(const float aspect_ratio) const {
  MathResult<Mat4> projection =
      projection_mode == CameraProjectionMode::Orthographic
          ? orthographic(-orthographic_height * aspect_ratio * 0.5f,
                         orthographic_height * aspect_ratio * 0.5f,
                         -orthographic_height * 0.5f, orthographic_height * 0.5f, near_plane,
                         far_plane, projection_policy)
          : perspective(vertical_fov, aspect_ratio, near_plane, far_plane, projection_policy);
  Mat4 out = projection ? projection.value : identity();
  if (jitter.x != 0.0f || jitter.y != 0.0f) {
    out.m[8] += jitter.x;
    out.m[9] += jitter.y;
  }
  return ViewToClip{out};
}

WorldToClip Camera::viewProjectionMatrix(const float aspect_ratio) const {
  return WorldToClip{projectionMatrix(aspect_ratio).value * viewMatrix().value};
}

CameraRay Camera::screenRay(const ScreenPoint pointer, const Viewport viewport) const {
  const float width = std::max(viewport.size.x, 1.0f);
  const float height = std::max(viewport.size.y, 1.0f);
  const float aspect = width / height;
  Viewport sanitized_viewport = viewport;
  sanitized_viewport.size = {width, height};
  const MathResult<Mat4> clip_to_world_result = inverse(viewProjectionMatrix(aspect).value);
  const ClipToWorld clip_to_world{clip_to_world_result ? clip_to_world_result.value : identity()};
  const RayOriginPolicy origin_policy = projection_mode == CameraProjectionMode::Perspective
                                            ? RayOriginPolicy::PerspectiveEye
                                            : RayOriginPolicy::NearPlane;
  const MathResult<WorldRay> ray =
      aster::screenRay(pointer, clip_to_world, sanitized_viewport, projection_policy,
                       origin_policy, WorldPoint{eye});
  if (ray) {
    return ray.value;
  }
  return {WorldPoint{eye}, Direction{normalizeOr(target - eye, {0.0f, 0.0f, -1.0f})}};
}

WorldPoint Camera::unproject(const ScreenPoint window, const Viewport viewport) const {
  const float width = std::max(viewport.size.x, 1.0f);
  const float height = std::max(viewport.size.y, 1.0f);
  Viewport sanitized_viewport = viewport;
  sanitized_viewport.size = {width, height};
  const MathResult<Mat4> clip_to_world = inverse(viewProjectionMatrix(width / height).value);
  if (!clip_to_world) {
    return {};
  }
  const MathResult<WorldPoint> result =
      aster::unproject(window, ClipToWorld{clip_to_world.value}, sanitized_viewport);
  return result ? result.value : WorldPoint{};
}

CameraFrustum Camera::frustum(const float aspect_ratio) const {
  const Mat4 matrix = viewProjectionMatrix(aspect_ratio).value;
  CameraFrustum out;
  out.planes[static_cast<std::size_t>(FrustumPlane::Left)] = combineRows(matrix, 3, 1.0f, 0, 1.0f);
  out.planes[static_cast<std::size_t>(FrustumPlane::Right)] = combineRows(matrix, 3, 1.0f, 0, -1.0f);
  out.planes[static_cast<std::size_t>(FrustumPlane::Bottom)] = combineRows(matrix, 3, 1.0f, 1, 1.0f);
  out.planes[static_cast<std::size_t>(FrustumPlane::Top)] = combineRows(matrix, 3, 1.0f, 1, -1.0f);
  if (projection_policy.depth_range == ClipDepthRange::ZeroToOne &&
      projection_policy.depth_direction == DepthDirection::ReverseZ) {
    out.planes[static_cast<std::size_t>(FrustumPlane::Near)] =
        combineRows(matrix, 3, 1.0f, 2, -1.0f);
    out.planes[static_cast<std::size_t>(FrustumPlane::Far)] =
        planeFromRows(rowValue(matrix, 2, 0), rowValue(matrix, 2, 1), rowValue(matrix, 2, 2),
                      rowValue(matrix, 2, 3));
  } else if (projection_policy.depth_range == ClipDepthRange::NegativeOneToOne) {
    out.planes[static_cast<std::size_t>(FrustumPlane::Near)] =
        combineRows(matrix, 3, 1.0f, 2, 1.0f);
    out.planes[static_cast<std::size_t>(FrustumPlane::Far)] =
        combineRows(matrix, 3, 1.0f, 2, -1.0f);
  } else {
    out.planes[static_cast<std::size_t>(FrustumPlane::Near)] =
        planeFromRows(rowValue(matrix, 2, 0), rowValue(matrix, 2, 1), rowValue(matrix, 2, 2),
                      rowValue(matrix, 2, 3));
    out.planes[static_cast<std::size_t>(FrustumPlane::Far)] =
        combineRows(matrix, 3, 1.0f, 2, -1.0f);
  }
  return out;
}

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

Camera OrbitCamera::camera() const {
  Camera out;
  out.eye = position();
  out.target = target;
  out.vertical_fov = vertical_fov;
  out.near_plane = near_plane;
  out.far_plane = far_plane;
  out.projection_policy = projection_policy;
  out.jitter = jitter;
  return out;
}

CameraRay OrbitCamera::screenRay(const ScreenPoint pointer, const Viewport viewport) const {
  return camera().screenRay(pointer, viewport);
}

WorldToView OrbitCamera::viewMatrix() const {
  return camera().viewMatrix();
}

ViewToClip OrbitCamera::projectionMatrix(const float aspect_ratio) const {
  return camera().projectionMatrix(aspect_ratio);
}

WorldToClip OrbitCamera::viewProjectionMatrix(const float aspect_ratio) const {
  return camera().viewProjectionMatrix(aspect_ratio);
}

CameraFrustum OrbitCamera::frustum(const float aspect_ratio) const {
  return camera().frustum(aspect_ratio);
}

} // namespace aster
