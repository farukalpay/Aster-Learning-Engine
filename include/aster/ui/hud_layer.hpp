// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/ui/control_legend.hpp"
#include "aster/ui/inventory_overlay.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <string>
#include <vector>

namespace aster {

struct PointerCueModel {
  bool active = false;
  bool pressed = false;
  Vec2 position{};
};

struct GameCursorModel {
  bool visible = false;
  bool pressed = false;
  Vec2 position{};
};

struct PauseMenuModel {
  bool open = false;
  bool options_open = false;
};

struct FocusPromptModel {
  bool visible = false;
  float alpha = 0.0f;
  std::string key;
  std::string action;
  std::string subject;
};

struct HotbarSlotModel {
  std::string key;
  std::string label;
  std::string quantity;
  Vec3 tint{0.30f, 0.32f, 0.34f};
  bool filled = false;
  bool selected = false;
};

struct HotbarHudModel {
  bool visible = true;
  std::vector<HotbarSlotModel> slots;
};

struct ChestContentsSlotModel {
  std::string label;
  std::string quantity;
  Vec3 tint{0.30f, 0.32f, 0.34f};
  bool filled = false;
  bool selected = false;
};

struct ChestContentsHudModel {
  bool visible = false;
  std::string title = "Chest";
  std::vector<ChestContentsSlotModel> slots;
};

struct HudVisibilityPolicy {
  bool status_panel = true;
  bool health = true;
  bool hotbar = true;
  bool focus_prompt = true;
  bool pointer = true;
  bool game_cursor = true;
};

struct HudModalState {
  bool inventory_open = false;
  bool pause_open = false;
  bool defeated = false;
  bool debug_capture = false;
};

[[nodiscard]] HudVisibilityPolicy hudVisibilityForState(HudModalState state);

struct HudModel {
  std::string title;
  std::string subtitle;
  int score = 0;
  int total = 0;
  int lives = 0;
  int health = 0;
  int max_health = 0;
  float elapsed_seconds = 0.0f;
  bool victory = false;
  bool defeated = false;
  ControlLegendModel controls;
  InventoryOverlayModel inventory;
  PointerCueModel pointer;
  GameCursorModel game_cursor;
  PauseMenuModel pause;
  FocusPromptModel focus_prompt;
  HotbarHudModel hotbar;
  ChestContentsHudModel chest_contents;
  HudVisibilityPolicy visibility;
};

enum class HudAction {
  None,
  CloseChest,
  TransferSupplyTorch,
  Resume,
  ToggleOptions,
  Quit,
};

class HudLayer {
public:
  HudLayer() = default;
  ~HudLayer();

  HudLayer(const HudLayer &) = delete;
  HudLayer &operator=(const HudLayer &) = delete;

  void initialize();
  void beginFrame(Vec2 viewport_size, const ControlSnapshot &input);
  [[nodiscard]] HudAction draw(const HudModel &model);
  void endFrame();
  void shutdown();

  [[nodiscard]] bool wantsKeyboard() const;
  [[nodiscard]] bool wantsMouse() const;

private:
  UiCanvas canvas_;
  InventoryOverlayWidget inventory_overlay_;
  bool initialized_ = false;
};

} // namespace aster
