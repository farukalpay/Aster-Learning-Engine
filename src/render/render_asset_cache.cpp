// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/render_device.hpp"

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/core/profiler.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <unordered_set>

namespace {

constexpr int kSphereSegments = 48;
constexpr int kSphereRings = 24;
constexpr int kRockSegments = 28;
constexpr int kRockRings = 16;
constexpr int kCrystalSides = 6;
constexpr int kPillarSides = 10;
constexpr float kPlaneSize = 12.0f;
constexpr float kContactShadowPlaneSize = 2.0f;

aster::MeshProcessOptions renderMeshOptions() {
  aster::MeshProcessOptions options;
  options.generate_tangents = false;
  return options;
}

} // namespace

namespace aster {

void RenderAssetCache::initializeBuiltins() {
  const MeshProcessOptions mesh_options = renderMeshOptions();
  box_ = prepareMeshForRendering(makeBox(), mesh_options);
  sphere_ =
      prepareMeshForRendering(makeUvSphere(kSphereSegments, kSphereRings, 1.0f), mesh_options);
  plane_ = prepareMeshForRendering(makePlane(kPlaneSize), mesh_options);
  contact_shadow_plane_ = prepareMeshForRendering(makePlane(kContactShadowPlaneSize), mesh_options);
  rock_ = prepareMeshForRendering(makeRock(kRockSegments, kRockRings, 1.0f), mesh_options);
  crystal_ = prepareMeshForRendering(makeCrystal(kCrystalSides, 1.0f, 1.8f), mesh_options);
  ruin_block_ = prepareMeshForRendering(makeRuinBlock(), mesh_options);
  pillar_ = prepareMeshForRendering(makePillar(kPillarSides, 1.0f, 1.0f), mesh_options);
}

const CpuMesh &RenderAssetCache::meshForPrimitive(const MeshPrimitive primitive) const {
  switch (primitive) {
  case MeshPrimitive::Box:
    return box_;
  case MeshPrimitive::Sphere:
    return sphere_;
  case MeshPrimitive::Plane:
    return plane_;
  case MeshPrimitive::Rock:
    return rock_;
  case MeshPrimitive::Crystal:
    return crystal_;
  case MeshPrimitive::RuinBlock:
    return ruin_block_;
  case MeshPrimitive::Pillar:
    return pillar_;
  }
  return sphere_;
}

const CpuMesh &RenderAssetCache::meshForObject(const RenderObject &object) {
  if (object.custom_mesh == nullptr) {
    return meshForPrimitive(object.primitive);
  }

  if (object.dynamic_mesh.valid()) {
    auto it = custom_mesh_resource_cache_.find(object.dynamic_mesh);
    if (it == custom_mesh_resource_cache_.end()) {
      it = custom_mesh_resource_cache_
               .emplace(object.dynamic_mesh,
                        prepareMeshForRendering(*object.custom_mesh, renderMeshOptions()))
               .first;
    }
    return it->second;
  }

  const CpuMesh *source = object.custom_mesh.get();
  auto it = custom_mesh_cache_.find(source);
  if (it == custom_mesh_cache_.end()) {
    it = custom_mesh_cache_.emplace(source, prepareMeshForRendering(*source, renderMeshOptions()))
             .first;
  }
  return it->second;
}

void RenderAssetCache::syncDynamicMeshes(const Scene &scene, rhi::ResourceRegistry &resource_registry,
                                         const bool immediate_eviction) {
  ASTER_PROFILE_SCOPE("RenderAssetCache::syncDynamicMeshes");
  if (immediate_eviction) {
    for (const auto &[mesh, handle] : custom_mesh_resource_handles_) {
      (void)mesh;
      resource_registry.destroy(handle);
    }
    for (const auto &[resource, handle] : dynamic_mesh_resource_handles_) {
      (void)resource;
      resource_registry.destroy(handle);
    }
    custom_mesh_cache_.clear();
    custom_mesh_last_seen_.clear();
    custom_mesh_resource_cache_.clear();
    custom_mesh_resource_last_seen_.clear();
    custom_mesh_resource_handles_.clear();
    dynamic_mesh_resource_handles_.clear();
    resource_registry.clearRetired();
  }

  ++mesh_cache_frame_;
  std::unordered_set<const CpuMesh *> live_meshes;
  std::unordered_set<DynamicMeshResourceKey, DynamicMeshResourceKeyHash> live_resources;
  live_meshes.reserve(scene.objects().size());
  live_resources.reserve(scene.objects().size());
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh == nullptr) {
      continue;
    }
    if (object.custom_mesh->vertices.empty() || object.custom_mesh->indices.empty()) {
      continue;
    }
    if (object.dynamic_mesh.valid()) {
      live_resources.insert(object.dynamic_mesh);
      custom_mesh_resource_last_seen_[object.dynamic_mesh] = mesh_cache_frame_;
    } else {
      live_meshes.insert(object.custom_mesh.get());
      custom_mesh_last_seen_[object.custom_mesh.get()] = mesh_cache_frame_;
    }
  }

  constexpr std::uint64_t kRetiredMeshGraceFrames = 3u;
  for (auto it = custom_mesh_cache_.begin(); it != custom_mesh_cache_.end();) {
    const auto seen = custom_mesh_last_seen_.find(it->first);
    const bool live = live_meshes.contains(it->first);
    const bool expired =
        immediate_eviction || seen == custom_mesh_last_seen_.end() ||
        mesh_cache_frame_ > seen->second + kRetiredMeshGraceFrames;
    if (live || !expired) {
      ++it;
    } else {
      if (const auto handle = custom_mesh_resource_handles_.find(it->first);
          handle != custom_mesh_resource_handles_.end()) {
        resource_registry.destroy(handle->second);
        custom_mesh_resource_handles_.erase(handle);
      }
      custom_mesh_last_seen_.erase(it->first);
      it = custom_mesh_cache_.erase(it);
    }
  }

  for (auto it = custom_mesh_resource_cache_.begin(); it != custom_mesh_resource_cache_.end();) {
    const auto seen = custom_mesh_resource_last_seen_.find(it->first);
    const bool live = live_resources.contains(it->first);
    const bool expired =
        immediate_eviction || seen == custom_mesh_resource_last_seen_.end() ||
        mesh_cache_frame_ > seen->second + kRetiredMeshGraceFrames;
    if (live || !expired) {
      ++it;
    } else {
      if (const auto handle = dynamic_mesh_resource_handles_.find(it->first);
          handle != dynamic_mesh_resource_handles_.end()) {
        resource_registry.destroy(handle->second);
        dynamic_mesh_resource_handles_.erase(handle);
      }
      custom_mesh_resource_last_seen_.erase(it->first);
      it = custom_mesh_resource_cache_.erase(it);
    }
  }

  for (const CpuMesh *mesh : live_meshes) {
    if (!custom_mesh_cache_.contains(mesh)) {
      custom_mesh_cache_.emplace(mesh, prepareMeshForRendering(*mesh, renderMeshOptions()));
    }
    if (!custom_mesh_resource_handles_.contains(mesh)) {
      const auto &prepared = custom_mesh_cache_.at(mesh);
      custom_mesh_resource_handles_.emplace(
          mesh, resource_registry.createBuffer(
                    {.byte_size = prepared.vertices.size() * sizeof(Vertex) +
                                  prepared.indices.size() * sizeof(std::uint32_t),
                     .usage = rhi::bufferUsageBit(rhi::BufferUsage::Vertex) |
                              rhi::bufferUsageBit(rhi::BufferUsage::Index),
                     .debug_label = "custom-mesh"}));
    }
  }
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh == nullptr || object.custom_mesh->vertices.empty() ||
        object.custom_mesh->indices.empty()) {
      continue;
    }
    if (object.dynamic_mesh.valid() &&
        !custom_mesh_resource_cache_.contains(object.dynamic_mesh)) {
      custom_mesh_resource_cache_.emplace(
          object.dynamic_mesh, prepareMeshForRendering(*object.custom_mesh, renderMeshOptions()));
    }
    if (object.dynamic_mesh.valid() &&
        !dynamic_mesh_resource_handles_.contains(object.dynamic_mesh)) {
      const auto &prepared = custom_mesh_resource_cache_.at(object.dynamic_mesh);
      dynamic_mesh_resource_handles_.emplace(
          object.dynamic_mesh,
          resource_registry.createBuffer(
              {.byte_size = prepared.vertices.size() * sizeof(Vertex) +
                            prepared.indices.size() * sizeof(std::uint32_t),
               .usage = rhi::bufferUsageBit(rhi::BufferUsage::Vertex) |
                        rhi::bufferUsageBit(rhi::BufferUsage::Index),
               .debug_label = "dynamic-mesh"}));
    }
  }
  resource_registry.clearRetired();
}

} // namespace aster
