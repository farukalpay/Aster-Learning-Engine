// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class WorldMechanismKind {
  Door,
  Lift,
  TimedLight,
  MaterialCycle,
};

enum class WorldMechanismMode {
  Resting,
  Opening,
  Open,
  Closing,
};

struct WorldMechanismDesc {
  std::string id;
  WorldMechanismKind kind = WorldMechanismKind::Door;
  Vec3 closed_position{};
  Vec3 open_position{};
  float speed = 1.0f;
  float hold_seconds = 0.0f;
  float light_intensity_closed = 0.0f;
  float light_intensity_open = 1.0f;
  int material_frames = 1;
};

struct WorldMechanismState {
  std::string id;
  WorldMechanismKind kind = WorldMechanismKind::Door;
  WorldMechanismMode mode = WorldMechanismMode::Resting;
  Vec3 closed_position{};
  Vec3 open_position{};
  Vec3 position{};
  float progress = 0.0f;
  float speed = 1.0f;
  float hold_seconds = 0.0f;
  float hold_timer = 0.0f;
  float light_intensity_closed = 0.0f;
  float light_intensity_open = 1.0f;
  int material_frames = 1;
  int material_frame = 0;
};

class WorldMechanismSystem {
public:
  void clear();
  std::size_t add(WorldMechanismDesc desc);
  bool trigger(std::string_view id, bool open = true);
  void update(float dt);

  [[nodiscard]] const WorldMechanismState *find(std::string_view id) const;
  [[nodiscard]] WorldMechanismState *findMutable(std::string_view id);
  [[nodiscard]] const std::vector<WorldMechanismState> &states() const;

private:
  std::vector<WorldMechanismState> states_;
};

[[nodiscard]] float mechanismLightIntensity(const WorldMechanismState &state);

} // namespace aster
