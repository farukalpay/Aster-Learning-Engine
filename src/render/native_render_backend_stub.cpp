// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#if !defined(__APPLE__) && !defined(_WIN32)

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
