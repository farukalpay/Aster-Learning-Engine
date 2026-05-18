// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/buffer.hpp"
#include "aster/rhi/compute_pipeline.hpp"
#include "aster/rhi/descriptor_heap.hpp"
#include "aster/rhi/descriptor_set.hpp"
#include "aster/rhi/fence.hpp"
#include "aster/rhi/framebuffer.hpp"
#include "aster/rhi/graphics_pipeline.hpp"
#include "aster/rhi/image_view.hpp"
#include "aster/rhi/sampler.hpp"
#include "aster/rhi/semaphore.hpp"
#include "aster/rhi/timestamp_query.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace aster::rhi {

enum class ResourceKind : std::uint32_t {
  Buffer,
  Image,
  ImageView,
  Sampler,
  ShaderModule,
  PipelineLayout,
  GraphicsPipeline,
  ComputePipeline,
  DescriptorSet,
  DescriptorHeap,
  RenderTarget,
  Framebuffer,
  Fence,
  Semaphore,
  TimestampQuery,
};

enum class ResourceResidency : std::uint32_t {
  Unresident,
  CpuVisible,
  BackendOwned,
  Retired,
};

struct ResourceRegistryStats {
  std::size_t live_resources = 0u;
  std::size_t retired_resources = 0u;
  std::size_t buffers = 0u;
  std::size_t images = 0u;
  std::size_t samplers = 0u;
  std::size_t shaders = 0u;
  std::size_t pipelines = 0u;
  std::size_t framebuffers = 0u;
};

class ResourceRegistry {
public:
  BufferHandle createBuffer(const BufferDesc &desc);
  ImageHandle createImage(const ImageDesc &desc);
  ImageViewHandle createImageView(const ImageViewDesc &desc);
  SamplerHandle createSampler(const SamplerDesc &desc);
  ShaderModuleHandle createShaderModule(const ShaderModuleDesc &desc);
  PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc &desc);
  GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc &desc);
  ComputePipelineHandle createComputePipeline(const ComputePipelineDesc &desc);
  DescriptorSetHandle createDescriptorSet(const DescriptorSetDesc &desc);
  DescriptorHeapHandle createDescriptorHeap(const DescriptorHeapDesc &desc);
  RenderTargetHandle createRenderTarget(const RenderTargetDesc &desc);
  FramebufferHandle createFramebuffer(const FramebufferDesc &desc);
  FenceHandle createFence(const FenceDesc &desc);
  SemaphoreHandle createSemaphore(const SemaphoreDesc &desc);
  TimestampQueryHandle createTimestampQuery(const TimestampQueryDesc &desc);

  template <typename Tag> [[nodiscard]] bool contains(const Handle<Tag> handle) const {
    const auto found = entries_.find(handle.id);
    return found != entries_.end() && found->second.generation == handle.generation &&
           found->second.kind == kindFor<Tag>() &&
           found->second.residency != ResourceResidency::Retired;
  }

  template <typename Tag> bool destroy(const Handle<Tag> handle) {
    auto found = entries_.find(handle.id);
    if (found == entries_.end() || found->second.generation != handle.generation ||
        found->second.kind != kindFor<Tag>() ||
        found->second.residency == ResourceResidency::Retired) {
      return false;
    }
    found->second.residency = ResourceResidency::Retired;
    return true;
  }

  template <typename Tag> [[nodiscard]] Handle<Tag> invalidate(const Handle<Tag> handle) {
    auto found = entries_.find(handle.id);
    if (found == entries_.end() || found->second.generation != handle.generation ||
        found->second.kind != kindFor<Tag>() ||
        found->second.residency == ResourceResidency::Retired) {
      return {};
    }
    ++found->second.generation;
    found->second.version++;
    return {handle.id, found->second.generation};
  }

  template <typename Tag> [[nodiscard]] std::string_view label(const Handle<Tag> handle) const {
    const auto found = entries_.find(handle.id);
    if (found == entries_.end() || found->second.generation != handle.generation ||
        found->second.kind != kindFor<Tag>()) {
      return {};
    }
    return found->second.label;
  }

  template <typename Tag> bool setLabel(const Handle<Tag> handle, std::string label) {
    auto found = entries_.find(handle.id);
    if (found == entries_.end() || found->second.generation != handle.generation ||
        found->second.kind != kindFor<Tag>() ||
        found->second.residency == ResourceResidency::Retired) {
      return false;
    }
    found->second.label = std::move(label);
    return true;
  }

  [[nodiscard]] ResourceRegistryStats stats() const;
  void clearRetired();

private:
  struct Entry {
    ResourceKind kind = ResourceKind::Buffer;
    std::uint32_t generation = 1u;
    ResourceResidency residency = ResourceResidency::Unresident;
    std::uint64_t version = 1u;
    std::string label;
  };

  template <typename Tag> [[nodiscard]] static constexpr ResourceKind kindFor();

  template <typename HandleType>
  [[nodiscard]] HandleType create(ResourceKind kind, std::string_view label,
                                  ResourceResidency residency = ResourceResidency::BackendOwned) {
    const std::uint64_t id = next_id_++;
    entries_.emplace(id, Entry{kind, 1u, residency, 1u, std::string(label)});
    return {id, 1u};
  }

  std::uint64_t next_id_ = 1u;
  std::unordered_map<std::uint64_t, Entry> entries_;
};

template <> constexpr ResourceKind ResourceRegistry::kindFor<BufferTag>() {
  return ResourceKind::Buffer;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<ImageTag>() {
  return ResourceKind::Image;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<ImageViewTag>() {
  return ResourceKind::ImageView;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<SamplerTag>() {
  return ResourceKind::Sampler;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<ShaderModuleTag>() {
  return ResourceKind::ShaderModule;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<PipelineLayoutTag>() {
  return ResourceKind::PipelineLayout;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<GraphicsPipelineTag>() {
  return ResourceKind::GraphicsPipeline;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<ComputePipelineTag>() {
  return ResourceKind::ComputePipeline;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<DescriptorSetTag>() {
  return ResourceKind::DescriptorSet;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<DescriptorHeapTag>() {
  return ResourceKind::DescriptorHeap;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<RenderTargetTag>() {
  return ResourceKind::RenderTarget;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<FramebufferTag>() {
  return ResourceKind::Framebuffer;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<FenceTag>() {
  return ResourceKind::Fence;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<SemaphoreTag>() {
  return ResourceKind::Semaphore;
}
template <> constexpr ResourceKind ResourceRegistry::kindFor<TimestampQueryTag>() {
  return ResourceKind::TimestampQuery;
}

} // namespace aster::rhi
