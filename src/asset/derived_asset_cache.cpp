// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/derived_asset_cache.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace aster {

namespace {

std::string toHexByte(const unsigned char value) {
  std::ostringstream out;
  out << '$' << std::hex << std::nouppercase << static_cast<unsigned int>(value);
  return out.str();
}

} // namespace

DerivedAssetCache::DerivedAssetCache(std::filesystem::path root_path,
                                     const DerivedAssetCacheBackend backend,
                                     JobGraphOptions job_options)
    : root_path_(std::move(root_path)), backend_(backend), jobs_(job_options) {}

std::string DerivedAssetCache::sanitizeCacheKey(const std::string_view key) {
  std::string result;
  result.reserve(key.size());
  for (const unsigned char c : key) {
    if (std::isalnum(c) || c == '_') {
      result.push_back(static_cast<char>(c));
    } else {
      result += toHexByte(c);
    }
  }
  return result;
}

std::string DerivedAssetCache::buildCacheKey(const std::string_view plugin_name,
                                             const std::string_view version,
                                             const std::string_view key_suffix) {
  std::string combined;
  combined.reserve(plugin_name.size() + version.size() + key_suffix.size() + 2u);
  combined.append(plugin_name);
  combined.push_back('_');
  combined.append(version);
  combined.push_back('_');
  combined.append(key_suffix);
  return sanitizeCacheKey(combined);
}

bool DerivedAssetCache::get(const std::string_view key, DerivedAssetBytes &out_data) {
  std::lock_guard<std::mutex> lock(mutex_);
  ++stats_.requests;
  if (getLocked(key, out_data)) {
    ++stats_.hits;
    stats_.bytes_read += out_data.size();
    events_.emit({DerivedAssetCacheEventKind::Hit, std::string(key), filePathForKey(key),
                  out_data.size(), {}});
    return true;
  }
  ++stats_.misses;
  events_.emit({DerivedAssetCacheEventKind::Miss, std::string(key), filePathForKey(key), 0u, {}});
  return false;
}

void DerivedAssetCache::put(const std::string_view key, const DerivedAssetBytes &data,
                            const DerivedAssetCachePutOptions options) {
  std::lock_guard<std::mutex> lock(mutex_);
  putLocked(key, data, options);
}

bool DerivedAssetCache::cachedDataProbablyExists(const std::string_view key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (usesMemory() && memory_entries_.find(std::string(key)) != memory_entries_.end()) {
    return true;
  }
  return usesFilesystem() && std::filesystem::exists(filePathForKey(key));
}

DerivedAssetAsyncHandle DerivedAssetCache::buildAsync(DerivedAssetBuildRequest request) {
  const std::string key = buildCacheKey(request.plugin_name, request.version, request.key_suffix);
  const DerivedAssetAsyncHandle handle = next_handle_++;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_[handle] = {.key = key};
    DerivedAssetBytes cached;
    ++stats_.requests;
    if (getLocked(key, cached)) {
      PendingBuild &pending = pending_[handle];
      pending.data = std::move(cached);
      pending.complete = true;
      pending.ok = true;
      pending.data_was_built = false;
      ++stats_.hits;
      stats_.bytes_read += pending.data.size();
      events_.emit(
          {DerivedAssetCacheEventKind::Hit, key, filePathForKey(key), pending.data.size(), {}});
      return handle;
    }
    ++stats_.misses;
  }

  jobs_.add({.name = "derived_asset_cache." + key,
             .priority = JobPriority::Normal,
             .run = [this, handle, key, request = std::move(request)](JobContext &) mutable {
               try {
                 DerivedAssetBytes built = request.build ? request.build() : DerivedAssetBytes{};
                 std::lock_guard<std::mutex> lock(mutex_);
                 putLocked(key, built, request.put_options);
                 PendingBuild &pending = pending_[handle];
                 pending.data = std::move(built);
                 pending.complete = true;
                 pending.ok = true;
                 pending.data_was_built = true;
                 ++stats_.builds;
                 events_.emit({DerivedAssetCacheEventKind::Built, key, filePathForKey(key),
                               pending.data.size(), {}});
               } catch (const std::exception &error) {
                 std::lock_guard<std::mutex> lock(mutex_);
                 PendingBuild &pending = pending_[handle];
                 pending.complete = true;
                 pending.ok = false;
                 pending.diagnostic = error.what();
                 ++stats_.failed_builds;
                 events_.emit(
                     {DerivedAssetCacheEventKind::Failed, key, filePathForKey(key), 0u, error.what()});
               } catch (...) {
                 std::lock_guard<std::mutex> lock(mutex_);
                 PendingBuild &pending = pending_[handle];
                 pending.complete = true;
                 pending.ok = false;
                 pending.diagnostic = "derived asset build threw an unknown exception";
                 ++stats_.failed_builds;
                 events_.emit({DerivedAssetCacheEventKind::Failed, key, filePathForKey(key), 0u,
                               pending.diagnostic});
               }
             }});
  ++queued_jobs_;
  return handle;
}

bool DerivedAssetCache::pollAsyncCompletion(const DerivedAssetAsyncHandle handle) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = pending_.find(handle);
  return found != pending_.end() && found->second.complete;
}

bool DerivedAssetCache::getAsyncResult(const DerivedAssetAsyncHandle handle,
                                       DerivedAssetBytes &out_data,
                                       bool *data_was_built) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = pending_.find(handle);
  if (found == pending_.end() || !found->second.complete || !found->second.ok) {
    return false;
  }
  out_data = found->second.data;
  if (data_was_built) {
    *data_was_built = found->second.data_was_built;
  }
  return true;
}

void DerivedAssetCache::waitForIdle() {
  if (queued_jobs_ == 0u) {
    return;
  }
  jobs_.run();
  jobs_.clear();
  queued_jobs_ = 0u;
}

void DerivedAssetCache::clearMemory() {
  std::lock_guard<std::mutex> lock(mutex_);
  memory_entries_.clear();
}

DerivedAssetCacheStats DerivedAssetCache::stats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

std::filesystem::path DerivedAssetCache::filePathForKey(const std::string_view key) const {
  if (root_path_.empty()) {
    return {};
  }
  return root_path_ / (std::string(key) + ".astercache");
}

bool DerivedAssetCache::usesMemory() const {
  return backend_ == DerivedAssetCacheBackend::Memory ||
         backend_ == DerivedAssetCacheBackend::MemoryAndFilesystem;
}

bool DerivedAssetCache::usesFilesystem() const {
  return backend_ == DerivedAssetCacheBackend::Filesystem ||
         backend_ == DerivedAssetCacheBackend::MemoryAndFilesystem;
}

bool DerivedAssetCache::getLocked(const std::string_view key, DerivedAssetBytes &out_data) const {
  const std::string key_string(key);
  if (usesMemory()) {
    const auto found = memory_entries_.find(key_string);
    if (found != memory_entries_.end()) {
      out_data = found->second;
      return true;
    }
  }
  if (!usesFilesystem()) {
    return false;
  }
  const std::filesystem::path path = filePathForKey(key);
  if (path.empty() || !std::filesystem::exists(path)) {
    return false;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  DerivedAssetBytes data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (!file.good() && !file.eof()) {
    return false;
  }
  out_data = data;
  if (usesMemory()) {
    const_cast<DerivedAssetCache *>(this)->memory_entries_[key_string] = std::move(data);
  }
  return true;
}

void DerivedAssetCache::putLocked(const std::string_view key, const DerivedAssetBytes &data,
                                  const DerivedAssetCachePutOptions options) {
  const std::string key_string(key);
  if (usesMemory()) {
    memory_entries_[key_string] = data;
  }
  if (usesFilesystem() && !options.transient) {
    const std::filesystem::path path = filePathForKey(key);
    if (!path.empty() && (options.put_even_if_exists || !std::filesystem::exists(path))) {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char *>(data.data()),
                 static_cast<std::streamsize>(data.size()));
      stats_.bytes_written += data.size();
    }
  }
  ++stats_.puts;
  events_.emit(
      {DerivedAssetCacheEventKind::Stored, key_string, filePathForKey(key), data.size(), {}});
}

const char *derivedAssetCacheEventKindName(const DerivedAssetCacheEventKind kind) {
  switch (kind) {
  case DerivedAssetCacheEventKind::Hit:
    return "hit";
  case DerivedAssetCacheEventKind::Miss:
    return "miss";
  case DerivedAssetCacheEventKind::Stored:
    return "stored";
  case DerivedAssetCacheEventKind::Built:
    return "built";
  case DerivedAssetCacheEventKind::Failed:
    return "failed";
  }
  return "unknown";
}

} // namespace aster
