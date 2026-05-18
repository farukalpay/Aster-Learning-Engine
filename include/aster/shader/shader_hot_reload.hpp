// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace aster {

struct ShaderHotReloadFileState {
  std::filesystem::file_time_type write_time{};
  std::uintmax_t size = 0u;
};

struct ShaderHotReloadState {
  std::map<std::filesystem::path, ShaderHotReloadFileState> files;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] ShaderHotReloadState snapshotShaderHotReloadInputs(
    const std::vector<std::filesystem::path> &paths);
[[nodiscard]] bool shaderHotReloadInputsChanged(const ShaderHotReloadState &before,
                                                const ShaderHotReloadState &after);

} // namespace aster
