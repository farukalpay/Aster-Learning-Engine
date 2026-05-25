// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/construction_yard_asset.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] Vec3 rotateX(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

[[nodiscard]] Vec3 rotateY(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

[[nodiscard]] Vec3 rotateZ(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

[[nodiscard]] Vec3 rotateEuler(const Vec3 value, const Vec3 rotation) {
  return rotateZ(rotateY(rotateX(value, rotation.x), rotation.y), rotation.z);
}

[[nodiscard]] Vertex transformedVertex(Vertex vertex, const Vec3 position, const Vec3 rotation,
                                       const Vec3 scale) {
  vertex.position = rotateEuler({vertex.position.x * scale.x,
                                 vertex.position.y * scale.y,
                                 vertex.position.z * scale.z},
                                rotation) +
                    position;
  vertex.normal = normalize(rotateEuler(vertex.normal, rotation));
  return vertex;
}

void appendMesh(CpuMesh &target, const CpuMesh &source, const Vec3 position,
                const Vec3 rotation = {}, const Vec3 scale = {1.0f, 1.0f, 1.0f}) {
  const std::uint32_t base = static_cast<std::uint32_t>(target.vertices.size());
  target.vertices.reserve(target.vertices.size() + source.vertices.size());
  target.indices.reserve(target.indices.size() + source.indices.size());
  for (Vertex vertex : source.vertices) {
    target.vertices.push_back(transformedVertex(vertex, position, rotation, scale));
  }
  for (const std::uint32_t index : source.indices) {
    target.indices.push_back(base + index);
  }
}

[[nodiscard]] Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return normalize(cross(b - a, c - a));
}

void appendTriangle(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec2 uv_a,
                    const Vec2 uv_b, const Vec2 uv_c) {
  const Vec3 normal = faceNormal(a, b, c);
  const Vec3 tangent = normalize(std::abs(normal.y) > 0.85f ? Vec3{1.0f, 0.0f, 0.0f}
                                                            : cross({0.0f, 1.0f, 0.0f}, normal));
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, uv_a, {tangent.x, tangent.y, tangent.z, 1.0f}});
  mesh.vertices.push_back({b, normal, uv_b, {tangent.x, tangent.y, tangent.z, 1.0f}});
  mesh.vertices.push_back({c, normal, uv_c, {tangent.x, tangent.y, tangent.z, 1.0f}});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u});
}

void appendQuad(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                const Vec2 uv_a = {0.0f, 0.0f}, const Vec2 uv_b = {1.0f, 0.0f},
                const Vec2 uv_c = {1.0f, 1.0f}, const Vec2 uv_d = {0.0f, 1.0f}) {
  appendTriangle(mesh, a, b, c, uv_a, uv_b, uv_c);
  appendTriangle(mesh, a, c, d, uv_a, uv_c, uv_d);
}

[[nodiscard]] CpuMesh makeCylinder(const int segments, const float radius, const float depth) {
  if (segments < 6 || radius <= 0.0f || depth <= 0.0f) {
    throw std::invalid_argument("Construction cylinder requires segments >= 6 and positive size.");
  }
  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(segments * 12));
  mesh.indices.reserve(static_cast<std::size_t>(segments * 12));
  const float half = depth * 0.5f;
  for (int i = 0; i < segments; ++i) {
    const float a0 = static_cast<float>(i) / static_cast<float>(segments) * kPi * 2.0f;
    const float a1 = static_cast<float>(i + 1) / static_cast<float>(segments) * kPi * 2.0f;
    const Vec3 p0{std::cos(a0) * radius, -half, std::sin(a0) * radius};
    const Vec3 p1{std::cos(a1) * radius, -half, std::sin(a1) * radius};
    const Vec3 p2{std::cos(a1) * radius, half, std::sin(a1) * radius};
    const Vec3 p3{std::cos(a0) * radius, half, std::sin(a0) * radius};
    appendQuad(mesh, p0, p1, p2, p3, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
               {0.0f, 1.0f});
    appendTriangle(mesh, {0.0f, -half, 0.0f}, p1, p0, {0.5f, 0.5f}, {1.0f, 0.0f},
                   {0.0f, 0.0f});
    appendTriangle(mesh, {0.0f, half, 0.0f}, p3, p2, {0.5f, 0.5f}, {0.0f, 1.0f},
                   {1.0f, 1.0f});
  }
  return mesh;
}

[[nodiscard]] CpuMesh makeCylinderX(const int segments, const float radius, const float depth) {
  CpuMesh mesh;
  appendMesh(mesh, makeCylinder(segments, radius, depth), {}, {0.0f, 0.0f, kPi * 0.5f});
  return mesh;
}

[[nodiscard]] CpuMesh makeForkliftTireMesh(const int segments, const float outer_radius,
                                           const float inner_radius, const float depth) {
  if (segments < 8 || outer_radius <= inner_radius || inner_radius <= 0.0f || depth <= 0.0f) {
    throw std::invalid_argument(
        "Construction forklift tire requires segments >= 8 and valid radii.");
  }
  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(segments * 36));
  mesh.indices.reserve(static_cast<std::size_t>(segments * 42));
  const float half = depth * 0.5f;
  for (int i = 0; i < segments; ++i) {
    const float a0 = static_cast<float>(i) / static_cast<float>(segments) * kPi * 2.0f;
    const float a1 = static_cast<float>(i + 1) / static_cast<float>(segments) * kPi * 2.0f;
    const Vec3 o0{-half, std::cos(a0) * outer_radius, std::sin(a0) * outer_radius};
    const Vec3 o1{-half, std::cos(a1) * outer_radius, std::sin(a1) * outer_radius};
    const Vec3 o2{half, std::cos(a1) * outer_radius, std::sin(a1) * outer_radius};
    const Vec3 o3{half, std::cos(a0) * outer_radius, std::sin(a0) * outer_radius};
    const Vec3 i0{-half, std::cos(a0) * inner_radius, std::sin(a0) * inner_radius};
    const Vec3 i1{-half, std::cos(a1) * inner_radius, std::sin(a1) * inner_radius};
    const Vec3 i2{half, std::cos(a1) * inner_radius, std::sin(a1) * inner_radius};
    const Vec3 i3{half, std::cos(a0) * inner_radius, std::sin(a0) * inner_radius};
    appendQuad(mesh, o0, o1, o2, o3);
    appendQuad(mesh, i1, i0, i3, i2);
    appendQuad(mesh, o3, o2, i2, i3);
    appendQuad(mesh, o1, o0, i0, i1);
  }

  const CpuMesh tread = makeBox();
  const int tread_count = std::max(10, segments / 2);
  for (int i = 0; i < tread_count; ++i) {
    const float angle = (static_cast<float>(i) + 0.5f) / static_cast<float>(tread_count) *
                        kPi * 2.0f;
    const Vec3 radial_center = rotateX({0.0f, outer_radius + 0.025f, 0.0f}, angle);
    appendMesh(mesh, tread, radial_center, {angle, 0.0f, i % 2 == 0 ? 0.12f : -0.12f},
               {depth * 0.82f, 0.070f, 0.18f});
  }
  return mesh;
}

[[nodiscard]] CpuMesh makeToothMesh() {
  CpuMesh mesh;
  const Vec3 a{-0.08f, -0.18f, -0.06f};
  const Vec3 b{0.08f, -0.18f, -0.06f};
  const Vec3 c{0.08f, -0.18f, 0.06f};
  const Vec3 d{-0.08f, -0.18f, 0.06f};
  const Vec3 tip{0.0f, 0.18f, 0.0f};
  appendTriangle(mesh, a, b, tip, {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f});
  appendTriangle(mesh, b, c, tip, {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f});
  appendTriangle(mesh, c, d, tip, {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f});
  appendTriangle(mesh, d, a, tip, {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f});
  appendQuad(mesh, d, c, b, a);
  return mesh;
}

[[nodiscard]] CpuMesh makeFramedDemolitionPanelMesh() {
  CpuMesh mesh;
  const CpuMesh box = makeBox();
  appendMesh(mesh, box, {-0.44f, 0.0f, 0.0f}, {}, {0.12f, 1.0f, 0.86f});
  appendMesh(mesh, box, {0.44f, 0.0f, 0.0f}, {}, {0.12f, 1.0f, 0.86f});
  appendMesh(mesh, box, {0.0f, -0.44f, 0.0f}, {}, {0.88f, 0.12f, 0.86f});
  appendMesh(mesh, box, {0.0f, 0.44f, 0.0f}, {}, {0.88f, 0.12f, 0.86f});
  appendMesh(mesh, box, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -0.68f},
             {0.10f, 1.12f, 0.72f});
  return mesh;
}

void addPart(AsterConstructionYardAsset &asset, std::string name, std::string slot, CpuMesh mesh,
             const Vec3 position, const Vec3 rotation = {},
             const Vec3 scale = {1.0f, 1.0f, 1.0f}) {
  asset.parts.push_back({std::move(name), std::move(slot), std::move(mesh), position, rotation,
                         scale});
}

} // namespace

CpuMesh AsterConstructionYardAsset::mergedRenderMesh() const {
  CpuMesh out;
  for (const AsterConstructionAssetPart &part : parts) {
    appendMesh(out, part.mesh, part.local_position, part.local_rotation, part.local_scale);
  }
  return out;
}

std::size_t AsterConstructionYardAsset::renderVertexCount() const noexcept {
  std::size_t total = 0u;
  for (const AsterConstructionAssetPart &part : parts) {
    total += part.mesh.vertices.size();
  }
  return total;
}

std::size_t AsterConstructionYardAsset::renderIndexCount() const noexcept {
  std::size_t total = 0u;
  for (const AsterConstructionAssetPart &part : parts) {
    total += part.mesh.indices.size();
  }
  return total;
}

AsterConstructionYardAsset makeAsterConstructionForkliftAsset(
    AsterConstructionForkliftSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh tire = makeForkliftTireMesh(24, 0.50f, 0.24f, 0.40f);
  const CpuMesh wheel_hub = makeCylinderX(18, 0.24f, 0.46f);

  addPart(asset, "low-poly forklift main orange chassis", "forklift.paint", box,
          {0.0f, 0.48f, 0.02f}, {}, {spec.body_width, 0.54f, spec.body_length * 0.56f});
  addPart(asset, "forklift rounded rear counterweight", "forklift.paint", box,
          {0.0f, 0.72f, -0.98f}, {}, {spec.body_width * 1.02f, 0.86f, 0.70f});
  addPart(asset, "forklift operator seat block", "forklift.rubber", box, {0.0f, 1.14f, -0.20f},
          {}, {0.46f, 0.20f, 0.50f});
  addPart(asset, "forklift back rest", "forklift.rubber", box, {0.0f, 1.38f, -0.43f},
          {0.18f, 0.0f, 0.0f}, {0.48f, 0.44f, 0.12f});
  addPart(asset, "forklift overhead guard roof", "forklift.steel", box, {0.0f, 2.04f, -0.20f},
          {}, {1.08f, 0.11f, 1.05f});
  addPart(asset, "forklift guard left upright", "forklift.steel", box, {-0.48f, 1.52f, -0.58f},
          {}, {0.08f, 0.98f, 0.08f});
  addPart(asset, "forklift guard right upright", "forklift.steel", box, {0.48f, 1.52f, -0.58f},
          {}, {0.08f, 0.98f, 0.08f});
  addPart(asset, "forklift guard front left upright", "forklift.steel", box,
          {-0.48f, 1.52f, 0.30f}, {}, {0.08f, 0.98f, 0.08f});
  addPart(asset, "forklift guard front right upright", "forklift.steel", box,
          {0.48f, 1.52f, 0.30f}, {}, {0.08f, 0.98f, 0.08f});
  addPart(asset, "forklift mast left rail", "forklift.steel", box, {-0.34f, 1.27f, 1.23f},
          {}, {0.10f, spec.mast_height, 0.10f});
  addPart(asset, "forklift mast right rail", "forklift.steel", box, {0.34f, 1.27f, 1.23f},
          {}, {0.10f, spec.mast_height, 0.10f});
  addPart(asset, "forklift lifting carriage", "forklift.steel", box, {0.0f, 0.78f, 1.30f},
          {}, {0.86f, 0.18f, 0.16f});
  addPart(asset, "forklift left fork tine", "forklift.steel", box, {-0.23f, 0.37f, 2.02f},
          {}, {0.11f, 0.08f, 1.40f});
  addPart(asset, "forklift right fork tine", "forklift.steel", box, {0.23f, 0.37f, 2.02f},
          {}, {0.11f, 0.08f, 1.40f});
  addPart(asset, "forklift left rear wheel treaded tire", "forklift.rubber", tire,
          {-0.66f, 0.29f, -0.78f}, {}, {0.56f, 0.56f, 0.56f});
  addPart(asset, "forklift left rear wheel steel rim", "forklift.steel", wheel_hub,
          {-0.66f, 0.29f, -0.78f}, {}, {0.56f, 0.56f, 0.56f});
  addPart(asset, "forklift right rear wheel treaded tire", "forklift.rubber", tire,
          {0.66f, 0.29f, -0.78f}, {}, {0.56f, 0.56f, 0.56f});
  addPart(asset, "forklift right rear wheel steel rim", "forklift.steel", wheel_hub,
          {0.66f, 0.29f, -0.78f}, {}, {0.56f, 0.56f, 0.56f});
  addPart(asset, "forklift left front wheel treaded tire", "forklift.rubber", tire,
          {-0.66f, 0.24f, 0.78f}, {}, {0.46f, 0.46f, 0.46f});
  addPart(asset, "forklift left front wheel steel rim", "forklift.steel", wheel_hub,
          {-0.66f, 0.24f, 0.78f}, {}, {0.46f, 0.46f, 0.46f});
  addPart(asset, "forklift right front wheel treaded tire", "forklift.rubber", tire,
          {0.66f, 0.24f, 0.78f}, {}, {0.46f, 0.46f, 0.46f});
  addPart(asset, "forklift right front wheel steel rim", "forklift.steel", wheel_hub,
          {0.66f, 0.24f, 0.78f}, {}, {0.46f, 0.46f, 0.46f});
  return asset;
}

AsterConstructionYardAsset makeAsterRecyclerShredderAsset(AsterRecyclerShredderSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh drum = makeCylinder(24, 0.34f, 1.86f);
  const CpuMesh shaft = makeCylinderX(18, 0.17f, 2.68f);
  const CpuMesh sprocket = makeCylinderX(20, 0.34f, 0.10f);
  const CpuMesh motor = makeCylinderX(24, 0.36f, 1.02f);
  const CpuMesh tooth = makeToothMesh();

  addPart(asset, "recycler shredder grounded skid left", "shredder.steel", box,
          {-0.94f, 0.10f, 0.12f}, {}, {0.18f, 0.20f, spec.length});
  addPart(asset, "recycler shredder grounded skid right", "shredder.steel", box,
          {0.94f, 0.10f, 0.12f}, {}, {0.18f, 0.20f, spec.length});
  for (const float x : {-1.02f, 1.02f}) {
    addPart(asset, "recycler shredder welded support leg", "shredder.blue_metal", box,
            {x, 0.63f, -1.42f}, {}, {0.16f, 1.06f, 0.16f});
    addPart(asset, "recycler shredder welded support leg", "shredder.blue_metal", box,
            {x, 0.63f, 0.72f}, {}, {0.16f, 1.06f, 0.16f});
  }
  addPart(asset, "recycler shredder open cutting tray left wall", "shredder.blue_metal", box,
          {-1.04f, 1.19f, -0.56f}, {}, {0.14f, 0.62f, 2.42f});
  addPart(asset, "recycler shredder open cutting tray right wall", "shredder.blue_metal", box,
          {1.04f, 1.19f, -0.56f}, {}, {0.14f, 0.62f, 2.42f});
  addPart(asset, "recycler shredder low open feed lip", "shredder.steel", box,
          {0.0f, 1.05f, -1.82f}, {}, {1.62f, 0.10f, 0.16f});
  addPart(asset, "recycler shredder left cutter shaft", "shredder.steel", shaft,
          {-0.43f, 1.30f, -0.54f});
  addPart(asset, "recycler shredder right cutter shaft", "shredder.steel", shaft,
          {0.43f, 1.30f, -0.54f});
  addPart(asset, "recycler shredder left cutter drum", "shredder.steel", drum,
          {-0.43f, 1.30f, -0.54f}, {kPi * 0.5f, 0.0f, 0.0f});
  addPart(asset, "recycler shredder right cutter drum", "shredder.steel", drum,
          {0.43f, 1.30f, -0.54f}, {kPi * 0.5f, 0.0f, 0.0f});
  for (int i = 0; i < 20; ++i) {
    const float z = -1.36f + static_cast<float>(i / 2) * 0.18f;
    const float x = i % 2 == 0 ? -0.43f : 0.43f;
    addPart(asset, "recycler shredder visible counter rotating cutter tooth " + std::to_string(i),
            "shredder.steel", tooth, {x, 1.60f, z},
            {0.0f, 0.0f, i % 2 == 0 ? 0.0f : kPi}, {1.55f, 1.55f, 1.55f});
  }
  addPart(asset, "recycler shredder exposed drive gearbox", "shredder.blue_metal", box,
          {1.50f, 1.10f, -0.22f}, {}, {0.72f, 0.82f, 0.72f});
  addPart(asset, "recycler shredder exposed drive motor", "shredder.blue_metal", motor,
          {1.84f, 0.74f, 0.88f});
  addPart(asset, "recycler shredder left chain sprocket", "shredder.hazard", sprocket,
          {1.24f, 1.30f, -0.92f});
  addPart(asset, "recycler shredder motor chain sprocket", "shredder.hazard", sprocket,
          {1.24f, 0.82f, 0.72f}, {}, {0.70f, 0.70f, 0.70f});
  addPart(asset, "recycler shredder visible chain upper run", "shredder.steel", box,
          {1.29f, 1.50f, -0.10f}, {0.0f, 0.0f, -0.22f}, {0.08f, 0.08f, 1.72f});
  addPart(asset, "recycler shredder visible chain lower run", "shredder.steel", box,
          {1.29f, 0.72f, -0.10f}, {0.0f, 0.0f, -0.22f}, {0.08f, 0.08f, 1.72f});
  addPart(asset, "recycler shredder motor power cable", "shredder.rubber", box,
          {1.95f, 0.34f, 1.02f}, {0.0f, 0.0f, 0.34f}, {0.045f, 0.70f, 0.045f});
  addPart(asset, "recycler shredder output chute", "shredder.blue_metal", box,
          {0.0f, 0.68f, 1.64f}, {-7.0f * kPi / 180.0f, 0.0f, 0.0f},
          {1.32f, 0.25f, 1.86f});
  addPart(asset, "recycler shredder yellow hazard panel left", "shredder.hazard", box,
          {-1.12f, 1.35f, -0.12f}, {}, {0.035f, 0.46f, 0.72f});
  addPart(asset, "recycler shredder yellow hazard panel right", "shredder.hazard", box,
          {1.12f, 1.35f, -0.12f}, {}, {0.035f, 0.46f, 0.72f});
  return asset;
}

AsterConstructionYardAsset makeAsterPipePalletAsset(AsterPipePalletSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh pipe = makeCylinder(28, spec.pipe_outer_radius, spec.pipe_length);
  addPart(asset, "pipe pallet timber deck", "pallet.wood", box, {0.0f, 0.17f, 0.0f}, {},
          {1.28f, 0.14f, 2.70f});
  addPart(asset, "pipe pallet left runner", "pallet.wood", box, {-0.44f, 0.055f, 0.0f}, {},
          {0.16f, 0.11f, 2.64f});
  addPart(asset, "pipe pallet right runner", "pallet.wood", box, {0.44f, 0.055f, 0.0f}, {},
          {0.16f, 0.11f, 2.64f});
  const int pipes = std::max(spec.pipe_count, 1);
  for (int i = 0; i < pipes; ++i) {
    const int row = i / 4;
    const int column = i % 4;
    const float x = -0.46f + static_cast<float>(column) * 0.31f + static_cast<float>(row) * 0.14f;
    const float y = 0.35f + static_cast<float>(row) * 0.26f;
    addPart(asset, "rusted hollow pipe on pallet " + std::to_string(i), "pipe.rusted", pipe,
            {x, y, 0.0f}, {kPi * 0.5f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  }
  addPart(asset, "pipe pallet fork pocket shadow left", "pallet.shadow", box,
          {-0.24f, 0.115f, -0.12f}, {}, {0.18f, 0.05f, 2.35f});
  addPart(asset, "pipe pallet fork pocket shadow right", "pallet.shadow", box,
          {0.24f, 0.115f, -0.12f}, {}, {0.18f, 0.05f, 2.35f});
  return asset;
}

AsterConstructionYardAsset makeAsterShreddedMetalScrapAsset(AsterScrapShardSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  for (int i = 0; i < std::max(spec.shard_count, 1); ++i) {
    CpuMesh shard;
    const float width = 0.06f + 0.012f * static_cast<float>(i % 4);
    const float length = 0.24f + 0.035f * static_cast<float>((i * 3) % 5);
    const float curl = 0.015f * static_cast<float>((i % 3) - 1);
    appendTriangle(shard, {-width, 0.0f, -length}, {width, curl, -length * 0.76f},
                   {width * 0.45f, -curl, length}, {0.0f, 0.0f}, {1.0f, 0.12f},
                   {0.75f, 1.0f});
    appendTriangle(shard, {-width, 0.0f, -length}, {width * 0.45f, -curl, length},
                   {-width * 0.62f, curl, length * 0.72f}, {0.0f, 0.0f}, {0.75f, 1.0f},
                   {0.12f, 0.86f});
    const float angle = static_cast<float>(i) * 0.71f;
    const float radius = 0.08f + static_cast<float>(i % 6) * 0.035f;
    addPart(asset, "shredded metal curled scrap shard " + std::to_string(i), "scrap.metal",
            shard, {std::cos(angle) * radius, static_cast<float>(i % 4) * 0.012f,
                    std::sin(angle) * radius},
            {0.12f * static_cast<float>(i % 3), angle, 0.20f * static_cast<float>(i % 5)},
            {1.0f, 1.0f, 1.0f});
  }
  return asset;
}

AsterConstructionYardAsset makeAsterModularConstructionSiteAsset(AsterConstructionSiteSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh framed_panel = makeFramedDemolitionPanelMesh();
  const float x = spec.width * 0.5f;
  const float z = spec.depth * 0.5f;
  constexpr float module_height = 2.54f;
  for (int face = 0; face < 2; ++face) {
    const float face_z = face == 0 ? z - 0.40f : -z + 0.40f;
    for (int column = 0; column <= 4; ++column) {
      const float column_x = -x + 0.28f + static_cast<float>(column) * 3.05f;
      addPart(asset, "construction site terrain foot pad", "site.concrete", box,
              {column_x, 0.035f, face_z}, {}, {0.72f, 0.07f, 0.72f});
      addPart(asset, "construction site vertical ground anchor column", "site.steel", box,
              {column_x, spec.height * 0.5f, face_z}, {}, {0.16f, spec.height, 0.16f});
    }
  }
  for (int i = 0; i < 16; ++i) {
    const int tier = i / 8;
    const int face = (i % 8) / 4;
    const int bay = i % 4;
    const Vec3 position = {-x + 1.54f + static_cast<float>(bay) * 3.05f,
                           module_height * 0.5f + static_cast<float>(tier) * module_height,
                           face == 0 ? z - 0.40f : -z + 0.40f};
    addPart(asset, "demolition light module " + std::to_string(i), "site.panel", framed_panel,
            position, {}, {2.52f, 2.54f, 0.20f});
  }
  for (int i = 0; i < 8; ++i) {
    const int face = i / 4;
    const int bay = i % 4;
    addPart(asset, "demolition heavy beam " + std::to_string(i), "site.steel", box,
            {-x + 1.54f + static_cast<float>(bay) * 3.05f, spec.height - 0.40f,
             face == 0 ? z - 0.40f : -z + 0.40f},
            {}, {2.76f, 0.28f, 0.34f});
  }
  addPart(asset, "construction site scaffold stair landing", "site.steel", box,
          {-x - 0.72f, 1.28f, z - 1.22f}, {}, {1.26f, 0.12f, 2.60f});
  for (const float leg_z : {z - 2.36f, z - 0.08f}) {
    addPart(asset, "construction site scaffold vertical ground pin", "site.steel", box,
            {-x - 0.72f, 0.64f, leg_z}, {}, {0.10f, 1.28f, 0.10f});
  }
  addPart(asset, "construction site safety rail", "site.hazard", box,
          {-x - 0.72f, 2.02f, z - 2.20f}, {}, {1.26f, 0.10f, 0.10f});
  return asset;
}

AsterConstructionYardAsset makeAsterMobileCraneAsset(AsterMobileCraneSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh tire = makeForkliftTireMesh(20, 0.56f, 0.29f, 0.38f);
  addPart(asset, "mobile crane wheeled chassis", "crane.paint", box, {0.0f, 0.62f, 0.0f}, {},
          {2.34f, 0.46f, 4.70f});
  addPart(asset, "mobile crane operator cab", "crane.paint", box, {-0.58f, 1.50f, 0.86f}, {},
          {0.92f, 1.22f, 1.28f});
  for (const float x : {-1.22f, 1.22f}) {
    for (const float z : {-1.36f, 1.36f}) {
      addPart(asset, "mobile crane rubber road wheel", "crane.rubber", tire, {x, 0.54f, z}, {},
              {0.86f, 0.86f, 0.86f});
      addPart(asset, "mobile crane deployed outrigger", "crane.steel", box,
              {x * 1.55f, 0.20f, z}, {}, {1.22f, 0.12f, 0.16f});
      addPart(asset, "mobile crane grounded outrigger pad", "crane.hazard", box,
              {x * 1.98f, 0.08f, z}, {}, {0.54f, 0.12f, 0.54f});
    }
  }
  addPart(asset, "mobile crane rotating turret", "crane.steel", box, {0.0f, 1.08f, -0.62f}, {},
          {1.48f, 0.34f, 1.36f});
  addPart(asset, "mobile crane telescopic boom", "crane.paint", box,
          {0.0f, spec.boom_height, -spec.boom_length * 0.33f}, {-0.60f, 0.0f, 0.0f},
          {0.42f, 0.42f, spec.boom_length});
  addPart(asset, "mobile crane hanging cable", "crane.cable", box,
          {0.0f, 3.15f, -spec.boom_length * 0.77f}, {}, {0.045f, 3.10f, 0.045f});
  addPart(asset, "mobile crane lifting hook", "crane.hazard", box,
          {0.0f, 1.56f, -spec.boom_length * 0.77f}, {}, {0.30f, 0.34f, 0.12f});
  return asset;
}

AsterConstructionYardAsset makeAsterHydraulicPressAsset(AsterHydraulicPressSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const CpuMesh cylinder = makeCylinder(20, 0.22f, 1.18f);
  addPart(asset, "hydraulic press left raised side rail", "press.steel", box,
          {-spec.width * 0.43f, 0.46f, 0.0f}, {}, {0.18f, 0.18f, spec.length});
  addPart(asset, "hydraulic press right raised side rail", "press.steel", box,
          {spec.width * 0.43f, 0.46f, 0.0f}, {}, {0.18f, 0.18f, spec.length});
  addPart(asset, "hydraulic press front raised cross rail", "press.steel", box,
          {0.0f, 0.46f, -spec.length * 0.43f}, {}, {spec.width, 0.16f, 0.16f});
  addPart(asset, "hydraulic press rear raised cross rail", "press.steel", box,
          {0.0f, 0.46f, spec.length * 0.43f}, {}, {spec.width, 0.16f, 0.16f});
  for (const float leg_x : {-spec.width * 0.42f, spec.width * 0.42f}) {
    for (const float leg_z : {-spec.length * 0.40f, spec.length * 0.40f}) {
      addPart(asset, "hydraulic press vertical ground pin leg", "press.steel", box,
              {leg_x, 0.24f, leg_z}, {}, {0.18f, 0.48f, 0.18f});
      addPart(asset, "hydraulic press terrain foot pad", "press.steel", box,
              {leg_x, 0.035f, leg_z}, {}, {0.46f, 0.07f, 0.46f});
    }
  }
  addPart(asset, "hydraulic press bunker left wall", "press.blue_metal", box,
          {-spec.width * 0.47f, 0.82f, -0.10f}, {}, {0.12f, 1.38f, spec.length});
  addPart(asset, "hydraulic press bunker right wall", "press.blue_metal", box,
          {spec.width * 0.47f, 0.82f, -0.10f}, {}, {0.12f, 1.38f, spec.length});
  addPart(asset, "hydraulic press ram carriage", "press.hazard", box, {0.0f, 1.02f, 1.05f}, {},
          {spec.width * 0.78f, 0.78f, 0.22f});
  addPart(asset, "hydraulic press piston left", "press.steel", cylinder, {-0.72f, 1.55f, 1.38f},
          {kPi * 0.5f, 0.0f, 0.0f});
  addPart(asset, "hydraulic press piston right", "press.steel", cylinder, {0.72f, 1.55f, 1.38f},
          {kPi * 0.5f, 0.0f, 0.0f});
  addPart(asset, "hydraulic press operator button pedestal", "press.blue_metal", box,
          {-1.66f, 0.68f, 0.62f}, {}, {0.34f, 1.08f, 0.34f});
  addPart(asset, "hydraulic press illuminated button", "press.hazard", box,
          {-1.66f, 1.26f, 0.62f}, {}, {0.20f, 0.10f, 0.20f});
  addPart(asset, "hydraulic press loose scrap bunker fill", "press.scrap", box,
          {0.0f, 0.38f, -0.54f}, {}, {spec.width * 0.72f, 0.36f, 1.42f});
  return asset;
}

AsterConstructionYardAsset makeAsterMetalBaleAsset(AsterMetalBaleSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  const float scale = std::clamp(spec.fill, 0.35f, 1.0f);
  addPart(asset, "pressed metal bale compacted body", "bale.scrap", box, {0.0f, 0.38f, 0.0f},
          {}, {1.04f * scale, 0.72f, 1.18f * scale});
  for (int i = 0; i < 3; ++i) {
    addPart(asset, "pressed metal bale binding strap", "bale.strap", box,
            {-0.34f + static_cast<float>(i) * 0.34f, 0.39f, 0.0f}, {},
            {0.045f, 0.76f, 1.24f * scale});
  }
  return asset;
}

AsterConstructionYardAsset makeAsterDeliveryRackAsset(AsterDeliveryRackSpec spec) {
  AsterConstructionYardAsset asset;
  asset.asset_id = std::move(spec.asset_id);
  const CpuMesh box = makeBox();
  addPart(asset, "bale delivery rack grounded platform", "rack.steel", box, {0.0f, 0.10f, 0.0f},
          {}, {6.60f, 0.20f, 4.40f});
  for (int row = 0; row < 3; ++row) {
    addPart(asset, "bale delivery rack painted divider", "rack.hazard", box,
            {-2.20f + static_cast<float>(row) * 2.20f, 0.30f, 0.0f}, {}, {0.06f, 0.40f, 4.10f});
  }
  addPart(asset, "bale delivery rack back stop", "rack.steel", box, {0.0f, 0.62f, 2.06f}, {},
          {6.60f, 1.02f, 0.10f});
  return asset;
}

CpuMesh makeAsterConstructionForkliftMesh(AsterConstructionForkliftSpec spec) {
  return makeAsterConstructionForkliftAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterRecyclerShredderMesh(AsterRecyclerShredderSpec spec) {
  return makeAsterRecyclerShredderAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterPipePalletMesh(AsterPipePalletSpec spec) {
  return makeAsterPipePalletAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterShreddedMetalScrapMesh(AsterScrapShardSpec spec) {
  return makeAsterShreddedMetalScrapAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterModularConstructionSiteMesh(AsterConstructionSiteSpec spec) {
  return makeAsterModularConstructionSiteAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterMobileCraneMesh(AsterMobileCraneSpec spec) {
  return makeAsterMobileCraneAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterHydraulicPressMesh(AsterHydraulicPressSpec spec) {
  return makeAsterHydraulicPressAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterMetalBaleMesh(AsterMetalBaleSpec spec) {
  return makeAsterMetalBaleAsset(std::move(spec)).mergedRenderMesh();
}

CpuMesh makeAsterDeliveryRackMesh(AsterDeliveryRackSpec spec) {
  return makeAsterDeliveryRackAsset(std::move(spec)).mergedRenderMesh();
}

} // namespace aster
