// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/core/deterministic_sim.hpp"
#include "aster/math/vec.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster {

enum class ClassicActorKind {
  Scout,
  Bruiser,
  Projectile,
};

enum class ClassicActorMode {
  Idle,
  Alert,
  Chase,
  Strike,
  Flinch,
  Dead,
};

struct ClassicActorDesc {
  std::string id;
  ClassicActorKind kind = ClassicActorKind::Scout;
  Vec3 position{};
  Vec3 home{};
  float radius = 0.32f;
  float speed = 1.0f;
  float notice_radius = 4.0f;
  float strike_radius = 0.55f;
  float strike_cooldown = 1.0f;
  int health = 3;
  std::uint32_t seed = 0;
};

struct ClassicActorState {
  std::string id;
  ClassicActorKind kind = ClassicActorKind::Scout;
  ClassicActorMode mode = ClassicActorMode::Idle;
  Vec3 position{};
  Vec3 velocity{};
  Vec3 home{};
  float facing_yaw = 0.0f;
  float radius = 0.32f;
  float speed = 1.0f;
  float notice_radius = 4.0f;
  float strike_radius = 0.55f;
  float strike_cooldown = 1.0f;
  float cooldown = 0.0f;
  float flinch_seconds = 0.0f;
  float age = 0.0f;
  int health = 3;
  int max_health = 3;
  std::uint32_t seed = 0;
};

struct ClassicActorEvent {
  std::string actor_id;
  ClassicActorMode mode = ClassicActorMode::Idle;
  bool strike = false;
  bool died = false;
};

struct ClassicActorFrame {
  std::vector<ClassicActorEvent> events;
};

class ClassicActorRuntime {
public:
  void clear();
  std::size_t spawn(const ClassicActorDesc &desc);
  bool damage(std::string_view id, int amount, Vec3 impulse_origin = {});
  ClassicActorFrame update(Vec3 target, float dt);

  [[nodiscard]] const std::vector<ClassicActorState> &actors() const;
  [[nodiscard]] const ClassicActorState *find(std::string_view id) const;
  [[nodiscard]] std::uint32_t checksum() const;

private:
  std::vector<ClassicActorState> actors_;
};

} // namespace aster
