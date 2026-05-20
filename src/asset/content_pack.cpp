// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/content_pack.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace aster {

void AsterContentPack::clear() {
  entries_.clear();
  next_sequence_ = 1u;
}

bool AsterContentPack::mountFile(const std::filesystem::path &path,
                                 const std::string_view mount_name,
                                 const int mount_priority,
                                 const bool reloadable) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error) {
    return false;
  }
  const std::uintmax_t bytes = std::filesystem::file_size(path, error);
  if (error || bytes > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }

  const std::string name =
      canonicalName(mount_name.empty() ? path.stem().generic_string() : std::string(mount_name));
  if (name.empty()) {
    return false;
  }
  entries_.push_back({.record = {name,
                                 path,
                                 0u,
                                 static_cast<std::uint32_t>(bytes),
                                 reloadable,
                                 false,
                                 mount_priority,
                                 AsterContentPackEntryKind::File},
                      .cache = {},
                      .sequence = next_sequence_++});
  return true;
}

bool AsterContentPack::mountLumpArchive(const std::filesystem::path &path,
                                        const int mount_priority,
                                        const bool reloadable) {
  LegacyLumpArchive archive;
  if (!archive.addFile(path, reloadable)) {
    return false;
  }
  return mountLegacyArchive(archive, mount_priority);
}

bool AsterContentPack::mountLegacyArchive(const LegacyLumpArchive &archive,
                                          const int mount_priority) {
  bool mounted_any = false;
  for (std::size_t index = 0u; index < archive.size(); ++index) {
    const LegacyLumpRecord *source = archive.record(index);
    if (source == nullptr || source->name.empty()) {
      continue;
    }
    entries_.push_back({.record = {canonicalName(source->name),
                                   source->source,
                                   source->offset,
                                   source->size,
                                   source->reloadable,
                                   false,
                                   mount_priority,
                                   AsterContentPackEntryKind::Lump},
                        .cache = {},
                        .sequence = next_sequence_++});
    mounted_any = true;
  }
  return mounted_any;
}

bool AsterContentPack::reload() {
  bool reloaded = false;
  for (Entry &entry : entries_) {
    if (!entry.record.reloadable || entry.cache.empty()) {
      continue;
    }
    entry.cache = read(&entry - entries_.data());
    entry.record.cached = true;
    reloaded = true;
  }
  return reloaded;
}

std::size_t AsterContentPack::size() const noexcept {
  return entries_.size();
}

const AsterContentPackRecord *AsterContentPack::record(const std::size_t index) const {
  return index < entries_.size() ? &entries_[index].record : nullptr;
}

std::optional<std::size_t> AsterContentPack::find(const std::string_view name) const {
  const std::string canonical = canonicalName(name);
  std::optional<std::size_t> best;
  for (std::size_t index = 0u; index < entries_.size(); ++index) {
    const Entry &entry = entries_[index];
    if (entry.record.name != canonical) {
      continue;
    }
    if (!best) {
      best = index;
      continue;
    }
    const Entry &current = entries_[*best];
    if (entry.record.mount_priority > current.record.mount_priority ||
        (entry.record.mount_priority == current.record.mount_priority &&
         entry.sequence > current.sequence)) {
      best = index;
    }
  }
  return best;
}

std::size_t AsterContentPack::require(const std::string_view name) const {
  const std::optional<std::size_t> index = find(name);
  if (!index) {
    throw std::out_of_range("Aster content pack entry was not found: " + std::string(name));
  }
  return *index;
}

std::vector<std::uint8_t> AsterContentPack::read(const std::size_t index) const {
  if (index >= entries_.size()) {
    throw std::out_of_range("Aster content pack index is out of range.");
  }
  const AsterContentPackRecord &entry = entries_[index].record;
  std::ifstream file(entry.source, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open Aster content pack source: " +
                             entry.source.string());
  }
  file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
  std::vector<std::uint8_t> bytes(entry.size);
  if (!bytes.empty()) {
    file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!file && file.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error("Failed to read Aster content pack entry: " + entry.name);
  }
  return bytes;
}

const std::vector<std::uint8_t> &AsterContentPack::cache(const std::size_t index) {
  if (index >= entries_.size()) {
    throw std::out_of_range("Aster content pack index is out of range.");
  }
  Entry &entry = entries_[index];
  if (entry.cache.empty() && entry.record.size > 0u) {
    entry.cache = read(index);
  }
  entry.record.cached = true;
  return entry.cache;
}

std::vector<AsterContentPackProfileRow> AsterContentPack::profile() const {
  std::vector<AsterContentPackProfileRow> rows;
  rows.reserve(entries_.size());
  for (const Entry &entry : entries_) {
    rows.push_back({entry.record.name,
                    entry.record.size,
                    entry.record.cached,
                    entry.record.reloadable,
                    entry.record.mount_priority,
                    entry.record.kind,
                    entry.record.source});
  }
  std::sort(rows.begin(), rows.end(),
            [](const AsterContentPackProfileRow &lhs, const AsterContentPackProfileRow &rhs) {
              if (lhs.mount_priority != rhs.mount_priority) {
                return lhs.mount_priority > rhs.mount_priority;
              }
              return lhs.name < rhs.name;
            });
  return rows;
}

std::string AsterContentPack::canonicalName(const std::string_view name) {
  return LegacyLumpArchive::canonicalName(name);
}

void AsterPackIndex::clear() {
  records_.clear();
}

void AsterPackIndex::addRecord(AsterContentPackRecord record) {
  record.name = AsterContentPack::canonicalName(record.name);
  if (!record.name.empty()) {
    records_.push_back(std::move(record));
  }
}

void AsterPackIndex::mount(const AsterContentPack &pack) {
  for (std::size_t index = 0u; index < pack.size(); ++index) {
    const AsterContentPackRecord *record = pack.record(index);
    if (record != nullptr) {
      addRecord(*record);
    }
  }
}

const AsterContentPackRecord *AsterPackIndex::find(const std::string_view name) const {
  const std::string canonical = AsterContentPack::canonicalName(name);
  const AsterContentPackRecord *best = nullptr;
  for (const AsterContentPackRecord &record : records_) {
    if (record.name != canonical) {
      continue;
    }
    if (best == nullptr || record.mount_priority > best->mount_priority) {
      best = &record;
    }
  }
  return best;
}

std::vector<AsterContentPackRecord> AsterPackIndex::records() const {
  return records_;
}

std::size_t AsterPackIndex::size() const noexcept {
  return records_.size();
}

const char *asterContentPackEntryKindName(const AsterContentPackEntryKind kind) {
  switch (kind) {
  case AsterContentPackEntryKind::File:
    return "file";
  case AsterContentPackEntryKind::Lump:
    return "lump";
  }
  return "unknown";
}

} // namespace aster
