// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/inventory_system.hpp"

#include <algorithm>

namespace aster {

namespace {

bool canStackInto(const ItemStack &stack, const ItemDefinition &definition) {
  return definition.stackable && stack.item_id == definition.id &&
         stack.quantity < definition.max_stack;
}

} // namespace

InventoryContainer::InventoryContainer(const std::size_t slot_count) : slots_(slot_count) {}

void InventoryContainer::resize(const std::size_t slot_count) {
  slots_.resize(slot_count);
}

void InventoryContainer::clear() {
  for (ItemStack &slot : slots_) {
    slot = {};
  }
}

std::size_t InventoryContainer::slotCount() const {
  return slots_.size();
}

const std::vector<ItemStack> &InventoryContainer::slots() const {
  return slots_;
}

const ItemStack *InventoryContainer::slot(const std::size_t index) const {
  return index < slots_.size() ? &slots_[index] : nullptr;
}

std::optional<std::size_t> InventoryContainer::addItem(const ItemDefinition &definition,
                                                       int quantity) {
  if (quantity <= 0) {
    return std::nullopt;
  }

  for (std::size_t i = 0; i < slots_.size() && quantity > 0; ++i) {
    ItemStack &slot = slots_[i];
    if (!canStackInto(slot, definition)) {
      continue;
    }
    const int room = std::max(0, definition.max_stack - slot.quantity);
    const int moved = std::min(room, quantity);
    slot.quantity += moved;
    quantity -= moved;
    if (quantity == 0) {
      return i;
    }
  }

  for (std::size_t i = 0; i < slots_.size(); ++i) {
    ItemStack &slot = slots_[i];
    if (!slot.empty()) {
      continue;
    }
    slot.item_id = definition.id;
    slot.quantity = std::min(quantity, definition.max_stack);
    return i;
  }

  return std::nullopt;
}

bool InventoryContainer::removeItem(const std::string_view item_id, int quantity) {
  if (item_id.empty() || quantity <= 0) {
    return false;
  }
  int available = 0;
  for (const ItemStack &slot : slots_) {
    if (slot.item_id == item_id) {
      available += slot.quantity;
    }
  }
  if (available < quantity) {
    return false;
  }

  for (ItemStack &slot : slots_) {
    if (slot.item_id != item_id || quantity <= 0) {
      continue;
    }
    const int removed = std::min(slot.quantity, quantity);
    slot.quantity -= removed;
    quantity -= removed;
    if (slot.quantity <= 0) {
      slot = {};
    }
  }
  return true;
}

std::optional<ItemStack> InventoryContainer::takeSlot(const std::size_t index) {
  if (index >= slots_.size() || slots_[index].empty()) {
    return std::nullopt;
  }
  ItemStack result = slots_[index];
  slots_[index] = {};
  return result;
}

Hotbar::Hotbar(const std::size_t slot_count) : container_(slot_count) {}

void Hotbar::resize(const std::size_t slot_count) {
  container_.resize(slot_count);
  if (slot_count == 0u) {
    selected_index_ = 0u;
  } else if (selected_index_ >= slot_count) {
    selected_index_ = slot_count - 1u;
  }
}

void Hotbar::clear() {
  container_.clear();
  selected_index_ = 0u;
}

std::size_t Hotbar::selectedIndex() const {
  return selected_index_;
}

bool Hotbar::select(const std::size_t index) {
  if (index >= container_.slotCount()) {
    return false;
  }
  selected_index_ = index;
  return true;
}

const std::vector<ItemStack> &Hotbar::slots() const {
  return container_.slots();
}

const ItemStack *Hotbar::selectedStack() const {
  return container_.slot(selected_index_);
}

std::optional<std::size_t> Hotbar::addItem(const ItemDefinition &definition, const int quantity) {
  return container_.addItem(definition, quantity);
}

} // namespace aster
