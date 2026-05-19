// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <vector>

namespace aster {

struct TransitionWipeFrame {
  bool active = false;
  bool finished = true;
  int width = 0;
  int height = 0;
  std::vector<float> column_progress;
};

class TransitionWipe {
public:
  void start(int columns, int height, std::uint32_t seed = 0u, float seconds = 0.72f);
  void update(float dt);
  void finish();

  [[nodiscard]] bool active() const;
  [[nodiscard]] bool finished() const;
  [[nodiscard]] TransitionWipeFrame frame() const;

private:
  int columns_ = 0;
  int height_ = 0;
  float age_ = 0.0f;
  float seconds_ = 0.72f;
  bool active_ = false;
  std::vector<float> offsets_;
};

} // namespace aster
