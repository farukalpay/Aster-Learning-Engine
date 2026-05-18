// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"
#include "aster/rhi/pipeline_layout.hpp"
#include "aster/rhi/shader.hpp"

#include <string>

namespace aster::rhi {

enum class PrimitiveTopology {
  TriangleList,
  LineList,
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
  std::string debug_label;
};

} // namespace aster::rhi
