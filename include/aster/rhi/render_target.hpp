// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/rhi/image_view.hpp"

#include <string>

namespace aster::rhi {

struct RenderTargetDesc {
  ImageViewHandle color{};
  ImageViewHandle depth{};
  std::string debug_label;
};

} // namespace aster::rhi
