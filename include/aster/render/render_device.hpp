// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_graph.hpp"
#include "aster/render/render_scene.hpp"
#include "aster/rhi/device.hpp"
#include "aster/rhi/resource_registry.hpp"

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
  rhi::DeviceCapabilities capability_table{};
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

[[nodiscard]] LightRig defaultLightRig();
[[nodiscard]] std::vector<Light> selectRenderLights(const LightRig &lights, Vec3 reference_position,
                                                    const RenderLightPolicy &policy);
[[nodiscard]] ClusteredLightGrid buildClusteredLightGrid(const LightRig &lights,
                                                         const OrbitCamera &camera,
                                                         int framebuffer_width,
                                                         int framebuffer_height,
                                                         const ClusteredLightPolicy &policy);

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

struct RendererSettings {
  float exposure = 1.05f;
  float ambient_strength = 0.18f;
  float ambient_floor = 0.0f;
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
  AtmosphereSettings atmosphere{};
  GraphicsPipelineState pipeline{};
  LineOfSightFadeSettings line_of_sight_fade{};
  RenderStyleProfile style{};
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
  std::vector<FramePassStats> passes;
  std::vector<FrameDiagnosticEvent> events;
};

class RenderDevice {
public:
  RenderDevice();
  ~RenderDevice();

  RenderDevice(const RenderDevice &) = delete;
  RenderDevice &operator=(const RenderDevice &) = delete;

  void initialize();
  void prepareScene(const Scene &scene);
  FrameStats render(const Scene &scene, const OrbitCamera &camera, const RendererSettings &settings,
                    int framebuffer_width, int framebuffer_height, double frame_seconds);

  [[nodiscard]] const char *backendName() const;
  [[nodiscard]] RenderBackendCapabilities backendCapabilities() const;
  [[nodiscard]] const FixedRenderGraph &renderGraph() const;
  [[nodiscard]] const FrameForensics &lastFrameForensics() const;

private:
  [[nodiscard]] const CpuMesh &meshForPrimitive(MeshPrimitive primitive) const;
  [[nodiscard]] const CpuMesh &meshForObject(const RenderObject &object);
  void syncDynamicMeshes(const Scene &scene, bool immediate_eviction);

  std::unique_ptr<NativeRenderBackend> native_backend_;
  CpuMesh box_;
  CpuMesh sphere_;
  CpuMesh plane_;
  CpuMesh contact_shadow_plane_;
  CpuMesh rock_;
  CpuMesh crystal_;
  CpuMesh ruin_block_;
  CpuMesh pillar_;
  std::unordered_map<const CpuMesh *, CpuMesh> custom_mesh_cache_;
  std::unordered_map<const CpuMesh *, std::uint64_t> custom_mesh_last_seen_;
  std::unordered_map<DynamicMeshResourceKey, CpuMesh, DynamicMeshResourceKeyHash>
      custom_mesh_resource_cache_;
  std::unordered_map<DynamicMeshResourceKey, std::uint64_t, DynamicMeshResourceKeyHash>
      custom_mesh_resource_last_seen_;
  std::uint64_t mesh_cache_frame_ = 0u;
  double graph_compile_seconds_ = 0.0;
  RenderScene render_scene_;
  FixedRenderGraph render_graph_;
  framegraph::FrameGraph frame_graph_;
  framegraph::CompiledFrameGraph compiled_frame_graph_;
  rhi::ResourceRegistry resource_registry_;
  std::unordered_map<const CpuMesh *, rhi::BufferHandle> custom_mesh_resource_handles_;
  std::unordered_map<DynamicMeshResourceKey, rhi::BufferHandle, DynamicMeshResourceKeyHash>
      dynamic_mesh_resource_handles_;
  std::unordered_map<std::uint64_t, MaterialPermutationArtifact> material_artifact_cache_;
  std::vector<std::size_t> previous_transparent_order_;
  FrameForensics last_forensics_;
};

[[nodiscard]] std::string_view renderBackendKindName(RenderBackendKind kind);
[[nodiscard]] std::string_view renderStylePresetName(RenderStylePreset preset);
[[nodiscard]] std::optional<RenderStylePreset> parseRenderStylePreset(std::string_view value);
[[nodiscard]] RenderStyleProfile makeRenderStyleProfile(RenderStylePreset preset);
void applyRenderStyleProfile(RendererSettings &settings, const RenderStyleProfile &profile);

} // namespace aster
