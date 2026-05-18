// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/kernel/abi.h"

#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace aster::kernel {

class Status {
public:
  constexpr Status() noexcept = default;
  constexpr explicit Status(const AsterStatus status) noexcept : status_(status) {}

  [[nodiscard]] static Status ok() noexcept {
    return Status(aster_kernel_status_ok());
  }

  [[nodiscard]] bool succeeded() const noexcept {
    return status_.code == ASTER_STATUS_OK;
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return succeeded();
  }

  [[nodiscard]] AsterStatusCode code() const noexcept {
    return status_.code;
  }

  [[nodiscard]] const char *message() const noexcept {
    return status_.message == nullptr ? "" : status_.message;
  }

  [[nodiscard]] const AsterStatus &abi() const noexcept {
    return status_;
  }

private:
  AsterStatus status_{sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_STATUS_OK, "ok"};
};

template <typename T> class Result {
public:
  static_assert(!std::is_reference_v<T>, "Result<T> cannot store references.");

  explicit Result(const Status status) noexcept : has_value_(false), status_(status) {}

  explicit Result(T &&value) noexcept(std::is_nothrow_move_constructible_v<T>) : has_value_(true) {
    new (&storage_) T(std::move(value));
  }

  Result(Result &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : has_value_(other.has_value_), status_(other.status_) {
    if (has_value_) {
      new (&storage_) T(std::move(other.value()));
      other.resetValue();
    }
  }

  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                             std::is_nothrow_move_assignable_v<T>) {
    if (this == &other) {
      return *this;
    }
    resetValue();
    status_ = other.status_;
    has_value_ = other.has_value_;
    if (has_value_) {
      new (&storage_) T(std::move(other.value()));
      other.resetValue();
    }
    return *this;
  }

  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

  ~Result() {
    resetValue();
  }

  [[nodiscard]] bool succeeded() const noexcept {
    return has_value_;
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return succeeded();
  }

  [[nodiscard]] const Status &status() const noexcept {
    return status_;
  }

  [[nodiscard]] T &value() & noexcept {
    return *reinterpret_cast<T *>(&storage_);
  }

  [[nodiscard]] const T &value() const & noexcept {
    return *reinterpret_cast<const T *>(&storage_);
  }

  [[nodiscard]] T &&value() && noexcept {
    return std::move(value());
  }

private:
  void resetValue() noexcept {
    if (has_value_) {
      value().~T();
      has_value_ = false;
    }
  }

  alignas(T) unsigned char storage_[sizeof(T)]{};
  bool has_value_ = false;
  Status status_{Status::ok()};
};

class Engine {
public:
  Engine() = default;

  explicit Engine(AsterEngineHandle handle) noexcept : handle_(handle) {}

  Engine(Engine &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

  Engine &operator=(Engine &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  ~Engine() {
    reset();
  }

  [[nodiscard]] static Result<Engine> create(const AsterEngineDesc &desc) noexcept {
    AsterEngineHandle handle = nullptr;
    const Status status(aster_kernel_engine_create(&desc, &handle));
    if (!status) {
      return Result<Engine>(status);
    }
    return Result<Engine>(Engine(handle));
  }

  [[nodiscard]] static Result<Engine> create() noexcept {
    const AsterEngineDesc desc{sizeof(AsterEngineDesc), ASTER_KERNEL_STRUCT_VERSION_1, {}, 0u};
    return create(desc);
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterEngineHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Status lastStatus() const noexcept {
    return Status(aster_kernel_engine_last_status(handle_));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_engine_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterEngineHandle handle_ = nullptr;
};

class Window {
public:
  Window() = default;
  explicit Window(AsterWindowHandle handle) noexcept : handle_(handle) {}

  Window(Window &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Window &operator=(Window &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  ~Window() {
    reset();
  }

  [[nodiscard]] static Result<Window> create(const AsterWindowDesc &desc) noexcept {
    AsterWindowHandle handle = nullptr;
    const Status status(aster_kernel_window_create(&desc, &handle));
    if (!status) {
      return Result<Window>(status);
    }
    return Result<Window>(Window(handle));
  }

  [[nodiscard]] static Result<Window> createHeadless(const uint32_t width,
                                                     const uint32_t height) noexcept {
    const AsterWindowDesc desc{sizeof(AsterWindowDesc),
                               ASTER_KERNEL_STRUCT_VERSION_1,
                               {"Aster Kernel Headless", 22u},
                               width,
                               height,
                               ASTER_KERNEL_WINDOW_FLAG_HEADLESS,
                               0u};
    return create(desc);
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterWindowHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Status poll() noexcept {
    return Status(aster_kernel_window_poll(handle_));
  }

  [[nodiscard]] Status swap() noexcept {
    return Status(aster_kernel_window_swap(handle_));
  }

  [[nodiscard]] Status setVsync(const bool enabled) noexcept {
    return Status(aster_kernel_window_set_vsync(handle_, enabled ? 1u : 0u));
  }

  [[nodiscard]] Result<AsterExtent2D> framebufferSize() const noexcept {
    AsterExtent2D size{};
    const Status status(aster_kernel_window_framebuffer_size(handle_, &size));
    if (!status) {
      return Result<AsterExtent2D>(status);
    }
    return Result<AsterExtent2D>(std::move(size));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_window_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterWindowHandle handle_ = nullptr;
};

class Scene {
public:
  Scene() = default;
  explicit Scene(AsterSceneHandle handle) noexcept : handle_(handle) {}

  Scene(Scene &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Scene &operator=(Scene &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;

  ~Scene() {
    reset();
  }

  [[nodiscard]] static Result<Scene> create(const Engine &engine) noexcept {
    AsterSceneHandle handle = nullptr;
    const Status status(aster_kernel_scene_create(engine.get(), &handle));
    if (!status) {
      return Result<Scene>(status);
    }
    return Result<Scene>(Scene(handle));
  }

  [[nodiscard]] Status clear() noexcept {
    return Status(aster_kernel_scene_clear(handle_));
  }

  [[nodiscard]] Status addObject(const AsterSceneObjectDesc &desc) noexcept {
    return Status(aster_kernel_scene_add_object(handle_, &desc));
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterSceneHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_scene_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterSceneHandle handle_ = nullptr;
};

class Mesh {
public:
  Mesh() = default;
  explicit Mesh(AsterMeshHandle handle) noexcept : handle_(handle) {}

  Mesh(Mesh &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Mesh &operator=(Mesh &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Mesh(const Mesh &) = delete;
  Mesh &operator=(const Mesh &) = delete;

  ~Mesh() {
    reset();
  }

  [[nodiscard]] static Result<Mesh> create(const Engine &engine,
                                           const AsterMeshDesc &desc) noexcept {
    AsterMeshHandle handle = nullptr;
    const Status status(aster_kernel_mesh_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<Mesh>(status);
    }
    return Result<Mesh>(Mesh(handle));
  }

  [[nodiscard]] AsterMeshHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_mesh_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterMeshHandle handle_ = nullptr;
};

class Material {
public:
  Material() = default;
  explicit Material(AsterMaterialHandle handle) noexcept : handle_(handle) {}

  Material(Material &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Material &operator=(Material &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Material(const Material &) = delete;
  Material &operator=(const Material &) = delete;

  ~Material() {
    reset();
  }

  [[nodiscard]] static Result<Material> create(const Engine &engine,
                                               const AsterMaterialDesc &desc) noexcept {
    AsterMaterialHandle handle = nullptr;
    const Status status(aster_kernel_material_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<Material>(status);
    }
    return Result<Material>(Material(handle));
  }

  [[nodiscard]] AsterMaterialHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_material_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterMaterialHandle handle_ = nullptr;
};

class ShaderArtifact {
public:
  ShaderArtifact() = default;
  explicit ShaderArtifact(AsterShaderArtifactHandle handle) noexcept : handle_(handle) {}

  ShaderArtifact(ShaderArtifact &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}
  ShaderArtifact &operator=(ShaderArtifact &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ShaderArtifact(const ShaderArtifact &) = delete;
  ShaderArtifact &operator=(const ShaderArtifact &) = delete;

  ~ShaderArtifact() {
    reset();
  }

  [[nodiscard]] static Result<ShaderArtifact>
  compile(const Engine &engine, const AsterShaderCompileDesc &desc) noexcept {
    AsterShaderArtifactHandle handle = nullptr;
    const Status status(aster_kernel_shader_compile(engine.get(), &desc, &handle));
    if (!status) {
      return Result<ShaderArtifact>(status);
    }
    return Result<ShaderArtifact>(ShaderArtifact(handle));
  }

  [[nodiscard]] Result<AsterShaderCompileResult> result() const noexcept {
    AsterShaderCompileResult value{sizeof(AsterShaderCompileResult),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_shader_get_result(handle_, &value));
    if (!status) {
      return Result<AsterShaderCompileResult>(status);
    }
    return Result<AsterShaderCompileResult>(std::move(value));
  }

  [[nodiscard]] Result<AsterStringView> source() const noexcept {
    AsterStringView view{};
    const Status status(aster_kernel_shader_get_source(handle_, &view));
    if (!status) {
      return Result<AsterStringView>(status);
    }
    return Result<AsterStringView>(std::move(view));
  }

  [[nodiscard]] Result<AsterShaderReflectionBinding> reflection(const size_t index) const noexcept {
    AsterShaderReflectionBinding binding{sizeof(AsterShaderReflectionBinding),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_shader_get_reflection(handle_, index, &binding));
    if (!status) {
      return Result<AsterShaderReflectionBinding>(status);
    }
    return Result<AsterShaderReflectionBinding>(std::move(binding));
  }

  [[nodiscard]] AsterShaderArtifactHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_shader_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterShaderArtifactHandle handle_ = nullptr;
};

class RenderPipeline {
public:
  RenderPipeline() = default;
  explicit RenderPipeline(AsterRenderPipelineHandle handle) noexcept : handle_(handle) {}

  RenderPipeline(RenderPipeline &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}
  RenderPipeline &operator=(RenderPipeline &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  RenderPipeline(const RenderPipeline &) = delete;
  RenderPipeline &operator=(const RenderPipeline &) = delete;

  ~RenderPipeline() {
    reset();
  }

  [[nodiscard]] static Result<RenderPipeline>
  create(const Engine &engine, const AsterRenderPipelineDesc &desc) noexcept {
    AsterRenderPipelineHandle handle = nullptr;
    const Status status(aster_kernel_render_pipeline_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<RenderPipeline>(status);
    }
    return Result<RenderPipeline>(RenderPipeline(handle));
  }

  [[nodiscard]] AsterRenderPipelineHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_render_pipeline_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterRenderPipelineHandle handle_ = nullptr;
};

class Renderer {
public:
  Renderer() = default;
  explicit Renderer(AsterRendererHandle handle) noexcept : handle_(handle) {}

  Renderer(Renderer &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Renderer &operator=(Renderer &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  ~Renderer() {
    reset();
  }

  [[nodiscard]] static Result<Renderer> create(const Engine &engine,
                                               const AsterRendererDesc &desc) noexcept {
    AsterRendererHandle handle = nullptr;
    const Status status(aster_kernel_renderer_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<Renderer>(status);
    }
    return Result<Renderer>(Renderer(handle));
  }

  [[nodiscard]] Result<AsterBackendCapabilities> capabilities() const noexcept {
    AsterBackendCapabilities value{sizeof(AsterBackendCapabilities),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_get_capabilities(handle_, &value));
    if (!status) {
      return Result<AsterBackendCapabilities>(status);
    }
    return Result<AsterBackendCapabilities>(std::move(value));
  }

  [[nodiscard]] Result<AsterBackendCapabilityTable> backendCapabilityTable() const noexcept {
    AsterBackendCapabilityTable value{sizeof(AsterBackendCapabilityTable),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_get_backend_capability_table(handle_, &value));
    if (!status) {
      return Result<AsterBackendCapabilityTable>(status);
    }
    return Result<AsterBackendCapabilityTable>(std::move(value));
  }

  [[nodiscard]] Status renderFrame(const Scene &scene, const AsterCameraDesc &camera,
                                   const AsterRendererSettings &settings) noexcept {
    return Status(aster_kernel_renderer_render_frame(handle_, scene.get(), &camera, &settings));
  }

  [[nodiscard]] Status present(Window &window) noexcept {
    return Status(aster_kernel_renderer_present(handle_, window.get()));
  }

  [[nodiscard]] Status capture(const AsterCaptureDesc &desc) noexcept {
    return Status(aster_kernel_renderer_capture(handle_, &desc));
  }

  [[nodiscard]] Result<AsterFrameStats> lastStats() const noexcept {
    AsterFrameStats stats{sizeof(AsterFrameStats), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_last_stats(handle_, &stats));
    if (!status) {
      return Result<AsterFrameStats>(status);
    }
    return Result<AsterFrameStats>(std::move(stats));
  }

  [[nodiscard]] Result<AsterFrameForensicsCounts> frameForensicsCounts() const noexcept {
    AsterFrameForensicsCounts counts{sizeof(AsterFrameForensicsCounts),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_forensics_counts(handle_, &counts));
    if (!status) {
      return Result<AsterFrameForensicsCounts>(status);
    }
    return Result<AsterFrameForensicsCounts>(std::move(counts));
  }

  [[nodiscard]] Result<AsterFramePassStats> framePassStats(const size_t index) const noexcept {
    AsterFramePassStats stats{sizeof(AsterFramePassStats), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_pass_stats(handle_, index, &stats));
    if (!status) {
      return Result<AsterFramePassStats>(status);
    }
    return Result<AsterFramePassStats>(std::move(stats));
  }

  [[nodiscard]] Result<AsterFrameDiagnosticEvent> frameDiagnostic(
      const size_t index) const noexcept {
    AsterFrameDiagnosticEvent event{sizeof(AsterFrameDiagnosticEvent),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_diagnostic(handle_, index, &event));
    if (!status) {
      return Result<AsterFrameDiagnosticEvent>(status);
    }
    return Result<AsterFrameDiagnosticEvent>(std::move(event));
  }

  [[nodiscard]] AsterRendererHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_renderer_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterRendererHandle handle_ = nullptr;
};

namespace math {

[[nodiscard]] inline AsterMathPolicy defaultPolicy() noexcept {
  return aster_kernel_math_default_policy();
}

[[nodiscard]] inline Result<float> dot(const AsterVec3 lhs, const AsterVec3 rhs) noexcept {
  float value = 0.0f;
  const Status status(aster_kernel_math_vec3_dot(lhs, rhs, &value));
  if (!status) {
    return Result<float>(status);
  }
  return Result<float>(std::move(value));
}

[[nodiscard]] inline Result<AsterVec3> cross(const AsterVec3 lhs, const AsterVec3 rhs) noexcept {
  AsterVec3 value{};
  const Status status(aster_kernel_math_vec3_cross(lhs, rhs, &value));
  if (!status) {
    return Result<AsterVec3>(status);
  }
  return Result<AsterVec3>(std::move(value));
}

[[nodiscard]] inline Result<AsterVec3> normalize(
    const AsterVec3 value, const AsterMathPolicy &policy = defaultPolicy(),
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterVec3 out{};
  const Status status(
      aster_kernel_math_vec3_normalize(value, &policy, &out, diagnostics));
  if (!status) {
    return Result<AsterVec3>(status);
  }
  return Result<AsterVec3>(std::move(out));
}

[[nodiscard]] inline Result<AsterMat4> inverse(
    const AsterMat4 &matrix, const AsterMathPolicy &policy = defaultPolicy(),
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterMat4 out{};
  const Status status(aster_kernel_math_mat4_inverse(&matrix, &policy, &out, diagnostics));
  if (!status) {
    return Result<AsterMat4>(status);
  }
  return Result<AsterMat4>(std::move(out));
}

[[nodiscard]] inline Result<AsterMat4> composeTrs(const AsterTransform &transform) noexcept {
  AsterMat4 out{};
  const Status status(aster_kernel_math_mat4_compose_trs(&transform, &out));
  if (!status) {
    return Result<AsterMat4>(status);
  }
  return Result<AsterMat4>(std::move(out));
}

[[nodiscard]] inline Result<AsterMat4> perspective(
    const float vertical_fov_radians, const float aspect_ratio, const float near_plane,
    const float far_plane,
    const AsterMathCoordinateHandedness handedness = ASTER_MATH_COORDINATE_RIGHT_HANDED,
    const AsterMathClipDepthRange depth_range = ASTER_MATH_CLIP_DEPTH_ZERO_TO_ONE,
    const AsterMathDepthDirection depth_direction = ASTER_MATH_DEPTH_REVERSE_Z,
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterMat4 out{};
  const Status status(aster_kernel_math_mat4_perspective(
      vertical_fov_radians, aspect_ratio, near_plane, far_plane, handedness, depth_range,
      depth_direction, &out, diagnostics));
  if (!status) {
    return Result<AsterMat4>(status);
  }
  return Result<AsterMat4>(std::move(out));
}

[[nodiscard]] inline Result<AsterScreenPoint> worldToScreen(
    const AsterWorldPoint point, const AsterMat4 &world_to_clip, const AsterViewport &viewport,
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterScreenPoint out{};
  const Status status(aster_kernel_math_world_to_screen(point, &world_to_clip, &viewport, &out,
                                                        diagnostics));
  if (!status) {
    return Result<AsterScreenPoint>(status);
  }
  return Result<AsterScreenPoint>(std::move(out));
}

[[nodiscard]] inline Result<AsterWorldPoint> screenToWorld(
    const AsterScreenPoint screen, const AsterMat4 &clip_to_world, const AsterViewport &viewport,
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterWorldPoint out{};
  const Status status(aster_kernel_math_screen_to_world(screen, &clip_to_world, &viewport, &out,
                                                        diagnostics));
  if (!status) {
    return Result<AsterWorldPoint>(status);
  }
  return Result<AsterWorldPoint>(std::move(out));
}

[[nodiscard]] inline Result<AsterWorldRay> screenToWorldRay(
    const AsterScreenPoint screen, const AsterMat4 &clip_to_world, const AsterViewport &viewport,
    const AsterProjectionConvention &convention, const AsterWorldPoint perspective_eye,
    AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterWorldRay out{};
  const Status status(aster_kernel_math_screen_to_world_ray(
      screen, &clip_to_world, &viewport, &convention, perspective_eye, &out, diagnostics));
  if (!status) {
    return Result<AsterWorldRay>(status);
  }
  return Result<AsterWorldRay>(std::move(out));
}

[[nodiscard]] inline Result<AsterQuat> axisAngle(
    const AsterVec3 axis, const float radians, AsterMathDiagnostics *diagnostics = nullptr) noexcept {
  AsterQuat out{};
  const Status status(aster_kernel_math_quat_axis_angle(axis, radians, &out, diagnostics));
  if (!status) {
    return Result<AsterQuat>(status);
  }
  return Result<AsterQuat>(std::move(out));
}

[[nodiscard]] inline Result<AsterVec3> rotate(const AsterQuat rotation,
                                              const AsterVec3 value) noexcept {
  AsterVec3 out{};
  const Status status(aster_kernel_math_quat_rotate_vec3(rotation, value, &out));
  if (!status) {
    return Result<AsterVec3>(status);
  }
  return Result<AsterVec3>(std::move(out));
}

} // namespace math

[[nodiscard]] inline AsterStringView stringView(const char *text, const size_t size) noexcept {
  return {text, size};
}

[[nodiscard]] inline AsterStringView stringView(const char *text) noexcept {
  return {text, text == nullptr ? 0u : std::strlen(text)};
}

[[nodiscard]] inline AsterAbiVersion abiVersion() noexcept {
  return aster_kernel_abi_version();
}

} // namespace aster::kernel
