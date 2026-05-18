// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/abi.h"

#include "aster/core/config.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/scene/scene.hpp"
#include "aster/shader/shader_compiler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h>
#endif

constexpr std::uint32_t kEngineMagic = 0x41535452u;
constexpr std::uint32_t kWindowMagic = 0x4154574eu;
constexpr std::uint32_t kSceneMagic = 0x41545343u;
constexpr std::uint32_t kRendererMagic = 0x41545244u;
constexpr std::uint32_t kMeshMagic = 0x41544d45u;
constexpr std::uint32_t kMaterialMagic = 0x41544d41u;
constexpr std::uint32_t kShaderMagic = 0x41545348u;
constexpr std::uint32_t kPipelineMagic = 0x41545049u;

struct AsterEngineHandle__ {
  std::uint32_t magic = kEngineMagic;
  AsterStatus last_status{sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_STATUS_OK,
                          "ok"};
};

struct AsterWindowHandle__ {
  std::uint32_t magic = kWindowMagic;
  bool headless = false;
  bool vsync = true;
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::string title;
  std::unique_ptr<aster::Window> window;
};

struct AsterSceneHandle__ {
  std::uint32_t magic = kSceneMagic;
  aster::Scene scene;
};

struct AsterMeshHandle__ {
  std::uint32_t magic = kMeshMagic;
  aster::MeshPrimitive primitive = aster::MeshPrimitive::Sphere;
  std::shared_ptr<const aster::CpuMesh> custom_mesh;
  std::string label;
};

struct AsterMaterialHandle__ {
  std::uint32_t magic = kMaterialMagic;
  aster::Material material;
  std::string label;
};

struct AsterRendererHandle__ {
  std::uint32_t magic = kRendererMagic;
  std::unique_ptr<aster::RenderDevice> renderer;
  AsterWindowHandle bound_window = nullptr;
  AsterFrameStats last_stats{};
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();

  AsterRendererHandle__() {
    last_stats.size = sizeof(AsterFrameStats);
    last_stats.version = ASTER_KERNEL_STRUCT_VERSION_1;
  }
};

struct AsterShaderArtifactHandle__ {
  std::uint32_t magic = kShaderMagic;
  aster::ShaderCompileResult result;
  std::vector<std::string> reflection_names;
  std::vector<AsterShaderReflectionBinding> reflection;
};

struct AsterRenderPipelineHandle__ {
  std::uint32_t magic = kPipelineMagic;
  AsterShaderArtifactHandle shader = nullptr;
  std::string label;
};

namespace {

AsterStatus makeStatus(const AsterStatusCode code, const char *message) {
  return {sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, code, message};
}

template <typename Struct> bool validStruct(const Struct *value) {
  return value != nullptr && value->size >= sizeof(Struct) &&
         value->version == ASTER_KERNEL_STRUCT_VERSION_1;
}

bool validStringView(const AsterStringView view) {
  return view.data != nullptr || view.size == 0u;
}

std::string stringFromView(const AsterStringView view) {
  if (view.data == nullptr || view.size == 0u) {
    return {};
  }
  return std::string(view.data, view.size);
}

AsterStringView viewFromString(const std::string &text) {
  return {text.data(), text.size()};
}

bool validEngine(const AsterEngineHandle engine) {
  return engine != nullptr && engine->magic == kEngineMagic;
}

bool validWindow(const AsterWindowHandle window) {
  return window != nullptr && window->magic == kWindowMagic;
}

bool validScene(const AsterSceneHandle scene) {
  return scene != nullptr && scene->magic == kSceneMagic;
}

bool validRenderer(const AsterRendererHandle renderer) {
  return renderer != nullptr && renderer->magic == kRendererMagic && renderer->renderer != nullptr;
}

bool validMesh(const AsterMeshHandle mesh) {
  return mesh != nullptr && mesh->magic == kMeshMagic;
}

bool validMaterial(const AsterMaterialHandle material) {
  return material != nullptr && material->magic == kMaterialMagic;
}

bool validShader(const AsterShaderArtifactHandle shader) {
  return shader != nullptr && shader->magic == kShaderMagic;
}

void setLastStatus(const AsterEngineHandle engine, const AsterStatus status) {
  if (validEngine(engine)) {
    engine->last_status = status;
  }
}

template <typename Handle> AsterStatus destroyHandle(Handle handle, const std::uint32_t magic) {
  if (handle == nullptr || handle->magic != magic) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "handle is invalid");
  }
  handle->magic = 0u;
  delete handle;
  return makeStatus(ASTER_STATUS_OK, "ok");
}

aster::Vec3 vec(const AsterVec3 value) {
  return {value.x, value.y, value.z};
}

aster::MeshPrimitive meshPrimitive(const AsterKernelMeshPrimitive primitive) {
  switch (primitive) {
  case ASTER_KERNEL_MESH_PRIMITIVE_BOX:
    return aster::MeshPrimitive::Box;
  case ASTER_KERNEL_MESH_PRIMITIVE_PLANE:
    return aster::MeshPrimitive::Plane;
  case ASTER_KERNEL_MESH_PRIMITIVE_ROCK:
    return aster::MeshPrimitive::Rock;
  case ASTER_KERNEL_MESH_PRIMITIVE_CRYSTAL:
    return aster::MeshPrimitive::Crystal;
  case ASTER_KERNEL_MESH_PRIMITIVE_RUIN_BLOCK:
    return aster::MeshPrimitive::RuinBlock;
  case ASTER_KERNEL_MESH_PRIMITIVE_PILLAR:
    return aster::MeshPrimitive::Pillar;
  case ASTER_KERNEL_MESH_PRIMITIVE_SPHERE:
  default:
    return aster::MeshPrimitive::Sphere;
  }
}

aster::MaterialAlphaMode alphaMode(const AsterKernelMaterialAlphaMode mode) {
  switch (mode) {
  case ASTER_KERNEL_MATERIAL_ALPHA_MASKED:
    return aster::MaterialAlphaMode::Masked;
  case ASTER_KERNEL_MATERIAL_ALPHA_DITHERED_COVERAGE:
    return aster::MaterialAlphaMode::DitheredCoverage;
  case ASTER_KERNEL_MATERIAL_ALPHA_BLEND:
    return aster::MaterialAlphaMode::Blend;
  case ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE:
  default:
    return aster::MaterialAlphaMode::Opaque;
  }
}

AsterKernelBackendKind backendKind(const aster::RenderBackendKind kind) {
  switch (kind) {
  case aster::RenderBackendKind::SoftwareReference:
    return ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE;
  case aster::RenderBackendKind::Metal:
    return ASTER_KERNEL_BACKEND_METAL;
  case aster::RenderBackendKind::D3D12:
    return ASTER_KERNEL_BACKEND_D3D12;
  case aster::RenderBackendKind::Null:
    return ASTER_KERNEL_BACKEND_NULL;
  case aster::RenderBackendKind::Unknown:
  default:
    return ASTER_KERNEL_BACKEND_UNKNOWN;
  }
}

std::uint32_t capabilityFlags(const aster::RenderBackendCapabilities &capabilities) {
  std::uint32_t flags = 0u;
  flags |= capabilities.gpu ? ASTER_KERNEL_BACKEND_CAP_GPU : 0u;
  flags |= capabilities.supports_shader_materials ? ASTER_KERNEL_BACKEND_CAP_SHADER_MATERIALS : 0u;
  flags |= capabilities.supports_texture_sampling ? ASTER_KERNEL_BACKEND_CAP_TEXTURE_SAMPLING : 0u;
  flags |= capabilities.supports_instancing ? ASTER_KERNEL_BACKEND_CAP_INSTANCING : 0u;
  flags |= capabilities.supports_capture ? ASTER_KERNEL_BACKEND_CAP_CAPTURE : 0u;
  flags |= capabilities.supports_ui_composite ? ASTER_KERNEL_BACKEND_CAP_UI_COMPOSITE : 0u;
  flags |= capabilities.supports_gpu_timestamps ? ASTER_KERNEL_BACKEND_CAP_GPU_TIMESTAMPS : 0u;
  return flags;
}

aster::ShaderBackend shaderBackend(const AsterKernelShaderBackend backend) {
  switch (backend) {
  case ASTER_KERNEL_SHADER_BACKEND_METAL_MSL:
    return aster::ShaderBackend::MetalMSL;
  case ASTER_KERNEL_SHADER_BACKEND_D3D12_HLSL:
    return aster::ShaderBackend::D3D12HLSL;
  case ASTER_KERNEL_SHADER_BACKEND_SOFTWARE_REFERENCE:
  default:
    return aster::ShaderBackend::SoftwareReference;
  }
}

AsterKernelShaderResourceKind shaderResourceKind(const aster::ShaderResourceKind kind) {
  switch (kind) {
  case aster::ShaderResourceKind::Texture:
    return ASTER_KERNEL_SHADER_RESOURCE_TEXTURE;
  case aster::ShaderResourceKind::Sampler:
    return ASTER_KERNEL_SHADER_RESOURCE_SAMPLER;
  case aster::ShaderResourceKind::UniformBuffer:
  default:
    return ASTER_KERNEL_SHADER_RESOURCE_UNIFORM_BUFFER;
  }
}

void setForceRendererEnvironment(const std::uint32_t flags) {
  if ((flags & ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE) != 0u) {
#if defined(_WIN32)
    (void)_putenv_s("ASTER_FORCE_SOFTWARE_RENDERER", "1");
#else
    (void)setenv("ASTER_FORCE_SOFTWARE_RENDERER", "1", 1);
#endif
  }
  if ((flags & ASTER_KERNEL_RENDERER_FLAG_FORCE_NULL) != 0u) {
#if defined(_WIN32)
    (void)_putenv_s("ASTER_FORCE_NULL_RENDERER", "1");
#else
    (void)setenv("ASTER_FORCE_NULL_RENDERER", "1", 1);
#endif
  }
}

AsterFrameStats abiFrameStats(const aster::FrameStats &stats) {
  return {.size = sizeof(AsterFrameStats),
          .version = ASTER_KERNEL_STRUCT_VERSION_1,
          .frame_seconds = stats.frame_seconds,
          .framebuffer_width = static_cast<std::uint32_t>(std::max(stats.framebuffer_width, 0)),
          .framebuffer_height = static_cast<std::uint32_t>(std::max(stats.framebuffer_height, 0)),
          .draw_calls = stats.draw_calls,
          .visible_objects = stats.visible_objects,
          .culled_objects = stats.culled_objects,
          .graph_passes = stats.graph_passes,
          .graph_resources = stats.graph_resources,
          .graph_barriers = stats.graph_barriers,
          .graph_transient_resources = stats.graph_transient_resources,
          .backend_feature_mask = stats.backend_feature_mask,
          .backend = static_cast<AsterKernelBackendKind>(stats.backend_kind_value),
          .rust_plan_seconds = stats.rust_plan_seconds,
          .graph_compile_seconds = stats.graph_compile_seconds,
          .render_encode_seconds = stats.render_encode_seconds};
}

aster::CpuMesh customMeshFromDesc(const AsterMeshDesc &desc) {
  aster::CpuMesh mesh;
  if (desc.vertices.data == nullptr || desc.vertices.size == 0u) {
    return mesh;
  }
  if (desc.vertices.stride < sizeof(AsterVertex)) {
    return mesh;
  }
  const auto *vertices = static_cast<const unsigned char *>(desc.vertices.data);
  mesh.vertices.reserve(desc.vertices.size);
  for (std::size_t i = 0; i < desc.vertices.size; ++i) {
    const auto *vertex = reinterpret_cast<const AsterVertex *>(vertices + i * desc.vertices.stride);
    mesh.vertices.push_back({.position = vec(vertex->position),
                             .normal = vec(vertex->normal),
                             .uv = {vertex->uv.x, vertex->uv.y},
                             .tangent = {vertex->tangent.x, vertex->tangent.y, vertex->tangent.z,
                                         vertex->tangent.w},
                             .ambient_occlusion = vertex->ambient_occlusion});
  }
  if (desc.indices.data != nullptr && desc.indices.size > 0u &&
      desc.indices.stride >= sizeof(std::uint32_t)) {
    const auto *indices = static_cast<const unsigned char *>(desc.indices.data);
    mesh.indices.reserve(desc.indices.size);
    for (std::size_t i = 0; i < desc.indices.size; ++i) {
      const auto *index = reinterpret_cast<const std::uint32_t *>(indices + i * desc.indices.stride);
      mesh.indices.push_back(*index);
    }
  }
  return mesh;
}

std::pair<std::uint32_t, std::uint32_t> framebufferSizeFor(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return {640u, 360u};
  }
  if (window->headless || window->window == nullptr) {
    return {std::max(window->width, 1u), std::max(window->height, 1u)};
  }
  const auto [width, height] = window->window->framebufferSize();
  return {static_cast<std::uint32_t>(std::max(width, 1)),
          static_cast<std::uint32_t>(std::max(height, 1))};
}

} // namespace

extern "C" {

AsterAbiVersion aster_kernel_abi_version(void) {
  return {ASTER_KERNEL_ABI_MAJOR, ASTER_KERNEL_ABI_MINOR, ASTER_KERNEL_ABI_PATCH};
}

AsterStatus aster_kernel_status_ok(void) {
  return makeStatus(ASTER_STATUS_OK, "ok");
}

AsterStatus aster_kernel_status_from_code(const AsterStatusCode code) {
  switch (code) {
  case ASTER_STATUS_OK:
    return makeStatus(ASTER_STATUS_OK, "ok");
  case ASTER_STATUS_INVALID_ARGUMENT:
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "invalid argument");
  case ASTER_STATUS_UNSUPPORTED:
    return makeStatus(ASTER_STATUS_UNSUPPORTED, "unsupported");
  case ASTER_STATUS_OUT_OF_MEMORY:
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "out of memory");
  case ASTER_STATUS_ABI_MISMATCH:
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "ABI mismatch");
  case ASTER_STATUS_INTERNAL_ERROR:
  default:
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "internal error");
  }
}

AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                       AsterEngineHandle *out_engine) {
  if (out_engine == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_engine is null");
  }
  *out_engine = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "engine descriptor version is not supported");
  }
  if (!validStringView(desc->application_name)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "application_name has a size but no data");
  }

  try {
    *out_engine = new AsterEngineHandle__();
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "engine allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "engine creation failed");
  }
  setLastStatus(*out_engine, aster_kernel_status_ok());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_destroy(const AsterEngineHandle engine) {
  return destroyHandle(engine, kEngineMagic);
}

AsterStatus aster_kernel_engine_last_status(const AsterEngineHandle engine) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  return engine->last_status;
}

AsterStatus aster_kernel_window_create(const AsterWindowDesc *desc,
                                       AsterWindowHandle *out_window) {
  if (out_window == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_window is null");
  }
  *out_window = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "window descriptor version is not supported");
  }
  if (!validStringView(desc->title)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window title has a size but no data");
  }
  try {
    auto *window = new AsterWindowHandle__();
    window->headless = (desc->flags & ASTER_KERNEL_WINDOW_FLAG_HEADLESS) != 0u;
    window->vsync = desc->vsync != 0u;
    window->width = std::max(desc->width, 1u);
    window->height = std::max(desc->height, 1u);
    window->title = stringFromView(desc->title);
    if (!window->headless) {
      aster::EngineConfig config;
      config.application_name = window->title.empty() ? "Aster Kernel App" : window->title.c_str();
      config.initial_width = static_cast<int>(window->width);
      config.initial_height = static_cast<int>(window->height);
      config.enable_vsync = window->vsync;
      window->window = std::make_unique<aster::Window>(config);
    }
    *out_window = window;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "window allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "window creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_poll(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (!window->headless && window->window != nullptr) {
    window->window->pollEvents();
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_swap(const AsterWindowHandle window) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (!window->headless && window->window != nullptr) {
    window->window->swapBuffers();
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_set_vsync(const AsterWindowHandle window, const std::uint32_t enabled) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  window->vsync = enabled != 0u;
  if (!window->headless && window->window != nullptr) {
    window->window->setVsync(window->vsync);
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_framebuffer_size(const AsterWindowHandle window,
                                                 AsterExtent2D *out_size) {
  if (!validWindow(window)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "window handle is invalid");
  }
  if (out_size == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_size is null");
  }
  const auto [width, height] = framebufferSizeFor(window);
  *out_size = {width, height};
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_window_destroy(const AsterWindowHandle window) {
  return destroyHandle(window, kWindowMagic);
}

AsterStatus aster_kernel_scene_create(const AsterEngineHandle engine, AsterSceneHandle *out_scene) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_scene == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_scene is null");
  }
  *out_scene = nullptr;
  try {
    *out_scene = new AsterSceneHandle__();
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "scene allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_clear(const AsterSceneHandle scene) {
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  scene->scene.objects().clear();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_add_object(const AsterSceneHandle scene,
                                          const AsterSceneObjectDesc *desc) {
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "scene object descriptor version is not supported");
  }
  if (!validStringView(desc->debug_label)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene object label has a size but no data");
  }
  aster::RenderObject object;
  object.name = stringFromView(desc->debug_label);
  object.primitive = validMesh(desc->mesh) ? desc->mesh->primitive : meshPrimitive(desc->primitive);
  object.custom_mesh = validMesh(desc->mesh) ? desc->mesh->custom_mesh : nullptr;
  if (validMaterial(desc->material)) {
    object.material = desc->material->material;
  } else {
    object.material = aster::makeMaterial({.base_color = {0.8f, 0.82f, 0.86f}});
  }
  object.transform.position = vec(desc->position);
  object.transform.rotation = vec(desc->rotation);
  object.transform.scale = vec(desc->scale);
  if (object.transform.scale.x == 0.0f && object.transform.scale.y == 0.0f &&
      object.transform.scale.z == 0.0f) {
    object.transform.scale = {1.0f, 1.0f, 1.0f};
  }
  scene->scene.objects().push_back(std::move(object));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_scene_destroy(const AsterSceneHandle scene) {
  return destroyHandle(scene, kSceneMagic);
}

AsterStatus aster_kernel_renderer_create(const AsterEngineHandle engine,
                                         const AsterRendererDesc *desc,
                                         AsterRendererHandle *out_renderer) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_renderer == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_renderer is null");
  }
  *out_renderer = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "renderer descriptor version is not supported");
  }
  try {
    setForceRendererEnvironment(desc->flags);
    auto *renderer = new AsterRendererHandle__();
    renderer->renderer = std::make_unique<aster::RenderDevice>();
    renderer->bound_window = validWindow(desc->window) ? desc->window : nullptr;
    renderer->renderer->initialize();
    *out_renderer = renderer;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "renderer allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "renderer creation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus
aster_kernel_renderer_get_capabilities(const AsterRendererHandle renderer,
                                        AsterBackendCapabilities *out_capabilities) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capabilities)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "backend capabilities struct version is not supported");
  }
  const aster::RenderBackendCapabilities capabilities = renderer->renderer->backendCapabilities();
  out_capabilities->backend = backendKind(capabilities.kind);
  out_capabilities->flags = capabilityFlags(capabilities);
  out_capabilities->graph_resource_mask = capabilities.graph_resource_mask;
  out_capabilities->name = {capabilities.name, std::strlen(capabilities.name)};
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_render_frame(const AsterRendererHandle renderer,
                                               const AsterSceneHandle scene,
                                               const AsterCameraDesc *camera,
                                               const AsterRendererSettings *settings) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validScene(scene)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "scene handle is invalid");
  }
  if (!validStruct(camera) || !validStruct(settings)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "render frame struct version is not supported");
  }
  try {
    auto [width, height] = framebufferSizeFor(renderer->bound_window);
    if (settings->framebuffer_width > 0u) {
      width = settings->framebuffer_width;
    }
    if (settings->framebuffer_height > 0u) {
      height = settings->framebuffer_height;
    }
    aster::OrbitCamera orbit;
    orbit.target = vec(camera->target);
    orbit.yaw = camera->yaw_radians;
    orbit.pitch = camera->pitch_radians;
    orbit.radius = std::max(camera->radius, 0.01f);
    orbit.vertical_fov =
        camera->vertical_fov_radians > 0.0f ? camera->vertical_fov_radians : aster::radians(45.0f);
    orbit.near_plane = camera->near_plane > 0.0f ? camera->near_plane : 0.01f;
    orbit.far_plane = camera->far_plane > orbit.near_plane ? camera->far_plane : 100.0f;

    aster::RendererSettings render_settings;
    render_settings.pipeline.clear_color = vec(settings->clear_color);
    render_settings.exposure = settings->exposure > 0.0f ? settings->exposure : 1.0f;
    render_settings.ambient_strength =
        settings->ambient_strength > 0.0f ? settings->ambient_strength : 0.18f;
    render_settings.sun_light.enabled = true;
    render_settings.sun_light.intensity = 1.0f;

    renderer->renderer->prepareScene(scene->scene);
    const double frame_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - renderer->started_at)
            .count();
    const aster::FrameStats stats =
        renderer->renderer->render(scene->scene, orbit, render_settings, static_cast<int>(width),
                                   static_cast<int>(height), frame_seconds);
    renderer->last_stats = abiFrameStats(stats);
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "render frame failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_present(const AsterRendererHandle renderer,
                                          const AsterWindowHandle window) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  return aster_kernel_window_swap(validWindow(window) ? window : renderer->bound_window);
}

AsterStatus aster_kernel_renderer_capture(const AsterRendererHandle renderer,
                                          const AsterCaptureDesc *desc) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "capture descriptor version is not supported");
  }
  if (!validStringView(desc->path) || desc->path.size == 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "capture path is invalid");
  }
  try {
    const std::uint32_t width =
        desc->width > 0u ? desc->width : std::max(renderer->last_stats.framebuffer_width, 1u);
    const std::uint32_t height =
        desc->height > 0u ? desc->height : std::max(renderer->last_stats.framebuffer_height, 1u);
    aster::writeFramebufferPpm(stringFromView(desc->path), static_cast<int>(width),
                               static_cast<int>(height));
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "frame capture failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_last_stats(const AsterRendererHandle renderer,
                                             AsterFrameStats *out_stats) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_stats)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "frame stats struct version is not supported");
  }
  *out_stats = renderer->last_stats;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_destroy(const AsterRendererHandle renderer) {
  return destroyHandle(renderer, kRendererMagic);
}

AsterStatus aster_kernel_mesh_create(const AsterEngineHandle engine, const AsterMeshDesc *desc,
                                     AsterMeshHandle *out_mesh) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_mesh == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_mesh is null");
  }
  *out_mesh = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "mesh descriptor version is not supported");
  }
  try {
    auto *mesh = new AsterMeshHandle__();
    mesh->primitive = meshPrimitive(desc->primitive);
    mesh->label = stringFromView(desc->debug_label);
    aster::CpuMesh custom = customMeshFromDesc(*desc);
    if (!custom.vertices.empty() && !custom.indices.empty()) {
      mesh->custom_mesh = std::make_shared<aster::CpuMesh>(std::move(custom));
    }
    *out_mesh = mesh;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "mesh allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_mesh_destroy(const AsterMeshHandle mesh) {
  return destroyHandle(mesh, kMeshMagic);
}

AsterStatus aster_kernel_material_create(const AsterEngineHandle engine,
                                         const AsterMaterialDesc *desc,
                                         AsterMaterialHandle *out_material) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_material == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_material is null");
  }
  *out_material = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "material descriptor version is not supported");
  }
  try {
    auto *material = new AsterMaterialHandle__();
    material->label = stringFromView(desc->debug_label);
    material->material = aster::makeMaterial({.base_color = vec(desc->base_color),
                                              .emission_color = vec(desc->emission_color),
                                              .roughness = desc->roughness > 0.0f ? desc->roughness : 0.55f,
                                              .metallic = desc->metallic,
                                              .emission_strength = desc->emission_strength,
                                              .opacity = desc->opacity > 0.0f ? desc->opacity : 1.0f,
                                              .double_sided = desc->double_sided != 0u,
                                              .alpha_mode = alphaMode(desc->alpha_mode)});
    *out_material = material;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "material allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_material_destroy(const AsterMaterialHandle material) {
  return destroyHandle(material, kMaterialMagic);
}

AsterStatus aster_kernel_shader_compile(const AsterEngineHandle engine,
                                        const AsterShaderCompileDesc *desc,
                                        AsterShaderArtifactHandle *out_shader) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_shader == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_shader is null");
  }
  *out_shader = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "shader compile descriptor version is not supported");
  }
  if (desc->modules.size > 0u &&
      (desc->modules.data == nullptr || desc->modules.stride < sizeof(AsterShaderModuleSource))) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader modules span is invalid");
  }
  try {
    aster::ShaderLibrary library;
    std::vector<std::string> module_names;
    const auto *modules = static_cast<const unsigned char *>(desc->modules.data);
    for (std::size_t i = 0; i < desc->modules.size; ++i) {
      const auto *module =
          reinterpret_cast<const AsterShaderModuleSource *>(modules + i * desc->modules.stride);
      if (!validStringView(module->name) || !validStringView(module->source)) {
        return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader module string view is invalid");
      }
      std::string name = stringFromView(module->name);
      if (name.empty()) {
        name = "module_" + std::to_string(i);
      }
      library.addModule(name, stringFromView(module->source));
      module_names.push_back(std::move(name));
    }

    auto *shader = new AsterShaderArtifactHandle__();
    aster::ShaderVariantKey variant;
    variant.feature_mask = desc->feature_mask;
    variant.tag = stringFromView(desc->variant_tag);
    shader->result = aster::compileShaderVariant(
        library, {.backend = shaderBackend(desc->backend),
                  .variant = variant,
                  .modules = module_names,
                  .entry_point = stringFromView(desc->entry_point).empty()
                                     ? std::string("fs_main")
                                     : stringFromView(desc->entry_point)});
    shader->reflection_names.reserve(shader->result.reflection.resources.size());
    shader->reflection.reserve(shader->result.reflection.resources.size());
    for (const aster::ShaderResourceBinding &binding : shader->result.reflection.resources) {
      shader->reflection_names.push_back(binding.name);
      shader->reflection.push_back({.size = sizeof(AsterShaderReflectionBinding),
                                    .version = ASTER_KERNEL_STRUCT_VERSION_1,
                                    .name = {},
                                    .kind = shaderResourceKind(binding.kind),
                                    .binding = binding.binding,
                                    .count = binding.count});
    }
    for (std::size_t i = 0; i < shader->reflection.size(); ++i) {
      shader->reflection[i].name = viewFromString(shader->reflection_names[i]);
    }
    *out_shader = shader;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "shader allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "shader compile failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_result(const AsterShaderArtifactHandle shader,
                                           AsterShaderCompileResult *out_result) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (!validStruct(out_result)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "shader result struct version is not supported");
  }
  out_result->success = shader->result.success ? 1u : 0u;
  out_result->source_size = shader->result.source.size();
  out_result->diagnostic_count = shader->result.diagnostics.size();
  out_result->reflection_binding_count = shader->reflection.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_source(const AsterShaderArtifactHandle shader,
                                           AsterStringView *out_source) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (out_source == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_source is null");
  }
  *out_source = viewFromString(shader->result.source);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_diagnostics(const AsterShaderArtifactHandle shader,
                                                const std::size_t index,
                                                AsterStringView *out_diagnostic) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (out_diagnostic == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_diagnostic is null");
  }
  if (index >= shader->result.diagnostics.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader diagnostic index is out of range");
  }
  *out_diagnostic = viewFromString(shader->result.diagnostics[index]);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_get_reflection(const AsterShaderArtifactHandle shader,
                                               const std::size_t index,
                                               AsterShaderReflectionBinding *out_binding) {
  if (!validShader(shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader handle is invalid");
  }
  if (!validStruct(out_binding)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "reflection binding struct version is not supported");
  }
  if (index >= shader->reflection.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "shader reflection index is out of range");
  }
  *out_binding = shader->reflection[index];
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_shader_destroy(const AsterShaderArtifactHandle shader) {
  return destroyHandle(shader, kShaderMagic);
}

AsterStatus aster_kernel_render_pipeline_create(const AsterEngineHandle engine,
                                                const AsterRenderPipelineDesc *desc,
                                                AsterRenderPipelineHandle *out_pipeline) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  if (out_pipeline == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_pipeline is null");
  }
  *out_pipeline = nullptr;
  if (!validStruct(desc)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "render pipeline descriptor version is not supported");
  }
  if (!validShader(desc->shader)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "pipeline shader handle is invalid");
  }
  try {
    auto *pipeline = new AsterRenderPipelineHandle__();
    pipeline->shader = desc->shader;
    pipeline->label = stringFromView(desc->debug_label);
    *out_pipeline = pipeline;
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "pipeline allocation failed");
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_render_pipeline_destroy(const AsterRenderPipelineHandle pipeline) {
  return destroyHandle(pipeline, kPipelineMagic);
}

AsterStatus aster_kernel_physics_world_destroy(const AsterPhysicsWorldHandle physics_world) {
  if (physics_world == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "physics world handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "physics world creation is not public yet");
}

AsterStatus aster_kernel_system_world_destroy(const AsterSystemWorldHandle system_world) {
  if (system_world == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "system world handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "system world creation is not public yet");
}

AsterStatus aster_kernel_sample_app_destroy(const AsterSampleAppHandle sample_app) {
  if (sample_app == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "sample app handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "sample app creation is not public yet");
}

} // extern "C"
