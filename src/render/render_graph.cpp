// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_graph.hpp"

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

} // namespace

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
  case RenderGraphResource::LightClusters:
    return "light-clusters";
  case RenderGraphResource::ShadowAtlas:
    return "shadow-atlas";
  case RenderGraphResource::VolumetricFog:
    return "volumetric-fog";
  case RenderGraphResource::ReflectionProbes:
    return "reflection-probes";
  case RenderGraphResource::UiOverlay:
    return "ui-overlay";
  case RenderGraphResource::CaptureReadback:
    return "capture-readback";
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
  if (name == renderGraphResourceName(RenderGraphResource::LightClusters)) {
    return RenderGraphResource::LightClusters;
  }
  if (name == renderGraphResourceName(RenderGraphResource::ShadowAtlas)) {
    return RenderGraphResource::ShadowAtlas;
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
  return RenderGraphResource::SceneColor;
}

framegraph::FrameGraph makeDefaultFrameGraph(const bool ui_overlay_enabled,
                                             const bool capture_enabled) {
  framegraph::FrameGraph graph;
  const auto color = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::SceneColor)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Bgra8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::ColorAttachment) |
                rhi::imageUsageBit(rhi::ImageUsage::Sampled) |
                rhi::imageUsageBit(rhi::ImageUsage::TransferSource)});
  const auto depth = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::SceneDepth)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Depth32Float,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment)});
  const auto light_clusters = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::LightClusters)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Rgba8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled)});
  const auto shadow_atlas = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::ShadowAtlas)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Depth32Float,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::DepthAttachment) |
                rhi::imageUsageBit(rhi::ImageUsage::Sampled)});
  const auto volumetric_fog = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::VolumetricFog)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Rgba8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled)});
  const auto reflection_probes = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::ReflectionProbes)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Frame),
       .format = rhi::ImageFormat::Rgba8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled)});
  const auto ui = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::UiOverlay)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Imported),
       .format = rhi::ImageFormat::Bgra8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::Sampled)});
  const auto capture = graph.addResource(
      std::string(renderGraphResourceName(RenderGraphResource::CaptureReadback)),
      {.lifetime = lifetimeFor(RenderGraphResourceLifetime::Readback),
       .format = rhi::ImageFormat::Bgra8Unorm,
       .usage = rhi::imageUsageBit(rhi::ImageUsage::TransferDestination)});

  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::SceneColorDepth)))
      .writes(color)
      .writes(depth);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::LightCull)))
      .reads(depth)
      .writes(light_clusters);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::ShadowAtlas)))
      .reads(light_clusters)
      .writes(shadow_atlas);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::Opaque)))
      .reads(color)
      .reads(depth)
      .reads(light_clusters)
      .reads(shadow_atlas)
      .writes(color)
      .writes(depth);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::ContactShadow)))
      .reads(color)
      .reads(depth)
      .writes(color)
      .writes(depth);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::SceneLighting)))
      .reads(color)
      .reads(depth)
      .reads(light_clusters)
      .writes(color);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::VolumetricFog)))
      .reads(depth)
      .reads(light_clusters)
      .writes(volumetric_fog);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::ReflectionProbe)))
      .reads(color)
      .writes(reflection_probes);
  graph.addPass(std::string(renderGraphPassName(RenderGraphPass::Transparent)))
      .reads(color)
      .reads(depth)
      .reads(light_clusters)
      .reads(volumetric_fog)
      .reads(reflection_probes)
      .writes(color);
  if (ui_overlay_enabled) {
    graph.addPass(std::string(renderGraphPassName(RenderGraphPass::UiComposite)))
        .reads(color)
        .reads(ui)
        .writes(color);
  }
  if (capture_enabled) {
    graph.addPass(std::string(renderGraphPassName(RenderGraphPass::Capture)))
        .reads(color)
        .writes(capture);
  }
  return graph;
}

FixedRenderGraph makeFixedRenderGraph(const bool ui_overlay_enabled, const bool capture_enabled) {
  return framegraph::compileFrameGraph(makeDefaultFrameGraph(ui_overlay_enabled, capture_enabled));
}

} // namespace aster
