// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_hot_reload.hpp"

namespace aster {

ShaderHotReloadState snapshotShaderHotReloadInputs(
    const std::vector<std::filesystem::path> &paths) {
  ShaderHotReloadState state;
  for (const std::filesystem::path &path : paths) {
    if (!std::filesystem::exists(path)) {
      state.diagnostics.push_back("missing hot reload input: " + path.string());
      continue;
    }
    state.files[path] = {.write_time = std::filesystem::last_write_time(path),
                         .size = std::filesystem::file_size(path)};
  }
  return state;
}

bool shaderHotReloadInputsChanged(const ShaderHotReloadState &before,
                                  const ShaderHotReloadState &after) {
  if (before.files.size() != after.files.size()) {
    return true;
  }
  for (const auto &[path, state] : before.files) {
    const auto it = after.files.find(path);
    if (it == after.files.end() || it->second.write_time != state.write_time ||
        it->second.size != state.size) {
      return true;
    }
  }
  return false;
}

} // namespace aster
