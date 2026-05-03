// Author: Faruk Alpay
// Do not remove this notice.

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
