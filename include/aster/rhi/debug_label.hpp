// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <string>
#include <string_view>

namespace aster::rhi {

struct DebugLabel {
  std::string name;

  [[nodiscard]] bool empty() const {
    return name.empty();
  }

  [[nodiscard]] std::string_view view() const {
    return name;
  }
};

} // namespace aster::rhi
