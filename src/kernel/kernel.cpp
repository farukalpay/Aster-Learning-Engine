// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/kernel/abi.h"

#include "aster/core/config.hpp"
#include "aster/core/world_state.hpp"
#include "aster/game_sdk/game_sdk.hpp"
#include "aster/math/geometry.hpp"
#include "aster/math/quat.hpp"
#include "aster/math/transform.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/render_quality.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/render/visual_regression.hpp"
#include "aster/scene/scene.hpp"
#include "aster/shader/shader_compiler.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h>
#endif

constexpr std::uint32_t kEngineMagic = 0x41535452u;
constexpr std::uint32_t kWindowMagic = 0x4154574eu;
constexpr std::uint32_t kSceneMagic = 0x41545343u;
constexpr std::uint32_t kRendererMagic = 0x41545244u;
constexpr std::uint32_t kMeshMagic = 0x41544d45u;
constexpr std::uint32_t kMaterialMagic = 0x41544d41u;
constexpr std::uint32_t kShaderMagic = 0x41545348u;
constexpr std::uint32_t kPipelineMagic = 0x41545049u;
constexpr std::uint32_t kTextureMagic = 0x41545458u;
constexpr std::uint32_t kRenderTargetMagic = 0x41545254u;
constexpr std::uint32_t kBufferMagic = 0x41544246u;
constexpr std::uint32_t kDescriptorHeapMagic = 0x41544448u;
constexpr std::uint32_t kDescriptorSetMagic = 0x41544453u;
constexpr std::uint32_t kPipelineCacheMagic = 0x41545043u;
constexpr std::uint32_t kFrameScheduleMagic = 0x41544653u;
constexpr std::uint32_t kWorldMagic = 0x41545744u;
constexpr std::uint32_t kSystemWorldMagic = 0x41545357u;
constexpr std::uint32_t kAuthoringDocumentMagic = 0x41544144u;
constexpr std::uint32_t kAuthoringExecutionMagic = 0x41544145u;
constexpr std::uint32_t kRetiredMagic = 0xDEAD5A5Au;

struct KernelValidationRecord {
  AsterValidationKind kind = ASTER_VALIDATION_UNKNOWN;
  AsterKernelFrameDiagnosticSeverity severity = ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR;
  std::string source;
  std::string label;
  std::string message;
  std::uint64_t value = 0u;
};

struct AsterEngineHandle__ {
  std::uint32_t magic = kEngineMagic;
  AsterStatus last_status{sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_STATUS_OK,
                          "ok"};
  std::vector<KernelValidationRecord> validation_events;
};

struct AsterWindowHandle__ {
  std::uint32_t magic = kWindowMagic;
  bool headless = false;
  bool vsync = true;
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::string title;
  std::unique_ptr<aster::Window> window;
};

struct AsterSceneHandle__ {
  std::uint32_t magic = kSceneMagic;
  AsterEngineHandle owner = nullptr;
  aster::Scene scene;
};

struct AsterMeshHandle__ {
  std::uint32_t magic = kMeshMagic;
  aster::MeshPrimitive primitive = aster::MeshPrimitive::Sphere;
  std::shared_ptr<const aster::CpuMesh> custom_mesh;
  std::string label;
};

struct AsterMaterialHandle__ {
  std::uint32_t magic = kMaterialMagic;
  aster::Material material;
  std::string label;
  std::vector<AsterTextureRole> texture_roles;
};

struct AsterRendererHandle__ {
  std::uint32_t magic = kRendererMagic;
  std::unique_ptr<aster::RenderDevice> renderer;
  AsterWindowHandle bound_window = nullptr;
  AsterFrameStats last_stats{};
  std::vector<std::string> string_scratch;
  std::vector<KernelValidationRecord> validation_events;
  AsterRenderTargetHandle active_target = nullptr;
  bool has_rendered_frame = false;
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
  std::uint64_t world_trace_hash = 0u;
  std::uint64_t simulation_tick = 0u;
  std::uint64_t extraction_hash = 0u;
  std::uint64_t asset_lineage_hash = 0u;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t actor_state_delta_hash = 0u;
  std::uint64_t sensory_event_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  std::uint64_t encounter_budget_hash = 0u;
  std::uint32_t navigation_valid = 0u;
  std::uint64_t streaming_region_id = 0u;
  float perceptual_salience_score = 0.0f;
  std::uint32_t perceptual_continuity_accepted = 0u;
  std::uint32_t perceptual_continuity_required_channel_mask = 0u;
  std::uint32_t perceptual_continuity_observed_channel_mask = 0u;
  std::uint32_t perceptual_continuity_missing_channel_mask = 0u;
  float perceptual_continuity_score = 0.0f;
  float perceptual_continuity_minimum_score = 0.0f;
  std::uint64_t reaction_package_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t lighting_atmosphere_hash = 0u;
  std::uint64_t ai_attention_hash = 0u;
  std::uint64_t streaming_residency_lod_hash = 0u;
  std::uint64_t resource_state_hash = 0u;
  std::uint64_t event_residue_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;

  AsterRendererHandle__() {
    last_stats.size = sizeof(AsterFrameStats);
    last_stats.version = ASTER_KERNEL_STRUCT_VERSION_1;
  }
};

struct AsterShaderArtifactHandle__ {
  std::uint32_t magic = kShaderMagic;
  aster::ShaderCompileResult result;
  std::vector<std::string> reflection_names;
  std::vector<AsterShaderReflectionBinding> reflection;
};

struct AsterRenderPipelineHandle__ {
  std::uint32_t magic = kPipelineMagic;
  AsterShaderArtifactHandle shader = nullptr;
  std::string label;
};

struct AsterTextureHandle__ {
  std::uint32_t magic = kTextureMagic;
  AsterTextureRole role = ASTER_TEXTURE_ROLE_UNKNOWN;
  AsterTextureColorSpace color_space = ASTER_TEXTURE_COLOR_SPACE_LINEAR;
  AsterTextureNormalConvention normal_convention = ASTER_TEXTURE_NORMAL_CONVENTION_NONE;
  AsterKernelBackendFormat format = ASTER_KERNEL_BACKEND_FORMAT_UNKNOWN;
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::uint32_t mip_count = 1u;
  std::string label;
};

struct AsterRenderTargetHandle__ {
  std::uint32_t magic = kRenderTargetMagic;
  AsterKernelBackendFormat color_format = ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM;
  AsterKernelBackendFormat depth_format = ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT;
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::uint32_t sample_count = 1u;
  std::string label;
};

struct AsterBufferHandle__ {
  std::uint32_t magic = kBufferMagic;
  std::uint64_t byte_size = 0u;
  std::uint32_t usage = 0u;
  std::string label;
};

struct AsterDescriptorHeapHandle__ {
  std::uint32_t magic = kDescriptorHeapMagic;
  std::uint32_t descriptor_capacity = 0u;
  bool shader_visible = true;
  std::string label;
};

struct AsterDescriptorSetHandle__ {
  std::uint32_t magic = kDescriptorSetMagic;
  AsterDescriptorHeapHandle heap = nullptr;
  std::uint32_t descriptor_count = 0u;
  std::string label;
};

struct AsterPipelineCacheHandle__ {
  std::uint32_t magic = kPipelineCacheMagic;
  std::uint64_t seed = 0u;
  std::string label;
};

struct AsterFrameScheduleHandle__ {
  std::uint32_t magic = kFrameScheduleMagic;
  std::vector<aster::FramePassStats> passes;
  aster::rhi::FrameTrace trace;
  std::vector<KernelValidationRecord> validation_events;
};

struct AsterWorldHandle__ {
  std::uint32_t magic = kWorldMagic;
  AsterEngineHandle owner = nullptr;
  aster::WorldState world;
  std::list<std::string> string_scratch;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t actor_state_delta_hash = 0u;
  std::uint64_t sensory_event_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  std::size_t actor_delta_count = 0u;
  std::uint64_t render_extraction_hash = 0u;
  std::uint64_t streaming_region_id = 0u;
  AsterWorldRegionGateVerdict gate_verdict = ASTER_WORLD_REGION_GATE_UNKNOWN;
  std::uint32_t navigation_valid = 0u;
  std::size_t navigation_checked_steps = 0u;
  std::size_t navigation_blocked_steps = 0u;
  std::uint64_t navigation_report_hash = 0u;
  std::string navigation_diagnostic;
  std::uint64_t encounter_budget_hash = 0u;
  std::uint64_t resource_probe_hash = 0u;
  std::uint32_t perceptual_accepted = 0u;
  float perceptual_salience_score = 0.0f;
  float perceptual_minimum_salience = 0.0f;
  std::uint64_t perceptual_report_hash = 0u;
  std::string perceptual_diagnostic;
  std::uint32_t perceptual_continuity_accepted = 0u;
  std::uint32_t perceptual_continuity_required_channel_mask = 0u;
  std::uint32_t perceptual_continuity_observed_channel_mask = 0u;
  std::uint32_t perceptual_continuity_missing_channel_mask = 0u;
  float perceptual_continuity_score = 0.0f;
  float perceptual_continuity_minimum_score = 0.0f;
  std::uint64_t reaction_package_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t lighting_atmosphere_hash = 0u;
  std::uint64_t ai_attention_hash = 0u;
  std::uint64_t streaming_residency_lod_hash = 0u;
  std::uint64_t resource_state_hash = 0u;
  std::uint64_t event_residue_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;
  std::string perceptual_continuity_diagnostic;
  std::string diagnostic;

  AsterWorldHandle__(AsterEngineHandle engine, aster::WorldStateConfig config)
      : owner(engine), world(std::move(config)) {}
};

struct AsterSystemWorldHandle__ {
  std::uint32_t magic = kSystemWorldMagic;
  AsterEngineHandle owner = nullptr;
  aster::WorldState world;
  std::list<std::string> string_scratch;

  AsterSystemWorldHandle__(AsterEngineHandle engine, aster::WorldStateConfig config)
      : owner(engine), world(std::move(config)) {}
};

struct AsterAuthoringDocumentHandle__ {
  std::uint32_t magic = kAuthoringDocumentMagic;
  AsterAuthoringDocumentKind kind = ASTER_AUTHORING_DOCUMENT_UNKNOWN;
  std::variant<std::monostate, aster::sdk::ProjectDocument, aster::sdk::SceneDocument,
               aster::sdk::PrefabDocument, aster::sdk::ItemDocument,
               aster::sdk::ActionGraphDocument, aster::sdk::InputMapDocument>
      document;
  std::vector<aster::sdk::Diagnostic> diagnostics;
  std::list<std::string> string_scratch;
};

struct AsterAuthoringActionExecutionHandle__ {
  std::uint32_t magic = kAuthoringExecutionMagic;
  aster::sdk::ActionExecution execution;
  std::list<std::string> string_scratch;
};

namespace {

std::uint64_t mixWorldEvidence(std::uint64_t hash, std::uint64_t value);

AsterStatus makeStatus(const AsterStatusCode code, const char *message) {
  return {sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, code, message};
}

template <typename Struct> bool validStruct(const Struct *value) {
  return value != nullptr && value->size >= sizeof(Struct) &&
         value->version == ASTER_KERNEL_STRUCT_VERSION_1;
}

template <typename Struct> Struct copyAbiStruct(const Struct *value) {
  Struct out{};
  if (value == nullptr) {
    return out;
  }
  const std::size_t byte_count = std::min<std::size_t>(value->size, sizeof(Struct));
  std::memcpy(&out, value, byte_count);
  out.size = sizeof(Struct);
  return out;
}

bool validTailExtendedStruct(const void *value, const std::size_t size,
                             const std::uint32_t version,
                             const std::size_t minimum_supported_size) {
  return value != nullptr && size >= minimum_supported_size &&
         version == ASTER_KERNEL_STRUCT_VERSION_1;
}

bool validCameraDesc(const AsterCameraDesc *value) {
  return validTailExtendedStruct(value, value == nullptr ? 0u : value->size,
                                 value == nullptr ? 0u : value->version,
                                 offsetof(AsterCameraDesc, focal_length_mm));
}

bool validRendererSettings(const AsterRendererSettings *value) {
  return validTailExtendedStruct(value, value == nullptr ? 0u : value->size,
                                 value == nullptr ? 0u : value->version,
                                 offsetof(AsterRendererSettings, quality_tier));
}

bool validWorldAdvanceDesc(const AsterWorldAdvanceDesc *value) {
  return validTailExtendedStruct(value, value == nullptr ? 0u : value->size,
                                 value == nullptr ? 0u : value->version,
                                 offsetof(AsterWorldAdvanceDesc,
                                          perceptual_continuity_budget));
}

bool validWorldRegionGateReport(const AsterWorldRegionGateReport *value) {
  return validTailExtendedStruct(value, value == nullptr ? 0u : value->size,
                                 value == nullptr ? 0u : value->version,
                                 offsetof(AsterWorldRegionGateReport,
                                          perceptual_continuity_budget));
}

bool validWorldForensics(const AsterWorldForensics *value) {
  return validTailExtendedStruct(value, value == nullptr ? 0u : value->size,
                                 value == nullptr ? 0u : value->version,
                                 offsetof(AsterWorldForensics, sensory_event_hash));
}

bool validFrameForensicsDetailCounts(const AsterFrameForensicsDetailCounts *value) {
  return value != nullptr &&
         value->size >= offsetof(AsterFrameForensicsDetailCounts, object_fate_count) &&
         value->version == ASTER_KERNEL_STRUCT_VERSION_1;
}

bool abiStructHasField(const std::size_t size, const std::size_t offset,
                       const std::size_t field_size) {
  return size >= offset + field_size;
}

bool hasPerceptualContinuityBudget(const AsterPerceptualContinuityBudget &budget) {
  return budget.size != 0u;
}

bool validPerceptualContinuityBudget(const AsterPerceptualContinuityBudget &budget) {
  return !hasPerceptualContinuityBudget(budget) ||
         (budget.size >= sizeof(AsterPerceptualContinuityBudget) &&
          budget.version == ASTER_KERNEL_STRUCT_VERSION_1 &&
          (budget.diagnostic.data != nullptr || budget.diagnostic.size == 0u));
}

bool validStringView(const AsterStringView view) {
  return view.data != nullptr || view.size == 0u;
}

std::string stringFromView(const AsterStringView view) {
  if (view.data == nullptr || view.size == 0u) {
    return {};
  }
  return std::string(view.data, view.size);
}

std::uint64_t continuityBudgetEvidenceHash(const AsterPerceptualContinuityBudget &budget) {
  if (!hasPerceptualContinuityBudget(budget)) {
    return 0u;
  }
  std::uint64_t hash = mixWorldEvidence(budget.required_channel_mask,
                                        budget.observed_channel_mask);
  hash = mixWorldEvidence(hash, budget.missing_channel_mask);
  hash = mixWorldEvidence(hash, static_cast<std::uint64_t>(
                                    std::max(budget.continuity_score, 0.0f) * 1000000.0f));
  hash = mixWorldEvidence(hash, static_cast<std::uint64_t>(
                                    std::max(budget.minimum_score, 0.0f) * 1000000.0f));
  hash = mixWorldEvidence(hash, budget.reaction_package_hash);
  hash = mixWorldEvidence(hash, budget.material_memory_hash);
  hash = mixWorldEvidence(hash, budget.lighting_atmosphere_hash);
  hash = mixWorldEvidence(hash, budget.ai_attention_hash);
  hash = mixWorldEvidence(hash, budget.streaming_residency_lod_hash);
  hash = mixWorldEvidence(hash, budget.resource_state_hash);
  hash = mixWorldEvidence(hash, budget.event_residue_hash);
  hash = mixWorldEvidence(hash, budget.readability_audit_hash);
  return hash;
}

void storeContinuityBudget(AsterWorldHandle__ &world,
                           const AsterPerceptualContinuityBudget &budget) {
  if (!hasPerceptualContinuityBudget(budget)) {
    return;
  }
  world.perceptual_continuity_accepted = budget.accepted;
  world.perceptual_continuity_required_channel_mask = budget.required_channel_mask;
  world.perceptual_continuity_observed_channel_mask = budget.observed_channel_mask;
  world.perceptual_continuity_missing_channel_mask = budget.missing_channel_mask;
  world.perceptual_continuity_score = budget.continuity_score;
  world.perceptual_continuity_minimum_score = budget.minimum_score;
  world.reaction_package_hash = budget.reaction_package_hash;
  world.material_memory_hash = budget.material_memory_hash;
  world.lighting_atmosphere_hash = budget.lighting_atmosphere_hash;
  world.ai_attention_hash = budget.ai_attention_hash;
  world.streaming_residency_lod_hash = budget.streaming_residency_lod_hash;
  world.resource_state_hash = budget.resource_state_hash;
  world.event_residue_hash = budget.event_residue_hash;
  world.readability_audit_hash = budget.readability_audit_hash;
  world.perceptual_continuity_diagnostic = stringFromView(budget.diagnostic);
}

AsterStringView viewFromString(const std::string &text) {
  return {text.data(), text.size()};
}

std::string joinStrings(const std::vector<std::string> &values, const std::string_view separator) {
  std::string out;
  for (std::size_t index = 0u; index < values.size(); ++index) {
    if (index > 0u) {
      out.append(separator);
    }
    out.append(values[index]);
  }
  return out;
}

AsterStringView viewFromScratch(const AsterRendererHandle renderer, std::string text) {
  renderer->string_scratch.push_back(std::move(text));
  return viewFromString(renderer->string_scratch.back());
}

std::string lowerExtension(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

void writeFramebufferByExtension(const std::filesystem::path &path, const int width,
                                 const int height) {
  if (lowerExtension(path) == ".png") {
    aster::writeFramebufferPng(path, width, height);
  } else {
    aster::writeFramebufferPpm(path, width, height);
  }
}

std::string sanitizeArtifactStem(std::string label) {
  if (label.empty()) {
    return "frame_vision_probe";
  }
  for (char &c : label) {
    const unsigned char value = static_cast<unsigned char>(c);
    if (!std::isalnum(value) && c != '-' && c != '_') {
      c = '_';
    }
  }
  return label;
}

std::string jsonEscape(const std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8u);
  for (const char c : text) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

bool validEngine(const AsterEngineHandle engine) {
  return engine != nullptr && engine->magic == kEngineMagic;
}

bool validWindow(const AsterWindowHandle window) {
  return window != nullptr && window->magic == kWindowMagic;
}

bool validScene(const AsterSceneHandle scene) {
  return scene != nullptr && scene->magic == kSceneMagic;
}

bool validRenderer(const AsterRendererHandle renderer) {
  return renderer != nullptr && renderer->magic == kRendererMagic && renderer->renderer != nullptr;
}

bool validMesh(const AsterMeshHandle mesh) {
  return mesh != nullptr && mesh->magic == kMeshMagic;
}

bool validMaterial(const AsterMaterialHandle material) {
  return material != nullptr && material->magic == kMaterialMagic;
}

bool validShader(const AsterShaderArtifactHandle shader) {
  return shader != nullptr && shader->magic == kShaderMagic;
}

bool validPipeline(const AsterRenderPipelineHandle pipeline) {
  return pipeline != nullptr && pipeline->magic == kPipelineMagic;
}

bool validTexture(const AsterTextureHandle texture) {
  return texture != nullptr && texture->magic == kTextureMagic;
}

bool retiredTexture(const AsterTextureHandle texture) {
  return texture != nullptr && texture->magic == kRetiredMagic;
}

bool validRenderTarget(const AsterRenderTargetHandle target) {
  return target != nullptr && target->magic == kRenderTargetMagic;
}

bool retiredRenderTarget(const AsterRenderTargetHandle target) {
  return target != nullptr && target->magic == kRetiredMagic;
}

bool validDescriptorHeap(const AsterDescriptorHeapHandle heap) {
  return heap != nullptr && heap->magic == kDescriptorHeapMagic;
}

bool retiredDescriptorHeap(const AsterDescriptorHeapHandle heap) {
  return heap != nullptr && heap->magic == kRetiredMagic;
}

bool validFrameSchedule(const AsterFrameScheduleHandle schedule) {
  return schedule != nullptr && schedule->magic == kFrameScheduleMagic;
}

bool validSystemWorld(const AsterSystemWorldHandle world) {
  return world != nullptr && world->magic == kSystemWorldMagic;
}

bool validWorld(const AsterWorldHandle world) {
  return world != nullptr && world->magic == kWorldMagic;
}

bool validAuthoringDocument(const AsterAuthoringDocumentHandle document) {
  return document != nullptr && document->magic == kAuthoringDocumentMagic;
}

bool validAuthoringExecution(const AsterAuthoringActionExecutionHandle execution) {
  return execution != nullptr && execution->magic == kAuthoringExecutionMagic;
}

template <typename Document>
[[nodiscard]] const Document *authoringDocumentAs(const AsterAuthoringDocumentHandle document) {
  return validAuthoringDocument(document) ? std::get_if<Document>(&document->document) : nullptr;
}

bool diagnosticsOk(const std::vector<aster::sdk::Diagnostic> &diagnostics) {
  for (const aster::sdk::Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.severity == aster::sdk::DiagnosticSeverity::Error) {
      return false;
    }
  }
  return true;
}

AsterStringView authoringScratch(AsterAuthoringDocumentHandle document, std::string text) {
  document->string_scratch.push_back(std::move(text));
  return viewFromString(document->string_scratch.back());
}

AsterStringView authoringScratch(AsterAuthoringActionExecutionHandle execution, std::string text) {
  execution->string_scratch.push_back(std::move(text));
  return viewFromString(execution->string_scratch.back());
}

AsterStringView systemWorldScratch(AsterSystemWorldHandle world, std::string text) {
  world->string_scratch.push_back(std::move(text));
  return viewFromString(world->string_scratch.back());
}

AsterStringView worldScratch(AsterWorldHandle world, std::string text) {
  world->string_scratch.push_back(std::move(text));
  return viewFromString(world->string_scratch.back());
}

std::uint64_t mixWorldEvidence(std::uint64_t hash, const std::uint64_t value) {
  hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
  hash ^= hash >> 29u;
  hash *= 0xbf58476d1ce4e5b9ull;
  return hash == 0u ? 1u : hash;
}

AsterAuthoringDiagnosticSeverity
authoringSeverity(const aster::sdk::DiagnosticSeverity severity) {
  return severity == aster::sdk::DiagnosticSeverity::Warning
             ? ASTER_AUTHORING_DIAGNOSTIC_WARNING
             : ASTER_AUTHORING_DIAGNOSTIC_ERROR;
}

AsterAuthoringAssetKind authoringAssetKind(const aster::sdk::AssetKind kind) {
  switch (kind) {
  case aster::sdk::AssetKind::Scene:
    return ASTER_AUTHORING_ASSET_SCENE;
  case aster::sdk::AssetKind::Prefab:
    return ASTER_AUTHORING_ASSET_PREFAB;
  case aster::sdk::AssetKind::Cave:
    return ASTER_AUTHORING_ASSET_CAVE;
  case aster::sdk::AssetKind::Material:
    return ASTER_AUTHORING_ASSET_MATERIAL;
  case aster::sdk::AssetKind::Item:
    return ASTER_AUTHORING_ASSET_ITEM;
  case aster::sdk::AssetKind::ActionGraph:
    return ASTER_AUTHORING_ASSET_ACTION_GRAPH;
  case aster::sdk::AssetKind::InputMap:
    return ASTER_AUTHORING_ASSET_INPUT_MAP;
  case aster::sdk::AssetKind::Ui:
    return ASTER_AUTHORING_ASSET_UI;
  case aster::sdk::AssetKind::Mesh:
    return ASTER_AUTHORING_ASSET_MESH;
  case aster::sdk::AssetKind::Texture:
    return ASTER_AUTHORING_ASSET_TEXTURE;
  case aster::sdk::AssetKind::AssetGraph:
    return ASTER_AUTHORING_ASSET_GRAPH;
  case aster::sdk::AssetKind::Unknown:
    return ASTER_AUTHORING_ASSET_UNKNOWN;
  }
  return ASTER_AUTHORING_ASSET_UNKNOWN;
}

AsterAuthoringInputDevice authoringInputDevice(const std::string_view device) {
  if (device == "keyboard") {
    return ASTER_AUTHORING_INPUT_KEYBOARD;
  }
  if (device == "mouse") {
    return ASTER_AUTHORING_INPUT_MOUSE;
  }
  if (device == "gamepad") {
    return ASTER_AUTHORING_INPUT_GAMEPAD;
  }
  if (device == "touch") {
    return ASTER_AUTHORING_INPUT_TOUCH;
  }
  return ASTER_AUTHORING_INPUT_UNKNOWN;
}

aster::WorldComponentAccessMode worldAccessMode(const AsterSystemComponentAccessMode mode) {
  return mode == ASTER_SYSTEM_COMPONENT_ACCESS_WRITE ? aster::WorldComponentAccessMode::Write
                                                     : aster::WorldComponentAccessMode::Read;
}

AsterSystemTraceEventKind abiWorldTraceEventKind(const aster::WorldTraceEventKind kind) {
  switch (kind) {
  case aster::WorldTraceEventKind::InputEvent:
    return ASTER_SYSTEM_TRACE_INPUT_EVENT;
  case aster::WorldTraceEventKind::SimulationTick:
    return ASTER_SYSTEM_TRACE_SIMULATION_TICK;
  case aster::WorldTraceEventKind::SchedulerDecision:
    return ASTER_SYSTEM_TRACE_SCHEDULER_DECISION;
  case aster::WorldTraceEventKind::AssetResolution:
    return ASTER_SYSTEM_TRACE_ASSET_RESOLUTION;
  case aster::WorldTraceEventKind::ResidencyDecision:
    return ASTER_SYSTEM_TRACE_RESIDENCY_DECISION;
  case aster::WorldTraceEventKind::RenderableExtraction:
    return ASTER_SYSTEM_TRACE_RENDERABLE_EXTRACTION;
  case aster::WorldTraceEventKind::FrameSubmission:
    return ASTER_SYSTEM_TRACE_FRAME_SUBMISSION;
  case aster::WorldTraceEventKind::TransactionBegin:
    return ASTER_SYSTEM_TRACE_TRANSACTION_BEGIN;
  case aster::WorldTraceEventKind::TransactionCommit:
    return ASTER_SYSTEM_TRACE_TRANSACTION_COMMIT;
  case aster::WorldTraceEventKind::TransactionAbort:
    return ASTER_SYSTEM_TRACE_TRANSACTION_ABORT;
  case aster::WorldTraceEventKind::EntityCreated:
    return ASTER_SYSTEM_TRACE_ENTITY_CREATED;
  case aster::WorldTraceEventKind::EntityDestroyed:
    return ASTER_SYSTEM_TRACE_ENTITY_DESTROYED;
  case aster::WorldTraceEventKind::SnapshotSaved:
    return ASTER_SYSTEM_TRACE_SNAPSHOT_SAVED;
  case aster::WorldTraceEventKind::SnapshotLoaded:
    return ASTER_SYSTEM_TRACE_SNAPSHOT_LOADED;
  case aster::WorldTraceEventKind::Migration:
    return ASTER_SYSTEM_TRACE_MIGRATION;
  case aster::WorldTraceEventKind::Replay:
    return ASTER_SYSTEM_TRACE_REPLAY;
  case aster::WorldTraceEventKind::ValidationError:
  default:
    return ASTER_SYSTEM_TRACE_VALIDATION_ERROR;
  }
}

AsterSystemEntityHandle abiWorldEntity(const aster::WorldEntityHandle handle) {
  return {.id = handle.id, .generation = handle.generation};
}

aster::WorldEntityHandle worldEntity(const AsterSystemEntityHandle handle) {
  return {.id = handle.id, .generation = handle.generation};
}

aster::WorldComponentAccess worldComponentAccess(const AsterSystemComponentAccess &access) {
  return {.component = stringFromView(access.component),
          .subject = stringFromView(access.subject),
          .mode = worldAccessMode(access.mode)};
}

std::vector<aster::WorldComponentAccess> worldComponentAccesses(const AsterSpan span,
                                                                AsterStatus *status) {
  std::vector<aster::WorldComponentAccess> out;
  if (status != nullptr) {
    *status = aster_kernel_status_ok();
  }
  if (span.size == 0u) {
    return out;
  }
  if (span.data == nullptr || span.stride < sizeof(AsterSystemComponentAccess)) {
    if (status != nullptr) {
      *status = makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "component access span is invalid");
    }
    return out;
  }
  const auto *bytes = static_cast<const unsigned char *>(span.data);
  out.reserve(span.size);
  for (std::size_t index = 0u; index < span.size; ++index) {
    const auto *access =
        reinterpret_cast<const AsterSystemComponentAccess *>(bytes + index * span.stride);
    if (!validStruct(access)) {
      if (status != nullptr) {
        *status =
            makeStatus(ASTER_STATUS_ABI_MISMATCH, "component access version is not supported");
      }
      out.clear();
      return out;
    }
    if (!validStringView(access->component) || !validStringView(access->subject) ||
        access->component.size == 0u || access->subject.size == 0u) {
      if (status != nullptr) {
        *status =
            makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                       "component access requires component and subject");
      }
      out.clear();
      return out;
    }
    out.push_back(worldComponentAccess(*access));
  }
  return out;
}

AsterSystemTransactionInfo abiWorldTransactionInfo(AsterSystemWorldHandle world,
                                                   const aster::WorldTransactionInfo &info) {
  return {.size = sizeof(AsterSystemTransactionInfo),
          .version = ASTER_KERNEL_STRUCT_VERSION_1,
          .transaction_id = info.transaction_id,
          .committed = info.committed ? 1u : 0u,
          .access_count = info.access_count,
          .parent_world_hash = info.parent_world_hash,
          .post_world_hash = info.post_world_hash,
          .deterministic_stamp = info.deterministic_stamp,
          .diagnostic = systemWorldScratch(world, info.diagnostic)};
}

std::uint32_t componentFlags(const aster::sdk::ComponentSet &components) {
  std::uint32_t flags = 0u;
  if (components.transform.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_TRANSFORM;
  }
  if (components.mesh_renderer.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_MESH_RENDERER;
  }
  if (components.collider.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_COLLIDER;
  }
  if (components.light.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_LIGHT;
  }
  if (components.interactable.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_INTERACTABLE;
  }
  if (components.inventory.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_INVENTORY;
  }
  if (components.camera.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_CAMERA;
  }
  if (components.cave_scene.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_CAVE_SCENE;
  }
  if (components.fixture.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_FIXTURE;
  }
  if (components.ore_node.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_ORE_NODE;
  }
  if (components.torch_socket.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_TORCH_SOCKET;
  }
  if (components.spawn_point.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_SPAWN_POINT;
  }
  if (components.mining.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_MINING;
  }
  if (components.cave_debug.has_value()) {
    flags |= ASTER_AUTHORING_ENTITY_COMPONENT_CAVE_DEBUG;
  }
  return flags;
}

bool finiteValue(const float value) {
  return std::isfinite(value);
}

bool finiteVec2(const AsterVec2 value) {
  return finiteValue(value.x) && finiteValue(value.y);
}

bool finiteVec3(const AsterVec3 value) {
  return finiteValue(value.x) && finiteValue(value.y) && finiteValue(value.z);
}

bool finiteVec4(const AsterVec4 value) {
  return finiteValue(value.x) && finiteValue(value.y) && finiteValue(value.z) &&
         finiteValue(value.w);
}

bool srgbFormat(const AsterKernelBackendFormat format) {
  switch (format) {
  case ASTER_KERNEL_BACKEND_FORMAT_RGBA8_SRGB:
  case ASTER_KERNEL_BACKEND_FORMAT_BGRA8_SRGB:
  case ASTER_KERNEL_BACKEND_FORMAT_BC1_RGBA_SRGB:
  case ASTER_KERNEL_BACKEND_FORMAT_BC3_RGBA_SRGB:
  case ASTER_KERNEL_BACKEND_FORMAT_BC7_RGBA_SRGB:
  case ASTER_KERNEL_BACKEND_FORMAT_ASTC4X4_RGBA_SRGB:
    return true;
  default:
    return false;
  }
}

bool colorSpaceRequiresSrgb(const AsterTextureRole role) {
  return role == ASTER_TEXTURE_ROLE_ALBEDO || role == ASTER_TEXTURE_ROLE_EMISSIVE;
}

const char *textureRoleName(const AsterTextureRole role) {
  switch (role) {
  case ASTER_TEXTURE_ROLE_ALBEDO:
    return "albedo";
  case ASTER_TEXTURE_ROLE_NORMAL:
    return "normal";
  case ASTER_TEXTURE_ROLE_ORM:
    return "orm";
  case ASTER_TEXTURE_ROLE_ROUGHNESS:
    return "roughness";
  case ASTER_TEXTURE_ROLE_METALLIC:
    return "metallic";
  case ASTER_TEXTURE_ROLE_AO:
    return "ao";
  case ASTER_TEXTURE_ROLE_HEIGHT:
    return "height";
  case ASTER_TEXTURE_ROLE_EMISSIVE:
    return "emissive";
  case ASTER_TEXTURE_ROLE_WETNESS:
    return "wetness";
  case ASTER_TEXTURE_ROLE_OPACITY:
    return "opacity";
  case ASTER_TEXTURE_ROLE_MASK:
    return "mask";
  case ASTER_TEXTURE_ROLE_UNKNOWN:
  default:
    return "unknown";
  }
}

bool hasFormatBit(const std::uint64_t mask, const AsterKernelBackendFormat format) {
  const std::uint32_t bit = static_cast<std::uint32_t>(format);
  return bit < 64u && (mask & (1ull << bit)) != 0ull;
}

bool hasSampleCountBit(const std::uint64_t mask, const std::uint32_t sample_count) {
  return sample_count < 64u && (mask & (1ull << sample_count)) != 0ull;
}

void setLastStatus(const AsterEngineHandle engine, const AsterStatus status) {
  if (validEngine(engine)) {
    engine->last_status = status;
  }
}

void appendValidation(std::vector<KernelValidationRecord> &events, const AsterValidationKind kind,
                      const AsterKernelFrameDiagnosticSeverity severity, std::string source,
                      std::string label, std::string message, const std::uint64_t value = 0u) {
  events.push_back({kind, severity, std::move(source), std::move(label), std::move(message), value});
}

AsterStatus failWithValidation(const AsterEngineHandle engine, const AsterStatusCode code,
                               const char *message, const AsterValidationKind kind,
                               std::string source, std::string label,
                               const std::uint64_t value = 0u) {
  const AsterStatus status = makeStatus(code, message);
  if (validEngine(engine)) {
    setLastStatus(engine, status);
    appendValidation(engine->validation_events, kind, ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                     std::move(source), std::move(label), message, value);
  }
  return status;
}

void appendRendererValidation(const AsterRendererHandle renderer, const AsterValidationKind kind,
                              const AsterKernelFrameDiagnosticSeverity severity,
                              std::string source, std::string label, std::string message,
                              const std::uint64_t value = 0u) {
  if (renderer != nullptr && renderer->magic == kRendererMagic) {
    appendValidation(renderer->validation_events, kind, severity, std::move(source),
                     std::move(label), std::move(message), value);
  }
}

AsterStatus validationEventAt(const std::vector<KernelValidationRecord> &events,
                              const std::size_t index, AsterValidationEvent *out_event) {
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "validation event struct version is not supported");
  }
  if (index >= events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "validation event index is out of range");
  }
  const KernelValidationRecord &event = events[index];
  out_event->kind = event.kind;
  out_event->severity = event.severity;
  out_event->source = viewFromString(event.source);
  out_event->label = viewFromString(event.label);
  out_event->message = viewFromString(event.message);
  out_event->value = event.value;
  return aster_kernel_status_ok();
}

template <typename Handle> AsterStatus destroyHandle(Handle handle, const std::uint32_t magic) {
  if (handle == nullptr || handle->magic != magic) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "handle is invalid");
  }
  handle->magic = 0u;
  delete handle;
  return makeStatus(ASTER_STATUS_OK, "ok");
}

template <typename Handle> AsterStatus retireHandle(Handle handle, const std::uint32_t magic) {
  if (handle == nullptr || handle->magic != magic) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "handle is invalid");
  }
  handle->magic = kRetiredMagic;
  return makeStatus(ASTER_STATUS_OK, "ok");
}

aster::Vec3 vec(const AsterVec3 value) {
  return {value.x, value.y, value.z};
}

aster::Vec2 vec(const AsterVec2 value) {
  return {value.x, value.y};
}

AsterVec3 abiVec(const aster::Vec3 value) {
  return {value.x, value.y, value.z};
}

aster::Viewport viewport(const AsterViewport *value) {
  if (value == nullptr) {
    return {};
  }
  return {.origin = vec(value->origin),
          .size = vec(value->size),
          .origin_convention = value->origin_top_left != 0u
                                   ? aster::ViewportOrigin::TopLeft
                                   : aster::ViewportOrigin::BottomLeft};
}

aster::WorldPoint worldPoint(const AsterWorldPoint value) {
  return aster::WorldPoint{vec(value.value)};
}

aster::ScreenPoint screenPoint(const AsterScreenPoint value) {
  return aster::ScreenPoint{vec(value.value)};
}

AsterWorldPoint abiWorldPoint(const aster::WorldPoint value) {
  return {abiVec(value.value)};
}

AsterScreenPoint abiScreenPoint(const aster::ScreenPoint value) {
  return {abiVec(value.value)};
}

AsterWorldRay abiWorldRay(const aster::WorldRay &value) {
  return {abiWorldPoint(value.origin), abiVec(value.direction.value), value.max_distance};
}

aster::Quat quat(const AsterQuat value) {
  return {value.x, value.y, value.z, value.w};
}

AsterQuat abiQuat(const aster::Quat value) {
  return {value.x, value.y, value.z, value.w};
}

aster::Mat4 mat4(const AsterMat4 &value) {
  aster::Mat4 out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = value.m[index];
  }
  return out;
}

AsterMat4 abiMat4(const aster::Mat4 &value) {
  AsterMat4 out{};
  for (std::size_t index = 0; index < value.m.size(); ++index) {
    out.m[index] = value.m[index];
  }
  return out;
}

aster::MathPolicy mathPolicy(const AsterMathPolicy *policy) {
  if (policy == nullptr || policy->size < sizeof(AsterMathPolicy) ||
      policy->version != ASTER_KERNEL_STRUCT_VERSION_1) {
    return aster::defaultMathPolicy();
  }
  return {.absolute_epsilon = policy->absolute_epsilon > 0.0f ? policy->absolute_epsilon : 0.000001f,
          .relative_epsilon = policy->relative_epsilon > 0.0f ? policy->relative_epsilon : 0.00001f,
          .max_iterations = policy->max_iterations,
          .deterministic = policy->deterministic != 0u,
          .debug_strict = policy->debug_strict != 0u};
}

AsterMathError abiMathError(const aster::MathError error) {
  switch (error) {
  case aster::MathError::InvalidArgument:
    return ASTER_MATH_ERROR_INVALID_ARGUMENT;
  case aster::MathError::NonFiniteInput:
    return ASTER_MATH_ERROR_NON_FINITE_INPUT;
  case aster::MathError::DegenerateInput:
    return ASTER_MATH_ERROR_DEGENERATE_INPUT;
  case aster::MathError::SingularMatrix:
    return ASTER_MATH_ERROR_SINGULAR_MATRIX;
  case aster::MathError::UnsupportedPolicy:
    return ASTER_MATH_ERROR_UNSUPPORTED_POLICY;
  case aster::MathError::None:
  default:
    return ASTER_MATH_ERROR_NONE;
  }
}

void writeMathDiagnostics(AsterMathDiagnostics *out, const aster::MathDiagnostics &diagnostics) {
  if (out == nullptr) {
    return;
  }
  out->size = sizeof(AsterMathDiagnostics);
  out->version = ASTER_KERNEL_STRUCT_VERSION_1;
  out->error = abiMathError(diagnostics.error);
  out->determinant = diagnostics.determinant;
  out->condition_hint = diagnostics.condition_hint;
  out->message = diagnostics.message == nullptr ? "" : diagnostics.message;
}

void writeMathOk(AsterMathDiagnostics *out) {
  writeMathDiagnostics(out, {});
}

AsterStatus statusFromMath(const aster::MathDiagnostics &diagnostics) {
  switch (diagnostics.error) {
  case aster::MathError::InvalidArgument:
  case aster::MathError::NonFiniteInput:
  case aster::MathError::DegenerateInput:
  case aster::MathError::SingularMatrix:
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      diagnostics.message == nullptr ? "math operation failed" : diagnostics.message);
  case aster::MathError::UnsupportedPolicy:
    return makeStatus(ASTER_STATUS_UNSUPPORTED,
                      diagnostics.message == nullptr ? "math policy is unsupported" : diagnostics.message);
  case aster::MathError::None:
  default:
    return aster_kernel_status_ok();
  }
}

aster::ProjectionPolicy projectionPolicy(const AsterMathCoordinateHandedness handedness,
                                         const AsterMathClipDepthRange depth_range,
                                         const AsterMathDepthDirection depth_direction) {
  return {.handedness = handedness == ASTER_MATH_COORDINATE_LEFT_HANDED
                            ? aster::CoordinateHandedness::LeftHanded
                            : aster::CoordinateHandedness::RightHanded,
          .depth_range = depth_range == ASTER_MATH_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE
                             ? aster::ClipDepthRange::NegativeOneToOne
                             : aster::ClipDepthRange::ZeroToOne,
          .depth_direction = depth_direction == ASTER_MATH_DEPTH_FORWARD_Z
                                 ? aster::DepthDirection::ForwardZ
                                 : aster::DepthDirection::ReverseZ};
}

aster::ProjectionPolicy projectionPolicy(const AsterProjectionConvention *convention) {
  if (convention == nullptr) {
    return aster::defaultProjectionPolicy();
  }
  aster::ProjectionPolicy policy =
      projectionPolicy(convention->handedness, convention->depth_range,
                       convention->depth_direction);
  policy.viewport_origin = convention->viewport_origin_top_left != 0u
                               ? aster::ViewportOrigin::TopLeft
                               : aster::ViewportOrigin::BottomLeft;
  policy.y_flip = convention->y_flip != 0u;
  policy.matrix_storage = convention->column_major != 0u
                              ? aster::MatrixStorageOrder::ColumnMajor
                              : aster::MatrixStorageOrder::RowMajor;
  policy.vector_convention = convention->column_vector != 0u
                                 ? aster::VectorConvention::ColumnVector
                                 : aster::VectorConvention::RowVector;
  return policy;
}

aster::MeshPrimitive meshPrimitive(const AsterKernelMeshPrimitive primitive) {
  switch (primitive) {
  case ASTER_KERNEL_MESH_PRIMITIVE_BOX:
    return aster::MeshPrimitive::Box;
  case ASTER_KERNEL_MESH_PRIMITIVE_PLANE:
    return aster::MeshPrimitive::Plane;
  case ASTER_KERNEL_MESH_PRIMITIVE_ROCK:
    return aster::MeshPrimitive::Rock;
  case ASTER_KERNEL_MESH_PRIMITIVE_CRYSTAL:
    return aster::MeshPrimitive::Crystal;
  case ASTER_KERNEL_MESH_PRIMITIVE_RUIN_BLOCK:
    return aster::MeshPrimitive::RuinBlock;
  case ASTER_KERNEL_MESH_PRIMITIVE_PILLAR:
    return aster::MeshPrimitive::Pillar;
  case ASTER_KERNEL_MESH_PRIMITIVE_SPHERE:
  default:
    return aster::MeshPrimitive::Sphere;
  }
}

aster::MaterialAlphaMode alphaMode(const AsterKernelMaterialAlphaMode mode) {
  switch (mode) {
  case ASTER_KERNEL_MATERIAL_ALPHA_MASKED:
    return aster::MaterialAlphaMode::Masked;
  case ASTER_KERNEL_MATERIAL_ALPHA_DITHERED_COVERAGE:
    return aster::MaterialAlphaMode::DitheredCoverage;
  case ASTER_KERNEL_MATERIAL_ALPHA_BLEND:
    return aster::MaterialAlphaMode::Blend;
  case ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE:
  default:
    return aster::MaterialAlphaMode::Opaque;
  }
}

aster::RenderQualityTier renderQualityTier(const std::uint32_t tier) {
  switch (tier) {
  case ASTER_KERNEL_RENDER_QUALITY_PROTOTYPE:
    return aster::RenderQualityTier::Prototype;
  case ASTER_KERNEL_RENDER_QUALITY_CINEMATIC:
    return aster::RenderQualityTier::Cinematic;
  case ASTER_KERNEL_RENDER_QUALITY_PRODUCTION:
  default:
    return aster::RenderQualityTier::Production;
  }
}

aster::ToneMapper toneMapper(const std::uint32_t mapper) {
  switch (mapper) {
  case ASTER_KERNEL_TONE_MAPPER_FILMIC_ACES:
    return aster::ToneMapper::FilmicAces;
  case ASTER_KERNEL_TONE_MAPPER_REINHARD:
    return aster::ToneMapper::Reinhard;
  case ASTER_KERNEL_TONE_MAPPER_PBR_NEUTRAL:
  default:
    return aster::ToneMapper::PbrNeutral;
  }
}

bool hasRenderSettingFlag(const AsterRendererSettings &settings, const std::uint32_t flag) {
  return (settings.flags & flag) != 0u;
}

float positiveOr(const float value, const float fallback) {
  return finiteValue(value) && value > 0.0f ? value : fallback;
}

std::uint32_t positiveOr(const std::uint32_t value, const std::uint32_t fallback) {
  return value > 0u ? value : fallback;
}

float cameraVerticalFov(const AsterCameraDesc &camera, const std::uint32_t width,
                        const std::uint32_t height) {
  const float fallback =
      camera.vertical_fov_radians > 0.0f ? camera.vertical_fov_radians : aster::radians(45.0f);
  if ((camera.camera_flags & ASTER_KERNEL_CAMERA_FLAG_USE_PHYSICAL_LENS) == 0u) {
    return fallback;
  }
  const float focal_length = positiveOr(camera.focal_length_mm, 46.0f);
  const float sensor_width = positiveOr(camera.sensor_width_mm, 36.0f);
  const float aspect =
      width > 0u && height > 0u ? static_cast<float>(width) / static_cast<float>(height)
                                : 16.0f / 9.0f;
  const float sensor_height = sensor_width / std::max(aspect, 0.01f);
  const float physical_fov = 2.0f * std::atan((sensor_height * 0.5f) / focal_length);
  return std::clamp(physical_fov, aster::radians(12.0f), aster::radians(85.0f));
}

aster::RendererSettings rendererSettingsFromAbi(const AsterRendererSettings &settings,
                                                const AsterCameraDesc &camera) {
  aster::RendererSettings out;
  aster::applyRenderQualityProfile(
      out, aster::makeRenderQualityProfile(renderQualityTier(settings.quality_tier)));

  out.pipeline.clear_color = vec(settings.clear_color);
  out.exposure = positiveOr(settings.exposure, out.exposure);
  out.ambient_strength = positiveOr(settings.ambient_strength, out.ambient_strength);
  out.ambient_floor = positiveOr(settings.ambient_floor, out.ambient_floor);
  out.pipeline.tone_mapper = toneMapper(settings.tone_mapper);
  out.post.bloom_threshold = positiveOr(settings.bloom_threshold, out.post.bloom_threshold);
  out.post.bloom_intensity = positiveOr(settings.bloom_intensity, out.post.bloom_intensity);
  out.post.bloom = out.post.bloom || out.post.bloom_intensity > 0.0f ||
                   hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_BLOOM);
  out.post.fxaa = out.post.fxaa ||
                  hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_FXAA);

  out.shadows.directional_cascades =
      positiveOr(settings.shadow_cascades, out.shadows.directional_cascades);
  out.shadows.atlas_size = positiveOr(settings.shadow_atlas_size, out.shadows.atlas_size);
  out.shadows.max_distance = positiveOr(settings.shadow_max_distance, out.shadows.max_distance);
  out.shadows.receiver_bias =
      positiveOr(settings.shadow_receiver_bias, out.shadows.receiver_bias);
  out.shadows.normal_bias = positiveOr(settings.shadow_normal_bias, out.shadows.normal_bias);
  out.shadows.pcf_radius = positiveOr(settings.shadow_softness, out.shadows.pcf_radius);
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_CASCADED_SHADOWS)) {
    out.shadows.enabled = true;
    out.shadows.cascaded_directional = true;
    out.shadows.directional_cascades = std::max(out.shadows.directional_cascades, 3u);
  }

  out.occlusion.radius = positiveOr(settings.occlusion_radius, out.occlusion.radius);
  out.occlusion.thickness = positiveOr(settings.occlusion_thickness, out.occlusion.thickness);
  out.occlusion.strength = positiveOr(settings.occlusion_strength, out.occlusion.strength);
  out.occlusion.sample_count =
      positiveOr(settings.occlusion_sample_count, out.occlusion.sample_count);
  out.occlusion.contact_hardening =
      positiveOr(settings.occlusion_contact_hardening, out.occlusion.contact_hardening);
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_SURFACE_OCCLUSION)) {
    out.occlusion.enabled = true;
    out.occlusion.mode = aster::RendererOcclusionMode::Hybrid;
  }

  out.grounding.contact_shadow_strength =
      positiveOr(settings.contact_shadow_strength, out.grounding.contact_shadow_strength);
  out.grounding.contact_shadow_radius_scale =
      positiveOr(settings.contact_shadow_radius_scale, out.grounding.contact_shadow_radius_scale);
  out.grounding.contact_shadow_receiver_height = positiveOr(
      settings.contact_shadow_receiver_height, out.grounding.contact_shadow_receiver_height);
  out.grounding.contact_shadow_receiver_bias = positiveOr(
      settings.contact_shadow_receiver_bias, out.grounding.contact_shadow_receiver_bias);
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_CONTACT_SHADOWS)) {
    out.grounding.enabled = true;
    out.grounding.contact_shadows = true;
    out.grounding.auto_contact_shadows = true;
  }

  out.surface_scale.physical_texel_density =
      positiveOr(settings.physical_texel_density, out.surface_scale.physical_texel_density);
  out.surface_scale.macro_frequency_breakup =
      positiveOr(settings.macro_frequency_breakup, out.surface_scale.macro_frequency_breakup);
  out.surface_scale.micro_frequency_breakup =
      positiveOr(settings.micro_frequency_breakup, out.surface_scale.micro_frequency_breakup);
  out.surface_scale.height_normal_coupling =
      positiveOr(settings.height_normal_coupling, out.surface_scale.height_normal_coupling);
  out.surface_scale.roughness_height_coupling = positiveOr(
      settings.roughness_height_coupling, out.surface_scale.roughness_height_coupling);

  out.atmosphere.fog_start = positiveOr(settings.fog_start, out.atmosphere.fog_start);
  out.atmosphere.fog_end = positiveOr(settings.fog_end, out.atmosphere.fog_end);
  out.atmosphere.fog_strength = positiveOr(settings.fog_strength, out.atmosphere.fog_strength);
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_VOLUMETRIC_FOG) ||
      settings.fog_strength > 0.0f) {
    out.atmosphere.enabled = true;
    out.atmosphere.fog_falloff = aster::AtmosphereFogFalloff::Powered;
    out.atmosphere.fog_power = 1.18f;
    out.atmosphere.local_light_scattering = 0.34f;
    out.atmosphere.source_glow_strength = 0.82f;
    out.atmosphere.local_light_extinction = 0.055f;
    out.atmosphere.phase_anisotropy = 0.24f;
    out.atmosphere.volumetric_light_steps = 6u;
  }

  out.reflections.fallback_intensity =
      positiveOr(settings.reflection_intensity, out.reflections.fallback_intensity);
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_REFLECTION_PROBES)) {
    out.reflections.enabled = true;
    out.reflections.static_local_probes = true;
    out.reflections.max_active_probes = std::max(out.reflections.max_active_probes, 8u);
  }

  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_PROCEDURAL_SURFACE_NORMALS)) {
    out.procedural_surface_normals = true;
  }
  if (hasRenderSettingFlag(settings, ASTER_KERNEL_RENDER_SETTING_PRESENTATION_LENS) ||
      (camera.camera_flags & ASTER_KERNEL_CAMERA_FLAG_USE_PHYSICAL_LENS) != 0u) {
    out.presentation.focal_length_mm =
        positiveOr(camera.focal_length_mm, out.presentation.focal_length_mm);
    out.presentation.sensor_width_mm =
        positiveOr(camera.sensor_width_mm, out.presentation.sensor_width_mm);
    out.presentation.composition_weight =
        positiveOr(camera.composition_weight, out.presentation.composition_weight);
    out.presentation.scale_reference_m =
        positiveOr(camera.scale_reference_m, out.presentation.scale_reference_m);
  }

  out.sun_light.enabled = true;
  out.sun_light.intensity = std::max(out.sun_light.intensity, 1.0f);
  return out;
}

AsterKernelBackendKind backendKind(const aster::RenderBackendKind kind) {
  switch (kind) {
  case aster::RenderBackendKind::SoftwareReference:
    return ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE;
  case aster::RenderBackendKind::Metal:
    return ASTER_KERNEL_BACKEND_METAL;
  case aster::RenderBackendKind::D3D12:
    return ASTER_KERNEL_BACKEND_D3D12;
  case aster::RenderBackendKind::Null:
    return ASTER_KERNEL_BACKEND_NULL;
  case aster::RenderBackendKind::Unknown:
  default:
    return ASTER_KERNEL_BACKEND_UNKNOWN;
  }
}

std::uint32_t capabilityFlags(const aster::RenderBackendCapabilities &capabilities) {
  std::uint32_t flags = 0u;
  flags |= capabilities.gpu ? ASTER_KERNEL_BACKEND_CAP_GPU : 0u;
  flags |= capabilities.supports_shader_materials ? ASTER_KERNEL_BACKEND_CAP_SHADER_MATERIALS : 0u;
  flags |= capabilities.supports_texture_sampling ? ASTER_KERNEL_BACKEND_CAP_TEXTURE_SAMPLING : 0u;
  flags |= capabilities.supports_instancing ? ASTER_KERNEL_BACKEND_CAP_INSTANCING : 0u;
  flags |= capabilities.supports_capture ? ASTER_KERNEL_BACKEND_CAP_CAPTURE : 0u;
  flags |= capabilities.supports_ui_composite ? ASTER_KERNEL_BACKEND_CAP_UI_COMPOSITE : 0u;
  flags |= capabilities.supports_gpu_timestamps ? ASTER_KERNEL_BACKEND_CAP_GPU_TIMESTAMPS : 0u;
  return flags;
}

AsterKernelBackendShaderModel shaderModel(const aster::rhi::ShaderModel model) {
  switch (model) {
  case aster::rhi::ShaderModel::SoftwareReference:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_SOFTWARE_REFERENCE;
  case aster::rhi::ShaderModel::MetalMSL23:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_METAL_MSL_2_3;
  case aster::rhi::ShaderModel::D3D12ShaderModel51:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_D3D12_SHADER_MODEL_5_1;
  case aster::rhi::ShaderModel::None:
  default:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_NONE;
  }
}

AsterKernelBackendPresentationMode presentationMode(
    const aster::rhi::PresentationMode presentation) {
  switch (presentation) {
  case aster::rhi::PresentationMode::SoftwareFramebuffer:
    return ASTER_KERNEL_BACKEND_PRESENTATION_SOFTWARE_FRAMEBUFFER;
  case aster::rhi::PresentationMode::MetalLayer:
    return ASTER_KERNEL_BACKEND_PRESENTATION_METAL_LAYER;
  case aster::rhi::PresentationMode::D3D12OffscreenReadback:
    return ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_OFFSCREEN_READBACK;
  case aster::rhi::PresentationMode::D3D12Swapchain:
    return ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_SWAPCHAIN;
  case aster::rhi::PresentationMode::None:
  default:
    return ASTER_KERNEL_BACKEND_PRESENTATION_NONE;
  }
}

aster::ShaderBackend shaderBackend(const AsterKernelShaderBackend backend) {
  switch (backend) {
  case ASTER_KERNEL_SHADER_BACKEND_METAL_MSL:
    return aster::ShaderBackend::MetalMSL;
  case ASTER_KERNEL_SHADER_BACKEND_D3D12_HLSL:
    return aster::ShaderBackend::D3D12HLSL;
  case ASTER_KERNEL_SHADER_BACKEND_SOFTWARE_REFERENCE:
  default:
    return aster::ShaderBackend::SoftwareReference;
  }
}

AsterKernelShaderResourceKind shaderResourceKind(const aster::ShaderResourceKind kind) {
  switch (kind) {
  case aster::ShaderResourceKind::Texture:
    return ASTER_KERNEL_SHADER_RESOURCE_TEXTURE;
  case aster::ShaderResourceKind::Sampler:
    return ASTER_KERNEL_SHADER_RESOURCE_SAMPLER;
  case aster::ShaderResourceKind::UniformBuffer:
  default:
    return ASTER_KERNEL_SHADER_RESOURCE_UNIFORM_BUFFER;
  }
}

AsterKernelRenderGraphPass renderGraphPass(const aster::RenderGraphPass pass) {
  switch (pass) {
  case aster::RenderGraphPass::LightCull:
    return ASTER_KERNEL_RENDER_PASS_LIGHT_CULL;
  case aster::RenderGraphPass::ShadowAtlas:
    return ASTER_KERNEL_RENDER_PASS_SHADOW_ATLAS;
  case aster::RenderGraphPass::Opaque:
    return ASTER_KERNEL_RENDER_PASS_OPAQUE;
  case aster::RenderGraphPass::ContactShadow:
    return ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW;
  case aster::RenderGraphPass::SurfaceOcclusion:
    return ASTER_KERNEL_RENDER_PASS_SURFACE_OCCLUSION;
  case aster::RenderGraphPass::SceneLighting:
    return ASTER_KERNEL_RENDER_PASS_SCENE_LIGHTING;
  case aster::RenderGraphPass::VolumetricFog:
    return ASTER_KERNEL_RENDER_PASS_VOLUMETRIC_FOG;
  case aster::RenderGraphPass::ReflectionProbe:
    return ASTER_KERNEL_RENDER_PASS_REFLECTION_PROBE;
  case aster::RenderGraphPass::Transparent:
    return ASTER_KERNEL_RENDER_PASS_TRANSPARENT;
  case aster::RenderGraphPass::UiComposite:
    return ASTER_KERNEL_RENDER_PASS_UI_COMPOSITE;
  case aster::RenderGraphPass::Capture:
    return ASTER_KERNEL_RENDER_PASS_CAPTURE;
  case aster::RenderGraphPass::SceneColorDepth:
  default:
    return ASTER_KERNEL_RENDER_PASS_SCENE_COLOR_DEPTH;
  }
}

AsterKernelRenderGraphResource renderGraphResource(const aster::RenderGraphResource resource) {
  switch (resource) {
  case aster::RenderGraphResource::SceneDepth:
    return ASTER_KERNEL_RENDER_RESOURCE_SCENE_DEPTH;
  case aster::RenderGraphResource::SurfaceAttributes:
    return ASTER_KERNEL_RENDER_RESOURCE_SURFACE_ATTRIBUTES;
  case aster::RenderGraphResource::LightClusters:
    return ASTER_KERNEL_RENDER_RESOURCE_LIGHT_CLUSTERS;
  case aster::RenderGraphResource::ShadowAtlas:
    return ASTER_KERNEL_RENDER_RESOURCE_SHADOW_ATLAS;
  case aster::RenderGraphResource::SurfaceOcclusion:
    return ASTER_KERNEL_RENDER_RESOURCE_SURFACE_OCCLUSION;
  case aster::RenderGraphResource::VolumetricFog:
    return ASTER_KERNEL_RENDER_RESOURCE_VOLUMETRIC_FOG;
  case aster::RenderGraphResource::ReflectionProbes:
    return ASTER_KERNEL_RENDER_RESOURCE_REFLECTION_PROBES;
  case aster::RenderGraphResource::UiOverlay:
    return ASTER_KERNEL_RENDER_RESOURCE_UI_OVERLAY;
  case aster::RenderGraphResource::CaptureReadback:
    return ASTER_KERNEL_RENDER_RESOURCE_CAPTURE_READBACK;
  case aster::RenderGraphResource::SceneColor:
  default:
    return ASTER_KERNEL_RENDER_RESOURCE_SCENE_COLOR;
  }
}

AsterKernelRhiResourceState rhiResourceState(const aster::rhi::ResourceState state) {
  switch (state) {
  case aster::rhi::ResourceState::CopySource:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_SOURCE;
  case aster::rhi::ResourceState::CopyDestination:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_DESTINATION;
  case aster::rhi::ResourceState::ShaderRead:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_READ;
  case aster::rhi::ResourceState::ShaderWrite:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_WRITE;
  case aster::rhi::ResourceState::ColorAttachment:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COLOR_ATTACHMENT;
  case aster::rhi::ResourceState::DepthAttachment:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_DEPTH_ATTACHMENT;
  case aster::rhi::ResourceState::Present:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_PRESENT;
  case aster::rhi::ResourceState::Readback:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_READBACK;
  case aster::rhi::ResourceState::Undefined:
  default:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_UNDEFINED;
  }
}

AsterKernelRhiQueueKind rhiQueueKind(const aster::rhi::QueueKind queue) {
  switch (queue) {
  case aster::rhi::QueueKind::Compute:
    return ASTER_KERNEL_RHI_QUEUE_COMPUTE;
  case aster::rhi::QueueKind::Copy:
    return ASTER_KERNEL_RHI_QUEUE_COPY;
  case aster::rhi::QueueKind::Graphics:
  default:
    return ASTER_KERNEL_RHI_QUEUE_GRAPHICS;
  }
}

AsterKernelBackendFeatureProofKind
backendFeatureProofKind(const aster::BackendFeatureProofKind kind) {
  switch (kind) {
  case aster::BackendFeatureProofKind::Capture:
    return ASTER_KERNEL_BACKEND_FEATURE_CAPTURE;
  case aster::BackendFeatureProofKind::TextureSampling:
    return ASTER_KERNEL_BACKEND_FEATURE_TEXTURE_SAMPLING;
  case aster::BackendFeatureProofKind::Instancing:
    return ASTER_KERNEL_BACKEND_FEATURE_INSTANCING;
  case aster::BackendFeatureProofKind::GpuTimestamps:
    return ASTER_KERNEL_BACKEND_FEATURE_GPU_TIMESTAMPS;
  case aster::BackendFeatureProofKind::HdrRenderTarget:
    return ASTER_KERNEL_BACKEND_FEATURE_HDR_RENDER_TARGET;
  case aster::BackendFeatureProofKind::Msaa:
    return ASTER_KERNEL_BACKEND_FEATURE_MSAA;
  case aster::BackendFeatureProofKind::Presentation:
    return ASTER_KERNEL_BACKEND_FEATURE_PRESENTATION;
  case aster::BackendFeatureProofKind::GraphResource:
  default:
    return ASTER_KERNEL_BACKEND_FEATURE_GRAPH_RESOURCE;
  }
}

AsterKernelBackendFeatureProofStatus
backendFeatureProofStatus(const aster::BackendFeatureProofStatus status) {
  switch (status) {
  case aster::BackendFeatureProofStatus::NotExercised:
    return ASTER_KERNEL_BACKEND_FEATURE_NOT_EXERCISED;
  case aster::BackendFeatureProofStatus::Proven:
    return ASTER_KERNEL_BACKEND_FEATURE_PROVEN;
  case aster::BackendFeatureProofStatus::MissingProof:
    return ASTER_KERNEL_BACKEND_FEATURE_MISSING_PROOF;
  case aster::BackendFeatureProofStatus::Unsupported:
    return ASTER_KERNEL_BACKEND_FEATURE_UNSUPPORTED;
  case aster::BackendFeatureProofStatus::NotAdvertised:
  default:
    return ASTER_KERNEL_BACKEND_FEATURE_NOT_ADVERTISED;
  }
}

AsterKernelRhiValidationKind
rhiValidationKind(const aster::rhi::ResourceLifetimeValidationKind kind) {
  switch (kind) {
  case aster::rhi::ResourceLifetimeValidationKind::ReadBeforeWrite:
    return ASTER_KERNEL_RHI_VALIDATION_READ_BEFORE_WRITE;
  case aster::rhi::ResourceLifetimeValidationKind::MissingBarrier:
    return ASTER_KERNEL_RHI_VALIDATION_MISSING_BARRIER;
  case aster::rhi::ResourceLifetimeValidationKind::QueueOwnershipMismatch:
    return ASTER_KERNEL_RHI_VALIDATION_QUEUE_OWNERSHIP_MISMATCH;
  case aster::rhi::ResourceLifetimeValidationKind::DescriptorResourceMismatch:
    return ASTER_KERNEL_RHI_VALIDATION_DESCRIPTOR_RESOURCE_MISMATCH;
  case aster::rhi::ResourceLifetimeValidationKind::MissingResource:
    return ASTER_KERNEL_RHI_VALIDATION_MISSING_RESOURCE;
  case aster::rhi::ResourceLifetimeValidationKind::RetiredResourceUse:
    return ASTER_KERNEL_RHI_VALIDATION_RETIRED_RESOURCE_USE;
  case aster::rhi::ResourceLifetimeValidationKind::InvalidResourceState:
  default:
    return ASTER_KERNEL_RHI_VALIDATION_INVALID_RESOURCE_STATE;
  }
}

AsterKernelFrameDiagnosticSeverity
rhiValidationSeverity(const aster::rhi::ResourceLifetimeValidationSeverity severity) {
  switch (severity) {
  case aster::rhi::ResourceLifetimeValidationSeverity::Warning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING;
  case aster::rhi::ResourceLifetimeValidationSeverity::Error:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR;
  case aster::rhi::ResourceLifetimeValidationSeverity::Info:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_INFO;
  }
}

AsterKernelFrameDiagnosticSeverity
diagnosticSeverity(const aster::FrameDiagnosticSeverity severity) {
  switch (severity) {
  case aster::FrameDiagnosticSeverity::Warning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING;
  case aster::FrameDiagnosticSeverity::Error:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR;
  case aster::FrameDiagnosticSeverity::Info:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_INFO;
  }
}

AsterKernelFrameDiagnosticKind diagnosticKind(const aster::FrameDiagnosticKind kind) {
  switch (kind) {
  case aster::FrameDiagnosticKind::MaterialVariantFallback:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_MATERIAL_VARIANT_FALLBACK;
  case aster::FrameDiagnosticKind::TranslucentSortChanged:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_TRANSLUCENT_SORT_CHANGED;
  case aster::FrameDiagnosticKind::NearPlaneClipping:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NEAR_PLANE_CLIPPING;
  case aster::FrameDiagnosticKind::ResourceLifetimeHazard:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_RESOURCE_LIFETIME_HAZARD;
  case aster::FrameDiagnosticKind::CapabilityMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_CAPABILITY_MISMATCH;
  case aster::FrameDiagnosticKind::ClusteredLightingFallback:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_CLUSTERED_LIGHTING_FALLBACK;
  case aster::FrameDiagnosticKind::MathContract:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_MATH_CONTRACT;
  case aster::FrameDiagnosticKind::NonFiniteWorldMatrix:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NON_FINITE_WORLD_MATRIX;
  case aster::FrameDiagnosticKind::SingularNormalMatrix:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_SINGULAR_NORMAL_MATRIX;
  case aster::FrameDiagnosticKind::NegativeScaleTangentFlip:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NEGATIVE_SCALE_TANGENT_FLIP;
  case aster::FrameDiagnosticKind::ProjectionConventionMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_PROJECTION_CONVENTION_MISMATCH;
  case aster::FrameDiagnosticKind::ViewportOriginMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_VIEWPORT_ORIGIN_MISMATCH;
  case aster::FrameDiagnosticKind::BackendProjectionDrift:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_PROJECTION_DRIFT;
  case aster::FrameDiagnosticKind::PredicateUncertainty:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_PREDICATE_UNCERTAINTY;
  case aster::FrameDiagnosticKind::AssetProvenanceWarning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_ASSET_PROVENANCE_WARNING;
  case aster::FrameDiagnosticKind::TextureRoleDegraded:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_TEXTURE_ROLE_DEGRADED;
  case aster::FrameDiagnosticKind::MeshAttributeDegraded:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_MESH_ATTRIBUTE_DEGRADED;
  case aster::FrameDiagnosticKind::SurfacePresentationWarning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_SURFACE_PRESENTATION_WARNING;
  case aster::FrameDiagnosticKind::BackendFallback:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_FALLBACK;
  }
}

void setForceRendererEnvironment(const std::uint32_t flags) {
  if ((flags & ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE) != 0u) {
#if defined(_WIN32)
    (void)_putenv_s("ASTER_FORCE_SOFTWARE_RENDERER", "1");
#else
    (void)setenv("ASTER_FORCE_SOFTWARE_RENDERER", "1", 1);
#endif
  }
  if ((flags & ASTER_KERNEL_RENDERER_FLAG_FORCE_NULL) != 0u) {
#if defined(_WIN32)
    (void)_putenv_s("ASTER_FORCE_NULL_RENDERER", "1");
#else
    (void)setenv("ASTER_FORCE_NULL_RENDERER", "1", 1);
#endif
  }
}

AsterFrameStats abiFrameStats(const aster::FrameStats &stats) {
  return {.size = sizeof(AsterFrameStats),
          .version = ASTER_KERNEL_STRUCT_VERSION_1,
          .frame_seconds = stats.frame_seconds,
          .framebuffer_width = static_cast<std::uint32_t>(std::max(stats.framebuffer_width, 0)),
          .framebuffer_height = static_cast<std::uint32_t>(std::max(stats.framebuffer_height, 0)),
          .draw_calls = stats.draw_calls,
          .visible_objects = stats.visible_objects,
          .culled_objects = stats.culled_objects,
          .graph_passes = stats.graph_passes,
          .graph_resources = stats.graph_resources,
          .graph_barriers = stats.graph_barriers,
          .graph_transient_resources = stats.graph_transient_resources,
          .backend_feature_mask = stats.backend_feature_mask,
          .backend = static_cast<AsterKernelBackendKind>(stats.backend_kind_value),
          .rust_plan_seconds = stats.rust_plan_seconds,
          .graph_compile_seconds = stats.graph_compile_seconds,
          .render_encode_seconds = stats.render_encode_seconds};
}

aster::CpuMesh customMeshFromDesc(const AsterMeshDesc &desc) {
  aster::CpuMesh mesh;
  if (desc.vertices.data == nullptr || desc.vertices.size == 0u) {
    return mesh;
  }
  if (desc.vertices.stride < sizeof(AsterVertex)) {
    return mesh;
  }
  const auto *vertices = static_cast<const unsigned char *>(desc.vertices.data);
  mesh.vertices.reserve(desc.vertices.size);
  for (std::size_t i = 0; i < desc.vertices.size; ++i) {
    const auto *vertex = reinterpret_cast<const AsterVertex *>(vertices + i * desc.vertices.stride);
    mesh.vertices.push_back({.position = vec(vertex->position),
                             .normal = vec(vertex->normal),
                             .uv = {vertex->uv.x, vertex->uv.y},
                             .tangent = {vertex->tangent.x, vertex->tangent.y, vertex->tangent.z,
                                         vertex->tangent.w},
                             .ambient_occlusion = vertex->ambient_occlusion});
  }
  if (desc.indices.data != nullptr && desc.indices.size > 0u &&
      desc.indices.stride >= sizeof(std::uint32_t)) {
    const auto *indices = static_cast<const unsigned char *>(desc.indices.data);
    mesh.indices.reserve(desc.indices.size);
    for (std::size_t i = 0; i < desc.indices.size; ++i) {
      const auto *index = reinterpret_cast<const std::uint32_t *>(indices + i * desc.indices.stride);
      mesh.indices.push_back(*index);
    }
  }
  return mesh;
}

std::pair<std::uint32_t, std::uint32_t> framebufferSizeFor(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return {640u, 360u};
  }
  if (window->headless || window->window == nullptr) {
    return {std::max(window->width, 1u), std::max(window->height, 1u)};
  }
  const auto [width, height] = window->window->framebufferSize();
  return {static_cast<std::uint32_t>(std::max(width, 1)),
          static_cast<std::uint32_t>(std::max(height, 1))};
}

aster::NativeWindowSurface nativeSurfaceFor(const AsterWindowHandle window) {
  if (!validWindow(window) || window->headless || window->window == nullptr) {
    const auto [width, height] = framebufferSizeFor(window);
    return {.width = static_cast<int>(width),
            .height = static_cast<int>(height),
            .vsync = validWindow(window) ? window->vsync : true};
  }
  return window->window->nativeSurface();
}

} // namespace

extern "C" {

AsterAbiVersion aster_kernel_abi_version(void) {
  return {ASTER_KERNEL_ABI_MAJOR, ASTER_KERNEL_ABI_MINOR, ASTER_KERNEL_ABI_PATCH};
}

AsterStatus aster_kernel_status_ok(void) {
  return makeStatus(ASTER_STATUS_OK, "ok");
}

AsterStatus aster_kernel_status_from_code(const AsterStatusCode code) {
  switch (code) {
  case ASTER_STATUS_OK:
    return makeStatus(ASTER_STATUS_OK, "ok");
  case ASTER_STATUS_INVALID_ARGUMENT:
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "invalid argument");
  case ASTER_STATUS_UNSUPPORTED:
    return makeStatus(ASTER_STATUS_UNSUPPORTED, "unsupported");
  case ASTER_STATUS_OUT_OF_MEMORY:
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "out of memory");
  case ASTER_STATUS_ABI_MISMATCH:
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "ABI mismatch");
  case ASTER_STATUS_VALIDATION_ERROR:
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "validation error");
  case ASTER_STATUS_CAPABILITY_MISMATCH:
    return makeStatus(ASTER_STATUS_CAPABILITY_MISMATCH, "capability mismatch");
  case ASTER_STATUS_LIFETIME_ERROR:
    return makeStatus(ASTER_STATUS_LIFETIME_ERROR, "lifetime error");
  case ASTER_STATUS_INTERNAL_ERROR:
  default:
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "internal error");
  }
}

AsterMathPolicy aster_kernel_math_default_policy(void) {
  const aster::MathPolicy policy = aster::defaultMathPolicy();
  return {sizeof(AsterMathPolicy), ASTER_KERNEL_STRUCT_VERSION_1, policy.absolute_epsilon,
          policy.relative_epsilon, policy.max_iterations, policy.deterministic ? 1u : 0u,
          policy.debug_strict ? 1u : 0u};
}

AsterStatus aster_kernel_math_vec3_dot(const AsterVec3 lhs, const AsterVec3 rhs,
                                       float *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = aster::dot(vec(lhs), vec(rhs));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_cross(const AsterVec3 lhs, const AsterVec3 rhs,
                                         AsterVec3 *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = abiVec(aster::cross(vec(lhs), vec(rhs)));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_length(const AsterVec3 value, float *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = aster::length(vec(value));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_normalize(const AsterVec3 value,
                                             const AsterMathPolicy *policy,
                                             AsterVec3 *out_value,
                                             AsterMathDiagnostics *out_diagnostics) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  const aster::MathPolicy internal_policy = mathPolicy(policy);
  const aster::MathResult<aster::Vec3> result =
      aster::safeNormalize(vec(value), internal_policy.absolute_epsilon);
  *out_value = abiVec(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_identity(AsterMat4 *out_matrix) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  *out_matrix = abiMat4(aster::identity());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_multiply(const AsterMat4 *lhs, const AsterMat4 *rhs,
                                            AsterMat4 *out_matrix) {
  if (lhs == nullptr || rhs == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix pointer is null");
  }
  *out_matrix = abiMat4(mat4(*lhs) * mat4(*rhs));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_inverse(const AsterMat4 *matrix,
                                           const AsterMathPolicy *policy,
                                           AsterMat4 *out_matrix,
                                           AsterMathDiagnostics *out_diagnostics) {
  if (matrix == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix pointer is null");
  }
  const aster::MathResult<aster::Mat4> result =
      aster::inverse(mat4(*matrix), mathPolicy(policy));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_compose_trs(const AsterTransform *transform,
                                               AsterMat4 *out_matrix) {
  if (transform == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "transform or out_matrix is null");
  }
  const aster::Mat4 matrix = aster::translation(vec(transform->position)) *
                             aster::mat4FromQuat(quat(transform->rotation)) *
                             aster::scale(vec(transform->scale));
  *out_matrix = abiMat4(matrix);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_decompose_trs(
    const AsterMat4 *matrix, AsterTransform *out_transform, AsterVec3 *out_skew,
    AsterVec4 *out_perspective, AsterMathDiagnostics *out_diagnostics) {
  if (matrix == nullptr || out_transform == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix or out_transform is null");
  }
  const aster::Mat4 m = mat4(*matrix);
  const aster::Vec3 translation_value{m.m[12], m.m[13], m.m[14]};
  aster::Vec3 columns[3] = {{m.m[0], m.m[1], m.m[2]},
                            {m.m[4], m.m[5], m.m[6]},
                            {m.m[8], m.m[9], m.m[10]}};
  const aster::Vec3 scale_value{aster::length(columns[0]), aster::length(columns[1]),
                                aster::length(columns[2])};
  if (scale_value.x <= 0.000001f || scale_value.y <= 0.000001f ||
      scale_value.z <= 0.000001f) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "TRS decomposition requires non-zero scale axes."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  columns[0] /= scale_value.x;
  columns[1] /= scale_value.y;
  columns[2] /= scale_value.z;
  const aster::Quat rotation = aster::quatFromMat3(aster::mat3FromColumns(columns[0], columns[1],
                                                                          columns[2]));
  out_transform->position = abiVec(translation_value);
  out_transform->rotation = abiQuat(rotation);
  out_transform->scale = abiVec(scale_value);
  if (out_skew != nullptr) {
    *out_skew = {0.0f, 0.0f, 0.0f};
  }
  if (out_perspective != nullptr) {
    *out_perspective = {0.0f, 0.0f, 0.0f, 1.0f};
  }
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_perspective(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane, const AsterMathCoordinateHandedness handedness,
    const AsterMathClipDepthRange depth_range, const AsterMathDepthDirection depth_direction,
    AsterMat4 *out_matrix, AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result = aster::perspective(
      vertical_fov_radians, aspect_ratio, near_plane, far_plane,
      projectionPolicy(handedness, depth_range, depth_direction));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_orthographic(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane, const AsterMathCoordinateHandedness handedness,
    const AsterMathClipDepthRange depth_range, const AsterMathDepthDirection depth_direction,
    AsterMat4 *out_matrix, AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result = aster::orthographic(
      left, right, bottom, top, near_plane, far_plane,
      projectionPolicy(handedness, depth_range, depth_direction));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_look_at(
    const AsterVec3 eye, const AsterVec3 target, const AsterVec3 up,
    const AsterMathCoordinateHandedness handedness, AsterMat4 *out_matrix,
    AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result =
      aster::lookAt(vec(eye), vec(target), vec(up),
                    handedness == ASTER_MATH_COORDINATE_LEFT_HANDED
                        ? aster::CoordinateHandedness::LeftHanded
                        : aster::CoordinateHandedness::RightHanded);
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_world_to_screen(const AsterWorldPoint point,
                                              const AsterMat4 *world_to_clip,
                                              const AsterViewport *viewport_desc,
                                              AsterScreenPoint *out_screen,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (world_to_clip == nullptr || viewport_desc == nullptr || out_screen == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world_to_screen pointer is null");
  }
  const aster::MathResult<aster::ScreenPoint> result =
      aster::project(worldPoint(point), aster::WorldToClip{mat4(*world_to_clip)},
                     viewport(viewport_desc));
  *out_screen = abiScreenPoint(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_screen_to_world(const AsterScreenPoint screen,
                                              const AsterMat4 *clip_to_world,
                                              const AsterViewport *viewport_desc,
                                              AsterWorldPoint *out_point,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (clip_to_world == nullptr || viewport_desc == nullptr || out_point == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "screen_to_world pointer is null");
  }
  const aster::MathResult<aster::WorldPoint> result =
      aster::unproject(screenPoint(screen), aster::ClipToWorld{mat4(*clip_to_world)},
                       viewport(viewport_desc));
  *out_point = abiWorldPoint(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_screen_to_world_ray(
    const AsterScreenPoint screen, const AsterMat4 *clip_to_world,
    const AsterViewport *viewport_desc, const AsterProjectionConvention *convention,
    const AsterWorldPoint perspective_eye, AsterWorldRay *out_ray,
    AsterMathDiagnostics *out_diagnostics) {
  if (clip_to_world == nullptr || viewport_desc == nullptr || out_ray == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "screen_to_world_ray pointer is null");
  }
  const aster::MathResult<aster::WorldRay> result = aster::screenRay(
      screenPoint(screen), aster::ClipToWorld{mat4(*clip_to_world)}, viewport(viewport_desc),
      projectionPolicy(convention), aster::RayOriginPolicy::PerspectiveEye,
      worldPoint(perspective_eye));
  *out_ray = abiWorldRay(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_quat_identity(AsterQuat *out_quat) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  *out_quat = abiQuat(aster::identityQuat());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_axis_angle(const AsterVec3 axis, const float radians,
                                              AsterQuat *out_quat,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  const aster::MathResult<aster::Quat> result = aster::axisAngleSafe(vec(axis), radians);
  *out_quat = abiQuat(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_quat_slerp(const AsterQuat lhs, const AsterQuat rhs, const float t,
                                         AsterQuat *out_quat) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  *out_quat = abiQuat(aster::slerp(quat(lhs), quat(rhs), t));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_rotate_vec3(const AsterQuat rotation, const AsterVec3 value,
                                               AsterVec3 *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = abiVec(aster::rotate(quat(rotation), vec(value)));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_inverse(const AsterQuat value, AsterQuat *out_quat,
                                           AsterMathDiagnostics *out_diagnostics) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  const aster::MathResult<aster::Quat> result = aster::inverse(quat(value));
  *out_quat = abiQuat(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_intersect_ray_plane(
    const AsterRay3 ray, const AsterPlane3 plane, float *out_distance, AsterVec3 *out_point,
    AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_point == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-plane output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit =
      aster::intersectRayPlane(vec(ray.origin), direction.value, {vec(plane.normal),
                                                                  plane.distance});
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the plane."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_point = abiVec(hit.point);
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_intersect_ray_triangle(
    const AsterRay3 ray, const AsterVec3 a, const AsterVec3 b, const AsterVec3 c,
    float *out_distance, AsterVec3 *out_barycentric, AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_barycentric == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-triangle output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit =
      aster::intersectRayTriangle(vec(ray.origin), direction.value, vec(a), vec(b), vec(c));
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the triangle."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_barycentric = {hit.barycentric.u, hit.barycentric.v, hit.barycentric.w};
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_intersect_ray_sphere(
    const AsterRay3 ray, const AsterSphere3 sphere, float *out_distance, AsterVec3 *out_point,
    AsterVec3 *out_normal, AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_point == nullptr || out_normal == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-sphere output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit = aster::intersectRaySphere(
      vec(ray.origin), direction.value, {vec(sphere.center), sphere.radius});
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the sphere."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_point = abiVec(hit.point);
  *out_normal = abiVec(hit.normal);
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                       AsterEngineHandle *out_engine) {
  if (out_engine == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_engine is null");
  }
  *out_engine = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "engine descriptor version is not supported");
  }
  if (!validStringView(desc->application_name)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "application_name has a size but no data");
  }

  try {
    *out_engine = new AsterEngineHandle__();
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "engine allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "engine creation failed");
  }
  setLastStatus(*out_engine, aster_kernel_status_ok());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_destroy(const AsterEngineHandle engine) {
  return destroyHandle(engine, kEngineMagic);
}

AsterStatus aster_kernel_engine_last_status(const AsterEngineHandle engine) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  return engine->last_status;
}

AsterStatus aster_kernel_engine_validation_event_count(const AsterEngineHandle engine,
                                                       std::size_t *out_count) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_count == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_count is null");
  }
  *out_count = engine->validation_events.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_validation_event(const AsterEngineHandle engine,
                                                 const std::size_t index,
                                                 AsterValidationEvent *out_event) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  return validationEventAt(engine->validation_events, index, out_event);
}

AsterStatus aster_kernel_world_create(const AsterEngineHandle engine, const AsterWorldDesc *desc,
                                      AsterWorldHandle *out_world) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_world == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_world is null");
  }
  *out_world = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world label has a size but no data");
  }
  aster::WorldStateConfig config;
  config.fixed_step_seconds = desc->fixed_step_seconds > 0.0 ? desc->fixed_step_seconds
                                                             : 1.0 / 60.0;
  config.seed = desc->seed != 0u ? desc->seed : config.seed;
  config.label = stringFromView(desc->debug_label);
  try {
    *out_world = new AsterWorldHandle__(engine, std::move(config));
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "world allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "world creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_world_advance(const AsterWorldHandle world,
                                       const AsterWorldAdvanceDesc *desc,
                                       AsterWorldAdvanceResult *out_result) {
  if (!validWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world handle is invalid");
  }
  if (!validWorldAdvanceDesc(desc) || !validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world advance struct version is not supported");
  }
  const bool has_continuity_budget =
      abiStructHasField(desc->size, offsetof(AsterWorldAdvanceDesc, perceptual_continuity_budget),
                        sizeof(desc->perceptual_continuity_budget)) &&
      hasPerceptualContinuityBudget(desc->perceptual_continuity_budget);
  if (has_continuity_budget &&
      !validPerceptualContinuityBudget(desc->perceptual_continuity_budget)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "world advance perceptual continuity budget version is not supported");
  }

  std::uint64_t input_intent_hash = mixWorldEvidence(desc->input_event_hash, desc->player_intent_hash);
  input_intent_hash = mixWorldEvidence(input_intent_hash, desc->actor_state_delta_hash);
  input_intent_hash = mixWorldEvidence(input_intent_hash, desc->sensory_event_hash);
  input_intent_hash = mixWorldEvidence(input_intent_hash, desc->visibility_set_hash);
  if (has_continuity_budget) {
    input_intent_hash =
        mixWorldEvidence(input_intent_hash,
                         continuityBudgetEvidenceHash(desc->perceptual_continuity_budget));
  }
  const aster::WorldTickResult result =
      world->world.tick({.tick = desc->epoch,
                         .delta_seconds = desc->delta_seconds,
                         .input_event_hash = input_intent_hash,
                         .asset_lineage_hash = desc->asset_lineage_hash});

  world->actor_state_delta_hash = desc->actor_state_delta_hash;
  world->sensory_event_hash = desc->sensory_event_hash;
  world->visibility_set_hash = desc->visibility_set_hash;
  world->actor_delta_count = desc->actor_state_delta_hash == 0u ? 0u : 1u;
  world->streaming_region_id = desc->streaming_region_id;
  if (has_continuity_budget) {
    storeContinuityBudget(*world, desc->perceptual_continuity_budget);
  }
  world->diagnostic = result.diagnostic;
  if (result.accepted) {
    std::uint64_t transition_hash = mixWorldEvidence(result.world_hash, result.trace_hash);
    transition_hash = mixWorldEvidence(transition_hash, result.tick);
    transition_hash = mixWorldEvidence(transition_hash, desc->player_intent_hash);
    transition_hash = mixWorldEvidence(transition_hash, desc->actor_state_delta_hash);
    transition_hash = mixWorldEvidence(transition_hash, desc->sensory_event_hash);
    transition_hash = mixWorldEvidence(transition_hash, desc->visibility_set_hash);
    if (has_continuity_budget) {
      transition_hash =
          mixWorldEvidence(transition_hash,
                           continuityBudgetEvidenceHash(desc->perceptual_continuity_budget));
    }
    world->world_transition_hash = transition_hash;
  }

  out_result->accepted = result.accepted ? 1u : 0u;
  out_result->epoch = result.tick;
  out_result->time_seconds = result.time_seconds;
  out_result->world_hash = result.world_hash;
  out_result->trace_hash = result.trace_hash;
  out_result->world_transition_hash = world->world_transition_hash;
  out_result->diagnostic = worldScratch(world, result.diagnostic);
  return result.accepted ? aster_kernel_status_ok()
                         : makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                                      "world advance was rejected");
}

AsterStatus aster_kernel_world_record_region_gate(const AsterWorldHandle world,
                                                  const AsterWorldRegionGateReport *report) {
  if (!validWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world handle is invalid");
  }
  if (!validWorldRegionGateReport(report) || !validStruct(&report->navigation) ||
      !validStruct(&report->perceptual_budget)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "world region gate report version is not supported");
  }
  const bool has_continuity_budget =
      abiStructHasField(report->size,
                        offsetof(AsterWorldRegionGateReport, perceptual_continuity_budget),
                        sizeof(report->perceptual_continuity_budget)) &&
      hasPerceptualContinuityBudget(report->perceptual_continuity_budget);
  if (has_continuity_budget &&
      !validPerceptualContinuityBudget(report->perceptual_continuity_budget)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "world region gate continuity budget version is not supported");
  }
  if (!validStringView(report->diagnostic) || !validStringView(report->navigation.diagnostic) ||
      !validStringView(report->perceptual_budget.diagnostic) ||
      (has_continuity_budget &&
       !validStringView(report->perceptual_continuity_budget.diagnostic))) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "world region gate diagnostics have a size but no data");
  }

  std::uint64_t report_hash = mixWorldEvidence(report->probe_trace_hash, report->region_id);
  report_hash = mixWorldEvidence(report_hash, report->navigation.report_hash);
  report_hash = mixWorldEvidence(report_hash, report->encounter_budget_hash);
  report_hash = mixWorldEvidence(report_hash, report->resource_probe_hash);
  report_hash = mixWorldEvidence(report_hash, report->perceptual_budget.report_hash);
  if (has_continuity_budget) {
    report_hash =
        mixWorldEvidence(report_hash,
                         continuityBudgetEvidenceHash(report->perceptual_continuity_budget));
  }
  const bool accepted = report->verdict == ASTER_WORLD_REGION_GATE_ACCEPTED &&
                        report->navigation.valid != 0u &&
                        report->perceptual_budget.accepted != 0u &&
                        (!has_continuity_budget ||
                         report->perceptual_continuity_budget.accepted != 0u);
  world->world.noteRegionGate(report->region_id, accepted, report_hash,
                              stringFromView(report->diagnostic));
  world->gate_verdict =
      accepted ? ASTER_WORLD_REGION_GATE_ACCEPTED : ASTER_WORLD_REGION_GATE_QUARANTINED;
  world->streaming_region_id = report->region_id;
  world->navigation_valid = report->navigation.valid;
  world->navigation_checked_steps = report->navigation.checked_steps;
  world->navigation_blocked_steps = report->navigation.blocked_steps;
  world->navigation_report_hash = report->navigation.report_hash;
  world->navigation_diagnostic = stringFromView(report->navigation.diagnostic);
  world->encounter_budget_hash = report->encounter_budget_hash;
  world->resource_probe_hash = report->resource_probe_hash;
  world->perceptual_accepted = report->perceptual_budget.accepted;
  world->perceptual_salience_score = report->perceptual_budget.salience_score;
  world->perceptual_minimum_salience = report->perceptual_budget.minimum_salience;
  world->perceptual_report_hash = report->perceptual_budget.report_hash;
  world->perceptual_diagnostic = stringFromView(report->perceptual_budget.diagnostic);
  if (has_continuity_budget) {
    storeContinuityBudget(*world, report->perceptual_continuity_budget);
  }
  world->diagnostic = stringFromView(report->diagnostic);
  world->world_transition_hash =
      mixWorldEvidence(world->world_transition_hash == 0u ? world->world.worldHash()
                                                          : world->world_transition_hash,
                       report_hash);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_world_extract_render(const AsterWorldHandle world,
                                              const AsterWorldRenderExtractionDesc *desc,
                                              AsterWorldRenderExtraction *out_extraction) {
  if (!validWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_extraction)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "world render extraction struct version is not supported");
  }
  if (world->world_transition_hash == 0u) {
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "world render extraction requires an accepted transition");
  }
  world->world.noteRenderableExtraction(desc->extraction_hash, desc->frame_submission_hash);
  world->render_extraction_hash = desc->extraction_hash;
  out_extraction->world_transition_hash = world->world_transition_hash;
  out_extraction->epoch = world->world.currentTick();
  out_extraction->trace_hash = world->world.traceHash();
  out_extraction->visibility_set_hash = desc->visibility_set_hash;
  out_extraction->extraction_hash = desc->extraction_hash;
  out_extraction->frame_submission_hash = desc->frame_submission_hash;
  out_extraction->streaming_region_id = world->streaming_region_id;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_world_forensics(const AsterWorldHandle world,
                                         AsterWorldForensics *out_forensics) {
  if (!validWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world handle is invalid");
  }
  if (!validWorldForensics(out_forensics)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world forensics version is not supported");
  }
  out_forensics->world_transition_hash = world->world_transition_hash;
  out_forensics->epoch = world->world.currentTick();
  out_forensics->world_hash = world->world.worldHash();
  out_forensics->trace_hash = world->world.traceHash();
  out_forensics->actor_state_delta_hash = world->actor_state_delta_hash;
  out_forensics->actor_delta_count = world->actor_delta_count;
  out_forensics->render_extraction_hash = world->render_extraction_hash;
  out_forensics->streaming_region_id = world->streaming_region_id;
  out_forensics->generated_region_gate = world->gate_verdict;
  out_forensics->navigation = {.size = sizeof(AsterNavValidityReport),
                               .version = ASTER_KERNEL_STRUCT_VERSION_1,
                               .valid = world->navigation_valid,
                               .checked_steps = world->navigation_checked_steps,
                               .blocked_steps = world->navigation_blocked_steps,
                               .report_hash = world->navigation_report_hash,
                               .diagnostic = worldScratch(world, world->navigation_diagnostic)};
  out_forensics->encounter_budget_hash = world->encounter_budget_hash;
  out_forensics->resource_probe_hash = world->resource_probe_hash;
  out_forensics->perceptual_budget = {
      .size = sizeof(AsterPerceptualBudget),
      .version = ASTER_KERNEL_STRUCT_VERSION_1,
      .accepted = world->perceptual_accepted,
      .salience_score = world->perceptual_salience_score,
      .minimum_salience = world->perceptual_minimum_salience,
      .report_hash = world->perceptual_report_hash,
      .diagnostic = worldScratch(world, world->perceptual_diagnostic)};
  out_forensics->diagnostic = worldScratch(world, world->diagnostic);
  if (abiStructHasField(out_forensics->size, offsetof(AsterWorldForensics, sensory_event_hash),
                        sizeof(out_forensics->sensory_event_hash))) {
    out_forensics->sensory_event_hash = world->sensory_event_hash;
  }
  if (abiStructHasField(out_forensics->size, offsetof(AsterWorldForensics, visibility_set_hash),
                        sizeof(out_forensics->visibility_set_hash))) {
    out_forensics->visibility_set_hash = world->visibility_set_hash;
  }
  if (abiStructHasField(out_forensics->size,
                        offsetof(AsterWorldForensics, perceptual_continuity_budget),
                        sizeof(out_forensics->perceptual_continuity_budget))) {
    out_forensics->perceptual_continuity_budget = {
        .size = sizeof(AsterPerceptualContinuityBudget),
        .version = ASTER_KERNEL_STRUCT_VERSION_1,
        .accepted = world->perceptual_continuity_accepted,
        .required_channel_mask = world->perceptual_continuity_required_channel_mask,
        .observed_channel_mask = world->perceptual_continuity_observed_channel_mask,
        .missing_channel_mask = world->perceptual_continuity_missing_channel_mask,
        .continuity_score = world->perceptual_continuity_score,
        .minimum_score = world->perceptual_continuity_minimum_score,
        .reaction_package_hash = world->reaction_package_hash,
        .material_memory_hash = world->material_memory_hash,
        .lighting_atmosphere_hash = world->lighting_atmosphere_hash,
        .ai_attention_hash = world->ai_attention_hash,
        .streaming_residency_lod_hash = world->streaming_residency_lod_hash,
        .resource_state_hash = world->resource_state_hash,
        .event_residue_hash = world->event_residue_hash,
        .readability_audit_hash = world->readability_audit_hash,
        .diagnostic = worldScratch(world, world->perceptual_continuity_diagnostic)};
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_world_destroy(const AsterWorldHandle world) {
  if (!validWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world handle is invalid");
  }
  world->magic = kRetiredMagic;
  delete world;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_create(const AsterEngineHandle engine,
                                             const AsterSystemWorldDesc *desc,
                                             AsterSystemWorldHandle *out_world) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_world == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_world is null");
  }
  *out_world = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "system world descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world label has a size but no data");
  }
  aster::WorldStateConfig config;
  config.fixed_step_seconds = desc->fixed_step_seconds > 0.0 ? desc->fixed_step_seconds
                                                             : 1.0 / 60.0;
  config.seed = desc->seed != 0u ? desc->seed : config.seed;
  config.label = stringFromView(desc->debug_label);
  try {
    *out_world = new AsterSystemWorldHandle__(engine, std::move(config));
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "system world allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "system world creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_tick(const AsterSystemWorldHandle world,
                                           const AsterSystemTickDesc *desc,
                                           AsterSystemTickResult *out_result) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "system world tick struct version is not supported");
  }
  const aster::WorldTickResult result = world->world.tick({.tick = desc->tick,
                                                           .delta_seconds = desc->delta_seconds,
                                                           .input_event_hash = desc->input_event_hash,
                                                           .asset_lineage_hash = desc->asset_lineage_hash,
                                                           .extraction_hash = desc->extraction_hash,
                                                           .frame_submission_hash = desc->frame_submission_hash});
  out_result->accepted = result.accepted ? 1u : 0u;
  out_result->tick = result.tick;
  out_result->time_seconds = result.time_seconds;
  out_result->events_emitted = result.events_emitted;
  out_result->world_hash = result.world_hash;
  out_result->trace_hash = result.trace_hash;
  out_result->diagnostic = systemWorldScratch(world, result.diagnostic);
  if (!result.accepted) {
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "system world tick was rejected");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_entity_create(const AsterSystemWorldHandle world,
                                                    const AsterStringView label,
                                                    AsterSystemEntityHandle *out_entity) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (out_entity == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_entity is null");
  }
  if (!validStringView(label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "entity label has a size but no data");
  }
  *out_entity = abiWorldEntity(world->world.createEntity(stringFromView(label)));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_entity_query(const AsterSystemWorldHandle world,
                                                   const AsterSystemEntityHandle entity,
                                                   AsterSystemEntityInfo *out_info) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "system entity info version is not supported");
  }
  const aster::WorldEntityHandle handle = worldEntity(entity);
  const std::optional<std::string_view> label = world->world.entityLabel(handle);
  out_info->handle = entity;
  out_info->alive = label.has_value() ? 1u : 0u;
  out_info->label =
      label.has_value() ? systemWorldScratch(world, std::string(*label)) : AsterStringView{};
  return label.has_value() ? aster_kernel_status_ok()
                           : makeStatus(ASTER_STATUS_LIFETIME_ERROR,
                                        "system entity handle is stale or invalid");
}

AsterStatus aster_kernel_system_world_entity_destroy(const AsterSystemWorldHandle world,
                                                     const AsterSystemEntityHandle entity) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!world->world.destroyEntity(worldEntity(entity), "kernel-destroy")) {
    return makeStatus(ASTER_STATUS_LIFETIME_ERROR, "system entity handle is stale or invalid");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_transaction_begin(
    const AsterSystemWorldHandle world, const AsterSystemTransactionDesc *desc,
    AsterSystemTransactionInfo *out_info) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "system transaction struct version is not supported");
  }
  if (!validStringView(desc->label) || !validStringView(desc->provenance)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "system transaction strings have a size but no data");
  }
  AsterStatus span_status = aster_kernel_status_ok();
  std::vector<aster::WorldComponentAccess> accesses =
      worldComponentAccesses(desc->accesses, &span_status);
  if (span_status.code != ASTER_STATUS_OK) {
    return span_status;
  }
  const aster::WorldTransactionInfo info =
      world->world.beginTransaction({.label = stringFromView(desc->label),
                                     .provenance = stringFromView(desc->provenance),
                                     .accesses = std::move(accesses)});
  *out_info = abiWorldTransactionInfo(world, info);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_transaction_append(
    const AsterSystemWorldHandle world, const std::uint64_t transaction_id,
    const AsterSystemComponentAccess *access) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(access)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "component access version is not supported");
  }
  if (!validStringView(access->component) || !validStringView(access->subject)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "component access strings have a size but no data");
  }
  std::string diagnostic;
  if (!world->world.appendAccess(transaction_id, worldComponentAccess(*access), &diagnostic)) {
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "system transaction access was rejected");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_transaction_commit(
    const AsterSystemWorldHandle world, const std::uint64_t transaction_id,
    AsterSystemTransactionInfo *out_info) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "system transaction info version is not supported");
  }
  const aster::WorldTransactionInfo info = world->world.commitTransaction(transaction_id);
  *out_info = abiWorldTransactionInfo(world, info);
  if (!info.committed) {
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "system transaction was rejected");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_transaction_abort(
    const AsterSystemWorldHandle world, const std::uint64_t transaction_id,
    const AsterStringView reason, AsterSystemTransactionInfo *out_info) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "system transaction info version is not supported");
  }
  if (!validStringView(reason)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "abort reason has a size but no data");
  }
  const aster::WorldTransactionInfo info =
      world->world.abortTransaction(transaction_id, stringFromView(reason));
  *out_info = abiWorldTransactionInfo(world, info);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_trace_counts(const AsterSystemWorldHandle world,
                                                   AsterSystemTraceCounts *out_counts) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "system trace counts version is not supported");
  }
  const aster::WorldTraceCounts counts = world->world.counts();
  out_counts->event_count = counts.event_count;
  out_counts->entity_count = counts.entity_count;
  out_counts->live_entity_count = counts.live_entity_count;
  out_counts->transaction_count = counts.transaction_count;
  out_counts->validation_event_count = counts.validation_event_count;
  out_counts->tick = counts.tick;
  out_counts->time_seconds = counts.time_seconds;
  out_counts->world_hash = counts.world_hash;
  out_counts->trace_hash = counts.trace_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_trace_event(const AsterSystemWorldHandle world,
                                                  const std::size_t index,
                                                  AsterSystemTraceEvent *out_event) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "system trace event version is not supported");
  }
  const std::vector<aster::WorldTraceEvent> &events = world->world.traceEvents();
  if (index >= events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system trace event index is out of range");
  }
  const aster::WorldTraceEvent &event = events[index];
  out_event->kind = abiWorldTraceEventKind(event.kind);
  out_event->sequence = event.sequence;
  out_event->tick = event.tick;
  out_event->transaction_id = event.transaction_id;
  out_event->entity = abiWorldEntity(event.entity);
  out_event->label = systemWorldScratch(world, event.label);
  out_event->subject = systemWorldScratch(world, event.subject);
  out_event->component = systemWorldScratch(world, event.component);
  out_event->detail = systemWorldScratch(world, event.detail);
  out_event->parent_world_hash = event.parent_world_hash;
  out_event->world_hash = event.world_hash;
  out_event->trace_hash = event.trace_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_save_snapshot(const AsterSystemWorldHandle world,
                                                    const AsterWorldSnapshotDesc *desc) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world snapshot descriptor version is not supported");
  }
  if (!validStringView(desc->path) || desc->path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world snapshot path is invalid");
  }
  std::string diagnostic;
  if (!world->world.saveSnapshot(stringFromView(desc->path), &diagnostic)) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "world snapshot save failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_system_world_load_snapshot(const AsterSystemWorldHandle world,
                                                    const AsterWorldSnapshotDesc *desc,
                                                    AsterWorldMigrationReport *out_report) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_report)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world migration report version is not supported");
  }
  if (!validStringView(desc->path) || desc->path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world snapshot path is invalid");
  }
  const aster::WorldMigrationReport report = world->world.loadSnapshot(stringFromView(desc->path));
  out_report->loaded_schema_version = report.loaded_schema_version;
  out_report->current_schema_version = report.current_schema_version;
  out_report->migration_applied = report.migration_applied ? 1u : 0u;
  out_report->entity_count = report.entity_count;
  out_report->world_hash = report.world_hash;
  out_report->trace_hash = report.trace_hash;
  out_report->diagnostic = systemWorldScratch(world, report.diagnostic);
  return report.world_hash != 0u ? aster_kernel_status_ok()
                                 : makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                                              "world snapshot load failed");
}

AsterStatus aster_kernel_system_world_replay_trace(const AsterSystemWorldHandle world,
                                                   const AsterWorldSnapshotDesc *desc,
                                                   AsterWorldReplayReport *out_report) {
  if (!validSystemWorld(world)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_report)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "world replay report version is not supported");
  }
  if (!validStringView(desc->path) || desc->path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world snapshot path is invalid");
  }
  const aster::WorldReplayReport report =
      world->world.replaySnapshot(stringFromView(desc->path), desc->expected_world_hash);
  out_report->matched = report.matched ? 1u : 0u;
  out_report->events_replayed = report.events_replayed;
  out_report->expected_world_hash = report.expected_world_hash;
  out_report->actual_world_hash = report.actual_world_hash;
  out_report->actual_trace_hash = report.actual_trace_hash;
  out_report->diagnostic = systemWorldScratch(world, report.diagnostic);
  return report.matched ? aster_kernel_status_ok()
                        : makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                                     "world replay hash mismatch");
}

AsterStatus aster_kernel_window_create(const AsterWindowDesc *desc,
                                       AsterWindowHandle *out_window) {
  if (out_window == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_window is null");
  }
  *out_window = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "window descriptor version is not supported");
  }
  if (!validStringView(desc->title)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window title has a size but no data");
  }
  try {
    auto *window = new AsterWindowHandle__();
    window->headless = (desc->flags & ASTER_KERNEL_WINDOW_FLAG_HEADLESS) != 0u;
    window->vsync = desc->vsync != 0u;
    window->width = std::max(desc->width, 1u);
    window->height = std::max(desc->height, 1u);
    window->title = stringFromView(desc->title);
    if (!window->headless) {
      aster::EngineConfig config;
      config.application_name = window->title.empty() ? "Aster Kernel App" : window->title.c_str();
      config.initial_width = static_cast<int>(window->width);
      config.initial_height = static_cast<int>(window->height);
      config.enable_vsync = window->vsync;
      window->window = std::make_unique<aster::Window>(config);
    }
    *out_window = window;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "window allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "window creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_poll(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (!window->headless && window->window != nullptr) {
    window->window->pollEvents();
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_swap(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (!window->headless && window->window != nullptr) {
    window->window->swapBuffers();
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_set_vsync(const AsterWindowHandle window, const std::uint32_t enabled) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  window->vsync = enabled != 0u;
  if (!window->headless && window->window != nullptr) {
    window->window->setVsync(window->vsync);
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_framebuffer_size(const AsterWindowHandle window,
                                                 AsterExtent2D *out_size) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (out_size == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_size is null");
  }
  const auto [width, height] = framebufferSizeFor(window);
  *out_size = {width, height};
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_destroy(const AsterWindowHandle window) {
  return destroyHandle(window, kWindowMagic);
}

AsterStatus aster_kernel_scene_create(const AsterEngineHandle engine, AsterSceneHandle *out_scene) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_scene == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_scene is null");
  }
  *out_scene = nullptr;
  try {
    auto *scene = new AsterSceneHandle__();
    scene->owner = engine;
    *out_scene = scene;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "scene allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_clear(const AsterSceneHandle scene) {
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  scene->scene.objects().clear();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_add_object(const AsterSceneHandle scene,
                                          const AsterSceneObjectDesc *desc) {
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "scene object descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene object label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  if (!finiteVec3(desc->position) || !finiteVec3(desc->rotation) || !finiteVec3(desc->scale) ||
      desc->scale.x == 0.0f || desc->scale.y == 0.0f || desc->scale.z == 0.0f) {
    return failWithValidation(scene->owner, ASTER_STATUS_VALIDATION_ERROR,
                              "scene object transform is invalid",
                              ASTER_VALIDATION_INVALID_TRANSFORM, "scene.add_object", label);
  }
  if (desc->mesh != nullptr && !validMesh(desc->mesh)) {
    return failWithValidation(scene->owner, ASTER_STATUS_LIFETIME_ERROR,
                              "scene object mesh handle is invalid",
                              ASTER_VALIDATION_DESTROYED_HANDLE_USE, "scene.add_object", label);
  }
  if (!validMaterial(desc->material)) {
    return failWithValidation(scene->owner, ASTER_STATUS_VALIDATION_ERROR,
                              "scene object requires a valid material",
                              ASTER_VALIDATION_MISSING_REQUIRED_TEXTURE, "scene.add_object", label);
  }
  if (desc->pipeline != nullptr && !validPipeline(desc->pipeline)) {
    return failWithValidation(scene->owner, ASTER_STATUS_LIFETIME_ERROR,
                              "scene object pipeline handle is invalid",
                              ASTER_VALIDATION_DESTROYED_HANDLE_USE, "scene.add_object", label);
  }
  aster::RenderObject object;
  object.name = label;
  object.primitive = validMesh(desc->mesh) ? desc->mesh->primitive : meshPrimitive(desc->primitive);
  object.custom_mesh = validMesh(desc->mesh) ? desc->mesh->custom_mesh : nullptr;
  object.material = desc->material->material;
  object.transform.position = vec(desc->position);
  object.transform.rotation = aster::quatFromEulerXyz(vec(desc->rotation));
  object.transform.scale = vec(desc->scale);
  scene->scene.objects().push_back(std::move(object));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_destroy(const AsterSceneHandle scene) {
  return destroyHandle(scene, kSceneMagic);
}

AsterStatus aster_kernel_renderer_create(const AsterEngineHandle engine,
                                         const AsterRendererDesc *desc,
                                         AsterRendererHandle *out_renderer) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_renderer == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_renderer is null");
  }
  *out_renderer = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "renderer descriptor version is not supported");
  }
  try {
    setForceRendererEnvironment(desc->flags);
    auto *renderer = new AsterRendererHandle__();
    renderer->renderer = std::make_unique<aster::RenderDevice>();
    renderer->bound_window = validWindow(desc->window) ? desc->window : nullptr;
    renderer->renderer->initialize();
    if (validWindow(renderer->bound_window)) {
      (void)renderer->renderer->bindWindow(nativeSurfaceFor(renderer->bound_window));
    }
    *out_renderer = renderer;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "renderer allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "renderer creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus
aster_kernel_renderer_get_capabilities(const AsterRendererHandle renderer,
                                        AsterBackendCapabilities *out_capabilities) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capabilities)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "backend capabilities struct version is not supported");
  }
  const aster::RenderBackendCapabilities capabilities = renderer->renderer->backendCapabilities();
  out_capabilities->backend = backendKind(capabilities.kind);
  out_capabilities->flags = capabilityFlags(capabilities);
  out_capabilities->graph_resource_mask = capabilities.graph_resource_mask;
  out_capabilities->name = {capabilities.name, std::strlen(capabilities.name)};
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_get_backend_capability_table(
    const AsterRendererHandle renderer, AsterBackendCapabilityTable *out_capabilities) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capabilities)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "backend capability table struct version is not supported");
  }
  const aster::RenderBackendCapabilities capabilities = renderer->renderer->backendCapabilities();
  const aster::rhi::DeviceCapabilities &table = capabilities.capability_table;
  out_capabilities->backend = backendKind(capabilities.kind);
  out_capabilities->flags = capabilityFlags(capabilities);
  out_capabilities->graph_resource_mask = capabilities.graph_resource_mask;
  out_capabilities->name = {capabilities.name, std::strlen(capabilities.name)};
  out_capabilities->color_format_mask = table.color_format_mask;
  out_capabilities->depth_format_mask = table.depth_format_mask;
  out_capabilities->sample_count_mask = table.sample_count_mask;
  out_capabilities->sampler_filter_mask = table.sampler_filter_mask;
  out_capabilities->sampler_address_mode_mask = table.sampler_address_mode_mask;
  out_capabilities->blend_mode_mask = table.blend_mode_mask;
  out_capabilities->shader_model = shaderModel(table.shader_model);
  out_capabilities->presentation = presentationMode(table.presentation);
  out_capabilities->max_color_attachments = table.limits.max_color_attachments;
  out_capabilities->max_sampled_textures_per_material =
      table.limits.max_sampled_textures_per_material;
  out_capabilities->max_samplers_per_material = table.limits.max_samplers_per_material;
  out_capabilities->max_uniform_buffers_per_stage = table.limits.max_uniform_buffers_per_stage;
  out_capabilities->max_storage_buffers_per_stage = table.limits.max_storage_buffers_per_stage;
  out_capabilities->max_bind_groups = table.limits.max_bind_groups;
  out_capabilities->max_vertex_attributes = table.limits.max_vertex_attributes;
  out_capabilities->max_texture_dimension_2d = table.limits.max_texture_dimension_2d;
  out_capabilities->max_dynamic_uniform_bytes = table.limits.max_dynamic_uniform_bytes;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_render_frame(const AsterRendererHandle renderer,
                                               const AsterSceneHandle scene,
                                               const AsterCameraDesc *camera,
                                               const AsterRendererSettings *settings) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (!validCameraDesc(camera) || !validRendererSettings(settings)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "render frame struct version is not supported");
  }
  const AsterCameraDesc camera_desc = copyAbiStruct(camera);
  const AsterRendererSettings settings_desc = copyAbiStruct(settings);
  const bool has_continuity_budget =
      abiStructHasField(settings->size,
                        offsetof(AsterRendererSettings, perceptual_continuity_budget),
                        sizeof(settings_desc.perceptual_continuity_budget)) &&
      hasPerceptualContinuityBudget(settings_desc.perceptual_continuity_budget);
  if (has_continuity_budget &&
      !validPerceptualContinuityBudget(settings_desc.perceptual_continuity_budget)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "renderer perceptual continuity budget version is not supported");
  }
  if (settings_desc.render_target != nullptr) {
    return aster_kernel_renderer_render_frame_to_target(renderer, scene, settings_desc.render_target,
                                                        &camera_desc, &settings_desc);
  }
  try {
    auto [width, height] = framebufferSizeFor(renderer->bound_window);
    if (settings_desc.framebuffer_width > 0u) {
      width = settings_desc.framebuffer_width;
    }
    if (settings_desc.framebuffer_height > 0u) {
      height = settings_desc.framebuffer_height;
    }
    aster::OrbitCamera orbit;
    orbit.target = vec(camera_desc.target);
    orbit.yaw = camera_desc.yaw_radians;
    orbit.pitch = camera_desc.pitch_radians;
    orbit.radius = std::max(camera_desc.radius, 0.01f);
    orbit.vertical_fov = cameraVerticalFov(camera_desc, width, height);
    orbit.near_plane = camera_desc.near_plane > 0.0f ? camera_desc.near_plane : 0.01f;
    orbit.far_plane =
        camera_desc.far_plane > orbit.near_plane ? camera_desc.far_plane : 100.0f;
    if (hasRenderSettingFlag(settings_desc, ASTER_KERNEL_RENDER_SETTING_PRESENTATION_LENS) ||
        (camera_desc.camera_flags & ASTER_KERNEL_CAMERA_FLAG_USE_PHYSICAL_LENS) != 0u) {
      const float composition = std::clamp(
          camera_desc.composition_weight > 0.0f ? camera_desc.composition_weight : 0.58f, 0.0f,
          1.0f);
      const float scale_reference =
          std::clamp(camera_desc.scale_reference_m > 0.0f ? camera_desc.scale_reference_m : 1.80f,
                     0.25f, 64.0f);
      const float focal_length =
          std::clamp(camera_desc.focal_length_mm > 0.0f ? camera_desc.focal_length_mm : 46.0f,
                     16.0f, 240.0f);
      const float compression = std::clamp((focal_length - 35.0f) / 105.0f, 0.0f, 1.0f);
      orbit.radius = std::max(0.01f, orbit.radius * (1.0f + compression * 0.18f));
      orbit.target.y += (composition - 0.50f) * scale_reference * 0.16f;
      orbit.pitch = std::clamp(orbit.pitch - compression * 0.035f, aster::radians(-80.0f),
                               aster::radians(80.0f));
    }

    aster::RendererSettings render_settings = rendererSettingsFromAbi(settings_desc, camera_desc);

    renderer->renderer->prepareScene(scene->scene);
    const double frame_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - renderer->started_at)
            .count();
    const aster::FrameStats stats =
        renderer->renderer->render(scene->scene, orbit, render_settings, static_cast<int>(width),
                                   static_cast<int>(height), frame_seconds);
    renderer->renderer->stampLastFrameCausalTrace(settings_desc.world_trace_hash,
                                                  settings_desc.simulation_tick,
                                                  settings_desc.extraction_hash,
                                                  settings_desc.asset_lineage_hash,
                                                  settings_desc.world_transition_hash,
                                                  settings_desc.actor_state_delta_hash,
                                                  settings_desc.sensory_event_hash,
                                                  settings_desc.visibility_set_hash,
                                                  settings_desc.encounter_budget_hash,
                                                  settings_desc.navigation_valid != 0u,
                                                  settings_desc.streaming_region_id,
                                                  settings_desc.perceptual_salience_score,
                                                  has_continuity_budget &&
                                                      settings_desc.perceptual_continuity_budget
                                                              .accepted != 0u,
                                                  settings_desc.perceptual_continuity_budget
                                                      .required_channel_mask,
                                                  settings_desc.perceptual_continuity_budget
                                                      .observed_channel_mask,
                                                  settings_desc.perceptual_continuity_budget
                                                      .missing_channel_mask,
                                                  settings_desc.perceptual_continuity_budget
                                                      .continuity_score,
                                                  settings_desc.perceptual_continuity_budget
                                                      .minimum_score,
                                                  settings_desc.perceptual_continuity_budget
                                                      .reaction_package_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .material_memory_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .lighting_atmosphere_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .ai_attention_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .streaming_residency_lod_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .resource_state_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .event_residue_hash,
                                                  settings_desc.perceptual_continuity_budget
                                                      .readability_audit_hash);
    renderer->last_stats = abiFrameStats(stats);
    renderer->active_target = nullptr;
    renderer->has_rendered_frame = true;
    renderer->world_trace_hash = settings_desc.world_trace_hash;
    renderer->simulation_tick = settings_desc.simulation_tick;
    renderer->extraction_hash = settings_desc.extraction_hash;
    renderer->asset_lineage_hash = settings_desc.asset_lineage_hash;
    renderer->world_transition_hash = settings_desc.world_transition_hash;
    renderer->actor_state_delta_hash = settings_desc.actor_state_delta_hash;
    renderer->sensory_event_hash = settings_desc.sensory_event_hash;
    renderer->visibility_set_hash = settings_desc.visibility_set_hash;
    renderer->encounter_budget_hash = settings_desc.encounter_budget_hash;
    renderer->navigation_valid = settings_desc.navigation_valid;
    renderer->streaming_region_id = settings_desc.streaming_region_id;
    renderer->perceptual_salience_score = settings_desc.perceptual_salience_score;
    renderer->perceptual_continuity_accepted =
        has_continuity_budget ? settings_desc.perceptual_continuity_budget.accepted : 0u;
    renderer->perceptual_continuity_required_channel_mask =
        settings_desc.perceptual_continuity_budget.required_channel_mask;
    renderer->perceptual_continuity_observed_channel_mask =
        settings_desc.perceptual_continuity_budget.observed_channel_mask;
    renderer->perceptual_continuity_missing_channel_mask =
        settings_desc.perceptual_continuity_budget.missing_channel_mask;
    renderer->perceptual_continuity_score =
        settings_desc.perceptual_continuity_budget.continuity_score;
    renderer->perceptual_continuity_minimum_score =
        settings_desc.perceptual_continuity_budget.minimum_score;
    renderer->reaction_package_hash = settings_desc.perceptual_continuity_budget.reaction_package_hash;
    renderer->material_memory_hash = settings_desc.perceptual_continuity_budget.material_memory_hash;
    renderer->lighting_atmosphere_hash =
        settings_desc.perceptual_continuity_budget.lighting_atmosphere_hash;
    renderer->ai_attention_hash = settings_desc.perceptual_continuity_budget.ai_attention_hash;
    renderer->streaming_residency_lod_hash =
        settings_desc.perceptual_continuity_budget.streaming_residency_lod_hash;
    renderer->resource_state_hash = settings_desc.perceptual_continuity_budget.resource_state_hash;
    renderer->event_residue_hash = settings_desc.perceptual_continuity_budget.event_residue_hash;
    renderer->readability_audit_hash =
        settings_desc.perceptual_continuity_budget.readability_audit_hash;
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "render frame failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_render_frame_to_target(
    const AsterRendererHandle renderer, const AsterSceneHandle scene,
    const AsterRenderTargetHandle target, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (!validCameraDesc(camera) || !validRendererSettings(settings)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "render target frame struct version is not supported");
  }
  const AsterCameraDesc camera_desc = copyAbiStruct(camera);
  const AsterRendererSettings settings_desc = copyAbiStruct(settings);
  if (retiredRenderTarget(target)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_DESTROYED_HANDLE_USE,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.render_frame",
                             "render-target", "render target has been destroyed");
    return makeStatus(ASTER_STATUS_LIFETIME_ERROR, "render target has been destroyed");
  }
  if (!validRenderTarget(target)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_RENDER_TARGET_MISMATCH,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.render_frame",
                             "render-target", "render target handle is invalid");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "render target handle is invalid");
  }
  if ((settings_desc.framebuffer_width > 0u && settings_desc.framebuffer_width != target->width) ||
      (settings_desc.framebuffer_height > 0u &&
       settings_desc.framebuffer_height != target->height)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_RENDER_TARGET_MISMATCH,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.render_frame",
                             target->label, "render target dimensions do not match settings");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "render target dimensions do not match settings");
  }
  const aster::RenderBackendCapabilities capabilities = renderer->renderer->backendCapabilities();
  const aster::rhi::DeviceCapabilities &table = capabilities.capability_table;
  if (capabilities.kind != aster::RenderBackendKind::Null &&
      ((!hasFormatBit(table.color_format_mask, target->color_format)) ||
       (target->depth_format != ASTER_KERNEL_BACKEND_FORMAT_UNKNOWN &&
        !hasFormatBit(table.depth_format_mask, target->depth_format)) ||
       !hasSampleCountBit(table.sample_count_mask, target->sample_count))) {
    appendRendererValidation(renderer, ASTER_VALIDATION_BACKEND_CAPABILITY_MISMATCH,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.render_frame",
                             target->label, "render target exceeds backend capability table");
    return makeStatus(ASTER_STATUS_CAPABILITY_MISMATCH,
                      "render target exceeds backend capability table");
  }

  AsterRendererSettings target_settings = settings_desc;
  target_settings.render_target = nullptr;
  target_settings.framebuffer_width = target->width;
  target_settings.framebuffer_height = target->height;
  const AsterStatus status =
      aster_kernel_renderer_render_frame(renderer, scene, &camera_desc, &target_settings);
  if (status.code == ASTER_STATUS_OK) {
    renderer->active_target = target;
  }
  return status;
}

AsterStatus aster_kernel_renderer_bind_window(const AsterRendererHandle renderer,
                                              const AsterWindowHandle window) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  renderer->bound_window = window;
  renderer->renderer->bindWindow(nativeSurfaceFor(window));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_present(const AsterRendererHandle renderer,
                                          const AsterWindowHandle window) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  AsterPresentDesc desc{sizeof(AsterPresentDesc), ASTER_KERNEL_STRUCT_VERSION_1, 1u, 1u};
  AsterPresentResult result{};
  result.size = sizeof(AsterPresentResult);
  result.version = ASTER_KERNEL_STRUCT_VERSION_1;
  const AsterWindowHandle target = validWindow(window) ? window : renderer->bound_window;
  const AsterStatus native_status =
      aster_kernel_renderer_present_frame(renderer, target, &desc, &result);
  if (native_status.code != ASTER_STATUS_OK || result.presented != 0u) {
    return native_status;
  }
  return aster_kernel_window_swap(target);
}

AsterStatus aster_kernel_renderer_present_frame(const AsterRendererHandle renderer,
                                                const AsterWindowHandle window,
                                                const AsterPresentDesc *desc,
                                                AsterPresentResult *out_result) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(desc) || !validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "present frame struct version is not supported");
  }
  const AsterWindowHandle target = validWindow(window) ? window : renderer->bound_window;
  const aster::NativeWindowSurface surface = nativeSurfaceFor(target);
  const aster::RendererPresentResult result =
      renderer->renderer->present(surface, {.vsync = desc->vsync != 0u,
                                            .wait_for_frame = desc->wait_for_frame != 0u});
  out_result->presented = result.presented ? 1u : 0u;
  out_result->backend = backendKind(result.backend);
  out_result->presentation = presentationMode(result.presentation);
  out_result->width = result.width;
  out_result->height = result.height;
  out_result->backbuffer_index = result.backbuffer_index;
  out_result->frame_index = result.frame_index;
  out_result->queue_waits = result.queue_waits;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_presentation_status(
    const AsterRendererHandle renderer, AsterRendererPresentationStatus *out_status) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_status)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "presentation status struct version is not supported");
  }
  const aster::RendererPresentationStatus status = renderer->renderer->presentationStatus();
  out_status->backend = backendKind(status.backend);
  out_status->presentation = presentationMode(status.presentation);
  out_status->native_present_supported = status.native_present_supported ? 1u : 0u;
  out_status->bound_window = status.bound_window ? 1u : 0u;
  out_status->width = status.width;
  out_status->height = status.height;
  out_status->last_presented_frame = status.last_presented_frame;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_capture(const AsterRendererHandle renderer,
                                          const AsterCaptureDesc *desc) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "capture descriptor version is not supported");
  }
  if (!validStringView(desc->path) || desc->path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "capture path is invalid");
  }
  if (!renderer->has_rendered_frame) {
    appendRendererValidation(renderer, ASTER_VALIDATION_CAPTURE_BEFORE_RENDER,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.capture", "frame",
                             "capture requires a completed rendered frame");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "capture requires a completed rendered frame");
  }
  try {
    const std::uint32_t width =
        desc->width > 0u ? desc->width : std::max(renderer->last_stats.framebuffer_width, 1u);
    const std::uint32_t height =
        desc->height > 0u ? desc->height : std::max(renderer->last_stats.framebuffer_height, 1u);
    writeFramebufferByExtension(stringFromView(desc->path), static_cast<int>(width),
                                static_cast<int>(height));
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "frame capture failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_capture_render_target(const AsterRendererHandle renderer,
                                                        const AsterRenderTargetHandle target,
                                                        const AsterCaptureDesc *desc) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (retiredRenderTarget(target)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_DESTROYED_HANDLE_USE,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.capture",
                             "render-target", "render target has been destroyed");
    return makeStatus(ASTER_STATUS_LIFETIME_ERROR, "render target has been destroyed");
  }
  if (!validRenderTarget(target)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_RENDER_TARGET_MISMATCH,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.capture",
                             "render-target", "render target handle is invalid");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR, "render target handle is invalid");
  }
  if (renderer->active_target != target) {
    appendRendererValidation(renderer, ASTER_VALIDATION_RENDER_TARGET_MISMATCH,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.capture",
                             target->label, "render target was not the last rendered target");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "render target was not the last rendered target");
  }
  return aster_kernel_renderer_capture(renderer, desc);
}

AsterStatus aster_kernel_renderer_frame_vision_probe(
    const AsterRendererHandle renderer, const AsterSceneHandle scene, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings, const AsterFrameVisionProbeDesc *desc,
    AsterFrameVisionProbeResult *out_result) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validScene(scene)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_LIFETIME_ERROR,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                             "renderer.frame_vision_probe", "scene",
                             "scene handle is invalid for frame vision probe");
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (camera != nullptr && !validCameraDesc(camera)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "camera descriptor version is not supported");
  }
  if (settings != nullptr && !validRendererSettings(settings)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "renderer settings version is not supported");
  }
  if (!validStruct(desc) || !validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame vision probe struct version is not supported");
  }
  if (!validStringView(desc->output_dir) || desc->output_dir.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame vision probe output dir is invalid");
  }
  if (!validStringView(desc->label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame vision probe label is invalid");
  }
  if (!renderer->has_rendered_frame) {
    appendRendererValidation(renderer, ASTER_VALIDATION_CAPTURE_BEFORE_RENDER,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                             "renderer.frame_vision_probe", "frame",
                             "frame vision probe requires a completed rendered frame");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "frame vision probe requires a completed rendered frame");
  }

  *out_result = {};
  out_result->size = sizeof(AsterFrameVisionProbeResult);
  out_result->version = ASTER_KERNEL_STRUCT_VERSION_1;

  try {
    const std::filesystem::path output_dir = stringFromView(desc->output_dir);
    const std::string stem = sanitizeArtifactStem(stringFromView(desc->label));
    const std::filesystem::path png_path = output_dir / (stem + ".png");
    const std::filesystem::path json_path = output_dir / (stem + ".json");
    std::filesystem::create_directories(output_dir);

    const std::uint32_t width =
        desc->width > 0u ? desc->width : std::max(renderer->last_stats.framebuffer_width, 1u);
    const std::uint32_t height =
        desc->height > 0u ? desc->height : std::max(renderer->last_stats.framebuffer_height, 1u);
    const std::uint32_t artifact_flags =
        desc->artifact_flags == ASTER_FRAME_VISION_PROBE_ARTIFACT_DEFAULT
            ? (ASTER_FRAME_VISION_PROBE_ARTIFACT_PNG |
               ASTER_FRAME_VISION_PROBE_ARTIFACT_JSON)
            : desc->artifact_flags;
    const bool write_png = (artifact_flags & ASTER_FRAME_VISION_PROBE_ARTIFACT_PNG) != 0u;
    const bool write_json = (artifact_flags & ASTER_FRAME_VISION_PROBE_ARTIFACT_JSON) != 0u;

    if (write_png) {
      aster::writeFramebufferPng(png_path, static_cast<int>(width), static_cast<int>(height));
    } else {
      writeFramebufferByExtension(png_path, static_cast<int>(width), static_cast<int>(height));
    }

    const std::span<const std::uint8_t> rgba = aster::activeFrameBuffer().rgba8();
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(aster::activeFrameBuffer().width()) *
        static_cast<std::uint64_t>(aster::activeFrameBuffer().height());
    const float void_threshold =
        desc->visible_void_luminance_threshold > 0.0f
            ? desc->visible_void_luminance_threshold
            : 0.000001f;
    std::uint64_t visible_void_count = 0u;
    if (rgba.size() >= static_cast<std::size_t>(pixel_count) * 4u) {
      for (std::uint64_t pixel = 0u; pixel < pixel_count; ++pixel) {
        const std::size_t base = static_cast<std::size_t>(pixel) * 4u;
        const float r = static_cast<float>(rgba[base + 0u]) / 255.0f;
        const float g = static_cast<float>(rgba[base + 1u]) / 255.0f;
        const float b = static_cast<float>(rgba[base + 2u]) / 255.0f;
        const float luminance = r * 0.2126f + g * 0.7152f + b * 0.0722f;
        if (luminance <= void_threshold) {
          ++visible_void_count;
        }
      }
    }

    const float visible_void_fraction =
        pixel_count > 0u ? static_cast<float>(static_cast<double>(visible_void_count) /
                                              static_cast<double>(pixel_count))
                         : 0.0f;
    const float max_visible_void_fraction =
        std::max(desc->max_visible_void_fraction, 0.0f);
    const std::uint64_t max_visible_void_count =
        static_cast<std::uint64_t>(std::floor(static_cast<double>(pixel_count) *
                                                  static_cast<double>(max_visible_void_fraction) +
                                              0.5));
    const std::uint64_t zfight_candidate_count = 0u;
    const std::uint64_t support_mismatch_count = 0u;
    const std::uint64_t traversal_blocked_count = 0u;
    const float support_delta = 0.0f;
    bool accepted = visible_void_count <= max_visible_void_count &&
                    zfight_candidate_count <= desc->max_zfight_candidate_pixels &&
                    support_mismatch_count <= desc->max_support_mismatch_count &&
                    traversal_blocked_count <= desc->max_traversal_blocked_count;
    if (desc->max_support_render_delta_m > 0.0f &&
        support_delta > desc->max_support_render_delta_m) {
      accepted = false;
    }

    out_result->accepted = accepted ? 1u : 0u;
    out_result->width = static_cast<std::uint32_t>(aster::activeFrameBuffer().width());
    out_result->height = static_cast<std::uint32_t>(aster::activeFrameBuffer().height());
    out_result->pixel_count = pixel_count;
    out_result->visible_void_count = visible_void_count;
    out_result->zfight_candidate_count = zfight_candidate_count;
    out_result->support_mismatch_count = support_mismatch_count;
    out_result->traversal_blocked_count = traversal_blocked_count;
    out_result->visible_void_fraction = visible_void_fraction;
    out_result->max_support_render_delta_m = support_delta;
    out_result->png_path = viewFromScratch(renderer, png_path.string());
    out_result->json_path = viewFromScratch(renderer, json_path.string());
    out_result->diagnostic_kind =
        accepted ? ASTER_KERNEL_FRAME_DIAGNOSTIC_SURFACE_PRESENTATION_WARNING
                 : ASTER_KERNEL_FRAME_DIAGNOSTIC_VISIBLE_VOID;

    if (write_json) {
      std::ofstream file(json_path, std::ios::binary);
      if (!file) {
        return makeStatus(ASTER_STATUS_INTERNAL_ERROR,
                          "frame vision probe metrics output could not be opened");
      }
      file << std::fixed << std::setprecision(6);
      file << "{\n";
      file << "  \"label\": \"" << jsonEscape(stem) << "\",\n";
      file << "  \"accepted\": " << (accepted ? "true" : "false") << ",\n";
      file << "  \"width\": " << out_result->width << ",\n";
      file << "  \"height\": " << out_result->height << ",\n";
      file << "  \"pixel_count\": " << pixel_count << ",\n";
      file << "  \"visible_void_count\": " << visible_void_count << ",\n";
      file << "  \"visible_void_fraction\": " << visible_void_fraction << ",\n";
      file << "  \"zfight_candidate_count\": " << zfight_candidate_count << ",\n";
      file << "  \"support_mismatch_count\": " << support_mismatch_count << ",\n";
      file << "  \"traversal_blocked_count\": " << traversal_blocked_count << ",\n";
      file << "  \"max_support_render_delta_m\": " << support_delta << ",\n";
      file << "  \"png_path\": \"" << jsonEscape(png_path.string()) << "\"\n";
      file << "}\n";
    }

    if (!accepted) {
      appendRendererValidation(renderer, ASTER_VALIDATION_VISIBLE_VOID,
                               ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                               "renderer.frame_vision_probe", stem,
                               "frame vision probe detected visible void pixels",
                               visible_void_count);
    }
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "frame vision probe failed");
  }

  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_lighting_probe(
    const AsterRendererHandle renderer, const AsterSceneHandle scene, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings, const AsterFrameLightingProbeDesc *desc,
    AsterFrameLightingProbeResult *out_result) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validScene(scene)) {
    appendRendererValidation(renderer, ASTER_VALIDATION_LIFETIME_ERROR,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                             "renderer.frame_lighting_probe", "scene",
                             "scene handle is invalid for frame lighting probe");
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (camera != nullptr && !validCameraDesc(camera)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "camera descriptor version is not supported");
  }
  if (settings != nullptr && !validRendererSettings(settings)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "renderer settings version is not supported");
  }
  if (!validStruct(desc) || !validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame lighting probe struct version is not supported");
  }
  if (!validStringView(desc->output_dir) || desc->output_dir.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame lighting probe output dir is invalid");
  }
  if (!validStringView(desc->label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame lighting probe label is invalid");
  }
  if (!renderer->has_rendered_frame) {
    appendRendererValidation(renderer, ASTER_VALIDATION_CAPTURE_BEFORE_RENDER,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                             "renderer.frame_lighting_probe", "frame",
                             "frame lighting probe requires a completed rendered frame");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "frame lighting probe requires a completed rendered frame");
  }

  *out_result = {};
  out_result->size = sizeof(AsterFrameLightingProbeResult);
  out_result->version = ASTER_KERNEL_STRUCT_VERSION_1;

  try {
    const std::filesystem::path output_dir = stringFromView(desc->output_dir);
    const std::string stem = sanitizeArtifactStem(stringFromView(desc->label));
    const std::filesystem::path png_path = output_dir / (stem + ".png");
    const std::filesystem::path json_path = output_dir / (stem + ".lighting.json");
    const std::filesystem::path heatmap_path = output_dir / (stem + ".lighting.png");
    std::filesystem::create_directories(output_dir);

    const std::uint32_t width =
        desc->width > 0u ? desc->width : std::max(renderer->last_stats.framebuffer_width, 1u);
    const std::uint32_t height =
        desc->height > 0u ? desc->height : std::max(renderer->last_stats.framebuffer_height, 1u);
    const std::uint32_t artifact_flags =
        desc->artifact_flags == ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_DEFAULT
            ? (ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_PNG |
               ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_JSON |
               ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_HEATMAP_PNG)
            : desc->artifact_flags;
    const bool write_png = (artifact_flags & ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_PNG) != 0u;
    const bool write_json = (artifact_flags & ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_JSON) != 0u;
    const bool write_heatmap =
        (artifact_flags & ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_HEATMAP_PNG) != 0u;
    if (write_png) {
      aster::writeFramebufferPng(png_path, static_cast<int>(width), static_cast<int>(height));
    }

    AsterCameraDesc camera_desc{};
    camera_desc.size = sizeof(AsterCameraDesc);
    camera_desc.version = ASTER_KERNEL_STRUCT_VERSION_1;
    camera_desc.pitch_radians = 0.25f;
    camera_desc.radius = 5.0f;
    camera_desc.vertical_fov_radians = 0.9f;
    camera_desc.near_plane = 0.01f;
    camera_desc.far_plane = 100.0f;
    if (camera != nullptr) {
      camera_desc = copyAbiStruct(camera);
    }
    AsterRendererSettings settings_desc{};
    settings_desc.size = sizeof(AsterRendererSettings);
    settings_desc.version = ASTER_KERNEL_STRUCT_VERSION_1;
    settings_desc.framebuffer_width = width;
    settings_desc.framebuffer_height = height;
    settings_desc.fog_strength = 0.16f;
    settings_desc.flags = ASTER_KERNEL_RENDER_SETTING_VOLUMETRIC_FOG;
    if (settings != nullptr) {
      settings_desc = copyAbiStruct(settings);
      settings_desc.framebuffer_width = width;
      settings_desc.framebuffer_height = height;
      settings_desc.flags |= ASTER_KERNEL_RENDER_SETTING_VOLUMETRIC_FOG;
      if (settings_desc.fog_strength <= 0.0f) {
        settings_desc.fog_strength = 0.16f;
      }
    }

    aster::OrbitCamera orbit;
    orbit.target = vec(camera_desc.target);
    orbit.yaw = camera_desc.yaw_radians;
    orbit.pitch = camera_desc.pitch_radians;
    orbit.radius = std::max(camera_desc.radius, 0.01f);
    orbit.vertical_fov = cameraVerticalFov(camera_desc, width, height);
    orbit.near_plane = camera_desc.near_plane > 0.0f ? camera_desc.near_plane : 0.01f;
    orbit.far_plane =
        camera_desc.far_plane > orbit.near_plane ? camera_desc.far_plane : 100.0f;
    aster::RendererSettings render_settings = rendererSettingsFromAbi(settings_desc, camera_desc);
    render_settings.atmosphere.enabled = true;
    render_settings.atmosphere.local_light_scattering =
        std::max(render_settings.atmosphere.local_light_scattering, 0.32f);
    render_settings.atmosphere.source_glow_strength =
        std::max(render_settings.atmosphere.source_glow_strength, 0.72f);
    render_settings.atmosphere.volumetric_light_steps =
        std::max(render_settings.atmosphere.volumetric_light_steps, 4u);

    const aster::SoftwarePreviewOptions options{
        .width = static_cast<int>(width),
        .height = static_cast<int>(height),
        .samples_per_axis = 1,
        .frame_seconds = 0.0,
        .settings = render_settings};
    const aster::SoftwarePreviewResult preview =
        aster::renderSoftwarePreviewWithProbe(scene->scene, orbit, options);

    const float source_threshold =
        desc->source_luminance_threshold > 0.0f ? desc->source_luminance_threshold : 0.020f;
    const float air_threshold =
        desc->air_scatter_luminance_threshold > 0.0f ? desc->air_scatter_luminance_threshold
                                                     : 0.0025f;
    const float overexposed_luminance_threshold =
        desc->overexposed_luminance_threshold > 0.0f ? desc->overexposed_luminance_threshold
                                                     : 246.0f;
    const float max_overexposed_fraction =
        desc->max_overexposed_pixel_fraction > 0.0f ? desc->max_overexposed_pixel_fraction
                                                    : 0.050f;
    const float min_frame_mean =
        desc->min_frame_mean_luminance > 0.0f ? desc->min_frame_mean_luminance : 0.0f;
    const float max_frame_mean =
        desc->max_frame_mean_luminance > 0.0f ? desc->max_frame_mean_luminance : 1.0f;
    double source_sum = 0.0;
    double air_sum = 0.0;
    double direct_sum = 0.0;
    double direct_delta_sum = 0.0;
    std::uint64_t source_pixels = 0u;
    std::uint64_t air_pixels = 0u;
    std::uint64_t direct_pixels = 0u;
    float previous_direct = -1.0f;
    for (const aster::SoftwareLightingProbePixel &pixel : preview.lighting.pixels) {
      if (pixel.source_readability_luminance >= source_threshold) {
        source_sum += pixel.source_readability_luminance;
        ++source_pixels;
      }
      if (pixel.volumetric_light_luminance >= air_threshold) {
        air_sum += pixel.volumetric_light_luminance;
        ++air_pixels;
      }
      if (pixel.direct_light_luminance > 0.00001f) {
        direct_sum += pixel.direct_light_luminance;
        if (previous_direct >= 0.0f) {
          direct_delta_sum += std::abs(pixel.direct_light_luminance - previous_direct);
        }
        previous_direct = pixel.direct_light_luminance;
        ++direct_pixels;
      }
    }
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(preview.lighting.width) *
        static_cast<std::uint64_t>(preview.lighting.height);
    std::uint64_t overexposed_pixels = 0u;
    const std::span<const std::uint8_t> rgba = preview.framebuffer.rgba8();
    double frame_luma_sum = 0.0;
    std::uint64_t frame_pixels = 0u;
    for (std::size_t offset = 0u; offset + 3u < rgba.size(); offset += 4u) {
      const float red = static_cast<float>(rgba[offset + 0u]);
      const float green = static_cast<float>(rgba[offset + 1u]);
      const float blue = static_cast<float>(rgba[offset + 2u]);
      const float luma = red * 0.2126f + green * 0.7152f + blue * 0.0722f;
      frame_luma_sum += static_cast<double>(luma / 255.0f);
      ++frame_pixels;
      const float max_channel = std::max(red, std::max(green, blue));
      if (luma >= overexposed_luminance_threshold && max_channel >= 254.0f) {
        ++overexposed_pixels;
      }
    }
    const float source_mean =
        source_pixels > 0u ? static_cast<float>(source_sum / static_cast<double>(source_pixels))
                           : 0.0f;
    const float air_mean =
        air_pixels > 0u ? static_cast<float>(air_sum / static_cast<double>(air_pixels)) : 0.0f;
    const float direct_mean =
        direct_pixels > 0u ? static_cast<float>(direct_sum / static_cast<double>(direct_pixels))
                           : 0.0f;
    const float frame_mean =
        frame_pixels > 0u ? static_cast<float>(frame_luma_sum / static_cast<double>(frame_pixels))
                          : 0.0f;
    const float source_to_air_ratio =
        air_mean > 0.000001f ? source_mean / air_mean : (source_mean > 0.0f ? 999.0f : 0.0f);
    const float delta_mean =
        direct_pixels > 1u
            ? static_cast<float>(direct_delta_sum / static_cast<double>(direct_pixels - 1u))
            : 0.0f;
    const float falloff_score =
        direct_mean > 0.000001f ? std::clamp(1.0f - delta_mean / (direct_mean + 0.0001f), 0.0f, 1.0f)
                                : 1.0f;
    const float temporal_delta = 0.0f;

    std::uint64_t unreadable = 0u;
    std::uint64_t missing_volume = 0u;
    std::uint64_t discontinuity = 0u;
    std::uint64_t underflow = 0u;
    std::uint64_t overflow = 0u;
    std::uint64_t overbright = 0u;
    if (desc->min_source_mean_luminance > 0.0f && source_mean < desc->min_source_mean_luminance) {
      unreadable = 1u;
    }
    if ((desc->min_air_scatter_pixels > 0u && air_pixels < desc->min_air_scatter_pixels) ||
        (desc->min_air_scatter_mean_luminance > 0.0f &&
         air_mean < desc->min_air_scatter_mean_luminance)) {
      missing_volume = 1u;
    }
    if (desc->min_falloff_continuity_score > 0.0f &&
        falloff_score < desc->min_falloff_continuity_score) {
      discontinuity = 1u;
    }
    if (desc->min_source_to_air_ratio > 0.0f && source_to_air_ratio < desc->min_source_to_air_ratio) {
      underflow = 1u;
    }
    if (desc->max_source_to_air_ratio > 0.0f && source_to_air_ratio > desc->max_source_to_air_ratio) {
      underflow = 1u;
    }
    if (desc->max_temporal_lighting_delta > 0.0f &&
        temporal_delta > desc->max_temporal_lighting_delta) {
      discontinuity = 1u;
    }
    const std::uint64_t max_overexposed_pixels =
        std::max<std::uint64_t>(16u, static_cast<std::uint64_t>(
                                         static_cast<double>(pixel_count) *
                                         static_cast<double>(max_overexposed_fraction)));
    if (overexposed_pixels > max_overexposed_pixels) {
      overflow = 1u;
    }
    if (frame_mean < min_frame_mean || frame_mean > max_frame_mean) {
      overbright = 1u;
    }
    const bool accepted =
        unreadable == 0u && missing_volume == 0u && discontinuity == 0u && underflow == 0u &&
        overflow == 0u && overbright == 0u;
    AsterKernelFrameDiagnosticKind diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_SURFACE_PRESENTATION_WARNING;
    if (unreadable != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_LIGHT_SOURCE_UNREADABLE;
    } else if (missing_volume != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_VOLUMETRIC_LIGHT_MISSING;
    } else if (discontinuity != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_LIGHT_FALLOFF_DISCONTINUITY;
    } else if (underflow != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_CAVE_LIGHT_EXPOSURE_UNDERFLOW;
    } else if (overflow != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_CAVE_LIGHT_EXPOSURE_OVERFLOW;
    } else if (overbright != 0u) {
      diagnostic = ASTER_KERNEL_FRAME_DIAGNOSTIC_CAVE_LIGHT_EXPOSURE_OVERFLOW;
    }

    out_result->accepted = accepted ? 1u : 0u;
    out_result->width = width;
    out_result->height = height;
    out_result->pixel_count = pixel_count;
    out_result->source_visible_pixels = source_pixels;
    out_result->air_scatter_pixels = air_pixels;
    out_result->light_source_unreadable_count = unreadable;
    out_result->volumetric_light_missing_count = missing_volume;
    out_result->light_falloff_discontinuity_count = discontinuity;
    out_result->cave_light_exposure_underflow_count = underflow;
    out_result->source_mean_luminance = source_mean;
    out_result->air_scatter_mean_luminance = air_mean;
    out_result->surface_direct_mean_luminance = direct_mean;
    out_result->source_to_air_ratio = source_to_air_ratio;
    out_result->falloff_continuity_score = falloff_score;
    out_result->temporal_lighting_delta = temporal_delta;
    out_result->png_path = viewFromScratch(renderer, png_path.string());
    out_result->json_path = viewFromScratch(renderer, json_path.string());
    out_result->heatmap_png_path = viewFromScratch(renderer, heatmap_path.string());
    out_result->diagnostic_kind = diagnostic;
    out_result->overexposed_pixels = overexposed_pixels;
    out_result->cave_light_exposure_overflow_count = overflow;
    out_result->cave_light_exposure_overbright_count = overbright;
    out_result->frame_mean_luminance = frame_mean;

    if (write_heatmap) {
      std::vector<std::uint8_t> heatmap(static_cast<std::size_t>(width) *
                                        static_cast<std::size_t>(height) * 4u);
      const float direct_scale = direct_mean > 0.0001f ? direct_mean : 0.06f;
      const float air_scale = air_mean > 0.0001f ? air_mean : 0.012f;
      const float source_scale = source_mean > 0.0001f ? source_mean : 0.08f;
      for (std::uint64_t pixel = 0u; pixel < pixel_count && pixel < preview.lighting.pixels.size();
           ++pixel) {
        const aster::SoftwareLightingProbePixel &probe = preview.lighting.pixels[pixel];
        const std::size_t base = static_cast<std::size_t>(pixel) * 4u;
        heatmap[base + 0u] = static_cast<std::uint8_t>(
            std::clamp(std::lround(std::clamp(probe.direct_light_luminance / direct_scale, 0.0f,
                                              1.0f) *
                                   255.0f),
                       0l, 255l));
        heatmap[base + 1u] = static_cast<std::uint8_t>(
            std::clamp(std::lround(std::clamp(probe.volumetric_light_luminance / air_scale, 0.0f,
                                              1.0f) *
                                   255.0f),
                       0l, 255l));
        heatmap[base + 2u] = static_cast<std::uint8_t>(
            std::clamp(std::lround(std::clamp(probe.source_readability_luminance / source_scale,
                                              0.0f, 1.0f) *
                                   255.0f),
                       0l, 255l));
        heatmap[base + 3u] = 255u;
      }
      aster::writeRgbaPng(heatmap_path, static_cast<int>(width), static_cast<int>(height), heatmap);
    }

    if (write_json) {
      std::ofstream file(json_path, std::ios::binary);
      if (!file) {
        return makeStatus(ASTER_STATUS_INTERNAL_ERROR,
                          "frame lighting probe metrics output could not be opened");
      }
      file << std::fixed << std::setprecision(6);
      file << "{\n";
      file << "  \"label\": \"" << jsonEscape(stem) << "\",\n";
      file << "  \"accepted\": " << (accepted ? "true" : "false") << ",\n";
      file << "  \"width\": " << width << ",\n";
      file << "  \"height\": " << height << ",\n";
      file << "  \"pixel_count\": " << pixel_count << ",\n";
      file << "  \"source_visible_pixels\": " << source_pixels << ",\n";
      file << "  \"air_scatter_pixels\": " << air_pixels << ",\n";
      file << "  \"light_source_unreadable_count\": " << unreadable << ",\n";
      file << "  \"volumetric_light_missing_count\": " << missing_volume << ",\n";
      file << "  \"light_falloff_discontinuity_count\": " << discontinuity << ",\n";
      file << "  \"cave_light_exposure_underflow_count\": " << underflow << ",\n";
      file << "  \"cave_light_exposure_overflow_count\": " << overflow << ",\n";
      file << "  \"cave_light_exposure_overbright_count\": " << overbright << ",\n";
      file << "  \"overexposed_pixels\": " << overexposed_pixels << ",\n";
      file << "  \"frame_mean_luminance\": " << frame_mean << ",\n";
      file << "  \"source_mean_luminance\": " << source_mean << ",\n";
      file << "  \"air_scatter_mean_luminance\": " << air_mean << ",\n";
      file << "  \"surface_direct_mean_luminance\": " << direct_mean << ",\n";
      file << "  \"source_to_air_ratio\": " << source_to_air_ratio << ",\n";
      file << "  \"falloff_continuity_score\": " << falloff_score << ",\n";
      file << "  \"temporal_lighting_delta\": " << temporal_delta << ",\n";
      file << "  \"png_path\": \"" << jsonEscape(png_path.string()) << "\",\n";
      file << "  \"heatmap_png_path\": \"" << jsonEscape(heatmap_path.string()) << "\"\n";
      file << "}\n";
    }

    if (!accepted) {
      appendRendererValidation(renderer, ASTER_VALIDATION_VISIBLE_VOID,
                               ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR,
                               "renderer.frame_lighting_probe", stem,
                               "frame lighting probe detected player-visible lighting failure",
                               unreadable + missing_volume + discontinuity + underflow);
    }
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "frame lighting probe failed");
  }

  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_last_stats(const AsterRendererHandle renderer,
                                             AsterFrameStats *out_stats) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_stats)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "frame stats struct version is not supported");
  }
  *out_stats = renderer->last_stats;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_validation_event_count(const AsterRendererHandle renderer,
                                                         std::size_t *out_count) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (out_count == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_count is null");
  }
  *out_count = renderer->validation_events.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_validation_event(const AsterRendererHandle renderer,
                                                   const std::size_t index,
                                                   AsterValidationEvent *out_event) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  return validationEventAt(renderer->validation_events, index, out_event);
}

AsterStatus aster_kernel_renderer_frame_forensics_counts(
    const AsterRendererHandle renderer, AsterFrameForensicsCounts *out_counts) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame forensics counts struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  out_counts->pass_count = forensics.passes.size();
  out_counts->event_count = forensics.events.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_forensics_detail_counts(
    const AsterRendererHandle renderer, AsterFrameForensicsDetailCounts *out_counts) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validFrameForensicsDetailCounts(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame forensics detail counts struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  out_counts->pass_count = forensics.passes.size();
  out_counts->event_count = forensics.events.size();
  out_counts->debug_capture_count = forensics.captures.size();
  out_counts->pass_artifact_count = forensics.pass_artifacts.size();
  out_counts->resource_transition_count = forensics.resource_traces.size();
  out_counts->rhi_validation_event_count = forensics.rhi_validation_events.size();
  out_counts->timestamp_sample_count = forensics.timestamp_samples.size();
  out_counts->backend_feature_proof_count = forensics.backend_feature_proofs.size();
  out_counts->certification_valid = forensics.certification.valid ? 1u : 0u;
  out_counts->certification_missing_proof_count =
      forensics.certification.missing_proof_count;
  out_counts->certification_validation_error_count =
      forensics.certification.validation_error_count;
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, object_fate_count),
                        sizeof(out_counts->object_fate_count))) {
    out_counts->object_fate_count = forensics.object_fates.size();
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, world_trace_hash),
                        sizeof(out_counts->world_trace_hash))) {
    out_counts->world_trace_hash = forensics.world_trace_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, simulation_tick),
                        sizeof(out_counts->simulation_tick))) {
    out_counts->simulation_tick = forensics.simulation_tick;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, extraction_hash),
                        sizeof(out_counts->extraction_hash))) {
    out_counts->extraction_hash = forensics.extraction_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, asset_lineage_hash),
                        sizeof(out_counts->asset_lineage_hash))) {
    out_counts->asset_lineage_hash = forensics.asset_lineage_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, world_extraction_provenance),
                        sizeof(out_counts->world_extraction_provenance))) {
    out_counts->world_extraction_provenance =
        forensics.world_transition_linked ? ASTER_WORLD_EXTRACTION_WORLD_TRANSITION
                                          : ASTER_WORLD_EXTRACTION_COMPATIBILITY_SCENE;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, world_transition_hash),
                        sizeof(out_counts->world_transition_hash))) {
    out_counts->world_transition_hash = forensics.world_transition_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, actor_state_delta_hash),
                        sizeof(out_counts->actor_state_delta_hash))) {
    out_counts->actor_state_delta_hash = forensics.actor_state_delta_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, encounter_budget_hash),
                        sizeof(out_counts->encounter_budget_hash))) {
    out_counts->encounter_budget_hash = forensics.encounter_budget_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, navigation_valid),
                        sizeof(out_counts->navigation_valid))) {
    out_counts->navigation_valid = forensics.navigation_valid ? 1u : 0u;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, streaming_region_id),
                        sizeof(out_counts->streaming_region_id))) {
    out_counts->streaming_region_id = forensics.streaming_region_id;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, perceptual_salience_score),
                        sizeof(out_counts->perceptual_salience_score))) {
    out_counts->perceptual_salience_score = forensics.perceptual_salience_score;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, sensory_event_hash),
                        sizeof(out_counts->sensory_event_hash))) {
    out_counts->sensory_event_hash = forensics.sensory_event_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts, visibility_set_hash),
                        sizeof(out_counts->visibility_set_hash))) {
    out_counts->visibility_set_hash = forensics.visibility_set_hash;
  }
  if (abiStructHasField(out_counts->size,
                        offsetof(AsterFrameForensicsDetailCounts,
                                 perceptual_continuity_budget),
                        sizeof(out_counts->perceptual_continuity_budget))) {
    out_counts->perceptual_continuity_budget = {
        .size = sizeof(AsterPerceptualContinuityBudget),
        .version = ASTER_KERNEL_STRUCT_VERSION_1,
        .accepted = forensics.perceptual_continuity_accepted ? 1u : 0u,
        .required_channel_mask = forensics.perceptual_continuity_required_channel_mask,
        .observed_channel_mask = forensics.perceptual_continuity_observed_channel_mask,
        .missing_channel_mask = forensics.perceptual_continuity_missing_channel_mask,
        .continuity_score = forensics.perceptual_continuity_score,
        .minimum_score = forensics.perceptual_continuity_minimum_score,
        .reaction_package_hash = forensics.reaction_package_hash,
        .material_memory_hash = forensics.material_memory_hash,
        .lighting_atmosphere_hash = forensics.lighting_atmosphere_hash,
        .ai_attention_hash = forensics.ai_attention_hash,
        .streaming_residency_lod_hash = forensics.streaming_residency_lod_hash,
        .resource_state_hash = forensics.resource_state_hash,
        .event_residue_hash = forensics.event_residue_hash,
        .readability_audit_hash = forensics.readability_audit_hash,
        .diagnostic = {}};
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_pass_stats(const AsterRendererHandle renderer,
                                                   const std::size_t index,
                                                   AsterFramePassStats *out_stats) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_stats)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame pass stats struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.passes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame pass stats index is out of range");
  }
  const aster::FramePassStats &pass = forensics.passes[index];
  out_stats->pass = renderGraphPass(pass.pass);
  out_stats->name = viewFromString(pass.name);
  out_stats->draw_calls = pass.draw_calls;
  out_stats->pipeline_switches = pass.pipeline_switches;
  out_stats->material_permutations = pass.material_permutations;
  out_stats->encode_seconds = pass.encode_seconds;
  out_stats->cpu_build_seconds = pass.cpu_build_seconds;
  out_stats->gpu_execution_seconds = pass.gpu_execution_seconds;
  out_stats->estimated_bandwidth_bytes = pass.estimated_bandwidth_bytes;
  out_stats->render_target_width = pass.render_target_width;
  out_stats->render_target_height = pass.render_target_height;
  out_stats->descriptor_heap_pressure = pass.descriptor_heap_pressure;
  out_stats->pipeline_cache_hits = pass.pipeline_cache_hits;
  out_stats->pipeline_cache_misses = pass.pipeline_cache_misses;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_diagnostic(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameDiagnosticEvent *out_event) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame diagnostic event struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame diagnostic index is out of range");
  }
  const aster::FrameDiagnosticEvent &event = forensics.events[index];
  out_event->kind = diagnosticKind(event.kind);
  out_event->severity = diagnosticSeverity(event.severity);
  out_event->pass = viewFromString(event.pass);
  out_event->label = viewFromString(event.label);
  out_event->message = viewFromString(event.message);
  out_event->value = event.value;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_debug_capture_info(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameDebugCaptureInfo *out_capture) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capture)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame debug capture struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.captures.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame debug capture index is out of range");
  }
  const aster::FrameDebugCapture &capture = forensics.captures[index];
  out_capture->pass = renderGraphPass(capture.pass);
  out_capture->resource = renderGraphResource(capture.resource);
  out_capture->view = static_cast<std::uint32_t>(capture.view);
  out_capture->label = viewFromString(capture.label);
  out_capture->width = capture.width;
  out_capture->height = capture.height;
  out_capture->row_stride_bytes = capture.row_stride_bytes;
  out_capture->content_hash = capture.content_hash;
  out_capture->available = capture.available ? 1u : 0u;
  out_capture->payload_size = capture.rgba8.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_pass_artifact_info(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFramePassArtifactInfo *out_artifact) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_artifact)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame pass artifact struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.pass_artifacts.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame pass artifact index is out of range");
  }
  const aster::FramePassArtifact &artifact = forensics.pass_artifacts[index];
  out_artifact->pass = renderGraphPass(artifact.pass);
  out_artifact->resource = renderGraphResource(artifact.resource);
  out_artifact->label = viewFromString(artifact.label);
  out_artifact->kind = viewFromString(artifact.kind);
  out_artifact->width = artifact.width;
  out_artifact->height = artifact.height;
  out_artifact->content_hash = artifact.content_hash;
  out_artifact->available = artifact.available ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_resource_transition(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameResourceTransition *out_transition) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_transition)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame resource transition struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.resource_traces.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame resource transition index is out of range");
  }
  const aster::FrameResourceTrace &trace = forensics.resource_traces[index];
  out_transition->pass = renderGraphPass(trace.pass);
  out_transition->resource = renderGraphResource(trace.resource);
  out_transition->pass_name = viewFromString(trace.pass_name);
  out_transition->resource_name = viewFromString(trace.resource_name);
  out_transition->before = rhiResourceState(trace.before);
  out_transition->after = rhiResourceState(trace.after);
  out_transition->queue = rhiQueueKind(trace.queue);
  out_transition->write = trace.write ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_object_render_fate(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterObjectRenderFate *out_fate) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_fate)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "object render fate struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.object_fates.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "object render fate index is out of range");
  }
  renderer->string_scratch.clear();
  renderer->string_scratch.reserve(6u);
  const aster::ObjectRenderFateTrace &fate = forensics.object_fates[index];
  out_fate->object_index = fate.object_index;
  out_fate->visible = fate.visible ? 1u : 0u;
  out_fate->object_name = viewFromString(fate.object_name);
  out_fate->mesh_key = viewFromString(fate.mesh_key);
  out_fate->material_key = viewFromString(fate.material_key);
  out_fate->material_asset_id = viewFromString(fate.material_asset_id);
  out_fate->shader_variant_key = viewFromString(fate.shader_variant_key);
  out_fate->pipeline_tag = viewFromString(fate.pipeline_tag);
  out_fate->source_graph_guid = viewFromString(fate.source_graph_guid);
  out_fate->source_graph_node = viewFromString(fate.source_graph_node);
  out_fate->pipeline_cache_key = viewFromString(fate.pipeline_cache_key);
  out_fate->procedural_capability_status =
      viewFromString(fate.procedural_capability_status);
  out_fate->texture_roles = viewFromScratch(renderer, joinStrings(fate.texture_roles, ","));
  out_fate->pass_list = viewFromScratch(renderer, joinStrings(fate.pass_list, ","));
  out_fate->resource_transitions =
      viewFromScratch(renderer, joinStrings(fate.resource_transitions, ","));
  out_fate->capture_labels = viewFromScratch(renderer, joinStrings(fate.capture_labels, ","));
  out_fate->feature_proofs = viewFromScratch(renderer, joinStrings(fate.feature_proofs, ","));
  out_fate->final_contribution = viewFromString(fate.final_contribution);
  out_fate->contribution_hash = fate.contribution_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_rhi_validation_event(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterRhiValidationEvent *out_event) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "RHI validation event struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.rhi_validation_events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "RHI validation event index is out of range");
  }
  const aster::rhi::ResourceLifetimeValidationEvent &event =
      forensics.rhi_validation_events[index];
  out_event->kind = rhiValidationKind(event.kind);
  out_event->severity = rhiValidationSeverity(event.severity);
  out_event->pass = viewFromString(event.pass);
  out_event->resource = viewFromString(event.resource);
  out_event->message = viewFromString(event.message);
  out_event->pass_index = event.pass_index;
  out_event->resource_id = event.resource_id;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_timestamp_sample(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameTimestampSample *out_sample) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_sample)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame timestamp sample struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.timestamp_samples.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "timestamp sample index is out of range");
  }
  const aster::rhi::TimestampQueryResult &sample = forensics.timestamp_samples[index];
  out_sample->slot = sample.slot;
  out_sample->ticks = sample.ticks;
  out_sample->nanoseconds = sample.nanoseconds;
  out_sample->available = sample.available ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_backend_feature_proof(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterBackendFeatureProof *out_proof) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_proof)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "backend feature proof struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.backend_feature_proofs.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "backend feature proof index is out of range");
  }
  const aster::BackendFeatureProof &proof = forensics.backend_feature_proofs[index];
  out_proof->kind = backendFeatureProofKind(proof.kind);
  out_proof->status = backendFeatureProofStatus(proof.status);
  out_proof->pass = renderGraphPass(proof.pass);
  out_proof->resource = renderGraphResource(proof.resource);
  out_proof->feature = viewFromString(proof.feature);
  out_proof->label = viewFromString(proof.label);
  out_proof->message = viewFromString(proof.message);
  out_proof->advertised = proof.advertised;
  out_proof->native = proof.native;
  out_proof->evidence_hash = proof.evidence_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_get_last_frame_schedule(
    const AsterRendererHandle renderer, AsterFrameScheduleHandle *out_schedule) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (out_schedule == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_schedule is null");
  }
  *out_schedule = nullptr;
  if (!renderer->has_rendered_frame) {
    appendRendererValidation(renderer, ASTER_VALIDATION_CAPTURE_BEFORE_RENDER,
                             ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR, "renderer.frame_schedule",
                             "frame", "frame schedule requires a completed rendered frame");
    return makeStatus(ASTER_STATUS_VALIDATION_ERROR,
                      "frame schedule requires a completed rendered frame");
  }
  try {
    const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
    auto *schedule = new AsterFrameScheduleHandle__();
    schedule->passes = forensics.passes;
    schedule->trace = forensics.rhi_trace;
    schedule->validation_events = renderer->validation_events;
    for (const aster::BackendFeatureProof &proof : forensics.backend_feature_proofs) {
      if (proof.status == aster::BackendFeatureProofStatus::Unsupported ||
          proof.status == aster::BackendFeatureProofStatus::MissingProof) {
        appendValidation(schedule->validation_events, ASTER_VALIDATION_UNSUPPORTED_BACKEND_RESOURCE,
                         ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING, "backend.proof", proof.label,
                         proof.message, proof.evidence_hash);
      }
    }
    *out_schedule = schedule;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "frame schedule allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_counts(const AsterFrameScheduleHandle schedule,
                                               AsterFrameScheduleCounts *out_counts) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule counts struct version is not supported");
  }
  out_counts->pass_count = schedule->passes.size();
  out_counts->transition_count = schedule->trace.transitions.size();
  out_counts->descriptor_layout_count = schedule->trace.descriptor_layouts.size();
  out_counts->pipeline_count = schedule->trace.pipelines.size();
  out_counts->transient_allocation_count = schedule->trace.transient_allocations.size();
  out_counts->timeline_count = schedule->trace.queue_submits.size();
  out_counts->validation_event_count = schedule->validation_events.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_pass(const AsterFrameScheduleHandle schedule,
                                             const std::size_t index,
                                             AsterFrameSchedulePassInfo *out_pass) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_pass)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule pass struct version is not supported");
  }
  if (index >= schedule->passes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule pass index is out of range");
  }
  const aster::FramePassStats &pass = schedule->passes[index];
  out_pass->pass = renderGraphPass(pass.pass);
  out_pass->queue = ASTER_KERNEL_RHI_QUEUE_GRAPHICS;
  out_pass->name = viewFromString(pass.name);
  out_pass->command_buffer_count = 0u;
  out_pass->signal_fence_value = 0u;
  if (index < schedule->trace.queue_submits.size()) {
    const aster::rhi::QueueSubmitTrace &submit = schedule->trace.queue_submits[index];
    out_pass->queue = rhiQueueKind(submit.queue);
    out_pass->command_buffer_count = submit.command_buffer_count;
    out_pass->signal_fence_value = submit.signal_fence_value;
  }
  out_pass->pipeline_cache_key = 0u;
  out_pass->descriptor_layout_hash = 0u;
  for (const aster::rhi::PipelineStateTrace &pipeline : schedule->trace.pipelines) {
    if (pipeline.label == pass.name) {
      out_pass->pipeline_cache_key = pipeline.cache_key;
      out_pass->descriptor_layout_hash = pipeline.descriptor_layout_hash;
      break;
    }
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_memory_report(
    const AsterFrameScheduleHandle schedule, AsterFrameScheduleMemoryReport *out_report) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_report)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule memory report struct version is not supported");
  }
  out_report->budget_bytes = schedule->trace.memory.budget_bytes;
  out_report->resident_bytes = schedule->trace.memory.resident_bytes;
  out_report->transient_bytes = schedule->trace.memory.transient_bytes;
  out_report->aliased_bytes_saved = schedule->trace.memory.aliased_bytes_saved;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_descriptor_layout(
    const AsterFrameScheduleHandle schedule, const std::size_t index,
    AsterFrameScheduleDescriptorInfo *out_info) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule descriptor struct version is not supported");
  }
  if (index >= schedule->trace.descriptor_layouts.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame schedule descriptor index is out of range");
  }
  const aster::rhi::DescriptorLayoutTrace &layout = schedule->trace.descriptor_layouts[index];
  out_info->label = viewFromString(layout.label);
  out_info->layout_hash = layout.layout_hash;
  out_info->range_count = layout.ranges.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_pipeline(
    const AsterFrameScheduleHandle schedule, const std::size_t index,
    AsterFrameSchedulePipelineInfo *out_info) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule pipeline struct version is not supported");
  }
  if (index >= schedule->trace.pipelines.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame schedule pipeline index is out of range");
  }
  const aster::rhi::PipelineStateTrace &pipeline = schedule->trace.pipelines[index];
  out_info->label = viewFromString(pipeline.label);
  out_info->cache_key = pipeline.cache_key;
  out_info->descriptor_layout_hash = pipeline.descriptor_layout_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_transient_allocation(
    const AsterFrameScheduleHandle schedule, const std::size_t index,
    AsterFrameScheduleTransientAllocationInfo *out_info) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule transient allocation struct version is not supported");
  }
  if (index >= schedule->trace.transient_allocations.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame schedule transient allocation index is out of range");
  }
  const aster::rhi::TransientAllocationTrace &allocation =
      schedule->trace.transient_allocations[index];
  out_info->label = viewFromString(allocation.label);
  out_info->physical_allocation_id = allocation.physical_allocation_id;
  out_info->first_pass = allocation.first_pass;
  out_info->last_pass = allocation.last_pass;
  out_info->byte_size = allocation.byte_size;
  out_info->resource_count = allocation.resources.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_timeline(
    const AsterFrameScheduleHandle schedule, const std::size_t index,
    AsterFrameScheduleTimelineInfo *out_info) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame schedule timeline struct version is not supported");
  }
  if (index >= schedule->trace.queue_submits.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame schedule timeline index is out of range");
  }
  const aster::rhi::QueueSubmitTrace &submit = schedule->trace.queue_submits[index];
  out_info->label = viewFromString(submit.label);
  out_info->queue = rhiQueueKind(submit.queue);
  out_info->submitted_value = submit.signal_fence_value;
  out_info->completed_value =
      index < schedule->trace.fences.size() ? schedule->trace.fences[index].completed_value
                                            : submit.signal_fence_value;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_frame_schedule_validation_event(
    const AsterFrameScheduleHandle schedule, const std::size_t index,
    AsterValidationEvent *out_event) {
  if (!validFrameSchedule(schedule)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame schedule handle is invalid");
  }
  return validationEventAt(schedule->validation_events, index, out_event);
}

AsterStatus aster_kernel_frame_schedule_destroy(const AsterFrameScheduleHandle schedule) {
  return destroyHandle(schedule, kFrameScheduleMagic);
}

AsterStatus aster_kernel_renderer_destroy(const AsterRendererHandle renderer) {
  return destroyHandle(renderer, kRendererMagic);
}

AsterStatus aster_kernel_mesh_create(const AsterEngineHandle engine, const AsterMeshDesc *desc,
                                     AsterMeshHandle *out_mesh) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_mesh == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_mesh is null");
  }
  *out_mesh = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "mesh descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "mesh label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  if (desc->vertices.size > 0u) {
    if (desc->vertices.data == nullptr || desc->vertices.stride < sizeof(AsterVertex)) {
      return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                "custom mesh vertex span is invalid",
                                ASTER_VALIDATION_INVALID_MESH, "mesh.create", label);
    }
    if (desc->indices.data == nullptr || desc->indices.size == 0u ||
        desc->indices.stride < sizeof(std::uint32_t) || (desc->indices.size % 3u) != 0u) {
      return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                "custom mesh index span must contain triangles",
                                ASTER_VALIDATION_INVALID_MESH, "mesh.create", label);
    }
    const auto *vertices = static_cast<const unsigned char *>(desc->vertices.data);
    for (std::size_t i = 0; i < desc->vertices.size; ++i) {
      const auto *vertex =
          reinterpret_cast<const AsterVertex *>(vertices + i * desc->vertices.stride);
      const float normal_len_sq = vertex->normal.x * vertex->normal.x +
                                  vertex->normal.y * vertex->normal.y +
                                  vertex->normal.z * vertex->normal.z;
      if (!finiteVec3(vertex->position) || !finiteVec3(vertex->normal) ||
          !finiteVec2(vertex->uv) || !finiteVec4(vertex->tangent) ||
          !finiteValue(vertex->ambient_occlusion) || normal_len_sq < 0.25f ||
          normal_len_sq > 2.25f) {
        return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                  "custom mesh vertices require finite attributes and normals",
                                  ASTER_VALIDATION_INVALID_MESH, "mesh.create", label, i);
      }
    }
    const auto *indices = static_cast<const unsigned char *>(desc->indices.data);
    for (std::size_t i = 0; i < desc->indices.size; ++i) {
      const auto *index =
          reinterpret_cast<const std::uint32_t *>(indices + i * desc->indices.stride);
      if (*index >= desc->vertices.size) {
        return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                  "custom mesh index references a missing vertex",
                                  ASTER_VALIDATION_INVALID_MESH, "mesh.create", label, i);
      }
    }
  } else if (desc->indices.size > 0u) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "custom mesh indices require vertices",
                              ASTER_VALIDATION_INVALID_MESH, "mesh.create", label);
  }
  try {
    auto *mesh = new AsterMeshHandle__();
    mesh->primitive = meshPrimitive(desc->primitive);
    mesh->label = label;
    aster::CpuMesh custom = customMeshFromDesc(*desc);
    if (!custom.vertices.empty() && !custom.indices.empty()) {
      mesh->custom_mesh = std::make_shared<aster::CpuMesh>(std::move(custom));
    }
    *out_mesh = mesh;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "mesh allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_mesh_destroy(const AsterMeshHandle mesh) {
  return destroyHandle(mesh, kMeshMagic);
}

AsterStatus aster_kernel_material_create(const AsterEngineHandle engine,
                                         const AsterMaterialDesc *desc,
                                         AsterMaterialHandle *out_material) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_material == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_material is null");
  }
  *out_material = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "material descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "material label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  std::vector<AsterTextureRole> texture_roles;
  if (desc->texture_bindings.size > 0u) {
    if (desc->texture_bindings.data == nullptr ||
        desc->texture_bindings.stride < sizeof(AsterMaterialTextureBinding)) {
      return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                "material texture binding span is invalid",
                                ASTER_VALIDATION_LIFETIME_ERROR, "material.create", label);
    }
    const auto *bindings = static_cast<const unsigned char *>(desc->texture_bindings.data);
    texture_roles.reserve(desc->texture_bindings.size);
    for (std::size_t i = 0; i < desc->texture_bindings.size; ++i) {
      const auto *binding = reinterpret_cast<const AsterMaterialTextureBinding *>(
          bindings + i * desc->texture_bindings.stride);
      if (!validStruct(binding)) {
        return failWithValidation(engine, ASTER_STATUS_ABI_MISMATCH,
                                  "material texture binding version is not supported",
                                  ASTER_VALIDATION_LIFETIME_ERROR, "material.create", label, i);
      }
      if (retiredTexture(binding->texture)) {
        return failWithValidation(engine, ASTER_STATUS_LIFETIME_ERROR,
                                  "material references a destroyed texture",
                                  ASTER_VALIDATION_DESTROYED_HANDLE_USE, "material.create", label,
                                  i);
      }
      if (!validTexture(binding->texture)) {
        return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                  "material texture handle is invalid",
                                  ASTER_VALIDATION_LIFETIME_ERROR, "material.create", label, i);
      }
      if (binding->role != binding->texture->role) {
        return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                  "material texture binding role does not match texture role",
                                  ASTER_VALIDATION_TEXTURE_ROLE_MISMATCH, "material.create",
                                  label, i);
      }
      if (binding->role == ASTER_TEXTURE_ROLE_NORMAL &&
          binding->texture->normal_convention != ASTER_TEXTURE_NORMAL_CONVENTION_OPENGL) {
        return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                  "normal texture convention must be OpenGL for LitPBR",
                                  ASTER_VALIDATION_TEXTURE_NORMAL_CONVENTION_MISMATCH,
                                  "material.create", label, i);
      }
      texture_roles.push_back(binding->role);
    }
    const auto has_role = [&texture_roles](const AsterTextureRole role) {
      return std::find(texture_roles.begin(), texture_roles.end(), role) != texture_roles.end();
    };
    if (!has_role(ASTER_TEXTURE_ROLE_ALBEDO) || !has_role(ASTER_TEXTURE_ROLE_NORMAL) ||
        !has_role(ASTER_TEXTURE_ROLE_ORM)) {
      return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                                "LitPBR texture binding requires albedo, normal, and ORM roles",
                                ASTER_VALIDATION_MISSING_REQUIRED_TEXTURE, "material.create",
                                label);
    }
  }
  try {
    auto *material = new AsterMaterialHandle__();
    material->label = label;
    material->material = aster::makeMaterial({.base_color = aster::LinearRgb{vec(desc->base_color)},
                                              .emission_color =
                                                  aster::EmissionColor{vec(desc->emission_color)},
                                              .roughness = desc->roughness > 0.0f ? desc->roughness : 0.55f,
                                              .metallic = desc->metallic,
                                              .emission_strength = desc->emission_strength,
                                              .opacity = desc->opacity > 0.0f ? desc->opacity : 1.0f,
                                              .double_sided = desc->double_sided != 0u,
                                              .alpha_mode = alphaMode(desc->alpha_mode)});
    material->texture_roles = std::move(texture_roles);
    *out_material = material;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "material allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_material_destroy(const AsterMaterialHandle material) {
  return destroyHandle(material, kMaterialMagic);
}

AsterStatus aster_kernel_texture_create(const AsterEngineHandle engine,
                                        const AsterTextureDesc *desc,
                                        AsterTextureHandle *out_texture) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_texture == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_texture is null");
  }
  *out_texture = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "texture descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "texture label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  if (desc->width == 0u || desc->height == 0u || desc->mip_count == 0u ||
      desc->format == ASTER_KERNEL_BACKEND_FORMAT_UNKNOWN ||
      desc->role == ASTER_TEXTURE_ROLE_UNKNOWN) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "texture descriptor dimensions, format, and role must be explicit",
                              ASTER_VALIDATION_TEXTURE_ROLE_MISMATCH, "texture.create", label);
  }
  const bool expected_srgb = colorSpaceRequiresSrgb(desc->role);
  if ((expected_srgb && desc->color_space != ASTER_TEXTURE_COLOR_SPACE_SRGB) ||
      (!expected_srgb && desc->color_space != ASTER_TEXTURE_COLOR_SPACE_LINEAR)) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "texture role does not match declared color space",
                              ASTER_VALIDATION_TEXTURE_COLOR_SPACE_MISMATCH, "texture.create",
                              label, static_cast<std::uint64_t>(desc->role));
  }
  if (srgbFormat(desc->format) != (desc->color_space == ASTER_TEXTURE_COLOR_SPACE_SRGB)) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "texture format sRGB flag does not match declared color space",
                              ASTER_VALIDATION_TEXTURE_COLOR_SPACE_MISMATCH, "texture.create",
                              label, static_cast<std::uint64_t>(desc->format));
  }
  if (desc->role == ASTER_TEXTURE_ROLE_NORMAL &&
      desc->normal_convention != ASTER_TEXTURE_NORMAL_CONVENTION_OPENGL) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "normal textures must declare the OpenGL normal convention",
                              ASTER_VALIDATION_TEXTURE_NORMAL_CONVENTION_MISMATCH,
                              "texture.create", label);
  }
  if (desc->role != ASTER_TEXTURE_ROLE_NORMAL &&
      desc->normal_convention != ASTER_TEXTURE_NORMAL_CONVENTION_NONE) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "only normal textures may declare a normal map convention",
                              ASTER_VALIDATION_TEXTURE_NORMAL_CONVENTION_MISMATCH,
                              "texture.create", label);
  }
  if (desc->data.size > 0u && (desc->data.data == nullptr || desc->data.stride == 0u)) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "texture payload span is invalid",
                              ASTER_VALIDATION_LIFETIME_ERROR, "texture.create", label);
  }
  try {
    auto *texture = new AsterTextureHandle__();
    texture->role = desc->role;
    texture->color_space = desc->color_space;
    texture->normal_convention = desc->normal_convention;
    texture->format = desc->format;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->mip_count = desc->mip_count;
    texture->label = label.empty() ? textureRoleName(desc->role) : label;
    *out_texture = texture;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "texture allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_texture_destroy(const AsterTextureHandle texture) {
  return retireHandle(texture, kTextureMagic);
}

AsterStatus aster_kernel_render_target_create(const AsterEngineHandle engine,
                                              const AsterRenderTargetDesc *desc,
                                              AsterRenderTargetHandle *out_target) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_target == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_target is null");
  }
  *out_target = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "render target descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "render target label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  if (desc->width == 0u || desc->height == 0u || desc->sample_count == 0u ||
      desc->color_format == ASTER_KERNEL_BACKEND_FORMAT_UNKNOWN) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "render target dimensions, samples, and color format must be explicit",
                              ASTER_VALIDATION_RENDER_TARGET_MISMATCH, "render_target.create",
                              label);
  }
  try {
    auto *target = new AsterRenderTargetHandle__();
    target->color_format = desc->color_format;
    target->depth_format = desc->depth_format;
    target->width = desc->width;
    target->height = desc->height;
    target->sample_count = desc->sample_count;
    target->label = label;
    *out_target = target;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "render target allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_render_target_destroy(const AsterRenderTargetHandle target) {
  return retireHandle(target, kRenderTargetMagic);
}

AsterStatus aster_kernel_buffer_create(const AsterEngineHandle engine,
                                       const AsterBufferDesc *desc,
                                       AsterBufferHandle *out_buffer) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_buffer == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_buffer is null");
  }
  *out_buffer = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "buffer descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "buffer label has a size but no data");
  }
  if (desc->byte_size == 0u) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "buffer byte_size must be non-zero",
                              ASTER_VALIDATION_LIFETIME_ERROR, "buffer.create",
                              stringFromView(desc->debug_label));
  }
  try {
    auto *buffer = new AsterBufferHandle__();
    buffer->byte_size = desc->byte_size;
    buffer->usage = desc->usage;
    buffer->label = stringFromView(desc->debug_label);
    *out_buffer = buffer;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "buffer allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_buffer_destroy(const AsterBufferHandle buffer) {
  return retireHandle(buffer, kBufferMagic);
}

AsterStatus aster_kernel_descriptor_heap_create(const AsterEngineHandle engine,
                                                const AsterDescriptorHeapDesc *desc,
                                                AsterDescriptorHeapHandle *out_heap) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_heap == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_heap is null");
  }
  *out_heap = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "descriptor heap descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "descriptor heap label has a size but no data");
  }
  if (desc->descriptor_capacity == 0u) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "descriptor heap capacity must be non-zero",
                              ASTER_VALIDATION_LIFETIME_ERROR, "descriptor_heap.create",
                              stringFromView(desc->debug_label));
  }
  try {
    auto *heap = new AsterDescriptorHeapHandle__();
    heap->descriptor_capacity = desc->descriptor_capacity;
    heap->shader_visible = desc->shader_visible != 0u;
    heap->label = stringFromView(desc->debug_label);
    *out_heap = heap;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "descriptor heap allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_descriptor_heap_destroy(const AsterDescriptorHeapHandle heap) {
  return retireHandle(heap, kDescriptorHeapMagic);
}

AsterStatus aster_kernel_descriptor_set_create(const AsterEngineHandle engine,
                                               const AsterDescriptorSetDesc *desc,
                                               AsterDescriptorSetHandle *out_set) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_set == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_set is null");
  }
  *out_set = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "descriptor set descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "descriptor set label has a size but no data");
  }
  const std::string label = stringFromView(desc->debug_label);
  if (retiredDescriptorHeap(desc->heap)) {
    return failWithValidation(engine, ASTER_STATUS_LIFETIME_ERROR,
                              "descriptor set references a destroyed descriptor heap",
                              ASTER_VALIDATION_DESTROYED_HANDLE_USE, "descriptor_set.create",
                              label);
  }
  if (!validDescriptorHeap(desc->heap) || desc->descriptor_count == 0u ||
      desc->descriptor_count > desc->heap->descriptor_capacity) {
    return failWithValidation(engine, ASTER_STATUS_VALIDATION_ERROR,
                              "descriptor set requires a valid heap and in-capacity descriptor count",
                              ASTER_VALIDATION_LIFETIME_ERROR, "descriptor_set.create", label);
  }
  try {
    auto *set = new AsterDescriptorSetHandle__();
    set->heap = desc->heap;
    set->descriptor_count = desc->descriptor_count;
    set->label = label;
    *out_set = set;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "descriptor set allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_descriptor_set_destroy(const AsterDescriptorSetHandle set) {
  return retireHandle(set, kDescriptorSetMagic);
}

AsterStatus aster_kernel_pipeline_cache_create(const AsterEngineHandle engine,
                                               const AsterPipelineCacheDesc *desc,
                                               AsterPipelineCacheHandle *out_cache) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_cache == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_cache is null");
  }
  *out_cache = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "pipeline cache descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "pipeline cache label has a size but no data");
  }
  try {
    auto *cache = new AsterPipelineCacheHandle__();
    cache->seed = desc->seed;
    cache->label = stringFromView(desc->debug_label);
    *out_cache = cache;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "pipeline cache allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_pipeline_cache_destroy(const AsterPipelineCacheHandle cache) {
  return retireHandle(cache, kPipelineCacheMagic);
}

AsterStatus aster_kernel_shader_compile(const AsterEngineHandle engine,
                                        const AsterShaderCompileDesc *desc,
                                        AsterShaderArtifactHandle *out_shader) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_shader == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_shader is null");
  }
  *out_shader = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "shader compile descriptor version is not supported");
  }
  if (desc->modules.size > 0u &&
      (desc->modules.data == nullptr || desc->modules.stride < sizeof(AsterShaderModuleSource))) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader modules span is invalid");
  }
  try {
    aster::ShaderLibrary library;
    std::vector<std::string> module_names;
    const auto *modules = static_cast<const unsigned char *>(desc->modules.data);
    for (std::size_t i = 0; i < desc->modules.size; ++i) {
      const auto *module =
          reinterpret_cast<const AsterShaderModuleSource *>(modules + i * desc->modules.stride);
      if (!validStringView(module->name) || !validStringView(module->source)) {
        return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader module string view is invalid");
      }
      std::string name = stringFromView(module->name);
      if (name.empty()) {
        name = "module_" + std::to_string(i);
      }
      library.addModule(name, stringFromView(module->source));
      module_names.push_back(std::move(name));
    }

    auto *shader = new AsterShaderArtifactHandle__();
    aster::ShaderVariantKey variant;
    variant.feature_mask = desc->feature_mask;
    variant.tag = stringFromView(desc->variant_tag);
    shader->result = aster::compileShaderVariant(
        library, {.backend = shaderBackend(desc->backend),
                  .variant = variant,
                  .modules = module_names,
                  .entry_point = stringFromView(desc->entry_point).empty()
                                     ? std::string("fs_main")
                                     : stringFromView(desc->entry_point)});
    shader->reflection_names.reserve(shader->result.reflection.resources.size());
    shader->reflection.reserve(shader->result.reflection.resources.size());
    for (const aster::ShaderResourceBinding &binding : shader->result.reflection.resources) {
      shader->reflection_names.push_back(binding.name);
      shader->reflection.push_back({.size = sizeof(AsterShaderReflectionBinding),
                                    .version = ASTER_KERNEL_STRUCT_VERSION_1,
                                    .name = {},
                                    .kind = shaderResourceKind(binding.kind),
                                    .binding = binding.binding,
                                    .count = binding.count});
    }
    for (std::size_t i = 0; i < shader->reflection.size(); ++i) {
      shader->reflection[i].name = viewFromString(shader->reflection_names[i]);
    }
    *out_shader = shader;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "shader allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "shader compile failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_result(const AsterShaderArtifactHandle shader,
                                           AsterShaderCompileResult *out_result) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (!validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "shader result struct version is not supported");
  }
  out_result->success = shader->result.success ? 1u : 0u;
  out_result->source_size = shader->result.source.size();
  out_result->diagnostic_count = shader->result.diagnostics.size();
  out_result->reflection_binding_count = shader->reflection.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_source(const AsterShaderArtifactHandle shader,
                                           AsterStringView *out_source) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (out_source == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_source is null");
  }
  *out_source = viewFromString(shader->result.source);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_diagnostics(const AsterShaderArtifactHandle shader,
                                                const std::size_t index,
                                                AsterStringView *out_diagnostic) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (out_diagnostic == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_diagnostic is null");
  }
  if (index >= shader->result.diagnostics.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader diagnostic index is out of range");
  }
  *out_diagnostic = viewFromString(shader->result.diagnostics[index]);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_reflection(const AsterShaderArtifactHandle shader,
                                               const std::size_t index,
                                               AsterShaderReflectionBinding *out_binding) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (!validStruct(out_binding)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "reflection binding struct version is not supported");
  }
  if (index >= shader->reflection.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader reflection index is out of range");
  }
  *out_binding = shader->reflection[index];
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_destroy(const AsterShaderArtifactHandle shader) {
  return destroyHandle(shader, kShaderMagic);
}

AsterStatus aster_kernel_render_pipeline_create(const AsterEngineHandle engine,
                                                const AsterRenderPipelineDesc *desc,
                                                AsterRenderPipelineHandle *out_pipeline) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_pipeline == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_pipeline is null");
  }
  *out_pipeline = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "render pipeline descriptor version is not supported");
  }
  if (!validShader(desc->shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "pipeline shader handle is invalid");
  }
  try {
    auto *pipeline = new AsterRenderPipelineHandle__();
    pipeline->shader = desc->shader;
    pipeline->label = stringFromView(desc->debug_label);
    *out_pipeline = pipeline;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "pipeline allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_render_pipeline_destroy(const AsterRenderPipelineHandle pipeline) {
  return destroyHandle(pipeline, kPipelineMagic);
}

AsterStatus aster_kernel_authoring_document_load(const AsterAuthoringDocumentDesc *desc,
                                                 AsterAuthoringDocumentHandle *out_document) {
  if (out_document == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_document is null");
  }
  *out_document = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring document descriptor version is not supported");
  }
  if (!validStringView(desc->source_text) || !validStringView(desc->source_path) ||
      !validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document string view is invalid");
  }
  if (desc->source_text.size == 0u && desc->source_path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "authoring document requires source text or source path");
  }
  const std::string source_text = stringFromView(desc->source_text);
  const std::filesystem::path source_path = stringFromView(desc->source_path);
  const bool parse_text = desc->source_text.size > 0u;
  try {
    auto *document = new AsterAuthoringDocumentHandle__();
    document->kind = desc->kind;
    const auto store = [&](auto result) {
      document->document = std::move(result.value);
      document->diagnostics = std::move(result.diagnostics);
    };
    switch (desc->kind) {
    case ASTER_AUTHORING_DOCUMENT_PROJECT:
      store(parse_text ? aster::sdk::parseProjectDocument(source_text, source_path)
                       : aster::sdk::loadProjectDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_SCENE:
      store(parse_text ? aster::sdk::parseSceneDocument(source_text, source_path)
                       : aster::sdk::loadSceneDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_PREFAB:
      store(parse_text ? aster::sdk::parsePrefabDocument(source_text, source_path)
                       : aster::sdk::loadPrefabDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_ITEM:
      store(parse_text ? aster::sdk::parseItemDocument(source_text, source_path)
                       : aster::sdk::loadItemDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_ACTION_GRAPH:
      store(parse_text ? aster::sdk::parseActionGraphDocument(source_text, source_path)
                       : aster::sdk::loadActionGraphDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_INPUT_MAP:
      store(parse_text ? aster::sdk::parseInputMapDocument(source_text, source_path)
                       : aster::sdk::loadInputMapDocument(source_path));
      break;
    case ASTER_AUTHORING_DOCUMENT_UNKNOWN:
    default:
      delete document;
      return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document kind is unsupported");
    }
    *out_document = document;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "authoring document allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "authoring document load failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_document_info(const AsterAuthoringDocumentHandle document,
                                                 AsterAuthoringDocumentInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring document info struct version is not supported");
  }
  const size_t size = out_info->size;
  const uint32_t version = out_info->version;
  *out_info = {};
  out_info->size = size;
  out_info->version = version;
  out_info->kind = document->kind;
  out_info->valid = diagnosticsOk(document->diagnostics) ? 1u : 0u;
  out_info->diagnostic_count = document->diagnostics.size();
  if (const auto *project = authoringDocumentAs<aster::sdk::ProjectDocument>(document)) {
    out_info->schema_version = project->schema_version;
    out_info->name = viewFromString(project->name);
    out_info->project_asset_count = project->assets.size();
  } else if (const auto *scene = authoringDocumentAs<aster::sdk::SceneDocument>(document)) {
    out_info->schema_version = scene->schema_version;
    out_info->id = viewFromString(scene->id);
    out_info->name = viewFromString(scene->name);
    out_info->entity_count = scene->entities.size();
  } else if (const auto *prefab = authoringDocumentAs<aster::sdk::PrefabDocument>(document)) {
    out_info->schema_version = prefab->schema_version;
    out_info->id = viewFromString(prefab->id);
    out_info->name = viewFromString(prefab->name);
    out_info->entity_count = prefab->entities.size();
  } else if (const auto *item = authoringDocumentAs<aster::sdk::ItemDocument>(document)) {
    out_info->schema_version = item->schema_version;
    out_info->id = viewFromString(item->id);
    out_info->name = viewFromString(item->display_name);
  } else if (const auto *graph = authoringDocumentAs<aster::sdk::ActionGraphDocument>(document)) {
    out_info->schema_version = graph->schema_version;
    out_info->id = viewFromString(graph->id);
    out_info->name = viewFromString(graph->name);
    out_info->action_node_count = graph->nodes.size();
    out_info->contract_stamp = aster::sdk::actionGraphContractStamp(*graph);
  } else if (const auto *input = authoringDocumentAs<aster::sdk::InputMapDocument>(document)) {
    out_info->schema_version = input->schema_version;
    out_info->id = viewFromString(input->id);
    out_info->name = viewFromString(input->name);
    out_info->input_binding_count = input->bindings.size();
    out_info->contract_stamp = aster::sdk::inputMapContractStamp(*input);
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_document_diagnostic(
    const AsterAuthoringDocumentHandle document, const size_t index,
    AsterAuthoringDiagnosticInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring diagnostic info struct version is not supported");
  }
  if (index >= document->diagnostics.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring diagnostic index is out of range");
  }
  const aster::sdk::Diagnostic &diagnostic = document->diagnostics[index];
  out_info->severity = authoringSeverity(diagnostic.severity);
  out_info->source = authoringScratch(document, diagnostic.source.string());
  out_info->path = viewFromString(diagnostic.path);
  out_info->message = viewFromString(diagnostic.message);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_project_asset(
    const AsterAuthoringDocumentHandle document, const size_t index,
    AsterAuthoringProjectAssetInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring project asset info struct version is not supported");
  }
  const auto *project = authoringDocumentAs<aster::sdk::ProjectDocument>(document);
  if (project == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not a project");
  }
  if (index >= project->assets.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "project asset index is out of range");
  }
  const aster::sdk::ProjectAssetRef &asset = project->assets[index];
  out_info->id = viewFromString(asset.id);
  out_info->kind = authoringAssetKind(asset.kind);
  out_info->kind_name = {aster::sdk::assetKindName(asset.kind).data(),
                         aster::sdk::assetKindName(asset.kind).size()};
  out_info->path = authoringScratch(document, asset.path.generic_string());
  out_info->startup = asset.id == project->startup_scene ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_entity(const AsterAuthoringDocumentHandle document,
                                          const size_t index,
                                          AsterAuthoringEntityInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring entity info struct version is not supported");
  }
  const std::vector<aster::sdk::EntityDefinition> *entities = nullptr;
  if (const auto *scene = authoringDocumentAs<aster::sdk::SceneDocument>(document)) {
    entities = &scene->entities;
  } else if (const auto *prefab = authoringDocumentAs<aster::sdk::PrefabDocument>(document)) {
    entities = &prefab->entities;
  }
  if (entities == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document has no entities");
  }
  if (index >= entities->size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "entity index is out of range");
  }
  const aster::sdk::EntityDefinition &entity = (*entities)[index];
  out_info->id = viewFromString(entity.id);
  out_info->name = viewFromString(entity.name);
  out_info->parent = viewFromString(entity.parent);
  out_info->component_flags = componentFlags(entity.components);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_node(const AsterAuthoringDocumentHandle document,
                                               const size_t index,
                                               AsterAuthoringActionNodeInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring action node info struct version is not supported");
  }
  const auto *graph = authoringDocumentAs<aster::sdk::ActionGraphDocument>(document);
  if (graph == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an action graph");
  }
  if (index >= graph->nodes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "action node index is out of range");
  }
  const aster::sdk::ActionNode &node = graph->nodes[index];
  out_info->id = viewFromString(node.id);
  out_info->type = viewFromString(node.type);
  out_info->parameter_count = node.parameters.size();
  out_info->tag_count = node.tags.size();
  out_info->deterministic_stamp =
      aster::sdk::actionGraphContractStamp(*graph) ^ ((index + 1u) * 1099511628211ull);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_node_parameter(
    const AsterAuthoringDocumentHandle document, const size_t node_index,
    const size_t parameter_index, AsterAuthoringKeyValue *out_parameter) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_parameter)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring key/value struct version is not supported");
  }
  const auto *graph = authoringDocumentAs<aster::sdk::ActionGraphDocument>(document);
  if (graph == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an action graph");
  }
  if (node_index >= graph->nodes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "action node index is out of range");
  }
  const auto &parameters = graph->nodes[node_index].parameters;
  if (parameter_index >= parameters.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "action parameter index is out of range");
  }
  auto it = parameters.begin();
  std::advance(it, static_cast<std::ptrdiff_t>(parameter_index));
  out_parameter->key = viewFromString(it->first);
  out_parameter->value = viewFromString(it->second);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_node_tag(const AsterAuthoringDocumentHandle document,
                                                   const size_t node_index,
                                                   const size_t tag_index,
                                                   AsterStringView *out_tag) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (out_tag == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_tag is null");
  }
  const auto *graph = authoringDocumentAs<aster::sdk::ActionGraphDocument>(document);
  if (graph == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an action graph");
  }
  if (node_index >= graph->nodes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "action node index is out of range");
  }
  const auto &tags = graph->nodes[node_index].tags;
  if (tag_index >= tags.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "action tag index is out of range");
  }
  *out_tag = viewFromString(tags[tag_index].value);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_input_binding(
    const AsterAuthoringDocumentHandle document, const size_t index,
    AsterAuthoringInputBindingInfo *out_info) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring input binding info struct version is not supported");
  }
  const auto *input = authoringDocumentAs<aster::sdk::InputMapDocument>(document);
  if (input == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an input map");
  }
  if (index >= input->bindings.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "input binding index is out of range");
  }
  const aster::sdk::InputBindingDocument &binding = input->bindings[index];
  out_info->command = viewFromString(binding.command);
  out_info->device = authoringInputDevice(binding.device);
  out_info->key = viewFromString(binding.key);
  out_info->button = viewFromString(binding.button);
  out_info->scale = binding.scale;
  out_info->deadzone = binding.deadzone;
  out_info->tag_count = binding.tags.size();
  out_info->deterministic_stamp =
      aster::sdk::inputMapContractStamp(*input) ^ ((index + 1u) * 1469598103934665603ull);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_input_binding_tag(
    const AsterAuthoringDocumentHandle document, const size_t binding_index,
    const size_t tag_index, AsterStringView *out_tag) {
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (out_tag == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_tag is null");
  }
  const auto *input = authoringDocumentAs<aster::sdk::InputMapDocument>(document);
  if (input == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an input map");
  }
  if (binding_index >= input->bindings.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "input binding index is out of range");
  }
  const auto &tags = input->bindings[binding_index].tags;
  if (tag_index >= tags.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "input binding tag index is out of range");
  }
  *out_tag = viewFromString(tags[tag_index].value);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_execute(
    const AsterAuthoringDocumentHandle document, const AsterAuthoringActionContext *context,
    AsterAuthoringActionExecutionHandle *out_execution) {
  if (out_execution == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_execution is null");
  }
  *out_execution = nullptr;
  if (!validAuthoringDocument(document)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document handle is invalid");
  }
  if (!validStruct(context)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring action context version is not supported");
  }
  if (!validStringView(context->actor) || !validStringView(context->target) ||
      !validStringView(context->input)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action context string is invalid");
  }
  const auto *graph = authoringDocumentAs<aster::sdk::ActionGraphDocument>(document);
  if (graph == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring document is not an action graph");
  }
  try {
    auto *execution = new AsterAuthoringActionExecutionHandle__();
    const aster::sdk::ActionContext sdk_context{.actor = stringFromView(context->actor),
                                                .target = stringFromView(context->target),
                                                .input = stringFromView(context->input)};
    execution->execution = aster::sdk::ActionGraphRuntime{}.execute(*graph, sdk_context);
    *out_execution = execution;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "authoring action execution allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "authoring action execution failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_execution_info(
    const AsterAuthoringActionExecutionHandle execution,
    AsterAuthoringActionExecutionInfo *out_info) {
  if (!validAuthoringExecution(execution)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action execution handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring action execution info struct version is not supported");
  }
  out_info->valid = diagnosticsOk(execution->execution.diagnostics) ? 1u : 0u;
  out_info->diagnostic_count = execution->execution.diagnostics.size();
  out_info->event_count = execution->execution.events.size();
  out_info->contract_stamp = execution->execution.contract_stamp;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_execution_diagnostic(
    const AsterAuthoringActionExecutionHandle execution, const size_t index,
    AsterAuthoringDiagnosticInfo *out_info) {
  if (!validAuthoringExecution(execution)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action execution handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring diagnostic info struct version is not supported");
  }
  if (index >= execution->execution.diagnostics.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "authoring action diagnostic index is out of range");
  }
  const aster::sdk::Diagnostic &diagnostic = execution->execution.diagnostics[index];
  out_info->severity = authoringSeverity(diagnostic.severity);
  out_info->source = authoringScratch(execution, diagnostic.source.string());
  out_info->path = viewFromString(diagnostic.path);
  out_info->message = viewFromString(diagnostic.message);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_event(
    const AsterAuthoringActionExecutionHandle execution, const size_t index,
    AsterAuthoringActionEventInfo *out_info) {
  if (!validAuthoringExecution(execution)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action execution handle is invalid");
  }
  if (!validStruct(out_info)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring action event info struct version is not supported");
  }
  if (index >= execution->execution.events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action event index is out of range");
  }
  const aster::sdk::ActionEvent &event = execution->execution.events[index];
  out_info->node_id = viewFromString(event.node_id);
  out_info->type = viewFromString(event.type);
  out_info->actor = viewFromString(event.actor);
  out_info->target = viewFromString(event.target);
  out_info->parameter_count = event.parameters.size();
  out_info->tag_count = event.tags.size();
  out_info->deterministic_stamp = event.deterministic_stamp;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_event_parameter(
    const AsterAuthoringActionExecutionHandle execution, const size_t event_index,
    const size_t parameter_index, AsterAuthoringKeyValue *out_parameter) {
  if (!validAuthoringExecution(execution)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action execution handle is invalid");
  }
  if (!validStruct(out_parameter)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "authoring key/value struct version is not supported");
  }
  if (event_index >= execution->execution.events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action event index is out of range");
  }
  const auto &parameters = execution->execution.events[event_index].parameters;
  if (parameter_index >= parameters.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "authoring action event parameter index is out of range");
  }
  auto it = parameters.begin();
  std::advance(it, static_cast<std::ptrdiff_t>(parameter_index));
  out_parameter->key = viewFromString(it->first);
  out_parameter->value = viewFromString(it->second);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_event_tag(
    const AsterAuthoringActionExecutionHandle execution, const size_t event_index,
    const size_t tag_index, AsterStringView *out_tag) {
  if (!validAuthoringExecution(execution)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action execution handle is invalid");
  }
  if (out_tag == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_tag is null");
  }
  if (event_index >= execution->execution.events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "authoring action event index is out of range");
  }
  const auto &tags = execution->execution.events[event_index].tags;
  if (tag_index >= tags.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "authoring action event tag index is out of range");
  }
  *out_tag = viewFromString(tags[tag_index].value);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_authoring_action_execution_destroy(
    AsterAuthoringActionExecutionHandle execution) {
  return destroyHandle(execution, kAuthoringExecutionMagic);
}

AsterStatus aster_kernel_authoring_document_destroy(AsterAuthoringDocumentHandle document) {
  return destroyHandle(document, kAuthoringDocumentMagic);
}

AsterStatus aster_kernel_physics_world_destroy(const AsterPhysicsWorldHandle physics_world) {
  if (physics_world == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "physics world handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "physics world creation is not public yet");
}

AsterStatus aster_kernel_system_world_destroy(const AsterSystemWorldHandle system_world) {
  return destroyHandle(system_world, kSystemWorldMagic);
}

AsterStatus aster_kernel_sample_app_destroy(const AsterSampleAppHandle sample_app) {
  if (sample_app == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "sample app handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "sample app creation is not public yet");
}

} // extern "C"
