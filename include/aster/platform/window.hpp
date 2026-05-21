// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/config.hpp"
#include "aster/input/control_scheme.hpp"

#include <memory>
#include <utility>

namespace aster {

struct WindowImpl;

enum class CursorMode {
  Normal,
  Hidden,
  Disabled,
};

enum class NativeWindowSurfaceKind {
  None,
  Win32Hwnd,
  CocoaView,
  X11Window,
  WaylandSurface,
};

struct NativeWindowSurface {
  NativeWindowSurfaceKind kind = NativeWindowSurfaceKind::None;
  void *handle = nullptr;
  void *display = nullptr;
  int width = 1;
  int height = 1;
  bool vsync = true;
  bool valid = false;
};

class Window {
public:
  explicit Window(const EngineConfig &config);
  ~Window();

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  Window(Window &&other) noexcept;
  Window &operator=(Window &&other) noexcept;

  [[nodiscard]] bool isOpen() const;
  void pollEvents();
  void swapBuffers();
  void setVsync(bool enabled);
  void setCursorMode(CursorMode mode);
  void requestClose();

  [[nodiscard]] std::pair<int, int> windowSize() const;
  [[nodiscard]] std::pair<int, int> framebufferSize() const;
  [[nodiscard]] NativeWindowSurface nativeSurface() const;
  [[nodiscard]] ControlSnapshot captureControls(const ControlScheme &scheme) const;

private:
  std::unique_ptr<WindowImpl> impl_;
  bool scale_framebuffer_to_display_ = false;
};

} // namespace aster
