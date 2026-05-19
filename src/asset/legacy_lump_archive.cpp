// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/legacy_lump_archive.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace {

std::uint32_t readLe32(const std::array<unsigned char, 4> bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

std::string readName8(const char *bytes) {
  std::string name;
  for (int i = 0; i < 8 && bytes[i] != '\0'; ++i) {
    name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(bytes[i]))));
  }
  return name;
}

} // namespace

namespace aster {

void LegacyLumpArchive::clear() {
  entries_.clear();
}

bool LegacyLumpArchive::addFile(const std::filesystem::path &path, const bool reloadable) {
  const std::string extension = path.extension().string();
  std::string lower;
  lower.reserve(extension.size());
  for (const char ch : extension) {
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  if (lower == ".wad") {
    return addWadFile(path, reloadable);
  }
  return addSingleFile(path, reloadable);
}

bool LegacyLumpArchive::reload() {
  bool reloaded = false;
  for (LumpEntry &entry : entries_) {
    if (!entry.record.reloadable || entry.cache.empty()) {
      continue;
    }
    entry.cache = read(&entry - entries_.data());
    entry.record.cached = true;
    reloaded = true;
  }
  return reloaded;
}

std::size_t LegacyLumpArchive::size() const {
  return entries_.size();
}

const LegacyLumpRecord *LegacyLumpArchive::record(const std::size_t index) const {
  if (index >= entries_.size()) {
    return nullptr;
  }
  return &entries_[index].record;
}

std::optional<std::size_t> LegacyLumpArchive::find(const std::string_view name) const {
  const std::string canonical = canonicalName(name);
  for (std::size_t i = entries_.size(); i > 0; --i) {
    if (entries_[i - 1u].record.name == canonical) {
      return i - 1u;
    }
  }
  return std::nullopt;
}

std::size_t LegacyLumpArchive::require(const std::string_view name) const {
  const std::optional<std::size_t> index = find(name);
  if (!index.has_value()) {
    throw std::out_of_range("Legacy lump was not found: " + std::string(name));
  }
  return *index;
}

std::vector<std::uint8_t> LegacyLumpArchive::read(const std::size_t index) const {
  if (index >= entries_.size()) {
    throw std::out_of_range("Legacy lump index is out of range.");
  }
  const LegacyLumpRecord &record = entries_[index].record;
  std::ifstream file(record.source, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open legacy lump source: " + record.source.string());
  }
  file.seekg(static_cast<std::streamoff>(record.offset), std::ios::beg);
  std::vector<std::uint8_t> bytes(record.size);
  if (!bytes.empty()) {
    file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!file && file.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error("Failed to read complete legacy lump: " + record.name);
  }
  return bytes;
}

const std::vector<std::uint8_t> &LegacyLumpArchive::cache(const std::size_t index) {
  if (index >= entries_.size()) {
    throw std::out_of_range("Legacy lump index is out of range.");
  }
  LumpEntry &entry = entries_[index];
  if (entry.cache.empty() && entry.record.size > 0u) {
    entry.cache = read(index);
  }
  entry.record.cached = true;
  return entry.cache;
}

std::vector<LegacyLumpProfileRow> LegacyLumpArchive::profile() const {
  std::vector<LegacyLumpProfileRow> rows;
  rows.reserve(entries_.size());
  for (const LumpEntry &entry : entries_) {
    rows.push_back({entry.record.name, entry.record.size, entry.record.cached,
                    entry.record.reloadable});
  }
  return rows;
}

std::string LegacyLumpArchive::canonicalName(const std::string_view name) {
  std::string out;
  out.reserve(8u);
  for (const char ch : name) {
    if (ch == '.' || out.size() == 8u) {
      break;
    }
    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
  }
  return out;
}

bool LegacyLumpArchive::addSingleFile(const std::filesystem::path &path, const bool reloadable) {
  std::error_code error;
  const auto bytes = std::filesystem::file_size(path, error);
  if (error) {
    return false;
  }
  entries_.push_back({LegacyLumpRecord{canonicalName(path.stem().string()), path, 0u,
                                       static_cast<std::uint32_t>(bytes), reloadable, false},
                      {}});
  return true;
}

bool LegacyLumpArchive::addWadFile(const std::filesystem::path &path, const bool reloadable) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  char id[4]{};
  file.read(id, 4);
  if (std::string_view(id, 4) != "IWAD" && std::string_view(id, 4) != "PWAD") {
    return false;
  }
  std::array<unsigned char, 4> count_bytes{};
  std::array<unsigned char, 4> table_bytes{};
  file.read(reinterpret_cast<char *>(count_bytes.data()), 4);
  file.read(reinterpret_cast<char *>(table_bytes.data()), 4);
  const std::uint32_t count = readLe32(count_bytes);
  const std::uint32_t table_offset = readLe32(table_bytes);
  file.seekg(static_cast<std::streamoff>(table_offset), std::ios::beg);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::array<unsigned char, 4> offset_bytes{};
    std::array<unsigned char, 4> size_bytes{};
    char name[8]{};
    file.read(reinterpret_cast<char *>(offset_bytes.data()), 4);
    file.read(reinterpret_cast<char *>(size_bytes.data()), 4);
    file.read(name, 8);
    if (!file) {
      return false;
    }
    entries_.push_back(
        {LegacyLumpRecord{readName8(name), path, readLe32(offset_bytes), readLe32(size_bytes),
                          reloadable, false},
         {}});
  }
  return true;
}

} // namespace aster
