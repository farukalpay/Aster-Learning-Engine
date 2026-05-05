// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/frame_capture.hpp"

#include "aster/render/software_framebuffer.hpp"
#include "native_render_backend.hpp"

#include <stdexcept>

namespace aster {

void writeFramebufferPpm(const std::filesystem::path &path, const int width, const int height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("Framebuffer capture requires a positive size.");
  }
  (void)captureNativeFrameToActiveFramebuffer();
  activeFrameBuffer().writePpm(path, width, height);
}

} // namespace aster
