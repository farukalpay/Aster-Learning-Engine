// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace aster::graphics_core7 {

struct PipelineWarmupRecord {
  std::uint64_t cache_key = 0u;
  std::string label;
  bool warmed = false;
};

struct PipelineWarmupReport {
  std::size_t requested = 0u;
  std::size_t warmed = 0u;
  std::size_t missing = 0u;
  std::vector<PipelineWarmupRecord> records;
};

class PipelineCacheWarmup {
public:
  void markWarmed(std::uint64_t cache_key);
  [[nodiscard]] PipelineWarmupReport inspect(const std::vector<PipelineWarmupRecord> &requested) const;

private:
  std::unordered_set<std::uint64_t> warmed_;
};

} // namespace aster::graphics_core7
