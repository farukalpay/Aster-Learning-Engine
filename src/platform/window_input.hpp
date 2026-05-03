// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/input/control_scheme.hpp"

struct DESKTOP_WINDOWwindow;

namespace aster {

ControlSnapshot captureWindowControls(DESKTOP_WINDOWwindow *window, const ControlScheme &scheme);

} // namespace aster
