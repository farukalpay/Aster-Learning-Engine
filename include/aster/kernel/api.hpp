// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/kernel/abi.h"

#include <cstring>
#include <new>
#include <string_view>
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

  [[nodiscard]] Result<size_t> validationEventCount() const noexcept {
    size_t count = 0u;
    const Status status(aster_kernel_engine_validation_event_count(handle_, &count));
    if (!status) {
      return Result<size_t>(status);
    }
    return Result<size_t>(std::move(count));
  }

  [[nodiscard]] Result<AsterValidationEvent> validationEvent(const size_t index) const noexcept {
    AsterValidationEvent event{sizeof(AsterValidationEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_engine_validation_event(handle_, index, &event));
    if (!status) {
      return Result<AsterValidationEvent>(status);
    }
    return Result<AsterValidationEvent>(std::move(event));
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

class World {
public:
  World() = default;
  explicit World(AsterWorldHandle handle) noexcept : handle_(handle) {}

  World(World &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  World &operator=(World &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  World(const World &) = delete;
  World &operator=(const World &) = delete;

  ~World() {
    reset();
  }

  [[nodiscard]] static Result<World> create(const Engine &engine,
                                            const AsterWorldDesc &desc) noexcept {
    AsterWorldHandle handle = nullptr;
    const Status status(aster_kernel_world_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<World>(status);
    }
    return Result<World>(World(handle));
  }

  [[nodiscard]] static Result<World> create(const Engine &engine) noexcept {
    const AsterWorldDesc desc{sizeof(AsterWorldDesc), ASTER_KERNEL_STRUCT_VERSION_1,
                              1.0 / 60.0, 0u, {}};
    return create(engine, desc);
  }

  [[nodiscard]] AsterWorldHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterWorldAdvanceResult>
  advance(const AsterWorldAdvanceDesc &desc) noexcept {
    AsterWorldAdvanceResult result{sizeof(AsterWorldAdvanceResult),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_advance(handle_, &desc, &result));
    if (!status) {
      return Result<AsterWorldAdvanceResult>(status);
    }
    return Result<AsterWorldAdvanceResult>(std::move(result));
  }

  [[nodiscard]] Status recordRegionGate(const AsterWorldRegionGateReport &report) noexcept {
    return Status(aster_kernel_world_record_region_gate(handle_, &report));
  }

  [[nodiscard]] Result<AsterWorldPerceptualPrimitiveInfo> perceptualPrimitive(
      const std::uint32_t index) const noexcept {
    AsterWorldPerceptualPrimitiveInfo primitive{sizeof(AsterWorldPerceptualPrimitiveInfo),
                                                ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_perceptual_primitive(handle_, index, &primitive));
    if (!status) {
      return Result<AsterWorldPerceptualPrimitiveInfo>(status);
    }
    return Result<AsterWorldPerceptualPrimitiveInfo>(std::move(primitive));
  }

  [[nodiscard]] Result<AsterWorldRenderExtraction>
  extractRender(const AsterWorldRenderExtractionDesc &desc) noexcept {
    AsterWorldRenderExtraction extraction{sizeof(AsterWorldRenderExtraction),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_extract_render(handle_, &desc, &extraction));
    if (!status) {
      return Result<AsterWorldRenderExtraction>(status);
    }
    return Result<AsterWorldRenderExtraction>(std::move(extraction));
  }

  [[nodiscard]] Result<AsterWorldForensics> forensics() const noexcept {
    AsterWorldForensics result{sizeof(AsterWorldForensics), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_forensics(handle_, &result));
    if (!status) {
      return Result<AsterWorldForensics>(status);
    }
    return Result<AsterWorldForensics>(std::move(result));
  }

  [[nodiscard]] Result<AsterBeliefReportInfo> beliefReport() const noexcept {
    AsterBeliefReportInfo report{sizeof(AsterBeliefReportInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_belief_report(handle_, &report));
    if (!status) {
      return Result<AsterBeliefReportInfo>(status);
    }
    return Result<AsterBeliefReportInfo>(std::move(report));
  }

  [[nodiscard]] Result<AsterBeliefFindingInfo> beliefFinding(
      const std::uint32_t index) const noexcept {
    AsterBeliefFindingInfo finding{sizeof(AsterBeliefFindingInfo),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_world_belief_finding(handle_, index, &finding));
    if (!status) {
      return Result<AsterBeliefFindingInfo>(status);
    }
    return Result<AsterBeliefFindingInfo>(std::move(finding));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_world_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterWorldHandle handle_ = nullptr;
};

class SystemWorld {
public:
  SystemWorld() = default;
  explicit SystemWorld(AsterSystemWorldHandle handle) noexcept : handle_(handle) {}

  SystemWorld(SystemWorld &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  SystemWorld &operator=(SystemWorld &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  SystemWorld(const SystemWorld &) = delete;
  SystemWorld &operator=(const SystemWorld &) = delete;

  ~SystemWorld() {
    reset();
  }

  [[nodiscard]] static Result<SystemWorld>
  create(const Engine &engine, const AsterSystemWorldDesc &desc) noexcept {
    AsterSystemWorldHandle handle = nullptr;
    const Status status(aster_kernel_system_world_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<SystemWorld>(status);
    }
    return Result<SystemWorld>(SystemWorld(handle));
  }

  [[nodiscard]] static Result<SystemWorld> create(const Engine &engine) noexcept {
    const AsterSystemWorldDesc desc{sizeof(AsterSystemWorldDesc),
                                    ASTER_KERNEL_STRUCT_VERSION_1,
                                    1.0 / 60.0,
                                    0u,
                                    {}};
    return create(engine, desc);
  }

  [[nodiscard]] AsterSystemWorldHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterSystemTickResult> tick(const AsterSystemTickDesc &desc) noexcept {
    AsterSystemTickResult result{sizeof(AsterSystemTickResult), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_tick(handle_, &desc, &result));
    if (!status) {
      return Result<AsterSystemTickResult>(status);
    }
    return Result<AsterSystemTickResult>(std::move(result));
  }

  [[nodiscard]] Result<AsterSystemEntityHandle> createEntity(
      const AsterStringView label = {}) noexcept {
    AsterSystemEntityHandle entity{};
    const Status status(aster_kernel_system_world_entity_create(handle_, label, &entity));
    if (!status) {
      return Result<AsterSystemEntityHandle>(status);
    }
    return Result<AsterSystemEntityHandle>(std::move(entity));
  }

  [[nodiscard]] Result<AsterSystemEntityInfo> entity(
      const AsterSystemEntityHandle handle) noexcept {
    AsterSystemEntityInfo info{sizeof(AsterSystemEntityInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_entity_query(handle_, handle, &info));
    if (!status) {
      return Result<AsterSystemEntityInfo>(status);
    }
    return Result<AsterSystemEntityInfo>(std::move(info));
  }

  [[nodiscard]] Status destroyEntity(const AsterSystemEntityHandle handle) noexcept {
    return Status(aster_kernel_system_world_entity_destroy(handle_, handle));
  }

  [[nodiscard]] Result<AsterSystemTransactionInfo>
  beginTransaction(const AsterSystemTransactionDesc &desc) noexcept {
    AsterSystemTransactionInfo info{sizeof(AsterSystemTransactionInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_transaction_begin(handle_, &desc, &info));
    if (!status) {
      return Result<AsterSystemTransactionInfo>(status);
    }
    return Result<AsterSystemTransactionInfo>(std::move(info));
  }

  [[nodiscard]] Status appendTransactionAccess(
      const uint64_t transaction_id, const AsterSystemComponentAccess &access) noexcept {
    return Status(aster_kernel_system_world_transaction_append(handle_, transaction_id, &access));
  }

  [[nodiscard]] Result<AsterSystemTransactionInfo> commitTransaction(
      const uint64_t transaction_id) noexcept {
    AsterSystemTransactionInfo info{sizeof(AsterSystemTransactionInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_system_world_transaction_commit(handle_, transaction_id, &info));
    if (!status) {
      return Result<AsterSystemTransactionInfo>(status);
    }
    return Result<AsterSystemTransactionInfo>(std::move(info));
  }

  [[nodiscard]] Result<AsterSystemTransactionInfo>
  abortTransaction(const uint64_t transaction_id, const AsterStringView reason = {}) noexcept {
    AsterSystemTransactionInfo info{sizeof(AsterSystemTransactionInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_system_world_transaction_abort(handle_, transaction_id, reason, &info));
    if (!status) {
      return Result<AsterSystemTransactionInfo>(status);
    }
    return Result<AsterSystemTransactionInfo>(std::move(info));
  }

  [[nodiscard]] Result<AsterSystemTraceCounts> traceCounts() const noexcept {
    AsterSystemTraceCounts counts{sizeof(AsterSystemTraceCounts), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_trace_counts(handle_, &counts));
    if (!status) {
      return Result<AsterSystemTraceCounts>(status);
    }
    return Result<AsterSystemTraceCounts>(std::move(counts));
  }

  [[nodiscard]] Result<AsterSystemTraceEvent> traceEvent(const size_t index) const noexcept {
    AsterSystemTraceEvent event{sizeof(AsterSystemTraceEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_trace_event(handle_, index, &event));
    if (!status) {
      return Result<AsterSystemTraceEvent>(status);
    }
    return Result<AsterSystemTraceEvent>(std::move(event));
  }

  [[nodiscard]] Result<AsterTypedTraceCounts> typedTraceCounts() const noexcept {
    AsterTypedTraceCounts counts{sizeof(AsterTypedTraceCounts), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_typed_trace_counts(handle_, &counts));
    if (!status) {
      return Result<AsterTypedTraceCounts>(status);
    }
    return Result<AsterTypedTraceCounts>(std::move(counts));
  }

  [[nodiscard]] Result<AsterTypedTraceEvent> typedTraceEvent(const size_t index) const noexcept {
    AsterTypedTraceEvent event{sizeof(AsterTypedTraceEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_typed_trace_event(handle_, index, &event));
    if (!status) {
      return Result<AsterTypedTraceEvent>(status);
    }
    return Result<AsterTypedTraceEvent>(std::move(event));
  }

  [[nodiscard]] Result<AsterTypedTraceEvent> appendTypedTrace(
      const AsterTypedTraceEvent &event) noexcept {
    AsterTypedTraceEvent out{sizeof(AsterTypedTraceEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_typed_trace_append(handle_, &event, &out));
    if (!status) {
      return Result<AsterTypedTraceEvent>(status);
    }
    return Result<AsterTypedTraceEvent>(std::move(out));
  }

  [[nodiscard]] Status saveSnapshot(const AsterWorldSnapshotDesc &desc) noexcept {
    return Status(aster_kernel_system_world_save_snapshot(handle_, &desc));
  }

  [[nodiscard]] Result<AsterWorldMigrationReport> loadSnapshot(
      const AsterWorldSnapshotDesc &desc) noexcept {
    AsterWorldMigrationReport report{sizeof(AsterWorldMigrationReport),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_load_snapshot(handle_, &desc, &report));
    if (!status) {
      return Result<AsterWorldMigrationReport>(status);
    }
    return Result<AsterWorldMigrationReport>(std::move(report));
  }

  [[nodiscard]] Result<AsterWorldReplayReport> replayTrace(
      const AsterWorldSnapshotDesc &desc) noexcept {
    AsterWorldReplayReport report{sizeof(AsterWorldReplayReport),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_system_world_replay_trace(handle_, &desc, &report));
    if (!status) {
      return Result<AsterWorldReplayReport>(status);
    }
    return Result<AsterWorldReplayReport>(std::move(report));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_system_world_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterSystemWorldHandle handle_ = nullptr;
};

class MemoryController {
public:
  MemoryController() = default;
  explicit MemoryController(AsterMemoryControllerHandle handle) noexcept : handle_(handle) {}

  MemoryController(MemoryController &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}
  MemoryController &operator=(MemoryController &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  MemoryController(const MemoryController &) = delete;
  MemoryController &operator=(const MemoryController &) = delete;

  ~MemoryController() {
    reset();
  }

  [[nodiscard]] static Result<MemoryController>
  create(const Engine &engine, const AsterMemoryControllerDesc &desc) noexcept {
    AsterMemoryControllerHandle handle = nullptr;
    const Status status(aster_kernel_memory_controller_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<MemoryController>(status);
    }
    return Result<MemoryController>(MemoryController(handle));
  }

  [[nodiscard]] AsterMemoryControllerHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterMemoryDecisionInfo>
  step(SystemWorld &world, const AsterMemoryControllerStepDesc &desc) noexcept {
    AsterMemoryDecisionInfo decision{sizeof(AsterMemoryDecisionInfo),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_memory_controller_step(handle_, world.get(), &desc, &decision));
    if (!status) {
      return Result<AsterMemoryDecisionInfo>(status);
    }
    return Result<AsterMemoryDecisionInfo>(std::move(decision));
  }

  [[nodiscard]] Result<size_t> decisionCount() const noexcept {
    size_t count = 0u;
    const Status status(aster_kernel_memory_controller_decision_count(handle_, &count));
    if (!status) {
      return Result<size_t>(status);
    }
    return Result<size_t>(std::move(count));
  }

  [[nodiscard]] Result<AsterMemoryDecisionInfo> decision(const size_t index) const noexcept {
    AsterMemoryDecisionInfo value{sizeof(AsterMemoryDecisionInfo),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_memory_controller_decision(handle_, index, &value));
    if (!status) {
      return Result<AsterMemoryDecisionInfo>(status);
    }
    return Result<AsterMemoryDecisionInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterGraphQueryResult> queryGraph(
      const AsterGraphQueryDesc &desc) const noexcept {
    AsterGraphQueryResult value{sizeof(AsterGraphQueryResult), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_memory_graph_query(handle_, &desc, &value));
    if (!status) {
      return Result<AsterGraphQueryResult>(status);
    }
    return Result<AsterGraphQueryResult>(std::move(value));
  }

  [[nodiscard]] Result<AsterMemoryBenchmarkReport> benchmarkReport() const noexcept {
    AsterMemoryBenchmarkReport value{sizeof(AsterMemoryBenchmarkReport),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_memory_benchmark_export(handle_, &value));
    if (!status) {
      return Result<AsterMemoryBenchmarkReport>(status);
    }
    return Result<AsterMemoryBenchmarkReport>(std::move(value));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_memory_controller_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterMemoryControllerHandle handle_ = nullptr;
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

class Texture {
public:
  Texture() = default;
  explicit Texture(AsterTextureHandle handle) noexcept : handle_(handle) {}
  Texture(Texture &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Texture &operator=(Texture &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;
  ~Texture() {
    reset();
  }

  [[nodiscard]] static Result<Texture> create(const Engine &engine,
                                              const AsterTextureDesc &desc) noexcept {
    AsterTextureHandle handle = nullptr;
    const Status status(aster_kernel_texture_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<Texture>(status);
    }
    return Result<Texture>(Texture(handle));
  }

  [[nodiscard]] AsterTextureHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_texture_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterTextureHandle handle_ = nullptr;
};

class RenderTarget {
public:
  RenderTarget() = default;
  explicit RenderTarget(AsterRenderTargetHandle handle) noexcept : handle_(handle) {}
  RenderTarget(RenderTarget &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  RenderTarget &operator=(RenderTarget &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;
  ~RenderTarget() {
    reset();
  }

  [[nodiscard]] static Result<RenderTarget> create(const Engine &engine,
                                                   const AsterRenderTargetDesc &desc) noexcept {
    AsterRenderTargetHandle handle = nullptr;
    const Status status(aster_kernel_render_target_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<RenderTarget>(status);
    }
    return Result<RenderTarget>(RenderTarget(handle));
  }

  [[nodiscard]] AsterRenderTargetHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_render_target_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterRenderTargetHandle handle_ = nullptr;
};

class Buffer {
public:
  Buffer() = default;
  explicit Buffer(AsterBufferHandle handle) noexcept : handle_(handle) {}
  Buffer(Buffer &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Buffer &operator=(Buffer &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;
  ~Buffer() {
    reset();
  }

  [[nodiscard]] static Result<Buffer> create(const Engine &engine,
                                             const AsterBufferDesc &desc) noexcept {
    AsterBufferHandle handle = nullptr;
    const Status status(aster_kernel_buffer_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<Buffer>(status);
    }
    return Result<Buffer>(Buffer(handle));
  }

  [[nodiscard]] AsterBufferHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_buffer_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterBufferHandle handle_ = nullptr;
};

class DescriptorHeap {
public:
  DescriptorHeap() = default;
  explicit DescriptorHeap(AsterDescriptorHeapHandle handle) noexcept : handle_(handle) {}
  DescriptorHeap(DescriptorHeap &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  DescriptorHeap &operator=(DescriptorHeap &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  DescriptorHeap(const DescriptorHeap &) = delete;
  DescriptorHeap &operator=(const DescriptorHeap &) = delete;
  ~DescriptorHeap() {
    reset();
  }

  [[nodiscard]] static Result<DescriptorHeap>
  create(const Engine &engine, const AsterDescriptorHeapDesc &desc) noexcept {
    AsterDescriptorHeapHandle handle = nullptr;
    const Status status(aster_kernel_descriptor_heap_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<DescriptorHeap>(status);
    }
    return Result<DescriptorHeap>(DescriptorHeap(handle));
  }

  [[nodiscard]] AsterDescriptorHeapHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_descriptor_heap_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterDescriptorHeapHandle handle_ = nullptr;
};

class DescriptorSet {
public:
  DescriptorSet() = default;
  explicit DescriptorSet(AsterDescriptorSetHandle handle) noexcept : handle_(handle) {}
  DescriptorSet(DescriptorSet &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  DescriptorSet &operator=(DescriptorSet &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  DescriptorSet(const DescriptorSet &) = delete;
  DescriptorSet &operator=(const DescriptorSet &) = delete;
  ~DescriptorSet() {
    reset();
  }

  [[nodiscard]] static Result<DescriptorSet>
  create(const Engine &engine, const AsterDescriptorSetDesc &desc) noexcept {
    AsterDescriptorSetHandle handle = nullptr;
    const Status status(aster_kernel_descriptor_set_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<DescriptorSet>(status);
    }
    return Result<DescriptorSet>(DescriptorSet(handle));
  }

  [[nodiscard]] AsterDescriptorSetHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_descriptor_set_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterDescriptorSetHandle handle_ = nullptr;
};

class PipelineCache {
public:
  PipelineCache() = default;
  explicit PipelineCache(AsterPipelineCacheHandle handle) noexcept : handle_(handle) {}
  PipelineCache(PipelineCache &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  PipelineCache &operator=(PipelineCache &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  PipelineCache(const PipelineCache &) = delete;
  PipelineCache &operator=(const PipelineCache &) = delete;
  ~PipelineCache() {
    reset();
  }

  [[nodiscard]] static Result<PipelineCache>
  create(const Engine &engine, const AsterPipelineCacheDesc &desc) noexcept {
    AsterPipelineCacheHandle handle = nullptr;
    const Status status(aster_kernel_pipeline_cache_create(engine.get(), &desc, &handle));
    if (!status) {
      return Result<PipelineCache>(status);
    }
    return Result<PipelineCache>(PipelineCache(handle));
  }

  [[nodiscard]] AsterPipelineCacheHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_pipeline_cache_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterPipelineCacheHandle handle_ = nullptr;
};

class FrameSchedule {
public:
  FrameSchedule() = default;
  explicit FrameSchedule(AsterFrameScheduleHandle handle) noexcept : handle_(handle) {}
  FrameSchedule(FrameSchedule &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  FrameSchedule &operator=(FrameSchedule &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  FrameSchedule(const FrameSchedule &) = delete;
  FrameSchedule &operator=(const FrameSchedule &) = delete;
  ~FrameSchedule() {
    reset();
  }

  [[nodiscard]] Result<AsterFrameScheduleCounts> counts() const noexcept {
    AsterFrameScheduleCounts value{sizeof(AsterFrameScheduleCounts),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_counts(handle_, &value));
    if (!status) {
      return Result<AsterFrameScheduleCounts>(status);
    }
    return Result<AsterFrameScheduleCounts>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameSchedulePassInfo> pass(const size_t index) const noexcept {
    AsterFrameSchedulePassInfo value{sizeof(AsterFrameSchedulePassInfo),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_pass(handle_, index, &value));
    if (!status) {
      return Result<AsterFrameSchedulePassInfo>(status);
    }
    return Result<AsterFrameSchedulePassInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameScheduleMemoryReport> memoryReport() const noexcept {
    AsterFrameScheduleMemoryReport value{sizeof(AsterFrameScheduleMemoryReport),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_memory_report(handle_, &value));
    if (!status) {
      return Result<AsterFrameScheduleMemoryReport>(status);
    }
    return Result<AsterFrameScheduleMemoryReport>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameScheduleDescriptorInfo>
  descriptorLayout(const size_t index) const noexcept {
    AsterFrameScheduleDescriptorInfo value{sizeof(AsterFrameScheduleDescriptorInfo),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_descriptor_layout(handle_, index, &value));
    if (!status) {
      return Result<AsterFrameScheduleDescriptorInfo>(status);
    }
    return Result<AsterFrameScheduleDescriptorInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameSchedulePipelineInfo> pipeline(
      const size_t index) const noexcept {
    AsterFrameSchedulePipelineInfo value{sizeof(AsterFrameSchedulePipelineInfo),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_pipeline(handle_, index, &value));
    if (!status) {
      return Result<AsterFrameSchedulePipelineInfo>(status);
    }
    return Result<AsterFrameSchedulePipelineInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameScheduleTransientAllocationInfo>
  transientAllocation(const size_t index) const noexcept {
    AsterFrameScheduleTransientAllocationInfo value{
        sizeof(AsterFrameScheduleTransientAllocationInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_transient_allocation(handle_, index, &value));
    if (!status) {
      return Result<AsterFrameScheduleTransientAllocationInfo>(status);
    }
    return Result<AsterFrameScheduleTransientAllocationInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterFrameScheduleTimelineInfo> timeline(
      const size_t index) const noexcept {
    AsterFrameScheduleTimelineInfo value{sizeof(AsterFrameScheduleTimelineInfo),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_timeline(handle_, index, &value));
    if (!status) {
      return Result<AsterFrameScheduleTimelineInfo>(status);
    }
    return Result<AsterFrameScheduleTimelineInfo>(std::move(value));
  }

  [[nodiscard]] Result<AsterValidationEvent> validationEvent(
      const size_t index) const noexcept {
    AsterValidationEvent value{sizeof(AsterValidationEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_frame_schedule_validation_event(handle_, index, &value));
    if (!status) {
      return Result<AsterValidationEvent>(status);
    }
    return Result<AsterValidationEvent>(std::move(value));
  }

  [[nodiscard]] AsterFrameScheduleHandle get() const noexcept {
    return handle_;
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_frame_schedule_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterFrameScheduleHandle handle_ = nullptr;
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

  [[nodiscard]] Status renderFrameToTarget(const Scene &scene, const RenderTarget &target,
                                           const AsterCameraDesc &camera,
                                           const AsterRendererSettings &settings) noexcept {
    return Status(aster_kernel_renderer_render_frame_to_target(handle_, scene.get(), target.get(),
                                                               &camera, &settings));
  }

  [[nodiscard]] Status bindWindow(Window &window) noexcept {
    return Status(aster_kernel_renderer_bind_window(handle_, window.get()));
  }

  [[nodiscard]] Status present(Window &window) noexcept {
    return Status(aster_kernel_renderer_present(handle_, window.get()));
  }

  [[nodiscard]] Result<AsterPresentResult> presentFrame(Window &window,
                                                        const AsterPresentDesc &desc) noexcept {
    AsterPresentResult result{sizeof(AsterPresentResult), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_present_frame(handle_, window.get(), &desc, &result));
    if (!status) {
      return Result<AsterPresentResult>(status);
    }
    return Result<AsterPresentResult>(std::move(result));
  }

  [[nodiscard]] Result<AsterRendererPresentationStatus> presentationStatus() const noexcept {
    AsterRendererPresentationStatus value{sizeof(AsterRendererPresentationStatus),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_presentation_status(handle_, &value));
    if (!status) {
      return Result<AsterRendererPresentationStatus>(status);
    }
    return Result<AsterRendererPresentationStatus>(std::move(value));
  }

  [[nodiscard]] Status capture(const AsterCaptureDesc &desc) noexcept {
    return Status(aster_kernel_renderer_capture(handle_, &desc));
  }

  [[nodiscard]] Status captureRenderTarget(const RenderTarget &target,
                                           const AsterCaptureDesc &desc) noexcept {
    return Status(aster_kernel_renderer_capture_render_target(handle_, target.get(), &desc));
  }

  [[nodiscard]] Result<AsterFrameVisionProbeResult>
  frameVisionProbe(const Scene &scene, const AsterCameraDesc &camera,
                   const AsterRendererSettings &settings,
                   const AsterFrameVisionProbeDesc &desc) noexcept {
    AsterFrameVisionProbeResult result{sizeof(AsterFrameVisionProbeResult),
                                       ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_vision_probe(
        handle_, scene.get(), &camera, &settings, &desc, &result));
    if (!status) {
      return Result<AsterFrameVisionProbeResult>(status);
    }
    return Result<AsterFrameVisionProbeResult>(std::move(result));
  }

  [[nodiscard]] Result<AsterFrameLightingProbeResult>
  frameLightingProbe(const Scene &scene, const AsterCameraDesc &camera,
                     const AsterRendererSettings &settings,
                     const AsterFrameLightingProbeDesc &desc) noexcept {
    AsterFrameLightingProbeResult result{sizeof(AsterFrameLightingProbeResult),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_lighting_probe(
        handle_, scene.get(), &camera, &settings, &desc, &result));
    if (!status) {
      return Result<AsterFrameLightingProbeResult>(status);
    }
    return Result<AsterFrameLightingProbeResult>(std::move(result));
  }

  [[nodiscard]] Result<AsterFrameStats> lastStats() const noexcept {
    AsterFrameStats stats{sizeof(AsterFrameStats), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_last_stats(handle_, &stats));
    if (!status) {
      return Result<AsterFrameStats>(status);
    }
    return Result<AsterFrameStats>(std::move(stats));
  }

  [[nodiscard]] Result<size_t> validationEventCount() const noexcept {
    size_t count = 0u;
    const Status status(aster_kernel_renderer_validation_event_count(handle_, &count));
    if (!status) {
      return Result<size_t>(status);
    }
    return Result<size_t>(std::move(count));
  }

  [[nodiscard]] Result<AsterValidationEvent> validationEvent(const size_t index) const noexcept {
    AsterValidationEvent event{sizeof(AsterValidationEvent), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_validation_event(handle_, index, &event));
    if (!status) {
      return Result<AsterValidationEvent>(status);
    }
    return Result<AsterValidationEvent>(std::move(event));
  }

  [[nodiscard]] Result<FrameSchedule> lastFrameSchedule() const noexcept {
    AsterFrameScheduleHandle schedule = nullptr;
    const Status status(aster_kernel_renderer_get_last_frame_schedule(handle_, &schedule));
    if (!status) {
      return Result<FrameSchedule>(status);
    }
    return Result<FrameSchedule>(FrameSchedule(schedule));
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

  [[nodiscard]] Result<AsterFrameForensicsDetailCounts>
  frameForensicsDetailCounts() const noexcept {
    AsterFrameForensicsDetailCounts counts{sizeof(AsterFrameForensicsDetailCounts),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_forensics_detail_counts(handle_, &counts));
    if (!status) {
      return Result<AsterFrameForensicsDetailCounts>(status);
    }
    return Result<AsterFrameForensicsDetailCounts>(std::move(counts));
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

  [[nodiscard]] Result<AsterBeliefReportInfo> frameFalsenessReport() const noexcept {
    AsterBeliefReportInfo report{sizeof(AsterBeliefReportInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_falseness_report(handle_, &report));
    if (!status) {
      return Result<AsterBeliefReportInfo>(status);
    }
    return Result<AsterBeliefReportInfo>(std::move(report));
  }

  [[nodiscard]] Result<AsterBeliefFindingInfo> frameFalsenessFinding(
      const std::uint32_t index) const noexcept {
    AsterBeliefFindingInfo finding{sizeof(AsterBeliefFindingInfo),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_falseness_finding(handle_, index, &finding));
    if (!status) {
      return Result<AsterBeliefFindingInfo>(status);
    }
    return Result<AsterBeliefFindingInfo>(std::move(finding));
  }

  [[nodiscard]] Result<AsterPerceptualWorldScheduleInfo>
  framePerceptualWorldSchedule() const noexcept {
    AsterPerceptualWorldScheduleInfo schedule{sizeof(AsterPerceptualWorldScheduleInfo),
                                              ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_frame_perceptual_world_schedule(handle_, &schedule));
    if (!status) {
      return Result<AsterPerceptualWorldScheduleInfo>(status);
    }
    return Result<AsterPerceptualWorldScheduleInfo>(std::move(schedule));
  }

  [[nodiscard]] Result<AsterPerceptualCausalityGraphInfo>
  framePerceptualCausalityGraph() const noexcept {
    AsterPerceptualCausalityGraphInfo graph{sizeof(AsterPerceptualCausalityGraphInfo),
                                            ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_frame_perceptual_causality_graph(handle_, &graph));
    if (!status) {
      return Result<AsterPerceptualCausalityGraphInfo>(status);
    }
    return Result<AsterPerceptualCausalityGraphInfo>(std::move(graph));
  }

  [[nodiscard]] Result<AsterWorldPerceptualPrimitiveInfo> framePerceptualPrimitive(
      const std::uint32_t index) const noexcept {
    AsterWorldPerceptualPrimitiveInfo primitive{sizeof(AsterWorldPerceptualPrimitiveInfo),
                                                ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_frame_perceptual_primitive(handle_, index, &primitive));
    if (!status) {
      return Result<AsterWorldPerceptualPrimitiveInfo>(status);
    }
    return Result<AsterWorldPerceptualPrimitiveInfo>(std::move(primitive));
  }

  [[nodiscard]] Result<AsterFrameDebugCaptureInfo> debugCaptureInfo(
      const size_t index) const noexcept {
    AsterFrameDebugCaptureInfo capture{sizeof(AsterFrameDebugCaptureInfo),
                                       ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_debug_capture_info(handle_, index, &capture));
    if (!status) {
      return Result<AsterFrameDebugCaptureInfo>(status);
    }
    return Result<AsterFrameDebugCaptureInfo>(std::move(capture));
  }

  [[nodiscard]] Result<AsterFramePassArtifactInfo> passArtifactInfo(
      const size_t index) const noexcept {
    AsterFramePassArtifactInfo artifact{sizeof(AsterFramePassArtifactInfo),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_pass_artifact_info(handle_, index, &artifact));
    if (!status) {
      return Result<AsterFramePassArtifactInfo>(status);
    }
    return Result<AsterFramePassArtifactInfo>(std::move(artifact));
  }

  [[nodiscard]] Result<AsterFrameResourceTransition> resourceTransition(
      const size_t index) const noexcept {
    AsterFrameResourceTransition transition{sizeof(AsterFrameResourceTransition),
                                            ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_resource_transition(handle_, index, &transition));
    if (!status) {
      return Result<AsterFrameResourceTransition>(status);
    }
    return Result<AsterFrameResourceTransition>(std::move(transition));
  }

  [[nodiscard]] Result<AsterObjectRenderFate> objectRenderFate(
      const size_t index) const noexcept {
    AsterObjectRenderFate fate{sizeof(AsterObjectRenderFate), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_object_render_fate(handle_, index, &fate));
    if (!status) {
      return Result<AsterObjectRenderFate>(status);
    }
    return Result<AsterObjectRenderFate>(std::move(fate));
  }

  [[nodiscard]] Result<AsterFrameDebuggerTimelineEventInfo>
  frameDebuggerTimelineEvent(const size_t index) const noexcept {
    AsterFrameDebuggerTimelineEventInfo event{
        sizeof(AsterFrameDebuggerTimelineEventInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_frame_debugger_timeline_event(handle_, index, &event));
    if (!status) {
      return Result<AsterFrameDebuggerTimelineEventInfo>(status);
    }
    return Result<AsterFrameDebuggerTimelineEventInfo>(std::move(event));
  }

  [[nodiscard]] Result<AsterMaterialBindingTraceInfo> materialBindingTrace(
      const size_t index) const noexcept {
    AsterMaterialBindingTraceInfo binding{sizeof(AsterMaterialBindingTraceInfo),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_material_binding_trace(handle_, index, &binding));
    if (!status) {
      return Result<AsterMaterialBindingTraceInfo>(status);
    }
    return Result<AsterMaterialBindingTraceInfo>(std::move(binding));
  }

  [[nodiscard]] Result<AsterAssetFrameTraceInfo> assetFrameTrace(
      const size_t index) const noexcept {
    AsterAssetFrameTraceInfo trace{sizeof(AsterAssetFrameTraceInfo),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_asset_frame_trace(handle_, index, &trace));
    if (!status) {
      return Result<AsterAssetFrameTraceInfo>(status);
    }
    return Result<AsterAssetFrameTraceInfo>(std::move(trace));
  }

  [[nodiscard]] Result<AsterFrameResourceProvenanceInfo> resourceProvenance(
      const size_t index) const noexcept {
    AsterFrameResourceProvenanceInfo provenance{
        sizeof(AsterFrameResourceProvenanceInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_resource_provenance(handle_, index, &provenance));
    if (!status) {
      return Result<AsterFrameResourceProvenanceInfo>(status);
    }
    return Result<AsterFrameResourceProvenanceInfo>(std::move(provenance));
  }

  [[nodiscard]] Result<AsterFrameRegressionGalleryEntryInfo> regressionGalleryEntry(
      const size_t index) const noexcept {
    AsterFrameRegressionGalleryEntryInfo entry{
        sizeof(AsterFrameRegressionGalleryEntryInfo), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_regression_gallery_entry(handle_, index, &entry));
    if (!status) {
      return Result<AsterFrameRegressionGalleryEntryInfo>(status);
    }
    return Result<AsterFrameRegressionGalleryEntryInfo>(std::move(entry));
  }

  [[nodiscard]] Result<AsterFramePipelineSignatureInfo> pipelineSignature(
      const size_t index) const noexcept {
    AsterFramePipelineSignatureInfo signature{sizeof(AsterFramePipelineSignatureInfo),
                                              ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_pipeline_signature(handle_, index, &signature));
    if (!status) {
      return Result<AsterFramePipelineSignatureInfo>(status);
    }
    return Result<AsterFramePipelineSignatureInfo>(std::move(signature));
  }

  [[nodiscard]] Result<AsterFrameMaterialResidencyInfo> materialResidency(
      const size_t index) const noexcept {
    AsterFrameMaterialResidencyInfo residency{sizeof(AsterFrameMaterialResidencyInfo),
                                              ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_material_residency(handle_, index, &residency));
    if (!status) {
      return Result<AsterFrameMaterialResidencyInfo>(status);
    }
    return Result<AsterFrameMaterialResidencyInfo>(std::move(residency));
  }

  [[nodiscard]] Result<AsterRhiValidationEvent> rhiValidationEvent(
      const size_t index) const noexcept {
    AsterRhiValidationEvent event{sizeof(AsterRhiValidationEvent),
                                  ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_rhi_validation_event(handle_, index, &event));
    if (!status) {
      return Result<AsterRhiValidationEvent>(status);
    }
    return Result<AsterRhiValidationEvent>(std::move(event));
  }

  [[nodiscard]] Result<AsterFrameTimestampSample> timestampSample(
      const size_t index) const noexcept {
    AsterFrameTimestampSample sample{sizeof(AsterFrameTimestampSample),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_timestamp_sample(handle_, index, &sample));
    if (!status) {
      return Result<AsterFrameTimestampSample>(status);
    }
    return Result<AsterFrameTimestampSample>(std::move(sample));
  }

  [[nodiscard]] Result<AsterBackendFeatureProof> backendFeatureProof(
      const size_t index) const noexcept {
    AsterBackendFeatureProof proof{sizeof(AsterBackendFeatureProof),
                                   ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_backend_feature_proof(handle_, index, &proof));
    if (!status) {
      return Result<AsterBackendFeatureProof>(status);
    }
    return Result<AsterBackendFeatureProof>(std::move(proof));
  }

  [[nodiscard]] Result<AsterGraphicsCore7VerdictInfo> graphicsCore7Verdict() const noexcept {
    AsterGraphicsCore7VerdictInfo verdict{sizeof(AsterGraphicsCore7VerdictInfo),
                                          ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_renderer_frame_graphics_core7_verdict(handle_, &verdict));
    if (!status) {
      return Result<AsterGraphicsCore7VerdictInfo>(status);
    }
    return Result<AsterGraphicsCore7VerdictInfo>(std::move(verdict));
  }

  [[nodiscard]] Result<AsterGraphicsCore7SignalInfo> graphicsCore7Signal(
      const size_t index) const noexcept {
    AsterGraphicsCore7SignalInfo signal{sizeof(AsterGraphicsCore7SignalInfo),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_renderer_frame_graphics_core7_signal(handle_, index, &signal));
    if (!status) {
      return Result<AsterGraphicsCore7SignalInfo>(status);
    }
    return Result<AsterGraphicsCore7SignalInfo>(std::move(signal));
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

class AuthoringActionExecution {
public:
  AuthoringActionExecution() = default;
  explicit AuthoringActionExecution(AsterAuthoringActionExecutionHandle handle) noexcept
      : handle_(handle) {}

  AuthoringActionExecution(AuthoringActionExecution &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}

  AuthoringActionExecution &operator=(AuthoringActionExecution &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  AuthoringActionExecution(const AuthoringActionExecution &) = delete;
  AuthoringActionExecution &operator=(const AuthoringActionExecution &) = delete;

  ~AuthoringActionExecution() {
    reset();
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterAuthoringActionExecutionHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterAuthoringActionExecutionInfo> info() const noexcept {
    AsterAuthoringActionExecutionInfo info{sizeof(AsterAuthoringActionExecutionInfo),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_action_execution_info(handle_, &info));
    if (!status) {
      return Result<AsterAuthoringActionExecutionInfo>(status);
    }
    return Result<AsterAuthoringActionExecutionInfo>(std::move(info));
  }

  [[nodiscard]] Result<AsterAuthoringDiagnosticInfo> diagnostic(const size_t index) const noexcept {
    AsterAuthoringDiagnosticInfo diagnostic{sizeof(AsterAuthoringDiagnosticInfo),
                                            ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(
        aster_kernel_authoring_action_execution_diagnostic(handle_, index, &diagnostic));
    if (!status) {
      return Result<AsterAuthoringDiagnosticInfo>(status);
    }
    return Result<AsterAuthoringDiagnosticInfo>(std::move(diagnostic));
  }

  [[nodiscard]] Result<AsterAuthoringActionEventInfo> event(const size_t index) const noexcept {
    AsterAuthoringActionEventInfo event{sizeof(AsterAuthoringActionEventInfo),
                                        ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_action_event(handle_, index, &event));
    if (!status) {
      return Result<AsterAuthoringActionEventInfo>(status);
    }
    return Result<AsterAuthoringActionEventInfo>(std::move(event));
  }

  [[nodiscard]] Result<AsterAuthoringKeyValue> eventParameter(
      const size_t event_index, const size_t parameter_index) const noexcept {
    AsterAuthoringKeyValue parameter{sizeof(AsterAuthoringKeyValue),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_action_event_parameter(
        handle_, event_index, parameter_index, &parameter));
    if (!status) {
      return Result<AsterAuthoringKeyValue>(status);
    }
    return Result<AsterAuthoringKeyValue>(std::move(parameter));
  }

  [[nodiscard]] Result<AsterStringView> eventTag(const size_t event_index,
                                                 const size_t tag_index) const noexcept {
    AsterStringView tag{};
    const Status status(
        aster_kernel_authoring_action_event_tag(handle_, event_index, tag_index, &tag));
    if (!status) {
      return Result<AsterStringView>(status);
    }
    return Result<AsterStringView>(std::move(tag));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_authoring_action_execution_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterAuthoringActionExecutionHandle handle_ = nullptr;
};

class AuthoringDocument {
public:
  AuthoringDocument() = default;
  explicit AuthoringDocument(AsterAuthoringDocumentHandle handle) noexcept : handle_(handle) {}

  AuthoringDocument(AuthoringDocument &&other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}

  AuthoringDocument &operator=(AuthoringDocument &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  AuthoringDocument(const AuthoringDocument &) = delete;
  AuthoringDocument &operator=(const AuthoringDocument &) = delete;

  ~AuthoringDocument() {
    reset();
  }

  [[nodiscard]] static Result<AuthoringDocument> load(
      const AsterAuthoringDocumentDesc &desc) noexcept {
    AsterAuthoringDocumentHandle handle = nullptr;
    const Status status(aster_kernel_authoring_document_load(&desc, &handle));
    if (!status) {
      return Result<AuthoringDocument>(status);
    }
    return Result<AuthoringDocument>(AuthoringDocument(handle));
  }

  [[nodiscard]] static Result<AuthoringDocument> parseText(
      const AsterAuthoringDocumentKind kind, const std::string_view source,
      const std::string_view source_path = {}) noexcept {
    const AsterAuthoringDocumentDesc desc{sizeof(AsterAuthoringDocumentDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          kind,
                                          {source.data(), source.size()},
                                          {source_path.data(), source_path.size()},
                                          {}};
    return load(desc);
  }

  [[nodiscard]] static Result<AuthoringDocument> loadPath(
      const AsterAuthoringDocumentKind kind, const std::string_view source_path) noexcept {
    const AsterAuthoringDocumentDesc desc{sizeof(AsterAuthoringDocumentDesc),
                                          ASTER_KERNEL_STRUCT_VERSION_1,
                                          kind,
                                          {},
                                          {source_path.data(), source_path.size()},
                                          {}};
    return load(desc);
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterAuthoringDocumentHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterAuthoringDocumentInfo> info() const noexcept {
    AsterAuthoringDocumentInfo info{sizeof(AsterAuthoringDocumentInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_document_info(handle_, &info));
    if (!status) {
      return Result<AsterAuthoringDocumentInfo>(status);
    }
    return Result<AsterAuthoringDocumentInfo>(std::move(info));
  }

  [[nodiscard]] Result<AsterAuthoringDiagnosticInfo> diagnostic(const size_t index) const noexcept {
    AsterAuthoringDiagnosticInfo diagnostic{sizeof(AsterAuthoringDiagnosticInfo),
                                            ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_document_diagnostic(handle_, index, &diagnostic));
    if (!status) {
      return Result<AsterAuthoringDiagnosticInfo>(status);
    }
    return Result<AsterAuthoringDiagnosticInfo>(std::move(diagnostic));
  }

  [[nodiscard]] Result<AsterAuthoringProjectAssetInfo> projectAsset(
      const size_t index) const noexcept {
    AsterAuthoringProjectAssetInfo asset{sizeof(AsterAuthoringProjectAssetInfo),
                                         ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_project_asset(handle_, index, &asset));
    if (!status) {
      return Result<AsterAuthoringProjectAssetInfo>(status);
    }
    return Result<AsterAuthoringProjectAssetInfo>(std::move(asset));
  }

  [[nodiscard]] Result<AsterAuthoringEntityInfo> entity(const size_t index) const noexcept {
    AsterAuthoringEntityInfo entity{sizeof(AsterAuthoringEntityInfo),
                                    ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_entity(handle_, index, &entity));
    if (!status) {
      return Result<AsterAuthoringEntityInfo>(status);
    }
    return Result<AsterAuthoringEntityInfo>(std::move(entity));
  }

  [[nodiscard]] Result<AsterAuthoringActionNodeInfo> actionNode(
      const size_t index) const noexcept {
    AsterAuthoringActionNodeInfo node{sizeof(AsterAuthoringActionNodeInfo),
                                      ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_action_node(handle_, index, &node));
    if (!status) {
      return Result<AsterAuthoringActionNodeInfo>(status);
    }
    return Result<AsterAuthoringActionNodeInfo>(std::move(node));
  }

  [[nodiscard]] Result<AsterAuthoringKeyValue> actionNodeParameter(
      const size_t node_index, const size_t parameter_index) const noexcept {
    AsterAuthoringKeyValue parameter{sizeof(AsterAuthoringKeyValue),
                                     ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_action_node_parameter(
        handle_, node_index, parameter_index, &parameter));
    if (!status) {
      return Result<AsterAuthoringKeyValue>(status);
    }
    return Result<AsterAuthoringKeyValue>(std::move(parameter));
  }

  [[nodiscard]] Result<AsterStringView> actionNodeTag(const size_t node_index,
                                                      const size_t tag_index) const noexcept {
    AsterStringView tag{};
    const Status status(aster_kernel_authoring_action_node_tag(handle_, node_index, tag_index, &tag));
    if (!status) {
      return Result<AsterStringView>(status);
    }
    return Result<AsterStringView>(std::move(tag));
  }

  [[nodiscard]] Result<AsterAuthoringInputBindingInfo> inputBinding(
      const size_t index) const noexcept {
    AsterAuthoringInputBindingInfo binding{sizeof(AsterAuthoringInputBindingInfo),
                                           ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_authoring_input_binding(handle_, index, &binding));
    if (!status) {
      return Result<AsterAuthoringInputBindingInfo>(status);
    }
    return Result<AsterAuthoringInputBindingInfo>(std::move(binding));
  }

  [[nodiscard]] Result<AsterStringView> inputBindingTag(const size_t binding_index,
                                                        const size_t tag_index) const noexcept {
    AsterStringView tag{};
    const Status status(
        aster_kernel_authoring_input_binding_tag(handle_, binding_index, tag_index, &tag));
    if (!status) {
      return Result<AsterStringView>(status);
    }
    return Result<AsterStringView>(std::move(tag));
  }

  [[nodiscard]] Result<AuthoringActionExecution> executeAction(
      const AsterAuthoringActionContext &context) const noexcept {
    AsterAuthoringActionExecutionHandle execution = nullptr;
    const Status status(aster_kernel_authoring_action_execute(handle_, &context, &execution));
    if (!status) {
      return Result<AuthoringActionExecution>(status);
    }
    return Result<AuthoringActionExecution>(AuthoringActionExecution(execution));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_authoring_document_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterAuthoringDocumentHandle handle_ = nullptr;
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

namespace physics {

class World {
public:
  World() = default;
  explicit World(AsterPhysicsWorldHandle handle) noexcept : handle_(handle) {}

  World(World &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  World &operator=(World &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  World(const World &) = delete;
  World &operator=(const World &) = delete;

  ~World() {
    reset();
  }

  [[nodiscard]] static Result<World> create(const AsterPhysicsWorldDesc &desc) noexcept {
    AsterPhysicsWorldHandle handle = nullptr;
    const Status status(aster_kernel_physics_world_create(&desc, &handle));
    if (!status) {
      return Result<World>(status);
    }
    return Result<World>(World(handle));
  }

  [[nodiscard]] static Result<World> create() noexcept {
    const AsterPhysicsWorldDesc desc{sizeof(AsterPhysicsWorldDesc),
                                     ASTER_KERNEL_STRUCT_VERSION_1,
                                     {0.0f, -9.81f, 0.0f},
                                     6,
                                     1.0f / 120.0f,
                                     0.035f,
                                     0.020f,
                                     0.55f};
    return create(desc);
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr;
  }

  [[nodiscard]] AsterPhysicsWorldHandle get() const noexcept {
    return handle_;
  }

  [[nodiscard]] Result<AsterPhysicsBodyHandle>
  createBody(const AsterPhysicsBodyDesc &desc) const noexcept {
    AsterPhysicsBodyHandle body{};
    const Status status(aster_kernel_physics_body_create(handle_, &desc, &body));
    if (!status) {
      return Result<AsterPhysicsBodyHandle>(status);
    }
    return Result<AsterPhysicsBodyHandle>(std::move(body));
  }

  [[nodiscard]] Status destroyBody(const AsterPhysicsBodyHandle body) const noexcept {
    return Status(aster_kernel_physics_body_destroy(handle_, body));
  }

  [[nodiscard]] Result<AsterPhysicsStepResult> step(const AsterPhysicsStepDesc &desc) const noexcept {
    AsterPhysicsStepResult result{sizeof(AsterPhysicsStepResult), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_physics_world_step(handle_, &desc, &result));
    if (!status) {
      return Result<AsterPhysicsStepResult>(status);
    }
    return Result<AsterPhysicsStepResult>(std::move(result));
  }

  [[nodiscard]] Result<AsterPhysicsBodyState>
  bodyState(const AsterPhysicsBodyHandle body) const noexcept {
    AsterPhysicsBodyState state{sizeof(AsterPhysicsBodyState), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_physics_body_state(handle_, body, &state));
    if (!status) {
      return Result<AsterPhysicsBodyState>(status);
    }
    return Result<AsterPhysicsBodyState>(std::move(state));
  }

  [[nodiscard]] Status setBodyState(const AsterPhysicsBodyHandle body,
                                    const AsterPhysicsBodyState &state) const noexcept {
    return Status(aster_kernel_physics_body_set_state(handle_, body, &state));
  }

  [[nodiscard]] Status applyForce(const AsterPhysicsBodyHandle body,
                                  const AsterVec3 force) const noexcept {
    return Status(aster_kernel_physics_body_apply_force(handle_, body, force));
  }

  [[nodiscard]] Status applyTorque(const AsterPhysicsBodyHandle body,
                                   const AsterVec3 torque) const noexcept {
    return Status(aster_kernel_physics_body_apply_torque(handle_, body, torque));
  }

  [[nodiscard]] Status applyImpulse(const AsterPhysicsBodyHandle body,
                                    const AsterVec3 impulse) const noexcept {
    return Status(aster_kernel_physics_body_apply_impulse(handle_, body, impulse));
  }

  [[nodiscard]] Result<AsterPhysicsStats> stats() const noexcept {
    AsterPhysicsStats value{sizeof(AsterPhysicsStats), ASTER_KERNEL_STRUCT_VERSION_1};
    const Status status(aster_kernel_physics_world_stats(handle_, &value));
    if (!status) {
      return Result<AsterPhysicsStats>(status);
    }
    return Result<AsterPhysicsStats>(std::move(value));
  }

  void reset() noexcept {
    if (handle_ != nullptr) {
      (void)aster_kernel_physics_world_destroy(handle_);
      handle_ = nullptr;
    }
  }

private:
  AsterPhysicsWorldHandle handle_ = nullptr;
};

[[nodiscard]] inline Result<AsterFrameControlOutput>
evaluateFrameControl(const AsterFrameControlInput &input) noexcept {
  AsterFrameControlOutput output{sizeof(AsterFrameControlOutput), ASTER_KERNEL_STRUCT_VERSION_1};
  const Status status(aster_kernel_frame_control_evaluate(&input, &output));
  if (!status) {
    return Result<AsterFrameControlOutput>(status);
  }
  return Result<AsterFrameControlOutput>(std::move(output));
}

} // namespace physics

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
