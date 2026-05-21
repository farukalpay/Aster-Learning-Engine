// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

namespace aster {

struct VerticalCapsuleContactVolume {
  Vec3 center{};
  float radius = 0.0f;
  float half_segment = 0.0f;
};

[[nodiscard]] bool overlaps(const VerticalCapsuleContactVolume &lhs,
                            const VerticalCapsuleContactVolume &rhs);

} // namespace aster
