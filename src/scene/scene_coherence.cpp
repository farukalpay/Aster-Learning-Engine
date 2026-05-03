// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/scene_coherence.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace aster {
namespace {

constexpr float kEpsilon = 0.000001f;

Vec3 absVec(const Vec3 value) {
  return {std::abs(value.x), std::abs(value.y), std::abs(value.z)};
}

Vec3 maxVec3(const Vec3 lhs, const Vec3 rhs) {
  return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z)};
}

Vec3 safeHalfExtents(const SceneCoherenceVolume &volume) {
  return maxVec3(volume.half_extents, {});
}

float component(const Vec3 value, const int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

float safeWeight(const float weight) {
  return std::max(weight, 0.0f);
}

float totalWeight(const std::vector<SceneCoherenceSample> &samples) {
  float total = 0.0f;
  for (const SceneCoherenceSample &sample : samples) {
    total += safeWeight(sample.weight);
  }
  return total;
}

float nearestSampleDistance(const SceneCoherenceSample &sample,
                            const std::vector<SceneCoherenceSample> &targets) {
  float best = std::numeric_limits<float>::infinity();
  for (const SceneCoherenceSample &target : targets) {
    const float combined_radius = std::max(sample.radius, 0.0f) + std::max(target.radius, 0.0f);
    best =
        std::min(best, std::max(0.0f, length(sample.position - target.position) - combined_radius));
  }
  return best;
}

float weightedNearestMean(const std::vector<SceneCoherenceSample> &samples,
                          const std::vector<SceneCoherenceSample> &targets,
                          std::size_t &sample_count) {
  if (samples.empty()) {
    return 0.0f;
  }
  sample_count += samples.size();
  if (targets.empty()) {
    return totalWeight(samples);
  }

  float weighted_sum = 0.0f;
  float weight_sum = 0.0f;
  for (const SceneCoherenceSample &sample : samples) {
    const float weight = safeWeight(sample.weight);
    weighted_sum += nearestSampleDistance(sample, targets) * weight;
    weight_sum += weight;
  }
  return weight_sum > kEpsilon ? weighted_sum / weight_sum : 0.0f;
}

float bidirectionalMismatch(const SceneCoherenceSet &lhs, const SceneCoherenceSet &rhs,
                            std::size_t &sample_count) {
  if (lhs.samples.empty() && rhs.samples.empty()) {
    return 0.0f;
  }
  const float forward = weightedNearestMean(lhs.samples, rhs.samples, sample_count);
  const float backward = weightedNearestMean(rhs.samples, lhs.samples, sample_count);
  if (lhs.samples.empty() || rhs.samples.empty()) {
    return forward + backward;
  }
  return (forward + backward) * 0.5f;
}

float nearestVolumeDistance(const Vec3 point, const std::vector<SceneCoherenceVolume> &volumes) {
  float best = std::numeric_limits<float>::infinity();
  for (const SceneCoherenceVolume &volume : volumes) {
    best = std::min(best, signedDistanceToVolume(point, volume));
  }
  return best;
}

bool insideAny(const std::vector<SceneCoherenceVolume> &volumes, const Vec3 point) {
  for (const SceneCoherenceVolume &volume : volumes) {
    if (contains(volume, point)) {
      return true;
    }
  }
  return false;
}

bool blockedByAny(const std::vector<SceneCoherenceVolume> &volumes, const Vec3 from,
                  const Vec3 to) {
  for (const SceneCoherenceVolume &volume : volumes) {
    if (segmentIntersectsVolume(from, to, volume)) {
      return true;
    }
  }
  return false;
}

Vec3 normalizedOrZero(const Vec3 value) {
  const float len = length(value);
  return len > kEpsilon ? value / len : Vec3{};
}

float squaredLength(const Vec3 value) {
  return dot(value, value);
}

float materialDistanceSquared(const SceneMaterialFieldSample &lhs,
                              const SceneMaterialFieldSample &rhs) {
  const std::size_t count = std::max(lhs.weights.size(), rhs.weights.size());
  float sum = 0.0f;
  for (std::size_t i = 0; i < count; ++i) {
    const float a = i < lhs.weights.size() ? lhs.weights[i] : 0.0f;
    const float b = i < rhs.weights.size() ? rhs.weights[i] : 0.0f;
    const float delta = a - b;
    sum += delta * delta;
  }
  return sum;
}

void addContribution(SceneCoherenceReport &report, const SceneCoherenceTerm term, std::string label,
                     const float raw_value, const float weight, const std::size_t sample_count) {
  if (sample_count == 0u) {
    return;
  }
  const float weighted_value = raw_value * std::max(weight, 0.0f);
  report.contributions.push_back({term, std::move(label), raw_value, weighted_value, sample_count});
  report.energy += weighted_value;
}

void evaluateRouteCollision(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                            const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  std::size_t samples = 0u;
  for (const SceneCoherenceRoute &route : problem.routes) {
    for (const Vec3 point : route.points) {
      if (problem.solid_volumes.empty()) {
        continue;
      }
      const float sdf = nearestVolumeDistance(point, problem.solid_volumes);
      raw += std::max(0.0f, route.clearance - sdf);
      ++samples;
    }
  }
  addContribution(report, SceneCoherenceTerm::RouteCollision, "route collision clearance", raw,
                  weights.route_collision, samples);
}

void evaluateRouteContinuity(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                             const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  std::size_t samples = 0u;
  for (const SceneCoherenceRoute &route : problem.routes) {
    if (route.points.size() < 2u) {
      continue;
    }
    for (std::size_t i = 1u; i < route.points.size(); ++i) {
      const float segment_length = length(route.points[i] - route.points[i - 1u]);
      if (route.max_segment_length > 0.0f) {
        raw += std::max(0.0f, segment_length - route.max_segment_length);
      }
      ++samples;
    }
    if (route.support_tolerance > 0.0f && !problem.navigation.samples.empty()) {
      for (const Vec3 point : route.points) {
        const SceneCoherenceSample route_sample{route.label, point};
        raw += std::max(0.0f, nearestSampleDistance(route_sample, problem.navigation.samples) -
                                  route.support_tolerance);
        ++samples;
      }
    }
  }
  addContribution(report, SceneCoherenceTerm::RouteContinuity, "route continuity", raw,
                  weights.route_continuity, samples);
}

void evaluateAffordanceAlignment(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                                 const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  float weight_sum = 0.0f;
  std::size_t samples = 0u;
  for (const SceneAffordanceSample &sample : problem.affordance_samples) {
    const Vec3 delta =
        normalizedOrZero(sample.affordance_gradient) - normalizedOrZero(sample.goal_direction);
    const float weight = safeWeight(sample.weight);
    raw += squaredLength(delta) * weight;
    weight_sum += weight;
    ++samples;
  }
  addContribution(report, SceneCoherenceTerm::AffordanceAlignment, "affordance alignment",
                  weight_sum > kEpsilon ? raw / weight_sum : 0.0f, weights.affordance_alignment,
                  samples);
}

void evaluateMaterialContinuity(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                                const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  std::size_t samples = 0u;
  for (const SceneMaterialContinuityField &field : problem.material_fields) {
    if (field.neighbor_radius <= 0.0f) {
      continue;
    }
    for (std::size_t i = 0u; i < field.samples.size(); ++i) {
      for (std::size_t j = i + 1u; j < field.samples.size(); ++j) {
        const SceneMaterialFieldSample &a = field.samples[i];
        const SceneMaterialFieldSample &b = field.samples[j];
        const float distance = length(a.position - b.position);
        if (distance <= kEpsilon || distance > field.neighbor_radius) {
          continue;
        }
        const float edge_weight =
            (std::max(a.edge_weight, 0.0f) + std::max(b.edge_weight, 0.0f)) * 0.5f;
        const float gradient = std::sqrt(materialDistanceSquared(a, b)) / distance;
        raw += gradient * gradient * edge_weight * edge_weight;
        ++samples;
      }
    }
  }
  addContribution(report, SceneCoherenceTerm::MaterialContinuity, "material continuity", raw,
                  weights.material_continuity, samples);
}

void evaluateFluidContainment(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                              const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  std::size_t samples = 0u;
  for (const SceneCoherenceSample &sample : problem.ecological_samples) {
    const float weight = safeWeight(sample.weight);
    if (problem.fluid_volumes.empty()) {
      raw += weight;
      ++samples;
      continue;
    }
    const float sdf = nearestVolumeDistance(sample.position, problem.fluid_volumes);
    raw += std::max(0.0f, sdf - std::max(sample.radius, 0.0f)) * weight;
    ++samples;
  }
  for (const SceneCoherenceSegment &segment : problem.fluid_interaction_segments) {
    const float weight = safeWeight(segment.weight);
    bool intersects_fluid = false;
    for (const SceneCoherenceVolume &volume : problem.fluid_volumes) {
      intersects_fluid =
          intersects_fluid || segmentIntersectsVolume(segment.from, segment.to, volume);
    }
    raw += intersects_fluid ? 0.0f : weight;
    ++samples;
  }
  addContribution(report, SceneCoherenceTerm::FluidContainment, "fluid containment", raw,
                  weights.fluid_containment, samples);
}

void evaluateVisibilityLeak(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                            const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  std::size_t samples = 0u;
  for (const SceneVisibilityProbe &probe : problem.visibility_probes) {
    if (probe.allowed_regions.empty()) {
      continue;
    }
    if (!insideAny(probe.allowed_regions, probe.camera)) {
      raw += 1.0f;
      ++samples;
    }
    for (const Vec3 point : probe.visible_points) {
      if (insideAny(probe.allowed_regions, point)) {
        ++samples;
        continue;
      }
      if (!blockedByAny(probe.blockers, probe.camera, point)) {
        raw += 1.0f;
      }
      ++samples;
    }
  }
  addContribution(report, SceneCoherenceTerm::VisibilityLeak, "visibility outside allowed region",
                  raw, weights.visibility_leak, samples);
}

void evaluateLightConsistency(SceneCoherenceReport &report, const SceneCoherenceProblem &problem,
                              const SceneCoherenceWeights &weights) {
  float raw = 0.0f;
  float weight_sum = 0.0f;
  std::size_t samples = 0u;
  for (const SceneLightSample &sample : problem.light_samples) {
    const float weight = safeWeight(sample.weight);
    raw += squaredLength(sample.incoming_radiance - sample.expected_response) * weight;
    weight_sum += weight;
    ++samples;
  }
  addContribution(report, SceneCoherenceTerm::LightConsistency, "light material consistency",
                  weight_sum > kEpsilon ? raw / weight_sum : 0.0f, weights.light_consistency,
                  samples);
}

} // namespace

const char *sceneCoherenceTermName(const SceneCoherenceTerm term) {
  switch (term) {
  case SceneCoherenceTerm::RouteCollision:
    return "route_collision";
  case SceneCoherenceTerm::AffordanceAlignment:
    return "affordance_alignment";
  case SceneCoherenceTerm::MaterialContinuity:
    return "material_continuity";
  case SceneCoherenceTerm::FluidContainment:
    return "fluid_containment";
  case SceneCoherenceTerm::VisibilityLeak:
    return "visibility_leak";
  case SceneCoherenceTerm::RepresentationCollision:
    return "representation_collision";
  case SceneCoherenceTerm::NavigationCollision:
    return "navigation_collision";
  case SceneCoherenceTerm::RouteContinuity:
    return "route_continuity";
  case SceneCoherenceTerm::LightConsistency:
    return "light_consistency";
  }
  return "unknown";
}

float signedDistanceToVolume(const Vec3 point, const SceneCoherenceVolume &volume) {
  const Vec3 offset = absVec(point - volume.center) - safeHalfExtents(volume);
  const Vec3 outside = maxVec3(offset, {});
  const float inside = std::min(std::max({offset.x, offset.y, offset.z}), 0.0f);
  return length(outside) + inside;
}

bool contains(const SceneCoherenceVolume &volume, const Vec3 point) {
  return signedDistanceToVolume(point, volume) <= 0.0f;
}

bool segmentIntersectsVolume(const Vec3 from, const Vec3 to, const SceneCoherenceVolume &volume) {
  const Vec3 half_extents = safeHalfExtents(volume);
  const Vec3 box_min = volume.center - half_extents;
  const Vec3 box_max = volume.center + half_extents;
  const Vec3 direction = to - from;
  float t_min = 0.0f;
  float t_max = 1.0f;

  for (int axis = 0; axis < 3; ++axis) {
    const float origin_axis = component(from, axis);
    const float direction_axis = component(direction, axis);
    const float min_axis = component(box_min, axis);
    const float max_axis = component(box_max, axis);
    if (std::abs(direction_axis) <= kEpsilon) {
      if (origin_axis < min_axis || origin_axis > max_axis) {
        return false;
      }
      continue;
    }
    float near_t = (min_axis - origin_axis) / direction_axis;
    float far_t = (max_axis - origin_axis) / direction_axis;
    if (near_t > far_t) {
      std::swap(near_t, far_t);
    }
    t_min = std::max(t_min, near_t);
    t_max = std::min(t_max, far_t);
    if (t_min > t_max) {
      return false;
    }
  }
  return true;
}

SceneCoherenceReport evaluateSceneCoherence(const SceneCoherenceProblem &problem,
                                            const SceneCoherenceWeights weights) {
  SceneCoherenceReport report;

  std::size_t representation_samples = 0u;
  addContribution(report, SceneCoherenceTerm::RepresentationCollision,
                  "visual collision correspondence",
                  bidirectionalMismatch(problem.visual, problem.collision, representation_samples),
                  weights.representation_collision, representation_samples);

  std::size_t navigation_samples = 0u;
  addContribution(report, SceneCoherenceTerm::NavigationCollision,
                  "navigation collision correspondence",
                  bidirectionalMismatch(problem.navigation, problem.collision, navigation_samples),
                  weights.navigation_collision, navigation_samples);

  evaluateRouteCollision(report, problem, weights);
  evaluateRouteContinuity(report, problem, weights);
  evaluateAffordanceAlignment(report, problem, weights);
  evaluateMaterialContinuity(report, problem, weights);
  evaluateFluidContainment(report, problem, weights);
  evaluateVisibilityLeak(report, problem, weights);
  evaluateLightConsistency(report, problem, weights);

  return report;
}

} // namespace aster
