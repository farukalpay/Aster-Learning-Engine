// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

namespace aster {

[[nodiscard]] float easeOutCubic(float value);

class ScalarAnimation {
public:
  void snap(float value);
  void setTarget(float target);
  void update(float dt);

  [[nodiscard]] float value() const;
  [[nodiscard]] float target() const;

  float response = 9.0f;

private:
  float value_ = 0.0f;
  float target_ = 0.0f;
};

} // namespace aster
