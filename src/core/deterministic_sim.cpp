// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/deterministic_sim.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr std::uint32_t kClassicNoiseSalt = 0x6d2b79f5u;

std::uint32_t mixStep(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

} // namespace

namespace aster {

bool SimCommand::pressed(const SimCommandButton button) const {
  return (buttons & static_cast<std::uint32_t>(button)) != 0u;
}

void SimCommand::set(const SimCommandButton button, const bool enabled) {
  const std::uint32_t bit = static_cast<std::uint32_t>(button);
  buttons = enabled ? (buttons | bit) : (buttons & ~bit);
}

std::uint32_t SimCommand::checksum(std::uint32_t seed) const {
  seed = hashCombine32(seed, tick);
  seed = hashCombine32(seed, static_cast<std::uint32_t>(static_cast<std::int32_t>(forward)));
  seed = hashCombine32(seed, static_cast<std::uint32_t>(static_cast<std::int32_t>(strafe)));
  seed = hashCombine32(seed, static_cast<std::uint32_t>(static_cast<std::int32_t>(turn)));
  seed = hashCombine32(seed, static_cast<std::uint32_t>(static_cast<std::int32_t>(look)));
  seed = hashCombine32(seed, buttons);
  return hashCombine32(seed, sequence);
}

DeterministicRandomStream::DeterministicRandomStream(const std::uint32_t seed) {
  reset(seed);
}

void DeterministicRandomStream::reset(const std::uint32_t seed) {
  seed_ = seed;
  state_ = mixStep(seed ^ kClassicNoiseSalt);
  draws_ = 0;
}

std::uint32_t DeterministicRandomStream::seed() const {
  return seed_;
}

std::uint32_t DeterministicRandomStream::draws() const {
  return draws_;
}

std::uint8_t DeterministicRandomStream::nextByte() {
  return static_cast<std::uint8_t>(nextU32() & 0xffu);
}

std::uint32_t DeterministicRandomStream::nextU32() {
  state_ += 0x9e3779b9u + draws_ * 0x85ebca6bu;
  state_ = mixStep(state_);
  ++draws_;
  return state_;
}

float DeterministicRandomStream::nextUnit() {
  return static_cast<float>(nextU32() >> 8u) * (1.0f / 16777215.0f);
}

float DeterministicRandomStream::nextSigned() {
  return nextUnit() * 2.0f - 1.0f;
}

Vec2 DeterministicRandomStream::nextUnitDisk() {
  const float angle = nextUnit() * 6.28318530717958647692f;
  const float radius = std::sqrt(std::max(nextUnit(), 0.0f));
  return {std::cos(angle) * radius, std::sin(angle) * radius};
}

void CommandReplay::clear() {
  frames_.clear();
}

void CommandReplay::record(SimCommand command) {
  if (!frames_.empty() && command.tick < frames_.back().tick) {
    const auto insertion =
        std::lower_bound(frames_.begin(), frames_.end(), command.tick,
                         [](const CommandReplayFrame &frame, const std::uint32_t tick) {
                           return frame.tick < tick;
                         });
    frames_.insert(insertion, {command.tick, command});
    return;
  }
  frames_.push_back({command.tick, command});
}

bool CommandReplay::empty() const {
  return frames_.empty();
}

std::size_t CommandReplay::size() const {
  return frames_.size();
}

std::span<const CommandReplayFrame> CommandReplay::frames() const {
  return frames_;
}

const CommandReplayFrame *CommandReplay::find(const std::uint32_t tick) const {
  const auto found = std::lower_bound(frames_.begin(), frames_.end(), tick,
                                      [](const CommandReplayFrame &frame,
                                         const std::uint32_t candidate) {
                                        return frame.tick < candidate;
                                      });
  if (found == frames_.end() || found->tick != tick) {
    return nullptr;
  }
  return &*found;
}

std::uint32_t CommandReplay::checksum(std::uint32_t seed) const {
  for (const CommandReplayFrame &frame : frames_) {
    seed = hashCombine32(seed, frame.command.checksum(frame.tick ^ 0x51f15eedu));
  }
  return seed;
}

Vec2 simCommandMoveAxis(const SimCommand &command, const float scale) {
  const float safe_scale = std::max(scale, 1.0f);
  Vec2 axis{static_cast<float>(command.strafe) / safe_scale,
            static_cast<float>(command.forward) / safe_scale};
  const float len = length(axis);
  if (len > 1.0f) {
    axis = axis / len;
  }
  return axis;
}

std::uint32_t checksumCommands(std::span<const SimCommand> commands, std::uint32_t seed) {
  for (const SimCommand &command : commands) {
    seed = hashCombine32(seed, command.checksum(seed ^ 0x91e10da5u));
  }
  return seed;
}

} // namespace aster
