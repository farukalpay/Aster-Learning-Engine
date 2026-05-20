// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/job_graph.hpp"

#include "aster/core/profiler.hpp"

#include <algorithm>
#include <exception>
#include <future>
#include <thread>
#include <utility>

namespace aster {

namespace {

std::size_t resolvedWorkerCount(const JobGraphOptions &options) {
  if (options.deterministic) {
    return 1u;
  }
  if (options.worker_count > 0u) {
    return options.worker_count;
  }
  const unsigned int hardware = std::thread::hardware_concurrency();
  return hardware == 0u ? 2u : static_cast<std::size_t>(hardware);
}

} // namespace

JobGraph::JobGraph(JobGraphOptions options) : options_(options) {}

JobId JobGraph::add(JobDesc desc) {
  const JobId id = next_id_++;
  if (desc.name.empty()) {
    desc.name = "job." + std::to_string(id);
  }
  JobNode node;
  node.record.id = id;
  node.record.name = std::move(desc.name);
  node.record.priority = desc.priority;
  node.record.lane = desc.lane;
  node.record.dependencies = std::move(desc.dependencies);
  node.run = std::move(desc.run);
  node.sequence = next_sequence_++;
  jobs_.push_back(std::move(node));
  appendTrace({JobTraceEventKind::Queued, id, jobs_.back().record.name, jobs_.back().record.lane,
               jobs_.back().record.status, 0u, jobs_.back().sequence, {}});
  return id;
}

std::size_t JobGraph::size() const noexcept {
  return jobs_.size();
}

bool JobGraph::empty() const noexcept {
  return jobs_.empty();
}

const JobRecord *JobGraph::record(const JobId id) const {
  const std::optional<std::size_t> index = indexOf(id);
  return index ? &jobs_[*index].record : nullptr;
}

std::vector<JobRecord> JobGraph::records() const {
  std::vector<JobRecord> result;
  result.reserve(jobs_.size());
  for (const JobNode &job : jobs_) {
    result.push_back(job.record);
  }
  return result;
}

std::vector<JobTraceEvent> JobGraph::traceEvents() const {
  std::lock_guard<std::mutex> lock(trace_mutex_);
  return trace_events_;
}

void JobGraph::clearTrace() {
  std::lock_guard<std::mutex> lock(trace_mutex_);
  trace_events_.clear();
}

JobGraphDiagnostics JobGraph::run() {
  JobGraphDiagnostics diagnostics;
  diagnostics.queued_jobs = jobs_.size();
  const std::size_t workers = resolvedWorkerCount(options_);

  while (true) {
    std::vector<std::size_t> ready;
    bool has_pending = false;
    for (std::size_t index = 0u; index < jobs_.size(); ++index) {
      JobNode &job = jobs_[index];
      if (job.record.status != JobStatus::Pending) {
        continue;
      }
      has_pending = true;
      bool failed_dependency = false;
      bool ready_to_run = true;
      for (const JobId dependency : job.record.dependencies) {
        if (dependencyFailed(dependency)) {
          failed_dependency = true;
          break;
        }
        if (!dependencyComplete(dependency)) {
          ready_to_run = false;
          break;
        }
      }
      if (failed_dependency) {
        job.record.status = JobStatus::Skipped;
        job.record.diagnostic = "dependency failed";
        appendTrace({JobTraceEventKind::Skipped, job.record.id, job.record.name, job.record.lane,
                     job.record.status, 0u, job.sequence, job.record.diagnostic});
        ++diagnostics.skipped_jobs;
        continue;
      }
      if (ready_to_run) {
        ready.push_back(index);
      }
    }

    if (!has_pending) {
      break;
    }
    if (ready.empty()) {
      ++diagnostics.dependency_cycles;
      diagnostics.diagnostics.push_back("job graph has unresolved dependencies or a cycle");
      for (JobNode &job : jobs_) {
        if (job.record.status == JobStatus::Pending) {
          job.record.status = JobStatus::Skipped;
          job.record.diagnostic = "unresolved dependency cycle";
          appendTrace({JobTraceEventKind::Skipped, job.record.id, job.record.name, job.record.lane,
                       job.record.status, 0u, job.sequence, job.record.diagnostic});
          ++diagnostics.skipped_jobs;
        }
      }
      break;
    }

    diagnostics.max_ready_jobs = std::max(diagnostics.max_ready_jobs, ready.size());
    std::sort(ready.begin(), ready.end(), [&](const std::size_t lhs, const std::size_t rhs) {
      if (jobs_[lhs].record.priority != jobs_[rhs].record.priority) {
        return static_cast<std::uint32_t>(jobs_[lhs].record.priority) >
               static_cast<std::uint32_t>(jobs_[rhs].record.priority);
      }
      return jobs_[lhs].sequence < jobs_[rhs].sequence;
    });

    const auto execute_job = [&](const std::size_t index, const std::uint32_t worker_index) {
      JobNode &job = jobs_[index];
      job.record.status = JobStatus::Running;
      appendTrace({JobTraceEventKind::Running, job.record.id, job.record.name, job.record.lane,
                   job.record.status, worker_index, job.sequence, {}});
      JobContext context{job.record.id, job.record.name, worker_index, options_.deterministic};
      try {
        if (options_.profile_jobs) {
          ASTER_PROFILE_SCOPE("aster.job_graph.job");
          if (job.run) {
            job.run(context);
          }
        } else if (job.run) {
          job.run(context);
        }
        job.record.status = JobStatus::Complete;
        appendTrace({JobTraceEventKind::Completed, job.record.id, job.record.name,
                     job.record.lane, job.record.status, worker_index, job.sequence, {}});
      } catch (const std::exception &error) {
        job.record.status = JobStatus::Failed;
        job.record.diagnostic = error.what();
        appendTrace({JobTraceEventKind::Failed, job.record.id, job.record.name, job.record.lane,
                     job.record.status, worker_index, job.sequence, job.record.diagnostic});
      } catch (...) {
        job.record.status = JobStatus::Failed;
        job.record.diagnostic = "job threw an unknown exception";
        appendTrace({JobTraceEventKind::Failed, job.record.id, job.record.name, job.record.lane,
                     job.record.status, worker_index, job.sequence, job.record.diagnostic});
      }
    };

    if (workers <= 1u || ready.size() <= 1u) {
      for (const std::size_t index : ready) {
        execute_job(index, 0u);
      }
    } else {
      std::vector<std::future<void>> futures;
      futures.reserve(ready.size());
      for (std::size_t i = 0u; i < ready.size(); ++i) {
        const std::size_t index = ready[i];
        const std::uint32_t worker_index = static_cast<std::uint32_t>(i % workers);
        futures.push_back(std::async(std::launch::async, execute_job, index, worker_index));
      }
      for (std::future<void> &future : futures) {
        future.get();
      }
    }
  }

  for (const JobNode &job : jobs_) {
    if (job.record.status == JobStatus::Complete) {
      ++diagnostics.executed_jobs;
    } else if (job.record.status == JobStatus::Failed) {
      ++diagnostics.failed_jobs;
      diagnostics.diagnostics.push_back(job.record.name + ": " + job.record.diagnostic);
    }
  }
  return diagnostics;
}

void JobGraph::clear() {
  jobs_.clear();
  next_id_ = 1u;
  next_sequence_ = 1u;
  clearTrace();
}

bool JobGraph::dependencyComplete(const JobId id) const {
  const std::optional<std::size_t> index = indexOf(id);
  return index && jobs_[*index].record.status == JobStatus::Complete;
}

bool JobGraph::dependencyFailed(const JobId id) const {
  const std::optional<std::size_t> index = indexOf(id);
  if (!index) {
    return true;
  }
  const JobStatus status = jobs_[*index].record.status;
  return status == JobStatus::Failed || status == JobStatus::Skipped;
}

std::optional<std::size_t> JobGraph::indexOf(const JobId id) const {
  for (std::size_t index = 0u; index < jobs_.size(); ++index) {
    if (jobs_[index].record.id == id) {
      return index;
    }
  }
  return std::nullopt;
}

void JobGraph::appendTrace(JobTraceEvent event) const {
  std::lock_guard<std::mutex> lock(trace_mutex_);
  trace_events_.push_back(std::move(event));
}

const char *jobPriorityName(const JobPriority priority) {
  switch (priority) {
  case JobPriority::Background:
    return "background";
  case JobPriority::Normal:
    return "normal";
  case JobPriority::High:
    return "high";
  }
  return "unknown";
}

const char *jobLaneName(const JobLane lane) {
  switch (lane) {
  case JobLane::Main:
    return "main";
  case JobLane::Render:
    return "render";
  case JobLane::Io:
    return "io";
  case JobLane::Background:
    return "background";
  case JobLane::Worker:
    return "worker";
  }
  return "unknown";
}

const char *jobStatusName(const JobStatus status) {
  switch (status) {
  case JobStatus::Pending:
    return "pending";
  case JobStatus::Running:
    return "running";
  case JobStatus::Complete:
    return "complete";
  case JobStatus::Failed:
    return "failed";
  case JobStatus::Skipped:
    return "skipped";
  }
  return "unknown";
}

const char *jobTraceEventKindName(const JobTraceEventKind kind) {
  switch (kind) {
  case JobTraceEventKind::Queued:
    return "queued";
  case JobTraceEventKind::Running:
    return "running";
  case JobTraceEventKind::Completed:
    return "completed";
  case JobTraceEventKind::Failed:
    return "failed";
  case JobTraceEventKind::Skipped:
    return "skipped";
  }
  return "unknown";
}

} // namespace aster
