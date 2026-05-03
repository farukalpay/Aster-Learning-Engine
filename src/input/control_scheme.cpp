// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/input/control_scheme.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

bool containsCode(const std::vector<int> &codes, const int code) {
  return std::find(codes.begin(), codes.end(), code) != codes.end();
}

float bindingStrength(const aster::ControlBinding &binding,
                      const aster::ControlSnapshot &snapshot) {
  switch (binding.device) {
  case aster::ControlDevice::Keyboard:
    return containsCode(snapshot.pressed_keys, binding.code) ? binding.scale : 0.0f;
  case aster::ControlDevice::MouseButton:
    return containsCode(snapshot.pressed_mouse_buttons, binding.code) ? binding.scale : 0.0f;
  }
  return 0.0f;
}

void pushUniqueSorted(std::vector<int> &values, const int value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
    std::sort(values.begin(), values.end());
  }
}

} // namespace

namespace aster {

void ControlScheme::addCommand(std::string name, const float deadzone) {
  if (name.empty()) {
    throw std::invalid_argument("Control command names must not be empty.");
  }
  if (commands_.contains(name)) {
    throw std::invalid_argument("Control command already exists: " + name);
  }
  const std::string key = name;
  ControlCommand command;
  command.name = std::move(name);
  command.deadzone = deadzone;
  commands_.emplace(key, std::move(command));
  command_order_.push_back(key);
  std::sort(command_order_.begin(), command_order_.end());
}

void ControlScheme::bind(std::string command_name, ControlBinding binding) {
  auto found = commands_.find(command_name);
  if (found == commands_.end()) {
    addCommand(command_name);
    found = commands_.find(command_name);
  }

  auto &bindings = found->second.bindings;
  const auto duplicate =
      std::find_if(bindings.begin(), bindings.end(), [&](const ControlBinding &value) {
        return value.device == binding.device && value.code == binding.code;
      });
  if (duplicate == bindings.end()) {
    bindings.push_back(binding);
    registerTrackedCode(binding);
  }
}

bool ControlScheme::hasCommand(const std::string &name) const {
  return commands_.contains(name);
}

const ControlCommand &ControlScheme::command(const std::string &name) const {
  const auto found = commands_.find(name);
  if (found == commands_.end()) {
    throw std::out_of_range("Control command not found: " + name);
  }
  return found->second;
}

std::vector<ControlCommand> ControlScheme::commands() const {
  std::vector<ControlCommand> result;
  result.reserve(commands_.size());
  for (const std::string &name : command_order_) {
    result.push_back(command(name));
  }
  return result;
}

const std::vector<std::string> &ControlScheme::commandNames() const {
  return command_order_;
}

const std::vector<int> &ControlScheme::trackedKeys() const {
  return tracked_keys_;
}

const std::vector<int> &ControlScheme::trackedMouseButtons() const {
  return tracked_mouse_buttons_;
}

void ControlScheme::registerTrackedCode(const ControlBinding binding) {
  switch (binding.device) {
  case ControlDevice::Keyboard:
    pushUniqueSorted(tracked_keys_, binding.code);
    break;
  case ControlDevice::MouseButton:
    pushUniqueSorted(tracked_mouse_buttons_, binding.code);
    break;
  }
}

void ControlState::update(const ControlScheme &scheme, const ControlSnapshot &snapshot) {
  snapshot_ = snapshot;

  for (const std::string &name : scheme.commandNames()) {
    const ControlCommand &command = scheme.command(name);
    CommandSignal &signal = signals_[command.name];
    signal.name = command.name;
    signal.previous_strength = signal.strength;

    float resolved_strength = 0.0f;
    for (const ControlBinding &binding : command.bindings) {
      const float candidate = bindingStrength(binding, snapshot);
      if (std::abs(candidate) > std::abs(resolved_strength)) {
        resolved_strength = candidate;
      }
    }
    signal.strength = std::abs(resolved_strength) >= command.deadzone ? resolved_strength : 0.0f;
  }
}

float ControlState::strength(const std::string &command_name) const {
  const auto found = signals_.find(command_name);
  return found == signals_.end() ? 0.0f : found->second.strength;
}

bool ControlState::pressed(const std::string &command_name) const {
  return strength(command_name) != 0.0f;
}

bool ControlState::justPressed(const std::string &command_name) const {
  const auto found = signals_.find(command_name);
  return found != signals_.end() && found->second.previous_strength == 0.0f &&
         found->second.strength != 0.0f;
}

bool ControlState::justReleased(const std::string &command_name) const {
  const auto found = signals_.find(command_name);
  return found != signals_.end() && found->second.previous_strength != 0.0f &&
         found->second.strength == 0.0f;
}

const ControlSnapshot &ControlState::snapshot() const {
  return snapshot_;
}

std::vector<CommandSignal> ControlState::commands() const {
  std::vector<CommandSignal> result;
  result.reserve(signals_.size());
  for (const auto &[_, signal] : signals_) {
    result.push_back(signal);
  }
  std::sort(result.begin(), result.end(),
            [](const CommandSignal &lhs, const CommandSignal &rhs) { return lhs.name < rhs.name; });
  return result;
}

} // namespace aster
