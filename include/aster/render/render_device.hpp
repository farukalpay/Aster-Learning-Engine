// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/mesh.hpp"

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

private:
  [[nodiscard]] const CpuMesh &meshForPrimitive(MeshPrimitive primitive) const;
  [[nodiscard]] const CpuMesh &meshForObject(const RenderObject &object);

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
};

} // namespace aster
