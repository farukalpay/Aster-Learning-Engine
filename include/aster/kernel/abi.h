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

#define ASTER_KERNEL_ABI_MAJOR 4u
#define ASTER_KERNEL_ABI_MINOR 0u
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
  ASTER_KERNEL_RENDER_PASS_LIGHT_CULL = 1,
  ASTER_KERNEL_RENDER_PASS_SHADOW_ATLAS = 2,
  ASTER_KERNEL_RENDER_PASS_OPAQUE = 3,
  ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW = 4,
  ASTER_KERNEL_RENDER_PASS_SCENE_LIGHTING = 5,
  ASTER_KERNEL_RENDER_PASS_VOLUMETRIC_FOG = 6,
  ASTER_KERNEL_RENDER_PASS_REFLECTION_PROBE = 7,
  ASTER_KERNEL_RENDER_PASS_TRANSPARENT = 8,
  ASTER_KERNEL_RENDER_PASS_UI_COMPOSITE = 9,
  ASTER_KERNEL_RENDER_PASS_CAPTURE = 10
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
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CAPABILITY_MISMATCH = 5,
  ASTER_KERNEL_FRAME_DIAGNOSTIC_CLUSTERED_LIGHTING_FALLBACK = 6
} AsterKernelFrameDiagnosticKind;

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
