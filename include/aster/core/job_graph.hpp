// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

using JobId = std::uint64_t;

enum class JobPriority : std::uint32_t {
  Background = 0u,
  Normal = 1u,
  High = 2u,
};

enum class JobLane : std::uint32_t {
  Main,
  Render,
  Io,
  Background,
  Worker,
};

enum class JobStatus : std::uint32_t {
  Pending,
  Running,
  Complete,
  Failed,
  Skipped,
};

struct JobContext {
  JobId id = 0u;
  std::string_view name;
  std::uint32_t worker_index = 0u;
  bool deterministic = false;
};

struct JobDesc {
  std::string name;
  JobPriority priority = JobPriority::Normal;
  JobLane lane = JobLane::Worker;
  std::vector<JobId> dependencies;
  std::function<void(JobContext &)> run;
};

struct JobRecord {
  JobId id = 0u;
  std::string name;
  JobPriority priority = JobPriority::Normal;
  JobLane lane = JobLane::Worker;
  JobStatus status = JobStatus::Pending;
  std::vector<JobId> dependencies;
  std::string diagnostic;
};

enum class JobTraceEventKind : std::uint32_t {
  Queued,
  Running,
  Completed,
  Failed,
  Skipped,
};

struct JobTraceEvent {
  JobTraceEventKind kind = JobTraceEventKind::Queued;
  JobId id = 0u;
  std::string name;
  JobLane lane = JobLane::Worker;
  JobStatus status = JobStatus::Pending;
  std::uint32_t worker_index = 0u;
  std::uint64_t sequence = 0u;
  std::string diagnostic;
};

struct JobGraphOptions {
  std::size_t worker_count = 0u;
  bool deterministic = false;
  bool profile_jobs = true;
};

struct JobGraphDiagnostics {
  std::size_t queued_jobs = 0u;
  std::size_t executed_jobs = 0u;
  std::size_t failed_jobs = 0u;
  std::size_t skipped_jobs = 0u;
  std::size_t dependency_cycles = 0u;
  std::size_t max_ready_jobs = 0u;
  std::vector<std::string> diagnostics;
};

class JobGraph {
public:
  explicit JobGraph(JobGraphOptions options = {});

  JobId add(JobDesc desc);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const JobRecord *record(JobId id) const;
  [[nodiscard]] std::vector<JobRecord> records() const;
  [[nodiscard]] std::vector<JobTraceEvent> traceEvents() const;
  void clearTrace();

  JobGraphDiagnostics run();
  void clear();

  template <typename Fn>
  JobGraphDiagnostics parallelFor(std::string name, std::size_t count, std::size_t grain_size,
                                  Fn &&fn) {
    if (count == 0u) {
      return {};
    }
    if (grain_size == 0u) {
      grain_size = 1u;
    }
    for (std::size_t begin = 0u; begin < count; begin += grain_size) {
      const std::size_t end = begin + grain_size < count ? begin + grain_size : count;
      add({.name = name + "[" + std::to_string(begin) + ":" + std::to_string(end) + "]",
           .priority = JobPriority::Normal,
           .run = [begin, end, fn](JobContext &) mutable {
             for (std::size_t index = begin; index < end; ++index) {
               fn(index);
             }
           }});
    }
    return run();
  }

private:
  struct JobNode {
    JobRecord record;
    std::function<void(JobContext &)> run;
    std::uint64_t sequence = 0u;
  };

  [[nodiscard]] bool dependencyComplete(JobId id) const;
  [[nodiscard]] bool dependencyFailed(JobId id) const;
  [[nodiscard]] std::optional<std::size_t> indexOf(JobId id) const;
  void appendTrace(JobTraceEvent event) const;

  JobGraphOptions options_{};
  std::vector<JobNode> jobs_;
  JobId next_id_ = 1u;
  std::uint64_t next_sequence_ = 1u;
  mutable std::mutex trace_mutex_;
  mutable std::vector<JobTraceEvent> trace_events_;
};

[[nodiscard]] const char *jobPriorityName(JobPriority priority);
[[nodiscard]] const char *jobLaneName(JobLane lane);
[[nodiscard]] const char *jobStatusName(JobStatus status);
[[nodiscard]] const char *jobTraceEventKindName(JobTraceEventKind kind);

} // namespace aster
