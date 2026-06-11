// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/game_sdk/game_sdk.hpp"

#include "aster/core/belief_extraction.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace aster::sdk {
namespace {

struct Json {
  enum class Kind {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
  };

  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Json> array;
  std::map<std::string, Json> object;

  [[nodiscard]] const Json *find(const std::string &key) const {
    if (kind != Kind::Object) {
      return nullptr;
    }
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
  }
};

class JsonParser {
public:
  explicit JsonParser(std::string_view source) : source_(source) {}

  [[nodiscard]] Json parse() {
    Json value = parseValue();
    skipWhitespace();
    if (position_ != source_.size()) {
      throw std::runtime_error("JSON has trailing data.");
    }
    return value;
  }

private:
  void skipWhitespace() {
    while (position_ < source_.size()) {
      const unsigned char c = static_cast<unsigned char>(source_[position_]);
      if (!std::isspace(c)) {
        break;
      }
      ++position_;
    }
  }

  [[nodiscard]] char peek() {
    skipWhitespace();
    if (position_ >= source_.size()) {
      throw std::runtime_error("Unexpected end of JSON.");
    }
    return source_[position_];
  }

  [[nodiscard]] bool consume(const char expected) {
    skipWhitespace();
    if (position_ < source_.size() && source_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  void require(const char expected) {
    if (!consume(expected)) {
      throw std::runtime_error("Unexpected JSON syntax.");
    }
  }

  [[nodiscard]] Json parseValue() {
    const char c = peek();
    if (c == '{') {
      return parseObject();
    }
    if (c == '[') {
      return parseArray();
    }
    if (c == '"') {
      Json value;
      value.kind = Json::Kind::String;
      value.string = parseString();
      return value;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parseNumber();
    }
    if (source_.substr(position_, 4) == "true") {
      position_ += 4;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = true;
      return value;
    }
    if (source_.substr(position_, 5) == "false") {
      position_ += 5;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = false;
      return value;
    }
    if (source_.substr(position_, 4) == "null") {
      position_ += 4;
      return {};
    }
    throw std::runtime_error("Unsupported JSON value.");
  }

  [[nodiscard]] Json parseObject() {
    Json value;
    value.kind = Json::Kind::Object;
    require('{');
    if (consume('}')) {
      return value;
    }
    for (;;) {
      skipWhitespace();
      if (position_ >= source_.size() || source_[position_] != '"') {
        throw std::runtime_error("JSON object key must be a string.");
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

  [[nodiscard]] Json parseArray() {
    Json value;
    value.kind = Json::Kind::Array;
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

  [[nodiscard]] std::string parseString() {
    require('"');
    std::string out;
    while (position_ < source_.size()) {
      const char c = source_[position_++];
      if (c == '"') {
        return out;
      }
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (position_ >= source_.size()) {
        throw std::runtime_error("Unterminated JSON escape.");
      }
      const char escaped = source_[position_++];
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
        throw std::runtime_error("Unsupported JSON string escape.");
      }
    }
    throw std::runtime_error("Unterminated JSON string.");
  }

  [[nodiscard]] Json parseNumber() {
    const std::size_t start = position_;
    if (source_[position_] == '-') {
      ++position_;
    }
    while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
      ++position_;
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      ++position_;
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      ++position_;
      if (position_ < source_.size() && (source_[position_] == '-' || source_[position_] == '+')) {
        ++position_;
      }
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
        ++position_;
      }
    }
    Json value;
    value.kind = Json::Kind::Number;
    value.number = std::stod(std::string(source_.substr(start, position_ - start)));
    return value;
  }

  std::string_view source_;
  std::size_t position_ = 0u;
};

void addDiagnostic(std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
                   std::string path, std::string message,
                   const DiagnosticSeverity severity = DiagnosticSeverity::Error) {
  diagnostics.push_back({severity, source, std::move(path), std::move(message)});
}

[[nodiscard]] bool expectObject(const Json &value, std::vector<Diagnostic> &diagnostics,
                                const std::filesystem::path &source, const std::string &path) {
  if (value.kind == Json::Kind::Object) {
    return true;
  }
  addDiagnostic(diagnostics, source, path, "expected object");
  return false;
}

[[nodiscard]] const Json *member(const Json &object, const char *key) {
  return object.find(key);
}

[[nodiscard]] std::string childPath(const std::string &path, const std::string_view child) {
  return path + "." + std::string(child);
}

[[nodiscard]] std::string indexPath(const std::string &path, const std::size_t index) {
  return path + "[" + std::to_string(index) + "]";
}

[[nodiscard]] std::optional<std::string>
readString(const Json &object, const char *key, std::vector<Diagnostic> &diagnostics,
           const std::filesystem::path &source, const std::string &path, const bool required) {
  const Json *value = member(object, key);
  if (value == nullptr) {
    if (required) {
      addDiagnostic(diagnostics, source, childPath(path, key), "missing required string");
    }
    return std::nullopt;
  }
  if (value->kind != Json::Kind::String) {
    addDiagnostic(diagnostics, source, childPath(path, key), "expected string");
    return std::nullopt;
  }
  return value->string;
}

[[nodiscard]] std::string readStringOr(const Json &object, const char *key,
                                       std::vector<Diagnostic> &diagnostics,
                                       const std::filesystem::path &source,
                                       const std::string &path, const std::string_view fallback) {
  const std::optional<std::string> value = readString(object, key, diagnostics, source, path, false);
  return value.value_or(std::string(fallback));
}

[[nodiscard]] float readFloatOr(const Json &object, const char *key,
                                std::vector<Diagnostic> &diagnostics,
                                const std::filesystem::path &source, const std::string &path,
                                const float fallback) {
  const Json *value = member(object, key);
  if (value == nullptr) {
    return fallback;
  }
  if (value->kind != Json::Kind::Number || !std::isfinite(value->number)) {
    addDiagnostic(diagnostics, source, childPath(path, key), "expected finite number");
    return fallback;
  }
  return static_cast<float>(value->number);
}

[[nodiscard]] int readIntOr(const Json &object, const char *key,
                            std::vector<Diagnostic> &diagnostics,
                            const std::filesystem::path &source, const std::string &path,
                            const int fallback) {
  const Json *value = member(object, key);
  if (value == nullptr) {
    return fallback;
  }
  if (value->kind != Json::Kind::Number || !std::isfinite(value->number)) {
    addDiagnostic(diagnostics, source, childPath(path, key), "expected finite integer");
    return fallback;
  }
  return static_cast<int>(value->number);
}

[[nodiscard]] bool readBoolOr(const Json &object, const char *key,
                              std::vector<Diagnostic> &diagnostics,
                              const std::filesystem::path &source, const std::string &path,
                              const bool fallback) {
  const Json *value = member(object, key);
  if (value == nullptr) {
    return fallback;
  }
  if (value->kind != Json::Kind::Bool) {
    addDiagnostic(diagnostics, source, childPath(path, key), "expected boolean");
    return fallback;
  }
  return value->boolean;
}

[[nodiscard]] std::uint32_t readSchemaVersion(const Json &root,
                                              std::vector<Diagnostic> &diagnostics,
                                              const std::filesystem::path &source) {
  const int version = readIntOr(root, "schema_version", diagnostics, source, "$", 0);
  if (version != static_cast<int>(kAuthoringSchemaVersion)) {
    addDiagnostic(diagnostics, source, "$.schema_version",
                  "unsupported authoring schema version " + std::to_string(version));
  }
  return static_cast<std::uint32_t>(std::max(version, 0));
}

[[nodiscard]] Vec3 readVec3Or(const Json &object, const char *key,
                              std::vector<Diagnostic> &diagnostics,
                              const std::filesystem::path &source, const std::string &path,
                              const Vec3 fallback) {
  const Json *value = member(object, key);
  if (value == nullptr) {
    return fallback;
  }
  const std::string value_path = childPath(path, key);
  if (value->kind != Json::Kind::Array || value->array.size() != 3u) {
    addDiagnostic(diagnostics, source, value_path, "expected numeric vec3 array");
    return fallback;
  }
  Vec3 out{};
  float *components[3] = {&out.x, &out.y, &out.z};
  for (std::size_t i = 0; i < 3u; ++i) {
    if (value->array[i].kind != Json::Kind::Number || !std::isfinite(value->array[i].number)) {
      addDiagnostic(diagnostics, source, indexPath(value_path, i), "expected finite number");
      return fallback;
    }
    *components[i] = static_cast<float>(value->array[i].number);
  }
  return out;
}

[[nodiscard]] std::vector<GameplayTag> readTags(const Json &object, const char *key,
                                                std::vector<Diagnostic> &diagnostics,
                                                const std::filesystem::path &source,
                                                const std::string &path) {
  std::vector<GameplayTag> tags;
  const Json *value = member(object, key);
  if (value == nullptr) {
    return tags;
  }
  const std::string value_path = childPath(path, key);
  if (value->kind != Json::Kind::Array) {
    addDiagnostic(diagnostics, source, value_path, "expected tag array");
    return tags;
  }
  for (std::size_t i = 0; i < value->array.size(); ++i) {
    const Json &entry = value->array[i];
    if (entry.kind != Json::Kind::String || entry.string.empty()) {
      addDiagnostic(diagnostics, source, indexPath(value_path, i), "expected non-empty tag string");
      continue;
    }
    tags.push_back({entry.string});
  }
  return tags;
}

[[nodiscard]] std::vector<std::string> readStringArray(const Json &object, const char *key,
                                                       std::vector<Diagnostic> &diagnostics,
                                                       const std::filesystem::path &source,
                                                       const std::string &path) {
  std::vector<std::string> values;
  const Json *value = member(object, key);
  if (value == nullptr) {
    return values;
  }
  const std::string value_path = childPath(path, key);
  if (value->kind != Json::Kind::Array) {
    addDiagnostic(diagnostics, source, value_path, "expected string array");
    return values;
  }
  for (std::size_t i = 0; i < value->array.size(); ++i) {
    const Json &entry = value->array[i];
    if (entry.kind != Json::Kind::String || entry.string.empty()) {
      addDiagnostic(diagnostics, source, indexPath(value_path, i), "expected non-empty string");
      continue;
    }
    values.push_back(entry.string);
  }
  return values;
}

[[nodiscard]] std::vector<Vec3> readVec3Array(const Json &object, const char *key,
                                              std::vector<Diagnostic> &diagnostics,
                                              const std::filesystem::path &source,
                                              const std::string &path) {
  std::vector<Vec3> values;
  const Json *value = member(object, key);
  if (value == nullptr) {
    return values;
  }
  const std::string value_path = childPath(path, key);
  if (value->kind != Json::Kind::Array) {
    addDiagnostic(diagnostics, source, value_path, "expected vec3 array");
    return values;
  }
  for (std::size_t i = 0; i < value->array.size(); ++i) {
    const Json &entry = value->array[i];
    const std::string entry_path = indexPath(value_path, i);
    if (entry.kind != Json::Kind::Array || entry.array.size() != 3u) {
      addDiagnostic(diagnostics, source, entry_path, "expected numeric vec3 array");
      continue;
    }
    Vec3 point{};
    float *components[3] = {&point.x, &point.y, &point.z};
    bool valid = true;
    for (std::size_t component = 0; component < 3u; ++component) {
      if (entry.array[component].kind != Json::Kind::Number ||
          !std::isfinite(entry.array[component].number)) {
        addDiagnostic(diagnostics, source, indexPath(entry_path, component),
                      "expected finite number");
        valid = false;
        break;
      }
      *components[component] = static_cast<float>(entry.array[component].number);
    }
    if (valid) {
      values.push_back(point);
    }
  }
  return values;
}

[[nodiscard]] std::string scalarToString(const Json &value) {
  switch (value.kind) {
  case Json::Kind::Bool:
    return value.boolean ? "true" : "false";
  case Json::Kind::Number: {
    std::ostringstream out;
    out << value.number;
    return out.str();
  }
  case Json::Kind::String:
    return value.string;
  case Json::Kind::Null:
  case Json::Kind::Array:
  case Json::Kind::Object:
    return {};
  }
  return {};
}

[[nodiscard]] std::map<std::string, std::string>
readStringMap(const Json &object, const char *key, std::vector<Diagnostic> &diagnostics,
              const std::filesystem::path &source, const std::string &path) {
  std::map<std::string, std::string> values;
  const Json *map_json = member(object, key);
  if (map_json == nullptr) {
    return values;
  }
  const std::string map_path = childPath(path, key);
  if (map_json->kind != Json::Kind::Object) {
    addDiagnostic(diagnostics, source, map_path, "expected object");
    return values;
  }
  for (const auto &[entry_key, entry_value] : map_json->object) {
    const std::string value = scalarToString(entry_value);
    if (value.empty() && entry_value.kind != Json::Kind::String) {
      addDiagnostic(diagnostics, source, childPath(map_path, entry_key),
                    "expected scalar string, number, or boolean");
      continue;
    }
    values.emplace(entry_key, value);
  }
  return values;
}

[[nodiscard]] std::map<std::string, std::filesystem::path>
readPathMap(const Json &object, const char *key, std::vector<Diagnostic> &diagnostics,
            const std::filesystem::path &source, const std::string &path) {
  std::map<std::string, std::filesystem::path> values;
  const Json *map_json = member(object, key);
  if (map_json == nullptr) {
    return values;
  }
  const std::string map_path = childPath(path, key);
  if (map_json->kind != Json::Kind::Object) {
    addDiagnostic(diagnostics, source, map_path, "expected object");
    return values;
  }
  for (const auto &[entry_key, entry_value] : map_json->object) {
    if (entry_value.kind != Json::Kind::String || entry_value.string.empty()) {
      addDiagnostic(diagnostics, source, childPath(map_path, entry_key),
                    "expected non-empty path string");
      continue;
    }
    values.emplace(entry_key, std::filesystem::path(entry_value.string));
  }
  return values;
}

void hashBytes(std::uint64_t &hash, const void *data, const std::size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t i = 0u; i < size; ++i) {
    hash ^= static_cast<std::uint64_t>(bytes[i]);
    hash *= 1099511628211ull;
  }
}

void hashString(std::uint64_t &hash, const std::string_view value) {
  hashBytes(hash, value.data(), value.size());
  const unsigned char separator = 0xffu;
  hashBytes(hash, &separator, 1u);
}

void hashNumber(std::uint64_t &hash, const std::uint64_t value) {
  hashBytes(hash, &value, sizeof(value));
}

[[nodiscard]] std::uint64_t beginContractHash() {
  return 1469598103934665603ull;
}

[[nodiscard]] std::uint64_t actionEventStamp(const ActionGraphDocument &graph,
                                             const ActionNode &node,
                                             const ActionContext &context) {
  std::uint64_t hash = beginContractHash();
  hashString(hash, "aster.action_event.v1");
  hashString(hash, graph.id);
  hashString(hash, node.id);
  hashString(hash, node.type);
  hashString(hash, context.actor);
  hashString(hash, context.target);
  hashString(hash, context.input);
  for (const auto &[key, value] : node.parameters) {
    hashString(hash, key);
    hashString(hash, value);
  }
  for (const GameplayTag &tag : node.tags) {
    hashString(hash, tag.value);
  }
  return hash;
}

[[nodiscard]] CaveSeedRecord parseCaveSeedRecord(const Json &value,
                                                 std::vector<Diagnostic> &diagnostics,
                                                 const std::filesystem::path &source,
                                                 const std::string &path) {
  CaveSeedRecord out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.value = static_cast<std::uint32_t>(readIntOr(value, "value", diagnostics, source, path, 0));
  return out;
}

[[nodiscard]] CaveTunnelProfileDocument parseCaveTunnelProfileDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveTunnelProfileDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.seed = static_cast<std::uint32_t>(readIntOr(value, "seed", diagnostics, source, path, 1));
  out.start = readVec3Or(value, "start", diagnostics, source, path, {});
  out.control = readVec3Or(value, "control", diagnostics, source, path, {});
  out.control_b = readVec3Or(value, "control_b", diagnostics, source, path, {});
  out.end = readVec3Or(value, "end", diagnostics, source, path, {});
  out.floor_relative = readBoolOr(value, "floor_relative", diagnostics, source, path, false);
  out.length_segments = readIntOr(value, "length_segments", diagnostics, source, path, 64);
  out.radial_segments = readIntOr(value, "radial_segments", diagnostics, source, path, 18);
  out.half_width = readFloatOr(value, "half_width", diagnostics, source, path, out.half_width);
  out.wall_height = readFloatOr(value, "wall_height", diagnostics, source, path, out.wall_height);
  out.floor_width = readFloatOr(value, "floor_width", diagnostics, source, path, out.floor_width);
  out.floor_crown = readFloatOr(value, "floor_crown", diagnostics, source, path, out.floor_crown);
  out.floor_edge_raise =
      readFloatOr(value, "floor_edge_raise", diagnostics, source, path, out.floor_edge_raise);
  out.wall_noise = readFloatOr(value, "wall_noise", diagnostics, source, path, out.wall_noise);
  out.visible_wall_start_t = readFloatOr(value, "visible_wall_start_t", diagnostics, source, path,
                                         out.visible_wall_start_t);
  out.collision_start_t =
      readFloatOr(value, "collision_start_t", diagnostics, source, path, out.collision_start_t);
  out.collision_end_t =
      readFloatOr(value, "collision_end_t", diagnostics, source, path, out.collision_end_t);
  out.ore_start_t = readFloatOr(value, "ore_start_t", diagnostics, source, path, out.ore_start_t);
  out.chest_t = readFloatOr(value, "chest_t", diagnostics, source, path, out.chest_t);
  out.chamber_t = readFloatOr(value, "chamber_t", diagnostics, source, path, out.chamber_t);
  out.chamber_falloff =
      readFloatOr(value, "chamber_falloff", diagnostics, source, path, out.chamber_falloff);
  out.chamber_width_scale = readFloatOr(value, "chamber_width_scale", diagnostics, source, path,
                                        out.chamber_width_scale);
  out.chamber_height_scale = readFloatOr(value, "chamber_height_scale", diagnostics, source, path,
                                         out.chamber_height_scale);
  out.end_constraint_enabled = readBoolOr(value, "end_constraint_enabled", diagnostics, source,
                                           path, out.end_constraint_enabled);
  return out;
}

[[nodiscard]] CavePortalProfileDocument parseCavePortalProfileDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePortalProfileDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.arch_segments = readIntOr(value, "arch_segments", diagnostics, source, path, out.arch_segments);
  out.inner_half_width =
      readFloatOr(value, "inner_half_width", diagnostics, source, path, out.inner_half_width);
  out.inner_height =
      readFloatOr(value, "inner_height", diagnostics, source, path, out.inner_height);
  out.outer_half_width =
      readFloatOr(value, "outer_half_width", diagnostics, source, path, out.outer_half_width);
  out.outer_height =
      readFloatOr(value, "outer_height", diagnostics, source, path, out.outer_height);
  out.depth = readFloatOr(value, "depth", diagnostics, source, path, out.depth);
  out.ground_lip = readFloatOr(value, "ground_lip", diagnostics, source, path, out.ground_lip);
  out.inner_lining_depth =
      readFloatOr(value, "inner_lining_depth", diagnostics, source, path, out.inner_lining_depth);
  out.lining_breakup =
      readFloatOr(value, "lining_breakup", diagnostics, source, path, out.lining_breakup);
  return out;
}

[[nodiscard]] CaveOreVeinProfileDocument parseCaveOreVeinProfileDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveOreVeinProfileDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.seed = static_cast<std::uint32_t>(readIntOr(value, "seed", diagnostics, source, path, 1));
  out.candidates = readIntOr(value, "candidates", diagnostics, source, path, out.candidates);
  out.max_nodes = readIntOr(value, "max_nodes", diagnostics, source, path, out.max_nodes);
  out.field_frequency_a =
      readFloatOr(value, "field_frequency_a", diagnostics, source, path, out.field_frequency_a);
  out.field_frequency_b =
      readFloatOr(value, "field_frequency_b", diagnostics, source, path, out.field_frequency_b);
  out.intersection_threshold_a = readFloatOr(value, "intersection_threshold_a", diagnostics,
                                             source, path, out.intersection_threshold_a);
  out.intersection_threshold_b = readFloatOr(value, "intersection_threshold_b", diagnostics,
                                             source, path, out.intersection_threshold_b);
  out.wall_inset = readFloatOr(value, "wall_inset", diagnostics, source, path, out.wall_inset);
  out.min_spacing = readFloatOr(value, "min_spacing", diagnostics, source, path, out.min_spacing);
  return out;
}

[[nodiscard]] CaveFeatureProfileDocument parseCaveFeatureProfileDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveFeatureProfileDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.seed = static_cast<std::uint32_t>(readIntOr(value, "seed", diagnostics, source, path, 1));
  out.candidates = readIntOr(value, "candidates", diagnostics, source, path, out.candidates);
  out.max_features =
      readIntOr(value, "max_features", diagnostics, source, path, out.max_features);
  out.start_t = readFloatOr(value, "start_t", diagnostics, source, path, out.start_t);
  out.end_t = readFloatOr(value, "end_t", diagnostics, source, path, out.end_t);
  out.min_spacing = readFloatOr(value, "min_spacing", diagnostics, source, path, out.min_spacing);
  out.wall_inset = readFloatOr(value, "wall_inset", diagnostics, source, path, out.wall_inset);
  out.ceiling_fraction =
      readFloatOr(value, "ceiling_fraction", diagnostics, source, path, out.ceiling_fraction);
  out.column_fraction =
      readFloatOr(value, "column_fraction", diagnostics, source, path, out.column_fraction);
  out.shelf_fraction =
      readFloatOr(value, "shelf_fraction", diagnostics, source, path, out.shelf_fraction);
  out.mineral_fraction =
      readFloatOr(value, "mineral_fraction", diagnostics, source, path, out.mineral_fraction);
  return out;
}

[[nodiscard]] CaveWallFixtureProfileDocument parseCaveWallFixtureProfileDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveWallFixtureProfileDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.start_t = readFloatOr(value, "start_t", diagnostics, source, path, out.start_t);
  out.end_t = readFloatOr(value, "end_t", diagnostics, source, path, out.end_t);
  out.target_spacing =
      readFloatOr(value, "target_spacing", diagnostics, source, path, out.target_spacing);
  out.max_count = readIntOr(value, "max_count", diagnostics, source, path, out.max_count);
  out.wall_side = readFloatOr(value, "wall_side", diagnostics, source, path, out.wall_side);
  out.mount_height =
      readFloatOr(value, "mount_height", diagnostics, source, path, out.mount_height);
  out.wall_inset = readFloatOr(value, "wall_inset", diagnostics, source, path, out.wall_inset);
  out.normal_up_bias =
      readFloatOr(value, "normal_up_bias", diagnostics, source, path, out.normal_up_bias);
  out.lens_offset = readFloatOr(value, "lens_offset", diagnostics, source, path, out.lens_offset);
  out.light_offset = readFloatOr(value, "light_offset", diagnostics, source, path, out.light_offset);
  out.light_color = readVec3Or(value, "light_color", diagnostics, source, path, out.light_color);
  return out;
}

[[nodiscard]] CaveSectionDocument parseCaveSectionDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveSectionDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.archetype = readStringOr(value, "archetype", diagnostics, source, path, {});
  out.terrain_cover_fit =
      readBoolOr(value, "terrain_cover_fit", diagnostics, source, path, out.terrain_cover_fit);
  out.derive_from_previous = readBoolOr(value, "derive_from_previous", diagnostics, source, path,
                                         out.derive_from_previous);
  out.contributes_entrance_light = readBoolOr(value, "contributes_entrance_light", diagnostics,
                                              source, path, out.contributes_entrance_light);
  const Json *tunnel = member(value, "tunnel");
  if (tunnel != nullptr) {
    out.tunnel = parseCaveTunnelProfileDocument(*tunnel, diagnostics, source,
                                                childPath(path, "tunnel"));
  }
  const Json *portal = member(value, "portal");
  if (portal != nullptr) {
    out.portal = parseCavePortalProfileDocument(*portal, diagnostics, source,
                                                childPath(path, "portal"));
  }
  const Json *ore = member(value, "ore");
  if (ore != nullptr) {
    out.ore = parseCaveOreVeinProfileDocument(*ore, diagnostics, source, childPath(path, "ore"));
  }
  const Json *features = member(value, "features");
  if (features != nullptr) {
    out.features = parseCaveFeatureProfileDocument(*features, diagnostics, source,
                                                    childPath(path, "features"));
  }
  const Json *fixtures = member(value, "fixtures");
  if (fixtures != nullptr) {
    const std::string fixtures_path = childPath(path, "fixtures");
    if (fixtures->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, fixtures_path, "expected fixture array");
    } else {
      for (std::size_t i = 0; i < fixtures->array.size(); ++i) {
        out.fixtures.push_back(parseCaveWallFixtureProfileDocument(
            fixtures->array[i], diagnostics, source, indexPath(fixtures_path, i)));
      }
    }
  }
  return out;
}

[[nodiscard]] CavePlacementDocument parseCavePlacementDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePlacementDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.kind = readStringOr(value, "kind", diagnostics, source, path, {});
  out.archetype = readStringOr(value, "archetype", diagnostics, source, path, {});
  out.prefab = readStringOr(value, "prefab", diagnostics, source, path, {});
  out.section = readStringOr(value, "section", diagnostics, source, path, {});
  out.position = readVec3Or(value, "position", diagnostics, source, path, {});
  out.rotation = readVec3Or(value, "rotation", diagnostics, source, path, {});
  out.scale = readVec3Or(value, "scale", diagnostics, source, path, {1.0f, 1.0f, 1.0f});
  out.floor_relative = readBoolOr(value, "floor_relative", diagnostics, source, path,
                                  out.floor_relative);
  return out;
}

[[nodiscard]] CaveRouteValidationDocument parseCaveRouteValidationDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveRouteValidationDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.points = readVec3Array(value, "points", diagnostics, source, path);
  out.max_segment_length = readFloatOr(value, "max_segment_length", diagnostics, source, path,
                                       out.max_segment_length);
  out.support_tolerance = readFloatOr(value, "support_tolerance", diagnostics, source, path,
                                      out.support_tolerance);
  return out;
}

[[nodiscard]] CaveVolumeValidationDocument parseCaveVolumeValidationDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveVolumeValidationDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.center = readVec3Or(value, "center", diagnostics, source, path, {});
  out.half_extents =
      readVec3Or(value, "half_extents", diagnostics, source, path, {0.5f, 0.5f, 0.5f});
  return out;
}

[[nodiscard]] CaveCameraValidationDocument parseCaveCameraValidationDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveCameraValidationDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.camera = readVec3Or(value, "camera", diagnostics, source, path, {});
  out.target = readVec3Or(value, "target", diagnostics, source, path, {});
  out.min_clearance =
      readFloatOr(value, "min_clearance", diagnostics, source, path, out.min_clearance);
  return out;
}

[[nodiscard]] CaveProbeAgentDocument parseCaveProbeAgentDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveProbeAgentDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.seed = static_cast<std::uint32_t>(readIntOr(value, "seed", diagnostics, source, path, 1));
  out.step_count = readIntOr(value, "step_count", diagnostics, source, path, out.step_count);
  out.step_length = readFloatOr(value, "step_length", diagnostics, source, path, out.step_length);
  out.entry_anchor = readStringOr(value, "entry_anchor", diagnostics, source, path, {});
  return out;
}

[[nodiscard]] CaveWorldProbeDocument parseCaveWorldProbeDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveWorldProbeDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.kind = readStringOr(value, "kind", diagnostics, source, path, {});
  out.position = readVec3Or(value, "position", diagnostics, source, path, {});
  out.radius = readFloatOr(value, "radius", diagnostics, source, path, out.radius);
  out.minimum_count = readIntOr(value, "minimum_count", diagnostics, source, path, out.minimum_count);
  out.minimum_budget =
      readFloatOr(value, "minimum_budget", diagnostics, source, path, out.minimum_budget);
  out.maximum_budget =
      readFloatOr(value, "maximum_budget", diagnostics, source, path, out.maximum_budget);
  return out;
}

[[nodiscard]] CavePerceptualBudgetDocument parseCavePerceptualBudgetDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePerceptualBudgetDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_salience =
      readFloatOr(value, "minimum_salience", diagnostics, source, path, out.minimum_salience);
  return out;
}

[[nodiscard]] CaveReactionPackageDocument parseCaveReactionPackageDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveReactionPackageDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.action = readStringOr(value, "action", diagnostics, source, path, {});
  out.minimum_score = readFloatOr(value, "minimum_score", diagnostics, source, path, 0.0f);
  out.required_channels =
      readStringArray(value, "required_channels", diagnostics, source, path);
  return out;
}

[[nodiscard]] CavePerceptualContinuityBudgetDocument
parseCavePerceptualContinuityBudgetDocument(const Json &value,
                                            std::vector<Diagnostic> &diagnostics,
                                            const std::filesystem::path &source,
                                            const std::string &path) {
  CavePerceptualContinuityBudgetDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_score =
      readFloatOr(value, "minimum_score", diagnostics, source, path, out.minimum_score);
  out.required_channels =
      readStringArray(value, "required_channels", diagnostics, source, path);
  const Json *reaction_packages = member(value, "reaction_packages");
  if (reaction_packages != nullptr) {
    if (reaction_packages->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "reaction_packages"),
                    "expected reaction package array");
    } else {
      for (std::size_t i = 0; i < reaction_packages->array.size(); ++i) {
        out.reaction_packages.push_back(parseCaveReactionPackageDocument(
            reaction_packages->array[i], diagnostics, source,
            indexPath(childPath(path, "reaction_packages"), i)));
      }
    }
  }
  return out;
}

[[nodiscard]] CavePerceptionLedgerCellDocument parseCavePerceptionLedgerCellDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePerceptionLedgerCellDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_score = readFloatOr(value, "minimum_score", diagnostics, source, path, 0.0f);
  out.required_channels =
      readStringArray(value, "required_channels", diagnostics, source, path);
  return out;
}

[[nodiscard]] CavePerceptionLedgerDocument parseCavePerceptionLedgerDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePerceptionLedgerDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_score =
      readFloatOr(value, "minimum_score", diagnostics, source, path, out.minimum_score);
  out.required_channels =
      readStringArray(value, "required_channels", diagnostics, source, path);
  const Json *cells = member(value, "cells");
  if (cells != nullptr) {
    if (cells->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "cells"),
                    "expected perception ledger cell array");
    } else {
      for (std::size_t i = 0; i < cells->array.size(); ++i) {
        out.cells.push_back(parseCavePerceptionLedgerCellDocument(
            cells->array[i], diagnostics, source, indexPath(childPath(path, "cells"), i)));
      }
    }
  }
  return out;
}

[[nodiscard]] CavePerceptualRuntimeDocument parseCavePerceptualRuntimeDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CavePerceptualRuntimeDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.exposure_horizon_seconds = readFloatOr(value, "exposure_horizon_seconds", diagnostics,
                                             source, path, out.exposure_horizon_seconds);
  out.minimum_continuity_score = readFloatOr(value, "minimum_continuity_score", diagnostics,
                                             source, path, out.minimum_continuity_score);
  out.minimum_occlusion_trust = readFloatOr(value, "minimum_occlusion_trust", diagnostics,
                                            source, path, out.minimum_occlusion_trust);
  out.minimum_lighting_believability =
      readFloatOr(value, "minimum_lighting_believability", diagnostics, source, path,
                  out.minimum_lighting_believability);
  out.minimum_player_readable_cause =
      readFloatOr(value, "minimum_player_readable_cause", diagnostics, source, path,
                  out.minimum_player_readable_cause);
  return out;
}

[[nodiscard]] CavePerceptualCausalityGraphDocument
parseCavePerceptualCausalityGraphDocument(const Json &value,
                                          std::vector<Diagnostic> &diagnostics,
                                          const std::filesystem::path &source,
                                          const std::string &path) {
  CavePerceptualCausalityGraphDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_decision_impact =
      readFloatOr(value, "minimum_decision_impact", diagnostics, source, path,
                  out.minimum_decision_impact);
  out.required_causal_edges =
      readStringArray(value, "required_causal_edges", diagnostics, source, path);
  out.required_decision_channels =
      readStringArray(value, "required_decision_channels", diagnostics, source, path);
  out.required_player_readable_causes =
      readStringArray(value, "required_player_readable_causes", diagnostics, source, path);
  return out;
}

[[nodiscard]] CaveBeliefContractDocument parseCaveBeliefContractDocument(
    const Json &value, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveBeliefContractDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.minimum_score =
      readFloatOr(value, "minimum_score", diagnostics, source, path, out.minimum_score);
  out.required_checks = readStringArray(value, "required_checks", diagnostics, source, path);
  return out;
}

[[nodiscard]] CaveValidationDocument parseCaveValidationDocument(
    const Json &root, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveValidationDocument out;
  const Json *routes = member(root, "walkable_routes");
  if (routes != nullptr) {
    if (routes->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "walkable_routes"),
                    "expected route array");
    } else {
      for (std::size_t i = 0; i < routes->array.size(); ++i) {
        out.walkable_routes.push_back(parseCaveRouteValidationDocument(
            routes->array[i], diagnostics, source, indexPath(childPath(path, "walkable_routes"), i)));
      }
    }
  }
  const Json *spawn = member(root, "spawn_volumes");
  if (spawn != nullptr) {
    if (spawn->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "spawn_volumes"),
                    "expected volume array");
    } else {
      for (std::size_t i = 0; i < spawn->array.size(); ++i) {
        out.spawn_volumes.push_back(parseCaveVolumeValidationDocument(
            spawn->array[i], diagnostics, source, indexPath(childPath(path, "spawn_volumes"), i)));
      }
    }
  }
  const Json *collision = member(root, "collision_volumes");
  if (collision != nullptr) {
    if (collision->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "collision_volumes"),
                    "expected volume array");
    } else {
      for (std::size_t i = 0; i < collision->array.size(); ++i) {
        out.collision_volumes.push_back(parseCaveVolumeValidationDocument(
            collision->array[i], diagnostics, source,
            indexPath(childPath(path, "collision_volumes"), i)));
      }
    }
  }
  const Json *camera = member(root, "camera_probes");
  if (camera != nullptr) {
    if (camera->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "camera_probes"),
                    "expected camera probe array");
    } else {
      for (std::size_t i = 0; i < camera->array.size(); ++i) {
        out.camera_probes.push_back(parseCaveCameraValidationDocument(
            camera->array[i], diagnostics, source, indexPath(childPath(path, "camera_probes"), i)));
      }
    }
  }
  if (const Json *probe_agent = member(root, "probe_agent")) {
    out.probe_agent =
        parseCaveProbeAgentDocument(*probe_agent, diagnostics, source, childPath(path, "probe_agent"));
  }
  const Json *resource_probes = member(root, "resource_probes");
  if (resource_probes != nullptr) {
    if (resource_probes->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "resource_probes"),
                    "expected resource probe array");
    } else {
      for (std::size_t i = 0; i < resource_probes->array.size(); ++i) {
        out.resource_probes.push_back(parseCaveWorldProbeDocument(
            resource_probes->array[i], diagnostics, source,
            indexPath(childPath(path, "resource_probes"), i)));
      }
    }
  }
  const Json *encounter_probes = member(root, "encounter_probes");
  if (encounter_probes != nullptr) {
    if (encounter_probes->kind != Json::Kind::Array) {
      addDiagnostic(diagnostics, source, childPath(path, "encounter_probes"),
                    "expected encounter probe array");
    } else {
      for (std::size_t i = 0; i < encounter_probes->array.size(); ++i) {
        out.encounter_probes.push_back(parseCaveWorldProbeDocument(
            encounter_probes->array[i], diagnostics, source,
            indexPath(childPath(path, "encounter_probes"), i)));
      }
    }
  }
  if (const Json *perceptual = member(root, "perceptual_budget")) {
    out.perceptual_budget = parseCavePerceptualBudgetDocument(
        *perceptual, diagnostics, source, childPath(path, "perceptual_budget"));
  }
  if (const Json *continuity = member(root, "perceptual_continuity_budget")) {
    out.perceptual_continuity_budget = parseCavePerceptualContinuityBudgetDocument(
        *continuity, diagnostics, source, childPath(path, "perceptual_continuity_budget"));
  }
  if (const Json *ledger = member(root, "perception_ledger")) {
    out.perception_ledger = parseCavePerceptionLedgerDocument(
        *ledger, diagnostics, source, childPath(path, "perception_ledger"));
  }
  if (const Json *runtime = member(root, "perceptual_runtime")) {
    out.perceptual_runtime = parseCavePerceptualRuntimeDocument(
        *runtime, diagnostics, source, childPath(path, "perceptual_runtime"));
  }
  if (const Json *causality = member(root, "perceptual_causality_graph")) {
    out.perceptual_causality_graph = parseCavePerceptualCausalityGraphDocument(
        *causality, diagnostics, source, childPath(path, "perceptual_causality_graph"));
  }
  if (const Json *belief = member(root, "belief_contract")) {
    out.belief_contract = parseCaveBeliefContractDocument(
        *belief, diagnostics, source, childPath(path, "belief_contract"));
  }
  return out;
}

[[nodiscard]] ColliderShape parseColliderShape(const std::string_view value,
                                               std::vector<Diagnostic> &diagnostics,
                                               const std::filesystem::path &source,
                                               const std::string &path) {
  if (value == "box") {
    return ColliderShape::Box;
  }
  if (value == "sphere") {
    return ColliderShape::Sphere;
  }
  if (value == "capsule") {
    return ColliderShape::Capsule;
  }
  if (value == "mesh") {
    return ColliderShape::Mesh;
  }
  addDiagnostic(diagnostics, source, path, "unknown collider shape '" + std::string(value) + "'");
  return ColliderShape::Box;
}

[[nodiscard]] LightKind parseLightKind(const std::string_view value,
                                       std::vector<Diagnostic> &diagnostics,
                                       const std::filesystem::path &source,
                                       const std::string &path) {
  if (value == "point") {
    return LightKind::Point;
  }
  if (value == "directional") {
    return LightKind::Directional;
  }
  if (value == "spot") {
    return LightKind::Spot;
  }
  addDiagnostic(diagnostics, source, path, "unknown light kind '" + std::string(value) + "'");
  return LightKind::Point;
}

[[nodiscard]] RigidBodyType parseRigidBodyType(const std::string_view value,
                                               std::vector<Diagnostic> &diagnostics,
                                               const std::filesystem::path &source,
                                               const std::string &path) {
  if (value == "static") {
    return RigidBodyType::Static;
  }
  if (value == "dynamic") {
    return RigidBodyType::Dynamic;
  }
  if (value == "kinematic") {
    return RigidBodyType::Kinematic;
  }
  addDiagnostic(diagnostics, source, path,
                "unknown rigid body type '" + std::string(value) + "'");
  return RigidBodyType::Static;
}

[[nodiscard]] TransformComponent parseTransformComponent(const Json &component,
                                                         std::vector<Diagnostic> &diagnostics,
                                                         const std::filesystem::path &source,
                                                         const std::string &path) {
  TransformComponent out;
  out.local.translation = readVec3Or(component, "translation", diagnostics, source, path, {});
  out.local.rotation = readVec3Or(component, "rotation", diagnostics, source, path, {});
  out.local.scale = readVec3Or(component, "scale", diagnostics, source, path, {1.0f, 1.0f, 1.0f});
  return out;
}

[[nodiscard]] MeshRendererComponent parseMeshRendererComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  MeshRendererComponent out;
  out.mesh = readString(component, "mesh", diagnostics, source, path, true).value_or("");
  out.material = readString(component, "material", diagnostics, source, path, true).value_or("");
  out.visible = readBoolOr(component, "visible", diagnostics, source, path, true);
  return out;
}

[[nodiscard]] PerceptualPlacementBindingComponent parsePerceptualPlacementBindingComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  PerceptualPlacementBindingComponent out;
  out.template_ref =
      readString(component, "template", diagnostics, source, path, true).value_or("");
  out.cell_anchor =
      readString(component, "cell_anchor", diagnostics, source, path, true).value_or("");
  out.residency_role = readStringOr(component, "residency_role", diagnostics, source, path, {});
  out.local_cause =
      readString(component, "local_cause", diagnostics, source, path, true).value_or("");
  out.traversal_role = readStringOr(component, "traversal_role", diagnostics, source, path, {});
  out.visual_occlusion_role =
      readStringOr(component, "visual_occlusion_role", diagnostics, source, path, {});
  out.acoustic_role = readStringOr(component, "acoustic_role", diagnostics, source, path, {});
  out.material_half_life_override =
      readFloatOr(component, "material_half_life_override", diagnostics, source, path, 0.0f);
  out.streaming_cost_override =
      readFloatOr(component, "streaming_cost_override", diagnostics, source, path, 0.0f);
  return out;
}

[[nodiscard]] ColliderComponent parseColliderComponent(const Json &component,
                                                       std::vector<Diagnostic> &diagnostics,
                                                       const std::filesystem::path &source,
                                                       const std::string &path) {
  ColliderComponent out;
  const std::string shape = readStringOr(component, "shape", diagnostics, source, path, "box");
  out.shape = parseColliderShape(shape, diagnostics, source, childPath(path, "shape"));
  out.half_extents =
      readVec3Or(component, "half_extents", diagnostics, source, path, {0.5f, 0.5f, 0.5f});
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 0.5f);
  out.height = readFloatOr(component, "height", diagnostics, source, path, 1.0f);
  out.trigger = readBoolOr(component, "trigger", diagnostics, source, path, false);
  out.layer = readStringOr(component, "layer", diagnostics, source, path, {});
  out.mask = readStringOr(component, "mask", diagnostics, source, path, {});
  return out;
}

[[nodiscard]] RigidBodyComponent parseRigidBodyComponent(const Json &component,
                                                         std::vector<Diagnostic> &diagnostics,
                                                         const std::filesystem::path &source,
                                                         const std::string &path) {
  RigidBodyComponent out;
  const std::string type = readStringOr(component, "type", diagnostics, source, path, "static");
  out.type = parseRigidBodyType(type, diagnostics, source, childPath(path, "type"));
  out.mass = readFloatOr(component, "mass", diagnostics, source, path, 1.0f);
  out.linear_damping = readFloatOr(component, "linear_damping", diagnostics, source, path, 0.08f);
  out.angular_damping = readFloatOr(component, "angular_damping", diagnostics, source, path, 0.10f);
  out.gravity_enabled = readBoolOr(component, "gravity_enabled", diagnostics, source, path, true);
  out.ccd_enabled = readBoolOr(component, "ccd_enabled", diagnostics, source, path, false);
  out.allow_sleep = readBoolOr(component, "allow_sleep", diagnostics, source, path, true);
  out.center_of_mass = readVec3Or(component, "center_of_mass", diagnostics, source, path, {});
  out.inertia_scale =
      readVec3Or(component, "inertia_scale", diagnostics, source, path, {1.0f, 1.0f, 1.0f});
  out.lock_linear_axes = readVec3Or(component, "lock_linear_axes", diagnostics, source, path, {});
  out.lock_angular_axes = readVec3Or(component, "lock_angular_axes", diagnostics, source, path, {});
  out.solver_iterations = readIntOr(component, "solver_iterations", diagnostics, source, path, 0);
  return out;
}

[[nodiscard]] LightComponent parseLightComponent(const Json &component,
                                                 std::vector<Diagnostic> &diagnostics,
                                                 const std::filesystem::path &source,
                                                 const std::string &path) {
  LightComponent out;
  const std::string kind = readStringOr(component, "kind", diagnostics, source, path, "point");
  out.kind = parseLightKind(kind, diagnostics, source, childPath(path, "kind"));
  out.color = readVec3Or(component, "color", diagnostics, source, path, {1.0f, 1.0f, 1.0f});
  out.intensity = readFloatOr(component, "intensity", diagnostics, source, path, 1.0f);
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 1.0f);
  return out;
}

[[nodiscard]] InteractableComponent parseInteractableComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  InteractableComponent out;
  out.tags = readTags(component, "tags", diagnostics, source, path);
  out.action_graph =
      readString(component, "action_graph", diagnostics, source, path, true).value_or("");
  out.prompt_action = readStringOr(component, "prompt_action", diagnostics, source, path, {});
  out.prompt_subject = readStringOr(component, "prompt_subject", diagnostics, source, path, {});
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 0.35f);
  out.max_distance = readFloatOr(component, "max_distance", diagnostics, source, path, 8.0f);
  return out;
}

[[nodiscard]] InventoryComponent parseInventoryComponent(const Json &component,
                                                         std::vector<Diagnostic> &diagnostics,
                                                         const std::filesystem::path &source,
                                                         const std::string &path) {
  InventoryComponent out;
  out.slot_count = readIntOr(component, "slot_count", diagnostics, source, path, 0);
  const Json *slots = member(component, "slots");
  if (slots == nullptr) {
    return out;
  }
  const std::string slots_path = childPath(path, "slots");
  if (slots->kind != Json::Kind::Array) {
    addDiagnostic(diagnostics, source, slots_path, "expected slot array");
    return out;
  }
  for (std::size_t i = 0; i < slots->array.size(); ++i) {
    const std::string slot_path = indexPath(slots_path, i);
    const Json &slot = slots->array[i];
    if (!expectObject(slot, diagnostics, source, slot_path)) {
      continue;
    }
    out.slots.push_back({readString(slot, "item", diagnostics, source, slot_path, true).value_or(""),
                         readIntOr(slot, "quantity", diagnostics, source, slot_path, 0)});
  }
  return out;
}

[[nodiscard]] CameraComponent parseCameraComponent(const Json &component,
                                                   std::vector<Diagnostic> &diagnostics,
                                                   const std::filesystem::path &source,
                                                   const std::string &path) {
  CameraComponent out;
  out.primary = readBoolOr(component, "primary", diagnostics, source, path, false);
  out.vertical_fov_degrees =
      readFloatOr(component, "vertical_fov_degrees", diagnostics, source, path, 60.0f);
  out.near_plane = readFloatOr(component, "near_plane", diagnostics, source, path, 0.01f);
  out.far_plane = readFloatOr(component, "far_plane", diagnostics, source, path, 1000.0f);
  return out;
}

[[nodiscard]] CaveSceneComponent parseCaveSceneComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveSceneComponent out;
  out.cave = readString(component, "cave", diagnostics, source, path, true).value_or("");
  out.section = readStringOr(component, "section", diagnostics, source, path, {});
  return out;
}

[[nodiscard]] FixtureComponent parseFixtureComponent(const Json &component,
                                                     std::vector<Diagnostic> &diagnostics,
                                                     const std::filesystem::path &source,
                                                     const std::string &path) {
  FixtureComponent out;
  out.archetype = readStringOr(component, "archetype", diagnostics, source, path, {});
  out.socket = readStringOr(component, "socket", diagnostics, source, path, {});
  return out;
}

[[nodiscard]] OreNodeComponent parseOreNodeComponent(const Json &component,
                                                     std::vector<Diagnostic> &diagnostics,
                                                     const std::filesystem::path &source,
                                                     const std::string &path) {
  OreNodeComponent out;
  out.resource_item =
      readStringOr(component, "resource_item", diagnostics, source, path, {});
  out.health = readIntOr(component, "health", diagnostics, source, path, 1);
  out.yield_quantity = readIntOr(component, "yield_quantity", diagnostics, source, path, 1);
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 0.25f);
  return out;
}

[[nodiscard]] TorchSocketComponent parseTorchSocketComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  TorchSocketComponent out;
  out.color = readVec3Or(component, "color", diagnostics, source, path, {1.0f, 0.5f, 0.2f});
  out.intensity = readFloatOr(component, "intensity", diagnostics, source, path, 1.0f);
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 1.0f);
  return out;
}

[[nodiscard]] SpawnPointComponent parseSpawnPointComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  SpawnPointComponent out;
  out.spawn_kind = readStringOr(component, "spawn_kind", diagnostics, source, path, {});
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 0.35f);
  out.half_extents =
      readVec3Or(component, "half_extents", diagnostics, source, path, {0.35f, 0.35f, 0.35f});
  return out;
}

[[nodiscard]] MiningComponent parseMiningComponent(const Json &component,
                                                   std::vector<Diagnostic> &diagnostics,
                                                   const std::filesystem::path &source,
                                                   const std::string &path) {
  MiningComponent out;
  out.resource_item = readStringOr(component, "resource_item", diagnostics, source, path, {});
  out.health = readIntOr(component, "health", diagnostics, source, path, 1);
  out.yield_quantity = readIntOr(component, "yield_quantity", diagnostics, source, path, 1);
  out.radius = readFloatOr(component, "radius", diagnostics, source, path, 0.35f);
  return out;
}

[[nodiscard]] CaveDebugComponent parseCaveDebugComponent(
    const Json &component, std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source,
    const std::string &path) {
  CaveDebugComponent out;
  out.layers = readTags(component, "layers", diagnostics, source, path);
  out.color = readVec3Or(component, "color", diagnostics, source, path, {1.0f, 1.0f, 1.0f});
  return out;
}

[[nodiscard]] ComponentSet parseComponents(const Json &components,
                                           std::vector<Diagnostic> &diagnostics,
                                           const std::filesystem::path &source,
                                           const std::string &path) {
  ComponentSet out;
  if (!expectObject(components, diagnostics, source, path)) {
    return out;
  }

  const std::set<std::string> known_components = {
      "transform",    "mesh_renderer", "perceptual_binding", "collider",   "rigid_body",
      "light",        "interactable",  "inventory",          "camera",     "cave_scene",
      "fixture",      "ore_node",      "torch_socket",       "spawn_point", "mining",
      "cave_debug"};
  for (const auto &[key, value] : components.object) {
    if (!known_components.contains(key)) {
      addDiagnostic(diagnostics, source, childPath(path, key), "unknown component type");
      continue;
    }
    if (!expectObject(value, diagnostics, source, childPath(path, key))) {
      continue;
    }
    if (key == "transform") {
      out.transform = parseTransformComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "mesh_renderer") {
      out.mesh_renderer =
          parseMeshRendererComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "perceptual_binding") {
      out.perceptual_binding =
          parsePerceptualPlacementBindingComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "collider") {
      out.collider = parseColliderComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "rigid_body") {
      out.rigid_body = parseRigidBodyComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "light") {
      out.light = parseLightComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "interactable") {
      out.interactable =
          parseInteractableComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "inventory") {
      out.inventory = parseInventoryComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "camera") {
      out.camera = parseCameraComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "cave_scene") {
      out.cave_scene = parseCaveSceneComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "fixture") {
      out.fixture = parseFixtureComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "ore_node") {
      out.ore_node = parseOreNodeComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "torch_socket") {
      out.torch_socket =
          parseTorchSocketComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "spawn_point") {
      out.spawn_point = parseSpawnPointComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "mining") {
      out.mining = parseMiningComponent(value, diagnostics, source, childPath(path, key));
    } else if (key == "cave_debug") {
      out.cave_debug = parseCaveDebugComponent(value, diagnostics, source, childPath(path, key));
    }
  }

  if (out.mesh_renderer.has_value() && !out.perceptual_binding.has_value()) {
    addDiagnostic(diagnostics, source, path,
                  "mesh_renderer requires perceptual_binding for strict world truth");
  }

  return out;
}

[[nodiscard]] EntityDefinition parseEntity(const Json &entity, std::vector<Diagnostic> &diagnostics,
                                           const std::filesystem::path &source,
                                           const std::string &path) {
  EntityDefinition out;
  if (!expectObject(entity, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(entity, "id", diagnostics, source, path, true).value_or("");
  out.name = readStringOr(entity, "name", diagnostics, source, path, {});
  out.parent = readStringOr(entity, "parent", diagnostics, source, path, {});
  const Json *components = member(entity, "components");
  if (components == nullptr) {
    addDiagnostic(diagnostics, source, childPath(path, "components"), "missing components object");
  } else {
    out.components = parseComponents(*components, diagnostics, source, childPath(path, "components"));
  }
  return out;
}

[[nodiscard]] std::vector<EntityDefinition> parseEntityArray(const Json &root,
                                                             std::vector<Diagnostic> &diagnostics,
                                                             const std::filesystem::path &source) {
  std::vector<EntityDefinition> entities;
  const Json *entities_json = member(root, "entities");
  if (entities_json == nullptr) {
    addDiagnostic(diagnostics, source, "$.entities", "missing required entity array");
    return entities;
  }
  if (entities_json->kind != Json::Kind::Array) {
    addDiagnostic(diagnostics, source, "$.entities", "expected entity array");
    return entities;
  }
  std::set<std::string> ids;
  for (std::size_t i = 0; i < entities_json->array.size(); ++i) {
    EntityDefinition entity =
        parseEntity(entities_json->array[i], diagnostics, source, indexPath("$.entities", i));
    if (!entity.id.empty() && !ids.insert(entity.id).second) {
      addDiagnostic(diagnostics, source, indexPath("$.entities", i) + ".id",
                    "duplicate entity id '" + entity.id + "'");
    }
    entities.push_back(std::move(entity));
  }
  return entities;
}

template <typename Document>
[[nodiscard]] LoadResult<Document> parseRoot(std::string_view source_text,
                                             const std::filesystem::path &source_path) {
  LoadResult<Document> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    result.value.entities = parseEntityArray(root, result.diagnostics, source_path);
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

[[nodiscard]] std::optional<std::string> readTextFile(const std::filesystem::path &path,
                                                      std::vector<Diagnostic> &diagnostics) {
  std::ifstream input(path);
  if (!input) {
    addDiagnostic(diagnostics, path, "$", "failed to open file");
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

template <typename Document, typename Parser>
[[nodiscard]] LoadResult<Document> loadDocument(const std::filesystem::path &path, Parser parser) {
  LoadResult<Document> result;
  std::vector<Diagnostic> io_diagnostics;
  const std::optional<std::string> source = readTextFile(path, io_diagnostics);
  if (!source.has_value()) {
    result.diagnostics = std::move(io_diagnostics);
    return result;
  }
  result = parser(*source, path);
  result.diagnostics.insert(result.diagnostics.begin(), io_diagnostics.begin(), io_diagnostics.end());
  return result;
}

} // namespace

AssetKind parseAssetKind(const std::string_view value) {
  if (value == "scene") {
    return AssetKind::Scene;
  }
  if (value == "prefab") {
    return AssetKind::Prefab;
  }
  if (value == "cave") {
    return AssetKind::Cave;
  }
  if (value == "material") {
    return AssetKind::Material;
  }
  if (value == "item") {
    return AssetKind::Item;
  }
  if (value == "action_graph") {
    return AssetKind::ActionGraph;
  }
  if (value == "input") {
    return AssetKind::InputMap;
  }
  if (value == "ui") {
    return AssetKind::Ui;
  }
  if (value == "mesh") {
    return AssetKind::Mesh;
  }
  if (value == "texture") {
    return AssetKind::Texture;
  }
  if (value == "asset_graph" || value == "procedural_asset" || value == "astergraph") {
    return AssetKind::AssetGraph;
  }
  if (value == "lesson" || value == "learning_lesson") {
    return AssetKind::Lesson;
  }
  if (value == "memory_policy") {
    return AssetKind::MemoryPolicy;
  }
  if (value == "memory_benchmark" || value == "memory_bench") {
    return AssetKind::MemoryBenchmark;
  }
  return AssetKind::Unknown;
}

std::string_view assetKindName(const AssetKind kind) {
  switch (kind) {
  case AssetKind::Scene:
    return "scene";
  case AssetKind::Prefab:
    return "prefab";
  case AssetKind::Cave:
    return "cave";
  case AssetKind::Material:
    return "material";
  case AssetKind::Item:
    return "item";
  case AssetKind::ActionGraph:
    return "action_graph";
  case AssetKind::InputMap:
    return "input";
  case AssetKind::Ui:
    return "ui";
  case AssetKind::Mesh:
    return "mesh";
  case AssetKind::Texture:
    return "texture";
  case AssetKind::AssetGraph:
    return "asset_graph";
  case AssetKind::Lesson:
    return "lesson";
  case AssetKind::MemoryPolicy:
    return "memory_policy";
  case AssetKind::MemoryBenchmark:
    return "memory_benchmark";
  case AssetKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

LoadResult<ProjectDocument> parseProjectDocument(std::string_view source_text,
                                                 std::filesystem::path source_path) {
  LoadResult<ProjectDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.name = readString(root, "name", result.diagnostics, source_path, "$", true)
                            .value_or("");
    result.value.startup_scene =
        readString(root, "startup_scene", result.diagnostics, source_path, "$", true).value_or("");

    const Json *assets = member(root, "assets");
    if (assets == nullptr) {
      addDiagnostic(result.diagnostics, source_path, "$.assets", "missing required asset array");
      return result;
    }
    if (assets->kind != Json::Kind::Array) {
      addDiagnostic(result.diagnostics, source_path, "$.assets", "expected asset array");
      return result;
    }
    std::set<std::string> asset_ids;
    bool startup_scene_found = false;
    for (std::size_t i = 0; i < assets->array.size(); ++i) {
      const Json &asset = assets->array[i];
      const std::string path = indexPath("$.assets", i);
      if (!expectObject(asset, result.diagnostics, source_path, path)) {
        continue;
      }
      ProjectAssetRef ref;
      ref.id = readString(asset, "id", result.diagnostics, source_path, path, true).value_or("");
      const std::string kind =
          readString(asset, "kind", result.diagnostics, source_path, path, true).value_or("");
      ref.kind = parseAssetKind(kind);
      if (ref.kind == AssetKind::Unknown) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "kind"),
                      "unknown asset kind '" + kind + "'");
      }
      ref.path =
          std::filesystem::path(readString(asset, "path", result.diagnostics, source_path, path, true)
                                    .value_or(""));
      if (!ref.id.empty() && !asset_ids.insert(ref.id).second) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "id"),
                      "duplicate asset id '" + ref.id + "'");
      }
      startup_scene_found =
          startup_scene_found || (ref.id == result.value.startup_scene && ref.kind == AssetKind::Scene);
      result.value.assets.push_back(std::move(ref));
    }
    if (!result.value.startup_scene.empty() && !startup_scene_found) {
      addDiagnostic(result.diagnostics, source_path, "$.startup_scene",
                    "startup scene must reference a scene asset id");
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<SceneDocument> parseSceneDocument(std::string_view source_text,
                                             std::filesystem::path source_path) {
  return parseRoot<SceneDocument>(source_text, source_path);
}

LoadResult<PrefabDocument> parsePrefabDocument(std::string_view source_text,
                                               std::filesystem::path source_path) {
  return parseRoot<PrefabDocument>(source_text, source_path);
}

LoadResult<CaveDocument> parseCaveDocument(std::string_view source_text,
                                           std::filesystem::path source_path) {
  LoadResult<CaveDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    const Json *seeds = member(root, "seeds");
    if (seeds != nullptr) {
      if (seeds->kind != Json::Kind::Array) {
        addDiagnostic(result.diagnostics, source_path, "$.seeds", "expected seed array");
      } else {
        std::set<std::string> seed_ids;
        for (std::size_t i = 0; i < seeds->array.size(); ++i) {
          CaveSeedRecord seed =
              parseCaveSeedRecord(seeds->array[i], result.diagnostics, source_path,
                                  indexPath("$.seeds", i));
          if (!seed.id.empty() && !seed_ids.insert(seed.id).second) {
            addDiagnostic(result.diagnostics, source_path, indexPath("$.seeds", i) + ".id",
                          "duplicate seed id '" + seed.id + "'");
          }
          result.value.seeds.push_back(std::move(seed));
        }
      }
    }
    result.value.required_assets =
        readStringArray(root, "required_assets", result.diagnostics, source_path, "$");
    const Json *sections = member(root, "sections");
    if (sections != nullptr) {
      if (sections->kind != Json::Kind::Array) {
        addDiagnostic(result.diagnostics, source_path, "$.sections", "expected section array");
      } else {
        std::set<std::string> ids;
        for (std::size_t i = 0; i < sections->array.size(); ++i) {
          CaveSectionDocument section = parseCaveSectionDocument(sections->array[i],
                                                                  result.diagnostics, source_path,
                                                                  indexPath("$.sections", i));
          if (!section.id.empty() && !ids.insert(section.id).second) {
            addDiagnostic(result.diagnostics, source_path, indexPath("$.sections", i) + ".id",
                          "duplicate section id '" + section.id + "'");
          }
          result.value.sections.push_back(std::move(section));
        }
      }
    }
    const Json *placements = member(root, "placements");
    if (placements != nullptr) {
      if (placements->kind != Json::Kind::Array) {
        addDiagnostic(result.diagnostics, source_path, "$.placements",
                      "expected placement array");
      } else {
        std::set<std::string> ids;
        for (std::size_t i = 0; i < placements->array.size(); ++i) {
          CavePlacementDocument placement = parseCavePlacementDocument(
              placements->array[i], result.diagnostics, source_path, indexPath("$.placements", i));
          if (!placement.id.empty() && !ids.insert(placement.id).second) {
            addDiagnostic(result.diagnostics, source_path, indexPath("$.placements", i) + ".id",
                          "duplicate placement id '" + placement.id + "'");
          }
          result.value.placements.push_back(std::move(placement));
        }
      }
    }
    const Json *validation = member(root, "validation");
    if (validation != nullptr) {
      if (validation->kind != Json::Kind::Object) {
        addDiagnostic(result.diagnostics, source_path, "$.validation", "expected object");
      } else {
        result.value.validation = parseCaveValidationDocument(*validation, result.diagnostics,
                                                              source_path, "$.validation");
      }
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<MaterialDocument> parseMaterialDocument(std::string_view source_text,
                                                   std::filesystem::path source_path) {
  LoadResult<MaterialDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    const Vec3 base_color =
        readVec3Or(root, "base_color", result.diagnostics, source_path, "$", {1.0f, 1.0f, 1.0f});
    result.value.base_color = LinearRgb{base_color.x, base_color.y, base_color.z};
    const Vec3 emission_color =
        readVec3Or(root, "emission_color", result.diagnostics, source_path, "$", {});
    result.value.emission_color =
        EmissionColor{emission_color.x, emission_color.y, emission_color.z};
    result.value.roughness = readFloatOr(root, "roughness", result.diagnostics, source_path, "$",
                                         result.value.roughness);
    result.value.metallic =
        readFloatOr(root, "metallic", result.diagnostics, source_path, "$", result.value.metallic);
    result.value.emission_strength = readFloatOr(root, "emission_strength", result.diagnostics,
                                                 source_path, "$", result.value.emission_strength);
    result.value.opacity =
        readFloatOr(root, "opacity", result.diagnostics, source_path, "$", result.value.opacity);
    result.value.double_sided =
        readBoolOr(root, "double_sided", result.diagnostics, source_path, "$",
                   result.value.double_sided);
    result.value.alpha_mode = readStringOr(root, "alpha_mode", result.diagnostics, source_path,
                                           "$", result.value.alpha_mode);
    result.value.depth_write = readStringOr(root, "depth_write", result.diagnostics, source_path,
                                             "$", result.value.depth_write);
    result.value.surface_pattern =
        readStringOr(root, "surface_pattern", result.diagnostics, source_path, "$",
                     result.value.surface_pattern);
    result.value.texture_slots = readPathMap(root, "textures", result.diagnostics, source_path, "$");
    result.value.compiler_hints = readStringMap(root, "compiler_hints", result.diagnostics,
                                                source_path, "$");
    result.value.tags = readTags(root, "tags", result.diagnostics, source_path, "$");
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<ItemDocument> parseItemDocument(std::string_view source_text,
                                           std::filesystem::path source_path) {
  LoadResult<ItemDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.display_name =
        readString(root, "display_name", result.diagnostics, source_path, "$", true).value_or("");
    result.value.short_label =
        readStringOr(root, "short_label", result.diagnostics, source_path, "$", {});
    result.value.stackable = readBoolOr(root, "stackable", result.diagnostics, source_path, "$",
                                        result.value.stackable);
    result.value.max_stack =
        readIntOr(root, "max_stack", result.diagnostics, source_path, "$", result.value.max_stack);
    result.value.icon = readStringOr(root, "icon", result.diagnostics, source_path, "$", {});
    result.value.pickup_prefab =
        readStringOr(root, "pickup_prefab", result.diagnostics, source_path, "$", {});
    result.value.use_action_graph =
        readStringOr(root, "use_action_graph", result.diagnostics, source_path, "$", {});
    result.value.equipment_slots =
        readStringArray(root, "equipment_slots", result.diagnostics, source_path, "$");
    result.value.tags = readTags(root, "tags", result.diagnostics, source_path, "$");
    result.value.metadata = readStringMap(root, "metadata", result.diagnostics, source_path, "$");
    if (!result.value.stackable && result.value.max_stack != 1) {
      addDiagnostic(result.diagnostics, source_path, "$.max_stack",
                    "non-stackable items must use max_stack 1");
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<ActionGraphDocument> parseActionGraphDocument(std::string_view source_text,
                                                         std::filesystem::path source_path) {
  LoadResult<ActionGraphDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    const Json *nodes = member(root, "nodes");
    if (nodes == nullptr) {
      addDiagnostic(result.diagnostics, source_path, "$.nodes", "missing required node array");
      return result;
    }
    if (nodes->kind != Json::Kind::Array) {
      addDiagnostic(result.diagnostics, source_path, "$.nodes", "expected node array");
      return result;
    }
    std::set<std::string> node_ids;
    for (std::size_t i = 0; i < nodes->array.size(); ++i) {
      const Json &node_json = nodes->array[i];
      const std::string path = indexPath("$.nodes", i);
      if (!expectObject(node_json, result.diagnostics, source_path, path)) {
        continue;
      }
      ActionNode node;
      node.id = readString(node_json, "id", result.diagnostics, source_path, path, true).value_or("");
      node.type =
          readString(node_json, "type", result.diagnostics, source_path, path, true).value_or("");
      node.parameters = readStringMap(node_json, "parameters", result.diagnostics, source_path, path);
      node.tags = readTags(node_json, "tags", result.diagnostics, source_path, path);
      if (!node.id.empty() && !node_ids.insert(node.id).second) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "id"),
                      "duplicate action node id '" + node.id + "'");
      }
      result.value.nodes.push_back(std::move(node));
    }
    const Json *reaction_contracts = member(root, "reaction_contracts");
    if (reaction_contracts != nullptr) {
      if (reaction_contracts->kind != Json::Kind::Array) {
        addDiagnostic(result.diagnostics, source_path, "$.reaction_contracts",
                      "expected reaction contract array");
      } else {
        std::set<std::string> contract_ids;
        for (std::size_t i = 0; i < reaction_contracts->array.size(); ++i) {
          const Json &contract_json = reaction_contracts->array[i];
          const std::string path = indexPath("$.reaction_contracts", i);
          if (!expectObject(contract_json, result.diagnostics, source_path, path)) {
            continue;
          }
          ActionReactionContractDocument contract;
          contract.id = readString(contract_json, "id", result.diagnostics, source_path, path, true)
                            .value_or("");
          contract.minimum_score =
              readFloatOr(contract_json, "minimum_score", result.diagnostics, source_path, path,
                          contract.minimum_score);
          contract.required_channels =
              readStringArray(contract_json, "required_channels", result.diagnostics,
                              source_path, path);
          contract.required_events =
              readStringArray(contract_json, "required_events", result.diagnostics,
                              source_path, path);
          if (!contract.id.empty() && !contract_ids.insert(contract.id).second) {
            addDiagnostic(result.diagnostics, source_path, childPath(path, "id"),
                          "duplicate reaction contract id '" + contract.id + "'");
          }
          result.value.reaction_contracts.push_back(std::move(contract));
        }
      }
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<InputMapDocument> parseInputMapDocument(std::string_view source_text,
                                                   std::filesystem::path source_path) {
  LoadResult<InputMapDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    const Json *bindings = member(root, "bindings");
    if (bindings == nullptr) {
      addDiagnostic(result.diagnostics, source_path, "$.bindings",
                    "missing required input binding array");
      return result;
    }
    if (bindings->kind != Json::Kind::Array) {
      addDiagnostic(result.diagnostics, source_path, "$.bindings", "expected input binding array");
      return result;
    }
    std::set<std::string> binding_keys;
    for (std::size_t i = 0u; i < bindings->array.size(); ++i) {
      const Json &binding_json = bindings->array[i];
      const std::string path = indexPath("$.bindings", i);
      if (!expectObject(binding_json, result.diagnostics, source_path, path)) {
        continue;
      }
      InputBindingDocument binding;
      binding.command =
          readString(binding_json, "command", result.diagnostics, source_path, path, true)
              .value_or("");
      binding.device =
          readString(binding_json, "device", result.diagnostics, source_path, path, true)
              .value_or("");
      binding.key = readStringOr(binding_json, "key", result.diagnostics, source_path, path, {});
      binding.button =
          readStringOr(binding_json, "button", result.diagnostics, source_path, path, {});
      binding.scale = readFloatOr(binding_json, "scale", result.diagnostics, source_path, path,
                                  binding.scale);
      binding.deadzone = readFloatOr(binding_json, "deadzone", result.diagnostics, source_path,
                                     path, binding.deadzone);
      binding.tags = readTags(binding_json, "tags", result.diagnostics, source_path, path);

      if (binding.device != "keyboard" && binding.device != "mouse" &&
          binding.device != "gamepad" && binding.device != "touch") {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "device"),
                      "input binding device must be keyboard, mouse, gamepad, or touch");
      }
      if ((binding.device == "keyboard" || binding.device == "gamepad") && binding.key.empty()) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "key"),
                      "keyboard and gamepad bindings require a key");
      }
      if (binding.device == "mouse" && binding.button.empty()) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "button"),
                      "mouse bindings require a button");
      }
      if (binding.deadzone < 0.0f || binding.deadzone > 1.0f) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "deadzone"),
                      "deadzone must be between 0 and 1");
      }
      if (binding.command.empty()) {
        addDiagnostic(result.diagnostics, source_path, childPath(path, "command"),
                      "input binding command must not be empty");
      }
      const std::string dedupe_key =
          binding.command + "|" + binding.device + "|" + binding.key + "|" + binding.button;
      if (!binding.command.empty() && !binding.device.empty() &&
          !binding_keys.insert(dedupe_key).second) {
        addDiagnostic(result.diagnostics, source_path, path,
                      "duplicate input binding for command '" + binding.command + "'");
      }
      result.value.bindings.push_back(std::move(binding));
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

[[nodiscard]] LearningEvidenceRefDocument
parseLearningEvidenceRefDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                                 const std::filesystem::path &source, const std::string &path) {
  LearningEvidenceRefDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.kind = readStringOr(value, "kind", diagnostics, source, path, {});
  out.asset = readStringOr(value, "asset", diagnostics, source, path, {});
  out.source = readStringOr(value, "source", diagnostics, source, path, {});
  out.required_events = readStringArray(value, "required_events", diagnostics, source, path);
  out.required_channels = readStringArray(value, "required_channels", diagnostics, source, path);
  return out;
}

[[nodiscard]] LearningObjectiveDocument
parseLearningObjectiveDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                               const std::filesystem::path &source, const std::string &path) {
  LearningObjectiveDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.description = readStringOr(value, "description", diagnostics, source, path, {});
  out.evidence_ids = readStringArray(value, "evidence", diagnostics, source, path);
  out.mastery_threshold =
      readFloatOr(value, "mastery_threshold", diagnostics, source, path, out.mastery_threshold);
  return out;
}

[[nodiscard]] LearningMisconceptionDocument
parseLearningMisconceptionDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                                   const std::filesystem::path &source,
                                   const std::string &path) {
  LearningMisconceptionDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.description = readStringOr(value, "description", diagnostics, source, path, {});
  out.trigger_events = readStringArray(value, "trigger_events", diagnostics, source, path);
  out.remediation_scaffold_ids =
      readStringArray(value, "remediation_scaffolds", diagnostics, source, path);
  return out;
}

[[nodiscard]] LearnerStateHypothesisDocument
parseLearnerStateHypothesisDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                                    const std::filesystem::path &source,
                                    const std::string &path) {
  LearnerStateHypothesisDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.description = readStringOr(value, "description", diagnostics, source, path, {});
  out.evidence_ids = readStringArray(value, "evidence", diagnostics, source, path);
  out.misconception_ids = readStringArray(value, "misconceptions", diagnostics, source, path);
  out.confidence = readFloatOr(value, "confidence", diagnostics, source, path, out.confidence);
  return out;
}

[[nodiscard]] LearningScaffoldRuleDocument
parseLearningScaffoldRuleDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                                  const std::filesystem::path &source,
                                  const std::string &path) {
  LearningScaffoldRuleDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.stage = readStringOr(value, "stage", diagnostics, source, path, {});
  out.intent = readStringOr(value, "intent", diagnostics, source, path, {});
  out.intervention = readStringOr(value, "intervention", diagnostics, source, path, {});
  out.evidence_ids = readStringArray(value, "evidence", diagnostics, source, path);
  out.hypothesis_ids = readStringArray(value, "hypotheses", diagnostics, source, path);
  out.misconception_ids = readStringArray(value, "misconceptions", diagnostics, source, path);
  out.force_gameplay_change =
      readBoolOr(value, "force_gameplay_change", diagnostics, source, path, false);
  return out;
}

[[nodiscard]] PedagogicalSafetyCheckDocument
parsePedagogicalSafetyCheckDocument(const Json &value, std::vector<Diagnostic> &diagnostics,
                                    const std::filesystem::path &source,
                                    const std::string &path) {
  PedagogicalSafetyCheckDocument out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readString(value, "id", diagnostics, source, path, true).value_or("");
  out.kind = readStringOr(value, "kind", diagnostics, source, path, {});
  out.required_evidence_ids =
      readStringArray(value, "required_evidence", diagnostics, source, path);
  out.message = readStringOr(value, "message", diagnostics, source, path, {});
  return out;
}

[[nodiscard]] LearningTraceEvent
parseLearningTraceEvent(const Json &value, std::vector<Diagnostic> &diagnostics,
                        const std::filesystem::path &source, const std::string &path) {
  LearningTraceEvent out;
  if (!expectObject(value, diagnostics, source, path)) {
    return out;
  }
  out.id = readStringOr(value, "id", diagnostics, source, path, {});
  out.stage = readString(value, "stage", diagnostics, source, path, true).value_or("");
  out.event = readStringOr(value, "event", diagnostics, source, path, {});
  out.evidence_id = readStringOr(value, "evidence_id", diagnostics, source, path, {});
  out.objective_id = readStringOr(value, "objective_id", diagnostics, source, path, {});
  out.hypothesis_id = readStringOr(value, "hypothesis_id", diagnostics, source, path, {});
  out.scaffold_id = readStringOr(value, "scaffold_id", diagnostics, source, path, {});
  out.misconception_id = readStringOr(value, "misconception_id", diagnostics, source, path, {});
  out.metadata = readStringMap(value, "metadata", diagnostics, source, path);
  out.timestamp =
      static_cast<std::uint64_t>(std::max(0, readIntOr(value, "timestamp", diagnostics, source,
                                                       path, 0)));
  out.claims_mastery = readBoolOr(value, "claims_mastery", diagnostics, source, path, false);
  return out;
}

LoadResult<LessonDocument> parseLessonDocument(std::string_view source_text,
                                               std::filesystem::path source_path) {
  LoadResult<LessonDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readString(root, "id", result.diagnostics, source_path, "$", true).value_or("");
    result.value.name = readStringOr(root, "name", result.diagnostics, source_path, "$", {});
    result.value.workflow_stages =
        readStringArray(root, "workflow_stages", result.diagnostics, source_path, "$");

    const auto read_array = [&](const char *key, auto parse_item, auto &out) {
      const Json *items = member(root, key);
      if (items == nullptr) {
        addDiagnostic(result.diagnostics, source_path, childPath("$", key), "missing array");
        return;
      }
      if (items->kind != Json::Kind::Array) {
        addDiagnostic(result.diagnostics, source_path, childPath("$", key), "expected array");
        return;
      }
      for (std::size_t i = 0; i < items->array.size(); ++i) {
        out.push_back(parse_item(items->array[i], result.diagnostics, source_path,
                                 indexPath(childPath("$", key), i)));
      }
    };

    read_array("objectives", parseLearningObjectiveDocument, result.value.objectives);
    read_array("evidence_refs", parseLearningEvidenceRefDocument, result.value.evidence_refs);
    read_array("misconceptions", parseLearningMisconceptionDocument, result.value.misconceptions);
    read_array("learner_state_hypotheses", parseLearnerStateHypothesisDocument,
               result.value.learner_state_hypotheses);
    read_array("scaffold_rules", parseLearningScaffoldRuleDocument,
               result.value.scaffold_rules);
    read_array("safety_checks", parsePedagogicalSafetyCheckDocument,
               result.value.safety_checks);
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<LearningTraceDocument>
parseLearningTraceDocument(std::string_view source_text, std::filesystem::path source_path) {
  LoadResult<LearningTraceDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.id = readStringOr(root, "id", result.diagnostics, source_path, "$", {});
    result.value.lesson = readString(root, "lesson", result.diagnostics, source_path, "$", true)
                              .value_or("");
    const Json *events = member(root, "events");
    if (events == nullptr) {
      addDiagnostic(result.diagnostics, source_path, "$.events", "missing required event array");
      return result;
    }
    if (events->kind != Json::Kind::Array) {
      addDiagnostic(result.diagnostics, source_path, "$.events", "expected event array");
      return result;
    }
    for (std::size_t i = 0u; i < events->array.size(); ++i) {
      result.value.events.push_back(parseLearningTraceEvent(events->array[i], result.diagnostics,
                                                            source_path,
                                                            indexPath("$.events", i)));
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<CaveWorldGateReportDocument>
parseCaveWorldGateReportDocument(std::string_view source_text,
                                 std::filesystem::path source_path) {
  LoadResult<CaveWorldGateReportDocument> result;
  try {
    const Json root = JsonParser(source_text).parse();
    if (!expectObject(root, result.diagnostics, source_path, "$")) {
      return result;
    }
    result.value.schema_version = readSchemaVersion(root, result.diagnostics, source_path);
    result.value.kind = readStringOr(root, "kind", result.diagnostics, source_path, "$", {});
    result.value.id = readStringOr(root, "id", result.diagnostics, source_path, "$", {});
    result.value.verdict = readStringOr(root, "verdict", result.diagnostics, source_path, "$", {});
    result.value.region_id =
        readStringOr(root, "region_id", result.diagnostics, source_path, "$", {});
    result.value.world_transition_hash =
        readStringOr(root, "world_transition_hash", result.diagnostics, source_path, "$", {});
    result.value.extraction_hash =
        readStringOr(root, "extraction_hash", result.diagnostics, source_path, "$", {});
    result.value.belief_contract_hash =
        readStringOr(root, "belief_contract_hash", result.diagnostics, source_path, "$", {});

    if (const Json *navigation = member(root, "navigation")) {
      if (expectObject(*navigation, result.diagnostics, source_path, "$.navigation")) {
        result.value.navigation_valid =
            readBoolOr(*navigation, "valid", result.diagnostics, source_path, "$.navigation",
                       false);
      }
    }
    if (const Json *runtime = member(root, "perceptual_runtime")) {
      if (expectObject(*runtime, result.diagnostics, source_path, "$.perceptual_runtime")) {
        result.value.perceptual_runtime_accepted =
            readBoolOr(*runtime, "accepted", result.diagnostics, source_path,
                       "$.perceptual_runtime", false);
      }
    }
    if (const Json *scheduler = member(root, "perceptual_world_scheduler")) {
      if (expectObject(*scheduler, result.diagnostics, source_path,
                       "$.perceptual_world_scheduler")) {
        CavePerceptualWorldSchedulerReport report;
        report.accepted =
            readBoolOr(*scheduler, "accepted", result.diagnostics, source_path,
                       "$.perceptual_world_scheduler", false);
        report.scheduler_hash =
            readStringOr(*scheduler, "scheduler_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.memory_residue_hash =
            readStringOr(*scheduler, "memory_residue_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.threat_signal_hash =
            readStringOr(*scheduler, "threat_signal_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.material_age_hash =
            readStringOr(*scheduler, "material_age_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.interaction_debt_hash =
            readStringOr(*scheduler, "interaction_debt_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.perceptual_priority_hash =
            readStringOr(*scheduler, "perceptual_priority_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.streaming_budget_hash =
            readStringOr(*scheduler, "streaming_budget_hash", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        report.memory_residue =
            readFloatOr(*scheduler, "memory_residue", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.threat_signal =
            readFloatOr(*scheduler, "threat_signal", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.material_age =
            readFloatOr(*scheduler, "material_age", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.interaction_debt =
            readFloatOr(*scheduler, "interaction_debt", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.perceptual_priority =
            readFloatOr(*scheduler, "perceptual_priority", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.streaming_budget =
            readFloatOr(*scheduler, "streaming_budget", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.belief_stability =
            readFloatOr(*scheduler, "belief_stability", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.decision_impact_score =
            readFloatOr(*scheduler, "decision_impact_score", result.diagnostics, source_path,
                        "$.perceptual_world_scheduler", 0.0f);
        report.diagnostic =
            readStringOr(*scheduler, "diagnostic", result.diagnostics, source_path,
                         "$.perceptual_world_scheduler", {});
        result.value.perceptual_world_scheduler = std::move(report);
      }
    }
    if (const Json *falseness = member(root, "falseness_report")) {
      if (expectObject(*falseness, result.diagnostics, source_path, "$.falseness_report")) {
        CaveBeliefFalsenessReport report;
        report.accepted =
            readBoolOr(*falseness, "accepted", result.diagnostics, source_path,
                       "$.falseness_report", false);
        report.score =
            readFloatOr(*falseness, "score", result.diagnostics, source_path,
                        "$.falseness_report", 0.0f);
        report.minimum_score =
            readFloatOr(*falseness, "minimum_score", result.diagnostics, source_path,
                        "$.falseness_report", 0.70f);
        report.world_transition_hash =
            readStringOr(*falseness, "world_transition_hash", result.diagnostics, source_path,
                         "$.falseness_report", {});
        report.extraction_hash =
            readStringOr(*falseness, "extraction_hash", result.diagnostics, source_path,
                         "$.falseness_report", {});
        report.belief_contract_hash =
            readStringOr(*falseness, "belief_contract_hash", result.diagnostics, source_path,
                         "$.falseness_report", {});
        report.readability_audit_hash =
            readStringOr(*falseness, "readability_audit_hash", result.diagnostics, source_path,
                         "$.falseness_report", {});
        report.perceptual_scheduler_hash =
            readStringOr(*falseness, "perceptual_scheduler_hash", result.diagnostics,
                         source_path, "$.falseness_report", {});
        report.decision_impact_score =
            readFloatOr(*falseness, "decision_impact_score", result.diagnostics, source_path,
                        "$.falseness_report", 0.0f);
        if (const Json *findings = member(*falseness, "findings")) {
          if (findings->kind != Json::Kind::Array) {
            addDiagnostic(result.diagnostics, source_path, "$.falseness_report.findings",
                          "expected findings array");
          } else {
            for (std::size_t i = 0u; i < findings->array.size(); ++i) {
              const Json &finding_json = findings->array[i];
              const std::string path = indexPath("$.falseness_report.findings", i);
              if (!expectObject(finding_json, result.diagnostics, source_path, path)) {
                continue;
              }
              CaveBeliefFindingReport finding;
              finding.kind =
                  readStringOr(finding_json, "kind", result.diagnostics, source_path, path, {});
              finding.severity =
                  readStringOr(finding_json, "severity", result.diagnostics, source_path, path, {});
              finding.subject =
                  readStringOr(finding_json, "subject", result.diagnostics, source_path, path, {});
              finding.score =
                  readFloatOr(finding_json, "score", result.diagnostics, source_path, path, 0.0f);
              finding.threshold = readFloatOr(finding_json, "threshold", result.diagnostics,
                                              source_path, path, 0.0f);
              finding.evidence_hash =
                  readStringOr(finding_json, "evidence_hash", result.diagnostics, source_path,
                               path, {});
              finding.source =
                  readStringOr(finding_json, "source", result.diagnostics, source_path, path, {});
              finding.message =
                  readStringOr(finding_json, "message", result.diagnostics, source_path, path, {});
              report.findings.push_back(std::move(finding));
            }
          }
        }
        result.value.falseness_report = std::move(report);
      }
    }
  } catch (const std::exception &error) {
    addDiagnostic(result.diagnostics, source_path, "$", error.what());
  }
  return result;
}

LoadResult<ProjectDocument> loadProjectDocument(const std::filesystem::path &path) {
  return loadDocument<ProjectDocument>(path, parseProjectDocument);
}

LoadResult<SceneDocument> loadSceneDocument(const std::filesystem::path &path) {
  return loadDocument<SceneDocument>(path, parseSceneDocument);
}

LoadResult<PrefabDocument> loadPrefabDocument(const std::filesystem::path &path) {
  return loadDocument<PrefabDocument>(path, parsePrefabDocument);
}

LoadResult<CaveDocument> loadCaveDocument(const std::filesystem::path &path) {
  return loadDocument<CaveDocument>(path, parseCaveDocument);
}

LoadResult<MaterialDocument> loadMaterialDocument(const std::filesystem::path &path) {
  return loadDocument<MaterialDocument>(path, parseMaterialDocument);
}

LoadResult<ItemDocument> loadItemDocument(const std::filesystem::path &path) {
  return loadDocument<ItemDocument>(path, parseItemDocument);
}

LoadResult<ActionGraphDocument> loadActionGraphDocument(const std::filesystem::path &path) {
  return loadDocument<ActionGraphDocument>(path, parseActionGraphDocument);
}

LoadResult<InputMapDocument> loadInputMapDocument(const std::filesystem::path &path) {
  return loadDocument<InputMapDocument>(path, parseInputMapDocument);
}

LoadResult<LessonDocument> loadLessonDocument(const std::filesystem::path &path) {
  return loadDocument<LessonDocument>(path, parseLessonDocument);
}

LoadResult<LearningTraceDocument> loadLearningTraceDocument(const std::filesystem::path &path) {
  if (path.extension() == ".jsonl") {
    LoadResult<LearningTraceDocument> result;
    const std::optional<std::string> source = readTextFile(path, result.diagnostics);
    if (!source.has_value()) {
      return result;
    }
    result.value.schema_version = kAuthoringSchemaVersion;
    result.value.id = "trace." + path.stem().string();
    std::istringstream lines(*source);
    std::string line;
    std::size_t line_number = 0u;
    while (std::getline(lines, line)) {
      ++line_number;
      if (std::all_of(line.begin(), line.end(), [](const unsigned char character) {
            return std::isspace(character) != 0;
          })) {
        continue;
      }
      const std::string event_path = "$[" + std::to_string(line_number - 1u) + "]";
      try {
        const Json root = JsonParser(line).parse();
        result.value.events.push_back(
            parseLearningTraceEvent(root, result.diagnostics, path, event_path));
        const std::string event_lesson =
            readStringOr(root, "lesson", result.diagnostics, path, event_path, {});
        if (!event_lesson.empty()) {
          if (result.value.lesson.empty()) {
            result.value.lesson = event_lesson;
          } else if (result.value.lesson != event_lesson) {
            addDiagnostic(result.diagnostics, path, childPath(event_path, "lesson"),
                          "JSONL trace mixes multiple lesson ids");
          }
        }
      } catch (const std::exception &error) {
        addDiagnostic(result.diagnostics, path, event_path, error.what());
      }
    }
    if (result.value.lesson.empty()) {
      const std::string filename = path.filename().string();
      constexpr std::string_view suffix = ".trace.jsonl";
      if (filename.size() > suffix.size() && filename.ends_with(suffix)) {
        result.value.lesson = "lesson." + filename.substr(0u, filename.size() - suffix.size());
      }
    }
    if (result.value.events.empty()) {
      addDiagnostic(result.diagnostics, path, "$", "JSONL trace contains no events");
    }
    return result;
  }
  return loadDocument<LearningTraceDocument>(path, parseLearningTraceDocument);
}

LoadResult<CaveWorldGateReportDocument>
loadCaveWorldGateReportDocument(const std::filesystem::path &path) {
  return loadDocument<CaveWorldGateReportDocument>(path, parseCaveWorldGateReportDocument);
}

void World::clear() {
  entities_.clear();
}

InstantiateResult World::instantiate(const SceneDocument &scene) {
  return instantiateEntities(scene.id, scene.entities);
}

InstantiateResult World::instantiate(const PrefabDocument &prefab) {
  return instantiateEntities(prefab.id, prefab.entities);
}

const EntityInstance *World::findEntity(const std::string_view id) const {
  const auto it = std::find_if(entities_.begin(), entities_.end(), [id](const EntityInstance &entity) {
    return entity.definition.id == id;
  });
  return it == entities_.end() ? nullptr : &*it;
}

const std::vector<EntityInstance> &World::entities() const {
  return entities_;
}

InstantiateResult World::instantiateEntities(const AssetId &source_asset,
                                             const std::vector<EntityDefinition> &entities) {
  InstantiateResult result;
  for (const EntityDefinition &entity : entities) {
    if (entity.id.empty()) {
      addDiagnostic(result.diagnostics, source_asset, "$.entities", "entity id must not be empty");
      continue;
    }
    if (findEntity(entity.id) != nullptr) {
      addDiagnostic(result.diagnostics, source_asset, "$.entities",
                    "world already contains entity id '" + entity.id + "'");
      continue;
    }
    entities_.push_back({entity, source_asset});
    ++result.created_entities;
  }
  return result;
}

ActionExecution ActionGraphRuntime::execute(const ActionGraphDocument &graph,
                                            const ActionContext &context) const {
  ActionExecution result;
  result.contract_stamp = actionGraphContractStamp(graph);
  if (graph.nodes.empty()) {
    addDiagnostic(result.diagnostics, graph.id, "$.nodes", "action graph has no nodes",
                  DiagnosticSeverity::Warning);
    return result;
  }
  for (const ActionNode &node : graph.nodes) {
    if (node.id.empty()) {
      addDiagnostic(result.diagnostics, graph.id, "$.nodes", "action node id must not be empty");
      continue;
    }
    if (node.type.empty()) {
      addDiagnostic(result.diagnostics, graph.id, "$.nodes." + node.id,
                    "action node type must not be empty");
      continue;
    }
    result.events.push_back({.node_id = node.id,
                             .type = node.type,
                             .actor = context.actor,
                             .target = context.target,
                             .parameters = node.parameters,
                             .tags = node.tags,
                             .deterministic_stamp = actionEventStamp(graph, node, context)});
    if (!context.input.empty()) {
      result.events.back().parameters.emplace("input", context.input);
    }
  }
  return result;
}

std::uint64_t actionGraphContractStamp(const ActionGraphDocument &graph) {
  std::uint64_t hash = beginContractHash();
  hashString(hash, "aster.action_graph.v1");
  hashString(hash, graph.id);
  hashString(hash, graph.name);
  hashNumber(hash, graph.schema_version);
  hashNumber(hash, graph.nodes.size());
  for (const ActionNode &node : graph.nodes) {
    hashString(hash, node.id);
    hashString(hash, node.type);
    hashNumber(hash, node.parameters.size());
    for (const auto &[key, value] : node.parameters) {
      hashString(hash, key);
      hashString(hash, value);
    }
    hashNumber(hash, node.tags.size());
    for (const GameplayTag &tag : node.tags) {
      hashString(hash, tag.value);
    }
  }
  hashNumber(hash, graph.reaction_contracts.size());
  for (const ActionReactionContractDocument &contract : graph.reaction_contracts) {
    hashString(hash, contract.id);
    hashString(hash, std::to_string(contract.minimum_score));
    hashNumber(hash, contract.required_channels.size());
    for (const std::string &channel : contract.required_channels) {
      hashString(hash, channel);
    }
    hashNumber(hash, contract.required_events.size());
    for (const std::string &event : contract.required_events) {
      hashString(hash, event);
    }
  }
  return hash;
}

std::uint64_t inputMapContractStamp(const InputMapDocument &input_map) {
  std::uint64_t hash = beginContractHash();
  hashString(hash, "aster.input_map.v1");
  hashString(hash, input_map.id);
  hashString(hash, input_map.name);
  hashNumber(hash, input_map.schema_version);
  hashNumber(hash, input_map.bindings.size());
  for (const InputBindingDocument &binding : input_map.bindings) {
    hashString(hash, binding.command);
    hashString(hash, binding.device);
    hashString(hash, binding.key);
    hashString(hash, binding.button);
    hashString(hash, std::to_string(binding.scale));
    hashString(hash, std::to_string(binding.deadzone));
    hashNumber(hash, binding.tags.size());
    for (const GameplayTag &tag : binding.tags) {
      hashString(hash, tag.value);
    }
  }
  return hash;
}

std::uint64_t lessonContractStamp(const LessonDocument &lesson) {
  std::uint64_t hash = beginContractHash();
  hashString(hash, "aster.lesson.v1");
  hashNumber(hash, lesson.schema_version);
  hashString(hash, lesson.id);
  hashString(hash, lesson.name);
  for (const std::string &stage : lesson.workflow_stages) {
    hashString(hash, stage);
  }
  for (const LearningObjectiveDocument &objective : lesson.objectives) {
    hashString(hash, objective.id);
    hashString(hash, objective.description);
    hashString(hash, std::to_string(objective.mastery_threshold));
    for (const std::string &evidence : objective.evidence_ids) {
      hashString(hash, evidence);
    }
  }
  for (const LearningEvidenceRefDocument &evidence : lesson.evidence_refs) {
    hashString(hash, evidence.id);
    hashString(hash, evidence.kind);
    hashString(hash, evidence.asset);
    hashString(hash, evidence.source);
    for (const std::string &event : evidence.required_events) {
      hashString(hash, event);
    }
    for (const std::string &channel : evidence.required_channels) {
      hashString(hash, channel);
    }
  }
  for (const LearningMisconceptionDocument &misconception : lesson.misconceptions) {
    hashString(hash, misconception.id);
    hashString(hash, misconception.description);
    for (const std::string &event : misconception.trigger_events) {
      hashString(hash, event);
    }
    for (const std::string &scaffold : misconception.remediation_scaffold_ids) {
      hashString(hash, scaffold);
    }
  }
  for (const LearnerStateHypothesisDocument &hypothesis : lesson.learner_state_hypotheses) {
    hashString(hash, hypothesis.id);
    hashString(hash, hypothesis.description);
    hashString(hash, std::to_string(hypothesis.confidence));
    for (const std::string &evidence : hypothesis.evidence_ids) {
      hashString(hash, evidence);
    }
    for (const std::string &misconception : hypothesis.misconception_ids) {
      hashString(hash, misconception);
    }
  }
  for (const LearningScaffoldRuleDocument &rule : lesson.scaffold_rules) {
    hashString(hash, rule.id);
    hashString(hash, rule.stage);
    hashString(hash, rule.intent);
    hashString(hash, rule.intervention);
    hashNumber(hash, rule.force_gameplay_change ? 1u : 0u);
    for (const std::string &evidence : rule.evidence_ids) {
      hashString(hash, evidence);
    }
    for (const std::string &hypothesis : rule.hypothesis_ids) {
      hashString(hash, hypothesis);
    }
    for (const std::string &misconception : rule.misconception_ids) {
      hashString(hash, misconception);
    }
  }
  for (const PedagogicalSafetyCheckDocument &check : lesson.safety_checks) {
    hashString(hash, check.id);
    hashString(hash, check.kind);
    hashString(hash, check.message);
    for (const std::string &evidence : check.required_evidence_ids) {
      hashString(hash, evidence);
    }
  }
  return hash;
}

std::uint64_t learningTraceContractStamp(const LearningTraceDocument &trace) {
  std::uint64_t hash = beginContractHash();
  hashString(hash, "aster.learning_trace.v1");
  hashNumber(hash, trace.schema_version);
  hashString(hash, trace.id);
  hashString(hash, trace.lesson);
  for (const LearningTraceEvent &event : trace.events) {
    hashString(hash, event.id);
    hashString(hash, event.stage);
    hashString(hash, event.event);
    hashString(hash, event.evidence_id);
    hashString(hash, event.objective_id);
    hashString(hash, event.hypothesis_id);
    hashString(hash, event.scaffold_id);
    hashString(hash, event.misconception_id);
    hashNumber(hash, event.timestamp);
    hashNumber(hash, event.claims_mastery ? 1u : 0u);
    for (const auto &[key, value] : event.metadata) {
      hashString(hash, key);
      hashString(hash, value);
    }
  }
  return hash;
}

std::vector<Diagnostic> validateLessonDocument(const LessonDocument &lesson,
                                               const ProjectDocument *project,
                                               std::filesystem::path source_path) {
  std::vector<Diagnostic> diagnostics;
  const auto addError = [&](std::string path, std::string message) {
    addDiagnostic(diagnostics, source_path, std::move(path), std::move(message));
  };
  const auto checkUnique = [&](const char *path, const auto &rows) {
    std::set<std::string> ids;
    for (const auto &row : rows) {
      if (row.id.empty()) {
        addError(path, "id must not be empty");
      } else if (!ids.insert(row.id).second) {
        addError(path, "duplicate id '" + row.id + "'");
      }
    }
    return ids;
  };

  if (lesson.id.empty()) {
    addError("$.id", "lesson id must not be empty");
  }
  if (lesson.name.empty()) {
    addError("$.name", "lesson name must not be empty");
  }
  if (lesson.workflow_stages.empty()) {
    addError("$.workflow_stages", "lesson must define workflow stages");
  }
  if (lesson.objectives.empty()) {
    addError("$.objectives", "lesson must define objectives");
  }
  if (lesson.evidence_refs.empty()) {
    addError("$.evidence_refs", "lesson must define evidence references");
  }

  const std::set<std::string> objective_ids = checkUnique("$.objectives", lesson.objectives);
  (void)objective_ids;
  const std::set<std::string> evidence_ids = checkUnique("$.evidence_refs", lesson.evidence_refs);
  const std::set<std::string> misconception_ids =
      checkUnique("$.misconceptions", lesson.misconceptions);
  const std::set<std::string> hypothesis_ids =
      checkUnique("$.learner_state_hypotheses", lesson.learner_state_hypotheses);
  const std::set<std::string> scaffold_ids = checkUnique("$.scaffold_rules", lesson.scaffold_rules);
  (void)checkUnique("$.safety_checks", lesson.safety_checks);
  const std::set<std::string> workflow_ids(lesson.workflow_stages.begin(),
                                           lesson.workflow_stages.end());

  std::set<std::string> project_assets;
  if (project != nullptr) {
    for (const ProjectAssetRef &asset : project->assets) {
      if (!asset.id.empty()) {
        project_assets.insert(asset.id);
      }
    }
  }

  for (const LearningEvidenceRefDocument &evidence : lesson.evidence_refs) {
    if (evidence.asset.empty()) {
      addError("$.evidence_refs." + evidence.id + ".asset", "evidence must bind to an asset id");
    } else if (project != nullptr && !project_assets.contains(evidence.asset)) {
      addError("$.evidence_refs." + evidence.id + ".asset",
               "evidence references missing project asset '" + evidence.asset + "'");
    }
  }
  for (const LearningObjectiveDocument &objective : lesson.objectives) {
    if (objective.evidence_ids.empty()) {
      addError("$.objectives." + objective.id + ".evidence",
               "objective must reference at least one evidence id");
    }
    if (objective.mastery_threshold <= 0.0f || objective.mastery_threshold > 1.0f) {
      addError("$.objectives." + objective.id + ".mastery_threshold",
               "mastery_threshold must be in (0, 1]");
    }
    for (const std::string &evidence : objective.evidence_ids) {
      if (!evidence_ids.contains(evidence)) {
        addError("$.objectives." + objective.id + ".evidence",
                 "objective references missing evidence '" + evidence + "'");
      }
    }
  }
  for (const LearnerStateHypothesisDocument &hypothesis : lesson.learner_state_hypotheses) {
    if (hypothesis.evidence_ids.empty()) {
      addError("$.learner_state_hypotheses." + hypothesis.id + ".evidence",
               "learner-state hypothesis must cite evidence");
    }
    for (const std::string &evidence : hypothesis.evidence_ids) {
      if (!evidence_ids.contains(evidence)) {
        addError("$.learner_state_hypotheses." + hypothesis.id + ".evidence",
                 "hypothesis references missing evidence '" + evidence + "'");
      }
    }
    for (const std::string &misconception : hypothesis.misconception_ids) {
      if (!misconception_ids.contains(misconception)) {
        addError("$.learner_state_hypotheses." + hypothesis.id + ".misconceptions",
                 "hypothesis references missing misconception '" + misconception + "'");
      }
    }
  }
  for (const LearningMisconceptionDocument &misconception : lesson.misconceptions) {
    for (const std::string &scaffold : misconception.remediation_scaffold_ids) {
      if (!scaffold_ids.contains(scaffold)) {
        addError("$.misconceptions." + misconception.id + ".remediation_scaffolds",
                 "misconception references missing scaffold '" + scaffold + "'");
      }
    }
  }
  for (const LearningScaffoldRuleDocument &rule : lesson.scaffold_rules) {
    if (!workflow_ids.contains(rule.stage)) {
      addError("$.scaffold_rules." + rule.id + ".stage",
               "scaffold stage is not declared in workflow_stages");
    }
    if (rule.evidence_ids.empty() || rule.hypothesis_ids.empty()) {
      addError("$.scaffold_rules." + rule.id,
               "scaffold must cite both evidence and learner-state hypotheses");
    }
    if (rule.force_gameplay_change) {
      addError("$.scaffold_rules." + rule.id + ".force_gameplay_change",
               "pedagogical scaffolds must not force gameplay changes");
    }
    for (const std::string &evidence : rule.evidence_ids) {
      if (!evidence_ids.contains(evidence)) {
        addError("$.scaffold_rules." + rule.id + ".evidence",
                 "scaffold references missing evidence '" + evidence + "'");
      }
    }
    for (const std::string &hypothesis : rule.hypothesis_ids) {
      if (!hypothesis_ids.contains(hypothesis)) {
        addError("$.scaffold_rules." + rule.id + ".hypotheses",
                 "scaffold references missing hypothesis '" + hypothesis + "'");
      }
    }
    for (const std::string &misconception : rule.misconception_ids) {
      if (!misconception_ids.contains(misconception)) {
        addError("$.scaffold_rules." + rule.id + ".misconceptions",
                 "scaffold references missing misconception '" + misconception + "'");
      }
    }
  }
  for (const PedagogicalSafetyCheckDocument &check : lesson.safety_checks) {
    if (check.kind.empty()) {
      addError("$.safety_checks." + check.id + ".kind", "safety check kind must not be empty");
    }
    for (const std::string &evidence : check.required_evidence_ids) {
      if (!evidence_ids.contains(evidence)) {
        addError("$.safety_checks." + check.id + ".required_evidence",
                 "safety check references missing evidence '" + evidence + "'");
      }
    }
  }
  return diagnostics;
}

LearningProofReport evaluateLearningTrace(const LessonDocument &lesson,
                                          const LearningTraceDocument &trace,
                                          std::filesystem::path source_path) {
  LearningProofReport report;
  report.contract_stamp = lessonContractStamp(lesson) ^ learningTraceContractStamp(trace);
  report.diagnostics = validateLessonDocument(lesson, nullptr, source_path);
  const auto append_unique = [](std::vector<std::string> &values, const std::string &value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
      values.push_back(value);
    }
  };
  if (trace.lesson != lesson.id) {
    addDiagnostic(report.diagnostics, source_path, "$.lesson",
                  "learning trace targets lesson '" + trace.lesson + "' but expected '" +
                      lesson.id + "'");
  }

  std::map<std::string, const LearningScaffoldRuleDocument *> scaffold_by_id;
  for (const LearningScaffoldRuleDocument &rule : lesson.scaffold_rules) {
    scaffold_by_id.emplace(rule.id, &rule);
  }

  std::set<std::string> observed_evidence;
  std::set<std::string> observed_hypotheses;
  std::set<std::string> observed_stages;
  bool false_mastery = false;
  const auto objectives_covered_now = [&]() {
    return std::all_of(lesson.objectives.begin(), lesson.objectives.end(),
                       [&](const LearningObjectiveDocument &objective) {
                         return std::all_of(objective.evidence_ids.begin(),
                                            objective.evidence_ids.end(),
                                            [&](const std::string &evidence) {
                                              return observed_evidence.contains(evidence);
                                            });
                       });
  };

  for (std::size_t index = 0u; index < trace.events.size(); ++index) {
    const LearningTraceEvent &event = trace.events[index];
    if (!event.stage.empty()) {
      observed_stages.insert(event.stage);
    }
    if (!event.evidence_id.empty()) {
      observed_evidence.insert(event.evidence_id);
    }
    if (!event.hypothesis_id.empty()) {
      observed_hypotheses.insert(event.hypothesis_id);
    }
    if (!event.scaffold_id.empty()) {
      LearningInterventionDecision decision;
      decision.id = event.id.empty() ? "intervention." + std::to_string(index) : event.id;
      decision.stage = event.stage;
      decision.scaffold_id = event.scaffold_id;
      decision.hypothesis_id = event.hypothesis_id;
      decision.misconception_id = event.misconception_id;
      const auto rationale = event.metadata.find("rationale");
      decision.rationale = rationale == event.metadata.end() ? std::string{} : rationale->second;

      const auto scaffold_it = scaffold_by_id.find(event.scaffold_id);
      if (scaffold_it == scaffold_by_id.end()) {
        append_unique(report.unsupported_interventions,
                      event.scaffold_id + ": scaffold is not declared by the lesson");
      } else {
        const LearningScaffoldRuleDocument &rule = *scaffold_it->second;
        decision.evidence_ids = rule.evidence_ids;
        bool accepted = true;
        if (rule.stage != event.stage) {
          append_unique(report.unsupported_interventions,
                        rule.id + ": selected at stage '" + event.stage +
                            "' but rule stage is '" + rule.stage + "'");
          accepted = false;
        }
        for (const std::string &evidence : rule.evidence_ids) {
          if (!observed_evidence.contains(evidence)) {
            append_unique(report.unsupported_interventions,
                          rule.id + ": evidence '" + evidence +
                              "' was not observed before intervention");
            accepted = false;
          }
        }
        for (const std::string &hypothesis : rule.hypothesis_ids) {
          if (!observed_hypotheses.contains(hypothesis)) {
            append_unique(report.unsupported_interventions,
                          rule.id + ": learner-state hypothesis '" + hypothesis +
                              "' was not diagnosed before intervention");
            accepted = false;
          }
        }
        if (decision.rationale.empty()) {
          append_unique(report.unsupported_interventions,
                        rule.id + ": intervention rationale is missing");
          accepted = false;
        } else {
          bool rationale_grounded = false;
          for (const std::string &evidence : rule.evidence_ids) {
            rationale_grounded = rationale_grounded ||
                                 decision.rationale.find(evidence) != std::string::npos;
          }
          for (const std::string &hypothesis : rule.hypothesis_ids) {
            rationale_grounded = rationale_grounded ||
                                 decision.rationale.find(hypothesis) != std::string::npos;
          }
          if (!rationale_grounded) {
            append_unique(report.unsupported_interventions,
                          rule.id + ": rationale does not cite declared evidence or hypothesis");
            accepted = false;
          }
        }
        decision.accepted = accepted;
      }
      report.interventions.push_back(std::move(decision));
    }
    if (event.claims_mastery && !objectives_covered_now()) {
      false_mastery = true;
    }
  }

  std::size_t covered_objectives = 0u;
  for (const LearningObjectiveDocument &objective : lesson.objectives) {
    const bool covered =
        std::all_of(objective.evidence_ids.begin(), objective.evidence_ids.end(),
                    [&](const std::string &evidence) {
                      return observed_evidence.contains(evidence);
                    });
    if (covered) {
      ++covered_objectives;
    } else {
      append_unique(report.missing_objectives, objective.id);
      for (const std::string &evidence : objective.evidence_ids) {
        if (!observed_evidence.contains(evidence)) {
          append_unique(report.missing_evidence, evidence);
        }
      }
    }
  }
  report.objective_coverage = lesson.objectives.empty()
                                  ? 0.0f
                                  : static_cast<float>(covered_objectives) /
                                        static_cast<float>(lesson.objectives.size());

  std::size_t covered_stages = 0u;
  for (const std::string &stage : lesson.workflow_stages) {
    if (observed_stages.contains(stage)) {
      ++covered_stages;
    } else {
      append_unique(report.missing_workflow_stages, stage);
    }
  }
  report.workflow_coverage = lesson.workflow_stages.empty()
                                 ? 0.0f
                                 : static_cast<float>(covered_stages) /
                                       static_cast<float>(lesson.workflow_stages.size());

  if (false_mastery) {
    append_unique(report.safety_failures,
                  "trace claimed mastery before all lesson objectives were evidenced");
  }
  for (const PedagogicalSafetyCheckDocument &check : lesson.safety_checks) {
    for (const std::string &evidence : check.required_evidence_ids) {
      if (!observed_evidence.contains(evidence)) {
        append_unique(report.safety_failures,
                      check.id + ": missing safety evidence '" + evidence + "'");
      }
    }
  }

  report.passed = report.diagnostics.empty() && report.missing_objectives.empty() &&
                  report.missing_workflow_stages.empty() && report.missing_evidence.empty() &&
                  report.unsupported_interventions.empty() && report.safety_failures.empty();
  return report;
}

std::string summarizeLearningProofFailures(const LearningProofReport &report) {
  if (report.passed) {
    return "learning proof passed";
  }
  std::ostringstream out;
  out << "learning proof failed";
  const auto append_list = [&](const char *label, const std::vector<std::string> &values) {
    if (values.empty()) {
      return;
    }
    out << "\n- " << label << ": ";
    for (std::size_t i = 0u; i < values.size(); ++i) {
      if (i > 0u) {
        out << ", ";
      }
      out << values[i];
    }
  };
  append_list("missing objectives", report.missing_objectives);
  append_list("missing workflow stages", report.missing_workflow_stages);
  append_list("missing evidence", report.missing_evidence);
  append_list("unsupported interventions", report.unsupported_interventions);
  append_list("safety failures", report.safety_failures);
  if (!report.diagnostics.empty()) {
    out << "\n- diagnostics: " << report.diagnostics.size();
  }
  return out.str();
}

std::vector<Diagnostic> validateCaveDocument(const CaveDocument &cave,
                                             const ProjectDocument *project,
                                             const SceneDocument *scene,
                                             std::filesystem::path source_path) {
  std::vector<Diagnostic> diagnostics;
  const auto addError = [&](std::string path, std::string message) {
    addDiagnostic(diagnostics, source_path, std::move(path), std::move(message));
  };
  const auto addWarning = [&](std::string path, std::string message) {
    addDiagnostic(diagnostics, source_path, std::move(path), std::move(message),
                  DiagnosticSeverity::Warning);
  };

  if (cave.id.empty()) {
    addError("$.id", "cave id must not be empty");
  }
  if (cave.name.empty()) {
    addError("$.name", "cave name must not be empty");
  }
  if (cave.sections.empty()) {
    addError("$.sections", "cave must define at least one section");
  }

  std::set<std::string> asset_ids;
  std::map<std::string, AssetKind> asset_kinds;
  if (project != nullptr) {
    for (const ProjectAssetRef &asset : project->assets) {
      if (!asset.id.empty()) {
        asset_ids.insert(asset.id);
        asset_kinds.emplace(asset.id, asset.kind);
      }
    }
  }
  for (const AssetId &asset : cave.required_assets) {
    if (!asset.empty() && !asset_ids.contains(asset) && project != nullptr) {
      addError("$.required_assets", "missing required asset id '" + asset + "'");
    }
  }

  std::set<std::string> section_ids;
  for (const CaveSectionDocument &section : cave.sections) {
    if (section.id.empty()) {
      addError("$.sections", "section id must not be empty");
      continue;
    }
    if (!section_ids.insert(section.id).second) {
      addError("$.sections", "duplicate section id '" + section.id + "'");
    }
    if (section.archetype.empty()) {
      addError("$.sections." + section.id, "section archetype must not be empty");
    }
    if (project != nullptr && !section.archetype.empty() && !asset_ids.contains(section.archetype)) {
      addError("$.sections." + section.id,
               "section references missing archetype asset '" + section.archetype + "'");
    }
    if (project != nullptr && !section.archetype.empty() &&
        asset_kinds[section.archetype] != AssetKind::Prefab) {
      addError("$.sections." + section.id,
               "section archetype must reference a prefab asset");
    }
    if (section.tunnel.length_segments < 8) {
      addError("$.sections." + section.id + ".tunnel.length_segments",
               "tunnel must use at least 8 segments");
    }
    if (section.tunnel.radial_segments < 8) {
      addError("$.sections." + section.id + ".tunnel.radial_segments",
               "tunnel must use at least 8 radial segments");
    }
    if (section.tunnel.collision_end_t <= section.tunnel.collision_start_t) {
      addError("$.sections." + section.id + ".tunnel.collision_end_t",
               "collision_end_t must be greater than collision_start_t");
    }
  }

  std::set<std::string> placement_ids;
  for (const CavePlacementDocument &placement : cave.placements) {
    if (placement.id.empty()) {
      addError("$.placements", "placement id must not be empty");
      continue;
    }
    if (!placement_ids.insert(placement.id).second) {
      addError("$.placements", "duplicate placement id '" + placement.id + "'");
    }
    if (placement.prefab.empty() && placement.archetype.empty()) {
      addError("$.placements." + placement.id,
               "placement must define either prefab or archetype");
    }
    if (project != nullptr && !placement.prefab.empty() && !asset_ids.contains(placement.prefab)) {
      addError("$.placements." + placement.id,
               "placement references missing prefab asset '" + placement.prefab + "'");
    }
    if (project != nullptr && !placement.prefab.empty() &&
        asset_kinds[placement.prefab] != AssetKind::Prefab) {
      addError("$.placements." + placement.id,
               "placement prefab must reference a prefab asset");
    }
    if (project != nullptr && !placement.archetype.empty() &&
        !asset_ids.contains(placement.archetype)) {
      addError("$.placements." + placement.id,
               "placement references missing asset id '" + placement.archetype + "'");
    }
    if (!placement.section.empty() && !section_ids.contains(placement.section)) {
      addError("$.placements." + placement.id,
               "placement references missing cave section '" + placement.section + "'");
    }
  }

  std::set<std::string> route_ids;
  for (const CaveRouteValidationDocument &route : cave.validation.walkable_routes) {
    if (route.id.empty()) {
      addError("$.validation.walkable_routes", "route id must not be empty");
      continue;
    }
    if (!route_ids.insert(route.id).second) {
      addError("$.validation.walkable_routes", "duplicate route id '" + route.id + "'");
    }
    if (route.points.size() < 2u) {
      addError("$.validation.walkable_routes." + route.id, "route must contain at least two points");
    }
  }

  for (const CaveVolumeValidationDocument &spawn : cave.validation.spawn_volumes) {
    if (spawn.id.empty()) {
      addError("$.validation.spawn_volumes", "spawn volume id must not be empty");
      continue;
    }
    if (spawn.half_extents.x <= 0.0f || spawn.half_extents.y <= 0.0f ||
        spawn.half_extents.z <= 0.0f) {
      addError("$.validation.spawn_volumes." + spawn.id,
               "spawn volume half extents must be positive");
    }
  }

  const auto overlapsVolume = [](const CaveVolumeValidationDocument &lhs,
                                 const CaveVolumeValidationDocument &rhs) {
    return std::abs(lhs.center.x - rhs.center.x) <= lhs.half_extents.x + rhs.half_extents.x &&
           std::abs(lhs.center.y - rhs.center.y) <= lhs.half_extents.y + rhs.half_extents.y &&
           std::abs(lhs.center.z - rhs.center.z) <= lhs.half_extents.z + rhs.half_extents.z;
  };
  for (const CaveVolumeValidationDocument &spawn : cave.validation.spawn_volumes) {
    for (const CaveVolumeValidationDocument &collision : cave.validation.collision_volumes) {
      if (overlapsVolume(spawn, collision)) {
        addError("$.validation.spawn_volumes." + spawn.id,
                 "spawn volume overlaps collision volume '" + collision.id + "'");
      }
    }
  }

  for (const CaveVolumeValidationDocument &collision : cave.validation.collision_volumes) {
    if (collision.id.empty()) {
      addError("$.validation.collision_volumes", "collision volume id must not be empty");
      continue;
    }
    if (collision.half_extents.x <= 0.0f || collision.half_extents.y <= 0.0f ||
        collision.half_extents.z <= 0.0f) {
      addError("$.validation.collision_volumes." + collision.id,
               "collision volume half extents must be positive");
    }
  }

  for (const CaveCameraValidationDocument &camera : cave.validation.camera_probes) {
    if (camera.id.empty()) {
      addError("$.validation.camera_probes", "camera probe id must not be empty");
      continue;
    }
    if (camera.min_clearance <= 0.0f) {
      addError("$.validation.camera_probes." + camera.id,
               "camera probe min_clearance must be positive");
    }
    for (const CaveVolumeValidationDocument &collision : cave.validation.collision_volumes) {
      const bool inside_x =
          std::abs(camera.camera.x - collision.center.x) <
          collision.half_extents.x + camera.min_clearance;
      const bool inside_y =
          std::abs(camera.camera.y - collision.center.y) <
          collision.half_extents.y + camera.min_clearance;
      const bool inside_z =
          std::abs(camera.camera.z - collision.center.z) <
          collision.half_extents.z + camera.min_clearance;
      if (inside_x && inside_y && inside_z) {
        addError("$.validation.camera_probes." + camera.id,
                 "camera probe starts inside or too close to collision volume '" +
                     collision.id + "'");
      }
    }
  }

  if (cave.validation.probe_agent.has_value()) {
    const CaveProbeAgentDocument &agent = *cave.validation.probe_agent;
    if (agent.id.empty()) {
      addError("$.validation.probe_agent.id", "probe agent id must not be empty");
    }
    if (agent.step_count <= 0) {
      addError("$.validation.probe_agent.step_count", "probe agent step_count must be positive");
    }
    if (agent.step_length <= 0.0f) {
      addError("$.validation.probe_agent.step_length", "probe agent step_length must be positive");
    }
  }

  for (const CaveWorldProbeDocument &probe : cave.validation.resource_probes) {
    if (probe.id.empty()) {
      addError("$.validation.resource_probes", "resource probe id must not be empty");
      continue;
    }
    if (probe.radius <= 0.0f) {
      addError("$.validation.resource_probes." + probe.id, "resource probe radius must be positive");
    }
    if (probe.minimum_count <= 0) {
      addError("$.validation.resource_probes." + probe.id,
               "resource probe minimum_count must be positive");
    }
  }

  for (const CaveWorldProbeDocument &probe : cave.validation.encounter_probes) {
    if (probe.id.empty()) {
      addError("$.validation.encounter_probes", "encounter probe id must not be empty");
      continue;
    }
    if (probe.radius <= 0.0f) {
      addError("$.validation.encounter_probes." + probe.id,
               "encounter probe radius must be positive");
    }
    if (probe.maximum_budget < probe.minimum_budget) {
      addError("$.validation.encounter_probes." + probe.id,
               "encounter probe maximum_budget must be >= minimum_budget");
    }
  }

  if (cave.validation.perceptual_budget.has_value()) {
    const CavePerceptualBudgetDocument &budget = *cave.validation.perceptual_budget;
    if (budget.id.empty()) {
      addError("$.validation.perceptual_budget.id", "perceptual budget id must not be empty");
    }
    if (budget.minimum_salience < 0.0f || budget.minimum_salience > 1.0f) {
      addError("$.validation.perceptual_budget.minimum_salience",
               "perceptual minimum_salience must be in [0, 1]");
    }
  }

  if (cave.validation.perceptual_continuity_budget.has_value()) {
    const CavePerceptualContinuityBudgetDocument &budget =
        *cave.validation.perceptual_continuity_budget;
    if (budget.id.empty()) {
      addError("$.validation.perceptual_continuity_budget.id",
               "perceptual continuity budget id must not be empty");
    }
    if (budget.minimum_score < 0.0f || budget.minimum_score > 1.0f) {
      addError("$.validation.perceptual_continuity_budget.minimum_score",
               "perceptual continuity minimum_score must be in [0, 1]");
    }
    if (budget.required_channels.empty()) {
      addError("$.validation.perceptual_continuity_budget.required_channels",
               "perceptual continuity budget must require at least one channel");
    }
    std::set<std::string> package_ids;
    for (const CaveReactionPackageDocument &package : budget.reaction_packages) {
      if (package.id.empty()) {
        addError("$.validation.perceptual_continuity_budget.reaction_packages",
                 "reaction package id must not be empty");
        continue;
      }
      if (!package_ids.insert(package.id).second) {
        addError("$.validation.perceptual_continuity_budget.reaction_packages." + package.id,
                 "duplicate reaction package id");
      }
      if (package.action.empty()) {
        addError("$.validation.perceptual_continuity_budget.reaction_packages." + package.id,
                 "reaction package action must not be empty");
      }
      if (package.minimum_score < 0.0f || package.minimum_score > 1.0f) {
        addError("$.validation.perceptual_continuity_budget.reaction_packages." + package.id,
                 "reaction package minimum_score must be in [0, 1]");
      }
      if (package.required_channels.empty()) {
        addError("$.validation.perceptual_continuity_budget.reaction_packages." + package.id,
                 "reaction package must require at least one channel");
      }
    }
  }

  if (cave.validation.perception_ledger.has_value()) {
    const auto validPerceptionLedgerChannel = [](const std::string &channel) {
      return channel == "material_memory" || channel == "contact_history" ||
             channel == "lighting_exposure" || channel == "atmosphere_cell" ||
             channel == "occlusion_role" || channel == "gameplay_affordance" ||
             channel == "wear_continuity" || channel == "streaming_semantic_lod" ||
             channel == "audio_visual_cue_budget";
    };
    const CavePerceptionLedgerDocument &ledger = *cave.validation.perception_ledger;
    if (ledger.id.empty()) {
      addError("$.validation.perception_ledger.id",
               "perception ledger id must not be empty");
    }
    if (ledger.minimum_score < 0.0f || ledger.minimum_score > 1.0f) {
      addError("$.validation.perception_ledger.minimum_score",
               "perception ledger minimum_score must be in [0, 1]");
    }
    if (ledger.required_channels.empty()) {
      addError("$.validation.perception_ledger.required_channels",
               "perception ledger must require at least one channel");
    }
    for (const std::string &channel : ledger.required_channels) {
      if (!validPerceptionLedgerChannel(channel)) {
        addError("$.validation.perception_ledger.required_channels",
                 "unknown perception ledger channel '" + channel + "'");
      }
    }
    std::set<std::string> cell_ids;
    for (const CavePerceptionLedgerCellDocument &cell : ledger.cells) {
      if (cell.id.empty()) {
        addError("$.validation.perception_ledger.cells", "ledger cell id must not be empty");
        continue;
      }
      if (!cell_ids.insert(cell.id).second) {
        addError("$.validation.perception_ledger.cells." + cell.id,
                 "duplicate perception ledger cell id");
      }
      if (cell.minimum_score < 0.0f || cell.minimum_score > 1.0f) {
        addError("$.validation.perception_ledger.cells." + cell.id,
                 "ledger cell minimum_score must be in [0, 1]");
      }
      for (const std::string &channel : cell.required_channels) {
        if (!validPerceptionLedgerChannel(channel)) {
          addError("$.validation.perception_ledger.cells." + cell.id,
                   "unknown perception ledger channel '" + channel + "'");
        }
      }
    }
  }

  if (cave.validation.perceptual_runtime.has_value()) {
    const CavePerceptualRuntimeDocument &runtime = *cave.validation.perceptual_runtime;
    const auto scoreInRange = [](const float value) {
      return value >= 0.0f && value <= 1.0f;
    };
    if (runtime.id.empty()) {
      addError("$.validation.perceptual_runtime.id",
               "perceptual runtime id must not be empty");
    }
    if (runtime.exposure_horizon_seconds <= 0.0f) {
      addError("$.validation.perceptual_runtime.exposure_horizon_seconds",
               "perceptual runtime exposure_horizon_seconds must be positive");
    }
    if (!scoreInRange(runtime.minimum_continuity_score)) {
      addError("$.validation.perceptual_runtime.minimum_continuity_score",
               "perceptual runtime minimum_continuity_score must be in [0, 1]");
    }
    if (!scoreInRange(runtime.minimum_occlusion_trust)) {
      addError("$.validation.perceptual_runtime.minimum_occlusion_trust",
               "perceptual runtime minimum_occlusion_trust must be in [0, 1]");
    }
    if (!scoreInRange(runtime.minimum_lighting_believability)) {
      addError("$.validation.perceptual_runtime.minimum_lighting_believability",
               "perceptual runtime minimum_lighting_believability must be in [0, 1]");
    }
    if (!scoreInRange(runtime.minimum_player_readable_cause)) {
      addError("$.validation.perceptual_runtime.minimum_player_readable_cause",
               "perceptual runtime minimum_player_readable_cause must be in [0, 1]");
    }
  }

  if (cave.validation.perceptual_causality_graph.has_value()) {
    const CavePerceptualCausalityGraphDocument &graph =
        *cave.validation.perceptual_causality_graph;
    const auto scoreInRange = [](const float value) {
      return value >= 0.0f && value <= 1.0f;
    };
    const auto validCausalityChannel = [](const std::string &channel) {
      return channel == "material_memory" || channel == "contact_residue" ||
             channel == "light_history" || channel == "acoustic_surface" ||
             channel == "traversal_affordance" || channel == "threat_cover" ||
             channel == "semantic_lod" || channel == "streaming_cost" ||
             channel == "player_readable_cause" || channel == "neural_irradiance";
    };
    if (graph.id.empty()) {
      addError("$.validation.perceptual_causality_graph.id",
               "perceptual causality graph id must not be empty");
    }
    if (!scoreInRange(graph.minimum_decision_impact)) {
      addError("$.validation.perceptual_causality_graph.minimum_decision_impact",
               "perceptual causality graph minimum_decision_impact must be in [0, 1]");
    }
    if (graph.required_causal_edges.empty()) {
      addError("$.validation.perceptual_causality_graph.required_causal_edges",
               "perceptual causality graph must require at least one causal edge");
    }
    if (graph.required_decision_channels.empty()) {
      addError("$.validation.perceptual_causality_graph.required_decision_channels",
               "perceptual causality graph must require at least one decision channel");
    }
    for (const std::string &channel : graph.required_causal_edges) {
      if (!validCausalityChannel(channel)) {
        addError("$.validation.perceptual_causality_graph.required_causal_edges",
                 "unknown perceptual causality channel '" + channel + "'");
      }
    }
    for (const std::string &channel : graph.required_decision_channels) {
      if (!validCausalityChannel(channel)) {
        addError("$.validation.perceptual_causality_graph.required_decision_channels",
                 "unknown perceptual causality decision channel '" + channel + "'");
      }
    }
    for (const std::string &cause : graph.required_player_readable_causes) {
      if (cause.empty()) {
        addError("$.validation.perceptual_causality_graph.required_player_readable_causes",
                 "player-readable cause id must not be empty");
      }
    }
  }

  if (cave.validation.belief_contract.has_value()) {
    const CaveBeliefContractDocument &belief = *cave.validation.belief_contract;
    if (belief.id.empty()) {
      addError("$.validation.belief_contract.id", "belief contract id must not be empty");
    }
    if (belief.minimum_score < 0.0f || belief.minimum_score > 1.0f) {
      addError("$.validation.belief_contract.minimum_score",
               "belief contract minimum_score must be in [0, 1]");
    }
    for (const std::string &check : belief.required_checks) {
      if (!aster::isCanonicalBeliefFindingName(check)) {
        addError("$.validation.belief_contract.required_checks",
                 "unknown belief contract check '" + check + "'");
      }
    }
  } else {
    addWarning("$.validation.belief_contract",
               "missing belief contract; using default V1 minimum_score 0.70");
  }

  if (scene != nullptr) {
    std::set<std::string> scene_ids;
    for (const EntityDefinition &entity : scene->entities) {
      if (!entity.id.empty()) {
        scene_ids.insert(entity.id);
      }
      if (entity.components.ore_node.has_value() && !entity.components.collider.has_value()) {
        addError("$.scene.entities." + entity.id, "ore nodes must define a collider");
      }
      if (entity.components.rigid_body.has_value() && !entity.components.collider.has_value()) {
        addError("$.scene.entities." + entity.id, "rigid bodies must define a collider");
      }
      if (entity.components.rigid_body.has_value() &&
          entity.components.rigid_body->type == RigidBodyType::Dynamic &&
          entity.components.rigid_body->mass <= 0.0f) {
        addError("$.scene.entities." + entity.id, "dynamic rigid bodies must define mass > 0");
      }
      if (entity.components.interactable.has_value() &&
          !entity.components.collider.has_value() &&
          !entity.components.spawn_point.has_value()) {
        addError("$.scene.entities." + entity.id,
                 "interactable cave entities should define a collider or spawn point");
      }
    }
    if (scene_ids.empty()) {
      addError("$.scene", "scene must contain at least one entity");
    }
  }

  return diagnostics;
}

} // namespace aster::sdk
