// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace aster {

struct SceneCoherenceVolume {
  std::string label;
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
};

struct SceneCoherenceSample {
  std::string label;
  Vec3 position{};
  float radius = 0.0f;
  float weight = 1.0f;
};

struct SceneCoherenceSegment {
  std::string label;
  Vec3 from{};
  Vec3 to{};
  float weight = 1.0f;
};

struct SceneCoherenceSet {
  std::string label;
  std::vector<SceneCoherenceSample> samples;
};

struct SceneCoherenceRoute {
  std::string label;
  std::vector<Vec3> points;
  float clearance = 0.0f;
  float max_segment_length = 0.0f;
  float support_tolerance = 0.0f;
};

struct SceneAffordanceSample {
  Vec3 position{};
  Vec3 affordance_gradient{};
  Vec3 goal_direction{};
  float weight = 1.0f;
};

struct SceneMaterialFieldSample {
  Vec3 position{};
  std::vector<float> weights;
  float edge_weight = 1.0f;
};

struct SceneMaterialContinuityField {
  std::string label;
  std::vector<SceneMaterialFieldSample> samples;
  float neighbor_radius = 0.0f;
};

struct SceneVisibilityProbe {
  std::string label;
  Vec3 camera{};
  std::vector<Vec3> visible_points;
  std::vector<SceneCoherenceVolume> allowed_regions;
  std::vector<SceneCoherenceVolume> blockers;
};

struct SceneLightSample {
  Vec3 position{};
  Vec3 incoming_radiance{};
  Vec3 expected_response{};
  float weight = 1.0f;
};

struct SceneCoherenceProblem {
  SceneCoherenceSet visual;
  SceneCoherenceSet collision;
  SceneCoherenceSet navigation;
  std::vector<SceneCoherenceVolume> solid_volumes;
  std::vector<SceneCoherenceVolume> fluid_volumes;
  std::vector<SceneCoherenceSample> ecological_samples;
  std::vector<SceneCoherenceSegment> fluid_interaction_segments;
  std::vector<SceneCoherenceRoute> routes;
  std::vector<SceneAffordanceSample> affordance_samples;
  std::vector<SceneMaterialContinuityField> material_fields;
  std::vector<SceneVisibilityProbe> visibility_probes;
  std::vector<SceneLightSample> light_samples;
};

struct SceneCoherenceWeights {
  float route_collision = 1.0f;
  float affordance_alignment = 1.0f;
  float material_continuity = 1.0f;
  float fluid_containment = 1.0f;
  float visibility_leak = 1.0f;
  float representation_collision = 1.0f;
  float navigation_collision = 1.0f;
  float route_continuity = 1.0f;
  float light_consistency = 1.0f;
};

enum class SceneCoherenceTerm {
  RouteCollision,
  AffordanceAlignment,
  MaterialContinuity,
  FluidContainment,
  VisibilityLeak,
  RepresentationCollision,
  NavigationCollision,
  RouteContinuity,
  LightConsistency,
};

struct SceneCoherenceContribution {
  SceneCoherenceTerm term = SceneCoherenceTerm::RepresentationCollision;
  std::string label;
  float raw_value = 0.0f;
  float weighted_value = 0.0f;
  std::size_t sample_count = 0;
};

struct SceneCoherenceReport {
  float energy = 0.0f;
  std::vector<SceneCoherenceContribution> contributions;
};

[[nodiscard]] const char *sceneCoherenceTermName(SceneCoherenceTerm term);
[[nodiscard]] float signedDistanceToVolume(Vec3 point, const SceneCoherenceVolume &volume);
[[nodiscard]] bool contains(const SceneCoherenceVolume &volume, Vec3 point);
[[nodiscard]] bool segmentIntersectsVolume(Vec3 from, Vec3 to, const SceneCoherenceVolume &volume);
[[nodiscard]] SceneCoherenceReport evaluateSceneCoherence(const SceneCoherenceProblem &problem,
                                                          SceneCoherenceWeights weights = {});

} // namespace aster
