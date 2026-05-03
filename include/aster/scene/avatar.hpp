// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/transform.hpp"
#include "aster/scene/scene.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace aster {

enum class AvatarJoint {
  Static,
  Torso,
  Head,
  Mouth,
  LeftArm,
  RightArm,
  LeftLeg,
  RightLeg,
};

enum class AvatarPartRole {
  Body,
  PointingFinger,
};

struct AvatarPart {
  std::string name;
  MeshPrimitive primitive = MeshPrimitive::Sphere;
  Transform local_transform{};
  Vec3 joint_pivot{};
  Material material{};
  AvatarJoint joint = AvatarJoint::Static;
  AvatarPartRole role = AvatarPartRole::Body;
};

struct AvatarRig {
  std::string name;
  std::vector<AvatarPart> parts;
};

struct AvatarBounds {
  Vec3 min{};
  Vec3 max{};
};

struct AvatarPose {
  Vec3 position{};
  float facing_yaw = 0.0f;
  float gait_phase = 0.0f;
  float stride_amplitude = 0.0f;
  float vertical_bob = 0.0f;
  float head_yaw_offset = 0.0f;
  float head_pitch_offset = 0.0f;
  float mouth_open = 0.0f;
  float swim_blend = 0.0f;
  float climb_blend = 0.0f;
  float point_yaw_offset = 0.0f;
  float point_pitch_offset = 0.0f;
  float point_blend = 0.0f;
};

struct AvatarInstance {
  std::vector<std::size_t> object_indices;
};

struct AvatarAttachmentSocket {
  std::string part_name;
  Vec3 local_position{};
  Vec3 local_rotation{};
};

struct AvatarAnimatorSettings {
  float facing_response = 14.0f;
  float max_facing_turn_rate = radians(520.0f);
  float stride_response = 12.0f;
  float look_response = 18.0f;
  float point_response = 15.0f;
  float expression_response = 22.0f;
  float gait_phase_per_meter = 6.8f;
  float max_stride_amplitude = 0.58f;
  float vertical_bob_amplitude = 0.016f;
  float head_yaw_limit = radians(74.0f);
  float head_pitch_limit = radians(26.0f);
  float point_yaw_limit = radians(86.0f);
  float point_pitch_limit = radians(44.0f);
};

struct AvatarAnimatorInput {
  Vec3 position{};
  Vec3 velocity{};
  float desired_facing_yaw = 0.0f;
  bool has_facing_target = false;
  float max_planar_speed = 1.0f;
  float head_yaw_offset = 0.0f;
  float head_pitch_offset = 0.0f;
  bool has_attention_target = false;
  Vec3 attention_target{};
  bool pointing_enabled = false;
  float mouth_open = 0.0f;
  float swim_blend = 0.0f;
  float climb_blend = 0.0f;
};

struct AvatarAnimatorState {
  bool initialized = false;
  float facing_yaw = 0.0f;
  float gait_phase = 0.0f;
  float stride_amplitude = 0.0f;
  float head_yaw_offset = 0.0f;
  float head_pitch_offset = 0.0f;
  float mouth_open = 0.0f;
  float swim_blend = 0.0f;
  float climb_blend = 0.0f;
  float point_yaw_offset = 0.0f;
  float point_pitch_offset = 0.0f;
  float point_blend = 0.0f;
};

struct PlushHumanoidAvatarSpec {
  float height = 0.78f;
  Material fur_material{};
  Material muzzle_material{};
  Material eye_material{};
  Material nose_material{};
};

[[nodiscard]] AvatarRig makePlushHumanoidAvatar(const PlushHumanoidAvatarSpec &spec);
[[nodiscard]] AvatarBounds avatarLocalBounds(const AvatarRig &rig);
[[nodiscard]] float avatarGroundSupportExtent(const AvatarRig &rig);
[[nodiscard]] AvatarPose updateAvatarAnimator(AvatarAnimatorState &state,
                                              const AvatarAnimatorSettings &settings,
                                              const AvatarAnimatorInput &input, float dt);
[[nodiscard]] AvatarInstance appendAvatar(Scene &scene, const AvatarRig &rig,
                                          const AvatarPose &pose);
[[nodiscard]] std::optional<Transform>
resolveAvatarAttachmentSocket(const AvatarRig &rig, const AvatarPose &pose,
                              const AvatarAttachmentSocket &socket);
void applyAvatarPose(Scene &scene, const AvatarRig &rig, const AvatarInstance &instance,
                     const AvatarPose &pose);

} // namespace aster
