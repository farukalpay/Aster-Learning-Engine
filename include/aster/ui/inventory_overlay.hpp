// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <string>
#include <vector>

namespace aster {

class UiCanvas;

enum class InventorySlotRole {
  None,
  PlayerBackpack,
  PlayerHotbar,
  Supply,
};

struct InventorySlotModel {
  std::string item_id;
  std::string label;
  std::string detail;
  std::string quantity;
  Vec3 tint{0.32f, 0.34f, 0.36f};
  bool filled = false;
  bool selected = false;
  bool infinite_quantity = false;
  bool draggable = false;
  bool drop_target = false;
  InventorySlotRole role = InventorySlotRole::None;
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
  bool secondary_inventory_visible = false;
  std::string title = "Inventory";
  std::string subtitle;
  std::string active_tab = "Inventory";
  std::string secondary_tab = "Crafting";
  std::string secondary_inventory_title;
  std::string secondary_inventory_status;
  std::string character_name = "Player";
  std::string character_status;
  std::string search_hint = "Search";
  std::vector<InventorySlotModel> equipment_slots;
  InventoryGridModel backpack;
  InventoryGridModel hotbar;
  InventoryGridModel secondary_inventory;
  std::vector<InventoryRecipeModel> recipes;
};

struct InventoryOverlayAction {
  enum class Type {
    None,
    TransferSupplyToPlayer,
  };

  Type type = Type::None;
  std::string item_id;
};

class InventoryOverlayWidget {
public:
  [[nodiscard]] InventoryOverlayAction draw(UiCanvas &canvas, const InventoryOverlayModel &model);

private:
  bool dragging_ = false;
  InventorySlotModel dragged_slot_{};
};

} // namespace aster
