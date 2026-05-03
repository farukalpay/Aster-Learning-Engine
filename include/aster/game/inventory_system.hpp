// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/game/item_system.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct ItemStack {
  std::string item_id;
  int quantity = 0;

  [[nodiscard]] bool empty() const {
    return item_id.empty() || quantity <= 0;
  }
};

class InventoryContainer {
public:
  explicit InventoryContainer(std::size_t slot_count = 0u);

  void resize(std::size_t slot_count);
  void clear();
  [[nodiscard]] std::size_t slotCount() const;
  [[nodiscard]] const std::vector<ItemStack> &slots() const;
  [[nodiscard]] const ItemStack *slot(std::size_t index) const;

  [[nodiscard]] std::optional<std::size_t> addItem(const ItemDefinition &definition,
                                                   int quantity = 1);
  [[nodiscard]] bool removeItem(std::string_view item_id, int quantity = 1);
  [[nodiscard]] std::optional<ItemStack> takeSlot(std::size_t index);

private:
  std::vector<ItemStack> slots_;
};

class Hotbar {
public:
  explicit Hotbar(std::size_t slot_count = 6u);

  void resize(std::size_t slot_count);
  void clear();
  [[nodiscard]] std::size_t selectedIndex() const;
  [[nodiscard]] bool select(std::size_t index);
  [[nodiscard]] const std::vector<ItemStack> &slots() const;
  [[nodiscard]] const ItemStack *selectedStack() const;
  [[nodiscard]] std::optional<std::size_t> addItem(const ItemDefinition &definition,
                                                   int quantity = 1);

private:
  InventoryContainer container_;
  std::size_t selected_index_ = 0u;
};

} // namespace aster
