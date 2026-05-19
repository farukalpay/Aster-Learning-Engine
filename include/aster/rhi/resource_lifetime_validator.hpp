// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"
#include "aster/rhi/resource_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster::rhi {

enum class ResourceLifetimeValidationSeverity : std::uint32_t {
  Info,
  Warning,
  Error,
};

enum class ResourceLifetimeValidationKind : std::uint32_t {
  ReadBeforeWrite,
  MissingBarrier,
  QueueOwnershipMismatch,
  DescriptorResourceMismatch,
  MissingResource,
  RetiredResourceUse,
  InvalidResourceState,
};

struct ResourceLifetimeValidationEvent {
  ResourceLifetimeValidationKind kind = ResourceLifetimeValidationKind::InvalidResourceState;
  ResourceLifetimeValidationSeverity severity = ResourceLifetimeValidationSeverity::Error;
  std::string pass;
  std::string resource;
  std::string message;
  std::size_t pass_index = 0u;
  std::uint64_t resource_id = 0u;
};

struct ResourceLifetimeValidationReport {
  std::vector<ResourceLifetimeValidationEvent> events;

  [[nodiscard]] bool valid() const;
  [[nodiscard]] std::size_t errorCount() const;
};

class ResourceLifetimeValidator {
public:
  [[nodiscard]] ResourceLifetimeValidationReport
  validateFrameGraph(const framegraph::CompiledFrameGraph &graph) const;

  [[nodiscard]] ResourceLifetimeValidationReport
  validateRegistryUse(const ResourceRegistry &registry, ResourceKind expected_kind,
                      std::uint64_t id, std::uint32_t generation,
                      std::string_view label = {}) const;
};

[[nodiscard]] std::string_view
resourceLifetimeValidationSeverityName(ResourceLifetimeValidationSeverity severity);
[[nodiscard]] std::string_view
resourceLifetimeValidationKindName(ResourceLifetimeValidationKind kind);

} // namespace aster::rhi
