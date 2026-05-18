// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat3.hpp"

#include <cmath>

namespace aster {

struct ProjectionPolicy {
  CoordinateHandedness handedness = CoordinateHandedness::RightHanded;
  ClipDepthRange depth_range = ClipDepthRange::ZeroToOne;
  DepthDirection depth_direction = DepthDirection::ReverseZ;
};

[[nodiscard]] inline constexpr ProjectionPolicy defaultProjectionPolicy() {
  return {};
}

[[nodiscard]] inline constexpr RenderConvention defaultRenderConvention() {
  return {};
}

[[nodiscard]] inline constexpr ProjectionPolicy projectionPolicy(const RenderConvention convention) {
  return {convention.handedness, convention.depth_range, convention.depth_direction};
}

[[nodiscard]] inline constexpr RenderConvention renderConventionFromProjectionPolicy(
    const ProjectionPolicy policy) {
  return {.handedness = policy.handedness,
          .depth_range = policy.depth_range,
          .depth_direction = policy.depth_direction};
}

enum class RayOriginPolicy {
  NearPlane,
  PerspectiveEye,
};

[[nodiscard]] inline Vec3 transformPoint(const Mat4 &matrix, const Vec3 point) {
  const Vec4 out = matrix * Vec4{point.x, point.y, point.z, 1.0f};
  if (std::abs(out.w) <= 0.000001f) {
    return {out.x, out.y, out.z};
  }
  return {out.x / out.w, out.y / out.w, out.z / out.w};
}

[[nodiscard]] inline Vec3 transformVector(const Mat4 &matrix, const Vec3 value) {
  const Vec4 out = matrix * Vec4{value.x, value.y, value.z, 0.0f};
  return {out.x, out.y, out.z};
}

[[nodiscard]] inline WorldPoint transformPoint(const LocalPoint point,
                                               const LocalToWorld local_to_world) {
  return WorldPoint{transformPoint(local_to_world.value, point.value)};
}

[[nodiscard]] inline ViewPoint transformPoint(const WorldPoint point,
                                              const WorldToView world_to_view) {
  return ViewPoint{transformPoint(world_to_view.value, point.value)};
}

[[nodiscard]] inline ClipPoint transformPoint(const ViewPoint point,
                                              const ViewToClip view_to_clip) {
  return ClipPoint{view_to_clip.value * Vec4{point.x, point.y, point.z, 1.0f}};
}

[[nodiscard]] inline ClipPoint transformPoint(const WorldPoint point,
                                              const WorldToClip world_to_clip) {
  return ClipPoint{world_to_clip.value * Vec4{point.x, point.y, point.z, 1.0f}};
}

[[nodiscard]] inline Direction transformDirection(const Direction direction, const Mat4 &matrix) {
  return Direction{transformVector(matrix, direction.value)};
}

[[nodiscard]] inline Mat4 translation(const Vec3 offset) {
  Mat4 out = identity();
  out.m[12] = offset.x;
  out.m[13] = offset.y;
  out.m[14] = offset.z;
  return out;
}

[[nodiscard]] inline Mat4 scale(const Vec3 value) {
  Mat4 out = identity();
  out.m[0] = value.x;
  out.m[5] = value.y;
  out.m[10] = value.z;
  return out;
}

[[nodiscard]] inline Mat4 rotation_x(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[5] = c;
  out.m[6] = s;
  out.m[9] = -s;
  out.m[10] = c;
  return out;
}

[[nodiscard]] inline Mat4 rotation_y(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[0] = c;
  out.m[2] = -s;
  out.m[8] = s;
  out.m[10] = c;
  return out;
}

[[nodiscard]] inline Mat4 rotation_z(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[0] = c;
  out.m[1] = s;
  out.m[4] = -s;
  out.m[5] = c;
  return out;
}

[[nodiscard]] inline MathResult<Mat4> axisRotation(const Vec3 axis_value,
                                                   const float radians_value) {
  const MathResult<Vec3> axis_result = safeNormalize(axis_value);
  if (!axis_result) {
    return MathResult<Mat4>::failure(MathError::DegenerateInput,
                                     "Axis rotation requires a non-zero axis.", identity());
  }
  const Vec3 axis = axis_result.value;
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  const Vec3 temp = axis * (1.0f - c);

  Mat4 out = identity();
  out.m[0] = c + temp.x * axis.x;
  out.m[1] = temp.x * axis.y + s * axis.z;
  out.m[2] = temp.x * axis.z - s * axis.y;
  out.m[4] = temp.y * axis.x - s * axis.z;
  out.m[5] = c + temp.y * axis.y;
  out.m[6] = temp.y * axis.z + s * axis.x;
  out.m[8] = temp.z * axis.x + s * axis.y;
  out.m[9] = temp.z * axis.y - s * axis.x;
  out.m[10] = c + temp.z * axis.z;
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> lookAtRH(const Vec3 eye, const Vec3 target,
                                               const Vec3 up) {
  const MathResult<Vec3> forward_result = safeNormalize(target - eye);
  if (!forward_result) {
    return MathResult<Mat4>::failure(MathError::DegenerateInput,
                                     "lookAt requires distinct eye and target.", identity());
  }
  const Vec3 f = forward_result.value;
  const MathResult<Vec3> side_result = safeNormalize(cross(f, up));
  if (!side_result) {
    return MathResult<Mat4>::failure(MathError::DegenerateInput,
                                     "lookAt requires a non-parallel up vector.", identity());
  }
  const Vec3 s = side_result.value;
  const Vec3 u = cross(s, f);

  Mat4 out = identity();
  out.m[0] = s.x;
  out.m[4] = s.y;
  out.m[8] = s.z;
  out.m[1] = u.x;
  out.m[5] = u.y;
  out.m[9] = u.z;
  out.m[2] = -f.x;
  out.m[6] = -f.y;
  out.m[10] = -f.z;
  out.m[12] = -dot(s, eye);
  out.m[13] = -dot(u, eye);
  out.m[14] = dot(f, eye);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> lookAtLH(const Vec3 eye, const Vec3 target,
                                               const Vec3 up) {
  const MathResult<Vec3> forward_result = safeNormalize(target - eye);
  if (!forward_result) {
    return MathResult<Mat4>::failure(MathError::DegenerateInput,
                                     "lookAt requires distinct eye and target.", identity());
  }
  const Vec3 f = forward_result.value;
  const MathResult<Vec3> side_result = safeNormalize(cross(up, f));
  if (!side_result) {
    return MathResult<Mat4>::failure(MathError::DegenerateInput,
                                     "lookAt requires a non-parallel up vector.", identity());
  }
  const Vec3 s = side_result.value;
  const Vec3 u = cross(f, s);

  Mat4 out = identity();
  out.m[0] = s.x;
  out.m[4] = s.y;
  out.m[8] = s.z;
  out.m[1] = u.x;
  out.m[5] = u.y;
  out.m[9] = u.z;
  out.m[2] = f.x;
  out.m[6] = f.y;
  out.m[10] = f.z;
  out.m[12] = -dot(s, eye);
  out.m[13] = -dot(u, eye);
  out.m[14] = -dot(f, eye);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> lookAt(
    const Vec3 eye, const Vec3 target, const Vec3 up,
    const CoordinateHandedness handedness = CoordinateHandedness::RightHanded) {
  return handedness == CoordinateHandedness::LeftHanded ? lookAtLH(eye, target, up)
                                                        : lookAtRH(eye, target, up);
}

[[nodiscard]] inline MathResult<Mat4> look_at(const Vec3 eye, const Vec3 target, const Vec3 up) {
  return lookAtRH(eye, target, up);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveRH_ZO(const float vertical_fov_radians,
                                                       const float aspect_ratio,
                                                       const float near_plane,
                                                       const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = far_plane / (near_plane - far_plane);
  out.m[11] = -1.0f;
  out.m[14] = -(far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveRH_ReverseZ_ZO(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid reverse-Z perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = near_plane / (far_plane - near_plane);
  out.m[11] = -1.0f;
  out.m[14] = (far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveRH_NO(const float vertical_fov_radians,
                                                       const float aspect_ratio,
                                                       const float near_plane,
                                                       const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = -(far_plane + near_plane) / (far_plane - near_plane);
  out.m[11] = -1.0f;
  out.m[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveRH_ReverseZ_NO(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid reverse-Z perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = (far_plane + near_plane) / (far_plane - near_plane);
  out.m[11] = -1.0f;
  out.m[14] = (2.0f * far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveLH_ZO(const float vertical_fov_radians,
                                                       const float aspect_ratio,
                                                       const float near_plane,
                                                       const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = far_plane / (far_plane - near_plane);
  out.m[11] = 1.0f;
  out.m[14] = -(far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveLH_ReverseZ_ZO(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid reverse-Z perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = -near_plane / (far_plane - near_plane);
  out.m[11] = 1.0f;
  out.m[14] = (far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveLH_NO(const float vertical_fov_radians,
                                                       const float aspect_ratio,
                                                       const float near_plane,
                                                       const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = (far_plane + near_plane) / (far_plane - near_plane);
  out.m[11] = 1.0f;
  out.m[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspectiveLH_ReverseZ_NO(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane) {
  if (aspect_ratio <= 0.0f || vertical_fov_radians <= 0.0f || near_plane <= 0.0f ||
      far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid reverse-Z perspective projection parameters.");
  }
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = -(far_plane + near_plane) / (far_plane - near_plane);
  out.m[11] = 1.0f;
  out.m[14] = (2.0f * far_plane * near_plane) / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> perspective(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane, const ProjectionPolicy policy = defaultProjectionPolicy()) {
  if (policy.handedness == CoordinateHandedness::RightHanded) {
    if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
        policy.depth_direction == DepthDirection::ForwardZ) {
      return perspectiveRH_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
    }
    if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
        policy.depth_direction == DepthDirection::ReverseZ) {
      return perspectiveRH_ReverseZ_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
    }
    if (policy.depth_range == ClipDepthRange::ZeroToOne &&
        policy.depth_direction == DepthDirection::ForwardZ) {
      return perspectiveRH_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
    }
    return perspectiveRH_ReverseZ_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveLH_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
      policy.depth_direction == DepthDirection::ReverseZ) {
    return perspectiveLH_ReverseZ_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::ZeroToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveLH_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  return perspectiveLH_ReverseZ_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
}

[[nodiscard]] inline MathResult<Mat4> orthographicRH_ReverseZ_ZO(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane) {
  if (right == left || top == bottom || far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid orthographic projection parameters.");
  }
  Mat4 out = identity();
  out.m[0] = 2.0f / (right - left);
  out.m[5] = 2.0f / (top - bottom);
  out.m[10] = 1.0f / (far_plane - near_plane);
  out.m[12] = -(right + left) / (right - left);
  out.m[13] = -(top + bottom) / (top - bottom);
  out.m[14] = far_plane / (far_plane - near_plane);
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> orthographic(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane,
    const ProjectionPolicy policy = defaultProjectionPolicy()) {
  if (right == left || top == bottom || far_plane <= near_plane) {
    return MathResult<Mat4>::failure(MathError::InvalidArgument,
                                     "Invalid orthographic projection parameters.");
  }

  const float z_near =
      policy.depth_direction == DepthDirection::ReverseZ
          ? (policy.depth_range == ClipDepthRange::NegativeOneToOne ? 1.0f : 1.0f)
          : (policy.depth_range == ClipDepthRange::NegativeOneToOne ? -1.0f : 0.0f);
  const float z_far = policy.depth_direction == DepthDirection::ReverseZ ? 0.0f : 1.0f;
  const float view_near =
      policy.handedness == CoordinateHandedness::RightHanded ? -near_plane : near_plane;
  const float view_far =
      policy.handedness == CoordinateHandedness::RightHanded ? -far_plane : far_plane;
  const float z_scale = (z_far - z_near) / (view_far - view_near);
  const float z_offset = z_near - z_scale * view_near;

  Mat4 out = identity();
  out.m[0] = 2.0f / (right - left);
  out.m[5] = 2.0f / (top - bottom);
  out.m[10] = z_scale;
  out.m[12] = -(right + left) / (right - left);
  out.m[13] = -(top + bottom) / (top - bottom);
  out.m[14] = z_offset;
  return MathResult<Mat4>::success(out);
}

[[nodiscard]] inline MathResult<Mat4> orthographic(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane,
    const RenderConvention convention) {
  return orthographic(left, right, bottom, top, near_plane, far_plane,
                      projectionPolicy(convention));
}

[[nodiscard]] inline MathResult<ViewToClip> perspectiveTransform(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane, const ProjectionPolicy policy = defaultProjectionPolicy()) {
  const MathResult<Mat4> result =
      perspective(vertical_fov_radians, aspect_ratio, near_plane, far_plane, policy);
  return result ? MathResult<ViewToClip>::success(ViewToClip{result.value})
                : MathResult<ViewToClip>::failure(result.diagnostics.error,
                                                  result.diagnostics.message);
}

[[nodiscard]] inline MathResult<ViewToClip> orthographicTransform(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane,
    const ProjectionPolicy policy = defaultProjectionPolicy()) {
  const MathResult<Mat4> result =
      orthographic(left, right, bottom, top, near_plane, far_plane, policy);
  return result ? MathResult<ViewToClip>::success(ViewToClip{result.value})
                : MathResult<ViewToClip>::failure(result.diagnostics.error,
                                                  result.diagnostics.message);
}

[[nodiscard]] inline NdcPoint divideByW(const ClipPoint clip) {
  const float inv_w = 1.0f / clip.w;
  return {clip.x * inv_w, clip.y * inv_w, clip.z * inv_w};
}

[[nodiscard]] inline MathResult<NdcPoint> clipToNdc(const ClipPoint clip) {
  if (std::abs(clip.w) <= 0.000001f) {
    return MathResult<NdcPoint>::failure(MathError::DegenerateInput,
                                         "Cannot divide by a near-zero clip W.");
  }
  return MathResult<NdcPoint>::success(divideByW(clip));
}

[[nodiscard]] inline MathResult<ScreenPoint> ndcToScreen(const NdcPoint ndc,
                                                         const Viewport viewport) {
  if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f) {
    return MathResult<ScreenPoint>::failure(MathError::InvalidArgument,
                                            "Viewport must have a positive size.");
  }
  const float normalized_x = ndc.x * 0.5f + 0.5f;
  const float normalized_y = ndc.y * 0.5f + 0.5f;
  const float screen_y = viewport.origin_convention == ViewportOrigin::TopLeft
                             ? 1.0f - normalized_y
                             : normalized_y;
  return MathResult<ScreenPoint>::success(
      {viewport.origin.x + normalized_x * viewport.size.x,
       viewport.origin.y + screen_y * viewport.size.y, ndc.z});
}

[[nodiscard]] inline MathResult<NdcPoint> screenToNdc(const ScreenPoint screen,
                                                      const Viewport viewport) {
  if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f) {
    return MathResult<NdcPoint>::failure(MathError::InvalidArgument,
                                         "Viewport must have a positive size.");
  }
  const float normalized_x = (screen.x - viewport.origin.x) / viewport.size.x;
  const float screen_y = (screen.y - viewport.origin.y) / viewport.size.y;
  const float normalized_y =
      viewport.origin_convention == ViewportOrigin::TopLeft ? 1.0f - screen_y : screen_y;
  return MathResult<NdcPoint>::success(
      {normalized_x * 2.0f - 1.0f, normalized_y * 2.0f - 1.0f, screen.z});
}

[[nodiscard]] inline MathResult<ScreenPoint> project(const WorldPoint point,
                                                     const WorldToClip world_to_clip,
                                                     const Viewport viewport) {
  const MathResult<NdcPoint> ndc = clipToNdc(transformPoint(point, world_to_clip));
  if (!ndc) {
    return MathResult<ScreenPoint>::failure(ndc.diagnostics.error, ndc.diagnostics.message);
  }
  return ndcToScreen(ndc.value, viewport);
}

[[nodiscard]] inline MathResult<WorldPoint> unproject(const ScreenPoint screen,
                                                      const ClipToWorld clip_to_world,
                                                      const Viewport viewport) {
  const MathResult<NdcPoint> ndc = screenToNdc(screen, viewport);
  if (!ndc) {
    return MathResult<WorldPoint>::failure(ndc.diagnostics.error, ndc.diagnostics.message);
  }
  const Vec4 world = clip_to_world.value * Vec4{ndc.value.x, ndc.value.y, ndc.value.z, 1.0f};
  if (std::abs(world.w) <= 0.000001f) {
    return MathResult<WorldPoint>::failure(MathError::DegenerateInput,
                                           "Unproject cannot divide by a near-zero world W.",
                                           WorldPoint{world.x, world.y, world.z});
  }
  return MathResult<WorldPoint>::success({world.x / world.w, world.y / world.w, world.z / world.w});
}

[[nodiscard]] inline MathResult<WorldRay> screenRay(
    const ScreenPoint screen, const ClipToWorld clip_to_world, const Viewport viewport,
    const ProjectionPolicy policy = defaultProjectionPolicy(),
    const RayOriginPolicy origin_policy = RayOriginPolicy::NearPlane,
    const WorldPoint perspective_eye = {}) {
  const float near_depth = policy.depth_direction == DepthDirection::ReverseZ
                               ? 1.0f
                               : (policy.depth_range == ClipDepthRange::NegativeOneToOne ? -1.0f
                                                                                          : 0.0f);
  const float far_depth = policy.depth_direction == DepthDirection::ReverseZ ? 0.0f : 1.0f;
  const MathResult<WorldPoint> near_point =
      unproject(ScreenPoint{screen.x, screen.y, near_depth}, clip_to_world, viewport);
  const MathResult<WorldPoint> far_point =
      unproject(ScreenPoint{screen.x, screen.y, far_depth}, clip_to_world, viewport);
  if (!near_point) {
    return MathResult<WorldRay>::failure(near_point.diagnostics.error,
                                         near_point.diagnostics.message);
  }
  if (!far_point) {
    return MathResult<WorldRay>::failure(far_point.diagnostics.error,
                                         far_point.diagnostics.message);
  }
  const Vec3 origin = origin_policy == RayOriginPolicy::PerspectiveEye ? perspective_eye.value
                                                                       : near_point.value.value;
  const MathResult<Vec3> direction = safeNormalize(far_point.value.value - near_point.value.value);
  if (!direction) {
    return MathResult<WorldRay>::failure(direction.diagnostics.error,
                                         direction.diagnostics.message);
  }
  return MathResult<WorldRay>::success({WorldPoint{origin}, Direction{direction.value}});
}

} // namespace aster
