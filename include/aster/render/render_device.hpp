// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/world_perception_ledger.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_graph.hpp"
#include "aster/render/render_graph_executor.hpp"
#include "aster/render/render_scene.hpp"
#include "aster/rhi/device.hpp"
#include "aster/rhi/frame_trace.hpp"
#include "aster/rhi/resource_registry.hpp"
#include "aster/rhi/resource_lifetime_validator.hpp"
#include "aster/texture/runtime_texture.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aster {

constexpr std::size_t kDefaultRenderLightBudget = 4;
constexpr std::size_t kRenderLightUniformCapacity = 64;

class Scene;
enum class MeshPrimitive;
class NativeRenderBackend;
struct NativeWindowSurface;
struct RenderObject;

enum class RenderBackendKind {
  SoftwareReference,
  Metal,
  D3D12,
  Null,
  Unknown,
};

struct RenderBackendCapabilities {
  RenderBackendKind kind = RenderBackendKind::Unknown;
  const char *name = "Unknown Renderer";
  bool gpu = false;
  bool supports_shader_materials = false;
  bool supports_texture_sampling = false;
  bool supports_instancing = false;
  bool supports_capture = false;
  bool supports_ui_composite = false;
  bool supports_gpu_timestamps = false;
  std::uint32_t graph_resource_mask = 0u;
  ProjectionConvention projection_convention = defaultProjectionConvention();
  rhi::DeviceCapabilities capability_table{};
};

struct RendererPresentDesc {
  bool vsync = true;
  bool wait_for_frame = true;
};

struct RendererPresentResult {
  bool presented = false;
  RenderBackendKind backend = RenderBackendKind::Unknown;
  rhi::PresentationMode presentation = rhi::PresentationMode::None;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t backbuffer_index = 0u;
  std::uint64_t frame_index = 0u;
  std::size_t queue_waits = 0u;
};

struct RendererPresentationStatus {
  RenderBackendKind backend = RenderBackendKind::Unknown;
  rhi::PresentationMode presentation = rhi::PresentationMode::None;
  bool native_present_supported = false;
  bool bound_window = false;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint64_t last_presented_frame = 0u;
};

struct Light {
  Vec3 position{};
  Vec3 color{};
  float intensity = 1.0f;
  float source_radius = 0.0f;
  float range = 0.0f;
  std::uint32_t emissive_source_tag = 0u;
  std::uint32_t flicker_profile_id = 0u;
  bool casts_shadow = false;
};

struct DirectionalLight {
  bool enabled = false;
  Vec3 direction_to_light{-0.42f, 0.82f, 0.38f};
  Vec3 color{1.0f, 0.86f, 0.62f};
  float intensity = 1.0f;
};

enum class ToneMapper {
  FilmicAces,
  PbrNeutral,
  Reinhard,
};

enum class AtmosphereFogFalloff : std::uint32_t {
  SmoothLinear,
  Exponential,
  Powered,
};

enum class RenderStylePreset : std::uint32_t {
  Neutral,
  RetroHorrorReadable,
};

struct RenderStyleProfile {
  RenderStylePreset preset = RenderStylePreset::Neutral;
  float unlit_mix = 0.0f;
  float emissive_gain = 1.0f;
  float luma_crush = 0.0f;
  float color_quantization_steps = 0.0f;
  float procedural_sample_snap = 0.0f;
};

using LightRig = std::vector<Light>;

struct RenderLightPolicy {
  std::size_t max_point_lights = kDefaultRenderLightBudget;
  bool distance_weighted = true;
  float min_intensity = 0.0f;
};

enum class ClusteredLightCullingMode : std::uint32_t {
  Disabled,
  CpuReference,
};

enum class ClusteredLightFallbackPolicy : std::uint32_t {
  SelectNearest,
  DisableOverflow,
};

struct ClusteredLightPolicy {
  bool enabled = false;
  ClusteredLightCullingMode mode = ClusteredLightCullingMode::CpuReference;
  ClusteredLightFallbackPolicy fallback = ClusteredLightFallbackPolicy::SelectNearest;
  std::uint32_t cluster_count_x = 8u;
  std::uint32_t cluster_count_y = 4u;
  std::uint32_t cluster_count_z = 8u;
  std::size_t max_visible_lights = kRenderLightUniformCapacity;
  std::uint32_t max_lights_per_cluster = 8u;
  float default_light_range = 12.0f;
};

struct ClusteredLightAssignment {
  std::uint32_t cluster_index = 0u;
  std::uint32_t light_index = 0u;
};

struct ClusteredLightGrid {
  std::uint32_t cluster_count_x = 0u;
  std::uint32_t cluster_count_y = 0u;
  std::uint32_t cluster_count_z = 0u;
  std::vector<Light> visible_lights;
  std::vector<std::uint32_t> cluster_offsets;
  std::vector<ClusteredLightAssignment> assignments;
  bool overflowed = false;
  bool fallback_used = false;
};

struct PackedClusteredLight {
  Vec3 position{};
  float radius = 0.0f;
  Vec3 color{};
  float intensity = 0.0f;
};

struct ClusteredLightFrameData {
  std::uint32_t cluster_count_x = 0u;
  std::uint32_t cluster_count_y = 0u;
  std::uint32_t cluster_count_z = 0u;
  std::vector<PackedClusteredLight> visible_lights;
  std::vector<std::uint32_t> cluster_offsets;
  std::vector<std::uint32_t> light_indices;
  std::uint64_t visible_lights_hash = 0u;
  std::uint64_t assignments_hash = 0u;
  bool overflowed = false;
  bool fallback_used = false;
};

[[nodiscard]] LightRig defaultLightRig();
[[nodiscard]] std::vector<Light> selectRenderLights(const LightRig &lights, Vec3 reference_position,
                                                    const RenderLightPolicy &policy);
[[nodiscard]] ClusteredLightGrid buildClusteredLightGrid(const LightRig &lights,
                                                         const OrbitCamera &camera,
                                                         int framebuffer_width,
                                                         int framebuffer_height,
                                                         const ClusteredLightPolicy &policy);
[[nodiscard]] ClusteredLightFrameData
buildClusteredLightFrameData(const ClusteredLightGrid &grid);

struct GroundingSettings {
  bool enabled = false;
  bool contact_shadows = false;
  bool auto_contact_shadows = false;
  float surface_occlusion_strength = 0.0f;
  float surface_occlusion_height = 0.80f;
  float surface_occlusion_mix = 0.35f;
  float surface_occlusion_min = 0.68f;
  float reference_y = 0.0f;
  float contact_shadow_strength = 0.34f;
  float contact_shadow_radius_scale = 1.16f;
  float contact_shadow_min_radius = 0.12f;
  float contact_shadow_max_radius = 1.35f;
  float contact_shadow_receiver_height = 1.20f;
  float contact_shadow_receiver_bias = 0.014f;
  float contact_shadow_detail_scale = 9.0f;
};

enum class RendererOcclusionMode : std::uint32_t {
  Disabled,
  SurfaceCavity,
  HorizonSearch,
  Hybrid,
};

struct RendererOcclusionSettings {
  bool enabled = false;
  RendererOcclusionMode mode = RendererOcclusionMode::Hybrid;
  float radius = 1.10f;
  float thickness = 0.18f;
  float strength = 0.36f;
  float distance_falloff = 1.60f;
  std::uint32_t sample_count = 12u;
  float cavity_bias = 0.045f;
  float contact_hardening = 0.28f;
  float micro_shadowing = 0.18f;
};

struct AtmosphereSettings {
  bool enabled = false;
  Vec3 fog_color{0.12f, 0.14f, 0.15f};
  float fog_start = 8.0f;
  float fog_end = 24.0f;
  float fog_strength = 0.0f;
  AtmosphereFogFalloff fog_falloff = AtmosphereFogFalloff::SmoothLinear;
  float fog_power = 1.0f;
  float saturation = 1.0f;
  float contrast = 1.0f;
  Vec3 shadow_tint{0.70f, 0.78f, 0.88f};
  float shadow_tint_strength = 0.0f;
  Vec3 highlight_tint{1.08f, 0.98f, 0.84f};
  float highlight_tint_strength = 0.0f;
};

struct GraphicsPipelineState {
  Vec3 clear_color{0.030f, 0.038f, 0.046f};
  bool depth_test = true;
  bool back_face_culling = true;
  bool multisampling = true;
  ToneMapper tone_mapper = ToneMapper::PbrNeutral;
};

struct LineOfSightFadeSettings {
  bool enabled = false;
  Vec3 camera_position{};
  Vec3 target_position{};
  float radius = 0.62f;
  float softness = 0.22f;
  float min_opacity = 0.24f;
  float camera_clearance = 0.25f;
  float target_clearance = 0.70f;
  float max_object_radius = 4.20f;
};

struct RendererPostSettings {
  bool hdr_scene_color = true;
  bool bloom = false;
  bool fxaa = false;
  float bloom_threshold = 2.6f;
  float bloom_intensity = 0.0f;
  float color_grade_saturation = 1.0f;
  float color_grade_contrast = 1.0f;
};

struct RendererShadowSettings {
  bool enabled = false;
  bool cascaded_directional = false;
  std::uint32_t directional_cascades = 0u;
  std::uint32_t atlas_size = 1024u;
  float max_distance = 32.0f;
  float receiver_bias = 0.014f;
  float normal_bias = 0.012f;
  float pcf_radius = 0.34f;
};

struct RendererReflectionSettings {
  bool enabled = false;
  bool static_local_probes = false;
  std::uint32_t probe_resolution = 128u;
  std::uint32_t max_active_probes = 0u;
  float fallback_intensity = 1.0f;
};

struct PresentationLensSettings {
  float focal_length_mm = 46.0f;
  float sensor_width_mm = 36.0f;
  float camera_height_m = 1.55f;
  float scale_reference_m = 1.80f;
  float composition_weight = 0.58f;
  float vignette_strength = 0.08f;
  float shoulder_strength = 0.16f;
};

struct SurfaceScaleSettings {
  float physical_texel_density = 512.0f;
  float macro_frequency_breakup = 0.32f;
  float micro_frequency_breakup = 0.46f;
  float height_normal_coupling = 0.82f;
  float roughness_height_coupling = 0.56f;
};

struct RendererForensicsSettings {
  bool detailed_traces = true;
  bool capture_payloads = true;
  bool backend_certification = true;
};

enum class MaterialDebugView {
  Beauty,
  BaseColor,
  Normal,
  Roughness,
  AmbientOcclusion,
  Fog,
};

struct RendererSettings {
  float exposure = 1.05f;
  float ambient_strength = 0.18f;
  float ambient_floor = 0.0f;
  float indirect_albedo_floor = 0.035f;
  Vec3 sky_ambient_color{0.42f, 0.47f, 0.52f};
  Vec3 ground_ambient_color{0.20f, 0.17f, 0.13f};
  bool use_aces_tonemap = false;
  bool animate_scene = true;
  bool procedural_surface_normals = false;
  DirectionalLight sun_light{};
  LightRig light_rig = defaultLightRig();
  RenderLightPolicy light_policy{};
  ClusteredLightPolicy clustered_lighting{};
  GroundingSettings grounding{};
  RendererOcclusionSettings occlusion{};
  AtmosphereSettings atmosphere{};
  GraphicsPipelineState pipeline{};
  LineOfSightFadeSettings line_of_sight_fade{};
  RendererPostSettings post{};
  RendererShadowSettings shadows{};
  RendererReflectionSettings reflections{};
  PresentationLensSettings presentation{};
  SurfaceScaleSettings surface_scale{};
  RendererForensicsSettings forensics{};
  RenderStyleProfile style{};
  MaterialDebugView material_debug_view = MaterialDebugView::Beauty;
};

struct FrameStats {
  double frame_seconds = 0.0;
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  std::size_t draw_calls = 0;
  std::size_t visible_objects = 0;
  std::size_t culled_objects = 0;
  std::size_t instance_groups = 0;
  std::size_t lod_culled_objects = 0;
  std::size_t visibility_hint_objects = 0;
  std::size_t dynamic_mesh_objects = 0;
  std::size_t dynamic_mesh_cache_entries = 0;
  std::size_t graph_passes = 0;
  std::size_t graph_resources = 0;
  std::size_t graph_barriers = 0;
  std::size_t graph_transient_resources = 0;
  std::size_t registry_live_resources = 0;
  std::size_t registry_retired_resources = 0;
  std::size_t timestamp_query_slots = 0;
  std::size_t queue_waits = 0;
  std::size_t pipeline_switches = 0;
  std::size_t material_permutations = 0;
  std::size_t material_variant_cache_hits = 0;
  std::size_t material_variant_cache_misses = 0;
  std::size_t active_point_lights = 0;
  std::size_t clustered_light_clusters = 0;
  std::size_t clustered_light_assignments = 0;
  std::size_t resource_lifetime_warnings = 0;
  std::uint32_t backend_feature_mask = 0u;
  std::uint32_t backend_kind_value = 0u;
  double rust_plan_seconds = 0.0;
  double graph_compile_seconds = 0.0;
  double render_encode_seconds = 0.0;
};

struct FramePassStats {
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  std::string name;
  std::size_t draw_calls = 0u;
  std::size_t pipeline_switches = 0u;
  std::size_t material_permutations = 0u;
  double encode_seconds = 0.0;
  double cpu_build_seconds = 0.0;
  double gpu_execution_seconds = 0.0;
  std::uint64_t estimated_bandwidth_bytes = 0u;
  std::uint32_t render_target_width = 0u;
  std::uint32_t render_target_height = 0u;
  std::size_t descriptor_heap_pressure = 0u;
  std::size_t pipeline_cache_hits = 0u;
  std::size_t pipeline_cache_misses = 0u;
};

enum class RendererDebugView : std::uint32_t {
  FinalColor,
  BaseColor,
  Normal,
  Roughness,
  Metallic,
  AmbientOcclusion,
  Emissive,
  Uv,
  MipLevel,
  Overdraw,
  LightClusters,
  ShadowMask,
  SurfaceAttributes,
  SurfaceOcclusion,
  Fog,
  ReflectionProbe,
};

struct FrameDebugRequest {
  RendererDebugView view = RendererDebugView::FinalColor;
  RenderGraphPass pass = RenderGraphPass::Capture;
  bool capture = false;
};

struct FrameDebugCapture {
  RenderGraphPass pass = RenderGraphPass::Capture;
  RendererDebugView view = RendererDebugView::FinalColor;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  std::string label;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t row_stride_bytes = 0u;
  std::uint64_t content_hash = 0u;
  bool available = false;
  std::vector<std::uint8_t> rgba8;
};

enum class BackendFeatureProofKind : std::uint32_t {
  GraphResource,
  Capture,
  TextureSampling,
  Instancing,
  GpuTimestamps,
  HdrRenderTarget,
  Msaa,
  Presentation,
};

enum class BackendFeatureProofStatus : std::uint32_t {
  NotAdvertised,
  NotExercised,
  Proven,
  MissingProof,
  Unsupported,
};

struct BackendFeatureProof {
  BackendFeatureProofKind kind = BackendFeatureProofKind::GraphResource;
  BackendFeatureProofStatus status = BackendFeatureProofStatus::NotAdvertised;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  std::string feature;
  std::string label;
  std::string message;
  std::uint32_t advertised = 0u;
  std::uint32_t native = 0u;
  std::uint64_t evidence_hash = 0u;
};

struct BackendCertificationReport {
  RenderBackendKind backend = RenderBackendKind::Unknown;
  bool valid = true;
  std::size_t proof_count = 0u;
  std::size_t proven_count = 0u;
  std::size_t missing_proof_count = 0u;
  std::size_t validation_error_count = 0u;
  std::size_t math_contract_error_count = 0u;
};

struct RenderMathContractReport {
  bool valid = true;
  bool backend_canonical = false;
  bool camera_canonical = false;
  bool camera_matches_backend = false;
  bool depth_contract_canonical = false;
  bool viewport_contract_canonical = false;
  bool matrix_contract_canonical = false;
  bool tangent_handedness_traced = true;
  bool normal_map_convention_valid = true;
  bool color_space_boundary_valid = true;
  std::size_t object_count = 0u;
  std::size_t non_finite_world_matrices = 0u;
  std::size_t singular_normal_matrices = 0u;
  std::size_t negative_tangent_flips = 0u;
  std::size_t texture_count = 0u;
  std::size_t normal_texture_count = 0u;
  std::size_t normal_convention_violations = 0u;
  std::size_t color_space_violations = 0u;
  std::size_t issue_count = 0u;
  std::uint64_t contract_hash = 0u;
  std::vector<std::string> issues;
};

struct FramePassArtifact {
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  std::string label;
  std::string kind;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint64_t content_hash = 0u;
  bool available = false;
};

struct FrameResourceTrace {
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  std::string pass_name;
  std::string resource_name;
  rhi::ResourceState before = rhi::ResourceState::Undefined;
  rhi::ResourceState after = rhi::ResourceState::Undefined;
  rhi::QueueKind queue = rhi::QueueKind::Graphics;
  bool write = false;
};

struct MaterialBindingTrace {
  std::string object_name;
  std::string material_asset_id;
  std::string role;
  std::string source_path;
  std::string texture_kind;
  std::string color_space;
  std::string fallback_reason;
  std::string backend_degradation;
  bool valid = false;
  bool fallback = true;
  bool bound = false;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t mip_count = 0u;
  std::uint64_t descriptor_layout_hash = 0u;
};

struct AssetFrameTrace {
  std::string object_name;
  std::size_t object_index = 0u;
  std::string source_asset_id;
  std::string source_graph_guid;
  std::string source_graph_node;
  std::string source_path;
  std::string source_node;
  std::string source_mesh;
  std::string material_slot;
  std::string shader_variant_key;
  std::string pipeline_cache_key;
  std::string procedural_capability_status;
  std::vector<std::string> issues;
  std::vector<std::string> texture_roles;
  std::vector<std::string> backend_degradations;
  std::uint64_t trace_hash = 0u;
};

struct MeshVisibilityTrace {
  std::string object_name;
  std::size_t object_index = 0u;
  bool visible = false;
  RenderGraphPass pass = RenderGraphPass::Opaque;
  float opacity = 1.0f;
  std::string reason;
};

struct ObjectClusterMembershipTrace {
  std::string object_name;
  std::size_t object_index = 0u;
  std::uint32_t cluster_index = 0u;
  bool visible = false;
};

struct ObjectRenderFateTrace {
  std::string object_name;
  std::size_t object_index = 0u;
  bool visible = false;
  std::string mesh_key;
  std::string material_key;
  std::string material_asset_id;
  std::string shader_variant_key;
  std::string pipeline_tag;
  std::string source_graph_guid;
  std::string source_graph_node;
  std::string pipeline_cache_key;
  std::string procedural_capability_status;
  std::string asset_source_path;
  std::string asset_source_node;
  std::string asset_source_mesh;
  std::string asset_material_slot;
  std::vector<std::string> asset_issues;
  std::vector<std::string> backend_degradations;
  std::vector<std::string> texture_roles;
  std::vector<std::string> pass_list;
  std::vector<std::string> resource_transitions;
  std::vector<std::string> capture_labels;
  std::vector<std::string> feature_proofs;
  std::string final_contribution;
  std::uint64_t contribution_hash = 0u;
};

struct SurfacePresentationTrace {
  std::string object_name;
  std::size_t object_index = 0u;
  float physical_texel_density = 0.0f;
  float height_normal_coupling = 0.0f;
  float roughness_height_coupling = 0.0f;
  float cavity_strength = 0.0f;
  float contact_hardening = 0.0f;
  float scale_reference_m = 0.0f;
  bool surface_occlusion_enabled = false;
  bool contact_shadow_receiver = false;
  std::uint64_t trace_hash = 0u;
};

enum class FrameDebuggerTimelineEventKind : std::uint32_t {
  Visibility,
  MaterialBinding,
  LightCluster,
  Shadow,
  SurfaceOcclusion,
  Fog,
  Probe,
  PassOutput,
  Overdraw,
  Fallback,
};

struct FrameDebuggerTimelineEvent {
  std::size_t sequence = 0u;
  FrameDebuggerTimelineEventKind kind = FrameDebuggerTimelineEventKind::Visibility;
  RenderGraphPass pass = RenderGraphPass::SceneColorDepth;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  std::string object_name;
  std::size_t object_index = 0u;
  std::string label;
  std::string evidence;
  std::string fallback_reason;
  std::uint64_t evidence_hash = 0u;
  double cpu_build_seconds = 0.0;
  double gpu_execution_seconds = 0.0;
  std::uint64_t estimated_bandwidth_bytes = 0u;
  std::uint32_t render_target_width = 0u;
  std::uint32_t render_target_height = 0u;
  std::size_t draw_count = 0u;
  std::size_t material_variant_count = 0u;
  std::size_t descriptor_heap_pressure = 0u;
  std::size_t pipeline_cache_hits = 0u;
  std::size_t pipeline_cache_misses = 0u;
};

enum class FrameResourceProvenanceKind : std::uint32_t {
  GraphResource,
  MaterialTexture,
};

struct FrameResourceProvenance {
  FrameResourceProvenanceKind kind = FrameResourceProvenanceKind::GraphResource;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  RenderGraphPass producer_pass = RenderGraphPass::SceneColorDepth;
  std::string resource_name;
  std::string producer_node;
  std::string material_asset_id;
  std::string material_graph_guid;
  std::string material_graph_node;
  std::string cook_report;
  std::string texture_role;
  std::string source_path;
  std::string asset_hash;
  std::string shader_variant_key;
  std::string backend_fallback;
  std::vector<std::string> upstream;
  std::uint64_t provenance_hash = 0u;
};

struct FrameRegressionGalleryEntry {
  std::string label;
  RenderBackendKind backend = RenderBackendKind::Unknown;
  RenderGraphPass pass = RenderGraphPass::Capture;
  RenderGraphResource resource = RenderGraphResource::SceneColor;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint64_t image_hash = 0u;
  std::uint64_t diff_hash = 0u;
  double mean_abs_error = 0.0;
  double differing_pixel_ratio = 0.0;
  std::string image_diff_status;
  std::string backend_difference;
  double pass_encode_seconds = 0.0;
  std::string asset_hash;
  std::string shader_variant_key;
  bool available = false;
};

enum class FrameDiagnosticSeverity : std::uint32_t {
  Info,
  Warning,
  Error,
};

enum class FrameDiagnosticKind : std::uint32_t {
  BackendFallback,
  MaterialVariantFallback,
  TranslucentSortChanged,
  NearPlaneClipping,
  ResourceLifetimeHazard,
  CapabilityMismatch,
  ClusteredLightingFallback,
  MathContract,
  NonFiniteWorldMatrix,
  SingularNormalMatrix,
  NegativeScaleTangentFlip,
  ProjectionConventionMismatch,
  ViewportOriginMismatch,
  BackendProjectionDrift,
  PredicateUncertainty,
  AssetProvenanceWarning,
  TextureRoleDegraded,
  MeshAttributeDegraded,
  SurfacePresentationWarning,
};

struct FrameDiagnosticEvent {
  FrameDiagnosticKind kind = FrameDiagnosticKind::BackendFallback;
  FrameDiagnosticSeverity severity = FrameDiagnosticSeverity::Info;
  std::string pass;
  std::string label;
  std::string message;
  std::uint64_t value = 0u;
};

struct FrameEvidence {
  std::uint64_t schema_id = 0x4153544552464556ull;
  std::uint64_t render_ir_hash = 0u;
  std::uint64_t visibility_plan_hash = 0u;
  std::uint32_t backend_kind = 0u;
  std::uint32_t draw_signature_count = 0u;
};

struct FrameForensics {
  FrameEvidence evidence{};
  std::uint64_t world_trace_hash = 0u;
  std::uint64_t simulation_tick = 0u;
  std::uint64_t extraction_hash = 0u;
  std::uint64_t asset_lineage_hash = 0u;
  bool world_transition_linked = false;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t actor_state_delta_hash = 0u;
  std::uint64_t sensory_event_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  std::uint64_t encounter_budget_hash = 0u;
  bool navigation_valid = false;
  std::uint64_t streaming_region_id = 0u;
  float perceptual_salience_score = 0.0f;
  bool perceptual_continuity_accepted = false;
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
  std::uint64_t perception_ledger_hash = 0u;
  std::size_t perception_ledger_cell_count = 0u;
  float perception_ledger_score = 0.0f;
  bool perception_ledger_accepted = false;
  std::vector<WorldPerceptionObjectTrace> perception_object_traces;
  std::vector<FramePassStats> passes;
  std::vector<FrameDiagnosticEvent> events;
  std::vector<FrameDebugCapture> captures;
  std::vector<FramePassArtifact> pass_artifacts;
  std::vector<FrameResourceTrace> resource_traces;
  std::vector<MaterialBindingTrace> material_bindings;
  std::vector<AssetFrameTrace> asset_traces;
  std::vector<MeshVisibilityTrace> mesh_visibility;
  std::vector<ObjectClusterMembershipTrace> object_clusters;
  std::vector<ObjectRenderFateTrace> object_fates;
  std::vector<SurfacePresentationTrace> surface_traces;
  std::vector<FrameDebuggerTimelineEvent> debug_timeline;
  std::vector<FrameResourceProvenance> resource_provenance;
  std::vector<FrameRegressionGalleryEntry> regression_gallery;
  std::vector<rhi::ResourceLifetimeValidationEvent> rhi_validation_events;
  std::vector<BackendFeatureProof> backend_feature_proofs;
  std::vector<rhi::TimestampQueryResult> timestamp_samples;
  BackendCertificationReport certification{};
  RenderMathContractReport math_contract{};
  ClusteredLightFrameData clustered_lights;
  rhi::FrameTrace rhi_trace{};
};

enum class AsterRenderProofSignal : std::uint32_t {
  PassProvenance,
  DescriptorPressure,
  PipelineCache,
  ResourceLifetime,
  MathContract,
  BackendFallback,
  AssetProvenance,
  VisualRegression,
  ClusteredLighting,
  SurfaceFidelity,
};

struct AsterRenderProofRow {
  AsterRenderProofSignal signal = AsterRenderProofSignal::PassProvenance;
  std::string label;
  std::string evidence;
  std::size_t count = 0u;
  bool ready = false;
  std::uint64_t hash = 0u;
};

struct AsterRenderProofSummary {
  bool production_trace_ready = false;
  std::size_t ready_signals = 0u;
  std::size_t blocked_signals = 0u;
  std::size_t descriptor_pressure = 0u;
  std::size_t pipeline_cache_hits = 0u;
  std::size_t pipeline_cache_misses = 0u;
  std::size_t backend_fallbacks = 0u;
  std::vector<AsterRenderProofRow> rows;
  std::vector<std::string> diagnostics;
};

class GpuFrameProfiler {
public:
  explicit GpuFrameProfiler(bool supported = false,
                            double timestamp_period_nanoseconds = 1.0);

  void beginFrame(std::uint32_t slot_count);
  void recordUnavailable(std::uint32_t slot_count = 0u);
  void recordSample(std::uint32_t slot, std::uint64_t ticks, bool available);

  [[nodiscard]] bool supported() const noexcept;
  [[nodiscard]] const std::vector<rhi::TimestampQueryResult> &samples() const noexcept;

private:
  bool supported_ = false;
  double timestamp_period_nanoseconds_ = 1.0;
  std::vector<rhi::TimestampQueryResult> samples_;
};

class RenderWorld {
public:
  void rebuild(const Scene &scene);
  [[nodiscard]] const RenderScene &scene() const noexcept;

private:
  RenderScene scene_;
};

class RenderGraphCompiler {
public:
  [[nodiscard]] FixedRenderGraph compileDefault(bool ui_overlay_enabled = true,
                                                bool capture_enabled = true);
  [[nodiscard]] double lastCompileSeconds() const noexcept;

private:
  double last_compile_seconds_ = 0.0;
};

class RenderAssetCache {
public:
  using CustomMeshCache = std::unordered_map<const CpuMesh *, CpuMesh>;
  using DynamicMeshCache =
      std::unordered_map<DynamicMeshResourceKey, CpuMesh, DynamicMeshResourceKeyHash>;

  void initializeBuiltins();
  [[nodiscard]] const CpuMesh &meshForPrimitive(MeshPrimitive primitive) const;
  [[nodiscard]] const CpuMesh &meshForObject(const RenderObject &object);
  void syncDynamicMeshes(const Scene &scene, rhi::ResourceRegistry &resource_registry,
                         bool immediate_eviction);

  [[nodiscard]] const CpuMesh &box() const noexcept {
    return box_;
  }
  [[nodiscard]] const CpuMesh &sphere() const noexcept {
    return sphere_;
  }
  [[nodiscard]] const CpuMesh &plane() const noexcept {
    return plane_;
  }
  [[nodiscard]] const CpuMesh &contactShadowPlane() const noexcept {
    return contact_shadow_plane_;
  }
  [[nodiscard]] const CpuMesh &rock() const noexcept {
    return rock_;
  }
  [[nodiscard]] const CpuMesh &crystal() const noexcept {
    return crystal_;
  }
  [[nodiscard]] const CpuMesh &ruinBlock() const noexcept {
    return ruin_block_;
  }
  [[nodiscard]] const CpuMesh &pillar() const noexcept {
    return pillar_;
  }
  [[nodiscard]] const CustomMeshCache &customMeshes() const noexcept {
    return custom_mesh_cache_;
  }
  [[nodiscard]] const DynamicMeshCache &customMeshResources() const noexcept {
    return custom_mesh_resource_cache_;
  }
  [[nodiscard]] std::size_t cachedMeshCount() const noexcept {
    return custom_mesh_cache_.size() + custom_mesh_resource_cache_.size();
  }

private:
  CpuMesh box_;
  CpuMesh sphere_;
  CpuMesh plane_;
  CpuMesh contact_shadow_plane_;
  CpuMesh rock_;
  CpuMesh crystal_;
  CpuMesh ruin_block_;
  CpuMesh pillar_;
  CustomMeshCache custom_mesh_cache_;
  std::unordered_map<const CpuMesh *, std::uint64_t> custom_mesh_last_seen_;
  DynamicMeshCache custom_mesh_resource_cache_;
  std::unordered_map<DynamicMeshResourceKey, std::uint64_t, DynamicMeshResourceKeyHash>
      custom_mesh_resource_last_seen_;
  std::unordered_map<const CpuMesh *, rhi::BufferHandle> custom_mesh_resource_handles_;
  std::unordered_map<DynamicMeshResourceKey, rhi::BufferHandle, DynamicMeshResourceKeyHash>
      dynamic_mesh_resource_handles_;
  std::uint64_t mesh_cache_frame_ = 0u;
};

class FrameExecutor {
public:
  [[nodiscard]] std::size_t execute(const FixedRenderGraph &graph,
                                    const RenderGraphPassCallback &callback) const;
};

class FrameDebugger {
public:
  void appendGraphForensics(const FixedRenderGraph &graph,
                            const RenderBackendCapabilities &capabilities, int framebuffer_width,
                            int framebuffer_height, FrameForensics &forensics) const;
};

class RenderDevice {
public:
  RenderDevice();
  ~RenderDevice();

  RenderDevice(const RenderDevice &) = delete;
  RenderDevice &operator=(const RenderDevice &) = delete;

  void initialize();
  void prepareScene(const Scene &scene);
  bool bindWindow(const NativeWindowSurface &surface);
  void setMaterialResourceLibrary(std::shared_ptr<const MaterialResourceLibrary> library);
  FrameStats render(const Scene &scene, const OrbitCamera &camera, const RendererSettings &settings,
                    int framebuffer_width, int framebuffer_height, double frame_seconds);
  RendererPresentResult present(const NativeWindowSurface &surface,
                                const RendererPresentDesc &desc);

  [[nodiscard]] const char *backendName() const;
  [[nodiscard]] RenderBackendCapabilities backendCapabilities() const;
  [[nodiscard]] RendererPresentationStatus presentationStatus() const;
  [[nodiscard]] const FixedRenderGraph &renderGraph() const;
  [[nodiscard]] const FrameForensics &lastFrameForensics() const;
  void stampLastFrameCausalTrace(std::uint64_t world_trace_hash, std::uint64_t simulation_tick,
                                 std::uint64_t extraction_hash, std::uint64_t asset_lineage_hash,
                                 std::uint64_t world_transition_hash,
                                 std::uint64_t actor_state_delta_hash,
                                 std::uint64_t sensory_event_hash,
                                 std::uint64_t visibility_set_hash,
                                 std::uint64_t encounter_budget_hash, bool navigation_valid,
                                 std::uint64_t streaming_region_id,
                                 float perceptual_salience_score,
                                 bool perceptual_continuity_accepted,
                                 std::uint32_t perceptual_continuity_required_channel_mask,
                                 std::uint32_t perceptual_continuity_observed_channel_mask,
                                 std::uint32_t perceptual_continuity_missing_channel_mask,
                                 float perceptual_continuity_score,
                                 float perceptual_continuity_minimum_score,
                                 std::uint64_t reaction_package_hash,
                                 std::uint64_t material_memory_hash,
                                 std::uint64_t lighting_atmosphere_hash,
                                 std::uint64_t ai_attention_hash,
                                 std::uint64_t streaming_residency_lod_hash,
                                 std::uint64_t resource_state_hash,
                                 std::uint64_t event_residue_hash,
                                 std::uint64_t readability_audit_hash);
  void stampLastFramePerceptionLedger(const WorldPerceptionLedgerReport &ledger,
                                      std::vector<WorldPerceptionObjectTrace> object_traces);
  [[nodiscard]] const std::shared_ptr<const MaterialResourceLibrary> &materialResourceLibrary()
      const noexcept;

private:
  [[nodiscard]] const CpuMesh &meshForPrimitive(MeshPrimitive primitive) const;
  [[nodiscard]] const CpuMesh &meshForObject(const RenderObject &object);
  void syncDynamicMeshes(const Scene &scene, bool immediate_eviction);

  std::unique_ptr<NativeRenderBackend> native_backend_;
  RenderAssetCache asset_cache_;
  RenderWorld render_world_;
  RenderGraphCompiler graph_compiler_;
  FrameExecutor frame_executor_;
  FrameDebugger frame_debugger_;
  FixedRenderGraph render_graph_;
  framegraph::FrameGraph frame_graph_;
  framegraph::CompiledFrameGraph compiled_frame_graph_;
  rhi::ResourceRegistry resource_registry_;
  std::unordered_map<std::uint64_t, MaterialPermutationArtifact> material_artifact_cache_;
  std::shared_ptr<const MaterialResourceLibrary> material_library_;
  std::vector<std::size_t> previous_transparent_order_;
  FrameForensics last_forensics_;
};

[[nodiscard]] std::string_view renderBackendKindName(RenderBackendKind kind);
[[nodiscard]] std::string_view backendFeatureProofKindName(BackendFeatureProofKind kind);
[[nodiscard]] std::string_view backendFeatureProofStatusName(BackendFeatureProofStatus status);
[[nodiscard]] std::string_view
frameDebuggerTimelineEventKindName(FrameDebuggerTimelineEventKind kind);
[[nodiscard]] std::string_view frameResourceProvenanceKindName(FrameResourceProvenanceKind kind);
[[nodiscard]] std::string_view asterRenderProofSignalName(AsterRenderProofSignal signal);
[[nodiscard]] AsterRenderProofSummary summarizeAsterRenderProof(
    const FrameForensics &forensics);
[[nodiscard]] RenderMathContractReport certifyRenderMathContract(
    const Scene &scene, const OrbitCamera &camera, const RenderBackendCapabilities &capabilities,
    const MaterialResourceLibrary *library = nullptr);
[[nodiscard]] std::string_view renderStylePresetName(RenderStylePreset preset);
[[nodiscard]] std::optional<RenderStylePreset> parseRenderStylePreset(std::string_view value);
[[nodiscard]] RenderStyleProfile makeRenderStyleProfile(RenderStylePreset preset);
void applyRenderStyleProfile(RendererSettings &settings, const RenderStyleProfile &profile);

} // namespace aster
