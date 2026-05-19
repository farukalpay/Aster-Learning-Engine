// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace aster {

enum class SimCommandButton : std::uint32_t {
  Primary = 1u << 0u,
  Secondary = 1u << 1u,
  Interact = 1u << 2u,
  Jump = 1u << 3u,
  Run = 1u << 4u,
  Menu = 1u << 5u,
};

struct SimCommand {
  std::uint32_t tick = 0;
  std::int16_t forward = 0;
  std::int16_t strafe = 0;
  std::int16_t turn = 0;
  std::int16_t look = 0;
  std::uint32_t buttons = 0;
  std::uint32_t sequence = 0;

  [[nodiscard]] bool pressed(SimCommandButton button) const;
  void set(SimCommandButton button, bool enabled);
  [[nodiscard]] std::uint32_t checksum(std::uint32_t seed = 0xA57E51C3u) const;
};

class DeterministicRandomStream {
public:
  explicit DeterministicRandomStream(std::uint32_t seed = 0u);

  void reset(std::uint32_t seed = 0u);
  [[nodiscard]] std::uint32_t seed() const;
  [[nodiscard]] std::uint32_t draws() const;
  [[nodiscard]] std::uint8_t nextByte();
  [[nodiscard]] std::uint32_t nextU32();
  [[nodiscard]] float nextUnit();
  [[nodiscard]] float nextSigned();
  [[nodiscard]] Vec2 nextUnitDisk();

private:
  std::uint32_t seed_ = 0;
  std::uint32_t state_ = 0;
  std::uint32_t draws_ = 0;
};

struct CommandReplayFrame {
  std::uint32_t tick = 0;
  SimCommand command{};
};

class CommandReplay {
public:
  void clear();
  void record(SimCommand command);
  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::span<const CommandReplayFrame> frames() const;
  [[nodiscard]] const CommandReplayFrame *find(std::uint32_t tick) const;
  [[nodiscard]] std::uint32_t checksum(std::uint32_t seed = 0xA57ECAFEu) const;

private:
  std::vector<CommandReplayFrame> frames_;
};

[[nodiscard]] Vec2 simCommandMoveAxis(const SimCommand &command, float scale = 32767.0f);
[[nodiscard]] std::uint32_t checksumCommands(std::span<const SimCommand> commands,
                                             std::uint32_t seed = 0xA57E1001u);

} // namespace aster
