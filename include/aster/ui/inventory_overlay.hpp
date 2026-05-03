// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <string>
#include <vector>

namespace aster {

class UiCanvas;

struct InventorySlotModel {
  std::string label;
  std::string detail;
  std::string quantity;
  Vec3 tint{0.32f, 0.34f, 0.36f};
  bool filled = false;
  bool selected = false;
};

struct InventoryGridModel {
  std::string title;
  int columns = 6;
  std::vector<InventorySlotModel> slots;
};

struct InventoryRecipeModel {
  std::string name;
  std::string requirement;
  bool available = false;
};

struct InventoryOverlayModel {
  bool open = false;
  bool world_character_preview = false;
  std::string title = "Inventory";
  std::string subtitle;
  std::string active_tab = "Inventory";
  std::string secondary_tab = "Crafting";
  std::string character_name = "Player";
  std::string character_status;
  std::string search_hint = "Search";
  std::vector<InventorySlotModel> equipment_slots;
  InventoryGridModel backpack;
  InventoryGridModel hotbar;
  std::vector<InventoryRecipeModel> recipes;
};

class InventoryOverlayWidget {
public:
  void draw(UiCanvas &canvas, const InventoryOverlayModel &model);
};

} // namespace aster
