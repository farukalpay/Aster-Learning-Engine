// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/scene.hpp"

namespace aster {

struct SoftwarePreviewOptions {
  int width = 960;
  int height = 540;
  int samples_per_axis = 1;
  double frame_seconds = 0.0;
  RendererSettings settings{};
};

[[nodiscard]] SoftwareFrameBuffer renderSoftwarePreview(const Scene &scene,
                                                        const OrbitCamera &camera,
                                                        const SoftwarePreviewOptions &options);

} // namespace aster
