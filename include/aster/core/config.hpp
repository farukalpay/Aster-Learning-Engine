// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

namespace aster {

struct EngineConfig {
  const char *application_name = "Aster Learning Studio";
  int initial_width = 1600;
  int initial_height = 900;
  int multisample_samples = 4;
  bool enable_vsync = true;
  bool scale_framebuffer_to_display = false;
};

} // namespace aster
