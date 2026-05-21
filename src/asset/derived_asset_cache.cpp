// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/derived_asset_cache.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace aster {

namespace {

std::string toHexByte(const unsigned char value) {
  std::ostringstream out;
  out << '$' << std::hex << std::nouppercase << static_cast<unsigned int>(value);
  return out.str();
}

std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

std::uint64_t fnv1a64(const std::string_view value) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

} // namespace

DerivedAssetCache::DerivedAssetCache(std::filesystem::path root_path,
                                     const DerivedAssetCacheBackend backend,
                                     JobGraphOptions job_options,
                                     DerivedAssetCachePolicy policy)
    : root_path_(std::move(root_path)), backend_(backend), policy_(std::move(policy)),
      jobs_(job_options) {}

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
    recordUsageLocked({"get", std::string(key), filePathForKey(key), out_data.size(), true,
                       false, true, {}});
    events_.emit({DerivedAssetCacheEventKind::Hit, std::string(key), filePathForKey(key),
                  out_data.size(), {}});
    return true;
  }
  ++stats_.misses;
  recordUsageLocked({"get", std::string(key), filePathForKey(key), 0u, false, false, true,
                     "miss"});
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
      recordUsageLocked({"buildAsync", key, filePathForKey(key), pending.data.size(), true, false,
                         true, {}});
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
                 recordUsageLocked({"buildAsync", key, filePathForKey(key), pending.data.size(),
                                    false, true, true, {}});
                 events_.emit({DerivedAssetCacheEventKind::Built, key, filePathForKey(key),
                               pending.data.size(), {}});
               } catch (const std::exception &error) {
                 std::lock_guard<std::mutex> lock(mutex_);
                 PendingBuild &pending = pending_[handle];
                 pending.complete = true;
                 pending.ok = false;
                 pending.diagnostic = error.what();
                 ++stats_.failed_builds;
                 recordUsageLocked({"buildAsync", key, filePathForKey(key), 0u, false, false,
                                    false, error.what()});
                 events_.emit(
                     {DerivedAssetCacheEventKind::Failed, key, filePathForKey(key), 0u, error.what()});
               } catch (...) {
                 std::lock_guard<std::mutex> lock(mutex_);
                 PendingBuild &pending = pending_[handle];
                 pending.complete = true;
                 pending.ok = false;
                 pending.diagnostic = "derived asset build threw an unknown exception";
                 ++stats_.failed_builds;
                 recordUsageLocked({"buildAsync", key, filePathForKey(key), 0u, false, false,
                                    false, pending.diagnostic});
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

DerivedAssetCacheRollup DerivedAssetCache::startRollup(std::string name) {
  return DerivedAssetCacheRollup(*this, std::move(name));
}

void DerivedAssetCache::clearMemory() {
  std::lock_guard<std::mutex> lock(mutex_);
  memory_entries_.clear();
}

void DerivedAssetCache::clearUsageRows() {
  std::lock_guard<std::mutex> lock(mutex_);
  usage_rows_.clear();
}

DerivedAssetCacheStats DerivedAssetCache::stats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

std::vector<DerivedAssetCacheUsageRow> DerivedAssetCache::usageRows() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return usage_rows_;
}

const DerivedAssetCachePolicy &DerivedAssetCache::policy() const noexcept {
  return policy_;
}

std::filesystem::path DerivedAssetCache::filePathForKey(const std::string_view key) const {
  if (root_path_.empty()) {
    return {};
  }
  std::filesystem::path path = root_path_;
  if (!policy_.namespace_prefix.empty()) {
    path /= sanitizeCacheKey(policy_.namespace_prefix);
  }
  return path / (storageKeyForKey(key) + ".astercache");
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
    if (policy_.treat_corruption_as_miss) {
      const_cast<DerivedAssetCache *>(this)->recordUsageLocked(
          {"read", std::string(key), path, 0u, false, false, false, "read failed"});
    }
    return false;
  }
  if (policy_.verify_reads) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size != data.size()) {
      if (policy_.treat_corruption_as_miss) {
        const_cast<DerivedAssetCache *>(this)->recordUsageLocked(
            {"read", std::string(key), path, data.size(), false, false, false,
             "verification failed"});
        return false;
      }
    }
  }
  out_data = data;
  if (usesMemory()) {
    const_cast<DerivedAssetCache *>(this)->memory_entries_[key_string] = std::move(data);
  }
  return true;
}

std::string DerivedAssetCache::storageKeyForKey(const std::string_view key) const {
  std::string storage_key = sanitizeCacheKey(key);
  if (policy_.max_key_length > 0u && storage_key.size() > policy_.max_key_length) {
    const std::string hash = hex64(fnv1a64(storage_key));
    const std::size_t prefix_length =
        policy_.max_key_length > hash.size() + 1u ? policy_.max_key_length - hash.size() - 1u : 0u;
    storage_key = storage_key.substr(0u, prefix_length) + "_" + hash;
  }
  return storage_key;
}

void DerivedAssetCache::putLocked(const std::string_view key, const DerivedAssetBytes &data,
                                  const DerivedAssetCachePutOptions options) {
  const std::string key_string(key);
  if (usesMemory()) {
    memory_entries_[key_string] = data;
  }
  if (usesFilesystem() && !options.transient && !policy_.transient_entries) {
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
  recordUsageLocked({"put", key_string, filePathForKey(key), data.size(), false, false, true, {}});
  events_.emit(
      {DerivedAssetCacheEventKind::Stored, key_string, filePathForKey(key), data.size(), {}});
}

void DerivedAssetCache::recordUsageLocked(DerivedAssetCacheUsageRow row) const {
  usage_rows_.push_back(std::move(row));
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

DerivedAssetCacheRollup::DerivedAssetCacheRollup(DerivedAssetCache &cache, std::string name)
    : cache_(&cache), name_(std::move(name)) {}

void DerivedAssetCacheRollup::add(const DerivedAssetAsyncHandle handle) {
  handles_.push_back(handle);
}

void DerivedAssetCacheRollup::wait() {
  if (cache_ != nullptr) {
    cache_->waitForIdle();
  }
}

DerivedAssetCacheRollupReport DerivedAssetCacheRollup::report() const {
  DerivedAssetCacheRollupReport result;
  result.name = name_;
  result.handle_count = handles_.size();
  if (cache_ == nullptr) {
    return result;
  }

  for (const DerivedAssetAsyncHandle handle : handles_) {
    if (!cache_->pollAsyncCompletion(handle)) {
      continue;
    }
    ++result.completed;
    DerivedAssetBytes bytes;
    bool data_was_built = false;
    if (cache_->getAsyncResult(handle, bytes, &data_was_built)) {
      result.bytes += bytes.size();
      if (data_was_built) {
        ++result.built;
      } else {
        ++result.hits;
      }
    } else {
      ++result.failed;
    }
  }
  return result;
}

const std::vector<DerivedAssetAsyncHandle> &DerivedAssetCacheRollup::handles() const noexcept {
  return handles_;
}

} // namespace aster
