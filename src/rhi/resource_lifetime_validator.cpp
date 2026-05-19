// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/rhi/resource_lifetime_validator.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace aster::rhi {
namespace {

[[nodiscard]] std::uint64_t frameGraphResourceId(const framegraph::ResourceHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32u) | handle.index;
}

[[nodiscard]] const framegraph::CompiledResource *
findResource(const framegraph::CompiledFrameGraph &graph, const framegraph::ResourceHandle handle) {
  const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
                                  [handle](const framegraph::CompiledResource &resource) {
                                    return resource.handle == handle;
                                  });
  return found == graph.resources.end() ? nullptr : &*found;
}

[[nodiscard]] bool passUsesResource(const framegraph::CompiledPass &pass,
                                    const framegraph::ResourceHandle handle) {
  return std::find(pass.reads.begin(), pass.reads.end(), handle) != pass.reads.end() ||
         std::find(pass.writes.begin(), pass.writes.end(), handle) != pass.writes.end();
}

[[nodiscard]] bool barrierFor(const framegraph::CompiledFrameGraph &graph,
                              const framegraph::ResourceHandle handle,
                              const std::size_t pass_index, ResourceState before,
                              ResourceState after, QueueKind source_queue,
                              QueueKind destination_queue, bool require_queue_transfer) {
  return std::any_of(graph.barriers.begin(), graph.barriers.end(),
                     [=](const framegraph::GraphBarrier &barrier) {
                       if (!(barrier.resource == handle) || barrier.pass_index != pass_index ||
                           barrier.before != before || barrier.after != after) {
                         return false;
                       }
                       if (require_queue_transfer &&
                           (barrier.expanded.source_queue != source_queue ||
                            barrier.expanded.destination_queue != destination_queue)) {
                         return false;
                       }
                       return true;
                     });
}

void append(ResourceLifetimeValidationReport &report, ResourceLifetimeValidationKind kind,
            ResourceLifetimeValidationSeverity severity, std::string pass, std::string resource,
            std::string message, const std::size_t pass_index, const std::uint64_t resource_id) {
  report.events.push_back({.kind = kind,
                           .severity = severity,
                           .pass = std::move(pass),
                           .resource = std::move(resource),
                           .message = std::move(message),
                           .pass_index = pass_index,
                           .resource_id = resource_id});
}

} // namespace

bool ResourceLifetimeValidationReport::valid() const {
  return errorCount() == 0u;
}

std::size_t ResourceLifetimeValidationReport::errorCount() const {
  return static_cast<std::size_t>(
      std::count_if(events.begin(), events.end(), [](const ResourceLifetimeValidationEvent &event) {
        return event.severity == ResourceLifetimeValidationSeverity::Error;
      }));
}

ResourceLifetimeValidationReport
ResourceLifetimeValidator::validateFrameGraph(const framegraph::CompiledFrameGraph &graph) const {
  ResourceLifetimeValidationReport report;
  std::unordered_set<framegraph::ResourceHandle> written;
  std::unordered_map<framegraph::ResourceHandle, ResourceState> states;
  std::unordered_map<framegraph::ResourceHandle, QueueKind> queues;

  for (std::size_t pass_index = 0u; pass_index < graph.passes.size(); ++pass_index) {
    const framegraph::CompiledPass &pass = graph.passes[pass_index];
    for (const framegraph::CompiledResourceAccess &access : pass.accesses) {
      const framegraph::CompiledResource *resource = findResource(graph, access.resource);
      const std::string resource_name = resource == nullptr ? "unknown" : resource->name;
      const std::uint64_t resource_id = frameGraphResourceId(access.resource);
      if (resource == nullptr) {
        append(report, ResourceLifetimeValidationKind::MissingResource,
               ResourceLifetimeValidationSeverity::Error, pass.name, resource_name,
               "Pass references a resource that is not in the compiled frame graph.",
               pass_index, resource_id);
        continue;
      }
      if (!access.write && !written.contains(access.resource) &&
          resource->desc.lifetime != framegraph::ResourceLifetime::Imported) {
        append(report, ResourceLifetimeValidationKind::ReadBeforeWrite,
               ResourceLifetimeValidationSeverity::Error, pass.name, resource_name,
               "Pass reads a non-imported resource before any previous pass writes it.",
               pass_index, resource_id);
      }

      const ResourceState before =
          states.contains(access.resource) ? states[access.resource] : ResourceState::Undefined;
      const QueueKind before_queue =
          queues.contains(access.resource) ? queues[access.resource] : pass.queue;
      const bool state_changes = before != access.state;
      const bool queue_changes = before_queue != pass.queue;
      if ((state_changes || queue_changes) &&
          !barrierFor(graph, access.resource, pass_index, before, access.state, before_queue,
                      pass.queue, queue_changes)) {
        append(report,
               queue_changes ? ResourceLifetimeValidationKind::QueueOwnershipMismatch
                             : ResourceLifetimeValidationKind::MissingBarrier,
               ResourceLifetimeValidationSeverity::Error, pass.name, resource_name,
               queue_changes ? "Resource queue ownership changes without a matching barrier."
                             : "Resource state changes without a matching barrier.",
               pass_index, resource_id);
      }

      states[access.resource] = access.state;
      queues[access.resource] = pass.queue;
      if (access.write) {
        written.insert(access.resource);
      }
    }

    for (const framegraph::DescriptorRequirement &requirement : pass.descriptor_requirements) {
      const framegraph::CompiledResource *resource = findResource(graph, requirement.resource);
      const std::string resource_name = resource == nullptr ? requirement.label : resource->name;
      if (resource == nullptr || !passUsesResource(pass, requirement.resource)) {
        append(report, ResourceLifetimeValidationKind::DescriptorResourceMismatch,
               ResourceLifetimeValidationSeverity::Error, pass.name, resource_name,
               "Descriptor requirement is not tied to a resource used by this pass.", pass_index,
               frameGraphResourceId(requirement.resource));
      }
    }
  }

  for (const std::string &error : graph.validation_errors) {
    append(report, ResourceLifetimeValidationKind::InvalidResourceState,
           ResourceLifetimeValidationSeverity::Error, "frame-graph", "graph", error,
           std::numeric_limits<std::size_t>::max(), 0u);
  }
  return report;
}

ResourceLifetimeValidationReport ResourceLifetimeValidator::validateRegistryUse(
    const ResourceRegistry &registry, const ResourceKind expected_kind, const std::uint64_t id,
    const std::uint32_t generation, const std::string_view label) const {
  ResourceLifetimeValidationReport report;
  const std::optional<ResourceRegistryEntrySnapshot> snapshot = registry.snapshot(id);
  const std::string resource_label =
      label.empty() ? std::string("resource:") + std::to_string(id) : std::string(label);
  if (!snapshot.has_value()) {
    append(report, ResourceLifetimeValidationKind::MissingResource,
           ResourceLifetimeValidationSeverity::Error, "resource-registry", resource_label,
           "Resource handle id is not present in the registry.", 0u, id);
    return report;
  }
  if (snapshot->generation != generation || snapshot->kind != expected_kind) {
    append(report, ResourceLifetimeValidationKind::InvalidResourceState,
           ResourceLifetimeValidationSeverity::Error, "resource-registry", resource_label,
           "Resource handle generation or kind does not match the registry entry.", 0u, id);
  }
  if (snapshot->residency == ResourceResidency::Retired) {
    append(report, ResourceLifetimeValidationKind::RetiredResourceUse,
           ResourceLifetimeValidationSeverity::Error, "resource-registry", resource_label,
           "Resource handle refers to a retired registry entry.", 0u, id);
  }
  return report;
}

std::string_view
resourceLifetimeValidationSeverityName(const ResourceLifetimeValidationSeverity severity) {
  switch (severity) {
  case ResourceLifetimeValidationSeverity::Info:
    return "info";
  case ResourceLifetimeValidationSeverity::Warning:
    return "warning";
  case ResourceLifetimeValidationSeverity::Error:
    return "error";
  }
  return "error";
}

std::string_view resourceLifetimeValidationKindName(const ResourceLifetimeValidationKind kind) {
  switch (kind) {
  case ResourceLifetimeValidationKind::ReadBeforeWrite:
    return "read-before-write";
  case ResourceLifetimeValidationKind::MissingBarrier:
    return "missing-barrier";
  case ResourceLifetimeValidationKind::QueueOwnershipMismatch:
    return "queue-ownership-mismatch";
  case ResourceLifetimeValidationKind::DescriptorResourceMismatch:
    return "descriptor-resource-mismatch";
  case ResourceLifetimeValidationKind::MissingResource:
    return "missing-resource";
  case ResourceLifetimeValidationKind::RetiredResourceUse:
    return "retired-resource-use";
  case ResourceLifetimeValidationKind::InvalidResourceState:
    return "invalid-resource-state";
  }
  return "invalid-resource-state";
}

} // namespace aster::rhi
