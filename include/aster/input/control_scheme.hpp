// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace aster {

enum class ControlDevice {
  Keyboard,
  MouseButton,
};

struct ControlBinding {
  ControlDevice device = ControlDevice::Keyboard;
  int code = 0;
  float scale = 1.0f;
};

struct ControlCommand {
  std::string name;
  float deadzone = 0.0f;
  std::vector<ControlBinding> bindings;
};

struct ControlSnapshot {
  std::vector<int> pressed_keys;
  std::vector<int> pressed_mouse_buttons;
  Vec2 pointer{};
  Vec2 scroll{};
};

struct CommandSignal {
  std::string name;
  float previous_strength = 0.0f;
  float strength = 0.0f;
};

class ControlScheme {
public:
  void addCommand(std::string name, float deadzone = 0.0f);
  void bind(std::string command_name, ControlBinding binding);
  [[nodiscard]] bool hasCommand(const std::string &name) const;
  [[nodiscard]] const ControlCommand &command(const std::string &name) const;
  [[nodiscard]] std::vector<ControlCommand> commands() const;
  [[nodiscard]] const std::vector<std::string> &commandNames() const;
  [[nodiscard]] const std::vector<int> &trackedKeys() const;
  [[nodiscard]] const std::vector<int> &trackedMouseButtons() const;

private:
  void registerTrackedCode(ControlBinding binding);

  std::unordered_map<std::string, ControlCommand> commands_;
  std::vector<std::string> command_order_;
  std::vector<int> tracked_keys_;
  std::vector<int> tracked_mouse_buttons_;
};

class ControlState {
public:
  void update(const ControlScheme &scheme, const ControlSnapshot &snapshot);

  [[nodiscard]] float strength(const std::string &command_name) const;
  [[nodiscard]] bool pressed(const std::string &command_name) const;
  [[nodiscard]] bool justPressed(const std::string &command_name) const;
  [[nodiscard]] bool justReleased(const std::string &command_name) const;
  [[nodiscard]] const ControlSnapshot &snapshot() const;
  [[nodiscard]] std::vector<CommandSignal> commands() const;

private:
  std::unordered_map<std::string, CommandSignal> signals_;
  ControlSnapshot snapshot_{};
};

} // namespace aster
