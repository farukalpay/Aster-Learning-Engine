// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <string>
#include <vector>

namespace aster {

enum class InteractionTargetKind {
  Generic,
  Container,
  Item,
};

enum class InteractionTargetShape {
  Sphere,
  ExplicitHit,
};

struct InteractionTarget {
  std::string id;
  InteractionTargetKind kind = InteractionTargetKind::Generic;
  InteractionTargetShape shape = InteractionTargetShape::Sphere;
  std::string action_label;
  std::string subject_label;
  Vec3 position{};
  float radius = 0.35f;
  float max_distance = 8.0f;
  float proximity_distance = 0.0f;
  float hit_distance = 0.0f;
  float evidence_strength = 1.0f;
  bool occluded = false;
  bool enabled = true;
};

struct InteractionFocus {
  std::string target_id;
  InteractionTargetKind kind = InteractionTargetKind::Generic;
  std::string action_label;
  std::string subject_label;
  Vec3 position{};
  float distance = 0.0f;
  float strength = 0.0f;
  bool visible = false;
};

class InteractionSystem {
public:
  void clear();
  void update(const std::vector<InteractionTarget> &targets, Vec3 ray_origin, Vec3 ray_direction,
              float dt);
  void update(const std::vector<InteractionTarget> &targets, Vec3 ray_origin, Vec3 ray_direction,
              Vec3 actor_position, float dt);

  [[nodiscard]] const InteractionFocus &focus() const;

private:
  InteractionFocus focus_{};
};

} // namespace aster
