// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/render_target.hpp"
#include "aster/rhi/resource_barrier.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster::rhi {

enum class AttachmentLoadOp : std::uint32_t {
  Load,
  Clear,
  DontCare,
};

enum class AttachmentStoreOp : std::uint32_t {
  Store,
  DontCare,
};

struct AttachmentClearValue {
  float color[4]{0.0f, 0.0f, 0.0f, 0.0f};
  float depth = 1.0f;
  std::uint32_t stencil = 0u;
};

struct FramebufferAttachmentDesc {
  ImageViewHandle view{};
  ImageFormat format = ImageFormat::Unknown;
  AttachmentLoadOp load_op = AttachmentLoadOp::Clear;
  AttachmentStoreOp store_op = AttachmentStoreOp::Store;
  ResourceState initial_state = ResourceState::Undefined;
  ResourceState final_state = ResourceState::ColorAttachment;
  ImageSubresourceRange subresources{};
  AttachmentClearValue clear_value{};
  bool transient = false;
};

struct FramebufferDesc {
  RenderTargetHandle render_target{};
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<FramebufferAttachmentDesc> color_attachments;
  FramebufferAttachmentDesc depth_stencil_attachment{
      .format = ImageFormat::Depth32Float,
      .final_state = ResourceState::DepthAttachment,
      .subresources = {.aspect_mask = textureAspectBit(TextureAspect::Depth)}};
  bool has_depth_stencil_attachment = false;
  std::string debug_label;
};

} // namespace aster::rhi
