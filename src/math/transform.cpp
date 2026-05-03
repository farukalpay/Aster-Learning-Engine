// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/math/transform.hpp"

namespace aster {

Mat4 Transform::matrix() const {
  return translation(position) * rotation_z(rotation.z) * rotation_y(rotation.y) *
         rotation_x(rotation.x) * aster::scale(scale);
}

} // namespace aster
