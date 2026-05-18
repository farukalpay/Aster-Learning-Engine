// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/result.hpp"
#include "aster/math/vec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aster {

template <typename T, std::size_t Columns, std::size_t Rows> struct Mat {
  using value_type = T;
  static constexpr std::size_t columns = Columns;
  static constexpr std::size_t rows = Rows;

  std::array<T, Columns * Rows> m{};

  [[nodiscard]] constexpr const T *data() const {
    return m.data();
  }

  [[nodiscard]] constexpr T *data() {
    return m.data();
  }
};

// Aster matrices are stored column-major and multiplied as column-vector transforms:
// `clip = projection * view * model * local_position`. GPU upload code must preserve this
// contract or explicitly transpose at the backend boundary.
enum class MatrixStorageOrder {
  ColumnMajor,
  RowMajor,
};

enum class VectorConvention {
  ColumnVector,
  RowVector,
};

enum class ViewportOrigin {
  TopLeft,
  BottomLeft,
};

enum class YAxisConvention {
  Up,
  Down,
};

enum class WindingOrder {
  CounterClockwise,
  Clockwise,
};

enum class CullConvention {
  Back,
  Front,
  None,
};

enum class CoordinateHandedness {
  RightHanded,
  LeftHanded,
};

enum class ClipDepthRange {
  ZeroToOne,
  NegativeOneToOne,
};

enum class DepthDirection {
  ForwardZ,
  ReverseZ,
};

template <typename T> using Mat2T = Mat<T, 2, 2>;
template <typename T> using Mat3T = Mat<T, 3, 3>;
template <typename T> using Mat4T = Mat<T, 4, 4>;
template <typename T> using Mat2x3T = Mat<T, 2, 3>;
template <typename T> using Mat3x2T = Mat<T, 3, 2>;
template <typename T> using Mat3x4T = Mat<T, 3, 4>;
template <typename T> using Mat4x3T = Mat<T, 4, 3>;

using Mat2 = Mat2T<float>;
using Mat3 = Mat3T<float>;
using Mat4 = Mat4T<float>;
using DMat2 = Mat2T<double>;
using DMat3 = Mat3T<double>;
using DMat4 = Mat4T<double>;
using IMat2 = Mat2T<std::int32_t>;
using IMat3 = Mat3T<std::int32_t>;
using IMat4 = Mat4T<std::int32_t>;
using Mat2x3 = Mat2x3T<float>;
using Mat3x2 = Mat3x2T<float>;
using Mat3x4 = Mat3x4T<float>;
using Mat4x3 = Mat4x3T<float>;

template <typename Tag> struct SemanticMat4 {
  Mat4 value{};

  constexpr SemanticMat4() = default;
  constexpr SemanticMat4(const Mat4 matrix) : value(matrix) {}
  constexpr SemanticMat4(const SemanticMat4 &) = default;
  constexpr SemanticMat4 &operator=(const SemanticMat4 &) = default;

  [[nodiscard]] constexpr operator Mat4() const {
    return value;
  }

  constexpr SemanticMat4 &operator=(const Mat4 matrix) {
    value = matrix;
    return *this;
  }
};

using LocalToWorld = SemanticMat4<LocalToWorldTag>;
using WorldToView = SemanticMat4<WorldToViewTag>;
using ViewToClip = SemanticMat4<ViewToClipTag>;
using WorldToClip = SemanticMat4<WorldToClipTag>;
using ClipToWorld = SemanticMat4<ClipToWorldTag>;

struct Viewport {
  Vec2 origin{};
  Vec2 size{};
  ViewportOrigin origin_convention = ViewportOrigin::TopLeft;
};

struct RenderConvention {
  CoordinateHandedness handedness = CoordinateHandedness::RightHanded;
  ClipDepthRange depth_range = ClipDepthRange::ZeroToOne;
  DepthDirection depth_direction = DepthDirection::ReverseZ;
  ViewportOrigin viewport_origin = ViewportOrigin::TopLeft;
  bool y_flip = true;
  MatrixStorageOrder matrix_storage = MatrixStorageOrder::ColumnMajor;
  VectorConvention vector_convention = VectorConvention::ColumnVector;
  WindingOrder winding = WindingOrder::CounterClockwise;
  CullConvention cull = CullConvention::Back;
};

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr T at(const Mat<T, Columns, Rows> &matrix, const int row,
                                    const int column) {
  return matrix.m[static_cast<std::size_t>(column) * Rows + static_cast<std::size_t>(row)];
}

template <typename T, std::size_t Columns, std::size_t Rows>
inline constexpr void setAt(Mat<T, Columns, Rows> &matrix, const int row, const int column,
                            const T value) {
  matrix.m[static_cast<std::size_t>(column) * Rows + static_cast<std::size_t>(row)] = value;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr MathResult<T> checkedAt(const Mat<T, Columns, Rows> &matrix,
                                                       const std::size_t row,
                                                       const std::size_t column) {
  if (row >= Rows || column >= Columns) {
    return MathResult<T>::failure(MathError::InvalidArgument, "Matrix index is out of range.");
  }
  return MathResult<T>::success(
      matrix.m[static_cast<std::size_t>(column) * Rows + static_cast<std::size_t>(row)]);
}

template <typename T, std::size_t N> [[nodiscard]] inline constexpr Mat<T, N, N> identityMatrix() {
  Mat<T, N, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out.m[i * N + i] = static_cast<T>(1);
  }
  return out;
}

[[nodiscard]] inline constexpr Mat2 identity2() {
  return identityMatrix<float, 2>();
}

[[nodiscard]] inline constexpr Mat3 identity3() {
  return identityMatrix<float, 3>();
}

[[nodiscard]] inline constexpr Mat4 identity() {
  return identityMatrix<float, 4>();
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr Vec<T, Rows> column(const Mat<T, Columns, Rows> &matrix,
                                                   const int index) {
  Vec<T, Rows> out{};
  const std::size_t base = static_cast<std::size_t>(index) * Rows;
  for (std::size_t row = 0; row < Rows; ++row) {
    out[row] = matrix.m[base + row];
  }
  return out;
}

template <typename T>
[[nodiscard]] inline constexpr Mat2T<T> mat2FromColumns(const Vec2T<T> x, const Vec2T<T> y) {
  return {{{x.x, x.y, y.x, y.y}}};
}

[[nodiscard]] inline constexpr Mat2 mat2FromColumns(const Vec2 x, const Vec2 y) {
  return mat2FromColumns<float>(x, y);
}

template <typename T>
[[nodiscard]] inline constexpr Mat3T<T> mat3FromColumns(const Vec3T<T> x, const Vec3T<T> y,
                                                        const Vec3T<T> z) {
  return {{{x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z}}};
}

[[nodiscard]] inline constexpr Mat3 mat3FromColumns(const Vec3 x, const Vec3 y, const Vec3 z) {
  return mat3FromColumns<float>(x, y, z);
}

template <typename T>
[[nodiscard]] inline constexpr Mat4T<T> mat4FromColumns(const Vec4T<T> x, const Vec4T<T> y,
                                                        const Vec4T<T> z, const Vec4T<T> w) {
  return {{{x.x, x.y, x.z, x.w, y.x, y.y, y.z, y.w, z.x, z.y, z.z, z.w, w.x, w.y, w.z,
            w.w}}};
}

[[nodiscard]] inline constexpr Mat4 mat4FromColumns(const Vec4 x, const Vec4 y, const Vec4 z,
                                                    const Vec4 w) {
  return mat4FromColumns<float>(x, y, z, w);
}

template <typename T, std::size_t LhsColumns, std::size_t LhsRows, std::size_t RhsColumns>
[[nodiscard]] inline constexpr Mat<T, RhsColumns, LhsRows>
multiply(const Mat<T, LhsColumns, LhsRows> &lhs, const Mat<T, RhsColumns, LhsColumns> &rhs) {
  Mat<T, RhsColumns, LhsRows> out{};
  for (std::size_t column_index = 0; column_index < RhsColumns; ++column_index) {
    for (std::size_t row = 0; row < LhsRows; ++row) {
      T sum{};
      for (std::size_t k = 0; k < LhsColumns; ++k) {
        sum += at(lhs, static_cast<int>(row), static_cast<int>(k)) *
               at(rhs, static_cast<int>(k), static_cast<int>(column_index));
      }
      setAt(out, static_cast<int>(row), static_cast<int>(column_index), sum);
    }
  }
  return out;
}

template <typename T, std::size_t LhsColumns, std::size_t LhsRows, std::size_t RhsColumns>
[[nodiscard]] inline constexpr Mat<T, RhsColumns, LhsRows>
operator*(const Mat<T, LhsColumns, LhsRows> &lhs,
          const Mat<T, RhsColumns, LhsColumns> &rhs) {
  return multiply(lhs, rhs);
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr Vec<T, Rows> operator*(const Mat<T, Columns, Rows> &matrix,
                                                      const Vec<T, Columns> value) {
  Vec<T, Rows> out{};
  for (std::size_t row = 0; row < Rows; ++row) {
    T sum{};
    for (std::size_t column_index = 0; column_index < Columns; ++column_index) {
      sum += at(matrix, static_cast<int>(row), static_cast<int>(column_index)) *
             value[column_index];
    }
    out[row] = sum;
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr Mat<T, Columns, Rows>
operator+(const Mat<T, Columns, Rows> &lhs, const Mat<T, Columns, Rows> &rhs) {
  Mat<T, Columns, Rows> out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = lhs.m[index] + rhs.m[index];
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr Mat<T, Columns, Rows>
operator-(const Mat<T, Columns, Rows> &lhs, const Mat<T, Columns, Rows> &rhs) {
  Mat<T, Columns, Rows> out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = lhs.m[index] - rhs.m[index];
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows, typename S>
[[nodiscard]] inline constexpr Mat<T, Columns, Rows>
operator*(const Mat<T, Columns, Rows> &matrix, const S scalar) {
  Mat<T, Columns, Rows> out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = static_cast<T>(matrix.m[index] * scalar);
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows, typename S>
[[nodiscard]] inline constexpr Mat<T, Columns, Rows>
operator/(const Mat<T, Columns, Rows> &matrix, const S scalar) {
  Mat<T, Columns, Rows> out{};
  for (std::size_t index = 0; index < out.m.size(); ++index) {
    out.m[index] = static_cast<T>(matrix.m[index] / scalar);
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr Mat<T, Rows, Columns>
transpose(const Mat<T, Columns, Rows> &matrix) {
  Mat<T, Rows, Columns> out{};
  for (std::size_t column_index = 0; column_index < Columns; ++column_index) {
    for (std::size_t row = 0; row < Rows; ++row) {
      setAt(out, static_cast<int>(column_index), static_cast<int>(row),
            at(matrix, static_cast<int>(row), static_cast<int>(column_index)));
    }
  }
  return out;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline bool allFinite(const Mat<T, Columns, Rows> &matrix) {
  for (const T value : matrix.m) {
    if (!isFiniteScalar(value)) {
      return false;
    }
  }
  return true;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline constexpr bool exactEqual(const Mat<T, Columns, Rows> &lhs,
                                               const Mat<T, Columns, Rows> &rhs) {
  for (std::size_t i = 0; i < lhs.m.size(); ++i) {
    if (lhs.m[i] != rhs.m[i]) {
      return false;
    }
  }
  return true;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline bool nearEqual(const Mat<T, Columns, Rows> &lhs,
                                    const Mat<T, Columns, Rows> &rhs,
                                    const T absolute_epsilon =
                                        ScalarTraits<T>::defaultAbsoluteEpsilon(),
                                    const T relative_epsilon =
                                        ScalarTraits<T>::defaultRelativeEpsilon()) {
  for (std::size_t i = 0; i < lhs.m.size(); ++i) {
    if (!nearEqual(lhs.m[i], rhs.m[i], absolute_epsilon, relative_epsilon)) {
      return false;
    }
  }
  return true;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline bool ulpsEqual(const Mat<T, Columns, Rows> &lhs,
                                    const Mat<T, Columns, Rows> &rhs,
                                    const std::uint64_t max_ulps) {
  for (std::size_t i = 0; i < lhs.m.size(); ++i) {
    if (!ulpsEqual(lhs.m[i], rhs.m[i], max_ulps)) {
      return false;
    }
  }
  return true;
}

template <typename T, std::size_t Columns, std::size_t Rows>
[[nodiscard]] inline T maxAbsElement(const Mat<T, Columns, Rows> &matrix) {
  T out{};
  for (const T value : matrix.m) {
    out = max(out, static_cast<T>(std::abs(value)));
  }
  return out;
}

template <typename T> [[nodiscard]] inline constexpr T determinant(const Mat2T<T> &matrix) {
  return at(matrix, 0, 0) * at(matrix, 1, 1) - at(matrix, 0, 1) * at(matrix, 1, 0);
}

template <typename T> [[nodiscard]] inline constexpr T determinant(const Mat3T<T> &matrix) {
  return at(matrix, 0, 0) * (at(matrix, 1, 1) * at(matrix, 2, 2) -
                             at(matrix, 1, 2) * at(matrix, 2, 1)) -
         at(matrix, 0, 1) * (at(matrix, 1, 0) * at(matrix, 2, 2) -
                             at(matrix, 1, 2) * at(matrix, 2, 0)) +
         at(matrix, 0, 2) * (at(matrix, 1, 0) * at(matrix, 2, 1) -
                             at(matrix, 1, 1) * at(matrix, 2, 0));
}

template <typename T>
[[nodiscard]] inline constexpr T determinant3x3(const T a00, const T a01, const T a02,
                                                const T a10, const T a11, const T a12,
                                                const T a20, const T a21, const T a22) {
  return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
         a02 * (a10 * a21 - a11 * a20);
}

template <typename T> [[nodiscard]] inline T determinant(const Mat4T<T> &matrix) {
  T det = T{};
  for (int column_index = 0; column_index < 4; ++column_index) {
    T minor[9]{};
    int index = 0;
    for (int c = 0; c < 4; ++c) {
      if (c == column_index) {
        continue;
      }
      for (int r = 1; r < 4; ++r) {
        minor[index++] = at(matrix, r, c);
      }
    }
    const T sign = (column_index % 2) == 0 ? static_cast<T>(1) : static_cast<T>(-1);
    det += sign * at(matrix, 0, column_index) *
           determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7],
                          minor[2], minor[5], minor[8]);
  }
  return det;
}

template <typename T>
[[nodiscard]] inline MathResult<Mat2T<T>> inverse(const Mat2T<T> &matrix,
                                                  const MathPolicy policy = defaultMathPolicy()) {
  if (!allFinite(matrix)) {
    auto out = MathResult<Mat2T<T>>::failure(MathError::NonFiniteInput,
                                             "Mat2 inverse requires finite inputs.");
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }
  const T det = determinant(matrix);
  if (nearZero(det, static_cast<T>(policy.absolute_epsilon))) {
    auto out = MathResult<Mat2T<T>>::failure(MathError::SingularMatrix,
                                             "Mat2 inverse requested for a singular matrix.");
    out.diagnostics.determinant = static_cast<float>(det);
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }
  const T inv_det = static_cast<T>(1) / det;
  const Mat2T<T> out{{{at(matrix, 1, 1) * inv_det, -at(matrix, 1, 0) * inv_det,
                       -at(matrix, 0, 1) * inv_det, at(matrix, 0, 0) * inv_det}}};
  return MathResult<Mat2T<T>>::success(out);
}

template <typename T>
[[nodiscard]] inline MathResult<Mat3T<T>> inverse(const Mat3T<T> &matrix,
                                                  const MathPolicy policy = defaultMathPolicy()) {
  if (!allFinite(matrix)) {
    auto out = MathResult<Mat3T<T>>::failure(MathError::NonFiniteInput,
                                             "Mat3 inverse requires finite inputs.");
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }
  const T det = determinant(matrix);
  if (nearZero(det, static_cast<T>(policy.absolute_epsilon))) {
    auto out = MathResult<Mat3T<T>>::failure(MathError::SingularMatrix,
                                             "Mat3 inverse requested for a singular matrix.");
    out.diagnostics.determinant = static_cast<float>(det);
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }

  Mat3T<T> out{};
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
  return MathResult<Mat3T<T>>::success(out);
}

template <typename T>
[[nodiscard]] inline MathResult<Mat4T<T>> inverse(const Mat4T<T> &matrix,
                                                  const MathPolicy policy = defaultMathPolicy()) {
  if (!allFinite(matrix)) {
    auto out = MathResult<Mat4T<T>>::failure(MathError::NonFiniteInput,
                                             "Mat4 inverse requires finite inputs.");
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }
  const T det = determinant(matrix);
  if (nearZero(det, static_cast<T>(policy.absolute_epsilon))) {
    auto out = MathResult<Mat4T<T>>::failure(MathError::SingularMatrix,
                                             "Mat4 inverse requested for a singular matrix.");
    out.diagnostics.determinant = static_cast<float>(det);
    recordMathDiagnostic(MathDiagnosticOperation::MatrixInverse, MathDiagnosticSource::MathCore,
                         out.diagnostics, policy, false);
    return out;
  }

  Mat4T<T> cofactors{};
  for (int column_index = 0; column_index < 4; ++column_index) {
    for (int row = 0; row < 4; ++row) {
      T minor[9]{};
      int index = 0;
      for (int c = 0; c < 4; ++c) {
        if (c == column_index) {
          continue;
        }
        for (int r = 0; r < 4; ++r) {
          if (r == row) {
            continue;
          }
          minor[index++] = at(matrix, r, c);
        }
      }
      const T sign = ((row + column_index) % 2) == 0 ? static_cast<T>(1) : static_cast<T>(-1);
      setAt(cofactors, row, column_index,
            sign * determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7],
                                  minor[2], minor[5], minor[8]));
    }
  }

  Mat4T<T> adjugate = transpose(cofactors);
  for (T &value : adjugate.m) {
    value /= det;
  }
  return MathResult<Mat4T<T>>::success(adjugate);
}

template <typename T>
[[nodiscard]] inline MathResult<Mat3T<T>>
inverseTranspose(const Mat3T<T> &matrix, const MathPolicy policy = defaultMathPolicy()) {
  const MathResult<Mat3T<T>> inv = inverse(matrix, policy);
  if (!inv) {
    return inv;
  }
  return MathResult<Mat3T<T>>::success(transpose(inv.value));
}

template <typename T> [[nodiscard]] inline Mat3T<T> upperLeftMat3(const Mat4T<T> &matrix) {
  Mat3T<T> out{};
  for (int column_index = 0; column_index < 3; ++column_index) {
    for (int row = 0; row < 3; ++row) {
      setAt(out, row, column_index, at(matrix, row, column_index));
    }
  }
  return out;
}

template <typename T> [[nodiscard]] inline Mat4T<T> mat4FromMat3(const Mat3T<T> &matrix) {
  Mat4T<T> out = identityMatrix<T, 4>();
  for (int column_index = 0; column_index < 3; ++column_index) {
    for (int row = 0; row < 3; ++row) {
      setAt(out, row, column_index, at(matrix, row, column_index));
    }
  }
  return out;
}

template <typename T>
[[nodiscard]] inline MathResult<Mat3T<T>>
orthonormalize(const Mat3T<T> &matrix, const MathPolicy policy = defaultMathPolicy()) {
  const T epsilon = static_cast<T>(policy.absolute_epsilon);
  const MathResult<Vec3T<T>> x_result = safeNormalize(column(matrix, 0), epsilon);
  if (!x_result) {
    return MathResult<Mat3T<T>>::failure(MathError::DegenerateInput,
                                         "Orthonormalization requires a non-zero first axis.");
  }
  const Vec3T<T> x = x_result.value;
  const Vec3T<T> y_seed = column(matrix, 1) - x * dot(column(matrix, 1), x);
  const MathResult<Vec3T<T>> y_result = safeNormalize(y_seed, epsilon);
  if (!y_result) {
    return MathResult<Mat3T<T>>::failure(
        MathError::DegenerateInput, "Orthonormalization requires independent first two axes.");
  }
  const Vec3T<T> y = y_result.value;
  const MathResult<Vec3T<T>> z_result = safeNormalize(cross(x, y), epsilon);
  if (!z_result) {
    return MathResult<Mat3T<T>>::failure(
        MathError::DegenerateInput, "Orthonormalization could not build a stable third axis.");
  }
  return MathResult<Mat3T<T>>::success(mat3FromColumns<T>(x, cross(z_result.value, x),
                                                          z_result.value));
}

template <typename T> struct PolarDecompositionT {
  Mat3T<T> rotation = identityMatrix<T, 3>();
  Mat3T<T> stretch = identityMatrix<T, 3>();
  std::uint32_t iterations = 0u;
};

using PolarDecomposition = PolarDecompositionT<float>;
using DPolarDecomposition = PolarDecompositionT<double>;

template <typename T>
[[nodiscard]] inline MathResult<PolarDecompositionT<T>>
polarDecompose(const Mat3T<T> &matrix, MathPolicy policy = defaultMathPolicy()) {
  if (!allFinite(matrix)) {
    return MathResult<PolarDecompositionT<T>>::failure(
        MathError::NonFiniteInput, "Polar decomposition requires finite inputs.");
  }
  policy.max_iterations = policy.max_iterations == 0u ? 1u : policy.max_iterations;
  Mat3T<T> current = matrix;
  std::uint32_t iteration = 0u;
  for (; iteration < policy.max_iterations; ++iteration) {
    const MathResult<Mat3T<T>> inv_transpose = inverseTranspose(current, policy);
    if (!inv_transpose) {
      return MathResult<PolarDecompositionT<T>>::failure(
          inv_transpose.diagnostics.error,
          inv_transpose.diagnostics.message == nullptr
              ? "Polar decomposition failed while inverting the basis."
              : inv_transpose.diagnostics.message);
    }
    const Mat3T<T> next = (current + inv_transpose.value) * static_cast<T>(0.5);
    if (maxAbsElement(next - current) <= static_cast<T>(policy.relative_epsilon)) {
      current = next;
      ++iteration;
      break;
    }
    current = next;
  }

  const MathResult<Mat3T<T>> rotation = orthonormalize(current, policy);
  if (!rotation) {
    return MathResult<PolarDecompositionT<T>>::failure(rotation.diagnostics.error,
                                                       rotation.diagnostics.message);
  }
  return MathResult<PolarDecompositionT<T>>::success(
      {.rotation = rotation.value, .stretch = transpose(rotation.value) * matrix,
       .iterations = iteration});
}

template <typename T>
[[nodiscard]] inline MathResult<Mat3T<T>>
normalMatrix(const Mat4T<T> &matrix, const MathPolicy policy = defaultMathPolicy()) {
  return inverseTranspose(upperLeftMat3(matrix), policy);
}

} // namespace aster
