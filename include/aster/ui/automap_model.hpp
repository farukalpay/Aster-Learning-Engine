// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

#include <string>
#include <vector>

namespace aster {

enum class AutomapMarkerKind {
  Player,
  Door,
  Lift,
  Threat,
  Secret,
  Objective,
};

struct AutomapLine {
  Vec2 from{};
  Vec2 to{};
  bool discovered = false;
  bool blocking = false;
};

struct AutomapMarker {
  std::string id;
  AutomapMarkerKind kind = AutomapMarkerKind::Objective;
  Vec2 position{};
  bool discovered = false;
  float pulse = 0.0f;
};

struct AutomapProjectedPoint {
  Vec2 position{};
  bool visible = false;
};

struct AutomapView {
  Vec2 center{};
  Vec2 half_extents{8.0f, 8.0f};
  float pixels_per_meter = 18.0f;
  bool follow_player = true;
};

class AutomapModel {
public:
  void clear();
  void addLine(AutomapLine line);
  void addMarker(AutomapMarker marker);
  void setPlayer(Vec2 position, float yaw);
  void revealWithin(Vec2 center, float radius);

  [[nodiscard]] AutomapProjectedPoint project(Vec2 world, Vec2 viewport,
                                              AutomapView view = {}) const;
  [[nodiscard]] const std::vector<AutomapLine> &lines() const;
  [[nodiscard]] const std::vector<AutomapMarker> &markers() const;
  [[nodiscard]] Vec2 playerPosition() const;
  [[nodiscard]] float playerYaw() const;
  [[nodiscard]] bool hasDiscovery() const;

private:
  std::vector<AutomapLine> lines_;
  std::vector<AutomapMarker> markers_;
  Vec2 player_position_{};
  float player_yaw_ = 0.0f;
};

} // namespace aster
