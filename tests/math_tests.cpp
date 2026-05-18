// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/math/geometry.hpp"
#include "aster/math/quat.hpp"
#include "aster/math/transform.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

namespace {

void expectNear(const float actual, const float expected, const float tolerance) {
  assert(std::abs(actual - expected) <= tolerance);
}

void expectNearVec3(const aster::Vec3 actual, const aster::Vec3 expected,
                    const float tolerance) {
  expectNear(actual.x, expected.x, tolerance);
  expectNear(actual.y, expected.y, tolerance);
  expectNear(actual.z, expected.z, tolerance);
}

void testScalarAndNormalizePolicy() {
  assert(aster::nearlyEqual(1.0f, 1.0f + 0.0000001f));
  assert(aster::almostEqualUlps(1.0f, std::nextafter(1.0f, 2.0f), 1u));
  assert(aster::isFiniteScalar(4.0f));
  assert(!aster::isFiniteScalar(std::numeric_limits<float>::infinity()));
  assert(aster::allFinite(aster::Vec3{1.0f, 2.0f, 3.0f}));

  const aster::MathResult<aster::Vec3> zero =
      aster::safeNormalize(aster::Vec3{0.0f, 0.0f, 0.0f});
  assert(!zero);
  assert(zero.diagnostics.error == aster::MathError::DegenerateInput);

  const aster::MathResult<aster::Vec3> normalized =
      aster::safeNormalize(aster::Vec3{3.0f, 0.0f, 4.0f});
  assert(normalized);
  expectNearVec3(normalized.value, {0.6f, 0.0f, 0.8f}, 0.00001f);
}

void testMatrixFamilyAndDiagnostics() {
  const aster::Mat2 mat2 = aster::mat2FromColumns(aster::Vec2{2.0f, 0.0f},
                                                  aster::Vec2{0.0f, 4.0f});
  const aster::MathResult<aster::Mat2> inv2 = aster::inverse(mat2);
  assert(inv2);
  expectNear(inv2.value.m[0], 0.5f, 0.00001f);
  expectNear(inv2.value.m[3], 0.25f, 0.00001f);

  const aster::Mat4 transform = aster::translation({1.0f, -2.0f, 3.0f}) *
                                aster::rotation_y(aster::radians(30.0f)) *
                                aster::scale({2.0f, 3.0f, 4.0f});
  const aster::MathResult<aster::Mat4> inverse = aster::inverse(transform);
  assert(inverse);
  const aster::Mat4 round_trip = transform * inverse.value;
  const aster::Mat4 identity = aster::identity();
  for (std::size_t index = 0; index < round_trip.m.size(); ++index) {
    expectNear(round_trip.m[index], identity.m[index], 0.001f);
  }

  const aster::Mat4 singular = aster::scale({1.0f, 0.0f, 1.0f});
  const aster::MathResult<aster::Mat4> singular_inverse = aster::inverse(singular);
  assert(!singular_inverse);
  assert(singular_inverse.diagnostics.error == aster::MathError::SingularMatrix);

  aster::Mat4 non_finite = aster::identity();
  non_finite.m[0] = std::numeric_limits<float>::infinity();
  const aster::MathResult<aster::Mat4> non_finite_inverse = aster::inverse(non_finite);
  assert(!non_finite_inverse);
  assert(non_finite_inverse.diagnostics.error == aster::MathError::NonFiniteInput);

  const aster::Mat3 skewed =
      aster::mat3FromColumns({1.0f, 0.1f, 0.0f}, {0.2f, 1.0f, 0.1f}, {0.0f, 0.2f, 1.0f});
  const aster::MathResult<aster::Mat3> orthonormal = aster::orthonormalize(skewed);
  assert(orthonormal);
  expectNear(aster::length(aster::column(orthonormal.value, 0)), 1.0f, 0.0001f);
  expectNear(aster::length(aster::column(orthonormal.value, 1)), 1.0f, 0.0001f);
  expectNear(aster::length(aster::column(orthonormal.value, 2)), 1.0f, 0.0001f);
  expectNear(aster::dot(aster::column(orthonormal.value, 0), aster::column(orthonormal.value, 1)),
             0.0f, 0.0001f);

  const aster::MathResult<aster::PolarDecomposition> polar = aster::polarDecompose(skewed);
  assert(polar);
  const aster::Mat3 recomposed = polar.value.rotation * polar.value.stretch;
  for (std::size_t index = 0; index < recomposed.m.size(); ++index) {
    expectNear(recomposed.m[index], skewed.m[index], 0.002f);
  }
}

void testProjectionRoundTrip() {
  const aster::MathResult<aster::Mat4> projection =
      aster::perspective(aster::radians(70.0f), 16.0f / 9.0f, 0.05f, 200.0f);
  assert(projection);
  const aster::MathResult<aster::Mat4> view =
      aster::lookAt({0.0f, 1.0f, 4.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  assert(view);
  const aster::Mat4 world_to_clip = projection.value * view.value;
  const aster::MathResult<aster::Mat4> clip_to_world = aster::inverse(world_to_clip);
  assert(clip_to_world);

  const aster::Vec3 world{0.25f, -0.1f, -0.4f};
  const aster::MathResult<aster::Vec3> window =
      aster::project(world, world_to_clip, {0.0f, 0.0f}, {1280.0f, 720.0f});
  assert(window);
  const aster::MathResult<aster::Vec3> restored =
      aster::unproject(window.value, clip_to_world.value, {0.0f, 0.0f}, {1280.0f, 720.0f});
  assert(restored);
  expectNearVec3(restored.value, world, 0.002f);
}

void testQuaternionAndDualQuaternion() {
  const aster::MathResult<aster::Quat> yaw =
      aster::axisAngleSafe(aster::Vec3{0.0f, 1.0f, 0.0f}, aster::radians(90.0f));
  assert(yaw);
  const aster::Vec3 rotated = aster::rotate(yaw.value, {0.0f, 0.0f, 1.0f});
  expectNearVec3(rotated, {1.0f, 0.0f, 0.0f}, 0.0001f);

  const aster::Quat half = aster::pow(yaw.value, 0.5f);
  expectNear(aster::length(aster::rotate(half, {0.0f, 0.0f, 1.0f})), 1.0f, 0.0001f);

  const aster::Quat logged_exp = aster::exp(aster::log(yaw.value));
  expectNear(std::abs(aster::dot(aster::normalize(logged_exp), yaw.value)), 1.0f, 0.0001f);

  const aster::DualQuat dq =
      aster::dualQuatFromRotationTranslation(yaw.value, {1.0f, 0.0f, 0.0f});
  const aster::Vec3 transformed = aster::transformPoint(dq, {0.0f, 0.0f, 1.0f});
  expectNearVec3(transformed, {2.0f, 0.0f, 0.0f}, 0.0001f);
}

void testGeometryQueries() {
  const aster::RayHit3 triangle = aster::intersectRayTriangle(
      {0.25f, 0.25f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  assert(triangle.hit);
  expectNear(triangle.distance, 1.0f, 0.0001f);
  expectNear(triangle.barycentric.u + triangle.barycentric.v + triangle.barycentric.w, 1.0f,
             0.0001f);

  const aster::RayHit3 sphere =
      aster::intersectRaySphere({0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f},
                                {{0.0f, 0.0f, 0.0f}, 1.0f});
  assert(sphere.hit);
  expectNear(sphere.distance, 2.0f, 0.0001f);
  expectNearVec3(sphere.normal, {0.0f, 0.0f, 1.0f}, 0.0001f);

  const aster::Vec3 closest =
      aster::closestPointOnTriangle({0.25f, 0.25f, 2.0f}, {0.0f, 0.0f, 0.0f},
                                    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  expectNearVec3(closest, {0.25f, 0.25f, 0.0f}, 0.0001f);
}

void testDeterministicRandomizedProperties() {
  std::mt19937 rng(0xA57E3u);
  std::uniform_real_distribution<float> offset(-5.0f, 5.0f);
  std::uniform_real_distribution<float> scale_value(0.25f, 4.0f);
  std::uniform_real_distribution<float> angle(-aster::pi(), aster::pi());

  for (int sample = 0; sample < 64; ++sample) {
    const aster::Mat4 transform =
        aster::translation({offset(rng), offset(rng), offset(rng)}) *
        aster::rotation_y(angle(rng)) *
        aster::scale({scale_value(rng), scale_value(rng), scale_value(rng)});
    const aster::MathResult<aster::Mat4> inverse = aster::inverse(transform);
    assert(inverse);
    const aster::Mat4 round_trip = transform * inverse.value;
    const aster::Mat4 identity = aster::identity();
    for (std::size_t index = 0; index < round_trip.m.size(); ++index) {
      expectNear(round_trip.m[index], identity.m[index], 0.01f);
    }

    const aster::Quat q = aster::axisAngle({0.0f, 1.0f, 0.0f}, angle(rng));
    const aster::Vec3 v{offset(rng), offset(rng), offset(rng)};
    expectNear(aster::length(aster::rotate(q, v)), aster::length(v), 0.001f);
  }
}

void testSemanticWrapperTypes() {
  static_assert(!std::is_same_v<aster::WorldPoint, aster::LocalPoint>);
  static_assert(!std::is_same_v<aster::Direction, aster::Normal>);
  const aster::WorldPoint world{{1.0f, 2.0f, 3.0f}};
  const aster::Direction direction{{0.0f, 0.0f, -1.0f}};
  assert(world.value.x == 1.0f);
  assert(direction.value.z == -1.0f);
}

} // namespace

int main() {
  testScalarAndNormalizePolicy();
  testMatrixFamilyAndDiagnostics();
  testProjectionRoundTrip();
  testQuaternionAndDualQuaternion();
  testGeometryQueries();
  testDeterministicRandomizedProperties();
  testSemanticWrapperTypes();
  std::cout << "math_tests passed.\n";
  return 0;
}
