// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/core/config.hpp"
#include "aster/input/control_scheme.hpp"

#include <memory>
#include <utility>

namespace aster {

enum class CursorMode {
  Normal,
  Hidden,
  Disabled,
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
  [[nodiscard]] ControlSnapshot captureControls(const ControlScheme &scheme) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool scale_framebuffer_to_display_ = false;
};

} // namespace aster
