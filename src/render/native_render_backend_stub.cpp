// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#ifndef ASTER_HAS_D3D12_BACKEND
#define ASTER_HAS_D3D12_BACKEND 0
#endif

#if !defined(__APPLE__) && !ASTER_HAS_D3D12_BACKEND

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
