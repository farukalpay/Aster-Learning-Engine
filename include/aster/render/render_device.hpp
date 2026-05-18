// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_graph.hpp"
#include "aster/render/render_scene.hpp"
#include "aster/rhi/resource_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace aster {

constexpr std::size_t kRenderLightCount = 4;

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
};

struct Light {
  Vec3 position{};
  Vec3 color{};
  float intensity = 1.0f;
  float source_radius = 0.0f;
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

using LightRig = std::array<Light, kRenderLightCount>;

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
  LightRig light_rig{{
      Light{{-5.8f, 7.2f, 3.8f}, {29.0f, 27.0f, 23.0f}, 1.0f},
      Light{{4.7f, 3.6f, 2.0f}, {11.0f, 12.0f, 13.5f}, 1.0f},
      Light{{0.2f, 3.8f, -8.8f}, {14.0f, 10.5f, 7.8f}, 1.0f},
      Light{{0.0f, 2.0f, 4.8f}, {7.0f, 6.7f, 7.2f}, 1.0f},
  }};
  GroundingSettings grounding{};
  AtmosphereSettings atmosphere{};
  GraphicsPipelineState pipeline{};
  LineOfSightFadeSettings line_of_sight_fade{};
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
  std::uint32_t backend_feature_mask = 0u;
  std::uint32_t backend_kind_value = 0u;
  double rust_plan_seconds = 0.0;
  double graph_compile_seconds = 0.0;
  double render_encode_seconds = 0.0;
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
};

[[nodiscard]] std::string_view renderBackendKindName(RenderBackendKind kind);

} // namespace aster
