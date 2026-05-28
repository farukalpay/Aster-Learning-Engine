// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/sprite2d.hpp"

#include <algorithm>
#include <utility>

namespace aster {

CpuMesh makeSpriteQuadMesh2D(const SpriteQuadDesc2D &desc) {
  const Vec2 size{std::max(desc.region.size.x, 0.001f), std::max(desc.region.size.y, 0.001f)};
  const float left = -desc.pivot.x * size.x;
  const float right = left + size.x;
  const float bottom = -desc.pivot.y * size.y;
  const float top = bottom + size.y;

  float u0 = desc.region.uv_min.x;
  float u1 = desc.region.uv_max.x;
  float v0 = desc.region.uv_min.y;
  float v1 = desc.region.uv_max.y;
  if (desc.flip_x) {
    std::swap(u0, u1);
  }
  if (desc.flip_y) {
    std::swap(v0, v1);
  }

  CpuMesh mesh;
  mesh.vertices = {
      {{left, 0.0f, bottom}, {0.0f, 1.0f, 0.0f}, {u0, v1}},
      {{right, 0.0f, bottom}, {0.0f, 1.0f, 0.0f}, {u1, v1}},
      {{right, 0.0f, top}, {0.0f, 1.0f, 0.0f}, {u1, v0}},
      {{left, 0.0f, top}, {0.0f, 1.0f, 0.0f}, {u0, v0}},
  };
  mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
  return mesh;
}

std::shared_ptr<const CpuMesh> makeSharedSpriteQuad2D(const SpriteQuadDesc2D &desc) {
  return std::make_shared<const CpuMesh>(makeSpriteQuadMesh2D(desc));
}

Material makeSpriteMaterial2D(const std::string_view asset_id, const LinearRgb base,
                              const EmissionColor emission, const float emission_strength,
                              const float opacity) {
  MaterialDesc desc;
  desc.base_color = base;
  desc.emission_color = emission;
  desc.emission_strength = emission_strength;
  desc.opacity = opacity;
  desc.double_sided = true;
  desc.cull_mode = FaceCullMode::None;
  desc.surface_profile = MaterialSurfaceProfile::Plain;
  desc.surface_pattern = SurfacePattern::None;
  desc.roughness = 0.82f;
  desc.metallic = 0.0f;
  desc.ambient_occlusion = 1.0f;
  desc.receives_shadows = false;
  if (opacity < 0.999f) {
    desc.alpha_mode = MaterialAlphaMode::Blend;
    desc.depth_write = MaterialDepthWrite::Disabled;
    desc.depth_policy.layer = RenderDepthLayer::SurfaceAttachment;
    desc.depth_policy.constant_bias = -0.004f;
  }
  Material material = makeMaterial(desc);
  material.asset_id = std::string(asset_id);
  return material;
}

RenderObject makeSpriteObject2D(std::string name, std::shared_ptr<const CpuMesh> mesh,
                                Material material, const Vec2 position, const float depth,
                                const Vec2 scale) {
  RenderObject object;
  object.name = std::move(name);
  object.custom_mesh = std::move(mesh);
  object.material = std::move(material);
  object.transform.position = {position.x, depth, position.y};
  object.transform.scale = {scale.x, 1.0f, scale.y};
  object.camera_occlusion_fade = false;
  object.auto_contact_shadow = false;
  object.casts_contact_shadow = false;
  object.casts_shadows = false;
  return object;
}

} // namespace aster
