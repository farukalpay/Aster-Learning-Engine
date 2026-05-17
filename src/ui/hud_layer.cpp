// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/hud_layer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr aster::UiColor kPanel{0.026f, 0.031f, 0.036f, 0.82f};
constexpr aster::UiColor kEdge{0.80f, 0.58f, 0.30f, 0.36f};
constexpr aster::UiColor kText{0.95f, 0.95f, 0.88f, 1.0f};
constexpr aster::UiColor kDim{0.58f, 0.66f, 0.64f, 1.0f};
constexpr aster::UiColor kAmber{0.92f, 0.59f, 0.23f, 1.0f};
constexpr aster::UiColor kMenuPanel{0.020f, 0.026f, 0.030f, 0.94f};
constexpr aster::UiColor kMenuPanelStrong{0.026f, 0.034f, 0.038f, 0.97f};

aster::UiColor withAlpha(const aster::UiColor color, const float alpha) {
  return {color.r, color.g, color.b, color.a * std::clamp(alpha, 0.0f, 1.0f)};
}

aster::UiColor slotTint(const aster::Vec3 tint, const float alpha) {
  return {tint.x, tint.y, tint.z, alpha};
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

void drawPanelTexture(aster::UiCanvas &canvas, const aster::UiRect rect) {
  canvas.fillRoundRect(rect, 8.0f, kPanel);
  canvas.strokeRect(rect, kEdge, 1.0f);
  for (int i = 0; i < 4; ++i) {
    const float y = rect.y + 14.0f + static_cast<float>(i) * 34.0f;
    canvas.line({rect.x + 12.0f, y}, {rect.x + rect.width - 12.0f, y + 10.0f},
                {0.95f, 0.71f, 0.38f, 0.04f}, 1.0f);
  }
}

void drawPointerCue(aster::UiCanvas &canvas, const aster::PointerCueModel &pointer) {
  if (!pointer.active) {
    return;
  }

  const float scale = pointer.pressed ? 0.92f : 1.0f;
  const aster::Vec2 tip = pointer.position;
  const auto point = [&](const float x, const float y) {
    return aster::Vec2{tip.x + x * scale, tip.y + y * scale};
  };

  const aster::UiColor shadow{0.03f, 0.04f, 0.04f, 0.75f};
  const aster::UiColor outline{0.24f, 0.16f, 0.07f, 0.96f};
  const aster::UiColor fill{0.96f, 0.68f, 0.32f, 1.0f};
  const aster::UiColor target = pointer.pressed ? aster::UiColor{0.38f, 0.92f, 0.82f, 0.88f}
                                                : aster::UiColor{1.0f, 0.78f, 0.38f, 0.80f};

  canvas.strokeCircle(tip, 14.0f * scale, shadow, 3.6f * scale, 28);
  canvas.strokeCircle(tip, 13.0f * scale, target, 1.8f * scale, 28);
  if (pointer.pressed) {
    const aster::Vec2 palm = point(-14.0f, 28.0f);
    const aster::Vec2 wrist = point(-21.0f, 47.0f);
    canvas.line(wrist, palm, shadow, 15.0f * scale);
    canvas.line(wrist, palm, outline, 11.0f * scale);
    canvas.line(wrist, palm, fill, 8.0f * scale);
    for (int i = 0; i < 5; ++i) {
      const float spread = (static_cast<float>(i) - 2.0f) * 7.2f;
      const float length = i == 0 || i == 4 ? 17.0f : 22.0f;
      const aster::Vec2 root = point(-21.0f + spread * 0.42f, 23.0f + std::abs(spread) * 0.10f);
      const aster::Vec2 end = point(-21.0f + spread, 23.0f - length);
      canvas.line(root, end, shadow, 8.0f * scale);
      canvas.line(root, end, outline, 6.2f * scale);
      canvas.line(root, end, fill, 4.1f * scale);
      canvas.fillCircle(end, 2.7f * scale, fill, 12);
    }
    canvas.fillCircle(palm, 11.5f * scale, fill, 24);
    canvas.strokeCircle(palm, 12.0f * scale, outline, 1.4f * scale, 24);
    return;
  }
  canvas.line(point(-21.0f, 32.0f), point(-5.0f, 4.0f), shadow, 13.0f * scale);
  canvas.line(point(-21.0f, 32.0f), point(-5.0f, 4.0f), outline, 10.0f * scale);
  canvas.line(point(-21.0f, 32.0f), point(-5.0f, 4.0f), fill, 7.0f * scale);
  canvas.fillCircle(point(-4.0f, 3.0f), 4.8f * scale, fill, 18);
  canvas.strokeCircle(point(-4.0f, 3.0f), 5.2f * scale, outline, 1.2f * scale, 18);
  canvas.line(point(-27.0f, 35.0f), point(-37.0f, 24.0f), outline, 8.5f * scale);
  canvas.line(point(-27.0f, 35.0f), point(-37.0f, 24.0f), fill, 5.5f * scale);
  canvas.fillCircle(point(-22.0f, 36.0f), 10.5f * scale, fill, 22);
  canvas.strokeCircle(point(-22.0f, 36.0f), 11.0f * scale, outline, 1.5f * scale, 22);
}

void drawGameCursor(aster::UiCanvas &canvas, const aster::GameCursorModel &cursor) {
  if (!cursor.visible) {
    return;
  }

  const float scale = cursor.pressed ? 0.92f : 1.0f;
  const aster::Vec2 tip = cursor.position;
  const auto point = [&](const float x, const float y) {
    return aster::Vec2{tip.x + x * scale, tip.y + y * scale};
  };

  const aster::UiColor shadow{0.0f, 0.0f, 0.0f, 0.46f};
  const aster::UiColor edge{0.14f, 0.10f, 0.06f, 0.98f};
  const aster::UiColor fill = cursor.pressed ? aster::UiColor{0.98f, 0.62f, 0.22f, 1.0f}
                                             : aster::UiColor{1.0f, 0.82f, 0.42f, 1.0f};
  const aster::UiColor glow{1.0f, 0.64f, 0.22f, cursor.pressed ? 0.42f : 0.30f};

  canvas.strokeCircle(point(1.0f, 1.0f), 14.0f * scale, shadow, 3.0f * scale, 32);
  canvas.strokeCircle(tip, 12.0f * scale, glow, 2.0f * scale, 32);
  canvas.line(point(0.0f, -18.0f), point(0.0f, -6.0f), edge, 3.4f * scale);
  canvas.line(point(0.0f, 6.0f), point(0.0f, 18.0f), edge, 3.4f * scale);
  canvas.line(point(-18.0f, 0.0f), point(-6.0f, 0.0f), edge, 3.4f * scale);
  canvas.line(point(6.0f, 0.0f), point(18.0f, 0.0f), edge, 3.4f * scale);
  canvas.line(point(0.0f, -17.0f), point(0.0f, -6.5f), fill, 1.5f * scale);
  canvas.line(point(0.0f, 6.5f), point(0.0f, 17.0f), fill, 1.5f * scale);
  canvas.line(point(-17.0f, 0.0f), point(-6.5f, 0.0f), fill, 1.5f * scale);
  canvas.line(point(6.5f, 0.0f), point(17.0f, 0.0f), fill, 1.5f * scale);
  canvas.fillCircle(tip, 3.2f * scale, fill, 16);
  canvas.strokeCircle(tip, 4.0f * scale, edge, 1.1f * scale, 16);
}

void drawFocusPrompt(aster::UiCanvas &canvas, const aster::FocusPromptModel &prompt) {
  const float alpha = std::clamp(prompt.alpha, 0.0f, 1.0f);
  if (!prompt.visible || alpha <= 0.01f) {
    return;
  }

  const aster::Vec2 viewport = canvas.viewportSize();
  const std::string key = prompt.key.empty() ? "E" : prompt.key;
  const std::string text = prompt.action + (prompt.subject.empty() ? "" : " " + prompt.subject);
  const float key_width = std::max(30.0f, canvas.textWidth(key, 1.35f) + 18.0f);
  const float text_width = canvas.textWidth(text, 1.45f);
  const float width = std::clamp(key_width + text_width + 38.0f, 128.0f, viewport.x - 32.0f);
  const float height = 38.0f;
  const float scale = 0.96f + alpha * 0.04f;
  const aster::UiRect rect{viewport.x * 0.5f - width * 0.5f * scale,
                           viewport.y * 0.58f - height * 0.5f * scale, width * scale,
                           height * scale};

  canvas.fillRoundRect({rect.x + 2.0f, rect.y + 4.0f, rect.width, rect.height}, 6.0f,
                       {0.0f, 0.0f, 0.0f, 0.20f * alpha});
  canvas.fillRoundRect(rect, 6.0f, {0.025f, 0.030f, 0.033f, 0.76f * alpha});
  canvas.strokeRect(rect, {0.95f, 0.62f, 0.27f, 0.48f * alpha}, 1.0f);

  const aster::UiRect key_rect{rect.x + 9.0f, rect.y + 8.0f, key_width, rect.height - 16.0f};
  canvas.fillRoundRect(key_rect, 4.0f, {0.92f, 0.59f, 0.23f, 0.22f * alpha});
  canvas.strokeRect(key_rect, {0.98f, 0.76f, 0.38f, 0.72f * alpha}, 1.0f);
  canvas.text(
      key, {key_rect.x + (key_rect.width - canvas.textWidth(key, 1.35f)) * 0.5f, key_rect.y + 6.0f},
      withAlpha(kText, alpha), 1.35f);

  const std::string fitted = fitTextToWidth(canvas, text, rect.width - key_width - 30.0f, 1.45f);
  canvas.text(fitted, {key_rect.x + key_rect.width + 14.0f, rect.y + 13.0f},
              withAlpha(kText, alpha), 1.45f);
}

void drawHotbar(aster::UiCanvas &canvas, const aster::HotbarHudModel &hotbar) {
  if (!hotbar.visible || hotbar.slots.empty()) {
    return;
  }

  const aster::Vec2 viewport = canvas.viewportSize();
  const float slot_size = std::clamp(viewport.x / 16.0f, 46.0f, 58.0f);
  const float gap = 7.0f;
  const float width = slot_size * static_cast<float>(hotbar.slots.size()) +
                      gap * static_cast<float>(hotbar.slots.size() - 1u);
  const float x0 = viewport.x * 0.5f - width * 0.5f;
  const float y0 = viewport.y - slot_size - 18.0f;

  for (std::size_t i = 0; i < hotbar.slots.size(); ++i) {
    const aster::HotbarSlotModel &slot = hotbar.slots[i];
    const aster::UiRect rect{x0 + static_cast<float>(i) * (slot_size + gap), y0, slot_size,
                             slot_size};
    const aster::UiColor fill =
        slot.filled ? slotTint(slot.tint, 0.72f) : aster::UiColor{0.030f, 0.036f, 0.040f, 0.72f};
    canvas.fillRoundRect({rect.x + 2.0f, rect.y + 4.0f, rect.width, rect.height}, 5.0f,
                         {0.0f, 0.0f, 0.0f, 0.26f});
    canvas.fillRoundRect(rect, 5.0f, fill);
    canvas.strokeRect(rect,
                      slot.selected ? aster::UiColor{1.0f, 0.72f, 0.30f, 0.96f}
                                    : aster::UiColor{0.50f, 0.58f, 0.56f, 0.36f},
                      slot.selected ? 2.0f : 1.0f);
    canvas.text(slot.key, {rect.x + 6.0f, rect.y + 6.0f}, {0.95f, 0.92f, 0.82f, 0.92f}, 1.15f);
    if (slot.filled) {
      const float icon_radius = slot_size * 0.18f;
      canvas.fillCircle({rect.x + slot_size * 0.5f, rect.y + slot_size * 0.43f}, icon_radius,
                        slotTint(slot.tint, 0.95f), 18);
      canvas.strokeCircle({rect.x + slot_size * 0.5f, rect.y + slot_size * 0.43f},
                          icon_radius + 2.0f, {1.0f, 0.86f, 0.54f, 0.28f}, 1.0f, 18);
      const std::string label = fitTextToWidth(canvas, slot.label, slot_size - 8.0f, 1.05f);
      const float label_width = canvas.textWidth(label, 1.05f);
      canvas.text(label, {rect.x + (slot_size - label_width) * 0.5f, rect.y + slot_size - 16.0f},
                  {0.95f, 0.95f, 0.88f, 0.98f}, 1.05f);
    }
    if (!slot.quantity.empty()) {
      const float quantity_width = canvas.textWidth(slot.quantity, 1.05f);
      canvas.text(slot.quantity,
                  {rect.x + rect.width - quantity_width - 5.0f, rect.y + rect.height - 16.0f},
                  {0.98f, 0.78f, 0.36f, 1.0f}, 1.05f);
    }
  }
}

void drawHeartIcon(aster::UiCanvas &canvas, const aster::Vec2 center, const float size,
                   const float fill_fraction) {
  const float fill = std::clamp(fill_fraction, 0.0f, 1.0f);
  const aster::UiColor shadow{0.0f, 0.0f, 0.0f, 0.30f};
  const aster::UiColor edge{0.18f, 0.028f, 0.030f, 0.96f};
  const aster::UiColor empty{0.12f, 0.035f, 0.040f, 0.82f};
  const aster::UiColor red{0.86f, 0.060f, 0.075f, 1.0f};
  const aster::UiColor hot{1.0f, 0.30f, 0.22f, 0.95f};

  const auto drawShape = [&](const aster::UiColor color, const aster::Vec2 offset = {}) {
    const aster::Vec2 c{center.x + offset.x, center.y + offset.y};
    canvas.fillCircle({c.x - size * 0.22f, c.y - size * 0.16f}, size * 0.24f, color, 18);
    canvas.fillCircle({c.x + size * 0.22f, c.y - size * 0.16f}, size * 0.24f, color, 18);
    canvas.fillRoundRect({c.x - size * 0.31f, c.y - size * 0.10f, size * 0.62f, size * 0.33f},
                         size * 0.10f, color);
    canvas.line({c.x - size * 0.42f, c.y + size * 0.04f}, {c.x, c.y + size * 0.43f}, color,
                size * 0.28f);
    canvas.line({c.x + size * 0.42f, c.y + size * 0.04f}, {c.x, c.y + size * 0.43f}, color,
                size * 0.28f);
  };

  drawShape(shadow, {1.5f, 2.0f});
  drawShape(edge);
  drawShape(empty);
  if (fill <= 0.001f) {
    return;
  }
  canvas.pushClip({center.x - size * 0.52f, center.y - size * 0.52f, size * fill, size * 1.08f});
  drawShape(red);
  drawShape(hot, {-size * 0.035f, -size * 0.030f});
  canvas.popClip();
}

void drawHeartBar(aster::UiCanvas &canvas, const int health, const int max_health) {
  if (max_health <= 0) {
    return;
  }
  const aster::Vec2 viewport = canvas.viewportSize();
  const int heart_count = std::max(1, (max_health + 1) / 2);
  const float size = std::clamp(viewport.x / 42.0f, 17.0f, 24.0f);
  const float gap = size * 0.18f;
  const float width = static_cast<float>(heart_count) * size +
                      static_cast<float>(heart_count - 1) * gap;
  const float x0 = std::max(18.0f, viewport.x * 0.5f - width * 0.5f);
  const float y = viewport.y - std::clamp(viewport.x / 16.0f, 46.0f, 58.0f) - size - 26.0f;
  for (int i = 0; i < heart_count; ++i) {
    const int remaining = std::clamp(health - i * 2, 0, 2);
    drawHeartIcon(canvas, {x0 + static_cast<float>(i) * (size + gap) + size * 0.5f,
                           y + size * 0.5f},
                  size, static_cast<float>(remaining) * 0.5f);
  }
}

bool drawChestContents(aster::UiCanvas &canvas, const aster::ChestContentsHudModel &contents) {
  if (!contents.visible || contents.slots.empty()) {
    return false;
  }

  const aster::Vec2 viewport = canvas.viewportSize();
  const float slot_size = 56.0f;
  const float gap = 9.0f;
  const float padding = 16.0f;
  const float title_height = 28.0f;
  const float width = padding * 2.0f + slot_size * static_cast<float>(contents.slots.size()) +
                      gap * static_cast<float>(contents.slots.size() - 1u);
  const float height = padding * 2.0f + title_height + slot_size;
  const aster::UiRect panel{viewport.x * 0.5f - width * 0.5f,
                            std::max(86.0f, viewport.y * 0.5f - height * 0.5f), width, height};

  canvas.fillRoundRect({panel.x + 3.0f, panel.y + 5.0f, panel.width, panel.height}, 7.0f,
                       {0.0f, 0.0f, 0.0f, 0.28f});
  canvas.fillRoundRect(panel, 7.0f, {0.025f, 0.030f, 0.033f, 0.90f});
  canvas.strokeRect(panel, {0.88f, 0.62f, 0.30f, 0.54f}, 1.0f);
  canvas.text(contents.title, {panel.x + padding, panel.y + 13.0f}, kText, 1.55f);
  const aster::UiRect close_rect{panel.x + panel.width - 38.0f, panel.y + 10.0f, 26.0f, 26.0f};
  const bool close_clicked = canvas.button(close_rect, "X", "chest.close");

  const float y = panel.y + padding + title_height;
  for (std::size_t i = 0; i < contents.slots.size(); ++i) {
    const aster::ChestContentsSlotModel &slot = contents.slots[i];
    const aster::UiRect rect{panel.x + padding + static_cast<float>(i) * (slot_size + gap), y,
                             slot_size, slot_size};
    const aster::UiColor fill =
        slot.filled ? slotTint(slot.tint, 0.78f) : aster::UiColor{0.045f, 0.050f, 0.054f, 0.90f};
    canvas.fillRoundRect(rect, 5.0f, fill);
    canvas.strokeRect(rect,
                      slot.selected ? aster::UiColor{1.0f, 0.74f, 0.32f, 0.98f}
                                    : aster::UiColor{0.47f, 0.54f, 0.54f, 0.38f},
                      slot.selected ? 2.0f : 1.0f);
    if (!slot.filled) {
      continue;
    }
    const float icon_radius = slot_size * 0.20f;
    canvas.fillCircle({rect.x + slot_size * 0.5f, rect.y + slot_size * 0.38f}, icon_radius,
                      slotTint(slot.tint, 1.0f), 20);
    canvas.strokeCircle({rect.x + slot_size * 0.5f, rect.y + slot_size * 0.38f}, icon_radius + 2.0f,
                        {1.0f, 0.86f, 0.54f, 0.30f}, 1.0f, 20);
    const std::string label = fitTextToWidth(canvas, slot.label, slot_size - 8.0f, 1.05f);
    const float label_width = canvas.textWidth(label, 1.05f);
    canvas.text(label, {rect.x + (slot_size - label_width) * 0.5f, rect.y + slot_size - 16.0f},
                {0.96f, 0.95f, 0.88f, 0.98f}, 1.05f);
    if (!slot.quantity.empty()) {
      const float quantity_width = canvas.textWidth(slot.quantity, 1.05f);
      canvas.text(slot.quantity,
                  {rect.x + rect.width - quantity_width - 5.0f, rect.y + rect.height - 16.0f},
                  {0.98f, 0.78f, 0.36f, 1.0f}, 1.05f);
    }
  }
  return close_clicked;
}

void drawMenuButton(aster::UiCanvas &canvas, const aster::UiRect rect, const bool selected) {
  canvas.fillRoundRect({rect.x + 3.0f, rect.y + 4.0f, rect.width, rect.height}, 6.0f,
                       {0.0f, 0.0f, 0.0f, 0.28f});
  canvas.fillRoundRect(rect, 6.0f,
                       selected ? aster::UiColor{0.36f, 0.24f, 0.12f, 0.96f}
                                : aster::UiColor{0.08f, 0.14f, 0.15f, 0.94f});
  canvas.strokeRect(rect,
                    selected ? aster::UiColor{0.95f, 0.68f, 0.30f, 0.78f}
                             : aster::UiColor{0.48f, 0.62f, 0.56f, 0.34f},
                    1.0f);
}

void drawOptionsArrow(aster::UiCanvas &canvas, const aster::UiRect from, const aster::UiRect to) {
  const float y = from.y + from.height * 0.5f;
  const float start_x = from.x + from.width + 8.0f;
  const float end_x = to.x - 12.0f;
  canvas.line({start_x, y}, {end_x, y}, {0.95f, 0.68f, 0.30f, 0.62f}, 2.0f);
  canvas.line({end_x, y}, {end_x - 9.0f, y - 7.0f}, {0.95f, 0.68f, 0.30f, 0.62f}, 2.0f);
  canvas.line({end_x, y}, {end_x - 9.0f, y + 7.0f}, {0.95f, 0.68f, 0.30f, 0.62f}, 2.0f);
}

void drawOptionsPanel(aster::UiCanvas &canvas, const aster::HudModel &model,
                      const aster::UiRect panel) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + 18.0f;
  const float x = panel.x + 20.0f;
  canvas.text("Options", {x, y}, kText, 2.0f);
  y += 34.0f;
  canvas.text("Controls", {x, y}, kDim, 1.45f);
  y += 25.0f;

  const float key_width = 122.0f;
  for (const aster::ControlLegendEntry &entry : model.controls.entries) {
    if (y + 28.0f > panel.y + panel.height - 20.0f) {
      break;
    }
    canvas.fillRoundRect({x, y, key_width, 26.0f}, 5.0f,
                         entry.primary ? aster::UiColor{0.34f, 0.23f, 0.11f, 0.96f}
                                       : aster::UiColor{0.06f, 0.11f, 0.12f, 0.96f});
    canvas.strokeRect({x, y, key_width, 26.0f},
                      entry.primary ? aster::UiColor{0.92f, 0.65f, 0.30f, 0.58f}
                                    : aster::UiColor{0.42f, 0.58f, 0.54f, 0.36f},
                      1.0f);
    const float key_text_width = canvas.textWidth(entry.input, 1.45f);
    canvas.text(entry.input, {x + (key_width - key_text_width) * 0.5f, y + 7.0f}, kText, 1.45f);
    canvas.text(entry.action, {x + key_width + 18.0f, y + 6.0f}, kText, 1.45f);
    y += 34.0f;
  }
}

aster::HudAction drawPauseMenu(aster::UiCanvas &canvas, const aster::HudModel &model) {
  if (!model.pause.open) {
    return aster::HudAction::None;
  }

  const aster::Vec2 viewport = canvas.viewportSize();
  canvas.fillRect({0.0f, 0.0f, viewport.x, viewport.y}, {0.0f, 0.0f, 0.0f, 0.36f});

  const float menu_width = 300.0f;
  const float menu_height = 278.0f;
  const aster::UiRect menu{std::max(24.0f, viewport.x * 0.5f - 330.0f),
                           std::max(24.0f, viewport.y * 0.5f - menu_height * 0.5f), menu_width,
                           menu_height};
  canvas.fillRoundRect(menu, 8.0f, kMenuPanel);
  canvas.strokeRect(menu, {0.86f, 0.62f, 0.31f, 0.48f}, 1.0f);
  canvas.text("Menu", {menu.x + 22.0f, menu.y + 20.0f}, kText, 2.25f);
  canvas.text("Lumen Run", {menu.x + 22.0f, menu.y + 56.0f}, kDim, 1.35f);

  aster::HudAction action = aster::HudAction::None;
  const aster::UiRect resume{menu.x + 22.0f, menu.y + 92.0f, menu.width - 44.0f, 44.0f};
  const aster::UiRect options{menu.x + 22.0f, menu.y + 146.0f, menu.width - 44.0f, 44.0f};
  const aster::UiRect quit{menu.x + 22.0f, menu.y + 200.0f, menu.width - 44.0f, 44.0f};

  drawMenuButton(canvas, resume, false);
  if (canvas.button(resume, "Resume", "pause.resume")) {
    action = aster::HudAction::Resume;
  }
  drawMenuButton(canvas, options, model.pause.options_open);
  if (canvas.button(options, "Options", "pause.options")) {
    action = aster::HudAction::ToggleOptions;
  }
  drawMenuButton(canvas, quit, false);
  if (canvas.button(quit, "Quit Game", "pause.quit")) {
    action = aster::HudAction::Quit;
  }

  if (model.pause.options_open) {
    const float options_width = std::min(380.0f, viewport.x - menu.x - menu.width - 48.0f);
    if (options_width > 220.0f) {
      const aster::UiRect panel{menu.x + menu.width + 34.0f, menu.y, options_width,
                                std::min(560.0f, viewport.y - menu.y - 24.0f)};
      drawOptionsArrow(canvas, options, panel);
      canvas.fillRoundRect(panel, 8.0f, kMenuPanelStrong);
      canvas.strokeRect(panel, {0.86f, 0.62f, 0.31f, 0.48f}, 1.0f);
      drawOptionsPanel(canvas, model, panel);
    }
  }

  return action;
}

} // namespace

namespace aster {

HudVisibilityPolicy hudVisibilityForState(const HudModalState state) {
  if (state.debug_capture) {
    return {};
  }
  const bool gameplay_visible = !state.inventory_open && !state.pause_open && !state.defeated;
  return {.status_panel = gameplay_visible,
          .health = gameplay_visible,
          .hotbar = gameplay_visible,
          .focus_prompt = gameplay_visible,
          .pointer = !state.pause_open && !state.defeated,
          .game_cursor = gameplay_visible};
}

HudLayer::~HudLayer() {
  shutdown();
}

void HudLayer::initialize() {
  canvas_.initialize();
  initialized_ = true;
}

void HudLayer::beginFrame(const Vec2 viewport_size, const ControlSnapshot &input) {
  canvas_.beginFrame(viewport_size, input);
}

HudAction HudLayer::draw(const HudModel &model) {
  const Vec2 viewport = canvas_.viewportSize();
  HudAction action = HudAction::None;

  if (model.visibility.status_panel && !model.inventory.open) {
    const UiRect panel{18.0f, 18.0f, 520.0f, 154.0f};
    drawPanelTexture(canvas_, panel);
    float y = panel.y + 16.0f;
    const float x = panel.x + 18.0f;
    const float width = panel.width - 36.0f;
    canvas_.text(model.title, {x, y}, kText, 2.25f);
    y += 32.0f;
    canvas_.wrappedText(model.subtitle, {x, y}, width, kDim, 1.45f);
    y += 40.0f;
    const float progress =
        model.total <= 0 ? 0.0f : static_cast<float>(model.score) / static_cast<float>(model.total);
    canvas_.progressBar({x, y, width, 12.0f}, std::clamp(progress, 0.0f, 1.0f), kAmber,
                        {0.06f, 0.08f, 0.08f, 0.92f});
    y += 27.0f;
    char buffer[128]{};
    std::snprintf(buffer, sizeof(buffer), "Shards %d / %d    Lives %d    Time %.1fs", model.score,
                  model.total, model.lives, model.elapsed_seconds);
    canvas_.text(buffer, {x, y}, kText, 1.55f);
  }

  const InventoryOverlayAction inventory_action = inventory_overlay_.draw(canvas_, model.inventory);
  if (inventory_action.type == InventoryOverlayAction::Type::TransferSupplyToPlayer &&
      inventory_action.item_id == "torch") {
    action = HudAction::TransferSupplyTorch;
  }
  if (drawChestContents(canvas_, model.chest_contents)) {
    action = HudAction::CloseChest;
  }
  HotbarHudModel hotbar = model.hotbar;
  hotbar.visible = hotbar.visible && model.visibility.hotbar;
  drawHotbar(canvas_, hotbar);
  if (model.visibility.health) {
    drawHeartBar(canvas_, model.health, model.max_health);
  }
  if (model.visibility.focus_prompt) {
    drawFocusPrompt(canvas_, model.focus_prompt);
  }
  if (model.visibility.pointer) {
    drawPointerCue(canvas_, model.pointer);
  }
  const HudAction pause_action = drawPauseMenu(canvas_, model);
  if (pause_action != HudAction::None) {
    action = pause_action;
  }
  if (model.visibility.game_cursor) {
    drawGameCursor(canvas_, model.game_cursor);
  }

  if (model.victory || model.defeated) {
    const UiRect result{viewport.x * 0.5f - 180.0f, viewport.y * 0.5f - 58.0f, 360.0f, 116.0f};
    drawPanelTexture(canvas_, result);
    canvas_.text(model.victory ? "Signal restored" : "Core breached",
                 {result.x + 28.0f, result.y + 24.0f}, kText, 2.1f);
    canvas_.text("Press R to restart the run.", {result.x + 28.0f, result.y + 64.0f}, kDim, 1.45f);
  }
  return action;
}

void HudLayer::endFrame() {
  canvas_.endFrame();
}

void HudLayer::shutdown() {
  if (!initialized_) {
    return;
  }
  canvas_.shutdown();
  initialized_ = false;
}

bool HudLayer::wantsKeyboard() const {
  return canvas_.wantsKeyboard();
}

bool HudLayer::wantsMouse() const {
  return canvas_.wantsMouse();
}

} // namespace aster
