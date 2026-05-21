// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/ui/automap_model.hpp"

#include <algorithm>
#include <cmath>

namespace {

float pointSegmentDistance(const aster::Vec2 point, const aster::Vec2 from, const aster::Vec2 to) {
  const aster::Vec2 edge = to - from;
  const float denom = std::max(aster::dot(edge, edge), 0.0001f);
  const float t = std::clamp(aster::dot(point - from, edge) / denom, 0.0f, 1.0f);
  return aster::length(point - (from + edge * t));
}

} // namespace

namespace aster {

void AutomapModel::clear() {
  lines_.clear();
  markers_.clear();
  player_position_ = {};
  player_yaw_ = 0.0f;
}

void AutomapModel::addLine(AutomapLine line) {
  lines_.push_back(line);
}

void AutomapModel::addMarker(AutomapMarker marker) {
  markers_.push_back(std::move(marker));
}

void AutomapModel::setPlayer(const Vec2 position, const float yaw) {
  player_position_ = position;
  player_yaw_ = yaw;
}

void AutomapModel::revealWithin(const Vec2 center, const float radius) {
  const float safe_radius = std::max(radius, 0.0f);
  for (AutomapLine &line : lines_) {
    line.discovered =
        line.discovered || pointSegmentDistance(center, line.from, line.to) <= safe_radius;
  }
  for (AutomapMarker &marker : markers_) {
    marker.discovered = marker.discovered || length(marker.position - center) <= safe_radius;
  }
}

AutomapProjectedPoint AutomapModel::project(const Vec2 world, const Vec2 viewport,
                                            AutomapView view) const {
  if (view.follow_player) {
    view.center = player_position_;
  }
  const Vec2 delta = world - view.center;
  if (std::abs(delta.x) > view.half_extents.x || std::abs(delta.y) > view.half_extents.y) {
    return {{}, false};
  }
  return {{viewport.x * 0.5f + delta.x * view.pixels_per_meter,
           viewport.y * 0.5f - delta.y * view.pixels_per_meter},
          true};
}

const std::vector<AutomapLine> &AutomapModel::lines() const {
  return lines_;
}

const std::vector<AutomapMarker> &AutomapModel::markers() const {
  return markers_;
}

Vec2 AutomapModel::playerPosition() const {
  return player_position_;
}

float AutomapModel::playerYaw() const {
  return player_yaw_;
}

bool AutomapModel::hasDiscovery() const {
  const bool line = std::any_of(lines_.begin(), lines_.end(),
                                [](const AutomapLine &entry) { return entry.discovered; });
  const bool marker = std::any_of(markers_.begin(), markers_.end(),
                                  [](const AutomapMarker &entry) { return entry.discovered; });
  return line || marker;
}

} // namespace aster
