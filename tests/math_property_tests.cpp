// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/math/authoring.hpp"
#include "aster/math/collision.hpp"
#include "aster/math/curves.hpp"
#include "aster/math/easing.hpp"
#include "aster/math/gameplay.hpp"
#include "aster/math/packing.hpp"
#include "aster/math/sampling.hpp"
#include "aster/math/transform.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

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

std::array<aster::Vec3, 8> cubePoints(const aster::Vec3 center, const aster::Vec3 half) {
  return {{{center.x - half.x, center.y - half.y, center.z - half.z},
           {center.x + half.x, center.y - half.y, center.z - half.z},
           {center.x - half.x, center.y + half.y, center.z - half.z},
           {center.x + half.x, center.y + half.y, center.z - half.z},
           {center.x - half.x, center.y - half.y, center.z + half.z},
           {center.x + half.x, center.y - half.y, center.z + half.z},
           {center.x - half.x, center.y + half.y, center.z + half.z},
           {center.x + half.x, center.y + half.y, center.z + half.z}}};
}

void testNormalizeContractDiagnostics() {
  aster::setCurrentMathPolicy(aster::defaultMathPolicy());
  aster::clearMathDiagnostics();

  const aster::MathResult<aster::Vec3> zero =
      aster::safeNormalize(aster::Vec3{0.0f, 0.0f, 0.0f});
  assert(!zero);
  assert(zero.diagnostics.error == aster::MathError::DegenerateInput);
  assert(aster::mathDiagnosticCount() == 0u);

  const aster::Vec3 fallback =
      aster::normalizeOr(aster::Vec3{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  expectNearVec3(fallback, {0.0f, 1.0f, 0.0f}, 0.0f);
  assert(aster::mathDiagnosticCount() == 1u);
  assert(aster::mathDiagnosticAt(0u).operation == aster::MathDiagnosticOperation::NormalizeFallback);
  assert(aster::mathDiagnosticAt(0u).fallback_used);

  const aster::Vec3 strict = aster::normalize(aster::Vec3{0.0f, 0.0f, 0.0f});
  expectNearVec3(strict, {0.0f, 0.0f, 0.0f}, 0.0f);
  assert(aster::mathDiagnosticCount() == 2u);
  assert(aster::mathDiagnosticAt(1u).operation == aster::MathDiagnosticOperation::Normalize);

  const aster::MathResult<aster::Vec3> non_finite =
      aster::safeNormalize(
          aster::Vec3{std::numeric_limits<float>::infinity(), 0.0f, 0.0f});
  assert(!non_finite);
  assert(non_finite.diagnostics.error == aster::MathError::NonFiniteInput);
}

void testCurvesEasingAndGameplayHelpers() {
  expectNear(aster::easeInOutCubic(0.5f), 0.5f, 0.00001f);
  expectNear(aster::dampingFactor(8.0f, 0.0f), 0.0f, 0.00001f);
  expectNear(aster::dampingFactor(8.0f, 1.0f), 0.999f, 0.0015f);

  const aster::Vec3 bezier =
      aster::bezierCubic(aster::Vec3{0.0f, 0.0f, 0.0f}, aster::Vec3{0.0f, 2.0f, 0.0f},
                         aster::Vec3{2.0f, 2.0f, 0.0f},
                         aster::Vec3{2.0f, 0.0f, 0.0f}, 0.5f);
  expectNearVec3(bezier, {1.0f, 1.5f, 0.0f}, 0.00001f);

  const auto table = aster::buildArcLengthTable(
      [](const float t) {
        return aster::bezierQuadratic(aster::Vec3{0.0f, 0.0f, 0.0f},
                                      aster::Vec3{1.0f, 1.0f, 0.0f},
                                      aster::Vec3{2.0f, 0.0f, 0.0f}, t);
      },
      16);
  assert(table.size() == 17u);
  assert(table.back().length > 2.0f);
  assert(aster::arcLengthToT(table, table.back().length * 0.5f) > 0.35f);

  const aster::Vec3 projected =
      aster::projectVelocityOnPlane({1.0f, -2.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  expectNearVec3(projected, {1.0f, 0.0f, 0.0f}, 0.00001f);
  const float damage = aster::damageFalloff(12.0f, 0.0f, 30.0f);
  assert(damage > 0.0f && damage < 1.0f);
}

void testHashRngPackingAndSampling() {
  assert(aster::stableHash32(aster::Vec3{1.0f, 2.0f, 3.0f}) == 0x0fc5c16bu);

  aster::Pcg32 rng(1234u, 5678u);
  assert(rng.nextU32() == 0x908c976du);
  assert(rng.nextU32() == 0x5b925303u);
  assert(rng.nextU32() == 0x83c565e2u);

  const aster::Vec3 normal = aster::normalize(aster::Vec3{0.3f, -0.7f, 0.64f});
  const aster::Vec3 unpacked_normal = aster::unpackOctNormal(aster::packOctNormal(normal));
  assert(aster::dot(normal, unpacked_normal) > 0.999f);

  const aster::Quat rotation =
      aster::axisAngle({0.0f, 1.0f, 0.0f}, aster::radians(42.0f));
  const aster::Quat unpacked_quat = aster::unpackUnitQuat48(aster::packUnitQuat48(rotation));
  assert(std::abs(aster::dot(rotation, unpacked_quat)) > 0.9999f);

  for (int i = 0; i < 64; ++i) {
    const aster::Vec2 u{rng.uniform01(), rng.uniform01()};
    assert(aster::length(aster::sampleDiskConcentric(u)) <= 1.0001f);
    expectNear(aster::length(aster::sampleSphere(u)), 1.0f, 0.0001f);
    const aster::Vec3 hemisphere = aster::sampleHemisphereCosine(u);
    assert(hemisphere.z >= -0.0001f);
    assert(aster::length(hemisphere) <= 1.0001f);
  }

  const std::vector<aster::Vec2> poisson =
      aster::poissonDiskSamples({0.0f, 0.0f}, {1.0f, 1.0f}, 0.08f, 96, 77u);
  assert(!poisson.empty());
  for (const aster::Vec2 point : poisson) {
    assert(point.x >= 0.0f && point.x <= 1.0f);
    assert(point.y >= 0.0f && point.y <= 1.0f);
  }
}

void testPredicatesBoundsCollisionAndAuthoring() {
  assert(aster::orient2d({0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}) ==
         aster::PredicateSign::Positive);
  assert(aster::orient3d({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                         {0.0f, 0.0f, 1.0f}) == aster::PredicateSign::Negative);
  assert(aster::incircle({1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {0.0f, 0.0f}) ==
         aster::PredicateSign::Positive);

  const aster::Obb3 lhs =
      aster::obbFromTransform({0.0f, 0.0f, 0.0f}, aster::Quat{}, {1.0f, 1.0f, 1.0f});
  const aster::Obb3 rhs =
      aster::obbFromTransform({1.2f, 0.0f, 0.0f}, aster::Quat{}, {1.0f, 1.0f, 1.0f});
  const aster::Obb3 far =
      aster::obbFromTransform({4.0f, 0.0f, 0.0f}, aster::Quat{}, {1.0f, 1.0f, 1.0f});
  assert(aster::overlaps(lhs, rhs));
  assert(!aster::overlaps(lhs, far));

  const aster::Aabb3 a{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
  const aster::Aabb3 b{{0.5f, -0.5f, -0.5f}, {2.0f, 0.5f, 0.5f}};
  const aster::ContactManifold3 manifold = aster::manifoldAabbAabb(a, b);
  assert(manifold.hit);
  assert(manifold.point_count == 1u);
  assert(aster::epaPenetrationFallback(a, b).hit);

  const std::array<aster::Vec3, 8> cube_a =
      cubePoints({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  const std::array<aster::Vec3, 8> cube_b =
      cubePoints({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  const std::array<aster::Vec3, 8> cube_c =
      cubePoints({5.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  assert(aster::gjkIntersects(cube_a, cube_b));
  assert(!aster::gjkIntersects(cube_a, cube_c));

  const std::array<aster::Vec3, 4> positions{
      {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
  const std::array<std::uint32_t, 12> indices{{0u, 2u, 1u, 0u, 1u, 3u, 1u, 2u, 3u, 2u, 0u, 3u}};
  const aster::MeshMeasure measure = aster::measureTriangleMesh(positions, indices);
  assert(measure.surface_area > 2.0f);
  assert(std::abs(measure.signed_volume) > 0.15f);
  assert(measure.min_triangle_quality > 0.80f);
  expectNearVec3(aster::snapPoint({1.24f, 2.49f, -0.26f}, {{}, {0.5f, 0.5f, 0.25f}, true}),
                 {1.0f, 2.5f, -0.25f}, 0.00001f);
}

void testRandomizedTransformProjectionAndQuaternionProperties() {
  std::mt19937 rng(0xA57E510u);
  std::uniform_real_distribution<float> offset(-6.0f, 6.0f);
  std::uniform_real_distribution<float> scale_value(0.35f, 3.0f);
  std::uniform_real_distribution<float> angle(-aster::pi(), aster::pi());

  std::uint32_t max_ulps = 0u;
  for (int sample = 0; sample < 96; ++sample) {
    const aster::Transform transform =
        aster::Transform::fromEuler({offset(rng), offset(rng), offset(rng)},
                                    {angle(rng), angle(rng), angle(rng)},
                                    {scale_value(rng), scale_value(rng), scale_value(rng)});
    const aster::Mat4 matrix = transform.matrix();
    const aster::MathResult<aster::Mat4> inverse = aster::inverse(matrix);
    assert(inverse);
    const aster::Vec3 point{offset(rng), offset(rng), offset(rng)};
    expectNearVec3(aster::transformPoint(inverse.value, aster::transformPoint(matrix, point)),
                   point, 0.004f);

    const aster::Mat3 linear =
        aster::mat3FromColumns({matrix.m[0], matrix.m[1], matrix.m[2]},
                               {matrix.m[4], matrix.m[5], matrix.m[6]},
                               {matrix.m[8], matrix.m[9], matrix.m[10]});
    const aster::MathResult<aster::PolarDecomposition> polar = aster::polarDecompose(linear);
    assert(polar);
    const aster::Mat3 recomposed = polar.value.rotation * polar.value.stretch;
    for (std::size_t i = 0; i < recomposed.m.size(); ++i) {
      expectNear(recomposed.m[i], linear.m[i], 0.01f);
    }

    const aster::Quat q = aster::axisAngle(aster::normalizeOr(
                                               aster::Vec3{offset(rng), offset(rng), offset(rng)},
                                               {0.0f, 1.0f, 0.0f}),
                                           angle(rng));
    const aster::DualQuat dq = aster::dualQuatFromRotationTranslation(q, transform.position);
    const aster::Vec3 dq_point = aster::transformPoint(dq, point);
    const aster::Vec3 matrix_point = aster::transformPoint(
        aster::translation(transform.position) * aster::mat4FromQuat(q), point);
    expectNearVec3(dq_point, matrix_point, 0.004f);

    const aster::ProjectionPolicy policy =
        (sample % 2) == 0 ? aster::defaultProjectionPolicy()
                          : aster::ProjectionPolicy{aster::CoordinateHandedness::LeftHanded,
                                                    aster::ClipDepthRange::ZeroToOne,
                                                    aster::DepthDirection::ForwardZ};
    const aster::MathResult<aster::Mat4> projection =
        aster::perspective(aster::radians(60.0f), 16.0f / 9.0f, 0.05f, 250.0f, policy);
    const aster::MathResult<aster::Mat4> view =
        aster::lookAt({0.0f, 1.0f, policy.handedness == aster::CoordinateHandedness::LeftHanded
                                      ? -5.0f
                                      : 5.0f},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, policy.handedness);
    assert(projection && view);
    const aster::WorldToClip world_to_clip{projection.value * view.value};
    const aster::MathResult<aster::Mat4> clip_to_world_matrix =
        aster::inverse(world_to_clip.value);
    assert(clip_to_world_matrix);
    const aster::Viewport viewport{{0.0f, 0.0f}, {1920.0f, 1080.0f}};
    const aster::WorldPoint world{{0.25f, -0.15f, 0.0f}};
    const aster::MathResult<aster::ScreenPoint> screen =
        aster::project(world, world_to_clip, viewport);
    assert(screen);
    const aster::MathResult<aster::WorldPoint> restored =
        aster::unproject(screen.value, {clip_to_world_matrix.value}, viewport);
    assert(restored);
    expectNearVec3(restored.value, world.value, 0.003f);
    for (int component = 0; component < 3; ++component) {
      const float actual = restored.value.value[component];
      const float expected = world.value[component];
      for (std::uint32_t ulp = 0u; ulp <= 64u; ++ulp) {
        if (aster::almostEqualUlps(actual, expected, ulp)) {
          max_ulps = std::max(max_ulps, ulp);
          break;
        }
      }
    }
  }

  const std::filesystem::path report_path =
      std::filesystem::temp_directory_path() / "aster_math_property_report.txt";
  std::ofstream report(report_path);
  report << "max_projection_round_trip_ulps=" << max_ulps << '\n';
  report << "seed=0xA57E510\n";
  assert(report.good());
}

} // namespace

int main() {
  testNormalizeContractDiagnostics();
  testCurvesEasingAndGameplayHelpers();
  testHashRngPackingAndSampling();
  testPredicatesBoundsCollisionAndAuthoring();
  testRandomizedTransformProjectionAndQuaternionProperties();
  std::cout << "math_property_tests passed.\n";
  return 0;
}
