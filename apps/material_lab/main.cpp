// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_compiler.hpp"
#include "aster/asset/asset_database.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/render/software_preview_renderer.hpp"
#include "aster/scene/scene.hpp"
#include "aster/texture/texture_debug.hpp"
#include "aster/texture/texture_importer.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::string argumentValue(const int argc, char **argv, const std::string_view name,
                          const std::string &fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

bool hasArgument(const int argc, char **argv, const std::string_view value) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == value) {
      return true;
    }
  }
  return false;
}

aster::MeshPrimitive previewPrimitive(const std::string &name) {
  if (name == "cube") {
    return aster::MeshPrimitive::Box;
  }
  if (name == "cave-wall") {
    return aster::MeshPrimitive::Plane;
  }
  if (name == "rock") {
    return aster::MeshPrimitive::Rock;
  }
  return aster::MeshPrimitive::Sphere;
}

aster::Scene previewScene(const aster::Material &material, const std::string &mesh_name) {
  aster::Scene scene;
  aster::RenderObject object;
  object.name = "Material Lab Preview";
  object.material = material;
  object.material_asset_id = material.asset_id;
  object.primitive = previewPrimitive(mesh_name);
  if (mesh_name == "cave-wall") {
    object.transform.scale = {1.8f, 1.0f, 1.8f};
  }
  scene.objects().push_back(object);
  return scene;
}

aster::OrbitCamera previewCamera(const std::string &mesh_name) {
  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = aster::radians(42.0f);
  camera.pitch = aster::radians(18.0f);
  camera.radius = mesh_name == "cave-wall" ? 3.3f : 4.0f;
  camera.vertical_fov = aster::radians(38.0f);
  return camera;
}

aster::RendererSettings previewSettings(const std::string &debug_mode) {
  aster::RendererSettings settings;
  settings.use_aces_tonemap = true;
  settings.procedural_surface_normals = debug_mode != "base-color";
  settings.ambient_strength = 0.26f;
  settings.ambient_floor = 0.02f;
  settings.exposure = 1.05f;
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.45f, 0.78f, 0.30f};
  settings.sun_light.color = {1.0f, 0.84f, 0.66f};
  settings.sun_light.intensity = 2.2f;
  settings.pipeline.clear_color = {0.028f, 0.032f, 0.038f};
  settings.atmosphere.enabled = debug_mode == "fog" || debug_mode == "beauty";
  settings.atmosphere.fog_color = {0.07f, 0.078f, 0.086f};
  settings.atmosphere.fog_start = 4.0f;
  settings.atmosphere.fog_end = 10.0f;
  settings.atmosphere.fog_strength = debug_mode == "fog" ? 0.44f : 0.08f;
  settings.light_rig = {
      aster::Light{{-2.4f, 2.8f, 2.2f}, {12.0f, 9.5f, 6.0f}, 1.0f, 0.6f},
      aster::Light{{2.6f, 1.4f, 1.8f}, {4.8f, 6.0f, 8.5f}, 1.0f, 0.8f},
      aster::Light{{0.0f, 2.5f, -2.5f}, {5.0f, 4.2f, 3.0f}, 1.0f, 1.0f},
      aster::Light{{0.0f, 0.8f, 2.5f}, {2.0f, 2.1f, 2.4f}, 1.0f, 1.2f},
  };
  return settings;
}

void printUsage() {
  std::cout << "usage: aster_material_lab --material <file.astermat> "
               "[--asset-db assetdb.asterdb.json] "
               "[--output preview.ppm] [--mesh sphere|cube|rock|cave-wall] "
               "[--debug beauty|base-color|normal|roughness|ao|fog]\n";
}

std::optional<aster::CookedMaterialAsset>
loadCookedMaterialForSource(const std::filesystem::path &material_path,
                            const std::filesystem::path &asset_db_path) {
  if (asset_db_path.empty() || !std::filesystem::exists(asset_db_path)) {
    return std::nullopt;
  }
  const aster::AssetDatabase database = aster::loadAssetDatabase(asset_db_path);
  const std::filesystem::path database_root = asset_db_path.parent_path();
  const std::string material_filename = material_path.filename().generic_string();
  const auto matches_source = [&](const aster::AssetDatabaseRecord &record) {
    return record.kind == "material" &&
           (record.source_path == material_path.generic_string() ||
            std::filesystem::path(record.source_path).filename().generic_string() ==
                material_filename);
  };
  const auto found =
      std::find_if(database.records.begin(), database.records.end(), matches_source);
  if (found == database.records.end()) {
    return std::nullopt;
  }
  const std::optional<std::filesystem::path> material_bin =
      aster::findAssetOutputPath(*found, database_root, "materialbin", "materialbin");
  if (!material_bin.has_value() || !std::filesystem::exists(*material_bin)) {
    return std::nullopt;
  }
  return aster::loadCookedMaterialAsset(*material_bin);
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (hasArgument(argc, argv, "--help") || hasArgument(argc, argv, "-h")) {
      printUsage();
      return 0;
    }
    const std::filesystem::path material_path = argumentValue(argc, argv, "--material", "");
    if (material_path.empty()) {
      printUsage();
      return 1;
    }
    const std::filesystem::path output =
        argumentValue(argc, argv, "--output", "material_lab_preview.ppm");
    std::filesystem::path asset_db_path = argumentValue(argc, argv, "--asset-db", "");
    if (asset_db_path.empty()) {
      asset_db_path = material_path.parent_path() / "cooked" / "desktop" / "assetdb.asterdb.json";
    }
    const std::string mesh = argumentValue(argc, argv, "--mesh", "sphere");
    const std::string debug_mode = argumentValue(argc, argv, "--debug", "beauty");
    const int width = std::stoi(argumentValue(argc, argv, "--width", "640"));
    const int height = std::stoi(argumentValue(argc, argv, "--height", "480"));

    const std::optional<aster::CookedMaterialAsset> cooked =
        loadCookedMaterialForSource(material_path, asset_db_path);
    aster::MaterialAssetLoadResult loaded;
    if (cooked.has_value()) {
      loaded.value = cooked->asset;
      loaded.diagnostics.push_back({.severity = aster::MaterialDiagnosticSeverity::Warning,
                                    .source_path = material_path,
                                    .message = "loaded cooked materialbin from asset database"});
      std::cout << "asset-db=" << asset_db_path << " materialbin=" << cooked->material_bin_path
                << '\n';
    } else {
      loaded = aster::loadMaterialAsset(material_path);
      std::cerr << "warning: no cooked asset database record found; using source .astermat "
                   "compatibility path\n";
    }
    for (const aster::MaterialDiagnostic &diagnostic : loaded.diagnostics) {
      std::cerr << (diagnostic.severity == aster::MaterialDiagnosticSeverity::Error ? "error" : "warning")
                << ": " << diagnostic.message << '\n';
    }
    if (!loaded.ok()) {
      return 1;
    }

    const aster::CompiledMaterialAsset compiled =
        aster::compileMaterialAssetForRendering(loaded.value);
    std::cout << "material=" << loaded.value.id << " variant=" << compiled.variant.tag
              << " bindings=" << compiled.binding_layout.bindings.size() << '\n';

    const aster::TextureSetValidation textures =
        aster::validateMaterialTextureSet(loaded.value, {}, {.require_existing_files = false});
    for (const aster::TextureAssetMetadata &texture : textures.textures) {
      std::cout << "texture " << aster::textureDebugSummary(texture) << '\n';
    }

    const aster::Scene scene = previewScene(compiled.fallback_material.material, mesh);
    const aster::SoftwareFrameBuffer framebuffer =
        aster::renderSoftwarePreview(scene, previewCamera(mesh),
                                     {.width = std::max(width, 1),
                                      .height = std::max(height, 1),
                                      .samples_per_axis = 1,
                                      .frame_seconds = 0.0,
                                      .settings = previewSettings(debug_mode)});
    framebuffer.writePpm(output, width, height);
    std::cout << "wrote " << output << '\n';
  } catch (const std::exception &error) {
    std::cerr << "aster_material_lab: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
