// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/pipeline_cache_warmup.hpp"

namespace aster::graphics_core7 {

void PipelineCacheWarmup::markWarmed(const std::uint64_t cache_key) {
  if (cache_key != 0u) {
    warmed_.insert(cache_key);
  }
}

PipelineWarmupReport
PipelineCacheWarmup::inspect(const std::vector<PipelineWarmupRecord> &requested) const {
  PipelineWarmupReport report;
  report.requested = requested.size();
  report.records = requested;
  for (PipelineWarmupRecord &record : report.records) {
    record.warmed = record.cache_key != 0u && warmed_.contains(record.cache_key);
    if (record.warmed) {
      ++report.warmed;
    }
  }
  report.missing = report.requested > report.warmed ? report.requested - report.warmed : 0u;
  return report;
}

} // namespace aster::graphics_core7
