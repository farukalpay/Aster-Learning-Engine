// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct SoftwarePreviewOptions {
  int width = 960;
  int height = 540;
  int samples_per_axis = 1;
  double frame_seconds = 0.0;
  RendererSettings settings{};
};

struct SoftwarePreviewProbePixel {
  std::uint32_t hit = 0u;
  float distance = 0.0f;
  Vec3 world_position{};
  Vec3 normal{};
  MaterialRenderRole render_role = MaterialRenderRole::Surface;
  RenderDepthLayer depth_layer = RenderDepthLayer::BaseSurface;
  std::uint64_t object_label_hash = 0u;
};

struct SoftwarePreviewProbeBuffer {
  int width = 0;
  int height = 0;
  std::vector<SoftwarePreviewProbePixel> pixels;
};

struct SoftwareLightingProbePixel {
  float direct_light_luminance = 0.0f;
  float volumetric_light_luminance = 0.0f;
  float emissive_luminance = 0.0f;
  float source_readability_luminance = 0.0f;
  float occlusion = 1.0f;
  float transmittance = 1.0f;
  std::uint64_t source_label_hash = 0u;
};

struct SoftwareLightingProbeBuffer {
  int width = 0;
  int height = 0;
  std::vector<SoftwareLightingProbePixel> pixels;
};

struct SoftwarePreviewResult {
  SoftwareFrameBuffer framebuffer;
  SoftwarePreviewProbeBuffer probe;
  SoftwareLightingProbeBuffer lighting;
};

[[nodiscard]] SoftwareFrameBuffer renderSoftwarePreview(const Scene &scene,
                                                        const OrbitCamera &camera,
                                                        const SoftwarePreviewOptions &options);

[[nodiscard]] SoftwarePreviewResult renderSoftwarePreviewWithProbe(
    const Scene &scene, const OrbitCamera &camera, const SoftwarePreviewOptions &options);

} // namespace aster
