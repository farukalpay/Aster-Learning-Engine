// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/asset/scene_asset_importer.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/core/profiler.hpp"
#include "aster/game/animation_system.hpp"
#include "aster/game/creature_motion.hpp"
#include "aster/game/equipment_system.hpp"
#include "aster/game/interaction_system.hpp"
#include "aster/game/inventory_system.hpp"
#include "aster/game/item_system.hpp"
#include "aster/game/light_system.hpp"
#include "aster/game/lumen_run.hpp"
#include "aster/game/particle_system.hpp"
#include "aster/game/player_motion.hpp"
#include "aster/game/third_person_camera.hpp"
#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/brush_level_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/castle_course.hpp"
#include "aster/geometry/cave_system.hpp"
#include "aster/geometry/generated_scenery.hpp"
#include "aster/geometry/mesh_projection.hpp"
#include "aster/geometry/nature_mesh.hpp"
#include "aster/geometry/stroke_mesh.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/geometry/terrain_sculpt.hpp"
#include "aster/geometry/voxel_cave.hpp"
#include "aster/geometry/voxel_structure.hpp"
#include "aster/geometry/water_mesh.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/math/color.hpp"
#include "aster/math/mat4.hpp"
#include "aster/math/transform.hpp"
#include "aster/net/net_message.hpp"
#include "aster/net/node_router.hpp"
#include "aster/physics/climb_locomotion.hpp"
#include "aster/physics/contact_query.hpp"
#include "aster/physics/fluid_locomotion.hpp"
#include "aster/physics/physics_world.hpp"
#include "aster/physics/placement_validation.hpp"
#include "aster/physics/surface_support.hpp"
#include "aster/physics/terrain_contact.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/render_scene.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/avatar.hpp"
#include "aster/scene/scene.hpp"
#include "aster/scene/scene_coherence.hpp"
#include "aster/scene/scene_trace.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void expectNear(const float actual, const float expected, const float tolerance) {
  assert(std::abs(actual - expected) <= tolerance);
}

template <typename T> void appendBinaryScalar(std::vector<std::uint8_t> &bytes, const T value) {
  const auto *data = reinterpret_cast<const std::uint8_t *>(&value);
  bytes.insert(bytes.end(), data, data + sizeof(T));
}

template <typename T, std::size_t N>
void appendBinaryArray(std::vector<std::uint8_t> &bytes, const std::array<T, N> &values) {
  for (const T value : values) {
    appendBinaryScalar(bytes, value);
  }
}

void writeGeneratedNormalTangentBinary(const std::filesystem::path &path) {
  std::vector<std::uint8_t> bytes;
  appendBinaryArray(bytes, std::array<float, 12>{-0.5f, 0.0f, -0.5f, 0.5f, 0.0f, -0.5f, 0.5f, 0.0f,
                                                 0.5f, -0.5f, 0.0f, 0.5f});
  appendBinaryArray(bytes, std::array<float, 12>{0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                                                 0.0f, 0.0f, 1.0f, 0.0f});
  appendBinaryArray(bytes, std::array<float, 8>{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f});
  appendBinaryArray(bytes, std::array<std::uint16_t, 6>{0u, 1u, 2u, 0u, 2u, 3u});

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  assert(out.good());
}

std::filesystem::path writeGeneratedNormalTangentFixture() {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "aster_generated_normal_tangent_fixture";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  writeGeneratedNormalTangentBinary(directory / "normal_tangent_probe.bin");

  const std::filesystem::path scene_path = directory / "normal_tangent_probe.scene";
  std::ofstream scene(scene_path);
  scene << R"json({
    "asset": { "generator": "Aster generated scene fixture", "version": "2.0" },
    "scene": 0,
    "scenes": [{ "nodes": [0] }],
    "nodes": [{ "mesh": 0, "translation": [0.0, 0.15, 0.0] }],
    "meshes": [{
      "name": "generated_normal_tangent_probe",
      "primitives": [{
        "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 },
        "indices": 3,
        "mode": 4,
        "material": 0
      }]
    }],
    "materials": [{
      "name": "generated_check_material",
      "pbrMetallicRoughness": {
        "baseColorFactor": [0.8, 0.55, 0.36, 1.0],
        "baseColorTexture": { "index": 0 },
        "metallicFactor": 0.0,
        "roughnessFactor": 0.62,
        "metallicRoughnessTexture": { "index": 1 }
      },
      "normalTexture": { "index": 2 },
      "occlusionTexture": { "index": 1 },
      "doubleSided": true,
      "alphaMode": "MASK",
      "emissiveFactor": [0.0, 0.0, 0.0]
    }],
    "textures": [{ "source": 0 }, { "source": 1 }, { "source": 2 }],
    "images": [{ "uri": "base.png" }, { "uri": "orm.png" }, { "uri": "normal.png" }],
    "buffers": [{ "byteLength": 140, "uri": "normal_tangent_probe.bin" }],
    "bufferViews": [
      { "buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962 },
      { "buffer": 0, "byteOffset": 48, "byteLength": 48, "target": 34962 },
      { "buffer": 0, "byteOffset": 96, "byteLength": 32, "target": 34962 },
      { "buffer": 0, "byteOffset": 128, "byteLength": 12, "target": 34963 }
    ],
    "accessors": [
      { "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" },
      { "bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" },
      { "bufferView": 2, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC2" },
      { "bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": 6, "type": "SCALAR" }
    ]
  })json";
  assert(scene.good());
  return scene_path;
}

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

bool startsWith(const std::string_view value, const std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0u, prefix.size()) == prefix;
}

std::size_t countOpenIndexedEdges(const aster::CpuMesh &mesh) {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
  edges.reserve(mesh.indices.size());
  const auto append_edge = [&](std::uint32_t a, std::uint32_t b) {
    if (a > b) {
      std::swap(a, b);
    }
    edges.push_back({a, b});
  };
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t a = mesh.indices[i + 0u];
    const std::uint32_t b = mesh.indices[i + 1u];
    const std::uint32_t c = mesh.indices[i + 2u];
    if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) {
      continue;
    }
    append_edge(a, b);
    append_edge(b, c);
    append_edge(c, a);
  }
  std::sort(edges.begin(), edges.end());

  std::size_t open = 0u;
  for (std::size_t i = 0; i < edges.size();) {
    std::size_t j = i + 1u;
    while (j < edges.size() && edges[j] == edges[i]) {
      ++j;
    }
    if (j - i == 1u) {
      ++open;
    }
    i = j;
  }
  return open;
}

std::size_t countFacesOpposingVertexNormals(const aster::CpuMesh &mesh) {
  std::size_t opposing = 0u;
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }
    const aster::Vertex &a = mesh.vertices[ia];
    const aster::Vertex &b = mesh.vertices[ib];
    const aster::Vertex &c = mesh.vertices[ic];
    const aster::Vec3 face =
        aster::normalize(aster::cross(b.position - a.position, c.position - a.position));
    const aster::Vec3 averaged_normal = aster::normalize(a.normal + b.normal + c.normal);
    if (aster::length(face) > 0.0001f && aster::length(averaged_normal) > 0.0001f &&
        aster::dot(face, averaged_normal) < -0.25f) {
      ++opposing;
    }
  }
  return opposing;
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

void testGameplayItemInteractionSystems() {
  aster::ItemRegistry registry;
  registry.add({.id = "torch",
                .display_name = "Torch",
                .short_label = "TRC",
                .type = aster::ItemType::LightTool,
                .tint = {1.0f, 0.45f, 0.12f},
                .creates_light = true,
                .creates_fire_particles = true});
  registry.add({.id = "pickaxe",
                .display_name = "Pickaxe",
                .short_label = "PCK",
                .type = aster::ItemType::Tool,
                .tint = {0.5f, 0.48f, 0.42f}});
  assert(registry.contains("torch"));

  aster::InventoryContainer chest(2u);
  assert(chest.addItem(*registry.find("torch"), 1).has_value());
  assert(chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(!chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(chest.removeItem("torch", 1));

  aster::Hotbar hotbar(3u);
  const std::optional<std::size_t> slot = hotbar.addItem(*registry.find("torch"), 1);
  assert(slot.has_value());
  assert(hotbar.select(*slot));
  aster::EquipmentSystem equipment;
  equipment.equipFromHotbar(hotbar);
  assert(equipment.isEquipped("torch"));

  aster::InteractionSystem interactions;
  interactions.update({{.id = "item:torch",
                        .kind = aster::InteractionTargetKind::Item,
                        .action_label = "Take",
                        .subject_label = "Torch",
                        .position = {0.0f, 0.0f, 3.0f},
                        .radius = 0.35f,
                        .max_distance = 6.0f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:torch");

  aster::ScalarAnimation animation;
  animation.setTarget(1.0f);
  animation.update(1.0f / 15.0f);
  assert(animation.value() > 0.0f);
  aster::FlickerLightSpec light_spec;
  light_spec.source_radius = 0.70f;
  const aster::DynamicPointLight light =
      aster::evaluateFlickerLight(light_spec, {1.0f, 2.0f, 3.0f}, 0.25f);
  assert(light.active);
  assert(light.intensity > 0.0f);
  expectNear(light.source_radius, 0.70f, 0.0001f);

  aster::ParticleEmitter emitter(3u);
  emitter.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, 0.5f, {.max_particles = 3u});
  assert(emitter.particles().size() == 3u);
  assert(emitter.particles().front().active);

  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 1.0f});
  aster::AvatarPose pose;
  pose.position = {1.0f, 2.0f, 3.0f};
  pose.facing_yaw = aster::radians(35.0f);
  pose.gait_phase = 0.8f;
  pose.stride_amplitude = 0.35f;
  const std::optional<aster::Transform> socket = aster::resolveAvatarAttachmentSocket(
      rig, pose, {.part_name = "right paw", .local_position = {0.0f, -0.10f, 0.20f}});
  assert(socket.has_value());
  assert(aster::length(socket->position - pose.position) > 0.10f);
  assert(!aster::resolveAvatarAttachmentSocket(rig, pose, {.part_name = "missing"}).has_value());
}

void testMeshGeneration() {
  const aster::CpuMesh sphere = aster::makeUvSphere(16, 8, 1.0f);
  assert(!sphere.vertices.empty());
  assert(!sphere.indices.empty());
  assert(sphere.indices.size() == 16u * 8u * 6u);

  const aster::CpuMesh plane = aster::makePlane(2.0f);
  assert(plane.vertices.size() == 4u);
  assert(plane.indices.size() == 6u);

  const aster::CpuMesh box = aster::makeBox();
  assert(box.vertices.size() == 36u);
  assert(box.indices.size() == 36u);

  const aster::CpuMesh rock = aster::makeRock(8, 5, 1.0f);
  assert(!rock.vertices.empty());
  assert(!rock.indices.empty());

  const aster::CpuMesh crystal = aster::makeCrystal(6, 1.0f, 1.8f);
  assert(crystal.indices.size() == 6u * 12u);

  const aster::CpuMesh block = aster::makeRuinBlock();
  assert(block.indices.size() == 36u);
  for (const aster::Vertex &vertex : block.vertices) {
    assert(aster::dot(vertex.position, vertex.normal) > 0.0f);
  }

  const aster::CpuMesh pillar = aster::makePillar(8, 1.0f, 1.0f);
  assert(!pillar.vertices.empty());
  assert(!pillar.indices.empty());

  const aster::CpuMesh cable = aster::makeCableMesh();
  assert(!cable.vertices.empty());
  assert(!cable.indices.empty());

  const aster::CpuMesh mound = aster::makeMoundMesh();
  assert(!mound.vertices.empty());
  assert(!mound.indices.empty());
  const aster::MoundMeshSpec basin_spec{.radial_segments = 48,
                                        .rings = 12,
                                        .radius_x = 3.2f,
                                        .radius_z = 2.1f,
                                        .height = 0.42f,
                                        .pond_radius_x = 1.8f,
                                        .pond_radius_z = 1.0f,
                                        .pond_depth = 0.72f,
                                        .bank_width = 0.42f,
                                        .bank_height = 0.24f,
                                        .basin_floor_radius = 0.48f,
                                        .shore_depth_fraction = 0.12f};
  const aster::CpuMesh basin = aster::makeMoundMesh(basin_spec);
  float basin_floor_y = std::numeric_limits<float>::max();
  float shore_lip_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : basin.vertices) {
    const float dx = vertex.position.x / basin_spec.pond_radius_x;
    const float dz = vertex.position.z / basin_spec.pond_radius_z;
    const float pond_distance = std::sqrt(dx * dx + dz * dz);
    if (pond_distance < basin_spec.basin_floor_radius * 0.55f) {
      basin_floor_y = std::min(basin_floor_y, vertex.position.y);
    }
    if (pond_distance > 0.92f && pond_distance < 1.12f) {
      shore_lip_y = std::max(shore_lip_y, vertex.position.y);
    }
  }
  assert(shore_lip_y > basin_floor_y + basin_spec.pond_depth * 0.35f);

  const aster::CpuMesh pond = aster::makePondWaterMesh();
  assert(!pond.vertices.empty());
  assert(!pond.indices.empty());
  const aster::SubmergedBasinMeshSpec submerged_spec{.floor_depth = 0.42f,
                                                     .shore_depth = 0.09f,
                                                     .basin_floor_radius = 0.40f,
                                                     .bottom_noise = 0.0f};
  const aster::CpuMesh submerged_basin = aster::makeSubmergedBasinMesh(submerged_spec);
  assert(!submerged_basin.vertices.empty());
  assert(!submerged_basin.indices.empty());
  float submerged_floor_y = std::numeric_limits<float>::max();
  float submerged_shore_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : submerged_basin.vertices) {
    const float dx = vertex.position.x / submerged_spec.radius_x;
    const float dz = vertex.position.z / submerged_spec.radius_z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance < submerged_spec.basin_floor_radius * 0.50f) {
      submerged_floor_y = std::min(submerged_floor_y, vertex.position.y);
    }
    if (distance > 0.94f) {
      submerged_shore_y = std::max(submerged_shore_y, vertex.position.y);
    }
  }
  assert(submerged_shore_y > submerged_floor_y + 0.25f);
  const aster::CpuMesh spectral_water =
      aster::makeEllipticalWaterMesh({.radial_segments = 32,
                                      .rings = 6,
                                      .radius_x = 2.0f,
                                      .radius_z = 1.0f,
                                      .shoreline_inset = 0.08f,
                                      .spectrum = {.amplitude = 0.024f, .component_count = 10}});
  assert(!spectral_water.vertices.empty());
  assert(!spectral_water.indices.empty());
  const aster::Vec3 water_normal = aster::spectralWaterNormal(
      {0.25f, -0.15f}, {.amplitude = 0.020f, .dominant_wavelength = 0.8f, .component_count = 8});
  assert(aster::length(water_normal) > 0.99f);

  const aster::CpuMesh terrain_transition = aster::makeTerrainTransitionMesh();
  assert(!terrain_transition.vertices.empty());
  assert(!terrain_transition.indices.empty());
  bool transition_has_edge_skirt = false;
  for (const aster::Vertex &vertex : terrain_transition.vertices) {
    transition_has_edge_skirt = transition_has_edge_skirt || vertex.position.y < 0.0f;
  }
  assert(transition_has_edge_skirt);
  const aster::TerrainTransitionMeshSpec directional_transition_spec{
      .radial_segments = 32,
      .rings = 4,
      .inner_radius_x = 1.4f,
      .inner_radius_z = 1.0f,
      .outer_radius_x = 2.2f,
      .outer_radius_z = 2.1f,
      .outer_radius_z_negative = 1.42f,
      .outer_radius_z_positive = 2.1f,
      .outer_radius_x_negative = 2.75f,
      .outer_radius_x_positive = 2.2f,
      .edge_irregularity = 0.0f,
      .edge_skirt_depth = 0.0f};
  const aster::CpuMesh directional_transition =
      aster::makeTerrainTransitionMesh(directional_transition_spec);
  float directional_min_x = std::numeric_limits<float>::max();
  float directional_max_x = std::numeric_limits<float>::lowest();
  float directional_min_z = std::numeric_limits<float>::max();
  float directional_max_z = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : directional_transition.vertices) {
    directional_min_x = std::min(directional_min_x, vertex.position.x);
    directional_max_x = std::max(directional_max_x, vertex.position.x);
    directional_min_z = std::min(directional_min_z, vertex.position.z);
    directional_max_z = std::max(directional_max_z, vertex.position.z);
  }
  assert(-directional_min_x > directional_max_x + 0.45f);
  assert(-directional_min_x > 2.70f && -directional_min_x < 2.80f);
  assert(directional_max_x > 2.15f && directional_max_x < 2.25f);
  assert(directional_max_z > -directional_min_z + 0.55f);
  assert(directional_max_z > 2.05f && directional_max_z < 2.15f);
  assert(-directional_min_z > 1.37f && -directional_min_z < 1.47f);

  const aster::CpuMesh grass = aster::makeGrassTuftMesh();
  assert(!grass.vertices.empty());
  assert(!grass.indices.empty());
  const auto flat_grass_sampler = [](const aster::Vec2 position) {
    return aster::TerrainSurfaceSample{.valid = true,
                                       .height = 0.25f + position.x * 0.02f - position.y * 0.01f,
                                       .normal = {0.0f, 1.0f, 0.0f}};
  };
  aster::GrassFieldScatterSpec grass_field_spec{.min = {-1.0f, -0.75f},
                                                .max = {1.0f, 0.75f},
                                                .seed = 77u,
                                                .target_blades = 32,
                                                .candidate_multiplier = 1,
                                                .min_spacing = 0.0f,
                                                .surface_offset = 0.015f,
                                                .min_height = 0.20f,
                                                .max_height = 0.48f,
                                                .min_width = 0.012f,
                                                .max_width = 0.026f,
                                                .max_bend = 0.10f,
                                                .max_lean = 0.035f,
                                                .density_noise_contrast = 0.0f,
                                                .min_surface_normal_y = 0.0f,
                                                .preferred_surface_normal_y = 0.10f};
  const std::vector<aster::GrassBladeAnchor> anchors_a =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  const std::vector<aster::GrassBladeAnchor> anchors_b =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(anchors_a.size() == 32u);
  assert(anchors_a.size() == anchors_b.size());
  for (std::size_t i = 0; i < anchors_a.size(); ++i) {
    expectNear(anchors_a[i].root.x, anchors_b[i].root.x, 0.000001f);
    expectNear(anchors_a[i].root.y, anchors_b[i].root.y, 0.000001f);
    expectNear(anchors_a[i].root.z, anchors_b[i].root.z, 0.000001f);
  }
  grass_field_spec.seed = 78u;
  const std::vector<aster::GrassBladeAnchor> anchors_c =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(anchors_c.size() == anchors_a.size());
  bool saw_seed_variation = false;
  for (std::size_t i = 0; i < anchors_a.size(); ++i) {
    saw_seed_variation =
        saw_seed_variation || std::abs(anchors_a[i].root.x - anchors_c[i].root.x) > 0.0001f ||
        std::abs(anchors_a[i].root.z - anchors_c[i].root.z) > 0.0001f;
  }
  assert(saw_seed_variation);
  const aster::CpuMesh grass_field =
      aster::makeGrassFieldMesh(anchors_a, {.blade_segments = 3, .double_sided = true});
  assert(!grass_field.vertices.empty());
  assert(!grass_field.indices.empty());
  bool saw_root_ao = false;
  bool saw_tip_ao = false;
  for (const aster::Vertex &vertex : grass_field.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
    assert(aster::length(vertex.normal) > 0.50f);
    saw_root_ao = saw_root_ao || (vertex.uv.y <= 0.001f && vertex.ambient_occlusion < 0.82f);
    saw_tip_ao = saw_tip_ao || (vertex.uv.y >= 0.999f && vertex.ambient_occlusion > 0.88f);
  }
  assert(saw_root_ao);
  assert(saw_tip_ao);
  grass_field_spec.seed = 77u;
  grass_field_spec.target_blades = 12;
  grass_field_spec.candidate_multiplier = 4;
  grass_field_spec.accepts_position = [](const aster::Vec2 position) { return position.x < 0.0f; };
  const std::vector<aster::GrassBladeAnchor> accepted_side =
      aster::scatterGrassFieldAnchors(grass_field_spec, flat_grass_sampler);
  assert(!accepted_side.empty());
  for (const aster::GrassBladeAnchor &anchor : accepted_side) {
    assert(anchor.root.x < 0.0f);
  }
  grass_field_spec.accepts_position = {};
  grass_field_spec.min_surface_normal_y = 0.80f;
  grass_field_spec.preferred_surface_normal_y = 0.90f;
  const auto steep_sampler = [](const aster::Vec2 position) {
    (void)position;
    return aster::TerrainSurfaceSample{.valid = true, .height = 0.0f, .normal = {0.86f, 0.22f, 0.0f}};
  };
  assert(aster::scatterGrassFieldAnchors(grass_field_spec, steep_sampler).empty());
  const aster::CpuMesh fish = aster::makeFishMesh();
  assert(!fish.vertices.empty());
  assert(!fish.indices.empty());
  const aster::CpuMesh predator = aster::makeAmphibiousPredatorMesh();
  assert(!predator.vertices.empty());
  assert(!predator.indices.empty());
  const aster::CpuMesh broad_leaf = aster::makeBroadLeafPlantMesh();
  assert(!broad_leaf.vertices.empty());
  assert(!broad_leaf.indices.empty());
  const aster::CpuMesh tree_trunk = aster::makeClimbableTreeTrunkMesh();
  assert(!tree_trunk.vertices.empty());
  assert(!tree_trunk.indices.empty());
  const aster::CpuMesh tree_canopy = aster::makeTreeCanopyMesh();
  assert(!tree_canopy.vertices.empty());
  assert(!tree_canopy.indices.empty());
  const aster::CpuMesh chest = aster::makeTreasureChestMesh();
  assert(!chest.vertices.empty());
  assert(!chest.indices.empty());
  const aster::CpuMesh sentinel = aster::makeSignalSentinelMesh();
  assert(!sentinel.vertices.empty());
  assert(!sentinel.indices.empty());
  const aster::CpuMesh underpass = aster::makeGothicUnderpassMesh();
  assert(!underpass.vertices.empty());
  assert(!underpass.indices.empty());
  const aster::GothicUnderpassMeshSpec underpass_spec;
  const std::vector<aster::ArchitecturalCollisionBox> underpass_collision =
      aster::makeGothicUnderpassCollisionBoxes(underpass_spec);
  assert(underpass_collision.size() == 3u);
  const auto underpassContains = [&](const aster::Vec3 point) {
    for (const aster::ArchitecturalCollisionBox &box : underpass_collision) {
      if (std::abs(point.x - box.center.x) <= box.half_extents.x &&
          std::abs(point.y - box.center.y) <= box.half_extents.y &&
          std::abs(point.z - box.center.z) <= box.half_extents.z) {
        return true;
      }
    }
    return false;
  };
  assert(!underpassContains({0.0f, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(
      underpassContains({underpass_spec.half_width, underpass_spec.passage_height * 0.40f, 0.0f}));
  assert(underpassContains({0.0f, underpass_spec.passage_height + 0.20f, 0.0f}));

  const aster::CpuMesh path = aster::makePathRibbonMesh();
  assert(!path.vertices.empty());
  assert(!path.indices.empty());
  const aster::CpuMesh tapered_path = aster::makePathRibbonMesh({.segments = 8,
                                                                 .width = 0.8f,
                                                                 .width_variation = 0.0f,
                                                                 .crown_height = 0.0f,
                                                                 .surface_noise = 0.0f,
                                                                 .endpoint_taper = 0.25f,
                                                                 .start = {-1.0f, 0.0f, 0.0f},
                                                                 .control = {-0.4f, 0.0f, 0.0f},
                                                                 .control_b = {0.4f, 0.0f, 0.0f},
                                                                 .end = {1.0f, 0.0f, 0.0f}});
  float tapered_start_min = std::numeric_limits<float>::max();
  float tapered_start_max = std::numeric_limits<float>::lowest();
  float tapered_mid_min = std::numeric_limits<float>::max();
  float tapered_mid_max = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : tapered_path.vertices) {
    if (std::abs(vertex.uv.y) < 0.0001f) {
      tapered_start_min = std::min(tapered_start_min, vertex.position.z);
      tapered_start_max = std::max(tapered_start_max, vertex.position.z);
    }
    if (std::abs(vertex.uv.y - 0.5f) < 0.0001f) {
      tapered_mid_min = std::min(tapered_mid_min, vertex.position.z);
      tapered_mid_max = std::max(tapered_mid_max, vertex.position.z);
    }
  }
  const float tapered_start_span = tapered_start_max - tapered_start_min;
  const float tapered_mid_span = tapered_mid_max - tapered_mid_min;
  assert(tapered_start_span > tapered_mid_span * 0.55f);
  assert(tapered_start_span < tapered_mid_span);
  const aster::CpuMesh path_shoulders =
      aster::makePathShoulderMesh({.path = {.segments = 8,
                                            .width = 0.8f,
                                            .endpoint_taper = 0.25f,
                                            .start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 0.0f},
                                            .control_b = {0.4f, 0.0f, 0.0f},
                                            .end = {1.0f, 0.0f, 0.0f}},
                                   .side_segments = 3,
                                   .shoulder_width = 0.35f,
                                   .shoulder_height = 0.12f});
  assert(!path_shoulders.vertices.empty());
  assert(!path_shoulders.indices.empty());
  float shoulder_min_y = std::numeric_limits<float>::max();
  float shoulder_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : path_shoulders.vertices) {
    shoulder_min_y = std::min(shoulder_min_y, vertex.position.y);
    shoulder_max_y = std::max(shoulder_max_y, vertex.position.y);
  }
  assert(shoulder_max_y > shoulder_min_y + 0.05f);
  aster::StrokePath stroke_path;
  stroke_path.points = {{{-0.5f, 0.0f}, 0.05f}, {{0.0f, 0.35f}, 0.04f}, {{0.5f, 0.0f}, 0.05f}};
  const aster::CpuMesh stroke = aster::makeStrokeRibbonMesh({{stroke_path}, 0.02f});
  assert(stroke.vertices.size() == 8u);
  assert(stroke.indices.size() == 12u);
  const aster::PathRibbonMeshSpec path_spec{.start = {-1.0f, 0.0f, 0.0f},
                                            .control = {-0.4f, 0.0f, 1.0f},
                                            .control_b = {0.8f, 0.0f, -0.6f},
                                            .end = {1.0f, 0.0f, 0.0f}};
  const aster::Vec3 path_start = aster::evaluatePathRibbonCenter(path_spec, 0.0f);
  const aster::Vec3 path_end = aster::evaluatePathRibbonCenter(path_spec, 1.0f);
  assert(std::abs(path_start.x + 1.0f) < 0.001f);
  assert(std::abs(path_end.x - 1.0f) < 0.001f);
  assert(aster::length(aster::evaluatePathRibbonTangent(path_spec, 0.5f)) > 0.9f);
  const std::vector<aster::PathRibbonMeshSpec> route_segments = {{.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {-2.0f, 0.0f, 0.0f},
                                                                  .control = {-1.5f, 0.0f, 1.0f},
                                                                  .control_b = {-0.8f, 0.0f, 1.0f},
                                                                  .end = {0.0f, 0.0f, 0.6f}},
                                                                 {.segments = 5,
                                                                  .width = 0.6f,
                                                                  .start = {0.0f, 0.0f, 0.6f},
                                                                  .control = {0.9f, 0.0f, 0.2f},
                                                                  .control_b = {1.4f, 0.0f, -0.5f},
                                                                  .end = {2.0f, 0.0f, 0.0f}}};
  const aster::CpuMesh route_path = aster::makePathRouteRibbonMesh({.segments = route_segments});
  assert(route_path.vertices.size() > tapered_path.vertices.size());
  assert(route_path.indices.size() > tapered_path.indices.size());
  aster::PathShoulderRouteMeshSpec route_shoulder_spec;
  for (const aster::PathRibbonMeshSpec &segment : route_segments) {
    route_shoulder_spec.segments.push_back({.path = segment, .side_segments = 2});
  }
  const aster::CpuMesh route_shoulders = aster::makePathRouteShoulderMesh(route_shoulder_spec);
  assert(!route_shoulders.vertices.empty());
  assert(!route_shoulders.indices.empty());

  const aster::TerrainHeightField terrain =
      aster::makeProceduralTerrain({.grid_size = 17, .square_size = 1.0f});
  const aster::TerrainSurfaceSample terrain_sample = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(terrain_sample.valid);
  assert(aster::length(terrain_sample.normal) > 0.99f);
  const aster::CpuMesh terrain_mesh = aster::makeTerrainMesh(terrain);
  assert(terrain_mesh.vertices.size() == 17u * 17u);
  assert(terrain_mesh.indices.size() == 16u * 16u * 6u);
  const auto assert_upward_winding = [](const aster::CpuMesh &mesh) {
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
      const aster::Vertex &a = mesh.vertices[mesh.indices[i]];
      const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
      const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
      assert(aster::cross(b.position - a.position, c.position - a.position).y > 0.0f);
    }
  };
  assert_upward_winding(terrain_mesh);
  aster::TerrainMeshBuildOptions smooth_terrain_options;
  smooth_terrain_options.subdivisions_per_square = 2;
  smooth_terrain_options.smooth_visual_surface = true;
  const aster::CpuMesh smooth_terrain_mesh =
      aster::makeTerrainMesh(terrain, smooth_terrain_options);
  assert(smooth_terrain_mesh.vertices.size() == 33u * 33u);
  assert(smooth_terrain_mesh.indices.size() == 32u * 32u * 6u);
  assert_upward_winding(smooth_terrain_mesh);

  aster::TerrainMeshBuildOptions clipped_terrain_options;
  clipped_terrain_options.clip_boxes.push_back({{-0.55f, -0.55f}, {0.55f, 0.55f}});
  const aster::CpuMesh clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, clipped_terrain_options);
  assert(clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
  aster::TerrainMeshBuildOptions oriented_clip_options;
  oriented_clip_options.clip_oriented_ellipses.push_back({.center = {0.0f, 0.0f},
                                                          .forward = {1.0f, 0.0f},
                                                          .radius = {1.2f, 1.8f},
                                                          .radius_forward_negative = 0.8f,
                                                          .radius_forward_positive = 2.4f});
  const aster::CpuMesh oriented_clipped_terrain_mesh =
      aster::makeTerrainMesh(terrain, oriented_clip_options);
  assert(oriented_clipped_terrain_mesh.indices.size() < terrain_mesh.indices.size());
}

void testTerrainSculpting() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 25;
  terrain.square_size = 1.0f;
  terrain.origin = {-12.0f, -12.0f};
  terrain.heights.assign(static_cast<std::size_t>(terrain.grid_size * terrain.grid_size), 0.0f);

  aster::applyTerrainMountainBrush(terrain, {.seed = 11u,
                                             .center = {0.0f, 0.0f},
                                             .radius = {7.0f, 7.0f},
                                             .height = 3.0f,
                                             .shoulder_height = 0.4f,
                                             .surface_noise = 0.0f,
                                             .ridge_frequency = 0.12f,
                                             .inner_plateau = 0.12f});
  assert(aster::sampleTerrain(terrain, {0.0f, 0.0f}).height > 2.3f);
  assert(std::abs(aster::sampleTerrain(terrain, {11.0f, 11.0f}).height) < 0.001f);

  aster::deformTerrainAlongPath(terrain, {.path = {.samples = 24,
                                                   .start = {-6.0f, 0.5f, -4.0f},
                                                   .control = {-2.0f, 0.9f, -1.5f},
                                                   .control_b = {2.0f, 1.4f, 1.5f},
                                                   .end = {6.0f, 1.8f, 4.0f}},
                                          .half_width = 0.55f,
                                          .shoulder_width = 0.75f,
                                          .crown_height = 0.0f,
                                          .surface_noise = 0.0f,
                                          .seed = 17u});
  const aster::TerrainSurfaceSample path_mid = aster::sampleTerrain(terrain, {0.0f, 0.0f});
  assert(path_mid.valid);
  assert(path_mid.height > 0.9f);
  assert(path_mid.height < 1.7f);

  aster::TerrainHeightField route_terrain;
  route_terrain.grid_size = 25;
  route_terrain.square_size = 1.0f;
  route_terrain.origin = {-12.0f, -12.0f};
  route_terrain.heights.assign(
      static_cast<std::size_t>(route_terrain.grid_size * route_terrain.grid_size), 0.0f);
  aster::deformTerrainAlongRoute(route_terrain,
                                 {.route = {.segments = {{.samples = 16,
                                                          .start = {-8.0f, 0.4f, 5.0f},
                                                          .control = {-5.2f, 0.5f, 7.2f},
                                                          .control_b = {-2.7f, 0.6f, 6.4f},
                                                          .end = {0.0f, 0.7f, 5.0f}},
                                                         {.samples = 16,
                                                          .start = {0.0f, 0.7f, 5.0f},
                                                          .control = {2.8f, 0.8f, 3.1f},
                                                          .control_b = {5.2f, 0.9f, 3.4f},
                                                          .end = {8.0f, 1.0f, 5.0f}}}},
                                  .half_width = 0.5f,
                                  .shoulder_width = 0.6f,
                                  .crown_height = 0.0f,
                                  .surface_noise = 0.0f,
                                  .seed = 31u});
  const aster::TerrainSurfaceSample route_join = aster::sampleTerrain(route_terrain, {0.0f, 5.0f});
  assert(route_join.valid);
  assert(route_join.height > 0.62f);
  assert(std::abs(aster::sampleTerrain(route_terrain, {11.0f, -11.0f}).height) < 0.001f);

  aster::sculptTerrainPortalShelf(terrain, {.seed = 23u,
                                            .entrance = {2.0f, 1.25f, 2.0f},
                                            .inward = {0.0f, 0.0f, -1.0f},
                                            .floor_height = 1.25f,
                                            .half_width = 1.2f,
                                            .front_depth = 1.0f,
                                            .back_depth = 1.5f,
                                            .feather = 0.6f,
                                            .side_berm_height = 0.3f,
                                            .rear_cover_height = 0.4f,
                                            .surface_noise = 0.0f});
  const aster::TerrainSurfaceSample portal_floor = aster::sampleTerrain(terrain, {2.0f, 2.0f});
  assert(portal_floor.valid);
  assert(std::abs(portal_floor.height - 1.25f) < 0.08f);
}

void testCaveInteriorVolume() {
  const aster::CaveTunnelProfile tunnel{.seed = 7u,
                                        .start = {0.0f, 0.0f, 0.0f},
                                        .control = {0.2f, -0.5f, -3.0f},
                                        .control_b = {1.1f, -1.2f, -6.0f},
                                        .end = {0.4f, -1.6f, -9.0f},
                                        .length_segments = 32,
                                        .radial_segments = 12,
                                        .half_width = 1.0f,
                                        .wall_height = 1.5f,
                                        .floor_width = 1.1f,
                                        .floor_crown = 0.02f,
                                        .wall_noise = 0.0f,
                                        .visible_wall_start_t = 0.20f,
                                        .collision_start_t = 0.18f,
                                        .ore_start_t = 0.32f,
                                        .chest_t = 0.62f,
                                        .chamber_t = 0.64f,
                                        .chamber_falloff = 0.20f,
                                        .chamber_width_scale = 1.7f,
                                        .chamber_height_scale = 1.25f};
  const aster::CaveInteriorSample outside_under =
      aster::sampleCaveInteriorVolume(tunnel, {0.0f, -1.0f, -1.0f});
  assert(outside_under.interior < 0.001f);

  const aster::Vec3 inside_floor = aster::evaluateCaveTunnelCenter(tunnel, 0.58f);
  const aster::CaveInteriorSample inside =
      aster::sampleCaveInteriorVolume(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f});
  assert(inside.interior > 0.5f);
  assert(inside.depth > 0.0f);
  assert(inside.half_width > tunnel.half_width);

  const aster::CaveComplex complex =
      aster::buildCaveComplex({.tunnel = tunnel,
                               .portal = {.arch_segments = 8},
                               .ore = {.seed = 13u, .candidates = 16, .max_nodes = 4}});
  assert(!complex.tunnel_chunks.empty());
  assert(!complex.collision_mesh.vertices.empty());
  assert(!complex.collision_mesh.indices.empty());
  assert(!complex.features.empty());
  assert(complex.portal_mesh.vertices.size() > 18u);
  assert(countOpenIndexedEdges(complex.portal_mesh) == 0u);
  assert(countFacesOpposingVertexNormals(complex.portal_mesh) == 0u);
  assert(!complex.portal_blend_mesh.vertices.empty());
  assert(!complex.portal_blend_mesh.indices.empty());
  assert(!complex.portal_formation_mesh.vertices.empty());
  assert(!complex.portal_formation_mesh.indices.empty());
  assert(complex.portal_formation_mesh.vertices.size() > complex.portal_mesh.vertices.size());
  assert(!complex.portal_seal_mesh.vertices.empty());
  assert(!complex.portal_seal_mesh.indices.empty());
  assert(countFacesOpposingVertexNormals(complex.portal_seal_mesh) == 0u);
  assert(!complex.entrance_throat_mesh.vertices.empty());
  assert(!complex.entrance_throat_mesh.indices.empty());
  assert(countFacesOpposingVertexNormals(complex.entrance_throat_mesh) == 0u);
  for (const aster::CpuMesh &chunk : complex.tunnel_chunks) {
    assert(!chunk.vertices.empty());
    assert(!chunk.indices.empty());
    assert(countFacesOpposingVertexNormals(chunk) == 0u);
  }
  assert(countFacesOpposingVertexNormals(complex.collision_mesh) == 0u);
  assert(!complex.portal_floor_mesh.vertices.empty());
  assert(!complex.portal_floor_mesh.indices.empty());
  aster::Vec3 portal_floor_center{};
  for (const aster::Vertex &vertex : complex.portal_floor_mesh.vertices) {
    portal_floor_center = portal_floor_center + vertex.position;
    assert(vertex.normal.y > 0.20f);
  }
  portal_floor_center =
      portal_floor_center / static_cast<float>(complex.portal_floor_mesh.vertices.size());
  const aster::TerrainSurfaceSample portal_floor_support = aster::sampleMeshSupport(
      complex.portal_floor_mesh, {},
      aster::SurfaceSupportQuery{{portal_floor_center.x, portal_floor_center.z}}, 0.36f);
  assert(portal_floor_support.valid);
  assert(complex.chest_position.y < -0.4f);
  const aster::Vec3 chamber_center = aster::evaluateCaveTunnelCenter(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_tangent = aster::evaluateCaveTunnelTangent(tunnel, tunnel.chamber_t);
  const aster::Vec3 chamber_side =
      aster::normalize(aster::cross({0.0f, 1.0f, 0.0f}, chamber_tangent));
  const aster::CaveTraversalConstraint side_constraint = aster::constrainCaveTraversalPosition(
      tunnel, chamber_center + chamber_side * 2.4f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(side_constraint.active);
  assert(aster::length(side_constraint.correction) > 0.0f);
  const aster::Vec3 end = aster::evaluateCaveTunnelCenter(tunnel, 1.0f);
  const aster::Vec3 end_tangent = aster::evaluateCaveTunnelTangent(tunnel, 1.0f);
  const aster::CaveTraversalConstraint end_constraint = aster::constrainCaveTraversalPosition(
      tunnel, end + end_tangent * 0.45f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(end_constraint.active);
  assert(aster::dot(end_constraint.corrected_position - end, end_tangent) < 0.0f);
  aster::CaveTunnelProfile streaming_tunnel = tunnel;
  streaming_tunnel.end_constraint_enabled = false;
  const aster::CaveTraversalConstraint streaming_end_constraint =
      aster::constrainCaveTraversalPosition(
          streaming_tunnel, end + end_tangent * 0.45f + aster::Vec3{0.0f, 0.55f, 0.0f}, 0.28f);
  assert(!streaming_end_constraint.active);
  const aster::CaveViewConstraint view_constraint =
      aster::constrainCaveViewSegment(tunnel, inside_floor + aster::Vec3{0.0f, 0.55f, 0.0f},
                                      tunnel.start + aster::Vec3{0.0f, 1.0f, 1.2f},
                                      {.samples = 18,
                                       .minimum_radius = 0.65f,
                                       .interior_threshold = 0.045f,
                                       .backtrack_tolerance_t = 0.12f});
  assert(view_constraint.active);
  assert(view_constraint.radius > 0.0f);
  const std::vector<aster::CaveWallFixturePlacement> fixtures =
      aster::placeCaveWallFixtures(tunnel, {.start_t = 0.28f,
                                            .end_t = 0.70f,
                                            .target_spacing = 2.5f,
                                            .max_count = 3,
                                            .wall_side = -1.0f,
                                            .mount_height = 1.0f,
                                            .wall_inset = 0.12f});
  assert(!fixtures.empty());
  for (const aster::CaveWallFixturePlacement &fixture : fixtures) {
    const aster::CaveInteriorSample fixture_sample =
        aster::sampleCaveInteriorVolume(tunnel, fixture.position);
    assert(fixture_sample.tunnel_t >= 0.24f);
    assert(aster::length(fixture.normal) > 0.90f);
    const float lens_gap =
        aster::dot(fixture.lens_position - fixture.mount_position, fixture.normal);
    const float light_gap =
        aster::dot(fixture.light_position - fixture.lens_position, fixture.normal);
    assert(lens_gap > 0.050f);
    assert(light_gap > 0.120f);
    assert(aster::dot(fixture.light_position - fixture.mount_position, fixture.normal) > lens_gap);
  }
  const aster::CaveTerrainPortalCut portal_cut =
      aster::makeCaveTerrainPortalCut(tunnel, {.arch_segments = 8});
  assert(portal_cut.radius_forward_positive > portal_cut.radius_forward_negative);
  const aster::CaveTerrainCoverFit uncovered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel, [](const aster::Vec2) { return aster::CaveTerrainCoverSample{true, -10.0f}; },
      {.samples = 24,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(!uncovered_fit.cover_found);
  assert(uncovered_fit.tunnel.visible_wall_start_t == tunnel.visible_wall_start_t);
  const aster::CaveTerrainCoverFit covered_fit = aster::fitCaveTunnelToTerrainCover(
      tunnel,
      [](const aster::Vec2 position) {
        return aster::CaveTerrainCoverSample{true, position.y < -4.0f ? 10.0f : -10.0f};
      },
      {.samples = 48,
       .required_consecutive_samples = 2,
       .min_t = tunnel.visible_wall_start_t,
       .max_t = 0.90f,
       .roof_clearance = 0.20f});
  assert(covered_fit.cover_found);
  assert(covered_fit.tunnel.visible_wall_start_t > tunnel.visible_wall_start_t);
  assert(covered_fit.tunnel.collision_start_t == tunnel.collision_start_t);
  for (const aster::CaveOreNodePlacement &ore : complex.ore_nodes) {
    const aster::CaveInteriorSample ore_sample =
        aster::sampleCaveInteriorVolume(tunnel, ore.position);
    assert(ore_sample.tunnel_t >= tunnel.ore_start_t - 0.08f);
  }
  bool saw_ceiling_feature = false;
  bool saw_floor_feature = false;
  for (const aster::CaveFeaturePlacement &feature : complex.features) {
    const aster::CaveInteriorSample feature_sample =
        aster::sampleCaveInteriorVolume(tunnel, feature.position);
    assert(feature_sample.tunnel_t >= tunnel.visible_wall_start_t - 0.16f);
    assert(aster::length(feature.normal) > 0.90f);
    saw_ceiling_feature = saw_ceiling_feature || feature.kind == aster::CaveFeatureKind::Stalactite;
    saw_floor_feature = saw_floor_feature || feature.kind == aster::CaveFeatureKind::Stalagmite ||
                        feature.kind == aster::CaveFeatureKind::Column;
  }
  assert(saw_ceiling_feature);
  assert(saw_floor_feature);
}

void testVoxelCaveStreamingAndFixtureContracts() {
  aster::VoxelCaveSpec spec;
  spec.seed = 77u;
  spec.origin = {-8.0f, -8.0f, -48.0f};
  spec.cell_size = 0.40f;
  spec.chunk_cells = 8;
  spec.stream_radius = 1;
  spec.unload_radius = 2;
  spec.max_chunk_rebuilds_per_update = 0;
  spec.forced_rebuild_radius = 0;
  spec.structural_surface_mode = aster::VoxelCaveStructuralSurfaceMode::RenderAndCollide;
  spec.authored_fixture_light_color = {1.0f, 0.14f, 0.06f};
  spec.procedural_fixture_light_color = {1.0f, 0.10f, 0.04f};
  spec.tunnels.push_back({{0.0f, 0.0f, -48.0f}, {0.0f, 0.0f, -58.0f}, 1.42f});
  spec.chambers.push_back({{0.0f, 0.1f, -54.0f}, {2.2f, 1.35f, 2.4f}});
  spec.procedural_fields.push_back({.enabled = true,
                                    .seed = 91u,
                                    .start = {0.0f, 0.0f, -58.0f},
                                    .forward = {0.0f, 0.0f, -1.0f},
                                    .up = {0.0f, 1.0f, 0.0f},
                                    .backtrack_distance = 1.5f,
                                    .tunnel_radius = 1.35f,
                                    .vertical_radius = 1.06f,
                                    .radius_variation = 0.08f,
                                    .wander_frequency = 0.05f,
                                    .side_wander = 0.55f,
                                    .vertical_wander = 0.18f,
                                    .chamber_spacing = 14.0f,
                                    .chamber_radius = 2.2f,
                                    .chamber_vertical_radius = 1.35f,
                                    .chamber_radius_variation = 0.18f,
                                    .chamber_jitter = 0.18f});
  spec.material_profiles = {{.material = aster::VoxelCaveMaterial::Rock,
                             .seed = 93u,
                             .display_name = "Rock",
                             .visual_material_id = "rock"}};

  aster::VoxelCaveState state;
  state.configure(spec);
  state.updateStreaming({0.0f, 0.0f, -52.0f}, 0.0f);
  const aster::VoxelChunkCoord first_center = state.chunkCoordFor({0.0f, 0.0f, -52.0f});
  bool saw_first_chunk = false;
  bool saw_collision_mesh = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    saw_first_chunk = saw_first_chunk || chunk.coord == first_center;
    saw_collision_mesh =
        saw_collision_mesh ||
        (chunk.collision_mesh != nullptr && !chunk.collision_mesh->vertices.empty() &&
         !chunk.collision_mesh->indices.empty() &&
         countFacesOpposingVertexNormals(*chunk.collision_mesh) == 0u);
  }
  assert(saw_first_chunk);
  assert(saw_collision_mesh);

  const aster::VoxelCaveInteriorSample authored = state.sampleInterior({0.0f, 0.0f, -54.0f});
  assert(authored.valid);
  assert(authored.inside);
  assert(authored.interior > 0.65f);
  const aster::VoxelCaveProceduralFrame procedural_frame =
      aster::sampleVoxelCaveProceduralFrameAt(spec.procedural_fields.front(), 8.0f);
  assert(procedural_frame.valid);
  const aster::Vec3 procedural_probe = procedural_frame.center;
  const aster::VoxelCaveInteriorSample procedural = state.sampleInterior(procedural_probe);
  assert(procedural.valid);
  assert(procedural.procedural);

  const std::vector<aster::VoxelCaveFixturePlacement> path_fixtures =
      aster::placeVoxelCavePathFixtures(
          spec, {.start_distance = 2.0f,
                 .target_spacing = 3.0f,
                 .max_count = 4,
                 .side_mode = aster::VoxelCaveFixtureSideMode::FixedNegative,
                 .wall_inset = 0.12f,
                 .mount_height = 0.08f,
                 .normal_up_bias = -0.10f,
                 .lens_offset = 0.075f,
                 .light_offset = 0.18f});
  assert(!path_fixtures.empty());
  for (const aster::VoxelCaveFixturePlacement &fixture : path_fixtures) {
    assert(aster::dot(fixture.lens_position - fixture.mount_position, fixture.normal) > 0.050f);
    assert(aster::dot(fixture.light_position - fixture.lens_position, fixture.normal) > 0.120f);
  }

  const std::vector<aster::VoxelCaveFixturePlacement> procedural_fixtures =
      aster::placeVoxelCaveProceduralFixturesNear(
          spec, procedural_probe,
          {.target_spacing = 5.8f,
           .max_count = 4,
           .procedural_behind = 1,
           .procedural_ahead = 2,
           .side_mode = aster::VoxelCaveFixtureSideMode::Alternating,
           .wall_inset = 0.12f,
           .mount_height = 0.08f,
           .normal_up_bias = -0.10f,
           .lens_offset = 0.075f,
           .light_offset = 0.18f});
  assert(procedural_fixtures.size() >= 2u);
  const std::vector<aster::VoxelCaveFixtureLightSample> ranked =
      aster::rankVoxelCaveFixtureLights(procedural_probe, procedural, procedural_fixtures,
                                        {.progress_window = 5.8f,
                                         .minimum_progress_gain = 0.70f,
                                         .distance_score_weight = 0.55f,
                                         .progress_score_weight = 1.30f});
  assert(!ranked.empty());
  assert(ranked.front().weight > 0.0f);
  assert(aster::length(ranked.front().placement.light_position -
                       ranked.front().placement.lens_position) > 0.12f);

  state.updateStreaming({0.0f, 0.0f, -76.0f}, 0.1f);
  const aster::VoxelChunkCoord advanced_center = state.chunkCoordFor({0.0f, 0.0f, -76.0f});
  bool saw_advanced_chunk = false;
  for (const aster::VoxelChunkSnapshot &chunk : state.activeChunks()) {
    saw_advanced_chunk = saw_advanced_chunk || chunk.coord == advanced_center;
  }
  assert(!(advanced_center == first_center));
  assert(saw_advanced_chunk);
}

void testMeshProcessingPipeline() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  for (aster::Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());
  mesh.indices.push_back(mesh.indices.back());

  aster::MeshDiagnostics diagnostics;
  const aster::CpuMesh prepared = aster::prepareMeshForRendering(std::move(mesh), {}, &diagnostics);
  assert(prepared.indices.size() == 6u);
  assert(diagnostics.degenerate_triangles == 1u);
  assert(diagnostics.invalid_normals == 4u);
  assert(diagnostics.generated_tangents == 6u);
  for (const aster::Vertex &vertex : prepared.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    assert(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
  }
}

void testMeshProcessingRejectsInvalidIndices() {
  aster::CpuMesh mesh = aster::makePlane(2.0f);
  mesh.indices.front() = 99u;

  bool rejected = false;
  try {
    (void)aster::prepareMeshForRendering(std::move(mesh));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
}

void testMeshDrapingRaisesEmbeddedVertices() {
  aster::CpuMesh path = aster::makePathRibbonMesh({.segments = 3,
                                                   .width = 0.3f,
                                                   .crown_height = 0.0f,
                                                   .surface_noise = 0.0f,
                                                   .start = {-1.0f, -0.2f, 0.0f},
                                                   .control = {-0.4f, -0.2f, 0.0f},
                                                   .control_b = {0.4f, -0.2f, 0.0f},
                                                   .end = {1.0f, -0.2f, 0.0f}});
  const aster::CpuMesh draped = aster::drapeMeshToSurface(
      std::move(path), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.35f}; },
      {.surface_offset = 0.02f, .raise_only = true});
  for (const aster::Vertex &vertex : draped.vertices) {
    assert(vertex.position.y >= 0.369f);
  }

  aster::CpuMesh raised_path = aster::makePathRibbonMesh({.segments = 3,
                                                          .width = 0.3f,
                                                          .crown_height = 0.0f,
                                                          .surface_noise = 0.0f,
                                                          .start = {-1.0f, 0.8f, 0.0f},
                                                          .control = {-0.4f, 0.8f, 0.0f},
                                                          .control_b = {0.4f, 0.8f, 0.0f},
                                                          .end = {1.0f, 0.8f, 0.0f}});
  const aster::CpuMesh projected = aster::drapeMeshToSurface(
      std::move(raised_path),
      [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.20f}; },
      {.surface_offset = 0.01f, .raise_only = false});
  for (const aster::Vertex &vertex : projected.vertices) {
    expectNear(vertex.position.y, 0.21f, 0.0001f);
  }

  aster::CpuMesh relief = aster::makePathShoulderMesh({.path = {.segments = 4,
                                                                .width = 0.4f,
                                                                .start = {-1.0f, 0.0f, 0.0f},
                                                                .control = {-0.4f, 0.0f, 0.0f},
                                                                .control_b = {0.4f, 0.0f, 0.0f},
                                                                .end = {1.0f, 0.0f, 0.0f}},
                                                       .shoulder_height = 0.16f});
  const aster::CpuMesh preserved = aster::drapeMeshToSurface(
      std::move(relief), [](const aster::Vec2) { return aster::TerrainSurfaceSample{true, 0.45f}; },
      {.surface_offset = 0.02f,
       .raise_only = false,
       .preserve_vertical_offset = true,
       .reference_y = 0.0f});
  float preserved_min_y = std::numeric_limits<float>::max();
  float preserved_max_y = std::numeric_limits<float>::lowest();
  for (const aster::Vertex &vertex : preserved.vertices) {
    preserved_min_y = std::min(preserved_min_y, vertex.position.y);
    preserved_max_y = std::max(preserved_max_y, vertex.position.y);
  }
  assert(preserved_max_y > preserved_min_y + 0.05f);
}

void testSceneAssetNormalTangentImport() {
  const std::filesystem::path sample = writeGeneratedNormalTangentFixture();
  assert(std::filesystem::exists(sample));

  const aster::SceneAsset asset =
      aster::importSceneAsset(sample, {{}, true, 1.0f, aster::AssetOriginPolicy::CenterOnGround});
  assert(asset.material_slots.size() == 2u);
  assert(asset.mesh_chunks.size() == 1u);
  assert(asset.mesh_chunks.front().material_slot == 1u);
  assert(asset.material_slots[1].has_base_color_texture);
  assert(asset.material_slots[1].has_metallic_roughness_texture);
  assert(asset.material_slots[1].has_normal_texture);
  assert(asset.material_slots[1].has_occlusion_texture);
  assert(asset.material_slots[1].material.double_sided);
  assert(asset.material_slots[1].material.alpha_mode == aster::MaterialAlphaMode::Masked);
  assert(aster::classifyMaterialRenderQueue(asset.material_slots[1].material) ==
         aster::MaterialRenderQueue::Masked);
  assert(!asset.mesh_chunks.front().mesh.vertices.empty());
  assert(asset.mesh_chunks.front().mesh.indices.size() == 6u);
  assert(asset.mesh_chunks.front().diagnostics.generated_tangents == 6u);

  float min_y = std::numeric_limits<float>::max();
  float max_radius = 0.0f;
  for (const aster::Vertex &vertex : asset.mesh_chunks.front().mesh.vertices) {
    assert(aster::length(vertex.normal) > 0.99f);
    assert(aster::length({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) > 0.99f);
    min_y = std::min(min_y, vertex.position.y);
    max_radius = std::max(max_radius, aster::length({vertex.position.x, 0.0f, vertex.position.z}));
  }
  expectNear(min_y, 0.0f, 0.001f);
  assert(max_radius > 0.1f);
  std::filesystem::remove_all(sample.parent_path());
}

void testBrushLevelMeshBuild() {
  const aster::CpuMesh mesh = aster::buildBrushLevelMesh(
      {
          {{0.0f, 1.0f, 0.0f}, {1.2f, 1.0f, 0.2f}, {}, aster::BrushVolume::Solid, 0, 0.08f},
          {{0.0f, 0.6f, 0.0f}, {0.3f, 0.6f, 0.4f}, {}, aster::BrushVolume::Air, 1, 0.0f},
      },
      {.uv_scale = 0.5f,
       .edge_softening = 0.04f,
       .noise_frequency = 0.5f,
       .noise_strength = 0.01f});

  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() > 36u);

  bool found_lower_door_triangle = false;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const aster::Vertex &a = mesh.vertices[mesh.indices[i + 0u]];
    const aster::Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    const aster::Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.22f && centroid.y < 0.48f && std::abs(centroid.z) < 0.18f) {
      found_lower_door_triangle = true;
      break;
    }
  }
  assert(!found_lower_door_triangle);
}

void testArchitecturalMeshBuild() {
  const aster::CpuMesh gatehouse = aster::makeGatehouseMesh({.wall_half_width = 1.2f,
                                                             .wall_height = 1.8f,
                                                             .tower_half_width = 0.34f,
                                                             .tower_height = 2.2f,
                                                             .depth = 0.55f,
                                                             .door_half_width = 0.32f,
                                                             .door_height = 1.0f,
                                                             .parapet_blocks = 4});
  assert(!gatehouse.vertices.empty());
  assert(!gatehouse.indices.empty());

  bool found_open_gate = true;
  for (std::size_t i = 0; i < gatehouse.indices.size(); i += 3u) {
    const aster::Vertex &a = gatehouse.vertices[gatehouse.indices[i + 0u]];
    const aster::Vertex &b = gatehouse.vertices[gatehouse.indices[i + 1u]];
    const aster::Vertex &c = gatehouse.vertices[gatehouse.indices[i + 2u]];
    const aster::Vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (std::abs(centroid.x) < 0.20f && centroid.y < 0.82f && std::abs(centroid.z) < 0.20f) {
      found_open_gate = false;
      break;
    }
  }
  assert(found_open_gate);

  const aster::CpuMesh ruined_castle = aster::makeRuinedCastleCourseMesh(
      {.half_width = 2.4f, .wall_height = 1.7f, .tower_height = 2.4f, .parapet_blocks = 7});
  assert(!ruined_castle.vertices.empty());
  assert(!ruined_castle.indices.empty());
}

void testVoxelStructureBuild() {
  std::vector<aster::VoxelCell> cells;
  aster::appendVoxelBox(cells, {0, 0, 0}, {2, 1, 1}, 3);
  const aster::VoxelStructure structure = aster::buildVoxelStructure(cells);
  assert(structure.mesh_batches.size() == 1u);
  assert(structure.mesh_batches.front().channel == 3u);
  assert(structure.mesh_batches.front().mesh.indices.size() == 10u * 6u);
  assert(structure.collision_boxes.size() == 1u);
  expectNear(structure.collision_boxes.front().center.x, 1.0f, 0.0001f);
  expectNear(structure.collision_boxes.front().half_extents.x, 1.0f, 0.0001f);

  std::vector<aster::VoxelCell> corner_cells;
  corner_cells.push_back({{0, 0, 0}, 1});
  corner_cells.push_back({{1, 0, 1}, 1});
  corner_cells.push_back({{0, 1, 1}, 1});
  const aster::VoxelStructure corner_structure = aster::buildVoxelStructure(corner_cells);
  bool found_occluded_vertex = false;
  for (const aster::VoxelMeshBatch &batch : corner_structure.mesh_batches) {
    for (const aster::Vertex &vertex : batch.mesh.vertices) {
      assert(vertex.ambient_occlusion >= 0.0f && vertex.ambient_occlusion <= 1.0f);
      found_occluded_vertex = found_occluded_vertex || vertex.ambient_occlusion < 0.70f;
    }
  }
  assert(found_occluded_vertex);
}

void testCastleCourseBuild() {
  const aster::CastleCourse course = aster::buildCastleCourse();
  assert(!course.structure.mesh_batches.empty());
  assert(!course.structure.collision_boxes.empty());
  assert(!course.box_elements.empty());
  assert(!course.ground_zones.empty());
  bool found_foundation = false;
  for (const aster::VoxelMeshBatch &batch : course.structure.mesh_batches) {
    found_foundation =
        found_foundation ||
        batch.channel == static_cast<std::uint32_t>(aster::CastleCourseChannel::Foundation);
  }
  assert(found_foundation);
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    min_x = std::min(min_x, box.center.x - box.half_extents.x);
    max_x = std::max(max_x, box.center.x + box.half_extents.x);
    min_z = std::min(min_z, box.center.z - box.half_extents.z);
    max_z = std::max(max_z, box.center.z + box.half_extents.z);
  }
  assert(max_x - min_x > 42.0f);
  assert(max_z - min_z > 34.0f);
  const aster::CastleCourseGroundZone &ground = course.ground_zones.front();
  assert(ground.half_extents.x > 20.0f);
  assert(ground.half_extents.y > 17.0f);
  assert(std::abs(ground.center.x) < 0.001f);
  const aster::Vec3 underpass_clearance{0.0f, 0.60f, -0.18f};
  bool underpass_blocked = false;
  for (const aster::VoxelCollisionBox &box : course.structure.collision_boxes) {
    underpass_blocked =
        underpass_blocked || (std::abs(underpass_clearance.x - box.center.x) < box.half_extents.x &&
                              std::abs(underpass_clearance.y - box.center.y) < box.half_extents.y &&
                              std::abs(underpass_clearance.z - box.center.z) < box.half_extents.z);
  }
  assert(!underpass_blocked);
}

void testSceneContract() {
  const aster::Scene scene = aster::Scene::makeShowcase();
  assert(scene.objects().size() >= 3u);
  assert(scene.objects().front().primitive == aster::MeshPrimitive::Plane);
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::CourseCells);
  assert(scene.objects().front().material.pattern_depth > 0.0f);
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
}

void testGeneratedSceneryAssembly() {
  auto mesh = std::make_shared<const aster::CpuMesh>(aster::makeBox());
  const aster::Material material =
      aster::makeMaterial({.base_color = {0.25f, 0.40f, 0.20f},
                           .camera_occlusion = aster::CameraOcclusionPolicy::Solid});
  const aster::GeneratedSceneryBundle bundle = aster::assembleGeneratedScenery(
      {.name = "grove",
       .root = {.position = {2.0f, 0.0f, 1.0f}, .scale = {2.0f, 1.0f, 2.0f}},
       .primitives = {aster::GeneratedSceneryPrimitivePart{
           .name = "stone",
           .primitive = aster::MeshPrimitive::Rock,
           .transform = {.position = {1.0f, 0.0f, 0.0f}, .scale = {0.5f, 0.5f, 0.5f}},
           .material = material}},
       .meshes = {aster::GeneratedSceneryMeshPart{.name = "trunk",
                                                  .mesh = mesh,
                                                  .transform = {.position = {0.0f, 1.0f, 0.0f}},
                                                  .material = material}},
       .collision_proxies = {{"bounds", {0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}}},
       .sockets = {aster::GeneratedScenerySocket{
           .name = "perch", .transform = {.position = {0.0f, 2.0f, 0.0f}}, .radius = 0.25f}}});
  assert(bundle.render_objects.size() == 2u);
  assert(bundle.render_objects[0].name == "grove/stone");
  expectNear(bundle.render_objects[0].transform.position.x, 4.0f, 0.0001f);
  assert(!bundle.render_objects[0].camera_occlusion_fade);
  assert(bundle.render_objects[1].custom_mesh == mesh);
  assert(bundle.collision_proxies.size() == 1u);
  expectNear(bundle.collision_proxies.front().half_extents.x, 1.0f, 0.0001f);
  const aster::GeneratedScenerySocket *socket =
      aster::findGeneratedScenerySocket(bundle, "grove/perch");
  assert(socket != nullptr);
  expectNear(socket->transform.position.y, 2.0f, 0.0001f);

  aster::Scene scene;
  std::vector<std::size_t> indices;
  aster::appendGeneratedScenery(scene, bundle, &indices);
  assert(scene.objects().size() == 2u);
  assert(indices.size() == 2u);
  assert(indices[0] == 0u && indices[1] == 1u);
}

const aster::SceneCoherenceContribution *findContribution(const aster::SceneCoherenceReport &report,
                                                          const aster::SceneCoherenceTerm term) {
  for (const aster::SceneCoherenceContribution &contribution : report.contributions) {
    if (contribution.term == term) {
      return &contribution;
    }
  }
  return nullptr;
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
  bool saw_cave_chunk = false;
  bool saw_streaming_voxel_surface = false;
  bool saw_central_grass_field = false;
  bool saw_cave_mouth_grass_field = false;
  bool saw_coal_ore = false;
  bool saw_supply_crate = false;
  bool saw_wall_light_lens = false;
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
      saw_supply_crate = true;
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.position.x, 0.0f, object.transform.position.z}) >
             45.0f);
      assert(!object.camera_occlusion_fade);
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
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
    }
    if (object.name == "Chunked procedural cave interior") {
      saw_cave_chunk = true;
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
    if (object.name == "Rock voxel cave surface" && object.custom_mesh != nullptr &&
        !object.custom_mesh->vertices.empty()) {
      saw_streaming_voxel_surface = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Coal ore vein node") {
      saw_coal_ore = true;
      ++coal_ore_count;
      assert(object.material.opacity >= 0.999f);
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
      assert(object.material.emission_strength > 0.5f);
      assert(!object.camera_occlusion_fade);
    }
  }

  assert(saw_cave_threshold);
  assert(saw_cave_overhang);
  assert(saw_cave_formation);
  assert(saw_cave_liner);
  assert(saw_cave_seal);
  assert(saw_cave_throat);
  assert(saw_cave_floor);
  assert(saw_cave_chunk);
  assert(saw_streaming_voxel_surface);
  assert(saw_central_grass_field);
  assert(saw_cave_mouth_grass_field);
  assert(saw_coal_ore);
  assert(coal_ore_count >= 4);
  assert(saw_supply_crate);
  assert(saw_wall_light_lens);
  const aster::CaveLightingState spawn_cave_light = run.caveLightingStateAt({0.0f, 0.32f, 0.0f});
  assert(spawn_cave_light.interior < 0.001f);
  assert(spawn_cave_light.entrance_light < 0.001f);
  for (const aster::CaveWallLightSample &light : spawn_cave_light.wall_lights) {
    assert(light.intensity <= 0.001f);
  }
  const aster::CaveLightingState cave_light = run.caveLightingState();
  assert(cave_light.interior > 0.20f);
  assert(!cave_light.wall_lights.empty());
  assert(cave_light.wall_lights.front().intensity > 12.0f);
  assert(cave_light.wall_lights.front().source_radius <= 1.40f);
  assert(cave_light.wall_light >= 0.0f && cave_light.wall_light <= 1.0f);
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

void testControlScheme() {
  aster::ControlScheme controls;
  controls.addCommand("camera.orbit.left");
  controls.bind("camera.orbit.left", {aster::ControlDevice::Keyboard, 263});

  aster::ControlState state;
  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(state.justPressed("camera.orbit.left"));

  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(!state.justPressed("camera.orbit.left"));

  state.update(controls, {{}, {}, {}});
  assert(!state.pressed("camera.orbit.left"));
  assert(state.justReleased("camera.orbit.left"));
}

void testPlayerMotionPlan() {
  const aster::PlayerMovePlan plan = aster::buildPlayerMovePlan(
      {.walk_speed = 2.0f, .run_multiplier = 1.5f, .response_rate = 10.0f, .jump_speed = 5.0f},
      {{2.0f, 0.0f}, true, true});
  expectNear(plan.target_speed, 3.0f, 0.0001f);
  expectNear(aster::length({plan.input.desired_velocity.x, 0.0f, plan.input.desired_velocity.z}),
             3.0f, 0.0001f);
  assert(plan.input.jump_requested);
  expectNear(plan.character_settings.jump_speed, 5.0f, 0.0001f);
}

void testSwimMotionPlan() {
  const aster::PhysicsFluidSample fluid{true, 0.75f, 0.40f, 0.24f, {0.20f, 0.0f, 0.0f}};
  const aster::SwimMotionResult swim =
      aster::buildSwimMotion(fluid, {0.0f, -1.0f, 0.0f}, {{2.0f, 0.0f, 0.0f}, true},
                             {.activation_submersion = 0.25f,
                              .full_swim_submersion = 0.75f,
                              .horizontal_speed_scale = 0.5f,
                              .flow_influence = 0.5f,
                              .surface_clearance = 0.10f,
                              .float_response = 4.0f,
                              .max_upward_speed = 1.0f,
                              .max_downward_speed = 0.4f,
                              .ascend_speed = 1.2f});
  assert(swim.swimming);
  expectNear(swim.blend, 1.0f, 0.0001f);
  assert(swim.desired_velocity.x > 1.0f);
  assert(swim.target_vertical_velocity >= 1.2f);
}

void testClimbMotionPlan() {
  const aster::ClimbableCylinder tree{{0.0f, 0.0f, 0.0f}, 0.40f, 3.0f};
  const aster::ClimbSurfaceSample sample =
      aster::sampleClimbableCylinder(tree, {0.55f, 1.2f, 0.0f},
                                     {.capture_distance = 0.22f,
                                      .character_clearance = 0.16f,
                                      .stick_response = 10.0f,
                                      .tangent_speed_scale = 0.5f,
                                      .ascend_speed = 1.4f,
                                      .hold_vertical_speed = 0.1f,
                                      .max_correction = 0.12f});
  assert(sample.climbable);
  const aster::ClimbMotionResult climb = aster::buildClimbMotion(
      sample,
      {.desired_velocity = {0.0f, 0.0f, 1.0f}, .engage_requested = true, .ascend_requested = true},
      {.capture_distance = 0.22f,
       .character_clearance = 0.16f,
       .stick_response = 10.0f,
       .tangent_speed_scale = 0.5f,
       .ascend_speed = 1.4f,
       .hold_vertical_speed = 0.1f,
       .max_correction = 0.12f});
  assert(climb.climbing);
  assert(climb.desired_velocity.y >= 1.4f);
  assert(aster::length(climb.position_correction) > 0.0f);
}

void testAmphibiousPredatorMotion() {
  aster::AmphibiousPredatorState state;
  state.position = {-0.4f, 0.0f, 0.0f};
  const aster::AmphibiousPredatorUpdate update =
      aster::updateAmphibiousPredator(state,
                                      {.water_center = {0.0f, 0.0f, 0.0f},
                                       .water_radius = {2.0f, 1.0f},
                                       .water_surface_y = 0.2f,
                                       .body_height = 0.2f,
                                       .swim_speed = 0.5f,
                                       .shore_speed = 0.3f,
                                       .pursue_speed = 1.0f,
                                       .aggression = 0.5f,
                                       .notice_radius = 3.0f,
                                       .water_pursuit_margin = 0.1f,
                                       .strike_radius = 0.5f,
                                       .strike_cooldown = 1.0f},
                                      {0.0f, 0.2f, 0.0f}, 0.2f);
  assert(update.mode == aster::AmphibiousMotionMode::Pursue ||
         update.mode == aster::AmphibiousMotionMode::Strike);
  assert(aster::length(update.position - aster::Vec3{-0.4f, 0.0f, 0.0f}) > 0.0f);
}

void testAvatarRigSceneBinding() {
  aster::Material fur;
  fur.surface_pattern = aster::SurfacePattern::FurFibers;
  aster::Material face;
  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 0.7f,
                                                               .fur_material = fur,
                                                               .muzzle_material = face,
                                                               .eye_material = face,
                                                               .nose_material = face});
  assert(rig.parts.size() >= 12u);
  bool has_pointing_finger = false;
  for (const aster::AvatarPart &part : rig.parts) {
    has_pointing_finger = has_pointing_finger || part.role == aster::AvatarPartRole::PointingFinger;
  }
  assert(has_pointing_finger);
  const aster::AvatarBounds bounds = aster::avatarLocalBounds(rig);
  assert(bounds.min.y < -0.35f);
  assert(aster::avatarGroundSupportExtent(rig) > 0.35f);

  aster::Scene scene;
  const aster::AvatarInstance instance =
      aster::appendAvatar(scene, rig, {.position = {1.0f, 0.5f, -2.0f}});
  assert(instance.object_indices.size() == rig.parts.size());
  assert(scene.objects().size() == rig.parts.size());
  aster::applyAvatarPose(scene, rig, instance,
                         {.position = {1.0f, 0.5f, -2.0f},
                          .facing_yaw = aster::radians(45.0f),
                          .gait_phase = 1.0f,
                          .stride_amplitude = 0.5f,
                          .vertical_bob = 0.01f,
                          .head_yaw_offset = 0.35f,
                          .mouth_open = 1.0f});
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::FurFibers);
  assert(scene.objects().front().transform.position.y > 0.0f);
  bool saw_head_turn = false;
  bool saw_open_mouth = false;
  for (std::size_t i = 0; i < rig.parts.size(); ++i) {
    if (rig.parts[i].joint == aster::AvatarJoint::Head) {
      const std::size_t object_index = instance.object_indices[i];
      saw_head_turn = saw_head_turn || scene.objects()[object_index].transform.rotation.y > 1.0f;
    }
    if (rig.parts[i].joint == aster::AvatarJoint::Mouth) {
      const std::size_t object_index = instance.object_indices[i];
      saw_open_mouth = scene.objects()[object_index].transform.scale.y >
                       rig.parts[i].local_transform.scale.y * 3.0f;
    }
  }
  assert(saw_head_turn);
  assert(saw_open_mouth);

  aster::AvatarAnimatorState animator;
  const aster::AvatarPose pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .head_yaw_offset = aster::radians(30.0f),
                                   .mouth_open = 1.0f},
                                  1.0f / 60.0f);
  assert(animator.initialized);
  assert(pose.stride_amplitude > 0.0f);
  assert(pose.head_yaw_offset > 0.0f);
  assert(pose.mouth_open > 0.0f);
  const aster::AvatarPose point_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {},
                                   .desired_facing_yaw = 0.0f,
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .has_attention_target = true,
                                   .attention_target = {1.0f, 1.1f, 2.0f},
                                   .pointing_enabled = true},
                                  1.0f / 30.0f);
  assert(point_pose.point_blend > 0.0f);
  assert(std::abs(point_pose.point_yaw_offset) > 0.001f);
  const aster::AvatarPose swim_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .swim_blend = 1.0f},
                                  1.0f / 30.0f);
  assert(swim_pose.swim_blend > 0.0f);
}

void testThirdPersonFollowController() {
  aster::ThirdPersonFollowState state;
  const aster::ThirdPersonFollowPose pose =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = true,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {40.0f, -12.0f},
                                      .focus_target = {0.0f, 1.0f, 0.0f},
                                      .fallback_yaw = aster::radians(38.0f),
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(pose.active);
  assert(std::abs(pose.camera_yaw - aster::radians(38.0f)) > 0.001f);
  assert(pose.camera_pitch > aster::radians(28.0f));
  expectNear(pose.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose shifted =
      aster::updateThirdPersonFollow(state, {.target_response = 12.0f},
                                     {.active = true,
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(shifted.camera_target.x > 0.0f);
  assert(shifted.camera_target.x < 10.0f);
  const aster::ThirdPersonFollowPose snapped = aster::updateThirdPersonFollow(
      state, {.target_response = 12.0f, .teleport_snap_distance = 4.0f},
      {.active = true,
       .focus_target = {40.0f, 1.0f, 0.0f},
       .fallback_yaw = 0.0f,
       .fallback_pitch = aster::radians(28.0f)},
      1.0f / 60.0f);
  expectNear(snapped.camera_target.x, 40.0f, 0.0001f);
  expectNear(snapped.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose released =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = false,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {100.0f, 0.0f},
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(!released.active);
  expectNear(released.camera_yaw, snapped.camera_yaw, 0.0001f);

  const aster::Vec2 forward = aster::cameraRelativeMoveAxis({0.0f, 1.0f}, 0.0f);
  expectNear(forward.x, 0.0f, 0.0001f);
  expectNear(forward.y, -1.0f, 0.0001f);
  const aster::Vec2 rotated_forward =
      aster::cameraRelativeMoveAxis({0.0f, 1.0f}, aster::radians(90.0f));
  expectNear(rotated_forward.x, -1.0f, 0.0001f);
  expectNear(rotated_forward.y, 0.0f, 0.0001f);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = 0.0f;
  camera.radius = 4.0f;
  const aster::CameraRay ray = camera.screenRay({400.0f, 300.0f}, {800.0f, 600.0f});
  assert(aster::length(ray.direction) > 0.99f);
  assert(ray.direction.z < -0.99f);
}

void testContactVolumes() {
  const aster::VerticalCapsuleContactVolume lower{{0.0f, 0.40f, 0.0f}, 0.22f, 0.28f};
  const aster::VerticalCapsuleContactVolume same_level{{0.34f, 0.42f, 0.0f}, 0.22f, 0.30f};
  assert(aster::overlaps(lower, same_level));

  const aster::VerticalCapsuleContactVolume overhead{{0.10f, 2.20f, 0.0f}, 0.22f, 0.22f};
  assert(!aster::overlaps(lower, overhead));
}

void testPlacementValidation() {
  aster::PlacementValidator validator;
  validator.addForbiddenAabb(aster::makePlacementAabb({0.0f, 0.5f, 0.0f}, {1.0f, 0.5f, 1.0f}));
  assert(validator.rejectsPoint({0.2f, 0.4f, 0.2f}));
  assert(validator.allowsPoint({0.2f, 1.4f, 0.2f}));
  assert(validator.rejectsFootprint({{-0.2f, -0.2f}, {0.2f, 0.2f}}));
  assert(validator.allowsAabb(aster::makePlacementAabb({0.0f, 1.6f, 0.0f}, {0.2f, 0.1f, 0.2f})));

  validator.addForbiddenEllipse({{3.0f, 0.0f}, {0.5f, 0.75f}});
  assert(validator.rejectsFootprint({{2.8f, -0.1f}, {3.2f, 0.1f}}));
  assert(validator.allowsTriangleFootprint({4.0f, 0.0f, 0.0f}, {4.2f, 0.0f, 0.0f},
                                           {4.0f, 0.0f, 0.2f}));

  const aster::PlacementOrientedEllipse oriented_ellipse{.center = {0.0f, 4.0f},
                                                         .forward = {1.0f, 0.0f},
                                                         .radius = {0.75f, 1.0f},
                                                         .radius_forward_negative = 0.45f,
                                                         .radius_forward_positive = 2.0f};
  validator.addForbiddenOrientedEllipse(oriented_ellipse);
  assert(validator.rejectsPoint({1.5f, 0.0f, 4.0f}));
  assert(validator.allowsPoint({-0.75f, 0.0f, 4.0f}));
  assert(validator.rejectsFootprint({{1.85f, 3.95f}, {2.05f, 4.05f}}));
  expectNear(aster::normalizedDistance(oriented_ellipse, {1.0f, 4.0f}), 0.5f, 0.0001f);

  const aster::PlacementEllipse ellipse{{0.0f, 0.0f}, {2.0f, 1.0f}};
  assert(aster::contains(ellipse, aster::Vec2{1.0f, 0.0f}));
  assert(!aster::contains(ellipse, aster::Vec2{2.2f, 0.0f}));
  assert(aster::contains(ellipse,
                         aster::makePlacementFootprintFromBounds({-0.5f, -0.25f}, {0.5f, 0.25f})));
  const aster::PlacementFootprint edge_overlap =
      aster::makePlacementFootprintFromBounds({1.8f, -0.20f}, {2.2f, 0.20f});
  assert(aster::intersects(ellipse, edge_overlap));
  assert(!aster::contains(ellipse, edge_overlap));
  expectNear(aster::normalizedDistance(ellipse, {1.0f, 0.0f}), 0.5f, 0.0001f);
  const aster::PlacementEllipseBand shoreline_band{ellipse, 0.82f, 1.08f};
  assert(!aster::contains(shoreline_band, aster::Vec2{0.5f, 0.0f}));
  assert(aster::contains(shoreline_band, aster::Vec2{1.8f, 0.0f}));
  assert(!aster::contains(shoreline_band, aster::Vec2{2.4f, 0.0f}));
}

void testNetworkFrameCodec() {
  aster::net::NetMessage message;
  message.channel = 42;
  message.sequence = 9;
  message.source_node = 11;
  message.target_node = 12;
  message.payload = aster::net::bytesFromText("hello");

  std::vector<std::uint8_t> frame = aster::net::encodeFrame(message);
  aster::net::NetMessage decoded;
  assert(aster::net::decodeNextFrame(frame, decoded) == aster::net::FrameDecodeResult::Ready);
  assert(frame.empty());
  assert(decoded.channel == message.channel);
  assert(decoded.sequence == message.sequence);
  assert(decoded.source_node == message.source_node);
  assert(decoded.target_node == message.target_node);
  assert(aster::net::textFromBytes(decoded.payload) == "hello");

  std::vector<std::uint8_t> partial = aster::net::encodeFrame(message);
  partial.pop_back();
  assert(aster::net::decodeNextFrame(partial, decoded) ==
         aster::net::FrameDecodeResult::Incomplete);
}

void testNodeRouter() {
  aster::net::NodeRouter router;
  int delivered = 0;
  const auto token = router.subscribe(7, [&](const aster::net::NetMessage &message) {
    delivered += message.sequence == 5 ? 1 : 0;
  });

  aster::net::NetMessage message;
  message.channel = 7;
  message.sequence = 5;
  router.enqueue(message);
  assert(router.drainAll() == 1u);
  assert(delivered == 1);
  assert(router.stats().delivered == 1u);
  assert(router.unsubscribe(token));
  assert(!router.route(message));
  assert(router.stats().dropped == 1u);
}

void testPhysicsWorldGravityAndStaticContact() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle floor =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, -0.25f, 0.0f},
                     {4.0f, 0.25f, 4.0f}});
  const aster::PhysicsBodyHandle ball = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {0.0f, 2.0f, 0.0f},
                                                       {0.5f, 0.5f, 0.5f},
                                                       0.5f,
                                                       1.0f,
                                                       {0.45f, 0.0f}});

  for (int i = 0; i < 150; ++i) {
    world.step(1.0f / 60.0f);
  }

  const aster::PhysicsBody &body = world.body(ball);
  expectNear(body.position.y, 0.5f, 0.035f);
  assert(!world.contacts().empty());
}

void testPhysicsDistanceConstraint() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  const aster::PhysicsBodyHandle body = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {3.0f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f});
  [[maybe_unused]] const aster::PhysicsConstraintHandle constraint = world.addDistanceConstraint(
      {body, {0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 0.0f, aster::DistanceConstraintMode::Maximum});
  world.step(1.0f / 60.0f);
  assert(aster::length(world.body(body).position) <= 1.001f);
}

void testPhysicsQueriesAndDynamicContact() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle wall =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, 0.0f, -2.0f},
                     {0.7f, 0.7f, 0.12f}});
  const aster::PhysicsBodyHandle left = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {-0.22f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f,
                                                       1.0f,
                                                       {0.35f, 0.0f}});
  const aster::PhysicsBodyHandle right = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                        aster::PhysicsShapeType::Sphere,
                                                        {0.22f, 0.0f, 0.0f},
                                                        {0.25f, 0.25f, 0.25f},
                                                        0.25f,
                                                        1.0f,
                                                        {0.35f, 0.0f}});

  world.step(1.0f / 60.0f);
  assert(aster::length(world.body(left).position - world.body(right).position) >= 0.49f);

  aster::PhysicsRayHit ray_hit;
  assert(world.raycast({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, 3.0f}, ray_hit));
  assert(aster::samePhysicsHandle(ray_hit.body, wall));

  aster::PhysicsShapeCastHit cast_hit;
  assert(world.castSphere({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -3.0f}, 0.25f}, cast_hit));
  assert(aster::samePhysicsHandle(cast_hit.body, wall));

  const std::vector<aster::PhysicsOverlapHit> overlaps =
      world.overlapSphere({0.0f, 0.0f, 0.0f}, 0.7f);
  assert(overlaps.size() >= 2u);
}

void testPhysicsStaticTriangleMeshContact() {
  aster::CpuMesh wall_mesh;
  wall_mesh.vertices = {{{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                        {{0.0f, 1.2f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                        {{0.0f, 1.2f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                        {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}};
  wall_mesh.indices = {0, 1, 2, 0, 2, 3};

  aster::PhysicsWorld wall_world;
  wall_world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc wall_desc;
  wall_desc.type = aster::PhysicsBodyType::Static;
  wall_desc.shape = aster::PhysicsShapeType::TriangleMesh;
  wall_desc.mesh = std::make_shared<const aster::CpuMesh>(wall_mesh);
  [[maybe_unused]] const aster::PhysicsBodyHandle wall = wall_world.addBody(wall_desc);

  aster::PhysicsShapeCastHit center_sweep_hit;
  assert(
      wall_world.castSphere({{-1.0f, 0.60f, 0.0f}, {2.0f, 0.0f, 0.0f}, 0.25f}, center_sweep_hit));
  assert(aster::samePhysicsHandle(center_sweep_hit.body, wall));
  assert(center_sweep_hit.distance > 0.70f && center_sweep_hit.distance < 0.80f);

  aster::PhysicsShapeCastHit edge_sweep_hit;
  assert(wall_world.castSphere({{-1.0f, 0.60f, 1.12f}, {2.0f, 0.0f, 0.0f}, 0.25f}, edge_sweep_hit));
  assert(aster::samePhysicsHandle(edge_sweep_hit.body, wall));

  aster::PhysicsBodyDesc capsule_desc;
  capsule_desc.type = aster::PhysicsBodyType::Dynamic;
  capsule_desc.shape = aster::PhysicsShapeType::Capsule;
  capsule_desc.position = {-0.10f, 0.60f, 0.0f};
  capsule_desc.half_extents = {0.20f, 0.32f, 0.20f};
  capsule_desc.radius = 0.20f;
  capsule_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle capsule = wall_world.addBody(capsule_desc);
  wall_world.step(1.0f / 60.0f);
  assert(wall_world.body(capsule).position.x < -0.19f);

  aster::CpuMesh corridor_mesh;
  corridor_mesh.vertices = {{{-0.55f, 0.0f, -1.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                            {{-0.55f, 1.2f, -1.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                            {{-0.55f, 1.2f, 1.2f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                            {{-0.55f, 0.0f, 1.2f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                            {{0.55f, 0.0f, -1.2f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                            {{0.55f, 1.2f, -1.2f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                            {{0.55f, 1.2f, 1.2f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                            {{0.55f, 0.0f, 1.2f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}};
  corridor_mesh.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7};

  aster::PhysicsWorld corridor_world;
  corridor_world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc corridor_desc;
  corridor_desc.type = aster::PhysicsBodyType::Static;
  corridor_desc.shape = aster::PhysicsShapeType::TriangleMesh;
  corridor_desc.mesh = std::make_shared<const aster::CpuMesh>(corridor_mesh);
  [[maybe_unused]] const aster::PhysicsBodyHandle corridor = corridor_world.addBody(corridor_desc);

  capsule_desc.position = {0.0f, 0.60f, -1.0f};
  const aster::PhysicsBodyHandle walker = corridor_world.addBody(capsule_desc);
  corridor_world.setVelocity(walker, {0.0f, 0.0f, 1.4f});
  for (int i = 0; i < 80; ++i) {
    corridor_world.step(1.0f / 60.0f);
  }
  assert(corridor_world.body(walker).position.z > 0.60f);
  expectNear(corridor_world.body(walker).position.y, 0.60f, 0.001f);
}

void testPhysicsCharacterController() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  [[maybe_unused]] const aster::PhysicsBodyHandle floor =
      world.addBody({aster::PhysicsBodyType::Static,
                     aster::PhysicsShapeType::Box,
                     {0.0f, -0.2f, 0.0f},
                     {4.0f, 0.2f, 4.0f}});

  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.55f, 0.0f};
  character_desc.half_extents = {0.25f, 0.30f, 0.25f};
  character_desc.radius = 0.25f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  for (int i = 0; i < 80; ++i) {
    (void)world.moveCharacter(character, {{1.6f, 0.0f, 0.0f}, false}, {}, 1.0f / 60.0f);
    world.step(1.0f / 60.0f);
  }

  aster::CharacterMoveResult state =
      world.moveCharacter(character, {{0.0f, 0.0f, 0.0f}, false}, {}, 1.0f / 60.0f);
  assert(state.grounded);
  assert(world.body(character).position.x > 0.4f);
}

void testTerrainCharacterContact() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.5f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::CharacterControllerSettings controller;
  controller.jump_speed = 4.0f;
  const aster::CharacterMoveResult state = aster::moveCharacterOnTerrain(
      world, character, terrain, {{0.0f, 0.0f, 0.0f}, true},
      {.controller = controller, .support_extent_y = 0.5f}, 1.0f / 60.0f);
  assert(state.grounded);
  expectNear(world.body(character).velocity.y, 4.0f, 0.0001f);
}

void testTerrainCharacterRaisesToSurface() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f};

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.0f, 0.50f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::CharacterControllerSettings controller;
  controller.ground_probe_distance = 0.5f;
  const aster::CharacterMoveResult state = aster::moveCharacterOnTerrain(
      world, character, terrain, {{0.0f, 0.0f, 0.0f}, false},
      {.controller = controller, .support_extent_y = 0.5f}, 1.0f / 60.0f);
  assert(state.grounded);
  expectNear(world.body(character).position.y, 0.85f, 0.0001f);
}

void testMeshSupportSurface() {
  aster::SupportSurfaceSet support;
  support.addMesh({std::make_shared<const aster::CpuMesh>(aster::makePlane(2.0f)),
                   {{0.0f, 0.24f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}},
                   0.5f});
  support.addBox({{1.0f, 0.50f, 1.0f}, {0.25f, 0.25f, 0.25f}});
  const aster::TerrainSurfaceSample sample = support.sample(aster::Vec2{0.0f, 0.0f});
  assert(sample.valid);
  expectNear(sample.height, 0.24f, 0.0001f);
  assert(sample.normal.y > 0.9f);
  const aster::TerrainSurfaceSample box_sample = support.sample(aster::Vec2{1.0f, 1.0f});
  assert(box_sample.valid);
  expectNear(box_sample.height, 0.75f, 0.0001f);
  const aster::TerrainSurfaceSample blocked_overhead =
      support.sample({{1.0f, 1.0f}, 0.10f, 0.12f, 2.0f});
  assert(!blocked_overhead.valid);
  const aster::TerrainSurfaceSample near_top = support.sample({{1.0f, 1.0f}, 0.66f, 0.12f, 2.0f});
  assert(near_top.valid);
  expectNear(near_top.height, 0.75f, 0.0001f);
}

void testSupportSurfaceTerrainPlacementFilter() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 3;
  terrain.square_size = 1.0f;
  terrain.origin = {-1.0f, -1.0f};
  terrain.heights = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  aster::PlacementValidator validator;
  validator.addForbiddenAabb(aster::makePlacementAabb({0.0f, 0.0f, 0.0f}, {0.4f, 0.2f, 0.4f}));

  aster::SupportSurfaceSet support;
  support.setTerrain(&terrain);
  support.setTerrainPlacementValidator(validator);
  assert(!support.sample(aster::Vec2{0.0f, 0.0f}).valid);

  support.addBox({{0.0f, 0.20f, 0.0f}, {0.3f, 0.1f, 0.3f}});
  const aster::TerrainSurfaceSample raised = support.sample(aster::Vec2{0.0f, 0.0f});
  assert(raised.valid);
  expectNear(raised.height, 0.30f, 0.0001f);
}

void testSupportSurfaceOrientedTerrainPlacementFilter() {
  aster::TerrainHeightField terrain;
  terrain.grid_size = 5;
  terrain.square_size = 1.0f;
  terrain.origin = {-2.0f, -2.0f};
  terrain.heights.assign(25u, 2.0f);

  aster::PlacementValidator validator;
  validator.addForbiddenOrientedEllipse({.center = {0.0f, 0.0f},
                                         .forward = {1.0f, 0.0f},
                                         .radius = {0.65f, 1.0f},
                                         .radius_forward_negative = 0.50f,
                                         .radius_forward_positive = 1.80f});

  aster::SupportSurfaceSet support;
  support.setTerrain(&terrain);
  support.setTerrainPlacementValidator(validator);
  assert(!support.sample(aster::Vec2{1.0f, 0.0f}).valid);

  support.addBox({{1.0f, 0.20f, 0.0f}, {0.30f, 0.10f, 0.30f}});
  const aster::TerrainSurfaceSample cave_floor = support.sample(aster::Vec2{1.0f, 0.0f});
  assert(cave_floor.valid);
  expectNear(cave_floor.height, 0.30f, 0.0001f);

  const aster::TerrainSurfaceSample outside_cover = support.sample(aster::Vec2{-0.90f, 0.0f});
  assert(outside_cover.valid);
  expectNear(outside_cover.height, 2.0f, 0.0001f);
}

void testSurfaceStepAssist() {
  aster::SupportSurfaceSet support;
  support.addBox({{0.0f, 0.0f, 0.0f}, {0.45f, 0.05f, 0.45f}});
  support.addBox({{0.55f, 0.16f, 0.0f}, {0.30f, 0.16f, 0.45f}});

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {0.25f, 0.55f, 0.0f};
  character_desc.half_extents = {0.20f, 0.28f, 0.20f};
  character_desc.radius = 0.20f;
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  const auto sampler = [&](const aster::Vec3 position) {
    return support.sample({{position.x, position.z}, position.y, 0.40f, 0.20f});
  };
  const aster::CharacterMoveResult assisted =
      aster::applySurfaceStepAssist(world, character, sampler, {1.0f, 0.0f, 0.0f},
                                    {.support_extent_y = 0.50f,
                                     .probe_distance = 0.32f,
                                     .max_step_up = 0.35f,
                                     .max_step_down = 0.05f,
                                     .min_horizontal_speed = 0.1f,
                                     .skin_width = 0.0f});
  assert(assisted.grounded);
  expectNear(world.body(character).position.y, 0.82f, 0.0001f);
}

void testContinuousHorizontalCollisionBlocksFastSweep() {
  constexpr std::uint32_t world_layer = 1u << 0u;
  constexpr std::uint32_t character_layer = 1u << 1u;

  aster::PhysicsWorld world;
  world.setSettings({{0.0f, 0.0f, 0.0f}, 8, 1.0f / 120.0f});
  aster::PhysicsBodyDesc character_desc;
  character_desc.type = aster::PhysicsBodyType::Dynamic;
  character_desc.shape = aster::PhysicsShapeType::Capsule;
  character_desc.position = {-1.0f, 0.0f, 0.0f};
  character_desc.half_extents = {0.25f, 0.40f, 0.25f};
  character_desc.radius = 0.25f;
  character_desc.filter = {character_layer, world_layer};
  character_desc.allow_sleep = false;
  const aster::PhysicsBodyHandle character = world.addBody(character_desc);

  aster::PhysicsBodyDesc wall_desc;
  wall_desc.type = aster::PhysicsBodyType::Static;
  wall_desc.shape = aster::PhysicsShapeType::Box;
  wall_desc.position = {0.0f, 0.0f, 0.0f};
  wall_desc.half_extents = {0.10f, 1.0f, 1.0f};
  wall_desc.filter = {world_layer, character_layer};
  [[maybe_unused]] const aster::PhysicsBodyHandle wall = world.addBody(wall_desc);

  aster::PhysicsBody &body = world.body(character);
  body.previous_position = {-1.0f, 0.0f, 0.0f};
  body.position = {1.0f, 0.0f, 0.0f};
  body.velocity = {12.0f, 0.0f, 0.0f};

  const bool resolved = aster::resolveContinuousHorizontalCollision(
      world, character, {.radius = 0.25f, .collision_mask = world_layer});
  assert(resolved);
  assert(world.body(character).position.x < -0.30f);
  assert(world.body(character).velocity.x <= 0.0001f);
}

void testPhysicsFluidVolumeDragAndBuoyancy() {
  aster::PhysicsWorld world;
  world.setSettings({{0.0f, -9.81f, 0.0f}, 8, 1.0f / 120.0f});
  const aster::PhysicsBodyHandle ball = world.addBody({aster::PhysicsBodyType::Dynamic,
                                                       aster::PhysicsShapeType::Sphere,
                                                       {0.0f, 0.0f, 0.0f},
                                                       {0.25f, 0.25f, 0.25f},
                                                       0.25f,
                                                       1.0f,
                                                       {0.20f, 0.0f}});
  [[maybe_unused]] const aster::PhysicsFluidHandle water = world.addFluidVolume(
      {{0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 2.0f}, 0.4f, 1.25f, 5.0f, {0.0f, 0.0f, 0.0f}});
  world.setVelocity(ball, {3.0f, -2.0f, 0.0f});
  world.step(1.0f / 30.0f);

  const aster::PhysicsBody &body = world.body(ball);
  assert(body.velocity.x < 3.0f);
  assert(body.velocity.y > -2.0f);
  const aster::PhysicsFluidSample sample = world.sampleFluid(ball);
  assert(sample.submerged);
  expectNear(sample.surface_y, 0.4f, 0.0001f);
  assert(sample.depth_below_surface > 0.0f);
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
  testSourceBoundaryContracts();
  testFramebufferOriginContract();
  testUiTextFittingContract();
  testGameplayItemInteractionSystems();
  testMeshGeneration();
  testTerrainSculpting();
  testCaveInteriorVolume();
  testVoxelCaveStreamingAndFixtureContracts();
  testMeshProcessingPipeline();
  testMeshProcessingRejectsInvalidIndices();
  testMeshDrapingRaisesEmbeddedVertices();
  testSceneAssetNormalTangentImport();
  testBrushLevelMeshBuild();
  testArchitecturalMeshBuild();
  testVoxelStructureBuild();
  testCastleCourseBuild();
  testSceneContract();
  testMaterialRenderPolicies();
  testRustRenderFramePlanContracts();
  testGeneratedSceneryAssembly();
  testSceneCoherenceEnergy();
  testSceneTraceValidation();
  testSceneTraceFoModelChecking();
  testSceneTraceFoQuantifierRank();
  testSceneTraceFoSeparatorSolving();
  testLumenSceneCoherenceReport();
  testLumenCameraCollisionCanBeatComfortRadius();
  testLumenInnerPondSeamHasSupport();
  testLumenSupportSurfacesRenderOpaque();
  testLumenSupplyCrateInventoryContract();
  testLumenCaveVisualContracts();
  testLumenPondWallLightIsMountedOutsideWater();
  testControlScheme();
  testPlayerMotionPlan();
  testSwimMotionPlan();
  testClimbMotionPlan();
  testAmphibiousPredatorMotion();
  testAvatarRigSceneBinding();
  testThirdPersonFollowController();
  testContactVolumes();
  testPlacementValidation();
  testNetworkFrameCodec();
  testNodeRouter();
  testPhysicsWorldGravityAndStaticContact();
  testPhysicsDistanceConstraint();
  testPhysicsQueriesAndDynamicContact();
  testPhysicsStaticTriangleMeshContact();
  testPhysicsCharacterController();
  testTerrainCharacterContact();
  testTerrainCharacterRaisesToSurface();
  testMeshSupportSurface();
  testSupportSurfaceTerrainPlacementFilter();
  testSupportSurfaceOrientedTerrainPlacementFilter();
  testSurfaceStepAssist();
  testContinuousHorizontalCollisionBlocksFastSweep();
  testPhysicsFluidVolumeDragAndBuoyancy();
  std::cout << "Aster tests passed.\n";
  return 0;
}
