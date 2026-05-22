// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "test_support.hpp"

#include "aster/core/belief_extraction.hpp"
#include "aster/core/job_graph.hpp"
#include "aster/core/module_registry.hpp"
#include "aster/core/neural_irradiance_volume.hpp"
#include "aster/core/perceptual_world_runtime.hpp"
#include "aster/core/session_journal.hpp"
#include "aster/core/signal.hpp"
#include "aster/core/world_perception_ledger.hpp"
#include "aster/core/world_state.hpp"

#include <algorithm>
#include <atomic>
#include <type_traits>

namespace {

void testVectorMath() {
  const aster::Vec3 x{1.0f, 0.0f, 0.0f};
  const aster::Vec3 y{0.0f, 1.0f, 0.0f};
  const aster::Vec3 z = aster::cross(x, y);
  expectNear(z.x, 0.0f, 0.0001f);
  expectNear(z.y, 0.0f, 0.0001f);
  expectNear(z.z, 1.0f, 0.0001f);
  expectNear(aster::length(aster::normalize({3.0f, 4.0f, 0.0f})), 1.0f, 0.0001f);
}

void testVectorAliasesAndShaderHelpers() {
  const aster::DVec3 precise{1.0, 2.0, 3.0};
  const aster::IVec2 tile{4, -2};
  const aster::BVec4 mask{true, false, true, false};
  assert(precise.z == 3.0);
  assert(tile.x == 4);
  assert(mask.x && !mask.y);

  const aster::Vec3 mixed =
      aster::mix(aster::Vec3{0.0f, 2.0f, 4.0f}, aster::Vec3{2.0f, 4.0f, 8.0f}, 0.5f);
  expectNear(mixed.x, 1.0f, 0.0001f);
  expectNear(mixed.y, 3.0f, 0.0001f);
  expectNear(mixed.z, 6.0f, 0.0001f);

  const aster::Vec3 saturated = aster::saturate(aster::Vec3{-1.0f, 0.45f, 2.0f});
  expectNear(saturated.x, 0.0f, 0.0001f);
  expectNear(saturated.y, 0.45f, 0.0001f);
  expectNear(saturated.z, 1.0f, 0.0001f);
  expectNear(aster::smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 0.0001f);
  expectNear(aster::fract(1.75f), 0.75f, 0.0001f);
  assert(aster::step(0.5f, 0.49f) == 0.0f);
  assert(aster::step(0.5f, 0.50f) == 1.0f);

  const aster::Vec3 reflected =
      aster::reflect(aster::Vec3{1.0f, -1.0f, 0.0f}, aster::Vec3{0.0f, 1.0f, 0.0f});
  expectNear(reflected.x, 1.0f, 0.0001f);
  expectNear(reflected.y, 1.0f, 0.0001f);
  expectNear(aster::distance(aster::Vec3{1.0f, 2.0f, 3.0f}, aster::Vec3{1.0f, 2.0f, 5.0f}),
             2.0f, 0.0001f);
}

void testSemanticMathBoundaries() {
  static_assert(!std::is_convertible_v<aster::WorldPoint, aster::Vec3>);
  static_assert(!std::is_convertible_v<aster::ClipPoint, aster::Vec4>);
  static_assert(!std::is_convertible_v<aster::WorldToClip, aster::Mat4>);
  static_assert(!std::is_convertible_v<aster::WorldFromLocal, aster::Mat4>);
  static_assert(std::is_constructible_v<aster::WorldPoint, aster::Vec3>);
  static_assert(std::is_constructible_v<aster::WorldToClip, aster::Mat4>);
}

void testMatrixComposition() {
  const aster::Mat4 transform =
      aster::translation({2.0f, 3.0f, 4.0f}) * aster::scale({2.0f, 2.0f, 2.0f});
  expectNear(transform.m[0], 2.0f, 0.0001f);
  expectNear(transform.m[5], 2.0f, 0.0001f);
  expectNear(transform.m[10], 2.0f, 0.0001f);
  expectNear(transform.m[12], 2.0f, 0.0001f);
  expectNear(transform.m[13], 3.0f, 0.0001f);
  expectNear(transform.m[14], 4.0f, 0.0001f);
}

void testMat3AndNormalMatrix() {
  const aster::Mat3 basis =
      aster::mat3FromColumns({2.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 4.0f});
  expectNear(aster::determinant(basis), 24.0f, 0.0001f);

  const aster::MathResult<aster::Mat3> inv_basis = aster::inverse(basis);
  assert(inv_basis);
  const aster::Mat3 round_trip = basis * inv_basis.value;
  const aster::Mat3 identity = aster::identity3();
  for (std::size_t i = 0; i < round_trip.m.size(); ++i) {
    expectNear(round_trip.m[i], identity.m[i], 0.0001f);
  }

  const aster::MathResult<aster::Mat3> normal_matrix_result =
      aster::normalMatrix(aster::scale({2.0f, 4.0f, 0.5f}));
  assert(normal_matrix_result);
  const aster::Mat3 normal_matrix = normal_matrix_result.value;
  const aster::Vec3 transformed_normal = normal_matrix * aster::Vec3{0.0f, 4.0f, 0.0f};
  expectNear(transformed_normal.x, 0.0f, 0.0001f);
  expectNear(transformed_normal.y, 1.0f, 0.0001f);
  expectNear(transformed_normal.z, 0.0f, 0.0001f);

  const aster::Mat3 singular =
      aster::mat3FromColumns({1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  assert(!aster::inverse(singular));
}

void testMatrixInverseAndDeterminant() {
  const aster::Mat4 transform = aster::translation({3.0f, -2.0f, 5.0f}) *
                                aster::rotation_y(aster::radians(28.0f)) *
                                aster::scale({1.5f, 2.0f, 0.75f});
  expectNear(aster::determinant(transform), 2.25f, 0.0005f);

  const aster::MathResult<aster::Mat4> inv_transform = aster::inverse(transform);
  assert(inv_transform);
  const aster::Mat4 round_trip = transform * inv_transform.value;
  const aster::Mat4 identity = aster::identity();
  for (std::size_t i = 0; i < round_trip.m.size(); ++i) {
    expectNear(round_trip.m[i], identity.m[i], 0.001f);
  }

  const aster::Vec3 point{0.25f, 0.5f, -0.75f};
  const aster::Vec3 restored =
      aster::transformPoint(inv_transform.value, aster::transformPoint(transform, point));
  expectNear(restored.x, point.x, 0.001f);
  expectNear(restored.y, point.y, 0.001f);
  expectNear(restored.z, point.z, 0.001f);

  const aster::Mat4 singular = aster::scale({1.0f, 0.0f, 1.0f});
  const aster::MathResult<aster::Mat4> singular_inverse = aster::inverse(singular);
  assert(!singular_inverse);
  assert(singular_inverse.diagnostics.error == aster::MathError::SingularMatrix);
  assert(!aster::normalMatrix(singular));
}

void testTransformContract() {
  aster::Transform transform;
  transform.position = {2.0f, 3.0f, 4.0f};
  transform.scale = {2.0f, 2.0f, 2.0f};

  const aster::Mat4 matrix = transform.matrix();
  expectNear(matrix.m[0], 2.0f, 0.0001f);
  expectNear(matrix.m[5], 2.0f, 0.0001f);
  expectNear(matrix.m[10], 2.0f, 0.0001f);
  expectNear(matrix.m[12], 2.0f, 0.0001f);
  expectNear(matrix.m[13], 3.0f, 0.0001f);
  expectNear(matrix.m[14], 4.0f, 0.0001f);
}

void testQuaternionTransformContract() {
  const aster::Quat yaw = aster::axisAngle({0.0f, 1.0f, 0.0f}, aster::radians(90.0f));
  const aster::Vec3 rotated = aster::rotate(yaw, {0.0f, 0.0f, 1.0f});
  expectNear(rotated.x, 1.0f, 0.0001f);
  expectNear(rotated.y, 0.0f, 0.0001f);
  expectNear(rotated.z, 0.0f, 0.0001f);

  const aster::Quat from_euler = aster::quatFromEulerXyz({0.0f, aster::radians(90.0f), 0.0f});
  const aster::Vec3 euler = aster::eulerXyz(from_euler);
  expectNear(euler.y, aster::radians(90.0f), 0.0001f);

  const aster::Quat halfway = aster::slerp(aster::identityQuat(), yaw, 0.5f);
  const aster::Vec3 halfway_forward = aster::rotate(halfway, {0.0f, 0.0f, 1.0f});
  const float diagonal = std::sqrt(0.5f);
  expectNear(halfway_forward.x, diagonal, 0.0001f);
  expectNear(halfway_forward.z, diagonal, 0.0001f);

  const aster::Transform transform =
      aster::Transform::fromEuler({1.0f, 2.0f, 3.0f}, {0.0f, aster::radians(90.0f), 0.0f},
                                  {2.0f, 2.0f, 2.0f});
  const aster::Vec3 point = aster::transformPoint(transform, {0.0f, 0.0f, 1.0f});
  expectNear(point.x, 3.0f, 0.0001f);
  expectNear(point.y, 2.0f, 0.0001f);
  expectNear(point.z, 3.0f, 0.0001f);
}

void testReverseZProjectionAndCamera() {
  constexpr float near_plane = 0.1f;
  constexpr float far_plane = 100.0f;
  const auto ndc_z = [](const aster::Mat4 &matrix, const aster::Vec3 point) {
    const aster::Vec4 clip = matrix * aster::Vec4{point.x, point.y, point.z, 1.0f};
    return clip.z / clip.w;
  };

  const aster::MathResult<aster::Mat4> perspective_result =
      aster::perspective(aster::radians(60.0f), 1.0f, near_plane, far_plane);
  assert(perspective_result);
  const aster::Mat4 perspective = perspective_result.value;
  expectNear(ndc_z(perspective, {0.0f, 0.0f, -near_plane}), 1.0f, 0.0001f);
  expectNear(ndc_z(perspective, {0.0f, 0.0f, -far_plane}), 0.0f, 0.0001f);

  const aster::ProjectionPolicy forward_no{aster::CoordinateHandedness::RightHanded,
                                           aster::ClipDepthRange::NegativeOneToOne,
                                           aster::DepthDirection::ForwardZ};
  const aster::MathResult<aster::Mat4> forward_no_perspective_result =
      aster::perspective(aster::radians(60.0f), 1.0f, near_plane, far_plane, forward_no);
  assert(forward_no_perspective_result);
  const aster::Mat4 forward_no_perspective = forward_no_perspective_result.value;
  expectNear(ndc_z(forward_no_perspective, {0.0f, 0.0f, -near_plane}), -1.0f, 0.0001f);
  expectNear(ndc_z(forward_no_perspective, {0.0f, 0.0f, -far_plane}), 1.0f, 0.0001f);

  const aster::MathResult<aster::Mat4> ortho_result =
      aster::orthographic(-2.0f, 2.0f, -1.0f, 1.0f, near_plane, far_plane);
  assert(ortho_result);
  const aster::Mat4 ortho = ortho_result.value;
  expectNear(ndc_z(ortho, {0.0f, 0.0f, -near_plane}), 1.0f, 0.0001f);
  expectNear(ndc_z(ortho, {0.0f, 0.0f, -far_plane}), 0.0f, 0.0001f);

  aster::Camera camera;
  camera.eye = {0.0f, 0.0f, 5.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.near_plane = near_plane;
  camera.far_plane = far_plane;
  const aster::WorldToClip view_projection = camera.viewProjectionMatrix(4.0f / 3.0f);
  const aster::MathResult<aster::Mat4> clip_to_world_result = aster::inverse(view_projection.value);
  assert(clip_to_world_result);
  const aster::ClipToWorld clip_to_world{clip_to_world_result.value};
  const aster::WorldPoint world{0.2f, -0.1f, 0.0f};
  const aster::Viewport viewport{{}, {800.0f, 600.0f}};
  const aster::MathResult<aster::ScreenPoint> window_result =
      aster::project(world, view_projection, viewport);
  assert(window_result);
  const aster::ScreenPoint window = window_result.value;
  const aster::MathResult<aster::WorldPoint> restored_result =
      aster::unproject(window, clip_to_world, viewport);
  assert(restored_result);
  const aster::WorldPoint restored = restored_result.value;
  expectNear(restored.x, world.x, 0.001f);
  expectNear(restored.y, world.y, 0.001f);
  expectNear(restored.z, world.z, 0.001f);

  const aster::CameraRay ray = camera.screenRay({400.0f, 300.0f, 0.0f}, viewport);
  expectNear(ray.origin.x, camera.eye.x, 0.0001f);
  expectNear(ray.origin.y, camera.eye.y, 0.0001f);
  expectNear(ray.origin.z, camera.eye.z, 0.0001f);
  assert(ray.direction.z < -0.99f);

  const aster::CameraFrustum frustum = camera.frustum(4.0f / 3.0f);
  for (const aster::Plane3 &plane : frustum.planes) {
    assert(aster::length(plane.normal) > 0.99f);
    assert(aster::length(plane.normal) < 1.01f);
  }
  for (const aster::Plane3 &plane : frustum.planes) {
    assert(aster::dot(plane.normal, aster::Vec3{0.0f, 0.0f, 0.0f}) + plane.distance >= -0.001f);
  }
}

void testColorPipeline() {
  const aster::LinearRgb linear_white = aster::srgbToLinear(aster::Srgb{1.0f, 1.0f, 1.0f});
  expectNear(linear_white.x, 1.0f, 0.0001f);
  const aster::LinearRgb linear_mid = aster::srgbToLinear(aster::Srgb{0.5f, 0.5f, 0.5f});
  expectNear(linear_mid.x, 0.2140f, 0.001f);
  const aster::Srgb restored_mid = aster::linearToSrgb(linear_mid);
  expectNear(restored_mid.x, 0.5f, 0.001f);
  const aster::EmissionColor emission{0.25f, 0.50f, 1.0f};
  const aster::HdrColor hdr_emission = aster::emissionToHdr(emission, 4.0f);
  expectNear(hdr_emission.z, 4.0f, 0.0001f);
  const aster::Luminance luma = aster::relativeLuminance(aster::LinearRgb{1.0f, 1.0f, 1.0f});
  expectNear(luma.value, 1.0f, 0.0001f);
  const aster::Vec3 mapped = aster::aces_tonemap({2.0f, 1.0f, 0.25f});
  assert(mapped.x <= 1.0f && mapped.x >= 0.0f);
  assert(mapped.y <= 1.0f && mapped.y >= 0.0f);
  assert(mapped.z <= 1.0f && mapped.z >= 0.0f);
  const aster::Vec3 reinhard = aster::reinhard_tonemap({1.0f, 3.0f, 0.0f});
  expectNear(reinhard.x, 0.5f, 0.0001f);
  expectNear(reinhard.y, 0.75f, 0.0001f);
  expectNear(reinhard.z, 0.0f, 0.0001f);
}

void testFixedTimestep() {
  aster::FixedTimestep timestep({1.0 / 60.0, 1.0 / 20.0, 4});
  assert(timestep.advance(1.0) == 3u);
  assert(timestep.accumulatorSeconds() < 0.0001);
  assert(timestep.advance(0.001) == 0u);
  assert(timestep.interpolationAlpha() > 0.0);
  assert(timestep.advance(1.0 / 60.0) == 1u);
  timestep.reset();
  assert(timestep.accumulatorSeconds() == 0.0);
}

void testFrameTimeStats() {
  aster::FrameTimeStats stats;
  assert(stats.empty());
  stats.addSample(0.010);
  stats.addSample(0.020);
  stats.addSample(0.030);
  stats.addSample(-1.0);

  const aster::FrameTimeSummary summary = stats.summarize(0.016);
  assert(summary.samples == 3u);
  expectNear(summary.min_seconds, 0.010, 0.000001);
  expectNear(summary.mean_seconds, 0.020, 0.000001);
  expectNear(summary.p95_seconds, 0.030, 0.000001);
  expectNear(summary.max_seconds, 0.030, 0.000001);
  assert(summary.budget_seconds.has_value());
  assert(summary.over_budget == 2u);
}

void testProfilerCaptureExport() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "aster_profile_test.csv";
  std::filesystem::remove(path);
  assert(aster::profile::startCapture());
  {
    ASTER_PROFILE_SCOPE("profiler.test.scope");
  }
  assert(aster::profile::stopCapture());
  assert(aster::profile::saveCapture(path.string().c_str()));
  std::ifstream file(path);
  std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  assert(text.find("profiler.test.scope") != std::string::npos);
  aster::profile::shutdown();
  std::filesystem::remove(path);
}

void testBudgetedWorkQueueContracts() {
  aster::BudgetedWorkQueue queue;
  queue.enqueue({.id = 30u, .class_id = 2u, .priority = 4.0, .sequence = 0u});
  queue.enqueue({.id = 10u, .class_id = 2u, .priority = 4.0, .sequence = 1u});
  queue.enqueue({.id = 20u, .class_id = 1u, .priority = 2.0, .sequence = 2u});

  const aster::BudgetedWorkSelection deterministic = queue.select({});
  assert(deterministic.selected.size() == 3u);
  assert(deterministic.selected[0].id == 10u);
  assert(deterministic.selected[1].id == 30u);
  assert(deterministic.selected[2].id == 20u);
  assert(deterministic.diagnostics.queued_items == 3u);
  assert(deterministic.diagnostics.deferred_items == 0u);

  aster::FrameWorkBudget class_limited;
  class_limited.max_items = 3u;
  class_limited.class_budgets.push_back({.class_id = 2u, .max_items = 1u});
  const aster::BudgetedWorkSelection limited = queue.select(class_limited);
  assert(limited.selected.size() == 2u);
  assert(limited.deferred.size() == 1u);
  assert(limited.diagnostics.budget_exhausted_items == 1u);

  aster::BudgetedWorkQueue starvation_queue;
  starvation_queue.enqueue({.id = 1u, .class_id = 1u, .priority = 100.0, .sequence = 0u});
  starvation_queue.enqueue({.id = 2u,
                            .class_id = 1u,
                            .priority = 0.0,
                            .virtual_backlog_frames = 5u,
                            .sequence = 1u});
  aster::FrameWorkBudget no_starvation;
  no_starvation.max_items = 1u;
  assert(starvation_queue.select(no_starvation).selected.front().id == 1u);

  aster::FrameWorkBudget starvation_bound = no_starvation;
  starvation_bound.starvation_frame_limit = 4u;
  assert(starvation_queue.select(starvation_bound).selected.front().id == 2u);

  aster::WorkCostModel costs;
  costs.observe({.class_id = 7u, .seconds = 0.004});
  costs.observe({.class_id = 7u, .seconds = 0.002});
  const std::optional<aster::WorkCostEstimate> estimate = costs.estimateFor(7u);
  assert(estimate.has_value());
  assert(estimate->sample_count == 2u);
  assert(costs.estimate(7u, 0.001) >= 0.001);

  aster::FrameBudgetController controller({.target_frame_seconds = 1.0 / 60.0,
                                           .min_work_seconds = 0.001,
                                           .max_work_seconds = 0.006,
                                           .min_items = 1u,
                                           .max_items = 5u});
  aster::FrameWorkBudget base_budget;
  base_budget.class_budgets.push_back({.class_id = 7u});
  const aster::FrameWorkBudget spare_budget =
      controller.nextBudget({.frame_seconds = 0.010, .backlog_items = 18u}, base_budget);
  assert(spare_budget.max_items >= 1u);
  assert(spare_budget.max_seconds >= 0.001);
  const aster::FrameWorkBudget pressured_budget =
      controller.nextBudget({.frame_seconds = 0.026, .backlog_items = 18u}, base_budget);
  assert(pressured_budget.max_seconds <= spare_budget.max_seconds);
  assert(controller.telemetry().pressure > 0.0);
}

void testAsterCoreRuntimeContracts() {
  int signal_total = 0;
  aster::Signal<int> signal;
  aster::SignalConnection connection = signal.connect([&](const int value) {
    signal_total += value;
  });
  assert(connection.connected());
  signal.emit(3);
  connection.disconnect();
  signal.emit(5);
  assert(signal_total == 3);
  assert(signal.listenerCount() == 0u);

  aster::ModuleRegistry modules;
  std::vector<std::string> events;
  aster::SignalConnection module_event_connection =
      modules.changes().connect([&](const aster::ModuleChangeEvent &event) {
        events.push_back(event.id + ":" + aster::moduleChangeKindName(event.kind));
      });
  assert(module_event_connection.connected());

  std::vector<std::string> lifecycle;
  assert(modules.registerModule({.id = "core.assets",
                                 .display_name = "Asset Spine",
                                 .initialize = [&](aster::ModuleContext &context) {
                                   lifecycle.push_back(context.module_id + ".init");
                                   return true;
                                 },
                                 .shutdown = [&](aster::ModuleContext &context) {
                                   lifecycle.push_back(context.module_id + ".shutdown");
                                   return true;
                                 }}));
  assert(modules.registerModule({.id = "studio.asset-browser",
                                 .display_name = "Asset Browser",
                                 .dependencies = {"core.assets"},
                                 .initialize = [&](aster::ModuleContext &context) {
                                   lifecycle.push_back(context.module_id + ".init");
                                   return true;
                                 },
                                 .shutdown = [&](aster::ModuleContext &context) {
                                   lifecycle.push_back(context.module_id + ".shutdown");
                                   return true;
                                 }}));
  assert(modules.loadModule("studio.asset-browser"));
  assert(modules.status("core.assets")->state == aster::ModuleLifecycleState::Loaded);
  assert(modules.loadedOrder().size() == 2u);
  assert(modules.loadedOrder()[0] == "core.assets");
  assert(modules.loadedOrder()[1] == "studio.asset-browser");
  assert(lifecycle[0] == "core.assets.init");
  assert(lifecycle[1] == "studio.asset-browser.init");
  assert(modules.unloadModule("core.assets"));
  assert(modules.status("studio.asset-browser")->state == aster::ModuleLifecycleState::Unloaded);
  assert(modules.status("core.assets")->state == aster::ModuleLifecycleState::Unloaded);
  assert(!events.empty());

  aster::JobGraph graph({.worker_count = 1u, .deterministic = true});
  std::vector<int> job_order;
  const aster::JobId prepare = graph.add({.name = "prepare",
                                          .priority = aster::JobPriority::High,
                                          .run = [&](aster::JobContext &context) {
                                            assert(context.deterministic);
                                            job_order.push_back(1);
                                          }});
  graph.add({.name = "background",
             .priority = aster::JobPriority::Background,
             .run = [&](aster::JobContext &) { job_order.push_back(0); }});
  const aster::JobId finish = graph.add({.name = "finish",
                                         .priority = aster::JobPriority::Normal,
                                         .dependencies = {prepare},
                                         .run = [&](aster::JobContext &) {
                                           job_order.push_back(2);
                                         }});
  const aster::JobGraphDiagnostics diagnostics = graph.run();
  assert(diagnostics.executed_jobs == 3u);
  assert(diagnostics.failed_jobs == 0u);
  assert(graph.record(finish)->status == aster::JobStatus::Complete);
  assert(job_order.size() == 3u);
  assert(job_order[0] == 1);
  assert(job_order[2] == 2);

  std::vector<int> values(12, 0);
  aster::JobGraph parallel({.worker_count = 2u, .deterministic = false});
  const aster::JobGraphDiagnostics parallel_diagnostics =
      parallel.parallelFor("fill", values.size(), 3u, [&](const std::size_t index) {
        values[index] = static_cast<int>(index * 2u);
      });
  assert(parallel_diagnostics.executed_jobs == 4u);
  for (std::size_t index = 0u; index < values.size(); ++index) {
    assert(values[index] == static_cast<int>(index * 2u));
  }
}

void testWorldStateTransitionContracts() {
  aster::WorldState world({.fixed_step_seconds = 1.0 / 30.0,
                           .seed = 0xA57E5300u,
                           .label = "core-world"});
  const aster::WorldEntityHandle entity = world.createEntity("entity.player");
  assert(world.contains(entity));
  const aster::WorldTickResult first_tick =
      world.tick({.tick = 1u,
                  .delta_seconds = 1.0 / 30.0,
                  .input_event_hash = 0x11u,
                  .asset_lineage_hash = 0x22u,
                  .extraction_hash = 0x33u,
                  .frame_submission_hash = 0x44u});
  assert(first_tick.accepted);
  assert(first_tick.tick == 1u);
  assert(first_tick.world_hash != 0u);
  assert(first_tick.trace_hash != 0u);
  aster::WorldState mirror({.fixed_step_seconds = 1.0 / 30.0,
                            .seed = 0xA57E5300u,
                            .label = "core-world"});
  const aster::WorldEntityHandle mirror_entity = mirror.createEntity("entity.player");
  assert(mirror_entity == entity);
  const aster::WorldTickResult mirror_tick =
      mirror.tick({.tick = 1u,
                   .delta_seconds = 1.0 / 30.0,
                   .input_event_hash = 0x11u,
                   .asset_lineage_hash = 0x22u,
                   .extraction_hash = 0x33u,
                   .frame_submission_hash = 0x44u});
  assert(mirror_tick.accepted);
  assert(mirror_tick.world_hash == first_tick.world_hash);
  assert(mirror_tick.trace_hash == first_tick.trace_hash);
  assert(!world.tick({.tick = 1u, .delta_seconds = 1.0 / 30.0}).accepted);

  const aster::WorldTransactionInfo read_tx = world.beginTransaction(
      {.label = "read-transform",
       .provenance = "movement",
       .accesses = {{.component = "Transform",
                     .subject = "entity.player",
                     .mode = aster::WorldComponentAccessMode::Read}}});
  const aster::WorldTransactionInfo read_commit = world.commitTransaction(read_tx.transaction_id);
  assert(read_commit.committed);
  const aster::WorldTransactionInfo mirror_read_tx = mirror.beginTransaction(
      {.label = "read-transform",
       .provenance = "movement",
       .accesses = {{.component = "Transform",
                     .subject = "entity.player",
                     .mode = aster::WorldComponentAccessMode::Read}}});
  const aster::WorldTransactionInfo mirror_read_commit =
      mirror.commitTransaction(mirror_read_tx.transaction_id);
  assert(mirror_read_commit.committed);
  assert(mirror_read_commit.deterministic_stamp == read_commit.deterministic_stamp);
  assert(mirror_read_commit.post_world_hash == read_commit.post_world_hash);

  const aster::WorldTransactionInfo write_tx = world.beginTransaction(
      {.label = "write-transform",
       .provenance = "animation",
       .accesses = {{.component = "Transform",
                     .subject = "entity.player",
                     .mode = aster::WorldComponentAccessMode::Write}}});
  const aster::WorldTransactionInfo rejected = world.commitTransaction(write_tx.transaction_id);
  assert(!rejected.committed);
  assert(rejected.diagnostic.find("component access hazard") != std::string::npos);

  const std::vector<aster::ResidencyDecision> decisions = world.planResidency(
      {.byte_budget = 100u},
      {{.asset_id = "asset.visible.high",
        .byte_cost = 60u,
        .priority = 10.0f,
        .visible = true,
        .resident = false},
       {.asset_id = "asset.visible.low",
        .byte_cost = 60u,
        .priority = 2.0f,
        .visible = true,
        .resident = true},
       {.asset_id = "asset.hidden",
        .byte_cost = 10u,
        .priority = 100.0f,
        .visible = false,
        .resident = true}});
  assert(decisions.size() == 3u);
  assert(decisions[0].decision == aster::ResidencyDecisionKind::Load);
  assert(decisions[1].decision == aster::ResidencyDecisionKind::Evict);
  assert(decisions[2].decision == aster::ResidencyDecisionKind::Evict);
  const std::vector<aster::ResidencyDecision> tie_decisions = world.planResidency(
      {.byte_budget = 50u},
      {{.asset_id = "asset.tie.b",
        .byte_cost = 50u,
        .priority = 1.0f,
        .visible = true,
        .resident = false},
       {.asset_id = "asset.tie.a",
        .byte_cost = 50u,
        .priority = 1.0f,
        .visible = true,
        .resident = false}});
  assert(tie_decisions.size() == 2u);
  assert(tie_decisions[0].decision == aster::ResidencyDecisionKind::Reject);
  assert(tie_decisions[1].decision == aster::ResidencyDecisionKind::Load);

  const aster::WorldTraceCounts counts = world.counts();
  assert(counts.tick == 1u);
  assert(counts.entity_count == 1u);
  assert(counts.validation_event_count >= 2u);
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "aster_world_state_core_test.txt";
  std::string diagnostic;
  assert(world.saveSnapshot(path, &diagnostic));
  const aster::WorldReplayReport replay = world.replaySnapshot(path, counts.world_hash);
  assert(replay.matched);
  const aster::WorldReplayReport mismatch_replay =
      world.replaySnapshot(path, counts.world_hash ^ 0x1u);
  assert(!mismatch_replay.matched);
  aster::WorldState loaded({.seed = 0xA57E5300u, .label = "core-world"});
  const aster::WorldMigrationReport migration = loaded.loadSnapshot(path);
  assert(migration.current_schema_version == 1u);
  assert(migration.entity_count == 1u);
  assert(migration.world_hash == counts.world_hash);
  std::filesystem::remove(path);

  assert(world.destroyEntity(entity, "test"));
  assert(!world.contains(entity));
  assert(!world.destroyEntity(entity, "stale"));
}

void testWorldPerceptionLedgerContracts() {
  const std::uint32_t required =
      aster::worldPerceptionLedgerChannelBit("material_memory") |
      aster::worldPerceptionLedgerChannelBit("contact_history") |
      aster::worldPerceptionLedgerChannelBit("streaming_semantic_lod");
  aster::WorldPerceptionLedgerCellDesc cell;
  cell.region_id = 0xCAFEu;
  cell.cell_id = "entry";
  cell.required_channel_mask = required;
  cell.minimum_score = 1.0f;
  cell.evidence = {
      {aster::WorldPerceptionLedgerChannel::MaterialMemory, 0x101u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::ContactHistory, 0x202u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::StreamingSemanticLod, 0x303u, 1.0f},
  };
  const aster::WorldPerceptionLedgerCellReport first =
      aster::evaluateWorldPerceptionLedgerCell(cell);
  const aster::WorldPerceptionLedgerCellReport second =
      aster::evaluateWorldPerceptionLedgerCell(cell);
  assert(first.accepted);
  assert(first.ledger_hash == second.ledger_hash);
  assert(first.missing_channel_mask == 0u);
  assert(first.material_memory_hash != 0u);
  assert(first.contact_history_hash != 0u);

  aster::WorldPerceptionLedgerCellDesc missing = cell;
  missing.evidence.pop_back();
  const aster::WorldPerceptionLedgerCellReport missing_report =
      aster::evaluateWorldPerceptionLedgerCell(missing);
  assert(!missing_report.accepted);
  assert(missing_report.missing_channel_mask ==
         aster::worldPerceptionLedgerChannelBit("streaming_semantic_lod"));
  const aster::WorldPerceptionLedgerReport ledger =
      aster::summarizeWorldPerceptionLedger(0xCAFEu, required, 1.0f, {first});
  assert(ledger.accepted);
  assert(ledger.cell_count == 1u);
  assert(ledger.ledger_hash != 0u);
}

aster::WorldPerceptionLedgerReport makeFullPerceptualLedger() {
  const std::uint32_t required =
      aster::worldPerceptionLedgerChannelBit("material_memory") |
      aster::worldPerceptionLedgerChannelBit("contact_history") |
      aster::worldPerceptionLedgerChannelBit("lighting_exposure") |
      aster::worldPerceptionLedgerChannelBit("atmosphere_cell") |
      aster::worldPerceptionLedgerChannelBit("occlusion_role") |
      aster::worldPerceptionLedgerChannelBit("gameplay_affordance") |
      aster::worldPerceptionLedgerChannelBit("wear_continuity") |
      aster::worldPerceptionLedgerChannelBit("streaming_semantic_lod") |
      aster::worldPerceptionLedgerChannelBit("audio_visual_cue_budget");
  aster::WorldPerceptionLedgerCellDesc cell;
  cell.region_id = 0x47u;
  cell.cell_id = "long-horizon-cave";
  cell.required_channel_mask = required;
  cell.minimum_score = 1.0f;
  cell.evidence = {
      {aster::WorldPerceptionLedgerChannel::MaterialMemory, 0x101u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::ContactHistory, 0x202u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::LightingExposure, 0x303u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::AtmosphereCell, 0x404u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::OcclusionRole, 0x505u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::GameplayAffordance, 0x606u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::WearContinuity, 0x707u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::StreamingSemanticLod, 0x808u, 1.0f},
      {aster::WorldPerceptionLedgerChannel::AudioVisualCueBudget, 0x909u, 1.0f},
  };
  return aster::summarizeWorldPerceptionLedger(
      0x47u, required, 1.0f, {aster::evaluateWorldPerceptionLedgerCell(cell)});
}

aster::PerceptualWorldObservation makeStrongPerceptualObservation() {
  aster::PerceptualWorldObservation observation =
      aster::makePerceptualWorldObservation(makeFullPerceptualLedger());
  observation.delta_seconds = 1.0f;
  observation.world_transition_hash = 0xA57E1001u;
  observation.visibility_set_hash = 0xA57E2002u;
  observation.player_position = {1.0f, 0.0f, 0.0f};
  observation.navigation_valid = true;
  observation.perceptual_salience_score = 1.0f;
  observation.traversal_speed = 2.2f;
  observation.encounter_pressure = 0.75f;
  observation.resource_pressure = 0.85f;
  observation.explicit_player_readable_cause = 1.0f;
  observation.reaction_package_hash = 0x1111u;
  observation.material_memory_hash = 0x2222u;
  observation.contact_history_hash = 0x3333u;
  observation.lighting_atmosphere_hash = 0x4444u;
  observation.wear_continuity_hash = 0x5555u;
  observation.ai_attention_hash = 0x6666u;
  observation.streaming_residency_lod_hash = 0x7777u;
  observation.resource_state_hash = 0x8888u;
  observation.event_residue_hash = 0x9999u;
  observation.audio_visual_cue_budget_hash = 0xAAA1u;
  observation.readability_audit_hash = 0xBBB2u;
  return observation;
}

void testPerceptualWorldRuntimeContracts() {
  const aster::PerceptualWorldRuntimeOptions options{
      .region_id = 0x47u,
      .id = "core-test-perceptual-runtime",
      .exposure_horizon_seconds = 47.0f,
      .minimum_continuity_score = 0.62f,
      .minimum_occlusion_trust = 0.45f,
      .minimum_lighting_believability = 0.45f,
      .minimum_player_readable_cause = 0.45f};
  aster::PerceptualWorldRuntime first(options);
  aster::PerceptualWorldRuntime second(options);
  aster::PerceptualWorldObservation strong = makeStrongPerceptualObservation();
  for (int i = 0; i < 47; ++i) {
    strong.player_position.x += 0.10f;
    const aster::PerceptualFrameState first_state = first.advance(strong);
    const aster::PerceptualFrameState second_state = second.advance(strong);
    assert(first_state.perceptual_state_hash == second_state.perceptual_state_hash);
  }
  const aster::PerceptualFrameState mature = first.lastState();
  assert(mature.accepted);
  assert(mature.exposure_seconds >= 47.0f);
  assert(mature.continuity_debt < 0.05f);
  assert(mature.material_memory > 0.70f);
  assert(mature.traversal_pressure > 0.55f);
  assert(mature.occlusion_trust > 0.55f);
  assert(mature.ecology_signal > 0.50f);
  assert(mature.player_readable_cause > 0.50f);
  assert(mature.semantic_budget_hash != 0u);

  aster::PerceptualWorldRuntime debt_runtime(options);
  aster::PerceptualWorldObservation weak;
  weak.delta_seconds = 1.0f;
  weak.region_id = 0x47u;
  for (int i = 0; i < 6; ++i) {
    (void)debt_runtime.advance(weak);
  }
  const float weak_debt = debt_runtime.lastState().continuity_debt;
  for (int i = 0; i < 47; ++i) {
    (void)debt_runtime.advance(strong);
  }
  assert(weak_debt > debt_runtime.lastState().continuity_debt);
  assert(debt_runtime.lastState().accepted);

  aster::PerceptualWorldObservation no_occlusion = strong;
  no_occlusion.ledger.occlusion_role_hash = 0u;
  no_occlusion.visibility_set_hash = 0u;
  (void)first.advance(no_occlusion);
  assert(first.lastState().occlusion_trust > 0.45f);

  aster::PerceptualWorldObservation no_residue = strong;
  no_residue.reaction_package_hash = 0u;
  no_residue.event_residue_hash = 0u;
  no_residue.audio_visual_cue_budget_hash = 0u;
  (void)first.advance(no_residue);
  assert(first.lastState().interaction_residue > 0.45f);
}

void testWorldPerceptualPrimitiveContracts() {
  const aster::WorldPerceptionLedgerReport ledger = makeFullPerceptualLedger();
  aster::PerceptualWorldRuntime runtime({.region_id = ledger.region_id,
                                         .id = "primitive-test-runtime",
                                         .exposure_horizon_seconds = 8.0f,
                                         .minimum_continuity_score = 0.50f,
                                         .minimum_occlusion_trust = 0.40f,
                                         .minimum_lighting_believability = 0.40f,
                                         .minimum_player_readable_cause = 0.40f});
  aster::PerceptualWorldObservation observation = makeStrongPerceptualObservation();
  for (int i = 0; i < 12; ++i) {
    observation.player_position.x += 0.20f;
    (void)runtime.advance(observation);
  }
  aster::PerceptualWorldScheduleDesc schedule_desc;
  schedule_desc.region_id = ledger.region_id;
  schedule_desc.world_transition_hash = 0xA57E5001u;
  schedule_desc.actor_state_delta_hash = 0xA57E5002u;
  schedule_desc.sensory_event_hash = 0xA57E5003u;
  schedule_desc.visibility_set_hash = 0xA57E5004u;
  schedule_desc.navigation_valid = true;
  schedule_desc.perceptual_salience_score = 0.92f;
  schedule_desc.encounter_pressure = 0.40f;
  schedule_desc.resource_pressure = 0.65f;
  schedule_desc.frame_cost_ms = 10.0f;
  schedule_desc.ledger = ledger;
  schedule_desc.perceptual_state = runtime.lastState();
  schedule_desc.reaction_package_hash = 0xA57E6001u;
  schedule_desc.material_memory_hash = 0xA57E6002u;
  schedule_desc.contact_history_hash = 0xA57E6003u;
  schedule_desc.lighting_atmosphere_hash = 0xA57E6004u;
  schedule_desc.wear_continuity_hash = 0xA57E6005u;
  schedule_desc.ai_attention_hash = 0xA57E6006u;
  schedule_desc.streaming_residency_lod_hash = 0xA57E6007u;
  schedule_desc.resource_state_hash = 0xA57E6008u;
  schedule_desc.event_residue_hash = 0xA57E6009u;
  schedule_desc.audio_visual_cue_budget_hash = 0xA57E600Au;
  schedule_desc.readability_audit_hash = 0xA57E600Bu;
  const aster::PerceptualWorldScheduleReport schedule =
      aster::schedulePerceptualWorld(schedule_desc);
  const aster::WorldPerceptualPrimitive primitive =
      aster::makeWorldPerceptualPrimitiveFromRuntime("primitive.core.ore",
                                                     "Core primitive ore",
                                                     runtime.lastState(), schedule, ledger);
  assert(primitive.accepted);
  assert(primitive.truth_hash != 0u);
  assert(primitive.active_cell_anchor_count == 1u);
  assert(primitive.active_surface_patch_count == 1u);
  assert(primitive.active_contact_zone_count == 1u);
  assert(primitive.active_residue_channel_count == 1u);
  assert(primitive.signals.material_memory > 0.0f);
  assert(primitive.signals.decision_impact > 0.0f);

  const aster::WorldPerceptualSignals decayed =
      aster::decayWorldPerceptualSignals({}, primitive.signals, 0.25f, 2.0f);
  assert(decayed.material_memory > 0.0f);
  assert(decayed.material_memory < primitive.signals.material_memory);
  const aster::WorldPerceptualPrimitiveSummary summary =
      aster::summarizeWorldPerceptualPrimitives({primitive});
  assert(summary.accepted);
  assert(summary.primitive_count == 1u);
  assert(summary.truth_hash != 0u);
  assert(summary.material_memory == primitive.signals.material_memory);

  aster::WorldPerceptualField field_a;
  aster::WorldPerceptualField field_b;
  aster::WorldPerceptualFieldObservation field_observation;
  field_observation.key = {.world_owner_hash = 0xA57E7101u,
                           .template_hash = 0xA57E7102u,
                           .cell_hash = 0xA57E7103u};
  field_observation.primitive_id = "field.ore";
  field_observation.object_name = "Field ore";
  field_observation.player_readable_cause_hash = 0xA57E7104u;
  field_observation.delta_seconds = 1.0f;
  field_observation.material_half_life_seconds = 2.0f;
  field_observation.cell_residency = 1.0f;
  field_observation.streaming_cost = 0.25f;
  field_observation.material_stability = 0.88f;
  field_observation.acoustic_occlusion_trust = 0.74f;
  field_observation.visual_occlusion_trust = 0.82f;
  field_observation.traversal_affordance = 0.62f;
  field_observation.semantic_lod = 0.77f;
  field_observation.target_signals = {.belief_state = 0.86f,
                                      .perceptual_debt = 0.10f,
                                      .material_memory = 0.72f,
                                      .interaction_residue = 0.58f,
                                      .contact_field = 0.76f,
                                      .light_history = 0.68f,
                                      .acoustic_occlusion = 0.22f,
                                      .ecology_pressure = 0.44f,
                                      .threat_gradient = 0.38f,
                                      .traversal_pressure = 0.62f,
                                      .semantic_lod = 0.77f,
                                      .decision_impact = 0.66f,
                                      .player_readable_cause = 0.84f};
  const aster::WorldPerceptualPrimitive field_first_a = field_a.advance(field_observation);
  const aster::WorldPerceptualPrimitive field_first_b = field_b.advance(field_observation);
  assert(field_first_a.accepted);
  assert(field_first_a.truth_hash == field_first_b.truth_hash);
  assert(field_first_a.template_hash == field_observation.key.template_hash);
  assert(field_a.states().size() == 1u);

  field_observation.target_signals.material_memory = 0.20f;
  field_observation.target_signals.interaction_residue = 0.18f;
  const aster::WorldPerceptualPrimitive field_second = field_a.advance(field_observation);
  assert(field_second.exposure_age_seconds > field_first_a.exposure_age_seconds);
  assert(field_second.truth_hash != field_first_a.truth_hash);
  assert(field_second.signals.material_memory < field_first_a.signals.material_memory);
}

void testNeuralIrradianceVolumeContracts() {
  const aster::NeuralIrradianceVolumeDesc volume =
      aster::makeDefaultNeuralIrradianceVolume(0xA57E2602u);
  const aster::NeuralIrradianceQuery query{.cell_position = {0.24f, -0.18f, 0.52f},
                                           .normal = {0.0f, 0.86f, 0.24f},
                                           .torch_intensity = 0.20f,
                                           .fixture_intensity = 0.40f,
                                           .exposure_age_seconds = 6.0f,
                                           .wetness = 0.30f,
                                           .material_memory = 0.58f,
                                           .occlusion_trust = 0.76f,
                                           .semantic_lod = 0.82f};
  const aster::NeuralIrradianceSample first =
      aster::evaluateNeuralIrradianceVolume(volume, query);
  const aster::NeuralIrradianceSample replay =
      aster::evaluateNeuralIrradianceVolume(volume, query);
  assert(first.sample_hash == replay.sample_hash);
  expectNear(first.diffuse_irradiance.x, replay.diffuse_irradiance.x, 0.000001f);
  assert(first.confidence > 0.0f);

  aster::NeuralIrradianceQuery torch_changed = query;
  torch_changed.torch_intensity = 0.82f;
  const aster::NeuralIrradianceSample changed =
      aster::evaluateNeuralIrradianceVolume(volume, torch_changed);
  assert(changed.sample_hash != first.sample_hash);
  assert(changed.diffuse_irradiance.x > first.diffuse_irradiance.x);
}

bool hasBeliefFinding(const aster::BeliefExtractionReport &report,
                      const aster::BeliefFindingKind kind) {
  return std::any_of(report.findings.begin(), report.findings.end(),
                     [kind](const aster::BeliefExtractionFinding &finding) {
                       return finding.kind == kind && finding.evidence_hash != 0u &&
                              !finding.message.empty();
                     });
}

void testBeliefExtractionContracts() {
  aster::BeliefExtractionDesc accepted;
  accepted.subject = "accepted cave";
  accepted.minimum_score = 0.70f;
  accepted.visible_object_count = 8u;
  accepted.material_family_count = 5u;
  accepted.perception_ledger = aster::summarizeWorldPerceptionLedger(0xA57Eu, 0u, 0.0f, {});
  accepted.perceptual_state = {.perceptual_state_hash = 0xA57E1001u,
                               .material_memory = 0.82f,
                               .interaction_residue = 0.78f,
                               .traversal_pressure = 0.76f,
                               .lighting_believability = 0.80f,
                               .occlusion_trust = 0.84f,
                               .ecology_signal = 0.72f,
                               .player_readable_cause = 0.74f,
                               .semantic_budget_hash = 0xA57E2002u,
                               .accepted = true};
  const aster::BeliefExtractionReport accepted_report =
      aster::extractBeliefContract(accepted);
  assert(accepted_report.accepted);
  assert(accepted_report.score >= accepted_report.minimum_score);
  assert(accepted_report.belief_contract_hash != 0u);
  assert(accepted_report.readability_audit_hash != 0u);
  assert(accepted_report.findings.empty());

  const auto require_finding = [&](aster::BeliefExtractionDesc desc,
                                   const aster::BeliefFindingKind kind) {
    const aster::BeliefExtractionReport report = aster::extractBeliefContract(desc);
    assert(!report.accepted);
    assert(hasBeliefFinding(report, kind));
  };

  aster::BeliefExtractionDesc material = accepted;
  material.subject = "repeated material spheres";
  material.visible_object_count = 8u;
  material.material_family_count = 1u;
  require_finding(material, aster::BeliefFindingKind::MaterialFamilyCollapse);

  aster::BeliefExtractionDesc grounding = accepted;
  const std::uint32_t contact_required =
      aster::worldPerceptionLedgerChannelBit("contact_history");
  grounding.perception_ledger =
      aster::summarizeWorldPerceptionLedger(0xA57Eu, contact_required, 1.0f, {});
  require_finding(grounding, aster::BeliefFindingKind::ContextualGroundingFailure);

  aster::BeliefExtractionDesc contact = accepted;
  contact.contact_shadow_enabled = false;
  require_finding(contact, aster::BeliefFindingKind::ContactShadowCredibilityFailure);

  aster::BeliefExtractionDesc fog = accepted;
  fog.volumetric_required = true;
  fog.volumetric_scene_coupled = false;
  require_finding(fog, aster::BeliefFindingKind::VolumetricSceneCouplingFailure);

  aster::BeliefExtractionDesc material_response = accepted;
  material_response.material_response_stability = 0.10f;
  material_response.perceptual_state.material_memory = 0.0f;
  require_finding(material_response, aster::BeliefFindingKind::MaterialResponseInstability);

  aster::BeliefExtractionDesc lod = accepted;
  lod.lod_transition_invisibility = 0.10f;
  require_finding(lod, aster::BeliefFindingKind::LodTransitionVisibility);

  aster::BeliefExtractionDesc scale = accepted;
  scale.asset_scale_coherence = 0.10f;
  require_finding(scale, aster::BeliefFindingKind::AssetScaleIncoherence);

  aster::BeliefExtractionDesc entropy = accepted;
  entropy.environmental_entropy = 0.10f;
  entropy.perceptual_state.ecology_signal = 0.0f;
  require_finding(entropy, aster::BeliefFindingKind::EnvironmentalEntropyDeficit);

  aster::BeliefExtractionDesc light = accepted;
  light.light_history_continuity = 0.10f;
  require_finding(light, aster::BeliefFindingKind::LightHistoryDiscontinuity);

  aster::BeliefExtractionDesc debt = accepted;
  debt.interaction_debt_leak = 0.90f;
  require_finding(debt, aster::BeliefFindingKind::InteractionDebtLeak);

  aster::BeliefExtractionDesc repetition = accepted;
  repetition.semantic_repetition_score = 0.90f;
  require_finding(repetition, aster::BeliefFindingKind::SemanticRepetition);

  aster::BeliefExtractionDesc ai = accepted;
  ai.ai_attention_coherence = 0.10f;
  require_finding(ai, aster::BeliefFindingKind::AiAttentionIncoherence);

  aster::BeliefExtractionDesc memory = accepted;
  memory.surface_memory_continuity = 0.10f;
  require_finding(memory, aster::BeliefFindingKind::SurfaceMemoryReset);

  aster::BeliefExtractionDesc acoustic = accepted;
  acoustic.acoustic_truth = 0.10f;
  require_finding(acoustic, aster::BeliefFindingKind::AcousticFalseness);

  aster::BeliefExtractionDesc sync = accepted;
  sync.world_state_sync = 0.10f;
  require_finding(sync, aster::BeliefFindingKind::WorldStateDesynchronization);

  assert(aster::beliefFindingKindName(
             aster::BeliefFindingKind::MaterialFamilyCollapse) ==
         "material_family_collapse");
  assert(aster::beliefFindingSeverityName(aster::BeliefFindingSeverity::Warning) ==
         "warning");
}

void testSourceBoundaryContracts() {
  const std::filesystem::path project_root =
      std::filesystem::path(__FILE__).parent_path().parent_path();
  assert(!std::filesystem::exists(project_root / "src/runtime"));
  assert(!std::filesystem::exists(project_root / "assets/sample_scene"));

  const std::filesystem::path public_root = project_root / "include/aster";
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(public_root)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".hpp") {
      continue;
    }
    std::ifstream file(entry.path());
    std::string line;
    while (std::getline(file, line)) {
      const std::string_view include_prefix = "#include \"";
      const std::size_t prefix = line.find(include_prefix);
      if (prefix == std::string::npos) {
        continue;
      }
      const std::size_t begin = prefix + include_prefix.size();
      const std::size_t end = line.find('"', begin);
      assert(end != std::string::npos);
      const std::string include_path = line.substr(begin, end - begin);
      assert(startsWith(include_path, "aster/"));
    }
  }
}

void testConfigLayerStackAndSessionJournal() {
  aster::ConfigLayerStack config;
  config.addLayer(aster::parseConfigLayerText("defaults", R"cfg(
[render]
backend = "software"
samples = 1
tool.history = "on"
)cfg"));
  config.addLayer(aster::parseConfigLayerText("project", R"json(
{
  "render.samples": "4",
  "tools.audit": "strict"
}
)json",
                                              10u));
  const aster::ConfigResolution resolution = config.resolve();
  assert(resolution.values.at("render.backend") == "software");
  assert(resolution.values.at("render.samples") == "4");
  assert(config.get("tools.audit").value() == "strict");
  assert(config.explain("render.samples").find("project") != std::string::npos);
  assert(resolution.stamp != 0u);

  aster::SessionJournal journal({.max_bytes = 2000u});
  journal.appendCommand("studio", "open material lab", "asset-db");
  journal.appendCommand("studio", "inspect cook lineage", "material.cli");
  journal.append({.session_id = "assetc", .kind = "tool", .text = "catalog-audit"});
  journal.appendAssetProduction("pipe-lab",
                                {.asset_id = "asset_graph.pipe_lab.rusted_pipe",
                                 .graph_hash = "hash.graph",
                                 .preview_artifact_hash = "hash.preview",
                                 .quality_gate = "production-ready",
                                 .cook_steps = {"graph-inspect", "graph-package", "preview-render"}});
  assert(!journal.empty());
  assert(journal.contractStamp() != 0u);
  assert(journal.toJsonLines().find("\"session_id\":\"studio\"") != std::string::npos);
  assert(journal.toJsonLines().find("\"kind\":\"asset-production\"") != std::string::npos);
  assert(journal.toJsonLines().find("preview_artifact_hash=hash.preview") != std::string::npos);
  assert(journal.byteSize() <= 2000u);

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "aster_session_journal_core_test.jsonl";
  assert(journal.save(path));
  const aster::SessionJournal loaded = aster::SessionJournal::load(path, {.max_bytes = 2000u});
  assert(!loaded.empty());
  assert(loaded.entriesFor("studio").size() >= 1u);

  const aster::SessionDiagnosticSnapshot snapshot =
      aster::snapshotSessionDiagnostics(config, loaded);
  assert(snapshot.config_layers == 2u);
  assert(snapshot.config_values >= 3u);
  assert(snapshot.journal_entries == loaded.size());
  assert(snapshot.config_stamp == resolution.stamp);
  std::filesystem::remove(path);
}

} // namespace

int main() {
  testVectorMath();
  testVectorAliasesAndShaderHelpers();
  testSemanticMathBoundaries();
  testMatrixComposition();
  testMat3AndNormalMatrix();
  testMatrixInverseAndDeterminant();
  testTransformContract();
  testQuaternionTransformContract();
  testReverseZProjectionAndCamera();
  testColorPipeline();
  testFixedTimestep();
  testFrameTimeStats();
  testProfilerCaptureExport();
  testBudgetedWorkQueueContracts();
  testAsterCoreRuntimeContracts();
  testWorldStateTransitionContracts();
  testWorldPerceptionLedgerContracts();
  testPerceptualWorldRuntimeContracts();
  testWorldPerceptualPrimitiveContracts();
  testNeuralIrradianceVolumeContracts();
  testBeliefExtractionContracts();
  testSourceBoundaryContracts();
  testConfigLayerStackAndSessionJournal();
  std::cout << "core_tests passed.\n";
  return 0;
}
