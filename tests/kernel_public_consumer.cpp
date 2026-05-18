// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/api.hpp"

#include <cassert>
#include <cstring>

int main() {
  const AsterAbiVersion version = aster::kernel::abiVersion();
  assert(version.major == ASTER_KERNEL_ABI_MAJOR);

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
  AsterSceneObjectDesc object{sizeof(AsterSceneObjectDesc),
                              ASTER_KERNEL_STRUCT_VERSION_1,
                              nullptr,
                              nullptr,
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
  assert(renderer.value().renderFrame(scene.value(), camera, settings));
  auto stats = renderer.value().lastStats();
  assert(stats);
  assert(stats.value().framebuffer_width == 32u);
  assert(stats.value().graph_passes > 0u);

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
