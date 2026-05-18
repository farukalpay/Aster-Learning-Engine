// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"
#include "aster/rhi/pipeline_layout.hpp"
#include "aster/rhi/shader.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster::rhi {

enum class PrimitiveTopology {
  TriangleList,
  LineList,
};

enum class CullMode : std::uint32_t {
  None,
  Front,
  Back,
};

enum class FrontFace : std::uint32_t {
  CounterClockwise,
  Clockwise,
};

enum class PolygonMode : std::uint32_t {
  Fill,
  Line,
  Point,
};

enum class CompareOp : std::uint32_t {
  Never,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always,
};

enum class BlendFactor : std::uint32_t {
  Zero,
  One,
  SourceColor,
  OneMinusSourceColor,
  DestinationColor,
  OneMinusDestinationColor,
  SourceAlpha,
  OneMinusSourceAlpha,
  DestinationAlpha,
  OneMinusDestinationAlpha,
};

enum class BlendOp : std::uint32_t {
  Add,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

enum class ColorComponent : std::uint32_t {
  Red = 1u << 0u,
  Green = 1u << 1u,
  Blue = 1u << 2u,
  Alpha = 1u << 3u,
};

[[nodiscard]] constexpr std::uint32_t colorComponentBit(const ColorComponent component) {
  return static_cast<std::uint32_t>(component);
}

constexpr std::uint32_t kColorWriteAll =
    colorComponentBit(ColorComponent::Red) | colorComponentBit(ColorComponent::Green) |
    colorComponentBit(ColorComponent::Blue) | colorComponentBit(ColorComponent::Alpha);

enum class VertexFormat : std::uint32_t {
  Float32,
  Float32x2,
  Float32x3,
  Float32x4,
  Uint16,
  Uint32,
};

enum class VertexInputRate : std::uint32_t {
  Vertex,
  Instance,
};

enum class DynamicState : std::uint32_t {
  Viewport = 1u << 0u,
  Scissor = 1u << 1u,
  BlendConstants = 1u << 2u,
  DepthBias = 1u << 3u,
  StencilReference = 1u << 4u,
};

[[nodiscard]] constexpr std::uint32_t dynamicStateBit(const DynamicState state) {
  return static_cast<std::uint32_t>(state);
}

struct RasterizerStateDesc {
  CullMode cull_mode = CullMode::Back;
  FrontFace front_face = FrontFace::CounterClockwise;
  PolygonMode polygon_mode = PolygonMode::Fill;
  bool depth_clamp_enabled = false;
  bool depth_bias_enabled = false;
  float depth_bias_constant = 0.0f;
  float depth_bias_slope = 0.0f;
};

struct MultisampleStateDesc {
  std::uint32_t sample_count = 1u;
  std::uint32_t sample_mask = 0xffffffffu;
  bool alpha_to_coverage_enabled = false;
};

struct DepthStencilStateDesc {
  bool depth_test_enabled = true;
  bool depth_write_enabled = true;
  CompareOp depth_compare = CompareOp::GreaterOrEqual;
  bool stencil_test_enabled = false;
  std::uint32_t stencil_read_mask = 0xffu;
  std::uint32_t stencil_write_mask = 0xffu;
  CompareOp front_compare = CompareOp::Always;
  CompareOp back_compare = CompareOp::Always;
};

struct ColorBlendAttachmentDesc {
  bool blend_enabled = false;
  BlendFactor source_color_factor = BlendFactor::SourceAlpha;
  BlendFactor destination_color_factor = BlendFactor::OneMinusSourceAlpha;
  BlendOp color_op = BlendOp::Add;
  BlendFactor source_alpha_factor = BlendFactor::One;
  BlendFactor destination_alpha_factor = BlendFactor::OneMinusSourceAlpha;
  BlendOp alpha_op = BlendOp::Add;
  std::uint32_t color_write_mask = kColorWriteAll;
};

struct VertexBindingDesc {
  std::uint32_t binding = 0u;
  std::uint32_t stride = 0u;
  VertexInputRate input_rate = VertexInputRate::Vertex;
};

struct VertexAttributeDesc {
  std::uint32_t location = 0u;
  std::uint32_t binding = 0u;
  VertexFormat format = VertexFormat::Float32x3;
  std::uint32_t offset = 0u;
};

struct VertexInputLayoutDesc {
  std::vector<VertexBindingDesc> bindings;
  std::vector<VertexAttributeDesc> attributes;
};

struct RenderPassCompatibilityDesc {
  std::vector<ImageFormat> color_formats;
  ImageFormat depth_format = ImageFormat::Depth32Float;
  std::uint32_t sample_count = 1u;
  std::uint64_t subpass_signature = 0u;
};

struct GraphicsPipelineDesc {
  PipelineLayoutHandle layout{};
  ShaderModuleHandle vertex_shader{};
  ShaderModuleHandle fragment_shader{};
  ImageFormat color_format = ImageFormat::Bgra8Unorm;
  ImageFormat depth_format = ImageFormat::Depth32Float;
  PrimitiveTopology topology = PrimitiveTopology::TriangleList;
  bool depth_test = true;
  bool depth_write = true;
  bool blend_enabled = false;
  RasterizerStateDesc rasterizer{};
  MultisampleStateDesc multisample{};
  DepthStencilStateDesc depth_stencil{};
  std::vector<ColorBlendAttachmentDesc> color_blend_attachments;
  VertexInputLayoutDesc vertex_input{};
  RenderPassCompatibilityDesc render_pass{};
  std::uint32_t dynamic_state_mask =
      dynamicStateBit(DynamicState::Viewport) | dynamicStateBit(DynamicState::Scissor);
  std::uint64_t pipeline_cache_key = 0u;
  std::string debug_label;
};

[[nodiscard]] inline RenderPassCompatibilityDesc
renderPassCompatibilityForLegacyFormats(const GraphicsPipelineDesc &desc) {
  RenderPassCompatibilityDesc compatibility = desc.render_pass;
  if (compatibility.color_formats.empty() && desc.color_format != ImageFormat::Unknown) {
    compatibility.color_formats.push_back(desc.color_format);
  }
  if (compatibility.depth_format == ImageFormat::Unknown) {
    compatibility.depth_format = desc.depth_format;
  }
  if (compatibility.sample_count == 0u) {
    compatibility.sample_count = desc.multisample.sample_count;
  }
  return compatibility;
}

[[nodiscard]] inline ColorBlendAttachmentDesc
colorBlendAttachmentForLegacyBlendFlag(const GraphicsPipelineDesc &desc) {
  ColorBlendAttachmentDesc attachment;
  attachment.blend_enabled = desc.blend_enabled;
  return attachment;
}

[[nodiscard]] inline DepthStencilStateDesc
depthStencilStateForCompatibility(const GraphicsPipelineDesc &desc) {
  DepthStencilStateDesc state = desc.depth_stencil;
  state.depth_test_enabled = state.depth_test_enabled && desc.depth_test;
  state.depth_write_enabled = state.depth_write_enabled && desc.depth_write;
  return state;
}

namespace detail {

[[nodiscard]] constexpr std::uint64_t fnv1aAppend(std::uint64_t hash,
                                                  const std::uint64_t value) {
  constexpr std::uint64_t kPrime = 1099511628211ull;
  hash ^= value;
  hash *= kPrime;
  return hash;
}

} // namespace detail

[[nodiscard]] inline std::uint64_t graphicsPipelineCacheKey(const GraphicsPipelineDesc &desc) {
  if (desc.pipeline_cache_key != 0u) {
    return desc.pipeline_cache_key;
  }

  std::uint64_t hash = 1469598103934665603ull;
  hash = detail::fnv1aAppend(hash, desc.layout.id);
  hash = detail::fnv1aAppend(hash, desc.vertex_shader.id);
  hash = detail::fnv1aAppend(hash, desc.fragment_shader.id);
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(desc.topology));
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(desc.rasterizer.cull_mode));
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(desc.rasterizer.front_face));
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(desc.rasterizer.polygon_mode));
  hash = detail::fnv1aAppend(hash, desc.rasterizer.depth_clamp_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, desc.rasterizer.depth_bias_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, desc.multisample.sample_count);
  hash = detail::fnv1aAppend(hash, desc.multisample.sample_mask);
  hash = detail::fnv1aAppend(hash, desc.multisample.alpha_to_coverage_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, desc.dynamic_state_mask);
  const DepthStencilStateDesc depth_stencil = depthStencilStateForCompatibility(desc);
  hash = detail::fnv1aAppend(hash, depth_stencil.depth_test_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, depth_stencil.depth_write_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(depth_stencil.depth_compare));
  hash = detail::fnv1aAppend(hash, depth_stencil.stencil_test_enabled ? 1u : 0u);
  hash = detail::fnv1aAppend(hash, depth_stencil.stencil_read_mask);
  hash = detail::fnv1aAppend(hash, depth_stencil.stencil_write_mask);
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(depth_stencil.front_compare));
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(depth_stencil.back_compare));

  const RenderPassCompatibilityDesc render_pass = renderPassCompatibilityForLegacyFormats(desc);
  hash = detail::fnv1aAppend(hash, render_pass.sample_count);
  hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(render_pass.depth_format));
  hash = detail::fnv1aAppend(hash, render_pass.subpass_signature);
  for (const ImageFormat format : render_pass.color_formats) {
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(format));
  }
  for (const VertexBindingDesc &binding : desc.vertex_input.bindings) {
    hash = detail::fnv1aAppend(hash, binding.binding);
    hash = detail::fnv1aAppend(hash, binding.stride);
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(binding.input_rate));
  }
  for (const VertexAttributeDesc &attribute : desc.vertex_input.attributes) {
    hash = detail::fnv1aAppend(hash, attribute.location);
    hash = detail::fnv1aAppend(hash, attribute.binding);
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attribute.format));
    hash = detail::fnv1aAppend(hash, attribute.offset);
  }
  if (desc.color_blend_attachments.empty()) {
    const ColorBlendAttachmentDesc attachment = colorBlendAttachmentForLegacyBlendFlag(desc);
    hash = detail::fnv1aAppend(hash, attachment.blend_enabled ? 1u : 0u);
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.source_color_factor));
    hash =
        detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.destination_color_factor));
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.color_op));
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.source_alpha_factor));
    hash =
        detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.destination_alpha_factor));
    hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.alpha_op));
    hash = detail::fnv1aAppend(hash, attachment.color_write_mask);
  } else {
    for (const ColorBlendAttachmentDesc &attachment : desc.color_blend_attachments) {
      hash = detail::fnv1aAppend(hash, attachment.blend_enabled ? 1u : 0u);
      hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.source_color_factor));
      hash =
          detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.destination_color_factor));
      hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.color_op));
      hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.source_alpha_factor));
      hash =
          detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.destination_alpha_factor));
      hash = detail::fnv1aAppend(hash, static_cast<std::uint64_t>(attachment.alpha_op));
      hash = detail::fnv1aAppend(hash, attachment.color_write_mask);
    }
  }
  return hash;
}

} // namespace aster::rhi
