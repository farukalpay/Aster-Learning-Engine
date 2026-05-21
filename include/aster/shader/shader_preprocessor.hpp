// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/shader/shader_library.hpp"
#include "aster/shader/shader_variant.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace aster {

enum class ShaderPreprocessDiagnosticKind {
  MissingInclude,
  CyclicInclude,
  IoError,
  InvalidDirective,
};

struct ShaderPreprocessDiagnostic {
  ShaderPreprocessDiagnosticKind kind = ShaderPreprocessDiagnosticKind::MissingInclude;
  std::string source_name;
  std::size_t line = 0u;
  std::string include_name;
  std::string message;
};

struct ShaderPreprocessDependency {
  std::string logical_path;
  std::filesystem::path resolved_path;
  std::uint64_t source_hash = 0u;
};

struct ShaderPreprocessRequest {
  std::string source_name = "<memory>";
  std::string source;
  std::vector<std::string> modules;
  std::vector<std::filesystem::path> include_roots;
  std::map<std::string, std::string> in_memory_sources;
  std::vector<ShaderPermutationDefine> defines;
  std::string backend_name = "software-reference";
};

struct ShaderPreprocessResult {
  bool success = false;
  std::string source;
  std::string lowered_source;
  std::vector<ShaderPreprocessDependency> dependencies;
  std::uint64_t source_hash = 0u;
  std::vector<ShaderPreprocessDiagnostic> diagnostics;
};

[[nodiscard]] std::string_view
shaderPreprocessDiagnosticKindName(ShaderPreprocessDiagnosticKind kind);
[[nodiscard]] ShaderPreprocessResult preprocessShaderSource(
    const ShaderLibrary &library, const ShaderPreprocessRequest &request);

} // namespace aster
