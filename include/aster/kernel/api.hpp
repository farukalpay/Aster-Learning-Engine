// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/kernel/abi.h"

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

[[nodiscard]] inline AsterStringView stringView(const char *text, const size_t size) noexcept {
  return {text, size};
}

[[nodiscard]] inline AsterAbiVersion abiVersion() noexcept {
  return aster_kernel_abi_version();
}

} // namespace aster::kernel
