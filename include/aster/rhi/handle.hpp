// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <functional>

namespace aster::rhi {

template <typename Tag> struct Handle {
  std::uint64_t id = 0u;
  std::uint32_t generation = 0u;

  [[nodiscard]] constexpr bool valid() const {
    return id != 0u && generation != 0u;
  }

  [[nodiscard]] friend constexpr bool operator==(const Handle lhs, const Handle rhs) {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }

  [[nodiscard]] friend constexpr bool operator!=(const Handle lhs, const Handle rhs) {
    return !(lhs == rhs);
  }
};

struct BufferTag;
struct ImageTag;
struct ImageViewTag;
struct SamplerTag;
struct ShaderModuleTag;
struct PipelineLayoutTag;
struct GraphicsPipelineTag;
struct ComputePipelineTag;
struct DescriptorSetTag;
struct DescriptorHeapTag;
struct RenderTargetTag;
struct FramebufferTag;
struct FenceTag;
struct SemaphoreTag;
struct TimestampQueryTag;

using BufferHandle = Handle<BufferTag>;
using ImageHandle = Handle<ImageTag>;
using ImageViewHandle = Handle<ImageViewTag>;
using SamplerHandle = Handle<SamplerTag>;
using ShaderModuleHandle = Handle<ShaderModuleTag>;
using PipelineLayoutHandle = Handle<PipelineLayoutTag>;
using GraphicsPipelineHandle = Handle<GraphicsPipelineTag>;
using ComputePipelineHandle = Handle<ComputePipelineTag>;
using DescriptorSetHandle = Handle<DescriptorSetTag>;
using DescriptorHeapHandle = Handle<DescriptorHeapTag>;
using RenderTargetHandle = Handle<RenderTargetTag>;
using FramebufferHandle = Handle<FramebufferTag>;
using FenceHandle = Handle<FenceTag>;
using SemaphoreHandle = Handle<SemaphoreTag>;
using TimestampQueryHandle = Handle<TimestampQueryTag>;

} // namespace aster::rhi

namespace std {

template <typename Tag> struct hash<aster::rhi::Handle<Tag>> {
  [[nodiscard]] std::size_t operator()(const aster::rhi::Handle<Tag> handle) const noexcept {
    const std::uint64_t mixed =
        handle.id ^ (static_cast<std::uint64_t>(handle.generation) << 32u);
    return std::hash<std::uint64_t>{}(mixed);
  }
};

} // namespace std
