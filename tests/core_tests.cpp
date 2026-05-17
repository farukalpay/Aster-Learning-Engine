// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

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

void testMatrixInverseAndDeterminant() {
  const aster::Mat4 transform = aster::translation({3.0f, -2.0f, 5.0f}) *
                                aster::rotation_y(aster::radians(28.0f)) *
                                aster::scale({1.5f, 2.0f, 0.75f});
  expectNear(aster::determinant(transform), 2.25f, 0.0005f);

  const aster::Mat4 round_trip = transform * aster::inverse(transform);
  const aster::Mat4 identity = aster::identity();
  for (std::size_t i = 0; i < round_trip.m.size(); ++i) {
    expectNear(round_trip.m[i], identity.m[i], 0.001f);
  }

  const aster::Vec3 point{0.25f, 0.5f, -0.75f};
  const aster::Vec3 restored =
      aster::transformPoint(aster::inverse(transform), aster::transformPoint(transform, point));
  expectNear(restored.x, point.x, 0.001f);
  expectNear(restored.y, point.y, 0.001f);
  expectNear(restored.z, point.z, 0.001f);
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

void testColorPipeline() {
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

} // namespace

int main() {
  testVectorMath();
  testMatrixComposition();
  testMatrixInverseAndDeterminant();
  testTransformContract();
  testColorPipeline();
  testFixedTimestep();
  testFrameTimeStats();
  testProfilerCaptureExport();
  testBudgetedWorkQueueContracts();
  testSourceBoundaryContracts();
  std::cout << "core_tests passed.\n";
  return 0;
}
