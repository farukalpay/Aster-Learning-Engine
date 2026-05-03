// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/control_legend.hpp"

#include "aster/ui/ui_canvas.hpp"

#include <algorithm>

namespace {

constexpr float kButtonHeight = 38.0f;
constexpr float kPanelGap = 8.0f;
constexpr float kPanelPadding = 14.0f;
constexpr float kKeyHeight = 28.0f;
constexpr float kEntryGap = 8.0f;
constexpr float kEntryLeftInset = 18.0f;

constexpr aster::UiColor kPanel{0.028f, 0.034f, 0.038f, 0.90f};
constexpr aster::UiColor kPanelEdge{0.80f, 0.58f, 0.29f, 0.35f};
constexpr aster::UiColor kText{0.93f, 0.94f, 0.88f, 1.0f};
constexpr aster::UiColor kDim{0.58f, 0.66f, 0.64f, 1.0f};

float keyWidth(const std::string &label) {
  return std::clamp(30.0f + static_cast<float>(label.size()) * 12.0f, 42.0f, 160.0f);
}

float entryRowHeight(aster::UiCanvas &canvas, const aster::ControlLegendEntry &entry,
                     const float panel_width) {
  const float label_x = kPanelPadding + kEntryLeftInset + keyWidth(entry.input) + 14.0f;
  const float text_width = panel_width - label_x - kPanelPadding;
  return std::max(kKeyHeight, canvas.wrappedTextHeight(entry.action, text_width, 1.55f) + 6.0f);
}

float requiredPanelHeight(aster::UiCanvas &canvas, const aster::ControlLegendModel &model,
                          const float panel_width) {
  float height = kPanelPadding + canvas.textHeight(1.8f);
  if (!model.subtitle.empty()) {
    height += canvas.textHeight(1.4f) + 8.0f;
  }
  height += 10.0f;
  for (const aster::ControlLegendEntry &entry : model.entries) {
    height += entryRowHeight(canvas, entry, panel_width) + kEntryGap;
  }
  if (!model.footer.empty()) {
    height +=
        18.0f + canvas.wrappedTextHeight(model.footer, panel_width - kPanelPadding * 2.0f, 1.35f);
  }
  return height + kPanelPadding;
}

void drawPanelTexture(aster::UiCanvas &canvas, const aster::UiRect rect) {
  canvas.fillRoundRect(rect, 8.0f, kPanel);
  canvas.strokeRect(rect, kPanelEdge, 1.0f);
  for (int i = 0; i < 6; ++i) {
    const float y = rect.y + 12.0f + static_cast<float>(i) * 31.0f;
    canvas.line({rect.x + 10.0f, y}, {rect.x + rect.width - 10.0f, y + 11.0f},
                {0.96f, 0.72f, 0.38f, 0.045f}, 1.0f);
  }
}

void drawKeycap(aster::UiCanvas &canvas, const aster::ControlLegendEntry &entry, const float x,
                const float y) {
  const aster::UiRect rect{x, y, keyWidth(entry.input), kKeyHeight};
  const aster::UiColor base = entry.primary ? aster::UiColor{0.33f, 0.23f, 0.11f, 0.98f}
                                            : aster::UiColor{0.07f, 0.12f, 0.13f, 0.98f};
  const aster::UiColor border = entry.primary ? aster::UiColor{0.92f, 0.66f, 0.32f, 0.72f}
                                              : aster::UiColor{0.44f, 0.62f, 0.58f, 0.42f};
  canvas.fillRoundRect({rect.x + 1.0f, rect.y + 3.0f, rect.width, rect.height}, 6.0f,
                       {0.0f, 0.0f, 0.0f, 0.28f});
  canvas.fillRoundRect(rect, 6.0f, base);
  canvas.strokeRect(rect, border, 1.0f);
  const float text_width = canvas.textWidth(entry.input, 1.6f);
  canvas.text(entry.input, {rect.x + (rect.width - text_width) * 0.5f, rect.y + 7.0f}, kText, 1.6f);
}

} // namespace

namespace aster {

void ControlLegendWidget::draw(UiCanvas &canvas, const ControlLegendModel &model,
                               const ControlLegendPlacement &placement) {
  if (model.entries.empty()) {
    return;
  }
  if (!seeded_) {
    open_ = model.open_by_default;
    seeded_ = true;
  }

  const Vec2 viewport = canvas.viewportSize();
  const float button_width = std::min(placement.button_width, viewport.x - placement.right * 2.0f);
  const float panel_width = std::min(placement.panel_width, viewport.x - placement.right * 2.0f);
  const float button_x = viewport.x - placement.right - button_width;
  const float button_y = placement.top;

  const std::string button_label = model.button_label + (open_ ? " -" : " +");
  if (canvas.button({button_x, button_y, button_width, kButtonHeight}, button_label,
                    "control.legend.toggle")) {
    open_ = !open_;
  }

  if (!open_) {
    return;
  }

  const float required_height = requiredPanelHeight(canvas, model, panel_width);
  const float panel_x = viewport.x - placement.right - panel_width;
  const float panel_y = button_y + kButtonHeight + kPanelGap;
  const float panel_height =
      std::min(required_height, std::max(160.0f, viewport.y - panel_y - 18.0f));
  const UiRect panel{panel_x, panel_y, panel_width, panel_height};
  drawPanelTexture(canvas, panel);

  float y = panel.y + kPanelPadding;
  const float x = panel.x + kPanelPadding;
  canvas.text(model.title, {x, y}, kText, 1.8f);
  y += 24.0f;
  if (!model.subtitle.empty()) {
    canvas.text(model.subtitle, {x, y}, kDim, 1.35f);
    y += 22.0f;
  }

  for (std::size_t i = 0; i < model.entries.size(); ++i) {
    const ControlLegendEntry &entry = model.entries[i];
    const float row_height = entryRowHeight(canvas, entry, panel_width);
    if (y + row_height > panel.y + panel.height - kPanelPadding) {
      break;
    }
    drawKeycap(canvas, entry, x + kEntryLeftInset, y);
    const float label_x = x + kEntryLeftInset + keyWidth(entry.input) + 14.0f;
    const float text_width = panel.x + panel_width - label_x - kPanelPadding;
    canvas.wrappedText(entry.action, {label_x, y + 5.0f}, text_width, kText, 1.55f);
    y += row_height + kEntryGap;
  }

  if (!model.footer.empty() && y + 24.0f < panel.y + panel.height) {
    canvas.line({x, y + 2.0f}, {panel.x + panel.width - kPanelPadding, y + 2.0f},
                {0.80f, 0.62f, 0.36f, 0.18f}, 1.0f);
    canvas.wrappedText(model.footer, {x, y + 12.0f}, panel_width - kPanelPadding * 2.0f, kDim,
                       1.35f);
  }
}

bool ControlLegendWidget::isOpen() const {
  return open_;
}

void ControlLegendWidget::setOpen(const bool open) {
  open_ = open;
  seeded_ = true;
}

} // namespace aster
