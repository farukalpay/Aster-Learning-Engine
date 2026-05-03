// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/platform/window.hpp"

#include "aster/render/software_framebuffer.hpp"
#include "frame_presenter.hpp"
#include "window_input.hpp"

#include <DESKTOP_WINDOW/desktop_window3.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

void desktop_windowErrorCallback(int code, const char *message) {
  const std::string text = message == nullptr ? "unknown DESKTOP_WINDOW error" : message;
  std::fprintf(stderr, "DESKTOP_WINDOW error %d: %s\n", code, text.c_str());
}

} // namespace

namespace aster {

Window::Window(const EngineConfig &config) {
  scale_framebuffer_to_display_ = config.scale_framebuffer_to_display;
  desktop_windowSetErrorCallback(desktop_windowErrorCallback);
  if (desktop_windowInit() != DESKTOP_WINDOW_TRUE) {
    throw std::runtime_error("Failed to initialize DESKTOP_WINDOW.");
  }

  desktop_windowWindowHint(DESKTOP_WINDOW_CLIENT_API, DESKTOP_WINDOW_NO_API);
  desktop_windowWindowHint(DESKTOP_WINDOW_SCALE_FRAMEBUFFER, config.scale_framebuffer_to_display
                                                                 ? DESKTOP_WINDOW_TRUE
                                                                 : DESKTOP_WINDOW_FALSE);

  handle_ = desktop_windowCreateWindow(config.initial_width, config.initial_height,
                                       config.application_name, nullptr, nullptr);
  if (handle_ == nullptr) {
    desktop_windowTerminate();
    throw std::runtime_error("Failed to create the application window.");
  }

  setVsync(config.enable_vsync);
}

Window::~Window() {
  if (handle_ != nullptr) {
    releaseFramePresenter(handle_);
    desktop_windowDestroyWindow(handle_);
    handle_ = nullptr;
  }
  desktop_windowTerminate();
}

Window::Window(Window &&other) noexcept
    : handle_(other.handle_), scale_framebuffer_to_display_(other.scale_framebuffer_to_display_) {
  other.handle_ = nullptr;
  other.scale_framebuffer_to_display_ = false;
}

Window &Window::operator=(Window &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (handle_ != nullptr) {
    releaseFramePresenter(handle_);
    desktop_windowDestroyWindow(handle_);
  }
  handle_ = other.handle_;
  scale_framebuffer_to_display_ = other.scale_framebuffer_to_display_;
  other.handle_ = nullptr;
  other.scale_framebuffer_to_display_ = false;
  return *this;
}

bool Window::isOpen() const {
  return handle_ != nullptr && desktop_windowWindowShouldClose(handle_) == DESKTOP_WINDOW_FALSE;
}

void Window::pollEvents() {
  desktop_windowPollEvents();
}

void Window::swapBuffers() {
  presentFrameBuffer(handle_, activeFrameBuffer());
}

void Window::setVsync(const bool enabled) {
  setFramePresentationVsync(handle_, enabled);
}

void Window::setCursorMode(const CursorMode mode) {
  int desktop_window_mode = DESKTOP_WINDOW_CURSOR_NORMAL;
  switch (mode) {
  case CursorMode::Normal:
    desktop_window_mode = DESKTOP_WINDOW_CURSOR_NORMAL;
    break;
  case CursorMode::Hidden:
    desktop_window_mode = DESKTOP_WINDOW_CURSOR_HIDDEN;
    break;
  case CursorMode::Disabled:
    desktop_window_mode = DESKTOP_WINDOW_CURSOR_DISABLED;
    break;
  }
  desktop_windowSetInputMode(handle_, DESKTOP_WINDOW_CURSOR, desktop_window_mode);
}

void Window::requestClose() {
  desktop_windowSetWindowShouldClose(handle_, DESKTOP_WINDOW_TRUE);
}

std::pair<int, int> Window::windowSize() const {
  int width = 0;
  int height = 0;
  desktop_windowGetWindowSize(handle_, &width, &height);
  return {width, height};
}

std::pair<int, int> Window::framebufferSize() const {
  if (!scale_framebuffer_to_display_) {
    return windowSize();
  }

  int width = 0;
  int height = 0;
  desktop_windowGetFramebufferSize(handle_, &width, &height);
  return {width, height};
}

ControlSnapshot Window::captureControls(const ControlScheme &scheme) const {
  return captureWindowControls(handle_, scheme);
}

DESKTOP_WINDOWwindow *Window::nativeHandle() const {
  return handle_;
}

} // namespace aster
