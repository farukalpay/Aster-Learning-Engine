// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"
#include "aster/scene/scene.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace aster {

struct SpriteAtlasRegion2D {
  std::string id;
  Vec2 uv_min{0.0f, 0.0f};
  Vec2 uv_max{1.0f, 1.0f};
  Vec2 size{1.0f, 1.0f};
};

struct SpriteQuadDesc2D {
  SpriteAtlasRegion2D region{};
  Vec2 pivot{0.5f, 0.5f};
  bool flip_x = false;
  bool flip_y = false;
};

[[nodiscard]] CpuMesh makeSpriteQuadMesh2D(const SpriteQuadDesc2D &desc);
[[nodiscard]] std::shared_ptr<const CpuMesh> makeSharedSpriteQuad2D(const SpriteQuadDesc2D &desc);
[[nodiscard]] Material makeSpriteMaterial2D(std::string_view asset_id, LinearRgb base,
                                            EmissionColor emission = {},
                                            float emission_strength = 0.0f, float opacity = 1.0f);
[[nodiscard]] RenderObject makeSpriteObject2D(std::string name, std::shared_ptr<const CpuMesh> mesh,
                                              Material material, Vec2 position, float depth = 0.0f,
                                              Vec2 scale = {1.0f, 1.0f});

} // namespace aster
