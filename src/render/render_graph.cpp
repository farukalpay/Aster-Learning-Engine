// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/render_graph.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] framegraph::ResourceLifetime lifetimeFor(const RenderGraphResourceLifetime lifetime) {
  switch (lifetime) {
  case RenderGraphResourceLifetime::Frame:
    return framegraph::ResourceLifetime::Transient;
  case RenderGraphResourceLifetime::Imported:
    return framegraph::ResourceLifetime::Imported;
  case RenderGraphResourceLifetime::Readback:
    return framegraph::ResourceLifetime::Readback;
  }
  return framegraph::ResourceLifetime::Transient;
}

[[nodiscard]] RenderGraphResourceBindingDesc readBinding(const RenderGraphResource resource,
                                                         const rhi::ResourceState state =
                                                             rhi::ResourceState::ShaderRead) {
  return {.resource = resource, .state = state};
}

[[nodiscard]] RenderGraphResourceBindingDesc writeBinding(
    const RenderGraphResource resource, const rhi::ResourceState state,
    const rhi::AttachmentLoadOp load_op = rhi::AttachmentLoadOp::DontCare,
    const rhi::AttachmentStoreOp store_op = rhi::AttachmentStoreOp::Store) {
  return {.resource = resource, .state = state, .load_op = load_op, .store_op = store_op};
}

} // namespace

RenderPassRegistry &RenderPassRegistry::add(RenderGraphPassDeclaration declaration) {
  passes_.push_back(std::move(declaration));
  return *this;
}

const RenderGraphPassDeclaration *RenderPassRegistry::find(const RenderGraphPass pass) const {
  const auto found = std::find_if(passes_.begin(), passes_.end(),
                                  [pass](const RenderGraphPassDeclaration &declaration) {
                                    return declaration.pass == pass;
                                  });
  return found == passes_.end() ? nullptr : &*found;
}

const RenderGraphPassDeclaration *RenderPassRegistry::find(const std::string_view name) const {
  const auto found = std::find_if(passes_.begin(), passes_.end(),
                                  [name](const RenderGraphPassDeclaration &declaration) {
                                    return declaration.name == name;
                                  });
  return found == passes_.end() ? nullptr : &*found;
}

std::vector<std::string> RenderPassRegistry::validate() const {
  std::vector<std::string> errors;
  std::unordered_set<std::uint32_t> seen_passes;
  std::unordered_set<std::string> seen_names;
  for (const RenderGraphPassDeclaration &pass : passes_) {
    if (pass.name.empty()) {
      errors.push_back("render pass declaration has an empty name");
    }
    const std::uint32_t pass_id = static_cast<std::uint32_t>(pass.pass);
    if (!seen_passes.insert(pass_id).second) {
      errors.push_back("render pass declaration repeats pass id " + std::to_string(pass_id));
    }
    if (!seen_names.insert(pass.name).second) {
      errors.push_back("render pass declaration repeats pass name '" + pass.name + "'");
    }
    if (pass.inputs.empty() && pass.outputs.empty()) {
      errors.push_back("render pass '" + pass.name + "' declares no resource usage");
    }
  }
  return errors;
}

std::string_view renderGraphPassName(const RenderGraphPass pass) {
  switch (pass) {
  case RenderGraphPass::SceneColorDepth:
    return "scene-color-depth";
  case RenderGraphPass::LightCull:
    return "light-cull";
  case RenderGraphPass::ShadowAtlas:
    return "shadow-atlas";
  case RenderGraphPass::Opaque:
    return "opaque";
  case RenderGraphPass::ContactShadow:
    return "contact-shadow";
  case RenderGraphPass::SurfaceOcclusion:
    return "surface-occlusion";
  case RenderGraphPass::SceneLighting:
    return "scene-lighting";
  case RenderGraphPass::VolumetricFog:
    return "volumetric-fog";
  case RenderGraphPass::ReflectionProbe:
    return "reflection-probe";
  case RenderGraphPass::Transparent:
    return "transparent";
  case RenderGraphPass::UiComposite:
    return "ui-composite";
  case RenderGraphPass::Capture:
    return "capture";
  }
  return "unknown";
}

std::string_view renderGraphResourceName(const RenderGraphResource resource) {
  switch (resource) {
  case RenderGraphResource::SceneColor:
    return "scene-color";
  case RenderGraphResource::SceneDepth:
    return "scene-depth";
  case RenderGraphResource::SurfaceAttributes:
    return "surface-attributes";
  case RenderGraphResource::LightClusters:
    return "light-clusters";
  case RenderGraphResource::ShadowAtlas:
    return "shadow-atlas";
  case RenderGraphResource::SurfaceOcclusion:
    return "surface-occlusion";
  case RenderGraphResource::VolumetricFog:
    return "volumetric-fog";
  case RenderGraphResource::ReflectionProbes:
    return "reflection-probes";
  case RenderGraphResource::UiOverlay:
    return "ui-overlay";
  case RenderGraphResource::CaptureReadback:
    return "capture-readback";
  case RenderGraphResource::SurfaceTruthBaseColor:
    return "surface-truth-base-color";
  case RenderGraphResource::SurfaceTruthNormal:
    return "surface-truth-normal";
  case RenderGraphResource::SurfaceTruthMaterial:
    return "surface-truth-material";
  case RenderGraphResource::SurfaceTruthVelocity:
    return "surface-truth-velocity";
  case RenderGraphResource::SurfaceHistory:
    return "surface-history";
  case RenderGraphResource::DepthHierarchy:
    return "depth-hierarchy";
  case RenderGraphResource::BloomChain:
    return "bloom-chain";
  case RenderGraphResource::TemporalAaHistory:
    return "temporal-aa-history";
  case RenderGraphResource::ExposureHistogram:
    return "exposure-histogram";
  case RenderGraphResource::ToneMapInput:
    return "tonemap-input";
  }
  return "unknown";
}

std::string_view renderGraphResourceLifetimeName(const RenderGraphResourceLifetime lifetime) {
  switch (lifetime) {
  case RenderGraphResourceLifetime::Frame:
    return "frame";
  case RenderGraphResourceLifetime::Imported:
    return "imported";
  case RenderGraphResourceLifetime::Readback:
    return "readback";
  }
  return "unknown";
}

RenderGraphPass renderGraphPassFromName(const std::string_view name) {
  if (name == renderGraphPassName(RenderGraphPass::LightCull)) {
    return RenderGraphPass::LightCull;
  }
  if (name == renderGraphPassName(RenderGraphPass::ShadowAtlas)) {
    return RenderGraphPass::ShadowAtlas;
  }
  if (name == renderGraphPassName(RenderGraphPass::Opaque)) {
    return RenderGraphPass::Opaque;
  }
  if (name == renderGraphPassName(RenderGraphPass::ContactShadow)) {
    return RenderGraphPass::ContactShadow;
  }
  if (name == renderGraphPassName(RenderGraphPass::SurfaceOcclusion)) {
    return RenderGraphPass::SurfaceOcclusion;
  }
  if (name == renderGraphPassName(RenderGraphPass::SceneLighting)) {
    return RenderGraphPass::SceneLighting;
  }
  if (name == renderGraphPassName(RenderGraphPass::VolumetricFog)) {
    return RenderGraphPass::VolumetricFog;
  }
  if (name == renderGraphPassName(RenderGraphPass::ReflectionProbe)) {
    return RenderGraphPass::ReflectionProbe;
  }
  if (name == renderGraphPassName(RenderGraphPass::Transparent)) {
    return RenderGraphPass::Transparent;
  }
  if (name == renderGraphPassName(RenderGraphPass::UiComposite)) {
    return RenderGraphPass::UiComposite;
  }
  if (name == renderGraphPassName(RenderGraphPass::Capture)) {
    return RenderGraphPass::Capture;
  }
  return RenderGraphPass::SceneColorDepth;
}

RenderGraphResource renderGraphResourceFromName(const std::string_view name) {
  if (name == renderGraphResourceName(RenderGraphResource::SceneDepth)) {
    return RenderGraphResource::SceneDepth;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceAttributes)) {
    return RenderGraphResource::SurfaceAttributes;
  }
  if (name == renderGraphResourceName(RenderGraphResource::LightClusters)) {
    return RenderGraphResource::LightClusters;
  }
  if (name == renderGraphResourceName(RenderGraphResource::ShadowAtlas)) {
    return RenderGraphResource::ShadowAtlas;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceOcclusion)) {
    return RenderGraphResource::SurfaceOcclusion;
  }
  if (name == renderGraphResourceName(RenderGraphResource::VolumetricFog)) {
    return RenderGraphResource::VolumetricFog;
  }
  if (name == renderGraphResourceName(RenderGraphResource::ReflectionProbes)) {
    return RenderGraphResource::ReflectionProbes;
  }
  if (name == renderGraphResourceName(RenderGraphResource::UiOverlay)) {
    return RenderGraphResource::UiOverlay;
  }
  if (name == renderGraphResourceName(RenderGraphResource::CaptureReadback)) {
    return RenderGraphResource::CaptureReadback;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceTruthBaseColor)) {
    return RenderGraphResource::SurfaceTruthBaseColor;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceTruthNormal)) {
    return RenderGraphResource::SurfaceTruthNormal;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceTruthMaterial)) {
    return RenderGraphResource::SurfaceTruthMaterial;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceTruthVelocity)) {
    return RenderGraphResource::SurfaceTruthVelocity;
  }
  if (name == renderGraphResourceName(RenderGraphResource::SurfaceHistory)) {
    return RenderGraphResource::SurfaceHistory;
  }
  if (name == renderGraphResourceName(RenderGraphResource::DepthHierarchy)) {
    return RenderGraphResource::DepthHierarchy;
  }
  if (name == renderGraphResourceName(RenderGraphResource::BloomChain)) {
    return RenderGraphResource::BloomChain;
  }
  if (name == renderGraphResourceName(RenderGraphResource::TemporalAaHistory)) {
    return RenderGraphResource::TemporalAaHistory;
  }
  if (name == renderGraphResourceName(RenderGraphResource::ExposureHistogram)) {
    return RenderGraphResource::ExposureHistogram;
  }
  if (name == renderGraphResourceName(RenderGraphResource::ToneMapInput)) {
    return RenderGraphResource::ToneMapInput;
  }
  return RenderGraphResource::SceneColor;
}

std::string_view renderGraphExecutorKeyName(const RenderGraphExecutorKey key) {
  switch (key) {
  case RenderGraphExecutorKey::None:
    return "none";
  case RenderGraphExecutorKey::ClearScene:
    return "clear-scene";
  case RenderGraphExecutorKey::LightCull:
    return "light-cull";
  case RenderGraphExecutorKey::ShadowAtlas:
    return "shadow-atlas";
  case RenderGraphExecutorKey::Opaque:
    return "opaque";
  case RenderGraphExecutorKey::ContactShadow:
    return "contact-shadow";
  case RenderGraphExecutorKey::SurfaceOcclusion:
    return "surface-occlusion";
  case RenderGraphExecutorKey::SceneLighting:
    return "scene-lighting";
  case RenderGraphExecutorKey::VolumetricFog:
    return "volumetric-fog";
  case RenderGraphExecutorKey::ReflectionProbe:
    return "reflection-probe";
  case RenderGraphExecutorKey::Transparent:
    return "transparent";
  case RenderGraphExecutorKey::UiComposite:
    return "ui-composite";
  case RenderGraphExecutorKey::Capture:
    return "capture";
  }
  return "none";
}

std::string_view
renderGraphDebugCapturePolicyName(const RenderGraphDebugCapturePolicy policy) {
  switch (policy) {
  case RenderGraphDebugCapturePolicy::Disabled:
    return "disabled";
  case RenderGraphDebugCapturePolicy::OnRequest:
    return "on-request";
  case RenderGraphDebugCapturePolicy::Always:
    return "always";
  }
  return "disabled";
}

framegraph::ResourceDesc defaultRenderGraphResourceDesc(const RenderGraphResource resource) {
  switch (resource) {
	  case RenderGraphResource::SceneColor:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Rgba16Float,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
	                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
	  case RenderGraphResource::SceneDepth:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Depth32Float,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Sampled)};
	  case RenderGraphResource::SurfaceAttributes:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Rgba8Unorm,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Storage) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::LightClusters:
    return {.kind = framegraph::ResourceKind::Buffer,
            .lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .usage = rhi::bufferUsageBit(rhi::BufferUsage::Storage),
            .byte_size = 64u * 1024u,
            .stride = 16u};
	  case RenderGraphResource::ShadowAtlas:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Depth32Float,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Sampled)};
	  case RenderGraphResource::SurfaceOcclusion:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::R8Unorm,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Storage) |
	                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
	  case RenderGraphResource::VolumetricFog:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Rgba8Unorm,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
	                     rhi::imageUsageBit(rhi::ImageUsage::Storage)};
	  case RenderGraphResource::ReflectionProbes:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
	            .format = rhi::ImageFormat::Rgba8Unorm,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
	                     rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment)};
  case RenderGraphResource::UiOverlay:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Imported),
            .format = rhi::ImageFormat::Bgra8Unorm,
            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled)};
	  case RenderGraphResource::CaptureReadback:
	    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Readback),
	            .format = rhi::ImageFormat::Bgra8Unorm,
	            .extent = {.width = 0u, .height = 0u, .depth = 1u},
	            .usage = rhi::imageUsageBit(rhi::ImageUsage::TransferDestination)};
  case RenderGraphResource::SurfaceTruthBaseColor:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rgba8Unorm,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::SurfaceTruthNormal:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rgba16Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::SurfaceTruthMaterial:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rgba8Unorm,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::SurfaceTruthVelocity:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rg8Unorm,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::SurfaceHistory:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Imported),
            .format = rhi::ImageFormat::Rgba16Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::DepthHierarchy:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Depth32Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::BloomChain:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rgba16Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::TemporalAaHistory:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Imported),
            .format = rhi::ImageFormat::Rgba16Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  case RenderGraphResource::ExposureHistogram:
    return {.kind = framegraph::ResourceKind::Buffer,
            .lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .usage = rhi::bufferUsageBit(rhi::BufferUsage::Storage) |
                     rhi::bufferUsageBit(rhi::BufferUsage::Readback),
            .byte_size = 256u * sizeof(std::uint32_t),
            .stride = sizeof(std::uint32_t)};
  case RenderGraphResource::ToneMapInput:
    return {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
            .format = rhi::ImageFormat::Rgba16Float,
            .extent = {.width = 0u, .height = 0u, .depth = 1u},
            .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                     rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                     rhi::imageUsageBit(rhi::ImageUsage::TransferSource)};
  }
  return {};
}

RenderPassRegistry makeDefaultRenderPassRegistry(const bool ui_overlay_enabled,
                                                 const bool capture_enabled) {
  RenderPassRegistry registry;
  registry.add({.pass = RenderGraphPass::SceneColorDepth,
                .name = std::string(renderGraphPassName(RenderGraphPass::SceneColorDepth)),
                .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Clear),
                            writeBinding(RenderGraphResource::SceneDepth,
                                         rhi::ResourceState::DepthAttachment,
                                         rhi::AttachmentLoadOp::Clear)},
                .debug_capture = RenderGraphDebugCapturePolicy::OnRequest,
                .executor = RenderGraphExecutorKey::ClearScene,
                .produces_backend_work = true});
  registry.add({.pass = RenderGraphPass::LightCull,
                .name = std::string(renderGraphPassName(RenderGraphPass::LightCull)),
                .inputs = {readBinding(RenderGraphResource::SceneDepth)},
                .outputs = {writeBinding(RenderGraphResource::LightClusters,
                                         rhi::ResourceState::ShaderWrite)},
                .debug_capture = RenderGraphDebugCapturePolicy::OnRequest,
                .executor = RenderGraphExecutorKey::LightCull,
                .produces_backend_work = true});
  registry.add({.pass = RenderGraphPass::ShadowAtlas,
                .name = std::string(renderGraphPassName(RenderGraphPass::ShadowAtlas)),
                .inputs = {readBinding(RenderGraphResource::LightClusters)},
                .outputs = {writeBinding(RenderGraphResource::ShadowAtlas,
                                         rhi::ResourceState::DepthAttachment,
                                         rhi::AttachmentLoadOp::Clear)},
                .debug_capture = RenderGraphDebugCapturePolicy::OnRequest,
                .executor = RenderGraphExecutorKey::ShadowAtlas,
                .produces_backend_work = false});
  registry.add({.pass = RenderGraphPass::Opaque,
                .name = std::string(renderGraphPassName(RenderGraphPass::Opaque)),
                .inputs = {readBinding(RenderGraphResource::SceneColor),
                           readBinding(RenderGraphResource::SceneDepth),
                           readBinding(RenderGraphResource::LightClusters),
                           readBinding(RenderGraphResource::ShadowAtlas)},
                .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Load),
                            writeBinding(RenderGraphResource::SceneDepth,
                                         rhi::ResourceState::DepthAttachment,
                                         rhi::AttachmentLoadOp::Load),
                            writeBinding(RenderGraphResource::SurfaceAttributes,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Clear)},
                .executor = RenderGraphExecutorKey::Opaque,
                .produces_backend_work = true});
  registry.add({.pass = RenderGraphPass::ContactShadow,
                .name = std::string(renderGraphPassName(RenderGraphPass::ContactShadow)),
                .inputs = {readBinding(RenderGraphResource::SceneColor),
                           readBinding(RenderGraphResource::SceneDepth)},
                .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Load),
                            writeBinding(RenderGraphResource::SceneDepth,
                                         rhi::ResourceState::DepthAttachment,
                                         rhi::AttachmentLoadOp::Load)},
                .executor = RenderGraphExecutorKey::ContactShadow,
                .produces_backend_work = true});
  registry.add({.pass = RenderGraphPass::SurfaceOcclusion,
                .name = std::string(renderGraphPassName(RenderGraphPass::SurfaceOcclusion)),
                .inputs = {readBinding(RenderGraphResource::SceneDepth),
                           readBinding(RenderGraphResource::SurfaceAttributes)},
                .outputs = {writeBinding(RenderGraphResource::SurfaceOcclusion,
                                         rhi::ResourceState::ShaderWrite)},
                .debug_capture = RenderGraphDebugCapturePolicy::OnRequest,
                .executor = RenderGraphExecutorKey::SurfaceOcclusion,
                .produces_backend_work = false});
  registry.add({.pass = RenderGraphPass::VolumetricFog,
                .name = std::string(renderGraphPassName(RenderGraphPass::VolumetricFog)),
                .inputs = {readBinding(RenderGraphResource::SceneDepth),
                           readBinding(RenderGraphResource::LightClusters)},
                .outputs = {writeBinding(RenderGraphResource::VolumetricFog,
                                         rhi::ResourceState::ShaderWrite)},
                .executor = RenderGraphExecutorKey::VolumetricFog,
                .produces_backend_work = false});
  registry.add({.pass = RenderGraphPass::ReflectionProbe,
                .name = std::string(renderGraphPassName(RenderGraphPass::ReflectionProbe)),
                .inputs = {readBinding(RenderGraphResource::SceneColor)},
                .outputs = {writeBinding(RenderGraphResource::ReflectionProbes,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Clear)},
                .executor = RenderGraphExecutorKey::ReflectionProbe,
                .produces_backend_work = false});
  registry.add({.pass = RenderGraphPass::SceneLighting,
                .name = std::string(renderGraphPassName(RenderGraphPass::SceneLighting)),
                .inputs = {readBinding(RenderGraphResource::SceneColor),
                           readBinding(RenderGraphResource::SceneDepth),
                           readBinding(RenderGraphResource::LightClusters),
                           readBinding(RenderGraphResource::ShadowAtlas),
                           readBinding(RenderGraphResource::SurfaceOcclusion),
                           readBinding(RenderGraphResource::VolumetricFog),
                           readBinding(RenderGraphResource::ReflectionProbes)},
                .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Load)},
                .executor = RenderGraphExecutorKey::SceneLighting,
                .produces_backend_work = true});
  registry.add({.pass = RenderGraphPass::Transparent,
                .name = std::string(renderGraphPassName(RenderGraphPass::Transparent)),
                .inputs = {readBinding(RenderGraphResource::SceneColor),
                           readBinding(RenderGraphResource::SceneDepth),
                           readBinding(RenderGraphResource::LightClusters),
                           readBinding(RenderGraphResource::SurfaceOcclusion),
                           readBinding(RenderGraphResource::VolumetricFog),
                           readBinding(RenderGraphResource::ReflectionProbes)},
                .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::ColorAttachment,
                                         rhi::AttachmentLoadOp::Load)},
                .executor = RenderGraphExecutorKey::Transparent,
                .produces_backend_work = true});
  if (ui_overlay_enabled) {
    registry.add({.pass = RenderGraphPass::UiComposite,
                  .name = std::string(renderGraphPassName(RenderGraphPass::UiComposite)),
                  .inputs = {readBinding(RenderGraphResource::SceneColor),
                             readBinding(RenderGraphResource::UiOverlay)},
                  .outputs = {writeBinding(RenderGraphResource::SceneColor,
                                           rhi::ResourceState::ColorAttachment,
                                           rhi::AttachmentLoadOp::Load)},
                  .executor = RenderGraphExecutorKey::UiComposite,
                  .produces_backend_work = true});
  }
  if (capture_enabled) {
    registry.add({.pass = RenderGraphPass::Capture,
                  .name = std::string(renderGraphPassName(RenderGraphPass::Capture)),
                  .inputs = {readBinding(RenderGraphResource::SceneColor,
                                         rhi::ResourceState::CopySource)},
                  .outputs = {writeBinding(RenderGraphResource::CaptureReadback,
                                           rhi::ResourceState::CopyDestination,
                                           rhi::AttachmentLoadOp::DontCare)},
                  .debug_capture = RenderGraphDebugCapturePolicy::Always,
                  .executor = RenderGraphExecutorKey::Capture,
                  .produces_backend_work = true});
  }
  return registry;
}

const RenderGraphPassDeclaration *defaultRenderPassDeclaration(const RenderGraphPass pass) {
  static const RenderPassRegistry registry = makeDefaultRenderPassRegistry(true, true);
  return registry.find(pass);
}

framegraph::FrameGraph makeDefaultFrameGraph(const bool ui_overlay_enabled,
                                             const bool capture_enabled) {
  framegraph::FrameGraph graph;
  const RenderPassRegistry registry =
      makeDefaultRenderPassRegistry(ui_overlay_enabled, capture_enabled);
  std::vector<framegraph::ResourceHandle> handles;
  handles.resize(kRenderGraphResourceCount);
  for (std::uint32_t i = 0u; i < static_cast<std::uint32_t>(handles.size()); ++i) {
    const auto resource = static_cast<RenderGraphResource>(i);
    handles[i] = graph.addResource(std::string(renderGraphResourceName(resource)),
                                   defaultRenderGraphResourceDesc(resource));
  }
  const auto handle_for = [&handles](const RenderGraphResource resource) {
    return handles[static_cast<std::uint32_t>(resource)];
  };
  for (const RenderGraphPassDeclaration &declaration : registry.passes()) {
    auto pass_builder = graph.addPass(declaration.name).queue(declaration.queue);
    for (const RenderGraphResourceBindingDesc &input : declaration.inputs) {
      pass_builder.reads(handle_for(input.resource));
    }
    for (const RenderGraphResourceBindingDesc &output : declaration.outputs) {
      pass_builder.writes(handle_for(output.resource));
    }
  }
  return graph;
}

framegraph::FrameGraphCompileOptions frameGraphCompileOptionsForIntent(const FrameIntent &intent) {
  return {.frame_extent = intent.frame_extent,
          .backend_resource_mask = intent.backend_resource_mask,
          .required_resource_mask = intent.required_resource_mask,
          .cull_unsupported_passes = intent.cull_unsupported_passes,
          .assign_physical_allocations = true};
}

FixedRenderGraph makeFixedRenderGraph(const bool ui_overlay_enabled, const bool capture_enabled) {
  return framegraph::compileFrameGraph(makeDefaultFrameGraph(ui_overlay_enabled, capture_enabled));
}

FixedRenderGraph makeFixedRenderGraph(const framegraph::FrameGraphCompileOptions &options,
                                      const bool ui_overlay_enabled,
                                      const bool capture_enabled) {
  return framegraph::compileFrameGraph(makeDefaultFrameGraph(ui_overlay_enabled, capture_enabled),
                                       options);
}

FixedRenderGraph compileFrameIntent(const FrameIntent &intent) {
  return makeFixedRenderGraph(frameGraphCompileOptionsForIntent(intent),
                              intent.ui_overlay_enabled, intent.capture_enabled);
}

FrameIntentCompileResult compileFrameIntentWithReport(const FrameIntent &intent) {
  FrameIntentCompileResult result;
  result.intent = intent;
  result.graph = compileFrameIntent(intent);
  result.report = framegraph::renderGraphCompilerReport(result.graph);
  return result;
}

} // namespace aster
