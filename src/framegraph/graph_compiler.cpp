// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/framegraph/graph_compiler.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace aster::framegraph {
namespace {

[[nodiscard]] rhi::ResourceState readStateFor(const ResourceDesc &desc) {
  if ((desc.usage & rhi::imageUsageBit(rhi::ImageUsage::TransferSource)) != 0u ||
      desc.lifetime == ResourceLifetime::Readback) {
    return rhi::ResourceState::Readback;
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
                                  .last_pass = 0u});
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
      const rhi::ResourceState before = current_states.contains(resource)
                                            ? current_states[resource]
                                            : rhi::ResourceState::Undefined;
      if (before != desired) {
        compiled.barriers.push_back({resource, before, desired, pass_index});
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
      if (before != desired) {
        compiled.barriers.push_back({resource, before, desired, pass_index});
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
  return out.str();
}

} // namespace aster::framegraph
