// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/render/render_scene.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aster {

struct RenderPipelineSignature {
  RenderSignatureKey key{};
  RenderMeshId mesh{};
  RenderMaterialKey material{};
  MaterialRenderQueue render_queue = MaterialRenderQueue::Opaque;
  RenderDepthPolicy depth_policy{};
  FrameRenderPass pass = FrameRenderPass::Opaque;
  std::string pipeline_tag;
  std::uint64_t stable_hash = 0u;
};

struct RenderPipelineSignatureCacheStats {
  std::size_t entries = 0u;
  std::size_t hits = 0u;
  std::size_t misses = 0u;
};

struct DescriptorBindingEvidence {
  std::string label;
  std::uint64_t layout_hash = 0u;
  std::size_t binding_count = 0u;
  std::size_t update_count = 0u;
};

struct MaterialResidencyEvidence {
  std::string object_name;
  std::string material_asset_id;
  std::string role;
  bool valid = false;
  bool bound = false;
  bool fallback = false;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t mip_count = 0u;
  std::uint64_t descriptor_layout_hash = 0u;
  std::string source_path;
  std::string backend_degradation;
};

struct DeferredGpuReleaseItem {
  std::string label;
  std::uint64_t resource_hash = 0u;
  std::uint64_t retire_after_frame = 0u;
};

struct SurfaceTruthGBuffer {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  bool surface_attributes = false;
  bool surface_occlusion = false;
  bool velocity = false;
  bool history = false;

  [[nodiscard]] bool valid() const noexcept {
    return width > 0u && height > 0u;
  }
};

struct DepthHierarchyLevel {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
};

struct DepthHierarchyPass {
  std::vector<DepthHierarchyLevel> levels;

  [[nodiscard]] bool valid() const noexcept {
    return !levels.empty() && levels.front().width > 0u && levels.front().height > 0u;
  }
};

struct RenderExtractionSet {
  CanonicalRenderIR ir;
  FrameRenderPlan plan;
  std::vector<RenderPipelineSignature> pipeline_signatures;
  std::uint64_t extraction_hash = 0u;
};

[[nodiscard]] inline std::uint64_t asterRenderExtractionHashAppend(
    std::uint64_t hash, const std::uint64_t value) noexcept {
  hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
  hash ^= hash >> 33u;
  hash *= 0xff51afd7ed558ccdull;
  hash ^= hash >> 33u;
  return hash;
}

[[nodiscard]] inline RenderPipelineSignature makeRenderPipelineSignature(
    const FrameRenderDrawGroup &group) {
  RenderPipelineSignature signature;
  signature.key = group.signature.key;
  signature.mesh = group.mesh;
  signature.material = group.material;
  signature.render_queue = group.render_queue;
  signature.depth_policy = group.signature.depth_policy;
  signature.pass = group.pass;
  signature.pipeline_tag = "aster.pipeline." + std::to_string(group.graph_pass_id);
  std::uint64_t hash = 1469598103934665603ull;
  hash = asterRenderExtractionHashAppend(hash, signature.key.value);
  hash = asterRenderExtractionHashAppend(hash, signature.mesh.value);
  hash = asterRenderExtractionHashAppend(hash, signature.material.value);
  hash = asterRenderExtractionHashAppend(
      hash, static_cast<std::uint64_t>(signature.render_queue));
  hash = asterRenderExtractionHashAppend(hash, static_cast<std::uint64_t>(signature.pass));
  hash = asterRenderExtractionHashAppend(
      hash, static_cast<std::uint64_t>(signature.depth_policy.layer));
  signature.stable_hash = hash;
  return signature;
}

class RenderPipelineSignatureCache {
public:
  [[nodiscard]] const RenderPipelineSignature &
  getOrInsert(RenderPipelineSignature signature) {
    if (const RenderPipelineSignature *existing = find(signature.key)) {
      ++hits_;
      return *existing;
    }
    ++misses_;
    signatures_.push_back(std::move(signature));
    return signatures_.back();
  }

  [[nodiscard]] const RenderPipelineSignature *find(RenderSignatureKey key) const {
    const auto found = std::find_if(
        signatures_.begin(), signatures_.end(),
        [key](const RenderPipelineSignature &signature) {
          return signature.key.value == key.value;
        });
    return found == signatures_.end() ? nullptr : &*found;
  }

  void clear() {
    signatures_.clear();
    hits_ = 0u;
    misses_ = 0u;
  }

  [[nodiscard]] const std::vector<RenderPipelineSignature> &signatures() const noexcept {
    return signatures_;
  }

  [[nodiscard]] RenderPipelineSignatureCacheStats stats() const noexcept {
    return {.entries = signatures_.size(), .hits = hits_, .misses = misses_};
  }

private:
  std::vector<RenderPipelineSignature> signatures_;
  std::size_t hits_ = 0u;
  std::size_t misses_ = 0u;
};

class DescriptorBindingCache {
public:
  void upsert(DescriptorBindingEvidence evidence) {
    if (DescriptorBindingEvidence *existing = findMutable(evidence.layout_hash)) {
      *existing = std::move(evidence);
      return;
    }
    bindings_.push_back(std::move(evidence));
  }

  [[nodiscard]] const DescriptorBindingEvidence *find(std::uint64_t layout_hash) const {
    const auto found = std::find_if(
        bindings_.begin(), bindings_.end(),
        [layout_hash](const DescriptorBindingEvidence &evidence) {
          return evidence.layout_hash == layout_hash;
        });
    return found == bindings_.end() ? nullptr : &*found;
  }

  [[nodiscard]] const std::vector<DescriptorBindingEvidence> &bindings() const noexcept {
    return bindings_;
  }

private:
  [[nodiscard]] DescriptorBindingEvidence *findMutable(std::uint64_t layout_hash) {
    const auto found = std::find_if(
        bindings_.begin(), bindings_.end(),
        [layout_hash](const DescriptorBindingEvidence &evidence) {
          return evidence.layout_hash == layout_hash;
        });
    return found == bindings_.end() ? nullptr : &*found;
  }

  std::vector<DescriptorBindingEvidence> bindings_;
};

class MaterialResidencyCache {
public:
  void upsert(MaterialResidencyEvidence evidence) {
    if (MaterialResidencyEvidence *existing =
            findMutable(evidence.object_name, evidence.role)) {
      *existing = std::move(evidence);
      return;
    }
    records_.push_back(std::move(evidence));
  }

  [[nodiscard]] const std::vector<MaterialResidencyEvidence> &records() const noexcept {
    return records_;
  }

  [[nodiscard]] std::size_t residentCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(records_.begin(), records_.end(),
                      [](const MaterialResidencyEvidence &evidence) {
                        return evidence.valid && evidence.bound && !evidence.fallback;
                      }));
  }

  [[nodiscard]] std::size_t fallbackCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(records_.begin(), records_.end(),
                      [](const MaterialResidencyEvidence &evidence) {
                        return evidence.fallback;
                      }));
  }

private:
  [[nodiscard]] MaterialResidencyEvidence *findMutable(std::string_view object_name,
                                                       std::string_view role) {
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [object_name, role](const MaterialResidencyEvidence &evidence) {
          return evidence.object_name == object_name && evidence.role == role;
        });
    return found == records_.end() ? nullptr : &*found;
  }

  std::vector<MaterialResidencyEvidence> records_;
};

class DeferredGpuReleaseQueue {
public:
  void enqueue(DeferredGpuReleaseItem item) {
    pending_.push_back(std::move(item));
  }

  [[nodiscard]] std::vector<DeferredGpuReleaseItem> retireCompleted(
      std::uint64_t completed_frame) {
    std::vector<DeferredGpuReleaseItem> retired;
    auto cursor = pending_.begin();
    while (cursor != pending_.end()) {
      if (cursor->retire_after_frame <= completed_frame) {
        retired.push_back(std::move(*cursor));
        cursor = pending_.erase(cursor);
      } else {
        ++cursor;
      }
    }
    retired_total_ += retired.size();
    return retired;
  }

  [[nodiscard]] std::size_t pendingCount() const noexcept {
    return pending_.size();
  }

  [[nodiscard]] std::size_t retiredCount() const noexcept {
    return retired_total_;
  }

private:
  std::vector<DeferredGpuReleaseItem> pending_;
  std::size_t retired_total_ = 0u;
};

[[nodiscard]] inline DepthHierarchyPass makeDepthHierarchyPass(
    std::uint32_t width, std::uint32_t height) {
  DepthHierarchyPass pass;
  while (width > 0u && height > 0u) {
    pass.levels.push_back({.width = width, .height = height});
    if (width == 1u && height == 1u) {
      break;
    }
    width = std::max(width / 2u, 1u);
    height = std::max(height / 2u, 1u);
  }
  return pass;
}

[[nodiscard]] inline RenderExtractionSet buildRenderExtractionSet(
    const Scene &scene, const OrbitCamera &camera, const LineOfSightFadeSettings &fade,
    const int framebuffer_width, const int framebuffer_height) {
  RenderScene render_scene;
  render_scene.rebuild(scene);

  RenderExtractionSet extraction;
  extraction.ir = render_scene.ir();
  extraction.plan = buildFrameRenderPlan(render_scene, camera, fade, framebuffer_width,
                                         framebuffer_height);
  extraction.pipeline_signatures.reserve(extraction.plan.groups.size());

  std::uint64_t hash = asterRenderExtractionHashAppend(
      1469598103934665603ull, extraction.ir.content_hash);
  hash = asterRenderExtractionHashAppend(hash, extraction.plan.source_ir_hash);
  hash = asterRenderExtractionHashAppend(hash, extraction.plan.diagnostics.visible_objects);
  for (const FrameRenderDrawGroup &group : extraction.plan.groups) {
    RenderPipelineSignature signature = makeRenderPipelineSignature(group);
    hash = asterRenderExtractionHashAppend(hash, signature.stable_hash);
    extraction.pipeline_signatures.push_back(std::move(signature));
  }
  extraction.extraction_hash = hash;
  return extraction;
}

} // namespace aster
