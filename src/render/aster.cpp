// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/aster.hpp"

#include "aster/asset/pipe_runtime_asset.hpp"
#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/core/config.hpp"
#include "aster/material/material_asset.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/texture/runtime_texture.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

struct AsterSimpleContext {
  explicit AsterSimpleContext(AsterAppConfig requested_config) : config(std::move(requested_config)) {}

  AsterAppConfig config;
  std::unique_ptr<Window> window;
  RenderDevice renderer;
  std::shared_ptr<MaterialResourceLibrary> material_library =
      std::make_shared<MaterialResourceLibrary>();
  Scene scene;
  OrbitCamera camera;
  RendererSettings settings;
  std::vector<Light> frame_lights;
  FrameStats last_stats;
  int frames_started = 0;
  bool frame_open = false;
  bool scene_open = false;
};

std::unique_ptr<AsterSimpleContext> g_context;
const FrameStats kEmptyFrameStats{};
const FrameForensics kEmptyFrameForensics{};

std::string lowerText(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::replace(value.begin(), value.end(), '_', '-');
  return value;
}

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not open Aster file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string primitiveFromAsterGraphSource(const std::string &source) {
  std::istringstream lines(source);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream tokens(line);
    std::string token;
    if (!(tokens >> token) || token == "#") {
      continue;
    }
    if (token == "primitive") {
      std::string primitive;
      if (tokens >> primitive) {
        return primitive;
      }
    }
    while (tokens >> token) {
      constexpr std::string_view kPrimitivePrefix = "primitive=";
      if (token.rfind(kPrimitivePrefix, 0u) == 0u) {
        return token.substr(kPrimitivePrefix.size());
      }
    }
  }
  return {};
}

RendererSettings makeSimpleRendererSettings() {
  RendererSettings settings;
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = true;
  settings.exposure = 1.02f;
  settings.ambient_strength = 0.22f;
  settings.ambient_floor = 0.014f;
  settings.indirect_albedo_floor = 0.018f;
  settings.pipeline.clear_color = {0.030f, 0.034f, 0.040f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.52f, 0.80f, 0.30f};
  settings.sun_light.color = {1.0f, 0.88f, 0.72f};
  settings.sun_light.intensity = 2.4f;
  settings.grounding.enabled = true;
  settings.grounding.contact_shadows = true;
  settings.grounding.auto_contact_shadows = true;
  settings.grounding.contact_shadow_strength = 0.54f;
  settings.occlusion.enabled = true;
  settings.occlusion.radius = 1.05f;
  settings.occlusion.strength = 0.32f;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.060f, 0.068f, 0.078f};
  settings.atmosphere.fog_start = 9.0f;
  settings.atmosphere.fog_end = 22.0f;
  settings.atmosphere.fog_strength = 0.08f;
  return settings;
}

void requireContext(const char *operation) {
  if (!g_context) {
    throw std::runtime_error(std::string(operation) + " requires InitAster first.");
  }
}

Mesh meshFromPrimitiveName(std::string name) {
  const std::string normalized = lowerText(name);
  Mesh mesh;
  mesh.name = std::move(name);

  if (normalized == "box" || normalized == "cube") {
    mesh.primitive = MeshPrimitive::Box;
    return mesh;
  }
  if (normalized == "plane" || normalized == "ground") {
    mesh.primitive = MeshPrimitive::Plane;
    return mesh;
  }
  if (normalized == "rock") {
    mesh.primitive = MeshPrimitive::Rock;
    return mesh;
  }
  if (normalized == "crystal") {
    mesh.primitive = MeshPrimitive::Crystal;
    return mesh;
  }
  if (normalized == "pillar") {
    mesh.primitive = MeshPrimitive::Pillar;
    return mesh;
  }
  if (normalized == "pipe" || normalized == "rusted-pipe" ||
      normalized == "industrial-pipe" || normalized == "production-rusted-pipe") {
    mesh.name = normalized;
    mesh.custom_mesh = std::make_shared<const CpuMesh>(makeAsterPipeRenderMesh({}));
    return mesh;
  }

  mesh.primitive = MeshPrimitive::Sphere;
  return mesh;
}

Material rustMaterial() {
  Material material = makeMaterial({.base_color = LinearRgb{0.34f, 0.14f, 0.055f},
                                    .roughness = 0.76f,
                                    .metallic = 0.48f,
                                    .detail_strength = 0.86f,
                                    .detail_scale = 8.0f,
                                    .edge_wear = 0.20f,
                                    .surface_profile = MaterialSurfaceProfile::CorrodedMetal,
                                    .surface_pattern = SurfacePattern::WeatheredMetal,
                                    .procedural = {.macro_variation = 0.82f,
                                                   .micro_normal_strength = 0.44f,
                                                   .roughness_variation = 0.66f,
                                                   .height_shading = 0.28f,
                                                   .pitting_density = 0.86f,
                                                   .oxide_layering = 0.76f,
                                                   .cavity_grime = 0.72f,
                                                   .axial_scratches = 0.58f,
                                                   .rust_bloom = 0.82f,
                                                   .black_scab = 0.64f}});
  material.asset_id = "aster.simple.rust";
  return material;
}

Material fallbackMaterialForName(const std::filesystem::path &path_or_name) {
  const std::string normalized = lowerText(path_or_name.stem().empty()
                                               ? path_or_name.string()
                                               : path_or_name.stem().string());
  if (normalized.find("rust") != std::string::npos ||
      normalized.find("weathered-metal") != std::string::npos ||
      normalized.find("pipe") != std::string::npos) {
    return rustMaterial();
  }
  Material material = makeMaterial({.base_color = LinearRgb{0.78f, 0.78f, 0.72f},
                                    .roughness = 0.58f,
                                    .surface_profile = MaterialSurfaceProfile::Plain});
  material.asset_id = "aster.simple.default";
  return material;
}

} // namespace

void InitAster(const int width, const int height, const std::string_view title) {
  AsterAppConfig config;
  config.width = width;
  config.height = height;
  config.title = std::string(title);
  InitAster(config);
}

void InitAster(const AsterAppConfig &config) {
  if (config.width <= 0 || config.height <= 0) {
    throw std::invalid_argument("InitAster requires a positive width and height.");
  }

  AsterAppConfig resolved = config;
  if (resolved.title.empty()) {
    resolved.title = "Aster";
  }
  if (resolved.headless && resolved.max_frames == 0) {
    resolved.max_frames = 1;
  }

  auto context = std::make_unique<AsterSimpleContext>(resolved);
  context->settings = makeSimpleRendererSettings();
  if (!resolved.headless) {
    EngineConfig window_config;
    window_config.application_name = context->config.title.c_str();
    window_config.initial_width = resolved.width;
    window_config.initial_height = resolved.height;
    window_config.enable_vsync = resolved.vsync;
    context->window = std::make_unique<Window>(window_config);
  }
  context->renderer.initialize();
  context->renderer.setMaterialResourceLibrary(context->material_library);
  g_context = std::move(context);
}

void CloseAster() {
  g_context.reset();
}

bool Frame() {
  requireContext("Frame");
  if (g_context->frame_open) {
    return true;
  }
  if (g_context->config.max_frames > 0 &&
      g_context->frames_started >= g_context->config.max_frames) {
    return false;
  }
  if (g_context->window) {
    g_context->window->pollEvents();
    if (!g_context->window->isOpen()) {
      return false;
    }
  }

  g_context->scene = Scene{};
  g_context->frame_lights.clear();
  g_context->frame_open = true;
  ++g_context->frames_started;
  return true;
}

void BeginScene(const Camera3D &camera) {
  BeginScene(camera.orbit);
}

void BeginScene(const OrbitCamera &camera) {
  requireContext("BeginScene");
  g_context->camera = camera;
  g_context->scene = Scene{};
  g_context->frame_lights.clear();
  g_context->scene_open = true;
}

void DrawMesh(const Mesh &mesh, const Material &material, const Transform &transform) {
  requireContext("DrawMesh");
  RenderObject object;
  object.name = mesh.name.empty() ? "mesh" : mesh.name;
  object.primitive = mesh.primitive;
  object.custom_mesh = mesh.custom_mesh;
  object.transform = transform;
  object.material = material;
  object.material_asset_id = material.asset_id;
  object.casts_contact_shadow = true;
  g_context->scene.objects().push_back(std::move(object));
}

void DrawLight(const Vec3 position, const Vec3 color, const float intensity,
               const float source_radius) {
  requireContext("DrawLight");
  g_context->frame_lights.push_back(
      Light{.position = position,
            .color = color,
            .intensity = intensity,
            .source_radius = source_radius,
            .range = 0.0f,
            .casts_shadow = true});
}

void EndScene() {
  requireContext("EndScene");
  RendererSettings settings = g_context->settings;
  if (!g_context->frame_lights.empty()) {
    settings.light_rig = g_context->frame_lights;
    settings.light_policy.max_point_lights =
        std::min<std::size_t>(g_context->frame_lights.size(), kRenderLightUniformCapacity);
  }

  int framebuffer_width = g_context->config.width;
  int framebuffer_height = g_context->config.height;
  if (g_context->window) {
    const auto [width, height] = g_context->window->framebufferSize();
    framebuffer_width = width;
    framebuffer_height = height;
  }

  g_context->renderer.prepareScene(g_context->scene);
  g_context->last_stats =
      g_context->renderer.render(g_context->scene, g_context->camera, settings, framebuffer_width,
                                 framebuffer_height, static_cast<double>(g_context->frames_started));
  if (g_context->window) {
    g_context->window->swapBuffers();
  }
  g_context->frame_open = false;
  g_context->scene_open = false;
}

void CaptureFrame(const std::filesystem::path &path) {
  requireContext("CaptureFrame");
  if (path.empty()) {
    throw std::invalid_argument("CaptureFrame requires an output path.");
  }
  const int width =
      g_context->last_stats.framebuffer_width > 0 ? g_context->last_stats.framebuffer_width
                                                  : g_context->config.width;
  const int height =
      g_context->last_stats.framebuffer_height > 0 ? g_context->last_stats.framebuffer_height
                                                   : g_context->config.height;
  const std::string extension = lowerText(path.extension().string());
  if (extension == ".png") {
    writeFramebufferPng(path, width, height);
  } else {
    writeFramebufferPpm(path, width, height);
  }
}

Camera3D MakeOrbitCamera(const Vec3 target, const float radius, const float yaw_degrees,
                         const float pitch_degrees) {
  Camera3D camera;
  camera.orbit.target = target;
  camera.orbit.radius = radius;
  camera.orbit.yaw = radians(yaw_degrees);
  camera.orbit.pitch = radians(pitch_degrees);
  return camera;
}

Material LoadMaterial(const std::filesystem::path &path_or_name) {
  if (std::filesystem::exists(path_or_name)) {
    if (path_or_name.extension() == ".assetgraphbin" || path_or_name.extension() == ".json") {
      return proceduralAssetGraphMaterial(loadProceduralAssetGraphPackage(path_or_name));
    }

    const MaterialAssetLoadResult loaded = loadMaterialAsset(path_or_name);
    if (!loaded.ok()) {
      throw std::runtime_error("material failed to load: " + path_or_name.string());
    }
    if (g_context) {
      (void)g_context->material_library->addMaterialAsset(loaded.value, path_or_name.parent_path(),
                                                          {.require_existing_files = false});
      g_context->renderer.setMaterialResourceLibrary(g_context->material_library);
    }
    return resolveMaterialAssetFallback(loaded.value);
  }

  return fallbackMaterialForName(path_or_name);
}

Mesh LoadMesh(const std::filesystem::path &path_or_name) {
  if (std::filesystem::exists(path_or_name)) {
    if (path_or_name.extension() == ".assetgraphbin" || path_or_name.extension() == ".json") {
      const ProceduralAssetGraphPackage package = loadProceduralAssetGraphPackage(path_or_name);
      Mesh mesh;
      mesh.name = package.id.empty() ? path_or_name.stem().string() : package.id;
      mesh.custom_mesh = std::make_shared<const CpuMesh>(proceduralAssetGraphMesh(package));
      return mesh;
    }
    if (path_or_name.extension() == ".astergraph") {
      const std::string primitive = primitiveFromAsterGraphSource(readTextFile(path_or_name));
      Mesh mesh = meshFromPrimitiveName(primitive.empty() ? path_or_name.stem().string() : primitive);
      if (mesh.name.empty()) {
        mesh.name = path_or_name.stem().string();
      }
      return mesh;
    }
  }

  return meshFromPrimitiveName(path_or_name.stem().empty() ? path_or_name.string()
                                                           : path_or_name.stem().string());
}

Transform MakeTransform(const Vec3 position, const Vec3 rotation_degrees, const Vec3 scale) {
  return Transform::fromEuler(position,
                              {radians(rotation_degrees.x), radians(rotation_degrees.y),
                               radians(rotation_degrees.z)},
                              scale);
}

const FrameStats &LastFrameStats() {
  return g_context ? g_context->last_stats : kEmptyFrameStats;
}

const FrameForensics &LastFrameForensics() {
  return g_context ? g_context->renderer.lastFrameForensics() : kEmptyFrameForensics;
}

const char *AsterBackendName() {
  return g_context ? g_context->renderer.backendName() : "uninitialized";
}

} // namespace aster
