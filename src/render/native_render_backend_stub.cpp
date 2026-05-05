// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#ifndef __APPLE__

namespace aster {

std::unique_ptr<NativeRenderBackend> createNativeRenderBackend() {
  return {};
}

bool captureNativeFrameToActiveFramebuffer() {
  return false;
}

void clearNativeFrame() {}

} // namespace aster

#endif
