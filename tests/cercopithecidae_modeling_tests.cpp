// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/geometry/mesh_modeling.hpp"
#include "aster/geometry/primate_anatomy.hpp"
#include "aster/geometry/procedural_modeling.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/samples/showcase_scenes.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

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

bool meshHasVisiblePixel(const aster::SoftwareFrameBuffer &framebuffer) {
  for (std::size_t i = 0u; i + 3u < framebuffer.rgba8().size(); i += 4u) {
    if (framebuffer.rgba8()[i + 0u] > 8u || framebuffer.rgba8()[i + 1u] > 8u ||
        framebuffer.rgba8()[i + 2u] > 8u) {
      return true;
    }
  }
  return false;
}

struct FrameColorStats {
  std::size_t visible_pixels = 0u;
  std::size_t non_white_pixels = 0u;
  std::size_t color_buckets = 0u;
  int min_luma = std::numeric_limits<int>::max();
  int max_luma = 0;
  int max_chroma = 0;
};

FrameColorStats frameColorStats(const aster::SoftwareFrameBuffer &framebuffer) {
  FrameColorStats stats;
  std::set<int> buckets;
  for (std::size_t i = 0u; i + 3u < framebuffer.rgba8().size(); i += 4u) {
    const int r = framebuffer.rgba8()[i + 0u];
    const int g = framebuffer.rgba8()[i + 1u];
    const int b = framebuffer.rgba8()[i + 2u];
    if (r + g + b <= 24) {
      continue;
    }
    ++stats.visible_pixels;
    if (r < 245 || g < 245 || b < 245) {
      ++stats.non_white_pixels;
    }
    const int luma = (r * 2126 + g * 7152 + b * 722) / 10000;
    stats.min_luma = std::min(stats.min_luma, luma);
    stats.max_luma = std::max(stats.max_luma, luma);
    stats.max_chroma = std::max(stats.max_chroma, std::max({r, g, b}) - std::min({r, g, b}));
    buckets.insert((r / 32) * 64 + (g / 32) * 8 + (b / 32));
  }
  stats.color_buckets = buckets.size();
  if (stats.visible_pixels == 0u) {
    stats.min_luma = 0;
  }
  return stats;
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
  }
  const aster::MeshTopologyReport report = aster::validateMeshTopology(mesh);
  assert(report.indexable());
  assert(report.triangles > 0u);
  assert(report.invalid_indices == 0u);
  assert(report.degenerate_triangles == 0u);
  assert(report.bounds.valid);
}

bool hasLandmark(const aster::CercopithecidaeMorphologyReport &report,
                 const std::string &id) {
  return std::any_of(report.landmarks.begin(), report.landmarks.end(),
                     [&](const aster::AnatomicalLandmark &landmark) {
                       return landmark.id == id;
                     });
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

void testProceduralModelingLayer() {
  aster::CpuMesh ellipsoid = aster::makeEllipsoidSection({.center = {0.0f, 0.6f, 0.0f},
                                                          .radius = {0.4f, 0.6f, 0.3f},
                                                          .segments = 24,
                                                          .rings = 12});
  assertFiniteRenderableMesh(ellipsoid);

  aster::LatheSurfaceSpec lathe;
  lathe.profile = {{0.16f, -0.22f}, {0.30f, -0.10f}, {0.24f, 0.18f}, {0.08f, 0.28f}};
  lathe.radial_segments = 24;
  aster::CpuMesh lathed = aster::makeLathedSurface(lathe);
  assertFiniteRenderableMesh(lathed);

  aster::SweepPath path;
  path.points = {{.position = {-0.5f, 0.2f, -0.4f}},
                 {.position = {-0.15f, 0.42f, -0.12f}, .radius_scale = 0.82f},
                 {.position = {0.35f, 0.35f, 0.28f}, .radius_scale = 0.68f}};
  aster::CpuMesh swept =
      aster::makeSweptTube({.path = path,
                            .profile = aster::makeCircularSweepProfile(12, 1.0f, 0.75f),
                            .radius = 0.045f});
  assertFiniteRenderableMesh(swept);

  aster::CpuMesh ridge = aster::makeExtrudedRidge({.spine = {{-0.3f, 0.4f, 0.0f},
                                                            {0.0f, 0.48f, 0.05f},
                                                            {0.3f, 0.4f, 0.0f}},
                                                   .up = {0.0f, 0.0f, 1.0f},
                                                   .width = 0.05f,
                                                   .height = 0.035f});
  assertFiniteRenderableMesh(ridge);

  aster::CpuMesh ribbon = aster::makeRibbonStrip({.centerline = {{-0.25f, 0.12f, -0.10f},
                                                                 {0.00f, 0.20f, 0.08f},
                                                                 {0.28f, 0.15f, 0.20f}},
                                                  .half_widths = {0.04f, 0.055f, 0.035f},
                                                  .up = {0.0f, 1.0f, 0.0f},
                                                  .thickness = 0.006f});
  assertFiniteRenderableMesh(ribbon);

  aster::CpuMesh capsule = aster::makeCapsule({.start = {-0.10f, 0.2f, -0.25f},
                                               .end = {0.12f, 0.42f, 0.24f},
                                               .radius = 0.035f,
                                               .segments = 12,
                                               .rings = 6,
                                               .profile_vertical_scale = 0.62f});
  assertFiniteRenderableMesh(capsule);
  const aster::Vec3 before_detail = capsule.vertices.front().position;
  aster::applyDeterministicSurfaceDetail(capsule, {.amplitude = 0.0035f,
                                                   .frequency = 18.0f,
                                                   .ridge_strength = 0.45f,
                                                   .cavity_ao_strength = 0.08f,
                                                   .seed = 91u});
  assertFiniteRenderableMesh(capsule);
  assert(aster::distance(before_detail, capsule.vertices.front().position) > 0.00001f);

  aster::AsterMeshAssembly assembly;
  const std::uint32_t part = assembly.beginPart("procedural proof");
  aster::mergeMesh(assembly.mesh(), ellipsoid);
  assembly.finishPart(part);
  assembly.appendPart("lathed proof", lathed, {0.7f, 0.0f, 0.0f});
  assert(assembly.parts().size() == 2u);
  assert(assembly.parts()[0].vertex_count == ellipsoid.vertices.size());
  assert(assembly.parts()[1].index_count == lathed.indices.size());
  assertFiniteRenderableMesh(assembly.mesh());
}

void testCercopithecidaeAnatomyModel() {
  const aster::AnatomicalModel model = aster::makeCercopithecidaeModel({.surface_segments = 28,
                                                                        .surface_rings = 14,
                                                                        .fur_strand_guides = 88,
                                                                        .include_integument = true,
                                                                        .integument_epidermal_layers = 3,
                                                                        .integument_shell_offset = 0.020f,
                                                                        .pigment_heterogeneity = 0.68f,
                                                                        .vascular_translucency = 0.41f,
                                                                        .follicle_density = 1.18f,
                                                                        .gland_cluster_count = 34,
                                                                        .tension_line_strength = 1.12f});
  assert(model.parts.size() >= 10u);
  assert(model.vertexCount() > 10000u);
  assert(model.indexCount() > 30000u);

  std::set<aster::AnatomicalTissue> tissues;
  bool saw_cranium = false;
  bool saw_dentition = false;
  bool saw_manus = false;
  bool saw_pes = false;
  const aster::AnatomicalModelPart *fur_skin = nullptr;
  for (const aster::AnatomicalModelPart &part : model.parts) {
    tissues.insert(part.tissue);
    assertFiniteRenderableMesh(part.mesh);
    saw_cranium = saw_cranium || part.name.find("craniofacial") != std::string::npos;
    saw_dentition = saw_dentition || part.name.find("dentition") != std::string::npos;
    saw_manus = saw_manus || part.name.find("manus") != std::string::npos;
    saw_pes = saw_pes || part.name.find("pes") != std::string::npos;
    if (part.tissue == aster::AnatomicalTissue::FurSkin) {
      fur_skin = &part;
    }
  }
  assert(saw_cranium);
  assert(saw_dentition);
  assert(saw_manus);
  assert(saw_pes);
  assert(tissues.count(aster::AnatomicalTissue::Bone) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::Enamel) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::Muscle) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::Tendon) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::SoftTissue) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::PlantarPad) == 1u);
  assert(tissues.count(aster::AnatomicalTissue::FurSkin) == 1u);
  assert(fur_skin != nullptr);
  assert(fur_skin->name.find("fur skin") != std::string::npos);
  assert(fur_skin->mesh.vertices.size() > 2600u);

  const auto &cranio = model.report.craniofacial;
  assert(cranio.neurocranium_volume_proxy > 0.16f);
  assert(cranio.supraorbital_ridge_projection > 0.075f);
  assert(cranio.zygomatic_arch_span > 0.70f);
  assert(cranio.maxilla_prognathism > 0.25f);
  assert(cranio.mandibular_ramus_height > 0.18f);
  assert(cranio.molar_count == 8);
  assert(cranio.bilophodont_loph_pairs == 16);
  assert(cranio.cranial_suture_count >= 4);
  assert(cranio.dentition_cusp_count >= 32);

  const auto &post = model.report.postcranial;
  assert(post.cervical_vertebrae == 7);
  assert(post.thoracic_vertebrae == 12);
  assert(post.lumbar_vertebrae == 7);
  assert(post.rib_pairs == 12);
  assert(post.pronation_supination_range_degrees >= 140.0f);
  assert(post.opposable_pollex_angle_degrees >= 50.0f);
  assert(post.iliac_crest_width > 0.55f);
  assert(post.femoroacetabular_angle_degrees > 120.0f);
  assert(post.plantar_pad_thickness > 0.045f);
  assert(post.pes_phalanx_elongation > 1.15f);
  assert(post.tendon_band_count >= 20);
  assert(post.fur_strand_guides >= 70);

  const auto &integument = model.report.integument;
  assert(integument.epidermal_layer_count == 3);
  assert(integument.epidermal_thickness_proxy > 0.012f);
  assert(integument.dermal_elasticity > 0.68f);
  assert(integument.hypodermal_vascularity > 0.45f);
  assert(integument.pigment_heterogeneity > 0.65f);
  assert(integument.follicle_guide_count >= 70);
  assert(integument.gland_cluster_count == 34);
  assert(integument.capillary_translucency > 0.40f);
  assert(integument.micro_abrasion_density > 0.48f);
  assert(integument.dynamic_tension_line_count >= 6);
  assert(integument.low_pelage_zone_ratio > 0.20f);
  assert(integument.plantar_integument_thickening > 0.020f);

  assert(hasLandmark(model.report, "neurocranium.volume_proxy"));
  assert(hasLandmark(model.report, "supraorbital.ridge_projection"));
  assert(hasLandmark(model.report, "dentition.bilophodont_loph_pairs"));
  assert(hasLandmark(model.report, "zygomatic.arch_span"));
  assert(hasLandmark(model.report, "axial.rib_pairs"));
  assert(hasLandmark(model.report, "antebrachium.pronation_supination"));
  assert(hasLandmark(model.report, "manus.opposable_pollex_angle"));
  assert(hasLandmark(model.report, "pes.phalanx_elongation"));
  assert(hasLandmark(model.report, "skin.fur_strand_guides"));
  assert(hasLandmark(model.report, "integument.epidermal_layers"));
  assert(hasLandmark(model.report, "integument.dermal_elasticity"));
  assert(hasLandmark(model.report, "integument.hypodermal_vascularity"));
  assert(hasLandmark(model.report, "integument.pigment_heterogeneity"));
  assert(hasLandmark(model.report, "integument.gland_clusters"));
  assert(hasLandmark(model.report, "integument.capillary_translucency"));
  assert(hasLandmark(model.report, "integument.micro_abrasion_density"));
  assert(hasLandmark(model.report, "integument.dynamic_tension_lines"));
  assert(hasLandmark(model.report, "integument.low_pelage_zones"));
  assert(hasLandmark(model.report, "integument.plantar_thickening"));
  assertFiniteRenderableMesh(model.mergedMesh());
}

void testAssetGraphAndRuntimeMeshResolution() {
  const std::filesystem::path source_dir = ASTER_SOURCE_DIR;
  const std::filesystem::path graph = source_dir / "showcases" / "primate_lab" /
                                      "cercopithecidae.astergraph";
  const std::filesystem::path project = source_dir / "showcases" / "primate_lab" /
                                        "primate_lab.asterproj";
  assert(std::filesystem::exists(graph));
  assert(std::filesystem::exists(project));

  const std::filesystem::path assetc = ASTER_ASSETC_EXECUTABLE;
  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "aster_cercopithecidae_assetgraph";
  const std::filesystem::path cooked =
      std::filesystem::temp_directory_path() / "aster_cercopithecidae_cooked";
  std::filesystem::remove_all(output);
  std::filesystem::remove_all(cooked);

  const std::string inspect_command =
      shellQuote(assetc) + " graph-inspect --input " + shellQuote(graph) + " > " +
      shellQuote(output.parent_path() / "aster_cercopithecidae_graph_inspect.txt");
  requireCommand(inspect_command);

  const std::string package_command =
      shellQuote(assetc) + " graph-package --input " + shellQuote(graph) + " --output " +
      shellQuote(output);
  requireCommand(package_command);

  const std::filesystem::path assetgraphbin = findAssetGraphBin(output);
  if (assetgraphbin.empty()) {
    throw std::runtime_error("asset graph package did not emit an .assetgraphbin under " +
                             output.string());
  }
  const aster::ProceduralAssetGraphPackage package =
      aster::loadProceduralAssetGraphPackage(assetgraphbin);
  assert(package.mesh.primitive == "cercopithecidae");
  assert(package.quality.production_ready);
  assert(package.quality.score >= 80u);
  const aster::Material packaged_material = aster::proceduralAssetGraphMaterial(package);
  assert(aster::resolveMaterialSurfaceProfile(packaged_material) ==
         aster::MaterialSurfaceProfile::BiologicalIntegument);
  assert((package.feature_mask & (1ull << 30u)) != 0u);
  assert((package.feature_mask & (1ull << 34u)) != 0u);
  assert((package.feature_mask & (1ull << 38u)) != 0u);
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "bilophodont_tooth_row" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "measurement_probe" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "surface_displacement" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                     [](const aster::ProceduralAssetGraphNode &node) {
                       return node.kind == "anatomical_texture" &&
                              node.capability_status == "runtime-procedural-reference";
                     }));
  for (const char *kind : {"epidermal_strata",
                           "dermal_lattice",
                           "hypodermal_vascular_field",
                           "follicle_distribution",
                           "pigment_mask",
                           "gland_cluster",
                           "capillary_translucency",
                           "surface_tension_line",
                           "micro_abrasion"}) {
    assert(std::any_of(package.nodes.begin(), package.nodes.end(),
                       [&](const aster::ProceduralAssetGraphNode &node) {
                         return node.kind == kind &&
                                node.capability_status == "runtime-procedural-reference";
                       }));
  }

  const aster::CpuMesh runtime_mesh = aster::proceduralAssetGraphMesh(package);
  assertFiniteRenderableMesh(runtime_mesh);
  assert(runtime_mesh.vertices.size() > 10000u);

  const std::string cook_command = shellQuote(assetc) + " cook --project " + shellQuote(project) +
                                   " --platform desktop --output " + shellQuote(cooked);
  requireCommand(cook_command);
  assert(std::filesystem::exists(cooked / "assetdb.asterdb.json"));
  std::ifstream db(cooked / "assetdb.asterdb.json");
  const std::string db_text((std::istreambuf_iterator<char>(db)),
                            std::istreambuf_iterator<char>());
  assert(db_text.find("asset_graph.primate_lab.cercopithecidae") != std::string::npos);
  assert(db_text.find("assetgraphbin") != std::string::npos);

  std::filesystem::remove_all(output);
  std::filesystem::remove_all(cooked);
}

void testCercopithecidaePreviewSceneRenders() {
  const aster::Scene scene = aster::makeCercopithecidaeShowcaseScene();
  assert(scene.objects().size() >= 10u);
  bool saw_dentition = false;
  bool saw_pes = false;
  bool saw_skin_envelope = false;
  for (const aster::RenderObject &object : scene.objects()) {
    saw_dentition = saw_dentition || object.name.find("dentition") != std::string::npos;
    saw_pes = saw_pes || object.name.find("pes") != std::string::npos;
    if (object.name.find("integument") != std::string::npos ||
        object.name.find("fur") != std::string::npos) {
      saw_skin_envelope = true;
      assert(object.material.base_color.x < 0.90f);
      assert(object.material.base_color.y < 0.90f);
      assert(object.material.base_color.z < 0.90f);
    }
    if (object.custom_mesh != nullptr) {
      assertFiniteRenderableMesh(*object.custom_mesh);
    }
  }
  assert(saw_dentition);
  assert(saw_pes);
  assert(saw_skin_envelope);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.92f, -0.48f};
  camera.yaw = aster::radians(35.0f);
  camera.pitch = aster::radians(18.0f);
  camera.radius = 4.7f;
  camera.vertical_fov = aster::radians(38.0f);

  aster::RendererSettings settings;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.40f, 0.82f, 0.26f};
  settings.sun_light.intensity = 2.0f;
  settings.procedural_surface_normals = true;
  settings.ambient_strength = 0.26f;
  settings.pipeline.clear_color = {0.020f, 0.024f, 0.026f};

  const aster::SoftwareFrameBuffer framebuffer =
      aster::renderSoftwarePreview(scene, camera, {.width = 128,
                                                   .height = 96,
                                                   .samples_per_axis = 1,
                                                   .settings = settings});
  assert(meshHasVisiblePixel(framebuffer));
  const FrameColorStats stats = frameColorStats(framebuffer);
  assert(stats.visible_pixels > 250u);
  assert(stats.non_white_pixels > stats.visible_pixels / 2u);
  assert(stats.color_buckets >= 6u);
  assert(stats.max_luma - stats.min_luma > 24);
  assert(stats.max_chroma > 8);
}

} // namespace

int main() {
  std::cout << "cercopithecidae_modeling_tests: procedural modeling layer\n";
  testProceduralModelingLayer();
  std::cout << "cercopithecidae_modeling_tests: anatomy model\n";
  testCercopithecidaeAnatomyModel();
  std::cout << "cercopithecidae_modeling_tests: asset graph runtime mesh\n";
  testAssetGraphAndRuntimeMeshResolution();
  std::cout << "cercopithecidae_modeling_tests: preview scene\n";
  testCercopithecidaePreviewSceneRenders();
  std::cout << "cercopithecidae_modeling_tests: passed\n";
  return 0;
}
