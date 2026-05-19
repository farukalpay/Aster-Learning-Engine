// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/framegraph/graph_compiler.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace aster::framegraph {
namespace {

[[nodiscard]] rhi::ResourceState readStateFor(const ResourceDesc &desc) {
  if (desc.lifetime == ResourceLifetime::Readback) {
    return rhi::ResourceState::Readback;
  }
  if (desc.kind == ResourceKind::Buffer) {
    return rhi::ResourceState::ShaderRead;
  }
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::TransferSource)) != 0u) {
    return rhi::ResourceState::CopySource;
  }
  return rhi::ResourceState::ShaderRead;
}

[[nodiscard]] rhi::ResourceState writeStateFor(const ResourceDesc &desc) {
  if (desc.kind == ResourceKind::Buffer) {
    if (desc.lifetime == ResourceLifetime::Readback ||
        (desc.usage & rhi::bufferUsageBit(rhi::BufferUsage::Readback)) != 0u) {
      return rhi::ResourceState::CopyDestination;
    }
    return rhi::ResourceState::ShaderWrite;
  }
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment)) != 0u) {
    return rhi::ResourceState::DepthAttachment;
  }
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment)) != 0u) {
    return rhi::ResourceState::ColorAttachment;
  }
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::TransferDestination)) != 0u ||
      desc.lifetime == ResourceLifetime::Readback) {
    return rhi::ResourceState::CopyDestination;
  }
  return rhi::ResourceState::ShaderWrite;
}

[[nodiscard]] std::uint32_t bitFor(const ResourceHandle handle) {
  return handle.index < 32u ? (1u << handle.index) : 0u;
}

[[nodiscard]] std::uint64_t resourceIdFor(const ResourceHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32u) | handle.index;
}

[[nodiscard]] rhi::FramebufferAttachmentDesc attachmentForWrite(const ResourceDesc &desc,
                                                                const rhi::ResourceState before,
                                                                const rhi::ResourceState after) {
  return {.format = desc.format,
          .load_op = before == rhi::ResourceState::Undefined ? rhi::AttachmentLoadOp::Clear
                                                             : rhi::AttachmentLoadOp::Load,
          .store_op = rhi::AttachmentStoreOp::Store,
          .initial_state = before,
          .final_state = after,
          .transient = desc.lifetime == ResourceLifetime::Transient};
}

[[nodiscard]] rhi::DescriptorRangeKind descriptorKindFor(const ResourceDesc &desc,
                                                         const bool write) {
  if (desc.kind == ResourceKind::Buffer) {
    if ((desc.usage & rhi::bufferUsageBit(rhi::BufferUsage::Uniform)) != 0u) {
      return rhi::DescriptorRangeKind::UniformBuffer;
    }
    return rhi::DescriptorRangeKind::StorageBuffer;
  }
  if (write && (desc.usage & rhi::imageUsageBit(rhi::ImageUsage::Storage)) != 0u) {
    return rhi::DescriptorRangeKind::StorageImage;
  }
  return rhi::DescriptorRangeKind::SampledImage;
}

[[nodiscard]] bool resourceNeedsReadDescriptor(const ResourceDesc &desc) {
  if (desc.kind == ResourceKind::Buffer) {
    return (desc.usage & (rhi::bufferUsageBit(rhi::BufferUsage::Uniform) |
                          rhi::bufferUsageBit(rhi::BufferUsage::Storage))) != 0u;
  }
  return (desc.usage & rhi::imageUsageBit(rhi::ImageUsage::Sampled)) != 0u;
}

[[nodiscard]] bool resourceNeedsWriteDescriptor(const ResourceDesc &desc) {
  if (desc.kind == ResourceKind::Buffer) {
    return (desc.usage & rhi::bufferUsageBit(rhi::BufferUsage::Storage)) != 0u;
  }
  return (desc.usage & rhi::imageUsageBit(rhi::ImageUsage::Storage)) != 0u;
}

[[nodiscard]] bool canAlias(const CompiledResource &lhs, const CompiledResource &rhs) {
  if (lhs.desc.lifetime != ResourceLifetime::Transient ||
      rhs.desc.lifetime != ResourceLifetime::Transient) {
    return false;
  }
  if (lhs.desc.kind != rhs.desc.kind || lhs.desc.format != rhs.desc.format ||
      lhs.desc.usage != rhs.desc.usage ||
      lhs.desc.extent.width != rhs.desc.extent.width ||
      lhs.desc.extent.height != rhs.desc.extent.height ||
      lhs.desc.extent.depth != rhs.desc.extent.depth ||
      lhs.desc.byte_size != rhs.desc.byte_size || lhs.desc.stride != rhs.desc.stride) {
    return false;
  }
  return lhs.last_pass < rhs.first_pass || rhs.last_pass < lhs.first_pass;
}

void assignAliasGroups(CompiledFrameGraph &compiled) {
  for (std::size_t resource_index = 0u; resource_index < compiled.resources.size();
       ++resource_index) {
    CompiledResource &resource = compiled.resources[resource_index];
    if (resource.desc.lifetime != ResourceLifetime::Transient) {
      continue;
    }
    std::size_t selected_group = 0u;
    for (std::size_t group_index = 0u; group_index < compiled.alias_groups.size(); ++group_index) {
      bool compatible = true;
      for (const ResourceHandle existing_handle : compiled.alias_groups[group_index].resources) {
        const auto existing = std::find_if(
            compiled.resources.begin(), compiled.resources.end(),
            [existing_handle](const CompiledResource &candidate) {
              return candidate.handle == existing_handle;
            });
        if (existing == compiled.resources.end() || !canAlias(*existing, resource)) {
          compatible = false;
          break;
        }
      }
      if (compatible) {
        selected_group = compiled.alias_groups[group_index].id;
        compiled.alias_groups[group_index].resources.push_back(resource.handle);
        compiled.alias_groups[group_index].first_pass =
            std::min(compiled.alias_groups[group_index].first_pass, resource.first_pass);
        compiled.alias_groups[group_index].last_pass =
            std::max(compiled.alias_groups[group_index].last_pass, resource.last_pass);
        break;
      }
    }
    if (selected_group == 0u) {
      ResourceAliasGroup group;
      group.id = compiled.alias_groups.size() + 1u;
      group.resources.push_back(resource.handle);
      group.first_pass = resource.first_pass;
      group.last_pass = resource.last_pass;
      selected_group = group.id;
      compiled.alias_groups.push_back(std::move(group));
    }
    resource.alias_group = selected_group;
    resource.physical_allocation_id = selected_group;
  }
}

} // namespace

CompiledFrameGraph compileFrameGraph(const FrameGraph &graph) {
  return compileFrameGraph(graph, {});
}

CompiledFrameGraph compileFrameGraph(const FrameGraph &graph,
                                     const FrameGraphCompileOptions &options) {
  CompiledFrameGraph compiled;
  compiled.resources.reserve(graph.resources().size());
  compiled.passes.reserve(graph.passes().size());

  std::unordered_map<ResourceHandle, std::size_t> resource_to_index;
  resource_to_index.reserve(graph.resources().size());
  for (const ResourceNode &resource : graph.resources()) {
    ResourceDesc desc = resource.desc;
    if (desc.kind == ResourceKind::Image && desc.extent.width == 0u &&
        desc.extent.height == 0u && options.frame_extent.width > 0u &&
        options.frame_extent.height > 0u) {
      desc.extent = options.frame_extent;
    }
    resource_to_index.emplace(resource.handle, compiled.resources.size());
    compiled.resources.push_back({.handle = resource.handle,
                                  .name = resource.name,
                                  .desc = desc,
                                  .first_pass = std::numeric_limits<std::size_t>::max(),
                                  .last_pass = 0u,
                                  .alias_group = 0u,
                                  .physical_allocation_id = 0u});
    if (desc.lifetime == ResourceLifetime::Transient) {
      ++compiled.transient_resource_count;
    }
    const std::uint32_t resource_bit = bitFor(resource.handle);
    if ((options.required_resource_mask & resource_bit) != 0u &&
        (options.backend_resource_mask & resource_bit) == 0u) {
      compiled.validation_errors.push_back("required frame graph resource '" + resource.name +
                                           "' is not supported by the backend");
    }
  }

  std::unordered_set<ResourceHandle> written;
  std::unordered_map<ResourceHandle, rhi::ResourceState> current_states;
  std::unordered_map<ResourceHandle, rhi::QueueKind> current_queues;
  for (std::size_t pass_index = 0; pass_index < graph.passes().size(); ++pass_index) {
    const PassDesc &pass = graph.passes()[pass_index];
    CompiledPass compiled_pass;
    compiled_pass.name = pass.name;
    compiled_pass.reads = pass.reads;
    compiled_pass.writes = pass.writes;
    compiled_pass.queue = pass.queue;
    compiled_pass.debug_marker_name = pass.name;
    compiled_pass.timestamp_zone_index = pass_index;

    for (const ResourceHandle resource : pass.reads) {
      const auto found = resource_to_index.find(resource);
      if (found == resource_to_index.end()) {
        compiled.validation_errors.push_back("pass '" + pass.name + "' reads an unknown resource");
        continue;
      }
      if (!written.contains(resource) &&
          compiled.resources[found->second].desc.lifetime != ResourceLifetime::Imported) {
        compiled.validation_errors.push_back("pass '" + pass.name +
                                             "' reads resource before any writer");
      }
      auto &compiled_resource = compiled.resources[found->second];
      compiled_resource.first_pass = std::min(compiled_resource.first_pass, pass_index);
      compiled_resource.last_pass = std::max(compiled_resource.last_pass, pass_index);
      compiled_pass.read_mask |= bitFor(resource);
      if ((options.backend_resource_mask & bitFor(resource)) == 0u) {
        compiled_pass.culled = options.cull_unsupported_passes;
      }

      const rhi::ResourceState desired = readStateFor(compiled_resource.desc);
      compiled_pass.accesses.push_back({.resource = resource,
                                        .state = desired,
                                        .usage = compiled_resource.desc.usage,
                                        .write = false});
      if (resourceNeedsReadDescriptor(compiled_resource.desc)) {
        DescriptorRequirement requirement{.pass_index = pass_index,
                                          .resource = resource,
                                          .kind = descriptorKindFor(compiled_resource.desc, false),
                                          .binding = static_cast<std::uint32_t>(
                                              compiled_pass.descriptor_requirements.size()),
                                          .count = 1u,
                                          .label = compiled_resource.name + "-srv"};
        compiled_pass.descriptor_requirements.push_back(requirement);
        compiled.descriptor_requirements.push_back(std::move(requirement));
      }
      const rhi::ResourceState before = current_states.contains(resource)
                                            ? current_states[resource]
                                            : rhi::ResourceState::Undefined;
      const rhi::QueueKind before_queue = current_queues.contains(resource)
                                              ? current_queues[resource]
                                              : pass.queue;
      if (before != desired || before_queue != pass.queue) {
        rhi::ResourceBarrier expanded =
            rhi::completeResourceBarrier({.resource_id = resourceIdFor(resource),
                                          .before = before,
                                          .after = desired,
                                          .source_queue = before_queue,
                                          .destination_queue = pass.queue});
        compiled.barriers.push_back({resource, before, desired, pass_index, expanded});
        compiled.resource_barriers.push_back(expanded);
        current_states[resource] = desired;
      }
      current_queues[resource] = pass.queue;
    }

    for (const ResourceHandle resource : pass.writes) {
      const auto found = resource_to_index.find(resource);
      if (found == resource_to_index.end()) {
        compiled.validation_errors.push_back("pass '" + pass.name + "' writes an unknown resource");
        continue;
      }
      auto &compiled_resource = compiled.resources[found->second];
      compiled_resource.first_pass = std::min(compiled_resource.first_pass, pass_index);
      compiled_resource.last_pass = std::max(compiled_resource.last_pass, pass_index);
      compiled_pass.write_mask |= bitFor(resource);
      if ((options.backend_resource_mask & bitFor(resource)) == 0u) {
        compiled_pass.culled = options.cull_unsupported_passes;
      }

      const rhi::ResourceState desired = writeStateFor(compiled_resource.desc);
      const rhi::ResourceState before = current_states.contains(resource)
                                            ? current_states[resource]
                                            : rhi::ResourceState::Undefined;
      const rhi::QueueKind before_queue = current_queues.contains(resource)
                                              ? current_queues[resource]
                                              : pass.queue;
      compiled_pass.accesses.push_back({.resource = resource,
                                        .state = desired,
                                        .usage = compiled_resource.desc.usage,
                                        .write = true});
      if (resourceNeedsWriteDescriptor(compiled_resource.desc)) {
        DescriptorRequirement requirement{.pass_index = pass_index,
                                          .resource = resource,
                                          .kind = descriptorKindFor(compiled_resource.desc, true),
                                          .binding = static_cast<std::uint32_t>(
                                              compiled_pass.descriptor_requirements.size()),
                                          .count = 1u,
                                          .label = compiled_resource.name + "-uav"};
        compiled_pass.descriptor_requirements.push_back(requirement);
        compiled.descriptor_requirements.push_back(std::move(requirement));
      }
      if (compiled_resource.desc.kind == ResourceKind::Image &&
          (compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment)) !=
          0u) {
        compiled_pass.attachments.push_back(attachmentForWrite(compiled_resource.desc, before, desired));
        compiled_pass.pipeline_compatibility.color_formats.push_back(compiled_resource.desc.format);
      }
      if (compiled_resource.desc.kind == ResourceKind::Image &&
          (compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment)) !=
          0u) {
        compiled_pass.attachments.push_back(attachmentForWrite(compiled_resource.desc, before, desired));
        compiled_pass.pipeline_compatibility.depth_format = compiled_resource.desc.format;
      }
      compiled_pass.pipeline_compatibility.sample_count = 1u;
      if (before != desired || before_queue != pass.queue) {
        rhi::ResourceBarrier expanded =
            rhi::completeResourceBarrier({.resource_id = resourceIdFor(resource),
                                          .before = before,
                                          .after = desired,
                                          .source_queue = before_queue,
                                          .destination_queue = pass.queue});
        compiled.barriers.push_back({resource, before, desired, pass_index, expanded});
        compiled.resource_barriers.push_back(expanded);
        current_states[resource] = desired;
      }
      current_queues[resource] = pass.queue;
      written.insert(resource);
    }

    compiled.passes.push_back(std::move(compiled_pass));
  }

  for (CompiledResource &resource : compiled.resources) {
    if (resource.first_pass == std::numeric_limits<std::size_t>::max()) {
      resource.first_pass = 0u;
      resource.last_pass = 0u;
    }
  }
  if (options.assign_physical_allocations) {
    assignAliasGroups(compiled);
  }
  return compiled;
}

RenderGraphCompilerReport renderGraphCompilerReport(const CompiledFrameGraph &graph) {
  RenderGraphCompilerReport report;
  report.resource_count = graph.resources.size();
  report.pass_count = graph.passes.size();
  report.transient_resource_count = graph.transient_resource_count;
  report.barrier_count = graph.barriers.size();
  report.expanded_barrier_count = graph.resource_barriers.size();
  report.descriptor_requirement_count = graph.descriptor_requirements.size();
  report.alias_group_count = graph.alias_groups.size();
  report.validation_error_count = graph.validation_errors.size();
  report.pipeline_compatibility_count =
      static_cast<std::size_t>(std::count_if(graph.passes.begin(), graph.passes.end(),
                                             [](const CompiledPass &pass) {
                                               return !pass.pipeline_compatibility.color_formats.empty() ||
                                                      pass.pipeline_compatibility.depth_format !=
                                                          rhi::ImageFormat::Unknown ||
                                                      pass.pipeline_compatibility.sample_count != 0u;
                                             }));
  report.queue_ownership_transfer_count =
      static_cast<std::size_t>(std::count_if(graph.resource_barriers.begin(),
                                             graph.resource_barriers.end(),
                                             [](const rhi::ResourceBarrier &barrier) {
                                               return rhi::transfersQueueOwnership(barrier);
                                             }));
  report.culled_pass_count =
      static_cast<std::size_t>(std::count_if(graph.passes.begin(), graph.passes.end(),
                                             [](const CompiledPass &pass) { return pass.culled; }));
  report.has_resource_lifetimes =
      std::all_of(graph.resources.begin(), graph.resources.end(), [](const CompiledResource &resource) {
        return resource.first_pass <= resource.last_pass;
      });
  report.has_expanded_barriers = !graph.resource_barriers.empty();
  report.has_descriptor_requirements = !graph.descriptor_requirements.empty();
  report.has_pipeline_cache_inputs = report.pipeline_compatibility_count > 0u;
  report.has_transient_aliasing = !graph.alias_groups.empty();
  report.has_queue_ownership_transfers = report.queue_ownership_transfer_count > 0u;
  return report;
}

std::string dumpFrameGraph(const CompiledFrameGraph &graph) {
  const RenderGraphCompilerReport report = renderGraphCompilerReport(graph);
  std::ostringstream out;
  out << "resources " << graph.resources.size() << '\n';
  for (const CompiledResource &resource : graph.resources) {
    out << resource.handle.index << ' ' << resource.name << " lifetime "
        << static_cast<std::uint32_t>(resource.desc.lifetime) << " passes "
        << resource.first_pass << '-' << resource.last_pass << '\n';
  }
  out << "passes " << graph.passes.size() << '\n';
  for (std::size_t i = 0; i < graph.passes.size(); ++i) {
    const CompiledPass &pass = graph.passes[i];
    out << i << ' ' << pass.name << " reads 0x" << std::hex << pass.read_mask << " writes 0x"
        << pass.write_mask << std::dec << '\n';
  }
  out << "barriers " << graph.barriers.size() << '\n';
  out << "expanded-barriers " << graph.resource_barriers.size() << '\n';
  out << "descriptor-requirements " << graph.descriptor_requirements.size() << '\n';
  out << "alias-groups " << graph.alias_groups.size() << '\n';
  out << "compiler-report resources=" << report.resource_count << " passes=" << report.pass_count
      << " lifetimes=" << (report.has_resource_lifetimes ? "yes" : "no")
      << " expanded-barriers=" << (report.has_expanded_barriers ? "yes" : "no")
      << " descriptors=" << (report.has_descriptor_requirements ? "yes" : "no")
      << " pipeline-inputs=" << (report.has_pipeline_cache_inputs ? "yes" : "no")
      << " aliasing=" << (report.has_transient_aliasing ? "yes" : "no")
      << " queue-transfers=" << report.queue_ownership_transfer_count << '\n';
  return out.str();
}

} // namespace aster::framegraph
