// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/shader/shader_library.hpp"
#include "aster/shader/shader_preprocessor.hpp"
#include "aster/shader/shader_reflection.hpp"

#include <filesystem>
#include <map>
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
  StableShaderKey stable_key{};
  std::vector<std::string> modules;
  std::string source;
  std::vector<std::filesystem::path> include_roots;
  std::map<std::string, std::string> in_memory_sources;
  std::vector<ShaderPermutationDefine> permutation_defines;
  std::uint32_t quality_level = 2u;
  std::string entry_point = "fs_main";
};

struct ShaderCompileResult {
  bool success = false;
  std::string source;
  std::string entry_point;
  StableShaderKey stable_key{};
  ShaderReflection reflection;
  std::vector<ShaderPreprocessDependency> dependencies;
  std::uint64_t preprocessed_source_hash = 0u;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string_view shaderBackendName(ShaderBackend backend);
[[nodiscard]] std::uint32_t shaderBackendPermutationValue(ShaderBackend backend);
[[nodiscard]] ShaderCompileResult compileShaderVariant(const ShaderLibrary &library,
                                                       const ShaderCompileRequest &request);

} // namespace aster
