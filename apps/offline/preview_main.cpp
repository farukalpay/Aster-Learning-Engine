// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/render/visual_regression.hpp"
#include "aster/samples/showcase_scenes.hpp"
#include "aster/scene/scene.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::string argumentValue(const int argc, char **argv, const std::string_view name,
                          const std::string &fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

aster::Scene makeScene(const std::string &scene_name) {
  if (scene_name == "industrial-pipe") {
    return aster::makeIndustrialPipeScene();
  }
  if (scene_name == "material-lab") {
    return aster::makeMaterialLabShowcaseScene();
  }
  if (scene_name == "mesh-lab") {
    return aster::makeMeshLabShowcaseScene();
  }
  if (scene_name == "lighting-lab") {
    return aster::makeLightingLabShowcaseScene();
  }
  if (scene_name == "scene-lab") {
    return aster::makeSceneLabShowcaseScene();
  }
  if (scene_name == "cave-conformance") {
    return aster::makeCaveConformanceShowcaseScene();
  }
  if (scene_name == "clean-cave") {
    return aster::makeCleanCaveShowcaseScene();
  }
  if (scene_name == "cercopithecidae") {
    return aster::makeCercopithecidaeShowcaseScene();
  }
  if (scene_name == "architecture") {
    return aster::makeArchitectureShowcaseScene();
  }
  throw std::runtime_error("Unknown preview scene: " + scene_name);
}

aster::OrbitCamera makeCamera(const std::string &scene_name) {
  aster::OrbitCamera camera;
  if (scene_name == "industrial-pipe") {
    camera.target = {0.0f, 0.58f, 0.0f};
    camera.yaw = aster::radians(64.0f);
    camera.pitch = aster::radians(15.0f);
    camera.radius = 5.0f;
    camera.vertical_fov = aster::radians(42.0f);
    return camera;
  }
  if (scene_name == "material-lab") {
    camera.target = {0.02f, 0.54f, -0.03f};
    camera.yaw = aster::radians(19.0f);
    camera.pitch = aster::radians(17.0f);
    camera.radius = 7.05f;
    camera.vertical_fov = aster::radians(42.0f);
    return camera;
  }
  if (scene_name == "mesh-lab") {
    camera.target = {0.12f, 0.72f, 0.0f};
    camera.yaw = aster::radians(24.0f);
    camera.pitch = aster::radians(18.0f);
    camera.radius = 6.5f;
    camera.vertical_fov = aster::radians(39.0f);
    return camera;
  }
  if (scene_name == "lighting-lab") {
    camera.target = {0.24f, 0.62f, 0.02f};
    camera.yaw = aster::radians(31.0f);
    camera.pitch = aster::radians(15.0f);
    camera.radius = 5.7f;
    camera.vertical_fov = aster::radians(38.0f);
    return camera;
  }
  if (scene_name == "scene-lab") {
    camera.target = {0.0f, 0.58f, 0.08f};
    camera.yaw = aster::radians(35.0f);
    camera.pitch = aster::radians(24.0f);
    camera.radius = 6.8f;
    camera.vertical_fov = aster::radians(44.0f);
    return camera;
  }
  if (scene_name == "cave-conformance") {
    camera.target = {0.0f, 0.92f, -0.92f};
    camera.yaw = aster::radians(28.0f);
    camera.pitch = aster::radians(13.0f);
    camera.radius = 4.9f;
    camera.vertical_fov = aster::radians(43.0f);
    return camera;
  }
  if (scene_name == "clean-cave") {
    camera.target = {0.0f, 0.82f, -1.35f};
    camera.yaw = aster::radians(0.0f);
    camera.pitch = aster::radians(7.0f);
    camera.radius = 3.55f;
    camera.vertical_fov = aster::radians(43.0f);
    return camera;
  }
  if (scene_name == "cercopithecidae") {
    camera.target = {0.08f, 0.92f, -0.52f};
    camera.yaw = aster::radians(28.0f);
    camera.pitch = aster::radians(16.0f);
    camera.radius = 5.35f;
    camera.vertical_fov = aster::radians(40.0f);
    return camera;
  }
  camera.target = {0.0f, 1.18f, -0.35f};
  camera.yaw = aster::radians(30.0f);
  camera.pitch = aster::radians(22.0f);
  camera.radius = 7.8f;
  return camera;
}

aster::RendererSettings makeSettings(const std::string &scene_name) {
  aster::RendererSettings settings;
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = true;
  settings.ambient_strength = 0.24f;
  settings.ambient_floor = 0.018f;
  settings.exposure = 0.96f;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.50f, 0.78f, 0.36f};
  settings.sun_light.color = {1.0f, 0.86f, 0.70f};
  settings.sun_light.intensity = 2.6f;
  settings.pipeline.clear_color = {0.024f, 0.028f, 0.032f};
  settings.sky_ambient_color = {0.32f, 0.37f, 0.42f};
  settings.ground_ambient_color = {0.12f, 0.105f, 0.092f};
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.050f, 0.056f, 0.064f};
  settings.atmosphere.fog_start = 8.0f;
  settings.atmosphere.fog_end = 18.0f;
  settings.atmosphere.fog_strength = 0.10f;
  settings.light_rig = {
      aster::Light{{-3.6f, 3.8f, 2.6f}, {16.0f, 13.5f, 10.0f}, 1.0f, 0.7f},
      aster::Light{{3.6f, 2.2f, 1.8f}, {5.0f, 6.8f, 9.5f}, 1.0f, 0.9f},
      aster::Light{{0.8f, 3.4f, -3.4f}, {7.5f, 6.4f, 5.0f}, 1.0f, 1.1f},
      aster::Light{{0.0f, 1.0f, 3.4f}, {1.8f, 2.1f, 2.5f}, 1.0f, 1.2f},
  };
  settings.occlusion.enabled = true;
  settings.occlusion.radius = 1.05f;
  settings.occlusion.thickness = 0.16f;
  settings.occlusion.strength = 0.34f;
  settings.occlusion.sample_count = 12u;
  settings.occlusion.contact_hardening = 0.28f;
  settings.surface_scale.physical_texel_density = 512.0f;
  settings.surface_scale.height_normal_coupling = 0.82f;
  settings.surface_scale.roughness_height_coupling = 0.56f;
  settings.presentation.focal_length_mm = 46.0f;
  settings.presentation.camera_height_m = 1.55f;
  settings.presentation.scale_reference_m = 1.80f;

  if (scene_name == "architecture") {
    settings.exposure = 1.16f;
    settings.ambient_strength = 0.28f;
    settings.sun_light.intensity = 1.04f;
    settings.pipeline.clear_color = {0.060f, 0.072f, 0.088f};
    settings.atmosphere.fog_color = {0.13f, 0.15f, 0.18f};
    settings.atmosphere.fog_start = 7.0f;
    settings.atmosphere.fog_end = 20.0f;
    settings.atmosphere.fog_strength = 0.12f;
  }
  if (scene_name == "material-lab") {
    settings.exposure = 1.10f;
    settings.ambient_strength = 0.30f;
    settings.ambient_floor = 0.016f;
    settings.indirect_albedo_floor = 0.022f;
    settings.sun_light.direction_to_light = {-0.52f, 0.80f, 0.30f};
    settings.sun_light.color = {1.0f, 0.90f, 0.74f};
    settings.sun_light.intensity = 4.35f;
    settings.pipeline.clear_color = {0.030f, 0.046f, 0.074f};
    settings.sky_ambient_color = {0.56f, 0.70f, 0.98f};
    settings.ground_ambient_color = {0.230f, 0.160f, 0.096f};
    settings.atmosphere.enabled = true;
    settings.atmosphere.fog_color = {0.150f, 0.184f, 0.245f};
    settings.atmosphere.fog_start = 5.0f;
    settings.atmosphere.fog_end = 24.0f;
    settings.atmosphere.fog_strength = 0.12f;
    settings.atmosphere.fog_falloff = aster::AtmosphereFogFalloff::Exponential;
    settings.atmosphere.fog_power = 1.55f;
    settings.atmosphere.local_light_scattering = 0.18f;
    settings.atmosphere.local_light_extinction = 0.034f;
    settings.atmosphere.source_glow_strength = 0.28f;
    settings.atmosphere.source_glow_radius_scale = 1.05f;
    settings.atmosphere.phase_anisotropy = 0.26f;
    settings.atmosphere.volumetric_light_steps = 6u;
    settings.atmosphere.saturation = 1.16f;
    settings.atmosphere.contrast = 1.03f;
    settings.atmosphere.shadow_tint = {0.68f, 0.76f, 0.90f};
    settings.atmosphere.shadow_tint_strength = 0.06f;
    settings.atmosphere.highlight_tint = {1.08f, 0.98f, 0.82f};
    settings.atmosphere.highlight_tint_strength = 0.09f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.contact_shadow_strength = 0.62f;
    settings.grounding.contact_shadow_radius_scale = 1.10f;
    settings.grounding.contact_shadow_max_radius = 1.34f;
    settings.grounding.contact_shadow_receiver_height = 1.36f;
    settings.occlusion.radius = 1.16f;
    settings.occlusion.thickness = 0.14f;
    settings.occlusion.strength = 0.46f;
    settings.occlusion.distance_falloff = 1.35f;
    settings.occlusion.sample_count = 16u;
    settings.occlusion.contact_hardening = 0.32f;
    settings.occlusion.micro_shadowing = 0.22f;
    settings.shadows.enabled = true;
    settings.shadows.cascaded_directional = true;
    settings.shadows.directional_cascades = 2u;
    settings.shadows.atlas_size = 192u;
    settings.shadows.max_distance = 16.0f;
    settings.shadows.pcf_radius = 0.36f;
    settings.reflections.enabled = true;
    settings.reflections.static_local_probes = true;
    settings.reflections.probe_resolution = 16u;
    settings.reflections.max_active_probes = 3u;
    settings.reflections.fallback_intensity = 1.75f;
    settings.post.bloom = true;
    settings.post.fxaa = true;
    settings.post.bloom_threshold = 1.75f;
    settings.post.bloom_intensity = 0.09f;
    settings.surface_scale.physical_texel_density = 960.0f;
    settings.surface_scale.height_normal_coupling = 1.25f;
    settings.surface_scale.roughness_height_coupling = 0.95f;
    settings.surface_scale.macro_frequency_breakup = 0.52f;
    settings.surface_scale.micro_frequency_breakup = 0.78f;
    settings.presentation.focal_length_mm = 50.0f;
    settings.presentation.composition_weight = 0.72f;
    settings.presentation.vignette_strength = 0.06f;
    settings.presentation.shoulder_strength = 0.26f;
    settings.light_rig = {
        aster::Light{{-3.6f, 3.7f, 1.8f}, {8.2f, 6.4f, 4.5f}, 1.0f, 0.82f},
        aster::Light{{2.9f, 1.9f, 1.5f}, {2.0f, 3.4f, 6.0f}, 1.0f, 1.05f},
        aster::Light{{0.4f, 2.9f, -3.0f}, {3.4f, 4.4f, 6.4f}, 1.0f, 1.22f},
    };
  }
  if (scene_name == "industrial-pipe") {
    settings.exposure = 0.92f;
    settings.ambient_strength = 0.18f;
    settings.ambient_floor = 0.010f;
    settings.sun_light.direction_to_light = {-0.52f, 0.80f, 0.30f};
    settings.sun_light.intensity = 2.35f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.contact_shadow_strength = 0.56f;
    settings.grounding.contact_shadow_radius_scale = 1.22f;
    settings.shadows.enabled = true;
    settings.shadows.cascaded_directional = true;
    settings.shadows.directional_cascades = 2u;
    settings.shadows.atlas_size = 256u;
  }
  if (scene_name == "mesh-lab") {
    settings.exposure = 1.04f;
    settings.ambient_strength = 0.25f;
    settings.sun_light.intensity = 2.10f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.atmosphere.fog_strength = 0.05f;
  }
  if (scene_name == "lighting-lab") {
    settings.exposure = 0.92f;
    settings.ambient_strength = 0.14f;
    settings.sun_light.intensity = 1.70f;
    settings.pipeline.clear_color = {0.018f, 0.020f, 0.023f};
    settings.atmosphere.fog_color = {0.045f, 0.042f, 0.040f};
    settings.atmosphere.fog_start = 4.0f;
    settings.atmosphere.fog_end = 10.0f;
    settings.atmosphere.fog_strength = 0.28f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.contact_shadow_strength = 0.42f;
  }
  if (scene_name == "scene-lab") {
    settings.exposure = 1.02f;
    settings.ambient_strength = 0.22f;
    settings.sun_light.intensity = 1.95f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
  }
  if (scene_name == "cave-conformance") {
    settings.exposure = 0.88f;
    settings.ambient_strength = 0.15f;
    settings.ambient_floor = 0.020f;
    settings.sun_light.enabled = true;
    settings.sun_light.direction_to_light = {-0.42f, 0.84f, 0.28f};
    settings.sun_light.color = {1.0f, 0.82f, 0.58f};
    settings.sun_light.intensity = 1.35f;
    settings.pipeline.clear_color = {0.014f, 0.014f, 0.016f};
    settings.atmosphere.fog_color = {0.048f, 0.044f, 0.040f};
    settings.atmosphere.fog_start = 2.0f;
    settings.atmosphere.fog_end = 7.4f;
    settings.atmosphere.fog_strength = 0.30f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.shadows.enabled = true;
    settings.shadows.cascaded_directional = true;
    settings.shadows.directional_cascades = 2u;
    settings.shadows.atlas_size = 128u;
    settings.occlusion.radius = 1.28f;
    settings.occlusion.strength = 0.46f;
    settings.occlusion.sample_count = 16u;
    settings.occlusion.contact_hardening = 0.38f;
    settings.surface_scale.physical_texel_density = 768.0f;
    settings.surface_scale.macro_frequency_breakup = 0.44f;
    settings.surface_scale.micro_frequency_breakup = 0.60f;
    settings.surface_scale.height_normal_coupling = 0.90f;
    settings.surface_scale.roughness_height_coupling = 0.70f;
    settings.presentation.focal_length_mm = 52.0f;
    settings.reflections.enabled = true;
    settings.reflections.static_local_probes = true;
    settings.reflections.probe_resolution = 16u;
    settings.reflections.max_active_probes = 1u;
    settings.reflections.fallback_intensity = 0.86f;
    settings.light_rig = {
        aster::Light{{-1.58f, 1.20f, -1.52f}, {5.2f, 2.0f, 0.62f}, 1.0f, 0.52f},
        aster::Light{{1.35f, 1.08f, -1.82f}, {4.4f, 1.7f, 0.54f}, 1.0f, 0.48f},
        aster::Light{{0.0f, 0.82f, 0.72f}, {0.62f, 1.05f, 1.45f}, 1.0f, 0.86f},
    };
  }
  if (scene_name == "clean-cave") {
    settings.exposure = 0.94f;
    settings.ambient_strength = 0.17f;
    settings.ambient_floor = 0.022f;
    settings.sun_light.enabled = true;
    settings.sun_light.direction_to_light = {-0.38f, 0.84f, 0.30f};
    settings.sun_light.color = {1.0f, 0.82f, 0.62f};
    settings.sun_light.intensity = 1.22f;
    settings.pipeline.clear_color = {0.014f, 0.014f, 0.016f};
    settings.atmosphere.fog_color = {0.050f, 0.046f, 0.041f};
    settings.atmosphere.fog_start = 2.2f;
    settings.atmosphere.fog_end = 7.2f;
    settings.atmosphere.fog_strength = 0.24f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.shadows.enabled = true;
    settings.shadows.cascaded_directional = true;
    settings.shadows.directional_cascades = 2u;
    settings.shadows.atlas_size = 128u;
    settings.occlusion.radius = 1.18f;
    settings.occlusion.strength = 0.40f;
    settings.occlusion.sample_count = 14u;
    settings.occlusion.contact_hardening = 0.32f;
    settings.surface_scale.physical_texel_density = 640.0f;
    settings.surface_scale.macro_frequency_breakup = 0.38f;
    settings.surface_scale.micro_frequency_breakup = 0.54f;
    settings.reflections.enabled = true;
    settings.reflections.static_local_probes = true;
    settings.reflections.probe_resolution = 16u;
    settings.reflections.max_active_probes = 1u;
    settings.reflections.fallback_intensity = 0.78f;
    settings.light_rig = {
        aster::Light{{-1.30f, 1.22f, -1.66f}, {4.6f, 1.9f, 0.62f}, 1.0f, 0.52f},
        aster::Light{{1.22f, 1.06f, -1.90f}, {3.7f, 1.5f, 0.52f}, 1.0f, 0.52f},
        aster::Light{{0.0f, 0.82f, -2.05f}, {2.2f, 1.5f, 1.1f}, 1.0f, 0.72f},
    };
  }
  if (scene_name == "cercopithecidae") {
    settings.exposure = 1.12f;
    settings.ambient_strength = 0.30f;
    settings.sun_light.intensity = 2.35f;
    settings.sun_light.direction_to_light = {-0.40f, 0.82f, 0.26f};
    settings.pipeline.clear_color = {0.020f, 0.024f, 0.026f};
    settings.atmosphere.fog_strength = 0.06f;
    settings.grounding.enabled = true;
    settings.grounding.contact_shadows = true;
    settings.grounding.auto_contact_shadows = true;
    settings.grounding.contact_shadow_strength = 0.36f;
    settings.light_rig = {
        aster::Light{{-2.8f, 3.2f, 1.4f}, {10.0f, 8.0f, 6.0f}, 1.0f, 0.76f},
        aster::Light{{2.4f, 1.8f, -1.8f}, {3.5f, 4.2f, 5.5f}, 1.0f, 0.90f},
        aster::Light{{0.0f, 2.4f, 1.9f}, {4.0f, 3.2f, 2.6f}, 1.0f, 0.94f},
    };
  }

  return settings;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const int width = std::stoi(argumentValue(argc, argv, "--width", "960"));
    const int height = std::stoi(argumentValue(argc, argv, "--height", "540"));
    const int samples = std::stoi(argumentValue(argc, argv, "--samples", "1"));
    const std::string scene_name = argumentValue(argc, argv, "--scene", "architecture");
    const std::filesystem::path output =
        argumentValue(argc, argv, "--output", "assets/screenshots/aster_preview.ppm");

    const aster::Scene scene = makeScene(scene_name);
    const aster::SoftwarePreviewOptions options{.width = width,
                                                .height = height,
                                                .samples_per_axis = std::clamp(samples, 1, 4),
                                                .frame_seconds = 0.0,
                                                .settings = makeSettings(scene_name)};
    const aster::SoftwareFrameBuffer framebuffer =
        aster::renderSoftwarePreview(scene, makeCamera(scene_name), options);
    if (output.extension() == ".png") {
      aster::writeFrameBufferPng(framebuffer, output, width, height);
    } else {
      framebuffer.writePpm(output, width, height);
    }

    std::cout << "Wrote preview: " << output << '\n';
  } catch (const std::exception &error) {
    std::cerr << "Preview render failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
