// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include <cstdlib>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

void setEnvFlag(const char *name, const bool enabled) {
#if defined(_WIN32)
  (void)_putenv_s(name, enabled ? "1" : "");
#else
  if (enabled) {
    (void)setenv(name, "1", 1);
  } else {
    (void)unsetenv(name);
  }
#endif
}

void testLumenSceneCoherenceReport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::SceneCoherenceReport &report = run.sceneCoherenceReport();
  assert(!report.contributions.empty());
  assert(report.energy >= 0.0f);
  assert(findContribution(report, aster::SceneCoherenceTerm::FluidContainment) != nullptr);
  assert(findContribution(report, aster::SceneCoherenceTerm::RouteCollision) != nullptr);
  const aster::SceneTraceValidationReport &trace_report = run.sceneTraceReport();
  assert(trace_report.trace_length > 0u);
  assert(!trace_report.rules.empty());
  assert(trace_report.valid);
}

void testLumenCameraCollisionCanBeatComfortRadius() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0, .playable_radius = 86.0f});
  const float radius = run.resolveCameraRadius({0.0f, 1.0f, 85.0f}, 0.0f, 0.0f, 6.0f);
  assert(radius < 1.0f);
}

void testLumenInnerPondSeamHasSupport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const auto &objects = run.scene().objects();

  const auto planar_half_extents = [](const aster::RenderObject &object) {
    aster::Vec2 half_extents{};
    if (object.custom_mesh == nullptr) {
      return half_extents;
    }
    for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
      half_extents.x =
          std::max(half_extents.x, std::abs(vertex.position.x * object.transform.scale.x));
      half_extents.y =
          std::max(half_extents.y, std::abs(vertex.position.z * object.transform.scale.z));
    }
    return half_extents;
  };

  const aster::RenderObject *largest_grass_surface = nullptr;
  float largest_grass_area = 0.0f;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::GrassSoil) {
      continue;
    }
    const aster::Vec2 half_extents = planar_half_extents(object);
    const float area = half_extents.x * half_extents.y;
    if (area > largest_grass_area) {
      largest_grass_area = area;
      largest_grass_surface = &object;
    }
  }
  assert(largest_grass_surface != nullptr);

  const aster::Vec2 seam_center{largest_grass_surface->transform.position.x,
                                largest_grass_surface->transform.position.z};
  const aster::RenderObject *seam_transition = nullptr;
  float closest_transition_distance = std::numeric_limits<float>::max();
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::TerrainBlend) {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - seam_center.x,
                            object.transform.position.z - seam_center.y};
    const float distance = aster::length(delta);
    if (distance < closest_transition_distance) {
      closest_transition_distance = distance;
      seam_transition = &object;
    }
  }
  assert(seam_transition != nullptr);
  const aster::Vec2 seam_radius = planar_half_extents(*seam_transition);
  assert(seam_radius.x > 1.0f && seam_radius.y > 1.0f);

  std::vector<const aster::RenderObject *> support_surfaces;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr) {
      continue;
    }
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
      support_surfaces.push_back(&object);
      break;
    default:
      break;
    }
  }
  assert(!support_surfaces.empty());

  constexpr int angular_samples = 64;
  const float radial_samples[] = {0.84f, 0.95f, 1.06f};
  for (const float radial : radial_samples) {
    for (int i = 0; i < angular_samples; ++i) {
      const float angle =
          static_cast<float>(i) / static_cast<float>(angular_samples) * aster::radians(360.0f);
      const aster::Vec2 point{seam_center.x + std::cos(angle) * seam_radius.x * radial,
                              seam_center.y + std::sin(angle) * seam_radius.y * radial};
      aster::TerrainSurfaceSample best;
      for (const aster::RenderObject *surface : support_surfaces) {
        const aster::TerrainSurfaceSample sample = aster::sampleMeshSupport(
            *surface->custom_mesh, surface->transform, aster::SurfaceSupportQuery{point}, 0.25f);
        if (sample.valid && (!best.valid || sample.height > best.height)) {
          best = sample;
        }
      }
      assert(best.valid);
    }
  }
}

void testLumenSupportSurfacesRenderOpaque() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  bool saw_support_surface = false;

  for (const aster::RenderObject &object : run.scene().objects()) {
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
    case aster::SurfacePattern::SoilPath:
    case aster::SurfacePattern::LayeredTerrain:
      saw_support_surface = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(object.material.camera_occlusion == aster::CameraOcclusionPolicy::Solid);
      assert(!aster::isMaterialTranslucent(object.material));
      assert(aster::materialWritesDepth(object.material));
      assert(aster::isDoubleSidedMaterial(object.material));
      break;
    default:
      break;
    }
  }

  assert(saw_support_surface);
}

void testLumenSupplyCrateInventoryContract() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  assert(run.torchCount() == 0);
  assert(!run.supplyCrateNearby());
  assert(!run.takeSupplyTorch());

  const aster::Vec3 crate_position = run.supplyCratePosition();
  assert(crate_position.z < -40.0f);
  assert(aster::length({crate_position.x, 0.0f, crate_position.z}) > 45.0f);

  run.relocatePlayer(crate_position, 0.0f);
  assert(run.supplyCrateNearby());
  assert(run.takeSupplyTorch());
  assert(run.torchCount() == 1);
  assert(run.takeSupplyTorch());
  assert(run.torchCount() == 2);
}

void testLumenPrismRelayProximityInteraction() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const std::optional<aster::DynamicPointLight> idle_light = run.prismRelayLight();
  assert(idle_light.has_value());

  const aster::Vec3 base = run.prismRelayBasePosition();
  run.relocatePlayer(base + aster::Vec3{1.35f, 0.0f, 0.95f}, 0.0f);
  run.updateInteractionFocus({base.x, base.y + 8.0f, base.z}, {0.0f, 0.0f, 1.0f},
                             1.0f / 240.0f);
  const aster::FocusPromptModel prompt = run.focusPromptModel();
  assert(prompt.visible);
  assert(prompt.action == "Ignite");
  assert(prompt.subject == "Prism Relay");

  run.interactFocused();
  const std::optional<aster::DynamicPointLight> active_light = run.prismRelayLight();
  assert(active_light.has_value());
  assert(active_light->intensity > idle_light->intensity * 2.0f);
}

void testLumenCaveVisualContracts() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  run.relocatePlayer(run.supplyCratePosition(), 0.0f);
  for (int i = 0; i < 4; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  bool saw_cave_threshold = false;
  bool saw_cave_overhang = false;
  bool saw_cave_formation = false;
  bool saw_cave_liner = false;
  bool saw_cave_seal = false;
  bool saw_cave_throat = false;
  bool saw_cave_floor = false;
  bool saw_deep_cave_floor = false;
  bool saw_authored_cave = false;
  bool saw_deep_cave = false;
  bool saw_central_grass_field = false;
  bool saw_cave_mouth_grass_field = false;
  bool saw_cave_web = false;
  aster::Vec3 cave_web_center{};
  int cave_skitter_count = 0;
  bool saw_coal_ore = false;
  bool saw_wall_light_lens = false;
  const aster::RenderObject *cave_floor_object = nullptr;
  const aster::RenderObject *deep_cave_floor_object = nullptr;
  const aster::RenderObject *parkour_chest_base = nullptr;
  int coal_ore_count = 0;

  for (const aster::RenderObject &object : run.scene().objects()) {
    if (startsWith(object.name, "Cave ")) {
      assert(object.primitive != aster::MeshPrimitive::Crystal);
    }
    if (object.name == "Engine central terrain grass field") {
      saw_central_grass_field = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 12000u);
      assert(object.material.surface_pattern == aster::SurfacePattern::Foliage);
    }
    if (object.name == "Engine cave mouth grass field") {
      saw_cave_mouth_grass_field = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 10000u);
      assert(object.material.surface_pattern == aster::SurfacePattern::Foliage);
    }
    if (startsWith(object.name, "Cave torch supply crate")) {
      assert(false);
    }
    if (object.name == "Walkable cave entrance threshold") {
      saw_cave_threshold = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);

      aster::Vec3 center{};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        center = center + vertex.position;
      }
      center = center / static_cast<float>(object.custom_mesh->vertices.size());
      const aster::TerrainSurfaceSample support =
          aster::sampleMeshSupport(*object.custom_mesh, object.transform,
                                   aster::SurfaceSupportQuery{{center.x, center.z}}, 0.36f);
      assert(support.valid);
    }
    if (object.name == "Smooth terrain blended cave overhang") {
      saw_cave_overhang = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.position.x, 0.0f, object.transform.position.z}) >
             45.0f);
    }
    if (object.name == "Continuous procedural cave mouth formation") {
      saw_cave_formation = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 120u);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.material.procedural.micro_normal_strength > 0.0f);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.position.x, 0.0f, object.transform.position.z}) >
             45.0f);
    }
    if (object.name == "Opaque recessed cave mouth liner") {
      saw_cave_liner = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.transform.position.z < -40.0f);
    }
    if (object.name == "Opaque cave portal terrain seal") {
      saw_cave_seal = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(countFacesOpposingVertexNormals(*object.custom_mesh) == 0u);
    }
    if (object.name == "Sealed cave entrance throat") {
      saw_cave_throat = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(countFacesOpposingVertexNormals(*object.custom_mesh) == 0u);
    }
    if (object.name == "Walkable packed cave floor") {
      saw_cave_floor = true;
      cave_floor_object = &object;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
    }
    if (object.name == "Walkable deep cave floor") {
      saw_deep_cave_floor = true;
      deep_cave_floor_object = &object;
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
    }
    if (object.name == "Authored cave interior") {
      saw_authored_cave = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.viewer_cull_volume.enabled);
      assert(object.viewer_cull_volume.outside == aster::FaceCullMode::Back);
      assert(object.viewer_cull_volume.inside == aster::FaceCullMode::Back);
      assert(object.custom_mesh != nullptr);
      aster::Vec3 center{};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        center = center + vertex.position;
      }
      center = center / static_cast<float>(object.custom_mesh->vertices.size());
      assert(center.z < -40.0f);
      assert(aster::length({center.x, 0.0f, center.z}) > 45.0f);
    }
    if (object.name == "Authored deep cave interior") {
      saw_deep_cave = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      assert(!object.dynamic_mesh.valid());
      assert(!object.camera_occlusion_fade);
    }
    assert(object.name.find("Opaque cave void") == std::string::npos);
    assert(object.name.find("Opaque deep cave void") == std::string::npos);
    assert(object.name.find("connector roof seal") == std::string::npos);
    assert(object.name.find("connector crown blocker") == std::string::npos);
    if (object.name == "Continuous streaming cave connector shell" ||
        object.name == "Walkable streaming cave connector floor" ||
        object.name == "Chunked procedural cave interior" ||
        object.name == "Rock voxel cave surface" ||
        object.name == "Ironstone voxel cave surface") {
      assert(false);
    }
    if (object.name == "Oval cave spider web span") {
      saw_cave_web = true;
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      cave_web_center = {};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        cave_web_center = cave_web_center + vertex.position;
      }
      cave_web_center = cave_web_center / static_cast<float>(object.custom_mesh->vertices.size());
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveWeb);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Blend);
      assert(object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::None);
      assert(object.material.depth_policy.layer == aster::RenderDepthLayer::SurfaceAttachment);
      assert(object.material.depth_policy.constant_bias > 0.0f);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Cave skitter arachnid") {
      ++cave_skitter_count;
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveSkitterChitin);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(!object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.transform.position.z < -40.0f);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Coal ore vein node") {
      saw_coal_ore = true;
      ++coal_ore_count;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(!object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CoalVein);
      assert(object.material.emission_strength >= 0.10f);
      assert(object.primitive == aster::MeshPrimitive::Rock);
      assert(!object.viewer_cull_volume.enabled);
      assert(!object.camera_occlusion_fade);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.scale.x, object.transform.scale.y,
                            object.transform.scale.z}) > 0.30f);
    }
    if (object.name == "Industrial red cave wall light glowing lens") {
      saw_wall_light_lens = true;
      assert(object.material.surface_pattern == aster::SurfacePattern::AmberResin);
      assert(object.material.emission_strength > 0.60f);
      assert(object.material.emission_color.x > object.material.emission_color.z);
      assert(object.material.emission_color.y < object.material.emission_color.x * 0.35f);
      assert(object.material.emission_color.z < object.material.emission_color.x * 0.20f);
      assert(object.material.depth_policy.layer == aster::RenderDepthLayer::SurfaceAttachment);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Parkour starter chest base") {
      parkour_chest_base = &object;
    }
  }

  assert(saw_cave_threshold);
  assert(saw_cave_overhang);
  assert(saw_cave_formation);
  assert(saw_cave_liner);
  assert(saw_cave_seal);
  assert(saw_cave_throat);
  assert(saw_cave_floor);
  assert(saw_deep_cave_floor);
  assert(saw_authored_cave);
  assert(saw_deep_cave);
  assert(saw_cave_web);
  assert(cave_skitter_count == 3);
  assert(saw_central_grass_field);
  assert(saw_cave_mouth_grass_field);
  assert(saw_coal_ore);
  assert(coal_ore_count >= 4);
  assert(saw_wall_light_lens);
  assert(parkour_chest_base != nullptr);
  assert(cave_floor_object != nullptr);
  assert(cave_floor_object->custom_mesh != nullptr);
  const aster::TerrainSurfaceSample chest_entry_floor =
      aster::sampleMeshSupport(*cave_floor_object->custom_mesh, cave_floor_object->transform,
                               aster::SurfaceSupportQuery{{parkour_chest_base->transform.position.x,
                                                           parkour_chest_base->transform.position.z}},
                               0.46f);
  aster::TerrainSurfaceSample chest_deep_floor{};
  if (deep_cave_floor_object != nullptr && deep_cave_floor_object->custom_mesh != nullptr) {
    chest_deep_floor =
        aster::sampleMeshSupport(*deep_cave_floor_object->custom_mesh,
                                 deep_cave_floor_object->transform,
                                 aster::SurfaceSupportQuery{{parkour_chest_base->transform.position.x,
                                                             parkour_chest_base->transform.position.z}},
                                 0.46f);
  }
  float chest_floor_height = chest_entry_floor.valid ? chest_entry_floor.height : -1000.0f;
  if (chest_deep_floor.valid) {
    chest_floor_height = std::max(chest_floor_height, chest_deep_floor.height);
  }
  assert(chest_floor_height > -999.0f);
  assert(parkour_chest_base->transform.position.y - parkour_chest_base->transform.scale.y >=
         chest_floor_height - 0.015f);
}

void testLumenDeepCaveCaptureLightingContract() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const float progress = 16.0f;
  const aster::Vec3 player_position = run.caveFrameReportPosition(progress);
  run.relocatePlayer(player_position, 0.0f);
  for (int i = 0; i < 4; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }

  const aster::Vec3 look_target = run.caveFrameReportLookTarget(progress, 1.0f);
  const aster::CaveLightingState cave_light = run.caveLightingStateAt(look_target);
  assert(cave_light.interior > 0.30f);
  assert(cave_light.wall_light >= 0.0f && cave_light.wall_light <= 1.0f);
  assert(!cave_light.wall_lights.empty());

  aster::RendererSettings settings;
  settings.pipeline.clear_color = {0.005f, 0.005f, 0.005f};
  settings.exposure = 0.98f;
  settings.ambient_strength = 0.060f;
  settings.ambient_floor = 0.018f;
  settings.sky_ambient_color = {0.014f, 0.016f, 0.017f};
  settings.ground_ambient_color = {0.014f, 0.014f, 0.013f};
  settings.sun_light.enabled = true;
  settings.sun_light.intensity = 0.010f;
  settings.sun_light.direction_to_light = {-0.46f, 0.86f, 0.30f};
  settings.atmosphere.enabled = false;
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    settings.light_rig.push_back({light.position, light.color, light.intensity, light.source_radius});
  }

  aster::OrbitCamera camera;
  camera.target = look_target;
  camera.pitch = aster::radians(6.0f);
  camera.yaw = aster::radians(180.0f);
  camera.radius = run.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, 2.70f);
  camera.vertical_fov = aster::radians(54.0f);

  aster::RenderDevice renderer;
  assert(renderer.initialize());
  renderer.prepareScene(run.scene());
  (void)renderer.render(run.scene(), camera, settings, 96, 64, 0.0);

  const std::span<const std::uint8_t> rgba = aster::activeFrameBuffer().rgba8();
  double mean_luma = 0.0;
  std::size_t red_dominant_pixels = 0u;
  for (std::size_t i = 0; i + 3u < rgba.size(); i += 4u) {
    const double r = static_cast<double>(rgba[i + 0u]) / 255.0;
    const double g = static_cast<double>(rgba[i + 1u]) / 255.0;
    const double b = static_cast<double>(rgba[i + 2u]) / 255.0;
    mean_luma += r * 0.2126 + g * 0.7152 + b * 0.0722;
    if (rgba[i + 0u] > 54u && rgba[i + 0u] > rgba[i + 1u] * 2u &&
        rgba[i + 0u] > rgba[i + 2u] * 2u) {
      ++red_dominant_pixels;
    }
  }
  mean_luma /= static_cast<double>(std::max<std::size_t>(rgba.size() / 4u, 1u));
  assert(mean_luma < 0.28);
  assert(red_dominant_pixels > 8u);

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testLumenCaveTraversalAndLightingContracts() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  aster::Vec3 cave_web_center{};
  bool found_cave_web = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name != "Oval cave spider web span") {
      continue;
    }
    assert(object.custom_mesh != nullptr);
    for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
      cave_web_center = cave_web_center + vertex.position;
    }
    cave_web_center = cave_web_center / static_cast<float>(object.custom_mesh->vertices.size());
    found_cave_web = true;
    break;
  }
  assert(found_cave_web);
  const auto require_lumen_transition = [](const bool condition, const std::string &message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  };
  const aster::Vec3 web_approach_position =
      cave_web_center + aster::Vec3{0.0f, -0.10f, 2.35f};
  run.relocatePlayer(web_approach_position, aster::radians(180.0f));
  const int health_before_web = run.status().health;
  for (int i = 0; i < 90; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  assert(run.status().health == health_before_web);
  const aster::Vec3 focus_origin = web_approach_position + aster::Vec3{0.0f, 0.42f, 0.0f};
  require_lumen_transition(run.takeChestItem("pickaxe"),
                           "test setup failed to equip the pickaxe for cave web mining");
  run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                             1.0f / 60.0f);
  const aster::FocusPromptModel transition_prompt = run.focusPromptModel();
  require_lumen_transition(transition_prompt.visible, "cave transition web prompt is not visible");
  require_lumen_transition(transition_prompt.action == "Cut",
                           "cave transition should focus the web before attached skitters; got " +
                               transition_prompt.action + " " + transition_prompt.subject);
  require_lumen_transition(transition_prompt.subject == "Spider Web",
                           "cave transition should not focus rock before the web is cut");
  require_lumen_transition(!(transition_prompt.visible && transition_prompt.action == "Strike" &&
                             transition_prompt.subject == "Rock"),
                           "cave transition exposed a rock prompt before the web was cut");
  bool web_cut_prompt_seen = false;
  bool web_cleared = false;
  for (int swing = 0; swing < 8; ++swing) {
    run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                               1.0f / 60.0f);
    const aster::FocusPromptModel prompt = run.focusPromptModel();
    if (prompt.visible && prompt.action == "Cut" && prompt.subject == "Spider Web") {
      web_cut_prompt_seen = true;
    } else if (web_cut_prompt_seen) {
      web_cleared = true;
      break;
    }
    run.interactFocused();
    for (int frame = 0; frame < 40; ++frame) {
      run.update(1.0f / 60.0f, {}, false, false);
    }
  }
  run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                             1.0f / 60.0f);
  const aster::FocusPromptModel cleared_prompt = run.focusPromptModel();
  web_cleared = web_cleared || !(cleared_prompt.visible && cleared_prompt.action == "Cut" &&
                                 cleared_prompt.subject == "Spider Web");
  require_lumen_transition(web_cut_prompt_seen, "cave transition web was never interactable");
  require_lumen_transition(web_cleared, "cave transition web remained focused after mining");
  require_lumen_transition(!(cleared_prompt.visible && cleared_prompt.action == "Strike" &&
                             cleared_prompt.subject == "Rock"),
                           "cave transition exposed a rock prompt immediately after web mining");

  const aster::Vec3 traversal_target = run.caveFrameReportPosition(16.0f);
  const auto planar_distance_to_target = [&](const aster::Vec3 position) {
    const aster::Vec2 delta{traversal_target.x - position.x, traversal_target.z - position.z};
    return aster::length(delta);
  };
  const aster::Vec3 approach_to_target = aster::normalize(traversal_target - cave_web_center);
  run.relocatePlayer(web_approach_position, aster::radians(180.0f));
  const float initial_target_distance = planar_distance_to_target(run.playerPosition());
  for (int frame = 0; frame < 520; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{traversal_target.x - position.x, traversal_target.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 traversed_position = run.playerPosition();
  require_lumen_transition(planar_distance_to_target(traversed_position) <
                               initial_target_distance - 6.0f,
                           "player did not advance through the cleared cave connector");
  require_lumen_transition(aster::dot(traversed_position - cave_web_center, approach_to_target) >
                               4.0f,
                           "player remained on the authored side of the cave web");
  require_lumen_transition(traversed_position.z < cave_web_center.z - 4.0f,
                           "player did not move past the web into the authored deep cave");
  const auto planar_distance_to_approach = [&](const aster::Vec3 position) {
    const aster::Vec2 delta{web_approach_position.x - position.x,
                            web_approach_position.z - position.z};
    return aster::length(delta);
  };
  const float return_initial_distance = planar_distance_to_approach(traversed_position);
  for (int frame = 0; frame < 420; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{web_approach_position.x - position.x,
                          web_approach_position.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 returned_position = run.playerPosition();
  require_lumen_transition(planar_distance_to_approach(returned_position) <
                               return_initial_distance - 3.0f,
                           "player could not backtrack through the cave connector");
  const aster::CaveLightingState spawn_cave_light = run.caveLightingStateAt({0.0f, 0.32f, 0.0f});
  assert(spawn_cave_light.interior < 0.001f);
  assert(spawn_cave_light.entrance_light < 0.001f);
  for (const aster::CaveWallLightSample &light : spawn_cave_light.wall_lights) {
    assert(light.intensity <= 0.001f);
  }
  const aster::CaveLightingState cave_light = run.caveLightingState();
  assert(cave_light.interior > 0.20f);
  assert(!cave_light.wall_lights.empty());
  bool saw_red_fixture_light = false;
  aster::CaveWallLightSample red_fixture_light{};
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    const bool red_fixture_color =
        light.color.x > 0.90f && light.color.y < light.color.x * 0.35f &&
        light.color.z < light.color.x * 0.20f;
    if (red_fixture_color && light.intensity > 12.0f && light.source_radius > 0.0f &&
        light.source_radius <= 2.40f) {
      saw_red_fixture_light = true;
      red_fixture_light = light;
      break;
    }
  }
  assert(saw_red_fixture_light);
  assert(cave_light.wall_light >= 0.0f && cave_light.wall_light <= 1.0f);
  const aster::CaveLightingState near_fixture_light =
      run.caveLightingStateAt(red_fixture_light.position);
  assert(near_fixture_light.wall_light > spawn_cave_light.wall_light + 0.05f);

  run.reset();
  const aster::CaveLightingState reset_spawn_light = run.caveLightingState();
  assert(reset_spawn_light.interior < 0.001f);
  assert(reset_spawn_light.wall_light < 0.001f);
  assert(reset_spawn_light.wall_lights.empty());
}

void testLumenPondWallLightIsMountedOutsideWater() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::RenderObject *inner_water = nullptr;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Deep inner castle pond water") {
      inner_water = &object;
      break;
    }
  }
  assert(inner_water != nullptr);
  assert(inner_water->custom_mesh != nullptr);

  aster::Vec2 water_radius{};
  for (const aster::Vertex &vertex : inner_water->custom_mesh->vertices) {
    water_radius.x =
        std::max(water_radius.x, std::abs(vertex.position.x * inner_water->transform.scale.x));
    water_radius.y =
        std::max(water_radius.y, std::abs(vertex.position.z * inner_water->transform.scale.z));
  }
  assert(water_radius.x > 1.0f && water_radius.y > 1.0f);

  const aster::Vec2 water_center{inner_water->transform.position.x,
                                 inner_water->transform.position.z};
  bool saw_wall_fixture = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name != "Industrial red cave wall light glowing lens") {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - water_center.x,
                            object.transform.position.z - water_center.y};
    if (std::abs(delta.x) > water_radius.x + 4.0f || std::abs(delta.y) > water_radius.y + 4.0f) {
      continue;
    }
    saw_wall_fixture = true;
    const float normalized_footprint =
        std::sqrt((delta.x * delta.x) / (water_radius.x * water_radius.x) +
                  (delta.y * delta.y) / (water_radius.y * water_radius.y));
    assert(normalized_footprint > 1.18f);
    assert(object.transform.position.y > inner_water->transform.position.y + 0.70f);
  }
  assert(saw_wall_fixture);
}

} // namespace

int main() {
  testLumenSceneCoherenceReport();
  testLumenCameraCollisionCanBeatComfortRadius();
  testLumenInnerPondSeamHasSupport();
  testLumenSupportSurfacesRenderOpaque();
  testLumenSupplyCrateInventoryContract();
  testLumenPrismRelayProximityInteraction();
  testLumenCaveVisualContracts();
  testLumenDeepCaveCaptureLightingContract();
  testLumenCaveTraversalAndLightingContracts();
  testLumenPondWallLightIsMountedOutsideWater();
  std::cout << "sample_tests passed.\n";
  return 0;
}
