// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aster {

enum class TileLayerRole2D : std::uint32_t {
  Background,
  Solid,
  Foreground,
};

struct TileCoord2D {
  int x = 0;
  int y = 0;

  [[nodiscard]] friend bool operator<(const TileCoord2D lhs, const TileCoord2D rhs) noexcept {
    if (lhs.y != rhs.y) {
      return lhs.y < rhs.y;
    }
    return lhs.x < rhs.x;
  }

  [[nodiscard]] friend bool operator==(const TileCoord2D lhs, const TileCoord2D rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
  }
};

struct TileCell2D {
  TileCoord2D coord{};
  std::string tile_id;
  TileLayerRole2D layer = TileLayerRole2D::Solid;
  bool solid = true;
  std::uint8_t autotile_mask = 0u;
};

struct TileAabb2D {
  Vec2 center{};
  Vec2 half_extents{0.5f, 0.5f};
};

class SparseTileLayer2D {
public:
  void clear();
  void set(TileCoord2D coord, std::string tile_id, TileLayerRole2D layer, bool solid);
  bool erase(TileCoord2D coord);
  [[nodiscard]] const TileCell2D *cell(TileCoord2D coord) const;
  [[nodiscard]] bool solidAt(TileCoord2D coord) const;
  [[nodiscard]] bool intersectsSolid(TileAabb2D bounds) const;
  [[nodiscard]] std::vector<TileCell2D> cells() const;
  void rebuildAutotileMasks();

private:
  std::map<TileCoord2D, TileCell2D> cells_;
};

[[nodiscard]] TileCoord2D tileCoordAt(Vec2 position, float tile_size = 1.0f);
[[nodiscard]] std::vector<TileCoord2D> tileCoordsOverlapping(TileAabb2D bounds,
                                                             float tile_size = 1.0f);

} // namespace aster
