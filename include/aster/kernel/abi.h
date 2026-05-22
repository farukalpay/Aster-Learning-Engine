// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#ifndef ASTER_KERNEL_ABI_H
#define ASTER_KERNEL_ABI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(ASTER_KERNEL_BUILD)
#define ASTER_KERNEL_API __declspec(dllexport)
#else
#define ASTER_KERNEL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASTER_KERNEL_API __attribute__((visibility("default")))
#else
#define ASTER_KERNEL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ASTER_KERNEL_ABI_MAJOR 6u
#define ASTER_KERNEL_ABI_MINOR 3u
#define ASTER_KERNEL_ABI_PATCH 0u
#define ASTER_KERNEL_STRUCT_VERSION_1 1u

typedef struct AsterAbiVersion {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} AsterAbiVersion;

typedef int32_t AsterStatusCode;
enum {
  ASTER_STATUS_OK = 0,
  ASTER_STATUS_INVALID_ARGUMENT = 1,
  ASTER_STATUS_UNSUPPORTED = 2,
  ASTER_STATUS_OUT_OF_MEMORY = 3,
  ASTER_STATUS_INTERNAL_ERROR = 4,
  ASTER_STATUS_ABI_MISMATCH = 5,
  ASTER_STATUS_VALIDATION_ERROR = 6,
  ASTER_STATUS_CAPABILITY_MISMATCH = 7,
  ASTER_STATUS_LIFETIME_ERROR = 8
};

typedef struct AsterStatus {
  size_t size;
  uint32_t version;
  AsterStatusCode code;
  const char *message;
} AsterStatus;

typedef struct AsterStringView {
  const char *data;
  size_t size;
} AsterStringView;

typedef struct AsterSpan {
  const void *data;
  size_t size;
  size_t stride;
} AsterSpan;

typedef struct AsterEngineHandle__ *AsterEngineHandle;
typedef struct AsterWindowHandle__ *AsterWindowHandle;
typedef struct AsterSceneHandle__ *AsterSceneHandle;
typedef struct AsterRendererHandle__ *AsterRendererHandle;
typedef struct AsterMeshHandle__ *AsterMeshHandle;
typedef struct AsterMaterialHandle__ *AsterMaterialHandle;
typedef struct AsterPhysicsWorldHandle__ *AsterPhysicsWorldHandle;
typedef struct AsterWorldHandle__ *AsterWorldHandle;
typedef struct AsterSystemWorldHandle__ *AsterSystemWorldHandle;
typedef struct AsterSampleAppHandle__ *AsterSampleAppHandle;
typedef struct AsterShaderArtifactHandle__ *AsterShaderArtifactHandle;
typedef struct AsterRenderPipelineHandle__ *AsterRenderPipelineHandle;
typedef struct AsterTextureHandle__ *AsterTextureHandle;
typedef struct AsterRenderTargetHandle__ *AsterRenderTargetHandle;
typedef struct AsterBufferHandle__ *AsterBufferHandle;
typedef struct AsterDescriptorHeapHandle__ *AsterDescriptorHeapHandle;
typedef struct AsterDescriptorSetHandle__ *AsterDescriptorSetHandle;
typedef struct AsterPipelineCacheHandle__ *AsterPipelineCacheHandle;
typedef struct AsterFrameScheduleHandle__ *AsterFrameScheduleHandle;
typedef struct AsterAuthoringDocumentHandle__ *AsterAuthoringDocumentHandle;
typedef struct AsterAuthoringActionExecutionHandle__ *AsterAuthoringActionExecutionHandle;

typedef enum AsterKernelBackendKind {
  ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE = 0,
  ASTER_KERNEL_BACKEND_METAL = 1,
  ASTER_KERNEL_BACKEND_D3D12 = 2,
  ASTER_KERNEL_BACKEND_NULL = 3,
  ASTER_KERNEL_BACKEND_UNKNOWN = 255
} AsterKernelBackendKind;

typedef enum AsterKernelShaderBackend {
  ASTER_KERNEL_SHADER_BACKEND_METAL_MSL = 0,
  ASTER_KERNEL_SHADER_BACKEND_D3D12_HLSL = 1,
  ASTER_KERNEL_SHADER_BACKEND_SOFTWARE_REFERENCE = 2
} AsterKernelShaderBackend;

typedef enum AsterKernelShaderResourceKind {
  ASTER_KERNEL_SHADER_RESOURCE_UNIFORM_BUFFER = 0,
  ASTER_KERNEL_SHADER_RESOURCE_TEXTURE = 1,
  ASTER_KERNEL_SHADER_RESOURCE_SAMPLER = 2,
  ASTER_KERNEL_SHADER_RESOURCE_STORAGE_BUFFER = 3
} AsterKernelShaderResourceKind;

typedef enum AsterKernelBackendFormat {
  ASTER_KERNEL_BACKEND_FORMAT_UNKNOWN = 0,
  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM = 1,
  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_SRGB = 2,
  ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM = 3,
  ASTER_KERNEL_BACKEND_FORMAT_BGRA8_SRGB = 4,
  ASTER_KERNEL_BACKEND_FORMAT_RGBA16_FLOAT = 5,
  ASTER_KERNEL_BACKEND_FORMAT_RG8_UNORM = 6,
  ASTER_KERNEL_BACKEND_FORMAT_R8_UNORM = 7,
  ASTER_KERNEL_BACKEND_FORMAT_BC1_RGBA_UNORM = 8,
  ASTER_KERNEL_BACKEND_FORMAT_BC1_RGBA_SRGB = 9,
  ASTER_KERNEL_BACKEND_FORMAT_BC3_RGBA_UNORM = 10,
  ASTER_KERNEL_BACKEND_FORMAT_BC3_RGBA_SRGB = 11,
  ASTER_KERNEL_BACKEND_FORMAT_BC5_RG_UNORM = 12,
  ASTER_KERNEL_BACKEND_FORMAT_BC7_RGBA_UNORM = 13,
  ASTER_KERNEL_BACKEND_FORMAT_BC7_RGBA_SRGB = 14,
  ASTER_KERNEL_BACKEND_FORMAT_ASTC4X4_RGBA_UNORM = 15,
  ASTER_KERNEL_BACKEND_FORMAT_ASTC4X4_RGBA_SRGB = 16,
  ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT = 17
} AsterKernelBackendFormat;

typedef enum AsterKernelBackendSamplerFilter {
  ASTER_KERNEL_BACKEND_SAMPLER_FILTER_NEAREST = 0,
  ASTER_KERNEL_BACKEND_SAMPLER_FILTER_LINEAR = 1
} AsterKernelBackendSamplerFilter;

typedef enum AsterKernelBackendSamplerAddressMode {
  ASTER_KERNEL_BACKEND_SAMPLER_ADDRESS_CLAMP_TO_EDGE = 0,
  ASTER_KERNEL_BACKEND_SAMPLER_ADDRESS_REPEAT = 1,
  ASTER_KERNEL_BACKEND_SAMPLER_ADDRESS_MIRRORED_REPEAT = 2
} AsterKernelBackendSamplerAddressMode;

typedef enum AsterKernelBackendBlendMode {
  ASTER_KERNEL_BACKEND_BLEND_OPAQUE = 0,
  ASTER_KERNEL_BACKEND_BLEND_ALPHA = 1,
  ASTER_KERNEL_BACKEND_BLEND_ADDITIVE = 2
} AsterKernelBackendBlendMode;

typedef enum AsterKernelBackendShaderModel {
  ASTER_KERNEL_BACKEND_SHADER_MODEL_NONE = 0,
  ASTER_KERNEL_BACKEND_SHADER_MODEL_SOFTWARE_REFERENCE = 1,
  ASTER_KERNEL_BACKEND_SHADER_MODEL_METAL_MSL_2_3 = 2,
  ASTER_KERNEL_BACKEND_SHADER_MODEL_D3D12_SHADER_MODEL_5_1 = 3
} AsterKernelBackendShaderModel;

typedef enum AsterKernelBackendPresentationMode {
  ASTER_KERNEL_BACKEND_PRESENTATION_NONE = 0,
  ASTER_KERNEL_BACKEND_PRESENTATION_SOFTWARE_FRAMEBUFFER = 1,
  ASTER_KERNEL_BACKEND_PRESENTATION_METAL_LAYER = 2,
  ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_OFFSCREEN_READBACK = 3,
  ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_SWAPCHAIN = 4
} AsterKernelBackendPresentationMode;

typedef enum AsterKernelMeshPrimitive {
  ASTER_KERNEL_MESH_PRIMITIVE_BOX = 0,
  ASTER_KERNEL_MESH_PRIMITIVE_SPHERE = 1,
  ASTER_KERNEL_MESH_PRIMITIVE_PLANE = 2,
  ASTER_KERNEL_MESH_PRIMITIVE_ROCK = 3,
  ASTER_KERNEL_MESH_PRIMITIVE_CRYSTAL = 4,
  ASTER_KERNEL_MESH_PRIMITIVE_RUIN_BLOCK = 5,
  ASTER_KERNEL_MESH_PRIMITIVE_PILLAR = 6
} AsterKernelMeshPrimitive;

typedef enum AsterKernelMaterialAlphaMode {
  ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE = 0,
  ASTER_KERNEL_MATERIAL_ALPHA_MASKED = 1,
  ASTER_KERNEL_MATERIAL_ALPHA_DITHERED_COVERAGE = 2,
  ASTER_KERNEL_MATERIAL_ALPHA_BLEND = 3
} AsterKernelMaterialAlphaMode;

typedef enum AsterKernelRenderQualityTier {
  ASTER_KERNEL_RENDER_QUALITY_PRODUCTION = 0,
  ASTER_KERNEL_RENDER_QUALITY_PROTOTYPE = 1,
  ASTER_KERNEL_RENDER_QUALITY_CINEMATIC = 2
} AsterKernelRenderQualityTier;

typedef enum AsterKernelToneMapper {
  ASTER_KERNEL_TONE_MAPPER_PBR_NEUTRAL = 0,
  ASTER_KERNEL_TONE_MAPPER_FILMIC_ACES = 1,
  ASTER_KERNEL_TONE_MAPPER_REINHARD = 2
} AsterKernelToneMapper;

typedef enum AsterKernelRenderGraphPass {
  ASTER_KERNEL_RENDER_PASS_SCENE_COLOR_DEPTH = 0,
  ASTER_KERNEL_RENDER_PASS_LIGHT_CULL = 1,
  ASTER_KERNEL_RENDER_PASS_SHADOW_ATLAS = 2,
  ASTER_KERNEL_RENDER_PASS_OPAQUE = 3,
  ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW = 4,
  ASTER_KERNEL_RENDER_PASS_SCENE_LIGHTING = 5,
  ASTER_KERNEL_RENDER_PASS_VOLUMETRIC_FOG = 6,
  ASTER_KERNEL_RENDER_PASS_REFLECTION_PROBE = 7,
  ASTER_KERNEL_RENDER_PASS_TRANSPARENT = 8,
  ASTER_KERNEL_RENDER_PASS_UI_COMPOSITE = 9,
  ASTER_KERNEL_RENDER_PASS_CAPTURE = 10,
  ASTER_KERNEL_RENDER_PASS_SURFACE_OCCLUSION = 11
} AsterKernelRenderGraphPass;

typedef enum AsterKernelRenderGraphResource {
  ASTER_KERNEL_RENDER_RESOURCE_SCENE_COLOR = 0,
  ASTER_KERNEL_RENDER_RESOURCE_SCENE_DEPTH = 1,
  ASTER_KERNEL_RENDER_RESOURCE_LIGHT_CLUSTERS = 2,
  ASTER_KERNEL_RENDER_RESOURCE_SHADOW_ATLAS = 3,
  ASTER_KERNEL_RENDER_RESOURCE_VOLUMETRIC_FOG = 4,
  ASTER_KERNEL_RENDER_RESOURCE_REFLECTION_PROBES = 5,
  ASTER_KERNEL_RENDER_RESOURCE_UI_OVERLAY = 6,
  ASTER_KERNEL_RENDER_RESOURCE_CAPTURE_READBACK = 7,
  ASTER_KERNEL_RENDER_RESOURCE_SURFACE_ATTRIBUTES = 8,
  ASTER_KERNEL_RENDER_RESOURCE_SURFACE_OCCLUSION = 9
} AsterKernelRenderGraphResource;

typedef enum AsterKernelRhiResourceState {
  ASTER_KERNEL_RHI_RESOURCE_STATE_UNDEFINED = 0,
  ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_SOURCE = 1,
  ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_DESTINATION = 2,
  ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_READ = 3,
  ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_WRITE = 4,
  ASTER_KERNEL_RHI_RESOURCE_STATE_COLOR_ATTACHMENT = 5,
  ASTER_KERNEL_RHI_RESOURCE_STATE_DEPTH_ATTACHMENT = 6,
  ASTER_KERNEL_RHI_RESOURCE_STATE_PRESENT = 7,
  ASTER_KERNEL_RHI_RESOURCE_STATE_READBACK = 8
} AsterKernelRhiResourceState;

typedef enum AsterKernelRhiQueueKind {
  ASTER_KERNEL_RHI_QUEUE_GRAPHICS = 0,
  ASTER_KERNEL_RHI_QUEUE_COMPUTE = 1,
  ASTER_KERNEL_RHI_QUEUE_COPY = 2
} AsterKernelRhiQueueKind;

typedef enum AsterKernelBackendFeatureProofKind {
  ASTER_KERNEL_BACKEND_FEATURE_GRAPH_RESOURCE = 0,
  ASTER_KERNEL_BACKEND_FEATURE_CAPTURE = 1,
  ASTER_KERNEL_BACKEND_FEATURE_TEXTURE_SAMPLING = 2,
  ASTER_KERNEL_BACKEND_FEATURE_INSTANCING = 3,
  ASTER_KERNEL_BACKEND_FEATURE_GPU_TIMESTAMPS = 4,
  ASTER_KERNEL_BACKEND_FEATURE_HDR_RENDER_TARGET = 5,
  ASTER_KERNEL_BACKEND_FEATURE_MSAA = 6,
  ASTER_KERNEL_BACKEND_FEATURE_PRESENTATION = 7
} AsterKernelBackendFeatureProofKind;

typedef enum AsterKernelBackendFeatureProofStatus {
  ASTER_KERNEL_BACKEND_FEATURE_NOT_ADVERTISED = 0,
  ASTER_KERNEL_BACKEND_FEATURE_NOT_EXERCISED = 1,
  ASTER_KERNEL_BACKEND_FEATURE_PROVEN = 2,
  ASTER_KERNEL_BACKEND_FEATURE_MISSING_PROOF = 3,
  ASTER_KERNEL_BACKEND_FEATURE_UNSUPPORTED = 4
} AsterKernelBackendFeatureProofStatus;

typedef enum AsterKernelRhiValidationKind {
  ASTER_KERNEL_RHI_VALIDATION_READ_BEFORE_WRITE = 0,
  ASTER_KERNEL_RHI_VALIDATION_MISSING_BARRIER = 1,
  ASTER_KERNEL_RHI_VALIDATION_QUEUE_OWNERSHIP_MISMATCH = 2,
  ASTER_KERNEL_RHI_VALIDATION_DESCRIPTOR_RESOURCE_MISMATCH = 3,
  ASTER_KERNEL_RHI_VALIDATION_MISSING_RESOURCE = 4,
  ASTER_KERNEL_RHI_VALIDATION_RETIRED_RESOURCE_USE = 5,
  ASTER_KERNEL_RHI_VALIDATION_INVALID_RESOURCE_STATE = 6
} AsterKernelRhiValidationKind;

typedef enum AsterKernelFrameDiagnosticSeverity {
  ASTER_KERNEL_FRAME_DIAGNOSTIC_INFO = 0,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING = 1,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR = 2
} AsterKernelFrameDiagnosticSeverity;

typedef enum AsterKernelFrameDiagnosticKind {
  ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_FALLBACK = 0,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_MATERIAL_VARIANT_FALLBACK = 1,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_TRANSLUCENT_SORT_CHANGED = 2,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_NEAR_PLANE_CLIPPING = 3,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_RESOURCE_LIFETIME_HAZARD = 4,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CAPABILITY_MISMATCH = 5,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CLUSTERED_LIGHTING_FALLBACK = 6,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_MATH_CONTRACT = 7,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_NON_FINITE_WORLD_MATRIX = 8,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_SINGULAR_NORMAL_MATRIX = 9,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_NEGATIVE_SCALE_TANGENT_FLIP = 10,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_PROJECTION_CONVENTION_MISMATCH = 11,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_VIEWPORT_ORIGIN_MISMATCH = 12,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_PROJECTION_DRIFT = 13,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_PREDICATE_UNCERTAINTY = 14,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_ASSET_PROVENANCE_WARNING = 15,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_TEXTURE_ROLE_DEGRADED = 16,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_MESH_ATTRIBUTE_DEGRADED = 17,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_SURFACE_PRESENTATION_WARNING = 18,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_VISIBLE_VOID = 19,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_SUPPORT_RENDER_MISMATCH = 20,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_Z_FIGHT_CANDIDATE = 21,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_TRAVERSAL_BLOCKER = 22,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_LIGHT_SOURCE_UNREADABLE = 23,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_VOLUMETRIC_LIGHT_MISSING = 24,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_LIGHT_FALLOFF_DISCONTINUITY = 25,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CAVE_LIGHT_EXPOSURE_UNDERFLOW = 26,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CAVE_LIGHT_EXPOSURE_OVERFLOW = 27
} AsterKernelFrameDiagnosticKind;

typedef enum AsterValidationKind {
  ASTER_VALIDATION_UNKNOWN = 0,
  ASTER_VALIDATION_TEXTURE_ROLE_MISMATCH = 1,
  ASTER_VALIDATION_TEXTURE_COLOR_SPACE_MISMATCH = 2,
  ASTER_VALIDATION_TEXTURE_NORMAL_CONVENTION_MISMATCH = 3,
  ASTER_VALIDATION_MISSING_REQUIRED_TEXTURE = 4,
  ASTER_VALIDATION_INVALID_TRANSFORM = 5,
  ASTER_VALIDATION_INVALID_MESH = 6,
  ASTER_VALIDATION_DESTROYED_HANDLE_USE = 7,
  ASTER_VALIDATION_UNSUPPORTED_BACKEND_RESOURCE = 8,
  ASTER_VALIDATION_RENDER_TARGET_MISMATCH = 9,
  ASTER_VALIDATION_CAPTURE_BEFORE_RENDER = 10,
  ASTER_VALIDATION_BACKEND_CAPABILITY_MISMATCH = 11,
  ASTER_VALIDATION_LIFETIME_ERROR = 12,
  ASTER_VALIDATION_WORLD_MONOTONICITY = 13,
  ASTER_VALIDATION_WORLD_STALE_ENTITY = 14,
  ASTER_VALIDATION_WORLD_TRANSACTION_HAZARD = 15,
  ASTER_VALIDATION_WORLD_REPLAY_MISMATCH = 16,
  ASTER_VALIDATION_VISIBLE_VOID = 17,
  ASTER_VALIDATION_SUPPORT_RENDER_MISMATCH = 18,
  ASTER_VALIDATION_Z_FIGHT_CANDIDATE = 19,
  ASTER_VALIDATION_TRAVERSAL_BLOCKER = 20
} AsterValidationKind;

typedef enum AsterSystemComponentAccessMode {
  ASTER_SYSTEM_COMPONENT_ACCESS_READ = 0,
  ASTER_SYSTEM_COMPONENT_ACCESS_WRITE = 1
} AsterSystemComponentAccessMode;

typedef enum AsterSystemTraceEventKind {
  ASTER_SYSTEM_TRACE_INPUT_EVENT = 0,
  ASTER_SYSTEM_TRACE_SIMULATION_TICK = 1,
  ASTER_SYSTEM_TRACE_SCHEDULER_DECISION = 2,
  ASTER_SYSTEM_TRACE_ASSET_RESOLUTION = 3,
  ASTER_SYSTEM_TRACE_RESIDENCY_DECISION = 4,
  ASTER_SYSTEM_TRACE_RENDERABLE_EXTRACTION = 5,
  ASTER_SYSTEM_TRACE_FRAME_SUBMISSION = 6,
  ASTER_SYSTEM_TRACE_TRANSACTION_BEGIN = 7,
  ASTER_SYSTEM_TRACE_TRANSACTION_COMMIT = 8,
  ASTER_SYSTEM_TRACE_TRANSACTION_ABORT = 9,
  ASTER_SYSTEM_TRACE_ENTITY_CREATED = 10,
  ASTER_SYSTEM_TRACE_ENTITY_DESTROYED = 11,
  ASTER_SYSTEM_TRACE_SNAPSHOT_SAVED = 12,
  ASTER_SYSTEM_TRACE_SNAPSHOT_LOADED = 13,
  ASTER_SYSTEM_TRACE_MIGRATION = 14,
  ASTER_SYSTEM_TRACE_REPLAY = 15,
  ASTER_SYSTEM_TRACE_VALIDATION_ERROR = 16
} AsterSystemTraceEventKind;

typedef enum AsterResidencyDecisionKind {
  ASTER_RESIDENCY_KEEP = 0,
  ASTER_RESIDENCY_LOAD = 1,
  ASTER_RESIDENCY_EVICT = 2,
  ASTER_RESIDENCY_REJECT = 3
} AsterResidencyDecisionKind;

typedef enum AsterResidencyEvictionPolicy {
  ASTER_RESIDENCY_EVICT_PRIORITY_THEN_VISIBILITY = 0
} AsterResidencyEvictionPolicy;

typedef enum AsterWorldRegionGateVerdict {
  ASTER_WORLD_REGION_GATE_UNKNOWN = 0,
  ASTER_WORLD_REGION_GATE_ACCEPTED = 1,
  ASTER_WORLD_REGION_GATE_QUARANTINED = 2
} AsterWorldRegionGateVerdict;

typedef enum AsterWorldExtractionProvenance {
  ASTER_WORLD_EXTRACTION_COMPATIBILITY_SCENE = 0,
  ASTER_WORLD_EXTRACTION_WORLD_TRANSITION = 1
} AsterWorldExtractionProvenance;

typedef enum AsterPerceptualContinuityChannel {
  ASTER_PERCEPTUAL_CONTINUITY_SPATIAL_AFFORDANCE = 1u << 0u,
  ASTER_PERCEPTUAL_CONTINUITY_MOTION_CONTINUITY = 1u << 1u,
  ASTER_PERCEPTUAL_CONTINUITY_HAZARD_READABILITY = 1u << 2u,
  ASTER_PERCEPTUAL_CONTINUITY_MATERIAL_MEMORY = 1u << 3u,
  ASTER_PERCEPTUAL_CONTINUITY_LIGHTING_ATMOSPHERE = 1u << 4u,
  ASTER_PERCEPTUAL_CONTINUITY_EVENT_RESIDUE = 1u << 5u,
  ASTER_PERCEPTUAL_CONTINUITY_SENSORY_FEEDBACK = 1u << 6u,
  ASTER_PERCEPTUAL_CONTINUITY_AI_ATTENTION = 1u << 7u,
  ASTER_PERCEPTUAL_CONTINUITY_STREAMING_RESIDENCY = 1u << 8u,
  ASTER_PERCEPTUAL_CONTINUITY_UI_FEEDBACK = 1u << 9u,
  ASTER_PERCEPTUAL_CONTINUITY_RESOURCE_STATE = 1u << 10u
} AsterPerceptualContinuityChannel;

typedef struct AsterPerceptualContinuityBudget {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  uint32_t required_channel_mask;
  uint32_t observed_channel_mask;
  uint32_t missing_channel_mask;
  float continuity_score;
  float minimum_score;
  uint64_t reaction_package_hash;
  uint64_t material_memory_hash;
  uint64_t lighting_atmosphere_hash;
  uint64_t ai_attention_hash;
  uint64_t streaming_residency_lod_hash;
  uint64_t resource_state_hash;
  uint64_t event_residue_hash;
  uint64_t readability_audit_hash;
  AsterStringView diagnostic;
} AsterPerceptualContinuityBudget;

typedef enum AsterAuthoringDocumentKind {
  ASTER_AUTHORING_DOCUMENT_UNKNOWN = 0,
  ASTER_AUTHORING_DOCUMENT_PROJECT = 1,
  ASTER_AUTHORING_DOCUMENT_SCENE = 2,
  ASTER_AUTHORING_DOCUMENT_PREFAB = 3,
  ASTER_AUTHORING_DOCUMENT_ITEM = 4,
  ASTER_AUTHORING_DOCUMENT_ACTION_GRAPH = 5,
  ASTER_AUTHORING_DOCUMENT_INPUT_MAP = 6
} AsterAuthoringDocumentKind;

typedef enum AsterAuthoringAssetKind {
  ASTER_AUTHORING_ASSET_UNKNOWN = 0,
  ASTER_AUTHORING_ASSET_SCENE = 1,
  ASTER_AUTHORING_ASSET_PREFAB = 2,
  ASTER_AUTHORING_ASSET_CAVE = 3,
  ASTER_AUTHORING_ASSET_MATERIAL = 4,
  ASTER_AUTHORING_ASSET_ITEM = 5,
  ASTER_AUTHORING_ASSET_ACTION_GRAPH = 6,
  ASTER_AUTHORING_ASSET_INPUT_MAP = 7,
  ASTER_AUTHORING_ASSET_UI = 8,
  ASTER_AUTHORING_ASSET_MESH = 9,
  ASTER_AUTHORING_ASSET_TEXTURE = 10,
  ASTER_AUTHORING_ASSET_GRAPH = 11
} AsterAuthoringAssetKind;

typedef enum AsterAuthoringDiagnosticSeverity {
  ASTER_AUTHORING_DIAGNOSTIC_WARNING = 0,
  ASTER_AUTHORING_DIAGNOSTIC_ERROR = 1
} AsterAuthoringDiagnosticSeverity;

typedef enum AsterAuthoringInputDevice {
  ASTER_AUTHORING_INPUT_UNKNOWN = 0,
  ASTER_AUTHORING_INPUT_KEYBOARD = 1,
  ASTER_AUTHORING_INPUT_MOUSE = 2,
  ASTER_AUTHORING_INPUT_GAMEPAD = 3,
  ASTER_AUTHORING_INPUT_TOUCH = 4
} AsterAuthoringInputDevice;

enum {
  ASTER_AUTHORING_ENTITY_COMPONENT_TRANSFORM = 1u << 0u,
  ASTER_AUTHORING_ENTITY_COMPONENT_MESH_RENDERER = 1u << 1u,
  ASTER_AUTHORING_ENTITY_COMPONENT_COLLIDER = 1u << 2u,
  ASTER_AUTHORING_ENTITY_COMPONENT_LIGHT = 1u << 3u,
  ASTER_AUTHORING_ENTITY_COMPONENT_INTERACTABLE = 1u << 4u,
  ASTER_AUTHORING_ENTITY_COMPONENT_INVENTORY = 1u << 5u,
  ASTER_AUTHORING_ENTITY_COMPONENT_CAMERA = 1u << 6u,
  ASTER_AUTHORING_ENTITY_COMPONENT_CAVE_SCENE = 1u << 7u,
  ASTER_AUTHORING_ENTITY_COMPONENT_FIXTURE = 1u << 8u,
  ASTER_AUTHORING_ENTITY_COMPONENT_ORE_NODE = 1u << 9u,
  ASTER_AUTHORING_ENTITY_COMPONENT_TORCH_SOCKET = 1u << 10u,
  ASTER_AUTHORING_ENTITY_COMPONENT_SPAWN_POINT = 1u << 11u,
  ASTER_AUTHORING_ENTITY_COMPONENT_MINING = 1u << 12u,
  ASTER_AUTHORING_ENTITY_COMPONENT_CAVE_DEBUG = 1u << 13u
};

typedef enum AsterTextureRole {
  ASTER_TEXTURE_ROLE_ALBEDO = 0,
  ASTER_TEXTURE_ROLE_NORMAL = 1,
  ASTER_TEXTURE_ROLE_ORM = 2,
  ASTER_TEXTURE_ROLE_ROUGHNESS = 3,
  ASTER_TEXTURE_ROLE_METALLIC = 4,
  ASTER_TEXTURE_ROLE_AO = 5,
  ASTER_TEXTURE_ROLE_HEIGHT = 6,
  ASTER_TEXTURE_ROLE_EMISSIVE = 7,
  ASTER_TEXTURE_ROLE_WETNESS = 8,
  ASTER_TEXTURE_ROLE_OPACITY = 9,
  ASTER_TEXTURE_ROLE_MASK = 10,
  ASTER_TEXTURE_ROLE_UNKNOWN = 255
} AsterTextureRole;

typedef enum AsterTextureColorSpace {
  ASTER_TEXTURE_COLOR_SPACE_LINEAR = 0,
  ASTER_TEXTURE_COLOR_SPACE_SRGB = 1
} AsterTextureColorSpace;

typedef enum AsterTextureNormalConvention {
  ASTER_TEXTURE_NORMAL_CONVENTION_NONE = 0,
  ASTER_TEXTURE_NORMAL_CONVENTION_OPENGL = 1,
  ASTER_TEXTURE_NORMAL_CONVENTION_DIRECTX = 2
} AsterTextureNormalConvention;

typedef enum AsterMathError {
  ASTER_MATH_ERROR_NONE = 0,
  ASTER_MATH_ERROR_INVALID_ARGUMENT = 1,
  ASTER_MATH_ERROR_NON_FINITE_INPUT = 2,
  ASTER_MATH_ERROR_DEGENERATE_INPUT = 3,
  ASTER_MATH_ERROR_SINGULAR_MATRIX = 4,
  ASTER_MATH_ERROR_UNSUPPORTED_POLICY = 5
} AsterMathError;

typedef enum AsterMathCoordinateHandedness {
  ASTER_MATH_COORDINATE_RIGHT_HANDED = 0,
  ASTER_MATH_COORDINATE_LEFT_HANDED = 1
} AsterMathCoordinateHandedness;

typedef enum AsterMathClipDepthRange {
  ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE = 0,
  ASTER_MATH_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE = 1
} AsterMathClipDepthRange;

typedef enum AsterMathDepthDirection {
  ASTER_MATH_DEPTH_FORWARD_Z = 0,
  ASTER_MATH_DEPTH_REVERSE_Z = 1
} AsterMathDepthDirection;

typedef enum AsterMathSemanticSpace {
  ASTER_MATH_SPACE_UNTYPED = 0,
  ASTER_MATH_SPACE_LOCAL = 1,
  ASTER_MATH_SPACE_WORLD = 2,
  ASTER_MATH_SPACE_VIEW = 3,
  ASTER_MATH_SPACE_CLIP = 4,
  ASTER_MATH_SPACE_NDC = 5,
  ASTER_MATH_SPACE_SCREEN = 6
} AsterMathSemanticSpace;

enum {
  ASTER_KERNEL_WINDOW_FLAG_HEADLESS = 1u << 0u,
  ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE = 1u << 0u,
  ASTER_KERNEL_RENDERER_FLAG_FORCE_NULL = 1u << 1u,
  ASTER_KERNEL_CAMERA_FLAG_USE_PHYSICAL_LENS = 1u << 0u,
  ASTER_KERNEL_RENDER_SETTING_CONTACT_SHADOWS = 1u << 0u,
  ASTER_KERNEL_RENDER_SETTING_SURFACE_OCCLUSION = 1u << 1u,
  ASTER_KERNEL_RENDER_SETTING_CASCADED_SHADOWS = 1u << 2u,
  ASTER_KERNEL_RENDER_SETTING_REFLECTION_PROBES = 1u << 3u,
  ASTER_KERNEL_RENDER_SETTING_VOLUMETRIC_FOG = 1u << 4u,
  ASTER_KERNEL_RENDER_SETTING_PROCEDURAL_SURFACE_NORMALS = 1u << 5u,
  ASTER_KERNEL_RENDER_SETTING_FXAA = 1u << 6u,
  ASTER_KERNEL_RENDER_SETTING_BLOOM = 1u << 7u,
  ASTER_KERNEL_RENDER_SETTING_PRESENTATION_LENS = 1u << 8u,
  ASTER_KERNEL_BACKEND_CAP_GPU = 1u << 0u,
  ASTER_KERNEL_BACKEND_CAP_SHADER_MATERIALS = 1u << 1u,
  ASTER_KERNEL_BACKEND_CAP_TEXTURE_SAMPLING = 1u << 2u,
  ASTER_KERNEL_BACKEND_CAP_INSTANCING = 1u << 3u,
  ASTER_KERNEL_BACKEND_CAP_CAPTURE = 1u << 4u,
  ASTER_KERNEL_BACKEND_CAP_UI_COMPOSITE = 1u << 5u,
  ASTER_KERNEL_BACKEND_CAP_GPU_TIMESTAMPS = 1u << 6u
};

typedef struct AsterVec2 {
  float x;
  float y;
} AsterVec2;

typedef struct AsterVec3 {
  float x;
  float y;
  float z;
} AsterVec3;

typedef struct AsterVec4 {
  float x;
  float y;
  float z;
  float w;
} AsterVec4;

typedef struct AsterDVec2 {
  double x;
  double y;
} AsterDVec2;

typedef struct AsterDVec3 {
  double x;
  double y;
  double z;
} AsterDVec3;

typedef struct AsterDVec4 {
  double x;
  double y;
  double z;
  double w;
} AsterDVec4;

typedef struct AsterMat2 {
  float m[4];
} AsterMat2;

typedef struct AsterMat3 {
  float m[9];
} AsterMat3;

typedef struct AsterMat4 {
  float m[16];
} AsterMat4;

typedef struct AsterDMat2 {
  double m[4];
} AsterDMat2;

typedef struct AsterDMat3 {
  double m[9];
} AsterDMat3;

typedef struct AsterDMat4 {
  double m[16];
} AsterDMat4;

typedef struct AsterQuat {
  float x;
  float y;
  float z;
  float w;
} AsterQuat;

typedef struct AsterTransform {
  AsterVec3 position;
  AsterQuat rotation;
  AsterVec3 scale;
} AsterTransform;

typedef struct AsterRay3 {
  AsterVec3 origin;
  AsterVec3 direction;
  float max_distance;
} AsterRay3;

typedef struct AsterWorldPoint {
  AsterVec3 value;
} AsterWorldPoint;

typedef struct AsterViewPoint {
  AsterVec3 value;
} AsterViewPoint;

typedef struct AsterClipPoint {
  AsterVec4 value;
} AsterClipPoint;

typedef struct AsterNdcPoint {
  AsterVec3 value;
} AsterNdcPoint;

typedef struct AsterScreenPoint {
  AsterVec3 value;
} AsterScreenPoint;

typedef struct AsterWorldRay {
  AsterWorldPoint origin;
  AsterVec3 direction;
  float max_distance;
} AsterWorldRay;

typedef struct AsterViewport {
  AsterVec2 origin;
  AsterVec2 size;
  uint32_t origin_top_left;
} AsterViewport;

typedef struct AsterProjectionConvention {
  AsterMathCoordinateHandedness handedness;
  AsterMathClipDepthRange depth_range;
  AsterMathDepthDirection depth_direction;
  uint32_t viewport_origin_top_left;
  uint32_t y_flip;
  uint32_t column_major;
  uint32_t column_vector;
} AsterProjectionConvention;

typedef struct AsterPlane3 {
  AsterVec3 normal;
  float distance;
} AsterPlane3;

typedef struct AsterAabb3 {
  AsterVec3 min;
  AsterVec3 max;
} AsterAabb3;

typedef struct AsterSphere3 {
  AsterVec3 center;
  float radius;
} AsterSphere3;

typedef struct AsterMathPolicy {
  size_t size;
  uint32_t version;
  float absolute_epsilon;
  float relative_epsilon;
  uint32_t max_iterations;
  uint32_t deterministic;
  uint32_t debug_strict;
} AsterMathPolicy;

typedef struct AsterMathDiagnostics {
  size_t size;
  uint32_t version;
  AsterMathError error;
  float determinant;
  float condition_hint;
  const char *message;
} AsterMathDiagnostics;

typedef struct AsterExtent2D {
  uint32_t width;
  uint32_t height;
} AsterExtent2D;

typedef struct AsterEngineDesc {
  size_t size;
  uint32_t version;
  AsterStringView application_name;
  uint32_t flags;
} AsterEngineDesc;

typedef struct AsterWindowDesc {
  size_t size;
  uint32_t version;
  AsterStringView title;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
  uint32_t vsync;
} AsterWindowDesc;

typedef struct AsterRendererDesc {
  size_t size;
  uint32_t version;
  AsterWindowHandle window;
  AsterKernelBackendKind backend_preference;
  uint32_t flags;
} AsterRendererDesc;

typedef struct AsterBackendCapabilities {
  size_t size;
  uint32_t version;
  AsterKernelBackendKind backend;
  uint32_t flags;
  uint32_t graph_resource_mask;
  AsterStringView name;
} AsterBackendCapabilities;

typedef struct AsterBackendCapabilityTable {
  size_t size;
  uint32_t version;
  AsterKernelBackendKind backend;
  uint32_t flags;
  uint32_t graph_resource_mask;
  AsterStringView name;
  uint64_t color_format_mask;
  uint64_t depth_format_mask;
  uint64_t sample_count_mask;
  uint64_t sampler_filter_mask;
  uint64_t sampler_address_mode_mask;
  uint64_t blend_mode_mask;
  AsterKernelBackendShaderModel shader_model;
  AsterKernelBackendPresentationMode presentation;
  uint32_t max_color_attachments;
  uint32_t max_sampled_textures_per_material;
  uint32_t max_samplers_per_material;
  uint32_t max_uniform_buffers_per_stage;
  uint32_t max_storage_buffers_per_stage;
  uint32_t max_bind_groups;
  uint32_t max_vertex_attributes;
  uint32_t max_texture_dimension_2d;
  uint32_t max_dynamic_uniform_bytes;
} AsterBackendCapabilityTable;

typedef struct AsterPresentDesc {
  size_t size;
  uint32_t version;
  uint32_t vsync;
  uint32_t wait_for_frame;
} AsterPresentDesc;

typedef struct AsterPresentResult {
  size_t size;
  uint32_t version;
  uint32_t presented;
  AsterKernelBackendKind backend;
  AsterKernelBackendPresentationMode presentation;
  uint32_t width;
  uint32_t height;
  uint32_t backbuffer_index;
  uint64_t frame_index;
  size_t queue_waits;
} AsterPresentResult;

typedef struct AsterRendererPresentationStatus {
  size_t size;
  uint32_t version;
  AsterKernelBackendKind backend;
  AsterKernelBackendPresentationMode presentation;
  uint32_t native_present_supported;
  uint32_t bound_window;
  uint32_t width;
  uint32_t height;
  uint64_t last_presented_frame;
} AsterRendererPresentationStatus;

typedef struct AsterValidationEvent {
  size_t size;
  uint32_t version;
  AsterValidationKind kind;
  AsterKernelFrameDiagnosticSeverity severity;
  AsterStringView source;
  AsterStringView label;
  AsterStringView message;
  uint64_t value;
} AsterValidationEvent;

typedef struct AsterAuthoringDocumentDesc {
  size_t size;
  uint32_t version;
  AsterAuthoringDocumentKind kind;
  AsterStringView source_text;
  AsterStringView source_path;
  AsterStringView debug_label;
} AsterAuthoringDocumentDesc;

typedef struct AsterAuthoringDocumentInfo {
  size_t size;
  uint32_t version;
  AsterAuthoringDocumentKind kind;
  uint32_t schema_version;
  uint32_t valid;
  AsterStringView id;
  AsterStringView name;
  size_t diagnostic_count;
  size_t project_asset_count;
  size_t entity_count;
  size_t action_node_count;
  size_t input_binding_count;
  uint64_t contract_stamp;
} AsterAuthoringDocumentInfo;

typedef struct AsterAuthoringDiagnosticInfo {
  size_t size;
  uint32_t version;
  AsterAuthoringDiagnosticSeverity severity;
  AsterStringView source;
  AsterStringView path;
  AsterStringView message;
} AsterAuthoringDiagnosticInfo;

typedef struct AsterAuthoringProjectAssetInfo {
  size_t size;
  uint32_t version;
  AsterStringView id;
  AsterAuthoringAssetKind kind;
  AsterStringView kind_name;
  AsterStringView path;
  uint32_t startup;
} AsterAuthoringProjectAssetInfo;

typedef struct AsterAuthoringEntityInfo {
  size_t size;
  uint32_t version;
  AsterStringView id;
  AsterStringView name;
  AsterStringView parent;
  uint32_t component_flags;
} AsterAuthoringEntityInfo;

typedef struct AsterAuthoringActionNodeInfo {
  size_t size;
  uint32_t version;
  AsterStringView id;
  AsterStringView type;
  size_t parameter_count;
  size_t tag_count;
  uint64_t deterministic_stamp;
} AsterAuthoringActionNodeInfo;

typedef struct AsterAuthoringKeyValue {
  size_t size;
  uint32_t version;
  AsterStringView key;
  AsterStringView value;
} AsterAuthoringKeyValue;

typedef struct AsterAuthoringInputBindingInfo {
  size_t size;
  uint32_t version;
  AsterStringView command;
  AsterAuthoringInputDevice device;
  AsterStringView key;
  AsterStringView button;
  float scale;
  float deadzone;
  size_t tag_count;
  uint64_t deterministic_stamp;
} AsterAuthoringInputBindingInfo;

typedef struct AsterAuthoringActionContext {
  size_t size;
  uint32_t version;
  AsterStringView actor;
  AsterStringView target;
  AsterStringView input;
} AsterAuthoringActionContext;

typedef struct AsterAuthoringActionExecutionInfo {
  size_t size;
  uint32_t version;
  uint32_t valid;
  size_t diagnostic_count;
  size_t event_count;
  uint64_t contract_stamp;
} AsterAuthoringActionExecutionInfo;

typedef struct AsterAuthoringActionEventInfo {
  size_t size;
  uint32_t version;
  AsterStringView node_id;
  AsterStringView type;
  AsterStringView actor;
  AsterStringView target;
  size_t parameter_count;
  size_t tag_count;
  uint64_t deterministic_stamp;
} AsterAuthoringActionEventInfo;

typedef struct AsterShaderModuleSource {
  AsterStringView name;
  AsterStringView source;
} AsterShaderModuleSource;

typedef struct AsterShaderCompileDesc {
  size_t size;
  uint32_t version;
  AsterKernelShaderBackend backend;
  AsterSpan modules;
  AsterStringView entry_point;
  AsterStringView variant_tag;
  uint64_t feature_mask;
} AsterShaderCompileDesc;

typedef struct AsterShaderCompileResult {
  size_t size;
  uint32_t version;
  uint32_t success;
  size_t source_size;
  size_t diagnostic_count;
  size_t reflection_binding_count;
} AsterShaderCompileResult;

typedef struct AsterShaderReflectionBinding {
  size_t size;
  uint32_t version;
  AsterStringView name;
  AsterKernelShaderResourceKind kind;
  uint32_t binding;
  uint32_t count;
} AsterShaderReflectionBinding;

typedef struct AsterRenderPipelineDesc {
  size_t size;
  uint32_t version;
  AsterShaderArtifactHandle shader;
  AsterStringView debug_label;
} AsterRenderPipelineDesc;

typedef struct AsterTextureDesc {
  size_t size;
  uint32_t version;
  AsterTextureRole role;
  AsterTextureColorSpace color_space;
  AsterTextureNormalConvention normal_convention;
  AsterKernelBackendFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t mip_count;
  AsterSpan data;
  AsterStringView debug_label;
} AsterTextureDesc;

typedef struct AsterMaterialTextureBinding {
  size_t size;
  uint32_t version;
  AsterTextureRole role;
  AsterTextureHandle texture;
} AsterMaterialTextureBinding;

typedef struct AsterRenderTargetDesc {
  size_t size;
  uint32_t version;
  AsterKernelBackendFormat color_format;
  AsterKernelBackendFormat depth_format;
  uint32_t width;
  uint32_t height;
  uint32_t sample_count;
  AsterStringView debug_label;
} AsterRenderTargetDesc;

typedef struct AsterBufferDesc {
  size_t size;
  uint32_t version;
  uint64_t byte_size;
  uint32_t usage;
  AsterStringView debug_label;
} AsterBufferDesc;

typedef struct AsterDescriptorHeapDesc {
  size_t size;
  uint32_t version;
  uint32_t descriptor_capacity;
  uint32_t shader_visible;
  AsterStringView debug_label;
} AsterDescriptorHeapDesc;

typedef struct AsterDescriptorSetDesc {
  size_t size;
  uint32_t version;
  AsterDescriptorHeapHandle heap;
  uint32_t descriptor_count;
  AsterStringView debug_label;
} AsterDescriptorSetDesc;

typedef struct AsterPipelineCacheDesc {
  size_t size;
  uint32_t version;
  uint64_t seed;
  AsterStringView debug_label;
} AsterPipelineCacheDesc;

typedef struct AsterFrameScheduleCounts {
  size_t size;
  uint32_t version;
  size_t pass_count;
  size_t transition_count;
  size_t descriptor_layout_count;
  size_t pipeline_count;
  size_t transient_allocation_count;
  size_t timeline_count;
  size_t validation_event_count;
} AsterFrameScheduleCounts;

typedef struct AsterFrameSchedulePassInfo {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterKernelRhiQueueKind queue;
  AsterStringView name;
  size_t command_buffer_count;
  uint64_t signal_fence_value;
  uint64_t pipeline_cache_key;
  uint64_t descriptor_layout_hash;
} AsterFrameSchedulePassInfo;

typedef struct AsterFrameScheduleMemoryReport {
  size_t size;
  uint32_t version;
  uint64_t budget_bytes;
  uint64_t resident_bytes;
  uint64_t transient_bytes;
  uint64_t aliased_bytes_saved;
} AsterFrameScheduleMemoryReport;

typedef struct AsterFrameScheduleDescriptorInfo {
  size_t size;
  uint32_t version;
  AsterStringView label;
  uint64_t layout_hash;
  size_t range_count;
} AsterFrameScheduleDescriptorInfo;

typedef struct AsterFrameSchedulePipelineInfo {
  size_t size;
  uint32_t version;
  AsterStringView label;
  uint64_t cache_key;
  uint64_t descriptor_layout_hash;
} AsterFrameSchedulePipelineInfo;

typedef struct AsterFrameScheduleTransientAllocationInfo {
  size_t size;
  uint32_t version;
  AsterStringView label;
  size_t physical_allocation_id;
  size_t first_pass;
  size_t last_pass;
  uint64_t byte_size;
  size_t resource_count;
} AsterFrameScheduleTransientAllocationInfo;

typedef struct AsterFrameScheduleTimelineInfo {
  size_t size;
  uint32_t version;
  AsterStringView label;
  AsterKernelRhiQueueKind queue;
  uint64_t submitted_value;
  uint64_t completed_value;
} AsterFrameScheduleTimelineInfo;

typedef struct AsterFrameGraphDesc {
  size_t size;
  uint32_t version;
  uint32_t pass_count;
  uint32_t resource_count;
} AsterFrameGraphDesc;

typedef struct AsterVertex {
  AsterVec3 position;
  AsterVec3 normal;
  AsterVec2 uv;
  AsterVec4 tangent;
  float ambient_occlusion;
} AsterVertex;

typedef struct AsterMeshDesc {
  size_t size;
  uint32_t version;
  AsterKernelMeshPrimitive primitive;
  AsterSpan vertices;
  AsterSpan indices;
  AsterStringView debug_label;
} AsterMeshDesc;

typedef struct AsterMaterialDesc {
  size_t size;
  uint32_t version;
  AsterVec3 base_color;
  AsterVec3 emission_color;
  float roughness;
  float metallic;
  float emission_strength;
  float opacity;
  AsterKernelMaterialAlphaMode alpha_mode;
  uint32_t double_sided;
  AsterStringView debug_label;
  AsterSpan texture_bindings;
} AsterMaterialDesc;

typedef struct AsterSceneObjectDesc {
  size_t size;
  uint32_t version;
  AsterMeshHandle mesh;
  AsterMaterialHandle material;
  AsterRenderPipelineHandle pipeline;
  AsterKernelMeshPrimitive primitive;
  AsterVec3 position;
  AsterVec3 rotation;
  AsterVec3 scale;
  AsterStringView debug_label;
} AsterSceneObjectDesc;

typedef struct AsterCameraDesc {
  size_t size;
  uint32_t version;
  AsterVec3 target;
  float yaw_radians;
  float pitch_radians;
  float radius;
  float vertical_fov_radians;
  float near_plane;
  float far_plane;
  float focal_length_mm;
  float sensor_width_mm;
  float composition_weight;
  float scale_reference_m;
  uint32_t camera_flags;
} AsterCameraDesc;

typedef struct AsterRendererSettings {
  size_t size;
  uint32_t version;
  AsterVec3 clear_color;
  float exposure;
  float ambient_strength;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint32_t flags;
  AsterRenderTargetHandle render_target;
  uint32_t quality_tier;
  uint32_t tone_mapper;
  float ambient_floor;
  uint32_t shadow_cascades;
  uint32_t shadow_atlas_size;
  float shadow_max_distance;
  float shadow_receiver_bias;
  float shadow_normal_bias;
  float shadow_softness;
  float occlusion_radius;
  float occlusion_thickness;
  float occlusion_strength;
  uint32_t occlusion_sample_count;
  float occlusion_contact_hardening;
  float contact_shadow_strength;
  float contact_shadow_radius_scale;
  float contact_shadow_receiver_height;
  float contact_shadow_receiver_bias;
  float physical_texel_density;
  float macro_frequency_breakup;
  float micro_frequency_breakup;
  float height_normal_coupling;
  float roughness_height_coupling;
  float fog_start;
  float fog_end;
  float fog_strength;
  float reflection_intensity;
  float bloom_threshold;
  float bloom_intensity;
  uint64_t world_trace_hash;
  uint64_t simulation_tick;
  uint64_t extraction_hash;
  uint64_t asset_lineage_hash;
  uint64_t world_transition_hash;
  uint64_t actor_state_delta_hash;
  uint64_t encounter_budget_hash;
  uint32_t navigation_valid;
  uint64_t streaming_region_id;
  float perceptual_salience_score;
  uint64_t sensory_event_hash;
  uint64_t visibility_set_hash;
  AsterPerceptualContinuityBudget perceptual_continuity_budget;
} AsterRendererSettings;

typedef struct AsterSystemEntityHandle {
  uint64_t id;
  uint32_t generation;
} AsterSystemEntityHandle;

typedef struct AsterActor {
  size_t size;
  uint32_t version;
  AsterSystemEntityHandle entity;
  AsterStringView label;
  AsterVec3 world_position;
  uint64_t state_hash;
} AsterActor;

typedef struct AsterStimulus {
  size_t size;
  uint32_t version;
  uint64_t stimulus_id;
  AsterStringView kind;
  AsterVec3 world_position;
  float intensity;
  uint64_t source_actor_id;
} AsterStimulus;

typedef struct AsterAffordance {
  size_t size;
  uint32_t version;
  uint64_t affordance_id;
  AsterStringView kind;
  AsterVec3 world_position;
  uint32_t reachable;
  float salience;
  uint64_t state_hash;
} AsterAffordance;

typedef struct AsterEncounter {
  size_t size;
  uint32_t version;
  uint64_t encounter_id;
  uint64_t region_id;
  float pressure;
  float budget;
  uint64_t state_hash;
} AsterEncounter;

typedef struct AsterBiomeCell {
  size_t size;
  uint32_t version;
  uint64_t cell_id;
  uint64_t region_id;
  AsterVec3 center;
  float ecology_pressure;
  uint64_t state_hash;
} AsterBiomeCell;

typedef struct AsterResourceNode {
  size_t size;
  uint32_t version;
  uint64_t resource_id;
  uint64_t region_id;
  AsterVec3 world_position;
  uint32_t available;
  float scarcity;
  uint64_t state_hash;
} AsterResourceNode;

typedef struct AsterNavValidityReport {
  size_t size;
  uint32_t version;
  uint32_t valid;
  size_t checked_steps;
  size_t blocked_steps;
  uint64_t report_hash;
  AsterStringView diagnostic;
} AsterNavValidityReport;

typedef struct AsterPerceptualBudget {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  float salience_score;
  float minimum_salience;
  uint64_t report_hash;
  AsterStringView diagnostic;
} AsterPerceptualBudget;

typedef struct AsterWorldDesc {
  size_t size;
  uint32_t version;
  double fixed_step_seconds;
  uint64_t seed;
  AsterStringView debug_label;
} AsterWorldDesc;

typedef struct AsterWorldAdvanceDesc {
  size_t size;
  uint32_t version;
  uint64_t epoch;
  double delta_seconds;
  uint64_t input_event_hash;
  uint64_t player_intent_hash;
  uint64_t actor_state_delta_hash;
  uint64_t sensory_event_hash;
  uint64_t visibility_set_hash;
  uint64_t asset_lineage_hash;
  uint64_t streaming_region_id;
  AsterPerceptualContinuityBudget perceptual_continuity_budget;
} AsterWorldAdvanceDesc;

typedef struct AsterWorldAdvanceResult {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  uint64_t epoch;
  double time_seconds;
  uint64_t world_hash;
  uint64_t trace_hash;
  uint64_t world_transition_hash;
  AsterStringView diagnostic;
} AsterWorldAdvanceResult;

typedef struct AsterWorldRegionGateReport {
  size_t size;
  uint32_t version;
  uint64_t region_id;
  uint64_t probe_trace_hash;
  AsterWorldRegionGateVerdict verdict;
  AsterNavValidityReport navigation;
  uint64_t encounter_budget_hash;
  uint64_t resource_probe_hash;
  AsterPerceptualBudget perceptual_budget;
  AsterStringView diagnostic;
  AsterPerceptualContinuityBudget perceptual_continuity_budget;
} AsterWorldRegionGateReport;

typedef struct AsterWorldRenderExtractionDesc {
  size_t size;
  uint32_t version;
  uint64_t visibility_set_hash;
  uint64_t extraction_hash;
  uint64_t frame_submission_hash;
} AsterWorldRenderExtractionDesc;

typedef struct AsterWorldRenderExtraction {
  size_t size;
  uint32_t version;
  uint64_t world_transition_hash;
  uint64_t epoch;
  uint64_t trace_hash;
  uint64_t visibility_set_hash;
  uint64_t extraction_hash;
  uint64_t frame_submission_hash;
  uint64_t streaming_region_id;
} AsterWorldRenderExtraction;

typedef struct AsterWorldForensics {
  size_t size;
  uint32_t version;
  uint64_t world_transition_hash;
  uint64_t epoch;
  uint64_t world_hash;
  uint64_t trace_hash;
  uint64_t actor_state_delta_hash;
  size_t actor_delta_count;
  uint64_t render_extraction_hash;
  uint64_t streaming_region_id;
  AsterWorldRegionGateVerdict generated_region_gate;
  AsterNavValidityReport navigation;
  uint64_t encounter_budget_hash;
  uint64_t resource_probe_hash;
  AsterPerceptualBudget perceptual_budget;
  AsterStringView diagnostic;
  uint64_t sensory_event_hash;
  uint64_t visibility_set_hash;
  AsterPerceptualContinuityBudget perceptual_continuity_budget;
} AsterWorldForensics;

typedef struct AsterSystemWorldDesc {
  size_t size;
  uint32_t version;
  double fixed_step_seconds;
  uint64_t seed;
  AsterStringView debug_label;
} AsterSystemWorldDesc;

typedef struct AsterSystemTickDesc {
  size_t size;
  uint32_t version;
  uint64_t tick;
  double delta_seconds;
  uint64_t input_event_hash;
  uint64_t asset_lineage_hash;
  uint64_t extraction_hash;
  uint64_t frame_submission_hash;
} AsterSystemTickDesc;

typedef struct AsterSystemTickResult {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  uint64_t tick;
  double time_seconds;
  size_t events_emitted;
  uint64_t world_hash;
  uint64_t trace_hash;
  AsterStringView diagnostic;
} AsterSystemTickResult;

typedef struct AsterSystemEntityInfo {
  size_t size;
  uint32_t version;
  AsterSystemEntityHandle handle;
  uint32_t alive;
  AsterStringView label;
} AsterSystemEntityInfo;

typedef struct AsterSystemComponentAccess {
  size_t size;
  uint32_t version;
  AsterStringView component;
  AsterStringView subject;
  AsterSystemComponentAccessMode mode;
} AsterSystemComponentAccess;

typedef struct AsterSystemTransactionDesc {
  size_t size;
  uint32_t version;
  AsterStringView label;
  AsterStringView provenance;
  AsterSpan accesses;
} AsterSystemTransactionDesc;

typedef struct AsterSystemTransactionInfo {
  size_t size;
  uint32_t version;
  uint64_t transaction_id;
  uint32_t committed;
  size_t access_count;
  uint64_t parent_world_hash;
  uint64_t post_world_hash;
  uint64_t deterministic_stamp;
  AsterStringView diagnostic;
} AsterSystemTransactionInfo;

typedef struct AsterSystemTraceCounts {
  size_t size;
  uint32_t version;
  size_t event_count;
  size_t entity_count;
  size_t live_entity_count;
  size_t transaction_count;
  size_t validation_event_count;
  uint64_t tick;
  double time_seconds;
  uint64_t world_hash;
  uint64_t trace_hash;
} AsterSystemTraceCounts;

typedef struct AsterSystemTraceEvent {
  size_t size;
  uint32_t version;
  AsterSystemTraceEventKind kind;
  uint64_t sequence;
  uint64_t tick;
  uint64_t transaction_id;
  AsterSystemEntityHandle entity;
  AsterStringView label;
  AsterStringView subject;
  AsterStringView component;
  AsterStringView detail;
  uint64_t parent_world_hash;
  uint64_t world_hash;
  uint64_t trace_hash;
} AsterSystemTraceEvent;

typedef struct AsterAssetLineageInfo {
  size_t size;
  uint32_t version;
  AsterStringView asset_id;
  AsterStringView source_hash;
  AsterStringView options_hash;
  AsterStringView dependency_hash;
  AsterStringView artifact_hash;
  AsterStringView material_hash;
  AsterStringView artifact_manifest_hash;
  uint32_t referentially_transparent;
} AsterAssetLineageInfo;

typedef struct AsterResidencyBudget {
  size_t size;
  uint32_t version;
  uint64_t byte_budget;
  AsterResidencyEvictionPolicy eviction_policy;
} AsterResidencyBudget;

typedef struct AsterResidencyDecision {
  size_t size;
  uint32_t version;
  AsterStringView asset_id;
  AsterResidencyDecisionKind decision;
  uint64_t byte_cost;
  float priority;
  uint32_t visible;
  AsterStringView reason;
} AsterResidencyDecision;

typedef struct AsterWorldSnapshotDesc {
  size_t size;
  uint32_t version;
  AsterStringView path;
  uint64_t expected_world_hash;
} AsterWorldSnapshotDesc;

typedef struct AsterWorldMigrationReport {
  size_t size;
  uint32_t version;
  uint32_t loaded_schema_version;
  uint32_t current_schema_version;
  uint32_t migration_applied;
  size_t entity_count;
  uint64_t world_hash;
  uint64_t trace_hash;
  AsterStringView diagnostic;
} AsterWorldMigrationReport;

typedef struct AsterWorldReplayReport {
  size_t size;
  uint32_t version;
  uint32_t matched;
  size_t events_replayed;
  uint64_t expected_world_hash;
  uint64_t actual_world_hash;
  uint64_t actual_trace_hash;
  AsterStringView diagnostic;
} AsterWorldReplayReport;

typedef struct AsterFrameStats {
  size_t size;
  uint32_t version;
  double frame_seconds;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  size_t draw_calls;
  size_t visible_objects;
  size_t culled_objects;
  size_t graph_passes;
  size_t graph_resources;
  size_t graph_barriers;
  size_t graph_transient_resources;
  uint32_t backend_feature_mask;
  AsterKernelBackendKind backend;
  double rust_plan_seconds;
  double graph_compile_seconds;
  double render_encode_seconds;
} AsterFrameStats;

typedef struct AsterFrameForensicsCounts {
  size_t size;
  uint32_t version;
  size_t pass_count;
  size_t event_count;
} AsterFrameForensicsCounts;

typedef struct AsterFrameForensicsDetailCounts {
  size_t size;
  uint32_t version;
  size_t pass_count;
  size_t event_count;
  size_t debug_capture_count;
  size_t pass_artifact_count;
  size_t resource_transition_count;
  size_t rhi_validation_event_count;
  size_t timestamp_sample_count;
  size_t backend_feature_proof_count;
  uint32_t certification_valid;
  size_t certification_missing_proof_count;
  size_t certification_validation_error_count;
  size_t object_fate_count;
  uint64_t world_trace_hash;
  uint64_t simulation_tick;
  uint64_t extraction_hash;
  uint64_t asset_lineage_hash;
  AsterWorldExtractionProvenance world_extraction_provenance;
  uint64_t world_transition_hash;
  uint64_t actor_state_delta_hash;
  uint64_t encounter_budget_hash;
  uint32_t navigation_valid;
  uint64_t streaming_region_id;
  float perceptual_salience_score;
  uint64_t sensory_event_hash;
  uint64_t visibility_set_hash;
  AsterPerceptualContinuityBudget perceptual_continuity_budget;
} AsterFrameForensicsDetailCounts;

typedef struct AsterFramePassStats {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterStringView name;
  size_t draw_calls;
  size_t pipeline_switches;
  size_t material_permutations;
  double encode_seconds;
  double cpu_build_seconds;
  double gpu_execution_seconds;
  uint64_t estimated_bandwidth_bytes;
  uint32_t render_target_width;
  uint32_t render_target_height;
  size_t descriptor_heap_pressure;
  size_t pipeline_cache_hits;
  size_t pipeline_cache_misses;
} AsterFramePassStats;

typedef struct AsterFrameDiagnosticEvent {
  size_t size;
  uint32_t version;
  AsterKernelFrameDiagnosticKind kind;
  AsterKernelFrameDiagnosticSeverity severity;
  AsterStringView pass;
  AsterStringView label;
  AsterStringView message;
  uint64_t value;
} AsterFrameDiagnosticEvent;

typedef struct AsterFrameDebugCaptureInfo {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterKernelRenderGraphResource resource;
  uint32_t view;
  AsterStringView label;
  uint32_t width;
  uint32_t height;
  uint32_t row_stride_bytes;
  uint64_t content_hash;
  uint32_t available;
  size_t payload_size;
} AsterFrameDebugCaptureInfo;

typedef struct AsterFramePassArtifactInfo {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterKernelRenderGraphResource resource;
  AsterStringView label;
  AsterStringView kind;
  uint32_t width;
  uint32_t height;
  uint64_t content_hash;
  uint32_t available;
} AsterFramePassArtifactInfo;

typedef struct AsterFrameResourceTransition {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterKernelRenderGraphResource resource;
  AsterStringView pass_name;
  AsterStringView resource_name;
  AsterKernelRhiResourceState before;
  AsterKernelRhiResourceState after;
  AsterKernelRhiQueueKind queue;
  uint32_t write;
} AsterFrameResourceTransition;

typedef struct AsterRhiValidationEvent {
  size_t size;
  uint32_t version;
  AsterKernelRhiValidationKind kind;
  AsterKernelFrameDiagnosticSeverity severity;
  AsterStringView pass;
  AsterStringView resource;
  AsterStringView message;
  size_t pass_index;
  uint64_t resource_id;
} AsterRhiValidationEvent;

typedef struct AsterFrameTimestampSample {
  size_t size;
  uint32_t version;
  uint32_t slot;
  uint64_t ticks;
  double nanoseconds;
  uint32_t available;
} AsterFrameTimestampSample;

typedef struct AsterBackendFeatureProof {
  size_t size;
  uint32_t version;
  AsterKernelBackendFeatureProofKind kind;
  AsterKernelBackendFeatureProofStatus status;
  AsterKernelRenderGraphPass pass;
  AsterKernelRenderGraphResource resource;
  AsterStringView feature;
  AsterStringView label;
  AsterStringView message;
  uint32_t advertised;
  uint32_t native;
  uint64_t evidence_hash;
} AsterBackendFeatureProof;

typedef struct AsterObjectRenderFate {
  size_t size;
  uint32_t version;
  size_t object_index;
  uint32_t visible;
  AsterStringView object_name;
  AsterStringView mesh_key;
  AsterStringView material_key;
  AsterStringView material_asset_id;
  AsterStringView shader_variant_key;
  AsterStringView pipeline_tag;
  AsterStringView source_graph_guid;
  AsterStringView source_graph_node;
  AsterStringView pipeline_cache_key;
  AsterStringView procedural_capability_status;
  AsterStringView texture_roles;
  AsterStringView pass_list;
  AsterStringView resource_transitions;
  AsterStringView capture_labels;
  AsterStringView feature_proofs;
  AsterStringView final_contribution;
  uint64_t contribution_hash;
} AsterObjectRenderFate;

typedef struct AsterCaptureDesc {
  size_t size;
  uint32_t version;
  AsterStringView path;
  uint32_t width;
  uint32_t height;
} AsterCaptureDesc;

typedef enum AsterFrameVisionProbeArtifactFlags {
  ASTER_FRAME_VISION_PROBE_ARTIFACT_DEFAULT = 0,
  ASTER_FRAME_VISION_PROBE_ARTIFACT_PNG = 1u << 0u,
  ASTER_FRAME_VISION_PROBE_ARTIFACT_JSON = 1u << 1u
} AsterFrameVisionProbeArtifactFlags;

typedef struct AsterFrameVisionProbeDesc {
  size_t size;
  uint32_t version;
  AsterStringView output_dir;
  AsterStringView label;
  uint32_t width;
  uint32_t height;
  uint32_t artifact_flags;
  float visible_void_luminance_threshold;
  float max_visible_void_fraction;
  uint32_t max_zfight_candidate_pixels;
  uint32_t max_support_mismatch_count;
  uint32_t max_traversal_blocked_count;
  float max_support_render_delta_m;
} AsterFrameVisionProbeDesc;

typedef struct AsterFrameVisionProbeResult {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  uint32_t width;
  uint32_t height;
  uint64_t pixel_count;
  uint64_t visible_void_count;
  uint64_t zfight_candidate_count;
  uint64_t support_mismatch_count;
  uint64_t traversal_blocked_count;
  float visible_void_fraction;
  float max_support_render_delta_m;
  AsterStringView png_path;
  AsterStringView json_path;
  AsterKernelFrameDiagnosticKind diagnostic_kind;
} AsterFrameVisionProbeResult;

typedef enum AsterFrameLightingProbeArtifactFlags {
  ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_DEFAULT = 0,
  ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_PNG = 1u << 0u,
  ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_JSON = 1u << 1u,
  ASTER_FRAME_LIGHTING_PROBE_ARTIFACT_HEATMAP_PNG = 1u << 2u
} AsterFrameLightingProbeArtifactFlags;

typedef struct AsterFrameLightingProbeDesc {
  size_t size;
  uint32_t version;
  AsterStringView output_dir;
  AsterStringView label;
  uint32_t width;
  uint32_t height;
  uint32_t artifact_flags;
  float min_source_mean_luminance;
  float min_air_scatter_mean_luminance;
  uint32_t min_air_scatter_pixels;
  float min_source_to_air_ratio;
  float max_source_to_air_ratio;
  float min_falloff_continuity_score;
  float max_temporal_lighting_delta;
  float source_luminance_threshold;
  float air_scatter_luminance_threshold;
  float max_overexposed_pixel_fraction;
  float overexposed_luminance_threshold;
  float min_frame_mean_luminance;
  float max_frame_mean_luminance;
} AsterFrameLightingProbeDesc;

typedef struct AsterFrameLightingProbeResult {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  uint32_t width;
  uint32_t height;
  uint64_t pixel_count;
  uint64_t source_visible_pixels;
  uint64_t air_scatter_pixels;
  uint64_t light_source_unreadable_count;
  uint64_t volumetric_light_missing_count;
  uint64_t light_falloff_discontinuity_count;
  uint64_t cave_light_exposure_underflow_count;
  float source_mean_luminance;
  float air_scatter_mean_luminance;
  float surface_direct_mean_luminance;
  float source_to_air_ratio;
  float falloff_continuity_score;
  float temporal_lighting_delta;
  AsterStringView png_path;
  AsterStringView json_path;
  AsterStringView heatmap_png_path;
  AsterKernelFrameDiagnosticKind diagnostic_kind;
  uint64_t overexposed_pixels;
  uint64_t cave_light_exposure_overflow_count;
  uint64_t cave_light_exposure_overbright_count;
  float frame_mean_luminance;
} AsterFrameLightingProbeResult;

ASTER_KERNEL_API AsterAbiVersion aster_kernel_abi_version(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_ok(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_from_code(AsterStatusCode code);

ASTER_KERNEL_API AsterMathPolicy aster_kernel_math_default_policy(void);
ASTER_KERNEL_API AsterStatus aster_kernel_math_vec3_dot(AsterVec3 lhs, AsterVec3 rhs,
                                                        float *out_value);
ASTER_KERNEL_API AsterStatus aster_kernel_math_vec3_cross(AsterVec3 lhs, AsterVec3 rhs,
                                                          AsterVec3 *out_value);
ASTER_KERNEL_API AsterStatus aster_kernel_math_vec3_length(AsterVec3 value, float *out_value);
ASTER_KERNEL_API AsterStatus
aster_kernel_math_vec3_normalize(AsterVec3 value, const AsterMathPolicy *policy,
                                 AsterVec3 *out_value, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_identity(AsterMat4 *out_matrix);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_multiply(const AsterMat4 *lhs,
                                                             const AsterMat4 *rhs,
                                                             AsterMat4 *out_matrix);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_inverse(
    const AsterMat4 *matrix, const AsterMathPolicy *policy, AsterMat4 *out_matrix,
    AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_compose_trs(const AsterTransform *transform,
                                                                AsterMat4 *out_matrix);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_decompose_trs(
    const AsterMat4 *matrix, AsterTransform *out_transform, AsterVec3 *out_skew,
    AsterVec4 *out_perspective, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_perspective(
    float vertical_fov_radians, float aspect_ratio, float near_plane, float far_plane,
    AsterMathCoordinateHandedness handedness, AsterMathClipDepthRange depth_range,
    AsterMathDepthDirection depth_direction, AsterMat4 *out_matrix,
    AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_orthographic(
    float left, float right, float bottom, float top, float near_plane, float far_plane,
    AsterMathCoordinateHandedness handedness, AsterMathClipDepthRange depth_range,
    AsterMathDepthDirection depth_direction, AsterMat4 *out_matrix,
    AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_mat4_look_at(
    AsterVec3 eye, AsterVec3 target, AsterVec3 up, AsterMathCoordinateHandedness handedness,
    AsterMat4 *out_matrix, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_world_to_screen(
    AsterWorldPoint point, const AsterMat4 *world_to_clip, const AsterViewport *viewport,
    AsterScreenPoint *out_screen, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_screen_to_world(
    AsterScreenPoint screen, const AsterMat4 *clip_to_world, const AsterViewport *viewport,
    AsterWorldPoint *out_point, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_screen_to_world_ray(
    AsterScreenPoint screen, const AsterMat4 *clip_to_world, const AsterViewport *viewport,
    const AsterProjectionConvention *convention, AsterWorldPoint perspective_eye,
    AsterWorldRay *out_ray, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_quat_identity(AsterQuat *out_quat);
ASTER_KERNEL_API AsterStatus aster_kernel_math_quat_axis_angle(
    AsterVec3 axis, float radians, AsterQuat *out_quat, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_quat_slerp(AsterQuat lhs, AsterQuat rhs, float t,
                                                          AsterQuat *out_quat);
ASTER_KERNEL_API AsterStatus aster_kernel_math_quat_rotate_vec3(AsterQuat rotation,
                                                                AsterVec3 value,
                                                                AsterVec3 *out_value);
ASTER_KERNEL_API AsterStatus aster_kernel_math_quat_inverse(
    AsterQuat value, AsterQuat *out_quat, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_intersect_ray_plane(
    AsterRay3 ray, AsterPlane3 plane, float *out_distance, AsterVec3 *out_point,
    AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_intersect_ray_triangle(
    AsterRay3 ray, AsterVec3 a, AsterVec3 b, AsterVec3 c, float *out_distance,
    AsterVec3 *out_barycentric, AsterMathDiagnostics *out_diagnostics);
ASTER_KERNEL_API AsterStatus aster_kernel_math_intersect_ray_sphere(
    AsterRay3 ray, AsterSphere3 sphere, float *out_distance, AsterVec3 *out_point,
    AsterVec3 *out_normal, AsterMathDiagnostics *out_diagnostics);

ASTER_KERNEL_API AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                                        AsterEngineHandle *out_engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_destroy(AsterEngineHandle engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_last_status(AsterEngineHandle engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_validation_event_count(
    AsterEngineHandle engine, size_t *out_count);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_validation_event(
    AsterEngineHandle engine, size_t index, AsterValidationEvent *out_event);

ASTER_KERNEL_API AsterStatus aster_kernel_world_create(
    AsterEngineHandle engine, const AsterWorldDesc *desc, AsterWorldHandle *out_world);
ASTER_KERNEL_API AsterStatus aster_kernel_world_advance(
    AsterWorldHandle world, const AsterWorldAdvanceDesc *desc,
    AsterWorldAdvanceResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_world_record_region_gate(
    AsterWorldHandle world, const AsterWorldRegionGateReport *report);
ASTER_KERNEL_API AsterStatus aster_kernel_world_extract_render(
    AsterWorldHandle world, const AsterWorldRenderExtractionDesc *desc,
    AsterWorldRenderExtraction *out_extraction);
ASTER_KERNEL_API AsterStatus aster_kernel_world_forensics(
    AsterWorldHandle world, AsterWorldForensics *out_forensics);
ASTER_KERNEL_API AsterStatus aster_kernel_world_destroy(AsterWorldHandle world);

ASTER_KERNEL_API AsterStatus aster_kernel_system_world_create(
    AsterEngineHandle engine, const AsterSystemWorldDesc *desc,
    AsterSystemWorldHandle *out_world);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_tick(
    AsterSystemWorldHandle world, const AsterSystemTickDesc *desc,
    AsterSystemTickResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_entity_create(
    AsterSystemWorldHandle world, AsterStringView label, AsterSystemEntityHandle *out_entity);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_entity_query(
    AsterSystemWorldHandle world, AsterSystemEntityHandle entity, AsterSystemEntityInfo *out_info);
ASTER_KERNEL_API AsterStatus
aster_kernel_system_world_entity_destroy(AsterSystemWorldHandle world,
                                         AsterSystemEntityHandle entity);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_transaction_begin(
    AsterSystemWorldHandle world, const AsterSystemTransactionDesc *desc,
    AsterSystemTransactionInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_transaction_append(
    AsterSystemWorldHandle world, uint64_t transaction_id,
    const AsterSystemComponentAccess *access);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_transaction_commit(
    AsterSystemWorldHandle world, uint64_t transaction_id, AsterSystemTransactionInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_transaction_abort(
    AsterSystemWorldHandle world, uint64_t transaction_id, AsterStringView reason,
    AsterSystemTransactionInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_trace_counts(
    AsterSystemWorldHandle world, AsterSystemTraceCounts *out_counts);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_trace_event(
    AsterSystemWorldHandle world, size_t index, AsterSystemTraceEvent *out_event);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_save_snapshot(
    AsterSystemWorldHandle world, const AsterWorldSnapshotDesc *desc);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_load_snapshot(
    AsterSystemWorldHandle world, const AsterWorldSnapshotDesc *desc,
    AsterWorldMigrationReport *out_report);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_replay_trace(
    AsterSystemWorldHandle world, const AsterWorldSnapshotDesc *desc,
    AsterWorldReplayReport *out_report);

ASTER_KERNEL_API AsterStatus aster_kernel_window_create(const AsterWindowDesc *desc,
                                                        AsterWindowHandle *out_window);
ASTER_KERNEL_API AsterStatus aster_kernel_window_poll(AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_window_swap(AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_window_set_vsync(AsterWindowHandle window,
                                                           uint32_t enabled);
ASTER_KERNEL_API AsterStatus aster_kernel_window_framebuffer_size(AsterWindowHandle window,
                                                                  AsterExtent2D *out_size);
ASTER_KERNEL_API AsterStatus aster_kernel_window_destroy(AsterWindowHandle window);

ASTER_KERNEL_API AsterStatus aster_kernel_scene_create(AsterEngineHandle engine,
                                                       AsterSceneHandle *out_scene);
ASTER_KERNEL_API AsterStatus aster_kernel_scene_clear(AsterSceneHandle scene);
ASTER_KERNEL_API AsterStatus aster_kernel_scene_add_object(AsterSceneHandle scene,
                                                           const AsterSceneObjectDesc *desc);
ASTER_KERNEL_API AsterStatus aster_kernel_scene_destroy(AsterSceneHandle scene);

ASTER_KERNEL_API AsterStatus aster_kernel_renderer_create(AsterEngineHandle engine,
                                                          const AsterRendererDesc *desc,
                                                          AsterRendererHandle *out_renderer);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_get_capabilities(AsterRendererHandle renderer,
                                        AsterBackendCapabilities *out_capabilities);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_get_backend_capability_table(
    AsterRendererHandle renderer, AsterBackendCapabilityTable *out_capabilities);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_render_frame(
    AsterRendererHandle renderer, AsterSceneHandle scene, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_render_frame_to_target(
    AsterRendererHandle renderer, AsterSceneHandle scene, AsterRenderTargetHandle target,
    const AsterCameraDesc *camera, const AsterRendererSettings *settings);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_bind_window(AsterRendererHandle renderer,
                                                               AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_present(AsterRendererHandle renderer,
                                                           AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_present_frame(
    AsterRendererHandle renderer, AsterWindowHandle window, const AsterPresentDesc *desc,
    AsterPresentResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_presentation_status(
    AsterRendererHandle renderer, AsterRendererPresentationStatus *out_status);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_capture(AsterRendererHandle renderer,
                                                           const AsterCaptureDesc *desc);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_capture_render_target(
    AsterRendererHandle renderer, AsterRenderTargetHandle target, const AsterCaptureDesc *desc);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_frame_vision_probe(
    AsterRendererHandle renderer, AsterSceneHandle scene, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings, const AsterFrameVisionProbeDesc *desc,
    AsterFrameVisionProbeResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_frame_lighting_probe(
    AsterRendererHandle renderer, AsterSceneHandle scene, const AsterCameraDesc *camera,
    const AsterRendererSettings *settings, const AsterFrameLightingProbeDesc *desc,
    AsterFrameLightingProbeResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_last_stats(AsterRendererHandle renderer,
                                                              AsterFrameStats *out_stats);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_validation_event_count(
    AsterRendererHandle renderer, size_t *out_count);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_validation_event(
    AsterRendererHandle renderer, size_t index, AsterValidationEvent *out_event);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_frame_forensics_counts(
    AsterRendererHandle renderer, AsterFrameForensicsCounts *out_counts);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_frame_forensics_detail_counts(
    AsterRendererHandle renderer, AsterFrameForensicsDetailCounts *out_counts);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_frame_pass_stats(AsterRendererHandle renderer, size_t index,
                                       AsterFramePassStats *out_stats);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_frame_diagnostic(AsterRendererHandle renderer, size_t index,
                                       AsterFrameDiagnosticEvent *out_event);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_debug_capture_info(AsterRendererHandle renderer, size_t index,
                                         AsterFrameDebugCaptureInfo *out_capture);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_pass_artifact_info(AsterRendererHandle renderer, size_t index,
                                         AsterFramePassArtifactInfo *out_artifact);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_resource_transition(AsterRendererHandle renderer, size_t index,
                                          AsterFrameResourceTransition *out_transition);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_object_render_fate(AsterRendererHandle renderer, size_t index,
                                         AsterObjectRenderFate *out_fate);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_rhi_validation_event(AsterRendererHandle renderer, size_t index,
                                           AsterRhiValidationEvent *out_event);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_timestamp_sample(AsterRendererHandle renderer, size_t index,
                                       AsterFrameTimestampSample *out_sample);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_backend_feature_proof(AsterRendererHandle renderer, size_t index,
                                            AsterBackendFeatureProof *out_proof);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_get_last_frame_schedule(
    AsterRendererHandle renderer, AsterFrameScheduleHandle *out_schedule);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_destroy(AsterRendererHandle renderer);

ASTER_KERNEL_API AsterStatus aster_kernel_mesh_create(AsterEngineHandle engine,
                                                      const AsterMeshDesc *desc,
                                                      AsterMeshHandle *out_mesh);
ASTER_KERNEL_API AsterStatus aster_kernel_mesh_destroy(AsterMeshHandle mesh);

ASTER_KERNEL_API AsterStatus aster_kernel_material_create(AsterEngineHandle engine,
                                                          const AsterMaterialDesc *desc,
                                                          AsterMaterialHandle *out_material);
ASTER_KERNEL_API AsterStatus aster_kernel_material_destroy(AsterMaterialHandle material);

ASTER_KERNEL_API AsterStatus aster_kernel_texture_create(AsterEngineHandle engine,
                                                         const AsterTextureDesc *desc,
                                                         AsterTextureHandle *out_texture);
ASTER_KERNEL_API AsterStatus aster_kernel_texture_destroy(AsterTextureHandle texture);

ASTER_KERNEL_API AsterStatus
aster_kernel_render_target_create(AsterEngineHandle engine, const AsterRenderTargetDesc *desc,
                                  AsterRenderTargetHandle *out_target);
ASTER_KERNEL_API AsterStatus aster_kernel_render_target_destroy(AsterRenderTargetHandle target);

ASTER_KERNEL_API AsterStatus aster_kernel_buffer_create(AsterEngineHandle engine,
                                                        const AsterBufferDesc *desc,
                                                        AsterBufferHandle *out_buffer);
ASTER_KERNEL_API AsterStatus aster_kernel_buffer_destroy(AsterBufferHandle buffer);

ASTER_KERNEL_API AsterStatus aster_kernel_descriptor_heap_create(
    AsterEngineHandle engine, const AsterDescriptorHeapDesc *desc,
    AsterDescriptorHeapHandle *out_heap);
ASTER_KERNEL_API AsterStatus aster_kernel_descriptor_heap_destroy(AsterDescriptorHeapHandle heap);

ASTER_KERNEL_API AsterStatus aster_kernel_descriptor_set_create(
    AsterEngineHandle engine, const AsterDescriptorSetDesc *desc,
    AsterDescriptorSetHandle *out_set);
ASTER_KERNEL_API AsterStatus aster_kernel_descriptor_set_destroy(AsterDescriptorSetHandle set);

ASTER_KERNEL_API AsterStatus aster_kernel_pipeline_cache_create(
    AsterEngineHandle engine, const AsterPipelineCacheDesc *desc,
    AsterPipelineCacheHandle *out_cache);
ASTER_KERNEL_API AsterStatus aster_kernel_pipeline_cache_destroy(AsterPipelineCacheHandle cache);

ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_counts(
    AsterFrameScheduleHandle schedule, AsterFrameScheduleCounts *out_counts);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_pass(
    AsterFrameScheduleHandle schedule, size_t index, AsterFrameSchedulePassInfo *out_pass);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_memory_report(
    AsterFrameScheduleHandle schedule, AsterFrameScheduleMemoryReport *out_report);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_descriptor_layout(
    AsterFrameScheduleHandle schedule, size_t index, AsterFrameScheduleDescriptorInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_pipeline(
    AsterFrameScheduleHandle schedule, size_t index, AsterFrameSchedulePipelineInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_transient_allocation(
    AsterFrameScheduleHandle schedule, size_t index,
    AsterFrameScheduleTransientAllocationInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_timeline(
    AsterFrameScheduleHandle schedule, size_t index, AsterFrameScheduleTimelineInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_validation_event(
    AsterFrameScheduleHandle schedule, size_t index, AsterValidationEvent *out_event);
ASTER_KERNEL_API AsterStatus aster_kernel_frame_schedule_destroy(
    AsterFrameScheduleHandle schedule);

ASTER_KERNEL_API AsterStatus
aster_kernel_shader_compile(AsterEngineHandle engine, const AsterShaderCompileDesc *desc,
                            AsterShaderArtifactHandle *out_shader);
ASTER_KERNEL_API AsterStatus
aster_kernel_shader_get_result(AsterShaderArtifactHandle shader,
                               AsterShaderCompileResult *out_result);
ASTER_KERNEL_API AsterStatus aster_kernel_shader_get_source(AsterShaderArtifactHandle shader,
                                                            AsterStringView *out_source);
ASTER_KERNEL_API AsterStatus aster_kernel_shader_get_diagnostics(
    AsterShaderArtifactHandle shader, size_t index, AsterStringView *out_diagnostic);
ASTER_KERNEL_API AsterStatus aster_kernel_shader_get_reflection(
    AsterShaderArtifactHandle shader, size_t index, AsterShaderReflectionBinding *out_binding);
ASTER_KERNEL_API AsterStatus aster_kernel_shader_destroy(AsterShaderArtifactHandle shader);

ASTER_KERNEL_API AsterStatus
aster_kernel_render_pipeline_create(AsterEngineHandle engine, const AsterRenderPipelineDesc *desc,
                                    AsterRenderPipelineHandle *out_pipeline);
ASTER_KERNEL_API AsterStatus
aster_kernel_render_pipeline_destroy(AsterRenderPipelineHandle pipeline);

ASTER_KERNEL_API AsterStatus aster_kernel_authoring_document_load(
    const AsterAuthoringDocumentDesc *desc, AsterAuthoringDocumentHandle *out_document);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_document_info(
    AsterAuthoringDocumentHandle document, AsterAuthoringDocumentInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_document_diagnostic(
    AsterAuthoringDocumentHandle document, size_t index, AsterAuthoringDiagnosticInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_project_asset(
    AsterAuthoringDocumentHandle document, size_t index, AsterAuthoringProjectAssetInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_entity(
    AsterAuthoringDocumentHandle document, size_t index, AsterAuthoringEntityInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_node(
    AsterAuthoringDocumentHandle document, size_t index, AsterAuthoringActionNodeInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_node_parameter(
    AsterAuthoringDocumentHandle document, size_t node_index, size_t parameter_index,
    AsterAuthoringKeyValue *out_parameter);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_node_tag(
    AsterAuthoringDocumentHandle document, size_t node_index, size_t tag_index,
    AsterStringView *out_tag);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_input_binding(
    AsterAuthoringDocumentHandle document, size_t index, AsterAuthoringInputBindingInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_input_binding_tag(
    AsterAuthoringDocumentHandle document, size_t binding_index, size_t tag_index,
    AsterStringView *out_tag);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_execute(
    AsterAuthoringDocumentHandle document, const AsterAuthoringActionContext *context,
    AsterAuthoringActionExecutionHandle *out_execution);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_execution_info(
    AsterAuthoringActionExecutionHandle execution, AsterAuthoringActionExecutionInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_execution_diagnostic(
    AsterAuthoringActionExecutionHandle execution, size_t index,
    AsterAuthoringDiagnosticInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_event(
    AsterAuthoringActionExecutionHandle execution, size_t index,
    AsterAuthoringActionEventInfo *out_info);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_event_parameter(
    AsterAuthoringActionExecutionHandle execution, size_t event_index, size_t parameter_index,
    AsterAuthoringKeyValue *out_parameter);
ASTER_KERNEL_API AsterStatus aster_kernel_authoring_action_event_tag(
    AsterAuthoringActionExecutionHandle execution, size_t event_index, size_t tag_index,
    AsterStringView *out_tag);
ASTER_KERNEL_API AsterStatus
aster_kernel_authoring_action_execution_destroy(AsterAuthoringActionExecutionHandle execution);
ASTER_KERNEL_API AsterStatus
aster_kernel_authoring_document_destroy(AsterAuthoringDocumentHandle document);

ASTER_KERNEL_API AsterStatus
aster_kernel_physics_world_destroy(AsterPhysicsWorldHandle physics_world);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_destroy(AsterSystemWorldHandle system_world);
ASTER_KERNEL_API AsterStatus aster_kernel_sample_app_destroy(AsterSampleAppHandle sample_app);

#ifdef __cplusplus
}
#endif

#endif
