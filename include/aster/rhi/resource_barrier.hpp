// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"
#include "aster/rhi/queue.hpp"

#include <cstdint>

namespace aster::rhi {

enum class ResourceState : std::uint32_t {
  Undefined,
  CopySource,
  CopyDestination,
  ShaderRead,
  ShaderWrite,
  ColorAttachment,
  DepthAttachment,
  Present,
  Readback,
};

enum class PipelineStage : std::uint64_t {
  TopOfPipe = 1ull << 0u,
  DrawIndirect = 1ull << 1u,
  VertexInput = 1ull << 2u,
  VertexShader = 1ull << 3u,
  FragmentShader = 1ull << 4u,
  EarlyFragmentTests = 1ull << 5u,
  LateFragmentTests = 1ull << 6u,
  ColorAttachmentOutput = 1ull << 7u,
  ComputeShader = 1ull << 8u,
  Transfer = 1ull << 9u,
  Host = 1ull << 10u,
  Present = 1ull << 11u,
  BottomOfPipe = 1ull << 12u,
};

[[nodiscard]] constexpr std::uint64_t pipelineStageBit(const PipelineStage stage) {
  return static_cast<std::uint64_t>(stage);
}

enum class ResourceAccess : std::uint64_t {
  IndirectCommandRead = 1ull << 0u,
  IndexRead = 1ull << 1u,
  VertexAttributeRead = 1ull << 2u,
  UniformRead = 1ull << 3u,
  ShaderRead = 1ull << 4u,
  ShaderWrite = 1ull << 5u,
  ColorAttachmentRead = 1ull << 6u,
  ColorAttachmentWrite = 1ull << 7u,
  DepthStencilRead = 1ull << 8u,
  DepthStencilWrite = 1ull << 9u,
  TransferRead = 1ull << 10u,
  TransferWrite = 1ull << 11u,
  HostRead = 1ull << 12u,
  HostWrite = 1ull << 13u,
  MemoryRead = 1ull << 14u,
  MemoryWrite = 1ull << 15u,
};

[[nodiscard]] constexpr std::uint64_t resourceAccessBit(const ResourceAccess access) {
  return static_cast<std::uint64_t>(access);
}

enum class ImageLayout : std::uint32_t {
  Undefined,
  General,
  ColorAttachment,
  DepthStencilAttachment,
  DepthStencilReadOnly,
  ShaderReadOnly,
  TransferSource,
  TransferDestination,
  Present,
};

enum class TextureAspect : std::uint32_t {
  Color = 1u << 0u,
  Depth = 1u << 1u,
  Stencil = 1u << 2u,
};

[[nodiscard]] constexpr std::uint32_t textureAspectBit(const TextureAspect aspect) {
  return static_cast<std::uint32_t>(aspect);
}

struct ImageSubresourceRange {
  std::uint32_t aspect_mask = textureAspectBit(TextureAspect::Color);
  std::uint32_t base_mip = 0u;
  std::uint32_t mip_count = 1u;
  std::uint32_t base_layer = 0u;
  std::uint32_t layer_count = 1u;
};

struct ResourceBarrier {
  std::uint64_t resource_id = 0u;
  ResourceState before = ResourceState::Undefined;
  ResourceState after = ResourceState::Undefined;
  std::uint64_t source_stage_mask = 0u;
  std::uint64_t destination_stage_mask = 0u;
  std::uint64_t source_access_mask = 0u;
  std::uint64_t destination_access_mask = 0u;
  ImageLayout old_layout = ImageLayout::Undefined;
  ImageLayout new_layout = ImageLayout::Undefined;
  ImageSubresourceRange subresources{};
  QueueKind source_queue = QueueKind::Graphics;
  QueueKind destination_queue = QueueKind::Graphics;
  bool discard_before = false;
  bool transient_attachment = false;
};

[[nodiscard]] constexpr ImageLayout resourceStateDefaultLayout(const ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
    return ImageLayout::Undefined;
  case ResourceState::CopySource:
    return ImageLayout::TransferSource;
  case ResourceState::CopyDestination:
    return ImageLayout::TransferDestination;
  case ResourceState::ShaderRead:
    return ImageLayout::ShaderReadOnly;
  case ResourceState::ShaderWrite:
    return ImageLayout::General;
  case ResourceState::ColorAttachment:
    return ImageLayout::ColorAttachment;
  case ResourceState::DepthAttachment:
    return ImageLayout::DepthStencilAttachment;
  case ResourceState::Present:
    return ImageLayout::Present;
  case ResourceState::Readback:
    return ImageLayout::General;
  }
  return ImageLayout::Undefined;
}

[[nodiscard]] constexpr std::uint64_t resourceStateDefaultStageMask(const ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
    return pipelineStageBit(PipelineStage::TopOfPipe);
  case ResourceState::CopySource:
  case ResourceState::CopyDestination:
    return pipelineStageBit(PipelineStage::Transfer);
  case ResourceState::ShaderRead:
    return pipelineStageBit(PipelineStage::VertexShader) |
           pipelineStageBit(PipelineStage::FragmentShader) |
           pipelineStageBit(PipelineStage::ComputeShader);
  case ResourceState::ShaderWrite:
    return pipelineStageBit(PipelineStage::ComputeShader);
  case ResourceState::ColorAttachment:
    return pipelineStageBit(PipelineStage::ColorAttachmentOutput);
  case ResourceState::DepthAttachment:
    return pipelineStageBit(PipelineStage::EarlyFragmentTests) |
           pipelineStageBit(PipelineStage::LateFragmentTests);
  case ResourceState::Present:
    return pipelineStageBit(PipelineStage::Present);
  case ResourceState::Readback:
    return pipelineStageBit(PipelineStage::Host);
  }
  return pipelineStageBit(PipelineStage::BottomOfPipe);
}

[[nodiscard]] constexpr std::uint64_t resourceStateDefaultAccessMask(const ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
  case ResourceState::Present:
    return 0u;
  case ResourceState::CopySource:
    return resourceAccessBit(ResourceAccess::TransferRead);
  case ResourceState::CopyDestination:
    return resourceAccessBit(ResourceAccess::TransferWrite);
  case ResourceState::ShaderRead:
    return resourceAccessBit(ResourceAccess::ShaderRead) |
           resourceAccessBit(ResourceAccess::UniformRead);
  case ResourceState::ShaderWrite:
    return resourceAccessBit(ResourceAccess::ShaderWrite);
  case ResourceState::ColorAttachment:
    return resourceAccessBit(ResourceAccess::ColorAttachmentRead) |
           resourceAccessBit(ResourceAccess::ColorAttachmentWrite);
  case ResourceState::DepthAttachment:
    return resourceAccessBit(ResourceAccess::DepthStencilRead) |
           resourceAccessBit(ResourceAccess::DepthStencilWrite);
  case ResourceState::Readback:
    return resourceAccessBit(ResourceAccess::HostRead);
  }
  return 0u;
}

[[nodiscard]] constexpr ResourceBarrier completeResourceBarrier(ResourceBarrier barrier) {
  if (barrier.source_stage_mask == 0u) {
    barrier.source_stage_mask = resourceStateDefaultStageMask(barrier.before);
  }
  if (barrier.destination_stage_mask == 0u) {
    barrier.destination_stage_mask = resourceStateDefaultStageMask(barrier.after);
  }
  if (barrier.source_access_mask == 0u) {
    barrier.source_access_mask = resourceStateDefaultAccessMask(barrier.before);
  }
  if (barrier.destination_access_mask == 0u) {
    barrier.destination_access_mask = resourceStateDefaultAccessMask(barrier.after);
  }
  if (barrier.old_layout == ImageLayout::Undefined && barrier.before != ResourceState::Undefined) {
    barrier.old_layout = resourceStateDefaultLayout(barrier.before);
  }
  if (barrier.new_layout == ImageLayout::Undefined && barrier.after != ResourceState::Undefined) {
    barrier.new_layout = resourceStateDefaultLayout(barrier.after);
  }
  return barrier;
}

[[nodiscard]] constexpr bool transfersQueueOwnership(const ResourceBarrier &barrier) {
  return barrier.source_queue != barrier.destination_queue;
}

} // namespace aster::rhi
