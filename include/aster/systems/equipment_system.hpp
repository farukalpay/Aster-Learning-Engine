// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/systems/inventory_system.hpp"

#include <string>

namespace aster {

class EquipmentSystem {
public:
  void clear();
  void equip(ItemStack stack);
  void equipFromHotbar(const Hotbar &hotbar);

  [[nodiscard]] const ItemStack &equipped() const;
  [[nodiscard]] bool hasEquippedItem() const;
  [[nodiscard]] bool isEquipped(const std::string &item_id) const;

private:
  ItemStack equipped_{};
};

} // namespace aster
