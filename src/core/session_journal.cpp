// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/session_journal.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] std::string trimCopy(const std::string_view value) {
  const auto is_space = [](const unsigned char c) {
    return std::isspace(c) != 0;
  };
  std::size_t first = 0u;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) {
    ++first;
  }
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1u]))) {
    --last;
  }
  return std::string(value.substr(first, last - first));
}

[[nodiscard]] std::string stripQuotes(std::string value) {
  value = trimCopy(value);
  while (!value.empty() && (value.back() == ',' || value.back() == ';')) {
    value.pop_back();
    value = trimCopy(value);
  }
  if (value.size() >= 2u &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1u, value.size() - 2u);
  }
  return value;
}

[[nodiscard]] std::string escapeJson(const std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8u);
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

[[nodiscard]] std::uint64_t stableStringStamp(const std::string_view value,
                                              std::uint64_t seed = 1469598103934665603ull) {
  for (const char c : value) {
    seed ^= static_cast<unsigned char>(c);
    seed *= 1099511628211ull;
  }
  return seed;
}

[[nodiscard]] std::uint64_t nowSeconds() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

[[nodiscard]] std::string findJsonString(const std::string &line, const std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const std::size_t key_pos = line.find(needle);
  if (key_pos == std::string::npos) {
    return {};
  }
  const std::size_t colon = line.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return {};
  }
  const std::size_t begin = line.find('"', colon + 1u);
  if (begin == std::string::npos) {
    return {};
  }
  std::string out;
  bool escaping = false;
  for (std::size_t i = begin + 1u; i < line.size(); ++i) {
    const char c = line[i];
    if (escaping) {
      switch (c) {
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
        out.push_back(c);
        break;
      }
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (c == '"') {
      break;
    }
    out.push_back(c);
  }
  return out;
}

[[nodiscard]] std::uint64_t findJsonU64(const std::string &line, const std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const std::size_t key_pos = line.find(needle);
  if (key_pos == std::string::npos) {
    return 0u;
  }
  const std::size_t colon = line.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return 0u;
  }
  const std::size_t begin = line.find_first_of("0123456789", colon + 1u);
  if (begin == std::string::npos) {
    return 0u;
  }
  const std::size_t end = line.find_first_not_of("0123456789", begin);
  return std::stoull(line.substr(begin, end == std::string::npos ? std::string::npos
                                                                 : end - begin));
}

} // namespace

void ConfigLayerStack::addLayer(ConfigLayer layer) {
  layer.diagnostics.push_back("sequence:" + std::to_string(next_sequence_++));
  layers_.push_back(std::move(layer));
}

void ConfigLayerStack::clear() {
  layers_.clear();
  next_sequence_ = 1u;
}

bool ConfigLayerStack::empty() const noexcept {
  return layers_.empty();
}

std::size_t ConfigLayerStack::size() const noexcept {
  return layers_.size();
}

std::vector<ConfigLayer> ConfigLayerStack::layers() const {
  return layers_;
}

ConfigResolution ConfigLayerStack::resolve() const {
  std::vector<const ConfigLayer *> ordered;
  ordered.reserve(layers_.size());
  for (const ConfigLayer &layer : layers_) {
    ordered.push_back(&layer);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const ConfigLayer *lhs,
                                                      const ConfigLayer *rhs) {
    return lhs->priority < rhs->priority;
  });

  ConfigResolution resolution;
  resolution.stamp = 0xA57ECAFEF16E0001ull;
  for (const ConfigLayer *layer : ordered) {
    resolution.stamp = hashCombine64(resolution.stamp, stableStringStamp(layer->name));
    resolution.stamp = hashCombine64(resolution.stamp, layer->priority);
    resolution.diagnostics.insert(resolution.diagnostics.end(), layer->diagnostics.begin(),
                                  layer->diagnostics.end());
    for (const auto &[key, value] : layer->values) {
      resolution.values[key] = value;
      resolution.source_layers[key] = layer->name;
      resolution.stamp = hashCombine64(resolution.stamp, stableStringStamp(key));
      resolution.stamp = hashCombine64(resolution.stamp, stableStringStamp(value));
    }
  }
  return resolution;
}

std::optional<std::string> ConfigLayerStack::get(const std::string_view key) const {
  const ConfigResolution resolution = resolve();
  const auto found = resolution.values.find(std::string(key));
  return found == resolution.values.end() ? std::nullopt : std::optional<std::string>(found->second);
}

std::string ConfigLayerStack::explain(const std::string_view key) const {
  const ConfigResolution resolution = resolve();
  const auto found = resolution.source_layers.find(std::string(key));
  if (found == resolution.source_layers.end()) {
    return std::string(key) + " is not set";
  }
  return std::string(key) + " from " + found->second + " = " +
         resolution.values.at(std::string(key));
}

SessionJournal::SessionJournal(SessionJournalOptions options) : options_(options) {}

void SessionJournal::append(SessionJournalEntry entry) {
  if (!options_.persistence_enabled) {
    return;
  }
  if (entry.timestamp_seconds == 0u) {
    entry.timestamp_seconds = nowSeconds();
  }
  if (entry.sequence == 0u) {
    entry.sequence = next_sequence_++;
  } else {
    next_sequence_ = std::max(next_sequence_, entry.sequence + 1u);
  }
  entry.stamp = 0xA57E5E5510A10001ull;
  entry.stamp = hashCombine64(entry.stamp, stableStringStamp(entry.session_id));
  entry.stamp = hashCombine64(entry.stamp, stableStringStamp(entry.kind));
  entry.stamp = hashCombine64(entry.stamp, stableStringStamp(entry.text));
  entry.stamp = hashCombine64(entry.stamp, stableStringStamp(entry.detail));
  entry.stamp = hashCombine64(entry.stamp, entry.sequence);
  entries_.push_back(std::move(entry));
  enforceLimit();
}

void SessionJournal::appendCommand(std::string session_id, std::string command,
                                   std::string detail) {
  append({.session_id = std::move(session_id),
          .kind = "command",
          .text = std::move(command),
          .detail = std::move(detail)});
}

void SessionJournal::appendAssetProduction(std::string session_id,
                                           AssetProductionSessionRecord record) {
  std::ostringstream detail;
  detail << "asset_id=" << record.asset_id << ";graph_hash=" << record.graph_hash
         << ";preview_artifact_hash=" << record.preview_artifact_hash
         << ";quality_gate=" << record.quality_gate << ";steps=";
  for (std::size_t i = 0u; i < record.cook_steps.size(); ++i) {
    if (i > 0u) {
      detail << ",";
    }
    detail << record.cook_steps[i];
  }
  append({.session_id = std::move(session_id),
          .kind = "asset-production",
          .text = std::move(record.asset_id),
          .detail = detail.str()});
}

void SessionJournal::clear() {
  entries_.clear();
  next_sequence_ = 1u;
}

bool SessionJournal::empty() const noexcept {
  return entries_.empty();
}

std::size_t SessionJournal::size() const noexcept {
  return entries_.size();
}

const std::vector<SessionJournalEntry> &SessionJournal::entries() const noexcept {
  return entries_;
}

std::vector<SessionJournalEntry> SessionJournal::entriesFor(
    const std::string_view session_id) const {
  std::vector<SessionJournalEntry> out;
  for (const SessionJournalEntry &entry : entries_) {
    if (entry.session_id == session_id) {
      out.push_back(entry);
    }
  }
  return out;
}

std::uint64_t SessionJournal::byteSize() const {
  return static_cast<std::uint64_t>(toJsonLines().size());
}

std::uint64_t SessionJournal::contractStamp() const {
  std::uint64_t stamp = 0xA57E5E5510A1FFFFull;
  for (const SessionJournalEntry &entry : entries_) {
    stamp = hashCombine64(stamp, entry.stamp);
  }
  return stamp;
}

std::string SessionJournal::toJsonLines() const {
  std::ostringstream out;
  for (const SessionJournalEntry &entry : entries_) {
    out << "{\"session_id\":\"" << escapeJson(entry.session_id) << "\","
        << "\"kind\":\"" << escapeJson(entry.kind) << "\","
        << "\"text\":\"" << escapeJson(entry.text) << "\","
        << "\"detail\":\"" << escapeJson(entry.detail) << "\","
        << "\"ts\":" << entry.timestamp_seconds << ","
        << "\"sequence\":" << entry.sequence << ","
        << "\"stamp\":" << entry.stamp << "}\n";
  }
  return out.str();
}

const SessionJournalOptions &SessionJournal::options() const noexcept {
  return options_;
}

bool SessionJournal::save(const std::filesystem::path &path) const {
  if (!options_.persistence_enabled) {
    return true;
  }
  if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  const std::string lines = toJsonLines();
  file.write(lines.data(), static_cast<std::streamsize>(lines.size()));
  return file.good();
}

SessionJournal SessionJournal::load(const std::filesystem::path &path,
                                    SessionJournalOptions options) {
  SessionJournal journal(options);
  std::ifstream file(path, std::ios::binary);
  std::string line;
  while (std::getline(file, line)) {
    if (trimCopy(line).empty()) {
      continue;
    }
    journal.append({.session_id = findJsonString(line, "session_id"),
                    .kind = findJsonString(line, "kind"),
                    .text = findJsonString(line, "text"),
                    .detail = findJsonString(line, "detail"),
                    .timestamp_seconds = findJsonU64(line, "ts"),
                    .sequence = findJsonU64(line, "sequence")});
  }
  return journal;
}

void SessionJournal::enforceLimit() {
  if (options_.max_bytes == 0u) {
    return;
  }
  while (!entries_.empty() && byteSize() > options_.max_bytes) {
    entries_.erase(entries_.begin());
  }
}

ConfigLayer parseConfigLayerText(std::string name, const std::string_view text,
                                 const std::uint32_t priority) {
  ConfigLayer layer;
  layer.name = std::move(name);
  layer.priority = priority;
  std::string section;
  std::istringstream in{std::string(text)};
  std::string line;
  while (std::getline(in, line)) {
    std::string clean = trimCopy(line);
    if (clean.empty() || clean.front() == '#' || clean.rfind("//", 0u) == 0u ||
        clean == "{" || clean == "}") {
      continue;
    }
    const std::size_t comment = clean.find('#');
    if (comment != std::string::npos) {
      clean = trimCopy(std::string_view(clean).substr(0u, comment));
    }
    if (clean.size() >= 2u && clean.front() == '[' && clean.back() == ']') {
      section = trimCopy(std::string_view(clean).substr(1u, clean.size() - 2u));
      continue;
    }
    const std::size_t delimiter = clean.find('=');
    const std::size_t json_delimiter = clean.find(':');
    const std::size_t split =
        delimiter == std::string::npos ? json_delimiter : delimiter;
    if (split == std::string::npos) {
      layer.diagnostics.push_back("ignored config line: " + clean);
      continue;
    }
    std::string key = stripQuotes(clean.substr(0u, split));
    std::string value = stripQuotes(clean.substr(split + 1u));
    if (!section.empty() && key.find('.') == std::string::npos) {
      key = section + "." + key;
    }
    if (!key.empty()) {
      layer.values[key] = value;
    }
  }
  return layer;
}

SessionDiagnosticSnapshot snapshotSessionDiagnostics(const ConfigLayerStack &config,
                                                     const SessionJournal &journal) {
  const ConfigResolution resolution = config.resolve();
  SessionDiagnosticSnapshot snapshot;
  snapshot.config_layers = config.size();
  snapshot.config_values = resolution.values.size();
  snapshot.journal_entries = journal.size();
  snapshot.journal_bytes = journal.byteSize();
  snapshot.config_stamp = resolution.stamp;
  snapshot.journal_stamp = journal.contractStamp();
  snapshot.diagnostics = resolution.diagnostics;
  if (journal.options().max_bytes > 0u && journal.byteSize() > journal.options().max_bytes) {
    snapshot.diagnostics.push_back("journal exceeds configured byte limit");
  }
  return snapshot;
}

} // namespace aster
