// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct ConfigLayer {
  std::string name;
  std::uint32_t priority = 0u;
  std::map<std::string, std::string> values;
  std::vector<std::string> diagnostics;
};

struct ConfigResolution {
  std::map<std::string, std::string> values;
  std::map<std::string, std::string> source_layers;
  std::vector<std::string> diagnostics;
  std::uint64_t stamp = 0u;
};

class ConfigLayerStack {
public:
  void addLayer(ConfigLayer layer);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::vector<ConfigLayer> layers() const;
  [[nodiscard]] ConfigResolution resolve() const;
  [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
  [[nodiscard]] std::string explain(std::string_view key) const;

private:
  std::vector<ConfigLayer> layers_;
  std::uint64_t next_sequence_ = 1u;
};

struct SessionJournalEntry {
  std::string session_id;
  std::string kind;
  std::string text;
  std::string detail;
  std::uint64_t timestamp_seconds = 0u;
  std::uint64_t sequence = 0u;
  std::uint64_t stamp = 0u;
};

struct SessionJournalOptions {
  std::uint64_t max_bytes = 0u;
  bool persistence_enabled = true;
};

struct AssetProductionSessionRecord {
  std::string asset_id;
  std::string graph_hash;
  std::string preview_artifact_hash;
  std::string quality_gate;
  std::vector<std::string> cook_steps;
};

struct SessionDiagnosticSnapshot {
  std::size_t config_layers = 0u;
  std::size_t config_values = 0u;
  std::size_t journal_entries = 0u;
  std::uint64_t journal_bytes = 0u;
  std::uint64_t config_stamp = 0u;
  std::uint64_t journal_stamp = 0u;
  std::vector<std::string> diagnostics;
};

class SessionJournal {
public:
  explicit SessionJournal(SessionJournalOptions options = {});

  void append(SessionJournalEntry entry);
  void appendCommand(std::string session_id, std::string command, std::string detail = {});
  void appendAssetProduction(std::string session_id, AssetProductionSessionRecord record);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const std::vector<SessionJournalEntry> &entries() const noexcept;
  [[nodiscard]] std::vector<SessionJournalEntry> entriesFor(std::string_view session_id) const;
  [[nodiscard]] std::uint64_t byteSize() const;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string toJsonLines() const;
  [[nodiscard]] const SessionJournalOptions &options() const noexcept;

  [[nodiscard]] bool save(const std::filesystem::path &path) const;
  [[nodiscard]] static SessionJournal load(const std::filesystem::path &path,
                                           SessionJournalOptions options = {});

private:
  void enforceLimit();

  SessionJournalOptions options_{};
  std::vector<SessionJournalEntry> entries_;
  std::uint64_t next_sequence_ = 1u;
};

[[nodiscard]] ConfigLayer parseConfigLayerText(std::string name, std::string_view text,
                                               std::uint32_t priority = 0u);
[[nodiscard]] SessionDiagnosticSnapshot snapshotSessionDiagnostics(
    const ConfigLayerStack &config, const SessionJournal &journal);

} // namespace aster
