// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/rhi/resource_registry.hpp"

namespace aster::rhi {

BufferHandle ResourceRegistry::createBuffer(const BufferDesc &desc) {
  return create<BufferHandle>(ResourceKind::Buffer, desc.debug_label,
                              desc.cpu_visible ? ResourceResidency::CpuVisible
                                               : ResourceResidency::BackendOwned);
}

ImageHandle ResourceRegistry::createImage(const ImageDesc &desc) {
  return create<ImageHandle>(ResourceKind::Image, desc.debug_label);
}

ImageViewHandle ResourceRegistry::createImageView(const ImageViewDesc &desc) {
  return create<ImageViewHandle>(ResourceKind::ImageView, desc.debug_label);
}

SamplerHandle ResourceRegistry::createSampler(const SamplerDesc &desc) {
  return create<SamplerHandle>(ResourceKind::Sampler, desc.debug_label);
}

ShaderModuleHandle ResourceRegistry::createShaderModule(const ShaderModuleDesc &desc) {
  return create<ShaderModuleHandle>(ResourceKind::ShaderModule, desc.debug_label);
}

PipelineLayoutHandle ResourceRegistry::createPipelineLayout(const PipelineLayoutDesc &desc) {
  return create<PipelineLayoutHandle>(ResourceKind::PipelineLayout, desc.debug_label);
}

GraphicsPipelineHandle ResourceRegistry::createGraphicsPipeline(const GraphicsPipelineDesc &desc) {
  return create<GraphicsPipelineHandle>(ResourceKind::GraphicsPipeline, desc.debug_label);
}

ComputePipelineHandle ResourceRegistry::createComputePipeline(const ComputePipelineDesc &desc) {
  return create<ComputePipelineHandle>(ResourceKind::ComputePipeline, desc.debug_label);
}

DescriptorSetHandle ResourceRegistry::createDescriptorSet(const DescriptorSetDesc &desc) {
  return create<DescriptorSetHandle>(ResourceKind::DescriptorSet, desc.debug_label);
}

DescriptorHeapHandle ResourceRegistry::createDescriptorHeap(const DescriptorHeapDesc &desc) {
  return create<DescriptorHeapHandle>(ResourceKind::DescriptorHeap, desc.debug_label);
}

RenderTargetHandle ResourceRegistry::createRenderTarget(const RenderTargetDesc &desc) {
  return create<RenderTargetHandle>(ResourceKind::RenderTarget, desc.debug_label);
}

FramebufferHandle ResourceRegistry::createFramebuffer(const FramebufferDesc &desc) {
  return create<FramebufferHandle>(ResourceKind::Framebuffer, desc.debug_label);
}

FenceHandle ResourceRegistry::createFence(const FenceDesc &desc) {
  return create<FenceHandle>(ResourceKind::Fence, desc.debug_label);
}

SemaphoreHandle ResourceRegistry::createSemaphore(const SemaphoreDesc &desc) {
  return create<SemaphoreHandle>(ResourceKind::Semaphore, desc.debug_label);
}

TimestampQueryHandle ResourceRegistry::createTimestampQuery(const TimestampQueryDesc &desc) {
  return create<TimestampQueryHandle>(ResourceKind::TimestampQuery, desc.debug_label);
}

ResourceRegistryStats ResourceRegistry::stats() const {
  ResourceRegistryStats stats;
  for (const auto &[id, entry] : entries_) {
    (void)id;
    if (entry.residency == ResourceResidency::Retired) {
      ++stats.retired_resources;
      continue;
    }
    ++stats.live_resources;
    switch (entry.kind) {
    case ResourceKind::Buffer:
      ++stats.buffers;
      break;
    case ResourceKind::Image:
    case ResourceKind::ImageView:
    case ResourceKind::RenderTarget:
      ++stats.images;
      break;
    case ResourceKind::Sampler:
      ++stats.samplers;
      break;
    case ResourceKind::ShaderModule:
      ++stats.shaders;
      break;
    case ResourceKind::PipelineLayout:
    case ResourceKind::GraphicsPipeline:
    case ResourceKind::ComputePipeline:
      ++stats.pipelines;
      break;
    case ResourceKind::Framebuffer:
      ++stats.framebuffers;
      break;
    case ResourceKind::DescriptorSet:
    case ResourceKind::DescriptorHeap:
    case ResourceKind::Fence:
    case ResourceKind::Semaphore:
    case ResourceKind::TimestampQuery:
      break;
    }
  }
  return stats;
}

std::optional<ResourceRegistryEntrySnapshot> ResourceRegistry::snapshot(const std::uint64_t id) const {
  const auto found = entries_.find(id);
  if (found == entries_.end()) {
    return std::nullopt;
  }
  return ResourceRegistryEntrySnapshot{.id = id,
                                       .generation = found->second.generation,
                                       .kind = found->second.kind,
                                       .residency = found->second.residency,
                                       .version = found->second.version,
                                       .label = found->second.label};
}

void ResourceRegistry::clearRetired() {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.residency == ResourceResidency::Retired) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace aster::rhi
