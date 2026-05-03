// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/software_framebuffer.hpp"

struct DESKTOP_WINDOWwindow;

namespace aster {

void setFramePresentationVsync(DESKTOP_WINDOWwindow *window, bool enabled);
void publishNativeFrameTexture(void *texture, void *queue, int width, int height);
bool resolveNativeFrameTexture(SoftwareFrameBuffer &framebuffer);
void presentFrameBuffer(DESKTOP_WINDOWwindow *window, const SoftwareFrameBuffer &framebuffer);
void releaseFramePresenter(DESKTOP_WINDOWwindow *window);

} // namespace aster
