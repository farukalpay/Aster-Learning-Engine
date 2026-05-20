// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

#include "aster/asset/content_pack.hpp"
#include "aster/asset/asset_registry.hpp"
#include "aster/asset/derived_asset_cache.hpp"
#include "aster/render/visual_regression.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

namespace {

struct TestCanvas {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};

TestCanvas makeCanvas(const int width, const int height, const std::array<std::uint8_t, 4> color) {
  TestCanvas canvas{width, height};
  canvas.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  for (std::size_t i = 0u; i + 3u < canvas.rgba.size(); i += 4u) {
    canvas.rgba[i + 0u] = color[0];
    canvas.rgba[i + 1u] = color[1];
    canvas.rgba[i + 2u] = color[2];
    canvas.rgba[i + 3u] = color[3];
  }
  return canvas;
}

void setPixel(TestCanvas &canvas, const int x, const int y,
              const std::array<std::uint8_t, 4> color) {
  if (x < 0 || y < 0 || x >= canvas.width || y >= canvas.height) {
    return;
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.width) +
       static_cast<std::size_t>(x)) *
      4u;
  canvas.rgba[offset + 0u] = color[0];
  canvas.rgba[offset + 1u] = color[1];
  canvas.rgba[offset + 2u] = color[2];
  canvas.rgba[offset + 3u] = color[3];
}

void fillRect(TestCanvas &canvas, const int x0, const int y0, const int width, const int height,
              const std::array<std::uint8_t, 4> color) {
  for (int y = y0; y < y0 + height; ++y) {
    for (int x = x0; x < x0 + width; ++x) {
      setPixel(canvas, x, y, color);
    }
  }
}

void strokeRect(TestCanvas &canvas, const int x0, const int y0, const int width,
                const int height, const std::array<std::uint8_t, 4> color) {
  fillRect(canvas, x0, y0, width, 2, color);
  fillRect(canvas, x0, y0 + height - 2, width, 2, color);
  fillRect(canvas, x0, y0, 2, height, color);
  fillRect(canvas, x0 + width - 2, y0, 2, height, color);
}

void drawLine(TestCanvas &canvas, int x0, int y0, const int x1, const int y1,
              const std::array<std::uint8_t, 4> color) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    fillRect(canvas, x0 - 1, y0 - 1, 3, 3, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int twice_error = 2 * error;
    if (twice_error >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice_error <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void writeCanvasPng(const std::filesystem::path &path, const TestCanvas &canvas) {
  aster::writeRgbaPng(path, canvas.width, canvas.height, canvas.rgba);
}

std::uint32_t readBe32(const std::array<std::uint8_t, 24> &bytes, const std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset + 0u]) << 24u) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 8u) |
         static_cast<std::uint32_t>(bytes[offset + 3u]);
}

void verifyPng(const std::filesystem::path &path, const int expected_width,
               const int expected_height) {
  std::ifstream file(path, std::ios::binary);
  assert(file.good());
  std::array<std::uint8_t, 24> header{};
  file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
  const std::array<std::uint8_t, 8> signature{137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  assert(std::equal(signature.begin(), signature.end(), header.begin()));
  assert(readBe32(header, 16u) == static_cast<std::uint32_t>(expected_width));
  assert(readBe32(header, 20u) == static_cast<std::uint32_t>(expected_height));
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
  assert(!bytes.empty());
}

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path);
  assert(file.good());
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeFileBytes(const std::filesystem::path &path, const std::string_view bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  assert(file.good());
}

void writeLe32(std::ofstream &file, const std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value & 0xffu),
                         static_cast<char>((value >> 8u) & 0xffu),
                         static_cast<char>((value >> 16u) & 0xffu),
                         static_cast<char>((value >> 24u) & 0xffu)};
  file.write(bytes, 4);
}

void writeTinyWad(const std::filesystem::path &path) {
  std::filesystem::create_directories(path.parent_path());
  constexpr std::string_view payload = "glow-lump";
  std::ofstream file(path, std::ios::binary);
  file.write("PWAD", 4);
  writeLe32(file, 1u);
  writeLe32(file, 12u + static_cast<std::uint32_t>(payload.size()));
  file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  writeLe32(file, 12u);
  writeLe32(file, static_cast<std::uint32_t>(payload.size()));
  char name[8]{};
  std::memcpy(name, "GLOW", 4u);
  file.write(name, 8);
  assert(file.good());
}

bool contains(const std::vector<std::string> &values, const std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void testAssetPathIndexRegistryAndQueue() {
  aster::AssetPathIndex paths;
  std::vector<std::string> events;
  const aster::SignalConnection connection =
      paths.changes().connect([&](const aster::AssetPathIndexEvent &event) {
        events.push_back(std::string(aster::assetPathIndexEventKindName(event.kind)) + ":" +
                         event.path);
      });
  assert(paths.addPath("Assets//Material/Weathered/"));
  assert(paths.contains("Assets/Material/Weathered"));
  assert(paths.parentPath("Assets/Material/Weathered") == "Assets/Material");
  assert(contains(paths.subPaths("Assets", false), "Assets/Material"));
  assert(contains(paths.subPaths("Assets", true), "Assets/Material/Weathered"));
  assert(paths.removePath("Assets/Material/"));
  assert(!paths.contains("Assets/Material/Weathered"));
  assert(!events.empty());

  aster::AssetRegistry registry;
  registry.upsert({.guid = "guid-wet",
                   .id = "material.wet",
                   .name = "Wet Rock",
                   .kind = "material",
                   .catalog_path = "Assets/Material",
                   .source_path = "/tmp/aster/materials/wet.astermat",
                   .production_ready = true,
                   .tags = {"material", "wet", "runtime"},
                   .dependency_ids = {"texture.wet.albedo"},
                   .metadata = {{"surface", "wet stratified rock"}, {"authoring", "batch1"}}});
  registry.upsert({.guid = "guid-pipe",
                   .id = "mesh.pipe",
                   .name = "Industrial Pipe",
                   .kind = "mesh",
                   .catalog_path = "Assets/Mesh/Industrial",
                   .source_path = "/tmp/aster/meshes/pipe.mesh",
                   .production_ready = false,
                   .tags = {"mesh", "industrial"},
                   .metadata = {{"surface", "rusted metal"}, {"authoring", "batch1"}}});
  const auto by_name = registry.query({.names = {"Wet Rock"}});
  assert(by_name.size() == 1u && by_name.front()->id == "material.wet");
  const auto by_source =
      registry.query({.source_paths = {std::filesystem::path("/tmp/aster/meshes/pipe.mesh")}});
  assert(by_source.size() == 1u && by_source.front()->id == "mesh.pipe");
  const auto by_any_tag = registry.query({.tags_any = {"industrial", "missing"}});
  assert(by_any_tag.size() == 1u && by_any_tag.front()->id == "mesh.pipe");
  const auto by_metadata = registry.query({.metadata_contains = {{"surface", "stratified"}}});
  assert(by_metadata.size() == 1u && by_metadata.front()->id == "material.wet");
  const auto recursive =
      registry.query({.catalog_paths = {"Assets/Mesh"}, .recursive_paths = true});
  assert(recursive.size() == 1u && recursive.front()->id == "mesh.pipe");
  assert(aster::assetDependencyKindFromRole("texture") == aster::AssetDependencyKind::Soft);

  aster::AssetGatherQueue queue;
  queue.push({.id = "material.wet", .source_path = "wet.astermat", .kind = "material",
              .priority = 4});
  queue.push({.id = "mesh.pipe", .source_path = "pipe.mesh", .kind = "mesh", .priority = 9});
  queue.push({.id = "texture.wet.albedo", .source_path = "wet.ktx2", .kind = "texture",
              .priority = 2});
  queue.prioritize([](const aster::AssetGatherItem &item) { return item.kind == "mesh"; });
  assert(queue.pop().id == "mesh.pipe");
  queue.trim();
  assert(queue.size() == 2u);
}

void testDerivedAssetCachePolicyAndRollup() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "aster_engine_contract_cache_test";
  std::filesystem::remove_all(root);

  aster::DerivedAssetCachePolicy policy;
  policy.verify_reads = true;
  policy.namespace_prefix = "batch1";
  policy.max_key_length = 48u;
  aster::DerivedAssetCache cache(root, aster::DerivedAssetCacheBackend::MemoryAndFilesystem,
                                 {.worker_count = 1u, .deterministic = true}, policy);

  aster::DerivedAssetCacheRollup rollup = cache.startRollup("contract-cache");
  const aster::DerivedAssetAsyncHandle first =
      cache.buildAsync({.plugin_name = "contract bake",
                        .version = "v1",
                        .key_suffix = "materials/wet:rock/with/a/long/key",
                        .build = []() { return aster::DerivedAssetBytes{3u, 1u, 4u, 1u, 5u}; }});
  rollup.add(first);
  rollup.wait();
  const aster::DerivedAssetCacheRollupReport built = rollup.report();
  assert(built.completed == 1u);
  assert(built.built == 1u);
  assert(built.bytes == 5u);

  const std::string key = aster::DerivedAssetCache::buildCacheKey(
      "contract bake", "v1", "materials/wet:rock/with/a/long/key");
  assert(std::filesystem::exists(cache.filePathForKey(key)));
  assert(cache.filePathForKey(key).parent_path().filename() == "batch1");

  aster::DerivedAssetCacheRollup cached_rollup = cache.startRollup("contract-cache-hit");
  const aster::DerivedAssetAsyncHandle second =
      cache.buildAsync({.plugin_name = "contract bake",
                        .version = "v1",
                        .key_suffix = "materials/wet:rock/with/a/long/key",
                        .build = []() { return aster::DerivedAssetBytes{99u}; }});
  cached_rollup.add(second);
  const aster::DerivedAssetCacheRollupReport cached = cached_rollup.report();
  assert(cached.completed == 1u);
  assert(cached.hits == 1u);

  cache.clearMemory();
  aster::DerivedAssetBytes from_disk;
  assert(cache.get(key, from_disk));
  assert(from_disk == (aster::DerivedAssetBytes{3u, 1u, 4u, 1u, 5u}));
  assert(!cache.usageRows().empty());

  aster::DerivedAssetCachePolicy transient_policy;
  transient_policy.transient_entries = true;
  aster::DerivedAssetCache transient_cache(root / "transient",
                                           aster::DerivedAssetCacheBackend::MemoryAndFilesystem,
                                           {.worker_count = 1u, .deterministic = true},
                                           transient_policy);
  transient_cache.put("transient-key", {1u, 2u, 3u});
  assert(!std::filesystem::exists(transient_cache.filePathForKey("transient-key")));
  std::filesystem::remove_all(root);
}

void testContentPackOverlay() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "aster_engine_contract_pack_test";
  std::filesystem::remove_all(root);
  writeFileBytes(root / "base_rock.bin", "base-rock");
  writeFileBytes(root / "overlay_rock.bin", "overlay-rock");
  writeTinyWad(root / "glow.wad");

  aster::AsterContentPack pack;
  assert(pack.mountFile(root / "base_rock.bin", "ROCK", 0, true));
  assert(pack.mountLumpArchive(root / "glow.wad", 5, true));
  assert(pack.mountFile(root / "overlay_rock.bin", "ROCK", 10, true));
  const std::size_t rock = pack.require("rock");
  const std::vector<std::uint8_t> rock_bytes = pack.read(rock);
  assert(std::string(rock_bytes.begin(), rock_bytes.end()) == "overlay-rock");
  assert(std::string(pack.cache(pack.require("GLOW")).begin(), pack.cache(pack.require("GLOW")).end()) ==
         "glow-lump");
  assert(pack.reload());

  const std::vector<aster::AsterContentPackProfileRow> profile = pack.profile();
  assert(profile.size() == 3u);
  assert(profile.front().mount_priority == 10);
  assert(aster::AsterContentPack::canonicalName("rock.texture") == "ROCK");

  aster::AsterPackIndex index;
  index.mount(pack);
  const aster::AsterContentPackRecord *record = index.find("ROCK");
  assert(record != nullptr);
  assert(record->mount_priority == 10);
  assert(index.size() == pack.size());
  std::filesystem::remove_all(root);
}

void testJobGraphTrace() {
  aster::JobGraph graph({.worker_count = 1u, .deterministic = true});
  int state = 0;
  const aster::JobId main =
      graph.add({.name = "prepare-assets", .lane = aster::JobLane::Main, .run = [&](auto &) {
                   state += 1;
                 }});
  graph.add({.name = "render-preview",
             .priority = aster::JobPriority::High,
             .lane = aster::JobLane::Render,
             .dependencies = {main},
             .run = [&](auto &) { state += 10; }});
  const aster::JobId io =
      graph.add({.name = "read-missing-pack", .lane = aster::JobLane::Io, .run = [](auto &) {
                   throw std::runtime_error("expected test failure");
                 }});
  graph.add({.name = "background-after-fail",
             .lane = aster::JobLane::Background,
             .dependencies = {io},
             .run = [&](auto &) { state += 100; }});

  const aster::JobGraphDiagnostics diagnostics = graph.run();
  assert(state == 11);
  assert(diagnostics.executed_jobs == 2u);
  assert(diagnostics.failed_jobs == 1u);
  assert(diagnostics.skipped_jobs == 1u);

  const std::vector<aster::JobTraceEvent> trace = graph.traceEvents();
  assert(std::count_if(trace.begin(), trace.end(), [](const aster::JobTraceEvent &event) {
           return event.kind == aster::JobTraceEventKind::Queued;
         }) == 4);
  assert(std::any_of(trace.begin(), trace.end(), [](const aster::JobTraceEvent &event) {
    return event.kind == aster::JobTraceEventKind::Completed &&
           event.lane == aster::JobLane::Render;
  }));
  assert(std::any_of(trace.begin(), trace.end(), [](const aster::JobTraceEvent &event) {
    return event.kind == aster::JobTraceEventKind::Skipped &&
           event.lane == aster::JobLane::Background;
  }));
  assert(std::string(aster::jobLaneName(aster::JobLane::Io)) == "io");
}

TestCanvas makeAssetGraphArtifact() {
  TestCanvas canvas = makeCanvas(360, 220, {20u, 25u, 32u, 255u});
  const std::array<std::uint8_t, 4> edge{86u, 144u, 214u, 255u};
  drawLine(canvas, 76, 64, 178, 112, edge);
  drawLine(canvas, 180, 112, 290, 64, edge);
  drawLine(canvas, 180, 112, 290, 162, edge);
  fillRect(canvas, 28, 38, 96, 52, {58u, 178u, 136u, 255u});
  fillRect(canvas, 132, 88, 96, 52, {224u, 177u, 79u, 255u});
  fillRect(canvas, 246, 38, 88, 52, {86u, 144u, 214u, 255u});
  fillRect(canvas, 246, 136, 88, 52, {201u, 102u, 130u, 255u});
  strokeRect(canvas, 20, 30, 112, 68, {240u, 246u, 252u, 255u});
  strokeRect(canvas, 124, 80, 112, 68, {240u, 246u, 252u, 255u});
  strokeRect(canvas, 238, 30, 104, 68, {240u, 246u, 252u, 255u});
  strokeRect(canvas, 238, 128, 104, 68, {240u, 246u, 252u, 255u});
  return canvas;
}

TestCanvas makeCacheRollupArtifact() {
  TestCanvas canvas = makeCanvas(360, 220, {18u, 22u, 28u, 255u});
  for (int x = 30; x < 330; x += 30) {
    fillRect(canvas, x, 34, 1, 150, {45u, 52u, 64u, 255u});
  }
  fillRect(canvas, 56, 104, 42, 80, {74u, 191u, 139u, 255u});
  fillRect(canvas, 126, 64, 42, 120, {86u, 144u, 214u, 255u});
  fillRect(canvas, 196, 134, 42, 50, {224u, 177u, 79u, 255u});
  fillRect(canvas, 266, 82, 42, 102, {201u, 102u, 130u, 255u});
  strokeRect(canvas, 28, 28, 304, 160, {233u, 238u, 244u, 255u});
  fillRect(canvas, 42, 194, 276, 6, {74u, 191u, 139u, 255u});
  return canvas;
}

TestCanvas makeJobTraceArtifact() {
  TestCanvas canvas = makeCanvas(420, 220, {19u, 23u, 30u, 255u});
  const std::array<std::uint8_t, 4> lanes[] = {{74u, 191u, 139u, 255u},
                                               {86u, 144u, 214u, 255u},
                                               {224u, 177u, 79u, 255u},
                                               {201u, 102u, 130u, 255u}};
  for (int row = 0; row < 4; ++row) {
    const int y = 34 + row * 42;
    fillRect(canvas, 24, y, 372, 22, {35u, 42u, 54u, 255u});
    fillRect(canvas, 46 + row * 54, y - 4, 88, 30, lanes[row]);
    strokeRect(canvas, 24, y, 372, 22, {79u, 87u, 102u, 255u});
  }
  drawLine(canvas, 90, 45, 158, 87, {235u, 240u, 247u, 255u});
  drawLine(canvas, 158, 87, 242, 129, {235u, 240u, 247u, 255u});
  drawLine(canvas, 242, 129, 296, 171, {235u, 240u, 247u, 255u});
  return canvas;
}

TestCanvas makePackOverlayArtifact() {
  TestCanvas canvas = makeCanvas(360, 220, {21u, 24u, 31u, 255u});
  fillRect(canvas, 54, 132, 250, 36, {72u, 89u, 112u, 255u});
  fillRect(canvas, 72, 94, 214, 44, {86u, 144u, 214u, 255u});
  fillRect(canvas, 94, 56, 170, 52, {74u, 191u, 139u, 255u});
  strokeRect(canvas, 54, 132, 250, 36, {236u, 241u, 248u, 255u});
  strokeRect(canvas, 72, 94, 214, 44, {236u, 241u, 248u, 255u});
  strokeRect(canvas, 94, 56, 170, 52, {236u, 241u, 248u, 255u});
  fillRect(canvas, 278, 66, 28, 120, {224u, 177u, 79u, 255u});
  fillRect(canvas, 312, 94, 16, 74, {201u, 102u, 130u, 255u});
  return canvas;
}

TestCanvas makeDiffSource(const bool shifted) {
  TestCanvas canvas = makeCanvas(96, 64, {22u, 25u, 31u, 255u});
  fillRect(canvas, shifted ? 22 : 18, 15, 34, 26, {74u, 191u, 139u, 255u});
  fillRect(canvas, 48, shifted ? 29 : 25, 28, 24, {86u, 144u, 214u, 255u});
  drawLine(canvas, 12, 52, shifted ? 84 : 78, 10, {224u, 177u, 79u, 255u});
  return canvas;
}

void writeArtifactSet(const std::filesystem::path &output) {
  std::filesystem::create_directories(output);
  writeCanvasPng(output / "asset_graph.png", makeAssetGraphArtifact());
  writeCanvasPng(output / "cache_rollup.png", makeCacheRollupArtifact());
  writeCanvasPng(output / "job_trace.png", makeJobTraceArtifact());
  writeCanvasPng(output / "pack_overlay.png", makePackOverlayArtifact());

  const TestCanvas approved = makeDiffSource(false);
  const TestCanvas incoming = makeDiffSource(true);
  const aster::VisualDiffResult diff = aster::compareRgbaImages(
      {.width = approved.width, .height = approved.height, .rgba = approved.rgba},
      {.width = incoming.width, .height = incoming.height, .rgba = incoming.rgba},
      aster::previewVisualDiffTolerance());
  const std::vector<std::uint8_t> sheet = aster::makeVisualDiffContactSheet(
      {.width = approved.width, .height = approved.height, .rgba = approved.rgba},
      {.width = incoming.width, .height = incoming.height, .rgba = incoming.rgba}, diff);
  aster::writeRgbaPng(output / "visual_diff_contact_sheet.png", diff.width * 3 + 8,
                      diff.height + 8, sheet);

  std::ofstream readme(output / "README.md");
  readme << "# Aster Engine Contract Batch 1 Artifacts\n\n";
  readme << "These PNGs are generated by `aster_engine_contract_port_tests write-artifacts`.\n\n";
  readme << "![Asset graph](asset_graph.png)\n\n";
  readme << "![Cache rollup](cache_rollup.png)\n\n";
  readme << "![Job trace](job_trace.png)\n\n";
  readme << "![Pack overlay](pack_overlay.png)\n\n";
  readme << "![Visual diff contact sheet](visual_diff_contact_sheet.png)\n";
  assert(readme.good());
}

void verifyArtifactSet(const std::filesystem::path &output) {
  verifyPng(output / "asset_graph.png", 360, 220);
  verifyPng(output / "cache_rollup.png", 360, 220);
  verifyPng(output / "job_trace.png", 420, 220);
  verifyPng(output / "pack_overlay.png", 360, 220);
  verifyPng(output / "visual_diff_contact_sheet.png", 296, 72);
  const std::string readme = readTextFile(output / "README.md");
  assert(readme.find("asset_graph.png") != std::string::npos);
  assert(readme.find("visual_diff_contact_sheet.png") != std::string::npos);
}

void testArtifactWriter() {
  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "aster_engine_contract_artifacts_test";
  std::filesystem::remove_all(output);
  writeArtifactSet(output);
  verifyArtifactSet(output);
  std::filesystem::remove_all(output);
}

void runAllTests() {
  testAssetPathIndexRegistryAndQueue();
  testDerivedAssetCachePolicyAndRollup();
  testContentPackOverlay();
  testJobGraphTrace();
  testArtifactWriter();
}

} // namespace

int main(const int argc, char **argv) {
  if (argc > 1 && std::string_view(argv[1]) == "write-artifacts") {
    std::filesystem::path output =
        std::filesystem::path(ASTER_SOURCE_DIR) / "tests" / "artifacts" /
        "engine_contract_batch1";
    for (int i = 2; i + 1 < argc; ++i) {
      if (std::string_view(argv[i]) == "--output") {
        output = argv[i + 1];
      }
    }
    writeArtifactSet(output);
    verifyArtifactSet(output);
    return 0;
  }

  if (argc <= 1) {
    runAllTests();
    return 0;
  }

  const std::string_view test_case = argv[1];
  if (test_case == "asset_path_index_registry_and_queue") {
    testAssetPathIndexRegistryAndQueue();
  } else if (test_case == "derived_asset_cache_policy_and_rollup") {
    testDerivedAssetCachePolicyAndRollup();
  } else if (test_case == "content_pack_overlay") {
    testContentPackOverlay();
  } else if (test_case == "job_graph_trace") {
    testJobGraphTrace();
  } else if (test_case == "artifact_writer") {
    testArtifactWriter();
  } else {
    throw std::runtime_error("Unknown engine contract port test case.");
  }
  return 0;
}
