// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aster {

enum class TypedTraceDomain : std::uint32_t {
  Gameplay,
  Learning,
  Memory,
  Perceptual,
  Causality,
  Residency,
  Render,
  Asset,
  Ai,
  Benchmark,
  Tool,
};

enum class TypedTraceEventKind : std::uint32_t {
  Input,
  StateRead,
  StateWrite,
  ReducerApplied,
  MemoryRead,
  MemoryWrite,
  MemoryEvict,
  MemoryReplay,
  ScaffoldDecision,
  ProviderRequest,
  ProviderResponse,
  GraphNode,
  GraphEdge,
  BenchmarkCase,
  BenchmarkAblation,
  Stop,
  ValidationError,
};

struct TypedTraceEvent {
  TypedTraceDomain domain = TypedTraceDomain::Gameplay;
  TypedTraceEventKind kind = TypedTraceEventKind::ReducerApplied;
  std::uint64_t sequence = 0u;
  std::uint64_t tick = 0u;
  std::string subject;
  std::string key;
  std::string payload;
  std::uint64_t value_hash = 0u;
  std::uint64_t parent_trace_hash = 0u;
  std::uint64_t trace_hash = 0u;
};

[[nodiscard]] std::string_view typedTraceDomainName(TypedTraceDomain domain);
[[nodiscard]] std::string_view typedTraceEventKindName(TypedTraceEventKind kind);
[[nodiscard]] std::uint64_t hashTypedTraceEvent(const TypedTraceEvent &event,
                                                std::uint64_t previous_hash);

} // namespace aster
