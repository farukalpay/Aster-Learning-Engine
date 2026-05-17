// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/platform/window.hpp"

#include "aster/input/input_codes.hpp"
#include "aster/render/software_framebuffer.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
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
  std::optional<X11Connection> x11;
};

Window::Window(const EngineConfig &config) : impl_(std::make_unique<WindowImpl>()) {
  scale_framebuffer_to_display_ = config.scale_framebuffer_to_display;
  impl_->x11 = openX11(config);
  if (!impl_->x11.has_value()) {
    throw std::runtime_error(
        "Aster could not open a raw X11 display. Set DISPLAY to a local X server for the Linux "
        "desktop backend.");
  }
  impl_->width = impl_->x11->width;
  impl_->height = impl_->x11->height;
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
  pollX11Events(*impl_->x11, impl_->open);
  impl_->width = impl_->x11->width;
  impl_->height = impl_->x11->height;
  impl_->input = impl_->x11->input;
}

void Window::swapBuffers() {
  if (impl_ != nullptr) {
    presentFramebuffer(*impl_->x11);
  }
}

void Window::setVsync(const bool enabled) {
  (void)enabled;
}

void Window::setCursorMode(const CursorMode mode) {
  if (impl_ != nullptr) {
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
