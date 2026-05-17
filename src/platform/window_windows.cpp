// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/platform/window.hpp"

#ifdef _WIN32

#include "aster/input/input_codes.hpp"
#include "aster/render/software_framebuffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"AsterLearningEngineWindow";
constexpr double kSoftwarePresentTargetSeconds = 1.0 / 60.0;
constexpr double kTimerSettleSeconds = 0.0005;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
constexpr DWORD CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002;
#endif

void addUnique(std::vector<int> &values, const int value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void removeValue(std::vector<int> &values, const int value) {
  values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

std::wstring widenAscii(const char *text) {
  std::wstring out;
  if (text == nullptr) {
    return L"Aster Learning Studio";
  }
  while (*text != '\0') {
    out.push_back(static_cast<unsigned char>(*text++));
  }
  return out.empty() ? L"Aster Learning Studio" : out;
}

int keyFromVirtualKey(const WPARAM key) {
  switch (key) {
  case VK_ESCAPE:
    return aster::code(aster::Key::Escape);
  case 'R':
    return aster::code(aster::Key::R);
  case 'A':
    return aster::code(aster::Key::A);
  case VK_LEFT:
    return aster::code(aster::Key::Left);
  case 'D':
    return aster::code(aster::Key::D);
  case VK_RIGHT:
    return aster::code(aster::Key::Right);
  case 'W':
    return aster::code(aster::Key::W);
  case VK_UP:
    return aster::code(aster::Key::Up);
  case 'S':
    return aster::code(aster::Key::S);
  case VK_DOWN:
    return aster::code(aster::Key::Down);
  case VK_SPACE:
    return aster::code(aster::Key::Space);
  case VK_SHIFT:
  case VK_LSHIFT:
  case VK_RSHIFT:
    return aster::code(aster::Key::LeftShift);
  case VK_TAB:
    return aster::code(aster::Key::Tab);
  case VK_LWIN:
    return aster::code(aster::Key::LeftSuper);
  case VK_RWIN:
    return aster::code(aster::Key::RightSuper);
  case 'E':
    return aster::code(aster::Key::E);
  case 'Q':
    return aster::code(aster::Key::Q);
  case '1':
    return aster::code(aster::Key::Num1);
  case '2':
    return aster::code(aster::Key::Num2);
  case '3':
    return aster::code(aster::Key::Num3);
  case '4':
    return aster::code(aster::Key::Num4);
  case '5':
    return aster::code(aster::Key::Num5);
  case '6':
    return aster::code(aster::Key::Num6);
  default:
    return -1;
  }
}

void updateCursor(HWND hwnd, const aster::CursorMode mode) {
  if (mode == aster::CursorMode::Normal) {
    ClipCursor(nullptr);
    while (ShowCursor(TRUE) < 0) {
    }
    return;
  }

  while (ShowCursor(FALSE) >= 0) {
  }
  if (mode == aster::CursorMode::Disabled && hwnd != nullptr) {
    RECT rect{};
    if (GetClientRect(hwnd, &rect)) {
      POINT top_left{rect.left, rect.top};
      POINT bottom_right{rect.right, rect.bottom};
      ClientToScreen(hwnd, &top_left);
      ClientToScreen(hwnd, &bottom_right);
      RECT screen_rect{top_left.x, top_left.y, bottom_right.x, bottom_right.y};
      ClipCursor(&screen_rect);
    }
  } else {
    ClipCursor(nullptr);
  }
}

void registerWindowClass() {
  static bool registered = false;
  if (registered) {
    return;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = kWindowClassName;
  if (RegisterClassExW(&wc) == 0u) {
    throw std::runtime_error("Could not register Win32 window class.");
  }
  registered = true;
}

void enableProcessDpiAwareness() {
  const HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32 == nullptr) {
    return;
  }
  using SetProcessDpiAwarenessContextFn = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
  const auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
      GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
  if (set_awareness != nullptr) {
    (void)set_awareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }
}

} // namespace

namespace aster {

struct WindowImpl {
  HWND hwnd = nullptr;
  int width = 1;
  int height = 1;
  bool open = false;
  bool vsync = true;
  CursorMode cursor = CursorMode::Normal;
  ControlSnapshot input;
  LARGE_INTEGER qpc_frequency{};
  LARGE_INTEGER qpc_last{};
  HANDLE frame_timer = nullptr;
};

namespace {

WindowImpl *implFromWindow(HWND hwnd) {
  return reinterpret_cast<WindowImpl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void updateClientSize(WindowImpl &impl) {
  RECT rect{};
  if (impl.hwnd != nullptr && GetClientRect(impl.hwnd, &rect)) {
    impl.width = std::max(rect.right - rect.left, 1L);
    impl.height = std::max(rect.bottom - rect.top, 1L);
  }
}

void paceSoftwarePresent(WindowImpl &impl) {
  if (!impl.vsync || impl.qpc_frequency.QuadPart <= 0) {
    QueryPerformanceCounter(&impl.qpc_last);
    return;
  }
  LARGE_INTEGER now{};
  QueryPerformanceCounter(&now);
  const double elapsed =
      static_cast<double>(now.QuadPart - impl.qpc_last.QuadPart) /
      static_cast<double>(impl.qpc_frequency.QuadPart);
  if (elapsed < kSoftwarePresentTargetSeconds) {
    const double remaining = kSoftwarePresentTargetSeconds - elapsed;
    if (remaining > kTimerSettleSeconds) {
      const double timer_seconds = remaining - kTimerSettleSeconds;
      bool timer_used = false;
      if (impl.frame_timer != nullptr) {
        LARGE_INTEGER due_time{};
        due_time.QuadPart =
            -static_cast<LONGLONG>(std::ceil(timer_seconds * 10000000.0));
        if (SetWaitableTimer(impl.frame_timer, &due_time, 0, nullptr, nullptr, FALSE)) {
          WaitForSingleObject(impl.frame_timer, INFINITE);
          timer_used = true;
        }
      }
      if (!timer_used) {
        std::this_thread::sleep_for(std::chrono::duration<double>(timer_seconds));
      }
    }
    do {
      QueryPerformanceCounter(&now);
    } while (static_cast<double>(now.QuadPart - impl.qpc_last.QuadPart) /
                 static_cast<double>(impl.qpc_frequency.QuadPart) <
             kSoftwarePresentTargetSeconds);
  }
  impl.qpc_last = now;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  WindowImpl *impl = implFromWindow(hwnd);
  switch (message) {
  case WM_CLOSE:
    if (impl != nullptr) {
      impl->open = false;
    }
    return 0;
  case WM_DESTROY:
    if (impl != nullptr) {
      impl->open = false;
    }
    return 0;
  case WM_SIZE:
    if (impl != nullptr) {
      updateClientSize(*impl);
    }
    return 0;
  case WM_DPICHANGED:
    if (impl != nullptr) {
      const RECT *suggested = reinterpret_cast<const RECT *>(lparam);
      if (suggested != nullptr) {
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      updateClientSize(*impl);
    }
    return 0;
  case WM_MOUSEMOVE:
    if (impl != nullptr && impl->cursor != CursorMode::Disabled) {
      impl->input.pointer = {static_cast<float>(GET_X_LPARAM(lparam)),
                             static_cast<float>(GET_Y_LPARAM(lparam))};
    }
    return 0;
  case WM_LBUTTONDOWN:
    if (impl != nullptr) {
      addUnique(impl->input.pressed_mouse_buttons, code(MouseButton::Left));
      SetCapture(hwnd);
    }
    return 0;
  case WM_LBUTTONUP:
    if (impl != nullptr) {
      removeValue(impl->input.pressed_mouse_buttons, code(MouseButton::Left));
      ReleaseCapture();
    }
    return 0;
  case WM_RBUTTONDOWN:
    if (impl != nullptr) {
      addUnique(impl->input.pressed_mouse_buttons, code(MouseButton::Right));
      SetCapture(hwnd);
    }
    return 0;
  case WM_RBUTTONUP:
    if (impl != nullptr) {
      removeValue(impl->input.pressed_mouse_buttons, code(MouseButton::Right));
      ReleaseCapture();
    }
    return 0;
  case WM_MOUSEWHEEL:
    if (impl != nullptr) {
      impl->input.scroll.y +=
          static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
    }
    return 0;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (impl != nullptr) {
      const int key = keyFromVirtualKey(wparam);
      if (key >= 0) {
        addUnique(impl->input.pressed_keys, key);
      }
    }
    return 0;
  case WM_KEYUP:
  case WM_SYSKEYUP:
    if (impl != nullptr) {
      const int key = keyFromVirtualKey(wparam);
      if (key >= 0) {
        removeValue(impl->input.pressed_keys, key);
      }
    }
    return 0;
  case WM_INPUT:
    if (impl != nullptr && impl->cursor == CursorMode::Disabled) {
      UINT size = 0u;
      if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size,
                          sizeof(RAWINPUTHEADER)) == 0u &&
          size > 0u) {
        std::vector<std::uint8_t> buffer(size);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buffer.data(), &size,
                            sizeof(RAWINPUTHEADER)) == size) {
          const RAWINPUT *raw = reinterpret_cast<const RAWINPUT *>(buffer.data());
          if (raw->header.dwType == RIM_TYPEMOUSE) {
            impl->input.pointer.x += static_cast<float>(raw->data.mouse.lLastX);
            impl->input.pointer.y += static_cast<float>(raw->data.mouse.lLastY);
          }
        }
      }
    }
    return 0;
  default:
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}

} // namespace

Window::Window(const EngineConfig &config)
    : impl_(std::make_unique<WindowImpl>()),
      scale_framebuffer_to_display_(config.scale_framebuffer_to_display) {
  enableProcessDpiAwareness();
  registerWindowClass();
  QueryPerformanceFrequency(&impl_->qpc_frequency);
  QueryPerformanceCounter(&impl_->qpc_last);
  impl_->frame_timer =
      CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                             SYNCHRONIZE | TIMER_MODIFY_STATE);
  if (impl_->frame_timer == nullptr) {
    impl_->frame_timer =
        CreateWaitableTimerExW(nullptr, nullptr, 0, SYNCHRONIZE | TIMER_MODIFY_STATE);
  }

  impl_->width = std::max(config.initial_width, 1);
  impl_->height = std::max(config.initial_height, 1);
  impl_->vsync = config.enable_vsync;

  RECT rect{0, 0, impl_->width, impl_->height};
  const DWORD style = WS_OVERLAPPEDWINDOW;
  AdjustWindowRectEx(&rect, style, FALSE, 0);
  const std::wstring title = widenAscii(config.application_name);
  impl_->hwnd =
      CreateWindowExW(0, kWindowClassName, title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                      rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                      GetModuleHandleW(nullptr), nullptr);
  if (impl_->hwnd == nullptr) {
    throw std::runtime_error("Could not create Win32 window.");
  }
  SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));
  SetWindowLongPtrW(impl_->hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProc));

  RAWINPUTDEVICE mouse{};
  mouse.usUsagePage = 0x01u;
  mouse.usUsage = 0x02u;
  mouse.dwFlags = 0u;
  mouse.hwndTarget = impl_->hwnd;
  RegisterRawInputDevices(&mouse, 1u, sizeof(mouse));

  ShowWindow(impl_->hwnd, SW_SHOW);
  UpdateWindow(impl_->hwnd);
  updateClientSize(*impl_);
  impl_->open = true;
}

Window::~Window() {
  if (impl_ != nullptr && impl_->hwnd != nullptr) {
    DestroyWindow(impl_->hwnd);
    impl_->hwnd = nullptr;
  }
  if (impl_ != nullptr && impl_->frame_timer != nullptr) {
    CloseHandle(impl_->frame_timer);
    impl_->frame_timer = nullptr;
  }
  ClipCursor(nullptr);
}

Window::Window(Window &&other) noexcept = default;
Window &Window::operator=(Window &&other) noexcept = default;

bool Window::isOpen() const {
  return impl_ != nullptr && impl_->open;
}

void Window::pollEvents() {
  if (impl_ == nullptr) {
    return;
  }
  impl_->input.scroll = {};
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE)) {
    if (message.message == WM_QUIT) {
      impl_->open = false;
      continue;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  updateCursor(impl_->hwnd, impl_->cursor);
}

void Window::swapBuffers() {
  if (impl_ == nullptr || impl_->hwnd == nullptr) {
    return;
  }
  const SoftwareFrameBuffer &framebuffer = activeFrameBuffer();
  if (framebuffer.empty()) {
    return;
  }

  HDC dc = GetDC(impl_->hwnd);
  if (dc == nullptr) {
    return;
  }
  BITMAPV4HEADER info{};
  info.bV4Size = sizeof(BITMAPV4HEADER);
  info.bV4Width = framebuffer.width();
  info.bV4Height = -framebuffer.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000ffu;
  info.bV4GreenMask = 0x0000ff00u;
  info.bV4BlueMask = 0x00ff0000u;
  info.bV4AlphaMask = 0xff000000u;
  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  SetStretchBltMode(dc, COLORONCOLOR);
  StretchDIBits(dc, 0, 0, impl_->width, impl_->height, 0, 0, framebuffer.width(),
                framebuffer.height(), rgba.data(), reinterpret_cast<BITMAPINFO *>(&info),
                DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(impl_->hwnd, dc);
  paceSoftwarePresent(*impl_);
}

void Window::setVsync(const bool enabled) {
  if (impl_ != nullptr) {
    impl_->vsync = enabled;
  }
}

void Window::setCursorMode(const CursorMode mode) {
  if (impl_ != nullptr) {
    impl_->cursor = mode;
    updateCursor(impl_->hwnd, mode);
  }
}

void Window::requestClose() {
  if (impl_ != nullptr) {
    impl_->open = false;
  }
}

std::pair<int, int> Window::windowSize() const {
  if (impl_ == nullptr) {
    return {1, 1};
  }
  return {std::max(impl_->width, 1), std::max(impl_->height, 1)};
}

std::pair<int, int> Window::framebufferSize() const {
  return windowSize();
}

ControlSnapshot Window::captureControls(const ControlScheme &scheme) const {
  (void)scheme;
  return impl_ == nullptr ? ControlSnapshot{} : impl_->input;
}

} // namespace aster

#endif
