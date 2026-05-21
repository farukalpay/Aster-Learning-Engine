// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/systems/classic_actor_runtime.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <cmath>

namespace {

float yawTo(const aster::Vec3 from, const aster::Vec3 to) {
  const aster::Vec3 delta = to - from;
  return std::atan2(delta.x, delta.z);
}

} // namespace

namespace aster {

void ClassicActorRuntime::clear() {
  actors_.clear();
}

std::size_t ClassicActorRuntime::spawn(const ClassicActorDesc &desc) {
  ClassicActorState state;
  state.id = desc.id;
  state.kind = desc.kind;
  state.position = desc.position;
  state.home = length(desc.home) > 0.0001f ? desc.home : desc.position;
  state.radius = std::max(desc.radius, 0.01f);
  state.speed = std::max(desc.speed, 0.0f);
  state.notice_radius = std::max(desc.notice_radius, 0.0f);
  state.strike_radius = std::max(desc.strike_radius, 0.0f);
  state.strike_cooldown = std::max(desc.strike_cooldown, 0.0f);
  state.health = std::max(desc.health, 0);
  state.max_health = state.health;
  state.seed = desc.seed;
  actors_.push_back(std::move(state));
  return actors_.size() - 1u;
}

bool ClassicActorRuntime::damage(const std::string_view id, const int amount, const Vec3) {
  if (amount <= 0) {
    return false;
  }
  for (ClassicActorState &actor : actors_) {
    if (actor.id != id || actor.mode == ClassicActorMode::Dead) {
      continue;
    }
    actor.health = std::max(actor.health - amount, 0);
    actor.flinch_seconds = 0.22f;
    actor.mode = actor.health <= 0 ? ClassicActorMode::Dead : ClassicActorMode::Flinch;
    return true;
  }
  return false;
}

ClassicActorFrame ClassicActorRuntime::update(const Vec3 target, const float dt) {
  const float step = std::max(dt, 0.0f);
  ClassicActorFrame frame;
  for (ClassicActorState &actor : actors_) {
    const ClassicActorMode previous = actor.mode;
    actor.age += step;
    actor.cooldown = std::max(0.0f, actor.cooldown - step);
    actor.flinch_seconds = std::max(0.0f, actor.flinch_seconds - step);
    bool strike = false;
    bool died = false;

    if (actor.health <= 0) {
      died = actor.mode != ClassicActorMode::Dead;
      actor.mode = ClassicActorMode::Dead;
      actor.velocity = {};
    } else if (actor.flinch_seconds > 0.0f) {
      actor.mode = ClassicActorMode::Flinch;
      actor.velocity = actor.velocity * 0.65f;
    } else {
      const Vec3 to_target = target - actor.position;
      const Vec2 planar_to_target{to_target.x, to_target.z};
      const float target_distance = length(planar_to_target);
      if (target_distance <= actor.strike_radius && actor.cooldown <= 0.0f) {
        actor.mode = ClassicActorMode::Strike;
        actor.cooldown = actor.strike_cooldown;
        actor.velocity = {};
        actor.facing_yaw = yawTo(actor.position, target);
        strike = true;
      } else if (target_distance <= actor.notice_radius) {
        actor.mode = target_distance <= actor.notice_radius * 0.72f ? ClassicActorMode::Chase
                                                                    : ClassicActorMode::Alert;
        const Vec3 planar_delta{to_target.x, 0.0f, to_target.z};
        const Vec3 direction = normalizeOr(planar_delta, Vec3{});
        actor.velocity = direction * actor.speed;
        actor.position = actor.position + actor.velocity * step;
        if (length(direction) > 0.0001f) {
          actor.facing_yaw = std::atan2(direction.x, direction.z);
        }
      } else {
        actor.mode = ClassicActorMode::Idle;
        const Vec3 home_delta = actor.home - actor.position;
        if (length(home_delta) > 0.08f) {
          const Vec3 planar_delta{home_delta.x, 0.0f, home_delta.z};
          const Vec3 direction = normalizeOr(planar_delta, Vec3{});
          actor.velocity = direction * (actor.speed * 0.32f);
          actor.position = actor.position + actor.velocity * step;
          actor.facing_yaw = std::atan2(direction.x, direction.z);
        } else {
          actor.velocity = {};
        }
      }
    }

    if (previous != actor.mode || strike || died) {
      frame.events.push_back({actor.id, actor.mode, strike, died});
    }
  }
  return frame;
}

const std::vector<ClassicActorState> &ClassicActorRuntime::actors() const {
  return actors_;
}

const ClassicActorState *ClassicActorRuntime::find(const std::string_view id) const {
  const auto found = std::find_if(actors_.begin(), actors_.end(),
                                  [id](const ClassicActorState &actor) {
                                    return actor.id == id;
                                  });
  return found == actors_.end() ? nullptr : &*found;
}

std::uint32_t ClassicActorRuntime::checksum() const {
  std::uint32_t seed = 0xA57EAC70u;
  for (const ClassicActorState &actor : actors_) {
    seed = hashCombine32(seed, stableHash32(actor.position));
    seed = hashCombine32(seed, static_cast<std::uint32_t>(actor.mode));
    seed = hashCombine32(seed, static_cast<std::uint32_t>(actor.health));
  }
  return seed;
}

} // namespace aster
