// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/scene/tile_layer2d.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aster {

void SparseTileLayer2D::clear() {
  cells_.clear();
}

void SparseTileLayer2D::set(const TileCoord2D coord, std::string tile_id,
                            const TileLayerRole2D layer, const bool solid) {
  cells_[coord] = {.coord = coord,
                   .tile_id = std::move(tile_id),
                   .layer = layer,
                   .solid = solid,
                   .autotile_mask = 0u};
}

bool SparseTileLayer2D::erase(const TileCoord2D coord) {
  return cells_.erase(coord) > 0u;
}

const TileCell2D *SparseTileLayer2D::cell(const TileCoord2D coord) const {
  const auto it = cells_.find(coord);
  return it == cells_.end() ? nullptr : &it->second;
}

bool SparseTileLayer2D::solidAt(const TileCoord2D coord) const {
  const TileCell2D *tile = cell(coord);
  return tile != nullptr && tile->solid;
}

bool SparseTileLayer2D::intersectsSolid(const TileAabb2D bounds) const {
  for (const TileCoord2D coord : tileCoordsOverlapping(bounds)) {
    if (solidAt(coord)) {
      return true;
    }
  }
  return false;
}

std::vector<TileCell2D> SparseTileLayer2D::cells() const {
  std::vector<TileCell2D> out;
  out.reserve(cells_.size());
  for (const auto &[coord, cell] : cells_) {
    out.push_back(cell);
  }
  return out;
}

void SparseTileLayer2D::rebuildAutotileMasks() {
  for (auto &[coord, cell] : cells_) {
    std::uint8_t mask = 0u;
    mask |= solidAt({coord.x, coord.y + 1}) ? 1u : 0u;
    mask |= solidAt({coord.x + 1, coord.y}) ? 2u : 0u;
    mask |= solidAt({coord.x, coord.y - 1}) ? 4u : 0u;
    mask |= solidAt({coord.x - 1, coord.y}) ? 8u : 0u;
    cell.autotile_mask = mask;
  }
}

TileCoord2D tileCoordAt(const Vec2 position, const float tile_size) {
  const float size = std::max(tile_size, 0.001f);
  return {static_cast<int>(std::floor(position.x / size)),
          static_cast<int>(std::floor(position.y / size))};
}

std::vector<TileCoord2D> tileCoordsOverlapping(const TileAabb2D bounds, const float tile_size) {
  const float size = std::max(tile_size, 0.001f);
  const int min_x = static_cast<int>(std::floor((bounds.center.x - bounds.half_extents.x) / size));
  const int max_x = static_cast<int>(std::floor((bounds.center.x + bounds.half_extents.x) / size));
  const int min_y = static_cast<int>(std::floor((bounds.center.y - bounds.half_extents.y) / size));
  const int max_y = static_cast<int>(std::floor((bounds.center.y + bounds.half_extents.y) / size));
  std::vector<TileCoord2D> coords;
  coords.reserve(static_cast<std::size_t>(std::max(max_x - min_x + 1, 0)) *
                 static_cast<std::size_t>(std::max(max_y - min_y + 1, 0)));
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      coords.push_back({x, y});
    }
  }
  return coords;
}

} // namespace aster
