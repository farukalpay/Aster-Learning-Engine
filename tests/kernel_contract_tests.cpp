// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/api.hpp"

#include <cassert>
#include <cstddef>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

namespace {

static_assert(std::is_standard_layout_v<AsterAbiVersion>);
static_assert(std::is_standard_layout_v<AsterStatus>);
static_assert(std::is_standard_layout_v<AsterStringView>);
static_assert(std::is_standard_layout_v<AsterSpan>);
static_assert(std::is_standard_layout_v<AsterEngineDesc>);
static_assert(sizeof(AsterStatusCode) == sizeof(std::int32_t));
static_assert(offsetof(AsterStatus, size) == 0u);
static_assert(offsetof(AsterStatus, version) > offsetof(AsterStatus, size));
static_assert(offsetof(AsterStatus, code) > offsetof(AsterStatus, version));
static_assert(offsetof(AsterStatus, message) > offsetof(AsterStatus, code));
static_assert(offsetof(AsterEngineDesc, size) == 0u);
static_assert(offsetof(AsterEngineDesc, version) > offsetof(AsterEngineDesc, size));
static_assert(offsetof(AsterEngineDesc, application_name) > offsetof(AsterEngineDesc, version));
static_assert(offsetof(AsterEngineDesc, flags) > offsetof(AsterEngineDesc, application_name));

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  assert(input.good());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::set<std::string> readManifest() {
  std::ifstream input(std::string(ASTER_SOURCE_DIR) + "/abi/aster_kernel.symbols");
  assert(input.good());
  std::set<std::string> symbols;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    const std::size_t last = line.find_last_not_of(" \t\r\n");
    symbols.insert(line.substr(first, last - first + 1u));
  }
  return symbols;
}

void testAbiHeaderStaysPlainC() {
  const std::string header = readFile(std::string(ASTER_SOURCE_DIR) + "/include/aster/kernel/abi.h");
  const char *forbidden[] = {
      "#include <vector", "#include <string", "#include <memory", "#include <optional",
      "#include <span",   "#include \"aster/", "std::"};
  for (const char *token : forbidden) {
    assert(header.find(token) == std::string::npos);
  }
}

void testStatusAndEngineLifecycle() {
  const AsterAbiVersion version = aster_kernel_abi_version();
  assert(version.major == ASTER_KERNEL_ABI_MAJOR);
  assert(version.minor == ASTER_KERNEL_ABI_MINOR);
  assert(version.patch == ASTER_KERNEL_ABI_PATCH);

  AsterEngineHandle engine = nullptr;
  const AsterEngineDesc desc{sizeof(AsterEngineDesc),
                             ASTER_KERNEL_STRUCT_VERSION_1,
                             {"kernel-contract-test", 20u},
                             0u};
  AsterStatus status = aster_kernel_engine_create(&desc, &engine);
  assert(status.code == ASTER_STATUS_OK);
  assert(engine != nullptr);
  assert(aster_kernel_engine_last_status(engine).code == ASTER_STATUS_OK);
  assert(aster_kernel_engine_destroy(engine).code == ASTER_STATUS_OK);

  AsterEngineDesc stale = desc;
  stale.version = 0u;
  engine = nullptr;
  status = aster_kernel_engine_create(&stale, &engine);
  assert(status.code == ASTER_STATUS_ABI_MISMATCH);
  assert(engine == nullptr);
}

void testCppWrapperUsesResultStatus() {
  auto engine = aster::kernel::Engine::create();
  assert(engine);
  assert(engine.value().valid());

  AsterEngineDesc bad_desc{sizeof(AsterEngineDesc), 0u, {}, 0u};
  auto failed = aster::kernel::Engine::create(bad_desc);
  assert(!failed);
  assert(failed.status().code() == ASTER_STATUS_ABI_MISMATCH);
}

void testManifestNamesMatchLinkedApi() {
  const std::set<std::string> expected{
      "aster_kernel_abi_version",
      "aster_kernel_status_ok",
      "aster_kernel_status_from_code",
      "aster_kernel_engine_create",
      "aster_kernel_engine_destroy",
      "aster_kernel_engine_last_status",
      "aster_kernel_window_destroy",
      "aster_kernel_scene_destroy",
      "aster_kernel_renderer_destroy",
      "aster_kernel_mesh_destroy",
      "aster_kernel_material_destroy",
      "aster_kernel_physics_world_destroy",
      "aster_kernel_system_world_destroy",
      "aster_kernel_sample_app_destroy",
  };
  assert(readManifest() == expected);

  (void)&aster_kernel_abi_version;
  (void)&aster_kernel_status_ok;
  (void)&aster_kernel_status_from_code;
  (void)&aster_kernel_engine_create;
  (void)&aster_kernel_engine_destroy;
  (void)&aster_kernel_engine_last_status;
  (void)&aster_kernel_window_destroy;
  (void)&aster_kernel_scene_destroy;
  (void)&aster_kernel_renderer_destroy;
  (void)&aster_kernel_mesh_destroy;
  (void)&aster_kernel_material_destroy;
  (void)&aster_kernel_physics_world_destroy;
  (void)&aster_kernel_system_world_destroy;
  (void)&aster_kernel_sample_app_destroy;
}

} // namespace

int main() {
  testAbiHeaderStaysPlainC();
  testStatusAndEngineLifecycle();
  testCppWrapperUsesResultStatus();
  testManifestNamesMatchLinkedApi();
  return 0;
}
