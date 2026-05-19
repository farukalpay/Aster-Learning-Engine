// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/scene/scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aster {

struct ShadowCascadeContract {
  Vec3 center{};
  Vec3 right{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  float radius = 1.0f;
  std::uint32_t tile_x = 0u;
  std::uint32_t tile_y = 0u;
  std::uint32_t tile_width = 1u;
  std::uint32_t tile_height = 1u;
  Mat4 view_projection = identity();
};

struct ShadowAtlasContract {
  std::uint32_t atlas_size = 0u;
  std::uint32_t cascade_count = 0u;
  std::array<ShadowCascadeContract, 4> cascades{};
};

struct FogVolumeContract {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
};

struct ReflectionProbeAtlasContract {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t face_size = 0u;
  std::uint32_t probe_count = 0u;
};

[[nodiscard]] inline ShadowAtlasContract makeShadowAtlasContract(
    const OrbitCamera &camera, const RendererSettings &settings) {
  ShadowAtlasContract contract;
  if (!settings.shadows.enabled || !settings.sun_light.enabled ||
      settings.sun_light.intensity <= 0.0f) {
    return contract;
  }

  const Vec3 sun_dir =
      normalizeOr(settings.sun_light.direction_to_light, Vec3{0.0f, 1.0f, 0.0f});
  const Vec3 forward = -sun_dir;
  const Vec3 basis_seed =
      std::abs(forward.y) < 0.92f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  const Vec3 right = normalizeOr(cross(basis_seed, forward), Vec3{1.0f, 0.0f, 0.0f});
  const Vec3 up = normalizeOr(cross(forward, right), Vec3{0.0f, 1.0f, 0.0f});

  contract.atlas_size = std::clamp(settings.shadows.atlas_size, 32u, 2048u);
  contract.cascade_count =
      settings.shadows.cascaded_directional
          ? std::clamp(settings.shadows.directional_cascades, 1u, 4u)
          : 1u;
  const std::uint32_t base_tile_width = std::max(contract.atlas_size / contract.cascade_count, 1u);
  const float max_distance = std::max(settings.shadows.max_distance, 1.0f);

  for (std::uint32_t cascade_index = 0u; cascade_index < contract.cascade_count; ++cascade_index) {
    ShadowCascadeContract &cascade = contract.cascades[cascade_index];
    cascade.center = camera.target;
    cascade.right = right;
    cascade.up = up;
    cascade.forward = forward;
    cascade.radius = std::max(max_distance *
                                  (static_cast<float>(cascade_index + 1u) /
                                   static_cast<float>(contract.cascade_count)),
                              0.001f);
    cascade.tile_x = cascade_index * base_tile_width;
    cascade.tile_y = 0u;
    cascade.tile_width = cascade_index + 1u == contract.cascade_count
                             ? contract.atlas_size - cascade.tile_x
                             : base_tile_width;
    cascade.tile_height = contract.atlas_size;

    Camera light_camera;
    light_camera.eye = cascade.center - cascade.forward * cascade.radius;
    light_camera.target = cascade.center;
    light_camera.up = cascade.up;
    light_camera.projection_mode = CameraProjectionMode::Orthographic;
    light_camera.orthographic_height = cascade.radius * 2.0f;
    light_camera.near_plane = 0.05f;
    light_camera.far_plane = cascade.radius * 4.0f;
    light_camera.projection_policy = defaultProjectionPolicy();
    const float aspect = static_cast<float>(std::max(cascade.tile_width, 1u)) /
                         static_cast<float>(std::max(cascade.tile_height, 1u));
    cascade.view_projection = light_camera.viewProjectionMatrix(aspect).value;
  }
  return contract;
}

[[nodiscard]] inline FogVolumeContract makeFogVolumeContract(
    const RendererSettings &settings, const std::uint32_t frame_width,
    const std::uint32_t frame_height) {
  if (!settings.atmosphere.enabled || settings.atmosphere.fog_strength <= 0.0f ||
      frame_width == 0u || frame_height == 0u) {
    return {};
  }
  return {.width = std::max(frame_width / 4u, 1u),
          .height = std::max(frame_height / 4u, 1u)};
}

[[nodiscard]] inline ReflectionProbeAtlasContract makeReflectionProbeAtlasContract(
    const Scene &scene, const RendererSettings &settings) {
  if (!settings.reflections.enabled || !settings.reflections.static_local_probes ||
      scene.reflectionProbes().empty()) {
    return {};
  }
  const std::uint32_t max_probes =
      settings.reflections.max_active_probes == 0u
          ? static_cast<std::uint32_t>(scene.reflectionProbes().size())
          : settings.reflections.max_active_probes;
  ReflectionProbeAtlasContract contract;
  contract.probe_count = std::min<std::uint32_t>(
      static_cast<std::uint32_t>(scene.reflectionProbes().size()), std::max(max_probes, 1u));
  contract.face_size = std::clamp(settings.reflections.probe_resolution, 8u, 256u);
  contract.width = contract.face_size * 6u * contract.probe_count;
  contract.height = contract.face_size;
  return contract;
}

} // namespace aster
