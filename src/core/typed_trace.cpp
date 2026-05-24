// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/typed_trace.hpp"

#include <cstddef>

namespace aster {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

[[nodiscard]] std::uint64_t append(std::uint64_t hash, const std::uint64_t value) {
  for (std::size_t byte = 0u; byte < sizeof(value); ++byte) {
    hash ^= (value >> (byte * 8u)) & 0xffu;
    hash *= kFnvPrime;
  }
  return hash == 0u ? kFnvOffset : hash;
}

[[nodiscard]] std::uint64_t append(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= kFnvPrime;
  }
  return append(hash, static_cast<std::uint64_t>(value.size()));
}

} // namespace

std::string_view typedTraceDomainName(const TypedTraceDomain domain) {
  switch (domain) {
  case TypedTraceDomain::Gameplay:
    return "gameplay";
  case TypedTraceDomain::Learning:
    return "learning";
  case TypedTraceDomain::Memory:
    return "memory";
  case TypedTraceDomain::Perceptual:
    return "perceptual";
  case TypedTraceDomain::Causality:
    return "causality";
  case TypedTraceDomain::Residency:
    return "residency";
  case TypedTraceDomain::Render:
    return "render";
  case TypedTraceDomain::Asset:
    return "asset";
  case TypedTraceDomain::Ai:
    return "ai";
  case TypedTraceDomain::Benchmark:
    return "benchmark";
  case TypedTraceDomain::Tool:
    return "tool";
  }
  return "unknown";
}

std::string_view typedTraceEventKindName(const TypedTraceEventKind kind) {
  switch (kind) {
  case TypedTraceEventKind::Input:
    return "input";
  case TypedTraceEventKind::StateRead:
    return "state_read";
  case TypedTraceEventKind::StateWrite:
    return "state_write";
  case TypedTraceEventKind::ReducerApplied:
    return "reducer_applied";
  case TypedTraceEventKind::MemoryRead:
    return "memory_read";
  case TypedTraceEventKind::MemoryWrite:
    return "memory_write";
  case TypedTraceEventKind::MemoryEvict:
    return "memory_evict";
  case TypedTraceEventKind::MemoryReplay:
    return "memory_replay";
  case TypedTraceEventKind::ScaffoldDecision:
    return "scaffold_decision";
  case TypedTraceEventKind::ProviderRequest:
    return "provider_request";
  case TypedTraceEventKind::ProviderResponse:
    return "provider_response";
  case TypedTraceEventKind::GraphNode:
    return "graph_node";
  case TypedTraceEventKind::GraphEdge:
    return "graph_edge";
  case TypedTraceEventKind::BenchmarkCase:
    return "benchmark_case";
  case TypedTraceEventKind::BenchmarkAblation:
    return "benchmark_ablation";
  case TypedTraceEventKind::Stop:
    return "stop";
  case TypedTraceEventKind::ValidationError:
    return "validation_error";
  }
  return "unknown";
}

std::uint64_t hashTypedTraceEvent(const TypedTraceEvent &event,
                                  const std::uint64_t previous_hash) {
  std::uint64_t hash = previous_hash == 0u ? kFnvOffset : previous_hash;
  hash = append(hash, "aster.typed-trace.v1");
  hash = append(hash, static_cast<std::uint64_t>(event.domain));
  hash = append(hash, static_cast<std::uint64_t>(event.kind));
  hash = append(hash, event.sequence);
  hash = append(hash, event.tick);
  hash = append(hash, event.subject);
  hash = append(hash, event.key);
  hash = append(hash, event.payload);
  hash = append(hash, event.value_hash);
  hash = append(hash, event.parent_trace_hash);
  return hash == 0u ? kFnvOffset : hash;
}

} // namespace aster
