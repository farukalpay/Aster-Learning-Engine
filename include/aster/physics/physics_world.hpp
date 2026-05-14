// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/transform.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace aster {

enum class PhysicsBodyType {
  Static,
  Dynamic,
};

enum class PhysicsShapeType {
  Box,
  Sphere,
  Capsule,
  TriangleMesh,
};

struct PhysicsBodyHandle {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;
};

struct PhysicsConstraintHandle {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;
};

struct PhysicsFluidHandle {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;
};

struct PhysicsMaterial {
  float friction = 0.72f;
  float restitution = 0.02f;
};

struct PhysicsCollisionFilter {
  std::uint32_t layer_bits = 1u;
  std::uint32_t collides_with = 0xffffffffu;
  bool sensor = false;
  bool query_enabled = true;
};

struct PhysicsBodyDesc {
  PhysicsBodyType type = PhysicsBodyType::Static;
  PhysicsShapeType shape = PhysicsShapeType::Box;
  Vec3 position{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  float mass = 1.0f;
  PhysicsMaterial material{};
  float linear_damping = 0.08f;
  PhysicsCollisionFilter filter{};
  bool allow_sleep = true;
  std::shared_ptr<const CpuMesh> mesh{};
  Transform mesh_transform{};
  bool mesh_double_sided = true;
};

enum class DistanceConstraintMode {
  Fixed,
  Maximum,
};

struct DistanceConstraintDesc {
  PhysicsBodyHandle body{};
  Vec3 world_anchor{};
  float distance = 1.0f;
  float stiffness = 1.0f;
  float damping = 0.08f;
  DistanceConstraintMode mode = DistanceConstraintMode::Fixed;
};

struct PhysicsMeshTriangle {
  Vec3 a{};
  Vec3 b{};
  Vec3 c{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
};

struct PhysicsBody {
  PhysicsBodyType type = PhysicsBodyType::Static;
  PhysicsShapeType shape = PhysicsShapeType::Box;
  Vec3 position{};
  Vec3 previous_position{};
  Vec3 velocity{};
  Vec3 force{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  float inverse_mass = 0.0f;
  PhysicsMaterial material{};
  float linear_damping = 0.08f;
  PhysicsCollisionFilter filter{};
  std::uint32_t generation = 1;
  bool active = true;
  bool allow_sleep = true;
  bool sleeping = false;
  float sleep_timer = 0.0f;
  bool mesh_double_sided = true;
  Vec3 mesh_bounds_min{};
  Vec3 mesh_bounds_max{};
  std::vector<PhysicsMeshTriangle> mesh_triangles;
};

struct PhysicsContact {
  PhysicsBodyHandle body_a{};
  PhysicsBodyHandle body_b{};
  Vec3 point{};
  Vec3 normal{};
  float penetration = 0.0f;
};

struct PhysicsBroadphasePair {
  PhysicsBodyHandle body_a{};
  PhysicsBodyHandle body_b{};
};

struct PhysicsQueryFilter {
  std::uint32_t collides_with = 0xffffffffu;
  bool include_sensors = false;
  PhysicsBodyHandle ignore_body{};
};

struct PhysicsRay {
  Vec3 origin{};
  Vec3 direction{0.0f, -1.0f, 0.0f};
  float max_distance = 1.0f;
  PhysicsQueryFilter filter{};
};

struct PhysicsRayHit {
  PhysicsBodyHandle body{};
  Vec3 point{};
  Vec3 normal{};
  float distance = 0.0f;
  float fraction = 0.0f;
};

struct PhysicsSphereCast {
  Vec3 origin{};
  Vec3 displacement{};
  float radius = 0.5f;
  PhysicsQueryFilter filter{};
};

struct PhysicsShapeCastHit {
  PhysicsBodyHandle body{};
  Vec3 point{};
  Vec3 normal{};
  float distance = 0.0f;
  float fraction = 0.0f;
};

struct PhysicsOverlapHit {
  PhysicsBodyHandle body{};
};

struct PhysicsFluidVolumeDesc {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float surface_y = 0.0f;
  float density = 1.0f;
  float linear_drag = 2.0f;
  Vec3 flow{};
  std::uint32_t affects_layers = 0xffffffffu;
};

struct PhysicsFluidSample {
  bool submerged = false;
  float submersion = 0.0f;
  float surface_y = 0.0f;
  float depth_below_surface = 0.0f;
  Vec3 flow{};
};

struct PhysicsSettings {
  Vec3 gravity{0.0f, -9.81f, 0.0f};
  int solver_iterations = 6;
  float max_step = 1.0f / 30.0f;
  float sleep_linear_threshold = 0.035f;
  float sleep_time_threshold = 0.55f;
};

struct CharacterControllerSettings {
  float acceleration = 28.0f;
  float air_acceleration = 7.5f;
  float braking = 22.0f;
  float ground_probe_distance = 0.16f;
  float max_slope_angle = 0.86f;
  float jump_speed = 0.0f;
};

struct CharacterMoveInput {
  Vec3 desired_velocity{};
  bool jump_requested = false;
};

struct CharacterMoveResult {
  bool grounded = false;
  Vec3 ground_normal{0.0f, 1.0f, 0.0f};
  Vec3 velocity{};
  float ground_distance = 0.0f;
};

class PhysicsWorld {
public:
  void clear();
  void setSettings(PhysicsSettings settings);

  [[nodiscard]] PhysicsBodyHandle addBody(const PhysicsBodyDesc &desc);
  [[nodiscard]] PhysicsConstraintHandle addDistanceConstraint(const DistanceConstraintDesc &desc);
  [[nodiscard]] PhysicsFluidHandle addFluidVolume(const PhysicsFluidVolumeDesc &desc);

  bool removeBody(PhysicsBodyHandle handle);
  bool removeConstraint(PhysicsConstraintHandle handle);

  [[nodiscard]] bool valid(PhysicsBodyHandle handle) const;
  [[nodiscard]] bool valid(PhysicsConstraintHandle handle) const;
  [[nodiscard]] bool valid(PhysicsFluidHandle handle) const;

  [[nodiscard]] PhysicsBody &body(PhysicsBodyHandle handle);
  [[nodiscard]] const PhysicsBody &body(PhysicsBodyHandle handle) const;

  void applyForce(PhysicsBodyHandle handle, Vec3 force);
  void applyImpulse(PhysicsBodyHandle handle, Vec3 impulse);
  void setVelocity(PhysicsBodyHandle handle, Vec3 velocity);
  void setPosition(PhysicsBodyHandle handle, Vec3 position);
  void wakeBody(PhysicsBodyHandle handle);

  void step(float dt);

  [[nodiscard]] bool raycast(const PhysicsRay &ray, PhysicsRayHit &hit) const;
  [[nodiscard]] bool castSphere(const PhysicsSphereCast &cast, PhysicsShapeCastHit &hit) const;
  [[nodiscard]] std::vector<PhysicsOverlapHit> overlapSphere(Vec3 center, float radius,
                                                             PhysicsQueryFilter filter = {}) const;
  [[nodiscard]] PhysicsFluidSample sampleFluid(PhysicsBodyHandle handle) const;

  [[nodiscard]] CharacterMoveResult moveCharacter(PhysicsBodyHandle handle,
                                                  const CharacterMoveInput &input,
                                                  const CharacterControllerSettings &settings,
                                                  float dt);

  [[nodiscard]] const std::vector<PhysicsContact> &contacts() const {
    return contacts_;
  }

  [[nodiscard]] const std::vector<PhysicsBroadphasePair> &broadphasePairs() const {
    return broadphase_pairs_;
  }

private:
  struct DistanceConstraint {
    DistanceConstraintDesc desc{};
    std::uint32_t generation = 1;
    bool active = true;
  };

  struct FluidVolume {
    PhysicsFluidVolumeDesc desc{};
    std::uint32_t generation = 1;
    bool active = true;
  };

  void updateSleeping(float dt);
  void applyFluidForces();
  void integrate(float dt);
  void buildBroadphasePairs();
  void solveConstraints(float dt);
  void solveCollisions();
  void solvePair(PhysicsBodyHandle handle_a, PhysicsBody &body_a, PhysicsBodyHandle handle_b,
                 PhysicsBody &body_b);

  PhysicsSettings settings_{};
  std::vector<PhysicsBody> bodies_;
  std::vector<DistanceConstraint> constraints_;
  std::vector<FluidVolume> fluid_volumes_;
  std::vector<PhysicsContact> contacts_;
  std::vector<PhysicsBroadphasePair> broadphase_pairs_;
};

[[nodiscard]] bool samePhysicsHandle(PhysicsBodyHandle lhs, PhysicsBodyHandle rhs);

} // namespace aster
