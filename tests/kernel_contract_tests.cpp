// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/kernel/api.hpp"
#include "aster/math/mat4.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
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
static_assert(std::is_standard_layout_v<AsterPresentDesc>);
static_assert(std::is_standard_layout_v<AsterPresentResult>);
static_assert(std::is_standard_layout_v<AsterRendererPresentationStatus>);
static_assert(std::is_standard_layout_v<AsterValidationEvent>);
static_assert(std::is_standard_layout_v<AsterAuthoringDocumentDesc>);
static_assert(std::is_standard_layout_v<AsterAuthoringDocumentInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringDiagnosticInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringProjectAssetInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringEntityInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringActionNodeInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringKeyValue>);
static_assert(std::is_standard_layout_v<AsterAuthoringInputBindingInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringActionContext>);
static_assert(std::is_standard_layout_v<AsterAuthoringActionExecutionInfo>);
static_assert(std::is_standard_layout_v<AsterAuthoringActionEventInfo>);
static_assert(std::is_standard_layout_v<AsterSystemEntityHandle>);
static_assert(std::is_standard_layout_v<AsterSystemWorldDesc>);
static_assert(std::is_standard_layout_v<AsterSystemTickDesc>);
static_assert(std::is_standard_layout_v<AsterSystemTickResult>);
static_assert(std::is_standard_layout_v<AsterSystemEntityInfo>);
static_assert(std::is_standard_layout_v<AsterSystemComponentAccess>);
static_assert(std::is_standard_layout_v<AsterSystemTransactionDesc>);
static_assert(std::is_standard_layout_v<AsterSystemTransactionInfo>);
static_assert(std::is_standard_layout_v<AsterSystemTraceCounts>);
static_assert(std::is_standard_layout_v<AsterSystemTraceEvent>);
static_assert(std::is_standard_layout_v<AsterActor>);
static_assert(std::is_standard_layout_v<AsterStimulus>);
static_assert(std::is_standard_layout_v<AsterAffordance>);
static_assert(std::is_standard_layout_v<AsterEncounter>);
static_assert(std::is_standard_layout_v<AsterBiomeCell>);
static_assert(std::is_standard_layout_v<AsterResourceNode>);
static_assert(std::is_standard_layout_v<AsterNavValidityReport>);
static_assert(std::is_standard_layout_v<AsterPerceptualBudget>);
static_assert(std::is_standard_layout_v<AsterWorldDesc>);
static_assert(std::is_standard_layout_v<AsterWorldAdvanceDesc>);
static_assert(std::is_standard_layout_v<AsterWorldAdvanceResult>);
static_assert(std::is_standard_layout_v<AsterWorldRegionGateReport>);
static_assert(std::is_standard_layout_v<AsterWorldRenderExtractionDesc>);
static_assert(std::is_standard_layout_v<AsterWorldRenderExtraction>);
static_assert(std::is_standard_layout_v<AsterWorldForensics>);
static_assert(std::is_standard_layout_v<AsterAssetLineageInfo>);
static_assert(std::is_standard_layout_v<AsterResidencyBudget>);
static_assert(std::is_standard_layout_v<AsterResidencyDecision>);
static_assert(std::is_standard_layout_v<AsterWorldSnapshotDesc>);
static_assert(std::is_standard_layout_v<AsterWorldMigrationReport>);
static_assert(std::is_standard_layout_v<AsterWorldReplayReport>);
static_assert(std::is_standard_layout_v<AsterShaderCompileDesc>);
static_assert(std::is_standard_layout_v<AsterShaderCompileResult>);
static_assert(std::is_standard_layout_v<AsterShaderReflectionBinding>);
static_assert(std::is_standard_layout_v<AsterRenderPipelineDesc>);
static_assert(std::is_standard_layout_v<AsterTextureDesc>);
static_assert(std::is_standard_layout_v<AsterMaterialTextureBinding>);
static_assert(std::is_standard_layout_v<AsterRenderTargetDesc>);
static_assert(std::is_standard_layout_v<AsterBufferDesc>);
static_assert(std::is_standard_layout_v<AsterDescriptorHeapDesc>);
static_assert(std::is_standard_layout_v<AsterDescriptorSetDesc>);
static_assert(std::is_standard_layout_v<AsterPipelineCacheDesc>);
static_assert(std::is_standard_layout_v<AsterFrameScheduleCounts>);
static_assert(std::is_standard_layout_v<AsterFrameSchedulePassInfo>);
static_assert(std::is_standard_layout_v<AsterFrameScheduleMemoryReport>);
static_assert(std::is_standard_layout_v<AsterFrameScheduleDescriptorInfo>);
static_assert(std::is_standard_layout_v<AsterFrameSchedulePipelineInfo>);
static_assert(std::is_standard_layout_v<AsterFrameScheduleTransientAllocationInfo>);
static_assert(std::is_standard_layout_v<AsterFrameScheduleTimelineInfo>);
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
static_assert(ASTER_KERNEL_RENDER_QUALITY_PRODUCTION == 0u);
static_assert(ASTER_KERNEL_TONE_MAPPER_PBR_NEUTRAL == 0u);
static_assert(ASTER_KERNEL_RENDER_PASS_SURFACE_OCCLUSION !=
              ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW);
static_assert(ASTER_KERNEL_RENDER_RESOURCE_SURFACE_ATTRIBUTES !=
              ASTER_KERNEL_RENDER_RESOURCE_SCENE_COLOR);
static_assert(ASTER_KERNEL_RENDER_RESOURCE_SURFACE_OCCLUSION !=
              ASTER_KERNEL_RENDER_RESOURCE_SHADOW_ATLAS);
static_assert(offsetof(AsterCameraDesc, focal_length_mm) > offsetof(AsterCameraDesc, far_plane));
static_assert(offsetof(AsterRendererSettings, quality_tier) >
              offsetof(AsterRendererSettings, render_target));

struct LegacyCameraDesc {
  size_t size;
  uint32_t version;
  AsterVec3 target;
  float yaw_radians;
  float pitch_radians;
  float radius;
  float vertical_fov_radians;
  float near_plane;
  float far_plane;
};

struct LegacyRendererSettings {
  size_t size;
  uint32_t version;
  AsterVec3 clear_color;
  float exposure;
  float ambient_strength;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint32_t flags;
  AsterRenderTargetHandle render_target;
};

static_assert(sizeof(LegacyCameraDesc) == offsetof(AsterCameraDesc, focal_length_mm));
static_assert(sizeof(LegacyRendererSettings) == offsetof(AsterRendererSettings, quality_tier));

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  assert(input.good());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string toString(const AsterStringView view) {
  return view.data == nullptr ? std::string() : std::string(view.data, view.size);
}

AsterMat4 abiMat4(const aster::Mat4 &matrix) {
  AsterMat4 out{};
  std::memcpy(out.m, matrix.m.data(), sizeof(out.m));
  return out;
}

AsterMathCoordinateHandedness abiHandedness(const aster::CoordinateHandedness value) {
  return value == aster::CoordinateHandedness::LeftHanded ? ASTER_MATH_COORDINATE_LEFT_HANDED
                                                          : ASTER_MATH_COORDINATE_RIGHT_HANDED;
}

AsterMathClipDepthRange abiDepthRange(const aster::ClipDepthRange value) {
  return value == aster::ClipDepthRange::NegativeOneToOne
             ? ASTER_MATH_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE
             : ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE;
}

AsterMathDepthDirection abiDepthDirection(const aster::DepthDirection value) {
  return value == aster::DepthDirection::ReverseZ ? ASTER_MATH_DEPTH_REVERSE_Z
                                                  : ASTER_MATH_DEPTH_FORWARD_Z;
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
  assert(cmake.find("target_compile_features(aster_kernel PUBLIC cxx_std_20)") !=
         std::string::npos);
  assert(cmake.find("target_compile_features(aster_game_sdk PUBLIC cxx_std_20)") !=
         std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/kernel DESTINATION") !=
         std::string::npos);
  assert(cmake.find("install(DIRECTORY include/aster/game_sdk DESTINATION") !=
         std::string::npos);
  assert(cmake.find("AsterKernelConfig.cmake") != std::string::npos);
  assert(cmake.find("AsterGameSdkConfig.cmake") != std::string::npos);
  assert(cmake.find("aster_external_app_minimal_install_tree") != std::string::npos);
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
  assert(version.major == 6u);
  assert(version.minor == ASTER_KERNEL_ABI_MINOR);
  assert(version.minor == 0u);
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

void testMathAbi5Contracts() {
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

  for (const AsterMathCoordinateHandedness handedness :
       {ASTER_MATH_COORDINATE_RIGHT_HANDED, ASTER_MATH_COORDINATE_LEFT_HANDED}) {
    for (const AsterMathClipDepthRange depth_range :
         {ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE, ASTER_MATH_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE}) {
      for (const AsterMathDepthDirection depth_direction :
           {ASTER_MATH_DEPTH_FORWARD_Z, ASTER_MATH_DEPTH_REVERSE_Z}) {
        for (const uint32_t origin_top_left : {0u, 1u}) {
          AsterMat4 projection{};
          diagnostics = {};
          assert(aster_kernel_math_mat4_perspective(
                     1.0f, 1.0f, 0.05f, 100.0f, handedness, depth_range, depth_direction,
                     &projection, &diagnostics)
                     .code == ASTER_STATUS_OK);
          AsterMat4 view{};
          const AsterVec3 eye =
              handedness == ASTER_MATH_COORDINATE_RIGHT_HANDED ? AsterVec3{0.0f, 0.0f, 4.0f}
                                                               : AsterVec3{0.0f, 0.0f, -4.0f};
          assert(aster_kernel_math_mat4_look_at(eye, {0.0f, 0.0f, 0.0f},
                                                {0.0f, 1.0f, 0.0f}, handedness, &view,
                                                &diagnostics)
                     .code == ASTER_STATUS_OK);
          AsterMat4 world_to_clip{};
          assert(aster_kernel_math_mat4_multiply(&projection, &view, &world_to_clip).code ==
                 ASTER_STATUS_OK);
          AsterMat4 clip_to_world{};
          diagnostics = {};
          assert(aster_kernel_math_mat4_inverse(&world_to_clip, &policy, &clip_to_world,
                                                &diagnostics)
                     .code == ASTER_STATUS_OK);
          const AsterViewport viewport{{0.0f, 0.0f}, {640.0f, 480.0f}, origin_top_left};
          const AsterWorldPoint world_point{{0.2f, -0.1f, 0.0f}};
          AsterScreenPoint screen_point{};
          diagnostics = {};
          assert(aster_kernel_math_world_to_screen(world_point, &world_to_clip, &viewport,
                                                   &screen_point, &diagnostics)
                     .code == ASTER_STATUS_OK);
          AsterWorldPoint restored_point{};
          assert(aster_kernel_math_screen_to_world(screen_point, &clip_to_world, &viewport,
                                                   &restored_point, &diagnostics)
                     .code == ASTER_STATUS_OK);
          assert(std::abs(restored_point.value.x - world_point.value.x) < 0.002f);
          assert(std::abs(restored_point.value.y - world_point.value.y) < 0.002f);
          assert(std::abs(restored_point.value.z - world_point.value.z) < 0.002f);
          const AsterProjectionConvention convention{handedness,
                                                     depth_range,
                                                     depth_direction,
                                                     origin_top_left,
                                                     1u,
                                                     1u,
                                                     1u};
          AsterWorldRay world_ray{};
          assert(aster_kernel_math_screen_to_world_ray(screen_point, &clip_to_world, &viewport,
                                                       &convention, {eye}, &world_ray,
                                                       &diagnostics)
                     .code == ASTER_STATUS_OK);
          const float expected_z =
              handedness == ASTER_MATH_COORDINATE_RIGHT_HANDED ? -1.0f : 1.0f;
          assert(world_ray.direction.z * expected_z > 0.99f);
        }
      }
    }
  }

  const AsterViewport bad_viewport{{0.0f, 0.0f}, {0.0f, 480.0f}, 1u};
  const AsterWorldPoint world_point{{0.2f, -0.1f, 0.0f}};
  AsterScreenPoint bad_screen{};
  diagnostics = {};
  assert(aster_kernel_math_world_to_screen(world_point, &identity, &bad_viewport, &bad_screen,
                                           &diagnostics)
             .code == ASTER_STATUS_INVALID_ARGUMENT);
  assert(diagnostics.error == ASTER_MATH_ERROR_INVALID_ARGUMENT);
}

void testProjectionAbiEntrypointsMatchHeaderMathBitwise() {
  const aster::ProjectionPolicy policies[] = {
      {aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::ZeroToOne,
       aster::DepthDirection::ReverseZ},
      {aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::ZeroToOne,
       aster::DepthDirection::ForwardZ},
      {aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::NegativeOneToOne,
       aster::DepthDirection::ReverseZ},
      {aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::NegativeOneToOne,
       aster::DepthDirection::ForwardZ},
      {aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::ZeroToOne,
       aster::DepthDirection::ReverseZ},
      {aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::ZeroToOne,
       aster::DepthDirection::ForwardZ},
      {aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::NegativeOneToOne,
       aster::DepthDirection::ReverseZ},
      {aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::NegativeOneToOne,
       aster::DepthDirection::ForwardZ},
  };

  const AsterMathPolicy math_policy = aster_kernel_math_default_policy();
  for (const aster::ProjectionPolicy policy : policies) {
    const aster::MathResult<aster::Mat4> header_projection =
        aster::perspective(aster::radians(57.0f), 16.0f / 9.0f, 0.05f, 250.0f, policy);
    assert(header_projection);
    AsterMat4 abi_projection{};
    AsterMathDiagnostics diagnostics{};
    assert(aster_kernel_math_mat4_perspective(
               aster::radians(57.0f), 16.0f / 9.0f, 0.05f, 250.0f,
               abiHandedness(policy.handedness), abiDepthRange(policy.depth_range),
               abiDepthDirection(policy.depth_direction), &abi_projection, &diagnostics)
               .code == ASTER_STATUS_OK);
    assert(std::memcmp(abi_projection.m, header_projection.value.m.data(),
                       sizeof(abi_projection.m)) == 0);

    const aster::Vec3 eye =
        policy.handedness == aster::CoordinateHandedness::RightHanded
            ? aster::Vec3{0.0f, 1.0f, 5.0f}
            : aster::Vec3{0.0f, 1.0f, -5.0f};
    const aster::MathResult<aster::Mat4> header_view =
        aster::lookAt(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, policy.handedness);
    assert(header_view);
    AsterMat4 abi_view{};
    assert(aster_kernel_math_mat4_look_at({eye.x, eye.y, eye.z}, {0.0f, 0.0f, 0.0f},
                                          {0.0f, 1.0f, 0.0f},
                                          abiHandedness(policy.handedness), &abi_view,
                                          &diagnostics)
               .code == ASTER_STATUS_OK);
    assert(std::memcmp(abi_view.m, header_view.value.m.data(), sizeof(abi_view.m)) == 0);

    const aster::Mat4 header_world_to_clip = header_projection.value * header_view.value;
    AsterMat4 abi_world_to_clip{};
    assert(aster_kernel_math_mat4_multiply(&abi_projection, &abi_view, &abi_world_to_clip).code ==
           ASTER_STATUS_OK);
    assert(std::memcmp(abi_world_to_clip.m, header_world_to_clip.m.data(),
                       sizeof(abi_world_to_clip.m)) == 0);

    const AsterViewport viewport{{7.0f, 11.0f}, {1024.0f, 576.0f},
                                 policy.viewport_origin == aster::ViewportOrigin::TopLeft ? 1u
                                                                                           : 0u};
    const AsterWorldPoint world{{0.25f, -0.1f, 0.0f}};
    const aster::MathResult<aster::ScreenPoint> header_screen =
        aster::project(aster::WorldPoint{world.value.x, world.value.y, world.value.z},
                       aster::WorldToClip{header_world_to_clip},
                       {{viewport.origin.x, viewport.origin.y},
                        {viewport.size.x, viewport.size.y},
                        policy.viewport_origin});
    assert(header_screen);
    AsterScreenPoint abi_screen{};
    assert(aster_kernel_math_world_to_screen(world, &abi_world_to_clip, &viewport, &abi_screen,
                                             &diagnostics)
               .code == ASTER_STATUS_OK);
    assert(std::memcmp(&abi_screen.value, &header_screen.value.value, sizeof(abi_screen.value)) ==
           0);

    const aster::MathResult<aster::Mat4> header_clip_to_world =
        aster::inverse(header_world_to_clip);
    assert(header_clip_to_world);
    const AsterMat4 abi_clip_to_world = abiMat4(header_clip_to_world.value);
    AsterWorldPoint abi_restored{};
    assert(aster_kernel_math_screen_to_world(abi_screen, &abi_clip_to_world, &viewport,
                                             &abi_restored, &diagnostics)
               .code == ASTER_STATUS_OK);
    const aster::MathResult<aster::WorldPoint> header_restored =
        aster::unproject(header_screen.value, aster::ClipToWorld{header_clip_to_world.value},
                         {{viewport.origin.x, viewport.origin.y},
                          {viewport.size.x, viewport.size.y},
                          policy.viewport_origin});
    assert(header_restored);
    assert(std::memcmp(&abi_restored.value, &header_restored.value.value,
                       sizeof(abi_restored.value)) == 0);

    AsterMat4 abi_inverse{};
    assert(aster_kernel_math_mat4_inverse(&abi_world_to_clip, &math_policy, &abi_inverse,
                                          &diagnostics)
               .code == ASTER_STATUS_OK);
    assert(std::memcmp(abi_inverse.m, header_clip_to_world.value.m.data(),
                       sizeof(abi_inverse.m)) == 0);
  }
}

void testRendererAbi5Lifecycle() {
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
  AsterRendererPresentationStatus presentation_status{
      sizeof(AsterRendererPresentationStatus), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_presentation_status(renderer, &presentation_status).code ==
         ASTER_STATUS_OK);
  assert(presentation_status.backend == table.backend);
  assert(presentation_status.native_present_supported == 0u);
  assert(presentation_status.bound_window == 0u);
  assert(aster_kernel_renderer_bind_window(renderer, window).code == ASTER_STATUS_OK);

  const std::filesystem::path early_capture_path =
      std::filesystem::temp_directory_path() / "aster_kernel_capture_before_render.ppm";
  const std::string early_capture_string = early_capture_path.string();
  const AsterCaptureDesc early_capture{sizeof(AsterCaptureDesc),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {early_capture_string.data(), early_capture_string.size()},
                                       64u,
                                       48u};
  assert(aster_kernel_renderer_capture(renderer, &early_capture).code ==
         ASTER_STATUS_VALIDATION_ERROR);
  size_t renderer_validation_count = 0u;
  assert(aster_kernel_renderer_validation_event_count(renderer, &renderer_validation_count).code ==
         ASTER_STATUS_OK);
  assert(renderer_validation_count >= 1u);
  AsterValidationEvent renderer_validation{sizeof(AsterValidationEvent),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_validation_event(renderer, renderer_validation_count - 1u,
                                                &renderer_validation)
             .code == ASTER_STATUS_OK);
  assert(renderer_validation.kind == ASTER_VALIDATION_CAPTURE_BEFORE_RENDER);

  AsterBufferHandle frame_buffer = nullptr;
  const AsterBufferDesc frame_buffer_desc{sizeof(AsterBufferDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          4096u,
                                          1u,
                                          {"frame-constants", 15u}};
  assert(aster_kernel_buffer_create(engine, &frame_buffer_desc, &frame_buffer).code ==
         ASTER_STATUS_OK);
  AsterDescriptorHeapHandle descriptor_heap = nullptr;
  const AsterDescriptorHeapDesc descriptor_heap_desc{sizeof(AsterDescriptorHeapDesc),
                                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                                    16u,
                                                    1u,
                                                    {"frame-heap", 10u}};
  assert(aster_kernel_descriptor_heap_create(engine, &descriptor_heap_desc, &descriptor_heap)
             .code == ASTER_STATUS_OK);
  AsterDescriptorSetHandle descriptor_set = nullptr;
  const AsterDescriptorSetDesc descriptor_set_desc{sizeof(AsterDescriptorSetDesc),
                                                  ASTER_KERNEL_STRUCT_VERSION_1,
                                                  descriptor_heap,
                                                  3u,
                                                  {"frame-set", 9u}};
  assert(aster_kernel_descriptor_set_create(engine, &descriptor_set_desc, &descriptor_set).code ==
         ASTER_STATUS_OK);
  AsterPipelineCacheHandle pipeline_cache = nullptr;
  const AsterPipelineCacheDesc pipeline_cache_desc{sizeof(AsterPipelineCacheDesc),
                                                  ASTER_KERNEL_STRUCT_VERSION_1,
                                                  0xA57E5005ull,
                                                  {"frame-pipelines", 15u}};
  assert(aster_kernel_pipeline_cache_create(engine, &pipeline_cache_desc, &pipeline_cache).code ==
         ASTER_STATUS_OK);

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
  AsterRenderTargetHandle target = nullptr;
  const AsterRenderTargetDesc target_desc{sizeof(AsterRenderTargetDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          ASTER_KERNEL_BACKEND_FORMAT_BGRA8_UNORM,
                                          ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT,
                                          64u,
                                          48u,
                                          1u,
                                          {"offscreen-main", 14u}};
  assert(aster_kernel_render_target_create(engine, &target_desc, &target).code ==
         ASTER_STATUS_OK);
  assert(aster_kernel_renderer_render_frame_to_target(renderer, scene, target, &camera, &settings)
             .code ==
         ASTER_STATUS_OK);

  const LegacyCameraDesc legacy_camera{sizeof(LegacyCameraDesc),
                                       ASTER_KERNEL_STRUCT_VERSION_1,
                                       {0.0f, 0.0f, 0.0f},
                                       0.0f,
                                       0.25f,
                                       5.0f,
                                       0.9f,
                                       0.01f,
                                       50.0f};
  const LegacyRendererSettings legacy_settings{sizeof(LegacyRendererSettings),
                                               ASTER_KERNEL_STRUCT_VERSION_1,
                                               {0.04f, 0.05f, 0.07f},
                                               1.0f,
                                               0.24f,
                                               64u,
                                               48u,
                                               0u,
                                               nullptr};
  assert(aster_kernel_renderer_render_frame_to_target(
             renderer, scene, target, reinterpret_cast<const AsterCameraDesc *>(&legacy_camera),
             reinterpret_cast<const AsterRendererSettings *>(&legacy_settings))
             .code == ASTER_STATUS_OK);

  AsterCameraDesc physical_camera = camera;
  physical_camera.focal_length_mm = 54.0f;
  physical_camera.sensor_width_mm = 36.0f;
  physical_camera.composition_weight = 0.66f;
  physical_camera.scale_reference_m = 2.0f;
  physical_camera.camera_flags = ASTER_KERNEL_CAMERA_FLAG_USE_PHYSICAL_LENS;
  AsterRendererSettings presentation_settings = settings;
  presentation_settings.flags =
      ASTER_KERNEL_RENDER_SETTING_CONTACT_SHADOWS |
      ASTER_KERNEL_RENDER_SETTING_SURFACE_OCCLUSION |
      ASTER_KERNEL_RENDER_SETTING_CASCADED_SHADOWS |
      ASTER_KERNEL_RENDER_SETTING_REFLECTION_PROBES |
      ASTER_KERNEL_RENDER_SETTING_VOLUMETRIC_FOG |
      ASTER_KERNEL_RENDER_SETTING_PROCEDURAL_SURFACE_NORMALS |
      ASTER_KERNEL_RENDER_SETTING_FXAA |
      ASTER_KERNEL_RENDER_SETTING_BLOOM |
      ASTER_KERNEL_RENDER_SETTING_PRESENTATION_LENS;
  presentation_settings.quality_tier = ASTER_KERNEL_RENDER_QUALITY_CINEMATIC;
  presentation_settings.tone_mapper = ASTER_KERNEL_TONE_MAPPER_FILMIC_ACES;
  presentation_settings.ambient_floor = 0.02f;
  presentation_settings.shadow_cascades = 4u;
  presentation_settings.shadow_atlas_size = 1024u;
  presentation_settings.shadow_max_distance = 80.0f;
  presentation_settings.shadow_receiver_bias = 0.010f;
  presentation_settings.shadow_normal_bias = 0.008f;
  presentation_settings.shadow_softness = 0.26f;
  presentation_settings.occlusion_radius = 1.35f;
  presentation_settings.occlusion_strength = 0.44f;
  presentation_settings.occlusion_sample_count = 16u;
  presentation_settings.occlusion_contact_hardening = 0.38f;
  presentation_settings.contact_shadow_strength = 0.55f;
  presentation_settings.contact_shadow_radius_scale = 1.22f;
  presentation_settings.contact_shadow_receiver_height = 1.35f;
  presentation_settings.contact_shadow_receiver_bias = 0.012f;
  presentation_settings.physical_texel_density = 768.0f;
  presentation_settings.macro_frequency_breakup = 0.42f;
  presentation_settings.micro_frequency_breakup = 0.58f;
  presentation_settings.height_normal_coupling = 0.90f;
  presentation_settings.roughness_height_coupling = 0.68f;
  presentation_settings.fog_start = 5.0f;
  presentation_settings.fog_end = 32.0f;
  presentation_settings.fog_strength = 0.18f;
  presentation_settings.reflection_intensity = 0.44f;
  presentation_settings.bloom_threshold = 1.9f;
  presentation_settings.bloom_intensity = 0.18f;
  presentation_settings.world_trace_hash = 0xA57E000000000011ull;
  presentation_settings.simulation_tick = 42u;
  presentation_settings.extraction_hash = 0xA57E000000000022ull;
  presentation_settings.asset_lineage_hash = 0xA57E000000000033ull;
  presentation_settings.world_transition_hash = 0xA57E000000000044ull;
  presentation_settings.actor_state_delta_hash = 0xA57E000000000055ull;
  presentation_settings.encounter_budget_hash = 0xA57E000000000066ull;
  presentation_settings.navigation_valid = 1u;
  presentation_settings.streaming_region_id = 0xA57E000000000077ull;
  presentation_settings.perceptual_salience_score = 0.82f;
  assert(aster_kernel_renderer_render_frame_to_target(renderer, scene, target, &physical_camera,
                                                      &presentation_settings)
             .code == ASTER_STATUS_OK);

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
  assert(detail_counts.world_trace_hash == presentation_settings.world_trace_hash);
  assert(detail_counts.simulation_tick == presentation_settings.simulation_tick);
  assert(detail_counts.extraction_hash == presentation_settings.extraction_hash);
  assert(detail_counts.asset_lineage_hash == presentation_settings.asset_lineage_hash);
  assert(detail_counts.world_extraction_provenance == ASTER_WORLD_EXTRACTION_WORLD_TRANSITION);
  assert(detail_counts.world_transition_hash == presentation_settings.world_transition_hash);
  assert(detail_counts.actor_state_delta_hash == presentation_settings.actor_state_delta_hash);
  assert(detail_counts.encounter_budget_hash == presentation_settings.encounter_budget_hash);
  assert(detail_counts.navigation_valid == presentation_settings.navigation_valid);
  assert(detail_counts.streaming_region_id == presentation_settings.streaming_region_id);
  assert(detail_counts.perceptual_salience_score ==
         presentation_settings.perceptual_salience_score);
  AsterFrameForensicsDetailCounts legacy_detail_counts{};
  legacy_detail_counts.size = offsetof(AsterFrameForensicsDetailCounts, world_trace_hash);
  legacy_detail_counts.version = ASTER_KERNEL_STRUCT_VERSION_1;
  assert(aster_kernel_renderer_frame_forensics_detail_counts(renderer, &legacy_detail_counts)
             .code == ASTER_STATUS_OK);
  assert(legacy_detail_counts.object_fate_count == detail_counts.object_fate_count);
  assert(legacy_detail_counts.world_trace_hash == 0u);
  bool saw_surface_occlusion_pass = false;
  bool saw_surface_attributes_capture = false;
  bool saw_surface_occlusion_capture = false;
  for (size_t index = 0u; index < detail_counts.pass_count; ++index) {
    AsterFramePassStats pass{sizeof(AsterFramePassStats), ASTER_KERNEL_STRUCT_VERSION_1};
    assert(aster_kernel_renderer_frame_pass_stats(renderer, index, &pass).code ==
           ASTER_STATUS_OK);
    saw_surface_occlusion_pass =
        saw_surface_occlusion_pass || pass.pass == ASTER_KERNEL_RENDER_PASS_SURFACE_OCCLUSION;
  }
  for (size_t index = 0u; index < detail_counts.debug_capture_count; ++index) {
    AsterFrameDebugCaptureInfo capture{sizeof(AsterFrameDebugCaptureInfo),
                                       ASTER_KERNEL_STRUCT_VERSION_1};
    assert(aster_kernel_renderer_debug_capture_info(renderer, index, &capture).code ==
           ASTER_STATUS_OK);
    if (capture.resource == ASTER_KERNEL_RENDER_RESOURCE_SURFACE_ATTRIBUTES) {
      saw_surface_attributes_capture =
          capture.available != 0u && capture.content_hash != 0u && capture.payload_size > 0u;
    }
    if (capture.resource == ASTER_KERNEL_RENDER_RESOURCE_SURFACE_OCCLUSION) {
      saw_surface_occlusion_capture =
          capture.available != 0u && capture.content_hash != 0u && capture.payload_size > 0u;
    }
  }
  assert(saw_surface_occlusion_pass);
  assert(saw_surface_attributes_capture);
  assert(saw_surface_occlusion_capture);
  AsterFramePassStats pass_stats{sizeof(AsterFramePassStats), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_frame_pass_stats(renderer, 0u, &pass_stats).code ==
         ASTER_STATUS_OK);
  assert(pass_stats.name.size > 0u);
  assert(pass_stats.render_target_width > 0u);
  assert(pass_stats.render_target_height > 0u);
  assert(pass_stats.estimated_bandwidth_bytes > 0u);
  assert(pass_stats.cpu_build_seconds >= 0.0);
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

  AsterFrameScheduleHandle schedule = nullptr;
  assert(aster_kernel_renderer_get_last_frame_schedule(renderer, &schedule).code ==
         ASTER_STATUS_OK);
  AsterFrameScheduleCounts schedule_counts{sizeof(AsterFrameScheduleCounts),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_counts(schedule, &schedule_counts).code ==
         ASTER_STATUS_OK);
  assert(schedule_counts.pass_count >= 1u);
  assert(schedule_counts.transition_count >= 1u);
  assert(schedule_counts.descriptor_layout_count >= 1u);
  assert(schedule_counts.pipeline_count >= 1u);
  assert(schedule_counts.transient_allocation_count >= 1u);
  assert(schedule_counts.timeline_count >= 1u);
  AsterFrameSchedulePassInfo schedule_pass{sizeof(AsterFrameSchedulePassInfo),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_pass(schedule, 0u, &schedule_pass).code ==
         ASTER_STATUS_OK);
  assert(schedule_pass.name.size > 0u);
  assert(schedule_pass.command_buffer_count >= 1u);
  assert(schedule_pass.signal_fence_value >= 1u);
  AsterFrameScheduleMemoryReport memory_report{sizeof(AsterFrameScheduleMemoryReport),
                                               ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_memory_report(schedule, &memory_report).code ==
         ASTER_STATUS_OK);
  assert(memory_report.resident_bytes >= memory_report.transient_bytes);
  AsterFrameScheduleDescriptorInfo descriptor_info{sizeof(AsterFrameScheduleDescriptorInfo),
                                                  ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_descriptor_layout(schedule, 0u, &descriptor_info).code ==
         ASTER_STATUS_OK);
  assert(descriptor_info.layout_hash != 0u);
  AsterFrameSchedulePipelineInfo pipeline_info{sizeof(AsterFrameSchedulePipelineInfo),
                                              ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_pipeline(schedule, 0u, &pipeline_info).code ==
         ASTER_STATUS_OK);
  assert(pipeline_info.cache_key != 0u);
  AsterFrameScheduleTransientAllocationInfo transient_info{
      sizeof(AsterFrameScheduleTransientAllocationInfo), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_transient_allocation(schedule, 0u, &transient_info).code ==
         ASTER_STATUS_OK);
  assert(transient_info.byte_size > 0u);
  AsterFrameScheduleTimelineInfo timeline_info{sizeof(AsterFrameScheduleTimelineInfo),
                                               ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_frame_schedule_timeline(schedule, 0u, &timeline_info).code ==
         ASTER_STATUS_OK);
  assert(timeline_info.submitted_value >= 1u);

  const std::filesystem::path capture_path =
      std::filesystem::temp_directory_path() / "aster_kernel_renderer_abi5.ppm";
  const std::string capture_string = capture_path.string();
  const AsterCaptureDesc capture{sizeof(AsterCaptureDesc),
                                 ASTER_KERNEL_STRUCT_VERSION_1,
                                 {capture_string.data(), capture_string.size()},
                                 64u,
                                 48u};
  assert(aster_kernel_renderer_capture_render_target(renderer, target, &capture).code ==
         ASTER_STATUS_OK);
  assert(std::filesystem::exists(capture_path));
  std::filesystem::remove(capture_path);

  AsterPresentDesc present_desc{sizeof(AsterPresentDesc), ASTER_KERNEL_STRUCT_VERSION_1, 1u, 1u};
  AsterPresentResult present_result{sizeof(AsterPresentResult), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_renderer_present_frame(renderer, window, &present_desc, &present_result)
             .code == ASTER_STATUS_OK);
  assert(present_result.backend == capabilities.backend);
  assert(present_result.presentation == table.presentation);
  assert(present_result.presented == 0u);
  assert(aster_kernel_renderer_present(renderer, window).code == ASTER_STATUS_OK);
  assert(aster_kernel_frame_schedule_destroy(schedule).code == ASTER_STATUS_OK);
  assert(aster_kernel_render_target_destroy(target).code == ASTER_STATUS_OK);
  assert(aster_kernel_pipeline_cache_destroy(pipeline_cache).code == ASTER_STATUS_OK);
  assert(aster_kernel_descriptor_set_destroy(descriptor_set).code == ASTER_STATUS_OK);
  assert(aster_kernel_descriptor_heap_destroy(descriptor_heap).code == ASTER_STATUS_OK);
  assert(aster_kernel_buffer_destroy(frame_buffer).code == ASTER_STATUS_OK);
  assert(aster_kernel_material_destroy(material).code == ASTER_STATUS_OK);
  assert(aster_kernel_mesh_destroy(mesh).code == ASTER_STATUS_OK);
  assert(aster_kernel_scene_destroy(scene).code == ASTER_STATUS_OK);
  assert(aster_kernel_renderer_destroy(renderer).code == ASTER_STATUS_OK);
  assert(aster_kernel_window_destroy(window).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
}

void testAbi5ExplicitValidationContracts() {
  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"kernel-validation-test", 22u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);

  AsterTextureHandle bad_albedo = nullptr;
  const AsterTextureDesc bad_albedo_desc{sizeof(AsterTextureDesc),
                                         ASTER_KERNEL_STRUCT_VERSION_1,
                                         ASTER_TEXTURE_ROLE_ALBEDO,
                                         ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                                         ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                                         ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM,
                                         4u,
                                         4u,
                                         1u,
                                         {},
                                         {"bad-albedo", 10u}};
  assert(aster_kernel_texture_create(engine, &bad_albedo_desc, &bad_albedo).code ==
         ASTER_STATUS_VALIDATION_ERROR);
  size_t engine_validation_count = 0u;
  assert(aster_kernel_engine_validation_event_count(engine, &engine_validation_count).code ==
         ASTER_STATUS_OK);
  assert(engine_validation_count >= 1u);
  AsterValidationEvent validation{sizeof(AsterValidationEvent),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_engine_validation_event(engine, engine_validation_count - 1u,
                                             &validation)
             .code == ASTER_STATUS_OK);
  assert(validation.kind == ASTER_VALIDATION_TEXTURE_COLOR_SPACE_MISMATCH);

  AsterTextureHandle bad_normal = nullptr;
  const AsterTextureDesc bad_normal_desc{sizeof(AsterTextureDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        ASTER_TEXTURE_ROLE_NORMAL,
                                        ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                                        ASTER_TEXTURE_NORMAL_CONVENTION_DIRECTX,
                                        ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM,
                                        4u,
                                        4u,
                                        1u,
                                        {},
                                        {"bad-normal", 10u}};
  assert(aster_kernel_texture_create(engine, &bad_normal_desc, &bad_normal).code ==
         ASTER_STATUS_VALIDATION_ERROR);

  AsterTextureHandle albedo = nullptr;
  const AsterTextureDesc albedo_desc{sizeof(AsterTextureDesc),
                                     ASTER_KERNEL_STRUCT_VERSION_1,
                                     ASTER_TEXTURE_ROLE_ALBEDO,
                                     ASTER_TEXTURE_COLOR_SPACE_SRGB,
                                     ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                                     ASTER_KERNEL_BACKEND_FORMAT_RGBA8_SRGB,
                                     4u,
                                     4u,
                                     1u,
                                     {},
                                     {"albedo", 6u}};
  assert(aster_kernel_texture_create(engine, &albedo_desc, &albedo).code == ASTER_STATUS_OK);
  AsterTextureHandle normal = nullptr;
  const AsterTextureDesc normal_desc{sizeof(AsterTextureDesc),
                                     ASTER_KERNEL_STRUCT_VERSION_1,
                                     ASTER_TEXTURE_ROLE_NORMAL,
                                     ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                                     ASTER_TEXTURE_NORMAL_CONVENTION_OPENGL,
                                     ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM,
                                     4u,
                                     4u,
                                     1u,
                                     {},
                                     {"normal", 6u}};
  assert(aster_kernel_texture_create(engine, &normal_desc, &normal).code == ASTER_STATUS_OK);
  AsterTextureHandle orm = nullptr;
  const AsterTextureDesc orm_desc{sizeof(AsterTextureDesc),
                                  ASTER_KERNEL_STRUCT_VERSION_1,
                                  ASTER_TEXTURE_ROLE_ORM,
                                  ASTER_TEXTURE_COLOR_SPACE_LINEAR,
                                  ASTER_TEXTURE_NORMAL_CONVENTION_NONE,
                                  ASTER_KERNEL_BACKEND_FORMAT_RGBA8_UNORM,
                                  4u,
                                  4u,
                                  1u,
                                  {},
                                  {"orm", 3u}};
  assert(aster_kernel_texture_create(engine, &orm_desc, &orm).code == ASTER_STATUS_OK);

  const AsterMaterialTextureBinding missing_bindings[] = {
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_ALBEDO, albedo},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_NORMAL, normal},
  };
  const AsterMaterialDesc missing_orm_desc{sizeof(AsterMaterialDesc),
                                           ASTER_KERNEL_STRUCT_VERSION_1,
                                           {1.0f, 1.0f, 1.0f},
                                           {0.0f, 0.0f, 0.0f},
                                           0.5f,
                                           0.0f,
                                           0.0f,
                                           1.0f,
                                           ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                           0u,
                                           {"missing-orm", 11u},
                                           {missing_bindings, 2u,
                                            sizeof(AsterMaterialTextureBinding)}};
  AsterMaterialHandle material = nullptr;
  assert(aster_kernel_material_create(engine, &missing_orm_desc, &material).code ==
         ASTER_STATUS_VALIDATION_ERROR);

  const AsterMaterialTextureBinding pbr_bindings[] = {
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_ALBEDO, albedo},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1,
       ASTER_TEXTURE_ROLE_NORMAL, normal},
      {sizeof(AsterMaterialTextureBinding), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_TEXTURE_ROLE_ORM,
       orm},
  };
  const AsterMaterialDesc pbr_desc{sizeof(AsterMaterialDesc),
                                   ASTER_KERNEL_STRUCT_VERSION_1,
                                   {1.0f, 1.0f, 1.0f},
                                   {0.0f, 0.0f, 0.0f},
                                   0.5f,
                                   0.0f,
                                   0.0f,
                                   1.0f,
                                   ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                   0u,
                                   {"lit-pbr", 7u},
                                   {pbr_bindings, 3u,
                                    sizeof(AsterMaterialTextureBinding)}};
  assert(aster_kernel_material_create(engine, &pbr_desc, &material).code == ASTER_STATUS_OK);
  assert(aster_kernel_material_destroy(material).code == ASTER_STATUS_OK);

  assert(aster_kernel_texture_destroy(orm).code == ASTER_STATUS_OK);
  material = nullptr;
  assert(aster_kernel_material_create(engine, &pbr_desc, &material).code ==
         ASTER_STATUS_LIFETIME_ERROR);

  const AsterVertex bad_vertices[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
      {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
      {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f},
  };
  const std::uint32_t bad_indices[] = {0u, 1u, 2u};
  const AsterMeshDesc bad_mesh_desc{sizeof(AsterMeshDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                    {bad_vertices, 3u, sizeof(AsterVertex)},
                                    {bad_indices, 3u, sizeof(std::uint32_t)},
                                    {"bad-mesh", 8u}};
  AsterMeshHandle bad_mesh = nullptr;
  assert(aster_kernel_mesh_create(engine, &bad_mesh_desc, &bad_mesh).code ==
         ASTER_STATUS_VALIDATION_ERROR);

  AsterMaterialHandle plain_material = nullptr;
  const AsterMaterialDesc plain_desc{sizeof(AsterMaterialDesc),
                                     ASTER_KERNEL_STRUCT_VERSION_1,
                                     {0.8f, 0.8f, 0.8f},
                                     {0.0f, 0.0f, 0.0f},
                                     0.55f,
                                     0.0f,
                                     0.0f,
                                     1.0f,
                                     ASTER_KERNEL_MATERIAL_ALPHA_OPAQUE,
                                     0u,
                                     {"plain", 5u}};
  assert(aster_kernel_material_create(engine, &plain_desc, &plain_material).code ==
         ASTER_STATUS_OK);
  AsterSceneHandle scene = nullptr;
  assert(aster_kernel_scene_create(engine, &scene).code == ASTER_STATUS_OK);
  const AsterSceneObjectDesc zero_scale_object{sizeof(AsterSceneObjectDesc),
                                               ASTER_KERNEL_STRUCT_VERSION_1,
                                               nullptr,
                                               plain_material,
                                               nullptr,
                                               ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                               {0.0f, 0.0f, 0.0f},
                                               {0.0f, 0.0f, 0.0f},
                                               {0.0f, 1.0f, 1.0f},
                                               {"zero-scale", 10u}};
  assert(aster_kernel_scene_add_object(scene, &zero_scale_object).code ==
         ASTER_STATUS_VALIDATION_ERROR);

  AsterWindowHandle window = nullptr;
  const AsterWindowDesc window_desc{sizeof(AsterWindowDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"headless", 8u},
                                    32u,
                                    24u,
                                    ASTER_KERNEL_WINDOW_FLAG_HEADLESS,
                                    0u};
  assert(aster_kernel_window_create(&window_desc, &window).code == ASTER_STATUS_OK);
  AsterRendererHandle renderer = nullptr;
  const AsterRendererDesc renderer_desc{sizeof(AsterRendererDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        window,
                                        ASTER_KERNEL_BACKEND_SOFTWARE_REFERENCE,
                                        ASTER_KERNEL_RENDERER_FLAG_FORCE_SOFTWARE};
  assert(aster_kernel_renderer_create(engine, &renderer_desc, &renderer).code == ASTER_STATUS_OK);
  const AsterSceneObjectDesc valid_object{sizeof(AsterSceneObjectDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          nullptr,
                                          plain_material,
                                          nullptr,
                                          ASTER_KERNEL_MESH_PRIMITIVE_BOX,
                                          {0.0f, 0.0f, 0.0f},
                                          {0.0f, 0.0f, 0.0f},
                                          {1.0f, 1.0f, 1.0f},
                                          {"valid-object", 12u}};
  assert(aster_kernel_scene_add_object(scene, &valid_object).code == ASTER_STATUS_OK);
  AsterRenderTargetHandle bad_target = nullptr;
  const AsterRenderTargetDesc bad_target_desc{sizeof(AsterRenderTargetDesc),
                                              ASTER_KERNEL_STRUCT_VERSION_1,
                                              ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT,
                                              ASTER_KERNEL_BACKEND_FORMAT_DEPTH32_FLOAT,
                                              32u,
                                              24u,
                                              1u,
                                              {"bad-target", 10u}};
  assert(aster_kernel_render_target_create(engine, &bad_target_desc, &bad_target).code ==
         ASTER_STATUS_OK);
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
                                       {0.0f, 0.0f, 0.0f},
                                       1.0f,
                                       0.2f,
                                       32u,
                                       24u,
                                       0u};
  assert(aster_kernel_renderer_render_frame_to_target(renderer, scene, bad_target, &camera,
                                                      &settings)
             .code == ASTER_STATUS_CAPABILITY_MISMATCH);

  assert(aster_kernel_render_target_destroy(bad_target).code == ASTER_STATUS_OK);
  assert(aster_kernel_renderer_destroy(renderer).code == ASTER_STATUS_OK);
  assert(aster_kernel_window_destroy(window).code == ASTER_STATUS_OK);
  assert(aster_kernel_scene_destroy(scene).code == ASTER_STATUS_OK);
  assert(aster_kernel_material_destroy(plain_material).code == ASTER_STATUS_OK);
  assert(aster_kernel_texture_destroy(normal).code == ASTER_STATUS_OK);
  assert(aster_kernel_texture_destroy(albedo).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
}

void testAuthoringDocumentAbi51Contracts() {
  const std::filesystem::path fixture_root =
      std::filesystem::path(ASTER_SOURCE_DIR) / "tests" / "fixtures" / "gameplay_contract";
  const std::string project_path = (fixture_root / "demo.asterproj").string();
  AsterAuthoringDocumentHandle project = nullptr;
  const AsterAuthoringDocumentDesc project_desc{sizeof(AsterAuthoringDocumentDesc),
                                                ASTER_KERNEL_STRUCT_VERSION_1,
                                                ASTER_AUTHORING_DOCUMENT_PROJECT,
                                                {},
                                                {project_path.data(), project_path.size()},
                                                {"fixture-project", 15u}};
  assert(aster_kernel_authoring_document_load(&project_desc, &project).code == ASTER_STATUS_OK);
  AsterAuthoringDocumentInfo project_info{sizeof(AsterAuthoringDocumentInfo),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_info(project, &project_info).code == ASTER_STATUS_OK);
  assert(project_info.kind == ASTER_AUTHORING_DOCUMENT_PROJECT);
  assert(project_info.valid == 1u);
  assert(toString(project_info.name) == "Aster Gameplay Contract Demo");
  assert(project_info.project_asset_count == 3u);
  AsterAuthoringProjectAssetInfo project_asset{sizeof(AsterAuthoringProjectAssetInfo),
                                               ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_project_asset(project, 0u, &project_asset).code ==
         ASTER_STATUS_OK);
  assert(project_asset.kind == ASTER_AUTHORING_ASSET_SCENE);
  assert(project_asset.startup == 1u);

  const std::string scene_path = (fixture_root / "scenes" / "entry.scene").string();
  AsterAuthoringDocumentHandle scene = nullptr;
  const AsterAuthoringDocumentDesc scene_desc{sizeof(AsterAuthoringDocumentDesc),
                                              ASTER_KERNEL_STRUCT_VERSION_1,
                                              ASTER_AUTHORING_DOCUMENT_SCENE,
                                              {},
                                              {scene_path.data(), scene_path.size()},
                                              {}};
  assert(aster_kernel_authoring_document_load(&scene_desc, &scene).code == ASTER_STATUS_OK);
  AsterAuthoringDocumentInfo scene_info{sizeof(AsterAuthoringDocumentInfo),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_info(scene, &scene_info).code == ASTER_STATUS_OK);
  assert(scene_info.entity_count == 2u);
  AsterAuthoringEntityInfo entity{sizeof(AsterAuthoringEntityInfo),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_entity(scene, 1u, &entity).code == ASTER_STATUS_OK);
  assert(toString(entity.id) == "demo.door");
  assert((entity.component_flags & ASTER_AUTHORING_ENTITY_COMPONENT_INTERACTABLE) != 0u);

  const std::string action_path = (fixture_root / "actions" / "door_open.action_graph").string();
  AsterAuthoringDocumentHandle action = nullptr;
  const AsterAuthoringDocumentDesc action_desc{sizeof(AsterAuthoringDocumentDesc),
                                               ASTER_KERNEL_STRUCT_VERSION_1,
                                               ASTER_AUTHORING_DOCUMENT_ACTION_GRAPH,
                                               {},
                                               {action_path.data(), action_path.size()},
                                               {}};
  assert(aster_kernel_authoring_document_load(&action_desc, &action).code == ASTER_STATUS_OK);
  AsterAuthoringDocumentInfo action_info{sizeof(AsterAuthoringDocumentInfo),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_info(action, &action_info).code == ASTER_STATUS_OK);
  assert(action_info.action_node_count == 2u);
  assert(action_info.contract_stamp != 0u);
  AsterAuthoringActionNodeInfo node{sizeof(AsterAuthoringActionNodeInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_action_node(action, 0u, &node).code == ASTER_STATUS_OK);
  assert(toString(node.type) == "emit_event");
  assert(node.parameter_count == 1u);
  assert(node.deterministic_stamp != 0u);
  AsterAuthoringKeyValue parameter{sizeof(AsterAuthoringKeyValue),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_action_node_parameter(action, 0u, 0u, &parameter).code ==
         ASTER_STATUS_OK);
  assert(toString(parameter.key) == "event");
  assert(toString(parameter.value) == "door.open_requested");
  AsterStringView node_tag{};
  assert(aster_kernel_authoring_action_node_tag(action, 0u, 0u, &node_tag).code ==
         ASTER_STATUS_OK);
  assert(toString(node_tag) == "interaction.door");

  const AsterAuthoringActionContext context{sizeof(AsterAuthoringActionContext),
                                            ASTER_KERNEL_STRUCT_VERSION_1,
                                            {"demo.player", 11u},
                                            {"demo.door", 9u},
                                            {"world.interact", 14u}};
  AsterAuthoringActionExecutionHandle execution = nullptr;
  assert(aster_kernel_authoring_action_execute(action, &context, &execution).code ==
         ASTER_STATUS_OK);
  AsterAuthoringActionExecutionInfo execution_info{sizeof(AsterAuthoringActionExecutionInfo),
                                                   ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_action_execution_info(execution, &execution_info).code ==
         ASTER_STATUS_OK);
  assert(execution_info.valid == 1u);
  assert(execution_info.event_count == 2u);
  assert(execution_info.contract_stamp == action_info.contract_stamp);
  AsterAuthoringActionEventInfo event{sizeof(AsterAuthoringActionEventInfo),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_action_event(execution, 0u, &event).code == ASTER_STATUS_OK);
  assert(toString(event.actor) == "demo.player");
  assert(toString(event.target) == "demo.door");
  assert(event.parameter_count == 2u);
  assert(event.deterministic_stamp != 0u);

  const std::string input_path = (fixture_root / "inputs" / "demo.input").string();
  AsterAuthoringDocumentHandle input = nullptr;
  const AsterAuthoringDocumentDesc input_desc{sizeof(AsterAuthoringDocumentDesc),
                                              ASTER_KERNEL_STRUCT_VERSION_1,
                                              ASTER_AUTHORING_DOCUMENT_INPUT_MAP,
                                              {},
                                              {input_path.data(), input_path.size()},
                                              {}};
  assert(aster_kernel_authoring_document_load(&input_desc, &input).code == ASTER_STATUS_OK);
  AsterAuthoringDocumentInfo input_info{sizeof(AsterAuthoringDocumentInfo),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_info(input, &input_info).code == ASTER_STATUS_OK);
  assert(input_info.input_binding_count == 2u);
  assert(input_info.contract_stamp != 0u);
  AsterAuthoringInputBindingInfo binding{sizeof(AsterAuthoringInputBindingInfo),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_input_binding(input, 0u, &binding).code == ASTER_STATUS_OK);
  assert(toString(binding.command) == "world.interact");
  assert(binding.device == ASTER_AUTHORING_INPUT_KEYBOARD);
  assert(toString(binding.key) == "E");
  assert(binding.tag_count == 1u);
  assert(binding.deterministic_stamp != 0u);
  AsterStringView binding_tag{};
  assert(aster_kernel_authoring_input_binding_tag(input, 0u, 0u, &binding_tag).code ==
         ASTER_STATUS_OK);
  assert(toString(binding_tag) == "input.primary");

  const char *bad_input = R"json({
    "schema_version": 1,
    "id": "input.bad",
    "bindings": [
      { "command": "world.interact", "device": "mouse" }
    ]
  })json";
  AsterAuthoringDocumentHandle invalid_input = nullptr;
  const AsterAuthoringDocumentDesc bad_desc{sizeof(AsterAuthoringDocumentDesc),
                                            ASTER_KERNEL_STRUCT_VERSION_1,
                                            ASTER_AUTHORING_DOCUMENT_INPUT_MAP,
                                            {bad_input, std::strlen(bad_input)},
                                            {"bad.input", 9u},
                                            {}};
  assert(aster_kernel_authoring_document_load(&bad_desc, &invalid_input).code ==
         ASTER_STATUS_OK);
  AsterAuthoringDocumentInfo bad_info{sizeof(AsterAuthoringDocumentInfo),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_info(invalid_input, &bad_info).code == ASTER_STATUS_OK);
  assert(bad_info.valid == 0u);
  assert(bad_info.diagnostic_count >= 1u);
  AsterAuthoringDiagnosticInfo diagnostic{sizeof(AsterAuthoringDiagnosticInfo),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_authoring_document_diagnostic(invalid_input, 0u, &diagnostic).code ==
         ASTER_STATUS_OK);
  assert(diagnostic.severity == ASTER_AUTHORING_DIAGNOSTIC_ERROR);
  assert(toString(diagnostic.message).find("button") != std::string::npos);

  assert(aster_kernel_authoring_document_destroy(invalid_input).code == ASTER_STATUS_OK);
  assert(aster_kernel_authoring_document_destroy(input).code == ASTER_STATUS_OK);
  assert(aster_kernel_authoring_action_execution_destroy(execution).code == ASTER_STATUS_OK);
  assert(aster_kernel_authoring_document_destroy(action).code == ASTER_STATUS_OK);
  assert(aster_kernel_authoring_document_destroy(scene).code == ASTER_STATUS_OK);
  assert(aster_kernel_authoring_document_destroy(project).code == ASTER_STATUS_OK);
}

void testShaderCompilerAbi5() {
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
                                            {"abi5-test", 9u},
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

std::uint64_t worldTransitionHashForSeed(const std::uint64_t seed) {
  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"world-transition-test", 21u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);
  AsterWorldHandle world = nullptr;
  const AsterWorldDesc world_desc{sizeof(AsterWorldDesc), ASTER_KERNEL_STRUCT_VERSION_1,
                                  1.0 / 60.0, seed, {"player-world", 12u}};
  assert(aster_kernel_world_create(engine, &world_desc, &world).code == ASTER_STATUS_OK);
  const AsterWorldAdvanceDesc advance{sizeof(AsterWorldAdvanceDesc),
                                      ASTER_KERNEL_STRUCT_VERSION_1,
                                      1u,
                                      1.0 / 60.0,
                                      0x11u,
                                      0x22u,
                                      0x33u,
                                      0x44u,
                                      0x55u,
                                      0x66u,
                                      0x77u};
  AsterWorldAdvanceResult result{sizeof(AsterWorldAdvanceResult),
                                 ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_world_advance(world, &advance, &result).code == ASTER_STATUS_OK);
  const std::uint64_t transition_hash = result.world_transition_hash;
  assert(aster_kernel_world_destroy(world).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
  return transition_hash;
}

void testWorldRootAbi6Contracts() {
  const std::uint64_t deterministic_hash = worldTransitionHashForSeed(0xA57E6000u);
  assert(deterministic_hash != 0u);
  assert(deterministic_hash == worldTransitionHashForSeed(0xA57E6000u));

  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"world-root-test", 15u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);
  AsterWorldHandle world = nullptr;
  const AsterWorldDesc world_desc{sizeof(AsterWorldDesc), ASTER_KERNEL_STRUCT_VERSION_1,
                                  1.0 / 60.0, 0xA57E6001u, {"world-root", 10u}};
  assert(aster_kernel_world_create(engine, &world_desc, &world).code == ASTER_STATUS_OK);

  const AsterWorldAdvanceDesc advance{sizeof(AsterWorldAdvanceDesc),
                                      ASTER_KERNEL_STRUCT_VERSION_1,
                                      1u,
                                      1.0 / 60.0,
                                      0x100u,
                                      0x200u,
                                      0x300u,
                                      0x400u,
                                      0x500u,
                                      0x600u,
                                      0x700u};
  AsterWorldAdvanceResult result{sizeof(AsterWorldAdvanceResult),
                                 ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_world_advance(world, &advance, &result).code == ASTER_STATUS_OK);
  assert(result.accepted == 1u);
  assert(result.world_transition_hash != 0u);

  const AsterWorldRegionGateReport rejected_gate{
      sizeof(AsterWorldRegionGateReport),
      ASTER_KERNEL_STRUCT_VERSION_1,
      0x700u,
      0x701u,
      ASTER_WORLD_REGION_GATE_ACCEPTED,
      {sizeof(AsterNavValidityReport), ASTER_KERNEL_STRUCT_VERSION_1, 0u, 4u, 1u, 0x702u,
       {"blocked", 7u}},
      0x703u,
      0x704u,
      {sizeof(AsterPerceptualBudget), ASTER_KERNEL_STRUCT_VERSION_1, 1u, 0.84f, 0.60f, 0x705u,
       {"readable", 8u}},
      {"route blocked", 13u}};
  assert(aster_kernel_world_record_region_gate(world, &rejected_gate).code == ASTER_STATUS_OK);

  const AsterWorldRenderExtractionDesc extraction_desc{
      sizeof(AsterWorldRenderExtractionDesc), ASTER_KERNEL_STRUCT_VERSION_1, 0x500u, 0x801u,
      0x802u};
  AsterWorldRenderExtraction extraction{sizeof(AsterWorldRenderExtraction),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_world_extract_render(world, &extraction_desc, &extraction).code ==
         ASTER_STATUS_OK);
  assert(extraction.world_transition_hash != 0u);
  assert(extraction.streaming_region_id == rejected_gate.region_id);

  AsterWorldForensics forensics{sizeof(AsterWorldForensics), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_world_forensics(world, &forensics).code == ASTER_STATUS_OK);
  assert(forensics.generated_region_gate == ASTER_WORLD_REGION_GATE_QUARANTINED);
  assert(forensics.navigation.valid == 0u);
  assert(forensics.navigation.blocked_steps == 1u);
  assert(forensics.actor_delta_count == 1u);
  assert(forensics.render_extraction_hash == extraction_desc.extraction_hash);
  assert(forensics.trace_hash == extraction.trace_hash);
  assert(aster_kernel_world_destroy(world).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
}

void testSystemWorldCompatibilityContracts() {
  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc engine_desc{sizeof(AsterEngineDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    {"world-contract-test", 19u},
                                    0u};
  assert(aster_kernel_engine_create(&engine_desc, &engine).code == ASTER_STATUS_OK);

  AsterSystemWorldHandle world = nullptr;
  const AsterSystemWorldDesc world_desc{sizeof(AsterSystemWorldDesc),
                                        ASTER_KERNEL_STRUCT_VERSION_1,
                                        1.0 / 60.0,
                                        0xA57E5300u,
                                        {"kernel-world", 12u}};
  assert(aster_kernel_system_world_create(engine, &world_desc, &world).code == ASTER_STATUS_OK);

  AsterSystemEntityHandle entity{};
  assert(aster_kernel_system_world_entity_create(world, {"entity.player", 13u}, &entity).code ==
         ASTER_STATUS_OK);
  assert(entity.id != 0u);
  assert(entity.generation == 1u);
  AsterSystemEntityInfo entity_info{sizeof(AsterSystemEntityInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_entity_query(world, entity, &entity_info).code ==
         ASTER_STATUS_OK);
  assert(entity_info.alive == 1u);
  assert(toString(entity_info.label) == "entity.player");

  const AsterSystemTickDesc tick_1{sizeof(AsterSystemTickDesc),
                                   ASTER_KERNEL_STRUCT_VERSION_1,
                                   1u,
                                   1.0 / 60.0,
                                   0x10u,
                                   0x20u,
                                   0x30u,
                                   0x40u};
  AsterSystemTickResult tick_result{sizeof(AsterSystemTickResult),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_tick(world, &tick_1, &tick_result).code == ASTER_STATUS_OK);
  assert(tick_result.accepted == 1u);
  assert(tick_result.tick == 1u);
  assert(tick_result.world_hash != 0u);
  assert(tick_result.trace_hash != 0u);
  assert(aster_kernel_system_world_tick(world, &tick_1, &tick_result).code ==
         ASTER_STATUS_VALIDATION_ERROR);
  assert(tick_result.accepted == 0u);

  const AsterSystemComponentAccess read_transform{sizeof(AsterSystemComponentAccess),
                                                  ASTER_KERNEL_STRUCT_VERSION_1,
                                                  {"Transform", 9u},
                                                  {"entity.player", 13u},
                                                  ASTER_SYSTEM_COMPONENT_ACCESS_READ};
  const AsterSystemTransactionDesc read_tx{sizeof(AsterSystemTransactionDesc),
                                           ASTER_KERNEL_STRUCT_VERSION_1,
                                           {"read-transform", 14u},
                                           {"movement-system", 15u},
                                           {&read_transform, 1u, sizeof(read_transform)}};
  AsterSystemTransactionInfo tx_info{sizeof(AsterSystemTransactionInfo),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_transaction_begin(world, &read_tx, &tx_info).code ==
         ASTER_STATUS_OK);
  const uint64_t read_tx_id = tx_info.transaction_id;
  assert(aster_kernel_system_world_transaction_commit(world, read_tx_id, &tx_info).code ==
         ASTER_STATUS_OK);
  assert(tx_info.committed == 1u);
  const uint64_t after_read_hash = tx_info.post_world_hash;

  const AsterSystemComponentAccess write_transform{sizeof(AsterSystemComponentAccess),
                                                   ASTER_KERNEL_STRUCT_VERSION_1,
                                                   {"Transform", 9u},
                                                   {"entity.player", 13u},
                                                   ASTER_SYSTEM_COMPONENT_ACCESS_WRITE};
  const AsterSystemTransactionDesc write_tx{sizeof(AsterSystemTransactionDesc),
                                            ASTER_KERNEL_STRUCT_VERSION_1,
                                            {"write-transform", 15u},
                                            {"animation-system", 16u},
                                            {&write_transform, 1u, sizeof(write_transform)}};
  assert(aster_kernel_system_world_transaction_begin(world, &write_tx, &tx_info).code ==
         ASTER_STATUS_OK);
  assert(aster_kernel_system_world_transaction_commit(world, tx_info.transaction_id, &tx_info)
             .code == ASTER_STATUS_VALIDATION_ERROR);
  assert(tx_info.committed == 0u);
  assert(toString(tx_info.diagnostic).find("component access hazard") != std::string::npos);

  AsterSystemTraceCounts counts{sizeof(AsterSystemTraceCounts), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_trace_counts(world, &counts).code == ASTER_STATUS_OK);
  assert(counts.tick == 1u);
  assert(counts.entity_count == 1u);
  assert(counts.live_entity_count == 1u);
  assert(counts.transaction_count == 2u);
  assert(counts.validation_event_count >= 2u);
  assert(counts.world_hash == after_read_hash);
  bool saw_scheduler_hazard = false;
  for (size_t index = 0u; index < counts.event_count; ++index) {
    AsterSystemTraceEvent event{sizeof(AsterSystemTraceEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    assert(aster_kernel_system_world_trace_event(world, index, &event).code == ASTER_STATUS_OK);
    if (event.kind == ASTER_SYSTEM_TRACE_SCHEDULER_DECISION &&
        toString(event.detail).find("component access hazard") != std::string::npos) {
      saw_scheduler_hazard = true;
    }
  }
  assert(saw_scheduler_hazard);

  const std::filesystem::path snapshot_path =
      std::filesystem::temp_directory_path() / "aster_kernel_world_snapshot_v53.txt";
  const std::string snapshot = snapshot_path.string();
  const AsterWorldSnapshotDesc snapshot_desc{sizeof(AsterWorldSnapshotDesc),
                                             ASTER_KERNEL_STRUCT_VERSION_1,
                                             {snapshot.data(), snapshot.size()},
                                             counts.world_hash};
  assert(aster_kernel_system_world_save_snapshot(world, &snapshot_desc).code == ASTER_STATUS_OK);
  AsterWorldReplayReport replay{sizeof(AsterWorldReplayReport), ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_replay_trace(world, &snapshot_desc, &replay).code ==
         ASTER_STATUS_OK);
  assert(replay.matched == 1u);
  assert(replay.actual_world_hash == counts.world_hash);

  AsterSystemWorldHandle loaded_world = nullptr;
  assert(aster_kernel_system_world_create(engine, &world_desc, &loaded_world).code ==
         ASTER_STATUS_OK);
  AsterWorldMigrationReport migration{sizeof(AsterWorldMigrationReport),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
  assert(aster_kernel_system_world_load_snapshot(loaded_world, &snapshot_desc, &migration).code ==
         ASTER_STATUS_OK);
  assert(migration.current_schema_version == 1u);
  assert(migration.entity_count == 1u);
  assert(migration.world_hash == counts.world_hash);

  assert(aster_kernel_system_world_destroy(loaded_world).code == ASTER_STATUS_OK);
  assert(aster_kernel_system_world_entity_destroy(world, entity).code == ASTER_STATUS_OK);
  assert(aster_kernel_system_world_entity_destroy(world, entity).code == ASTER_STATUS_LIFETIME_ERROR);
  assert(aster_kernel_system_world_destroy(world).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);
  std::filesystem::remove(snapshot_path);
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
      "aster_kernel_engine_validation_event_count",
      "aster_kernel_engine_validation_event",
      "aster_kernel_world_create",
      "aster_kernel_world_advance",
      "aster_kernel_world_record_region_gate",
      "aster_kernel_world_extract_render",
      "aster_kernel_world_forensics",
      "aster_kernel_world_destroy",
      "aster_kernel_system_world_create",
      "aster_kernel_system_world_tick",
      "aster_kernel_system_world_entity_create",
      "aster_kernel_system_world_entity_query",
      "aster_kernel_system_world_entity_destroy",
      "aster_kernel_system_world_transaction_begin",
      "aster_kernel_system_world_transaction_append",
      "aster_kernel_system_world_transaction_commit",
      "aster_kernel_system_world_transaction_abort",
      "aster_kernel_system_world_trace_counts",
      "aster_kernel_system_world_trace_event",
      "aster_kernel_system_world_save_snapshot",
      "aster_kernel_system_world_load_snapshot",
      "aster_kernel_system_world_replay_trace",
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
      "aster_kernel_renderer_render_frame_to_target",
      "aster_kernel_renderer_bind_window",
      "aster_kernel_renderer_present",
      "aster_kernel_renderer_present_frame",
      "aster_kernel_renderer_presentation_status",
      "aster_kernel_renderer_capture",
      "aster_kernel_renderer_capture_render_target",
      "aster_kernel_renderer_last_stats",
      "aster_kernel_renderer_validation_event_count",
      "aster_kernel_renderer_validation_event",
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
      "aster_kernel_renderer_object_render_fate",
      "aster_kernel_renderer_get_last_frame_schedule",
      "aster_kernel_renderer_destroy",
      "aster_kernel_mesh_create",
      "aster_kernel_mesh_destroy",
      "aster_kernel_material_create",
      "aster_kernel_material_destroy",
      "aster_kernel_texture_create",
      "aster_kernel_texture_destroy",
      "aster_kernel_render_target_create",
      "aster_kernel_render_target_destroy",
      "aster_kernel_buffer_create",
      "aster_kernel_buffer_destroy",
      "aster_kernel_descriptor_heap_create",
      "aster_kernel_descriptor_heap_destroy",
      "aster_kernel_descriptor_set_create",
      "aster_kernel_descriptor_set_destroy",
      "aster_kernel_pipeline_cache_create",
      "aster_kernel_pipeline_cache_destroy",
      "aster_kernel_frame_schedule_counts",
      "aster_kernel_frame_schedule_pass",
      "aster_kernel_frame_schedule_memory_report",
      "aster_kernel_frame_schedule_descriptor_layout",
      "aster_kernel_frame_schedule_pipeline",
      "aster_kernel_frame_schedule_transient_allocation",
      "aster_kernel_frame_schedule_timeline",
      "aster_kernel_frame_schedule_validation_event",
      "aster_kernel_frame_schedule_destroy",
      "aster_kernel_shader_compile",
      "aster_kernel_shader_get_result",
      "aster_kernel_shader_get_source",
      "aster_kernel_shader_get_diagnostics",
      "aster_kernel_shader_get_reflection",
      "aster_kernel_shader_destroy",
      "aster_kernel_render_pipeline_create",
      "aster_kernel_render_pipeline_destroy",
      "aster_kernel_authoring_document_load",
      "aster_kernel_authoring_document_info",
      "aster_kernel_authoring_document_diagnostic",
      "aster_kernel_authoring_project_asset",
      "aster_kernel_authoring_entity",
      "aster_kernel_authoring_action_node",
      "aster_kernel_authoring_action_node_parameter",
      "aster_kernel_authoring_action_node_tag",
      "aster_kernel_authoring_input_binding",
      "aster_kernel_authoring_input_binding_tag",
      "aster_kernel_authoring_action_execute",
      "aster_kernel_authoring_action_execution_info",
      "aster_kernel_authoring_action_execution_diagnostic",
      "aster_kernel_authoring_action_event",
      "aster_kernel_authoring_action_event_parameter",
      "aster_kernel_authoring_action_event_tag",
      "aster_kernel_authoring_action_execution_destroy",
      "aster_kernel_authoring_document_destroy",
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
  (void)&aster_kernel_engine_validation_event_count;
  (void)&aster_kernel_engine_validation_event;
  (void)&aster_kernel_world_create;
  (void)&aster_kernel_world_advance;
  (void)&aster_kernel_world_record_region_gate;
  (void)&aster_kernel_world_extract_render;
  (void)&aster_kernel_world_forensics;
  (void)&aster_kernel_world_destroy;
  (void)&aster_kernel_system_world_create;
  (void)&aster_kernel_system_world_tick;
  (void)&aster_kernel_system_world_entity_create;
  (void)&aster_kernel_system_world_entity_query;
  (void)&aster_kernel_system_world_entity_destroy;
  (void)&aster_kernel_system_world_transaction_begin;
  (void)&aster_kernel_system_world_transaction_append;
  (void)&aster_kernel_system_world_transaction_commit;
  (void)&aster_kernel_system_world_transaction_abort;
  (void)&aster_kernel_system_world_trace_counts;
  (void)&aster_kernel_system_world_trace_event;
  (void)&aster_kernel_system_world_save_snapshot;
  (void)&aster_kernel_system_world_load_snapshot;
  (void)&aster_kernel_system_world_replay_trace;
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
  (void)&aster_kernel_renderer_render_frame_to_target;
  (void)&aster_kernel_renderer_bind_window;
  (void)&aster_kernel_renderer_present;
  (void)&aster_kernel_renderer_present_frame;
  (void)&aster_kernel_renderer_presentation_status;
  (void)&aster_kernel_renderer_capture;
  (void)&aster_kernel_renderer_capture_render_target;
  (void)&aster_kernel_renderer_last_stats;
  (void)&aster_kernel_renderer_validation_event_count;
  (void)&aster_kernel_renderer_validation_event;
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
  (void)&aster_kernel_renderer_object_render_fate;
  (void)&aster_kernel_renderer_get_last_frame_schedule;
  (void)&aster_kernel_renderer_destroy;
  (void)&aster_kernel_mesh_create;
  (void)&aster_kernel_mesh_destroy;
  (void)&aster_kernel_material_create;
  (void)&aster_kernel_material_destroy;
  (void)&aster_kernel_texture_create;
  (void)&aster_kernel_texture_destroy;
  (void)&aster_kernel_render_target_create;
  (void)&aster_kernel_render_target_destroy;
  (void)&aster_kernel_buffer_create;
  (void)&aster_kernel_buffer_destroy;
  (void)&aster_kernel_descriptor_heap_create;
  (void)&aster_kernel_descriptor_heap_destroy;
  (void)&aster_kernel_descriptor_set_create;
  (void)&aster_kernel_descriptor_set_destroy;
  (void)&aster_kernel_pipeline_cache_create;
  (void)&aster_kernel_pipeline_cache_destroy;
  (void)&aster_kernel_frame_schedule_counts;
  (void)&aster_kernel_frame_schedule_pass;
  (void)&aster_kernel_frame_schedule_memory_report;
  (void)&aster_kernel_frame_schedule_descriptor_layout;
  (void)&aster_kernel_frame_schedule_pipeline;
  (void)&aster_kernel_frame_schedule_transient_allocation;
  (void)&aster_kernel_frame_schedule_timeline;
  (void)&aster_kernel_frame_schedule_validation_event;
  (void)&aster_kernel_frame_schedule_destroy;
  (void)&aster_kernel_shader_compile;
  (void)&aster_kernel_shader_get_result;
  (void)&aster_kernel_shader_get_source;
  (void)&aster_kernel_shader_get_diagnostics;
  (void)&aster_kernel_shader_get_reflection;
  (void)&aster_kernel_shader_destroy;
  (void)&aster_kernel_render_pipeline_create;
  (void)&aster_kernel_render_pipeline_destroy;
  (void)&aster_kernel_authoring_document_load;
  (void)&aster_kernel_authoring_document_info;
  (void)&aster_kernel_authoring_document_diagnostic;
  (void)&aster_kernel_authoring_project_asset;
  (void)&aster_kernel_authoring_entity;
  (void)&aster_kernel_authoring_action_node;
  (void)&aster_kernel_authoring_action_node_parameter;
  (void)&aster_kernel_authoring_action_node_tag;
  (void)&aster_kernel_authoring_input_binding;
  (void)&aster_kernel_authoring_input_binding_tag;
  (void)&aster_kernel_authoring_action_execute;
  (void)&aster_kernel_authoring_action_execution_info;
  (void)&aster_kernel_authoring_action_execution_diagnostic;
  (void)&aster_kernel_authoring_action_event;
  (void)&aster_kernel_authoring_action_event_parameter;
  (void)&aster_kernel_authoring_action_event_tag;
  (void)&aster_kernel_authoring_action_execution_destroy;
  (void)&aster_kernel_authoring_document_destroy;
  (void)&aster_kernel_physics_world_destroy;
  (void)&aster_kernel_system_world_destroy;
  (void)&aster_kernel_sample_app_destroy;
}

} // namespace

int main() {
  testAbiHeaderStaysPlainC();
  testPublicApiBoundaryIsFrozen();
  testStatusAndEngineLifecycle();
  testMathAbi5Contracts();
  testProjectionAbiEntrypointsMatchHeaderMathBitwise();
  testRendererAbi5Lifecycle();
  testAbi5ExplicitValidationContracts();
  testAuthoringDocumentAbi51Contracts();
  testShaderCompilerAbi5();
  testWorldRootAbi6Contracts();
  testSystemWorldCompatibilityContracts();
  testCppWrapperUsesResultStatus();
  testManifestNamesMatchLinkedApi();
  return 0;
}
