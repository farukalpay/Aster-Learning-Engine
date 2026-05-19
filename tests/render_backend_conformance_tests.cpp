// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/render/frame_capture.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/samples/showcase_scenes.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

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

struct CapturedImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};

struct ImageDiffMetrics {
  double mean_abs_error = 0.0;
  std::uint8_t max_abs_error = 0u;
  double differing_pixel_ratio = 0.0;
};

struct LabSceneCase {
  const char *name = "";
  aster::Scene (*make_scene)() = nullptr;
};

constexpr int kGoldenWidth = 96;
constexpr int kGoldenHeight = 64;
constexpr int kCaveConformanceWidth = 160;
constexpr int kCaveConformanceHeight = 90;

const LabSceneCase kLabScenes[] = {
    {"material_lab", aster::makeMaterialLabShowcaseScene},
    {"mesh_lab", aster::makeMeshLabShowcaseScene},
    {"lighting_lab", aster::makeLightingLabShowcaseScene},
    {"scene_lab", aster::makeSceneLabShowcaseScene},
};

std::filesystem::path goldenRoot() {
  return std::filesystem::path(ASTER_SOURCE_DIR) / "tests" / "golden" / "render";
}

std::filesystem::path goldenPath(const LabSceneCase &lab) {
  return goldenRoot() / (std::string(lab.name) + "_software.ppm");
}

std::filesystem::path artifactRoot() {
  return std::filesystem::temp_directory_path() / "aster_render_backend_conformance_artifacts";
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeTextFile(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << text;
  assert(output.good());
}

void writeKtx2Header(const std::filesystem::path &path, const std::uint32_t width,
                     const std::uint32_t height, const std::uint32_t mip_count) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.put(static_cast<char>(0xab));
  file.write("KTX 20", 6);
  file.put(static_cast<char>(0xbb));
  file.put('\r');
  file.put('\n');
  file.put(static_cast<char>(0x1a));
  file.put('\n');
  const auto write_le32 = [&file](const std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffu),
                           static_cast<char>((value >> 8u) & 0xffu),
                           static_cast<char>((value >> 16u) & 0xffu),
                           static_cast<char>((value >> 24u) & 0xffu)};
    file.write(bytes, 4);
  };
  write_le32(37u);
  write_le32(1u);
  write_le32(width);
  write_le32(height);
  write_le32(0u);
  write_le32(0u);
  write_le32(1u);
  write_le32(mip_count);
  write_le32(1u);
  file.write("ASTER_CAVE_CONFORMANCE_TEXTURE", 30);
  assert(file.good());
}

void writePpmFromRgba(const std::filesystem::path &path, const CapturedImage &image) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  assert(output.good());
  output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const std::size_t i =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
           static_cast<std::size_t>(x)) *
          4u;
      const char rgb[3] = {static_cast<char>(image.rgba[i + 0u]),
                           static_cast<char>(image.rgba[i + 1u]),
                           static_cast<char>(image.rgba[i + 2u])};
      output.write(rgb, sizeof(rgb));
    }
  }
}

CapturedImage parsePpm(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  std::string magic;
  int width = 0;
  int height = 0;
  int max_value = 0;
  input >> magic >> width >> height >> max_value;
  assert(magic == "P6");
  assert(width > 0 && height > 0 && max_value == 255);
  input.get();
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) *
                                static_cast<std::size_t>(height) * 3u);
  input.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  assert(input.gcount() == static_cast<std::streamsize>(rgb.size()));
  CapturedImage image{.width = width, .height = height};
  image.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  for (std::size_t pixel = 0; pixel < rgb.size() / 3u; ++pixel) {
    image.rgba[pixel * 4u + 0u] = rgb[pixel * 3u + 0u];
    image.rgba[pixel * 4u + 1u] = rgb[pixel * 3u + 1u];
    image.rgba[pixel * 4u + 2u] = rgb[pixel * 3u + 2u];
    image.rgba[pixel * 4u + 3u] = 255u;
  }
  return image;
}

ImageDiffMetrics diffImages(const CapturedImage &reference, const CapturedImage &candidate,
                            CapturedImage *out_diff = nullptr) {
  assert(reference.width == candidate.width);
  assert(reference.height == candidate.height);
  assert(reference.rgba.size() == candidate.rgba.size());
  ImageDiffMetrics metrics;
  std::size_t differing_pixels = 0u;
  const std::size_t pixel_count = reference.rgba.size() / 4u;
  if (out_diff != nullptr) {
    *out_diff = {.width = reference.width, .height = reference.height};
    out_diff->rgba.assign(reference.rgba.size(), 255u);
  }
  for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
    std::uint8_t max_pixel_error = 0u;
    for (std::size_t channel = 0; channel < 3u; ++channel) {
      const std::size_t i = pixel * 4u + channel;
      const int delta = std::abs(static_cast<int>(reference.rgba[i]) -
                                 static_cast<int>(candidate.rgba[i]));
      metrics.mean_abs_error += static_cast<double>(delta);
      metrics.max_abs_error =
          std::max(metrics.max_abs_error, static_cast<std::uint8_t>(std::min(delta, 255)));
      max_pixel_error =
          std::max(max_pixel_error, static_cast<std::uint8_t>(std::min(delta, 255)));
      if (out_diff != nullptr) {
        out_diff->rgba[pixel * 4u + channel] = static_cast<std::uint8_t>(std::min(delta * 4, 255));
      }
    }
    if (max_pixel_error > 2u) {
      ++differing_pixels;
    }
  }
  metrics.mean_abs_error /=
      static_cast<double>(std::max<std::size_t>(pixel_count * 3u, 1u));
  metrics.differing_pixel_ratio =
      static_cast<double>(differing_pixels) / static_cast<double>(std::max<std::size_t>(pixel_count, 1u));
  return metrics;
}

void writeDiffArtifacts(const std::filesystem::path &root, const std::string &label,
                        const CapturedImage &reference, const CapturedImage &candidate,
                        const ImageDiffMetrics &metrics) {
  CapturedImage diff;
  (void)diffImages(reference, candidate, &diff);
  writePpmFromRgba(root / (label + "_reference.ppm"), reference);
  writePpmFromRgba(root / (label + "_candidate.ppm"), candidate);
  writePpmFromRgba(root / (label + "_diff.ppm"), diff);
  std::ostringstream json;
  json << std::fixed << std::setprecision(6)
       << "{\n"
       << "  \"label\": \"" << label << "\",\n"
       << "  \"mean_abs_error\": " << metrics.mean_abs_error << ",\n"
       << "  \"max_abs_error\": " << static_cast<int>(metrics.max_abs_error) << ",\n"
       << "  \"differing_pixel_ratio\": " << metrics.differing_pixel_ratio << "\n"
       << "}\n";
  writeTextFile(root / (label + "_metrics.json"), json.str());
}

aster::OrbitCamera labCamera(const std::string &name) {
  aster::OrbitCamera camera;
  camera.vertical_fov = aster::radians(40.0f);
  camera.near_plane = 0.04f;
  camera.far_plane = 64.0f;
  if (name == "material_lab") {
    camera.target = {0.0f, 0.58f, 0.0f};
    camera.yaw = aster::radians(18.0f);
    camera.pitch = aster::radians(14.0f);
    camera.radius = 6.2f;
  } else if (name == "mesh_lab") {
    camera.target = {0.12f, 0.72f, 0.0f};
    camera.yaw = aster::radians(24.0f);
    camera.pitch = aster::radians(18.0f);
    camera.radius = 6.5f;
  } else if (name == "lighting_lab") {
    camera.target = {0.24f, 0.62f, 0.02f};
    camera.yaw = aster::radians(31.0f);
    camera.pitch = aster::radians(15.0f);
    camera.radius = 5.7f;
  } else {
    camera.target = {0.0f, 0.58f, 0.08f};
    camera.yaw = aster::radians(35.0f);
    camera.pitch = aster::radians(24.0f);
    camera.radius = 6.8f;
    camera.vertical_fov = aster::radians(44.0f);
  }
  return camera;
}

aster::RendererSettings makeContractSettings();

aster::RendererSettings labSettings(const std::string &name) {
  aster::RendererSettings settings = makeContractSettings();
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = true;
  settings.pipeline.clear_color = {0.024f, 0.027f, 0.031f};
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = true;
  settings.grounding.auto_contact_shadows = true;
  settings.grounding.reference_y = 0.0f;
  if (name == "lighting_lab") {
    settings.exposure = 0.92f;
    settings.ambient_strength = 0.14f;
    settings.sun_light.intensity = 1.70f;
    settings.atmosphere.enabled = true;
    settings.atmosphere.fog_color = {0.045f, 0.042f, 0.040f};
    settings.atmosphere.fog_start = 4.0f;
    settings.atmosphere.fog_end = 10.0f;
    settings.atmosphere.fog_strength = 0.28f;
  } else if (name == "scene_lab") {
    settings.exposure = 1.02f;
    settings.ambient_strength = 0.22f;
  } else {
    settings.exposure = 1.00f;
    settings.ambient_strength = 0.24f;
    settings.sun_light.intensity = 2.20f;
  }
  return settings;
}

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
  crystal.transform.rotation = aster::quatFromEulerXyz({0.0f, aster::radians(22.0f), 0.0f});
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
  settings.light_rig = {
      aster::Light{{-1.4f, 2.2f, 1.6f}, {4.5f, 2.6f, 1.4f}, 1.0f, 0.85f},
      aster::Light{{1.3f, 1.4f, -1.4f}, {0.8f, 1.7f, 2.4f}, 1.0f, 1.0f},
      aster::Light{{0.0f, 1.2f, 1.2f}, {1.8f, 1.1f, 0.6f}, 1.0f, 1.1f},
      aster::Light{{0.8f, 0.8f, -0.6f}, {1.8f, 0.7f, 0.2f}, 1.0f, 0.65f},
      aster::Light{{-0.6f, 0.9f, -1.8f}, {0.5f, 1.2f, 2.1f}, 1.0f, 0.70f},
      aster::Light{{1.9f, 1.7f, 0.3f}, {2.4f, 1.2f, 0.5f}, 1.0f, 0.95f},
  };
  settings.clustered_lighting.enabled = true;
  settings.clustered_lighting.cluster_count_x = 4u;
  settings.clustered_lighting.cluster_count_y = 3u;
  settings.clustered_lighting.cluster_count_z = 4u;
  settings.clustered_lighting.max_visible_lights = 16u;
  settings.clustered_lighting.max_lights_per_cluster = 6u;
  settings.light_policy.max_point_lights = settings.clustered_lighting.max_visible_lights;
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

std::filesystem::path caveMaterialRoot() {
  const std::filesystem::path root = artifactRoot() / "cave_conformance_materials";
  std::filesystem::create_directories(root);
  return root;
}

std::shared_ptr<aster::MaterialResourceLibrary> makeCaveConformanceMaterialLibrary() {
  const std::filesystem::path root = caveMaterialRoot();
  for (const char *name : {"cave_albedo.ktx2",   "cave_normal.ktx2",  "cave_orm.ktx2",
                           "cave_height.ktx2",   "cave_wetness.ktx2", "cave_opacity.ktx2",
                           "lamp_emissive.ktx2", "metal_rough.ktx2",  "metal_ao.ktx2"}) {
    writeKtx2Header(root / name, 32u, 32u, 4u);
  }

  aster::MaterialAsset wet_rock;
  wet_rock.id = "CaveConformanceWetRock";
  wet_rock.name = "Cave Conformance Wet Rock";
  wet_rock.surface_profile = aster::MaterialSurfaceProfile::StratifiedRock;
  wet_rock.receives_shadows = true;
  wet_rock.textures["albedo"] = {.role = "albedo", .uri = "cave_albedo.ktx2", .srgb = true};
  wet_rock.textures["normal"] = {.role = "normal", .uri = "cave_normal.ktx2"};
  wet_rock.textures["orm"] = {.role = "orm", .uri = "cave_orm.ktx2"};
  wet_rock.textures["height"] = {.role = "height", .uri = "cave_height.ktx2"};
  wet_rock.textures["wetness"] = {.role = "wetness", .uri = "cave_wetness.ktx2"};
  wet_rock.textures["opacity"] = {.role = "opacity", .uri = "cave_opacity.ktx2"};
  wet_rock.params["base_color_r"] = 0.19f;
  wet_rock.params["base_color_g"] = 0.17f;
  wet_rock.params["base_color_b"] = 0.145f;
  wet_rock.params["roughness"] = 0.86f;
  wet_rock.params["wetness_strength"] = 0.64f;
  wet_rock.params["micro_normal_strength"] = 0.56f;
  wet_rock.params["height_shading"] = 0.34f;
  wet_rock.explicit_features["normal_map"] = true;
  wet_rock.explicit_features["parallax"] = true;
  wet_rock.explicit_features["triplanar"] = true;

  aster::MaterialAsset lamp;
  lamp.id = "CaveConformanceEmissive";
  lamp.name = "Cave Conformance Emissive";
  lamp.shading_model = aster::MaterialShadingModel::Emissive;
  lamp.surface_profile = aster::MaterialSurfaceProfile::Resin;
  lamp.textures["emissive"] = {.role = "emissive", .uri = "lamp_emissive.ktx2"};
  lamp.params["base_color_r"] = 0.74f;
  lamp.params["base_color_g"] = 0.34f;
  lamp.params["base_color_b"] = 0.12f;
  lamp.params["emission_strength"] = 0.92f;

  aster::MaterialAsset metal;
  metal.id = "CaveConformanceWetMetal";
  metal.name = "Cave Conformance Wet Metal";
  metal.surface_profile = aster::MaterialSurfaceProfile::CorrodedMetal;
  metal.textures["albedo"] = {.role = "albedo", .uri = "cave_albedo.ktx2", .srgb = true};
  metal.textures["normal"] = {.role = "normal", .uri = "cave_normal.ktx2"};
  metal.textures["roughness"] = {.role = "roughness", .uri = "metal_rough.ktx2"};
  metal.textures["ao"] = {.role = "ao", .uri = "metal_ao.ktx2"};
  metal.textures["wetness"] = {.role = "wetness", .uri = "cave_wetness.ktx2"};
  metal.params["metallic"] = 0.76f;
  metal.params["roughness"] = 0.58f;

  auto library = std::make_shared<aster::MaterialResourceLibrary>();
  assert(library->addMaterialAsset(wet_rock, root, {.require_existing_files = true}));
  assert(library->addMaterialAsset(lamp, root, {.require_existing_files = true}));
  assert(library->addMaterialAsset(metal, root, {.require_existing_files = true}));
  return library;
}

aster::OrbitCamera makeCaveConformanceCamera() {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.92f, -0.92f};
  camera.yaw = aster::radians(28.0f);
  camera.pitch = aster::radians(13.0f);
  camera.radius = 4.9f;
  camera.vertical_fov = aster::radians(43.0f);
  camera.near_plane = 0.04f;
  camera.far_plane = 64.0f;
  return camera;
}

aster::RendererSettings makeCaveConformanceSettings() {
  aster::RendererSettings settings = makeContractSettings();
  settings.exposure = 0.88f;
  settings.ambient_strength = 0.15f;
  settings.ambient_floor = 0.020f;
  settings.pipeline.clear_color = {0.014f, 0.014f, 0.016f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.42f, 0.84f, 0.28f};
  settings.sun_light.color = {1.0f, 0.82f, 0.58f};
  settings.sun_light.intensity = 1.35f;
  settings.light_rig = {
      aster::Light{{-1.58f, 1.20f, -1.52f}, {5.2f, 2.0f, 0.62f}, 1.0f, 0.52f},
      aster::Light{{1.35f, 1.08f, -1.82f}, {4.4f, 1.7f, 0.54f}, 1.0f, 0.48f},
      aster::Light{{0.0f, 0.82f, 0.72f}, {0.62f, 1.05f, 1.45f}, 1.0f, 0.86f},
  };
  settings.light_policy.max_point_lights = 8u;
  settings.clustered_lighting.enabled = true;
  settings.clustered_lighting.cluster_count_x = 4u;
  settings.clustered_lighting.cluster_count_y = 3u;
  settings.clustered_lighting.cluster_count_z = 4u;
  settings.atmosphere.enabled = true;
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
  settings.shadows.max_distance = 14.0f;
  settings.reflections.enabled = true;
  settings.reflections.static_local_probes = true;
  settings.reflections.probe_resolution = 16u;
  settings.reflections.max_active_probes = 1u;
  settings.reflections.fallback_intensity = 0.86f;
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

struct LabRenderResult {
  aster::RenderBackendCapabilities backend{};
  aster::FrameStats stats{};
  aster::FrameForensics forensics{};
  ImageMetrics metrics{};
  CapturedImage image{};
};

LabRenderResult renderLabFrame(const LabSceneCase &lab, const bool force_software,
                               const std::filesystem::path &capture_path) {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", force_software);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);
  aster::RenderDevice renderer;
  renderer.initialize();
  aster::Scene scene = lab.make_scene();
  renderer.prepareScene(scene);
  const std::string name = lab.name;
  const aster::RendererSettings settings = labSettings(name);
  const aster::FrameStats stats =
      renderer.render(scene, labCamera(name), settings, kGoldenWidth, kGoldenHeight, 0.0);
  aster::writeFramebufferPpm(capture_path, kGoldenWidth, kGoldenHeight);
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  LabRenderResult result;
  result.backend = renderer.backendCapabilities();
  result.stats = stats;
  result.forensics = renderer.lastFrameForensics();
  result.metrics = metricsForActiveFrame(settings.pipeline.clear_color);
  result.image.width = framebuffer.width();
  result.image.height = framebuffer.height();
  result.image.rgba.assign(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
  return result;
}

LabRenderResult renderCaveConformanceFrameForBackend(const bool force_software,
                                                     const bool force_null,
                                                     const std::filesystem::path &capture_path) {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", force_software);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", force_null);
  auto library = makeCaveConformanceMaterialLibrary();
  aster::RenderDevice renderer;
  renderer.initialize();
  renderer.setMaterialResourceLibrary(library);
  aster::Scene scene = aster::makeCaveConformanceShowcaseScene();
  renderer.prepareScene(scene);
  const aster::RendererSettings settings = makeCaveConformanceSettings();
  const aster::FrameStats stats =
      renderer.render(scene, makeCaveConformanceCamera(), settings, kCaveConformanceWidth,
                      kCaveConformanceHeight, 0.0);
  aster::writeFramebufferPpm(capture_path, kCaveConformanceWidth, kCaveConformanceHeight);
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  LabRenderResult result;
  result.backend = renderer.backendCapabilities();
  result.stats = stats;
  result.forensics = renderer.lastFrameForensics();
  result.metrics = metricsForActiveFrame(settings.pipeline.clear_color);
  result.image.width = framebuffer.width();
  result.image.height = framebuffer.height();
  result.image.rgba.assign(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);
  return result;
}

LabRenderResult renderCaveConformanceFrame(const bool force_software,
                                           const std::filesystem::path &capture_path) {
  return renderCaveConformanceFrameForBackend(force_software, false, capture_path);
}

LabRenderResult renderCaveConformanceNullFrame(const std::filesystem::path &capture_path) {
  return renderCaveConformanceFrameForBackend(false, true, capture_path);
}

void assertRenderableFrame(const RenderResult &result) {
  assert(result.stats.framebuffer_width == 96);
  assert(result.stats.framebuffer_height == 64);
  assert(result.stats.visible_objects >= 4u);
  assert(result.stats.material_permutations >= 3u);
  assert(result.stats.material_variant_cache_misses >= 1u);
  assert(result.stats.active_point_lights > 4u);
  assert(result.stats.clustered_light_clusters == 48u);
  assert(result.stats.clustered_light_assignments >= result.stats.active_point_lights);
  assert(result.forensics.clustered_lights.visible_lights.size() >
         aster::kDefaultRenderLightBudget);
  assert(result.forensics.clustered_lights.cluster_offsets.size() == 49u);
  assert(!result.forensics.clustered_lights.light_indices.empty());
  assert(result.forensics.clustered_lights.visible_lights_hash != 0u);
  assert(result.forensics.clustered_lights.assignments_hash != 0u);
  assert(!result.forensics.clustered_lights.fallback_used);
  assert(!result.forensics.clustered_lights.overflowed);
  assert(!result.forensics.object_clusters.empty());
  if ((result.backend.graph_resource_mask &
       aster::renderGraphResourceBit(aster::RenderGraphResource::LightClusters)) != 0u) {
    assert(std::any_of(result.forensics.rhi_trace.descriptor_layouts.begin(),
                       result.forensics.rhi_trace.descriptor_layouts.end(),
                       [](const aster::rhi::DescriptorLayoutTrace &layout) {
                         return std::any_of(layout.ranges.begin(), layout.ranges.end(),
                                            [](const aster::rhi::DescriptorRangeDesc &range) {
                                              return range.kind ==
                                                     aster::rhi::DescriptorRangeKind::StorageBuffer;
                                            });
                       }));
  }
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
  if (native.backend.kind == aster::RenderBackendKind::Metal) {
    assert(native.backend.supports_ui_composite);
  }
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

void testBackendCapabilityTableContracts() {
  const RenderResult software = renderContractFrame(true);
  const aster::rhi::DeviceCapabilities &software_table = software.backend.capability_table;
  assert(software_table.backend == aster::rhi::BackendKind::SoftwareReference);
  assert(software_table.shader_model == aster::rhi::ShaderModel::SoftwareReference);
  assert(software_table.presentation == aster::rhi::PresentationMode::SoftwareFramebuffer);
  assert((software_table.color_format_mask &
          aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Bgra8Unorm)) != 0ull);
  assert((software_table.depth_format_mask &
          aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Depth32Float)) != 0ull);
  assert((software_table.sample_count_mask & aster::rhi::sampleCountCapabilityBit(1u)) != 0ull);
  assert((software_table.blend_mode_mask &
          aster::rhi::blendModeCapabilityBit(aster::rhi::BlendMode::AlphaBlend)) != 0ull);
  assert(software_table.max_sampled_textures_per_material >= 10u);
  assert(software_table.texture_sampling);
  assert(!software_table.storage_buffers);
  assert(software_table.shadow_maps);

  const RenderResult native = renderContractFrame(false);
  if (native.backend.kind == aster::RenderBackendKind::Metal) {
    assert(native.backend.capability_table.shader_model == aster::rhi::ShaderModel::MetalMSL23);
    assert(native.backend.capability_table.presentation == aster::rhi::PresentationMode::MetalLayer);
  }
  if (native.backend.kind == aster::RenderBackendKind::D3D12) {
    assert(native.backend.capability_table.shader_model ==
           aster::rhi::ShaderModel::D3D12ShaderModel51);
    assert(native.backend.capability_table.presentation ==
           aster::rhi::PresentationMode::D3D12OffscreenReadback);
  }
  if (native.backend.kind != aster::RenderBackendKind::Null) {
    assert(native.backend.capability_table.max_color_attachments == 1u);
    if (native.backend.kind == aster::RenderBackendKind::Metal ||
        native.backend.kind == aster::RenderBackendKind::D3D12) {
      assert(native.backend.capability_table.texture_sampling);
      assert(native.backend.supports_texture_sampling);
      assert(native.backend.capability_table.max_sampled_textures_per_material >= 10u);
      assert(native.backend.capability_table.max_samplers_per_material >= 1u);
    } else {
      assert(!native.backend.capability_table.texture_sampling);
    }
    if (native.backend.kind == aster::RenderBackendKind::Metal ||
        native.backend.kind == aster::RenderBackendKind::D3D12) {
      assert(native.backend.capability_table.storage_buffers);
      assert(native.backend.capability_table.texture_arrays);
      assert(native.backend.capability_table.shadow_maps ==
             (native.backend.kind == aster::RenderBackendKind::Metal));
    }
  }
}

void testGoldenLabScenes() {
  std::filesystem::create_directories(artifactRoot());
  for (const LabSceneCase &lab : kLabScenes) {
    const std::filesystem::path current_path =
        artifactRoot() / (std::string(lab.name) + "_software_current.ppm");
    const LabRenderResult result = renderLabFrame(lab, true, current_path);
    assert(result.backend.kind == aster::RenderBackendKind::SoftwareReference);
    assert(result.stats.visible_objects >= 2u);
    assert(result.metrics.foreground_ratio > 0.08);
    const std::filesystem::path expected_path = goldenPath(lab);
    if (!std::filesystem::exists(expected_path)) {
      std::cerr << "Missing golden baseline: " << expected_path << '\n';
      assert(false);
    }
    const std::vector<std::uint8_t> expected = readBytes(expected_path);
    const std::vector<std::uint8_t> current = readBytes(current_path);
    if (expected != current) {
      const CapturedImage reference = parsePpm(expected_path);
      const ImageDiffMetrics metrics = diffImages(reference, result.image);
      writeDiffArtifacts(artifactRoot(), std::string(lab.name) + "_software_golden_mismatch",
                         reference, result.image, metrics);
      std::cerr << "Software golden mismatch for " << lab.name
                << " mean_abs_error=" << metrics.mean_abs_error
                << " max_abs_error=" << static_cast<int>(metrics.max_abs_error) << '\n';
      assert(false);
    }
  }
}

const aster::FrameDebugCapture *captureFor(const aster::FrameForensics &forensics,
                                           const aster::RenderGraphResource resource) {
  const auto it = std::find_if(forensics.captures.begin(), forensics.captures.end(),
                               [resource](const aster::FrameDebugCapture &capture) {
                                 return capture.resource == resource && capture.available;
                               });
  return it == forensics.captures.end() ? nullptr : &*it;
}

bool hasCapabilityMismatchForProofResources(const aster::FrameForensics &forensics) {
  for (const aster::FrameDiagnosticEvent &event : forensics.events) {
    if (event.kind == aster::FrameDiagnosticKind::CapabilityMismatch &&
        (event.pass == "shadow-atlas" || event.pass == "volumetric-fog" ||
         event.pass == "reflection-probe")) {
      return true;
    }
  }
  return false;
}

void assertCaveProofCaptures(const LabRenderResult &result) {
  const aster::FrameDebugCapture *shadow =
      captureFor(result.forensics, aster::RenderGraphResource::ShadowAtlas);
  const aster::FrameDebugCapture *fog =
      captureFor(result.forensics, aster::RenderGraphResource::VolumetricFog);
  const aster::FrameDebugCapture *reflection =
      captureFor(result.forensics, aster::RenderGraphResource::ReflectionProbes);
  const aster::FrameDebugCapture *final =
      captureFor(result.forensics, aster::RenderGraphResource::CaptureReadback);
  assert(shadow != nullptr);
  assert(shadow->width == 128u && shadow->height == 128u && shadow->content_hash != 0u);
  assert(fog != nullptr);
  assert(fog->width == 40u && fog->height == 22u && fog->content_hash != 0u);
  assert(reflection != nullptr);
  assert(reflection->width == 96u && reflection->height == 16u &&
         reflection->content_hash != 0u);
  assert(final != nullptr);
  assert(final->width == kCaveConformanceWidth && final->height == kCaveConformanceHeight);
}

void testCaveConformanceSoftwareGolden() {
  std::filesystem::create_directories(artifactRoot());
  const std::filesystem::path first_path = artifactRoot() / "cave_conformance_software_current.ppm";
  const std::filesystem::path second_path = artifactRoot() / "cave_conformance_software_second.ppm";
  const LabRenderResult first = renderCaveConformanceFrame(true, first_path);
  const LabRenderResult second = renderCaveConformanceFrame(true, second_path);
  assert(first.backend.kind == aster::RenderBackendKind::SoftwareReference);
  assert(second.backend.kind == aster::RenderBackendKind::SoftwareReference);
  assert(first.stats.visible_objects >= 8u);
  assert(first.metrics.foreground_ratio > 0.18);
  assert(first.metrics.hash == second.metrics.hash);
  assertCaveProofCaptures(first);

  bool saw_albedo = false;
  bool saw_normal = false;
  bool saw_orm = false;
  bool saw_height = false;
  bool saw_wetness = false;
  bool saw_emissive = false;
  bool saw_opacity = false;
  for (const aster::MaterialBindingTrace &binding : first.forensics.material_bindings) {
    const bool authored = binding.valid && binding.bound && !binding.fallback;
    saw_albedo = saw_albedo || (binding.role == "albedo" && authored);
    saw_normal = saw_normal || (binding.role == "normal" && authored);
    saw_orm = saw_orm || (binding.role == "orm" && authored);
    saw_height = saw_height || (binding.role == "height" && authored);
    saw_wetness = saw_wetness || (binding.role == "wetness" && authored);
    saw_emissive = saw_emissive || (binding.role == "emissive" && authored);
    saw_opacity = saw_opacity || (binding.role == "opacity" && authored);
  }
  assert(saw_albedo && saw_normal && saw_orm && saw_height && saw_wetness && saw_emissive &&
         saw_opacity);

  const std::filesystem::path expected_path = goldenRoot() / "cave_conformance_software.ppm";
  if (!std::filesystem::exists(expected_path)) {
    std::cerr << "Missing golden baseline: " << expected_path << '\n';
    assert(false);
  }
  const std::vector<std::uint8_t> expected = readBytes(expected_path);
  const std::vector<std::uint8_t> current = readBytes(first_path);
  if (expected != current) {
    const CapturedImage reference = parsePpm(expected_path);
    const ImageDiffMetrics metrics = diffImages(reference, first.image);
    writeDiffArtifacts(artifactRoot(), "cave_conformance_software_golden_mismatch", reference,
                       first.image, metrics);
    std::cerr << "Software golden mismatch for cave_conformance"
              << " mean_abs_error=" << metrics.mean_abs_error
              << " max_abs_error=" << static_cast<int>(metrics.max_abs_error) << '\n';
    assert(false);
  }
}

void testCapabilityMismatchRequiresResourceMask() {
  const LabRenderResult result =
      renderCaveConformanceNullFrame(artifactRoot() / "cave_conformance_null.ppm");
  assert(result.backend.kind == aster::RenderBackendKind::Null);
  assert(hasCapabilityMismatchForProofResources(result.forensics));
  assert(captureFor(result.forensics, aster::RenderGraphResource::ShadowAtlas) == nullptr);
  assert(captureFor(result.forensics, aster::RenderGraphResource::VolumetricFog) == nullptr);
  assert(captureFor(result.forensics, aster::RenderGraphResource::ReflectionProbes) == nullptr);
}

void testNativeCaveConformanceWhenAvailable() {
  const LabRenderResult software =
      renderCaveConformanceFrame(true, artifactRoot() / "cave_conformance_software_reference.ppm");
  const LabRenderResult native =
      renderCaveConformanceFrame(false, artifactRoot() / "cave_conformance_native_candidate.ppm");
  if (native.backend.kind == aster::RenderBackendKind::Null ||
      native.backend.kind == aster::RenderBackendKind::SoftwareReference) {
    return;
  }
  assert(native.backend.supports_capture);
  assert(native.stats.visible_objects == software.stats.visible_objects);
  if (native.backend.kind == aster::RenderBackendKind::Metal) {
    assert(native.backend.capability_table.shadow_maps);
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::ShadowAtlas)) != 0u);
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::VolumetricFog)) != 0u);
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::ReflectionProbes)) != 0u);
    assert(!hasCapabilityMismatchForProofResources(native.forensics));
    assertCaveProofCaptures(native);
  }
#if defined(_WIN32)
  if (native.backend.kind == aster::RenderBackendKind::D3D12) {
    assert(native.backend.capability_table.presentation ==
           aster::rhi::PresentationMode::D3D12OffscreenReadback);
    if (!native.backend.capability_table.shadow_maps) {
      assert((native.backend.graph_resource_mask &
              aster::renderGraphResourceBit(aster::RenderGraphResource::ShadowAtlas)) == 0u);
      assert((native.backend.graph_resource_mask &
              aster::renderGraphResourceBit(aster::RenderGraphResource::VolumetricFog)) == 0u);
      assert((native.backend.graph_resource_mask &
              aster::renderGraphResourceBit(aster::RenderGraphResource::ReflectionProbes)) == 0u);
      assert(hasCapabilityMismatchForProofResources(native.forensics));
      return;
    }
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::ShadowAtlas)) != 0u);
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::VolumetricFog)) != 0u);
    assert((native.backend.graph_resource_mask &
            aster::renderGraphResourceBit(aster::RenderGraphResource::ReflectionProbes)) != 0u);
    assert(!hasCapabilityMismatchForProofResources(native.forensics));
    assertCaveProofCaptures(native);
  }
#endif
  const ImageDiffMetrics metrics = diffImages(software.image, native.image);
  if (metrics.mean_abs_error > 62.0 || metrics.differing_pixel_ratio > 0.88) {
    writeDiffArtifacts(artifactRoot(), "cave_conformance_native_reference_mismatch",
                       software.image, native.image, metrics);
    std::cerr << "Native/reference diff too high for cave_conformance"
              << " mean_abs_error=" << metrics.mean_abs_error
              << " differing_pixel_ratio=" << metrics.differing_pixel_ratio << '\n';
    assert(false);
  }
}

std::uint64_t firstMaterialPipelineKey(const aster::FrameForensics &forensics) {
  for (const aster::rhi::PipelineStateTrace &pipeline : forensics.rhi_trace.pipelines) {
    if (pipeline.label.rfind("material:", 0u) == 0u && pipeline.cache_key != 0u) {
      return pipeline.cache_key;
    }
  }
  return 0u;
}

LabRenderResult renderFeatureKeyProbe(const bool with_normal,
                                      const std::filesystem::path &capture_path) {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);
  const std::filesystem::path root = artifactRoot() / "pipeline_key_materials";
  std::filesystem::create_directories(root);
  writeKtx2Header(root / "feature_albedo.ktx2", 16u, 16u, 2u);
  writeKtx2Header(root / "feature_normal.ktx2", 16u, 16u, 2u);

  aster::MaterialAsset asset;
  asset.id = with_normal ? "FeatureNormalMaterial" : "FeatureFlatMaterial";
  asset.name = with_normal ? "Feature Normal Material" : "Feature Flat Material";
  asset.surface_profile = aster::MaterialSurfaceProfile::StratifiedRock;
  asset.textures["albedo"] = {.role = "albedo", .uri = "feature_albedo.ktx2", .srgb = true};
  if (with_normal) {
    asset.textures["normal"] = {.role = "normal", .uri = "feature_normal.ktx2"};
    asset.explicit_features["normal_map"] = true;
  }
  auto library = std::make_shared<aster::MaterialResourceLibrary>();
  assert(library->addMaterialAsset(asset, root, {.require_existing_files = true}));

  aster::RenderObject object;
  object.name = "feature pipeline key probe";
  object.primitive = aster::MeshPrimitive::Box;
  object.transform.position = {0.0f, 0.45f, 0.0f};
  object.transform.scale = {0.70f, 0.70f, 0.70f};
  object.material_asset_id = asset.id;
  object.material = aster::makeMaterial({.base_color = {0.46f, 0.36f, 0.28f},
                                         .roughness = 0.72f,
                                         .surface_profile =
                                             aster::MaterialSurfaceProfile::StratifiedRock});
  object.material.asset_id = asset.id;
  aster::Scene scene;
  scene.objects().push_back(object);

  aster::RenderDevice renderer;
  renderer.initialize();
  renderer.setMaterialResourceLibrary(library);
  renderer.prepareScene(scene);
  aster::RendererSettings settings = makeContractSettings();
  settings.shadows.enabled = true;
  settings.shadows.atlas_size = 64u;
  settings.atmosphere.enabled = true;
  settings.reflections.enabled = false;
  const aster::FrameStats stats =
      renderer.render(scene, makeContractCamera(), settings, 80, 56, 0.0);
  aster::writeFramebufferPpm(capture_path, 80, 56);
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  LabRenderResult result;
  result.backend = renderer.backendCapabilities();
  result.stats = stats;
  result.forensics = renderer.lastFrameForensics();
  result.metrics = metricsForActiveFrame(settings.pipeline.clear_color);
  result.image.width = framebuffer.width();
  result.image.height = framebuffer.height();
  result.image.rgba.assign(framebuffer.rgba8().begin(), framebuffer.rgba8().end());
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
  return result;
}

void testNativeMaterialPipelineKeyTracksFeatureBits() {
  const LabRenderResult flat =
      renderFeatureKeyProbe(false, artifactRoot() / "feature_key_flat.ppm");
  const LabRenderResult normal =
      renderFeatureKeyProbe(true, artifactRoot() / "feature_key_normal.ppm");
  if (flat.backend.kind == aster::RenderBackendKind::Null ||
      flat.backend.kind == aster::RenderBackendKind::SoftwareReference ||
      normal.backend.kind == aster::RenderBackendKind::Null ||
      normal.backend.kind == aster::RenderBackendKind::SoftwareReference) {
    return;
  }
  const std::uint64_t flat_key = firstMaterialPipelineKey(flat.forensics);
  const std::uint64_t normal_key = firstMaterialPipelineKey(normal.forensics);
  assert(flat_key != 0u);
  assert(normal_key != 0u);
  assert(flat_key != normal_key);
}

void testNativeLabScenesMatchSoftwareReferenceWhenAvailable() {
  for (const LabSceneCase &lab : kLabScenes) {
    const LabRenderResult software = renderLabFrame(
        lab, true, artifactRoot() / (std::string(lab.name) + "_software_reference.ppm"));
    const LabRenderResult native = renderLabFrame(
        lab, false, artifactRoot() / (std::string(lab.name) + "_native_candidate.ppm"));
    if (native.backend.kind == aster::RenderBackendKind::Null ||
        native.backend.kind == aster::RenderBackendKind::SoftwareReference) {
      continue;
    }
    assert(native.stats.visible_objects == software.stats.visible_objects);
    assert(native.stats.draw_calls > 0u);
    assert(!native.forensics.passes.empty());
    const ImageDiffMetrics metrics = diffImages(software.image, native.image);
    if (metrics.mean_abs_error > 52.0 || metrics.differing_pixel_ratio > 0.82) {
      writeDiffArtifacts(artifactRoot(), std::string(lab.name) + "_native_reference_mismatch",
                         software.image, native.image, metrics);
      std::cerr << "Native/reference diff too high for " << lab.name
                << " mean_abs_error=" << metrics.mean_abs_error
                << " differing_pixel_ratio=" << metrics.differing_pixel_ratio << '\n';
      assert(false);
    }
  }
}

void testFailureArtifactWriter() {
  CapturedImage reference{.width = 2, .height = 2, .rgba = {0, 0, 0, 255, 32, 32, 32, 255,
                                                            64, 64, 64, 255, 255, 255, 255, 255}};
  CapturedImage candidate = reference;
  candidate.rgba[4u] = 200u;
  candidate.rgba[5u] = 20u;
  const ImageDiffMetrics metrics = diffImages(reference, candidate);
  const std::filesystem::path root = artifactRoot() / "artifact_writer";
  std::filesystem::remove_all(root);
  writeDiffArtifacts(root, "synthetic", reference, candidate, metrics);
  assert(std::filesystem::exists(root / "synthetic_reference.ppm"));
  assert(std::filesystem::exists(root / "synthetic_candidate.ppm"));
  assert(std::filesystem::exists(root / "synthetic_diff.ppm"));
  assert(std::filesystem::exists(root / "synthetic_metrics.json"));
}

void writeGoldenBaselines() {
  std::filesystem::create_directories(goldenRoot());
  for (const LabSceneCase &lab : kLabScenes) {
    const LabRenderResult result = renderLabFrame(lab, true, goldenPath(lab));
    assert(result.backend.kind == aster::RenderBackendKind::SoftwareReference);
    assert(result.metrics.foreground_ratio > 0.08);
    std::cout << "wrote " << goldenPath(lab) << '\n';
  }
  const std::filesystem::path cave_path = goldenRoot() / "cave_conformance_software.ppm";
  const LabRenderResult cave = renderCaveConformanceFrame(true, cave_path);
  assert(cave.backend.kind == aster::RenderBackendKind::SoftwareReference);
  assert(cave.metrics.foreground_ratio > 0.18);
  std::cout << "wrote " << cave_path << '\n';
}

struct TestCase {
  const char *name = "";
  void (*run)() = nullptr;
};

constexpr TestCase kTestCases[] = {
    {"software_deterministic_hash_and_forensics", testSoftwareDeterministicHashAndForensics},
    {"native_backend_conforms_when_available", testNativeBackendConformsWhenAvailable},
    {"backend_capability_table_contracts", testBackendCapabilityTableContracts},
    {"golden_lab_scenes", testGoldenLabScenes},
    {"cave_conformance_software_golden", testCaveConformanceSoftwareGolden},
    {"resource_capability_mismatch_requires_resource_mask",
     testCapabilityMismatchRequiresResourceMask},
    {"native_cave_conformance_when_available", testNativeCaveConformanceWhenAvailable},
    {"native_material_pipeline_key_tracks_feature_bits",
     testNativeMaterialPipelineKeyTracksFeatureBits},
    {"native_lab_scenes_match_software_reference_when_available",
     testNativeLabScenesMatchSoftwareReferenceWhenAvailable},
    {"failure_artifact_writer", testFailureArtifactWriter},
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
    if (std::strcmp(argv[1], "--write-golden") == 0) {
      writeGoldenBaselines();
      return 0;
    }
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
