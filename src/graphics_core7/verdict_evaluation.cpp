// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/verdict_evaluation.hpp"

#include "aster/graphics_core7/backend_visual_delta.hpp"
#include "aster/graphics_core7/clustered_light_gpu_tables.hpp"
#include "aster/graphics_core7/reflection_probe_atlas.hpp"
#include "aster/graphics_core7/shadow_atlas_native.hpp"
#include "aster/graphics_core7/temporal_resolve.hpp"
#include "aster/graphics_core7/volumetric_fog_native.hpp"
#include "aster/render/render_device.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>
#include <utility>

namespace aster::graphics_core7 {
namespace {

[[nodiscard]] bool graphResourceAdvertised(const RenderBackendCapabilities &capabilities,
                                           const RenderGraphResource resource) {
  return (capabilities.graph_resource_mask & renderGraphResourceBit(resource)) != 0u;
}

[[nodiscard]] bool hasAvailableCapture(const FrameForensics &forensics,
                                       const RenderGraphResource resource) {
  return std::any_of(forensics.captures.begin(), forensics.captures.end(),
                     [resource](const FrameDebugCapture &capture) {
                       return capture.resource == resource && capture.available &&
                              capture.content_hash != 0u;
                     });
}

[[nodiscard]] bool hasReadAfterWrite(const FrameForensics &forensics,
                                     const RenderGraphResource resource) {
  bool saw_write = false;
  bool saw_read = false;
  for (const FrameResourceTrace &trace : forensics.resource_traces) {
    if (trace.resource != resource) {
      continue;
    }
    saw_write = saw_write || trace.write;
    saw_read = saw_read || !trace.write;
  }
  return saw_write && saw_read;
}

[[nodiscard]] std::size_t missingProofCount(const FrameForensics &forensics) {
  return static_cast<std::size_t>(
      std::count_if(forensics.backend_feature_proofs.begin(),
                    forensics.backend_feature_proofs.end(), [](const BackendFeatureProof &proof) {
                      return proof.status == BackendFeatureProofStatus::MissingProof;
                    }));
}

[[nodiscard]] std::size_t availableTimestampCount(const FrameForensics &forensics) {
  return static_cast<std::size_t>(
      std::count_if(forensics.timestamp_samples.begin(), forensics.timestamp_samples.end(),
                    [](const rhi::TimestampQueryResult &sample) { return sample.available; }));
}

[[nodiscard]] bool settingsRequestLightTransport(const RendererSettings &settings) {
  return settings.sun_light.enabled || !settings.light_rig.empty() ||
         settings.clustered_lighting.enabled ||
         (settings.atmosphere.enabled && settings.atmosphere.fog_strength > 0.0f);
}

[[nodiscard]] bool settingsRequestShadow(const RendererSettings &settings) {
  return settings.shadows.enabled && settings.shadows.directional_cascades > 0u;
}

[[nodiscard]] bool settingsRequestProbe(const RendererSettings &settings) {
  return settings.reflections.enabled && settings.reflections.static_local_probes;
}

[[nodiscard]] bool settingsRequestFog(const RendererSettings &settings) {
  return settings.atmosphere.enabled && settings.atmosphere.fog_strength > 0.0f;
}

[[nodiscard]] std::uint32_t defaultRequiredMask(const RendererSettings &settings) {
  std::uint32_t mask = signalBit(Signal::SurfaceTruthBuffer) |
                       signalBit(Signal::MaterialFrequencyAudit) |
                       signalBit(Signal::TemporalStabilityAudit) |
                       signalBit(Signal::BackendVisualDelta) |
                       signalBit(Signal::PlayerReadableFrameVerdict);
  if (settingsRequestLightTransport(settings)) {
    mask |= signalBit(Signal::LightTransportEvidence);
  }
  if (settingsRequestShadow(settings)) {
    mask |= signalBit(Signal::ShadowContinuityField);
  }
  if (settingsRequestProbe(settings)) {
    mask |= signalBit(Signal::ReflectionProbeResidency);
  }
  return mask;
}

[[nodiscard]] bool signalRequired(const std::uint32_t mask, const Signal signal) {
  return (mask & signalBit(signal)) != 0u;
}

[[nodiscard]] SignalStatus statusFromProof(const bool required, const bool requested,
                                           const bool advertised, const bool capture_available,
                                           const bool sampled) {
  if (!required && !requested) {
    return SignalStatus::NotRequired;
  }
  if (!advertised) {
    return SignalStatus::Unsupported;
  }
  if (capture_available && sampled) {
    return SignalStatus::Proven;
  }
  return SignalStatus::MissingProof;
}

void finalizeSignal(SignalEvidence &signal) {
  signal.evidence_hash = signalEvidenceHash(signal);
}

void appendSignal(std::vector<SignalEvidence> &signals, SignalEvidence signal) {
  finalizeSignal(signal);
  signals.push_back(std::move(signal));
}

[[nodiscard]] std::string signalList(const std::vector<SignalEvidence> &signals,
                                     const bool strict_only) {
  std::ostringstream out;
  bool first = true;
  for (const SignalEvidence &signal : signals) {
    const bool rejected = signal.required && signal.status != SignalStatus::Proven;
    if (strict_only && !rejected) {
      continue;
    }
    if (!first) {
      out << ", ";
    }
    first = false;
    out << signalName(signal.signal) << ":" << signalStatusName(signal.status);
  }
  return out.str();
}

} // namespace

EvaluationResult evaluateFrameTruth(const FrameForensics &forensics,
                                    const RenderBackendCapabilities &capabilities,
                                    const RendererSettings &settings,
                                    const FrameStats &stats) {
  EvaluationResult result;
  const Settings gc7 = settings.graphics_core7;
  const bool strict = gc7.strict();
  const std::uint32_t required_mask =
      gc7.required_signal_mask != 0u ? gc7.required_signal_mask : defaultRequiredMask(settings);
  const float minimum_score =
      std::isfinite(gc7.minimum_score) && gc7.minimum_score > 0.0f ? gc7.minimum_score : 0.74f;
  const bool require_native_backend = (gc7.flags & RequireNativeBackend) != 0u;

  const bool native_gpu = capabilities.gpu &&
                          capabilities.kind != RenderBackendKind::SoftwareReference &&
                          capabilities.kind != RenderBackendKind::Null;
  const bool reference_backend = capabilities.kind == RenderBackendKind::SoftwareReference;

  const bool surface_capture = hasAvailableCapture(forensics, RenderGraphResource::SurfaceAttributes) ||
                               hasAvailableCapture(forensics, RenderGraphResource::SurfaceOcclusion) ||
                               hasAvailableCapture(forensics, RenderGraphResource::CaptureReadback);
  appendSignal(result.signals,
               {.signal = Signal::SurfaceTruthBuffer,
                .status = surface_capture ? SignalStatus::Proven : SignalStatus::MissingProof,
                .pass = RenderGraphPass::Opaque,
                .resource = RenderGraphResource::SurfaceAttributes,
                .required = signalRequired(required_mask, Signal::SurfaceTruthBuffer),
                .score = surface_capture ? 1.0f : 0.0f,
                .threshold = 1.0f,
                .label = "surface truth buffer",
                .evidence = surface_capture ? "surface or final capture available"
                                            : "no surface/final capture payload",
                .message = surface_capture
                               ? "Surface-facing proof buffers are available for frame truth."
                               : "Frame lacks a surface truth buffer or final capture payload."});

  const bool clustered_gpu_tables_proven =
      reference_backend ||
      (native_gpu && capabilities.capability_table.storage_buffers &&
       graphResourceAdvertised(capabilities, RenderGraphResource::LightClusters));
  const ClusteredLightGpuTableReport light_report =
      inspectClusteredLightGpuTables(forensics.clustered_lights, clustered_gpu_tables_proven);
  const bool light_requested = settingsRequestLightTransport(settings);
  const bool light_proven = !light_requested ||
                            (light_report.cluster_count > 0u && !light_report.fallback_used &&
                             (!native_gpu || light_report.gpu_consumption_proven));
  appendSignal(result.signals,
               {.signal = Signal::LightTransportEvidence,
                .status = !signalRequired(required_mask, Signal::LightTransportEvidence) &&
                                  !light_requested
                              ? SignalStatus::NotRequired
                              : (light_proven ? SignalStatus::Proven : SignalStatus::Degraded),
                .pass = RenderGraphPass::LightCull,
                .resource = RenderGraphResource::LightClusters,
                .required = signalRequired(required_mask, Signal::LightTransportEvidence),
                .score = light_proven ? 1.0f : 0.50f,
                .threshold = 0.74f,
                .label = "light transport evidence",
                .evidence = std::to_string(light_report.cluster_count) + " clusters, " +
                            std::to_string(light_report.light_index_count) + " light refs",
                .message = light_proven
                               ? "Light selection has table evidence for this backend."
                               : "Light transport is CPU/reference-only or fell back before native proof."});

  const NativeShadowAtlasReport shadow_report = inspectNativeShadowAtlas(
      settingsRequestShadow(settings), graphResourceAdvertised(capabilities, RenderGraphResource::ShadowAtlas),
      hasAvailableCapture(forensics, RenderGraphResource::ShadowAtlas),
      hasReadAfterWrite(forensics, RenderGraphResource::ShadowAtlas), settings.shadows.atlas_size,
      settings.shadows.directional_cascades);
  appendSignal(result.signals,
               {.signal = Signal::ShadowContinuityField,
                .status = statusFromProof(signalRequired(required_mask, Signal::ShadowContinuityField),
                                          shadow_report.requested,
                                          shadow_report.resource_advertised,
                                          shadow_report.capture_available,
                                          shadow_report.sampled_by_final_frame),
                .pass = RenderGraphPass::ShadowAtlas,
                .resource = RenderGraphResource::ShadowAtlas,
                .required = signalRequired(required_mask, Signal::ShadowContinuityField),
                .score = shadow_report.capture_available && shadow_report.sampled_by_final_frame
                             ? 1.0f
                             : 0.0f,
                .threshold = 1.0f,
                .label = "shadow continuity field",
                .evidence = std::to_string(shadow_report.cascade_count) + " cascades, atlas=" +
                            std::to_string(shadow_report.atlas_size),
                .message = shadow_report.capture_available && shadow_report.sampled_by_final_frame
                               ? "Shadow atlas has capture and final-frame sampling evidence."
                               : "Shadow atlas proof is missing native capture or sampling evidence."});

  const ReflectionProbeAtlasReport probe_report = inspectReflectionProbeAtlas(
      settingsRequestProbe(settings),
      graphResourceAdvertised(capabilities, RenderGraphResource::ReflectionProbes),
      hasAvailableCapture(forensics, RenderGraphResource::ReflectionProbes),
      hasReadAfterWrite(forensics, RenderGraphResource::ReflectionProbes),
      forensics.resource_provenance.size(), settings.reflections.probe_resolution);
  appendSignal(result.signals,
               {.signal = Signal::ReflectionProbeResidency,
                .status = statusFromProof(signalRequired(required_mask, Signal::ReflectionProbeResidency),
                                          probe_report.requested,
                                          probe_report.resource_advertised,
                                          probe_report.capture_available,
                                          probe_report.sampled_by_final_frame),
                .pass = RenderGraphPass::ReflectionProbe,
                .resource = RenderGraphResource::ReflectionProbes,
                .required = signalRequired(required_mask, Signal::ReflectionProbeResidency),
                .score = probe_report.capture_available && probe_report.sampled_by_final_frame
                             ? 1.0f
                             : 0.0f,
                .threshold = 1.0f,
                .label = "reflection probe residency",
                .evidence = std::to_string(probe_report.probe_count) + " provenance records",
                .message = probe_report.capture_available && probe_report.sampled_by_final_frame
                               ? "Reflection probe atlas is resident and sampled."
                               : "Reflection probe residency lacks native capture or sampling proof."});

  const std::size_t authored_materials =
      static_cast<std::size_t>(std::count_if(
          forensics.material_bindings.begin(), forensics.material_bindings.end(),
          [](const MaterialBindingTrace &binding) { return binding.valid && binding.bound; }));
  const std::size_t surface_frequency =
      static_cast<std::size_t>(std::count_if(forensics.surface_traces.begin(),
                                             forensics.surface_traces.end(),
                                             [](const SurfacePresentationTrace &trace) {
                                               return trace.physical_texel_density > 0.0f &&
                                                      trace.height_normal_coupling > 0.0f;
                                             }));
  const bool material_frequency_ok = authored_materials > 0u || surface_frequency > 0u;
  appendSignal(result.signals,
               {.signal = Signal::MaterialFrequencyAudit,
                .status = material_frequency_ok ? SignalStatus::Proven : SignalStatus::MissingProof,
                .pass = RenderGraphPass::Opaque,
                .resource = RenderGraphResource::SceneColor,
                .required = signalRequired(required_mask, Signal::MaterialFrequencyAudit),
                .score = material_frequency_ok ? 1.0f : 0.0f,
                .threshold = 1.0f,
                .label = "material frequency audit",
                .evidence = std::to_string(authored_materials) + " material bindings, " +
                            std::to_string(surface_frequency) + " surface traces",
                .message = material_frequency_ok
                               ? "Material frequency and surface-scale evidence are present."
                               : "Material frequency lacks binding or surface-scale proof."});

  const TemporalStabilityReport temporal_report = inspectTemporalStability(
      forensics.timestamp_samples.size(), availableTimestampCount(forensics),
      settings.procedural_surface_normals || settings.animate_scene,
      strict && native_gpu);
  appendSignal(result.signals,
               {.signal = Signal::TemporalStabilityAudit,
                .status = temporal_report.stable ? SignalStatus::Proven : SignalStatus::MissingProof,
                .pass = RenderGraphPass::Capture,
                .resource = RenderGraphResource::CaptureReadback,
                .required = signalRequired(required_mask, Signal::TemporalStabilityAudit),
                .score = temporal_report.score,
                .threshold = strict && native_gpu ? 1.0f : 0.45f,
                .label = "temporal stability audit",
                .evidence = std::to_string(temporal_report.available_timestamp_count) + "/" +
                            std::to_string(temporal_report.timestamp_sample_count) +
                            " timestamp samples",
                .message = temporal_report.stable
                               ? "Temporal evidence satisfies the active backend policy."
                               : "Strict native truth requires resolved GPU timestamp evidence."});

  const std::size_t capture_count = forensics.captures.size();
  const std::size_t available_captures = static_cast<std::size_t>(
      std::count_if(forensics.captures.begin(), forensics.captures.end(),
                    [](const FrameDebugCapture &capture) {
                      return capture.available && capture.content_hash != 0u;
                    }));
  const BackendVisualDeltaReport delta_report =
      inspectBackendVisualDelta(capture_count, available_captures, missingProofCount(forensics));
  const bool backend_delta_proven =
      delta_report.score >= 0.74f && (!require_native_backend || native_gpu);
  appendSignal(result.signals,
               {.signal = Signal::BackendVisualDelta,
                .status = backend_delta_proven ? SignalStatus::Proven : SignalStatus::MissingProof,
                .pass = RenderGraphPass::Capture,
                .resource = RenderGraphResource::CaptureReadback,
                .required = signalRequired(required_mask, Signal::BackendVisualDelta),
                .score =
                    backend_delta_proven ? delta_report.score : std::min(delta_report.score, 0.35f),
                .threshold = 0.74f,
                .label = "backend visual delta",
                .evidence = std::to_string(available_captures) + "/" +
                            std::to_string(capture_count) + " captures, " +
                            std::to_string(delta_report.missing_proof_count) + " missing proofs",
                .message =
                    backend_delta_proven
                        ? "Backend visual delta has usable capture and proof evidence."
                        : (require_native_backend && !native_gpu
                               ? "Backend visual delta requires native GPU proof for this frame."
                               : "Backend visual delta lacks capture coverage or proof parity.")});

  if (settingsRequestFog(settings)) {
    const NativeVolumetricFogReport fog_report = inspectNativeVolumetricFog(
        true, graphResourceAdvertised(capabilities, RenderGraphResource::VolumetricFog),
        hasAvailableCapture(forensics, RenderGraphResource::VolumetricFog),
        hasReadAfterWrite(forensics, RenderGraphResource::VolumetricFog),
        static_cast<std::uint32_t>(std::max(stats.framebuffer_width / 4, 1)),
        static_cast<std::uint32_t>(std::max(stats.framebuffer_height / 4, 1)));
    if (!(fog_report.resource_advertised && fog_report.capture_available &&
          fog_report.sampled_by_final_frame)) {
      for (SignalEvidence &signal : result.signals) {
        if (signal.signal == Signal::LightTransportEvidence && signal.status == SignalStatus::Proven) {
          signal.status = SignalStatus::Degraded;
          signal.score = std::min(signal.score, 0.60f);
          signal.message = "Light transport is degraded because volumetric fog proof is incomplete.";
          finalizeSignal(signal);
        }
      }
    }
  }

  float total = 0.0f;
  std::size_t scored_count = 0u;
  std::size_t rejected_count = 0u;
  std::uint32_t proven_mask = 0u;
  std::uint32_t degraded_mask = 0u;
  std::uint32_t missing_mask = 0u;
  std::uint32_t unsupported_mask = 0u;
  for (const SignalEvidence &signal : result.signals) {
    if (!signal.required) {
      continue;
    }
    total += signal.score;
    ++scored_count;
    const std::uint32_t bit = signalBit(signal.signal);
    if (signal.status == SignalStatus::Proven) {
      proven_mask |= bit;
    } else if (signal.status == SignalStatus::Degraded) {
      degraded_mask |= bit;
      ++rejected_count;
    } else if (signal.status == SignalStatus::Unsupported) {
      unsupported_mask |= bit;
      ++rejected_count;
    } else {
      missing_mask |= bit;
      ++rejected_count;
    }
  }

  PlayerReadableFrameVerdict verdict;
  verdict.strict = strict;
  verdict.minimum_score = minimum_score;
  verdict.required_signal_mask = required_mask;
  verdict.proven_signal_mask = proven_mask;
  verdict.degraded_signal_mask = degraded_mask;
  verdict.missing_signal_mask = missing_mask;
  verdict.unsupported_signal_mask = unsupported_mask;
  verdict.rejected_signal_count = rejected_count;
  verdict.score = scored_count == 0u ? 1.0f : total / static_cast<float>(scored_count);
  verdict.accepted = strict ? (rejected_count == 0u && verdict.score >= minimum_score)
                            : (verdict.score >= minimum_score && missing_mask == 0u &&
                               unsupported_mask == 0u);
  verdict.status = verdict.accepted ? VerdictStatus::Accepted
                                    : (strict ? VerdictStatus::Rejected : VerdictStatus::Degraded);
  verdict.diagnostic =
      verdict.accepted ? "graphics_core7 frame truth accepted"
                       : ("graphics_core7 frame truth blocked by " +
                          signalList(result.signals, strict));

  appendSignal(result.signals,
               {.signal = Signal::PlayerReadableFrameVerdict,
                .status = verdict.accepted
                              ? SignalStatus::Proven
                              : (strict ? SignalStatus::MissingProof : SignalStatus::Degraded),
                .pass = RenderGraphPass::Capture,
                .resource = RenderGraphResource::CaptureReadback,
                .required = signalRequired(required_mask, Signal::PlayerReadableFrameVerdict),
                .score = verdict.score,
                .threshold = minimum_score,
                .label = "player readable frame verdict",
                .evidence = signalList(result.signals, false),
                .message = verdict.diagnostic});

  verdict.signal_count = result.signals.size();
  verdict.evidence_hash = verdictHash(verdict, result.signals);
  result.verdict = std::move(verdict);
  return result;
}

void applyEvaluation(FrameForensics &forensics, EvaluationResult result) {
  std::vector<SignalEvidence> signals = std::move(forensics.graphics_core7_signals);
  const PlayerReadableFrameVerdict preflight_verdict = forensics.graphics_core7_verdict;
  const bool preflight_rejected =
      preflight_verdict.status == VerdictStatus::Rejected && preflight_verdict.evidence_hash != 0u;
  signals.insert(signals.end(), std::make_move_iterator(result.signals.begin()),
                 std::make_move_iterator(result.signals.end()));

  PlayerReadableFrameVerdict verdict = std::move(result.verdict);
  if (preflight_rejected) {
    verdict.accepted = false;
    verdict.status = VerdictStatus::Rejected;
    verdict.score = std::min(verdict.score, preflight_verdict.score);
    verdict.missing_signal_mask |= preflight_verdict.missing_signal_mask;
    verdict.unsupported_signal_mask |= preflight_verdict.unsupported_signal_mask;
    verdict.rejected_signal_count += preflight_verdict.rejected_signal_count;
    verdict.diagnostic = preflight_verdict.diagnostic + "; " + verdict.diagnostic;
  }
  verdict.signal_count = signals.size();
  verdict.evidence_hash = verdictHash(verdict, signals);
  forensics.graphics_core7_verdict = std::move(verdict);
  forensics.graphics_core7_signals = std::move(signals);
}

} // namespace aster::graphics_core7
