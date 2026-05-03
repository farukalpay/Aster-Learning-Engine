// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/item_system.hpp"

#include <stdexcept>

namespace aster {

void ItemRegistry::clear() {
  definitions_.clear();
  by_id_.clear();
}

void ItemRegistry::add(ItemDefinition definition) {
  if (definition.id.empty()) {
    throw std::invalid_argument("Item definitions require a stable id.");
  }
  if (definition.display_name.empty()) {
    definition.display_name = definition.id;
  }
  if (definition.short_label.empty()) {
    definition.short_label = definition.display_name.substr(0u, 1u);
  }
  if (definition.max_stack < 1) {
    throw std::invalid_argument("Item max_stack must be positive.");
  }
  if (!definition.stackable) {
    definition.max_stack = 1;
  }
  if (by_id_.contains(definition.id)) {
    throw std::invalid_argument("Duplicate item definition id: " + definition.id);
  }

  const std::size_t index = definitions_.size();
  by_id_.emplace(definition.id, index);
  definitions_.push_back(std::move(definition));
}

const ItemDefinition *ItemRegistry::find(const std::string_view id) const {
  const auto found = by_id_.find(std::string(id));
  return found == by_id_.end() ? nullptr : &definitions_[found->second];
}

bool ItemRegistry::contains(const std::string_view id) const {
  return find(id) != nullptr;
}

const std::vector<ItemDefinition> &ItemRegistry::definitions() const {
  return definitions_;
}

} // namespace aster
