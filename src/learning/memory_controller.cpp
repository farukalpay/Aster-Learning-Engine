// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/learning/memory_controller.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string_view>
#include <utility>

extern "C" {
std::uint32_t aster_runtime_memory_store_init(const char *db_path, std::size_t db_path_len,
                                              char *diagnostic, std::size_t diagnostic_len);
std::uint32_t aster_runtime_memory_store_trace_event(
    const char *db_path, std::size_t db_path_len, std::uint32_t domain, std::uint32_t kind,
    std::uint64_t sequence, std::uint64_t tick, const char *subject, std::size_t subject_len,
    const char *semantic_key, std::size_t semantic_key_len, const char *payload,
    std::size_t payload_len, std::uint64_t value_hash, std::uint64_t trace_hash,
    char *diagnostic, std::size_t diagnostic_len);
std::uint32_t aster_runtime_memory_store_decision(
    const char *db_path, std::size_t db_path_len, std::uint64_t sequence, std::uint64_t tick,
    std::uint32_t action, std::uint32_t status, const char *subject, std::size_t subject_len,
    const char *semantic_key, std::size_t semantic_key_len, const char *rationale,
    std::size_t rationale_len, float confidence, float success_score, std::uint64_t token_cost,
    std::uint64_t byte_cost, std::uint64_t saved_bytes, const char *provider_json,
    std::size_t provider_json_len, std::uint64_t decision_hash, char *diagnostic,
    std::size_t diagnostic_len);
std::uint32_t aster_runtime_memory_graph_query(
    const char *db_path, std::size_t db_path_len, const char *subject, std::size_t subject_len,
    const char *semantic_key, std::size_t semantic_key_len, std::size_t limit,
    char **out_json, char *diagnostic, std::size_t diagnostic_len);
std::uint32_t aster_runtime_generic_http_json(
    const char *url, std::size_t url_len, const char *method, std::size_t method_len,
    const char *headers_json, std::size_t headers_json_len, const char *body_json,
    std::size_t body_json_len, std::uint32_t timeout_ms, std::uint32_t *out_status_code,
    char **out_body, char *diagnostic, std::size_t diagnostic_len);
void aster_runtime_free_string(char *value);
}

namespace aster {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

[[nodiscard]] std::uint64_t appendHash(std::uint64_t hash, const std::uint64_t value) {
  for (std::size_t byte = 0u; byte < sizeof(value); ++byte) {
    hash ^= (value >> (byte * 8u)) & 0xffu;
    hash *= kFnvPrime;
  }
  return hash == 0u ? kFnvOffset : hash;
}

[[nodiscard]] std::uint64_t appendHash(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= kFnvPrime;
  }
  return appendHash(hash, static_cast<std::uint64_t>(value.size()));
}

[[nodiscard]] std::uint64_t decisionHash(const MemoryDecision &decision) {
  std::uint64_t hash = appendHash(kFnvOffset, "aster.memory.decision.v1");
  hash = appendHash(hash, decision.sequence);
  hash = appendHash(hash, decision.tick);
  hash = appendHash(hash, static_cast<std::uint64_t>(decision.action));
  hash = appendHash(hash, static_cast<std::uint64_t>(decision.status));
  hash = appendHash(hash, decision.subject);
  hash = appendHash(hash, decision.semantic_key);
  hash = appendHash(hash, decision.rationale);
  hash = appendHash(hash, decision.token_cost);
  hash = appendHash(hash, decision.byte_cost);
  hash = appendHash(hash, decision.saved_bytes);
  return hash;
}

[[nodiscard]] std::string jsonEscape(const std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8u);
  for (const char c : value) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

[[nodiscard]] bool containsAction(const std::vector<MemoryActionKind> &actions,
                                  const MemoryActionKind action) {
  return actions.empty() || std::find(actions.begin(), actions.end(), action) != actions.end();
}

[[nodiscard]] std::uint64_t budgetTokens(const MemoryControllerOptions &options,
                                         const MemoryControllerStepDesc &desc) {
  return desc.budget.token_budget != 0u ? desc.budget.token_budget : options.budget.token_budget;
}

[[nodiscard]] std::uint64_t budgetBytes(const MemoryControllerOptions &options,
                                        const MemoryControllerStepDesc &desc) {
  return desc.budget.byte_budget != 0u ? desc.budget.byte_budget : options.budget.byte_budget;
}

[[nodiscard]] std::string diagnosticFromBuffer(const char *buffer) {
  return buffer == nullptr ? std::string() : std::string(buffer);
}

[[nodiscard]] MemoryActionKind actionFromText(const std::string_view text,
                                              const MemoryActionKind fallback) {
  if (text == "read") {
    return MemoryActionKind::Read;
  }
  if (text == "write") {
    return MemoryActionKind::Write;
  }
  if (text == "evict") {
    return MemoryActionKind::Evict;
  }
  if (text == "replay") {
    return MemoryActionKind::Replay;
  }
  if (text == "scaffold") {
    return MemoryActionKind::Scaffold;
  }
  if (text == "stop") {
    return MemoryActionKind::Stop;
  }
  return fallback;
}

[[nodiscard]] std::string fieldFromJson(const std::string_view json, const std::string_view field) {
  const std::string needle = "\"" + std::string(field) + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string_view::npos) {
    return {};
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return {};
  }
  const std::size_t quote = json.find('"', colon + 1u);
  if (quote == std::string_view::npos) {
    return {};
  }
  std::string out;
  bool escaped = false;
  for (std::size_t index = quote + 1u; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      out += c;
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      break;
    }
    out += c;
  }
  return out;
}

[[nodiscard]] float numberFieldFromJson(const std::string_view json, const std::string_view field,
                                        const float fallback) {
  const std::string needle = "\"" + std::string(field) + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string_view::npos) {
    return fallback;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return fallback;
  }
  std::size_t begin = colon + 1u;
  while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin]))) {
    ++begin;
  }
  std::size_t end = begin;
  while (end < json.size() &&
         (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '.' ||
          json[end] == '-' || json[end] == '+')) {
    ++end;
  }
  float value = fallback;
  const auto result = std::from_chars(json.data() + begin, json.data() + end, value);
  return result.ec == std::errc{} ? value : fallback;
}

[[nodiscard]] std::string traceWindowJson(const WorldState &world, const std::size_t limit) {
  const std::vector<TypedTraceEvent> &events = world.typedTraceEvents();
  const std::size_t begin = events.size() > limit ? events.size() - limit : 0u;
  std::ostringstream out;
  out << "[";
  for (std::size_t index = begin; index < events.size(); ++index) {
    const TypedTraceEvent &event = events[index];
    if (index != begin) {
      out << ",";
    }
    out << "{\"sequence\":" << event.sequence << ",\"tick\":" << event.tick
        << ",\"domain\":\"" << typedTraceDomainName(event.domain) << "\",\"kind\":\""
        << typedTraceEventKindName(event.kind) << "\",\"subject\":\""
        << jsonEscape(event.subject) << "\",\"semantic_key\":\"" << jsonEscape(event.key)
        << "\",\"payload\":\"" << jsonEscape(event.payload) << "\",\"value_hash\":"
        << event.value_hash << ",\"trace_hash\":" << event.trace_hash << "}";
  }
  out << "]";
  return out.str();
}

[[nodiscard]] std::string requestJson(const MemoryControllerOptions &options,
                                      const WorldState &world,
                                      const MemoryControllerStepDesc &desc,
                                      const MemoryGraphQueryResult &graph) {
  std::ostringstream out;
  out << "{\"schema_version\":1,\"kind\":\"aster_memory_provider_request\",";
  out << "\"controller_id\":\"" << jsonEscape(options.controller_id) << "\",";
  out << "\"task\":\"" << jsonEscape(desc.task) << "\",";
  out << "\"objective_id\":\"" << jsonEscape(options.objective_id) << "\",";
  out << "\"subject\":\"" << jsonEscape(desc.subject) << "\",";
  out << "\"semantic_key\":\"" << jsonEscape(desc.semantic_key) << "\",";
  out << "\"budgets\":{\"token_budget\":" << budgetTokens(options, desc)
      << ",\"byte_budget\":" << budgetBytes(options, desc)
      << ",\"time_budget_ms\":" << (desc.budget.time_budget_ms > 0.0 ? desc.budget.time_budget_ms
                                                                       : options.budget.time_budget_ms)
      << "},";
  out << "\"allowed_actions\":[";
  const std::vector<MemoryActionKind> allowed =
      desc.allowed_actions.empty()
          ? std::vector<MemoryActionKind>{MemoryActionKind::Read, MemoryActionKind::Write,
                                          MemoryActionKind::Evict, MemoryActionKind::Replay,
                                          MemoryActionKind::Scaffold, MemoryActionKind::Stop}
          : desc.allowed_actions;
  for (std::size_t index = 0u; index < allowed.size(); ++index) {
    if (index != 0u) {
      out << ",";
    }
    out << "\"" << memoryActionKindName(allowed[index]) << "\"";
  }
  out << "],\"trace_window\":" << traceWindowJson(world, options.trace_window) << ",";
  out << "\"graph_query\":" << (graph.json.empty() ? "{}" : graph.json) << ",";
  out << "\"provider_extra\":" << (options.provider.extra_json.empty() ? "{}"
                                                                       : options.provider.extra_json);
  out << "}";
  return out.str();
}

} // namespace

std::string_view memoryActionKindName(const MemoryActionKind kind) {
  switch (kind) {
  case MemoryActionKind::Read:
    return "read";
  case MemoryActionKind::Write:
    return "write";
  case MemoryActionKind::Evict:
    return "evict";
  case MemoryActionKind::Replay:
    return "replay";
  case MemoryActionKind::Scaffold:
    return "scaffold";
  case MemoryActionKind::Stop:
    return "stop";
  }
  return "stop";
}

std::string_view memoryDecisionStatusName(const MemoryDecisionStatus status) {
  switch (status) {
  case MemoryDecisionStatus::Accepted:
    return "accepted";
  case MemoryDecisionStatus::Rejected:
    return "rejected";
  case MemoryDecisionStatus::Blocked:
    return "blocked";
  case MemoryDecisionStatus::ProviderError:
    return "provider_error";
  }
  return "blocked";
}

MemoryController::MemoryController(MemoryControllerOptions options) {
  setOptions(std::move(options));
}

void MemoryController::setOptions(MemoryControllerOptions options) {
  if (options.trace_window == 0u) {
    options.trace_window = 16u;
  }
  if (options.provider.method.empty()) {
    options.provider.method = "POST";
  }
  options_ = std::move(options);
}

const MemoryControllerOptions &MemoryController::options() const noexcept {
  return options_;
}

MemoryDecision MemoryController::step(WorldState &world, const MemoryControllerStepDesc &desc) {
  persistTraceWindow(world);
  const MemoryGraphQueryResult graph =
      queryGraph({.store_path = options_.store_path,
                  .subject = desc.subject,
                  .semantic_key = desc.semantic_key,
                  .limit = options_.trace_window});

  MemoryDecision decision =
      !options_.provider.url.empty() ? providerDecision(world, desc, graph)
                                     : reducerDecision(world, desc, graph);
  if (options_.provider.require_provider && options_.provider.url.empty()) {
    decision.status = MemoryDecisionStatus::Blocked;
    decision.provider_status = "provider_url_missing";
    decision.rationale = "memory provider is required but no provider URL was configured";
  }
  decision.sequence = decisions_.size() + 1u;
  decision.tick = world.currentTick();
  decision.subject = decision.subject.empty() ? desc.subject : decision.subject;
  decision.semantic_key = decision.semantic_key.empty() ? desc.semantic_key : decision.semantic_key;
  decision.decision_hash = decisionHash(decision);

  const TypedTraceEventKind trace_kind =
      decision.action == MemoryActionKind::Read      ? TypedTraceEventKind::MemoryRead
      : decision.action == MemoryActionKind::Write   ? TypedTraceEventKind::MemoryWrite
      : decision.action == MemoryActionKind::Evict   ? TypedTraceEventKind::MemoryEvict
      : decision.action == MemoryActionKind::Replay  ? TypedTraceEventKind::MemoryReplay
      : decision.action == MemoryActionKind::Scaffold ? TypedTraceEventKind::ScaffoldDecision
                                                      : TypedTraceEventKind::Stop;
  world.appendTypedTrace({.domain = TypedTraceDomain::Memory,
                          .kind = trace_kind,
                          .tick = world.currentTick(),
                          .subject = decision.subject,
                          .key = decision.semantic_key,
                          .payload = decision.rationale,
                          .value_hash = decision.decision_hash});
  persistDecision(decision, decision.provider_response_json);
  decisions_.push_back(decision);
  return decision;
}

MemoryGraphQueryResult MemoryController::queryGraph(const MemoryGraphQueryDesc &desc) const {
  MemoryGraphQueryResult result;
  const std::string db_path = desc.store_path.empty() ? std::string() : desc.store_path.string();
  if (db_path.empty()) {
    result.diagnostic = "memory graph store path is empty";
    return result;
  }
  char diagnostic[512]{};
  char *json = nullptr;
  const std::uint32_t ok = aster_runtime_memory_graph_query(
      db_path.data(), db_path.size(), desc.subject.data(), desc.subject.size(),
      desc.semantic_key.data(), desc.semantic_key.size(), desc.limit, &json, diagnostic,
      sizeof(diagnostic));
  if (ok == 0u) {
    result.diagnostic = diagnosticFromBuffer(diagnostic);
    return result;
  }
  if (json != nullptr) {
    result.json = json;
    aster_runtime_free_string(json);
  }
  result.node_count = static_cast<std::size_t>(
      numberFieldFromJson(result.json, "node_count", 0.0f));
  result.edge_count = static_cast<std::size_t>(
      numberFieldFromJson(result.json, "edge_count", 0.0f));
  result.conflict_count = static_cast<std::size_t>(
      numberFieldFromJson(result.json, "conflict_count", 0.0f));
  result.query_hash = appendHash(appendHash(kFnvOffset, result.json), result.node_count);
  return result;
}

MemoryBenchmarkReport MemoryController::benchmarkReport(std::string suite_id) const {
  MemoryBenchmarkReport report;
  report.suite_id = suite_id.empty() ? options_.objective_id : std::move(suite_id);
  report.store_path = options_.store_path;
  report.case_count = decisions_.size();
  report.ablation_count = std::count_if(decisions_.begin(), decisions_.end(),
                                        [](const MemoryDecision &decision) {
                                          return decision.action == MemoryActionKind::Evict;
                                        });
  report.regression_replay_count = std::count_if(decisions_.begin(), decisions_.end(),
                                                 [](const MemoryDecision &decision) {
                                                   return decision.action == MemoryActionKind::Replay;
                                                 });
  const std::size_t accepted = std::count_if(decisions_.begin(), decisions_.end(),
                                            [](const MemoryDecision &decision) {
                                              return decision.status == MemoryDecisionStatus::Accepted;
                                            });
  report.score = decisions_.empty()
                     ? 0.0f
                     : static_cast<float>(accepted) / static_cast<float>(decisions_.size());
  report.blocked = std::any_of(decisions_.begin(), decisions_.end(),
                               [](const MemoryDecision &decision) {
                                 return decision.status == MemoryDecisionStatus::Blocked;
                               });
  report.passed = !report.blocked && report.score >= 0.75f;
  report.report_hash = appendHash(appendHash(kFnvOffset, report.suite_id), report.case_count);
  report.diagnostic = report.blocked ? "memory benchmark blocked"
                       : report.passed ? "memory benchmark passed"
                                       : "memory benchmark below passing score";
  return report;
}

const std::vector<MemoryDecision> &MemoryController::decisions() const noexcept {
  return decisions_;
}

MemoryDecision MemoryController::reducerDecision(const WorldState &world,
                                                 const MemoryControllerStepDesc &desc,
                                                 const MemoryGraphQueryResult &graph) const {
  MemoryDecision decision;
  decision.subject = desc.subject;
  decision.semantic_key = desc.semantic_key;
  decision.confidence = 0.62f;
  decision.success_score = graph.conflict_count == 0u ? 0.72f : 0.45f;
  decision.token_cost = std::min<std::uint64_t>(budgetTokens(options_, desc), 128u);
  decision.byte_cost = static_cast<std::uint64_t>(traceWindowJson(world, options_.trace_window).size());
  const std::uint64_t bytes = budgetBytes(options_, desc);
  if (bytes != 0u && decision.byte_cost > bytes && containsAction(desc.allowed_actions, MemoryActionKind::Evict)) {
    decision.action = MemoryActionKind::Evict;
    decision.saved_bytes = decision.byte_cost - bytes;
    decision.rationale = "trace window exceeds memory byte budget";
  } else if (graph.conflict_count > 0u && containsAction(desc.allowed_actions, MemoryActionKind::Replay)) {
    decision.action = MemoryActionKind::Replay;
    decision.rationale = "semantic memory conflict requires replay before write";
  } else if (containsAction(desc.allowed_actions, MemoryActionKind::Write) && !desc.semantic_key.empty()) {
    decision.action = MemoryActionKind::Write;
    decision.rationale = "typed trace window produced a durable memory write";
  } else if (containsAction(desc.allowed_actions, MemoryActionKind::Read)) {
    decision.action = MemoryActionKind::Read;
    decision.rationale = "read graph memory for current trace subject";
  } else {
    decision.action = MemoryActionKind::Stop;
    decision.rationale = "no allowed memory action applies";
  }
  return decision;
}

MemoryDecision MemoryController::providerDecision(const WorldState &world,
                                                  const MemoryControllerStepDesc &desc,
                                                  const MemoryGraphQueryResult &graph) const {
  MemoryDecision decision;
  decision.subject = desc.subject;
  decision.semantic_key = desc.semantic_key;
  const std::string body = requestJson(options_, world, desc, graph);
  char diagnostic[1024]{};
  char *response_body = nullptr;
  std::uint32_t status_code = 0u;
  const std::uint32_t ok = aster_runtime_generic_http_json(
      options_.provider.url.data(), options_.provider.url.size(), options_.provider.method.data(),
      options_.provider.method.size(), options_.provider.headers_json.data(),
      options_.provider.headers_json.size(), body.data(), body.size(), options_.provider.timeout_ms,
      &status_code, &response_body, diagnostic, sizeof(diagnostic));
  decision.provider_status_code = status_code;
  if (ok == 0u) {
    decision.status = MemoryDecisionStatus::ProviderError;
    decision.provider_status = diagnosticFromBuffer(diagnostic);
    decision.rationale = "memory provider request failed";
    decision.action = MemoryActionKind::Stop;
    return decision;
  }
  std::string response;
  if (response_body != nullptr) {
    response = response_body;
    aster_runtime_free_string(response_body);
  }
  decision.provider_response_json = response;
  decision.provider_status = "http_" + std::to_string(status_code);
  decision.action = actionFromText(fieldFromJson(response, "action"), MemoryActionKind::Stop);
  decision.confidence = numberFieldFromJson(response, "confidence", 0.5f);
  decision.success_score = numberFieldFromJson(response, "success_score", decision.confidence);
  decision.rationale = fieldFromJson(response, "rationale");
  if (decision.rationale.empty()) {
    decision.rationale = "provider returned " + std::string(memoryActionKindName(decision.action));
  }
  if (!containsAction(desc.allowed_actions, decision.action)) {
    decision.status = MemoryDecisionStatus::Rejected;
    decision.rationale = "provider selected an action outside the allowed action set";
  }
  decision.byte_cost = static_cast<std::uint64_t>(response.size());
  decision.token_cost = static_cast<std::uint64_t>(std::max<std::size_t>(1u, response.size() / 4u));
  return decision;
}

void MemoryController::persistTraceWindow(const WorldState &world) const {
  if (options_.store_path.empty()) {
    return;
  }
  const std::string db_path = options_.store_path.string();
  char diagnostic[512]{};
  if (aster_runtime_memory_store_init(db_path.data(), db_path.size(), diagnostic,
                                      sizeof(diagnostic)) == 0u) {
    return;
  }
  const std::vector<TypedTraceEvent> &events = world.typedTraceEvents();
  const std::size_t begin =
      events.size() > options_.trace_window ? events.size() - options_.trace_window : 0u;
  for (std::size_t index = begin; index < events.size(); ++index) {
    const TypedTraceEvent &event = events[index];
    (void)aster_runtime_memory_store_trace_event(
        db_path.data(), db_path.size(), static_cast<std::uint32_t>(event.domain),
        static_cast<std::uint32_t>(event.kind), event.sequence, event.tick, event.subject.data(),
        event.subject.size(), event.key.data(), event.key.size(), event.payload.data(),
        event.payload.size(), event.value_hash, event.trace_hash, diagnostic, sizeof(diagnostic));
  }
}

void MemoryController::persistDecision(const MemoryDecision &decision,
                                       std::string provider_json) const {
  if (options_.store_path.empty()) {
    return;
  }
  const std::string db_path = options_.store_path.string();
  char diagnostic[512]{};
  (void)aster_runtime_memory_store_decision(
      db_path.data(), db_path.size(), decision.sequence, decision.tick,
      static_cast<std::uint32_t>(decision.action), static_cast<std::uint32_t>(decision.status),
      decision.subject.data(), decision.subject.size(), decision.semantic_key.data(),
      decision.semantic_key.size(), decision.rationale.data(), decision.rationale.size(),
      decision.confidence, decision.success_score, decision.token_cost, decision.byte_cost,
      decision.saved_bytes, provider_json.data(), provider_json.size(), decision.decision_hash,
      diagnostic, sizeof(diagnostic));
}

} // namespace aster
