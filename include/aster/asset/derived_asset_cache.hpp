// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/job_graph.hpp"
#include "aster/core/signal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

using DerivedAssetBytes = std::vector<std::uint8_t>;
using DerivedAssetAsyncHandle = std::uint32_t;

class DerivedAssetCache;

struct DerivedAssetCachePolicy {
  bool verify_reads = false;
  bool treat_corruption_as_miss = true;
  bool transient_entries = false;
  std::size_t max_key_length = 180u;
  std::string namespace_prefix;
};

enum class DerivedAssetCacheBackend {
  Memory,
  Filesystem,
  MemoryAndFilesystem,
};

enum class DerivedAssetCacheEventKind {
  Hit,
  Miss,
  Stored,
  Built,
  Failed,
};

struct DerivedAssetCacheEvent {
  DerivedAssetCacheEventKind kind = DerivedAssetCacheEventKind::Hit;
  std::string key;
  std::filesystem::path path;
  std::size_t bytes = 0u;
  std::string message;
};

struct DerivedAssetCacheStats {
  std::uint64_t requests = 0u;
  std::uint64_t hits = 0u;
  std::uint64_t misses = 0u;
  std::uint64_t puts = 0u;
  std::uint64_t builds = 0u;
  std::uint64_t failed_builds = 0u;
  std::uint64_t bytes_read = 0u;
  std::uint64_t bytes_written = 0u;
};

struct DerivedAssetCacheUsageRow {
  std::string operation;
  std::string key;
  std::filesystem::path path;
  std::size_t bytes = 0u;
  bool hit = false;
  bool built = false;
  bool ok = true;
  std::string message;
};

struct DerivedAssetCacheRollupReport {
  std::string name;
  std::size_t handle_count = 0u;
  std::size_t completed = 0u;
  std::size_t hits = 0u;
  std::size_t built = 0u;
  std::size_t failed = 0u;
  std::size_t bytes = 0u;
};

class DerivedAssetCacheRollup {
public:
  explicit DerivedAssetCacheRollup(DerivedAssetCache &cache, std::string name = {});

  void add(DerivedAssetAsyncHandle handle);
  void wait();
  [[nodiscard]] DerivedAssetCacheRollupReport report() const;
  [[nodiscard]] const std::vector<DerivedAssetAsyncHandle> &handles() const noexcept;

private:
  DerivedAssetCache *cache_ = nullptr;
  std::string name_;
  std::vector<DerivedAssetAsyncHandle> handles_;
};

struct DerivedAssetCachePutOptions {
  bool put_even_if_exists = false;
  bool transient = false;
};

struct DerivedAssetBuildRequest {
  std::string plugin_name;
  std::string version;
  std::string key_suffix;
  std::function<DerivedAssetBytes()> build;
  DerivedAssetCachePutOptions put_options;
};

class DerivedAssetCache {
public:
  explicit DerivedAssetCache(std::filesystem::path root_path = {},
                             DerivedAssetCacheBackend backend =
                                 DerivedAssetCacheBackend::MemoryAndFilesystem,
                             JobGraphOptions job_options = {},
                             DerivedAssetCachePolicy policy = {});

  [[nodiscard]] static std::string sanitizeCacheKey(std::string_view key);
  [[nodiscard]] static std::string buildCacheKey(std::string_view plugin_name,
                                                 std::string_view version,
                                                 std::string_view key_suffix);

  [[nodiscard]] bool get(std::string_view key, DerivedAssetBytes &out_data);
  void put(std::string_view key, const DerivedAssetBytes &data,
           DerivedAssetCachePutOptions options = {});
  [[nodiscard]] bool cachedDataProbablyExists(std::string_view key) const;

  DerivedAssetAsyncHandle buildAsync(DerivedAssetBuildRequest request);
  [[nodiscard]] bool pollAsyncCompletion(DerivedAssetAsyncHandle handle) const;
  [[nodiscard]] bool getAsyncResult(DerivedAssetAsyncHandle handle, DerivedAssetBytes &out_data,
                                    bool *data_was_built = nullptr) const;
  void waitForIdle();
  [[nodiscard]] DerivedAssetCacheRollup startRollup(std::string name = {});

  void clearMemory();
  void clearUsageRows();
  [[nodiscard]] DerivedAssetCacheStats stats() const;
  [[nodiscard]] std::vector<DerivedAssetCacheUsageRow> usageRows() const;
  [[nodiscard]] const DerivedAssetCachePolicy &policy() const noexcept;
  [[nodiscard]] std::filesystem::path filePathForKey(std::string_view key) const;

  [[nodiscard]] Signal<const DerivedAssetCacheEvent &> &events() {
    return events_;
  }

private:
  struct PendingBuild {
    std::string key;
    DerivedAssetBytes data;
    bool complete = false;
    bool ok = false;
    bool data_was_built = false;
    std::string diagnostic;
  };

  [[nodiscard]] bool usesMemory() const;
  [[nodiscard]] bool usesFilesystem() const;
  [[nodiscard]] bool getLocked(std::string_view key, DerivedAssetBytes &out_data) const;
  [[nodiscard]] std::string storageKeyForKey(std::string_view key) const;
  void putLocked(std::string_view key, const DerivedAssetBytes &data,
                 DerivedAssetCachePutOptions options);
  void recordUsageLocked(DerivedAssetCacheUsageRow row) const;

  std::filesystem::path root_path_;
  DerivedAssetCacheBackend backend_ = DerivedAssetCacheBackend::MemoryAndFilesystem;
  DerivedAssetCachePolicy policy_{};
  mutable std::mutex mutex_;
  std::map<std::string, DerivedAssetBytes> memory_entries_;
  std::map<DerivedAssetAsyncHandle, PendingBuild> pending_;
  DerivedAssetCacheStats stats_{};
  JobGraph jobs_;
  DerivedAssetAsyncHandle next_handle_ = 1u;
  std::size_t queued_jobs_ = 0u;
  mutable std::vector<DerivedAssetCacheUsageRow> usage_rows_;
  Signal<const DerivedAssetCacheEvent &> events_;
};

[[nodiscard]] const char *derivedAssetCacheEventKindName(DerivedAssetCacheEventKind kind);

} // namespace aster
