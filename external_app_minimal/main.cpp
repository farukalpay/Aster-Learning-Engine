// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include <aster/kernel/api.hpp>
#include <aster/game_sdk/game_sdk.hpp>

#include <array>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <filesystem>

namespace {

void verifyGameSdkPackage() {
  const auto project = aster::sdk::parseProjectDocument(R"json({
    "schema_version": 1,
    "name": "External Minimal",
    "startup_scene": "scene.external",
    "assets": [
      { "id": "scene.external", "kind": "scene", "path": "scenes/external.scene" },
      { "id": "material.external", "kind": "material", "path": "materials/external.material" }
    ]
  })json");
  assert(project.ok());
  assert(project.value.assets.size() == 2u);
  assert(project.value.assets.front().kind == aster::sdk::AssetKind::Scene);

  const auto material = aster::sdk::parseMaterialDocument(R"json({
    "schema_version": 1,
    "id": "material.external",
    "name": "External Material",
    "base_color": [0.8, 0.7, 0.6],
    "emission_color": [0.1, 0.2, 0.3],
    "emission_strength": 1.5,
    "alpha_mode": "opaque"
  })json");
  assert(material.ok());
  assert(material.value.base_color.x > 0.0f);
  assert(material.value.emission_color.z > 0.0f);
}

AsterTextureDesc textureDesc(const AsterTextureRole role,
                             const AsterTextureColorSpace color_space,
                             const AsterTextureNormalConvention normal_convention,
                             const AsterKernelBackendFormat format,
                             const char *label) {
  return {sizeof(AsterTextureDesc),
          ASTER_KERNEL_STRUCT_VERSION_1,
          role,
          color_space,
          normal_convention,
          format,
          4u,
          4u,
          1u,
          {},
          aster::kernel::stringView(label)};
}

} // namespace

int main() {
  verifyGameSdkPackage();

  auto engine = aster::kernel::Engine::create();
  assert(engine);

  auto bad_albedo = aster::kernel::Texture::create(
      engine.value(),
      textureDesc(ASTER_TEXTURE_ROLE_ALBEDO, ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                  ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM, "bad-albedo"));
  assert(!bad_albedo);
  assert(bad_albedo.status().code() == ASTER_STATUS_VALIDATION_ERROR);
  auto engine_events = engine.value().validationEventCount();
  assert(engine_events);
  assert(engine_events.value() > 0u);

  auto window = aster::kernel::Window::createHeadless(96u, 64u);
  assert(window);
  const AsterRendererDesc renderer_desc{sizeof(AsterRendererDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        window.value().get(),
                                        ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE,
                                        ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE};
  auto renderer = aster::kernel::Renderer::create(engine.value(), renderer_desc);
  assert(renderer);

  const auto early_capture_path =
      std::filesystem::temp_directory_path() / "aster_external_capture_before_frame.ppm";
  const std::string early_capture_string = early_capture_path.string();
  const AsterCaptureDesc early_capture{sizeof(AsterCaptureDesc),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {early_capture_string.data(), early_capture_string.size()},
                                       96u,
                                       64u};
  const auto early_capture_status = renderer.value().capture(early_capture);
  assert(early_capture_status.code() == ASTER_STATUS_VALIDATION_ERROR);
  auto renderer_events = renderer.value().validationEventCount();
  assert(renderer_events);
  assert(renderer_events.value() > 0u);

  auto albedo = aster::kernel::Texture::create(
      engine.value(),
      textureDesc(ASTER_TEXTURE_ROLE_ALBEDO, ASTER_TEXTURE_COLOR_SPACE_SRGB,
                  ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_SRGB, "albedo"));
  auto normal = aster::kernel::Texture::create(
      engine.value(),
      textureDesc(ASTER_TEXTURE_ROLE_NORMAL, ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                  ASTER_TEXTURE_NORMAL_CONVENTION_OPENGL,
                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM, "normal"));
  auto orm = aster::kernel::Texture::create(
      engine.value(),
      textureDesc(ASTER_TEXTURE_ROLE_ORM, ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                  ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM, "orm"));
  assert(albedo && normal && orm);

  auto retired_texture = aster::kernel::Texture::create(
      engine.value(),
      textureDesc(ASTER_TEXTURE_ROLE_ORM, ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                  ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM, "retired-orm"));
  assert(retired_texture);
  const AsterTextureHandle retired_handle = retired_texture.value().get();
  retired_texture.value().reset();
  const AsterMaterialTextureBinding retired_bindings[] = {
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_ALBEDO, albedo.value().get()},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_NORMAL, normal.value().get()},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_TEXTURE_ROLE_ORM,
       retired_handle},
  };
  const AsterMaterialDesc retired_material_desc{sizeof(AsterMaterialDesc),
                                                ASTER_KERNEL_STRUCT_VERSION_1,
                                                {1.0f, 1.0f, 1.0f},
                                                {0.0f, 0.0f, 0.0f},
                                                0.5f,
                                                0.0f,
                                                0.0f,
                                                1.0f,
                                                ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                                0u,
                                                {"retired-material", 16u},
                                                {retired_bindings, 3u,
                                                 sizeof(AsterMaterialTextureBinding)}};
  auto retired_material =
      aster::kernel::Material::create(engine.value(), retired_material_desc);
  assert(!retired_material);
  assert(retired_material.status().code() == ASTER_STATUS_LIFETIME_ERROR);

  const AsterMaterialTextureBinding bindings[] = {
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_ALBEDO, albedo.value().get()},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_NORMAL, normal.value().get()},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_TEXTURE_ROLE_ORM,
       orm.value().get()},
  };
  const AsterMaterialDesc material_desc{sizeof(AsterMaterialDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        {0.9f, 0.8f, 0.7f},
                                        {0.0f, 0.0f, 0.0f},
                                        0.48f,
                                        0.0f,
                                        0.0f,
                                        1.0f,
                                        ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                        0u,
                                        {"external-lit-pbr", 16u},
                                        {bindings, 3u,
                                         sizeof(AsterMaterialTextureBinding)}};
  auto material = aster::kernel::Material::create(engine.value(), material_desc);
  assert(material);

  const AsterVertex vertices[] = {
      {{-0.75f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f},
       {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
      {{0.75f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f},
       {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
      {{0.0f, 0.75f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f},
       {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
  };
  const std::uint32_t indices[] = {0u, 1u, 2u};
  const AsterMeshDesc mesh_desc{sizeof(AsterMeshDesc),
                                ASTER_KERNEL_STRUCT_VERSION_1,
                                ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                {vertices, 3u, sizeof(AsterVertex)},
                                {indices, 3u, sizeof(std::uint32_t)},
                                {"external-triangle", 17u}};
  auto mesh = aster::kernel::Mesh::create(engine.value(), mesh_desc);
  assert(mesh);

  auto scene = aster::kernel::Scene::create(engine.value());
  assert(scene);
  const AsterSceneObjectDesc object_desc{sizeof(AsterSceneObjectDesc),
                                         ASTER_KERNEL_STRUCT_VERSION_1,
                                         mesh.value().get(),
                                         material.value().get(),
                                         nullptr,
                                         ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                         {0.0f, 0.0f, 0.0f},
                                         {0.0f, 0.0f, 0.0f},
                                         {1.0f, 1.0f, 1.0f},
                                         {"external-object", 15u}};
  assert(scene.value().addObject(object_desc));

  const char *source = "float4 fs_main() { return float4(1.0); }\n";
  const AsterShaderModuleSource module{{"material", 8u}, {source, std::strlen(source)}};
  const AsterShaderCompileDesc shader_desc{sizeof(AsterShaderCompileDesc),
                                           ASTER_KERNEL_STRUCT_VERSION_1,
                                           ASTER_KERNEL_SHADER_BACKEND_METAL_MSL,
                                           {&module, 1u, sizeof(module)},
                                           {"fs_main", 7u},
                                           {"external", 8u},
                                           0u};
  auto shader = aster::kernel::ShaderArtifact::compile(engine.value(), shader_desc);
  assert(shader);
  auto shader_result = shader.value().result();
  assert(shader_result);
  assert(shader_result.value().success == 1u);

  const AsterRenderTargetDesc target_desc{sizeof(AsterRenderTargetDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM,
                                          ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT,
                                          96u,
                                          64u,
                                          1u,
                                          {"external-target", 15u}};
  auto target = aster::kernel::RenderTarget::create(engine.value(), target_desc);
  assert(target);
  const AsterCameraDesc camera{sizeof(AsterCameraDesc),
                               ASTER_KERNEL_STRUCT_VERSION_1,
                               {0.0f, 0.0f, 0.0f},
                               0.0f,
                               0.2f,
                               4.0f,
                               0.9f,
                               0.01f,
                               50.0f};
  const AsterRendererSettings settings{sizeof(AsterRendererSettings),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {0.03f, 0.04f, 0.05f},
                                       1.0f,
                                       0.22f,
                                       96u,
                                       64u,
                                       0u};
  assert(renderer.value().renderFrameToTarget(scene.value(), target.value(), camera, settings));

  auto schedule = renderer.value().lastFrameSchedule();
  assert(schedule);
  auto counts = schedule.value().counts();
  assert(counts);
  assert(counts.value().pass_count > 0u);
  assert(counts.value().transition_count > 0u);
  auto first_pass = schedule.value().pass(0u);
  assert(first_pass);
  assert(first_pass.value().name.size > 0u);
  auto memory = schedule.value().memoryReport();
  assert(memory);
  assert(memory.value().resident_bytes >= memory.value().transient_bytes);

  const auto capture_path =
      std::filesystem::temp_directory_path() / "aster_external_minimal_capture.ppm";
  const std::string capture_string = capture_path.string();
  const AsterCaptureDesc capture{sizeof(AsterCaptureDesc),
                                 ASTER_KERNEL_STRUCT_VERSION_1,
                                 {capture_string.data(), capture_string.size()},
                                 96u,
                                 64u};
  assert(renderer.value().captureRenderTarget(target.value(), capture));
  assert(std::filesystem::exists(capture_path));
  std::filesystem::remove(capture_path);

  return 0;
}
