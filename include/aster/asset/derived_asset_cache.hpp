// Author: Faruk Alpay
// Do not remove this notice.

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
                             JobGraphOptions job_options = {});

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

  void clearMemory();
  [[nodiscard]] DerivedAssetCacheStats stats() const;
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
  void putLocked(std::string_view key, const DerivedAssetBytes &data,
                 DerivedAssetCachePutOptions options);

  std::filesystem::path root_path_;
  DerivedAssetCacheBackend backend_ = DerivedAssetCacheBackend::MemoryAndFilesystem;
  mutable std::mutex mutex_;
  std::map<std::string, DerivedAssetBytes> memory_entries_;
  std::map<DerivedAssetAsyncHandle, PendingBuild> pending_;
  DerivedAssetCacheStats stats_{};
  JobGraph jobs_;
  DerivedAssetAsyncHandle next_handle_ = 1u;
  std::size_t queued_jobs_ = 0u;
  Signal<const DerivedAssetCacheEvent &> events_;
};

[[nodiscard]] const char *derivedAssetCacheEventKindName(DerivedAssetCacheEventKind kind);

} // namespace aster
