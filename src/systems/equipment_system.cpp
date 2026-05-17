// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/equipment_system.hpp"

#include <utility>

namespace aster {

void EquipmentSystem::clear() {
  equipped_ = {};
}

void EquipmentSystem::equip(ItemStack stack) {
  equipped_ = std::move(stack);
}

void EquipmentSystem::equipFromHotbar(const Hotbar &hotbar) {
  const ItemStack *stack = hotbar.selectedStack();
  if (stack == nullptr || stack->empty()) {
    clear();
    return;
  }
  equip(*stack);
}

const ItemStack &EquipmentSystem::equipped() const {
  return equipped_;
}

bool EquipmentSystem::hasEquippedItem() const {
  return !equipped_.empty();
}

bool EquipmentSystem::isEquipped(const std::string &item_id) const {
  return !equipped_.empty() && equipped_.item_id == item_id;
}

} // namespace aster
