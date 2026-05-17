// Author: Faruk Alpay
// Do not remove this notice.

#include "lumen_run_detail.hpp"

namespace aster {

// Scene coherence and symbolic trace validation.
SceneCoherenceProblem LumenRun::buildSceneCoherenceProblem() const {
  SceneCoherenceProblem problem;
  problem.visual.label = "lumen visual navigation representation";
  problem.collision.label = "lumen physical navigation representation";
  problem.navigation.label = "lumen navigable route representation";

  const auto addSurfaceSample = [&](const char *label, const Vec3 position, const float radius) {
    const SceneCoherenceSample sample{label, position, radius, 1.0f};
    problem.visual.samples.push_back(sample);
    problem.collision.samples.push_back(sample);
    problem.navigation.samples.push_back(sample);
  };

  constexpr int route_sample_count = 18;
  constexpr float route_clearance = 0.18f;
  constexpr float route_max_segment_length = 1.45f;
  constexpr float route_support_tolerance = 0.24f;
  SceneMaterialContinuityField route_material;
  route_material.label = "lumen route material field";
  route_material.neighbor_radius = route_max_segment_length * 1.15f;

  const auto addRoute = [&](const char *label, const PathRibbonMeshSpec &spec) {
    SceneCoherenceRoute route;
    route.label = label;
    route.clearance = route_clearance;
    route.max_segment_length = route_max_segment_length;
    route.support_tolerance = route_support_tolerance;
    route.points.reserve(route_sample_count);
    for (int i = 0; i < route_sample_count; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(route_sample_count - 1);
      Vec3 point = evaluatePathRibbonCenter(spec, t);
      const TerrainSurfaceSample ground = support_surfaces_.sample(Vec2{point.x, point.z});
      if (ground.valid) {
        point.y = ground.height;
      }
      route.points.push_back(point);
      addSurfaceSample(label, point, spec.width * 0.5f);
      const Vec3 tangent = evaluatePathRibbonTangent(spec, t);
      problem.affordance_samples.push_back({point, tangent, tangent, 1.0f});
      route_material.samples.push_back({point, {1.0f}, 1.0f});
    }
    problem.routes.push_back(std::move(route));
  };

  addRoute("underpass path", castleUnderpassPathSpec());
  addRoute("nest path", castleNestPathSpec());
  addRoute("pond path",
           naturePathSpec(
               plazaWetlandClips(pond_center_, inner_pond_center_, inner_pond_radius_).front()));
  int cave_route_index = 0;
  for (const PathRibbonMeshSpec &segment : caveApproachPathSpecs()) {
    const std::string route_label = "cave path " + std::to_string(++cave_route_index);
    addRoute(route_label.c_str(), segment);
  }
  for (std::size_t section_index = 0; section_index < cave_sections_.size(); ++section_index) {
    const CaveTunnelProfile &tunnel = cave_sections_[section_index].tunnel;
    const std::string label =
        section_index == 0u ? "cave passage" : "deep cave passage " + std::to_string(section_index);
    addRoute(label.c_str(), {.segments = 64,
                             .width = tunnel.floor_width,
                             .width_variation = 0.0f,
                             .crown_height = tunnel.floor_crown,
                             .surface_noise = 0.0f,
                             .endpoint_taper = 0.08f,
                             .start = tunnel.start,
                             .control = tunnel.control,
                             .control_b = tunnel.control_b,
                             .end = tunnel.end});
  }
  problem.material_fields.push_back(std::move(route_material));

  const auto addSolid = [&](const char *label, const Vec3 center, const Vec3 half_extents) {
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f) {
      return;
    }
    problem.solid_volumes.push_back({label, center, half_extents});
  };

  const CastleCourse &course = castleCourse();
  const Vec3 castle_origin = castleOrigin();
  for (const VoxelCollisionBox &box : course.structure.collision_boxes) {
    addSolid("castle voxel collision", castle_origin + box.center, box.half_extents);
  }
  for (const CastleCourseBoxElement &element : course.box_elements) {
    if (element.collidable) {
      addSolid("castle authored collision", castle_origin + element.center, element.half_extents);
    }
  }
  for (const UnderpassPortalPlacement &portal : gothicUnderpassPortals()) {
    for (const ArchitecturalCollisionBox &box : makeGothicUnderpassCollisionBoxes()) {
      addSolid("underpass collision",
               {portal.position.x + box.center.x * portal.scale.x,
                portal.position.y + box.center.y * portal.scale.y,
                portal.position.z + box.center.z * portal.scale.z},
               {box.half_extents.x * portal.scale.x, box.half_extents.y * portal.scale.y,
                box.half_extents.z * portal.scale.z});
    }
  }

  const float boundary = std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  addSolid("playable north boundary", {0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f});
  addSolid("playable south boundary", {0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f});
  addSolid("playable east boundary", {boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addSolid("playable west boundary", {-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary});
  addSolid("cave chest collision", chest_base_ + Vec3{0.0f, 0.25f, 0.0f}, {0.40f, 0.25f, 0.30f});
  addSolid("climbable trunk collision",
           climbable_tree_.base + Vec3{0.0f, climbable_tree_.height * 0.5f, 0.0f},
           {climbable_tree_.radius * 0.82f, climbable_tree_.height * 0.5f,
            climbable_tree_.radius * 0.82f});
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      addSolid("coal ore collision", ore.position, ore.scale * 0.54f);
    }
  }

  problem.fluid_volumes.push_back(
      {"courtyard pond volume",
       pond_center_ + Vec3{0.0f, kCourtyardPondCenterOffsetY, 0.0f},
       {kCourtyardPondRadius.x, kCourtyardPondHalfDepth, kCourtyardPondRadius.y}});
  problem.fluid_volumes.push_back(
      {"inner pond volume",
       inner_pond_center_ + Vec3{0.0f, kInnerPondCenterOffsetY, 0.0f},
       {inner_pond_radius_.x, kInnerPondHalfDepth, inner_pond_radius_.y}});

  for (const AquaticCreature &fish : fish_) {
    Vec3 sample_position = fish.center;
    if (fish.object_index < scene_.objects().size()) {
      sample_position = scene_.objects()[fish.object_index].transform.position;
    }
    problem.ecological_samples.push_back({"aquatic creature", sample_position, 0.06f, 1.0f});
  }
  if (fishing_float_object_ < scene_.objects().size()) {
    problem.ecological_samples.push_back(
        {"floating rig contact", scene_.objects()[fishing_float_object_].transform.position, 0.04f,
         1.0f});
  }
  problem.fluid_interaction_segments.push_back(
      {"fishing line", fishing_rod_tip_,
       fishing_float_object_ < scene_.objects().size()
           ? scene_.objects()[fishing_float_object_].transform.position
           : fishing_float_rest_,
       1.0f});

  SceneVisibilityProbe camera_probe;
  camera_probe.label = "playable camera visibility";
  camera_probe.camera = player_position_ + Vec3{0.0f, 1.05f, 0.0f};
  camera_probe.allowed_regions.push_back(
      {"playable interior", {0.0f, 1.0f, 0.0f}, {boundary, 3.0f, boundary}});
  camera_probe.blockers.push_back(
      {"north boundary blocker", {0.0f, 1.0f, boundary}, {boundary, 2.0f, 0.30f}});
  camera_probe.blockers.push_back(
      {"south boundary blocker", {0.0f, 1.0f, -boundary}, {boundary, 2.0f, 0.30f}});
  camera_probe.blockers.push_back(
      {"east boundary blocker", {boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary}});
  camera_probe.blockers.push_back(
      {"west boundary blocker", {-boundary, 1.0f, 0.0f}, {0.30f, 2.0f, boundary}});
  camera_probe.visible_points = {{0.0f, 1.0f, boundary + 2.0f},
                                 {0.0f, 1.0f, -boundary - 2.0f},
                                 {boundary + 2.0f, 1.0f, 0.0f},
                                 {-boundary - 2.0f, 1.0f, 0.0f}};
  problem.visibility_probes.push_back(std::move(camera_probe));

  return problem;
}

std::vector<SceneTraceRule> LumenRun::sceneTraceRules() const {
  return {
      forbidTraceSymbol("no-wall-leak.camera-inside", kTraceCameraInsideWall),
      forbidTraceSymbol("no-wall-leak.outside-visible", kTraceOutsideVisible),
      forbidTraceSymbol("collision.no-mesh-penetration", kTraceMeshPenetration),
      forbidTraceSymbol("path.no-local-cut", kTracePathCut),
      requireTraceSymbolWithin("path.continuity", kTracePathVisible, kTracePathContinues,
                               kSceneTracePathContinuityHorizon),
      requireTraceSymbolSameFrame("water.fish-embedding", kTraceFishVisible, kTraceFishInsideWater),
      forbidTraceSymbol("water.no-fish-misembedded", kTraceFishMisembedded),
      forbidTraceSymbol("water.no-fish-on-surface", kTraceFishOnSurface),
      forbidTraceSymbol("props.fishing-support-on-shore", kTraceFishingSupportWet),
      requireTraceSymbolSameFrame("affordance.reward-reachable", kTraceRewardVisible,
                                  kTraceRewardReachable),
      forbidTraceSymbol("affordance.no-false-reward", kTraceFalseRewardAffordance),
      requireTraceSymbolSameFrame("affordance.threat-readable", kTraceThreatVisible,
                                  kTraceThreatReadable),
  };
}

SceneSymbolicTrace LumenRun::buildSceneSymbolicTrace() const {
  const SceneCoherenceProblem problem = buildSceneCoherenceProblem();
  SceneSymbolicTrace trace;

  const auto pushFrame = [&trace]() -> SceneTraceFrame & {
    SceneTraceFrame frame;
    frame.time_seconds = static_cast<double>(trace.frames.size()) / kSceneTraceCheckpointRate;
    trace.frames.push_back(std::move(frame));
    return trace.frames.back();
  };

  const auto supported = [this](const Vec3 point, const float tolerance) {
    const TerrainSurfaceSample support = support_surfaces_.sample(Vec2{point.x, point.z});
    return support.valid && std::abs(support.height - point.y) <= tolerance;
  };

  for (const SceneCoherenceRoute &route : problem.routes) {
    if (route.points.size() < 2u) {
      continue;
    }
    for (std::size_t i = 0u; i + 1u < route.points.size(); ++i) {
      const Vec3 point = route.points[i];
      const Vec3 next = route.points[i + 1u];
      const float support_tolerance =
          route.support_tolerance > 0.0f ? route.support_tolerance : 0.28f;
      const bool point_supported = supported(point, support_tolerance);
      const bool next_supported = supported(next, support_tolerance);
      const bool blocked = insideAnySceneVolume(problem.solid_volumes, point);
      const bool next_blocked = insideAnySceneVolume(problem.solid_volumes, next);
      const bool continuous =
          (route.max_segment_length <= 0.0f ||
           length(next - point) <= route.max_segment_length + support_tolerance) &&
          point_supported && next_supported && !blocked && !next_blocked;

      SceneTraceFrame &frame = pushFrame();
      addTraceSymbol(frame, kTracePathVisible);
      if (blocked || !point_supported) {
        addTraceSymbol(frame, kTraceBlocked);
      } else {
        addTraceSymbol(frame, kTraceWalkable);
      }
      if (blocked) {
        addTraceSymbol(frame, kTraceMeshPenetration);
      }
      addTraceSymbol(frame, continuous ? kTracePathContinues : kTracePathCut);
    }
  }

  for (const SceneVisibilityProbe &probe : problem.visibility_probes) {
    SceneTraceFrame &frame = pushFrame();
    if (insideAnySceneVolume(problem.solid_volumes, probe.camera)) {
      addTraceSymbol(frame, kTraceCameraInsideWall);
    }
    for (const Vec3 point : probe.visible_points) {
      const bool allowed = insideAnySceneVolume(probe.allowed_regions, point);
      const bool blocked = blockedByAnySceneVolume(probe.blockers, probe.camera, point);
      if (!allowed && !blocked) {
        addTraceSymbol(frame, kTraceOutsideVisible);
      }
    }
  }

  const auto fluidSurfaceForPoint = [this](const Vec3 point, float &surface_y) {
    if (insideEllipticalFootprint(point, pond_center_, kCourtyardPondRadius,
                                  kSceneTraceWaterFootprintScale)) {
      surface_y = kCourtyardPondSurfaceY;
      return true;
    }
    if (insideEllipticalFootprint(point, inner_pond_center_, inner_pond_radius_,
                                  kSceneTraceWaterFootprintScale)) {
      surface_y = kInnerPondSurfaceY;
      return true;
    }
    return false;
  };

  for (const AquaticCreature &fish : fish_) {
    Vec3 position = fish.center;
    if (fish.object_index < scene_.objects().size()) {
      position = scene_.objects()[fish.object_index].transform.position;
    }
    const bool inside_fluid = insideAnySceneVolume(problem.fluid_volumes, position);
    float surface_y = 0.0f;
    const bool has_surface = fluidSurfaceForPoint(position, surface_y);

    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceFishVisible);
    addTraceSymbol(frame, inside_fluid ? kTraceFishInsideWater : kTraceFishMisembedded);
    if (!inside_fluid && has_surface &&
        std::abs(position.y - surface_y) <= kSceneTraceFishSurfaceTolerance) {
      addTraceSymbol(frame, kTraceFishOnSurface);
    }
  }

  {
    const bool support_in_water =
        insideEllipticalFootprint(fishing_rod_base_, pond_center_, kCourtyardPondRadius, 1.0f) ||
        insideEllipticalFootprint(fishing_rod_base_, inner_pond_center_, inner_pond_radius_, 1.0f);
    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, support_in_water ? kTraceFishingSupportWet : kTraceFishingSupportDry);
  }

  for (const Shard &shard : shards_) {
    if (shard.collected) {
      continue;
    }
    const TerrainSurfaceSample support =
        support_surfaces_.sample(Vec2{shard.position.x, shard.position.z});
    const bool reachable =
        support.valid && !insideAnySceneVolume(problem.solid_volumes, shard.position);

    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceRewardVisible);
    addTraceSymbol(frame, reachable ? kTraceRewardReachable : kTraceFalseRewardAffordance);
  }

  for (const Sentinel &sentinel : sentinels_) {
    SceneTraceFrame &frame = pushFrame();
    addTraceSymbol(frame, kTraceThreatVisible);
    if (sentinel.object_index < scene_.objects().size()) {
      addTraceSymbol(frame, kTraceThreatReadable);
    }
  }

  return trace;
}

void LumenRun::invalidateSceneReports() {
  scene_coherence_dirty_ = true;
  scene_trace_dirty_ = true;
}

void LumenRun::rebuildSceneCoherenceReport() const {
  scene_coherence_ = evaluateSceneCoherence(buildSceneCoherenceProblem());
  scene_coherence_dirty_ = false;
}

void LumenRun::rebuildSceneTraceReport() const {
  scene_trace_ = validateSceneTrace(buildSceneSymbolicTrace(), sceneTraceRules());
  scene_trace_dirty_ = false;
}

} // namespace aster
