// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/mesh_pipeline.hpp"
#include "aster/asset/scene_asset_importer.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/core/frame_time_stats.hpp"
#include "aster/core/profiler.hpp"
#include "aster/core/work_budget.hpp"
#include "aster/systems/animation_system.hpp"
#include "aster/systems/creature_motion.hpp"
#include "aster/systems/equipment_system.hpp"
#include "aster/systems/interaction_system.hpp"
#include "aster/systems/inventory_system.hpp"
#include "aster/systems/item_system.hpp"
#include "aster/systems/light_system.hpp"
#include "aster/samples/lumen_run.hpp"
#include "aster/systems/particle_system.hpp"
#include "aster/systems/player_motion.hpp"
#include "aster/systems/third_person_camera.hpp"
#include "aster/geometry/architectural_mesh.hpp"
#include "aster/geometry/brush_level_mesh.hpp"
#include "aster/geometry/cable_mesh.hpp"
#include "aster/geometry/castle_course.hpp"
#include "aster/geometry/cave_system.hpp"
#include "aster/geometry/cave_web_mesh.hpp"
#include "aster/geometry/energy_conduit_mesh.hpp"
#include "aster/geometry/fracture_mesh.hpp"
#include "aster/geometry/generated_scenery.hpp"
#include "aster/geometry/implicit_surface.hpp"
#include "aster/geometry/mesh_modeling.hpp"
#include "aster/geometry/mesh_projection.hpp"
#include "aster/geometry/mesh_cut.hpp"
#include "aster/geometry/nature_mesh.hpp"
#include "aster/geometry/stroke_mesh.hpp"
#include "aster/geometry/terrain_mesh.hpp"
#include "aster/geometry/terrain_sculpt.hpp"
#include "aster/geometry/tube_mesh.hpp"
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
#include "aster/render/material_compiler.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/render/render_scene.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/showcase_scenes.hpp"
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

const aster::SceneCoherenceContribution *findContribution(const aster::SceneCoherenceReport &report,
                                                          const aster::SceneCoherenceTerm term) {
  for (const aster::SceneCoherenceContribution &contribution : report.contributions) {
    if (contribution.term == term) {
      return &contribution;
    }
  }
  return nullptr;
}

} // namespace
