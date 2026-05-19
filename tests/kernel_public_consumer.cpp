// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/api.hpp"

#include <cassert>
#include <cmath>
#include <cstring>

int main() {
  const AsterAbiVersion version = aster::kernel::abiVersion();
  assert(version.major == ASTER_KERNEL_ABI_MAJOR);
  assert(version.major == 5u);

  const auto normalized = aster::kernel::math::normalize({3.0f, 0.0f, 4.0f});
  assert(normalized);
  assert(std::abs(normalized.value().x - 0.6f) < 0.00001f);
  assert(std::abs(normalized.value().z - 0.8f) < 0.00001f);

  const auto projection =
      aster::kernel::math::perspective(1.0f, 1.0f, 0.01f, 100.0f);
  assert(projection);

  const AsterTransform transform{{1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                                 {1.0f, 1.0f, 1.0f}};
  const auto matrix = aster::kernel::math::composeTrs(transform);
  assert(matrix);

  auto engine = aster::kernel::Engine::create();
  assert(engine);
  assert(engine.value().valid());
  assert(engine.value().lastStatus());

  auto window = aster::kernel::Window::createHeadless(32u, 24u);
  assert(window);

  AsterRendererDesc renderer_desc{sizeof(AsterRendererDesc),
                                  ASTER_KERNEL_STRUCT_VERSION_1,
                                  window.value().get(),
                                  ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE,
                                  ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE};
  auto renderer = aster::kernel::Renderer::create(engine.value(), renderer_desc);
  assert(renderer);
  auto capabilities = renderer.value().capabilities();
  assert(capabilities);
  assert(capabilities.value().name.size > 0u);

  auto scene = aster::kernel::Scene::create(engine.value());
  assert(scene);
  const AsterMaterialDesc material_desc{sizeof(AsterMaterialDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        {0.7f, 0.62f, 0.52f},
                                        {0.0f, 0.0f, 0.0f},
                                        0.55f,
                                        0.0f,
                                        0.0f,
                                        1.0f,
                                        ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                        0u,
                                        {"public-material", 15u}};
  auto material = aster::kernel::Material::create(engine.value(), material_desc);
  assert(material);
  AsterSceneObjectDesc object{sizeof(AsterSceneObjectDesc),
                              ASTER_KERNEL_STRUCT_VERSION_1,
                              nullptr,
                              material.value().get(),
                              nullptr,
                              ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                              {0.0f, 0.0f, 0.0f},
                              {0.0f, 0.0f, 0.0f},
                              {1.0f, 1.0f, 1.0f},
                              {"public-consumer-box", 19u}};
  assert(scene.value().addObject(object));

  const AsterCameraDesc camera{sizeof(AsterCameraDesc),
                               ASTER_KERNEL_STRUCT_VERSION_1,
                               {0.0f, 0.0f, 0.0f},
                               0.0f,
                               0.22f,
                               4.0f,
                               0.9f,
                               0.01f,
                               50.0f};
  const AsterRendererSettings settings{sizeof(AsterRendererSettings),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {0.03f, 0.04f, 0.05f},
                                       1.0f,
                                       0.22f,
                                       32u,
                                       24u,
                                       0u};
  const AsterRenderTargetDesc target_desc{sizeof(AsterRenderTargetDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM,
                                          ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT,
                                          32u,
                                          24u,
                                          1u,
                                          {"public-target", 13u}};
  auto target = aster::kernel::RenderTarget::create(engine.value(), target_desc);
  assert(target);
  assert(renderer.value().renderFrameToTarget(scene.value(), target.value(), camera, settings));
  auto stats = renderer.value().lastStats();
  assert(stats);
  assert(stats.value().framebuffer_width == 32u);
  assert(stats.value().graph_passes > 0u);
  auto schedule = renderer.value().lastFrameSchedule();
  assert(schedule);
  auto schedule_counts = schedule.value().counts();
  assert(schedule_counts);
  assert(schedule_counts.value().pass_count > 0u);

  const char *source = "float4 fs_main() { return float4(1.0); }\n";
  const AsterShaderModuleSource module{{"material", 8u}, {source, std::strlen(source)}};
  const AsterShaderCompileDesc shader_desc{sizeof(AsterShaderCompileDesc),
                                           ASTER_KERNEL_STRUCT_VERSION_1,
                                           ASTER_KERNEL_SHADER_BACKEND_METAL_MSL,
                                           {&module, 1u, sizeof(module)},
                                           {"fs_main", 7u},
                                           {"consumer", 8u},
                                           0u};
  auto shader = aster::kernel::ShaderArtifact::compile(engine.value(), shader_desc);
  assert(shader);
  auto shader_result = shader.value().result();
  assert(shader_result);
  assert(shader_result.value().success == 1u);
  return 0;
}
