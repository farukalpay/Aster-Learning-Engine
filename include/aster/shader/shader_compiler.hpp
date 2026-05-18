// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/shader/shader_library.hpp"
#include "aster/shader/shader_reflection.hpp"

#include <string>
#include <vector>

namespace aster {

enum class ShaderBackend {
  MetalMSL,
  D3D12HLSL,
  SoftwareReference,
};

struct ShaderCompileRequest {
  ShaderBackend backend = ShaderBackend::SoftwareReference;
  ShaderVariantKey variant{};
  std::vector<std::string> modules;
  std::string entry_point = "fs_main";
};

struct ShaderCompileResult {
  bool success = false;
  std::string source;
  std::string entry_point;
  ShaderReflection reflection;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string_view shaderBackendName(ShaderBackend backend);
[[nodiscard]] ShaderCompileResult compileShaderVariant(const ShaderLibrary &library,
                                                       const ShaderCompileRequest &request);

} // namespace aster
