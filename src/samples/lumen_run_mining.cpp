// Author: Faruk Alpay
// Do not remove this notice.

#include "lumen_run_detail.hpp"

namespace aster {

// Placement, mining, resource storage, and fracture effects.
PhysicsBodyHandle LumenRun::addPlacedRockPhysics(const PlacedResourceRock &rock) {
  if (rock.collision_half_extents.x <= 0.0f || rock.collision_half_extents.y <= 0.0f ||
      rock.collision_half_extents.z <= 0.0f) {
    return {};
  }
  PhysicsBodyDesc body;
  body.type = PhysicsBodyType::Static;
  body.shape = PhysicsShapeType::Box;
  body.position = rock.position;
  body.half_extents = rock.collision_half_extents;
  body.material = {0.76f, 0.0f};
  body.filter = {kPhysicsLayerWorld, kPhysicsLayerPlayer};
  return physics_.addBody(body);
}

bool LumenRun::storeMinedResource(const ItemDefinition &definition, const int quantity) {
  if (quantity <= 0) {
    return true;
  }
  return hotbar_.addItem(definition, quantity).has_value();
}

MiningToolStats LumenRun::activePickaxeStats() const {
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *definition = item_registry_.find(equipped.item_id);
  if (definition != nullptr && definition->has_mining_tool) {
    return definition->mining_tool;
  }
  return starterPickaxeStats();
}

void LumenRun::spawnMiningFractureEffect(const Vec3 center, Vec3 normal, Vec3 half_extents,
                                         const Material &material, const std::uint32_t seed,
                                         const int shard_count) {
  if (mining_fracture_shards_.empty()) {
    return;
  }
  normal = length(normal) > 0.0001f ? normalize(normal) : Vec3{0.0f, 1.0f, 0.0f};
  half_extents = {std::max(half_extents.x, 0.035f), std::max(half_extents.y, 0.035f),
                  std::max(half_extents.z, 0.035f)};

  Vec3 axis_y = std::abs(normal.y) < 0.94f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  Vec3 axis_x = normalize(cross(axis_y, normal));
  if (length(axis_x) <= 0.0001f) {
    axis_x = {1.0f, 0.0f, 0.0f};
  }
  axis_y = normalize(cross(normal, axis_x));

  const int requested_shards =
      std::clamp(shard_count, 2, static_cast<int>(mining_fracture_shards_.size()));
  MeshCutImpactSpec cut;
  cut.volume = {.center = center,
                .half_extents = half_extents,
                .axis_x = axis_x,
                .axis_y = axis_y,
                .axis_z = normal};
  cut.impact_point = center + normal * half_extents.z;
  cut.impact_normal = normal;
  cut.seed = seed;
  cut.plane_count =
      std::clamp(static_cast<int>(std::ceil(std::log2(static_cast<float>(requested_shards)))),
                 2, 5);
  cut.angular_spread = 0.54f;
  cut.offset_spread = 0.18f;
  cut.minimum_fragment_area = 0.00004f;

  MeshCutResult cut_result = buildImpactMeshCut(cut);
  std::vector<MeshCutFragment> fragments = std::move(cut_result.fragments);
  if (fragments.empty()) {
    return;
  }
  if (fragments.size() > static_cast<std::size_t>(requested_shards)) {
    std::sort(fragments.begin(), fragments.end(),
              [](const MeshCutFragment &lhs, const MeshCutFragment &rhs) {
                return lhs.surface_area > rhs.surface_area;
              });
    fragments.resize(static_cast<std::size_t>(requested_shards));
  }
  float total_area = 0.0f;
  for (const MeshCutFragment &fragment : fragments) {
    total_area += std::max(fragment.surface_area, 0.0f);
  }

  const auto acquireSlot = [&]() -> MiningFractureShardVisual * {
    for (MiningFractureShardVisual &visual : mining_fracture_shards_) {
      if (!visual.active) {
        return &visual;
      }
    }
    return &*std::max_element(mining_fracture_shards_.begin(), mining_fracture_shards_.end(),
                              [](const MiningFractureShardVisual &lhs,
                                 const MiningFractureShardVisual &rhs) {
                                return lhs.age < rhs.age;
                              });
  };

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  for (std::size_t i = 0; i < fragments.size(); ++i) {
    MiningFractureShardVisual *visual = acquireSlot();
    if (visual == nullptr || visual->object_index >= scene_.objects().size()) {
      continue;
    }
    CpuMesh local_mesh = std::move(fragments[i].mesh);
    for (Vertex &vertex : local_mesh.vertices) {
      vertex.position = vertex.position - fragments[i].centroid;
    }

    const float fill =
        fragments.size() <= 1u ? 0.0f
                               : static_cast<float>(i) / static_cast<float>(fragments.size() - 1u);
    const float area_fraction =
        total_area > 0.0001f ? fragments[i].surface_area / total_area : 0.0f;
    const float volume_factor = std::clamp(1.0f - area_fraction, 0.35f, 1.0f);
    const Vec3 tangent_kick =
        axis_x * std::cos(golden_angle * static_cast<float>(i)) +
        axis_y * std::sin(golden_angle * static_cast<float>(i));

    visual->active = true;
    visual->position = fragments[i].centroid;
    visual->velocity = fragments[i].impulse_direction * (0.78f + volume_factor * 0.72f) +
                       tangent_kick * (0.18f + fill * 0.18f) + normal * 0.18f;
    visual->rotation = {fill * 0.7f, fill * 1.3f, fill * 0.5f};
    visual->angular_velocity =
        {1.8f + fill * 2.0f, 2.4f + volume_factor * 2.3f, 1.2f + fill * 1.7f};
    visual->age = 0.0f;
    visual->lifetime = 0.68f + volume_factor * 0.32f;
    visual->base_scale = 1.0f;

    RenderObject &object = scene_.objects()[visual->object_index];
    object.name = "Mesh-cut mining fragment";
    object.primitive = MeshPrimitive::Box;
    object.custom_mesh = makeSharedMesh(std::move(local_mesh));
    object.transform.position = visual->position;
    object.transform.rotation = quatFromEulerXyz(visual->rotation);
    object.transform.scale = {1.0f, 1.0f, 1.0f};
    object.material = material;
    object.material.edge_wear = std::max(object.material.edge_wear, 0.34f + fill * 0.18f);
    object.material.pattern_depth = std::max(object.material.pattern_depth, 0.16f);
    object.material.pattern_contrast = std::max(object.material.pattern_contrast, 0.30f);
    object.material.emission_color = object.material.emission_strength > 0.0f
                                         ? object.material.emission_color
                                         : Vec3{0.54f, 0.38f, 0.24f};
    object.material.emission_strength =
        std::max(object.material.emission_strength, 0.070f + volume_factor * 0.035f);
    object.camera_occlusion_fade = false;
    object.casts_contact_shadow = false;
  }
}

bool LumenRun::placeEquippedResource(const Vec3 ray_origin, Vec3 ray_direction) {
  const ItemStack *selected = hotbar_.selectedStack();
  if (selected == nullptr || selected->empty()) {
    return false;
  }
  const ItemDefinition *definition = item_registry_.find(selected->item_id);
  if (definition == nullptr || !definition->placeable ||
      selected->quantity < std::max(definition->placement_cost, 1)) {
    return false;
  }

  ray_direction = normalize(ray_direction);
  if (length(ray_direction) <= 0.0001f) {
    return false;
  }

  const float reach = std::max(definition->placement_reach, 0.1f);
  Vec3 hit_point{};
  Vec3 hit_normal{0.0f, 1.0f, 0.0f};
  float hit_distance = std::numeric_limits<float>::infinity();
  bool hit = false;
  const auto considerHit = [&](const Vec3 point, Vec3 normal, const float distance) {
    if (distance < 0.0f || distance > reach || (hit && distance >= hit_distance - 0.025f)) {
      return;
    }
    if (length(normal) <= 0.0001f) {
      normal = {0.0f, 1.0f, 0.0f};
    }
    hit_point = point;
    hit_normal = normalize(normal);
    hit_distance = distance;
    hit = true;
  };

  PhysicsRayHit physics_hit;
  PhysicsRay ray;
  ray.origin = ray_origin;
  ray.direction = ray_direction;
  ray.max_distance = reach;
  ray.filter.collides_with = kPhysicsLayerWorld;
  ray.filter.ignore_body = player_body_;
  if (physics_.raycast(ray, physics_hit)) {
    considerHit(physics_hit.point, physics_hit.normal, physics_hit.distance);
  }

  constexpr int kSurfaceRaySteps = 48;
  bool previous_valid = false;
  float previous_signed_height = 0.0f;
  float previous_t = 0.0f;
  TerrainSurfaceSample previous_sample;
  for (int i = 1; i <= kSurfaceRaySteps; ++i) {
    const float t = reach * static_cast<float>(i) / static_cast<float>(kSurfaceRaySteps);
    const Vec3 probe = ray_origin + ray_direction * t;
    const TerrainSurfaceSample sample =
        sampleWorldSupport({{probe.x, probe.z}, probe.y, 0.34f, 2.0f});
    if (!sample.valid) {
      previous_valid = false;
      continue;
    }
    const float signed_height = probe.y - sample.height;
    if (previous_valid && previous_signed_height >= 0.0f && signed_height <= 0.0f) {
      float lo = previous_t;
      float hi = t;
      TerrainSurfaceSample hit_sample = sample;
      for (int step = 0; step < 8; ++step) {
        const float mid = (lo + hi) * 0.5f;
        const Vec3 mid_probe = ray_origin + ray_direction * mid;
        const TerrainSurfaceSample mid_sample =
            sampleWorldSupport({{mid_probe.x, mid_probe.z}, mid_probe.y, 0.34f, 2.0f});
        if (!mid_sample.valid) {
          lo = mid;
          continue;
        }
        const float mid_signed_height = mid_probe.y - mid_sample.height;
        if (mid_signed_height > 0.0f) {
          lo = mid;
        } else {
          hi = mid;
          hit_sample = mid_sample;
        }
      }
      const Vec3 surface_probe = ray_origin + ray_direction * hi;
      considerHit({surface_probe.x, hit_sample.height, surface_probe.z}, hit_sample.normal, hi);
      break;
    }
    previous_valid = true;
    previous_signed_height = signed_height;
    previous_t = t;
    previous_sample = sample;
  }
  (void)previous_sample;

  if (!hit) {
    return false;
  }

  const Vec3 half_extents = {std::max(definition->placement_collision_half_extents.x, 0.04f),
                             std::max(definition->placement_collision_half_extents.y, 0.04f),
                             std::max(definition->placement_collision_half_extents.z, 0.04f)};
  const float normal_clearance = std::max({half_extents.x, half_extents.y, half_extents.z});
  const Vec3 center = hit_point + hit_normal * (normal_clearance + 0.018f);
  const float player_clearance =
      tuning_.player_radius + std::max({half_extents.x, half_extents.z}) + 0.10f;
  if (length(center - player_position_) < player_clearance) {
    return false;
  }

  const float yaw_seed =
      std::sin(center.x * 12.9898f + center.y * 37.719f + center.z * 78.233f) * 43758.5453f;
  const float yaw = (yaw_seed - std::floor(yaw_seed)) * kPi * 2.0f;
  PlacedResourceRock rock;
  rock.id = placedResourceId(placed_resource_serial_++);
  rock.item_id = definition->id;
  rock.placement_shape = definition->placement_shape;
  rock.position = center;
  rock.normal = hit_normal;
  rock.rotation = {0.0f, yaw, 0.0f};
  rock.scale = definition->placement_scale;
  rock.collision_half_extents = half_extents;

  RenderObject object;
  object.name = "Placed cave resource rock";
  object.primitive = placementPrimitiveFor(definition->placement_shape);
  object.transform.position = rock.position;
  object.transform.rotation = quatFromEulerXyz(rock.rotation);
  object.transform.scale = rock.scale;
  object.material = lumenPlacedResourceMaterial(definition);
  object.camera_occlusion_fade = false;
  object.casts_contact_shadow = true;
  object.contact_shadow_strength = 0.18f;
  object.contact_shadow_radius_scale = 0.78f;
  rock.object_index = appendObject(object);
  scenery_objects_.push_back(rock.object_index);
  support_surfaces_.addBox({rock.position, rock.collision_half_extents});
  rock.body = addPlacedRockPhysics(rock);
  placed_rocks_.push_back(rock);

  (void)hotbar_.removeSelectedItem(std::max(definition->placement_cost, 1));
  equipment_.equipFromHotbar(hotbar_);
  setAvatarPointTarget(hit_point);
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedCaveWeb(const std::size_t web_index) {
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (web_index >= cave_webs_.size() || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }

  CaveWebObstacle &web = cave_webs_[web_index];
  if (web.broken || length(player_position_ - web.center) > kCaveWebInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  Vec3 hit_normal = player_position_ - web.center;
  hit_normal = length(hit_normal) > 0.0001f ? normalize(hit_normal) : web.normal;
  const MineableHit web_hit{.hit = true,
                            .target_key = web.id.empty()
                                              ? "lumen.cave_web." + std::to_string(web_index)
                                              : web.id,
                            .point = web.center,
                            .normal = hit_normal,
                            .material = VoxelCaveMaterial::Rock};
  const MineableAttempt mine_attempt{.now_seconds = status_.elapsed_seconds,
                                     .hit = web_hit,
                                     .tool = tool,
                                     .material_hardness = std::max(web.hardness, 0.1f),
                                     .resource_item_id = {},
                                     .resource_quantity = 0,
                                     .carve_surface = false};
  const MiningFeedback feedback = mining_.tryMine(mine_attempt);
  if (!feedback.accepted) {
    return false;
  }

  web.hit_flash = 1.0f;
  setAvatarPointTarget(feedback.impact_point);
  Material fracture_material =
      web.object_index < scene_.objects().size() ? scene_.objects()[web.object_index].material
                                                 : Material{};
  fracture_material.surface_pattern = SurfacePattern::CaveWeb;
  fracture_material.opacity = std::max(fracture_material.opacity, 0.62f);
  const Vec3 fracture_extent{std::max(web.radius_x * 0.22f, 0.16f),
                             std::max(web.radius_y * 0.12f, 0.10f), 0.035f};
  if (!feedback.carved) {
    fracture_material.emission_strength = 0.08f + feedback.crack_fraction * 0.10f;
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal, fracture_extent,
                              fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8719u),
                              4 + static_cast<int>(std::ceil(feedback.crack_fraction * 5.0f)));
    return true;
  }

  web.broken = true;
  if (web.object_index < scene_.objects().size()) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {std::max(web.radius_x * 0.42f, 0.22f),
                               std::max(web.radius_y * 0.20f, 0.14f), 0.045f},
                              fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8729u), 14);
    hideRenderObject(scene_.objects()[web.object_index]);
  }
  for (CaveSkitter &skitter : cave_skitters_) {
    if (skitter.dead || skitter.state.dead) {
      continue;
    }
    const Vec3 skitter_center = skitter.state.position + Vec3{0.0f, 0.08f, 0.0f};
    spawnBloodBurst(skitter_center, feedback.impact_point, 1.0f);
    if (skitter.object_index < scene_.objects().size()) {
      Material skitter_burst_material = scene_.objects()[skitter.object_index].material;
      skitter_burst_material.base_color = {0.42f, 0.018f, 0.010f};
      skitter_burst_material.emission_color = {0.12f, 0.0f, 0.0f};
      skitter_burst_material.emission_strength =
          std::max(skitter_burst_material.emission_strength, 0.10f);
      spawnMiningFractureEffect(skitter_center, feedback.impact_normal, {0.22f, 0.12f, 0.24f},
                                skitter_burst_material,
                                fractureSeedFor(skitter_center, kLumenCaveSeed + 8837u), 8);
      hideRenderObject(scene_.objects()[skitter.object_index]);
    }
    skitter.dead = true;
    skitter.state.dead = true;
    skitter.state.velocity = {};
    skitter.health = 0;
  }
  focused_cave_web_valid_ = false;
  interaction_.clear();
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedCaveSkitter(const std::size_t skitter_index) {
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (skitter_index >= cave_skitters_.size() || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }

  CaveSkitter &skitter = cave_skitters_[skitter_index];
  if (skitter.dead || skitter.state.dead ||
      length(player_position_ - skitter.state.position) > kCaveSkitterInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  Vec3 hit_normal = player_position_ - skitter.state.position;
  hit_normal = length(hit_normal) > 0.0001f ? normalize(hit_normal) : Vec3{0.0f, 1.0f, 0.0f};
  const MineableHit skitter_hit{
      .hit = true,
      .target_key =
          skitter.id.empty() ? "lumen.cave_skitter." + std::to_string(skitter_index) : skitter.id,
      .point = skitter.state.position + Vec3{0.0f, 0.08f, 0.0f},
      .normal = hit_normal,
      .material = VoxelCaveMaterial::Rock};
  const MiningFeedback feedback =
      mining_.tryMine({.now_seconds = status_.elapsed_seconds,
                       .hit = skitter_hit,
                       .tool = tool,
                       .material_hardness = static_cast<float>(std::max(skitter.max_health, 1)),
                       .resource_item_id = {},
                       .resource_quantity = 0,
                       .carve_surface = false});
  if (!feedback.accepted) {
    return false;
  }

  skitter.hit_flash = 1.0f;
  skitter.last_hit_normal = feedback.impact_normal;
  skitter.state.flinch_seconds = 0.36f;
  setAvatarPointTarget(feedback.impact_point);
  skitter.health = std::max(
      feedback.carved ? 0 : 1,
      static_cast<int>(std::ceil((1.0f - feedback.crack_fraction) *
                                 static_cast<float>(std::max(skitter.max_health, 1)))));
  const Material fracture_material =
      skitter.object_index < scene_.objects().size() ? scene_.objects()[skitter.object_index].material
                                                     : Material{};
  if (!feedback.carved) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {0.24f, 0.16f, 0.26f}, fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8819u),
                              8 + static_cast<int>(std::ceil(feedback.crack_fraction * 7.0f)));
    return true;
  }

  skitter.dead = true;
  skitter.state.dead = true;
  skitter.state.velocity = {};
  spawnBloodBurst(skitter.state.position + Vec3{0.0f, 0.08f, 0.0f}, feedback.impact_point, 1.0f);
  if (skitter.object_index < scene_.objects().size()) {
    spawnMiningFractureEffect(feedback.impact_point, feedback.impact_normal,
                              {0.24f, 0.14f, 0.28f}, fracture_material,
                              fractureSeedFor(feedback.impact_point, kLumenCaveSeed + 8829u), 18);
    hideRenderObject(scene_.objects()[skitter.object_index]);
  }
  interaction_.clear();
  invalidateSceneReports();
  return true;
}

bool LumenRun::mineFocusedOre(const std::size_t ore_index) {
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  if (ore_index >= coal_ores_.size() || equipped_definition == nullptr ||
      !equipped_definition->has_mining_tool) {
    return false;
  }

  CoalOreNode &ore = coal_ores_[ore_index];
  if (ore.collected || length(player_position_ - ore.position) > kOreInteractionDistance) {
    return false;
  }

  const MiningToolStats tool = activePickaxeStats();
  MineableHit ore_hit;
  ore_hit.hit = true;
  ore_hit.target_key = "lumen.coal_ore." + std::to_string(ore_index);
  ore_hit.point = ore.position;
  ore_hit.normal = length(ore.normal) > 0.0001f ? normalize(ore.normal) : Vec3{0.0f, 1.0f, 0.0f};
  ore_hit.material = VoxelCaveMaterial::Coal;
  MiningFeedback feedback = mining_.tryMine({.now_seconds = status_.elapsed_seconds,
                                             .hit = ore_hit,
                                             .tool = tool,
                                             .material_hardness =
                                                 static_cast<float>(std::max(ore.max_health, 1)),
                                             .resource_item_id = "coal",
                                             .resource_quantity = ore.yield_quantity});
  if (!feedback.accepted) {
    return false;
  }

  ore.hit_flash = 1.0f;
  setAvatarPointTarget(feedback.impact_point);
  ore.health = std::max(
      feedback.carved ? 0 : 1,
      static_cast<int>(std::ceil((1.0f - feedback.crack_fraction) *
                                 static_cast<float>(std::max(ore.max_health, 1)))));
  if (!feedback.carved) {
    return true;
  }

  const ItemDefinition *coal = item_registry_.find("coal");
  if (coal == nullptr || !storeMinedResource(*coal, feedback.resource_quantity)) {
    ore.health = 1;
    return false;
  }

  if (ore.object_index < scene_.objects().size()) {
    const Material fracture_material = scene_.objects()[ore.object_index].material;
    spawnMiningFractureEffect(ore.position, ore.normal, ore.scale * 0.52f, fracture_material,
                              fractureSeedFor(ore.position, kLumenCaveSeed + 9001u), 12);
    hideRenderObject(scene_.objects()[ore.object_index]);
  }
  ore.collected = true;
  equipment_.equipFromHotbar(hotbar_);
  return true;
}

} // namespace aster
