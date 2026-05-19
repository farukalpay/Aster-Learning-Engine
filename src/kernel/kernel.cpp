// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/abi.h"

#include "aster/core/config.hpp"
#include "aster/math/geometry.hpp"
#include "aster/math/quat.hpp"
#include "aster/math/transform.hpp"
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
  std::vector<std::string> string_scratch;
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

bool validFrameForensicsDetailCounts(const AsterFrameForensicsDetailCounts *value) {
  return value != nullptr &&
         value->size >= offsetof(AsterFrameForensicsDetailCounts, object_fate_count) &&
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

std::string joinStrings(const std::vector<std::string> &values, const std::string_view separator) {
  std::string out;
  for (std::size_t index = 0u; index < values.size(); ++index) {
    if (index > 0u) {
      out.append(separator);
    }
    out.append(values[index]);
  }
  return out;
}

AsterStringView viewFromScratch(const AsterRendererHandle renderer, std::string text) {
  renderer->string_scratch.push_back(std::move(text));
  return viewFromString(renderer->string_scratch.back());
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

aster::Vec2 vec(const AsterVec2 value) {
  return {value.x, value.y};
}

AsterVec3 abiVec(const aster::Vec3 value) {
  return {value.x, value.y, value.z};
}

aster::Viewport viewport(const AsterViewport *value) {
  if (value == nullptr) {
    return {};
  }
  return {.origin = vec(value->origin),
          .size = vec(value->size),
          .origin_convention = value->origin_top_left != 0u
                                   ? aster::ViewportOrigin::TopLeft
                                   : aster::ViewportOrigin::BottomLeft};
}

aster::WorldPoint worldPoint(const AsterWorldPoint value) {
  return aster::WorldPoint{vec(value.value)};
}

aster::ScreenPoint screenPoint(const AsterScreenPoint value) {
  return aster::ScreenPoint{vec(value.value)};
}

AsterWorldPoint abiWorldPoint(const aster::WorldPoint value) {
  return {abiVec(value.value)};
}

AsterScreenPoint abiScreenPoint(const aster::ScreenPoint value) {
  return {abiVec(value.value)};
}

AsterWorldRay abiWorldRay(const aster::WorldRay &value) {
  return {abiWorldPoint(value.origin), abiVec(value.direction.value), value.max_distance};
}

aster::Quat quat(const AsterQuat value) {
  return {value.x, value.y, value.z, value.w};
}

AsterQuat abiQuat(const aster::Quat value) {
  return {value.x, value.y, value.z, value.w};
}

aster::Mat4 mat4(const AsterMat4 &value) {
  aster::Mat4 out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = value.m[index];
  }
  return out;
}

AsterMat4 abiMat4(const aster::Mat4 &value) {
  AsterMat4 out{};
  for (std::size_t index = 0; index < value.m.size(); ++index) {
    out.m[index] = value.m[index];
  }
  return out;
}

aster::MathPolicy mathPolicy(const AsterMathPolicy *policy) {
  if (policy == nullptr || policy->size < sizeof(AsterMathPolicy) ||
      policy->version != ASTER_KERNEL_STRUCT_VERSION_1) {
    return aster::defaultMathPolicy();
  }
  return {.absolute_epsilon = policy->absolute_epsilon > 0.0f ? policy->absolute_epsilon : 0.000001f,
          .relative_epsilon = policy->relative_epsilon > 0.0f ? policy->relative_epsilon : 0.00001f,
          .max_iterations = policy->max_iterations,
          .deterministic = policy->deterministic != 0u,
          .debug_strict = policy->debug_strict != 0u};
}

AsterMathError abiMathError(const aster::MathError error) {
  switch (error) {
  case aster::MathError::InvalidArgument:
    return ASTER_MATH_ERROR_INVALID_ARGUMENT;
  case aster::MathError::NonFiniteInput:
    return ASTER_MATH_ERROR_NON_FINITE_INPUT;
  case aster::MathError::DegenerateInput:
    return ASTER_MATH_ERROR_DEGENERATE_INPUT;
  case aster::MathError::SingularMatrix:
    return ASTER_MATH_ERROR_SINGULAR_MATRIX;
  case aster::MathError::UnsupportedPolicy:
    return ASTER_MATH_ERROR_UNSUPPORTED_POLICY;
  case aster::MathError::None:
  default:
    return ASTER_MATH_ERROR_NONE;
  }
}

void writeMathDiagnostics(AsterMathDiagnostics *out, const aster::MathDiagnostics &diagnostics) {
  if (out == nullptr) {
    return;
  }
  out->size = sizeof(AsterMathDiagnostics);
  out->version = ASTER_KERNEL_STRUCT_VERSION_1;
  out->error = abiMathError(diagnostics.error);
  out->determinant = diagnostics.determinant;
  out->condition_hint = diagnostics.condition_hint;
  out->message = diagnostics.message == nullptr ? "" : diagnostics.message;
}

void writeMathOk(AsterMathDiagnostics *out) {
  writeMathDiagnostics(out, {});
}

AsterStatus statusFromMath(const aster::MathDiagnostics &diagnostics) {
  switch (diagnostics.error) {
  case aster::MathError::InvalidArgument:
  case aster::MathError::NonFiniteInput:
  case aster::MathError::DegenerateInput:
  case aster::MathError::SingularMatrix:
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      diagnostics.message == nullptr ? "math operation failed" : diagnostics.message);
  case aster::MathError::UnsupportedPolicy:
    return makeStatus(ASTER_STATUS_UNSUPPORTED,
                      diagnostics.message == nullptr ? "math policy is unsupported" : diagnostics.message);
  case aster::MathError::None:
  default:
    return aster_kernel_status_ok();
  }
}

aster::ProjectionPolicy projectionPolicy(const AsterMathCoordinateHandedness handedness,
                                         const AsterMathClipDepthRange depth_range,
                                         const AsterMathDepthDirection depth_direction) {
  return {.handedness = handedness == ASTER_MATH_COORDINATE_LEFT_HANDED
                            ? aster::CoordinateHandedness::LeftHanded
                            : aster::CoordinateHandedness::RightHanded,
          .depth_range = depth_range == ASTER_MATH_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE
                             ? aster::ClipDepthRange::NegativeOneToOne
                             : aster::ClipDepthRange::ZeroToOne,
          .depth_direction = depth_direction == ASTER_MATH_DEPTH_FORWARD_Z
                                 ? aster::DepthDirection::ForwardZ
                                 : aster::DepthDirection::ReverseZ};
}

aster::ProjectionPolicy projectionPolicy(const AsterProjectionConvention *convention) {
  if (convention == nullptr) {
    return aster::defaultProjectionPolicy();
  }
  return projectionPolicy(convention->handedness, convention->depth_range,
                          convention->depth_direction);
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

AsterKernelBackendShaderModel shaderModel(const aster::rhi::ShaderModel model) {
  switch (model) {
  case aster::rhi::ShaderModel::SoftwareReference:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_SOFTWARE_REFERENCE;
  case aster::rhi::ShaderModel::MetalMSL23:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_METAL_MSL_2_3;
  case aster::rhi::ShaderModel::D3D12ShaderModel51:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_D3D12_SHADER_MODEL_5_1;
  case aster::rhi::ShaderModel::None:
  default:
    return ASTER_KERNEL_BACKEND_SHADER_MODEL_NONE;
  }
}

AsterKernelBackendPresentationMode presentationMode(
    const aster::rhi::PresentationMode presentation) {
  switch (presentation) {
  case aster::rhi::PresentationMode::SoftwareFramebuffer:
    return ASTER_KERNEL_BACKEND_PRESENTATION_SOFTWARE_FRAMEBUFFER;
  case aster::rhi::PresentationMode::MetalLayer:
    return ASTER_KERNEL_BACKEND_PRESENTATION_METAL_LAYER;
  case aster::rhi::PresentationMode::D3D12OffscreenReadback:
    return ASTER_KERNEL_BACKEND_PRESENTATION_D3D12_OFFSCREEN_READBACK;
  case aster::rhi::PresentationMode::None:
  default:
    return ASTER_KERNEL_BACKEND_PRESENTATION_NONE;
  }
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

AsterKernelRenderGraphPass renderGraphPass(const aster::RenderGraphPass pass) {
  switch (pass) {
  case aster::RenderGraphPass::LightCull:
    return ASTER_KERNEL_RENDER_PASS_LIGHT_CULL;
  case aster::RenderGraphPass::ShadowAtlas:
    return ASTER_KERNEL_RENDER_PASS_SHADOW_ATLAS;
  case aster::RenderGraphPass::Opaque:
    return ASTER_KERNEL_RENDER_PASS_OPAQUE;
  case aster::RenderGraphPass::ContactShadow:
    return ASTER_KERNEL_RENDER_PASS_CONTACT_SHADOW;
  case aster::RenderGraphPass::SceneLighting:
    return ASTER_KERNEL_RENDER_PASS_SCENE_LIGHTING;
  case aster::RenderGraphPass::VolumetricFog:
    return ASTER_KERNEL_RENDER_PASS_VOLUMETRIC_FOG;
  case aster::RenderGraphPass::ReflectionProbe:
    return ASTER_KERNEL_RENDER_PASS_REFLECTION_PROBE;
  case aster::RenderGraphPass::Transparent:
    return ASTER_KERNEL_RENDER_PASS_TRANSPARENT;
  case aster::RenderGraphPass::UiComposite:
    return ASTER_KERNEL_RENDER_PASS_UI_COMPOSITE;
  case aster::RenderGraphPass::Capture:
    return ASTER_KERNEL_RENDER_PASS_CAPTURE;
  case aster::RenderGraphPass::SceneColorDepth:
  default:
    return ASTER_KERNEL_RENDER_PASS_SCENE_COLOR_DEPTH;
  }
}

AsterKernelRenderGraphResource renderGraphResource(const aster::RenderGraphResource resource) {
  switch (resource) {
  case aster::RenderGraphResource::SceneDepth:
    return ASTER_KERNEL_RENDER_RESOURCE_SCENE_DEPTH;
  case aster::RenderGraphResource::LightClusters:
    return ASTER_KERNEL_RENDER_RESOURCE_LIGHT_CLUSTERS;
  case aster::RenderGraphResource::ShadowAtlas:
    return ASTER_KERNEL_RENDER_RESOURCE_SHADOW_ATLAS;
  case aster::RenderGraphResource::VolumetricFog:
    return ASTER_KERNEL_RENDER_RESOURCE_VOLUMETRIC_FOG;
  case aster::RenderGraphResource::ReflectionProbes:
    return ASTER_KERNEL_RENDER_RESOURCE_REFLECTION_PROBES;
  case aster::RenderGraphResource::UiOverlay:
    return ASTER_KERNEL_RENDER_RESOURCE_UI_OVERLAY;
  case aster::RenderGraphResource::CaptureReadback:
    return ASTER_KERNEL_RENDER_RESOURCE_CAPTURE_READBACK;
  case aster::RenderGraphResource::SceneColor:
  default:
    return ASTER_KERNEL_RENDER_RESOURCE_SCENE_COLOR;
  }
}

AsterKernelRhiResourceState rhiResourceState(const aster::rhi::ResourceState state) {
  switch (state) {
  case aster::rhi::ResourceState::CopySource:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_SOURCE;
  case aster::rhi::ResourceState::CopyDestination:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COPY_DESTINATION;
  case aster::rhi::ResourceState::ShaderRead:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_READ;
  case aster::rhi::ResourceState::ShaderWrite:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_SHADER_WRITE;
  case aster::rhi::ResourceState::ColorAttachment:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_COLOR_ATTACHMENT;
  case aster::rhi::ResourceState::DepthAttachment:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_DEPTH_ATTACHMENT;
  case aster::rhi::ResourceState::Present:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_PRESENT;
  case aster::rhi::ResourceState::Readback:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_READBACK;
  case aster::rhi::ResourceState::Undefined:
  default:
    return ASTER_KERNEL_RHI_RESOURCE_STATE_UNDEFINED;
  }
}

AsterKernelRhiQueueKind rhiQueueKind(const aster::rhi::QueueKind queue) {
  switch (queue) {
  case aster::rhi::QueueKind::Compute:
    return ASTER_KERNEL_RHI_QUEUE_COMPUTE;
  case aster::rhi::QueueKind::Copy:
    return ASTER_KERNEL_RHI_QUEUE_COPY;
  case aster::rhi::QueueKind::Graphics:
  default:
    return ASTER_KERNEL_RHI_QUEUE_GRAPHICS;
  }
}

AsterKernelBackendFeatureProofKind
backendFeatureProofKind(const aster::BackendFeatureProofKind kind) {
  switch (kind) {
  case aster::BackendFeatureProofKind::Capture:
    return ASTER_KERNEL_BACKEND_FEATURE_CAPTURE;
  case aster::BackendFeatureProofKind::TextureSampling:
    return ASTER_KERNEL_BACKEND_FEATURE_TEXTURE_SAMPLING;
  case aster::BackendFeatureProofKind::Instancing:
    return ASTER_KERNEL_BACKEND_FEATURE_INSTANCING;
  case aster::BackendFeatureProofKind::GpuTimestamps:
    return ASTER_KERNEL_BACKEND_FEATURE_GPU_TIMESTAMPS;
  case aster::BackendFeatureProofKind::HdrRenderTarget:
    return ASTER_KERNEL_BACKEND_FEATURE_HDR_RENDER_TARGET;
  case aster::BackendFeatureProofKind::Msaa:
    return ASTER_KERNEL_BACKEND_FEATURE_MSAA;
  case aster::BackendFeatureProofKind::Presentation:
    return ASTER_KERNEL_BACKEND_FEATURE_PRESENTATION;
  case aster::BackendFeatureProofKind::GraphResource:
  default:
    return ASTER_KERNEL_BACKEND_FEATURE_GRAPH_RESOURCE;
  }
}

AsterKernelBackendFeatureProofStatus
backendFeatureProofStatus(const aster::BackendFeatureProofStatus status) {
  switch (status) {
  case aster::BackendFeatureProofStatus::NotExercised:
    return ASTER_KERNEL_BACKEND_FEATURE_NOT_EXERCISED;
  case aster::BackendFeatureProofStatus::Proven:
    return ASTER_KERNEL_BACKEND_FEATURE_PROVEN;
  case aster::BackendFeatureProofStatus::MissingProof:
    return ASTER_KERNEL_BACKEND_FEATURE_MISSING_PROOF;
  case aster::BackendFeatureProofStatus::Unsupported:
    return ASTER_KERNEL_BACKEND_FEATURE_UNSUPPORTED;
  case aster::BackendFeatureProofStatus::NotAdvertised:
  default:
    return ASTER_KERNEL_BACKEND_FEATURE_NOT_ADVERTISED;
  }
}

AsterKernelRhiValidationKind
rhiValidationKind(const aster::rhi::ResourceLifetimeValidationKind kind) {
  switch (kind) {
  case aster::rhi::ResourceLifetimeValidationKind::ReadBeforeWrite:
    return ASTER_KERNEL_RHI_VALIDATION_READ_BEFORE_WRITE;
  case aster::rhi::ResourceLifetimeValidationKind::MissingBarrier:
    return ASTER_KERNEL_RHI_VALIDATION_MISSING_BARRIER;
  case aster::rhi::ResourceLifetimeValidationKind::QueueOwnershipMismatch:
    return ASTER_KERNEL_RHI_VALIDATION_QUEUE_OWNERSHIP_MISMATCH;
  case aster::rhi::ResourceLifetimeValidationKind::DescriptorResourceMismatch:
    return ASTER_KERNEL_RHI_VALIDATION_DESCRIPTOR_RESOURCE_MISMATCH;
  case aster::rhi::ResourceLifetimeValidationKind::MissingResource:
    return ASTER_KERNEL_RHI_VALIDATION_MISSING_RESOURCE;
  case aster::rhi::ResourceLifetimeValidationKind::RetiredResourceUse:
    return ASTER_KERNEL_RHI_VALIDATION_RETIRED_RESOURCE_USE;
  case aster::rhi::ResourceLifetimeValidationKind::InvalidResourceState:
  default:
    return ASTER_KERNEL_RHI_VALIDATION_INVALID_RESOURCE_STATE;
  }
}

AsterKernelFrameDiagnosticSeverity
rhiValidationSeverity(const aster::rhi::ResourceLifetimeValidationSeverity severity) {
  switch (severity) {
  case aster::rhi::ResourceLifetimeValidationSeverity::Warning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING;
  case aster::rhi::ResourceLifetimeValidationSeverity::Error:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR;
  case aster::rhi::ResourceLifetimeValidationSeverity::Info:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_INFO;
  }
}

AsterKernelFrameDiagnosticSeverity
diagnosticSeverity(const aster::FrameDiagnosticSeverity severity) {
  switch (severity) {
  case aster::FrameDiagnosticSeverity::Warning:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_WARNING;
  case aster::FrameDiagnosticSeverity::Error:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_ERROR;
  case aster::FrameDiagnosticSeverity::Info:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_INFO;
  }
}

AsterKernelFrameDiagnosticKind diagnosticKind(const aster::FrameDiagnosticKind kind) {
  switch (kind) {
  case aster::FrameDiagnosticKind::MaterialVariantFallback:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_MATERIAL_VARIANT_FALLBACK;
  case aster::FrameDiagnosticKind::TranslucentSortChanged:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_TRANSLUCENT_SORT_CHANGED;
  case aster::FrameDiagnosticKind::NearPlaneClipping:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NEAR_PLANE_CLIPPING;
  case aster::FrameDiagnosticKind::ResourceLifetimeHazard:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_RESOURCE_LIFETIME_HAZARD;
  case aster::FrameDiagnosticKind::CapabilityMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_CAPABILITY_MISMATCH;
  case aster::FrameDiagnosticKind::ClusteredLightingFallback:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_CLUSTERED_LIGHTING_FALLBACK;
  case aster::FrameDiagnosticKind::MathContract:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_MATH_CONTRACT;
  case aster::FrameDiagnosticKind::NonFiniteWorldMatrix:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NON_FINITE_WORLD_MATRIX;
  case aster::FrameDiagnosticKind::SingularNormalMatrix:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_SINGULAR_NORMAL_MATRIX;
  case aster::FrameDiagnosticKind::NegativeScaleTangentFlip:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_NEGATIVE_SCALE_TANGENT_FLIP;
  case aster::FrameDiagnosticKind::ProjectionConventionMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_PROJECTION_CONVENTION_MISMATCH;
  case aster::FrameDiagnosticKind::ViewportOriginMismatch:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_VIEWPORT_ORIGIN_MISMATCH;
  case aster::FrameDiagnosticKind::BackendProjectionDrift:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_PROJECTION_DRIFT;
  case aster::FrameDiagnosticKind::PredicateUncertainty:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_PREDICATE_UNCERTAINTY;
  case aster::FrameDiagnosticKind::BackendFallback:
  default:
    return ASTER_KERNEL_FRAME_DIAGNOSTIC_BACKEND_FALLBACK;
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

AsterMathPolicy aster_kernel_math_default_policy(void) {
  const aster::MathPolicy policy = aster::defaultMathPolicy();
  return {sizeof(AsterMathPolicy), ASTER_KERNEL_STRUCT_VERSION_1, policy.absolute_epsilon,
          policy.relative_epsilon, policy.max_iterations, policy.deterministic ? 1u : 0u,
          policy.debug_strict ? 1u : 0u};
}

AsterStatus aster_kernel_math_vec3_dot(const AsterVec3 lhs, const AsterVec3 rhs,
                                       float *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = aster::dot(vec(lhs), vec(rhs));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_cross(const AsterVec3 lhs, const AsterVec3 rhs,
                                         AsterVec3 *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = abiVec(aster::cross(vec(lhs), vec(rhs)));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_length(const AsterVec3 value, float *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = aster::length(vec(value));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_vec3_normalize(const AsterVec3 value,
                                             const AsterMathPolicy *policy,
                                             AsterVec3 *out_value,
                                             AsterMathDiagnostics *out_diagnostics) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  const aster::MathPolicy internal_policy = mathPolicy(policy);
  const aster::MathResult<aster::Vec3> result =
      aster::safeNormalize(vec(value), internal_policy.absolute_epsilon);
  *out_value = abiVec(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_identity(AsterMat4 *out_matrix) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  *out_matrix = abiMat4(aster::identity());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_multiply(const AsterMat4 *lhs, const AsterMat4 *rhs,
                                            AsterMat4 *out_matrix) {
  if (lhs == nullptr || rhs == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix pointer is null");
  }
  *out_matrix = abiMat4(mat4(*lhs) * mat4(*rhs));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_inverse(const AsterMat4 *matrix,
                                           const AsterMathPolicy *policy,
                                           AsterMat4 *out_matrix,
                                           AsterMathDiagnostics *out_diagnostics) {
  if (matrix == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix pointer is null");
  }
  const aster::MathResult<aster::Mat4> result =
      aster::inverse(mat4(*matrix), mathPolicy(policy));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_compose_trs(const AsterTransform *transform,
                                               AsterMat4 *out_matrix) {
  if (transform == nullptr || out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "transform or out_matrix is null");
  }
  const aster::Mat4 matrix = aster::translation(vec(transform->position)) *
                             aster::mat4FromQuat(quat(transform->rotation)) *
                             aster::scale(vec(transform->scale));
  *out_matrix = abiMat4(matrix);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_decompose_trs(
    const AsterMat4 *matrix, AsterTransform *out_transform, AsterVec3 *out_skew,
    AsterVec4 *out_perspective, AsterMathDiagnostics *out_diagnostics) {
  if (matrix == nullptr || out_transform == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "matrix or out_transform is null");
  }
  const aster::Mat4 m = mat4(*matrix);
  const aster::Vec3 translation_value{m.m[12], m.m[13], m.m[14]};
  aster::Vec3 columns[3] = {{m.m[0], m.m[1], m.m[2]},
                            {m.m[4], m.m[5], m.m[6]},
                            {m.m[8], m.m[9], m.m[10]}};
  const aster::Vec3 scale_value{aster::length(columns[0]), aster::length(columns[1]),
                                aster::length(columns[2])};
  if (scale_value.x <= 0.000001f || scale_value.y <= 0.000001f ||
      scale_value.z <= 0.000001f) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "TRS decomposition requires non-zero scale axes."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  columns[0] /= scale_value.x;
  columns[1] /= scale_value.y;
  columns[2] /= scale_value.z;
  const aster::Quat rotation = aster::quatFromMat3(aster::mat3FromColumns(columns[0], columns[1],
                                                                          columns[2]));
  out_transform->position = abiVec(translation_value);
  out_transform->rotation = abiQuat(rotation);
  out_transform->scale = abiVec(scale_value);
  if (out_skew != nullptr) {
    *out_skew = {0.0f, 0.0f, 0.0f};
  }
  if (out_perspective != nullptr) {
    *out_perspective = {0.0f, 0.0f, 0.0f, 1.0f};
  }
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_mat4_perspective(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane, const AsterMathCoordinateHandedness handedness,
    const AsterMathClipDepthRange depth_range, const AsterMathDepthDirection depth_direction,
    AsterMat4 *out_matrix, AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result = aster::perspective(
      vertical_fov_radians, aspect_ratio, near_plane, far_plane,
      projectionPolicy(handedness, depth_range, depth_direction));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_orthographic(
    const float left, const float right, const float bottom, const float top,
    const float near_plane, const float far_plane, const AsterMathCoordinateHandedness handedness,
    const AsterMathClipDepthRange depth_range, const AsterMathDepthDirection depth_direction,
    AsterMat4 *out_matrix, AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result = aster::orthographic(
      left, right, bottom, top, near_plane, far_plane,
      projectionPolicy(handedness, depth_range, depth_direction));
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_mat4_look_at(
    const AsterVec3 eye, const AsterVec3 target, const AsterVec3 up,
    const AsterMathCoordinateHandedness handedness, AsterMat4 *out_matrix,
    AsterMathDiagnostics *out_diagnostics) {
  if (out_matrix == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_matrix is null");
  }
  const aster::MathResult<aster::Mat4> result =
      aster::lookAt(vec(eye), vec(target), vec(up),
                    handedness == ASTER_MATH_COORDINATE_LEFT_HANDED
                        ? aster::CoordinateHandedness::LeftHanded
                        : aster::CoordinateHandedness::RightHanded);
  *out_matrix = abiMat4(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_world_to_screen(const AsterWorldPoint point,
                                              const AsterMat4 *world_to_clip,
                                              const AsterViewport *viewport_desc,
                                              AsterScreenPoint *out_screen,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (world_to_clip == nullptr || viewport_desc == nullptr || out_screen == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "world_to_screen pointer is null");
  }
  const aster::MathResult<aster::ScreenPoint> result =
      aster::project(worldPoint(point), aster::WorldToClip{mat4(*world_to_clip)},
                     viewport(viewport_desc));
  *out_screen = abiScreenPoint(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_screen_to_world(const AsterScreenPoint screen,
                                              const AsterMat4 *clip_to_world,
                                              const AsterViewport *viewport_desc,
                                              AsterWorldPoint *out_point,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (clip_to_world == nullptr || viewport_desc == nullptr || out_point == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "screen_to_world pointer is null");
  }
  const aster::MathResult<aster::WorldPoint> result =
      aster::unproject(screenPoint(screen), aster::ClipToWorld{mat4(*clip_to_world)},
                       viewport(viewport_desc));
  *out_point = abiWorldPoint(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_screen_to_world_ray(
    const AsterScreenPoint screen, const AsterMat4 *clip_to_world,
    const AsterViewport *viewport_desc, const AsterProjectionConvention *convention,
    const AsterWorldPoint perspective_eye, AsterWorldRay *out_ray,
    AsterMathDiagnostics *out_diagnostics) {
  if (clip_to_world == nullptr || viewport_desc == nullptr || out_ray == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "screen_to_world_ray pointer is null");
  }
  const aster::MathResult<aster::WorldRay> result = aster::screenRay(
      screenPoint(screen), aster::ClipToWorld{mat4(*clip_to_world)}, viewport(viewport_desc),
      projectionPolicy(convention), aster::RayOriginPolicy::PerspectiveEye,
      worldPoint(perspective_eye));
  *out_ray = abiWorldRay(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_quat_identity(AsterQuat *out_quat) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  *out_quat = abiQuat(aster::identityQuat());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_axis_angle(const AsterVec3 axis, const float radians,
                                              AsterQuat *out_quat,
                                              AsterMathDiagnostics *out_diagnostics) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  const aster::MathResult<aster::Quat> result = aster::axisAngleSafe(vec(axis), radians);
  *out_quat = abiQuat(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_quat_slerp(const AsterQuat lhs, const AsterQuat rhs, const float t,
                                         AsterQuat *out_quat) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  *out_quat = abiQuat(aster::slerp(quat(lhs), quat(rhs), t));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_rotate_vec3(const AsterQuat rotation, const AsterVec3 value,
                                               AsterVec3 *out_value) {
  if (out_value == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_value is null");
  }
  *out_value = abiVec(aster::rotate(quat(rotation), vec(value)));
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_quat_inverse(const AsterQuat value, AsterQuat *out_quat,
                                           AsterMathDiagnostics *out_diagnostics) {
  if (out_quat == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_quat is null");
  }
  const aster::MathResult<aster::Quat> result = aster::inverse(quat(value));
  *out_quat = abiQuat(result.value);
  writeMathDiagnostics(out_diagnostics, result.diagnostics);
  return result ? aster_kernel_status_ok() : statusFromMath(result.diagnostics);
}

AsterStatus aster_kernel_math_intersect_ray_plane(
    const AsterRay3 ray, const AsterPlane3 plane, float *out_distance, AsterVec3 *out_point,
    AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_point == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-plane output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit =
      aster::intersectRayPlane(vec(ray.origin), direction.value, {vec(plane.normal),
                                                                  plane.distance});
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the plane."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_point = abiVec(hit.point);
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_intersect_ray_triangle(
    const AsterRay3 ray, const AsterVec3 a, const AsterVec3 b, const AsterVec3 c,
    float *out_distance, AsterVec3 *out_barycentric, AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_barycentric == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-triangle output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit =
      aster::intersectRayTriangle(vec(ray.origin), direction.value, vec(a), vec(b), vec(c));
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the triangle."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_barycentric = {hit.barycentric.u, hit.barycentric.v, hit.barycentric.w};
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_math_intersect_ray_sphere(
    const AsterRay3 ray, const AsterSphere3 sphere, float *out_distance, AsterVec3 *out_point,
    AsterVec3 *out_normal, AsterMathDiagnostics *out_diagnostics) {
  if (out_distance == nullptr || out_point == nullptr || out_normal == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "ray-sphere output pointer is null");
  }
  const aster::MathResult<aster::Vec3> direction = aster::safeNormalize(vec(ray.direction));
  if (!direction) {
    writeMathDiagnostics(out_diagnostics, direction.diagnostics);
    return statusFromMath(direction.diagnostics);
  }
  const aster::RayHit3 hit = aster::intersectRaySphere(
      vec(ray.origin), direction.value, {vec(sphere.center), sphere.radius});
  if (!hit.hit || (ray.max_distance > 0.0f && hit.distance > ray.max_distance)) {
    const aster::MathDiagnostics diagnostics{aster::MathError::DegenerateInput, 0.0f, 0.0f,
                                             "Ray does not intersect the sphere."};
    writeMathDiagnostics(out_diagnostics, diagnostics);
    return statusFromMath(diagnostics);
  }
  *out_distance = hit.distance;
  *out_point = abiVec(hit.point);
  *out_normal = abiVec(hit.normal);
  writeMathOk(out_diagnostics);
  return aster_kernel_status_ok();
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
    object.material =
        aster::makeMaterial({.base_color = aster::LinearRgb{0.8f, 0.82f, 0.86f}});
  }
  object.transform.position = vec(desc->position);
  object.transform.rotation = aster::quatFromEulerXyz(vec(desc->rotation));
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

AsterStatus aster_kernel_renderer_get_backend_capability_table(
    const AsterRendererHandle renderer, AsterBackendCapabilityTable *out_capabilities) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capabilities)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "backend capability table struct version is not supported");
  }
  const aster::RenderBackendCapabilities capabilities = renderer->renderer->backendCapabilities();
  const aster::rhi::DeviceCapabilities &table = capabilities.capability_table;
  out_capabilities->backend = backendKind(capabilities.kind);
  out_capabilities->flags = capabilityFlags(capabilities);
  out_capabilities->graph_resource_mask = capabilities.graph_resource_mask;
  out_capabilities->name = {capabilities.name, std::strlen(capabilities.name)};
  out_capabilities->color_format_mask = table.color_format_mask;
  out_capabilities->depth_format_mask = table.depth_format_mask;
  out_capabilities->sample_count_mask = table.sample_count_mask;
  out_capabilities->sampler_filter_mask = table.sampler_filter_mask;
  out_capabilities->sampler_address_mode_mask = table.sampler_address_mode_mask;
  out_capabilities->blend_mode_mask = table.blend_mode_mask;
  out_capabilities->shader_model = shaderModel(table.shader_model);
  out_capabilities->presentation = presentationMode(table.presentation);
  out_capabilities->max_color_attachments = table.limits.max_color_attachments;
  out_capabilities->max_sampled_textures_per_material =
      table.limits.max_sampled_textures_per_material;
  out_capabilities->max_samplers_per_material = table.limits.max_samplers_per_material;
  out_capabilities->max_uniform_buffers_per_stage = table.limits.max_uniform_buffers_per_stage;
  out_capabilities->max_storage_buffers_per_stage = table.limits.max_storage_buffers_per_stage;
  out_capabilities->max_bind_groups = table.limits.max_bind_groups;
  out_capabilities->max_vertex_attributes = table.limits.max_vertex_attributes;
  out_capabilities->max_texture_dimension_2d = table.limits.max_texture_dimension_2d;
  out_capabilities->max_dynamic_uniform_bytes = table.limits.max_dynamic_uniform_bytes;
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

AsterStatus aster_kernel_renderer_frame_forensics_counts(
    const AsterRendererHandle renderer, AsterFrameForensicsCounts *out_counts) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame forensics counts struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  out_counts->pass_count = forensics.passes.size();
  out_counts->event_count = forensics.events.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_forensics_detail_counts(
    const AsterRendererHandle renderer, AsterFrameForensicsDetailCounts *out_counts) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validFrameForensicsDetailCounts(out_counts)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame forensics detail counts struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  out_counts->pass_count = forensics.passes.size();
  out_counts->event_count = forensics.events.size();
  out_counts->debug_capture_count = forensics.captures.size();
  out_counts->pass_artifact_count = forensics.pass_artifacts.size();
  out_counts->resource_transition_count = forensics.resource_traces.size();
  out_counts->rhi_validation_event_count = forensics.rhi_validation_events.size();
  out_counts->timestamp_sample_count = forensics.timestamp_samples.size();
  out_counts->backend_feature_proof_count = forensics.backend_feature_proofs.size();
  out_counts->certification_valid = forensics.certification.valid ? 1u : 0u;
  out_counts->certification_missing_proof_count =
      forensics.certification.missing_proof_count;
  out_counts->certification_validation_error_count =
      forensics.certification.validation_error_count;
  if (out_counts->size >= sizeof(AsterFrameForensicsDetailCounts)) {
    out_counts->object_fate_count = forensics.object_fates.size();
  }
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_pass_stats(const AsterRendererHandle renderer,
                                                   const std::size_t index,
                                                   AsterFramePassStats *out_stats) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_stats)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame pass stats struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.passes.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame pass stats index is out of range");
  }
  const aster::FramePassStats &pass = forensics.passes[index];
  out_stats->pass = renderGraphPass(pass.pass);
  out_stats->name = viewFromString(pass.name);
  out_stats->draw_calls = pass.draw_calls;
  out_stats->pipeline_switches = pass.pipeline_switches;
  out_stats->material_permutations = pass.material_permutations;
  out_stats->encode_seconds = pass.encode_seconds;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_frame_diagnostic(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameDiagnosticEvent *out_event) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame diagnostic event struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame diagnostic index is out of range");
  }
  const aster::FrameDiagnosticEvent &event = forensics.events[index];
  out_event->kind = diagnosticKind(event.kind);
  out_event->severity = diagnosticSeverity(event.severity);
  out_event->pass = viewFromString(event.pass);
  out_event->label = viewFromString(event.label);
  out_event->message = viewFromString(event.message);
  out_event->value = event.value;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_debug_capture_info(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameDebugCaptureInfo *out_capture) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_capture)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame debug capture struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.captures.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame debug capture index is out of range");
  }
  const aster::FrameDebugCapture &capture = forensics.captures[index];
  out_capture->pass = renderGraphPass(capture.pass);
  out_capture->resource = renderGraphResource(capture.resource);
  out_capture->view = static_cast<std::uint32_t>(capture.view);
  out_capture->label = viewFromString(capture.label);
  out_capture->width = capture.width;
  out_capture->height = capture.height;
  out_capture->row_stride_bytes = capture.row_stride_bytes;
  out_capture->content_hash = capture.content_hash;
  out_capture->available = capture.available ? 1u : 0u;
  out_capture->payload_size = capture.rgba8.size();
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_pass_artifact_info(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFramePassArtifactInfo *out_artifact) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_artifact)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame pass artifact struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.pass_artifacts.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "frame pass artifact index is out of range");
  }
  const aster::FramePassArtifact &artifact = forensics.pass_artifacts[index];
  out_artifact->pass = renderGraphPass(artifact.pass);
  out_artifact->resource = renderGraphResource(artifact.resource);
  out_artifact->label = viewFromString(artifact.label);
  out_artifact->kind = viewFromString(artifact.kind);
  out_artifact->width = artifact.width;
  out_artifact->height = artifact.height;
  out_artifact->content_hash = artifact.content_hash;
  out_artifact->available = artifact.available ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_resource_transition(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameResourceTransition *out_transition) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_transition)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame resource transition struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.resource_traces.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT,
                      "frame resource transition index is out of range");
  }
  const aster::FrameResourceTrace &trace = forensics.resource_traces[index];
  out_transition->pass = renderGraphPass(trace.pass);
  out_transition->resource = renderGraphResource(trace.resource);
  out_transition->pass_name = viewFromString(trace.pass_name);
  out_transition->resource_name = viewFromString(trace.resource_name);
  out_transition->before = rhiResourceState(trace.before);
  out_transition->after = rhiResourceState(trace.after);
  out_transition->queue = rhiQueueKind(trace.queue);
  out_transition->write = trace.write ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_object_render_fate(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterObjectRenderFate *out_fate) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_fate)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "object render fate struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.object_fates.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "object render fate index is out of range");
  }
  renderer->string_scratch.clear();
  renderer->string_scratch.reserve(6u);
  const aster::ObjectRenderFateTrace &fate = forensics.object_fates[index];
  out_fate->object_index = fate.object_index;
  out_fate->visible = fate.visible ? 1u : 0u;
  out_fate->object_name = viewFromString(fate.object_name);
  out_fate->mesh_key = viewFromString(fate.mesh_key);
  out_fate->material_key = viewFromString(fate.material_key);
  out_fate->material_asset_id = viewFromString(fate.material_asset_id);
  out_fate->shader_variant_key = viewFromString(fate.shader_variant_key);
  out_fate->pipeline_tag = viewFromString(fate.pipeline_tag);
  out_fate->texture_roles = viewFromScratch(renderer, joinStrings(fate.texture_roles, ","));
  out_fate->pass_list = viewFromScratch(renderer, joinStrings(fate.pass_list, ","));
  out_fate->resource_transitions =
      viewFromScratch(renderer, joinStrings(fate.resource_transitions, ","));
  out_fate->capture_labels = viewFromScratch(renderer, joinStrings(fate.capture_labels, ","));
  out_fate->feature_proofs = viewFromScratch(renderer, joinStrings(fate.feature_proofs, ","));
  out_fate->final_contribution = viewFromString(fate.final_contribution);
  out_fate->contribution_hash = fate.contribution_hash;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_rhi_validation_event(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterRhiValidationEvent *out_event) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_event)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "RHI validation event struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.rhi_validation_events.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "RHI validation event index is out of range");
  }
  const aster::rhi::ResourceLifetimeValidationEvent &event =
      forensics.rhi_validation_events[index];
  out_event->kind = rhiValidationKind(event.kind);
  out_event->severity = rhiValidationSeverity(event.severity);
  out_event->pass = viewFromString(event.pass);
  out_event->resource = viewFromString(event.resource);
  out_event->message = viewFromString(event.message);
  out_event->pass_index = event.pass_index;
  out_event->resource_id = event.resource_id;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_timestamp_sample(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterFrameTimestampSample *out_sample) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_sample)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "frame timestamp sample struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.timestamp_samples.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "timestamp sample index is out of range");
  }
  const aster::rhi::TimestampQueryResult &sample = forensics.timestamp_samples[index];
  out_sample->slot = sample.slot;
  out_sample->ticks = sample.ticks;
  out_sample->nanoseconds = sample.nanoseconds;
  out_sample->available = sample.available ? 1u : 0u;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_renderer_backend_feature_proof(
    const AsterRendererHandle renderer, const std::size_t index,
    AsterBackendFeatureProof *out_proof) {
  if (!validRenderer(renderer)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "renderer handle is invalid");
  }
  if (!validStruct(out_proof)) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH,
                      "backend feature proof struct version is not supported");
  }
  const aster::FrameForensics &forensics = renderer->renderer->lastFrameForensics();
  if (index >= forensics.backend_feature_proofs.size()) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "backend feature proof index is out of range");
  }
  const aster::BackendFeatureProof &proof = forensics.backend_feature_proofs[index];
  out_proof->kind = backendFeatureProofKind(proof.kind);
  out_proof->status = backendFeatureProofStatus(proof.status);
  out_proof->pass = renderGraphPass(proof.pass);
  out_proof->resource = renderGraphResource(proof.resource);
  out_proof->feature = viewFromString(proof.feature);
  out_proof->label = viewFromString(proof.label);
  out_proof->message = viewFromString(proof.message);
  out_proof->advertised = proof.advertised;
  out_proof->native = proof.native;
  out_proof->evidence_hash = proof.evidence_hash;
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
    material->material = aster::makeMaterial({.base_color = aster::LinearRgb{vec(desc->base_color)},
                                              .emission_color =
                                                  aster::EmissionColor{vec(desc->emission_color)},
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
