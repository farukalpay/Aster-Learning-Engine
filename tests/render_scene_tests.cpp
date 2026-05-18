// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include <cstring>

namespace {

void testFramebufferOriginContract() {
  aster::SoftwareFrameBuffer framebuffer;
  framebuffer.resize(4, 4);
  framebuffer.clearTransparent();
  framebuffer.fillUiRect(0.0f, 0.0f, 2.0f, 2.0f, {255, 32, 16, 255});
  framebuffer.fillUiRect(0.0f, 2.0f, 2.0f, 2.0f, {24, 64, 255, 255});

  const auto pixels = framebuffer.rgba8();
  const auto pixel = [&](const int x, const int y) -> const std::uint8_t * {
    return pixels.data() + (static_cast<std::size_t>(y * framebuffer.width() + x) * 4u);
  };
  assert(pixel(0, 0)[0] == 255u);
  assert(pixel(0, 0)[1] == 32u);
  assert(pixel(0, 0)[2] == 16u);
  assert(pixel(0, 2)[0] == 24u);
  assert(pixel(0, 2)[1] == 64u);
  assert(pixel(0, 2)[2] == 255u);

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "aster_framebuffer_origin.ppm";
  framebuffer.writePpm(path, 4, 4);
  {
    std::ifstream file(path, std::ios::binary);
    std::string magic;
    std::string dimensions;
    std::string max_value;
    std::getline(file, magic);
    std::getline(file, dimensions);
    std::getline(file, max_value);
    assert(magic == "P6");
    assert(dimensions == "4 4");
    assert(max_value == "255");
    std::vector<std::uint8_t> rgb(4u * 4u * 3u);
    file.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    assert(rgb[0] == 255u);
    assert(rgb[1] == 32u);
    assert(rgb[2] == 16u);
    const std::size_t third_row = 2u * 4u * 3u;
    assert(rgb[third_row + 0u] == 24u);
    assert(rgb[third_row + 1u] == 64u);
    assert(rgb[third_row + 2u] == 255u);
  }
  std::filesystem::remove(path);
}

void testUiTextFittingContract() {
  aster::UiCanvas canvas;
  const float preferred = 1.6f;
  const float narrow_width = 104.0f;
  const float fitted = canvas.fittedTextScale("Back-face culling", narrow_width, preferred, 0.85f);
  assert(fitted < preferred);
  assert(fitted >= 0.85f);
  assert(canvas.textWidth("Back-face culling", fitted) <= narrow_width + 0.001f);
  assert(canvas.fittedTextScale("Back-face culling", 400.0f, preferred, 0.85f) == preferred);
}

void testHudVisibilityPolicy() {
  const aster::HudVisibilityPolicy gameplay = aster::hudVisibilityForState({});
  assert(gameplay.health);
  assert(gameplay.hotbar);
  assert(gameplay.focus_prompt);

  const aster::HudVisibilityPolicy pause =
      aster::hudVisibilityForState({.pause_open = true});
  assert(!pause.health);
  assert(!pause.hotbar);
  assert(!pause.focus_prompt);
  assert(!pause.status_panel);
  assert(!pause.game_cursor);

  const aster::HudVisibilityPolicy inventory =
      aster::hudVisibilityForState({.inventory_open = true});
  assert(!inventory.health);
  assert(!inventory.hotbar);
  assert(!inventory.focus_prompt);
  assert(!inventory.status_panel);

  const aster::HudVisibilityPolicy defeat =
      aster::hudVisibilityForState({.defeated = true});
  assert(!defeat.health);
  assert(!defeat.hotbar);
  assert(!defeat.pointer);

  const aster::HudVisibilityPolicy debug =
      aster::hudVisibilityForState({.pause_open = true, .defeated = true, .debug_capture = true});
  assert(debug.health);
  assert(debug.hotbar);
  assert(debug.status_panel);
}

void testSceneContract() {
  const aster::Scene scene = aster::makeArchitectureShowcaseScene();
  assert(scene.objects().size() >= 3u);
  assert(scene.objects().front().primitive == aster::MeshPrimitive::Plane);
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::CourseCells);
  assert(scene.objects().front().material.pattern_depth > 0.0f);
}

void testIndustrialPipeSceneContract() {
  const aster::CpuMesh tube = aster::makeTubeMesh({.length = 2.0f,
                                                   .outer_radius = 0.5f,
                                                   .wall_thickness = 0.08f,
                                                   .radial_segments = 24,
                                                   .length_segments = 3});
  const aster::CpuMesh bead =
      aster::makeCircumferentialBeadMesh({.pipe_radius = 0.5f,
                                          .bead_radius = 0.04f,
                                          .axial_width = 0.14f,
                                          .radial_segments = 24,
                                          .bead_segments = 8});
  assert(!tube.vertices.empty());
  assert(!tube.indices.empty());
  assert(!bead.vertices.empty());
  assert(!bead.indices.empty());

  const aster::Scene scene = aster::makeIndustrialPipeScene();
  std::size_t weathered_metal = 0;
  std::size_t weld_beads = 0;
  for (const aster::RenderObject &object : scene.objects()) {
    weathered_metal +=
        object.material.surface_pattern == aster::SurfacePattern::WeatheredMetal ? 1u : 0u;
    weld_beads += object.material.surface_pattern == aster::SurfacePattern::WeldBead ? 1u : 0u;
  }
  assert(weathered_metal == 1u);
  assert(weld_beads == 2u);
}

void testSoftwarePreviewRendererProducesImage() {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.58f, 0.0f};
  camera.yaw = aster::radians(64.0f);
  camera.pitch = aster::radians(15.0f);
  camera.radius = 5.0f;
  camera.vertical_fov = aster::radians(42.0f);

  aster::RendererSettings settings;
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = true;
  settings.sun_light.enabled = true;
  settings.sun_light.intensity = 2.0f;
  settings.sun_light.direction_to_light = {-0.50f, 0.78f, 0.36f};
  settings.ambient_strength = 0.24f;
  settings.ambient_floor = 0.018f;

  const aster::SoftwareFrameBuffer framebuffer = aster::renderSoftwarePreview(
      aster::makeIndustrialPipeScene(), camera,
      {.width = 48, .height = 28, .samples_per_axis = 1, .frame_seconds = 0.0, .settings = settings});
  assert(framebuffer.width() == 48);
  assert(framebuffer.height() == 28);
  assert(framebuffer.rgba8().size() == 48u * 28u * 4u);
  bool has_non_black_pixel = false;
  for (std::size_t i = 0; i + 2u < framebuffer.rgba8().size(); i += 4u) {
    has_non_black_pixel = has_non_black_pixel || framebuffer.rgba8()[i + 0u] > 8u ||
                          framebuffer.rgba8()[i + 1u] > 8u || framebuffer.rgba8()[i + 2u] > 8u;
  }
  assert(has_non_black_pixel);
}

void testMaterialRenderPolicies() {
  aster::Material opaque = aster::makeMaterial({.base_color = {0.4f, 0.3f, 0.2f}});
  assert(aster::classifyMaterialRenderQueue(opaque) == aster::MaterialRenderQueue::Opaque);
  assert(aster::materialWritesDepth(opaque));
  assert(aster::allowsCameraOcclusionFade(opaque));

  aster::Material foliage =
      aster::makeMaterial({.alpha_mode = aster::MaterialAlphaMode::Masked,
                           .camera_occlusion = aster::CameraOcclusionPolicy::Solid});
  assert(aster::classifyMaterialRenderQueue(foliage) == aster::MaterialRenderQueue::Masked);
  assert(aster::materialWritesDepth(foliage));
  assert(!aster::allowsCameraOcclusionFade(foliage));

  aster::Material water =
      aster::makeMaterial({.opacity = 0.72f,
                           .alpha_mode = aster::MaterialAlphaMode::Blend,
                           .depth_write = aster::MaterialDepthWrite::Disabled,
                           .camera_occlusion = aster::CameraOcclusionPolicy::Solid});
  assert(aster::isMaterialTranslucent(water));
  assert(!aster::materialWritesDepth(water));
  assert(!aster::allowsCameraOcclusionFade(water));

  aster::Material support = aster::makeSupportSurfaceMaterial(water);
  assert(support.render_role == aster::MaterialRenderRole::SupportSurface);
  assert(aster::classifyMaterialRenderQueue(support) == aster::MaterialRenderQueue::Opaque);
  assert(aster::materialWritesDepth(support));
  assert(!aster::allowsCameraOcclusionFade(support));
}

void testFixedRenderGraphContract() {
  const aster::FixedRenderGraph graph = aster::makeFixedRenderGraph();
  assert(graph.resources.size() == 4u);
  assert(graph.resources[0].resource == aster::RenderGraphResource::SceneColor);
  assert(graph.resources[0].lifetime == aster::RenderGraphResourceLifetime::Frame);
  assert(graph.resources[3].resource == aster::RenderGraphResource::CaptureReadback);
  assert(graph.resources[3].lifetime == aster::RenderGraphResourceLifetime::Readback);
  assert(graph.passes.size() == 6u);
  assert(graph.passes[0].pass == aster::RenderGraphPass::SceneColorDepth);
  assert(graph.passes[1].pass == aster::RenderGraphPass::Opaque);
  assert(graph.passes[2].pass == aster::RenderGraphPass::ContactShadow);
  assert(graph.passes[3].pass == aster::RenderGraphPass::Transparent);
  assert(graph.passes[4].pass == aster::RenderGraphPass::UiComposite);
  assert(graph.passes[5].pass == aster::RenderGraphPass::Capture);
  const std::uint32_t color =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor);
  const std::uint32_t depth =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth);
  const std::uint32_t ui = aster::renderGraphResourceBit(aster::RenderGraphResource::UiOverlay);
  const std::uint32_t capture =
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  assert(graph.passes[0].reads == 0u && graph.passes[0].writes == (color | depth));
  assert(graph.passes[1].reads == (color | depth) && graph.passes[1].writes == (color | depth));
  assert(graph.passes[2].reads == (color | depth) && graph.passes[2].writes == (color | depth));
  assert(graph.passes[3].reads == (color | depth) && graph.passes[3].writes == color);
  assert(graph.passes[4].reads == (color | ui) && graph.passes[4].writes == color);
  assert(graph.passes[5].reads == color && graph.passes[5].writes == capture);
  assert(aster::renderGraphPassName(graph.passes[0].pass) == "scene-color-depth");
  assert(aster::renderGraphResourceName(aster::RenderGraphResource::UiOverlay) == "ui-overlay");
  assert(aster::renderGraphResourceLifetimeName(aster::RenderGraphResourceLifetime::Readback) ==
         "readback");
}

void testMaterialCompilerContract() {
  aster::Material opaque = aster::makeMaterial({.base_color = {0.45f, 0.40f, 0.32f}});
  const aster::CompiledMaterial opaque_compiled =
      aster::compileMaterialForRendering(opaque, false, "material.opaque");
  const aster::CompiledMaterial opaque_repeated =
      aster::compileMaterialForRendering(opaque, false, "material.opaque");
  assert(opaque_compiled.permutation_key == opaque_repeated.permutation_key);
  assert(opaque_compiled.permutation_flags == opaque_repeated.permutation_flags);
  assert(opaque_compiled.pipeline_tag == "opaque.depth-write.culled");

  const aster::CompiledMaterial textured =
      aster::compileMaterialForRendering(opaque, true, "material.opaque");
  assert(textured.permutation_key != opaque_compiled.permutation_key);
  assert((textured.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::Textured)) != 0u);

  aster::Material translucent =
      aster::makeMaterial({.base_color = {0.25f, 0.30f, 0.40f},
                           .opacity = 0.5f,
                           .double_sided = true,
                           .alpha_mode = aster::MaterialAlphaMode::Blend,
                           .depth_write = aster::MaterialDepthWrite::Disabled});
  const aster::CompiledMaterial translucent_compiled =
      aster::compileMaterialForRendering(translucent, false, "material.glass");
  assert(aster::isMaterialTranslucent(translucent_compiled.material));
  assert(translucent_compiled.pipeline_tag == "transparent.depth-read.double-sided");
  assert((translucent_compiled.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::Transparent)) != 0u);
  assert((translucent_compiled.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::DoubleSided)) != 0u);
}

void testRustRenderFramePlanContracts() {
  aster::Scene scene;
  const aster::Material opaque = aster::makeMaterial({.base_color = {0.4f, 0.3f, 0.2f}});
  const aster::Material translucent =
      aster::makeMaterial({.base_color = {0.2f, 0.4f, 0.6f},
                           .opacity = 0.55f,
                           .alpha_mode = aster::MaterialAlphaMode::Blend,
                           .depth_write = aster::MaterialDepthWrite::Disabled});

  auto add_object = [&](const char *name, const aster::Vec3 position,
                        const aster::Material &material) {
    aster::RenderObject object;
    object.name = name;
    object.primitive = aster::MeshPrimitive::Box;
    object.transform.position = position;
    object.material = material;
    scene.objects().push_back(object);
  };

  add_object("opaque near a", {-0.35f, 0.0f, 0.0f}, opaque);
  add_object("opaque near b", {0.35f, 0.0f, 0.0f}, opaque);
  add_object("culled far side", {200.0f, 0.0f, 0.0f}, opaque);
  add_object("transparent near", {0.0f, 0.0f, 1.0f}, translucent);
  add_object("transparent far", {0.0f, 0.0f, -4.0f}, translucent);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = 0.0f;
  camera.radius = 6.0f;
  camera.vertical_fov = aster::radians(90.0f);
  camera.far_plane = 40.0f;

  aster::RenderScene render_scene;
  render_scene.rebuild(scene);
  const aster::FrameRenderPlan plan =
      aster::buildFrameRenderPlan(render_scene, camera, {}, 800, 600);
  const aster::FrameRenderPlan repeated_plan =
      aster::buildFrameRenderPlan(render_scene, camera, {}, 800, 600);

  assert(plan.diagnostics.object_count == 5u);
  assert(plan.diagnostics.visible_objects == 4u);
  assert(plan.diagnostics.culled_objects == 1u);
  assert(plan.diagnostics.opaque_groups == 1u);
  assert(plan.diagnostics.transparent_groups == 1u);
  assert(plan.diagnostics.planned_instances == 4u);

  const auto transparent_group =
      std::find_if(plan.groups.begin(), plan.groups.end(), [](const aster::FrameRenderDrawGroup &group) {
        return group.pass == aster::FrameRenderPass::Transparent;
      });
  assert(transparent_group != plan.groups.end());
  assert(transparent_group->instance_count == 2u);
  const std::size_t first = transparent_group->first_instance;
  assert(plan.instances[first].object_index == 4u);
  assert(plan.instances[first + 1u].object_index == 3u);

  assert(repeated_plan.instances.size() == plan.instances.size());
  assert(repeated_plan.groups.size() == plan.groups.size());
  for (std::size_t i = 0; i < plan.instances.size(); ++i) {
    assert(repeated_plan.instances[i].object_index == plan.instances[i].object_index);
    expectNear(repeated_plan.instances[i].opacity, plan.instances[i].opacity, 0.0001f);
  }
  for (std::size_t i = 0; i < plan.groups.size(); ++i) {
    assert(repeated_plan.groups[i].mesh.value == plan.groups[i].mesh.value);
    assert(repeated_plan.groups[i].material.value == plan.groups[i].material.value);
    assert(repeated_plan.groups[i].pass == plan.groups[i].pass);
    assert(repeated_plan.groups[i].first_instance == plan.groups[i].first_instance);
    assert(repeated_plan.groups[i].instance_count == plan.groups[i].instance_count);
  }

  aster::Scene diagnostic_scene;
  auto dynamic_mesh = std::make_shared<const aster::CpuMesh>(aster::makeBox());
  aster::RenderObject dynamic;
  dynamic.name = "dynamic cave cell";
  dynamic.custom_mesh = dynamic_mesh;
  dynamic.material = opaque;
  dynamic.dynamic_mesh = {.id = 42u, .generation = 7u};
  dynamic.visibility_hint = {.visibility_class = aster::RenderVisibilityClass::DynamicVoxel,
                             .cell = {1.0f, 2.0f, 3.0f},
                             .portal_depth = 4.0f};
  diagnostic_scene.objects().push_back(dynamic);

  aster::RenderObject dynamic_clone = dynamic;
  dynamic_clone.custom_mesh = std::make_shared<const aster::CpuMesh>(aster::makeBox());
  assert(aster::renderMeshIdForObject(dynamic_clone).value ==
         aster::renderMeshIdForObject(dynamic).value);
  dynamic_clone.dynamic_mesh.generation = 8u;
  assert(aster::renderMeshIdForObject(dynamic_clone).value !=
         aster::renderMeshIdForObject(dynamic).value);

  aster::RenderObject lod_culled;
  lod_culled.name = "lod culled";
  lod_culled.primitive = aster::MeshPrimitive::Box;
  lod_culled.transform.position = {0.0f, 0.0f, 0.0f};
  lod_culled.material = opaque;
  lod_culled.lod.max_distance = 1.0f;
  diagnostic_scene.objects().push_back(lod_culled);

  aster::RenderScene diagnostic_render_scene;
  diagnostic_render_scene.rebuild(diagnostic_scene);
  const aster::FrameRenderPlan diagnostic_plan =
      aster::buildFrameRenderPlan(diagnostic_render_scene, camera, {}, 800, 600);
  assert(diagnostic_plan.diagnostics.object_count == 2u);
  assert(diagnostic_plan.diagnostics.visible_objects == 1u);
  assert(diagnostic_plan.diagnostics.lod_culled_objects == 1u);
  assert(diagnostic_plan.diagnostics.visibility_hint_objects == 1u);
  assert(diagnostic_plan.diagnostics.dynamic_mesh_objects == 1u);
}

void testSceneCoherenceEnergy() {
  aster::SceneCoherenceProblem coherent;
  coherent.visual.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.collision.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.navigation.samples.push_back({"surface", {0.0f, 0.0f, 0.0f}, 0.10f, 1.0f});
  coherent.routes.push_back(
      {"walkable", {{0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}}, 0.05f, 1.0f, 0.25f});
  coherent.solid_volumes.push_back({"wall", {3.0f, 0.0f, 0.0f}, {0.2f, 1.0f, 1.0f}});
  coherent.fluid_volumes.push_back({"water", {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
  coherent.ecological_samples.push_back({"aquatic", {0.2f, 0.0f, 0.0f}, 0.05f, 1.0f});
  coherent.fluid_interaction_segments.push_back(
      {"line", {-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 1.0f});
  coherent.affordance_samples.push_back(
      {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1.0f});
  coherent.material_fields.push_back(
      {"blend",
       {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, 1.0f}, {{0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}, 1.0f}},
       1.0f});
  coherent.visibility_probes.push_back({"camera",
                                        {0.0f, 0.0f, 0.0f},
                                        {{3.0f, 0.0f, 0.0f}},
                                        {{"room", {-0.5f, 0.0f, 0.0f}, {1.5f, 1.0f, 1.0f}}},
                                        {{"wall", {1.5f, 0.0f, 0.0f}, {0.1f, 1.0f, 1.0f}}}});
  coherent.light_samples.push_back(
      {{0.0f, 1.0f, 0.0f}, {0.6f, 0.5f, 0.4f}, {0.6f, 0.5f, 0.4f}, 1.0f});

  const aster::SceneCoherenceReport coherent_report = aster::evaluateSceneCoherence(coherent);
  assert(coherent_report.energy < 0.0001f);
  assert(aster::contains(coherent.fluid_volumes.front(), {0.0f, 0.0f, 0.0f}));
  assert(aster::segmentIntersectsVolume({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},
                                        coherent.fluid_volumes.front()));

  aster::SceneCoherenceProblem broken = coherent;
  broken.collision.samples.front().position = {2.0f, 0.0f, 0.0f};
  broken.ecological_samples.front().position = {4.0f, 0.0f, 0.0f};
  broken.fluid_interaction_segments.front().from = {3.0f, 0.0f, 0.0f};
  broken.fluid_interaction_segments.front().to = {4.0f, 0.0f, 0.0f};
  broken.visibility_probes.front().blockers.clear();
  const aster::SceneCoherenceReport broken_report = aster::evaluateSceneCoherence(broken);
  assert(broken_report.energy > coherent_report.energy);
  const aster::SceneCoherenceContribution *representation =
      findContribution(broken_report, aster::SceneCoherenceTerm::RepresentationCollision);
  const aster::SceneCoherenceContribution *fluid =
      findContribution(broken_report, aster::SceneCoherenceTerm::FluidContainment);
  const aster::SceneCoherenceContribution *visibility =
      findContribution(broken_report, aster::SceneCoherenceTerm::VisibilityLeak);
  assert(representation != nullptr && representation->raw_value > 0.0f);
  assert(fluid != nullptr && fluid->raw_value > 0.0f);
  assert(visibility != nullptr && visibility->raw_value > 0.0f);
}

void testSceneTraceValidation() {
  const std::vector<aster::SceneTraceRule> rules{
      aster::forbidTraceSymbol("camera", "CameraInsideWall"),
      aster::requireTraceSymbolWithin("path", "PathVisible", "PathContinues", 2u),
      aster::requireTraceSymbolSameFrame("water", "FishVisible", "FishInsideWater"),
      aster::forbidTraceSymbolSameFrame("water-surface", "FishVisible", "FishOnSurface")};

  aster::SceneSymbolicTrace valid;
  valid.frames.push_back({0.0, {"PathVisible", "PathContinues", "Walkable"}});
  valid.frames.push_back({0.1, {"FishVisible", "FishInsideWater"}});
  const aster::SceneTraceValidationReport valid_report = aster::validateSceneTrace(valid, rules);
  assert(valid_report.valid);
  assert(valid_report.defect_scale == aster::SceneTraceDefectScale::None);

  aster::SceneSymbolicTrace broken;
  broken.frames.push_back({0.0, {"PathVisible", "Walkable"}});
  broken.frames.push_back({0.1, {"Walkable"}});
  broken.frames.push_back({0.2, {"CameraInsideWall"}});
  broken.frames.push_back({0.3, {"FishVisible", "FishOnSurface"}});
  const aster::SceneTraceValidationReport broken_report = aster::validateSceneTrace(broken, rules);
  assert(!broken_report.valid);
  assert(broken_report.rank_proxy >= 1u);
  assert(broken_report.defect_scale != aster::SceneTraceDefectScale::None);

  const aster::SceneTraceSeparatorProfile profile =
      aster::solveSceneTraceFoSeparatorProfile({valid}, {broken}, {.horizon = 8u});
  assert(profile.separated);
  assert(profile.quantifier_rank == 1u);
  assert(profile.indistinguishable_pairs.empty());
}

void testSceneTraceFoModelChecking() {
  aster::SceneSymbolicTrace trace;
  trace.frames.push_back({0.0, {"P", "Q"}});
  trace.frames.push_back({0.1, {"Q"}});
  trace.frames.push_back({0.2, {"R", "arbitrary symbol"}});

  const aster::SceneTraceFoFormula exists_p = aster::parseSceneTraceFoFormula("exists x. P(x)");
  assert(aster::evaluateSceneTraceFoFormula(trace, exists_p));

  const aster::SceneTraceFoFormula p_implies_q =
      aster::parseSceneTraceFoFormula("forall x. (P(x) -> Q(x))");
  assert(aster::evaluateSceneTraceFoFormula(trace, p_implies_q));

  const aster::SceneTraceFoFormula ordered =
      aster::parseSceneTraceFoFormula("exists x. exists y. (x < y && P(x) && R(y))");
  assert(aster::evaluateSceneTraceFoFormula(trace, ordered));

  const aster::SceneTraceFoFormula quoted =
      aster::parseSceneTraceFoFormula("exists x. \"arbitrary symbol\"(x)");
  assert(aster::evaluateSceneTraceFoFormula(trace, quoted));

  const aster::SceneTraceFoFormula reflexive =
      aster::sceneTraceFoForall("x", aster::sceneTraceFoEqual("x", "x"));
  assert(aster::evaluateSceneTraceFoFormula(trace, reflexive));

  const aster::SceneTraceFoFormula free_predicate = aster::sceneTraceFoPredicate("P", "x");
  assert(aster::evaluateSceneTraceFoFormula(trace, free_predicate, {{"x", 0u}}));
  assert(!aster::evaluateSceneTraceFoFormula(trace, free_predicate, {{"x", 1u}}));

  aster::SceneSymbolicTrace empty;
  assert(!aster::evaluateSceneTraceFoFormula(empty, aster::parseSceneTraceFoFormula("exists x. true")));
  assert(aster::evaluateSceneTraceFoFormula(empty, aster::parseSceneTraceFoFormula("forall x. false")));

  const aster::SceneTraceFoFormula free_variables =
      aster::parseSceneTraceFoFormula("exists x. (P(x) && y < x)");
  const std::vector<std::string> free = aster::sceneTraceFoFreeVariables(free_variables);
  assert(free.size() == 1u && free.front() == "y");

  bool parser_rejected_invalid_input = false;
  try {
    (void)aster::parseSceneTraceFoFormula("exists x P(x)");
  } catch (const std::runtime_error &) {
    parser_rejected_invalid_input = true;
  }
  assert(parser_rejected_invalid_input);
}

void testSceneTraceFoQuantifierRank() {
  assert(aster::sceneTraceFoQuantifierRank(aster::sceneTraceFoPredicate("P", "x")) == 0u);
  assert(aster::sceneTraceFoQuantifierRank(
             aster::parseSceneTraceFoFormula("exists x. P(x)")) == 1u);
  assert(aster::sceneTraceFoQuantifierRank(
             aster::parseSceneTraceFoFormula("exists x. forall y. (x < y -> P(y))")) == 2u);

  const aster::SceneTraceFoFormula rank_one =
      aster::sceneTraceFoExists("x", aster::sceneTraceFoPredicate("P", "x"));
  const aster::SceneTraceFoFormula rank_two = aster::sceneTraceFoForall(
      "y", aster::sceneTraceFoExists("z", aster::sceneTraceFoLess("y", "z")));
  assert(aster::sceneTraceFoQuantifierRank(
             aster::sceneTraceFoAnd(rank_one, rank_two)) == 2u);
}

void testSceneTraceFoSeparatorSolving() {
  aster::SceneSymbolicTrace empty;
  aster::SceneSymbolicTrace singleton_a;
  singleton_a.frames.push_back({0.0, {"A"}});

  aster::SceneTraceSeparatorProfile profile =
      aster::solveSceneTraceFoSeparatorProfile({empty}, {singleton_a});
  assert(profile.separated);
  assert(profile.complete_search);
  assert(profile.quantifier_rank == 1u);

  aster::SceneSymbolicTrace singleton_b;
  singleton_b.frames.push_back({0.0, {"B"}});
  profile = aster::solveSceneTraceFoSeparatorProfile({singleton_a}, {singleton_b});
  assert(profile.separated);
  assert(profile.quantifier_rank == 1u);

  aster::SceneSymbolicTrace two_a;
  two_a.frames.push_back({0.0, {"A"}});
  two_a.frames.push_back({0.1, {"A"}});
  profile = aster::solveSceneTraceFoSeparatorProfile({singleton_a}, {two_a});
  assert(profile.separated);
  assert(profile.quantifier_rank == 2u);
  assert(aster::sceneTraceFoEquivalentAtRank(singleton_a, two_a, 1u));
  assert(!aster::sceneTraceFoEquivalentAtRank(singleton_a, two_a, 2u));

  aster::SceneSymbolicTrace ab;
  ab.frames.push_back({0.0, {"A"}});
  ab.frames.push_back({0.1, {"B"}});
  aster::SceneSymbolicTrace ba;
  ba.frames.push_back({0.0, {"B"}});
  ba.frames.push_back({0.1, {"A"}});
  profile = aster::solveSceneTraceFoSeparatorProfile({ab}, {ba});
  assert(profile.separated);
  assert(profile.quantifier_rank == 2u);

  profile = aster::solveSceneTraceFoSeparatorProfile({singleton_a}, {singleton_a});
  assert(!profile.separated);
  assert(profile.complete_search);
  assert(profile.searched_quantifier_rank == 1u);
  assert(profile.indistinguishable_pairs.size() == 1u);
  assert(profile.indistinguishable_pairs.front().accepted_index == 0u);
  assert(profile.indistinguishable_pairs.front().rejected_index == 0u);

  profile = aster::solveSceneTraceFoSeparatorProfile({}, {singleton_a});
  assert(profile.separated);
  assert(profile.vacuous);
  assert(profile.quantifier_rank == 0u);
}

struct TestCase {
  const char *name = "";
  void (*run)() = nullptr;
};

constexpr TestCase kTestCases[] = {
    {"framebuffer_origin", testFramebufferOriginContract},
    {"ui_text_fitting", testUiTextFittingContract},
    {"hud_visibility", testHudVisibilityPolicy},
    {"scene_contract", testSceneContract},
    {"industrial_pipe_scene", testIndustrialPipeSceneContract},
    {"software_preview_renderer", testSoftwarePreviewRendererProducesImage},
    {"material_render_policies", testMaterialRenderPolicies},
    {"fixed_render_graph", testFixedRenderGraphContract},
    {"material_compiler", testMaterialCompilerContract},
    {"rust_render_frame_plan", testRustRenderFramePlanContracts},
    {"scene_coherence_energy", testSceneCoherenceEnergy},
    {"scene_trace_validation", testSceneTraceValidation},
    {"scene_trace_fo_model_checking", testSceneTraceFoModelChecking},
    {"scene_trace_fo_quantifier_rank", testSceneTraceFoQuantifierRank},
    {"scene_trace_fo_separator_solving", testSceneTraceFoSeparatorSolving},
};

int runTestCase(const TestCase &test_case) {
  std::cout << "render_scene_tests: " << test_case.name << '\n';
  test_case.run();
  std::cout << "render_scene_tests: " << test_case.name << " passed.\n";
  return 0;
}

} // namespace

int main(const int argc, const char **argv) {
  if (argc > 1) {
    for (const TestCase &test_case : kTestCases) {
      if (std::strcmp(argv[1], test_case.name) == 0) {
        return runTestCase(test_case);
      }
    }
    std::cerr << "Unknown render_scene_tests case: " << argv[1] << '\n';
    return 1;
  }

  for (const TestCase &test_case : kTestCases) {
    runTestCase(test_case);
  }
  std::cout << "render_scene_tests passed.\n";
  return 0;
}
