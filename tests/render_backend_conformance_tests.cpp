// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_framebuffer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

void setEnvFlag(const char *name, const bool enabled) {
#if defined(_WIN32)
  (void)_putenv_s(name, enabled ? "1" : "");
#else
  if (enabled) {
    (void)setenv(name, "1", 1);
  } else {
    (void)unsetenv(name);
  }
#endif
}

std::uint64_t hashBytes(const std::span<const std::uint8_t> bytes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const std::uint8_t byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

struct ImageMetrics {
  std::uint64_t hash = 0u;
  double mean_luma = 0.0;
  double foreground_ratio = 0.0;
  double alpha_ratio = 0.0;
};

ImageMetrics metricsForActiveFrame(const aster::Vec3 clear_color) {
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  assert(!framebuffer.empty());
  ImageMetrics metrics;
  metrics.hash = hashBytes(framebuffer.rgba8());
  const double clear_luma = static_cast<double>(clear_color.x) * 0.2126 +
                            static_cast<double>(clear_color.y) * 0.7152 +
                            static_cast<double>(clear_color.z) * 0.0722;
  std::size_t foreground = 0u;
  std::size_t alpha = 0u;
  std::size_t pixels = 0u;
  for (std::size_t i = 0; i + 3u < framebuffer.rgba8().size(); i += 4u) {
    const double r = static_cast<double>(framebuffer.rgba8()[i + 0u]) / 255.0;
    const double g = static_cast<double>(framebuffer.rgba8()[i + 1u]) / 255.0;
    const double b = static_cast<double>(framebuffer.rgba8()[i + 2u]) / 255.0;
    const double a = static_cast<double>(framebuffer.rgba8()[i + 3u]) / 255.0;
    const double luma = r * 0.2126 + g * 0.7152 + b * 0.0722;
    metrics.mean_luma += luma;
    foreground += std::abs(luma - clear_luma) > 0.025 ? 1u : 0u;
    alpha += a > 0.01 ? 1u : 0u;
    ++pixels;
  }
  metrics.mean_luma /= static_cast<double>(std::max<std::size_t>(pixels, 1u));
  metrics.foreground_ratio = static_cast<double>(foreground) /
                             static_cast<double>(std::max<std::size_t>(pixels, 1u));
  metrics.alpha_ratio =
      static_cast<double>(alpha) / static_cast<double>(std::max<std::size_t>(pixels, 1u));
  return metrics;
}

aster::Material caveMaterial() {
  aster::Material material = aster::makeMaterial({.base_color = {0.26f, 0.23f, 0.20f},
                                                  .roughness = 0.86f,
                                                  .detail_strength = 0.42f,
                                                  .detail_scale = 3.1f,
                                                  .edge_wear = 0.18f,
                                                  .ambient_occlusion = 0.88f,
                                                  .surface_pattern = aster::SurfacePattern::CaveRock,
                                                  .pattern_scale = {1.7f, 2.4f},
                                                  .pattern_depth = 0.20f,
                                                  .pattern_contrast = 0.56f,
                                                  .procedural = {.macro_variation = 0.42f,
                                                                 .micro_normal_strength = 0.34f,
                                                                 .roughness_variation = 0.24f,
                                                                 .wetness = 0.22f,
                                                                 .height_shading = 0.18f}});
  material.asset_id = "conformance.cave-rock";
  material.shader_variant_key = 0xA57E0001u;
  return material;
}

aster::Scene makeContractScene() {
  aster::Scene scene;
  aster::Material ground =
      aster::makeSupportSurfaceMaterial(aster::makeMaterial({.base_color = {0.18f, 0.19f, 0.18f},
                                                             .roughness = 0.9f,
                                                             .surface_pattern =
                                                                 aster::SurfacePattern::CourseCells,
                                                             .pattern_scale = {1.5f, 1.5f},
                                                             .pattern_depth = 0.08f,
                                                             .pattern_contrast = 0.24f}));
  ground.asset_id = "conformance.support";

  aster::RenderObject floor;
  floor.name = "contract floor";
  floor.primitive = aster::MeshPrimitive::Plane;
  floor.transform.scale = {2.0f, 1.0f, 2.0f};
  floor.material = ground;
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  for (int i = 0; i < 2; ++i) {
    aster::RenderObject block;
    block.name = "instanced cave block";
    block.primitive = aster::MeshPrimitive::Box;
    block.transform.position = {i == 0 ? -0.78f : 0.78f, 0.36f, 0.0f};
    block.transform.scale = {0.58f, 0.72f, 0.58f};
    block.material = caveMaterial();
    block.casts_contact_shadow = true;
    block.contact_shadow_strength = 0.70f;
    scene.objects().push_back(block);
  }

  aster::RenderObject dynamic_rock;
  dynamic_rock.name = "dynamic cave rock";
  dynamic_rock.custom_mesh = std::make_shared<const aster::CpuMesh>(aster::makeRock(18, 12, 1.0f));
  dynamic_rock.dynamic_mesh = {.id = 1001u, .generation = 3u};
  dynamic_rock.transform.position = {0.0f, 0.62f, -0.92f};
  dynamic_rock.transform.scale = {0.54f, 0.42f, 0.48f};
  dynamic_rock.material = caveMaterial();
  dynamic_rock.material.asset_id = "conformance.dynamic-rock";
  dynamic_rock.casts_contact_shadow = true;
  scene.objects().push_back(dynamic_rock);

  aster::Material translucent = aster::makeMaterial({.base_color = {0.18f, 0.48f, 0.72f},
                                                     .emission_color = {0.02f, 0.10f, 0.15f},
                                                     .roughness = 0.18f,
                                                     .emission_strength = 0.10f,
                                                     .opacity = 0.45f,
                                                     .alpha_mode = aster::MaterialAlphaMode::Blend,
                                                     .depth_write =
                                                         aster::MaterialDepthWrite::Disabled});
  translucent.asset_id = "conformance.translucent";
  aster::RenderObject orb;
  orb.name = "translucent orb";
  orb.primitive = aster::MeshPrimitive::Sphere;
  orb.transform.position = {0.0f, 0.96f, 0.58f};
  orb.transform.scale = {0.38f, 0.38f, 0.38f};
  orb.material = translucent;
  orb.casts_contact_shadow = false;
  scene.objects().push_back(orb);

  aster::Material emissive = aster::makeMaterial({.base_color = {0.32f, 0.20f, 0.08f},
                                                  .emission_color = {1.0f, 0.42f, 0.12f},
                                                  .roughness = 0.36f,
                                                  .emission_strength = 0.42f,
                                                  .surface_pattern =
                                                      aster::SurfacePattern::AmberResin,
                                                  .pattern_depth = 0.12f,
                                                  .pattern_contrast = 0.48f});
  emissive.asset_id = "conformance.emissive";
  aster::RenderObject crystal;
  crystal.name = "emissive crystal";
  crystal.primitive = aster::MeshPrimitive::Crystal;
  crystal.transform.position = {0.88f, 0.72f, -0.68f};
  crystal.transform.rotation = {0.0f, aster::radians(22.0f), 0.0f};
  crystal.transform.scale = {0.32f, 0.42f, 0.32f};
  crystal.material = emissive;
  crystal.casts_contact_shadow = true;
  scene.objects().push_back(crystal);

  return scene;
}

aster::OrbitCamera makeContractCamera() {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.55f, -0.10f};
  camera.yaw = aster::radians(35.0f);
  camera.pitch = aster::radians(17.0f);
  camera.radius = 4.1f;
  camera.vertical_fov = aster::radians(43.0f);
  camera.near_plane = 0.04f;
  camera.far_plane = 48.0f;
  return camera;
}

aster::RendererSettings makeContractSettings() {
  aster::RendererSettings settings;
  settings.exposure = 0.92f;
  settings.ambient_strength = 0.22f;
  settings.ambient_floor = 0.018f;
  settings.procedural_surface_normals = true;
  settings.sky_ambient_color = {0.20f, 0.22f, 0.25f};
  settings.ground_ambient_color = {0.10f, 0.085f, 0.070f};
  settings.pipeline.clear_color = {0.020f, 0.023f, 0.027f};
  settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.44f, 0.80f, 0.32f};
  settings.sun_light.color = {1.0f, 0.78f, 0.52f};
  settings.sun_light.intensity = 1.30f;
  settings.light_rig = {{
      aster::Light{{-1.4f, 2.2f, 1.6f}, {4.5f, 2.6f, 1.4f}, 1.0f, 0.85f},
      aster::Light{{1.3f, 1.4f, -1.4f}, {0.8f, 1.7f, 2.4f}, 1.0f, 1.0f},
      aster::Light{{0.0f, 1.2f, 1.2f}, {1.8f, 1.1f, 0.6f}, 1.0f, 1.1f},
      aster::Light{{0.8f, 0.8f, -0.6f}, {1.8f, 0.7f, 0.2f}, 1.0f, 0.65f},
  }};
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = true;
  settings.grounding.auto_contact_shadows = true;
  settings.grounding.reference_y = 0.0f;
  settings.grounding.contact_shadow_strength = 0.34f;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.035f, 0.034f, 0.032f};
  settings.atmosphere.fog_start = 2.2f;
  settings.atmosphere.fog_end = 7.2f;
  settings.atmosphere.fog_strength = 0.24f;
  settings.atmosphere.saturation = 0.82f;
  settings.atmosphere.contrast = 1.12f;
  return settings;
}

struct RenderResult {
  aster::RenderBackendCapabilities backend{};
  aster::FrameStats stats{};
  aster::FrameForensics forensics{};
  ImageMetrics metrics{};
};

RenderResult renderContractFrame(const bool force_software) {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", force_software);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);
  aster::RenderDevice renderer;
  renderer.initialize();
  aster::Scene scene = makeContractScene();
  renderer.prepareScene(scene);
  const aster::RendererSettings settings = makeContractSettings();
  const aster::FrameStats stats =
      renderer.render(scene, makeContractCamera(), settings, 96, 64, 0.0);
  const std::filesystem::path capture_path =
      std::filesystem::temp_directory_path() /
      (force_software ? "aster_conformance_software.ppm" : "aster_conformance_native.ppm");
  aster::writeFramebufferPpm(capture_path, 96, 64);
  std::filesystem::remove(capture_path);
  RenderResult result;
  result.backend = renderer.backendCapabilities();
  result.stats = stats;
  result.forensics = renderer.lastFrameForensics();
  result.metrics = metricsForActiveFrame(settings.pipeline.clear_color);
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
  return result;
}

void assertRenderableFrame(const RenderResult &result) {
  assert(result.stats.framebuffer_width == 96);
  assert(result.stats.framebuffer_height == 64);
  assert(result.stats.visible_objects >= 4u);
  assert(result.stats.material_permutations >= 3u);
  assert(result.stats.material_variant_cache_misses >= 1u);
  assert(!result.forensics.passes.empty());
  assert(result.metrics.hash != 0u);
  assert(result.metrics.alpha_ratio > 0.90);
  assert(result.metrics.foreground_ratio > 0.10);
}

void testSoftwareDeterministicHashAndForensics() {
  const RenderResult first = renderContractFrame(true);
  const RenderResult second = renderContractFrame(true);
  assert(first.backend.kind == aster::RenderBackendKind::SoftwareReference);
  assert(!first.backend.gpu);
  assert(first.backend.supports_shader_materials);
  assertRenderableFrame(first);
  assertRenderableFrame(second);
  assert(first.metrics.hash == second.metrics.hash);
  assert(std::abs(first.metrics.mean_luma - second.metrics.mean_luma) < 0.00001);
}

void testNativeBackendConformsWhenAvailable() {
  const RenderResult software = renderContractFrame(true);
  const RenderResult native = renderContractFrame(false);
  assertRenderableFrame(software);
  if (native.backend.kind == aster::RenderBackendKind::Null ||
      native.backend.kind == aster::RenderBackendKind::SoftwareReference) {
    return;
  }
  assertRenderableFrame(native);
  assert(native.backend.supports_capture);
  assert(native.backend.supports_ui_composite);
  assert(native.stats.draw_calls > 0u);
  assert(std::abs(native.metrics.mean_luma - software.metrics.mean_luma) < 0.35);
  assert(std::abs(native.metrics.foreground_ratio - software.metrics.foreground_ratio) < 0.55);
#if defined(_WIN32)
  assert(native.backend.kind == aster::RenderBackendKind::D3D12);
  assert(native.backend.supports_shader_materials);
  assert(native.backend.supports_instancing);
  assert(native.stats.material_permutations >= 3u);
#endif
}

struct TestCase {
  const char *name = "";
  void (*run)() = nullptr;
};

constexpr TestCase kTestCases[] = {
    {"software_deterministic_hash_and_forensics", testSoftwareDeterministicHashAndForensics},
    {"native_backend_conforms_when_available", testNativeBackendConformsWhenAvailable},
};

int runTestCase(const TestCase &test_case) {
  std::cout << "render_backend_conformance_tests: " << test_case.name << '\n';
  test_case.run();
  std::cout << "render_backend_conformance_tests: " << test_case.name << " passed.\n";
  return 0;
}

} // namespace

int main(const int argc, const char **argv) {
  if (argc > 1) {
    for (const TestCase &test_case : kTestCases) {
      if (std::strcmp(argv[1], test_case.name) == 0) {
        return runTestCase(test_case);
      }
    }
    std::cerr << "Unknown render_backend_conformance_tests case: " << argv[1] << '\n';
    return 1;
  }
  for (const TestCase &test_case : kTestCases) {
    runTestCase(test_case);
  }
  std::cout << "render_backend_conformance_tests passed.\n";
  return 0;
}
