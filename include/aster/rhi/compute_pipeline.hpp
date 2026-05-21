// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/rhi/pipeline_layout.hpp"
#include "aster/rhi/shader.hpp"

#include <string>

namespace aster::rhi {

struct ComputePipelineDesc {
  PipelineLayoutHandle layout{};
  ShaderModuleHandle shader{};
  std::string debug_label;
};

} // namespace aster::rhi
