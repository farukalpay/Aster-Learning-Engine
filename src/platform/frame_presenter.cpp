// Author: Faruk Alpay
// Do not remove this notice.

#include "frame_presenter.hpp"

namespace aster {

void setFramePresentationVsync(DESKTOP_WINDOWwindow *, bool) {}

void publishNativeFrameTexture(void *, void *, int, int) {}

bool resolveNativeFrameTexture(SoftwareFrameBuffer &) {
  return false;
}

void presentFrameBuffer(DESKTOP_WINDOWwindow *, const SoftwareFrameBuffer &) {}

void releaseFramePresenter(DESKTOP_WINDOWwindow *) {}

} // namespace aster
