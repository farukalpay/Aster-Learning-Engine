// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/framegraph/transient_resource_allocator.hpp"
#include "aster/rhi/graphics_pipeline.hpp"
#include "aster/rhi/resource_barrier.hpp"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <span>
#include <unordered_set>

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

void writeRenderTraceKtx2Header(const std::filesystem::path &path, const std::uint32_t width,
                                const std::uint32_t height, const std::uint32_t mip_count) {
  std::ofstream file(path, std::ios::binary);
  file.put(static_cast<char>(0xab));
  file.write("KTX 20", 6);
  file.put(static_cast<char>(0xbb));
  file.put('\r');
  file.put('\n');
  file.put(static_cast<char>(0x1a));
  file.put('\n');
  const auto write_le32 = [&file](const std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffu),
                           static_cast<char>((value >> 8u) & 0xffu),
                           static_cast<char>((value >> 16u) & 0xffu),
                           static_cast<char>((value >> 24u) & 0xffu)};
    file.write(bytes, 4);
  };
  write_le32(37u);
  write_le32(1u);
  write_le32(width);
  write_le32(height);
  write_le32(0u);
  write_le32(0u);
  write_le32(1u);
  write_le32(mip_count);
  write_le32(1u);
  file.write("ASTER_TEST_PAYLOAD_PADDING_0000", 30);
  assert(file.good());
}

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
  assert(aster::resolveMaterialSurfaceProfile(scene.objects().front().material) ==
         aster::MaterialSurfaceProfile::Masonry);
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
        aster::resolveMaterialSurfaceProfile(object.material) ==
                aster::MaterialSurfaceProfile::CorrodedMetal
            ? 1u
            : 0u;
    weld_beads += aster::resolveMaterialSurfaceProfile(object.material) ==
                          aster::MaterialSurfaceProfile::WeldBead
                      ? 1u
                      : 0u;
  }
  assert(weathered_metal == 1u);
  assert(weld_beads == 2u);
}

void testShowcaseLabSceneContracts() {
  const aster::Scene material_lab = aster::makeMaterialLabShowcaseScene();
  const aster::Scene mesh_lab = aster::makeMeshLabShowcaseScene();
  const aster::Scene lighting_lab = aster::makeLightingLabShowcaseScene();
  const aster::Scene scene_lab = aster::makeSceneLabShowcaseScene();

  assert(material_lab.objects().size() >= 5u);
  assert(mesh_lab.objects().size() >= 5u);
  assert(lighting_lab.objects().size() >= 7u);
  assert(scene_lab.objects().size() >= 20u);

  std::size_t custom_meshes = 0u;
  for (const aster::RenderObject &object : mesh_lab.objects()) {
    custom_meshes += object.custom_mesh != nullptr ? 1u : 0u;
  }
  assert(custom_meshes >= 4u);

  std::size_t translucent = 0u;
  for (const aster::RenderObject &object : material_lab.objects()) {
    translucent += aster::isMaterialTranslucent(object.material) ? 1u : 0u;
  }
  assert(translucent >= 1u);
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

void testSoftwareDepthPolicySeparatesCoplanarAttachments() {
  aster::SoftwareFrameBuffer framebuffer;
  framebuffer.resize(32, 32);
  const aster::FrameVertex a{4.0f, 4.0f, 0.50f, {16u, 32u, 220u, 255u}};
  const aster::FrameVertex b{28.0f, 4.0f, 0.50f, {16u, 32u, 220u, 255u}};
  const aster::FrameVertex c{4.0f, 28.0f, 0.50f, {16u, 32u, 220u, 255u}};
  const aster::FrameVertex decal_a{4.0f, 4.0f, 0.50f, {240u, 20u, 16u, 255u}};
  const aster::FrameVertex decal_b{28.0f, 4.0f, 0.50f, {240u, 20u, 16u, 255u}};
  const aster::FrameVertex decal_c{4.0f, 28.0f, 0.50f, {240u, 20u, 16u, 255u}};
  const auto pixel = [&framebuffer](const int x, const int y) {
    const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
    return rgba.data() + (static_cast<std::size_t>(y * framebuffer.width() + x) * 4u);
  };

  framebuffer.clear({0.0f, 0.0f, 0.0f});
  framebuffer.drawTriangle(a, b, c, true, true, false);
  framebuffer.drawTriangle(decal_a, decal_b, decal_c, true, false, false);
  assert(pixel(10, 10)[0] == 16u);
  assert(pixel(10, 10)[2] == 220u);

  framebuffer.clear({0.0f, 0.0f, 0.0f});
  framebuffer.drawTriangle(a, b, c, true, true, false);
  framebuffer.drawTriangle(decal_a, decal_b, decal_c, true, false, false, 0.010f, 0.0f);
  assert(pixel(10, 10)[0] == 240u);
  assert(pixel(10, 10)[1] == 20u);
}

void testSoftwareDepthPolicyIsStableAcrossObjectOrder() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  auto make_scene = [](const bool decal_first) {
    aster::RenderObject base;
    base.name = "coplanar base";
    base.primitive = aster::MeshPrimitive::Plane;
    base.material = aster::makeMaterial({.base_color = {0.02f, 0.04f, 0.42f},
                                         .emission_color = {0.02f, 0.04f, 0.42f},
                                         .emission_strength = 0.35f,
                                         .roughness = 1.0f});

    aster::RenderObject decal;
    decal.name = "coplanar red decal";
    decal.primitive = aster::MeshPrimitive::Plane;
    decal.material = aster::makeMaterial({.base_color = {1.0f, 0.04f, 0.02f},
                                          .emission_color = {1.0f, 0.04f, 0.02f},
                                          .emission_strength = 0.55f,
                                          .opacity = 1.0f,
                                          .alpha_mode = aster::MaterialAlphaMode::Blend,
                                          .depth_write = aster::MaterialDepthWrite::Disabled,
                                          .double_sided = true});
    decal.material.depth_policy = {.layer = aster::RenderDepthLayer::Decal,
                                   .constant_bias = 0.0025f,
                                   .slope_bias = 0.0f};

    aster::Scene scene;
    if (decal_first) {
      scene.objects().push_back(decal);
      scene.objects().push_back(base);
    } else {
      scene.objects().push_back(base);
      scene.objects().push_back(decal);
    }
    return scene;
  };

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = aster::radians(42.0f);
  camera.pitch = aster::radians(60.0f);
  camera.radius = 4.0f;
  camera.vertical_fov = aster::radians(44.0f);

  aster::RendererSettings settings;
  settings.pipeline.clear_color = {0.0f, 0.0f, 0.0f};
  settings.atmosphere.enabled = false;
  settings.sun_light.enabled = false;
  settings.ambient_strength = 0.10f;
  settings.ambient_floor = 0.01f;

  auto render = [&](const aster::Scene &scene) {
    aster::RenderDevice renderer;
    assert(renderer.initialize());
    renderer.prepareScene(scene);
    (void)renderer.render(scene, camera, settings, 48, 48, 0.0);
    return std::vector<std::uint8_t>(aster::activeFrameBuffer().rgba8().begin(),
                                     aster::activeFrameBuffer().rgba8().end());
  };

  const std::vector<std::uint8_t> base_first = render(make_scene(false));
  const std::vector<std::uint8_t> decal_first = render(make_scene(true));
  assert(base_first == decal_first);

  std::size_t red_pixels = 0u;
  for (std::size_t i = 0; i + 3u < base_first.size(); i += 4u) {
    if (base_first[i + 0u] > base_first[i + 2u] * 2u && base_first[i + 0u] > 80u) {
      ++red_pixels;
    }
  }
  assert(red_pixels > 120u);
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

std::size_t countForegroundPixels(const aster::SoftwareFrameBuffer &framebuffer) {
  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  if (rgba.size() < 4u) {
    return 0u;
  }
  const std::uint8_t bg_r = rgba[0u];
  const std::uint8_t bg_g = rgba[1u];
  const std::uint8_t bg_b = rgba[2u];
  std::size_t foreground = 0u;
  for (std::size_t i = 0; i + 3u < rgba.size(); i += 4u) {
    const int dr = std::abs(static_cast<int>(rgba[i + 0u]) - static_cast<int>(bg_r));
    const int dg = std::abs(static_cast<int>(rgba[i + 1u]) - static_cast<int>(bg_g));
    const int db = std::abs(static_cast<int>(rgba[i + 2u]) - static_cast<int>(bg_b));
    foreground += dr + dg + db > 32 ? 1u : 0u;
  }
  return foreground;
}

void testPrepareSceneInvalidatesCustomMeshCache() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  aster::RenderDevice renderer;
  renderer.initialize();

  auto mesh = std::make_shared<aster::CpuMesh>(aster::makeBox());
  aster::RenderObject object;
  object.name = "cache invalidation probe";
  object.custom_mesh = mesh;
  object.transform.position = {0.0f, 0.55f, 0.0f};
  object.transform.scale = {1.0f, 1.0f, 1.0f};
  object.material = aster::makeMaterial({.base_color = {0.92f, 0.18f, 0.10f},
                                         .roughness = 0.6f,
                                         .double_sided = true});

  aster::Scene scene;
  scene.objects().push_back(object);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.55f, 0.0f};
  camera.yaw = aster::radians(36.0f);
  camera.pitch = aster::radians(8.0f);
  camera.radius = 4.0f;
  camera.vertical_fov = aster::radians(42.0f);

  aster::RendererSettings settings;
  settings.pipeline.clear_color = {0.002f, 0.002f, 0.002f};
  settings.sun_light.enabled = true;
  settings.sun_light.intensity = 2.0f;
  settings.sun_light.direction_to_light = {-0.4f, 0.8f, 0.3f};
  settings.ambient_strength = 0.35f;
  settings.ambient_floor = 0.04f;
  settings.atmosphere.enabled = false;

  renderer.prepareScene(scene);
  (void)renderer.render(scene, camera, settings, 72, 48, 0.0);
  const std::size_t visible_before = countForegroundPixels(aster::activeFrameBuffer());
  assert(visible_before > 80u);

  *mesh = {};
  renderer.prepareScene(scene);
  (void)renderer.render(scene, camera, settings, 72, 48, 1.0 / 60.0);
  const std::size_t visible_after = countForegroundPixels(aster::activeFrameBuffer());
  assert(visible_after < visible_before / 8u);

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testFrameMathDiagnostics() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  aster::RenderDevice renderer;
  renderer.initialize();

  aster::RenderObject object;
  object.name = "math diagnostic probe";
  object.primitive = aster::MeshPrimitive::Box;
  object.transform.position = {0.0f, 0.5f, 0.0f};
  object.transform.scale = {-1.0f, 0.0000001f, 1.0f};
  object.material = aster::makeMaterial({.base_color = {0.4f, 0.7f, 0.9f}});

  aster::Scene scene;
  scene.objects().push_back(object);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.5f, 0.0f};
  camera.yaw = aster::radians(42.0f);
  camera.pitch = aster::radians(14.0f);
  camera.radius = 5.0f;
  camera.vertical_fov = aster::radians(44.0f);

  aster::RendererSettings settings;
  settings.atmosphere.enabled = false;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.2f, 1.0f, 0.1f};

  aster::clearMathDiagnostics();
  (void)aster::normalizeOr(aster::Vec3{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  (void)renderer.render(scene, camera, settings, 48, 32, 0.0);

  const aster::FrameForensics &forensics = renderer.lastFrameForensics();
  assert(forensics.evidence.schema_id != 0u);
  assert(forensics.evidence.render_ir_hash != 0u);
  assert(forensics.evidence.visibility_plan_hash != 0u);
  assert(forensics.evidence.draw_signature_count > 0u);
  assert(!forensics.resource_traces.empty());
  assert(!forensics.captures.empty());
  assert(!forensics.rhi_trace.transitions.empty());
  assert(!forensics.rhi_trace.queue_submits.empty());
  assert(!forensics.material_bindings.empty());
  assert(forensics.mesh_visibility.size() == scene.objects().size());
  bool saw_math_contract = false;
  bool saw_singular_normal = false;
  bool saw_negative_scale = false;
  for (const aster::FrameDiagnosticEvent &event : forensics.events) {
    saw_math_contract =
        saw_math_contract || event.kind == aster::FrameDiagnosticKind::MathContract;
    saw_singular_normal =
        saw_singular_normal || event.kind == aster::FrameDiagnosticKind::SingularNormalMatrix;
    saw_negative_scale =
        saw_negative_scale || event.kind == aster::FrameDiagnosticKind::NegativeScaleTangentFlip;
  }
  assert(saw_math_contract);
  assert(saw_singular_normal);
  assert(saw_negative_scale);
  assert(aster::mathDiagnosticCount() == 0u);

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testFrameDebuggerMaterialBindingTrace() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "aster_frame_debug_material_trace";
  std::filesystem::create_directories(dir);
  writeRenderTraceKtx2Header(dir / "trace_albedo.ktx2", 16u, 16u, 4u);
  writeRenderTraceKtx2Header(dir / "trace_normal.ktx2", 16u, 16u, 4u);

  aster::MaterialAsset asset;
  asset.id = "TraceMaterial";
  asset.name = "Trace Material";
  asset.source_path = dir / "trace.astermat";
  asset.textures["albedo"] = {.role = "albedo", .uri = "trace_albedo.ktx2", .srgb = true};
  asset.textures["normal"] = {.role = "normal", .uri = "trace_normal.ktx2", .srgb = false};
  asset.explicit_features["normal_map"] = true;

  auto library = std::make_shared<aster::MaterialResourceLibrary>();
  assert(library->addMaterialAsset(asset, dir, {.require_existing_files = false}));

  aster::RenderObject object;
  object.name = "material binding probe";
  object.primitive = aster::MeshPrimitive::Box;
  object.transform.position = {0.0f, 0.5f, 0.0f};
  object.material_asset_id = "TraceMaterial";
  object.material = aster::makeMaterial({.base_color = {0.7f, 0.5f, 0.3f}});
  object.material.asset_id = "TraceMaterial";

  aster::Scene scene;
  scene.objects().push_back(object);
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.5f, 0.0f};
  camera.radius = 4.5f;

  aster::RendererSettings settings;
  settings.sun_light.enabled = true;
  aster::RenderDevice renderer;
  renderer.initialize();
  renderer.setMaterialResourceLibrary(library);
  renderer.prepareScene(scene);
  (void)renderer.render(scene, camera, settings, 64, 48, 0.0);

  const aster::FrameForensics &forensics = renderer.lastFrameForensics();
  bool saw_authored_albedo = false;
  bool saw_authored_normal = false;
  bool saw_fallback_roughness = false;
  for (const aster::MaterialBindingTrace &binding : forensics.material_bindings) {
    if (binding.role == "albedo") {
      saw_authored_albedo = binding.valid && binding.bound && !binding.fallback &&
                            binding.descriptor_layout_hash != 0u && binding.mip_count >= 1u;
    }
    if (binding.role == "normal") {
      saw_authored_normal = binding.valid && binding.bound && !binding.fallback;
    }
    if (binding.role == "roughness") {
      saw_fallback_roughness = binding.valid && binding.bound && binding.fallback;
    }
  }
  assert(saw_authored_albedo);
  assert(saw_authored_normal);
  assert(saw_fallback_roughness);
  assert(!forensics.captures.empty());
  assert(!forensics.resource_traces.empty());
  std::filesystem::remove_all(dir);
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testSoftwareReferenceFrameResourceCaptures() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  aster::Scene scene;
  aster::RenderObject floor;
  floor.name = "shadow receiver floor";
  floor.primitive = aster::MeshPrimitive::Plane;
  floor.transform.scale = {1.4f, 1.0f, 1.4f};
  floor.material = aster::makeSupportSurfaceMaterial(
      aster::makeMaterial({.base_color = {0.35f, 0.34f, 0.31f}, .roughness = 0.82f}));
  floor.auto_contact_shadow = false;
  scene.objects().push_back(floor);

  aster::RenderObject caster;
  caster.name = "shadow atlas caster";
  caster.primitive = aster::MeshPrimitive::Box;
  caster.transform.position = {0.0f, 0.48f, 0.0f};
  caster.transform.scale = {0.46f, 0.96f, 0.46f};
  caster.material = aster::makeMaterial({.base_color = {0.68f, 0.45f, 0.24f},
                                         .roughness = 0.55f,
                                         .metallic = 0.15f});
  scene.objects().push_back(caster);

  scene.reflectionProbes().push_back({.name = "software local probe",
                                      .position = {0.0f, 0.9f, 0.0f},
                                      .influence_radius = 5.0f,
                                      .sky_irradiance = {0.28f, 0.38f, 0.58f},
                                      .ground_irradiance = {0.18f, 0.12f, 0.08f},
                                      .specular_tint = {1.0f, 0.92f, 0.82f},
                                      .intensity = 1.2f});

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.52f, 0.0f};
  camera.yaw = aster::radians(38.0f);
  camera.pitch = aster::radians(18.0f);
  camera.radius = 4.2f;

  aster::RendererSettings settings;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.42f, 0.82f, 0.26f};
  settings.sun_light.intensity = 1.4f;
  settings.shadows.enabled = true;
  settings.shadows.cascaded_directional = true;
  settings.shadows.directional_cascades = 2u;
  settings.shadows.atlas_size = 64u;
  settings.shadows.max_distance = 12.0f;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.10f, 0.13f, 0.16f};
  settings.atmosphere.fog_start = 1.5f;
  settings.atmosphere.fog_end = 8.0f;
  settings.atmosphere.fog_strength = 0.42f;
  settings.reflections.enabled = true;
  settings.reflections.static_local_probes = true;
  settings.reflections.probe_resolution = 16u;
  settings.reflections.max_active_probes = 1u;
  settings.clustered_lighting.enabled = true;
  settings.clustered_lighting.cluster_count_x = 4u;
  settings.clustered_lighting.cluster_count_y = 3u;
  settings.clustered_lighting.cluster_count_z = 4u;

  aster::RenderDevice renderer;
  renderer.initialize();
  renderer.prepareScene(scene);
  (void)renderer.render(scene, camera, settings, 80, 56, 0.0);

  const aster::FrameForensics &forensics = renderer.lastFrameForensics();
  const auto capture_for = [&forensics](const aster::RenderGraphResource resource) {
    return std::find_if(forensics.captures.begin(), forensics.captures.end(),
                        [resource](const aster::FrameDebugCapture &capture) {
                          return capture.resource == resource && capture.available;
                        });
  };
  const auto shadow = capture_for(aster::RenderGraphResource::ShadowAtlas);
  const auto fog = capture_for(aster::RenderGraphResource::VolumetricFog);
  const auto reflection = capture_for(aster::RenderGraphResource::ReflectionProbes);
  const auto final = capture_for(aster::RenderGraphResource::CaptureReadback);
  assert(shadow != forensics.captures.end());
  assert(shadow->width == 64u && shadow->height == 64u);
  assert(shadow->row_stride_bytes == shadow->width * 4u);
  assert(shadow->content_hash != 0u && !shadow->rgba8.empty());
  assert(fog != forensics.captures.end());
  assert(fog->width == 20u && fog->height == 14u);
  assert(fog->content_hash != 0u && !fog->rgba8.empty());
  assert(reflection != forensics.captures.end());
  assert(reflection->width == 96u && reflection->height == 16u);
  assert(reflection->content_hash != 0u && !reflection->rgba8.empty());
  assert(final != forensics.captures.end());
  assert(final->width == 80u && final->height == 56u);
  assert(final->rgba8.size() == 80u * 56u * 4u);
  assert(forensics.mesh_visibility.size() == scene.objects().size());
  assert(!forensics.object_clusters.empty());
  bool saw_caster_visibility = false;
  for (const aster::MeshVisibilityTrace &visibility : forensics.mesh_visibility) {
    if (visibility.object_name == "shadow atlas caster") {
      saw_caster_visibility = visibility.visible && visibility.reason == "visible" &&
                              visibility.pass == aster::RenderGraphPass::Opaque;
    }
  }
  assert(saw_caster_visibility);

  bool saw_placeholder_warning = false;
  for (const aster::FrameDiagnosticEvent &event : forensics.events) {
    saw_placeholder_warning =
        saw_placeholder_warning ||
        (event.kind == aster::FrameDiagnosticKind::CapabilityMismatch &&
         (event.pass == "shadow-atlas" || event.pass == "volumetric-fog" ||
          event.pass == "reflection-probe"));
  }
  assert(!saw_placeholder_warning);
  const aster::RenderBackendCapabilities capabilities = renderer.backendCapabilities();
  assert((capabilities.graph_resource_mask &
          aster::renderGraphResourceBit(aster::RenderGraphResource::ShadowAtlas)) != 0u);
  assert(capabilities.capability_table.shadow_maps);

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

aster::OrbitCamera retroStyleTestCamera() {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.55f, 0.0f};
  camera.yaw = aster::radians(42.0f);
  camera.pitch = aster::radians(14.0f);
  camera.radius = 6.2f;
  camera.vertical_fov = aster::radians(44.0f);
  return camera;
}

struct PixelStats {
  double mean_r = 0.0;
  double mean_g = 0.0;
  double mean_b = 0.0;
  double mean_luma = 0.0;
  std::size_t unique_rgb = 0u;
};

PixelStats measurePixels(const aster::SoftwareFrameBuffer &framebuffer) {
  PixelStats stats;
  std::unordered_set<std::uint32_t> unique;
  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  const std::size_t pixels = std::max<std::size_t>(rgba.size() / 4u, 1u);
  for (std::size_t i = 0; i + 3u < rgba.size(); i += 4u) {
    const double r = static_cast<double>(rgba[i + 0u]) / 255.0;
    const double g = static_cast<double>(rgba[i + 1u]) / 255.0;
    const double b = static_cast<double>(rgba[i + 2u]) / 255.0;
    stats.mean_r += r;
    stats.mean_g += g;
    stats.mean_b += b;
    stats.mean_luma += r * 0.2126 + g * 0.7152 + b * 0.0722;
    unique.insert((static_cast<std::uint32_t>(rgba[i + 0u]) << 16u) |
                  (static_cast<std::uint32_t>(rgba[i + 1u]) << 8u) |
                  static_cast<std::uint32_t>(rgba[i + 2u]));
  }
  stats.mean_r /= static_cast<double>(pixels);
  stats.mean_g /= static_cast<double>(pixels);
  stats.mean_b /= static_cast<double>(pixels);
  stats.mean_luma /= static_cast<double>(pixels);
  stats.unique_rgb = unique.size();
  return stats;
}

void testRetroStyleNeutralSoftwarePreviewMatchesDefault() {
  aster::RendererSettings baseline;
  baseline.sun_light.enabled = true;
  baseline.sun_light.intensity = 1.4f;
  baseline.ambient_strength = 0.22f;
  baseline.ambient_floor = 0.02f;

  aster::RendererSettings explicit_neutral = baseline;
  aster::applyRenderStyleProfile(
      explicit_neutral, aster::makeRenderStyleProfile(aster::RenderStylePreset::Neutral));

  const aster::OrbitCamera camera = retroStyleTestCamera();
  const aster::Scene scene = aster::makeIndustrialPipeScene();
  const aster::SoftwareFrameBuffer default_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 42,
                                    .height = 28,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = baseline});
  const aster::SoftwareFrameBuffer neutral_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 42,
                                    .height = 28,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = explicit_neutral});
  assert(default_frame.rgba8() == neutral_frame.rgba8());
}

void testRetroStyleSoftwarePreviewEffects() {
  const aster::OrbitCamera camera = retroStyleTestCamera();
  const aster::Scene scene = aster::makeIndustrialPipeScene();

  aster::RendererSettings neutral;
  neutral.sun_light.enabled = true;
  neutral.sun_light.intensity = 1.7f;
  neutral.ambient_strength = 0.26f;
  neutral.ambient_floor = 0.02f;

  aster::RendererSettings retro = neutral;
  aster::applyRenderStyleProfile(
      retro, aster::makeRenderStyleProfile(aster::RenderStylePreset::RetroHorrorReadable));

  const aster::SoftwareFrameBuffer neutral_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 54,
                                    .height = 34,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = neutral});
  const aster::SoftwareFrameBuffer retro_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 54,
                                    .height = 34,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = retro});
  const PixelStats neutral_stats = measurePixels(neutral_frame);
  const PixelStats retro_stats = measurePixels(retro_frame);
  assert(retro_stats.mean_r > retro_stats.mean_g);
  assert(retro_stats.mean_r > retro_stats.mean_b);
  assert(retro_stats.mean_luma > neutral_stats.mean_luma * 0.20);
  assert(retro_stats.unique_rgb < neutral_stats.unique_rgb);
}

void testRetroStyleEmissiveSoftwarePreviewGain() {
  aster::Scene scene;
  aster::RenderObject object;
  object.name = "emissive style probe";
  object.primitive = aster::MeshPrimitive::Sphere;
  object.transform.position = {0.0f, 0.55f, 0.0f};
  object.transform.scale = {0.85f, 0.85f, 0.85f};
  object.material = aster::makeMaterial({.base_color = {0.12f, 0.08f, 0.06f},
                                         .emission_color = {1.0f, 0.18f, 0.06f},
                                         .emission_strength = 0.42f});
  scene.objects().push_back(object);

  aster::RendererSettings neutral;
  neutral.sun_light.enabled = false;
  neutral.ambient_strength = 0.05f;
  neutral.ambient_floor = 0.01f;

  aster::RendererSettings boosted = neutral;
  boosted.style.emissive_gain = 4.0f;

  const aster::OrbitCamera camera = retroStyleTestCamera();
  const aster::SoftwareFrameBuffer neutral_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 42,
                                    .height = 30,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = neutral});
  const aster::SoftwareFrameBuffer boosted_frame =
      aster::renderSoftwarePreview(scene, camera,
                                   {.width = 42,
                                    .height = 30,
                                    .samples_per_axis = 1,
                                    .frame_seconds = 0.0,
                                    .settings = boosted});
  assert(measurePixels(boosted_frame).mean_luma > measurePixels(neutral_frame).mean_luma * 1.10);
}

void testMaterialRenderPolicies() {
  aster::Material opaque = aster::makeMaterial({.base_color = {0.4f, 0.3f, 0.2f}});
  assert(aster::classifyMaterialRenderQueue(opaque) == aster::MaterialRenderQueue::Opaque);
  assert(aster::materialWritesDepth(opaque));
  assert(aster::allowsCameraOcclusionFade(opaque));
  assert(opaque.receives_shadows);

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

  aster::Material no_shadow_receiver = aster::makeMaterial({.receives_shadows = false});
  assert(!no_shadow_receiver.receives_shadows);

  aster::Material support = aster::makeSupportSurfaceMaterial(water);
  assert(support.render_role == aster::MaterialRenderRole::SupportSurface);
  assert(aster::classifyMaterialRenderQueue(support) == aster::MaterialRenderQueue::Opaque);
  assert(aster::materialWritesDepth(support));
  assert(!aster::allowsCameraOcclusionFade(support));

  aster::RenderObject caster;
  caster.material = opaque;
  assert(aster::renderObjectCastsShadows(caster));
  caster.material = water;
  assert(!aster::renderObjectCastsShadows(caster));
  caster.material = opaque;
  caster.casts_shadows = false;
  assert(!aster::renderObjectCastsShadows(caster));

  aster::Scene scene;
  scene.reflectionProbes().push_back({.name = "local cave probe",
                                      .position = {1.0f, 2.0f, 3.0f},
                                      .influence_radius = 6.0f,
                                      .intensity = 0.85f});
  assert(scene.reflectionProbes().size() == 1u);
  assert(scene.reflectionProbes().front().influence_radius == 6.0f);
}

void testRuntimeLightPolicy() {
  aster::LightRig lights;
  for (int i = 0; i < 12; ++i) {
    lights.push_back({.position = {static_cast<float>(i), 0.0f, 0.0f},
                      .color = {1.0f + static_cast<float>(i), 1.0f, 1.0f},
                      .intensity = 1.0f + static_cast<float>(i) * 0.25f,
                      .source_radius = 0.2f});
  }

  const std::vector<aster::Light> selected =
      aster::selectRenderLights(lights, {10.0f, 0.0f, 0.0f}, {.max_point_lights = 5u});
  assert(selected.size() == 5u);
  assert(selected.front().position.x >= 7.0f);

  const std::vector<aster::Light> unbounded = aster::selectRenderLights(
      lights, {}, {.max_point_lights = aster::kRenderLightUniformCapacity + 32u});
  assert(unbounded.size() == lights.size());

  aster::OrbitCamera camera;
  camera.target = {6.0f, 0.0f, 0.0f};
  camera.radius = 12.0f;
  aster::ClusteredLightPolicy clustered;
  clustered.enabled = true;
  clustered.cluster_count_x = 4u;
  clustered.cluster_count_y = 3u;
  clustered.cluster_count_z = 2u;
  clustered.max_visible_lights = 7u;
  clustered.max_lights_per_cluster = 3u;
  const aster::ClusteredLightGrid grid =
      aster::buildClusteredLightGrid(lights, camera, 128, 72, clustered);
  assert(grid.cluster_count_x == 4u);
  assert(grid.cluster_count_y == 3u);
  assert(grid.cluster_count_z == 2u);
  assert(grid.cluster_offsets.size() == 25u);
  assert(grid.visible_lights.size() == 7u);
  assert(grid.fallback_used);
  assert(!grid.assignments.empty());
  assert(grid.cluster_offsets.back() == grid.assignments.size());
  for (std::size_t i = 1u; i < grid.assignments.size(); ++i) {
    assert(grid.assignments[i - 1u].cluster_index <= grid.assignments[i].cluster_index);
  }

  aster::ClusteredLightPolicy packed_policy = clustered;
  packed_policy.max_visible_lights = 16u;
  packed_policy.max_lights_per_cluster = 12u;
  const aster::ClusteredLightGrid packed_grid =
      aster::buildClusteredLightGrid(lights, camera, 128, 72, packed_policy);
  const aster::ClusteredLightFrameData frame_data =
      aster::buildClusteredLightFrameData(packed_grid);
  assert(frame_data.visible_lights.size() > aster::kDefaultRenderLightBudget);
  assert(frame_data.cluster_count_x == packed_policy.cluster_count_x);
  assert(frame_data.cluster_offsets.size() ==
         (packed_policy.cluster_count_x * packed_policy.cluster_count_y *
              packed_policy.cluster_count_z +
          1u));
  assert(!frame_data.light_indices.empty());
  assert(frame_data.visible_lights_hash != 0u);
  assert(frame_data.assignments_hash != 0u);
  assert(!frame_data.fallback_used);
  assert(!frame_data.overflowed);
}

void testRhiResourceRegistryContract() {
  aster::rhi::ResourceRegistry registry;
  const aster::rhi::BufferHandle vertex_buffer = registry.createBuffer(
      {.byte_size = 256u,
       .usage = aster::rhi::bufferUsageBit(aster::rhi::BufferUsage::Vertex),
       .debug_label = "unit.vertex-buffer"});
  assert(vertex_buffer.valid());
  assert(registry.contains(vertex_buffer));
  assert(registry.label(vertex_buffer) == "unit.vertex-buffer");
  assert(registry.setLabel(vertex_buffer, "unit.renamed-buffer"));
  assert(registry.label(vertex_buffer) == "unit.renamed-buffer");

  const aster::rhi::BufferHandle regenerated = registry.invalidate(vertex_buffer);
  assert(regenerated.valid());
  assert(!registry.contains(vertex_buffer));
  assert(registry.contains(regenerated));
  assert(regenerated.id == vertex_buffer.id);
  assert(regenerated.generation == vertex_buffer.generation + 1u);

  const aster::rhi::ImageHandle color = registry.createImage(
      {.format = aster::rhi::ImageFormat::Bgra8Unorm,
       .extent = {.width = 64u, .height = 64u, .depth = 1u},
       .usage = aster::rhi::imageUsageBit(aster::rhi::ImageUsage::ColorAttachment),
       .debug_label = "unit.color"});
  const aster::rhi::SamplerHandle sampler =
      registry.createSampler({.debug_label = "unit.sampler"});
  const aster::rhi::GraphicsPipelineHandle pipeline =
      registry.createGraphicsPipeline({.debug_label = "unit.pipeline"});
  assert(registry.contains(color));
  assert(registry.contains(sampler));
  assert(registry.contains(pipeline));

  const aster::rhi::ResourceRegistryStats stats = registry.stats();
  assert(stats.live_resources == 4u);
  assert(stats.buffers == 1u);
  assert(stats.images == 1u);
  assert(stats.samplers == 1u);
  assert(stats.pipelines == 1u);

  assert(registry.destroy(regenerated));
  assert(!registry.contains(regenerated));
  assert(registry.stats().retired_resources == 1u);
  registry.clearRetired();
  assert(registry.stats().retired_resources == 0u);
}

void testRhiExplicitGpuContracts() {
  aster::rhi::ResourceBarrier upload_to_sample =
      aster::rhi::completeResourceBarrier({.resource_id = 42u,
                                           .before = aster::rhi::ResourceState::CopyDestination,
                                           .after = aster::rhi::ResourceState::ShaderRead,
                                           .subresources =
                                               {.aspect_mask = aster::rhi::textureAspectBit(
                                                    aster::rhi::TextureAspect::Color),
                                                .base_mip = 0u,
                                                .mip_count = 4u,
                                                .base_layer = 0u,
                                                .layer_count = 6u},
                                           .source_queue = aster::rhi::QueueKind::Copy,
                                           .destination_queue = aster::rhi::QueueKind::Graphics});
  assert((upload_to_sample.source_stage_mask &
          aster::rhi::pipelineStageBit(aster::rhi::PipelineStage::Transfer)) != 0u);
  assert((upload_to_sample.destination_stage_mask &
          aster::rhi::pipelineStageBit(aster::rhi::PipelineStage::FragmentShader)) != 0u);
  assert((upload_to_sample.destination_access_mask &
          aster::rhi::resourceAccessBit(aster::rhi::ResourceAccess::ShaderRead)) != 0u);
  assert(upload_to_sample.old_layout == aster::rhi::ImageLayout::TransferDestination);
  assert(upload_to_sample.new_layout == aster::rhi::ImageLayout::ShaderReadOnly);
  assert(aster::rhi::transfersQueueOwnership(upload_to_sample));

  aster::rhi::GraphicsPipelineDesc pipeline;
  pipeline.layout = {7u, 1u};
  pipeline.vertex_shader = {8u, 1u};
  pipeline.fragment_shader = {9u, 1u};
  pipeline.rasterizer.cull_mode = aster::rhi::CullMode::Back;
  pipeline.rasterizer.front_face = aster::rhi::FrontFace::CounterClockwise;
  assert(pipeline.depth_stencil.depth_compare == aster::rhi::CompareOp::GreaterOrEqual);
  pipeline.depth_stencil.depth_compare = aster::rhi::CompareOp::LessOrEqual;
  pipeline.multisample.sample_count = 4u;
  pipeline.vertex_input.bindings.push_back(
      {.binding = 0u, .stride = 32u, .input_rate = aster::rhi::VertexInputRate::Vertex});
  pipeline.vertex_input.attributes.push_back(
      {.location = 0u,
       .binding = 0u,
       .format = aster::rhi::VertexFormat::Float32x3,
       .offset = 0u});
  pipeline.vertex_input.attributes.push_back(
      {.location = 1u,
       .binding = 0u,
       .format = aster::rhi::VertexFormat::Float32x3,
       .offset = 12u});
  pipeline.render_pass.color_formats.push_back(aster::rhi::ImageFormat::Bgra8Unorm);
  pipeline.render_pass.depth_format = aster::rhi::ImageFormat::Depth32Float;
  pipeline.render_pass.sample_count = 4u;
  pipeline.color_blend_attachments.push_back(
      {.blend_enabled = true,
       .source_color_factor = aster::rhi::BlendFactor::SourceAlpha,
       .destination_color_factor = aster::rhi::BlendFactor::OneMinusSourceAlpha,
       .color_op = aster::rhi::BlendOp::Add,
       .source_alpha_factor = aster::rhi::BlendFactor::One,
       .destination_alpha_factor = aster::rhi::BlendFactor::OneMinusSourceAlpha,
       .alpha_op = aster::rhi::BlendOp::Add,
       .color_write_mask = aster::rhi::kColorWriteAll});
  const std::uint64_t solid_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
  pipeline.rasterizer.polygon_mode = aster::rhi::PolygonMode::Line;
  const std::uint64_t wire_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
  assert(solid_key != 0u);
  assert(wire_key != solid_key);
  pipeline.rasterizer.polygon_mode = aster::rhi::PolygonMode::Fill;
  pipeline.depth_test = false;
  const std::uint64_t no_depth_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
  assert(no_depth_key != solid_key);
  pipeline.depth_test = true;
  pipeline.rasterizer.depth_bias_enabled = true;
  pipeline.rasterizer.depth_bias_constant = 1.0f;
  pipeline.rasterizer.depth_bias_slope = 0.5f;
  const std::uint64_t biased_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
  assert(biased_key != solid_key);
  pipeline.rasterizer.depth_bias_constant = 2.0f;
  const std::uint64_t biased_constant_key = aster::rhi::graphicsPipelineCacheKey(pipeline);
  assert(biased_constant_key != biased_key);

  aster::rhi::PipelineLayoutDesc material_layout;
  material_layout.descriptor_ranges.push_back(
      {.kind = aster::rhi::DescriptorRangeKind::SampledImage,
       .binding = 0u,
       .count = 10u,
       .stage_mask = static_cast<std::uint32_t>(
           aster::rhi::pipelineStageBit(aster::rhi::PipelineStage::FragmentShader))});
  material_layout.descriptor_ranges.push_back(
      {.kind = aster::rhi::DescriptorRangeKind::Sampler,
       .binding = 10u,
       .count = 1u,
       .stage_mask = static_cast<std::uint32_t>(
           aster::rhi::pipelineStageBit(aster::rhi::PipelineStage::FragmentShader))});
  material_layout.descriptor_range_count =
      static_cast<std::uint32_t>(material_layout.descriptor_ranges.size());
  const std::uint64_t material_layout_hash = aster::rhi::descriptorLayoutHash(material_layout);
  material_layout.descriptor_ranges.front().count = 9u;
  assert(material_layout_hash != 0u);
  assert(aster::rhi::descriptorLayoutHash(material_layout) != material_layout_hash);

  aster::rhi::FramebufferDesc framebuffer;
  framebuffer.width = 128u;
  framebuffer.height = 64u;
  framebuffer.color_attachments.push_back({.format = aster::rhi::ImageFormat::Bgra8Unorm,
                                           .load_op = aster::rhi::AttachmentLoadOp::Clear,
                                           .store_op = aster::rhi::AttachmentStoreOp::Store,
                                           .initial_state = aster::rhi::ResourceState::Undefined,
                                           .final_state =
                                               aster::rhi::ResourceState::ColorAttachment,
                                           .transient = true});
  framebuffer.has_depth_stencil_attachment = true;
  assert(framebuffer.color_attachments.front().transient);
  assert(framebuffer.depth_stencil_attachment.subresources.aspect_mask ==
         aster::rhi::textureAspectBit(aster::rhi::TextureAspect::Depth));
}

void testFrameGraphContract() {
  const aster::RenderPassRegistry registry = aster::makeDefaultRenderPassRegistry();
  assert(registry.validate().empty());
  const aster::RenderGraphPassDeclaration *shadow_decl =
      registry.find(aster::RenderGraphPass::ShadowAtlas);
  assert(shadow_decl != nullptr);
  assert(shadow_decl->outputs.size() == 1u);
  assert(shadow_decl->outputs.front().resource == aster::RenderGraphResource::ShadowAtlas);
  assert(shadow_decl->outputs.front().load_op == aster::rhi::AttachmentLoadOp::Clear);
  assert(shadow_decl->debug_capture == aster::RenderGraphDebugCapturePolicy::OnRequest);
  assert(aster::renderGraphExecutorKeyName(shadow_decl->executor) == "shadow-atlas");

  const aster::FixedRenderGraph graph = aster::makeFixedRenderGraph();
  assert(graph.valid());
  assert(graph.validation_errors.empty());
  assert(graph.resources.size() == 8u);
  assert(graph.resources[0].name == "scene-color");
  assert(graph.resources[0].desc.kind == aster::framegraph::ResourceKind::Image);
  assert(graph.resources[0].desc.lifetime == aster::framegraph::ResourceLifetime::Transient);
  assert(graph.resources[2].name == "light-clusters");
  assert(graph.resources[2].desc.kind == aster::framegraph::ResourceKind::Buffer);
  assert((graph.resources[2].desc.usage &
          aster::rhi::bufferUsageBit(aster::rhi::BufferUsage::Storage)) != 0u);
  assert(graph.resources[2].desc.byte_size >= 64u * 1024u);
  assert(graph.resources[6].name == "ui-overlay");
  assert(graph.resources[6].desc.lifetime == aster::framegraph::ResourceLifetime::Imported);
  assert(graph.resources[7].name == "capture-readback");
  assert(graph.resources[7].desc.lifetime == aster::framegraph::ResourceLifetime::Readback);
  assert(graph.passes.size() == 11u);
  assert(graph.passes[0].name == "scene-color-depth");
  assert(graph.passes[1].name == "light-cull");
  assert(graph.passes[2].name == "shadow-atlas");
  assert(graph.passes[3].name == "opaque");
  assert(graph.passes[4].name == "contact-shadow");
  assert(graph.passes[5].name == "scene-lighting");
  assert(graph.passes[6].name == "volumetric-fog");
  assert(graph.passes[7].name == "reflection-probe");
  assert(graph.passes[8].name == "transparent");
  assert(graph.passes[9].name == "ui-composite");
  assert(graph.passes[10].name == "capture");
  const std::uint32_t color =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor);
  const std::uint32_t depth =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth);
  const std::uint32_t light_clusters =
      aster::renderGraphResourceBit(aster::RenderGraphResource::LightClusters);
  const std::uint32_t shadow_atlas =
      aster::renderGraphResourceBit(aster::RenderGraphResource::ShadowAtlas);
  const std::uint32_t volumetric_fog =
      aster::renderGraphResourceBit(aster::RenderGraphResource::VolumetricFog);
  const std::uint32_t reflection_probes =
      aster::renderGraphResourceBit(aster::RenderGraphResource::ReflectionProbes);
  const std::uint32_t ui = aster::renderGraphResourceBit(aster::RenderGraphResource::UiOverlay);
  const std::uint32_t capture =
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  assert(graph.passes[0].read_mask == 0u && graph.passes[0].write_mask == (color | depth));
  assert(graph.passes[1].read_mask == depth && graph.passes[1].write_mask == light_clusters);
  assert(graph.passes[2].read_mask == light_clusters && graph.passes[2].write_mask == shadow_atlas);
  assert(graph.passes[3].read_mask == (color | depth | light_clusters | shadow_atlas) &&
         graph.passes[3].write_mask == (color | depth));
  assert(graph.passes[4].read_mask == (color | depth) &&
         graph.passes[4].write_mask == (color | depth));
  assert(graph.passes[5].read_mask == (color | depth | light_clusters) &&
         graph.passes[5].write_mask == color);
  assert(graph.passes[6].read_mask == (depth | light_clusters) &&
         graph.passes[6].write_mask == volumetric_fog);
  assert(graph.passes[7].read_mask == color && graph.passes[7].write_mask == reflection_probes);
  assert(graph.passes[8].read_mask ==
             (color | depth | light_clusters | volumetric_fog | reflection_probes) &&
         graph.passes[8].write_mask == color);
  assert(graph.passes[9].read_mask == (color | ui) && graph.passes[9].write_mask == color);
  assert(graph.passes[10].read_mask == color && graph.passes[10].write_mask == capture);
  assert(graph.transient_resource_count == 6u);
  assert(!graph.barriers.empty());
  assert(!graph.resource_barriers.empty());
  assert(graph.resource_barriers.size() == graph.barriers.size());
  assert(!graph.alias_groups.empty());
  assert(!graph.descriptor_requirements.empty());
  assert(!graph.passes[3].attachments.empty());
  assert(!graph.passes[3].pipeline_compatibility.color_formats.empty());
  assert(graph.passes[1].descriptor_requirements.front().kind ==
         aster::rhi::DescriptorRangeKind::StorageBuffer);
  assert(graph.passes[1].debug_marker_name == "light-cull");
  assert(graph.passes[1].timestamp_zone_index == 1u);
  assert(graph.resources[2].physical_allocation_id != 0u);
  const aster::framegraph::TransientResourceAllocationPlan allocation_plan =
      aster::framegraph::TransientResourceAllocator{}.allocate(graph);
  assert(!allocation_plan.physical_allocations.empty());
  assert(allocation_plan.stats.physical_allocations == allocation_plan.physical_allocations.size());
  assert(allocation_plan.stats.transient_bytes > 0u);
  const std::string dump = aster::framegraph::dumpFrameGraph(graph);
  assert(dump.find("scene-color-depth") != std::string::npos);
  assert(dump.find("expanded-barriers") != std::string::npos);
  assert(dump.find("descriptor-requirements") != std::string::npos);
  std::vector<aster::RenderGraphPass> executed_passes;
  const std::size_t executed = aster::executeFixedRenderGraph(
      graph, [&executed_passes](const aster::RenderGraphPassInvocation &invocation) {
        executed_passes.push_back(invocation.semantic);
        assert(invocation.pass != nullptr);
        assert(invocation.declaration != nullptr);
        assert(invocation.declaration->name == invocation.pass->name);
      });
  assert(executed == graph.passes.size());
  assert(executed_passes.size() == graph.passes.size());
  assert(executed_passes[0] == aster::RenderGraphPass::SceneColorDepth);
  assert(executed_passes[1] == aster::RenderGraphPass::LightCull);
  assert(executed_passes[3] == aster::RenderGraphPass::Opaque);
  assert(executed_passes[4] == aster::RenderGraphPass::ContactShadow);
  assert(executed_passes[8] == aster::RenderGraphPass::Transparent);

  aster::framegraph::FrameGraph invalid_graph;
  const auto dangling = invalid_graph.addResource("dangling", {});
  invalid_graph.addPass("invalid-read").reads(dangling);
  const aster::framegraph::CompiledFrameGraph invalid =
      aster::framegraph::compileFrameGraph(invalid_graph);
  assert(!invalid.valid());
  assert(!invalid.validation_errors.empty());

  aster::framegraph::FrameGraph queue_graph;
  const auto queue_buffer = queue_graph.addResource(
      "queue-buffer",
      {.kind = aster::framegraph::ResourceKind::Buffer,
       .lifetime = aster::framegraph::ResourceLifetime::Transient,
       .usage = aster::rhi::bufferUsageBit(aster::rhi::BufferUsage::Storage),
       .byte_size = 256u,
       .stride = 16u});
  queue_graph.addPass("compute-write").queue(aster::rhi::QueueKind::Compute).writes(queue_buffer);
  queue_graph.addPass("graphics-read").queue(aster::rhi::QueueKind::Graphics).reads(queue_buffer);
  const aster::framegraph::CompiledFrameGraph queued =
      aster::framegraph::compileFrameGraph(queue_graph);
  assert(queued.valid());
  assert(std::any_of(queued.resource_barriers.begin(), queued.resource_barriers.end(),
                     [](const aster::rhi::ResourceBarrier &barrier) {
                       return aster::rhi::transfersQueueOwnership(barrier);
                     }));

  aster::framegraph::FrameGraphCompileOptions backend_options;
  backend_options.backend_resource_mask =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor);
  backend_options.required_resource_mask =
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  backend_options.cull_unsupported_passes = true;
  const aster::FixedRenderGraph backend_graph = aster::makeFixedRenderGraph(backend_options);
  assert(!backend_graph.valid());
  assert(!backend_graph.validation_errors.empty());
  assert(std::any_of(backend_graph.passes.begin(), backend_graph.passes.end(),
                     [](const aster::framegraph::CompiledPass &pass) {
                       return pass.culled;
                     }));

  assert(aster::renderGraphResourceName(aster::RenderGraphResource::UiOverlay) == "ui-overlay");
  assert(aster::renderGraphResourceLifetimeName(aster::RenderGraphResourceLifetime::Readback) ==
         "readback");
}

void testBackendKindContract() {
  assert(aster::renderBackendKindName(aster::RenderBackendKind::SoftwareReference) ==
         "software-reference");
  assert(aster::renderBackendKindName(aster::RenderBackendKind::Metal) == "metal");
  assert(aster::renderBackendKindName(aster::RenderBackendKind::D3D12) == "d3d12");
  assert(aster::renderBackendKindName(aster::RenderBackendKind::Null) == "null");
  assert(aster::renderBackendKindName(aster::RenderBackendKind::Unknown) == "unknown");
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

  aster::Material decal = translucent;
  decal.depth_policy = {.layer = aster::RenderDepthLayer::Decal,
                        .constant_bias = 0.00008f,
                        .slope_bias = 0.00012f};
  const aster::CompiledMaterial decal_compiled =
      aster::compileMaterialForRendering(decal, false, "material.decal");
  assert((decal_compiled.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::DepthBias)) != 0u);
  assert(decal_compiled.permutation_key != translucent_compiled.permutation_key);
  assert(decal_compiled.pipeline_tag.find(".depth-bias") != std::string::npos);

  aster::Material fur =
      aster::makeMaterial({.surface_profile = aster::MaterialSurfaceProfile::OrganicFiber,
                           .surface_pattern = aster::SurfacePattern::FurFibers});
  aster::Material twig = fur;
  twig.surface_pattern = aster::SurfacePattern::TwigNest;
  const aster::CompiledMaterial fur_compiled =
      aster::compileMaterialForRendering(fur, false, "material.organic-fiber");
  const aster::CompiledMaterial twig_compiled =
      aster::compileMaterialForRendering(twig, false, "material.organic-fiber");
  assert(fur_compiled.permutation_key == twig_compiled.permutation_key);
  assert(fur_compiled.permutation_flags == twig_compiled.permutation_flags);

  aster::Material cave_alias =
      aster::makeMaterial({.surface_pattern = aster::SurfacePattern::CaveRock});
  aster::Material coal_alias = cave_alias;
  coal_alias.surface_pattern = aster::SurfacePattern::CoalVein;
  assert(aster::resolveMaterialSurfaceProfile(cave_alias) ==
         aster::MaterialSurfaceProfile::StratifiedRock);
  assert(aster::resolveMaterialSurfaceProfile(coal_alias) ==
         aster::MaterialSurfaceProfile::MineralVein);
  assert(aster::compileMaterialForRendering(cave_alias, false, "material.alias")
             .permutation_key !=
         aster::compileMaterialForRendering(coal_alias, false, "material.alias")
             .permutation_key);
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
  assert(render_scene.ir().content_hash != 0u);
  assert(render_scene.ir().objects.size() == scene.objects().size());
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
  assert(plan.source_ir_hash == render_scene.ir().content_hash);
  assert(!plan.groups.empty());
  assert(plan.groups.front().signature.key.value != 0u);

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
    {"showcase_lab_scenes", testShowcaseLabSceneContracts},
    {"software_preview_renderer", testSoftwarePreviewRendererProducesImage},
    {"software_depth_policy_coplanar", testSoftwareDepthPolicySeparatesCoplanarAttachments},
    {"software_depth_policy_object_order", testSoftwareDepthPolicyIsStableAcrossObjectOrder},
    {"prepare_scene_custom_mesh_cache", testPrepareSceneInvalidatesCustomMeshCache},
    {"frame_math_diagnostics", testFrameMathDiagnostics},
    {"frame_debugger_material_binding_trace", testFrameDebuggerMaterialBindingTrace},
    {"software_reference_frame_resource_captures", testSoftwareReferenceFrameResourceCaptures},
    {"retro_style_neutral_preview", testRetroStyleNeutralSoftwarePreviewMatchesDefault},
    {"retro_style_preview_effects", testRetroStyleSoftwarePreviewEffects},
    {"retro_style_emissive_gain", testRetroStyleEmissiveSoftwarePreviewGain},
    {"material_render_policies", testMaterialRenderPolicies},
    {"runtime_light_policy", testRuntimeLightPolicy},
    {"rhi_resource_registry", testRhiResourceRegistryContract},
    {"rhi_explicit_gpu_contracts", testRhiExplicitGpuContracts},
    {"frame_graph", testFrameGraphContract},
    {"backend_kinds", testBackendKindContract},
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
