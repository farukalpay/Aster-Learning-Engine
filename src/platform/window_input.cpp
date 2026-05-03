// Author: Faruk Alpay
// Do not remove this notice.

#include "window_input.hpp"

#include "aster/input/input_codes.hpp"

#include <DESKTOP_WINDOW/desktop_window3.h>

#include <unordered_map>

namespace {

struct ScrollCaptureState {
  aster::Vec2 delta{};
  DESKTOP_WINDOWscrollfun previous_callback = nullptr;
  bool installed = false;
};

std::unordered_map<DESKTOP_WINDOWwindow *, ScrollCaptureState> gScrollCaptureStates;

void scrollCallback(DESKTOP_WINDOWwindow *window, const double x_offset, const double y_offset) {
  ScrollCaptureState &state = gScrollCaptureStates[window];
  state.delta.x += static_cast<float>(x_offset);
  state.delta.y += static_cast<float>(y_offset);
  if (state.previous_callback != nullptr) {
    state.previous_callback(window, x_offset, y_offset);
  }
}

void ensureScrollCapture(DESKTOP_WINDOWwindow *window) {
  ScrollCaptureState &state = gScrollCaptureStates[window];
  if (state.installed) {
    return;
  }
  state.previous_callback = desktop_windowSetScrollCallback(window, scrollCallback);
  state.installed = true;
}

aster::Vec2 drainScrollDelta(DESKTOP_WINDOWwindow *window) {
  ScrollCaptureState &state = gScrollCaptureStates[window];
  const aster::Vec2 delta = state.delta;
  state.delta = {};
  return delta;
}

int desktop_windowKey(const int code) {
  switch (static_cast<aster::Key>(code)) {
  case aster::Key::Escape:
    return DESKTOP_WINDOW_KEY_ESCAPE;
  case aster::Key::R:
    return DESKTOP_WINDOW_KEY_R;
  case aster::Key::A:
    return DESKTOP_WINDOW_KEY_A;
  case aster::Key::Left:
    return DESKTOP_WINDOW_KEY_LEFT;
  case aster::Key::D:
    return DESKTOP_WINDOW_KEY_D;
  case aster::Key::Right:
    return DESKTOP_WINDOW_KEY_RIGHT;
  case aster::Key::W:
    return DESKTOP_WINDOW_KEY_W;
  case aster::Key::Up:
    return DESKTOP_WINDOW_KEY_UP;
  case aster::Key::S:
    return DESKTOP_WINDOW_KEY_S;
  case aster::Key::Down:
    return DESKTOP_WINDOW_KEY_DOWN;
  case aster::Key::Space:
    return DESKTOP_WINDOW_KEY_SPACE;
  case aster::Key::LeftShift:
    return DESKTOP_WINDOW_KEY_LEFT_SHIFT;
  case aster::Key::Tab:
    return DESKTOP_WINDOW_KEY_TAB;
  case aster::Key::LeftSuper:
    return DESKTOP_WINDOW_KEY_LEFT_SUPER;
  case aster::Key::RightSuper:
    return DESKTOP_WINDOW_KEY_RIGHT_SUPER;
  case aster::Key::E:
    return DESKTOP_WINDOW_KEY_E;
  case aster::Key::Q:
    return DESKTOP_WINDOW_KEY_Q;
  case aster::Key::Num1:
    return DESKTOP_WINDOW_KEY_1;
  case aster::Key::Num2:
    return DESKTOP_WINDOW_KEY_2;
  case aster::Key::Num3:
    return DESKTOP_WINDOW_KEY_3;
  case aster::Key::Num4:
    return DESKTOP_WINDOW_KEY_4;
  case aster::Key::Num5:
    return DESKTOP_WINDOW_KEY_5;
  case aster::Key::Num6:
    return DESKTOP_WINDOW_KEY_6;
  }
  return DESKTOP_WINDOW_KEY_UNKNOWN;
}

int desktop_windowMouseButton(const int code) {
  switch (static_cast<aster::MouseButton>(code)) {
  case aster::MouseButton::Left:
    return DESKTOP_WINDOW_MOUSE_BUTTON_LEFT;
  case aster::MouseButton::Right:
    return DESKTOP_WINDOW_MOUSE_BUTTON_RIGHT;
  }
  return -1;
}

} // namespace

namespace aster {

ControlSnapshot captureWindowControls(DESKTOP_WINDOWwindow *window, const ControlScheme &scheme) {
  ControlSnapshot snapshot;
  ensureScrollCapture(window);

  for (const int key : scheme.trackedKeys()) {
    if (desktop_windowGetKey(window, desktop_windowKey(key)) == DESKTOP_WINDOW_PRESS) {
      snapshot.pressed_keys.push_back(key);
    }
  }

  for (const int button : scheme.trackedMouseButtons()) {
    if (desktop_windowGetMouseButton(window, desktop_windowMouseButton(button)) ==
        DESKTOP_WINDOW_PRESS) {
      snapshot.pressed_mouse_buttons.push_back(button);
    }
  }

  double pointer_x = 0.0;
  double pointer_y = 0.0;
  desktop_windowGetCursorPos(window, &pointer_x, &pointer_y);
  snapshot.pointer = {static_cast<float>(pointer_x), static_cast<float>(pointer_y)};
  snapshot.scroll = drainScrollDelta(window);

  return snapshot;
}

} // namespace aster
