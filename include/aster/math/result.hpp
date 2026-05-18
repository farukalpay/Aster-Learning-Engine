// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace aster {

enum class MathError {
  None,
  InvalidArgument,
  NonFiniteInput,
  DegenerateInput,
  SingularMatrix,
  UnsupportedPolicy,
};

struct MathPolicy {
  float absolute_epsilon = 0.000001f;
  float relative_epsilon = 0.00001f;
  std::uint32_t max_iterations = 32u;
  bool deterministic = true;
  bool debug_strict = false;
};

struct MathDiagnostics {
  MathError error = MathError::None;
  float determinant = 0.0f;
  float condition_hint = 0.0f;
  const char *message = "ok";
};

template <typename T> struct MathResult {
  T value{};
  MathDiagnostics diagnostics{};
  bool ok = true;

  [[nodiscard]] explicit constexpr operator bool() const {
    return ok;
  }

  [[nodiscard]] static constexpr MathResult success(const T &result) {
    return {.value = result, .diagnostics = {}, .ok = true};
  }

  [[nodiscard]] static constexpr MathResult failure(const MathError error, const char *message,
                                                    const T &fallback = {}) {
    return {.value = fallback,
            .diagnostics = {.error = error, .determinant = 0.0f, .condition_hint = 0.0f,
                            .message = message},
            .ok = false};
  }
};

[[nodiscard]] inline constexpr MathPolicy defaultMathPolicy() {
  return {};
}

template <typename T> struct ScalarTraits {
  static_assert(std::is_arithmetic_v<T>, "Aster math scalar types must be arithmetic.");

  static constexpr bool floating = std::is_floating_point_v<T>;

  [[nodiscard]] static constexpr T epsilon() {
    if constexpr (std::is_floating_point_v<T>) {
      return std::numeric_limits<T>::epsilon();
    } else {
      return T{0};
    }
  }

  [[nodiscard]] static constexpr T defaultAbsoluteEpsilon() {
    if constexpr (std::is_same_v<T, double>) {
      return static_cast<T>(0.000000000001);
    } else if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(0.000001);
    } else {
      return T{0};
    }
  }

  [[nodiscard]] static constexpr T defaultRelativeEpsilon() {
    if constexpr (std::is_same_v<T, double>) {
      return static_cast<T>(0.0000000001);
    } else if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(0.00001);
    } else {
      return T{0};
    }
  }
};

template <typename T> [[nodiscard]] inline bool isFiniteScalar(const T value) {
  if constexpr (std::is_floating_point_v<T>) {
    return std::isfinite(value);
  } else {
    return true;
  }
}

template <typename T>
[[nodiscard]] inline bool nearlyEqual(const T lhs, const T rhs,
                                      const T absolute_epsilon =
                                          ScalarTraits<T>::defaultAbsoluteEpsilon(),
                                      const T relative_epsilon =
                                          ScalarTraits<T>::defaultRelativeEpsilon()) {
  if constexpr (!std::is_floating_point_v<T>) {
    return lhs == rhs;
  } else {
    const T delta = std::abs(lhs - rhs);
    if (delta <= absolute_epsilon) {
      return true;
    }
    return delta <= std::max(std::abs(lhs), std::abs(rhs)) * relative_epsilon;
  }
}

template <typename T>
[[nodiscard]] inline bool nearZero(const T value,
                                   const T absolute_epsilon =
                                       ScalarTraits<T>::defaultAbsoluteEpsilon()) {
  if constexpr (!std::is_floating_point_v<T>) {
    return value == T{0};
  } else {
    return std::abs(value) <= absolute_epsilon;
  }
}

[[nodiscard]] inline std::uint32_t orderedFloatBits(const float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  return (bits & 0x80000000u) != 0u ? 0x80000000u - (bits & 0x7fffffffu) : bits + 0x80000000u;
}

[[nodiscard]] inline std::uint64_t orderedDoubleBits(const double value) {
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
  return (bits & 0x8000000000000000ull) != 0ull
             ? 0x8000000000000000ull - (bits & 0x7fffffffffffffffull)
             : bits + 0x8000000000000000ull;
}

[[nodiscard]] inline std::uint64_t ulpDistance(const float lhs, const float rhs) {
  const std::uint32_t a = orderedFloatBits(lhs);
  const std::uint32_t b = orderedFloatBits(rhs);
  return a > b ? static_cast<std::uint64_t>(a - b) : static_cast<std::uint64_t>(b - a);
}

[[nodiscard]] inline std::uint64_t ulpDistance(const double lhs, const double rhs) {
  const std::uint64_t a = orderedDoubleBits(lhs);
  const std::uint64_t b = orderedDoubleBits(rhs);
  return a > b ? a - b : b - a;
}

template <typename T>
[[nodiscard]] inline bool almostEqualUlps(const T lhs, const T rhs, const std::uint64_t max_ulps) {
  if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    if (!isFiniteScalar(lhs) || !isFiniteScalar(rhs)) {
      return false;
    }
    return ulpDistance(lhs, rhs) <= max_ulps;
  } else {
    return lhs == rhs;
  }
}

} // namespace aster
