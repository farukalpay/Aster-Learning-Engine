// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/systems/world_mechanism.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aster {

void WorldMechanismSystem::clear() {
  states_.clear();
}

std::size_t WorldMechanismSystem::add(WorldMechanismDesc desc) {
  WorldMechanismState state;
  state.id = std::move(desc.id);
  state.kind = desc.kind;
  state.closed_position = desc.closed_position;
  state.open_position = desc.open_position;
  state.position = desc.closed_position;
  state.speed = std::max(desc.speed, 0.0f);
  state.hold_seconds = std::max(desc.hold_seconds, 0.0f);
  state.light_intensity_closed = desc.light_intensity_closed;
  state.light_intensity_open = desc.light_intensity_open;
  state.material_frames = std::max(desc.material_frames, 1);
  states_.push_back(std::move(state));
  return states_.size() - 1u;
}

bool WorldMechanismSystem::trigger(const std::string_view id, const bool open) {
  WorldMechanismState *state = findMutable(id);
  if (state == nullptr) {
    return false;
  }
  state->mode = open ? WorldMechanismMode::Opening : WorldMechanismMode::Closing;
  if (open) {
    state->hold_timer = state->hold_seconds;
  }
  return true;
}

void WorldMechanismSystem::update(const float dt) {
  const float step = std::max(dt, 0.0f);
  for (WorldMechanismState &state : states_) {
    if (state.kind == WorldMechanismKind::MaterialCycle) {
      state.progress = std::fmod(state.progress + step * std::max(state.speed, 0.0f), 1.0f);
      state.material_frame =
          static_cast<int>(std::floor(state.progress * static_cast<float>(state.material_frames))) %
          std::max(state.material_frames, 1);
      continue;
    }
    if (state.mode == WorldMechanismMode::Opening) {
      state.progress = std::min(1.0f, state.progress + state.speed * step);
      if (state.progress >= 1.0f) {
        state.mode = WorldMechanismMode::Open;
      }
    } else if (state.mode == WorldMechanismMode::Open && state.hold_seconds > 0.0f) {
      state.hold_timer -= step;
      if (state.hold_timer <= 0.0f) {
        state.mode = WorldMechanismMode::Closing;
      }
    } else if (state.mode == WorldMechanismMode::Closing) {
      state.progress = std::max(0.0f, state.progress - state.speed * step);
      if (state.progress <= 0.0f) {
        state.mode = WorldMechanismMode::Resting;
      }
    }
    state.position = state.closed_position + (state.open_position - state.closed_position) *
                                                std::clamp(state.progress, 0.0f, 1.0f);
  }
}

const WorldMechanismState *WorldMechanismSystem::find(const std::string_view id) const {
  const auto found = std::find_if(states_.begin(), states_.end(),
                                  [id](const WorldMechanismState &state) {
                                    return state.id == id;
                                  });
  return found == states_.end() ? nullptr : &*found;
}

WorldMechanismState *WorldMechanismSystem::findMutable(const std::string_view id) {
  const auto found = std::find_if(states_.begin(), states_.end(),
                                  [id](const WorldMechanismState &state) {
                                    return state.id == id;
                                  });
  return found == states_.end() ? nullptr : &*found;
}

const std::vector<WorldMechanismState> &WorldMechanismSystem::states() const {
  return states_;
}

float mechanismLightIntensity(const WorldMechanismState &state) {
  return std::lerp(state.light_intensity_closed, state.light_intensity_open,
                   std::clamp(state.progress, 0.0f, 1.0f));
}

} // namespace aster
