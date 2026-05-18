// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/avatar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr float kPi = 3.14159265358979323846f;

float wrapRadians(float value) {
  while (value > kPi) {
    value -= kPi * 2.0f;
  }
  while (value < -kPi) {
    value += kPi * 2.0f;
  }
  return value;
}

float dampingFactor(const float response, const float dt) {
  return 1.0f - std::exp(-std::max(response, 0.0f) * std::max(dt, 0.0f));
}

float dampValue(const float current, const float target, const float response, const float dt) {
  return current + (target - current) * dampingFactor(response, dt);
}

float dampAngle(const float current, const float target, const float response, const float dt) {
  return wrapRadians(current + wrapRadians(target - current) * dampingFactor(response, dt));
}

float dampAngleCapped(const float current, const float target, const float response,
                      const float max_turn_rate, const float dt) {
  float delta = wrapRadians(target - current) * dampingFactor(response, dt);
  if (max_turn_rate > 0.0f && dt > 0.0f) {
    delta = std::clamp(delta, -max_turn_rate * dt, max_turn_rate * dt);
  }
  return wrapRadians(current + delta);
}

float clampSymmetric(const float value, const float limit) {
  const float magnitude = std::max(limit, 0.0f);
  return std::clamp(value, -magnitude, magnitude);
}

struct AttentionAngles {
  float yaw = 0.0f;
  float pitch = 0.0f;
};

struct EulerTransformSpec {
  aster::Vec3 position{};
  aster::Vec3 rotation{};
  aster::Vec3 scale{1.0f, 1.0f, 1.0f};
};

AttentionAngles attentionAngles(const aster::Vec3 origin, const aster::Vec3 target,
                                const float facing_yaw,
                                const aster::AvatarAnimatorSettings &settings) {
  const aster::Vec3 delta = target - origin;
  const aster::Vec2 planar{delta.x, delta.z};
  const float planar_distance = std::max(aster::length(planar), 0.001f);
  const float target_yaw = std::atan2(delta.x, delta.z);
  const float target_pitch = std::atan2(delta.y, planar_distance);
  return {clampSymmetric(wrapRadians(target_yaw - facing_yaw), settings.point_yaw_limit),
          clampSymmetric(target_pitch, settings.point_pitch_limit)};
}

float jointSwing(const aster::AvatarJoint joint, const float gait_phase,
                 const float stride_amplitude) {
  const float swing = std::sin(gait_phase) * stride_amplitude;
  switch (joint) {
  case aster::AvatarJoint::LeftArm:
  case aster::AvatarJoint::RightLeg:
    return -swing;
  case aster::AvatarJoint::RightArm:
  case aster::AvatarJoint::LeftLeg:
    return swing;
  case aster::AvatarJoint::Static:
  case aster::AvatarJoint::Torso:
  case aster::AvatarJoint::Head:
  case aster::AvatarJoint::Mouth:
    return 0.0f;
  }
  return 0.0f;
}

aster::Vec3 jointRotation(const aster::AvatarJoint joint, const aster::AvatarPose &pose,
                          const float stride) {
  aster::Vec3 rotation{jointSwing(joint, pose.gait_phase, stride), 0.0f, 0.0f};
  const float swim = std::clamp(pose.swim_blend, 0.0f, 1.0f);
  if (swim > 0.0f) {
    const float paddle = std::sin(pose.gait_phase * 1.45f) * (0.34f + stride * 0.26f);
    switch (joint) {
    case aster::AvatarJoint::LeftArm:
      rotation.x = rotation.x * (1.0f - swim) + (-0.86f + paddle) * swim;
      rotation.z += 0.42f * swim;
      break;
    case aster::AvatarJoint::RightArm:
      rotation.x = rotation.x * (1.0f - swim) + (-0.86f - paddle) * swim;
      rotation.z -= 0.42f * swim;
      break;
    case aster::AvatarJoint::LeftLeg:
      rotation.x = rotation.x * (1.0f - swim) + (0.45f - paddle * 0.48f) * swim;
      break;
    case aster::AvatarJoint::RightLeg:
      rotation.x = rotation.x * (1.0f - swim) + (0.45f + paddle * 0.48f) * swim;
      break;
    case aster::AvatarJoint::Torso:
      rotation.x += -0.22f * swim;
      break;
    case aster::AvatarJoint::Head:
    case aster::AvatarJoint::Mouth:
    case aster::AvatarJoint::Static:
      break;
    }
  }
  const float climb = std::clamp(pose.climb_blend, 0.0f, 1.0f);
  if (climb > 0.0f) {
    const float reach = std::sin(pose.gait_phase * 1.18f) * (0.28f + stride * 0.18f);
    switch (joint) {
    case aster::AvatarJoint::LeftArm:
      rotation.x = rotation.x * (1.0f - climb) + (-1.48f + reach) * climb;
      rotation.z += 0.34f * climb;
      break;
    case aster::AvatarJoint::RightArm:
      rotation.x = rotation.x * (1.0f - climb) + (-1.48f - reach) * climb;
      rotation.z -= 0.34f * climb;
      break;
    case aster::AvatarJoint::LeftLeg:
      rotation.x = rotation.x * (1.0f - climb) + (-0.34f - reach * 0.52f) * climb;
      rotation.z += 0.20f * climb;
      break;
    case aster::AvatarJoint::RightLeg:
      rotation.x = rotation.x * (1.0f - climb) + (-0.34f + reach * 0.52f) * climb;
      rotation.z -= 0.20f * climb;
      break;
    case aster::AvatarJoint::Torso:
      rotation.x += 0.20f * climb;
      break;
    case aster::AvatarJoint::Head:
    case aster::AvatarJoint::Mouth:
    case aster::AvatarJoint::Static:
      break;
    }
  }
  if (joint == aster::AvatarJoint::Torso) {
    rotation.z += std::sin(pose.gait_phase * 2.0f) * stride * 0.035f;
  }
  const float point =
      std::clamp(pose.point_blend * (1.0f - swim * 0.70f) * (1.0f - climb * 0.80f), 0.0f, 1.0f);
  if (point > 0.0f) {
    const float yaw = clampSymmetric(pose.point_yaw_offset, aster::radians(86.0f));
    const float pitch = clampSymmetric(pose.point_pitch_offset, aster::radians(44.0f));
    switch (joint) {
    case aster::AvatarJoint::RightArm: {
      const aster::Vec3 target{-1.48f - pitch * 0.72f, yaw * 0.62f, -0.22f - std::abs(yaw) * 0.10f};
      rotation = rotation * (1.0f - point) + target * point;
      break;
    }
    case aster::AvatarJoint::LeftArm:
      rotation.x = rotation.x * (1.0f - point) + (-0.28f + pitch * 0.12f) * point;
      rotation.z += 0.18f * point;
      break;
    case aster::AvatarJoint::Torso:
      rotation.y += yaw * 0.10f * point;
      rotation.x -= pitch * 0.08f * point;
      break;
    case aster::AvatarJoint::Static:
    case aster::AvatarJoint::Head:
    case aster::AvatarJoint::Mouth:
    case aster::AvatarJoint::LeftLeg:
    case aster::AvatarJoint::RightLeg:
      break;
    }
  }
  if (joint == aster::AvatarJoint::Head) {
    rotation.x += pose.head_pitch_offset;
    rotation.y += pose.head_yaw_offset + std::sin(pose.gait_phase * 0.5f) * stride * 0.045f;
  }
  if (joint == aster::AvatarJoint::Mouth) {
    rotation.x += pose.head_pitch_offset;
    rotation.y += pose.head_yaw_offset + std::sin(pose.gait_phase * 0.5f) * stride * 0.045f;
  }
  return rotation;
}

aster::Vec3 transformedLocalPoint(const aster::Transform &transform,
                                  const aster::Vec3 local_offset) {
  return transform.position + aster::rotate(transform.rotation, local_offset * transform.scale);
}

aster::Transform posedAvatarPartTransform(const aster::AvatarPart &part,
                                          const aster::AvatarPose &pose) {
  const float stride = std::clamp(pose.stride_amplitude, 0.0f, 1.0f);
  aster::Transform transform = part.local_transform;
  if (part.role == aster::AvatarPartRole::PointingFinger) {
    const float visible = std::max(std::clamp(pose.point_blend, 0.0f, 1.0f), 0.001f);
    transform.scale = transform.scale * visible;
  }
  if (part.joint == aster::AvatarJoint::Mouth) {
    const float open = std::clamp(pose.mouth_open, 0.0f, 1.0f);
    transform.position.y -= open * 0.014f;
    transform.scale.x *= 1.0f + open * 0.25f;
    transform.scale.y *= 1.0f + open * 4.6f;
    transform.scale.z *= 1.0f + open * 0.65f;
  }
  const aster::Vec3 rotation = jointRotation(part.joint, pose, stride);
  const aster::Quat rotation_delta = aster::quatFromEulerXyz(rotation);
  transform.position =
      part.joint_pivot + aster::rotate(rotation_delta, transform.position - part.joint_pivot);
  transform.rotation = rotation_delta * transform.rotation;

  const aster::Quat facing = aster::axisAngle({0.0f, 1.0f, 0.0f}, pose.facing_yaw);
  transform.position = pose.position + aster::rotate(facing, transform.position) +
                       aster::Vec3{0.0f, pose.vertical_bob, 0.0f};
  transform.rotation = facing * transform.rotation;
  return transform;
}

float primitiveExtentScale(const aster::MeshPrimitive primitive) {
  return primitive == aster::MeshPrimitive::Box ? 0.5f : 1.0f;
}

void expandBounds(aster::AvatarBounds &bounds, const aster::Vec3 point) {
  bounds.min.x = std::min(bounds.min.x, point.x);
  bounds.min.y = std::min(bounds.min.y, point.y);
  bounds.min.z = std::min(bounds.min.z, point.z);
  bounds.max.x = std::max(bounds.max.x, point.x);
  bounds.max.y = std::max(bounds.max.y, point.y);
  bounds.max.z = std::max(bounds.max.z, point.z);
}

void addPart(aster::AvatarRig &rig, std::string name, const aster::MeshPrimitive primitive,
             const EulerTransformSpec transform, const aster::Material &material,
             const aster::AvatarJoint joint, const aster::Vec3 joint_pivot,
             const aster::AvatarPartRole role = aster::AvatarPartRole::Body) {
  rig.parts.push_back({std::move(name), primitive,
                       aster::Transform::fromEuler(transform.position, transform.rotation,
                                                   transform.scale),
                       joint_pivot, material, joint, role});
}

} // namespace

namespace aster {

AvatarRig makePlushHumanoidAvatar(const PlushHumanoidAvatarSpec &spec) {
  if (spec.height <= 0.0f) {
    throw std::invalid_argument("Plush avatar height must be positive.");
  }

  const float h = spec.height;
  AvatarRig rig;
  rig.name = "plush humanoid";

  const Vec3 torso_pivot{0.0f, -0.10f * h, 0.0f};
  const Vec3 neck_pivot{0.0f, 0.10f * h, 0.0f};
  const Vec3 left_shoulder{-0.188f * h, 0.048f * h, 0.0f};
  const Vec3 right_shoulder{0.188f * h, 0.048f * h, 0.0f};
  const Vec3 left_hip{-0.090f * h, -0.265f * h, 0.0f};
  const Vec3 right_hip{0.090f * h, -0.265f * h, 0.0f};

  addPart(rig, "plush torso", MeshPrimitive::Sphere,
          {{0.0f, -0.10f * h, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.245f * h, 0.325f * h, 0.185f * h}},
          spec.fur_material, AvatarJoint::Torso, torso_pivot);
  addPart(
      rig, "stitched belly patch", MeshPrimitive::Sphere,
      {{0.0f, -0.12f * h, 0.160f * h}, {-0.06f, 0.0f, 0.0f}, {0.125f * h, 0.170f * h, 0.040f * h}},
      spec.muzzle_material, AvatarJoint::Torso, torso_pivot);
  addPart(
      rig, "plush head", MeshPrimitive::Sphere,
      {{0.0f, 0.255f * h, 0.010f * h}, {0.0f, 0.0f, 0.0f}, {0.215f * h, 0.200f * h, 0.185f * h}},
      spec.fur_material, AvatarJoint::Head, neck_pivot);
  addPart(rig, "left rounded ear", MeshPrimitive::Sphere,
          {{-0.155f * h, 0.425f * h, -0.010f * h},
           {0.0f, 0.0f, -0.10f},
           {0.080f * h, 0.088f * h, 0.055f * h}},
          spec.fur_material, AvatarJoint::Head, neck_pivot);
  addPart(rig, "right rounded ear", MeshPrimitive::Sphere,
          {{0.155f * h, 0.425f * h, -0.010f * h},
           {0.0f, 0.0f, 0.10f},
           {0.080f * h, 0.088f * h, 0.055f * h}},
          spec.fur_material, AvatarJoint::Head, neck_pivot);
  addPart(
      rig, "soft muzzle", MeshPrimitive::Sphere,
      {{0.0f, 0.205f * h, 0.168f * h}, {-0.08f, 0.0f, 0.0f}, {0.112f * h, 0.066f * h, 0.074f * h}},
      spec.muzzle_material, AvatarJoint::Head, neck_pivot);
  addPart(rig, "left bead eye", MeshPrimitive::Sphere,
          {{-0.073f * h, 0.300f * h, 0.163f * h},
           {0.0f, 0.0f, 0.0f},
           {0.022f * h, 0.025f * h, 0.017f * h}},
          spec.eye_material, AvatarJoint::Head, neck_pivot);
  addPart(rig, "right bead eye", MeshPrimitive::Sphere,
          {{0.073f * h, 0.300f * h, 0.163f * h},
           {0.0f, 0.0f, 0.0f},
           {0.022f * h, 0.025f * h, 0.017f * h}},
          spec.eye_material, AvatarJoint::Head, neck_pivot);
  addPart(
      rig, "small nose", MeshPrimitive::Sphere,
      {{0.0f, 0.212f * h, 0.235f * h}, {-0.12f, 0.0f, 0.0f}, {0.038f * h, 0.027f * h, 0.022f * h}},
      spec.nose_material, AvatarJoint::Head, neck_pivot);
  addPart(
      rig, "round open mouth", MeshPrimitive::Sphere,
      {{0.0f, 0.173f * h, 0.241f * h}, {-0.08f, 0.0f, 0.0f}, {0.032f * h, 0.010f * h, 0.008f * h}},
      spec.nose_material, AvatarJoint::Mouth, neck_pivot);

  addPart(rig, "left shoulder stitch", MeshPrimitive::Sphere,
          {{-0.205f * h, 0.010f * h, 0.018f * h},
           {0.0f, 0.0f, -0.14f},
           {0.075f * h, 0.122f * h, 0.066f * h}},
          spec.fur_material, AvatarJoint::Torso, torso_pivot);
  addPart(rig, "right shoulder stitch", MeshPrimitive::Sphere,
          {{0.205f * h, 0.010f * h, 0.018f * h},
           {0.0f, 0.0f, 0.14f},
           {0.075f * h, 0.122f * h, 0.066f * h}},
          spec.fur_material, AvatarJoint::Torso, torso_pivot);
  addPart(rig, "left arm", MeshPrimitive::Sphere,
          {{-0.250f * h, -0.100f * h, 0.020f * h},
           {0.09f, 0.0f, -0.16f},
           {0.070f * h, 0.278f * h, 0.063f * h}},
          spec.fur_material, AvatarJoint::LeftArm, left_shoulder);
  addPart(rig, "right arm", MeshPrimitive::Sphere,
          {{0.250f * h, -0.100f * h, 0.020f * h},
           {0.09f, 0.0f, 0.16f},
           {0.070f * h, 0.278f * h, 0.063f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder);
  addPart(rig, "left paw", MeshPrimitive::Sphere,
          {{-0.280f * h, -0.390f * h, 0.040f * h},
           {0.0f, 0.0f, -0.05f},
           {0.075f * h, 0.069f * h, 0.068f * h}},
          spec.fur_material, AvatarJoint::LeftArm, left_shoulder);
  addPart(rig, "right paw", MeshPrimitive::Sphere,
          {{0.280f * h, -0.390f * h, 0.040f * h},
           {0.0f, 0.0f, 0.05f},
           {0.075f * h, 0.069f * h, 0.068f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder);
  addPart(rig, "right command thumb", MeshPrimitive::Box,
          {{0.226f * h, -0.398f * h, 0.120f * h},
           {-0.18f, -0.18f, -0.46f},
           {0.014f * h, 0.017f * h, 0.070f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder, AvatarPartRole::PointingFinger);
  addPart(rig, "right command index", MeshPrimitive::Box,
          {{0.258f * h, -0.430f * h, 0.150f * h},
           {-0.08f, -0.06f, -0.18f},
           {0.013f * h, 0.016f * h, 0.088f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder, AvatarPartRole::PointingFinger);
  addPart(rig, "right command middle", MeshPrimitive::Box,
          {{0.288f * h, -0.438f * h, 0.154f * h},
           {0.0f, 0.0f, 0.02f},
           {0.014f * h, 0.017f * h, 0.096f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder, AvatarPartRole::PointingFinger);
  addPart(rig, "right command ring", MeshPrimitive::Box,
          {{0.318f * h, -0.432f * h, 0.146f * h},
           {-0.05f, 0.05f, 0.20f},
           {0.013f * h, 0.016f * h, 0.084f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder, AvatarPartRole::PointingFinger);
  addPart(rig, "right command little", MeshPrimitive::Box,
          {{0.344f * h, -0.414f * h, 0.132f * h},
           {-0.12f, 0.10f, 0.38f},
           {0.012f * h, 0.015f * h, 0.066f * h}},
          spec.fur_material, AvatarJoint::RightArm, right_shoulder, AvatarPartRole::PointingFinger);
  addPart(rig, "left leg", MeshPrimitive::Sphere,
          {{-0.105f * h, -0.430f * h, 0.006f * h},
           {-0.03f, 0.0f, 0.055f},
           {0.070f * h, 0.220f * h, 0.062f * h}},
          spec.fur_material, AvatarJoint::LeftLeg, left_hip);
  addPart(rig, "right leg", MeshPrimitive::Sphere,
          {{0.105f * h, -0.430f * h, 0.006f * h},
           {-0.03f, 0.0f, -0.055f},
           {0.070f * h, 0.220f * h, 0.062f * h}},
          spec.fur_material, AvatarJoint::RightLeg, right_hip);
  addPart(rig, "left foot", MeshPrimitive::Sphere,
          {{-0.110f * h, -0.610f * h, 0.066f * h},
           {0.13f, 0.0f, 0.0f},
           {0.092f * h, 0.054f * h, 0.122f * h}},
          spec.fur_material, AvatarJoint::LeftLeg, left_hip);
  addPart(rig, "right foot", MeshPrimitive::Sphere,
          {{0.110f * h, -0.610f * h, 0.066f * h},
           {0.13f, 0.0f, 0.0f},
           {0.092f * h, 0.054f * h, 0.122f * h}},
          spec.fur_material, AvatarJoint::RightLeg, right_hip);

  return rig;
}

AvatarBounds avatarLocalBounds(const AvatarRig &rig) {
  AvatarBounds bounds;
  bounds.min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
  bounds.max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()};

  for (const AvatarPart &part : rig.parts) {
    const float extent_scale = primitiveExtentScale(part.primitive);
    for (int x = -1; x <= 1; x += 2) {
      for (int y = -1; y <= 1; y += 2) {
        for (int z = -1; z <= 1; z += 2) {
          expandBounds(bounds, transformedLocalPoint(part.local_transform,
                                                     {static_cast<float>(x) * extent_scale,
                                                      static_cast<float>(y) * extent_scale,
                                                      static_cast<float>(z) * extent_scale}));
        }
      }
    }
  }

  if (rig.parts.empty()) {
    bounds.min = {};
    bounds.max = {};
  }
  return bounds;
}

float avatarGroundSupportExtent(const AvatarRig &rig) {
  const AvatarBounds bounds = avatarLocalBounds(rig);
  return std::max(-bounds.min.y, 0.0f);
}

AvatarPose updateAvatarAnimator(AvatarAnimatorState &state, const AvatarAnimatorSettings &settings,
                                const AvatarAnimatorInput &input, const float dt) {
  const Vec2 planar_velocity{input.velocity.x, input.velocity.z};
  const float planar_speed = length(planar_velocity);
  const float max_planar_speed = std::max(input.max_planar_speed, 0.001f);
  const float target_stride = clamp(planar_speed / max_planar_speed, 0.0f, 1.0f) *
                              std::max(settings.max_stride_amplitude, 0.0f);
  const float reference_facing_yaw =
      state.initialized ? state.facing_yaw : input.desired_facing_yaw;
  AttentionAngles target_attention{};
  if (input.has_attention_target) {
    target_attention =
        attentionAngles(input.position, input.attention_target, reference_facing_yaw, settings);
  }
  const float target_head_yaw =
      input.has_attention_target ? clampSymmetric(target_attention.yaw, settings.head_yaw_limit)
                                 : clampSymmetric(input.head_yaw_offset, settings.head_yaw_limit);
  const float target_head_pitch =
      input.has_attention_target
          ? clampSymmetric(target_attention.pitch, settings.head_pitch_limit)
          : clampSymmetric(input.head_pitch_offset, settings.head_pitch_limit);
  const float target_point_yaw =
      input.has_attention_target ? clampSymmetric(target_attention.yaw, settings.point_yaw_limit)
                                 : 0.0f;
  const float target_point_pitch =
      input.has_attention_target
          ? clampSymmetric(target_attention.pitch, settings.point_pitch_limit)
          : 0.0f;
  const float target_point_blend =
      input.has_attention_target && input.pointing_enabled ? 1.0f : 0.0f;
  const float target_mouth_open = clamp(input.mouth_open, 0.0f, 1.0f);
  const float target_swim_blend = clamp(input.swim_blend, 0.0f, 1.0f);
  const float target_climb_blend = clamp(input.climb_blend, 0.0f, 1.0f);

  if (!state.initialized) {
    state.initialized = true;
    state.facing_yaw = input.desired_facing_yaw;
    state.stride_amplitude = target_stride;
    state.head_yaw_offset = target_head_yaw;
    state.head_pitch_offset = target_head_pitch;
    state.mouth_open = target_mouth_open;
    state.swim_blend = target_swim_blend;
    state.climb_blend = target_climb_blend;
    state.point_yaw_offset = target_point_yaw;
    state.point_pitch_offset = target_point_pitch;
    state.point_blend = target_point_blend;
  }

  if (input.has_facing_target) {
    state.facing_yaw = dampAngleCapped(state.facing_yaw, input.desired_facing_yaw,
                                       settings.facing_response, settings.max_facing_turn_rate, dt);
  }
  state.stride_amplitude =
      dampValue(state.stride_amplitude, target_stride, settings.stride_response, dt);
  state.head_yaw_offset =
      dampAngle(state.head_yaw_offset, target_head_yaw, settings.look_response, dt);
  state.head_pitch_offset =
      dampValue(state.head_pitch_offset, target_head_pitch, settings.look_response, dt);
  state.point_yaw_offset =
      dampAngle(state.point_yaw_offset, target_point_yaw, settings.point_response, dt);
  state.point_pitch_offset =
      dampValue(state.point_pitch_offset, target_point_pitch, settings.point_response, dt);
  state.point_blend = dampValue(state.point_blend, target_point_blend, settings.point_response, dt);
  state.mouth_open =
      dampValue(state.mouth_open, target_mouth_open, settings.expression_response, dt);
  state.swim_blend = dampValue(state.swim_blend, target_swim_blend, settings.stride_response, dt);
  state.climb_blend =
      dampValue(state.climb_blend, target_climb_blend, settings.stride_response, dt);
  state.gait_phase =
      wrapRadians(state.gait_phase + planar_speed * std::max(settings.gait_phase_per_meter, 0.0f) *
                                         std::max(dt, 0.0f));

  return {.position = input.position,
          .facing_yaw = state.facing_yaw,
          .gait_phase = state.gait_phase,
          .stride_amplitude = state.stride_amplitude,
          .vertical_bob = std::sin(state.gait_phase * 2.0f) * state.stride_amplitude *
                          std::max(settings.vertical_bob_amplitude, 0.0f),
          .head_yaw_offset = state.head_yaw_offset,
          .head_pitch_offset = state.head_pitch_offset,
          .mouth_open = state.mouth_open,
          .swim_blend = state.swim_blend,
          .climb_blend = state.climb_blend,
          .point_yaw_offset = state.point_yaw_offset,
          .point_pitch_offset = state.point_pitch_offset,
          .point_blend = state.point_blend};
}

AvatarInstance appendAvatar(Scene &scene, const AvatarRig &rig, const AvatarPose &pose) {
  AvatarInstance instance;
  instance.object_indices.reserve(rig.parts.size());
  for (const AvatarPart &part : rig.parts) {
    RenderObject object;
    object.name = part.name;
    object.primitive = part.primitive;
    object.material = part.material;
    object.camera_occlusion_fade = false;
    scene.objects().push_back(std::move(object));
    instance.object_indices.push_back(scene.objects().size() - 1u);
  }
  applyAvatarPose(scene, rig, instance, pose);
  return instance;
}

std::optional<Transform> resolveAvatarAttachmentSocket(const AvatarRig &rig, const AvatarPose &pose,
                                                       const AvatarAttachmentSocket &socket) {
  for (const AvatarPart &part : rig.parts) {
    if (part.name != socket.part_name) {
      continue;
    }
    Transform transform = posedAvatarPartTransform(part, pose);
    transform.position = transformedLocalPoint(transform, socket.local_position);
    transform.rotation = transform.rotation * quatFromEulerXyz(socket.local_rotation);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
  }
  return std::nullopt;
}

void applyAvatarPose(Scene &scene, const AvatarRig &rig, const AvatarInstance &instance,
                     const AvatarPose &pose) {
  if (instance.object_indices.size() != rig.parts.size()) {
    throw std::invalid_argument("Avatar instance does not match avatar rig.");
  }

  std::vector<RenderObject> &objects = scene.objects();
  for (std::size_t i = 0; i < rig.parts.size(); ++i) {
    const std::size_t object_index = instance.object_indices[i];
    if (object_index >= objects.size()) {
      throw std::out_of_range("Avatar object index is outside the scene.");
    }

    objects[object_index].transform = posedAvatarPartTransform(rig.parts[i], pose);
  }
}

} // namespace aster
