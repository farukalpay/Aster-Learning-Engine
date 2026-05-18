// Author: Faruk Alpay
// Do not remove this notice.

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

#define ASTER_KERNEL_ABI_MAJOR 2u
#define ASTER_KERNEL_ABI_MINOR 2u
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
  ASTER_STATUS_ABI_MISMATCH = 5
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
typedef struct AsterSystemWorldHandle__ *AsterSystemWorldHandle;
typedef struct AsterSampleAppHandle__ *AsterSampleAppHandle;
typedef struct AsterShaderArtifactHandle__ *AsterShaderArtifactHandle;
typedef struct AsterRenderPipelineHandle__ *AsterRenderPipelineHandle;

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
  ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_OFFSCREEN_READBACK = 3
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

typedef enum AsterKernelRenderGraphPass {
  ASTER_KERNEL_RENDER_PASS_SCENE_COLOR_DEPTH = 0,
  ASTER_KERNEL_RENDER_PASS_OPAQUE = 1,
  ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW = 2,
  ASTER_KERNEL_RENDER_PASS_TRANSPARENT = 3,
  ASTER_KERNEL_RENDER_PASS_UI_COMPOSITE = 4,
  ASTER_KERNEL_RENDER_PASS_CAPTURE = 5
} AsterKernelRenderGraphPass;

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
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CAPABILITY_MISMATCH = 5
} AsterKernelFrameDiagnosticKind;

enum {
  ASTER_KERNEL_WINDOW_FLAG_HEADLESS = 1u << 0u,
  ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE = 1u << 0u,
  ASTER_KERNEL_RENDERER_FLAG_FORCE_NULL = 1u << 1u,
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
} AsterRendererSettings;

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

typedef struct AsterFramePassStats {
  size_t size;
  uint32_t version;
  AsterKernelRenderGraphPass pass;
  AsterStringView name;
  size_t draw_calls;
  size_t pipeline_switches;
  size_t material_permutations;
  double encode_seconds;
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

typedef struct AsterCaptureDesc {
  size_t size;
  uint32_t version;
  AsterStringView path;
  uint32_t width;
  uint32_t height;
} AsterCaptureDesc;

ASTER_KERNEL_API AsterAbiVersion aster_kernel_abi_version(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_ok(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_from_code(AsterStatusCode code);

ASTER_KERNEL_API AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                                        AsterEngineHandle *out_engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_destroy(AsterEngineHandle engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_last_status(AsterEngineHandle engine);

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
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_present(AsterRendererHandle renderer,
                                                           AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_capture(AsterRendererHandle renderer,
                                                           const AsterCaptureDesc *desc);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_last_stats(AsterRendererHandle renderer,
                                                              AsterFrameStats *out_stats);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_frame_forensics_counts(
    AsterRendererHandle renderer, AsterFrameForensicsCounts *out_counts);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_frame_pass_stats(AsterRendererHandle renderer, size_t index,
                                       AsterFramePassStats *out_stats);
ASTER_KERNEL_API AsterStatus
aster_kernel_renderer_frame_diagnostic(AsterRendererHandle renderer, size_t index,
                                       AsterFrameDiagnosticEvent *out_event);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_destroy(AsterRendererHandle renderer);

ASTER_KERNEL_API AsterStatus aster_kernel_mesh_create(AsterEngineHandle engine,
                                                      const AsterMeshDesc *desc,
                                                      AsterMeshHandle *out_mesh);
ASTER_KERNEL_API AsterStatus aster_kernel_mesh_destroy(AsterMeshHandle mesh);

ASTER_KERNEL_API AsterStatus aster_kernel_material_create(AsterEngineHandle engine,
                                                          const AsterMaterialDesc *desc,
                                                          AsterMaterialHandle *out_material);
ASTER_KERNEL_API AsterStatus aster_kernel_material_destroy(AsterMaterialHandle material);

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

ASTER_KERNEL_API AsterStatus
aster_kernel_physics_world_destroy(AsterPhysicsWorldHandle physics_world);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_destroy(AsterSystemWorldHandle system_world);
ASTER_KERNEL_API AsterStatus aster_kernel_sample_app_destroy(AsterSampleAppHandle sample_app);

#ifdef __cplusplus
}
#endif

#endif
