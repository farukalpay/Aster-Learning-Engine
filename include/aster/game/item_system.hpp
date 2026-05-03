// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aster {

enum class ItemType {
  Resource,
  Tool,
  LightTool,
  Weapon,
};

struct ItemDefinition {
  std::string id;
  std::string display_name;
  std::string short_label;
  ItemType type = ItemType::Resource;
  Vec3 tint{0.64f, 0.64f, 0.64f};
  Vec3 world_scale{1.0f, 1.0f, 1.0f};
  Vec3 hand_scale{1.0f, 1.0f, 1.0f};
  bool stackable = false;
  int max_stack = 1;
  bool creates_light = false;
  bool creates_fire_particles = false;
};

class ItemRegistry {
public:
  void clear();
  void add(ItemDefinition definition);

  [[nodiscard]] const ItemDefinition *find(std::string_view id) const;
  [[nodiscard]] bool contains(std::string_view id) const;
  [[nodiscard]] const std::vector<ItemDefinition> &definitions() const;

private:
  std::vector<ItemDefinition> definitions_;
  std::unordered_map<std::string, std::size_t> by_id_;
};

} // namespace aster
