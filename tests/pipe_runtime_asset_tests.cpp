// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/pipe_runtime_asset.hpp"
#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/geometry/mesh_modeling.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/showcase_scenes.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

#ifndef ASTER_ASSETC_EXECUTABLE
#define ASTER_ASSETC_EXECUTABLE ""
#endif

namespace {

std::string shellQuote(const std::filesystem::path &path) {
  std::string value = path.string();
  std::string quoted = "'";
  for (const char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(c);
    }
  }
  quoted += "'";
  return quoted;
}

void requireCommand(const std::string &command) {
  const int result = std::system(command.c_str());
  if (result != 0) {
    throw std::runtime_error("command failed: " + command);
  }
}

std::filesystem::path findAssetGraphBin(const std::filesystem::path &root) {
  if (!std::filesystem::exists(root)) {
    return {};
  }
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (entry.path().extension() == ".assetgraphbin") {
      return entry.path();
    }
  }
  return {};
}

bool meshHasVisiblePixel(const aster::SoftwareFrameBuffer &framebuffer) {
  for (std::size_t i = 0u; i + 3u < framebuffer.rgba8().size(); i += 4u) {
    if (framebuffer.rgba8()[i + 0u] > 8u || framebuffer.rgba8()[i + 1u] > 8u ||
        framebuffer.rgba8()[i + 2u] > 8u) {
      return true;
    }
  }
  return false;
}

std::size_t colorBucketCount(const aster::SoftwareFrameBuffer &framebuffer) {
  std::set<int> buckets;
  for (std::size_t i = 0u; i + 3u < framebuffer.rgba8().size(); i += 4u) {
    const int r = framebuffer.rgba8()[i + 0u];
    const int g = framebuffer.rgba8()[i + 1u];
    const int b = framebuffer.rgba8()[i + 2u];
    if (r + g + b <= 24) {
      continue;
    }
    buckets.insert((r / 32) * 64 + (g / 32) * 8 + (b / 32));
  }
  return buckets.size();
}

void assertFiniteRenderableMesh(const aster::CpuMesh &mesh) {
  assert(!mesh.vertices.empty());
  assert(!mesh.indices.empty());
  assert(mesh.indices.size() % 3u == 0u);
  for (const aster::Vertex &vertex : mesh.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
    assert(aster::length(vertex.normal) > 0.45f);
    assert(aster::length(aster::Vec3{vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) >
           0.45f);
    assert(vertex.uv.x >= -0.01f);
    assert(vertex.uv.y >= -0.01f);
  }
  const aster::MeshTopologyReport report = aster::validateMeshTopology(mesh);
  assert(report.indexable());
  assert(report.triangles > 0u);
  assert(report.invalid_indices == 0u);
  assert(report.degenerate_triangles == 0u);
  assert(report.bounds.valid);
}

float minRadialDistance(const aster::CpuMesh &mesh) {
  float min_radius = std::numeric_limits<float>::max();
  for (const aster::Vertex &vertex : mesh.vertices) {
    min_radius =
        std::min(min_radius,
                 std::sqrt(vertex.position.y * vertex.position.y +
                           vertex.position.z * vertex.position.z));
  }
  return min_radius;
}

float maxRadialDistance(const aster::CpuMesh &mesh) {
  float max_radius = 0.0f;
  for (const aster::Vertex &vertex : mesh.vertices) {
    max_radius =
        std::max(max_radius,
                 std::sqrt(vertex.position.y * vertex.position.y +
                           vertex.position.z * vertex.position.z));
  }
  return max_radius;
}

void testPipeRuntimeAssetModel() {
  const aster::AsterPipeAsset asset = aster::makeAsterPipeAsset(
      {.asset_id = "test.pipe.production",
       .radial_segments = 72,
       .length_segments = 18,
       .bolt_count_per_flange = 8,
       .rust_strength = 0.88f,
       .wetness_strength = 0.24f});
  assert(asset.cook_report.production_ready);
  assert(asset.parts.size() >= 4u);
  assert(asset.material_slots.size() == 3u);
  assert(asset.uv_islands.size() == 3u);
  assert(asset.rust_anchors.size() == 11u);
  assert(asset.wetness_streaks.size() == 7u);
  assert(asset.surface_masks.size() == asset.parts.size());
  assert(std::any_of(asset.surface_masks.begin(), asset.surface_masks.end(),
                     [](const aster::AsterPipeSurfaceMask &mask) {
                       return mask.pitting_coverage > 0.02f && mask.oxide_coverage > 0.05f;
                     }));
  assert(std::any_of(asset.surface_masks.begin(), asset.surface_masks.end(),
                     [](const aster::AsterPipeSurfaceMask &mask) {
                       return mask.cavity_coverage > 0.01f || mask.wetness_coverage > 0.01f;
                     }));
  assert(asset.lods.size() == 3u);
  assert(asset.lods[0].mesh.indices.size() > asset.lods[1].mesh.indices.size());
  assert(asset.lods[1].mesh.indices.size() > asset.lods[2].mesh.indices.size());
  assert(asset.collision_proxies.size() == 1u);
  assert(asset.collision_proxies.front().covers_render_bounds);
  assert(asset.collision_proxies.front().triangle_budget <= 64u);
  assert(asset.cook_report.dependency_edges.size() >= 6u);
  assert(asset.cook_report.modifier_stack_hash != 0u);
  assert(std::any_of(asset.cook_report.dependency_edges.begin(),
                     asset.cook_report.dependency_edges.end(), [](const std::string &edge) {
                       return edge.find("voronoi_pitting") != std::string::npos;
                     }));
  assert(std::any_of(asset.cook_report.dependency_edges.begin(),
                     asset.cook_report.dependency_edges.end(), [](const std::string &edge) {
                       return edge.find("contact_skirt") != std::string::npos;
                     }));
  assert(std::any_of(asset.cook_report.dependency_edges.begin(),
                     asset.cook_report.dependency_edges.end(), [](const std::string &edge) {
                       return edge.find("depth_bias_policy") != std::string::npos;
                     }));
  bool saw_contact_weld = false;
  bool saw_inset_seam = false;
  for (const aster::AsterPipeAssetPart &part : asset.parts) {
    assert(part.name.find("flange") == std::string::npos);
    assert(part.name.find("bolt") == std::string::npos);
    if (part.name.find("contact skirt") != std::string::npos) {
      saw_contact_weld = true;
      assert(part.material_slot == "pipe.weld");
      assert(minRadialDistance(part.mesh) <= 0.54f + 0.010f);
      assert(maxRadialDistance(part.mesh) >= 0.54f + 0.040f);
    }
    if (part.name.find("longitudinal") != std::string::npos) {
      saw_inset_seam = true;
      assert(part.material_slot == "pipe.body");
      assert(maxRadialDistance(part.mesh) <= 0.54f + 0.012f);
    }
  }
  assert(saw_contact_weld);
  assert(saw_inset_seam);
  assertFiniteRenderableMesh(asset.mergedRenderMesh());
  for (const aster::AsterPipeLod &lod : asset.lods) {
    assertFiniteRenderableMesh(lod.mesh);
  }

  const aster::AsterPipeAsset hardware_asset = aster::makeAsterPipeAsset(
      {.asset_id = "test.pipe.hardware_variant",
       .radial_segments = 72,
       .length_segments = 18,
       .include_flanges = true,
       .include_bolts = true,
       .bolt_count_per_flange = 8,
       .rust_strength = 0.88f,
       .wetness_strength = 0.24f});
  assert(std::any_of(hardware_asset.parts.begin(), hardware_asset.parts.end(),
                     [](const aster::AsterPipeAssetPart &part) {
                       return part.name.find("flange") != std::string::npos;
                     }));
  assert(std::any_of(hardware_asset.parts.begin(), hardware_asset.parts.end(),
                     [](const aster::AsterPipeAssetPart &part) {
                       return part.name.find("bolt") != std::string::npos;
                     }));
}

void testPipeAssetGraphCook() {
  const std::filesystem::path source_dir = ASTER_SOURCE_DIR;
  const std::filesystem::path assetc = ASTER_ASSETC_EXECUTABLE;
  if (assetc.empty() || !std::filesystem::exists(assetc)) {
    throw std::runtime_error("ASTER_ASSETC_EXECUTABLE is not available for pipe asset test");
  }
  const std::filesystem::path graph = source_dir / "showcases" / "pipe_lab" /
                                      "rusted_pipe.astergraph";
  const std::filesystem::path project = source_dir / "showcases" / "pipe_lab" /
                                        "pipe_lab.asterproj";
  const std::filesystem::path packaged =
      std::filesystem::temp_directory_path() / "aster_pipe_assetgraph";
  const std::filesystem::path cooked = std::filesystem::temp_directory_path() /
                                       "aster_pipe_lab_cooked";
  std::filesystem::remove_all(packaged);
  std::filesystem::remove_all(cooked);

  requireCommand(shellQuote(assetc) + " graph-inspect --input " + shellQuote(graph) + " > " +
                 shellQuote(packaged.parent_path() / "aster_pipe_graph_inspect.txt"));
  requireCommand(shellQuote(assetc) + " graph-package --input " + shellQuote(graph) +
                 " --output " + shellQuote(packaged));
  requireCommand(shellQuote(assetc) + " cook --project " + shellQuote(project) +
                 " --platform desktop --output " + shellQuote(cooked));

  const std::filesystem::path assetgraphbin = findAssetGraphBin(packaged);
  if (assetgraphbin.empty()) {
    throw std::runtime_error("pipe graph package did not emit an .assetgraphbin");
  }
  const aster::ProceduralAssetGraphPackage package =
      aster::loadProceduralAssetGraphPackage(assetgraphbin);
  assert(package.mesh.primitive == "rusted-pipe");
  assert(package.mesh.collision_proxy == "pipe-runtime-bounds");
  assert(package.mesh.lod_policy == "lod0-lod1-lod2");
  assert(package.quality.production_ready);
  assert(package.quality.score >= 80u);
  assert((package.feature_mask & (1ull << 39u)) != 0u);
  assert((package.feature_mask & (1ull << 40u)) != 0u);
  assert((package.feature_mask & (1ull << 41u)) != 0u);
  assert((package.feature_mask & (1ull << 42u)) != 0u);
  assert((package.feature_mask & (1ull << 43u)) != 0u);
  assert((package.feature_mask & (1ull << 44u)) != 0u);
  assert((package.feature_mask & (1ull << 45u)) != 0u);
  assert((package.feature_mask & (1ull << 46u)) != 0u);
  assert((package.feature_mask & (1ull << 48u)) != 0u);
  assert((package.feature_mask & (1ull << 49u)) != 0u);
  for (std::uint32_t bit = 57u; bit <= 63u; ++bit) {
    assert((package.feature_mask & (1ull << bit)) != 0u);
  }
  assert(!package.factory_report.stable_recipe_hash.empty());
  assert(package.factory_report.stage_diagnostics.size() >= 6u);
  assert(std::any_of(package.factory_report.surface_signal_coverage.begin(),
                     package.factory_report.surface_signal_coverage.end(),
                     [](const aster::ProceduralAssetGraphFactorySignalCoverage &signal) {
                       return signal.signal == "open_hollow_rims" &&
                              signal.status == "claimed";
                     }));
  assert(package.factory_report.collision_proxy_summary.at("shape") ==
         "pipe-runtime-bounds");
  assert(std::find(package.factory_report.visual_brief_claims.begin(),
                   package.factory_report.visual_brief_claims.end(),
                   "reference_silhouette") !=
         package.factory_report.visual_brief_claims.end());
  assert(std::find(package.factory_report.visual_brief_rejections.begin(),
                   package.factory_report.visual_brief_rejections.end(),
                   "decorative_bolts_without_reference") !=
         package.factory_report.visual_brief_rejections.end());
  assert(!package.production_session.session_id.empty());
  assert(package.production_session.quality_gate == "production-ready");
  assert(!package.production_session.preview_artifact_hash.empty());
  assert(std::find(package.production_session.cook_steps.begin(),
                   package.production_session.cook_steps.end(),
                   "preview-render") != package.production_session.cook_steps.end());
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "pipe_body" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "weld_seam" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  assertFiniteRenderableMesh(aster::proceduralAssetGraphMesh(package));

  const std::filesystem::path asset_db = cooked / "assetdb.asterdb.json";
  assert(std::filesystem::exists(asset_db));
  std::ifstream file(asset_db);
  const std::string db_text((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  assert(db_text.find("asset_graph.pipe_lab.rusted_pipe") != std::string::npos);
  assert(db_text.find("assetgraphbin") != std::string::npos);
}

void testIndustrialPipePreviewRenders() {
  const aster::Scene scene = aster::makeIndustrialPipeScene();
  std::size_t runtime_parts = 0u;
  std::size_t weld_profiles = 0u;
  std::size_t corroded_profiles = 0u;
  for (const aster::RenderObject &object : scene.objects()) {
    runtime_parts += object.name.find("runtime rusted pipe") != std::string::npos ? 1u : 0u;
    weld_profiles += aster::resolveMaterialSurfaceProfile(object.material) ==
                             aster::MaterialSurfaceProfile::WeldBead
                         ? 1u
                         : 0u;
    corroded_profiles += aster::resolveMaterialSurfaceProfile(object.material) ==
                                 aster::MaterialSurfaceProfile::CorrodedMetal
                             ? 1u
                             : 0u;
    if (aster::resolveMaterialSurfaceProfile(object.material) ==
        aster::MaterialSurfaceProfile::CorrodedMetal) {
      assert(object.material.procedural.pitting_density > 0.40f);
      assert(object.material.procedural.cavity_grime > 0.30f);
      assert(object.material.procedural.axial_scratches > 0.20f);
    }
    if (aster::resolveMaterialSurfaceProfile(object.material) ==
        aster::MaterialSurfaceProfile::WeldBead) {
      assert(object.material.procedural.weld_heat_tint > 0.40f);
      assert(object.material.depth_policy.layer == aster::RenderDepthLayer::SurfaceAttachment);
      assert(object.material.depth_policy.constant_bias > 0.0f);
    }
    if (object.custom_mesh != nullptr) {
      assertFiniteRenderableMesh(*object.custom_mesh);
    }
  }
  assert(runtime_parts >= 3u);
  assert(weld_profiles >= 2u);
  assert(corroded_profiles >= 1u);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.58f, 0.0f};
  camera.yaw = aster::radians(64.0f);
  camera.pitch = aster::radians(15.0f);
  camera.radius = 5.0f;
  camera.vertical_fov = aster::radians(42.0f);

  aster::RendererSettings settings;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.48f, 0.82f, 0.32f};
  settings.sun_light.intensity = 2.2f;
  settings.procedural_surface_normals = true;
  settings.ambient_strength = 0.24f;
  settings.pipeline.clear_color = {0.020f, 0.024f, 0.026f};

  const aster::SoftwareFrameBuffer framebuffer =
      aster::renderSoftwarePreview(scene, camera, {.width = 128,
                                                   .height = 96,
                                                   .samples_per_axis = 1,
                                                   .settings = settings});
  assert(meshHasVisiblePixel(framebuffer));
  assert(colorBucketCount(framebuffer) >= 5u);
}

} // namespace

int main() {
  std::cout << "pipe_runtime_asset_tests: runtime asset model\n";
  testPipeRuntimeAssetModel();
  std::cout << "pipe_runtime_asset_tests: asset graph cook\n";
  testPipeAssetGraphCook();
  std::cout << "pipe_runtime_asset_tests: industrial pipe preview\n";
  testIndustrialPipePreviewRenders();
  std::cout << "pipe_runtime_asset_tests: passed\n";
  return 0;
}
