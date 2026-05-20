// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/editor_ui.hpp"

#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/platform/window.hpp"
#include "aster/physics/xpbd_authoring.hpp"
#include "aster/scene/scene.hpp"
#include "aster/texture/texture_importer.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr aster::UiColor kPanel{0.026f, 0.032f, 0.038f, 0.88f};
constexpr aster::UiColor kPanelEdge{0.72f, 0.52f, 0.26f, 0.34f};
constexpr aster::UiColor kText{0.91f, 0.93f, 0.89f, 1.0f};
constexpr aster::UiColor kDim{0.55f, 0.63f, 0.63f, 1.0f};
constexpr aster::UiColor kAmber{0.88f, 0.58f, 0.23f, 1.0f};
constexpr float kPanelMargin = 16.0f;
constexpr float kPanelPad = 18.0f;

bool pointInRect(const aster::Vec2 point, const aster::UiRect rect) {
  return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
         point.y <= rect.y + rect.height;
}

void drawPanelTexture(aster::UiCanvas &canvas, const aster::UiRect rect) {
  canvas.fillRoundRect(rect, 8.0f, kPanel);
  canvas.strokeRect(rect, kPanelEdge, 1.0f);
  for (int i = 0; i < 7; ++i) {
    const float y = rect.y + 18.0f + static_cast<float>(i) * 44.0f;
    canvas.line({rect.x + 14.0f, y}, {rect.x + rect.width - 16.0f, y + 14.0f},
                {0.88f, 0.65f, 0.34f, 0.035f}, 1.0f);
  }
}

void section(aster::UiCanvas &canvas, const std::string &title, const float x, float &y,
             const float width) {
  canvas.text(title, {x, y}, kAmber, 1.8f);
  y += 18.0f;
  canvas.line({x, y}, {x + width, y}, {0.82f, 0.60f, 0.32f, 0.26f}, 1.0f);
  y += 18.0f;
}

void sliderRow(aster::UiCanvas &canvas, const std::string &label, float &value,
               const float min_value, const float max_value, const float x, float &y,
               const float width, const std::string &id, const float visible_top,
               const float visible_bottom) {
  const float row_height = canvas.sliderHeight(label, width);
  if (y >= visible_top && y + row_height <= visible_bottom) {
    canvas.slider({x, y + 17.0f, width, 8.0f}, label, value, min_value, max_value, id);
  }
  y += row_height + 3.0f;
}

void checkboxRow(aster::UiCanvas &canvas, const std::string &label, bool &value, const float x,
                 float &y, const float width, const std::string &id, const float visible_top,
                 const float visible_bottom) {
  const float row_height = canvas.checkboxHeight(label, width);
  if (y >= visible_top && y + row_height <= visible_bottom) {
    canvas.checkbox({x, y, width, row_height}, label, value, id);
  }
  y += row_height + 3.0f;
}

void renderStyleRow(aster::UiCanvas &canvas, aster::RendererSettings &settings, const float x,
                    float &y, const float width, const float visible_top,
                    const float visible_bottom) {
  constexpr float kRowHeight = 34.0f;
  const float button_width = std::max((width - 8.0f) * 0.5f, 84.0f);
  if (y >= visible_top && y + kRowHeight <= visible_bottom) {
    const bool neutral = settings.style.preset == aster::RenderStylePreset::Neutral;
    const bool retro = settings.style.preset == aster::RenderStylePreset::RetroHorrorReadable;
    if (canvas.button({x, y, button_width, 30.0f}, neutral ? "Neutral*" : "Neutral",
                      "style.neutral") &&
        !neutral) {
      aster::applyRenderStyleProfile(
          settings, aster::makeRenderStyleProfile(aster::RenderStylePreset::Neutral));
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f},
                      retro ? "Retro Horror*" : "Retro Horror", "style.retro") &&
        !retro) {
      aster::applyRenderStyleProfile(
          settings,
          aster::makeRenderStyleProfile(aster::RenderStylePreset::RetroHorrorReadable));
    }
  }
  y += kRowHeight + 3.0f;
}

void textRow(aster::UiCanvas &canvas, const std::string_view label, const std::string_view value,
             const float x, float &y, const float width, const float visible_top,
             const float visible_bottom) {
  constexpr float kRowHeight = 20.0f;
  if (y >= visible_top && y + kRowHeight <= visible_bottom) {
    canvas.text(label, {x, y}, kDim, 1.22f);
    const float value_scale =
        canvas.fittedTextScale(value, std::max(width * 0.52f, 0.0f), 1.18f, 0.82f);
    const float value_width = canvas.textWidth(value, value_scale);
    canvas.text(value, {x + width - value_width, y}, kText, value_scale);
  }
  y += kRowHeight;
}

std::string yesNo(const bool value) {
  return value ? "yes" : "no";
}

void drawBackendSummary(aster::UiCanvas &canvas, const aster::EditorRuntimeModel &runtime,
                        const float x, float &y, const float width, const float visible_top,
                        const float visible_bottom) {
  section(canvas, "Backend", x, y, width);
  textRow(canvas, "Name", runtime.backend.name, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Kind", aster::renderBackendKindName(runtime.backend.kind), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "GPU", yesNo(runtime.backend.gpu), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Shader materials", yesNo(runtime.backend.supports_shader_materials), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Instancing", yesNo(runtime.backend.supports_instancing), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Capture", yesNo(runtime.backend.supports_capture), x, y, width, visible_top,
          visible_bottom);
}

std::string clippedValue(const std::string &value, std::size_t max_size = 44u);

void drawGraphSummary(aster::UiCanvas &canvas, std::size_t &selected_pass,
                      std::size_t &selected_provenance,
                      const aster::FixedRenderGraph *graph,
                      const aster::FrameForensics *forensics, const float x, float &y,
                      const float width, const float visible_top, const float visible_bottom) {
  section(canvas, "Resource Graph", x, y, width);
  if (graph == nullptr || graph->passes.empty()) {
    textRow(canvas, "Passes", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  if (selected_pass >= graph->passes.size()) {
    selected_pass = graph->passes.size() - 1u;
  }
  const std::string pass_count = std::to_string(graph->passes.size());
  textRow(canvas, "Passes", pass_count, x, y, width, visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "graph.prev") &&
        selected_pass > 0u) {
      --selected_pass;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "graph.next") &&
        selected_pass + 1u < graph->passes.size()) {
      ++selected_pass;
    }
  }
  y += 38.0f;
  const aster::framegraph::CompiledPass &pass = graph->passes[selected_pass];
  const std::string pass_label =
      std::to_string(selected_pass + 1u) + " " + pass.name;
  textRow(canvas, "Selected", pass_label, x, y, width, visible_top, visible_bottom);
  char buffer[64]{};
  std::snprintf(buffer, sizeof(buffer), "0x%08x", pass.read_mask);
  textRow(canvas, "Reads", buffer, x, y, width, visible_top, visible_bottom);
  std::snprintf(buffer, sizeof(buffer), "0x%08x", pass.write_mask);
  textRow(canvas, "Writes", buffer, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Resources", std::to_string(graph->resources.size()), x, y, width, visible_top,
          visible_bottom);
  const std::size_t provenance_count =
      forensics == nullptr ? 0u : forensics->resource_provenance.size();
  textRow(canvas, "Provenance", std::to_string(provenance_count), x, y, width, visible_top,
          visible_bottom);
  if (forensics == nullptr || forensics->resource_provenance.empty()) {
    return;
  }
  selected_provenance = std::min(selected_provenance, forensics->resource_provenance.size() - 1u);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "provenance.prev") &&
        selected_provenance > 0u) {
      --selected_provenance;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "provenance.next") &&
        selected_provenance + 1u < forensics->resource_provenance.size()) {
      ++selected_provenance;
    }
  }
  y += 38.0f;
  const aster::FrameResourceProvenance &provenance =
      forensics->resource_provenance[selected_provenance];
  textRow(canvas, "Kind", aster::frameResourceProvenanceKindName(provenance.kind), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Resource", clippedValue(provenance.resource_name), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Producer", clippedValue(provenance.producer_node), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Material", clippedValue(provenance.material_asset_id), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Graph node", clippedValue(provenance.material_graph_node), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Cook", clippedValue(provenance.cook_report), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Fallback", clippedValue(provenance.backend_fallback), x, y, width,
          visible_top, visible_bottom);
}

std::string clippedValue(const std::string &value, const std::size_t max_size) {
  if (value.size() <= max_size) {
    return value;
  }
  return value.substr(0u, max_size - 3u) + "...";
}

std::string firstOrNone(const std::vector<std::string> &values) {
  return values.empty() ? std::string("none") : clippedValue(values.front());
}

void drawAssetPipelinePanel(aster::UiCanvas &canvas, std::size_t &selected_asset,
                           const aster::EditorRuntimeModel &runtime, const float x, float &y,
                           const float width, const float visible_top,
                           const float visible_bottom) {
  section(canvas, "Asset Pipeline", x, y, width);
  const aster::AssetDatabase *database = runtime.asset_database;
  const aster::AssetLibrary *library = runtime.asset_library;
  if (database == nullptr) {
    textRow(canvas, "Asset DB", "not loaded", x, y, width, visible_top, visible_bottom);
    return;
  }
  textRow(canvas, "Schema", std::to_string(database->schema_version), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Assets", std::to_string(database->records.size()), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Graph", std::to_string(database->asset_graph.nodes.size()) + " nodes / " +
                              std::to_string(database->asset_graph.edges.size()) + " edges",
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Fate reports", std::to_string(database->fate_reports.size()), x, y, width,
          visible_top, visible_bottom);
  if (library == nullptr || library->assets.empty()) {
    return;
  }
  selected_asset = std::min(selected_asset, library->assets.size() - 1u);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "asset.prev") &&
        selected_asset > 0u) {
      --selected_asset;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "asset.next") &&
        selected_asset + 1u < library->assets.size()) {
      ++selected_asset;
    }
  }
  y += 38.0f;
  const aster::AssetRepresentation &asset = library->assets[selected_asset];
  textRow(canvas, "Selected", clippedValue(asset.name), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Kind", asset.kind, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Ready", yesNo(asset.production_ready), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Source", clippedValue(asset.source_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Material hash", clippedValue(asset.derived_hashes.material_hash), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Shader key", clippedValue(asset.derived_hashes.shader_variant_key), x, y, width,
          visible_top, visible_bottom);
  if (!asset.diagnostics.empty()) {
    textRow(canvas, "Diagnostic", clippedValue(asset.diagnostics.front()), x, y, width,
            visible_top, visible_bottom);
  }
}

std::string byteCost(const std::uint64_t bytes) {
  char buffer[64]{};
  if (bytes >= 1024ull * 1024ull) {
    std::snprintf(buffer, sizeof(buffer), "%.2f MiB",
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024ull) {
    std::snprintf(buffer, sizeof(buffer), "%.1f KiB", static_cast<double>(bytes) / 1024.0);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%llu B",
                  static_cast<unsigned long long>(bytes));
  }
  return buffer;
}

std::string hexU64(const std::uint64_t value) {
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "0x%016llx",
                static_cast<unsigned long long>(value));
  return buffer;
}

std::string dimensions(const aster::AssetProductionTextureAudit &texture) {
  if (texture.width == 0u || texture.height == 0u) {
    return "unknown";
  }
  return std::to_string(texture.width) + "x" + std::to_string(texture.height) + " mips " +
         std::to_string(texture.mip_count);
}

std::string textureSummary(const aster::AssetProductionTextureAudit &texture) {
  std::string summary = texture.present ? "present" : "missing";
  if (!texture.color_space.empty()) {
    summary += " " + texture.color_space;
  }
  if (!texture.runtime_format.empty()) {
    summary += " " + texture.runtime_format;
  }
  if (texture.width > 0u && texture.height > 0u) {
    summary += " " + dimensions(texture);
  }
  if (texture.byte_cost > 0u) {
    summary += " " + byteCost(texture.byte_cost);
  }
  return summary;
}

std::string diagnosticSummary(const aster::AssetCookDiagnostic &diagnostic) {
  std::string summary = diagnostic.severity.empty() ? diagnostic.message
                                                    : diagnostic.severity + ": " +
                                                          diagnostic.message;
  if (!diagnostic.source_path.empty()) {
    summary += " (" + diagnostic.source_path;
    if (diagnostic.line > 0u) {
      summary += ":" + std::to_string(diagnostic.line);
    }
    summary += ")";
  }
  return summary;
}

void listRows(aster::UiCanvas &canvas, const std::string_view label,
              const std::vector<std::string> &values, const std::size_t max_rows, const float x,
              float &y, const float width, const float visible_top, const float visible_bottom) {
  if (values.empty()) {
    textRow(canvas, label, "none", x, y, width, visible_top, visible_bottom);
    return;
  }
  for (std::size_t i = 0u; i < values.size() && i < max_rows; ++i) {
    textRow(canvas, i == 0u ? label : "", clippedValue(values[i]), x, y, width, visible_top,
            visible_bottom);
  }
  if (values.size() > max_rows) {
    textRow(canvas, "", "+" + std::to_string(values.size() - max_rows) + " more", x, y, width,
            visible_top, visible_bottom);
  }
}

void drawPreview(aster::UiCanvas &canvas, const aster::AssetPreviewImage &preview, const float x,
                 float &y, const float width, const float visible_top,
                 const float visible_bottom) {
  const float size = std::min(width, 132.0f);
  const aster::UiRect rect{x, y, size, size};
  if (y + size >= visible_top && y <= visible_bottom) {
    canvas.fillRoundRect(rect, 6.0f, {0.055f, 0.075f, 0.078f, 0.96f});
    if (preview.available) {
      canvas.image({rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f},
                   preview.width, preview.height, preview.rgba8);
      canvas.strokeRect(rect, {0.86f, 0.64f, 0.32f, 0.48f}, 1.0f);
    } else {
      canvas.strokeRect(rect, {0.72f, 0.37f, 0.25f, 0.42f}, 1.0f);
      canvas.text("No preview", {rect.x + 12.0f, rect.y + rect.height * 0.46f}, kDim, 1.15f);
    }
  }
  y += size + 12.0f;
}

std::filesystem::path projectRootForDatabaseRoot(const std::filesystem::path &database_root) {
  if (database_root.filename() == "desktop" &&
      database_root.parent_path().filename() == "cooked") {
    return database_root.parent_path().parent_path();
  }
  return database_root;
}

std::filesystem::path resolveProjectPath(const aster::AssetProductionModel &model,
                                         const std::filesystem::path &path) {
  if (path.is_absolute()) {
    return path;
  }
  const std::filesystem::path project_root = projectRootForDatabaseRoot(model.root_path);
  const std::filesystem::path project_path = project_root / path;
  std::error_code error;
  if (std::filesystem::exists(project_path, error)) {
    return project_path;
  }
  return model.root_path / path;
}

std::optional<std::filesystem::path> outputPathForRole(const aster::AssetProductionAsset &asset,
                                                       const aster::AssetProductionModel &model,
                                                       const std::string_view role,
                                                       const std::string_view kind = {}) {
  for (const aster::AssetCookedOutput &output : asset.cook.outputs) {
    if (output.role != role || (!kind.empty() && output.kind != kind)) {
      continue;
    }
    return output.path.empty() ? std::optional<std::filesystem::path>{}
                               : std::optional<std::filesystem::path>{model.root_path /
                                                                       output.path};
  }
  return std::nullopt;
}

void loadMaterialLabSelection(const aster::AssetProductionAsset &asset,
                              const aster::AssetProductionModel &model,
                              aster::MaterialAsset &material,
                              aster::MaterialAuthoringGraph &graph,
                              std::vector<std::string> &diagnostics,
                              std::filesystem::path &save_path, bool &save_supported,
                              bool &dirty) {
  diagnostics.clear();
  save_path.clear();
  save_supported = false;
  dirty = false;
  material = {};
  graph = {};
  if (asset.kind == "material") {
    const std::filesystem::path source_path = resolveProjectPath(model, asset.source_path);
    if (source_path.extension() == ".astermat" && std::filesystem::exists(source_path)) {
      const aster::MaterialAssetLoadResult loaded = aster::loadMaterialAsset(source_path);
      material = loaded.value;
      graph = aster::materialAuthoringGraphForAsset(material);
      for (const aster::MaterialDiagnostic &diagnostic : loaded.diagnostics) {
        diagnostics.push_back((diagnostic.severity == aster::MaterialDiagnosticSeverity::Error
                                   ? "error: "
                                   : "warning: ") +
                              diagnostic.message);
      }
      save_path = source_path;
      save_supported = loaded.ok();
      return;
    }
    if (!asset.material.material_bin_path.empty() &&
        std::filesystem::exists(asset.material.material_bin_path)) {
      const aster::CookedMaterialAsset cooked =
          aster::loadCookedMaterialAsset(asset.material.material_bin_path);
      material = cooked.asset;
      graph = aster::materialAuthoringGraphForAsset(material);
      diagnostics.push_back("warning: loaded cooked materialbin; source save disabled");
      return;
    }
    diagnostics.push_back("error: material source or materialbin is not available");
    return;
  }
  if (asset.kind == "asset_graph") {
    const std::optional<std::filesystem::path> graph_package =
        outputPathForRole(asset, model, "assetgraphbin", "assetgraphbin");
    if (!graph_package.has_value() || !std::filesystem::exists(*graph_package)) {
      diagnostics.push_back("error: asset graph package output is not available");
      return;
    }
    const aster::ProceduralAssetGraphPackage package =
        aster::loadProceduralAssetGraphPackage(*graph_package);
    material = package.material;
    graph = aster::materialAuthoringGraphForPackage(package);
    diagnostics.push_back("warning: asset graph edits are preview-only in V1");
    for (const aster::MaterialDiagnostic &diagnostic : package.diagnostics) {
      diagnostics.push_back((diagnostic.severity == aster::MaterialDiagnosticSeverity::Error
                                 ? "error: "
                                 : "warning: ") +
                            diagnostic.message);
    }
    return;
  }
  diagnostics.push_back("warning: Material Lab supports material and asset_graph assets");
}

std::string materialLabCacheKey(const aster::MaterialAsset &asset, const std::size_t mesh,
                                const std::size_t environment) {
  return std::to_string(mesh) + ":" + std::to_string(environment) + ":" +
         aster::serializeMaterialAsset(asset);
}

aster::MaterialLabMeshTarget materialLabMeshTargetAt(const std::size_t index) {
  constexpr std::array<aster::MaterialLabMeshTarget, 3> values{
      aster::MaterialLabMeshTarget::Sphere, aster::MaterialLabMeshTarget::Rock,
      aster::MaterialLabMeshTarget::CaveWall};
  return values[std::min(index, values.size() - 1u)];
}

aster::MaterialLabEnvironmentRig materialLabEnvironmentAt(const std::size_t index) {
  constexpr std::array<aster::MaterialLabEnvironmentRig, 4> values{
      aster::MaterialLabEnvironmentRig::StudioNeutral, aster::MaterialLabEnvironmentRig::CaveDark,
      aster::MaterialLabEnvironmentRig::ProbeLit, aster::MaterialLabEnvironmentRig::Fog};
  return values[std::min(index, values.size() - 1u)];
}

void refreshMaterialLabPreviews(const aster::MaterialAsset &material, const std::size_t mesh,
                                const std::size_t environment, std::string &cache_key,
                                std::vector<aster::MaterialLabPreviewImage> &previews) {
  const std::string next_key = materialLabCacheKey(material, mesh, environment);
  if (cache_key == next_key && previews.size() == 6u) {
    return;
  }
  cache_key = next_key;
  previews.clear();
  constexpr std::array<aster::MaterialLabPreviewMode, 6> modes{
      aster::MaterialLabPreviewMode::Beauty, aster::MaterialLabPreviewMode::BaseColor,
      aster::MaterialLabPreviewMode::Normal, aster::MaterialLabPreviewMode::Roughness,
      aster::MaterialLabPreviewMode::AmbientOcclusion, aster::MaterialLabPreviewMode::Fog};
  previews.reserve(modes.size());
  for (const aster::MaterialLabPreviewMode mode : modes) {
    previews.push_back(aster::renderMaterialLabPreview(
        material, {.mode = mode,
                   .mesh = materialLabMeshTargetAt(mesh),
                   .environment = materialLabEnvironmentAt(environment),
                   .width = 128,
                   .height = 84}));
  }
}

void drawSegmentedButtons(aster::UiCanvas &canvas, const std::vector<std::string> &labels,
                          std::size_t &selected, const float x, float &y, const float width,
                          const std::string &id_prefix, const float visible_top,
                          const float visible_bottom) {
  if (labels.empty()) {
    return;
  }
  const float gap = 6.0f;
  const float button_width =
      std::max(48.0f, (width - gap * static_cast<float>(labels.size() - 1u)) /
                           static_cast<float>(labels.size()));
  if (y >= visible_top && y + 30.0f <= visible_bottom) {
    for (std::size_t i = 0u; i < labels.size(); ++i) {
      const std::string label = labels[i] + (selected == i ? "*" : "");
      if (canvas.button({x + static_cast<float>(i) * (button_width + gap), y, button_width,
                         28.0f},
                        label, id_prefix + "." + std::to_string(i))) {
        selected = i;
      }
    }
  }
  y += 34.0f;
}

void drawAssetTabs(aster::UiCanvas &canvas, std::size_t &selected_tab, const float x, float &y,
                   const float width, const float visible_top, const float visible_bottom) {
  constexpr std::array<std::string_view, 7> tabs{"Catalog", "Material", "Texture", "Mesh",
                                                 "Cook", "Lab", "XPBD"};
  const std::size_t columns = width < 330.0f ? 3u : (width < 430.0f ? 4u : tabs.size());
  const float gap = 6.0f;
  const float button_width =
      std::max(52.0f, (width - gap * static_cast<float>(columns - 1u)) /
                           static_cast<float>(columns));
  const std::size_t rows = (tabs.size() + columns - 1u) / columns;
  for (std::size_t row = 0u; row < rows; ++row) {
    if (y >= visible_top && y + 30.0f <= visible_bottom) {
      for (std::size_t column = 0u; column < columns; ++column) {
        const std::size_t index = row * columns + column;
        if (index >= tabs.size()) {
          continue;
        }
        const std::string label =
            std::string(tabs[index]) + (selected_tab == index ? "*" : "");
        if (canvas.button({x + static_cast<float>(column) * (button_width + gap), y,
                           button_width, 28.0f},
                          label, "asset.tab." + std::to_string(index))) {
          selected_tab = index;
        }
      }
    }
    y += 34.0f;
  }
}

void drawCatalogTab(aster::UiCanvas &canvas, const aster::AssetProductionModel &model,
                    const aster::AssetProductionAsset &asset, const float x, float &y,
                    const float width, const float visible_top, const float visible_bottom) {
  section(canvas, "Catalog", x, y, width);
  textRow(canvas, "Assets", std::to_string(model.assets.size()), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Selected", clippedValue(asset.name.empty() ? asset.id : asset.name), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Kind", asset.kind, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Ready", yesNo(asset.production_ready), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Errors", std::to_string(asset.cook.error_count), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Warnings", std::to_string(asset.cook.warning_count), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Source", clippedValue(asset.source_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "GUID", clippedValue(asset.guid), x, y, width, visible_top, visible_bottom);
  drawPreview(canvas, asset.preview, x, y, width, visible_top, visible_bottom);
  listRows(canvas, "Inspector", asset.model_diagnostics, 4u, x, y, width, visible_top,
           visible_bottom);
}

void drawMaterialTab(aster::UiCanvas &canvas, const aster::AssetProductionAsset &asset,
                     const float x, float &y, const float width, const float visible_top,
                     const float visible_bottom) {
  section(canvas, "Material", x, y, width);
  if (!asset.material.attempted) {
    textRow(canvas, "Material", "not applicable", x, y, width, visible_top, visible_bottom);
    return;
  }
  textRow(canvas, "Loaded", yesNo(asset.material.loaded), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Package", clippedValue(asset.material.material_bin_path.filename().string()),
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "ID", clippedValue(asset.material.id), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Variant", clippedValue(asset.material.shader_variant_tag), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Pipeline", clippedValue(asset.material.pipeline_tag), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Feature mask", hexU64(asset.material.feature_mask), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Shader key", hexU64(asset.material.shader_variant_key), x, y, width,
          visible_top, visible_bottom);
  for (const std::string &role : asset.material.required_roles) {
    const aster::AssetProductionTextureAudit *texture = asset.findTexture(role);
    textRow(canvas, role, texture == nullptr ? "missing" : textureSummary(*texture), x, y, width,
            visible_top, visible_bottom);
  }
  listRows(canvas, "Diagnostic", asset.material.diagnostics, 5u, x, y, width, visible_top,
           visible_bottom);
}

void drawTextureTab(aster::UiCanvas &canvas, std::size_t &selected_texture,
                    const aster::AssetProductionAsset &asset, const float x, float &y,
                    const float width, const float visible_top, const float visible_bottom) {
  section(canvas, "Texture", x, y, width);
  if (asset.textures.empty()) {
    textRow(canvas, "Textures", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_texture = std::min(selected_texture, asset.textures.size() - 1u);
  textRow(canvas, "Textures", std::to_string(asset.textures.size()), x, y, width, visible_top,
          visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "texture.prev") &&
        selected_texture > 0u) {
      --selected_texture;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "texture.next") &&
        selected_texture + 1u < asset.textures.size()) {
      ++selected_texture;
    }
  }
  y += 38.0f;
  const aster::AssetProductionTextureAudit &texture = asset.textures[selected_texture];
  textRow(canvas, "Role", texture.role, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Present", yesNo(texture.present), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Kind", texture.kind, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Color", texture.color_space, x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Source fmt", texture.source_format, x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Runtime fmt", texture.runtime_format, x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Dimensions", dimensions(texture), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Byte cost", byteCost(texture.byte_cost), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Source hash", clippedValue(texture.source_hash), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Cooked hash", clippedValue(texture.cooked_hash), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Runtime path", clippedValue(texture.cooked_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Fallback",
          texture.fallback_reason.empty() ? "none" : clippedValue(texture.fallback_reason), x, y,
          width, visible_top, visible_bottom);
  listRows(canvas, "Diagnostic", texture.diagnostics, 5u, x, y, width, visible_top,
           visible_bottom);
}

void drawMeshTab(aster::UiCanvas &canvas, const aster::AssetProductionAsset &asset, const float x,
                 float &y, const float width, const float visible_top,
                 const float visible_bottom) {
  section(canvas, "Mesh", x, y, width);
  if (!asset.mesh.attempted) {
    textRow(canvas, "Mesh", "not applicable", x, y, width, visible_top, visible_bottom);
    return;
  }
  textRow(canvas, "Loaded", yesNo(asset.mesh.loaded), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Cache", clippedValue(asset.mesh.cache_path.filename().string()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Materials", std::to_string(asset.mesh.material_count), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Meshes", std::to_string(asset.mesh.mesh_count), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Collision", std::to_string(asset.mesh.collision_mesh_count), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Scene nodes", std::to_string(asset.mesh.scene_node_count), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Vertices", std::to_string(asset.mesh.total_vertices), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Indices", std::to_string(asset.mesh.total_indices), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Collision tris", std::to_string(asset.mesh.total_collision_triangles), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Invalid normals", std::to_string(asset.mesh.diagnostics.invalid_normals), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Tangents gen", std::to_string(asset.mesh.diagnostics.generated_tangents), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Degenerate tris",
          std::to_string(asset.mesh.diagnostics.degenerate_triangles), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Remapped verts", std::to_string(asset.mesh.diagnostics.remapped_vertices), x,
          y, width, visible_top, visible_bottom);
  listRows(canvas, "Diagnostic", asset.mesh.messages, 4u, x, y, width, visible_top,
           visible_bottom);
}

void drawCookTab(aster::UiCanvas &canvas, const aster::AssetProductionAsset &asset, const float x,
                 float &y, const float width, const float visible_top,
                 const float visible_bottom) {
  section(canvas, "Cook", x, y, width);
  textRow(canvas, "Ready", yesNo(asset.production_ready), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Errors", std::to_string(asset.cook.error_count), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Warnings", std::to_string(asset.cook.warning_count), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Preset", clippedValue(asset.cook.import_preset.name), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Platform", clippedValue(asset.cook.platform_profile.name), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Runtime fmt", clippedValue(asset.cook.platform_profile.runtime_texture_format),
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Source hash", clippedValue(asset.cook.hashes.source_hash), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Options hash", clippedValue(asset.cook.hashes.options_hash), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Dependency hash", clippedValue(asset.cook.hashes.dependency_hash), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Artifact hash", clippedValue(asset.cook.hashes.artifact_hash), x, y, width,
          visible_top, visible_bottom);

  std::vector<std::string> outputs;
  outputs.reserve(asset.cook.outputs.size());
  for (const aster::AssetCookedOutput &output : asset.cook.outputs) {
    outputs.push_back(output.role + " -> " + output.path);
  }
  listRows(canvas, "Output", outputs, 6u, x, y, width, visible_top, visible_bottom);

  std::vector<std::string> edges;
  edges.reserve(asset.cook.dependency_edges.size());
  for (const aster::AssetDependencyEdge &edge : asset.cook.dependency_edges) {
    edges.push_back(edge.role + " -> " + edge.to + " " + (edge.present ? "present" : "missing"));
  }
  listRows(canvas, "Dependency", edges, 6u, x, y, width, visible_top, visible_bottom);

  std::vector<std::string> diagnostics;
  diagnostics.reserve(asset.cook.diagnostics.size() + asset.model_diagnostics.size());
  for (const aster::AssetCookDiagnostic &diagnostic : asset.cook.diagnostics) {
    diagnostics.push_back(diagnosticSummary(diagnostic));
  }
  diagnostics.insert(diagnostics.end(), asset.model_diagnostics.begin(),
                     asset.model_diagnostics.end());
  listRows(canvas, "Diagnostic", diagnostics, 7u, x, y, width, visible_top, visible_bottom);
}

void drawMaterialLabGraph(aster::UiCanvas &canvas, const aster::MaterialAuthoringGraph &graph,
                          std::size_t &selected_node, const float x, float &y,
                          const float width, const float visible_top,
                          const float visible_bottom) {
  section(canvas, "Node Graph", x, y, width);
  textRow(canvas, "Source", graph.source_kind.empty() ? "none" : graph.source_kind, x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Nodes", std::to_string(graph.nodes.size()), x, y, width, visible_top,
          visible_bottom);
  if (graph.nodes.empty()) {
    return;
  }
  selected_node = std::min(selected_node, graph.nodes.size() - 1u);
  const float graph_height = std::min(210.0f, std::max(132.0f, width * 0.52f));
  const aster::UiRect graph_rect{x, y, width, graph_height};
  if (y + graph_height >= visible_top && y <= visible_bottom) {
    canvas.fillRoundRect(graph_rect, 6.0f, {0.035f, 0.047f, 0.050f, 0.96f});
    canvas.strokeRect(graph_rect, {0.80f, 0.60f, 0.32f, 0.34f}, 1.0f);
    std::vector<std::pair<std::string, aster::Vec2>> centers;
    const std::size_t visible_nodes = std::min<std::size_t>(graph.nodes.size(), 12u);
    const float node_width = (width - 44.0f) * 0.5f;
    const float node_height = 24.0f;
    for (std::size_t i = 0u; i < visible_nodes; ++i) {
      const std::size_t row = i / 2u;
      const std::size_t column = i % 2u;
      const float nx = graph_rect.x + 14.0f + static_cast<float>(column) * (node_width + 16.0f);
      const float ny = graph_rect.y + 14.0f + static_cast<float>(row) * 31.0f;
      centers.push_back({graph.nodes[i].id, {nx + node_width * 0.5f, ny + node_height * 0.5f}});
    }
    const auto center_for = [&](const std::string &id) -> std::optional<aster::Vec2> {
      const auto found = std::find_if(centers.begin(), centers.end(),
                                      [&](const auto &entry) { return entry.first == id; });
      return found == centers.end() ? std::optional<aster::Vec2>{}
                                    : std::optional<aster::Vec2>{found->second};
    };
    for (const aster::MaterialAuthoringEdge &edge : graph.edges) {
      const std::optional<aster::Vec2> from = center_for(edge.from);
      const std::optional<aster::Vec2> to = center_for(edge.to);
      if (from.has_value() && to.has_value()) {
        canvas.line(*from, *to, {0.86f, 0.62f, 0.28f, 0.42f}, 1.0f);
      }
    }
    for (std::size_t i = 0u; i < visible_nodes; ++i) {
      const std::size_t row = i / 2u;
      const std::size_t column = i % 2u;
      const float nx = graph_rect.x + 14.0f + static_cast<float>(column) * (node_width + 16.0f);
      const float ny = graph_rect.y + 14.0f + static_cast<float>(row) * 31.0f;
      const aster::MaterialAuthoringNode &node = graph.nodes[i];
      const std::string label = clippedValue(node.label.empty() ? node.id : node.label, 18u) +
                                (i == selected_node ? "*" : "");
      if (canvas.button({nx, ny, node_width, node_height}, label,
                        "material_lab.node." + std::to_string(i))) {
        selected_node = i;
      }
      if (node.capability_status == "unsupported") {
        canvas.strokeRect({nx, ny, node_width, node_height}, {0.90f, 0.23f, 0.18f, 0.82f},
                          2.0f);
      }
    }
  }
  y += graph_height + 12.0f;
  const aster::MaterialAuthoringNode &node = graph.nodes[selected_node];
  textRow(canvas, "Selected", clippedValue(node.id), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Op", clippedValue(node.operation), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Role", clippedValue(node.role), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Persisted", node.persisted ? "yes" : "preview-only", x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Capability", clippedValue(node.capability_status), x, y, width, visible_top,
          visible_bottom);
  std::vector<std::string> params;
  for (const auto &[key, value] : node.params) {
    params.push_back(key + "=" + value);
  }
  listRows(canvas, "Param", params, 5u, x, y, width, visible_top, visible_bottom);
}

void drawMaterialLabControls(aster::UiCanvas &canvas, aster::MaterialAsset &material,
                             bool &dirty, const float x, float &y, const float width,
                             const float visible_top, const float visible_bottom) {
  section(canvas, "Authoring", x, y, width);
  const auto slider_param = [&](const std::string &name, const float fallback,
                               const float min_value, const float max_value) {
    auto found = material.params.find(name);
    if (found == material.params.end()) {
      found = material.params.emplace(name, fallback).first;
    }
    float &value = found->second;
    const float before = value;
    sliderRow(canvas, name, value, min_value, max_value, x, y, width,
              "material_lab.param." + name, visible_top, visible_bottom);
    dirty = dirty || before != value;
  };
  slider_param("roughness", 0.55f, 0.02f, 1.0f);
  slider_param("metallic", 0.0f, 0.0f, 1.0f);
  slider_param("wetness_strength", 0.0f, 0.0f, 1.0f);
  slider_param("micro_normal_strength", 0.0f, 0.0f, 1.25f);
  slider_param("height_shading", 0.0f, 0.0f, 0.85f);
  slider_param("triplanar_scale", 1.0f, 0.25f, 8.0f);
  const auto checkbox_feature = [&](const std::string &name) {
    bool value = material.explicit_features[name];
    const bool before = value;
    checkboxRow(canvas, name, value, x, y, width, "material_lab.feature." + name, visible_top,
                visible_bottom);
    material.explicit_features[name] = value;
    dirty = dirty || before != value;
  };
  checkbox_feature("triplanar");
  checkbox_feature("normal_map");
  checkbox_feature("parallax");
}

void drawMaterialLabPreviewGrid(aster::UiCanvas &canvas,
                                const std::vector<aster::MaterialLabPreviewImage> &previews,
                                const float x, float &y, const float width,
                                const float visible_top, const float visible_bottom) {
  section(canvas, "Preview", x, y, width);
  constexpr std::array<aster::MaterialLabPreviewMode, 6> modes{
      aster::MaterialLabPreviewMode::Beauty, aster::MaterialLabPreviewMode::BaseColor,
      aster::MaterialLabPreviewMode::Normal, aster::MaterialLabPreviewMode::Roughness,
      aster::MaterialLabPreviewMode::AmbientOcclusion, aster::MaterialLabPreviewMode::Fog};
  const float gap = 8.0f;
  const float thumb_width = std::max(86.0f, (width - gap) * 0.5f);
  const float thumb_height = thumb_width * 0.66f;
  for (std::size_t i = 0u; i < modes.size(); ++i) {
    const std::size_t column = i % 2u;
    if (column == 0u && i > 0u) {
      y += thumb_height + 25.0f;
    }
    const float tx = x + static_cast<float>(column) * (thumb_width + gap);
    if (y + thumb_height >= visible_top && y <= visible_bottom) {
      canvas.text(std::string(aster::materialLabPreviewModeName(modes[i])), {tx, y}, kDim,
                  1.08f);
      const aster::UiRect rect{tx, y + 15.0f, thumb_width, thumb_height};
      canvas.fillRoundRect(rect, 5.0f, {0.045f, 0.058f, 0.062f, 0.96f});
      if (i < previews.size() && previews[i].available) {
        canvas.image({rect.x + 3.0f, rect.y + 3.0f, rect.width - 6.0f, rect.height - 6.0f},
                     static_cast<std::uint32_t>(previews[i].width),
                     static_cast<std::uint32_t>(previews[i].height), previews[i].rgba8);
      } else {
        canvas.text("No preview", {rect.x + 8.0f, rect.y + rect.height * 0.45f}, kDim, 0.95f);
      }
      canvas.strokeRect(rect, {0.82f, 0.61f, 0.34f, 0.36f}, 1.0f);
    }
  }
  y += thumb_height + 31.0f;
}

void drawMaterialLabAudit(aster::UiCanvas &canvas, const aster::MaterialLabAudit &audit,
                          const float x, float &y, const float width, const float visible_top,
                          const float visible_bottom) {
  section(canvas, "Audit", x, y, width);
  textRow(canvas, "Ready", yesNo(audit.production_ready), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Score", std::to_string(audit.score), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Shader", clippedValue(audit.shader_variant_tag), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Variant key", hexU64(audit.shader_variant_key), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Feature mask", hexU64(audit.feature_mask), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Texture bytes", byteCost(audit.texture_byte_cost), x, y, width, visible_top,
          visible_bottom);
  std::vector<std::string> textures;
  for (const aster::MaterialLabTextureAudit &texture : audit.textures) {
    textures.push_back(texture.role + " " + std::to_string(texture.width) + "x" +
                       std::to_string(texture.height) + " mips " +
                       std::to_string(texture.mip_count) + " " + byteCost(texture.byte_cost));
  }
  listRows(canvas, "Texture", textures, 6u, x, y, width, visible_top, visible_bottom);
  listRows(canvas, "Surface", audit.surface_fidelity, 6u, x, y, width, visible_top,
           visible_bottom);
  listRows(canvas, "Issue", audit.issues, 6u, x, y, width, visible_top, visible_bottom);
  listRows(canvas, "Mobile", audit.mobile_degradations, 4u, x, y, width, visible_top,
           visible_bottom);
  listRows(canvas, "Provenance", audit.provenance_notes, 4u, x, y, width, visible_top,
           visible_bottom);
}

void drawMaterialLabTab(aster::UiCanvas &canvas, const aster::AssetProductionModel &model,
                        const aster::AssetProductionAsset &asset,
                        aster::MaterialAsset &material, aster::MaterialAuthoringGraph &graph,
                        std::string &loaded_asset_id, std::filesystem::path &save_path,
                        bool &save_supported, bool &dirty, std::size_t &selected_node,
                        std::size_t &selected_mesh, std::size_t &selected_environment,
                        std::string &cache_key,
                        std::vector<aster::MaterialLabPreviewImage> &previews,
                        std::vector<std::string> &diagnostics, const float x, float &y,
                        const float width, const float visible_top, const float visible_bottom) {
  section(canvas, "Material Lab", x, y, width);
  if (asset.kind != "material" && asset.kind != "asset_graph") {
    textRow(canvas, "Asset", "not material", x, y, width, visible_top, visible_bottom);
    return;
  }
  if (loaded_asset_id != asset.id) {
    loaded_asset_id = asset.id;
    selected_node = 0u;
    cache_key.clear();
    previews.clear();
    loadMaterialLabSelection(asset, model, material, graph, diagnostics, save_path,
                             save_supported, dirty);
  }
  textRow(canvas, "Asset", clippedValue(asset.id), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Source", clippedValue(material.source_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  listRows(canvas, "Status", diagnostics, 3u, x, y, width, visible_top, visible_bottom);
  drawSegmentedButtons(canvas, {"Sphere", "Rock", "Cave"}, selected_mesh, x, y, width,
                       "material_lab.mesh", visible_top, visible_bottom);
  drawSegmentedButtons(canvas, {"Studio", "Cave", "Probe", "Fog"}, selected_environment, x, y,
                       width, "material_lab.env", visible_top, visible_bottom);

  drawMaterialLabGraph(canvas, graph, selected_node, x, y, width, visible_top, visible_bottom);
  drawMaterialLabControls(canvas, material, dirty, x, y, width, visible_top, visible_bottom);
  if (dirty && graph.source_kind != "assetgraphbin") {
    graph = aster::materialAuthoringGraphForAsset(material);
  }
  const aster::TextureSetValidation validation =
      aster::validateMaterialTextureSet(material, {}, {.require_existing_files = false});
  const aster::MaterialLabAudit audit = aster::buildMaterialLabAudit(material, validation);
  refreshMaterialLabPreviews(material, selected_mesh, selected_environment, cache_key, previews);
  drawMaterialLabPreviewGrid(canvas, previews, x, y, width, visible_top, visible_bottom);
  drawMaterialLabAudit(canvas, audit, x, y, width, visible_top, visible_bottom);

  section(canvas, "Save", x, y, width);
  textRow(canvas, "Dirty", yesNo(dirty), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Mode", save_supported ? "canonical astermat" : "preview-only", x, y, width,
          visible_top, visible_bottom);
  if (save_supported && y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, width, 30.0f}, dirty ? "Save Material*" : "Save Material",
                      "material_lab.save")) {
      std::ofstream file(save_path, std::ios::binary);
      if (file) {
        file << aster::serializeMaterialAsset(material);
        dirty = false;
        diagnostics.push_back("saved: " + save_path.generic_string());
      } else {
        diagnostics.push_back("error: could not save " + save_path.generic_string());
      }
    }
  }
  y += 38.0f;
}

void applyXpbdPreset(const std::size_t index, aster::XpbdMeshAuthoringSettings &settings) {
  settings = {};
  settings.pin_rule.axis = aster::XpbdPinAxis::Y;
  settings.pin_rule.pin_greater_equal = true;
  switch (index) {
  case 1u:
    settings.frames = 18u;
    settings.simulation.iterations = 12;
    settings.simulation.dt = 1.0f / 60.0f;
    settings.simulation.gravity = {0.0f, -6.4f, 0.0f};
    settings.compliance = 0.00008f;
    settings.damping = 0.10f;
    settings.linear_damping = 0.08f;
    settings.pin_rule.threshold = 0.65f;
    break;
  case 2u:
    settings.frames = 10u;
    settings.simulation.iterations = 8;
    settings.simulation.dt = 1.0f / 50.0f;
    settings.simulation.gravity = {0.0f, -3.0f, 0.0f};
    settings.compliance = 0.0006f;
    settings.damping = 0.18f;
    settings.linear_damping = 0.16f;
    settings.pin_rule.threshold = 0.45f;
    break;
  case 0u:
  default:
    settings.frames = 12u;
    settings.simulation.iterations = 10;
    settings.simulation.dt = 1.0f / 60.0f;
    settings.simulation.gravity = {0.0f, -9.81f, 0.0f};
    settings.compliance = 0.00025f;
    settings.damping = 0.04f;
    settings.linear_damping = 0.03f;
    settings.pin_rule.threshold = 0.55f;
    break;
  }
}

bool xpbdSettingsEqual(const aster::XpbdMeshAuthoringSettings &lhs,
                       const aster::XpbdMeshAuthoringSettings &rhs) {
  constexpr float kEpsilon = 0.000001f;
  return lhs.frames == rhs.frames && lhs.simulation.iterations == rhs.simulation.iterations &&
         std::abs(lhs.simulation.dt - rhs.simulation.dt) <= kEpsilon &&
         aster::length(lhs.simulation.gravity - rhs.simulation.gravity) <= kEpsilon &&
         std::abs(lhs.compliance - rhs.compliance) <= kEpsilon &&
         std::abs(lhs.damping - rhs.damping) <= kEpsilon &&
         std::abs(lhs.linear_damping - rhs.linear_damping) <= kEpsilon &&
         lhs.pin_rule.axis == rhs.pin_rule.axis &&
         std::abs(lhs.pin_rule.threshold - rhs.pin_rule.threshold) <= kEpsilon &&
         lhs.pin_rule.pin_greater_equal == rhs.pin_rule.pin_greater_equal &&
         lhs.pin_rule.enabled == rhs.pin_rule.enabled;
}

void appendXpbdReport(std::vector<std::string> &diagnostics,
                      const aster::XpbdSourceEditReport &report) {
  diagnostics.insert(diagnostics.end(), report.diagnostics.begin(), report.diagnostics.end());
  diagnostics.insert(diagnostics.end(), report.import_report.diagnostics.begin(),
                     report.import_report.diagnostics.end());
  diagnostics.insert(diagnostics.end(), report.export_report.diagnostics.begin(),
                     report.export_report.diagnostics.end());
  if (report.applied) {
    diagnostics.push_back("saved: " + report.source_path.generic_string());
  }
  if (!report.backup_path.empty()) {
    diagnostics.push_back("backup: " + report.backup_path.generic_string());
  }
}

void loadXpbdSelection(const aster::AssetProductionModel &model,
                       const aster::AssetProductionAsset &asset,
                       const aster::XpbdMeshAuthoringSettings &settings,
                       aster::XpbdMeshAuthoringSession &session,
                       std::filesystem::path &source_path,
                       std::vector<std::string> &diagnostics, bool &preview_ready) {
  source_path = resolveProjectPath(model, asset.source_path);
  diagnostics.clear();
  preview_ready = false;
  session = {};
  if (source_path.empty()) {
    diagnostics.push_back("error: asset has no source path");
    return;
  }
  if (!aster::isXpbdEditableMeshSource(source_path)) {
    diagnostics.push_back("warning: source is preview-only for XPBD edits");
    diagnostics.push_back("source: " + source_path.generic_string());
    return;
  }
  if (!std::filesystem::exists(source_path)) {
    diagnostics.push_back("error: source OBJ is missing: " + source_path.generic_string());
    return;
  }
  const aster::AssetMeshImportResult imported =
      aster::importMeshAsset(source_path, aster::AssetMeshFormat::Obj);
  diagnostics.insert(diagnostics.end(), imported.report.diagnostics.begin(),
                     imported.report.diagnostics.end());
  if (!imported.report.ok) {
    diagnostics.push_back("error: could not load source OBJ for XPBD");
    return;
  }
  session = aster::makeXpbdMeshAuthoringSession(imported.mesh, settings);
  diagnostics.insert(diagnostics.end(), session.diagnostics.begin(), session.diagnostics.end());
}

void rebuildXpbdSessionForCurrentSettings(aster::XpbdMeshAuthoringSession &session,
                                          const aster::XpbdMeshAuthoringSettings &settings,
                                          std::vector<std::string> &diagnostics) {
  if (!session.source_loaded) {
    diagnostics.push_back("error: no editable XPBD mesh is loaded");
    return;
  }
  const aster::CpuMesh source = session.source_mesh;
  session = aster::makeXpbdMeshAuthoringSession(source, settings);
  diagnostics.insert(diagnostics.end(), session.diagnostics.begin(), session.diagnostics.end());
}

void drawXpbdScalarControls(aster::UiCanvas &canvas,
                            aster::XpbdMeshAuthoringSettings &settings, const float x,
                            float &y, const float width, const float visible_top,
                            const float visible_bottom) {
  section(canvas, "Solver", x, y, width);
  float frames = static_cast<float>(settings.frames);
  sliderRow(canvas, "Frames", frames, 1.0f, 60.0f, x, y, width, "xpbd.frames", visible_top,
            visible_bottom);
  settings.frames = static_cast<std::uint32_t>(std::round(frames));
  float iterations = static_cast<float>(settings.simulation.iterations);
  sliderRow(canvas, "Iterations", iterations, 1.0f, 32.0f, x, y, width, "xpbd.iterations",
            visible_top, visible_bottom);
  settings.simulation.iterations = static_cast<int>(std::round(iterations));
  sliderRow(canvas, "Delta time", settings.simulation.dt, 0.004f, 0.05f, x, y, width, "xpbd.dt",
            visible_top, visible_bottom);
  sliderRow(canvas, "Compliance", settings.compliance, 0.0f, 0.004f, x, y, width,
            "xpbd.compliance", visible_top, visible_bottom);
  sliderRow(canvas, "Constraint damp", settings.damping, 0.0f, 0.7f, x, y, width,
            "xpbd.damping", visible_top, visible_bottom);
  sliderRow(canvas, "Linear damp", settings.linear_damping, 0.0f, 0.7f, x, y, width,
            "xpbd.linear_damping", visible_top, visible_bottom);
  sliderRow(canvas, "Gravity Y", settings.simulation.gravity.y, -20.0f, 5.0f, x, y, width,
            "xpbd.gravity_y", visible_top, visible_bottom);

  section(canvas, "Pins", x, y, width);
  checkboxRow(canvas, "Pin enabled", settings.pin_rule.enabled, x, y, width,
              "xpbd.pin_enabled", visible_top, visible_bottom);
  std::size_t axis = settings.pin_rule.axis == aster::XpbdPinAxis::X
                         ? 0u
                         : (settings.pin_rule.axis == aster::XpbdPinAxis::Y ? 1u : 2u);
  drawSegmentedButtons(canvas, {"X", "Y", "Z"}, axis, x, y, width, "xpbd.pin_axis",
                       visible_top, visible_bottom);
  settings.pin_rule.axis = axis == 0u ? aster::XpbdPinAxis::X
                                      : (axis == 1u ? aster::XpbdPinAxis::Y
                                                    : aster::XpbdPinAxis::Z);
  std::size_t side = settings.pin_rule.pin_greater_equal ? 0u : 1u;
  drawSegmentedButtons(canvas, {"Above", "Below"}, side, x, y, width, "xpbd.pin_side",
                       visible_top, visible_bottom);
  settings.pin_rule.pin_greater_equal = side == 0u;
  sliderRow(canvas, "Pin threshold", settings.pin_rule.threshold, -4.0f, 4.0f, x, y, width,
            "xpbd.pin_threshold", visible_top, visible_bottom);
}

void drawXpbdTab(aster::UiCanvas &canvas, const aster::AssetProductionModel &model,
                 const aster::AssetProductionAsset &asset, std::size_t &selected_preset,
                 aster::XpbdMeshAuthoringSettings &settings,
                 aster::XpbdMeshAuthoringSession &session, std::string &loaded_asset_id,
                 std::filesystem::path &source_path, std::vector<std::string> &diagnostics,
                 bool &preview_ready, const float x, float &y, const float width,
                 const float visible_top, const float visible_bottom) {
  section(canvas, "XPBD", x, y, width);
  if (loaded_asset_id != asset.id) {
    loaded_asset_id = asset.id;
    loadXpbdSelection(model, asset, settings, session, source_path, diagnostics, preview_ready);
  }
  textRow(canvas, "Asset", clippedValue(asset.id), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Source", clippedValue(source_path.generic_string()), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Editable", aster::isXpbdEditableMeshSource(source_path) ? "yes" : "no", x, y,
          width, visible_top, visible_bottom);

  const std::size_t previous_preset = selected_preset;
  drawSegmentedButtons(canvas, {"Cloth", "Cable", "Settle"}, selected_preset, x, y, width,
                       "xpbd.preset", visible_top, visible_bottom);
  if (selected_preset != previous_preset) {
    applyXpbdPreset(selected_preset, settings);
    if (session.source_loaded) {
      diagnostics.clear();
      rebuildXpbdSessionForCurrentSettings(session, settings, diagnostics);
      preview_ready = false;
    }
  }

  const aster::XpbdMeshAuthoringSettings before_controls = settings;
  drawXpbdScalarControls(canvas, settings, x, y, width, visible_top, visible_bottom);
  if (!xpbdSettingsEqual(before_controls, settings)) {
    preview_ready = false;
  }

  section(canvas, "Mesh", x, y, width);
  textRow(canvas, "Vertices", std::to_string(session.source_mesh.vertices.size()), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Edges", std::to_string(session.constraints.size()), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Pinned", std::to_string(session.pinned_particles), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Preview", preview_ready ? "ready" : "stale", x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Solved", std::to_string(session.last_simulation.constraints_solved), x, y,
          width, visible_top, visible_bottom);
  textRow(canvas, "Error", std::to_string(session.last_simulation.max_distance_error), x, y,
          width, visible_top, visible_bottom);

  section(canvas, "Actions", x, y, width);
  const float button_width = std::max((width - 8.0f) * 0.5f, 82.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Reset", "xpbd.reset")) {
      loadXpbdSelection(model, asset, settings, session, source_path, diagnostics, preview_ready);
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Simulate",
                      "xpbd.simulate")) {
      diagnostics.clear();
      rebuildXpbdSessionForCurrentSettings(session, settings, diagnostics);
      const aster::XpbdSimulationReport report =
          aster::simulateXpbdMeshAuthoringSession(session);
      if (!report.stable) {
        diagnostics.push_back("error: XPBD simulation did not remain stable");
      }
      preview_ready = report.stable;
    }
  }
  y += 38.0f;
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, width, 30.0f}, "Apply to source", "xpbd.apply")) {
      diagnostics.clear();
      if (!preview_ready) {
        rebuildXpbdSessionForCurrentSettings(session, settings, diagnostics);
        const aster::XpbdSimulationReport report =
            aster::simulateXpbdMeshAuthoringSession(session);
        preview_ready = report.stable;
        if (!report.stable) {
          diagnostics.push_back("error: XPBD simulation did not remain stable");
        }
      }
      if (preview_ready) {
        const aster::XpbdSourceEditReport edit_report =
            aster::writeXpbdMeshSourceEdit(source_path, session.preview_mesh);
        appendXpbdReport(diagnostics, edit_report);
        if (edit_report.ok) {
          session.source_mesh = session.preview_mesh;
          session = aster::makeXpbdMeshAuthoringSession(session.source_mesh, settings);
          preview_ready = false;
        }
      }
    }
  }
  y += 38.0f;
  listRows(canvas, "Status", diagnostics, 7u, x, y, width, visible_top, visible_bottom);
}

void drawAssetStudioPanel(aster::UiCanvas &canvas, std::size_t &selected_asset,
                          std::size_t &selected_tab, std::size_t &selected_texture,
                          std::size_t &selected_material_lab_node,
                          std::size_t &selected_material_lab_mesh,
                          std::size_t &selected_material_lab_environment,
                          std::size_t &selected_xpbd_preset,
                          aster::MaterialAsset &material_lab_asset,
                          aster::MaterialAuthoringGraph &material_lab_graph,
                          std::string &material_lab_loaded_asset_id,
                          std::filesystem::path &material_lab_save_path,
                          bool &material_lab_save_supported, bool &material_lab_dirty,
                          std::string &material_lab_cache_key,
                          std::vector<aster::MaterialLabPreviewImage> &material_lab_previews,
                          std::vector<std::string> &material_lab_diagnostics,
                          aster::XpbdMeshAuthoringSettings &xpbd_settings,
                          aster::XpbdMeshAuthoringSession &xpbd_session,
                          std::string &xpbd_loaded_asset_id,
                          std::filesystem::path &xpbd_source_path,
                          std::vector<std::string> &xpbd_diagnostics,
                          bool &xpbd_preview_ready,
                          const aster::EditorRuntimeModel &runtime, const float x, float &y,
                          const float width, const float visible_top,
                          const float visible_bottom) {
  const aster::AssetProductionModel *model = runtime.asset_production_model;
  if (model == nullptr) {
    drawAssetPipelinePanel(canvas, selected_asset, runtime, x, y, width, visible_top,
                          visible_bottom);
    return;
  }
  section(canvas, "Asset Studio", x, y, width);
  if (model->assets.empty()) {
    textRow(canvas, "Assets", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_asset = std::min(selected_asset, model->assets.size() - 1u);
  textRow(canvas, "Database", clippedValue(model->root_path.generic_string()), x, y, width,
          visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "asset.prev") &&
        selected_asset > 0u) {
      --selected_asset;
      selected_texture = 0u;
      material_lab_loaded_asset_id.clear();
      xpbd_loaded_asset_id.clear();
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "asset.next") &&
        selected_asset + 1u < model->assets.size()) {
      ++selected_asset;
      selected_texture = 0u;
      material_lab_loaded_asset_id.clear();
      xpbd_loaded_asset_id.clear();
    }
  }
  y += 38.0f;
  drawAssetTabs(canvas, selected_tab, x, y, width, visible_top, visible_bottom);
  y += 2.0f;

  const aster::AssetProductionAsset &asset = model->assets[selected_asset];
  switch (selected_tab) {
  case 1u:
    drawMaterialTab(canvas, asset, x, y, width, visible_top, visible_bottom);
    break;
  case 2u:
    drawTextureTab(canvas, selected_texture, asset, x, y, width, visible_top, visible_bottom);
    break;
  case 3u:
    drawMeshTab(canvas, asset, x, y, width, visible_top, visible_bottom);
    break;
  case 4u:
    drawCookTab(canvas, asset, x, y, width, visible_top, visible_bottom);
    break;
  case 5u:
    drawMaterialLabTab(canvas, *model, asset, material_lab_asset, material_lab_graph,
                       material_lab_loaded_asset_id, material_lab_save_path,
                       material_lab_save_supported, material_lab_dirty,
                       selected_material_lab_node, selected_material_lab_mesh,
                       selected_material_lab_environment, material_lab_cache_key,
                       material_lab_previews, material_lab_diagnostics, x, y, width, visible_top,
                       visible_bottom);
    break;
  case 6u:
    drawXpbdTab(canvas, *model, asset, selected_xpbd_preset, xpbd_settings, xpbd_session,
                xpbd_loaded_asset_id, xpbd_source_path, xpbd_diagnostics, xpbd_preview_ready, x, y,
                width, visible_top, visible_bottom);
    break;
  default:
    selected_tab = 0u;
    drawCatalogTab(canvas, *model, asset, x, y, width, visible_top, visible_bottom);
    break;
  }
}

void drawObjectFatePanel(aster::UiCanvas &canvas, std::size_t &selected_object,
                         const aster::FrameForensics *forensics, const float x, float &y,
                         const float width, const float visible_top,
                         const float visible_bottom) {
  section(canvas, "Object Fate", x, y, width);
  if (forensics == nullptr || forensics->object_fates.empty()) {
    textRow(canvas, "Objects", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_object = std::min(selected_object, forensics->object_fates.size() - 1u);
  textRow(canvas, "Records", std::to_string(forensics->object_fates.size()), x, y, width,
          visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "fate.prev") &&
        selected_object > 0u) {
      --selected_object;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "fate.next") &&
        selected_object + 1u < forensics->object_fates.size()) {
      ++selected_object;
    }
  }
  y += 38.0f;
  const aster::ObjectRenderFateTrace &fate = forensics->object_fates[selected_object];
  textRow(canvas, "Object", clippedValue(fate.object_name), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Visible", yesNo(fate.visible), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Mesh", clippedValue(fate.mesh_key), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Material", clippedValue(fate.material_asset_id), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Shader", clippedValue(fate.shader_variant_key), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Pass", firstOrNone(fate.pass_list), x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Texture", firstOrNone(fate.texture_roles), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Resource", firstOrNone(fate.resource_transitions), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Capture", firstOrNone(fate.capture_labels), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Contribution", fate.final_contribution, x, y, width, visible_top,
          visible_bottom);
}

void drawDebuggerTimelinePanel(aster::UiCanvas &canvas, std::size_t &selected_event,
                               const aster::FrameForensics *forensics, const float x, float &y,
                               const float width, const float visible_top,
                               const float visible_bottom) {
  section(canvas, "Proof Timeline", x, y, width);
  if (forensics == nullptr || forensics->debug_timeline.empty()) {
    textRow(canvas, "Events", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_event = std::min(selected_event, forensics->debug_timeline.size() - 1u);
  textRow(canvas, "Events", std::to_string(forensics->debug_timeline.size()), x, y, width,
          visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "timeline.prev") &&
        selected_event > 0u) {
      --selected_event;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "timeline.next") &&
        selected_event + 1u < forensics->debug_timeline.size()) {
      ++selected_event;
    }
  }
  y += 38.0f;
  const aster::FrameDebuggerTimelineEvent &event =
      forensics->debug_timeline[selected_event];
  textRow(canvas, "Kind", aster::frameDebuggerTimelineEventKindName(event.kind), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Object", clippedValue(event.object_name), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Label", clippedValue(event.label), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Evidence", clippedValue(event.evidence), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "CPU/GPU",
          std::to_string(event.cpu_build_seconds * 1000.0) + " / " +
              std::to_string(event.gpu_execution_seconds * 1000.0) + " ms",
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Target",
          std::to_string(event.render_target_width) + "x" +
              std::to_string(event.render_target_height),
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Bandwidth", byteCost(event.estimated_bandwidth_bytes), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Pressure",
          "draw " + std::to_string(event.draw_count) + " mat " +
              std::to_string(event.material_variant_count) + " desc " +
              std::to_string(event.descriptor_heap_pressure),
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Pipe cache",
          std::to_string(event.pipeline_cache_hits) + " / " +
              std::to_string(event.pipeline_cache_misses),
          x, y, width, visible_top, visible_bottom);
  textRow(canvas, "Fallback", clippedValue(event.fallback_reason), x, y, width, visible_top,
          visible_bottom);
}

void drawRegressionGalleryPanel(aster::UiCanvas &canvas, std::size_t &selected_entry,
                                const aster::FrameForensics *forensics, const float x, float &y,
                                const float width, const float visible_top,
                                const float visible_bottom) {
  section(canvas, "Regression Lab", x, y, width);
  if (forensics == nullptr || forensics->regression_gallery.empty()) {
    textRow(canvas, "Images", "0", x, y, width, visible_top, visible_bottom);
    return;
  }
  selected_entry = std::min(selected_entry, forensics->regression_gallery.size() - 1u);
  textRow(canvas, "Images", std::to_string(forensics->regression_gallery.size()), x, y, width,
          visible_top, visible_bottom);
  const float button_width = std::max((width - 8.0f) * 0.5f, 72.0f);
  if (y >= visible_top && y + 34.0f <= visible_bottom) {
    if (canvas.button({x, y, button_width, 30.0f}, "Prev", "gallery.prev") &&
        selected_entry > 0u) {
      --selected_entry;
    }
    if (canvas.button({x + width - button_width, y, button_width, 30.0f}, "Next",
                      "gallery.next") &&
        selected_entry + 1u < forensics->regression_gallery.size()) {
      ++selected_entry;
    }
  }
  y += 38.0f;
  const aster::FrameRegressionGalleryEntry &entry =
      forensics->regression_gallery[selected_entry];
  textRow(canvas, "Capture", clippedValue(entry.label), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Available", yesNo(entry.available), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Image hash", hexU64(entry.image_hash), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Diff", clippedValue(entry.image_diff_status), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Backend", clippedValue(entry.backend_difference), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Pass ms", std::to_string(entry.pass_encode_seconds * 1000.0), x, y, width,
          visible_top, visible_bottom);
  textRow(canvas, "Asset hash", clippedValue(entry.asset_hash), x, y, width, visible_top,
          visible_bottom);
  textRow(canvas, "Shader key", clippedValue(entry.shader_variant_key), x, y, width,
          visible_top, visible_bottom);
}

void drawFramePanel(aster::UiCanvas &canvas, const aster::UiRect panel,
                    const aster::FrameStats &stats) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  canvas.text("Frame", {x, y}, kAmber, 1.8f);
  y += 28.0f;
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "Frame %.2f ms", stats.frame_seconds * 1000.0);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  const double fps = stats.frame_seconds > 0.0 ? 1.0 / stats.frame_seconds : 0.0;
  std::snprintf(buffer, sizeof(buffer), "Rate %.1f fps", fps);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Resolution %d x %d", stats.framebuffer_width,
                stats.framebuffer_height);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Draw calls %zu", stats.draw_calls);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Visible %zu culled %zu", stats.visible_objects,
                stats.culled_objects);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Groups %zu Rust %.3f ms", stats.instance_groups,
                stats.rust_plan_seconds * 1000.0);
  canvas.text(buffer, {x, y}, kText, 1.45f);
  y += 22.0f;
  std::snprintf(buffer, sizeof(buffer), "Pipelines %zu materials %zu", stats.pipeline_switches,
                stats.material_permutations);
  canvas.text(buffer, {x, y}, kText, 1.45f);
}

void drawCameraPanel(aster::UiCanvas &canvas, const aster::UiRect panel,
                     aster::OrbitCamera &camera) {
  drawPanelTexture(canvas, panel);
  float y = panel.y + kPanelPad;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  const float visible_top = panel.y + kPanelPad;
  const float visible_bottom = panel.y + panel.height - kPanelPad;
  section(canvas, "Camera", x, y, width);
  sliderRow(canvas, "Yaw", camera.yaw, -3.14159f, 3.14159f, x, y, width, "yaw",
            visible_top, visible_bottom);
  sliderRow(canvas, "Pitch", camera.pitch, aster::radians(-80.0f), aster::radians(80.0f), x, y,
            width, "pitch", visible_top, visible_bottom);
  sliderRow(canvas, "Radius", camera.radius, 1.8f, 24.0f, x, y, width, "radius", visible_top,
            visible_bottom);
}

void drawSceneSummary(aster::UiCanvas &canvas, const aster::Scene &scene, const float x, float &y,
                      const float width, const float visible_top, const float visible_bottom) {
  const float row_top = y;
  constexpr float kSummaryHeight = 76.0f;
  if (row_top < visible_top || row_top + kSummaryHeight > visible_bottom) {
    y += kSummaryHeight;
    return;
  }
  section(canvas, "Scene", x, y, width);
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "Objects %zu", scene.objects().size());
  canvas.text(buffer, {x, y}, kText, 1.45f);
}

} // namespace

namespace aster {

EditorUi::~EditorUi() {
  shutdown();
}

void EditorUi::initialize() {
  canvas_.initialize();
  initialized_ = true;
}

void EditorUi::beginFrame(const Vec2 viewport_size, const ControlSnapshot &input) {
  input_ = input;
  canvas_.beginFrame(viewport_size, input);
}

void EditorUi::draw(Scene &scene, OrbitCamera &camera, RendererSettings &settings,
                    const FrameStats &stats, const EditorRuntimeModel &runtime) {
  const Vec2 viewport = canvas_.viewportSize();
  const float panel_height = std::max(320.0f, viewport.y - kPanelMargin * 2.0f);
  const float panel_width = std::min(
      std::max(viewport.x - kPanelMargin * 2.0f, 260.0f),
      std::clamp(viewport.x < 760.0f ? viewport.x - kPanelMargin * 2.0f : viewport.x * 0.34f,
                 360.0f, 460.0f));
  const UiRect panel{kPanelMargin, kPanelMargin, panel_width, panel_height};
  drawPanelTexture(canvas_, panel);
  if (pointInRect(input_.pointer, panel) && input_.scroll.y != 0.0f) {
    renderer_panel_scroll_ =
        std::max(0.0f, renderer_panel_scroll_ - input_.scroll.y * 36.0f);
  }

  canvas_.pushClip({panel.x + 1.0f, panel.y + 1.0f, panel.width - 2.0f, panel.height - 2.0f});
  float y = panel.y + kPanelPad - renderer_panel_scroll_;
  const float x = panel.x + kPanelPad;
  const float width = panel.width - kPanelPad * 2.0f;
  const float panel_bottom = panel.y + panel.height - kPanelPad;
  const float visible_top = panel.y + kPanelPad;

  canvas_.text("Aster Learning Studio", {x, y}, kText, 2.2f);
  y += 31.0f;
  canvas_.wrappedText("Native engine controls over scene, renderer, and camera state.", {x, y},
                      width, kDim, 1.3f);
  y += canvas_.wrappedTextHeight("Native engine controls over scene, renderer, and camera state.",
                                 width, 1.3f) +
       22.0f;

  section(canvas_, "Renderer", x, y, width);
  sliderRow(canvas_, "Exposure", settings.exposure, 0.2f, 2.5f, x, y, width, "exposure",
            visible_top, panel_bottom);
  sliderRow(canvas_, "Ambient", settings.ambient_strength, 0.0f, 0.6f, x, y, width, "ambient",
            visible_top, panel_bottom);
  checkboxRow(canvas_, "Grounding", settings.grounding.enabled, x, y, width, "grounding",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Contact shadows", settings.grounding.contact_shadows, x, y, width,
              "contact.shadows", visible_top, panel_bottom);
  sliderRow(canvas_, "Ground AO", settings.grounding.surface_occlusion_strength, 0.0f, 0.7f, x, y,
            width, "ground.ao", visible_top, panel_bottom);
  sliderRow(canvas_, "AO mix", settings.grounding.surface_occlusion_mix, 0.0f, 1.0f, x, y, width,
            "ground.ao.mix", visible_top, panel_bottom);
  sliderRow(canvas_, "AO floor", settings.grounding.surface_occlusion_min, 0.0f, 1.0f, x, y, width,
            "ground.ao.floor", visible_top, panel_bottom);
  sliderRow(canvas_, "Shadow weight", settings.grounding.contact_shadow_strength, 0.0f, 0.8f, x, y,
            width, "contact.weight", visible_top, panel_bottom);
  checkboxRow(canvas_, "Atmosphere", settings.atmosphere.enabled, x, y, width, "atmosphere",
              visible_top, panel_bottom);
  renderStyleRow(canvas_, settings, x, y, width, visible_top, panel_bottom);
  sliderRow(canvas_, "Fog", settings.atmosphere.fog_strength, 0.0f, 0.6f, x, y, width, "fog",
            visible_top, panel_bottom);
  sliderRow(canvas_, "Saturation", settings.atmosphere.saturation, 0.0f, 1.4f, x, y, width,
            "saturation", visible_top, panel_bottom);
  sliderRow(canvas_, "Contrast", settings.atmosphere.contrast, 0.5f, 1.5f, x, y, width,
            "contrast", visible_top, panel_bottom);
  checkboxRow(canvas_, "ACES tone map", settings.use_aces_tonemap, x, y, width, "aces",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Animation", settings.animate_scene, x, y, width, "animation", visible_top,
              panel_bottom);
  checkboxRow(canvas_, "Depth test", settings.pipeline.depth_test, x, y, width, "depth",
              visible_top, panel_bottom);
  checkboxRow(canvas_, "Back-face culling", settings.pipeline.back_face_culling, x, y, width,
              "cull", visible_top, panel_bottom);
  checkboxRow(canvas_, "Multisampling", settings.pipeline.multisampling, x, y, width, "msaa",
              visible_top, panel_bottom);
  y += 8.0f;
  drawBackendSummary(canvas_, runtime, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawGraphSummary(canvas_, selected_graph_pass_, selected_resource_provenance_,
                   runtime.render_graph, runtime.frame_forensics, x, y, width, visible_top,
                   panel_bottom);
  y += 8.0f;
  drawSceneSummary(canvas_, scene, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawAssetStudioPanel(canvas_, selected_asset_, selected_asset_tab_, selected_texture_,
                       selected_material_lab_node_, selected_material_lab_mesh_,
                       selected_material_lab_environment_, selected_xpbd_preset_,
                       material_lab_asset_, material_lab_graph_,
                       material_lab_loaded_asset_id_, material_lab_save_path_,
                       material_lab_save_supported_, material_lab_dirty_,
                       material_lab_cache_key_, material_lab_previews_,
                       material_lab_diagnostics_, xpbd_settings_, xpbd_session_,
                       xpbd_loaded_asset_id_, xpbd_source_path_, xpbd_diagnostics_,
                       xpbd_preview_ready_, runtime, x, y, width, visible_top, panel_bottom);
  y += 8.0f;
  drawObjectFatePanel(canvas_, selected_object_fate_, runtime.frame_forensics, x, y, width,
                      visible_top, panel_bottom);
  y += 8.0f;
  drawDebuggerTimelinePanel(canvas_, selected_timeline_event_, runtime.frame_forensics, x, y,
                            width, visible_top, panel_bottom);
  y += 8.0f;
  drawRegressionGalleryPanel(canvas_, selected_regression_entry_, runtime.frame_forensics, x, y,
                             width, visible_top, panel_bottom);
  const float content_height = y + kPanelPad + renderer_panel_scroll_ - panel.y;
  const float max_scroll = std::max(0.0f, content_height - panel.height);
  renderer_panel_scroll_ = std::clamp(renderer_panel_scroll_, 0.0f, max_scroll);
  if (max_scroll > 1.0f) {
    const float track_height = panel.height - 32.0f;
    const float thumb_height = std::max(34.0f, track_height * (panel.height / content_height));
    const float thumb_y = panel.y + 16.0f +
                          (track_height - thumb_height) *
                              (renderer_panel_scroll_ / std::max(max_scroll, 0.001f));
    canvas_.fillRoundRect({panel.x + panel.width - 9.0f, thumb_y, 4.0f, thumb_height}, 2.0f,
                          {0.78f, 0.58f, 0.31f, 0.46f});
  }
  canvas_.popClip();

  if (viewport.x >= 900.0f) {
    const float side_width = 324.0f;
    const float side_x = viewport.x - side_width - kPanelMargin;
    const UiRect frame_panel{side_x, kPanelMargin, side_width, 206.0f};
    drawFramePanel(canvas_, frame_panel, stats);
    const UiRect camera_panel{side_x, frame_panel.y + frame_panel.height + 14.0f, side_width,
                              198.0f};
    drawCameraPanel(canvas_, camera_panel, camera);
  }
}

void EditorUi::endFrame() {
  canvas_.endFrame();
}

void EditorUi::shutdown() {
  if (!initialized_) {
    return;
  }
  canvas_.shutdown();
  initialized_ = false;
}

bool EditorUi::wantsMouse() const {
  return canvas_.wantsMouse();
}

bool EditorUi::wantsKeyboard() const {
  return canvas_.wantsKeyboard();
}

} // namespace aster
