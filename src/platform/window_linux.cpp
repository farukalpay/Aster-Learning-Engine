// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/platform/window.hpp"

#include "aster/input/input_codes.hpp"
#include "aster/render/software_framebuffer.hpp"

#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kXEventKeyPress = 2;
constexpr std::uint32_t kXEventKeyRelease = 3;
constexpr std::uint32_t kXEventButtonPress = 4;
constexpr std::uint32_t kXEventButtonRelease = 5;
constexpr std::uint32_t kXEventMotionNotify = 6;
constexpr std::uint32_t kXEventConfigureNotify = 22;
constexpr std::uint32_t kXEventClientMessage = 33;

constexpr std::uint32_t kXEventMaskKeyPress = 1u << 0u;
constexpr std::uint32_t kXEventMaskKeyRelease = 1u << 1u;
constexpr std::uint32_t kXEventMaskButtonPress = 1u << 2u;
constexpr std::uint32_t kXEventMaskButtonRelease = 1u << 3u;
constexpr std::uint32_t kXEventMaskPointerMotion = 1u << 6u;
constexpr std::uint32_t kXEventMaskExposure = 1u << 15u;
constexpr std::uint32_t kXEventMaskStructureNotify = 1u << 17u;

constexpr std::uint32_t kXAtomAtom = 4;
constexpr std::uint32_t kXAtomString = 31;
constexpr std::uint32_t kXAtomWmName = 39;

constexpr std::uint32_t kXVisualTrueColor = 4;

struct DisplayName {
  std::string socket_path;
  std::string display_number = "0";
};

struct XAuthority {
  std::string name;
  std::vector<std::uint8_t> data;
};

std::uint16_t readBe16(const std::vector<std::uint8_t> &bytes, const std::size_t offset) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8u) |
                                    static_cast<std::uint16_t>(bytes[offset + 1u]));
}

std::uint16_t readLe16(const std::vector<std::uint8_t> &bytes, const std::size_t offset) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) |
                                    (static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u));
}

std::uint16_t readLe16(const std::array<std::uint8_t, 32> &bytes, const std::size_t offset) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) |
                                    (static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u));
}

std::int16_t readLeI16(const std::array<std::uint8_t, 32> &bytes, const std::size_t offset) {
  return static_cast<std::int16_t>(readLe16(bytes, offset));
}

std::uint32_t readLe32(const std::vector<std::uint8_t> &bytes, const std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

std::uint32_t readLe32(const std::array<std::uint8_t, 32> &bytes, const std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

void write8(std::vector<std::uint8_t> &out, const std::uint8_t value) {
  out.push_back(value);
}

void write16(std::vector<std::uint8_t> &out, const std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void write32(std::vector<std::uint8_t> &out, const std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void pad4(std::vector<std::uint8_t> &out) {
  while ((out.size() % 4u) != 0u) {
    out.push_back(0);
  }
}

std::size_t padded4(const std::size_t size) {
  return (size + 3u) & ~std::size_t{3u};
}

bool sendFull(const int fd, const std::vector<std::uint8_t> &bytes) {
  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool readFull(const int fd, std::uint8_t *bytes, const std::size_t count) {
  std::size_t read = 0;
  while (read < count) {
    const ssize_t n = ::recv(fd, bytes + read, count - read, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    read += static_cast<std::size_t>(n);
  }
  return true;
}

std::optional<DisplayName> parseDisplayName() {
  const char *display_env = std::getenv("DISPLAY");
  if (display_env == nullptr || *display_env == '\0') {
    return std::nullopt;
  }

  std::string display = display_env;
  const std::size_t colon = display.rfind(':');
  if (colon == std::string::npos) {
    return std::nullopt;
  }

  const std::string host = display.substr(0u, colon);
  std::string number = display.substr(colon + 1u);
  if (const std::size_t dot = number.find('.'); dot != std::string::npos) {
    number = number.substr(0u, dot);
  }
  if (number.empty()) {
    return std::nullopt;
  }

  if (!host.empty() && host != "localhost" && host != "unix") {
    return std::nullopt;
  }

  return DisplayName{"/tmp/.X11-unix/X" + number, number};
}

std::optional<std::filesystem::path> xAuthorityPath() {
  if (const char *path = std::getenv("XAUTHORITY"); path != nullptr && *path != '\0') {
    return std::filesystem::path(path);
  }
  if (const char *home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".Xauthority";
  }
  return std::nullopt;
}

std::optional<XAuthority> readXAuthority(const std::string_view display_number) {
  const std::optional<std::filesystem::path> path = xAuthorityPath();
  if (!path.has_value()) {
    return std::nullopt;
  }
  std::ifstream stream(*path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                        std::istreambuf_iterator<char>());

  std::optional<XAuthority> fallback;
  std::size_t offset = 0;
  while (offset + 2u <= bytes.size()) {
    const std::uint16_t family = readBe16(bytes, offset);
    offset += 2u;
    if (offset + 2u > bytes.size()) {
      break;
    }
    const std::uint16_t address_len = readBe16(bytes, offset);
    offset += 2u + address_len;
    if (offset + 2u > bytes.size()) {
      break;
    }
    const std::uint16_t number_len = readBe16(bytes, offset);
    offset += 2u;
    if (offset + number_len + 2u > bytes.size()) {
      break;
    }
    const std::string number(reinterpret_cast<const char *>(bytes.data() + offset), number_len);
    offset += number_len;

    const std::uint16_t name_len = readBe16(bytes, offset);
    offset += 2u;
    if (offset + name_len + 2u > bytes.size()) {
      break;
    }
    const std::string name(reinterpret_cast<const char *>(bytes.data() + offset), name_len);
    offset += name_len;

    const std::uint16_t data_len = readBe16(bytes, offset);
    offset += 2u;
    if (offset + data_len > bytes.size()) {
      break;
    }
    std::vector<std::uint8_t> data(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                   bytes.begin() + static_cast<std::ptrdiff_t>(offset + data_len));
    offset += data_len;

    if (name != "MIT-MAGIC-COOKIE-1") {
      continue;
    }
    XAuthority entry{std::move(name), std::move(data)};
    if (number == display_number) {
      return entry;
    }
    if (family == 256u && !fallback.has_value()) {
      fallback = std::move(entry);
    }
  }
  return fallback;
}

int keyFromX11Keycode(const std::uint8_t keycode) {
  switch (keycode) {
  case 9:
    return aster::code(aster::Key::Escape);
  case 10:
    return aster::code(aster::Key::Num1);
  case 11:
    return aster::code(aster::Key::Num2);
  case 12:
    return aster::code(aster::Key::Num3);
  case 13:
    return aster::code(aster::Key::Num4);
  case 14:
    return aster::code(aster::Key::Num5);
  case 15:
    return aster::code(aster::Key::Num6);
  case 23:
    return aster::code(aster::Key::Tab);
  case 24:
    return aster::code(aster::Key::Q);
  case 25:
    return aster::code(aster::Key::W);
  case 26:
    return aster::code(aster::Key::E);
  case 27:
    return aster::code(aster::Key::R);
  case 38:
    return aster::code(aster::Key::A);
  case 39:
    return aster::code(aster::Key::S);
  case 40:
    return aster::code(aster::Key::D);
  case 50:
  case 62:
    return aster::code(aster::Key::LeftShift);
  case 65:
    return aster::code(aster::Key::Space);
  case 111:
    return aster::code(aster::Key::Up);
  case 113:
    return aster::code(aster::Key::Left);
  case 114:
    return aster::code(aster::Key::Right);
  case 116:
    return aster::code(aster::Key::Down);
  default:
    return -1;
  }
}

int mouseButtonFromX11Detail(const std::uint8_t detail) {
  switch (detail) {
  case 1:
    return aster::code(aster::MouseButton::Left);
  case 3:
    return aster::code(aster::MouseButton::Right);
  default:
    return -1;
  }
}

int keyFromWaylandKeycode(const std::uint32_t keycode) {
  switch (keycode) {
  case 1:
    return aster::code(aster::Key::Escape);
  case 2:
    return aster::code(aster::Key::Num1);
  case 3:
    return aster::code(aster::Key::Num2);
  case 4:
    return aster::code(aster::Key::Num3);
  case 5:
    return aster::code(aster::Key::Num4);
  case 6:
    return aster::code(aster::Key::Num5);
  case 7:
    return aster::code(aster::Key::Num6);
  case 15:
    return aster::code(aster::Key::Tab);
  case 16:
    return aster::code(aster::Key::Q);
  case 17:
    return aster::code(aster::Key::W);
  case 18:
    return aster::code(aster::Key::E);
  case 19:
    return aster::code(aster::Key::R);
  case 30:
    return aster::code(aster::Key::A);
  case 31:
    return aster::code(aster::Key::S);
  case 32:
    return aster::code(aster::Key::D);
  case 42:
  case 54:
    return aster::code(aster::Key::LeftShift);
  case 57:
    return aster::code(aster::Key::Space);
  case 103:
    return aster::code(aster::Key::Up);
  case 105:
    return aster::code(aster::Key::Left);
  case 106:
    return aster::code(aster::Key::Right);
  case 108:
    return aster::code(aster::Key::Down);
  case 125:
    return aster::code(aster::Key::LeftSuper);
  case 126:
    return aster::code(aster::Key::RightSuper);
  default:
    return -1;
  }
}

int mouseButtonFromWaylandButton(const std::uint32_t button) {
  switch (button) {
  case 0x110:
    return aster::code(aster::MouseButton::Left);
  case 0x111:
    return aster::code(aster::MouseButton::Right);
  default:
    return -1;
  }
}

void addUnique(std::vector<int> &values, const int value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void removeValue(std::vector<int> &values, const int value) {
  values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

int maskShift(const std::uint32_t mask) {
  if (mask == 0u) {
    return 0;
  }
  int shift = 0;
  while (((mask >> static_cast<unsigned>(shift)) & 1u) == 0u && shift < 31) {
    ++shift;
  }
  return shift;
}

int maskBits(std::uint32_t mask) {
  int bits = 0;
  while (mask != 0u) {
    bits += static_cast<int>(mask & 1u);
    mask >>= 1u;
  }
  return bits;
}

std::uint32_t colorToMask(const std::uint8_t value, const std::uint32_t mask) {
  if (mask == 0u) {
    return 0u;
  }
  const int bits = std::clamp(maskBits(mask), 1, 8);
  const std::uint32_t max_value = (1u << static_cast<unsigned>(bits)) - 1u;
  const std::uint32_t scaled =
      (static_cast<std::uint32_t>(value) * max_value + 127u) / 255u;
  return (scaled << static_cast<unsigned>(maskShift(mask))) & mask;
}

int createAnonymousFile(const std::size_t size) {
  const char *runtime_dir = std::getenv("XDG_RUNTIME_DIR");
  if (runtime_dir == nullptr || *runtime_dir == '\0') {
    return -1;
  }

  std::string path = std::string(runtime_dir) + "/aster-wayland-buffer-XXXXXX";
  std::vector<char> writable_path(path.begin(), path.end());
  writable_path.push_back('\0');

  const int fd = ::mkstemp(writable_path.data());
  if (fd < 0) {
    return -1;
  }
  (void)::unlink(writable_path.data());
  if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

struct WaylandConnection;

struct WaylandBuffer {
  wl_buffer *buffer = nullptr;
  void *memory = nullptr;
  std::size_t size = 0;
  int width = 0;
  int height = 0;
  int stride = 0;
  bool busy = false;

  WaylandBuffer() = default;
  WaylandBuffer(const WaylandBuffer &) = delete;
  WaylandBuffer &operator=(const WaylandBuffer &) = delete;

  WaylandBuffer(WaylandBuffer &&other) noexcept {
    *this = std::move(other);
  }

  WaylandBuffer &operator=(WaylandBuffer &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    release();
    buffer = std::exchange(other.buffer, nullptr);
    memory = std::exchange(other.memory, nullptr);
    size = std::exchange(other.size, 0u);
    width = std::exchange(other.width, 0);
    height = std::exchange(other.height, 0);
    stride = std::exchange(other.stride, 0);
    busy = std::exchange(other.busy, false);
    return *this;
  }

  ~WaylandBuffer() {
    release();
  }

  void release() {
    if (buffer != nullptr) {
      wl_buffer_destroy(buffer);
      buffer = nullptr;
    }
    if (memory != nullptr && size > 0u) {
      (void)::munmap(memory, size);
      memory = nullptr;
    }
    size = 0u;
    width = 0;
    height = 0;
    stride = 0;
    busy = false;
  }
};

void handleWaylandBufferRelease(void *data, wl_buffer *) {
  auto *buffer = static_cast<WaylandBuffer *>(data);
  if (buffer != nullptr) {
    buffer->busy = false;
  }
}

constexpr wl_buffer_listener kWaylandBufferListener{handleWaylandBufferRelease};

struct WaylandConnection {
  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_compositor *compositor = nullptr;
  wl_shm *shm = nullptr;
  wl_seat *seat = nullptr;
  wl_pointer *pointer = nullptr;
  wl_keyboard *keyboard = nullptr;
  wl_surface *surface = nullptr;
  wl_surface *cursor_surface = nullptr;
  xdg_wm_base *wm_base = nullptr;
  xdg_surface *xdg_surface = nullptr;
  xdg_toplevel *xdg_toplevel = nullptr;
  zwp_relative_pointer_manager_v1 *relative_pointer_manager = nullptr;
  zwp_relative_pointer_v1 *relative_pointer = nullptr;
  zwp_pointer_constraints_v1 *pointer_constraints = nullptr;
  zwp_locked_pointer_v1 *locked_pointer = nullptr;
  std::array<WaylandBuffer, 2> frame_buffers;
  WaylandBuffer cursor_buffer;
  std::size_t next_frame_buffer = 0;
  int width = 1;
  int height = 1;
  int configured_width = 0;
  int configured_height = 0;
  bool configured = false;
  bool open = true;
  bool supports_xrgb8888 = false;
  bool supports_argb8888 = false;
  bool has_pointer_focus = false;
  bool has_last_pointer = false;
  bool pointer_locked = false;
  bool locked_pointer_listener_installed = false;
  std::uint32_t pointer_enter_serial = 0;
  aster::Vec2 last_pointer{};
  aster::Vec2 virtual_pointer{};
  aster::ControlSnapshot input;
  aster::CursorMode cursor_mode = aster::CursorMode::Normal;

  WaylandConnection() = default;
  WaylandConnection(const WaylandConnection &) = delete;
  WaylandConnection &operator=(const WaylandConnection &) = delete;

  WaylandConnection(WaylandConnection &&other) noexcept {
    *this = std::move(other);
  }

  WaylandConnection &operator=(WaylandConnection &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    release();
    display = std::exchange(other.display, nullptr);
    registry = std::exchange(other.registry, nullptr);
    compositor = std::exchange(other.compositor, nullptr);
    shm = std::exchange(other.shm, nullptr);
    seat = std::exchange(other.seat, nullptr);
    pointer = std::exchange(other.pointer, nullptr);
    keyboard = std::exchange(other.keyboard, nullptr);
    surface = std::exchange(other.surface, nullptr);
    cursor_surface = std::exchange(other.cursor_surface, nullptr);
    wm_base = std::exchange(other.wm_base, nullptr);
    xdg_surface = std::exchange(other.xdg_surface, nullptr);
    xdg_toplevel = std::exchange(other.xdg_toplevel, nullptr);
    relative_pointer_manager = std::exchange(other.relative_pointer_manager, nullptr);
    relative_pointer = std::exchange(other.relative_pointer, nullptr);
    pointer_constraints = std::exchange(other.pointer_constraints, nullptr);
    locked_pointer = std::exchange(other.locked_pointer, nullptr);
    frame_buffers = std::move(other.frame_buffers);
    cursor_buffer = std::move(other.cursor_buffer);
    next_frame_buffer = other.next_frame_buffer;
    width = other.width;
    height = other.height;
    configured_width = other.configured_width;
    configured_height = other.configured_height;
    configured = other.configured;
    open = other.open;
    supports_xrgb8888 = other.supports_xrgb8888;
    supports_argb8888 = other.supports_argb8888;
    has_pointer_focus = other.has_pointer_focus;
    has_last_pointer = other.has_last_pointer;
    pointer_locked = other.pointer_locked;
    locked_pointer_listener_installed = other.locked_pointer_listener_installed;
    pointer_enter_serial = other.pointer_enter_serial;
    last_pointer = other.last_pointer;
    virtual_pointer = other.virtual_pointer;
    input = std::move(other.input);
    cursor_mode = other.cursor_mode;
    return *this;
  }

  ~WaylandConnection() {
    release();
  }

  void release() {
    if (locked_pointer != nullptr) {
      zwp_locked_pointer_v1_destroy(locked_pointer);
      locked_pointer = nullptr;
    }
    if (relative_pointer != nullptr) {
      zwp_relative_pointer_v1_destroy(relative_pointer);
      relative_pointer = nullptr;
    }
    for (WaylandBuffer &buffer : frame_buffers) {
      buffer.release();
    }
    cursor_buffer.release();
    if (xdg_toplevel != nullptr) {
      xdg_toplevel_destroy(xdg_toplevel);
      xdg_toplevel = nullptr;
    }
    if (xdg_surface != nullptr) {
      xdg_surface_destroy(xdg_surface);
      xdg_surface = nullptr;
    }
    if (surface != nullptr) {
      wl_surface_destroy(surface);
      surface = nullptr;
    }
    if (cursor_surface != nullptr) {
      wl_surface_destroy(cursor_surface);
      cursor_surface = nullptr;
    }
    if (pointer != nullptr) {
      wl_pointer_destroy(pointer);
      pointer = nullptr;
    }
    if (keyboard != nullptr) {
      wl_keyboard_destroy(keyboard);
      keyboard = nullptr;
    }
    if (seat != nullptr) {
      wl_seat_destroy(seat);
      seat = nullptr;
    }
    if (pointer_constraints != nullptr) {
      zwp_pointer_constraints_v1_destroy(pointer_constraints);
      pointer_constraints = nullptr;
    }
    if (relative_pointer_manager != nullptr) {
      zwp_relative_pointer_manager_v1_destroy(relative_pointer_manager);
      relative_pointer_manager = nullptr;
    }
    if (wm_base != nullptr) {
      xdg_wm_base_destroy(wm_base);
      wm_base = nullptr;
    }
    if (shm != nullptr) {
      wl_shm_destroy(shm);
      shm = nullptr;
    }
    if (compositor != nullptr) {
      wl_compositor_destroy(compositor);
      compositor = nullptr;
    }
    if (registry != nullptr) {
      wl_registry_destroy(registry);
      registry = nullptr;
    }
    if (display != nullptr) {
      wl_display_disconnect(display);
      display = nullptr;
    }
  }
};

bool createWaylandBuffer(WaylandConnection &connection, WaylandBuffer &target, const int width,
                         const int height, const std::uint32_t format) {
  if (connection.shm == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  target.release();
  target.width = width;
  target.height = height;
  target.stride = width * 4;
  target.size = static_cast<std::size_t>(target.stride) * static_cast<std::size_t>(height);

  const int fd = createAnonymousFile(target.size);
  if (fd < 0) {
    target.release();
    return false;
  }
  target.memory = ::mmap(nullptr, target.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (target.memory == MAP_FAILED) {
    target.memory = nullptr;
    ::close(fd);
    target.release();
    return false;
  }

  wl_shm_pool *pool = wl_shm_create_pool(connection.shm, fd, static_cast<int>(target.size));
  if (pool == nullptr) {
    ::close(fd);
    target.release();
    return false;
  }
  target.buffer = wl_shm_pool_create_buffer(pool, 0, width, height, target.stride, format);
  wl_shm_pool_destroy(pool);
  ::close(fd);
  if (target.buffer == nullptr) {
    target.release();
    return false;
  }
  wl_buffer_add_listener(target.buffer, &kWaylandBufferListener, &target);
  return true;
}

WaylandBuffer *nextWaylandFrameBuffer(WaylandConnection &connection, const int width,
                                      const int height) {
  for (std::size_t attempt = 0; attempt < connection.frame_buffers.size(); ++attempt) {
    const std::size_t index =
        (connection.next_frame_buffer + attempt) % connection.frame_buffers.size();
    WaylandBuffer &buffer = connection.frame_buffers[index];
    if (buffer.busy) {
      continue;
    }
    if (buffer.buffer == nullptr || buffer.width != width || buffer.height != height) {
      if (!createWaylandBuffer(connection, buffer, width, height, WL_SHM_FORMAT_XRGB8888)) {
        continue;
      }
    }
    if (buffer.memory != nullptr) {
      connection.next_frame_buffer = (index + 1u) % connection.frame_buffers.size();
      return &buffer;
    }
  }
  return nullptr;
}

void fillWaylandCursorBuffer(WaylandBuffer &buffer) {
  auto *pixels = static_cast<std::uint32_t *>(buffer.memory);
  if (pixels == nullptr) {
    return;
  }
  std::fill(pixels, pixels + static_cast<std::size_t>(buffer.width * buffer.height), 0x00000000u);
  for (int y = 0; y < buffer.height; ++y) {
    for (int x = 0; x <= y / 2 && x < buffer.width; ++x) {
      const bool edge = x == 0 || x == y / 2 || y == 0;
      pixels[static_cast<std::size_t>(y * buffer.width + x)] = edge ? 0xff101010u : 0xffffffffu;
    }
  }
}

void applyWaylandCursor(WaylandConnection &connection) {
  if (connection.pointer == nullptr || !connection.has_pointer_focus) {
    return;
  }
  if (connection.cursor_mode != aster::CursorMode::Normal) {
    wl_pointer_set_cursor(connection.pointer, connection.pointer_enter_serial, nullptr, 0, 0);
    return;
  }
  if (connection.cursor_surface == nullptr) {
    connection.cursor_surface = wl_compositor_create_surface(connection.compositor);
  }
  if (connection.cursor_surface == nullptr) {
    return;
  }
  if (connection.cursor_buffer.buffer == nullptr && connection.supports_argb8888 &&
      createWaylandBuffer(connection, connection.cursor_buffer, 24, 24, WL_SHM_FORMAT_ARGB8888)) {
    fillWaylandCursorBuffer(connection.cursor_buffer);
  }
  if (connection.cursor_buffer.buffer == nullptr) {
    wl_pointer_set_cursor(connection.pointer, connection.pointer_enter_serial, nullptr, 0, 0);
    return;
  }
  wl_surface_attach(connection.cursor_surface, connection.cursor_buffer.buffer, 0, 0);
  wl_surface_damage(connection.cursor_surface, 0, 0, connection.cursor_buffer.width,
                    connection.cursor_buffer.height);
  wl_surface_commit(connection.cursor_surface);
  wl_pointer_set_cursor(connection.pointer, connection.pointer_enter_serial,
                        connection.cursor_surface, 0, 0);
}

void destroyWaylandPointerLock(WaylandConnection &connection) {
  if (connection.locked_pointer != nullptr) {
    zwp_locked_pointer_v1_destroy(connection.locked_pointer);
    connection.locked_pointer = nullptr;
  }
  connection.pointer_locked = false;
  connection.locked_pointer_listener_installed = false;
}

void handleWaylandLockedPointerLocked(void *data, zwp_locked_pointer_v1 *);
void handleWaylandLockedPointerUnlocked(void *data, zwp_locked_pointer_v1 *);

constexpr zwp_locked_pointer_v1_listener kWaylandLockedPointerListener{
    handleWaylandLockedPointerLocked, handleWaylandLockedPointerUnlocked};

void updateWaylandPointerLock(WaylandConnection &connection) {
  if (connection.cursor_mode != aster::CursorMode::Disabled || connection.pointer == nullptr ||
      connection.pointer_constraints == nullptr || connection.surface == nullptr ||
      !connection.has_pointer_focus) {
    if (connection.cursor_mode != aster::CursorMode::Disabled) {
      destroyWaylandPointerLock(connection);
    }
    return;
  }
  if (connection.locked_pointer != nullptr) {
    return;
  }
  connection.locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
      connection.pointer_constraints, connection.surface, connection.pointer, nullptr,
      ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
  if (connection.locked_pointer != nullptr && !connection.locked_pointer_listener_installed) {
    zwp_locked_pointer_v1_add_listener(connection.locked_pointer, &kWaylandLockedPointerListener,
                                       &connection);
    connection.locked_pointer_listener_installed = true;
  }
}

void handleWaylandWmPing(void *data, xdg_wm_base *wm_base, const std::uint32_t serial) {
  (void)data;
  xdg_wm_base_pong(wm_base, serial);
}

constexpr xdg_wm_base_listener kWaylandWmBaseListener{handleWaylandWmPing};

void handleWaylandShmFormat(void *data, wl_shm *, const std::uint32_t format) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  if (format == WL_SHM_FORMAT_XRGB8888) {
    connection->supports_xrgb8888 = true;
  } else if (format == WL_SHM_FORMAT_ARGB8888) {
    connection->supports_argb8888 = true;
  }
}

constexpr wl_shm_listener kWaylandShmListener{handleWaylandShmFormat};

void handleWaylandToplevelConfigure(void *data, xdg_toplevel *, const std::int32_t width,
                                    const std::int32_t height, wl_array *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  if (width > 0) {
    connection->configured_width = width;
  }
  if (height > 0) {
    connection->configured_height = height;
  }
}

void handleWaylandToplevelClose(void *data, xdg_toplevel *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection != nullptr) {
    connection->open = false;
  }
}

constexpr xdg_toplevel_listener kWaylandToplevelListener{handleWaylandToplevelConfigure,
                                                         handleWaylandToplevelClose};

void handleWaylandSurfaceConfigure(void *data, xdg_surface *surface, const std::uint32_t serial) {
  auto *connection = static_cast<WaylandConnection *>(data);
  xdg_surface_ack_configure(surface, serial);
  if (connection == nullptr) {
    return;
  }
  if (connection->configured_width > 0) {
    connection->width = connection->configured_width;
  }
  if (connection->configured_height > 0) {
    connection->height = connection->configured_height;
  }
  connection->configured = true;
}

constexpr xdg_surface_listener kWaylandSurfaceListener{handleWaylandSurfaceConfigure};

void handleWaylandPointerEnter(void *data, wl_pointer *, const std::uint32_t serial,
                               wl_surface *, const wl_fixed_t surface_x,
                               const wl_fixed_t surface_y) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  connection->has_pointer_focus = true;
  connection->pointer_enter_serial = serial;
  connection->last_pointer = {static_cast<float>(wl_fixed_to_double(surface_x)),
                              static_cast<float>(wl_fixed_to_double(surface_y))};
  connection->has_last_pointer = true;
  if (connection->cursor_mode != aster::CursorMode::Disabled) {
    connection->input.pointer = connection->last_pointer;
  }
  applyWaylandCursor(*connection);
  updateWaylandPointerLock(*connection);
}

void handleWaylandPointerLeave(void *data, wl_pointer *, std::uint32_t, wl_surface *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection != nullptr) {
    connection->has_pointer_focus = false;
    connection->has_last_pointer = false;
    destroyWaylandPointerLock(*connection);
  }
}

void handleWaylandPointerMotion(void *data, wl_pointer *, std::uint32_t,
                                const wl_fixed_t surface_x, const wl_fixed_t surface_y) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  const aster::Vec2 pointer{static_cast<float>(wl_fixed_to_double(surface_x)),
                            static_cast<float>(wl_fixed_to_double(surface_y))};
  if (connection->cursor_mode == aster::CursorMode::Disabled) {
    if (connection->has_last_pointer) {
      connection->virtual_pointer =
          connection->virtual_pointer + (pointer - connection->last_pointer);
      connection->input.pointer = connection->virtual_pointer;
    }
  } else {
    connection->input.pointer = pointer;
  }
  connection->last_pointer = pointer;
  connection->has_last_pointer = true;
}

void handleWaylandPointerButton(void *data, wl_pointer *, std::uint32_t, std::uint32_t,
                                const std::uint32_t button, const std::uint32_t state) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  const int mapped = mouseButtonFromWaylandButton(button);
  if (mapped < 0) {
    return;
  }
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    addUnique(connection->input.pressed_mouse_buttons, mapped);
  } else {
    removeValue(connection->input.pressed_mouse_buttons, mapped);
  }
}

void handleWaylandPointerAxis(void *data, wl_pointer *, std::uint32_t, const std::uint32_t axis,
                              const wl_fixed_t value) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
    return;
  }
  const double amount = wl_fixed_to_double(value);
  if (std::abs(amount) > 0.0001) {
    connection->input.scroll.y += amount > 0.0 ? -1.0f : 1.0f;
  }
}

constexpr wl_pointer_listener kWaylandPointerListener{handleWaylandPointerEnter,
                                                      handleWaylandPointerLeave,
                                                      handleWaylandPointerMotion,
                                                      handleWaylandPointerButton,
                                                      handleWaylandPointerAxis};

void handleWaylandKeyboardKeymap(void *, wl_keyboard *, std::uint32_t, int fd, std::uint32_t) {
  if (fd >= 0) {
    ::close(fd);
  }
}

void handleWaylandKeyboardEnter(void *, wl_keyboard *, std::uint32_t, wl_surface *, wl_array *) {}

void handleWaylandKeyboardLeave(void *data, wl_keyboard *, std::uint32_t, wl_surface *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection != nullptr) {
    connection->input.pressed_keys.clear();
  }
}

void handleWaylandKeyboardKey(void *data, wl_keyboard *, std::uint32_t, std::uint32_t,
                              const std::uint32_t key, const std::uint32_t state) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  const int mapped = keyFromWaylandKeycode(key);
  if (mapped < 0) {
    return;
  }
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    addUnique(connection->input.pressed_keys, mapped);
  } else {
    removeValue(connection->input.pressed_keys, mapped);
  }
}

void handleWaylandKeyboardModifiers(void *, wl_keyboard *, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t) {}

constexpr wl_keyboard_listener kWaylandKeyboardListener{
    handleWaylandKeyboardKeymap, handleWaylandKeyboardEnter, handleWaylandKeyboardLeave,
    handleWaylandKeyboardKey, handleWaylandKeyboardModifiers};

void destroyWaylandPointer(WaylandConnection &connection) {
  destroyWaylandPointerLock(connection);
  if (connection.relative_pointer != nullptr) {
    zwp_relative_pointer_v1_destroy(connection.relative_pointer);
    connection.relative_pointer = nullptr;
  }
  if (connection.pointer != nullptr) {
    wl_pointer_destroy(connection.pointer);
    connection.pointer = nullptr;
  }
  connection.has_pointer_focus = false;
  connection.has_last_pointer = false;
  connection.input.pressed_mouse_buttons.clear();
}

void handleWaylandRelativeMotion(void *data, zwp_relative_pointer_v1 *, std::uint32_t,
                                 std::uint32_t, wl_fixed_t, wl_fixed_t,
                                 const wl_fixed_t dx_unaccel,
                                 const wl_fixed_t dy_unaccel) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr || connection->cursor_mode != aster::CursorMode::Disabled) {
    return;
  }
  connection->virtual_pointer =
      connection->virtual_pointer +
      aster::Vec2{static_cast<float>(wl_fixed_to_double(dx_unaccel)),
                  static_cast<float>(wl_fixed_to_double(dy_unaccel))};
  connection->input.pointer = connection->virtual_pointer;
}

constexpr zwp_relative_pointer_v1_listener kWaylandRelativePointerListener{
    handleWaylandRelativeMotion};

void createWaylandRelativePointer(WaylandConnection &connection) {
  if (connection.pointer == nullptr || connection.relative_pointer != nullptr ||
      connection.relative_pointer_manager == nullptr) {
    return;
  }
  connection.relative_pointer =
      zwp_relative_pointer_manager_v1_get_relative_pointer(connection.relative_pointer_manager,
                                                           connection.pointer);
  if (connection.relative_pointer != nullptr) {
    zwp_relative_pointer_v1_add_listener(connection.relative_pointer,
                                         &kWaylandRelativePointerListener, &connection);
  }
}

void handleWaylandLockedPointerLocked(void *data, zwp_locked_pointer_v1 *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection != nullptr) {
    connection->pointer_locked = true;
  }
}

void handleWaylandLockedPointerUnlocked(void *data, zwp_locked_pointer_v1 *) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection != nullptr) {
    connection->pointer_locked = false;
  }
}

void createWaylandPointer(WaylandConnection &connection) {
  if (connection.seat == nullptr || connection.pointer != nullptr) {
    return;
  }
  connection.pointer = wl_seat_get_pointer(connection.seat);
  if (connection.pointer != nullptr) {
    wl_pointer_add_listener(connection.pointer, &kWaylandPointerListener, &connection);
  }
  createWaylandRelativePointer(connection);
}

void handleWaylandSeatCapabilities(void *data, wl_seat *, const std::uint32_t capabilities) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr) {
    return;
  }
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0u) {
    createWaylandPointer(*connection);
  } else {
    destroyWaylandPointer(*connection);
  }
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0u) {
    if (connection->keyboard == nullptr) {
      connection->keyboard = wl_seat_get_keyboard(connection->seat);
      if (connection->keyboard != nullptr) {
        wl_keyboard_add_listener(connection->keyboard, &kWaylandKeyboardListener, connection);
      }
    }
  } else if (connection->keyboard != nullptr) {
    wl_keyboard_destroy(connection->keyboard);
    connection->keyboard = nullptr;
    connection->input.pressed_keys.clear();
  }
}

void handleWaylandSeatName(void *, wl_seat *, const char *) {}

constexpr wl_seat_listener kWaylandSeatListener{handleWaylandSeatCapabilities,
                                                handleWaylandSeatName};

void handleWaylandRegistryGlobal(void *data, wl_registry *registry, const std::uint32_t name,
                                 const char *interface, const std::uint32_t version) {
  auto *connection = static_cast<WaylandConnection *>(data);
  if (connection == nullptr || interface == nullptr) {
    return;
  }
  const std::string_view id(interface);
  if (id == wl_compositor_interface.name) {
    connection->compositor = static_cast<wl_compositor *>(wl_registry_bind(
        registry, name, &wl_compositor_interface, std::min<std::uint32_t>(version, 4u)));
  } else if (id == wl_shm_interface.name) {
    connection->shm = static_cast<wl_shm *>(
        wl_registry_bind(registry, name, &wl_shm_interface, std::min<std::uint32_t>(version, 1u)));
    if (connection->shm != nullptr) {
      wl_shm_add_listener(connection->shm, &kWaylandShmListener, connection);
    }
  } else if (id == wl_seat_interface.name) {
    connection->seat = static_cast<wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, std::min<std::uint32_t>(version, 5u)));
    if (connection->seat != nullptr) {
      wl_seat_add_listener(connection->seat, &kWaylandSeatListener, connection);
    }
  } else if (id == xdg_wm_base_interface.name) {
    connection->wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(
        registry, name, &xdg_wm_base_interface, std::min<std::uint32_t>(version, 3u)));
    if (connection->wm_base != nullptr) {
      xdg_wm_base_add_listener(connection->wm_base, &kWaylandWmBaseListener, connection);
    }
  } else if (id == zwp_relative_pointer_manager_v1_interface.name) {
    connection->relative_pointer_manager =
        static_cast<zwp_relative_pointer_manager_v1 *>(wl_registry_bind(
            registry, name, &zwp_relative_pointer_manager_v1_interface, 1u));
    createWaylandRelativePointer(*connection);
  } else if (id == zwp_pointer_constraints_v1_interface.name) {
    connection->pointer_constraints = static_cast<zwp_pointer_constraints_v1 *>(wl_registry_bind(
        registry, name, &zwp_pointer_constraints_v1_interface, 1u));
  }
}

void handleWaylandRegistryRemove(void *, wl_registry *, std::uint32_t) {}

constexpr wl_registry_listener kWaylandRegistryListener{handleWaylandRegistryGlobal,
                                                        handleWaylandRegistryRemove};

std::optional<WaylandConnection> openWayland(const aster::EngineConfig &config) {
  WaylandConnection connection;
  connection.display = wl_display_connect(nullptr);
  if (connection.display == nullptr) {
    return std::nullopt;
  }

  connection.registry = wl_display_get_registry(connection.display);
  if (connection.registry == nullptr) {
    return std::nullopt;
  }
  wl_registry_add_listener(connection.registry, &kWaylandRegistryListener, &connection);
  if (wl_display_roundtrip(connection.display) < 0 || wl_display_roundtrip(connection.display) < 0) {
    return std::nullopt;
  }
  if (connection.compositor == nullptr || connection.shm == nullptr ||
      connection.wm_base == nullptr || !connection.supports_xrgb8888) {
    return std::nullopt;
  }

  connection.width = std::max(config.initial_width, 1);
  connection.height = std::max(config.initial_height, 1);
  connection.surface = wl_compositor_create_surface(connection.compositor);
  if (connection.surface == nullptr) {
    return std::nullopt;
  }
  connection.xdg_surface = xdg_wm_base_get_xdg_surface(connection.wm_base, connection.surface);
  if (connection.xdg_surface == nullptr) {
    return std::nullopt;
  }
  xdg_surface_add_listener(connection.xdg_surface, &kWaylandSurfaceListener, &connection);
  connection.xdg_toplevel = xdg_surface_get_toplevel(connection.xdg_surface);
  if (connection.xdg_toplevel == nullptr) {
    return std::nullopt;
  }
  xdg_toplevel_add_listener(connection.xdg_toplevel, &kWaylandToplevelListener, &connection);
  xdg_toplevel_set_title(connection.xdg_toplevel, config.application_name);
  xdg_toplevel_set_app_id(connection.xdg_toplevel, "aster-learning-engine");
  wl_surface_commit(connection.surface);
  if (wl_display_flush(connection.display) < 0 && errno != EAGAIN) {
    return std::nullopt;
  }
  while (!connection.configured) {
    if (wl_display_dispatch(connection.display) < 0) {
      return std::nullopt;
    }
  }
  return connection;
}

void presentWaylandFramebuffer(WaylandConnection &connection) {
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  if (framebuffer.empty() || connection.surface == nullptr || !connection.configured) {
    return;
  }
  if (wl_display_dispatch_pending(connection.display) < 0) {
    connection.open = false;
    return;
  }

  const int width = framebuffer.width();
  const int height = framebuffer.height();
  WaylandBuffer *target = nextWaylandFrameBuffer(connection, width, height);
  if (target == nullptr || target->memory == nullptr) {
    return;
  }

  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  auto *pixels = static_cast<std::uint32_t *>(target->memory);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t src =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x)) *
          4u;
      pixels[static_cast<std::size_t>(y * width + x)] =
          0xff000000u | (static_cast<std::uint32_t>(rgba[src]) << 16u) |
          (static_cast<std::uint32_t>(rgba[src + 1u]) << 8u) |
          static_cast<std::uint32_t>(rgba[src + 2u]);
    }
  }

  wl_surface_attach(connection.surface, target->buffer, 0, 0);
  if (wl_surface_get_version(connection.surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
    wl_surface_damage_buffer(connection.surface, 0, 0, width, height);
  } else {
    wl_surface_damage(connection.surface, 0, 0, width, height);
  }
  wl_surface_commit(connection.surface);
  target->busy = true;
  if (wl_display_flush(connection.display) < 0 && errno != EAGAIN) {
    connection.open = false;
  }
}

void pollWaylandEvents(WaylandConnection &connection) {
  connection.input.scroll = {};
  while (wl_display_prepare_read(connection.display) != 0) {
    if (wl_display_dispatch_pending(connection.display) < 0) {
      connection.open = false;
      return;
    }
  }
  if (wl_display_flush(connection.display) < 0 && errno != EAGAIN) {
    wl_display_cancel_read(connection.display);
    connection.open = false;
    return;
  }

  pollfd fd{};
  fd.fd = wl_display_get_fd(connection.display);
  fd.events = POLLIN;
  const int ready = ::poll(&fd, 1, 0);
  if (ready > 0 && (fd.revents & POLLIN) != 0) {
    if (wl_display_read_events(connection.display) < 0) {
      connection.open = false;
      return;
    }
  } else {
    wl_display_cancel_read(connection.display);
    if (ready < 0 && errno != EINTR) {
      connection.open = false;
      return;
    }
  }
  if (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
    connection.open = false;
    return;
  }
  if (wl_display_dispatch_pending(connection.display) < 0) {
    connection.open = false;
  }
}

void applyWaylandCursorMode(WaylandConnection &connection, const aster::CursorMode mode) {
  connection.cursor_mode = mode;
  if (mode == aster::CursorMode::Disabled) {
    connection.virtual_pointer =
        connection.has_last_pointer
            ? connection.last_pointer
            : aster::Vec2{static_cast<float>(connection.width) * 0.5f,
                          static_cast<float>(connection.height) * 0.5f};
    connection.input.pointer = connection.virtual_pointer;
  }
  applyWaylandCursor(connection);
  updateWaylandPointerLock(connection);
  if (wl_display_flush(connection.display) < 0 && errno != EAGAIN) {
    connection.open = false;
  }
}

struct X11Connection {
  int fd = -1;
  std::uint32_t resource_base = 0;
  std::uint32_t resource_mask = 0;
  std::uint32_t resource_next = 1;
  std::uint32_t max_request_length = 65535;
  std::uint32_t root = 0;
  std::uint32_t root_visual = 0;
  std::uint32_t root_depth = 24;
  std::uint32_t black_pixel = 0;
  std::uint32_t white_pixel = 0xffffffu;
  std::uint32_t red_mask = 0x00ff0000u;
  std::uint32_t green_mask = 0x0000ff00u;
  std::uint32_t blue_mask = 0x000000ffu;
  std::uint8_t bits_per_pixel = 32;
  std::uint8_t scanline_pad = 32;
  std::uint8_t image_byte_order = 0;
  std::uint32_t window = 0;
  std::uint32_t gc = 0;
  std::uint32_t blank_pixmap = 0;
  std::uint32_t blank_cursor = 0;
  std::uint32_t wm_protocols = 0;
  std::uint32_t wm_delete_window = 0;
  int width = 1;
  int height = 1;
  aster::ControlSnapshot input;
  aster::CursorMode cursor_mode = aster::CursorMode::Normal;
  aster::Vec2 virtual_pointer{};
  bool suppress_center_motion = false;

  ~X11Connection() {
    if (fd >= 0) {
      ::close(fd);
    }
  }

  X11Connection() = default;
  X11Connection(const X11Connection &) = delete;
  X11Connection &operator=(const X11Connection &) = delete;
  X11Connection(X11Connection &&other) noexcept {
    *this = std::move(other);
  }
  X11Connection &operator=(X11Connection &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (fd >= 0) {
      ::close(fd);
    }
    fd = std::exchange(other.fd, -1);
    resource_base = other.resource_base;
    resource_mask = other.resource_mask;
    resource_next = other.resource_next;
    max_request_length = other.max_request_length;
    root = other.root;
    root_visual = other.root_visual;
    root_depth = other.root_depth;
    black_pixel = other.black_pixel;
    white_pixel = other.white_pixel;
    red_mask = other.red_mask;
    green_mask = other.green_mask;
    blue_mask = other.blue_mask;
    bits_per_pixel = other.bits_per_pixel;
    scanline_pad = other.scanline_pad;
    image_byte_order = other.image_byte_order;
    window = other.window;
    gc = other.gc;
    wm_protocols = other.wm_protocols;
    wm_delete_window = other.wm_delete_window;
    blank_pixmap = other.blank_pixmap;
    blank_cursor = other.blank_cursor;
    width = other.width;
    height = other.height;
    input = std::move(other.input);
    cursor_mode = other.cursor_mode;
    virtual_pointer = other.virtual_pointer;
    suppress_center_motion = other.suppress_center_motion;
    return *this;
  }

  [[nodiscard]] std::uint32_t resourceId() {
    const std::uint32_t id = resource_base | (resource_next & resource_mask);
    ++resource_next;
    return id;
  }
};

bool connectUnixSocket(X11Connection &connection, const std::string &path) {
  connection.fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (connection.fd < 0) {
    return false;
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    return false;
  }
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1u);
  if (::connect(connection.fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    ::close(connection.fd);
    connection.fd = -1;
    return false;
  }
  return true;
}

bool sendSetup(X11Connection &connection, const std::optional<XAuthority> &authority) {
  std::vector<std::uint8_t> request;
  request.reserve(64u);
  write8(request, 'l');
  write8(request, 0);
  write16(request, 11);
  write16(request, 0);

  const std::string_view auth_name =
      authority.has_value() ? std::string_view(authority->name) : std::string_view{};
  const std::vector<std::uint8_t> empty_auth;
  const std::vector<std::uint8_t> &auth_data =
      authority.has_value() ? authority->data : empty_auth;

  write16(request, static_cast<std::uint16_t>(auth_name.size()));
  write16(request, static_cast<std::uint16_t>(auth_data.size()));
  write16(request, 0);
  request.insert(request.end(), auth_name.begin(), auth_name.end());
  pad4(request);
  request.insert(request.end(), auth_data.begin(), auth_data.end());
  pad4(request);
  if (!sendFull(connection.fd, request)) {
    return false;
  }

  std::array<std::uint8_t, 8> prefix{};
  if (!readFull(connection.fd, prefix.data(), prefix.size())) {
    return false;
  }
  const std::uint8_t status = prefix[0];
  const std::uint16_t payload_units = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(prefix[6]) | (static_cast<std::uint16_t>(prefix[7]) << 8u));
  std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_units) * 4u);
  if (!payload.empty() && !readFull(connection.fd, payload.data(), payload.size())) {
    return false;
  }
  if (status != 1u || payload.size() < 40u) {
    return false;
  }

  connection.resource_base = readLe32(payload, 4u);
  connection.resource_mask = readLe32(payload, 8u);
  connection.max_request_length = std::max<std::uint32_t>(readLe16(payload, 18u), 64u);
  const std::uint8_t screen_count = payload[20];
  const std::uint8_t format_count = payload[21];
  connection.image_byte_order = payload[22];
  std::size_t offset = 32u;
  const std::uint16_t vendor_len = readLe16(payload, 16u);
  offset += padded4(vendor_len);

  struct PixmapFormat {
    std::uint8_t depth = 0;
    std::uint8_t bits_per_pixel = 0;
    std::uint8_t scanline_pad = 0;
  };
  std::vector<PixmapFormat> formats;
  for (std::uint8_t i = 0; i < format_count && offset + 8u <= payload.size(); ++i) {
    formats.push_back({payload[offset], payload[offset + 1u], payload[offset + 2u]});
    offset += 8u;
  }
  if (screen_count == 0u || offset + 40u > payload.size()) {
    return false;
  }

  connection.root = readLe32(payload, offset);
  connection.white_pixel = readLe32(payload, offset + 8u);
  connection.black_pixel = readLe32(payload, offset + 12u);
  connection.root_visual = readLe32(payload, offset + 32u);
  connection.root_depth = payload[offset + 38u];
  const std::uint8_t depth_count = payload[offset + 39u];
  offset += 40u;

  for (const PixmapFormat &format : formats) {
    if (format.depth == connection.root_depth) {
      connection.bits_per_pixel = format.bits_per_pixel;
      connection.scanline_pad = format.scanline_pad;
      break;
    }
  }

  for (std::uint8_t i = 0; i < depth_count && offset + 8u <= payload.size(); ++i) {
    const std::uint8_t depth = payload[offset];
    const std::uint16_t visual_count = readLe16(payload, offset + 2u);
    offset += 8u;
    for (std::uint16_t visual = 0; visual < visual_count && offset + 24u <= payload.size();
         ++visual) {
      const std::uint32_t visual_id = readLe32(payload, offset);
      const std::uint8_t visual_class = payload[offset + 4u];
      if (depth == connection.root_depth && visual_id == connection.root_visual &&
          visual_class == kXVisualTrueColor) {
        connection.red_mask = readLe32(payload, offset + 8u);
        connection.green_mask = readLe32(payload, offset + 12u);
        connection.blue_mask = readLe32(payload, offset + 16u);
      }
      offset += 24u;
    }
  }

  return true;
}

std::optional<std::uint32_t> internAtom(X11Connection &connection, const std::string_view name) {
  std::vector<std::uint8_t> request;
  const std::size_t total = 8u + padded4(name.size());
  request.reserve(total);
  write8(request, 16);
  write8(request, 0);
  write16(request, static_cast<std::uint16_t>(total / 4u));
  write16(request, static_cast<std::uint16_t>(name.size()));
  write16(request, 0);
  request.insert(request.end(), name.begin(), name.end());
  pad4(request);
  if (!sendFull(connection.fd, request)) {
    return std::nullopt;
  }
  std::array<std::uint8_t, 32> reply{};
  if (!readFull(connection.fd, reply.data(), reply.size()) || reply[0] != 1u) {
    return std::nullopt;
  }
  return readLe32(reply, 8u);
}

bool createInvisibleCursor(X11Connection &connection) {
  connection.blank_pixmap = connection.resourceId();
  connection.blank_cursor = connection.resourceId();

  std::vector<std::uint8_t> pixmap;
  write8(pixmap, 53);
  write8(pixmap, 1);
  write16(pixmap, 4);
  write32(pixmap, connection.blank_pixmap);
  write32(pixmap, connection.window);
  write16(pixmap, 1);
  write16(pixmap, 1);
  if (!sendFull(connection.fd, pixmap)) {
    return false;
  }

  std::vector<std::uint8_t> cursor;
  write8(cursor, 93);
  write8(cursor, 0);
  write16(cursor, 8);
  write32(cursor, connection.blank_cursor);
  write32(cursor, connection.blank_pixmap);
  write32(cursor, connection.blank_pixmap);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  write16(cursor, 0);
  return sendFull(connection.fd, cursor);
}

bool changeCursor(X11Connection &connection, const std::uint32_t cursor) {
  std::vector<std::uint8_t> request;
  write8(request, 2);
  write8(request, 0);
  write16(request, 4);
  write32(request, connection.window);
  write32(request, 1u << 14u);
  write32(request, cursor);
  return sendFull(connection.fd, request);
}

bool warpPointerToCenter(X11Connection &connection) {
  const std::int16_t center_x = static_cast<std::int16_t>(std::max(connection.width, 1) / 2);
  const std::int16_t center_y = static_cast<std::int16_t>(std::max(connection.height, 1) / 2);
  std::vector<std::uint8_t> request;
  write8(request, 41);
  write8(request, 0);
  write16(request, 6);
  write32(request, 0);
  write32(request, connection.window);
  write16(request, 0);
  write16(request, 0);
  write16(request, 0);
  write16(request, 0);
  write16(request, static_cast<std::uint16_t>(center_x));
  write16(request, static_cast<std::uint16_t>(center_y));
  connection.suppress_center_motion = true;
  return sendFull(connection.fd, request);
}

void applyCursorMode(X11Connection &connection, const aster::CursorMode mode) {
  connection.cursor_mode = mode;
  if (mode == aster::CursorMode::Normal) {
    (void)changeCursor(connection, 0);
    return;
  }

  if (connection.blank_cursor == 0u) {
    (void)createInvisibleCursor(connection);
  }
  if (connection.blank_cursor != 0u) {
    (void)changeCursor(connection, connection.blank_cursor);
  }
  if (mode == aster::CursorMode::Disabled) {
    connection.virtual_pointer = {static_cast<float>(std::max(connection.width, 1)) * 0.5f,
                                  static_cast<float>(std::max(connection.height, 1)) * 0.5f};
    connection.input.pointer = connection.virtual_pointer;
    (void)warpPointerToCenter(connection);
  }
}

bool createWindow(X11Connection &connection, const int width, const int height,
                  const std::string_view title) {
  connection.width = std::max(width, 1);
  connection.height = std::max(height, 1);
  connection.window = connection.resourceId();
  connection.gc = connection.resourceId();

  const std::uint32_t event_mask = kXEventMaskKeyPress | kXEventMaskKeyRelease |
                                   kXEventMaskButtonPress | kXEventMaskButtonRelease |
                                   kXEventMaskPointerMotion | kXEventMaskExposure |
                                   kXEventMaskStructureNotify;
  std::vector<std::uint8_t> create;
  write8(create, 1);
  write8(create, static_cast<std::uint8_t>(connection.root_depth));
  write16(create, 10);
  write32(create, connection.window);
  write32(create, connection.root);
  write16(create, 80);
  write16(create, 80);
  write16(create, static_cast<std::uint16_t>(connection.width));
  write16(create, static_cast<std::uint16_t>(connection.height));
  write16(create, 0);
  write16(create, 1);
  write32(create, connection.root_visual);
  write32(create, (1u << 1u) | (1u << 11u));
  write32(create, connection.black_pixel);
  write32(create, event_mask);
  if (!sendFull(connection.fd, create)) {
    return false;
  }

  std::vector<std::uint8_t> gc;
  write8(gc, 55);
  write8(gc, 0);
  write16(gc, 6);
  write32(gc, connection.gc);
  write32(gc, connection.window);
  write32(gc, (1u << 2u) | (1u << 3u));
  write32(gc, connection.white_pixel);
  write32(gc, connection.black_pixel);
  if (!sendFull(connection.fd, gc)) {
    return false;
  }

  std::vector<std::uint8_t> name;
  const std::size_t name_total = 24u + padded4(title.size());
  write8(name, 18);
  write8(name, 0);
  write16(name, static_cast<std::uint16_t>(name_total / 4u));
  write32(name, connection.window);
  write32(name, kXAtomWmName);
  write32(name, kXAtomString);
  write8(name, 8);
  write8(name, 0);
  write16(name, 0);
  write32(name, static_cast<std::uint32_t>(title.size()));
  name.insert(name.end(), title.begin(), title.end());
  pad4(name);
  if (!sendFull(connection.fd, name)) {
    return false;
  }

  connection.wm_protocols = internAtom(connection, "WM_PROTOCOLS").value_or(0u);
  connection.wm_delete_window = internAtom(connection, "WM_DELETE_WINDOW").value_or(0u);
  if (connection.wm_protocols != 0u && connection.wm_delete_window != 0u) {
    std::vector<std::uint8_t> protocols;
    write8(protocols, 18);
    write8(protocols, 0);
    write16(protocols, 7);
    write32(protocols, connection.window);
    write32(protocols, connection.wm_protocols);
    write32(protocols, kXAtomAtom);
    write8(protocols, 32);
    write8(protocols, 0);
    write16(protocols, 0);
    write32(protocols, 1);
    write32(protocols, connection.wm_delete_window);
    if (!sendFull(connection.fd, protocols)) {
      return false;
    }
  }

  std::vector<std::uint8_t> map;
  write8(map, 8);
  write8(map, 0);
  write16(map, 2);
  write32(map, connection.window);
  return sendFull(connection.fd, map);
}

std::optional<X11Connection> openX11(const aster::EngineConfig &config) {
  const std::optional<DisplayName> display = parseDisplayName();
  if (!display.has_value()) {
    return std::nullopt;
  }

  X11Connection connection;
  if (!connectUnixSocket(connection, display->socket_path)) {
    return std::nullopt;
  }

  const std::optional<XAuthority> authority = readXAuthority(display->display_number);
  if (!sendSetup(connection, authority)) {
    return std::nullopt;
  }
  if (!createWindow(connection, config.initial_width, config.initial_height,
                    config.application_name)) {
    return std::nullopt;
  }
  return connection;
}

std::uint32_t packPixel(const X11Connection &connection, const std::uint8_t r,
                        const std::uint8_t g, const std::uint8_t b) {
  return colorToMask(r, connection.red_mask) | colorToMask(g, connection.green_mask) |
         colorToMask(b, connection.blue_mask);
}

std::size_t imageRowStride(const X11Connection &connection, const int width) {
  const std::size_t pad_bits = std::max<std::size_t>(connection.scanline_pad, 8u);
  const std::size_t row_bits =
      static_cast<std::size_t>(width) * std::max<std::size_t>(connection.bits_per_pixel, 8u);
  return ((row_bits + pad_bits - 1u) / pad_bits) * (pad_bits / 8u);
}

void writePixelBytes(const X11Connection &connection, std::vector<std::uint8_t> &out,
                     const std::size_t offset, const std::uint32_t pixel) {
  const std::size_t bytes_per_pixel =
      std::max<std::size_t>((connection.bits_per_pixel + 7u) / 8u, 1u);
  if (connection.image_byte_order == 0u) {
    for (std::size_t byte = 0; byte < bytes_per_pixel; ++byte) {
      out[offset + byte] =
          static_cast<std::uint8_t>((pixel >> static_cast<unsigned>(byte * 8u)) & 0xffu);
    }
  } else {
    for (std::size_t byte = 0; byte < bytes_per_pixel; ++byte) {
      const std::size_t source = bytes_per_pixel - 1u - byte;
      out[offset + byte] =
          static_cast<std::uint8_t>((pixel >> static_cast<unsigned>(source * 8u)) & 0xffu);
    }
  }
}

bool putImageStrip(X11Connection &connection, const std::span<const std::uint8_t> rgba,
                   const int framebuffer_width, const int y, const int strip_height) {
  const std::size_t row_stride = imageRowStride(connection, framebuffer_width);
  const std::size_t bytes_per_pixel =
      std::max<std::size_t>((connection.bits_per_pixel + 7u) / 8u, 1u);
  std::vector<std::uint8_t> pixels(row_stride * static_cast<std::size_t>(strip_height), 0);
  for (int row = 0; row < strip_height; ++row) {
    const std::size_t src_row =
        static_cast<std::size_t>(y + row) * static_cast<std::size_t>(framebuffer_width) * 4u;
    const std::size_t dst_row = static_cast<std::size_t>(row) * row_stride;
    for (int x = 0; x < framebuffer_width; ++x) {
      const std::size_t src = src_row + static_cast<std::size_t>(x) * 4u;
      const std::uint32_t pixel = packPixel(connection, rgba[src], rgba[src + 1u], rgba[src + 2u]);
      writePixelBytes(connection, pixels, dst_row + static_cast<std::size_t>(x) * bytes_per_pixel,
                      pixel);
    }
  }

  std::vector<std::uint8_t> request;
  request.reserve(24u + padded4(pixels.size()));
  write8(request, 72);
  write8(request, 2);
  write16(request, static_cast<std::uint16_t>((24u + padded4(pixels.size())) / 4u));
  write32(request, connection.window);
  write32(request, connection.gc);
  write16(request, static_cast<std::uint16_t>(framebuffer_width));
  write16(request, static_cast<std::uint16_t>(strip_height));
  write16(request, 0);
  write16(request, static_cast<std::uint16_t>(y));
  write8(request, 0);
  write8(request, static_cast<std::uint8_t>(connection.root_depth));
  write16(request, 0);
  request.insert(request.end(), pixels.begin(), pixels.end());
  pad4(request);
  return sendFull(connection.fd, request);
}

void updatePointer(X11Connection &connection, const std::int16_t x, const std::int16_t y) {
  if (connection.cursor_mode != aster::CursorMode::Disabled) {
    connection.input.pointer = {static_cast<float>(x), static_cast<float>(y)};
    return;
  }

  const std::int16_t center_x = static_cast<std::int16_t>(std::max(connection.width, 1) / 2);
  const std::int16_t center_y = static_cast<std::int16_t>(std::max(connection.height, 1) / 2);
  if (connection.suppress_center_motion && std::abs(x - center_x) <= 1 &&
      std::abs(y - center_y) <= 1) {
    connection.suppress_center_motion = false;
    return;
  }

  connection.virtual_pointer =
      connection.virtual_pointer + aster::Vec2{static_cast<float>(x - center_x),
                                               static_cast<float>(y - center_y)};
  connection.input.pointer = connection.virtual_pointer;
  (void)warpPointerToCenter(connection);
}

void presentFramebuffer(X11Connection &connection) {
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  if (framebuffer.empty()) {
    return;
  }

  const int width = framebuffer.width();
  const int height = framebuffer.height();
  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  const std::size_t row_stride = imageRowStride(connection, width);
  const std::size_t max_request_bytes =
      std::max<std::size_t>((connection.max_request_length - 6u) * 4u, row_stride);
  const int rows_per_strip =
      std::max(1, static_cast<int>(std::max<std::size_t>(max_request_bytes / row_stride, 1u)));
  for (int y = 0; y < height; y += rows_per_strip) {
    const int strip_height = std::min(rows_per_strip, height - y);
    if (!putImageStrip(connection, rgba, width, y, strip_height)) {
      break;
    }
  }
}

void handleEvent(X11Connection &connection, const std::array<std::uint8_t, 32> &event,
                 bool &open) {
  const std::uint8_t type = event[0] & 0x7fu;
  switch (type) {
  case kXEventKeyPress:
  case kXEventKeyRelease: {
    const int key = keyFromX11Keycode(event[1]);
    if (key >= 0) {
      if (type == kXEventKeyPress) {
        addUnique(connection.input.pressed_keys, key);
      } else {
        removeValue(connection.input.pressed_keys, key);
      }
    }
    break;
  }
  case kXEventButtonPress:
  case kXEventButtonRelease: {
    const std::uint8_t detail = event[1];
    if (detail == 4u || detail == 5u) {
      if (type == kXEventButtonPress) {
        connection.input.scroll.y += detail == 4u ? 1.0f : -1.0f;
      }
      break;
    }
    const int button = mouseButtonFromX11Detail(detail);
    if (button >= 0) {
      if (type == kXEventButtonPress) {
        addUnique(connection.input.pressed_mouse_buttons, button);
      } else {
        removeValue(connection.input.pressed_mouse_buttons, button);
      }
    }
    updatePointer(connection, readLeI16(event, 24u), readLeI16(event, 26u));
    break;
  }
  case kXEventMotionNotify:
    updatePointer(connection, readLeI16(event, 24u), readLeI16(event, 26u));
    break;
  case kXEventConfigureNotify:
    connection.width = std::max(1, static_cast<int>(readLe16(event, 20u)));
    connection.height = std::max(1, static_cast<int>(readLe16(event, 22u)));
    if (connection.cursor_mode == aster::CursorMode::Disabled) {
      (void)warpPointerToCenter(connection);
    }
    break;
  case kXEventClientMessage:
    if (readLe32(event, 8u) == connection.wm_protocols &&
        readLe32(event, 12u) == connection.wm_delete_window) {
      open = false;
    }
    break;
  default:
    break;
  }
}

void pollX11Events(X11Connection &connection, bool &open) {
  connection.input.scroll = {};
  for (;;) {
    std::array<std::uint8_t, 32> event{};
    const ssize_t n = ::recv(connection.fd, event.data(), event.size(), MSG_DONTWAIT);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      open = false;
      break;
    }
    if (n == 0) {
      open = false;
      break;
    }
    if (n == static_cast<ssize_t>(event.size())) {
      handleEvent(connection, event, open);
    }
  }
}

} // namespace

namespace aster {

struct WindowImpl {
  int width = 1;
  int height = 1;
  bool open = true;
  ControlSnapshot input;
  std::optional<WaylandConnection> wayland;
  std::optional<X11Connection> x11;
};

Window::Window(const EngineConfig &config) : impl_(std::make_unique<WindowImpl>()) {
  scale_framebuffer_to_display_ = config.scale_framebuffer_to_display;
  const bool force_x11 = std::getenv("ASTER_FORCE_X11") != nullptr;
  const bool force_wayland = std::getenv("ASTER_FORCE_WAYLAND") != nullptr;
  if (!force_x11) {
    impl_->wayland = openWayland(config);
  }
  if (!impl_->wayland.has_value() && !force_wayland) {
    impl_->x11 = openX11(config);
  }
  if (!impl_->wayland.has_value() && !impl_->x11.has_value()) {
    throw std::runtime_error(
        "Aster could not open a Linux desktop display. Set WAYLAND_DISPLAY for Wayland or DISPLAY "
        "for the raw X11 fallback. Use ASTER_FORCE_WAYLAND=1 or ASTER_FORCE_X11=1 to select a "
        "specific backend.");
  }
  impl_->width = impl_->wayland.has_value() ? impl_->wayland->width : impl_->x11->width;
  impl_->height = impl_->wayland.has_value() ? impl_->wayland->height : impl_->x11->height;
}

Window::~Window() = default;
Window::Window(Window &&other) noexcept = default;
Window &Window::operator=(Window &&other) noexcept = default;

bool Window::isOpen() const {
  return impl_ != nullptr && impl_->open;
}

void Window::pollEvents() {
  if (impl_ == nullptr) {
    return;
  }
  if (impl_->wayland.has_value()) {
    pollWaylandEvents(*impl_->wayland);
    impl_->open = impl_->wayland->open;
    impl_->width = impl_->wayland->width;
    impl_->height = impl_->wayland->height;
    impl_->input = impl_->wayland->input;
  } else if (impl_->x11.has_value()) {
    pollX11Events(*impl_->x11, impl_->open);
    impl_->width = impl_->x11->width;
    impl_->height = impl_->x11->height;
    impl_->input = impl_->x11->input;
  }
}

void Window::swapBuffers() {
  if (impl_ != nullptr && impl_->wayland.has_value()) {
    presentWaylandFramebuffer(*impl_->wayland);
  } else if (impl_ != nullptr && impl_->x11.has_value()) {
    presentFramebuffer(*impl_->x11);
  }
}

void Window::setVsync(const bool enabled) {
  (void)enabled;
}

void Window::setCursorMode(const CursorMode mode) {
  if (impl_ != nullptr && impl_->wayland.has_value()) {
    applyWaylandCursorMode(*impl_->wayland, mode);
    impl_->input = impl_->wayland->input;
  } else if (impl_ != nullptr && impl_->x11.has_value()) {
    applyCursorMode(*impl_->x11, mode);
    impl_->input = impl_->x11->input;
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
  return {impl_->width, impl_->height};
}

std::pair<int, int> Window::framebufferSize() const {
  return windowSize();
}

ControlSnapshot Window::captureControls(const ControlScheme &scheme) const {
  (void)scheme;
  return impl_ == nullptr ? ControlSnapshot{} : impl_->input;
}

} // namespace aster
