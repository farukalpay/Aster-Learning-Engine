// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/frame_gate.hpp"

#include "aster/render/render_device.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aster::graphics_core7 {
namespace {

[[nodiscard]] bool hasPass(const FixedRenderGraph &graph, const RenderGraphPass pass) {
  const std::string_view name = renderGraphPassName(pass);
  return std::any_of(graph.passes.begin(), graph.passes.end(),
                     [name](const framegraph::CompiledPass &compiled) {
                       return compiled.name == name || compiled.event_label == name;
                     });
}

[[nodiscard]] bool hasResource(const FixedRenderGraph &graph, const RenderGraphResource resource) {
  const std::string_view name = renderGraphResourceName(resource);
  return std::any_of(graph.resources.begin(), graph.resources.end(),
                     [name](const framegraph::CompiledResource &compiled) {
                       return compiled.name == name;
                     });
}

[[nodiscard]] bool backendAdvertises(const RenderBackendCapabilities &capabilities,
                                     const RenderGraphResource resource) {
  return (capabilities.graph_resource_mask & renderGraphResourceBit(resource)) != 0u;
}

[[nodiscard]] bool nativeGpu(const RenderBackendCapabilities &capabilities) {
  return capabilities.gpu && capabilities.kind != RenderBackendKind::SoftwareReference &&
         capabilities.kind != RenderBackendKind::Null;
}

[[nodiscard]] bool requestLightTransport(const RendererSettings &settings) {
  return settings.sun_light.enabled || !settings.light_rig.empty() ||
         settings.clustered_lighting.enabled ||
         (settings.atmosphere.enabled && settings.atmosphere.fog_strength > 0.0f);
}

[[nodiscard]] bool requestShadow(const RendererSettings &settings) {
  return settings.shadows.enabled && settings.shadows.directional_cascades > 0u;
}

[[nodiscard]] bool requestProbe(const RendererSettings &settings) {
  return settings.reflections.enabled && settings.reflections.static_local_probes;
}

[[nodiscard]] std::uint32_t defaultRequiredMask(const RendererSettings &settings) {
  std::uint32_t mask = signalBit(Signal::SurfaceTruthBuffer) |
                       signalBit(Signal::MaterialFrequencyAudit) |
                       signalBit(Signal::TemporalStabilityAudit) |
                       signalBit(Signal::BackendVisualDelta) |
                       signalBit(Signal::PlayerReadableFrameVerdict);
  if (requestLightTransport(settings)) {
    mask |= signalBit(Signal::LightTransportEvidence);
  }
  if (requestShadow(settings)) {
    mask |= signalBit(Signal::ShadowContinuityField);
  }
  if (requestProbe(settings)) {
    mask |= signalBit(Signal::ReflectionProbeResidency);
  }
  return mask;
}

[[nodiscard]] bool required(const std::uint32_t mask, const Signal signal) {
  return (mask & signalBit(signal)) != 0u;
}

void reject(std::vector<SignalEvidence> &signals, SignalEvidence signal) {
  signal.required = true;
  signal.score = 0.0f;
  signal.evidence_hash = signalEvidenceHash(signal);
  signals.push_back(std::move(signal));
}

[[nodiscard]] std::string diagnosticFor(const std::vector<SignalEvidence> &signals) {
  std::ostringstream out;
  out << "graphics_core7 preflight blocked by ";
  for (std::size_t i = 0u; i < signals.size(); ++i) {
    if (i != 0u) {
      out << ", ";
    }
    out << signalName(signals[i].signal) << ":" << signalStatusName(signals[i].status);
  }
  return out.str();
}

} // namespace

FrameGateReport preflightFrameTruth(const FixedRenderGraph &graph,
                                    const RenderBackendCapabilities &capabilities,
                                    const RendererSettings &settings, const FrameStats &stats,
                                    const FrameForensics &forensics) {
  (void)stats;
  FrameGateReport report;
  const Settings gc7 = settings.graphics_core7;
  const std::uint32_t required_mask =
      gc7.required_signal_mask != 0u ? gc7.required_signal_mask : defaultRequiredMask(settings);
  const bool strict = gc7.strict();
  const bool require_native = (gc7.flags & RequireNativeBackend) != 0u;
  if (!strict) {
    report.verdict = {.status = VerdictStatus::Accepted,
                      .accepted = true,
                      .strict = false,
                      .score = 1.0f,
                      .minimum_score = gc7.minimum_score,
                      .required_signal_mask = required_mask,
                      .diagnostic = "graphics_core7 preflight compatibility pass"};
    report.verdict.evidence_hash = verdictHash(report.verdict, report.rejected_signals);
    return report;
  }

  if (required(required_mask, Signal::SurfaceTruthBuffer) &&
      !(hasResource(graph, RenderGraphResource::SurfaceAttributes) ||
        hasResource(graph, RenderGraphResource::CaptureReadback))) {
    reject(report.rejected_signals,
           {.signal = Signal::SurfaceTruthBuffer,
            .status = SignalStatus::MissingProof,
            .pass = RenderGraphPass::Opaque,
            .resource = RenderGraphResource::SurfaceAttributes,
            .threshold = 1.0f,
            .label = "preflight surface truth buffer",
            .evidence = "compiled graph lacks surface/final truth resources",
            .message = "GraphicsCore7 cannot begin a strict frame without surface truth resources."});
  }

  if (required(required_mask, Signal::LightTransportEvidence) && requestLightTransport(settings) &&
      !(hasPass(graph, RenderGraphPass::LightCull) &&
        hasResource(graph, RenderGraphResource::LightClusters) &&
        backendAdvertises(capabilities, RenderGraphResource::LightClusters))) {
    reject(report.rejected_signals,
           {.signal = Signal::LightTransportEvidence,
            .status = SignalStatus::MissingProof,
            .pass = RenderGraphPass::LightCull,
            .resource = RenderGraphResource::LightClusters,
            .threshold = 1.0f,
            .label = "preflight light transport",
            .evidence = "light clusters pass/resource/backend advertisement incomplete",
            .message = "Strict light transport needs LightCull, LightClusters, and backend residency."});
  }

  if (required(required_mask, Signal::ShadowContinuityField) && requestShadow(settings) &&
      !(hasPass(graph, RenderGraphPass::ShadowAtlas) &&
        hasResource(graph, RenderGraphResource::ShadowAtlas) &&
        backendAdvertises(capabilities, RenderGraphResource::ShadowAtlas) &&
        settings.shadows.atlas_size > 0u)) {
    reject(report.rejected_signals,
           {.signal = Signal::ShadowContinuityField,
            .status = backendAdvertises(capabilities, RenderGraphResource::ShadowAtlas)
                          ? SignalStatus::MissingProof
                          : SignalStatus::Unsupported,
            .pass = RenderGraphPass::ShadowAtlas,
            .resource = RenderGraphResource::ShadowAtlas,
            .threshold = 1.0f,
            .label = "preflight shadow atlas",
            .evidence = "shadow pass/resource/backend atlas contract incomplete",
            .message = "Strict shadow continuity cannot rely on a backend that does not own the atlas."});
  }

  if (required(required_mask, Signal::ReflectionProbeResidency) && requestProbe(settings) &&
      !(hasPass(graph, RenderGraphPass::ReflectionProbe) &&
        hasResource(graph, RenderGraphResource::ReflectionProbes) &&
        backendAdvertises(capabilities, RenderGraphResource::ReflectionProbes) &&
        settings.reflections.probe_resolution > 0u)) {
    reject(report.rejected_signals,
           {.signal = Signal::ReflectionProbeResidency,
            .status = backendAdvertises(capabilities, RenderGraphResource::ReflectionProbes)
                          ? SignalStatus::MissingProof
                          : SignalStatus::Unsupported,
            .pass = RenderGraphPass::ReflectionProbe,
            .resource = RenderGraphResource::ReflectionProbes,
            .threshold = 1.0f,
            .label = "preflight reflection probes",
            .evidence = "probe pass/resource/backend atlas contract incomplete",
            .message = "Strict reflection proof needs resident probe-atlas ownership before encode."});
  }

  const bool has_material_path =
      !forensics.material_bindings.empty() || !forensics.surface_traces.empty() ||
      std::any_of(forensics.rhi_trace.pipelines.begin(), forensics.rhi_trace.pipelines.end(),
                  [](const rhi::PipelineStateTrace &pipeline) {
                    return pipeline.cache_key != 0u && !pipeline.label.empty();
                  });
  if (required(required_mask, Signal::MaterialFrequencyAudit) && !has_material_path) {
    reject(report.rejected_signals,
           {.signal = Signal::MaterialFrequencyAudit,
            .status = SignalStatus::MissingProof,
            .pass = RenderGraphPass::Opaque,
            .resource = RenderGraphResource::SceneColor,
            .threshold = 1.0f,
            .label = "preflight material frequency",
            .evidence = "no material bindings, surface traces, or pipeline keys before encode",
            .message = "Strict material truth needs shader/material frequency evidence before draw."});
  }

  if (required(required_mask, Signal::TemporalStabilityAudit) && nativeGpu(capabilities) &&
      capabilities.supports_gpu_timestamps && forensics.rhi_trace.timestamps.empty()) {
    reject(report.rejected_signals,
           {.signal = Signal::TemporalStabilityAudit,
            .status = SignalStatus::MissingProof,
            .pass = RenderGraphPass::Capture,
            .resource = RenderGraphResource::CaptureReadback,
            .threshold = 1.0f,
            .label = "preflight temporal stability",
            .evidence = "backend advertises timestamps but no timestamp trace is scheduled",
            .message = "Strict native temporal proof requires timestamp trace scheduling."});
  }

  if (required(required_mask, Signal::BackendVisualDelta) && require_native && !nativeGpu(capabilities)) {
    reject(report.rejected_signals,
           {.signal = Signal::BackendVisualDelta,
            .status = SignalStatus::Unsupported,
            .pass = RenderGraphPass::Capture,
            .resource = RenderGraphResource::CaptureReadback,
            .threshold = 1.0f,
            .label = "preflight native backend",
            .evidence = "settings require native backend but active backend is not native GPU",
            .message = "Strict GC7 native proof cannot be satisfied by software or null rendering."});
  }

  report.verdict.strict = true;
  report.verdict.minimum_score = gc7.minimum_score > 0.0f ? gc7.minimum_score : 0.74f;
  report.verdict.required_signal_mask = required_mask;
  report.verdict.signal_count = report.rejected_signals.size();
  report.verdict.rejected_signal_count = report.rejected_signals.size();
  report.verdict.accepted = report.rejected_signals.empty();
  report.verdict.status =
      report.verdict.accepted ? VerdictStatus::Accepted : VerdictStatus::Rejected;
  report.verdict.score = report.verdict.accepted ? 1.0f : 0.0f;
  for (const SignalEvidence &signal : report.rejected_signals) {
    const std::uint32_t bit = signalBit(signal.signal);
    if (signal.status == SignalStatus::Unsupported) {
      report.verdict.unsupported_signal_mask |= bit;
    } else {
      report.verdict.missing_signal_mask |= bit;
    }
  }
  report.verdict.diagnostic =
      report.verdict.accepted ? "graphics_core7 preflight accepted"
                              : diagnosticFor(report.rejected_signals);
  report.verdict.evidence_hash = verdictHash(report.verdict, report.rejected_signals);
  return report;
}

void applyFrameGateReport(FrameForensics &forensics, FrameGateReport report) {
  if (report.verdict.evidence_hash == 0u) {
    report.verdict.evidence_hash = verdictHash(report.verdict, report.rejected_signals);
  }
  forensics.graphics_core7_verdict = std::move(report.verdict);
  forensics.graphics_core7_signals = std::move(report.rejected_signals);
}

} // namespace aster::graphics_core7
