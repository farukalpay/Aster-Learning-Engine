// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/math/authoring.hpp"
#include "aster/math/collision.hpp"
#include "aster/math/curves.hpp"
#include "aster/math/easing.hpp"
#include "aster/math/gameplay.hpp"
#include "aster/math/packing.hpp"
#include "aster/math/sampling.hpp"
#include "aster/math/tangent_space.hpp"
#include "aster/math/transform.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace {

template <typename Point, typename Transform>
concept SemanticTransformPointInvocable = requires(Point point, Transform transform) {
  aster::transformPoint(point, transform);
};

template <typename Point, typename Transform, typename Result>
concept SemanticTransformPointReturns = requires(Point point, Transform transform) {
  { aster::transformPoint(point, transform) } -> std::same_as<Result>;
};

static_assert(
    SemanticTransformPointReturns<aster::LocalPoint, aster::LocalToWorld, aster::WorldPoint>);
static_assert(
    SemanticTransformPointReturns<aster::WorldPoint, aster::WorldToView, aster::ViewPoint>);
static_assert(
    SemanticTransformPointReturns<aster::ViewPoint, aster::ViewToClip, aster::ClipPoint>);
static_assert(
    SemanticTransformPointReturns<aster::WorldPoint, aster::WorldToClip, aster::ClipPoint>);
static_assert(
    !SemanticTransformPointInvocable<aster::WorldPoint, aster::LocalToWorld>);
static_assert(
    !SemanticTransformPointInvocable<aster::LocalPoint, aster::WorldToClip>);
static_assert(
    !SemanticTransformPointInvocable<aster::ViewPoint, aster::WorldToClip>);
static_assert(
    !SemanticTransformPointInvocable<aster::ClipPoint, aster::ClipToWorld>);

void expectNear(const float actual, const float expected, const float tolerance) {
  if (std::abs(actual - expected) > tolerance) {
    std::cerr << "expectNear failed: actual=" << actual << " expected=" << expected
              << " tolerance=" << tolerance << '\n';
    assert(false);
  }
}

void expectNearVec3(const aster::Vec3 actual, const aster::Vec3 expected,
                    const float tolerance) {
  expectNear(actual.x, expected.x, tolerance);
  expectNear(actual.y, expected.y, tolerance);
  expectNear(actual.z, expected.z, tolerance);
}

float expectedNearDepth(const aster::ProjectionPolicy policy) {
  return policy.depth_direction == aster::DepthDirection::ReverseZ
             ? 1.0f
             : (policy.depth_range == aster::ClipDepthRange::NegativeOneToOne ? -1.0f
                                                                               : 0.0f);
}

float expectedFarDepth(const aster::ProjectionPolicy policy) {
  return policy.depth_direction == aster::DepthDirection::ReverseZ
             ? (policy.depth_range == aster::ClipDepthRange::NegativeOneToOne ? -1.0f : 0.0f)
             : 1.0f;
}

aster::Vec4 shaderColumnMajorMul(const aster::Mat4 &matrix, const aster::Vec4 value) {
  return {matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z +
              matrix.m[12] * value.w,
          matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z +
              matrix.m[13] * value.w,
          matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z +
              matrix.m[14] * value.w,
          matrix.m[3] * value.x + matrix.m[7] * value.y + matrix.m[11] * value.z +
              matrix.m[15] * value.w};
}

aster::Vec4 shaderRowMajorMisreadMul(const aster::Mat4 &matrix, const aster::Vec4 value) {
  return {matrix.m[0] * value.x + matrix.m[1] * value.y + matrix.m[2] * value.z +
              matrix.m[3] * value.w,
          matrix.m[4] * value.x + matrix.m[5] * value.y + matrix.m[6] * value.z +
              matrix.m[7] * value.w,
          matrix.m[8] * value.x + matrix.m[9] * value.y + matrix.m[10] * value.z +
              matrix.m[11] * value.w,
          matrix.m[12] * value.x + matrix.m[13] * value.y + matrix.m[14] * value.z +
              matrix.m[15] * value.w};
}

aster::MathResult<aster::ScreenPoint> screenFromShaderClip(const aster::Vec4 clip,
                                                           const aster::Viewport viewport) {
  return aster::ndcToScreen(aster::divideByW(aster::ClipPoint{clip}), viewport);
}

aster::Vec3 shaderNormalMapToWorld(const aster::Vec3 normal, const aster::Vec4 tangent,
                                   const aster::Vec3 encoded_rgb,
                                   const float normal_y_sign) {
  aster::Vec3 encoded = encoded_rgb * 2.0f - aster::Vec3{1.0f, 1.0f, 1.0f};
  encoded.y *= normal_y_sign < 0.0f ? -1.0f : 1.0f;
  const aster::Vec3 n = aster::normalize(normal);
  aster::Vec3 t{tangent.x, tangent.y, tangent.z};
  t = t - n * aster::dot(n, t);
  if (aster::length(t) < 0.001f) {
    return n;
  }
  t = aster::normalize(t);
  const float handedness = tangent.w < 0.0f ? -1.0f : 1.0f;
  const aster::Vec3 b = aster::normalize(aster::cross(n, t)) * handedness;
  return aster::normalize(t * encoded.x + b * encoded.y +
                          n * std::max(encoded.z, 0.001f));
}

double forwardZoDistanceForDepth(const float depth, const double near_plane,
                                 const double far_plane) {
  const double a = far_plane / (far_plane - near_plane);
  const double b = (far_plane * near_plane) / (far_plane - near_plane);
  return b / (a - static_cast<double>(depth));
}

double reverseZoDistanceForDepth(const float depth, const double near_plane,
                                 const double far_plane) {
  const double c = near_plane / (far_plane - near_plane);
  const double b = (far_plane * near_plane) / (far_plane - near_plane);
  return b / (static_cast<double>(depth) + c);
}

void expectDepthEndpoint(const aster::Mat4 &projection, const aster::ProjectionPolicy policy,
                         const float distance, const float expected_depth) {
  const float view_z =
      policy.handedness == aster::CoordinateHandedness::RightHanded ? -distance : distance;
  const aster::Vec4 clip = projection * aster::Vec4{0.0f, 0.0f, view_z, 1.0f};
  assert(std::abs(clip.w) > 0.000001f);
  expectNear(clip.z / clip.w, expected_depth, 0.0005f);
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
        aster::unproject(screen.value, aster::ClipToWorld{clip_to_world_matrix.value}, viewport);
    assert(restored);
    expectNearVec3(restored.value.value, world.value, 0.003f);
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

void testProjectionConventionMatrix() {
  constexpr float near_plane = 0.05f;
  constexpr float far_plane = 80.0f;
  const aster::Viewport base_viewport{{13.0f, 17.0f}, {800.0f, 600.0f}};
  for (const aster::CoordinateHandedness handedness :
       {aster::CoordinateHandedness::RightHanded, aster::CoordinateHandedness::LeftHanded}) {
    for (const aster::ClipDepthRange depth_range :
         {aster::ClipDepthRange::ZeroToOne, aster::ClipDepthRange::NegativeOneToOne}) {
      for (const aster::DepthDirection depth_direction :
           {aster::DepthDirection::ForwardZ, aster::DepthDirection::ReverseZ}) {
        for (const aster::ViewportOrigin viewport_origin :
             {aster::ViewportOrigin::TopLeft, aster::ViewportOrigin::BottomLeft}) {
          for (const bool perspective_mode : {true, false}) {
            aster::ProjectionPolicy policy{.handedness = handedness,
                                           .depth_range = depth_range,
                                           .depth_direction = depth_direction,
                                           .viewport_origin = viewport_origin};
            const aster::MathResult<aster::Mat4> projection =
                perspective_mode
                    ? aster::perspective(aster::radians(58.0f), 4.0f / 3.0f, near_plane,
                                         far_plane, policy)
                    : aster::orthographic(-4.0f, 4.0f, -3.0f, 3.0f, near_plane,
                                          far_plane, policy);
            assert(projection);
            expectDepthEndpoint(projection.value, policy, near_plane, expectedNearDepth(policy));
            expectDepthEndpoint(projection.value, policy, far_plane, expectedFarDepth(policy));

            const aster::Vec3 eye =
                handedness == aster::CoordinateHandedness::RightHanded
                    ? aster::Vec3{0.0f, 1.0f, 6.0f}
                    : aster::Vec3{0.0f, 1.0f, -6.0f};
            const aster::Vec3 target{0.0f, 0.2f, 0.0f};
            const aster::MathResult<aster::Mat4> view =
                aster::lookAt(eye, target, {0.0f, 1.0f, 0.0f}, handedness);
            assert(view);
            const aster::ClipFromWorld world_to_clip =
                aster::ClipFromView{projection.value} * aster::ViewFromWorld{view.value};
            const aster::MathResult<aster::WorldFromClip> clip_to_world =
                aster::inverse(world_to_clip);
            assert(clip_to_world);
            aster::Viewport viewport = base_viewport;
            viewport.origin_convention = viewport_origin;
            const aster::WorldPoint world{target + aster::Vec3{0.17f, -0.11f, 0.0f}};
            const aster::MathResult<aster::ScreenPoint> screen =
                aster::project(world, world_to_clip, viewport);
            assert(screen);
            const aster::MathResult<aster::WorldPoint> restored =
                aster::unproject(screen.value, clip_to_world.value, viewport);
            assert(restored);
            expectNearVec3(restored.value.value, world.value, 0.005f);

            const aster::ScreenPoint center{
                viewport.origin.x + viewport.size.x * 0.5f,
                viewport.origin.y + viewport.size.y * 0.5f,
                expectedNearDepth(policy)};
            const aster::MathResult<aster::WorldRay> ray =
                aster::screenRay(center, clip_to_world.value, viewport, policy,
                                 aster::RayOriginPolicy::PerspectiveEye,
                                 aster::WorldPoint{eye});
            assert(ray);
            const aster::Vec3 expected_direction = aster::normalize(target - eye);
            assert(aster::dot(ray.value.direction.value, expected_direction) >= 0.999f);
          }
        }
      }
    }
  }
}

void testProjectionPolicyGoldenMatrices() {
  constexpr float kVerticalFov = 1.047197580f;
  constexpr float kAspect = 16.0f / 9.0f;
  constexpr float kNear = 0.125f;
  constexpr float kFar = 2048.0f;
  constexpr float kExpectedX = 0.9742785793f;
  constexpr float kExpectedY = 1.732050808f;
  struct GoldenProjection {
    aster::ProjectionPolicy policy;
    float m10 = 0.0f;
    float m11 = 0.0f;
    float m14 = 0.0f;
  };

  const GoldenProjection goldens[] = {
      {{aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::ZeroToOne,
        aster::DepthDirection::ForwardZ},
       -1.000061039f, -1.0f, -0.1250076299f},
      {{aster::CoordinateHandedness::RightHanded, aster::ClipDepthRange::ZeroToOne,
        aster::DepthDirection::ReverseZ},
       0.00006103888177f, -1.0f, 0.1250076299f},
      {{aster::CoordinateHandedness::RightHanded,
        aster::ClipDepthRange::NegativeOneToOne, aster::DepthDirection::ForwardZ},
       -1.000122078f, -1.0f, -0.2500152597f},
      {{aster::CoordinateHandedness::RightHanded,
        aster::ClipDepthRange::NegativeOneToOne, aster::DepthDirection::ReverseZ},
       1.000122078f, -1.0f, 0.2500152597f},
      {{aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::ZeroToOne,
        aster::DepthDirection::ForwardZ},
       1.000061039f, 1.0f, -0.1250076299f},
      {{aster::CoordinateHandedness::LeftHanded, aster::ClipDepthRange::ZeroToOne,
        aster::DepthDirection::ReverseZ},
       -0.00006103888177f, 1.0f, 0.1250076299f},
      {{aster::CoordinateHandedness::LeftHanded,
        aster::ClipDepthRange::NegativeOneToOne, aster::DepthDirection::ForwardZ},
       1.000122078f, 1.0f, -0.2500152597f},
      {{aster::CoordinateHandedness::LeftHanded,
        aster::ClipDepthRange::NegativeOneToOne, aster::DepthDirection::ReverseZ},
       -1.000122078f, 1.0f, 0.2500152597f},
  };

  for (const GoldenProjection &golden : goldens) {
    const aster::MathResult<aster::Mat4> projection =
        aster::perspective(kVerticalFov, kAspect, kNear, kFar, golden.policy);
    assert(projection);
    expectNear(projection.value.m[0], kExpectedX, 0.000001f);
    expectNear(projection.value.m[5], kExpectedY, 0.000001f);
    expectNear(projection.value.m[10], golden.m10, 0.000001f);
    expectNear(projection.value.m[11], golden.m11, 0.0f);
    expectNear(projection.value.m[14], golden.m14, 0.000001f);
    expectNear(projection.value.m[15], 0.0f, 0.0f);
    for (const std::size_t index : {1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u, 12u, 13u}) {
      expectNear(projection.value.m[index], 0.0f, 0.0f);
    }
  }
}

void testReverseZDepthPrecisionContract() {
  constexpr double kNear = 0.05;
  constexpr double kFar = 100000.0;

  const float forward_far_prev = std::nextafter(1.0f, 0.0f);
  const float reverse_far_next = std::nextafter(0.0f, 1.0f);
  const double forward_far_step =
      kFar - forwardZoDistanceForDepth(forward_far_prev, kNear, kFar);
  const double reverse_far_step =
      kFar - reverseZoDistanceForDepth(reverse_far_next, kNear, kFar);
  assert(forward_far_step > 1000.0);
  assert(reverse_far_step < forward_far_step * 0.000001);

  const aster::ProjectionPolicy forward_policy{aster::CoordinateHandedness::RightHanded,
                                               aster::ClipDepthRange::ZeroToOne,
                                               aster::DepthDirection::ForwardZ};
  const aster::ProjectionPolicy reverse_policy{aster::CoordinateHandedness::RightHanded,
                                               aster::ClipDepthRange::ZeroToOne,
                                               aster::DepthDirection::ReverseZ};
  const aster::MathResult<aster::Mat4> forward =
      aster::perspective(aster::radians(60.0f), 1.0f, static_cast<float>(kNear),
                         static_cast<float>(kFar), forward_policy);
  const aster::MathResult<aster::Mat4> reverse =
      aster::perspective(aster::radians(60.0f), 1.0f, static_cast<float>(kNear),
                         static_cast<float>(kFar), reverse_policy);
  assert(forward && reverse);
  expectDepthEndpoint(forward.value, forward_policy, static_cast<float>(kNear), 0.0f);
  expectDepthEndpoint(forward.value, forward_policy, static_cast<float>(kFar), 1.0f);
  expectDepthEndpoint(reverse.value, reverse_policy, static_cast<float>(kNear), 1.0f);
  expectDepthEndpoint(reverse.value, reverse_policy, static_cast<float>(kFar), 0.0f);
}

void testViewportOriginGoldenContract() {
  const aster::Viewport top_left{{10.0f, 20.0f}, {800.0f, 600.0f},
                                 aster::ViewportOrigin::TopLeft};
  const aster::Viewport bottom_left{{10.0f, 20.0f}, {800.0f, 600.0f},
                                    aster::ViewportOrigin::BottomLeft};
  const aster::MathResult<aster::ScreenPoint> top_left_corner =
      aster::ndcToScreen(aster::NdcPoint{-1.0f, 1.0f, 0.25f}, top_left);
  const aster::MathResult<aster::ScreenPoint> bottom_left_same_ndc =
      aster::ndcToScreen(aster::NdcPoint{-1.0f, 1.0f, 0.25f}, bottom_left);
  assert(top_left_corner && bottom_left_same_ndc);
  expectNearVec3(top_left_corner.value.value, {10.0f, 20.0f, 0.25f}, 0.0f);
  expectNearVec3(bottom_left_same_ndc.value.value, {10.0f, 620.0f, 0.25f}, 0.0f);

  const aster::MathResult<aster::NdcPoint> restored_top =
      aster::screenToNdc(top_left_corner.value, top_left);
  const aster::MathResult<aster::NdcPoint> restored_bottom =
      aster::screenToNdc(bottom_left_same_ndc.value, bottom_left);
  assert(restored_top && restored_bottom);
  expectNearVec3(restored_top.value.value, {-1.0f, 1.0f, 0.25f}, 0.0f);
  expectNearVec3(restored_bottom.value.value, {-1.0f, 1.0f, 0.25f}, 0.0f);
}

void testCpuGpuWorldToScreenColumnMajorParity() {
  const aster::ProjectionPolicy policy = aster::defaultProjectionPolicy();
  const aster::Mat4 model = aster::translation({1.2f, -0.35f, 0.8f}) *
                            aster::rotation_y(aster::radians(23.0f)) *
                            aster::scale({0.75f, 1.35f, 1.1f});
  const aster::MathResult<aster::Mat4> view =
      aster::lookAt({1.5f, 2.0f, 6.0f}, {0.1f, -0.15f, 0.2f}, {0.0f, 1.0f, 0.0f},
                    policy.handedness);
  const aster::MathResult<aster::Mat4> projection =
      aster::perspective(aster::radians(64.0f), 16.0f / 9.0f, 0.05f, 500.0f, policy);
  assert(view && projection);

  const aster::Mat4 view_projection = projection.value * view.value;
  const aster::Mat4 mvp = view_projection * model;
  const aster::Vec4 local{0.31f, -0.27f, 0.42f, 1.0f};
  const aster::Vec4 shader_clip = shaderColumnMajorMul(mvp, local);
  const aster::MathResult<aster::ScreenPoint> shader_screen =
      screenFromShaderClip(shader_clip, {{3.0f, 5.0f}, {1280.0f, 720.0f}});
  assert(shader_screen);

  const aster::Vec4 world4 = shaderColumnMajorMul(model, local);
  assert(std::abs(world4.w) > 0.000001f);
  const aster::WorldPoint world{{world4.x / world4.w, world4.y / world4.w, world4.z / world4.w}};
  const aster::MathResult<aster::ScreenPoint> cpu_screen =
      aster::project(world, aster::WorldToClip{view_projection},
                     {{3.0f, 5.0f}, {1280.0f, 720.0f}});
  assert(cpu_screen);
  expectNearVec3(shader_screen.value.value, cpu_screen.value.value, 0.001f);

  const aster::MathResult<aster::ScreenPoint> row_major_misread =
      screenFromShaderClip(shaderRowMajorMisreadMul(mvp, local),
                           {{3.0f, 5.0f}, {1280.0f, 720.0f}});
  assert(row_major_misread);
  const float row_major_drift =
      std::abs(row_major_misread.value.x - shader_screen.value.x) +
      std::abs(row_major_misread.value.y - shader_screen.value.y);
  assert(row_major_drift > 100.0f);
}

void testNormalMapTangentSpaceContract() {
  const aster::Vec3 normal{0.0f, 0.0f, 1.0f};
  const aster::Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  expectNearVec3(shaderNormalMapToWorld(normal, tangent, {0.5f, 0.5f, 1.0f}, 1.0f),
                 normal, 0.0001f);
  expectNearVec3(shaderNormalMapToWorld(normal, tangent, {1.0f, 0.5f, 0.5f}, 1.0f),
                 {0.9999995f, 0.0f, 0.0010f}, 0.0001f);
  expectNearVec3(shaderNormalMapToWorld(normal, tangent, {0.5f, 1.0f, 0.5f}, 1.0f),
                 {0.0f, 0.9999995f, 0.0010f}, 0.0001f);
  expectNearVec3(shaderNormalMapToWorld(normal, tangent, {0.5f, 1.0f, 0.5f}, -1.0f),
                 {0.0f, -0.9999995f, 0.0010f}, 0.0001f);
  expectNearVec3(
      shaderNormalMapToWorld(normal, {1.0f, 0.0f, 0.0f, -1.0f}, {0.5f, 1.0f, 0.5f}, 1.0f),
      {0.0f, -0.9999995f, 0.0010f}, 0.0001f);
}

void testProjectionAndTangentFailureProperties() {
  const aster::Viewport viewport{{0.0f, 0.0f}, {640.0f, 480.0f}};
  aster::Mat4 non_finite = aster::identity();
  non_finite.m[0] = std::numeric_limits<float>::infinity();
  const aster::MathResult<aster::ScreenPoint> bad_project =
      aster::project(aster::WorldPoint{0.0f, 0.0f, 0.0f},
                     aster::WorldToClip{non_finite}, viewport);
  assert(!bad_project);
  assert(bad_project.diagnostics.error == aster::MathError::NonFiniteInput);

  const aster::MathResult<aster::WorldPoint> bad_unproject =
      aster::unproject(aster::ScreenPoint{std::numeric_limits<float>::quiet_NaN(), 0.0f,
                                          0.0f},
                       aster::ClipToWorld{aster::identity()}, viewport);
  assert(!bad_unproject);
  assert(bad_unproject.diagnostics.error == aster::MathError::NonFiniteInput);

  const aster::MathResult<aster::WorldNormalFromLocal> singular_normal =
      aster::worldNormalFromLocal(aster::WorldFromLocal{aster::scale({1.0f, 0.0f, 1.0f})});
  assert(!singular_normal);
  assert(singular_normal.diagnostics.error == aster::MathError::SingularMatrix);

  const aster::MathResult<aster::WorldNormalFromLocal> non_uniform_normal =
      aster::worldNormalFromLocal(aster::WorldFromLocal{aster::scale({2.0f, 4.0f, 0.5f})});
  assert(non_uniform_normal);
  const aster::Normal transformed_y =
      aster::transformNormal(aster::Normal{0.0f, 1.0f, 0.0f}, non_uniform_normal.value);
  expectNear(aster::length(transformed_y.value), 1.0f, 0.0001f);
  assert(aster::dot(transformed_y.value, {0.0f, 1.0f, 0.0f}) >= 0.999f);

  const aster::MathResult<aster::WorldNormalFromLocal> negative_normal =
      aster::worldNormalFromLocal(aster::WorldFromLocal{aster::scale({-1.0f, 2.0f, 3.0f})});
  assert(negative_normal);
  const aster::Normal transformed_x =
      aster::transformNormal(aster::Normal{1.0f, 0.0f, 0.0f}, negative_normal.value);
  assert(aster::dot(transformed_x.value, {-1.0f, 0.0f, 0.0f}) >= 0.999f);

  const aster::TangentFrame frame =
      aster::tangentFrameFromTriangle({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                      {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f},
                                      {1.0f, 0.0f}, {0.0f, 1.0f});
  const aster::TangentFrame mirrored =
      aster::tangentFrameFromTriangle({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                      {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f},
                                      {-1.0f, 0.0f}, {0.0f, 1.0f});
  assert(frame.handedness == 1.0f);
  assert(mirrored.handedness == -1.0f);
  const aster::MathResult<aster::TangentFrame> negative_scaled =
      aster::transformTangentFrame(frame, aster::WorldFromLocal{aster::scale({-1.0f, 1.0f, 1.0f})});
  assert(negative_scaled);
  assert(negative_scaled.value.handedness == -1.0f);
  const aster::MathResult<aster::TangentFrame> mirrored_negative_scaled =
      aster::transformTangentFrame(mirrored,
                                   aster::WorldFromLocal{aster::scale({-1.0f, 1.0f, 1.0f})});
  assert(mirrored_negative_scaled);
  assert(mirrored_negative_scaled.value.handedness == 1.0f);
}

} // namespace

int main() {
  testNormalizeContractDiagnostics();
  testCurvesEasingAndGameplayHelpers();
  testHashRngPackingAndSampling();
  testPredicatesBoundsCollisionAndAuthoring();
  testRandomizedTransformProjectionAndQuaternionProperties();
  testProjectionConventionMatrix();
  testProjectionPolicyGoldenMatrices();
  testReverseZDepthPrecisionContract();
  testViewportOriginGoldenContract();
  testCpuGpuWorldToScreenColumnMajorParity();
  testNormalMapTangentSpaceContract();
  testProjectionAndTangentFailureProperties();
  std::cout << "math_property_tests passed.\n";
  return 0;
}
