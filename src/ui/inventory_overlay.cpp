// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/inventory_overlay.hpp"

#include "aster/ui/ui_canvas.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kOverlayMargin = 18.0f;
constexpr float kPanelPadding = 16.0f;
constexpr float kGap = 12.0f;
constexpr float kSlotGap = 7.0f;

constexpr aster::UiColor kPanel{0.027f, 0.031f, 0.035f, 0.94f};
constexpr aster::UiColor kPanelSoft{0.040f, 0.048f, 0.052f, 0.86f};
constexpr aster::UiColor kEdge{0.78f, 0.58f, 0.31f, 0.34f};
constexpr aster::UiColor kText{0.94f, 0.94f, 0.88f, 1.0f};
constexpr aster::UiColor kDim{0.58f, 0.65f, 0.62f, 1.0f};

aster::UiColor color(const aster::Vec3 value, const float alpha) {
  return {value.x, value.y, value.z, alpha};
}

void drawPanelTexture(aster::UiCanvas &canvas, const aster::UiRect rect) {
  canvas.fillRoundRect(rect, 8.0f, kPanel);
  canvas.strokeRect(rect, kEdge, 1.0f);
  for (int i = 0; i < 8; ++i) {
    const float y = rect.y + 14.0f + static_cast<float>(i) * 38.0f;
    canvas.line({rect.x + 12.0f, y}, {rect.x + rect.width - 14.0f, y + 11.0f},
                {0.92f, 0.68f, 0.38f, 0.035f}, 1.0f);
  }
}

std::string fitTextToWidth(aster::UiCanvas &canvas, const std::string &text, const float width,
                           const float scale) {
  if (canvas.textWidth(text, scale) <= width) {
    return text;
  }
  std::string fitted = text;
  while (fitted.size() > 1u && canvas.textWidth(fitted + ".", scale) > width) {
    fitted.pop_back();
  }
  return fitted + ".";
}

void drawSlot(aster::UiCanvas &canvas, const aster::InventorySlotModel &slot,
              const aster::UiRect rect) {
  const aster::UiColor base = slot.filled ? color(slot.tint, 0.88f) : kPanelSoft;
  const aster::UiColor border = slot.selected ? aster::UiColor{0.94f, 0.71f, 0.34f, 0.95f}
                                              : aster::UiColor{0.47f, 0.54f, 0.54f, 0.34f};
  canvas.fillRoundRect(rect, 4.0f, base);
  canvas.strokeRect(rect, border, slot.selected ? 2.0f : 1.0f);
  canvas.line({rect.x + 5.0f, rect.y + 6.0f}, {rect.x + rect.width - 6.0f, rect.y + 6.0f},
              {1.0f, 1.0f, 1.0f, slot.filled ? 0.10f : 0.035f}, 1.0f);

  if (!slot.label.empty()) {
    const float text_width = std::max(0.0f, rect.width - 10.0f);
    const std::string label = fitTextToWidth(canvas, slot.label, text_width, 1.35f);
    const float label_width = canvas.textWidth(label, 1.35f);
    canvas.text(label, {rect.x + (rect.width - label_width) * 0.5f, rect.y + 10.0f}, kText, 1.35f);
  }

  if (!slot.quantity.empty()) {
    const float quantity_width = canvas.textWidth(slot.quantity, 1.25f);
    canvas.text(slot.quantity,
                {rect.x + rect.width - quantity_width - 5.0f, rect.y + rect.height - 15.0f},
                {0.98f, 0.83f, 0.50f, 1.0f}, 1.25f);
  }
}

void drawSearchField(aster::UiCanvas &canvas, const aster::UiRect rect,
                     const std::string &placeholder) {
  if (rect.width <= 0.0f || rect.height <= 0.0f) {
    return;
  }
  canvas.fillRoundRect(rect, 5.0f, {0.050f, 0.058f, 0.062f, 0.90f});
  canvas.strokeRect(rect, {0.32f, 0.39f, 0.38f, 0.28f}, 1.0f);
  if (rect.width < 48.0f) {
    return;
  }
  const aster::Vec2 glass_center{rect.x + 15.0f, rect.y + rect.height * 0.5f - 1.0f};
  canvas.strokeCircle(glass_center, 4.8f, {0.58f, 0.65f, 0.62f, 0.86f}, 1.3f, 14);
  canvas.line({glass_center.x + 3.6f, glass_center.y + 3.8f},
              {glass_center.x + 8.0f, glass_center.y + 8.0f}, {0.58f, 0.65f, 0.62f, 0.86f}, 1.5f);
  const std::string fitted =
      fitTextToWidth(canvas, placeholder, std::max(0.0f, rect.width - 38.0f), 1.25f);
  canvas.text(fitted, {rect.x + 30.0f, rect.y + 8.0f}, kDim, 1.25f);
}

float drawGrid(aster::UiCanvas &canvas, const aster::InventoryGridModel &grid, const float x,
               float y, const float width) {
  const int columns = std::max(grid.columns, 1);
  const int rows = static_cast<int>(
      std::ceil(static_cast<float>(grid.slots.size()) / static_cast<float>(columns)));
  const float slot_size =
      std::clamp((width - static_cast<float>(columns - 1) * kSlotGap) / static_cast<float>(columns),
                 36.0f, 62.0f);
  const float actual_width =
      slot_size * static_cast<float>(columns) + static_cast<float>(columns - 1) * kSlotGap;
  canvas.text(grid.title, {x, y}, kText, 1.55f);
  y += 24.0f;
  for (std::size_t i = 0; i < grid.slots.size(); ++i) {
    const int column = static_cast<int>(i % static_cast<std::size_t>(columns));
    const int row = static_cast<int>(i / static_cast<std::size_t>(columns));
    const aster::UiRect slot{x + static_cast<float>(column) * (slot_size + kSlotGap),
                             y + static_cast<float>(row) * (slot_size + kSlotGap), slot_size,
                             slot_size};
    drawSlot(canvas, grid.slots[i], slot);
  }
  if (grid.slots.empty()) {
    canvas.fillRoundRect({x, y, actual_width, slot_size}, 4.0f, kPanelSoft);
    canvas.text("Empty", {x + 10.0f, y + 11.0f}, kDim, 1.4f);
  }
  const float grid_height =
      rows <= 0 ? slot_size
                : static_cast<float>(rows) * slot_size + static_cast<float>(rows - 1) * kSlotGap;
  return 24.0f + grid_height + 3.0f;
}

void drawPaperDoll(aster::UiCanvas &canvas, const aster::UiRect rect) {
  const float cx = rect.x + rect.width * 0.5f;
  const float cy = rect.y + rect.height * 0.5f;
  const float scale = std::min(rect.width, rect.height) / 190.0f;
  const aster::UiColor wool{0.68f, 0.49f, 0.30f, 1.0f};
  const aster::UiColor shadow{0.24f, 0.14f, 0.08f, 1.0f};
  const aster::UiColor core{0.94f, 0.66f, 0.30f, 1.0f};
  canvas.fillCircle({cx, cy + 8.0f * scale}, 50.0f * scale, shadow, 28);
  canvas.fillCircle({cx, cy - 40.0f * scale}, 42.0f * scale, wool, 28);
  canvas.fillCircle({cx - 28.0f * scale, cy - 52.0f * scale}, 16.0f * scale, shadow, 18);
  canvas.fillCircle({cx + 28.0f * scale, cy - 52.0f * scale}, 16.0f * scale, shadow, 18);
  canvas.fillCircle({cx, cy - 24.0f * scale}, 17.0f * scale, core, 20);
  canvas.fillCircle({cx - 14.0f * scale, cy - 48.0f * scale}, 4.8f * scale,
                    {0.03f, 0.025f, 0.020f, 1.0f}, 14);
  canvas.fillCircle({cx + 14.0f * scale, cy - 48.0f * scale}, 4.8f * scale,
                    {0.03f, 0.025f, 0.020f, 1.0f}, 14);
  canvas.fillCircle({cx - 44.0f * scale, cy + 14.0f * scale}, 15.0f * scale, wool, 18);
  canvas.fillCircle({cx + 44.0f * scale, cy + 14.0f * scale}, 15.0f * scale, wool, 18);
  canvas.fillCircle({cx - 23.0f * scale, cy + 64.0f * scale}, 18.0f * scale, wool, 18);
  canvas.fillCircle({cx + 23.0f * scale, cy + 64.0f * scale}, 18.0f * scale, wool, 18);
}

void drawRecipeList(aster::UiCanvas &canvas,
                    const std::vector<aster::InventoryRecipeModel> &recipes, const float x, float y,
                    const float width) {
  for (const aster::InventoryRecipeModel &recipe : recipes) {
    const aster::UiColor fill = recipe.available ? aster::UiColor{0.12f, 0.20f, 0.17f, 0.92f}
                                                 : aster::UiColor{0.060f, 0.068f, 0.072f, 0.92f};
    const aster::UiColor edge = recipe.available ? aster::UiColor{0.53f, 0.74f, 0.52f, 0.42f}
                                                 : aster::UiColor{0.45f, 0.50f, 0.50f, 0.26f};
    canvas.fillRoundRect({x, y, width, 42.0f}, 4.0f, fill);
    canvas.strokeRect({x, y, width, 42.0f}, edge, 1.0f);
    canvas.text(recipe.name, {x + 10.0f, y + 7.0f}, kText, 1.35f);
    canvas.text(recipe.requirement, {x + 10.0f, y + 24.0f}, kDim, 1.25f);
    y += 48.0f;
  }
}

void drawWorldPreviewLayout(aster::UiCanvas &canvas, const aster::InventoryOverlayModel &model) {
  const aster::Vec2 viewport = canvas.viewportSize();
  const float margin = 18.0f;
  const float top_height = 72.0f;
  const float side_top = top_height + margin * 1.5f;
  const float side_height = std::max(260.0f, viewport.y - top_height - margin * 2.5f);
  const float left_width = std::clamp(viewport.x * 0.30f, 330.0f, 450.0f);
  const float right_width = std::clamp(viewport.x * 0.24f, 280.0f, 360.0f);

  drawPanelTexture(canvas, {margin, margin, viewport.x - margin * 2.0f, top_height});
  canvas.text(model.title, {margin + 16.0f, margin + 17.0f}, kText, 2.1f);
  canvas.text(model.subtitle, {margin + 160.0f, margin + 22.0f}, kDim, 1.45f);
  canvas.button({margin + 220.0f, margin + 18.0f, 118.0f, 32.0f}, model.active_tab,
                "inventory.world.tab.a");
  canvas.button({margin + 346.0f, margin + 18.0f, 118.0f, 32.0f}, model.secondary_tab,
                "inventory.world.tab.b");
  const float search_x = margin + 484.0f;
  const float search_available = std::max(0.0f, viewport.x - search_x - margin - 16.0f);
  const float search_width = std::min(search_available, 520.0f);
  drawSearchField(canvas, {search_x, margin + 19.0f, search_width, 30.0f}, model.search_hint);

  const aster::UiRect left{margin, side_top, left_width, side_height};
  drawPanelTexture(canvas, left);
  float y = left.y + kPanelPadding;
  const float x = left.x + kPanelPadding;
  canvas.text(model.character_name, {x, y}, kText, 1.8f);
  y += 24.0f;
  if (!model.character_status.empty()) {
    canvas.text(model.character_status, {x, y}, kDim, 1.35f);
    y += 24.0f;
  }
  canvas.text("Equipment", {x, y}, kText, 1.5f);
  y += 24.0f;
  for (const aster::InventorySlotModel &slot : model.equipment_slots) {
    drawSlot(canvas, slot, {x, y, left.width - kPanelPadding * 2.0f, 36.0f});
    y += 42.0f;
  }
  y += 8.0f;
  drawGrid(canvas, model.backpack, x, y, left.width - kPanelPadding * 2.0f);

  const aster::UiRect right{viewport.x - margin - right_width, side_top, right_width, side_height};
  drawPanelTexture(canvas, right);
  y = right.y + kPanelPadding;
  const float rx = right.x + kPanelPadding;
  y += drawGrid(canvas, model.hotbar, rx, y, right.width - kPanelPadding * 2.0f);
  y += 14.0f;
  canvas.line({rx, y}, {right.x + right.width - kPanelPadding, y}, {0.80f, 0.62f, 0.36f, 0.18f},
              1.0f);
  y += 16.0f;
  canvas.text("Crafting", {rx, y}, kText, 1.6f);
  y += 21.0f;
  canvas.text("Known recipes", {rx, y}, kDim, 1.25f);
  y += 22.0f;
  drawRecipeList(canvas, model.recipes, rx, y, right.width - kPanelPadding * 2.0f);
}

} // namespace

namespace aster {

void InventoryOverlayWidget::draw(UiCanvas &canvas, const InventoryOverlayModel &model) {
  if (!model.open) {
    return;
  }
  if (model.world_character_preview) {
    drawWorldPreviewLayout(canvas, model);
    return;
  }

  const Vec2 viewport = canvas.viewportSize();
  const float width = std::min(1060.0f, std::max(320.0f, viewport.x - kOverlayMargin * 2.0f));
  const float height = std::min(640.0f, std::max(320.0f, viewport.y - kOverlayMargin * 2.0f));
  const UiRect panel{(viewport.x - width) * 0.5f, (viewport.y - height) * 0.5f, width, height};
  drawPanelTexture(canvas, panel);

  float y = panel.y + kPanelPadding;
  const float x = panel.x + kPanelPadding;
  canvas.text(model.title, {x, y}, kText, 2.1f);
  if (!model.subtitle.empty()) {
    canvas.text(model.subtitle, {x + 150.0f, y + 4.0f}, kDim, 1.4f);
  }
  canvas.button({x, y + 38.0f, 112.0f, 30.0f}, model.active_tab, "inventory.tab.a");
  canvas.button({x + 124.0f, y + 38.0f, 112.0f, 30.0f}, model.secondary_tab, "inventory.tab.b");
  const float compact_search_x = x + 248.0f;
  const float compact_search_available =
      std::max(0.0f, panel.x + panel.width - kPanelPadding - compact_search_x);
  const float compact_search_width = std::min(compact_search_available, 430.0f);
  drawSearchField(canvas, {compact_search_x, y + 39.0f, compact_search_width, 28.0f},
                  model.search_hint);
  y += 84.0f;

  const float content_width = panel.width - kPanelPadding * 2.0f;
  const float character_width = std::clamp(content_width * 0.28f, 220.0f, 300.0f);
  const float craft_width = std::clamp(content_width * 0.25f, 220.0f, 280.0f);
  const float grid_width =
      std::max(240.0f, content_width - character_width - craft_width - kGap * 2.0f);
  const float content_height = panel.y + panel.height - y - kPanelPadding;

  const UiRect character_panel{x, y, character_width, content_height};
  canvas.fillRoundRect(character_panel, 6.0f, {0.032f, 0.038f, 0.042f, 0.62f});
  float cy = y + 14.0f;
  canvas.text(model.character_name, {x + 12.0f, cy}, kText, 1.55f);
  cy += 22.0f;
  if (!model.character_status.empty()) {
    canvas.text(model.character_status, {x + 12.0f, cy}, kDim, 1.25f);
    cy += 20.0f;
  }
  const UiRect doll{x + 12.0f, cy, character_width - 24.0f,
                    std::max(150.0f, content_height * 0.42f)};
  canvas.fillRoundRect(doll, 5.0f, {0.048f, 0.056f, 0.060f, 0.82f});
  drawPaperDoll(canvas, doll);
  cy += doll.height + 12.0f;
  canvas.text("Equipment", {x + 12.0f, cy}, kText, 1.45f);
  cy += 24.0f;
  for (const InventorySlotModel &slot : model.equipment_slots) {
    drawSlot(canvas, slot, {x + 12.0f, cy, character_width - 24.0f, 34.0f});
    cy += 40.0f;
  }

  const float grid_x = x + character_width + kGap;
  float grid_y = y + 14.0f;
  grid_y += drawGrid(canvas, model.backpack, grid_x, grid_y, grid_width);
  grid_y += 12.0f;
  drawGrid(canvas, model.hotbar, grid_x, grid_y, grid_width);

  const float craft_x = grid_x + grid_width + kGap;
  float craft_y = y + 14.0f;
  canvas.text("Crafting", {craft_x, craft_y}, kText, 1.55f);
  craft_y += 21.0f;
  canvas.text("Known recipes", {craft_x, craft_y}, kDim, 1.25f);
  craft_y += 22.0f;
  drawRecipeList(canvas, model.recipes, craft_x, craft_y, craft_width);
}

} // namespace aster
