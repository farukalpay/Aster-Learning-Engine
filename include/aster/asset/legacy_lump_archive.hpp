// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct LegacyLumpRecord {
  std::string name;
  std::filesystem::path source;
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
  bool reloadable = false;
  bool cached = false;
};

struct LegacyLumpProfileRow {
  std::string name;
  std::uint32_t size = 0;
  bool cached = false;
  bool reloadable = false;
};

class LegacyLumpArchive {
public:
  void clear();
  bool addFile(const std::filesystem::path &path, bool reloadable = false);
  bool reload();

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] const LegacyLumpRecord *record(std::size_t index) const;
  [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const;
  [[nodiscard]] std::size_t require(std::string_view name) const;
  [[nodiscard]] std::vector<std::uint8_t> read(std::size_t index) const;
  [[nodiscard]] const std::vector<std::uint8_t> &cache(std::size_t index);
  [[nodiscard]] std::vector<LegacyLumpProfileRow> profile() const;

  [[nodiscard]] static std::string canonicalName(std::string_view name);

private:
  struct LumpEntry {
    LegacyLumpRecord record{};
    mutable std::vector<std::uint8_t> cache;
  };

  bool addSingleFile(const std::filesystem::path &path, bool reloadable);
  bool addWadFile(const std::filesystem::path &path, bool reloadable);

  std::vector<LumpEntry> entries_;
};

} // namespace aster
