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
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::TransferSource)) != 0u) {
    return rhi::ResourceState::CopySource;
  }
  return rhi::ResourceState::ShaderRead;
}

[[nodiscard]] rhi::ResourceState writeStateFor(const ResourceDesc &desc) {
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

[[nodiscard]] bool canAlias(const CompiledResource &lhs, const CompiledResource &rhs) {
  if (lhs.desc.lifetime != ResourceLifetime::Transient ||
      rhs.desc.lifetime != ResourceLifetime::Transient) {
    return false;
  }
  if (lhs.desc.format != rhs.desc.format || lhs.desc.usage != rhs.desc.usage ||
      lhs.desc.extent.width != rhs.desc.extent.width ||
      lhs.desc.extent.height != rhs.desc.extent.height ||
      lhs.desc.extent.depth != rhs.desc.extent.depth) {
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
  }
}

} // namespace

CompiledFrameGraph compileFrameGraph(const FrameGraph &graph) {
  CompiledFrameGraph compiled;
  compiled.resources.reserve(graph.resources().size());
  compiled.passes.reserve(graph.passes().size());

  std::unordered_map<ResourceHandle, std::size_t> resource_to_index;
  resource_to_index.reserve(graph.resources().size());
  for (const ResourceNode &resource : graph.resources()) {
    resource_to_index.emplace(resource.handle, compiled.resources.size());
    compiled.resources.push_back({.handle = resource.handle,
                                  .name = resource.name,
                                  .desc = resource.desc,
                                  .first_pass = std::numeric_limits<std::size_t>::max(),
                                  .last_pass = 0u,
                                  .alias_group = 0u});
    if (resource.desc.lifetime == ResourceLifetime::Transient) {
      ++compiled.transient_resource_count;
    }
  }

  std::unordered_set<ResourceHandle> written;
  std::unordered_map<ResourceHandle, rhi::ResourceState> current_states;
  for (std::size_t pass_index = 0; pass_index < graph.passes().size(); ++pass_index) {
    const PassDesc &pass = graph.passes()[pass_index];
    CompiledPass compiled_pass;
    compiled_pass.name = pass.name;
    compiled_pass.reads = pass.reads;
    compiled_pass.writes = pass.writes;

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

      const rhi::ResourceState desired = readStateFor(compiled_resource.desc);
      compiled_pass.accesses.push_back({.resource = resource,
                                        .state = desired,
                                        .usage = compiled_resource.desc.usage,
                                        .write = false});
      if ((compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::Sampled)) != 0u) {
        DescriptorRequirement requirement{.pass_index = pass_index,
                                          .resource = resource,
                                          .kind = rhi::DescriptorRangeKind::SampledImage,
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
      if (before != desired) {
        rhi::ResourceBarrier expanded =
            rhi::completeResourceBarrier({.resource_id = resourceIdFor(resource),
                                          .before = before,
                                          .after = desired});
        compiled.barriers.push_back({resource, before, desired, pass_index, expanded});
        compiled.resource_barriers.push_back(expanded);
        current_states[resource] = desired;
      }
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

      const rhi::ResourceState desired = writeStateFor(compiled_resource.desc);
      const rhi::ResourceState before = current_states.contains(resource)
                                            ? current_states[resource]
                                            : rhi::ResourceState::Undefined;
      compiled_pass.accesses.push_back({.resource = resource,
                                        .state = desired,
                                        .usage = compiled_resource.desc.usage,
                                        .write = true});
      if ((compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::Storage)) != 0u) {
        DescriptorRequirement requirement{.pass_index = pass_index,
                                          .resource = resource,
                                          .kind = rhi::DescriptorRangeKind::StorageImage,
                                          .binding = static_cast<std::uint32_t>(
                                              compiled_pass.descriptor_requirements.size()),
                                          .count = 1u,
                                          .label = compiled_resource.name + "-uav"};
        compiled_pass.descriptor_requirements.push_back(requirement);
        compiled.descriptor_requirements.push_back(std::move(requirement));
      }
      if ((compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment)) !=
          0u) {
        compiled_pass.attachments.push_back(attachmentForWrite(compiled_resource.desc, before, desired));
        compiled_pass.pipeline_compatibility.color_formats.push_back(compiled_resource.desc.format);
      }
      if ((compiled_resource.desc.usage & rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment)) !=
          0u) {
        compiled_pass.attachments.push_back(attachmentForWrite(compiled_resource.desc, before, desired));
        compiled_pass.pipeline_compatibility.depth_format = compiled_resource.desc.format;
      }
      compiled_pass.pipeline_compatibility.sample_count = 1u;
      if (before != desired) {
        rhi::ResourceBarrier expanded =
            rhi::completeResourceBarrier({.resource_id = resourceIdFor(resource),
                                          .before = before,
                                          .after = desired});
        compiled.barriers.push_back({resource, before, desired, pass_index, expanded});
        compiled.resource_barriers.push_back(expanded);
        current_states[resource] = desired;
      }
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
  assignAliasGroups(compiled);
  return compiled;
}

std::string dumpFrameGraph(const CompiledFrameGraph &graph) {
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
  return out.str();
}

} // namespace aster::framegraph
