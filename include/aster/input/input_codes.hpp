// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/input/control_scheme.hpp"

namespace aster {

enum class Key {
  Escape,
  R,
  A,
  Left,
  D,
  Right,
  W,
  Up,
  S,
  Down,
  Space,
  LeftShift,
  Tab,
  LeftSuper,
  RightSuper,
  E,
  Q,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
};

enum class MouseButton {
  Left,
  Right,
};

[[nodiscard]] constexpr int code(const Key key) {
  return static_cast<int>(key);
}

[[nodiscard]] constexpr int code(const MouseButton button) {
  return static_cast<int>(button);
}

[[nodiscard]] constexpr ControlBinding keyBinding(const Key key, const float scale = 1.0f) {
  return {ControlDevice::Keyboard, code(key), scale};
}

[[nodiscard]] constexpr ControlBinding mouseBinding(const MouseButton button,
                                                    const float scale = 1.0f) {
  return {ControlDevice::MouseButton, code(button), scale};
}

} // namespace aster
