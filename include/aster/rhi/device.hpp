// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/adapter.hpp"
#include "aster/rhi/image.hpp"
#include "aster/rhi/sampler.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class BlendMode : std::uint32_t {
  Opaque,
  AlphaBlend,
  Additive,
};

enum class ShaderModel : std::uint32_t {
  None,
  SoftwareReference,
  MetalMSL23,
  D3D12ShaderModel51,
};

enum class PresentationMode : std::uint32_t {
  None,
  SoftwareFramebuffer,
  MetalLayer,
  D3D12OffscreenReadback,
};

struct DeviceLimits {
  std::uint32_t max_color_attachments = 0u;
  std::uint32_t max_sampled_textures_per_material = 0u;
  std::uint32_t max_samplers_per_material = 0u;
  std::uint32_t max_uniform_buffers_per_stage = 0u;
  std::uint32_t max_storage_buffers_per_stage = 0u;
  std::uint32_t max_bind_groups = 0u;
  std::uint32_t max_vertex_attributes = 0u;
  std::uint32_t max_texture_dimension_2d = 0u;
  std::uint32_t max_dynamic_uniform_bytes = 0u;
};

[[nodiscard]] constexpr std::uint64_t imageFormatCapabilityBit(const ImageFormat format) {
  return 1ull << static_cast<std::uint32_t>(format);
}

[[nodiscard]] constexpr std::uint64_t sampleCountCapabilityBit(const std::uint32_t sample_count) {
  return sample_count < 64u ? (1ull << sample_count) : 0ull;
}

[[nodiscard]] constexpr std::uint64_t filterModeCapabilityBit(const FilterMode mode) {
  return 1ull << static_cast<std::uint32_t>(mode);
}

[[nodiscard]] constexpr std::uint64_t addressModeCapabilityBit(const AddressMode mode) {
  return 1ull << static_cast<std::uint32_t>(mode);
}

[[nodiscard]] constexpr std::uint64_t blendModeCapabilityBit(const BlendMode mode) {
  return 1ull << static_cast<std::uint32_t>(mode);
}

struct DeviceCapabilities {
  BackendKind backend = BackendKind::Unknown;
  bool shader_materials = false;
  bool texture_sampling = false;
  bool instancing = false;
  bool capture = false;
  bool ui_composite = false;
  bool gpu_timestamps = false;
  bool storage_buffers = false;
  bool texture_arrays = false;
  bool shadow_maps = false;
  bool debug_markers = false;
  bool hdr_render_targets = false;
  bool msaa = false;
  std::uint64_t color_format_mask = 0u;
  std::uint64_t depth_format_mask = 0u;
  std::uint64_t sample_count_mask = 0u;
  std::uint64_t sampler_filter_mask = 0u;
  std::uint64_t sampler_address_mode_mask = 0u;
  std::uint64_t blend_mode_mask = 0u;
  ShaderModel shader_model = ShaderModel::None;
  PresentationMode presentation = PresentationMode::None;
  DeviceLimits limits{};
};

struct DeviceDesc {
  AdapterDesc adapter{};
  std::string debug_label;
};

} // namespace aster::rhi
