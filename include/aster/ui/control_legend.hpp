// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <string>
#include <vector>

namespace aster {

class UiCanvas;

struct ControlLegendEntry {
  std::string input;
  std::string action;
  bool primary = false;
};

struct ControlLegendModel {
  std::string button_label = "Controls";
  std::string title = "Controls";
  std::string subtitle;
  std::string footer;
  std::vector<ControlLegendEntry> entries;
  bool open_by_default = false;
};

struct ControlLegendPlacement {
  float top = 18.0f;
  float right = 18.0f;
  float button_width = 132.0f;
  float panel_width = 348.0f;
};

class ControlLegendWidget {
public:
  void draw(UiCanvas &canvas, const ControlLegendModel &model,
            const ControlLegendPlacement &placement = {});

  [[nodiscard]] bool isOpen() const;
  void setOpen(bool open);

private:
  bool open_ = false;
  bool seeded_ = false;
};

} // namespace aster
