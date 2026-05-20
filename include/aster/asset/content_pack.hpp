// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/legacy_lump_archive.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class AsterContentPackEntryKind {
  File,
  Lump,
};

struct AsterContentPackRecord {
  std::string name;
  std::filesystem::path source;
  std::uint32_t offset = 0u;
  std::uint32_t size = 0u;
  bool reloadable = false;
  bool cached = false;
  int mount_priority = 0;
  AsterContentPackEntryKind kind = AsterContentPackEntryKind::File;
};

struct AsterContentPackProfileRow {
  std::string name;
  std::uint32_t size = 0u;
  bool cached = false;
  bool reloadable = false;
  int mount_priority = 0;
  AsterContentPackEntryKind kind = AsterContentPackEntryKind::File;
  std::filesystem::path source;
};

class AsterContentPack {
public:
  void clear();

  bool mountFile(const std::filesystem::path &path, std::string_view mount_name = {},
                 int mount_priority = 0, bool reloadable = false);
  bool mountLumpArchive(const std::filesystem::path &path, int mount_priority = 0,
                        bool reloadable = false);
  bool mountLegacyArchive(const LegacyLumpArchive &archive, int mount_priority = 0);
  bool reload();

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const AsterContentPackRecord *record(std::size_t index) const;
  [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const;
  [[nodiscard]] std::size_t require(std::string_view name) const;
  [[nodiscard]] std::vector<std::uint8_t> read(std::size_t index) const;
  [[nodiscard]] const std::vector<std::uint8_t> &cache(std::size_t index);
  [[nodiscard]] std::vector<AsterContentPackProfileRow> profile() const;

  [[nodiscard]] static std::string canonicalName(std::string_view name);

private:
  struct Entry {
    AsterContentPackRecord record;
    mutable std::vector<std::uint8_t> cache;
    std::uint64_t sequence = 0u;
  };

  std::vector<Entry> entries_;
  std::uint64_t next_sequence_ = 1u;
};

class AsterPackIndex {
public:
  void clear();
  void addRecord(AsterContentPackRecord record);
  void mount(const AsterContentPack &pack);

  [[nodiscard]] const AsterContentPackRecord *find(std::string_view name) const;
  [[nodiscard]] std::vector<AsterContentPackRecord> records() const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::vector<AsterContentPackRecord> records_;
};

[[nodiscard]] const char *asterContentPackEntryKindName(AsterContentPackEntryKind kind);

} // namespace aster
