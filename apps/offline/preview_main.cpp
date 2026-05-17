// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_preview_renderer.hpp"
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
  settings.light_rig = {{
      aster::Light{{-3.6f, 3.8f, 2.6f}, {16.0f, 13.5f, 10.0f}, 1.0f, 0.7f},
      aster::Light{{3.6f, 2.2f, 1.8f}, {5.0f, 6.8f, 9.5f}, 1.0f, 0.9f},
      aster::Light{{0.8f, 3.4f, -3.4f}, {7.5f, 6.4f, 5.0f}, 1.0f, 1.1f},
      aster::Light{{0.0f, 1.0f, 3.4f}, {1.8f, 2.1f, 2.5f}, 1.0f, 1.2f},
  }};

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
    framebuffer.writePpm(output, width, height);

    std::cout << "Wrote preview: " << output << '\n';
  } catch (const std::exception &error) {
    std::cerr << "Preview render failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
