// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aster::asset_json {

struct SourceLocation {
  std::uint32_t line = 1u;
  std::uint32_t column = 1u;
};

class Error : public std::runtime_error {
public:
  Error(std::string message, const SourceLocation location)
      : std::runtime_error(message + " at " + std::to_string(location.line) + ":" +
                           std::to_string(location.column)),
        location_(location) {}

  [[nodiscard]] SourceLocation location() const noexcept {
    return location_;
  }

private:
  SourceLocation location_{};
};

struct Value {
  enum class Kind {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
  };

  Kind kind = Kind::Null;
  SourceLocation location{};
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::map<std::string, Value> object;

  [[nodiscard]] const Value *find(const std::string_view key) const {
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
  }
};

class Parser {
public:
  explicit Parser(std::string_view source) : source_(source) {}

  [[nodiscard]] Value parse() {
    Value value = parseValue();
    skipWhitespace();
    if (position_ != source_.size()) {
      fail("asset JSON has trailing data");
    }
    return value;
  }

private:
  [[nodiscard]] SourceLocation location() const noexcept {
    return {line_, column_};
  }

  [[noreturn]] void fail(const std::string &message) const {
    throw Error(message, location());
  }

  void advance() {
    if (position_ >= source_.size()) {
      return;
    }
    if (source_[position_] == '\n') {
      ++line_;
      column_ = 1u;
    } else {
      ++column_;
    }
    ++position_;
  }

  void skipWhitespace() {
    while (position_ < source_.size() &&
           std::isspace(static_cast<unsigned char>(source_[position_]))) {
      advance();
    }
  }

  [[nodiscard]] char peek() {
    skipWhitespace();
    if (position_ >= source_.size()) {
      fail("asset JSON ended unexpectedly");
    }
    return source_[position_];
  }

  bool consume(const char expected) {
    skipWhitespace();
    if (position_ < source_.size() && source_[position_] == expected) {
      advance();
      return true;
    }
    return false;
  }

  void require(const char expected) {
    if (!consume(expected)) {
      fail("asset JSON has unexpected syntax");
    }
  }

  [[nodiscard]] Value parseValue() {
    const char c = peek();
    Value value;
    value.location = location();
    if (c == '{') {
      return parseObject(value.location);
    }
    if (c == '[') {
      return parseArray(value.location);
    }
    if (c == '"') {
      value.kind = Value::Kind::String;
      value.string = parseString();
      return value;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parseNumber(value.location);
    }
    if (source_.substr(position_, 4) == "true") {
      for (int i = 0; i < 4; ++i) {
        advance();
      }
      value.kind = Value::Kind::Bool;
      value.boolean = true;
      return value;
    }
    if (source_.substr(position_, 5) == "false") {
      for (int i = 0; i < 5; ++i) {
        advance();
      }
      value.kind = Value::Kind::Bool;
      value.boolean = false;
      return value;
    }
    if (source_.substr(position_, 4) == "null") {
      for (int i = 0; i < 4; ++i) {
        advance();
      }
      return value;
    }
    fail("asset JSON contains an unsupported value");
  }

  [[nodiscard]] Value parseObject(const SourceLocation value_location) {
    Value value;
    value.kind = Value::Kind::Object;
    value.location = value_location;
    require('{');
    if (consume('}')) {
      return value;
    }
    for (;;) {
      if (peek() != '"') {
        fail("asset JSON object key must be a string");
      }
      std::string key = parseString();
      require(':');
      value.object.emplace(std::move(key), parseValue());
      if (consume('}')) {
        return value;
      }
      require(',');
    }
  }

  [[nodiscard]] Value parseArray(const SourceLocation value_location) {
    Value value;
    value.kind = Value::Kind::Array;
    value.location = value_location;
    require('[');
    if (consume(']')) {
      return value;
    }
    for (;;) {
      value.array.push_back(parseValue());
      if (consume(']')) {
        return value;
      }
      require(',');
    }
  }

  [[nodiscard]] Value parseNumber(const SourceLocation value_location) {
    Value value;
    value.kind = Value::Kind::Number;
    value.location = value_location;
    const std::size_t start = position_;
    if (source_[position_] == '-') {
      advance();
    }
    while (position_ < source_.size() &&
           std::isdigit(static_cast<unsigned char>(source_[position_]))) {
      advance();
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      advance();
      while (position_ < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[position_]))) {
        advance();
      }
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      advance();
      if (position_ < source_.size() && (source_[position_] == '-' || source_[position_] == '+')) {
        advance();
      }
      while (position_ < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[position_]))) {
        advance();
      }
    }
    value.number = std::stod(std::string(source_.substr(start, position_ - start)));
    return value;
  }

  [[nodiscard]] std::string parseString() {
    require('"');
    std::string out;
    while (position_ < source_.size()) {
      const char c = source_[position_];
      advance();
      if (c == '"') {
        return out;
      }
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (position_ >= source_.size()) {
        fail("asset JSON has an unterminated escape");
      }
      const char escaped = source_[position_];
      advance();
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        out.push_back(escaped);
        break;
      case 'b':
        out.push_back('\b');
        break;
      case 'f':
        out.push_back('\f');
        break;
      case 'n':
        out.push_back('\n');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case 't':
        out.push_back('\t');
        break;
      default:
        out.push_back(escaped);
        break;
      }
    }
    fail("asset JSON has an unterminated string");
  }

  std::string_view source_;
  std::size_t position_ = 0u;
  std::uint32_t line_ = 1u;
  std::uint32_t column_ = 1u;
};

[[nodiscard]] inline std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not open asset file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] inline Value parse(std::string_view source) {
  return Parser(source).parse();
}

[[nodiscard]] inline Value parseFile(const std::filesystem::path &path) {
  return parse(readTextFile(path));
}

[[nodiscard]] inline const Value &required(const Value &json, const std::string_view key) {
  const Value *value = json.find(key);
  if (value == nullptr) {
    throw Error("asset JSON is missing required key: " + std::string(key), json.location);
  }
  return *value;
}

[[nodiscard]] inline std::string textOr(const Value &json, const std::string_view key,
                                        const std::string_view fallback = {}) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::String ? value->string
                                                               : std::string(fallback);
}

[[nodiscard]] inline std::uint32_t u32Or(const Value &json, const std::string_view key,
                                         const std::uint32_t fallback = 0u) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Number
             ? static_cast<std::uint32_t>(std::max(value->number, 0.0))
             : fallback;
}

[[nodiscard]] inline std::uint64_t u64Or(const Value &json, const std::string_view key,
                                         const std::uint64_t fallback = 0u) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Number
             ? static_cast<std::uint64_t>(std::max(value->number, 0.0))
             : fallback;
}

[[nodiscard]] inline float f32Or(const Value &json, const std::string_view key,
                                 const float fallback = 0.0f) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Number ? static_cast<float>(value->number)
                                                               : fallback;
}

[[nodiscard]] inline bool boolOr(const Value &json, const std::string_view key,
                                 const bool fallback = false) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Bool ? value->boolean : fallback;
}

} // namespace aster::asset_json
