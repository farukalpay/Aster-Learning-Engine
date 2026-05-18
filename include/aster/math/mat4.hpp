// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat3.hpp"

#include <cmath>

namespace aster {

enum class CoordinateHandedness {
  RightHanded,
  LeftHanded,
};

enum class ClipDepthRange {
  ZeroToOne,
  NegativeOneToOne,
};

enum class DepthDirection {
  ForwardZ,
  ReverseZ,
};

struct ProjectionPolicy {
  CoordinateHandedness handedness = CoordinateHandedness::RightHanded;
  ClipDepthRange depth_range = ClipDepthRange::ZeroToOne;
  DepthDirection depth_direction = DepthDirection::ReverseZ;
};

[[nodiscard]] inline constexpr ProjectionPolicy defaultProjectionPolicy() {
  return {};
}

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

[[nodiscard]] inline MathResult<Mat4> perspective(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane, const ProjectionPolicy policy = defaultProjectionPolicy()) {
  if (policy.handedness != CoordinateHandedness::RightHanded) {
    return MathResult<Mat4>::failure(MathError::UnsupportedPolicy,
                                     "Aster perspective currently supports right-handed view space.");
  }
  if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveRH_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::ZeroToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveRH_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::ZeroToOne &&
      policy.depth_direction == DepthDirection::ReverseZ) {
    return perspectiveRH_ReverseZ_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  return MathResult<Mat4>::failure(MathError::UnsupportedPolicy,
                                   "Aster reverse-Z projection requires zero-to-one clip depth.");
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
  if (policy.handedness != CoordinateHandedness::RightHanded ||
      policy.depth_range != ClipDepthRange::ZeroToOne ||
      policy.depth_direction != DepthDirection::ReverseZ) {
    return MathResult<Mat4>::failure(MathError::UnsupportedPolicy,
                                     "Aster orthographic currently supports RH reverse-Z zero-to-one.");
  }
  return orthographicRH_ReverseZ_ZO(left, right, bottom, top, near_plane, far_plane);
}

[[nodiscard]] inline MathResult<Vec3> project(const Vec3 point, const Mat4 &world_to_clip,
                                              const Vec2 viewport_origin,
                                              const Vec2 viewport_size) {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return MathResult<Vec3>::failure(MathError::InvalidArgument, "Project requires a positive viewport.");
  }
  const Vec4 clip = world_to_clip * Vec4{point.x, point.y, point.z, 1.0f};
  if (std::abs(clip.w) <= 0.000001f) {
    return MathResult<Vec3>::failure(MathError::DegenerateInput,
                                     "Project cannot divide by a near-zero clip W.");
  }
  const float inv_w = 1.0f / clip.w;
  const Vec3 ndc{clip.x * inv_w, clip.y * inv_w, clip.z * inv_w};
  return MathResult<Vec3>::success(
      {viewport_origin.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x,
       viewport_origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y, ndc.z});
}

[[nodiscard]] inline MathResult<Vec3> unproject(const Vec3 window, const Mat4 &clip_to_world,
                                                const Vec2 viewport_origin,
                                                const Vec2 viewport_size) {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return MathResult<Vec3>::failure(MathError::InvalidArgument,
                                     "Unproject requires a positive viewport.");
  }
  const Vec4 clip{(window.x - viewport_origin.x) / viewport_size.x * 2.0f - 1.0f,
                  1.0f - (window.y - viewport_origin.y) / viewport_size.y * 2.0f,
                  window.z, 1.0f};
  const Vec4 world = clip_to_world * clip;
  if (std::abs(world.w) <= 0.000001f) {
    return MathResult<Vec3>::failure(MathError::DegenerateInput,
                                     "Unproject cannot divide by a near-zero world W.",
                                     {world.x, world.y, world.z});
  }
  return MathResult<Vec3>::success({world.x / world.w, world.y / world.w, world.z / world.w});
}

} // namespace aster
