// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace aster {

struct Mat3 {
  std::array<float, 9> m{};

  [[nodiscard]] const float *data() const {
    return m.data();
  }

  [[nodiscard]] float *data() {
    return m.data();
  }
};

inline Mat3 identity3() {
  Mat3 out{};
  out.m[0] = 1.0f;
  out.m[4] = 1.0f;
  out.m[8] = 1.0f;
  return out;
}

inline Mat3 mat3FromColumns(const Vec3 x, const Vec3 y, const Vec3 z) {
  return {{{x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z}}};
}

inline Vec3 column(const Mat3 &matrix, const int index) {
  const int base = index * 3;
  return {matrix.m[base + 0], matrix.m[base + 1], matrix.m[base + 2]};
}

inline float at(const Mat3 &matrix, const int row, const int column_index) {
  return matrix.m[column_index * 3 + row];
}

inline void setAt(Mat3 &matrix, const int row, const int column_index, const float value) {
  matrix.m[column_index * 3 + row] = value;
}

inline Mat3 multiply(const Mat3 &lhs, const Mat3 &rhs) {
  Mat3 out{};
  for (int column_index = 0; column_index < 3; ++column_index) {
    for (int row = 0; row < 3; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 3; ++k) {
        sum += at(lhs, row, k) * at(rhs, k, column_index);
      }
      setAt(out, row, column_index, sum);
    }
  }
  return out;
}

inline Mat3 operator*(const Mat3 &lhs, const Mat3 &rhs) {
  return multiply(lhs, rhs);
}

inline Vec3 operator*(const Mat3 &matrix, const Vec3 value) {
  return {
      matrix.m[0] * value.x + matrix.m[3] * value.y + matrix.m[6] * value.z,
      matrix.m[1] * value.x + matrix.m[4] * value.y + matrix.m[7] * value.z,
      matrix.m[2] * value.x + matrix.m[5] * value.y + matrix.m[8] * value.z,
  };
}

inline Mat3 transpose(const Mat3 &matrix) {
  Mat3 out{};
  for (int column_index = 0; column_index < 3; ++column_index) {
    for (int row = 0; row < 3; ++row) {
      setAt(out, row, column_index, at(matrix, column_index, row));
    }
  }
  return out;
}

inline float determinant(const Mat3 &matrix) {
  return at(matrix, 0, 0) * (at(matrix, 1, 1) * at(matrix, 2, 2) -
                             at(matrix, 1, 2) * at(matrix, 2, 1)) -
         at(matrix, 0, 1) * (at(matrix, 1, 0) * at(matrix, 2, 2) -
                             at(matrix, 1, 2) * at(matrix, 2, 0)) +
         at(matrix, 0, 2) * (at(matrix, 1, 0) * at(matrix, 2, 1) -
                             at(matrix, 1, 1) * at(matrix, 2, 0));
}

inline std::optional<Mat3> try_inverse(const Mat3 &matrix,
                                       const float epsilon = 0.000001f) {
  const float det = determinant(matrix);
  if (std::abs(det) <= epsilon) {
    return std::nullopt;
  }

  Mat3 out{};
  setAt(out, 0, 0, +(at(matrix, 1, 1) * at(matrix, 2, 2) -
                     at(matrix, 2, 1) * at(matrix, 1, 2)) /
                       det);
  setAt(out, 0, 1, -(at(matrix, 0, 1) * at(matrix, 2, 2) -
                     at(matrix, 2, 1) * at(matrix, 0, 2)) /
                       det);
  setAt(out, 0, 2, +(at(matrix, 0, 1) * at(matrix, 1, 2) -
                     at(matrix, 1, 1) * at(matrix, 0, 2)) /
                       det);
  setAt(out, 1, 0, -(at(matrix, 1, 0) * at(matrix, 2, 2) -
                     at(matrix, 2, 0) * at(matrix, 1, 2)) /
                       det);
  setAt(out, 1, 1, +(at(matrix, 0, 0) * at(matrix, 2, 2) -
                     at(matrix, 2, 0) * at(matrix, 0, 2)) /
                       det);
  setAt(out, 1, 2, -(at(matrix, 0, 0) * at(matrix, 1, 2) -
                     at(matrix, 1, 0) * at(matrix, 0, 2)) /
                       det);
  setAt(out, 2, 0, +(at(matrix, 1, 0) * at(matrix, 2, 1) -
                     at(matrix, 2, 0) * at(matrix, 1, 1)) /
                       det);
  setAt(out, 2, 1, -(at(matrix, 0, 0) * at(matrix, 2, 1) -
                     at(matrix, 2, 0) * at(matrix, 0, 1)) /
                       det);
  setAt(out, 2, 2, +(at(matrix, 0, 0) * at(matrix, 1, 1) -
                     at(matrix, 1, 0) * at(matrix, 0, 1)) /
                       det);
  return out;
}

inline Mat3 inverse(const Mat3 &matrix) {
  if (const std::optional<Mat3> result = try_inverse(matrix)) {
    return *result;
  }
  throw std::runtime_error("Mat3 inverse requested for a singular matrix.");
}

inline Mat3 inverseTranspose(const Mat3 &matrix) {
  return transpose(inverse(matrix));
}

} // namespace aster
