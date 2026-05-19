// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/api.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

namespace {

static_assert(std::is_standard_layout_v<AsterAbiVersion>);
static_assert(std::is_standard_layout_v<AsterStatus>);
static_assert(std::is_standard_layout_v<AsterStringView>);
static_assert(std::is_standard_layout_v<AsterSpan>);
static_assert(std::is_standard_layout_v<AsterEngineDesc>);
static_assert(std::is_standard_layout_v<AsterWindowDesc>);
static_assert(std::is_standard_layout_v<AsterRendererDesc>);
static_assert(std::is_standard_layout_v<AsterBackendCapabilities>);
static_assert(std::is_standard_layout_v<AsterBackendCapabilityTable>);
static_assert(std::is_standard_layout_v<AsterShaderCompileDesc>);
static_assert(std::is_standard_layout_v<AsterShaderCompileResult>);
static_assert(std::is_standard_layout_v<AsterShaderReflectionBinding>);
static_assert(std::is_standard_layout_v<AsterRenderPipelineDesc>);
static_assert(std::is_standard_layout_v<AsterFrameGraphDesc>);
static_assert(std::is_standard_layout_v<AsterMeshDesc>);
static_assert(std::is_standard_layout_v<AsterMaterialDesc>);
static_assert(std::is_standard_layout_v<AsterSceneObjectDesc>);
static_assert(std::is_standard_layout_v<AsterCameraDesc>);
static_assert(std::is_standard_layout_v<AsterRendererSettings>);
static_assert(std::is_standard_layout_v<AsterFrameStats>);
static_assert(std::is_standard_layout_v<AsterFrameForensicsCounts>);
static_assert(std::is_standard_layout_v<AsterFrameForensicsDetailCounts>);
static_assert(std::is_standard_layout_v<AsterFramePassStats>);
static_assert(std::is_standard_layout_v<AsterFrameDiagnosticEvent>);
static_assert(std::is_standard_layout_v<AsterFrameDebugCaptureInfo>);
static_assert(std::is_standard_layout_v<AsterFramePassArtifactInfo>);
static_assert(std::is_standard_layout_v<AsterFrameResourceTransition>);
static_assert(std::is_standard_layout_v<AsterObjectRenderFate>);
static_assert(std::is_standard_layout_v<AsterRhiValidationEvent>);
static_assert(std::is_standard_layout_v<AsterFrameTimestampSample>);
static_assert(std::is_standard_layout_v<AsterBackendFeatureProof>);
static_assert(std::is_standard_layout_v<AsterCaptureDesc>);
static_assert(std::is_standard_layout_v<AsterVec2>);
static_assert(std::is_standard_layout_v<AsterVec3>);
static_assert(std::is_standard_layout_v<AsterVec4>);
static_assert(std::is_standard_layout_v<AsterDVec2>);
static_assert(std::is_standard_layout_v<AsterDVec3>);
static_assert(std::is_standard_layout_v<AsterDVec4>);
static_assert(std::is_standard_layout_v<AsterMat2>);
static_assert(std::is_standard_layout_v<AsterMat3>);
static_assert(std::is_standard_layout_v<AsterMat4>);
static_assert(std::is_standard_layout_v<AsterDMat2>);
static_assert(std::is_standard_layout_v<AsterDMat3>);
static_assert(std::is_standard_layout_v<AsterDMat4>);
static_assert(std::is_standard_layout_v<AsterQuat>);
static_assert(std::is_standard_layout_v<AsterTransform>);
static_assert(std::is_standard_layout_v<AsterRay3>);
static_assert(std::is_standard_layout_v<AsterWorldPoint>);
static_assert(std::is_standard_layout_v<AsterViewPoint>);
static_assert(std::is_standard_layout_v<AsterClipPoint>);
static_assert(std::is_standard_layout_v<AsterNdcPoint>);
static_assert(std::is_standard_layout_v<AsterScreenPoint>);
static_assert(std::is_standard_layout_v<AsterWorldRay>);
static_assert(std::is_standard_layout_v<AsterViewport>);
static_assert(std::is_standard_layout_v<AsterProjectionConvention>);
static_assert(std::is_standard_layout_v<AsterPlane3>);
static_assert(std::is_standard_layout_v<AsterAabb3>);
static_assert(std::is_standard_layout_v<AsterSphere3>);
static_assert(std::is_standard_layout_v<AsterMathPolicy>);
static_assert(std::is_standard_layout_v<AsterMathDiagnostics>);
static_assert(sizeof(AsterStatusCode) == sizeof(std::int32_t));
static_assert(offsetof(AsterStatus, size) == 0u);
static_assert(offsetof(AsterStatus, version) > offsetof(AsterStatus, size));
static_assert(offsetof(AsterStatus, code) > offsetof(AsterStatus, version));
static_assert(offsetof(AsterStatus, message) > offsetof(AsterStatus, code));
static_assert(offsetof(AsterEngineDesc, size) == 0u);
static_assert(offsetof(AsterEngineDesc, version) > offsetof(AsterEngineDesc, size));
static_assert(offsetof(AsterEngineDesc, application_name) > offsetof(AsterEngineDesc, version));
static_assert(offsetof(AsterEngineDesc, flags) > offsetof(AsterEngineDesc, application_name));

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  assert(input.good());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::set<std::string> readManifest() {
  std::ifstream input(std::string(ASTER_SOURCE_DIR) + "/abi/aster_kernel.symbols");
  assert(input.good());
  std::set<std::string> symbols;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    const std::size_t last = line.find_last_not_of(" \t\r\n");
    symbols.insert(line.substr(first, last - first + 1u));
  }
  return symbols;
}

void testAbiHeaderStaysPlainC() {
  const std::string header = readFile(std::string(ASTER_SOURCE_DIR) + "/include/aster/kernel/abi.h");
  const char *forbidden[] = {
      "#include <vector", "#include <string", "#include <memory", "#include <optional",
      "#include <span",   "#include \"aster/", "std::"};
  for (const char *token : forbidden) {
    assert(header.find(token) == std::string::npos);
  }
}

void testPublicApiBoundaryIsFrozen() {
  const std::string cmake = readFile(std::string(ASTER_SOURCE_DIR) + "/CMakeLists.txt");
  assert(cmake.find("add_library(aster_kernel SHARED src/kernel/kernel.cpp)") !=
         std::string::npos);
  assert(cmake.find("add_library(aster_game_sdk STATIC") != std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/kernel DESTINATION") !=
         std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/game_sdk DESTINATION") !=
         std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster DESTINATION") == std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/render") == std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/rhi") == std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/framegraph") == std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/scene") == std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/material") == std::string::npos);
}

void testStatusAndEngineLifecycle() {
  const AsterAbiVersion version = aster_kernel_abi_version();
  assert(version.major == ASTER_KERNEL_ABI_MAJOR);
  assert(version.major == 4u);
  assert(version.minor == ASTER_KERNEL_ABI_MINOR);
  assert(version.minor == 1u);
  assert(version.patch == ASTER_KERNEL_ABI_PATCH);

  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc desc{sizeof(AsterEngineDesc),
                             ASTER_KERNEL_STRUCT_VERSION_1,
                             {"kernel-contract-test", 20u},
                             0u};
  AsterStatus status = aster_kernel_engine_create(&desc, &engine);
  assert(status.code == ASTER_STATUS_OK);
  assert(engine != nullptr);
  assert(aster_kernel_engine_last_status(engine).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);

  AsterEngineDesc stale = desc;
  stale.version = 0u;
  engine = nullptr;
  status = aster_kernel_engine_create(&stale, &engine);
  assert(status.code == ASTER_STATUS_ABI_MISMATCH);
  assert(engine == nullptr);
}

void testMathAbi3Contracts() {
  const AsterMathPolicy policy = aster_kernel_math_default_policy();
  assert(policy.size == sizeof(AsterMathPolicy));
  assert(policy.version == ASTER_KERNEL_STRUCT_VERSION_1);
  assert(policy.absolute_epsilon > 0.0f);

  float dot = 0.0f;
  assert(aster_kernel_math_vec3_dot({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, &dot).code ==
         ASTER_STATUS_OK);
  assert(std::abs(dot - 32.0f) < 0.00001f);

  AsterMathDiagnostics diagnostics{};
  AsterVec3 normalized{};
  assert(aster_kernel_math_vec3_normalize({0.0f, 0.0f, 0.0f}, &policy, &normalized,
                                          &diagnostics)
             .code == ASTER_STATUS_INVALID_ARGUMENT);
  assert(diagnostics.error == ASTER_MATH_ERROR_DEGENERATE_INPUT);

  AsterMat4 identity{};
  assert(aster_kernel_math_mat4_identity(&identity).code == ASTER_STATUS_OK);
  AsterMat4 inverse{};
  diagnostics = {};
  assert(aster_kernel_math_mat4_inverse(&identity, &policy, &inverse, &diagnostics).code ==
         ASTER_STATUS_OK);
  assert(diagnostics.error == ASTER_MATH_ERROR_NONE);
  assert(std::abs(inverse.m[0] - 1.0f) < 0.00001f);
  assert(std::abs(inverse.m[5] - 1.0f) < 0.00001f);
  assert(std::abs(inverse.m[10] - 1.0f) < 0.00001f);
  assert(std::abs(inverse.m[15] - 1.0f) < 0.00001f);

  AsterMat4 singular{};
  diagnostics = {};
  assert(aster_kernel_math_mat4_inverse(&singular, &policy, &inverse, &diagnostics).code ==
         ASTER_STATUS_INVALID_ARGUMENT);
  assert(diagnostics.error == ASTER_MATH_ERROR_SINGULAR_MATRIX);

  AsterQuat rotation{};
  diagnostics = {};
  assert(aster_kernel_math_quat_axis_angle({0.0f, 1.0f, 0.0f}, 1.57079632679f, &rotation,
                                           &diagnostics)
             .code == ASTER_STATUS_OK);
  AsterVec3 rotated{};
  assert(aster_kernel_math_quat_rotate_vec3(rotation, {0.0f, 0.0f, -1.0f}, &rotated).code ==
         ASTER_STATUS_OK);
  assert(std::abs(rotated.x + 1.0f) < 0.0001f);

  AsterMat4 projection{};
  diagnostics = {};
  assert(aster_kernel_math_mat4_perspective(
             1.0f, 1.0f, 0.05f, 100.0f, ASTER_MATH_COORDINATE_RIGHT_HANDED,
             ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE, ASTER_MATH_DEPTH_REVERSE_Z, &projection,
             &diagnostics)
             .code == ASTER_STATUS_OK);
  AsterMat4 view{};
  assert(aster_kernel_math_mat4_look_at({0.0f, 0.0f, 4.0f}, {0.0f, 0.0f, 0.0f},
                                        {0.0f, 1.0f, 0.0f},
                                        ASTER_MATH_COORDINATE_RIGHT_HANDED, &view,
                                        &diagnostics)
             .code == ASTER_STATUS_OK);
  AsterMat4 world_to_clip{};
  assert(aster_kernel_math_mat4_multiply(&projection, &view, &world_to_clip).code ==
         ASTER_STATUS_OK);
  AsterMat4 clip_to_world{};
  assert(aster_kernel_math_mat4_inverse(&world_to_clip, &policy, &clip_to_world,
                                        &diagnostics)
             .code == ASTER_STATUS_OK);
  const AsterViewport viewport{{0.0f, 0.0f}, {640.0f, 480.0f}, 1u};
  const AsterWorldPoint world_point{{0.2f, -0.1f, 0.0f}};
  AsterScreenPoint screen_point{};
  assert(aster_kernel_math_world_to_screen(world_point, &world_to_clip, &viewport, &screen_point,
                                           &diagnostics)
             .code == ASTER_STATUS_OK);
  AsterWorldPoint restored_point{};
  assert(aster_kernel_math_screen_to_world(screen_point, &clip_to_world, &viewport,
                                           &restored_point, &diagnostics)
             .code == ASTER_STATUS_OK);
  assert(std::abs(restored_point.value.x - world_point.value.x) < 0.002f);
  assert(std::abs(restored_point.value.y - world_point.value.y) < 0.002f);
  assert(std::abs(restored_point.value.z - world_point.value.z) < 0.002f);
  const AsterProjectionConvention convention{ASTER_MATH_COORDINATE_RIGHT_HANDED,
                                             ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE,
                                             ASTER_MATH_DEPTH_REVERSE_Z,
                                             1u,
                                             1u,
                                             1u,
                                             1u};
  AsterWorldRay world_ray{};
  assert(aster_kernel_math_screen_to_world_ray(screen_point, &clip_to_world, &viewport,
                                               &convention, {{0.0f, 0.0f, 4.0f}}, &world_ray,
                                               &diagnostics)
             .code == ASTER_STATUS_OK);
  assert(world_ray.direction.z < -0.99f);
}

void testRendererAbi3Lifecycle() {
  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"kernel-renderer-test", 20u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);

  AsterWindowHandle window = nullptr;
  const AsterWindowDesc window_desc{sizeof(AsterWindowDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"headless", 8u},
                                    64u,
                                    48u,
                                    ASTER_KERNEL_WINDOW_FLAG_HEADLESS,
                                    0u};
  assert(aster_kernel_window_create(&window_desc, &window).code == ASTER_STATUS_OK);
  AsterExtent2D extent{};
  assert(aster_kernel_window_framebuffer_size(window, &extent).code == ASTER_STATUS_OK);
  assert(extent.width == 64u && extent.height == 48u);

  AsterRendererHandle renderer = nullptr;
  const AsterRendererDesc renderer_desc{sizeof(AsterRendererDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        window,
                                        ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE,
                                        ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE};
  assert(aster_kernel_renderer_create(engine, &renderer_desc, &renderer).code == ASTER_STATUS_OK);

  AsterBackendCapabilities capabilities{sizeof(AsterBackendCapabilities),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_get_capabilities(renderer, &capabilities).code ==
         ASTER_STATUS_OK);
  assert(capabilities.backend == ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE ||
         capabilities.backend == ASTER_KERNEL_BACKEND_NULL);
  AsterBackendCapabilityTable table{sizeof(AsterBackendCapabilityTable),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_get_backend_capability_table(renderer, &table).code ==
         ASTER_STATUS_OK);
  assert(table.backend == capabilities.backend);
  assert((table.color_format_mask & (1ull << ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM)) != 0ull ||
         table.backend == ASTER_KERNEL_BACKEND_NULL);
  assert((table.sample_count_mask & (1ull << 1u)) != 0ull ||
         table.backend == ASTER_KERNEL_BACKEND_NULL);
  assert(table.presentation == ASTER_KERNEL_BACKEND_PRESENTATION_SOFTWARE_FRAMEBUFFER ||
         table.backend == ASTER_KERNEL_BACKEND_NULL);

  AsterSceneHandle scene = nullptr;
  assert(aster_kernel_scene_create(engine, &scene).code == ASTER_STATUS_OK);

  AsterMeshHandle mesh = nullptr;
  const AsterMeshDesc mesh_desc{sizeof(AsterMeshDesc),
                                ASTER_KERNEL_STRUCT_VERSION_1,
                                ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                {},
                                {},
                                {"box", 3u}};
  assert(aster_kernel_mesh_create(engine, &mesh_desc, &mesh).code == ASTER_STATUS_OK);

  AsterMaterialHandle material = nullptr;
  const AsterMaterialDesc material_desc{sizeof(AsterMaterialDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        {0.72f, 0.50f, 0.32f},
                                        {0.0f, 0.0f, 0.0f},
                                        0.58f,
                                        0.0f,
                                        0.0f,
                                        1.0f,
                                        ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                        0u,
                                        {"clay", 4u}};
  assert(aster_kernel_material_create(engine, &material_desc, &material).code ==
         ASTER_STATUS_OK);

  const AsterSceneObjectDesc object_desc{sizeof(AsterSceneObjectDesc),
                                         ASTER_KERNEL_STRUCT_VERSION_1,
                                         mesh,
                                         material,
                                         nullptr,
                                         ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                         {0.0f, 0.0f, 0.0f},
                                         {0.0f, 0.0f, 0.0f},
                                         {1.0f, 1.0f, 1.0f},
                                         {"box", 3u}};
  assert(aster_kernel_scene_add_object(scene, &object_desc).code == ASTER_STATUS_OK);

  const AsterCameraDesc camera{sizeof(AsterCameraDesc),
                               ASTER_KERNEL_STRUCT_VERSION_1,
                               {0.0f, 0.0f, 0.0f},
                               0.0f,
                               0.25f,
                               5.0f,
                               0.9f,
                               0.01f,
                               50.0f};
  const AsterRendererSettings settings{sizeof(AsterRendererSettings),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {0.04f, 0.05f, 0.07f},
                                       1.0f,
                                       0.24f,
                                       64u,
                                       48u,
                                       0u};
  assert(aster_kernel_renderer_render_frame(renderer, scene, &camera, &settings).code ==
         ASTER_STATUS_OK);
  AsterFrameStats stats{sizeof(AsterFrameStats), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_last_stats(renderer, &stats).code == ASTER_STATUS_OK);
  assert(stats.framebuffer_width == 64u);
  assert(stats.framebuffer_height == 48u);
  assert(stats.graph_passes >= 1u);
  AsterFrameForensicsCounts forensics_counts{sizeof(AsterFrameForensicsCounts),
                                             ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_frame_forensics_counts(renderer, &forensics_counts).code ==
         ASTER_STATUS_OK);
  assert(forensics_counts.pass_count >= 1u);
  AsterFrameForensicsDetailCounts detail_counts{sizeof(AsterFrameForensicsDetailCounts),
                                                ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_frame_forensics_detail_counts(renderer, &detail_counts).code ==
         ASTER_STATUS_OK);
  assert(detail_counts.pass_count == forensics_counts.pass_count);
  assert(detail_counts.debug_capture_count >= 1u);
  assert(detail_counts.resource_transition_count >= 1u);
  assert(detail_counts.object_fate_count >= 1u);
  assert(detail_counts.backend_feature_proof_count >= 1u);
  AsterFramePassStats pass_stats{sizeof(AsterFramePassStats), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_frame_pass_stats(renderer, 0u, &pass_stats).code ==
         ASTER_STATUS_OK);
  assert(pass_stats.name.size > 0u);
  AsterFrameDebugCaptureInfo capture_info{sizeof(AsterFrameDebugCaptureInfo),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_debug_capture_info(renderer, 0u, &capture_info).code ==
         ASTER_STATUS_OK);
  assert(capture_info.label.size > 0u);
  AsterFramePassArtifactInfo artifact_info{sizeof(AsterFramePassArtifactInfo),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_pass_artifact_info(renderer, 0u, &artifact_info).code ==
         ASTER_STATUS_OK);
  assert(artifact_info.kind.size > 0u);
  AsterFrameResourceTransition transition{sizeof(AsterFrameResourceTransition),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_resource_transition(renderer, 0u, &transition).code ==
         ASTER_STATUS_OK);
  AsterObjectRenderFate fate{sizeof(AsterObjectRenderFate), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_object_render_fate(renderer, 0u, &fate).code == ASTER_STATUS_OK);
  assert(fate.object_name.size > 0u);
  assert(fate.mesh_key.size > 0u);
  assert(fate.material_key.size > 0u);
  assert(fate.pass_list.size > 0u);
  assert(fate.final_contribution.size > 0u);
  assert(fate.contribution_hash != 0u);
  AsterFrameTimestampSample timestamp{sizeof(AsterFrameTimestampSample),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
  if (detail_counts.timestamp_sample_count > 0u) {
    assert(aster_kernel_renderer_timestamp_sample(renderer, 0u, &timestamp).code ==
           ASTER_STATUS_OK);
  }
  AsterRhiValidationEvent validation_event{sizeof(AsterRhiValidationEvent),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
  if (detail_counts.rhi_validation_event_count > 0u) {
    assert(aster_kernel_renderer_rhi_validation_event(renderer, 0u, &validation_event).code ==
           ASTER_STATUS_OK);
  }
  AsterBackendFeatureProof proof{sizeof(AsterBackendFeatureProof),
                                 ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_backend_feature_proof(renderer, 0u, &proof).code ==
         ASTER_STATUS_OK);
  assert(proof.feature.size > 0u);

  const std::filesystem::path capture_path =
      std::filesystem::temp_directory_path() / "aster_kernel_renderer_abi4.ppm";
  const std::string capture_string = capture_path.string();
  const AsterCaptureDesc capture{sizeof(AsterCaptureDesc),
                                 ASTER_KERNEL_STRUCT_VERSION_1,
                                 {capture_string.data(), capture_string.size()},
                                 64u,
                                 48u};
  assert(aster_kernel_renderer_capture(renderer, &capture).code == ASTER_STATUS_OK);
  assert(std::filesystem::exists(capture_path));
  std::filesystem::remove(capture_path);

  assert(aster_kernel_renderer_present(renderer, window).code == ASTER_STATUS_OK);
  assert(aster_kernel_material_destroy(material).code == ASTER_STATUS_OK);
  assert(aster_kernel_mesh_destroy(mesh).code == ASTER_STATUS_OK);
  assert(aster_kernel_scene_destroy(scene).code == ASTER_STATUS_OK);
  assert(aster_kernel_renderer_destroy(renderer).code == ASTER_STATUS_OK);
  assert(aster_kernel_window_destroy(window).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
}

void testShaderCompilerAbi3() {
  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"kernel-shader-test", 18u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);

  const char *source = "float4 fs_main() { return float4(0.2, 0.4, 0.8, 1.0); }\n";
  const AsterShaderModuleSource modules[] = {{{"material", 8u}, {source, std::strlen(source)}}};
  const AsterShaderCompileDesc compile_desc{sizeof(AsterShaderCompileDesc),
                                            ASTER_KERNEL_STRUCT_VERSION_1,
                                            ASTER_KERNEL_SHADER_BACKEND_D3D12_HLSL,
                                            {modules, 1u, sizeof(AsterShaderModuleSource)},
                                            {"fs_main", 7u},
                                            {"abi4-test", 9u},
                                            1ull};
  AsterShaderArtifactHandle shader = nullptr;
  assert(aster_kernel_shader_compile(engine, &compile_desc, &shader).code == ASTER_STATUS_OK);
  AsterShaderCompileResult result{sizeof(AsterShaderCompileResult),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_shader_get_result(shader, &result).code == ASTER_STATUS_OK);
  assert(result.success == 1u);
  assert(result.source_size > 0u);
  assert(result.reflection_binding_count >= 2u);
  AsterStringView generated{};
  assert(aster_kernel_shader_get_source(shader, &generated).code == ASTER_STATUS_OK);
  const std::string generated_source(generated.data, generated.size);
  assert(generated_source.find("generated HLSL") != std::string::npos);

  AsterShaderReflectionBinding binding{sizeof(AsterShaderReflectionBinding),
                                       ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_shader_get_reflection(shader, 0u, &binding).code == ASTER_STATUS_OK);
  assert(binding.binding == 0u);
  assert(aster_kernel_shader_destroy(shader).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
}

void testCppWrapperUsesResultStatus() {
  auto engine = aster::kernel::Engine::create();
  assert(engine);
  assert(engine.value().valid());

  AsterEngineDesc bad_desc{sizeof(AsterEngineDesc), 0u, {}, 0u};
  auto failed = aster::kernel::Engine::create(bad_desc);
  assert(!failed);
  assert(failed.status().code() == ASTER_STATUS_ABI_MISMATCH);
}

void testManifestNamesMatchLinkedApi() {
  const std::set<std::string> expected{
      "aster_kernel_abi_version",
      "aster_kernel_status_ok",
      "aster_kernel_status_from_code",
      "aster_kernel_math_default_policy",
      "aster_kernel_math_vec3_dot",
      "aster_kernel_math_vec3_cross",
      "aster_kernel_math_vec3_length",
      "aster_kernel_math_vec3_normalize",
      "aster_kernel_math_mat4_identity",
      "aster_kernel_math_mat4_multiply",
      "aster_kernel_math_mat4_inverse",
      "aster_kernel_math_mat4_compose_trs",
      "aster_kernel_math_mat4_decompose_trs",
      "aster_kernel_math_mat4_perspective",
      "aster_kernel_math_mat4_orthographic",
      "aster_kernel_math_mat4_look_at",
      "aster_kernel_math_world_to_screen",
      "aster_kernel_math_screen_to_world",
      "aster_kernel_math_screen_to_world_ray",
      "aster_kernel_math_quat_identity",
      "aster_kernel_math_quat_axis_angle",
      "aster_kernel_math_quat_slerp",
      "aster_kernel_math_quat_rotate_vec3",
      "aster_kernel_math_quat_inverse",
      "aster_kernel_math_intersect_ray_plane",
      "aster_kernel_math_intersect_ray_triangle",
      "aster_kernel_math_intersect_ray_sphere",
      "aster_kernel_engine_create",
      "aster_kernel_engine_destroy",
      "aster_kernel_engine_last_status",
      "aster_kernel_window_create",
      "aster_kernel_window_poll",
      "aster_kernel_window_swap",
      "aster_kernel_window_set_vsync",
      "aster_kernel_window_framebuffer_size",
      "aster_kernel_window_destroy",
      "aster_kernel_scene_create",
      "aster_kernel_scene_clear",
      "aster_kernel_scene_add_object",
      "aster_kernel_scene_destroy",
      "aster_kernel_renderer_create",
      "aster_kernel_renderer_get_capabilities",
      "aster_kernel_renderer_get_backend_capability_table",
      "aster_kernel_renderer_render_frame",
      "aster_kernel_renderer_present",
      "aster_kernel_renderer_capture",
      "aster_kernel_renderer_last_stats",
      "aster_kernel_renderer_frame_forensics_counts",
      "aster_kernel_renderer_frame_forensics_detail_counts",
      "aster_kernel_renderer_frame_pass_stats",
      "aster_kernel_renderer_frame_diagnostic",
      "aster_kernel_renderer_debug_capture_info",
      "aster_kernel_renderer_pass_artifact_info",
      "aster_kernel_renderer_resource_transition",
      "aster_kernel_renderer_rhi_validation_event",
      "aster_kernel_renderer_timestamp_sample",
      "aster_kernel_renderer_backend_feature_proof",
      "aster_kernel_renderer_destroy",
      "aster_kernel_mesh_create",
      "aster_kernel_mesh_destroy",
      "aster_kernel_material_create",
      "aster_kernel_material_destroy",
      "aster_kernel_shader_compile",
      "aster_kernel_shader_get_result",
      "aster_kernel_shader_get_source",
      "aster_kernel_shader_get_diagnostics",
      "aster_kernel_shader_get_reflection",
      "aster_kernel_shader_destroy",
      "aster_kernel_render_pipeline_create",
      "aster_kernel_render_pipeline_destroy",
      "aster_kernel_physics_world_destroy",
      "aster_kernel_system_world_destroy",
      "aster_kernel_sample_app_destroy",
  };
  assert(readManifest() == expected);

  (void)&aster_kernel_abi_version;
  (void)&aster_kernel_status_ok;
  (void)&aster_kernel_status_from_code;
  (void)&aster_kernel_math_default_policy;
  (void)&aster_kernel_math_vec3_dot;
  (void)&aster_kernel_math_vec3_cross;
  (void)&aster_kernel_math_vec3_length;
  (void)&aster_kernel_math_vec3_normalize;
  (void)&aster_kernel_math_mat4_identity;
  (void)&aster_kernel_math_mat4_multiply;
  (void)&aster_kernel_math_mat4_inverse;
  (void)&aster_kernel_math_mat4_compose_trs;
  (void)&aster_kernel_math_mat4_decompose_trs;
  (void)&aster_kernel_math_mat4_perspective;
  (void)&aster_kernel_math_mat4_orthographic;
  (void)&aster_kernel_math_mat4_look_at;
  (void)&aster_kernel_math_world_to_screen;
  (void)&aster_kernel_math_screen_to_world;
  (void)&aster_kernel_math_screen_to_world_ray;
  (void)&aster_kernel_math_quat_identity;
  (void)&aster_kernel_math_quat_axis_angle;
  (void)&aster_kernel_math_quat_slerp;
  (void)&aster_kernel_math_quat_rotate_vec3;
  (void)&aster_kernel_math_quat_inverse;
  (void)&aster_kernel_math_intersect_ray_plane;
  (void)&aster_kernel_math_intersect_ray_triangle;
  (void)&aster_kernel_math_intersect_ray_sphere;
  (void)&aster_kernel_engine_create;
  (void)&aster_kernel_engine_destroy;
  (void)&aster_kernel_engine_last_status;
  (void)&aster_kernel_window_create;
  (void)&aster_kernel_window_poll;
  (void)&aster_kernel_window_swap;
  (void)&aster_kernel_window_set_vsync;
  (void)&aster_kernel_window_framebuffer_size;
  (void)&aster_kernel_window_destroy;
  (void)&aster_kernel_scene_create;
  (void)&aster_kernel_scene_clear;
  (void)&aster_kernel_scene_add_object;
  (void)&aster_kernel_scene_destroy;
  (void)&aster_kernel_renderer_create;
  (void)&aster_kernel_renderer_get_capabilities;
  (void)&aster_kernel_renderer_get_backend_capability_table;
  (void)&aster_kernel_renderer_render_frame;
  (void)&aster_kernel_renderer_present;
  (void)&aster_kernel_renderer_capture;
  (void)&aster_kernel_renderer_last_stats;
  (void)&aster_kernel_renderer_frame_forensics_counts;
  (void)&aster_kernel_renderer_frame_forensics_detail_counts;
  (void)&aster_kernel_renderer_frame_pass_stats;
  (void)&aster_kernel_renderer_frame_diagnostic;
  (void)&aster_kernel_renderer_debug_capture_info;
  (void)&aster_kernel_renderer_pass_artifact_info;
  (void)&aster_kernel_renderer_resource_transition;
  (void)&aster_kernel_renderer_rhi_validation_event;
  (void)&aster_kernel_renderer_timestamp_sample;
  (void)&aster_kernel_renderer_backend_feature_proof;
  (void)&aster_kernel_renderer_destroy;
  (void)&aster_kernel_mesh_create;
  (void)&aster_kernel_mesh_destroy;
  (void)&aster_kernel_material_create;
  (void)&aster_kernel_material_destroy;
  (void)&aster_kernel_shader_compile;
  (void)&aster_kernel_shader_get_result;
  (void)&aster_kernel_shader_get_source;
  (void)&aster_kernel_shader_get_diagnostics;
  (void)&aster_kernel_shader_get_reflection;
  (void)&aster_kernel_shader_destroy;
  (void)&aster_kernel_render_pipeline_create;
  (void)&aster_kernel_render_pipeline_destroy;
  (void)&aster_kernel_physics_world_destroy;
  (void)&aster_kernel_system_world_destroy;
  (void)&aster_kernel_sample_app_destroy;
}

} // namespace

int main() {
  testAbiHeaderStaysPlainC();
  testPublicApiBoundaryIsFrozen();
  testStatusAndEngineLifecycle();
  testMathAbi3Contracts();
  testRendererAbi3Lifecycle();
  testShaderCompilerAbi3();
  testCppWrapperUsesResultStatus();
  testManifestNamesMatchLinkedApi();
  return 0;
}
