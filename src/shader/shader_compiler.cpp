// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/shader/shader_compiler.hpp"

#include <algorithm>
#include <string>

namespace aster {
namespace {

std::string backendPreamble(const ShaderBackend backend) {
  switch (backend) {
  case ShaderBackend::MetalMSL:
    return "#include <metal_stdlib>\nusing namespace metal;\n";
  case ShaderBackend::D3D12HLSL:
    return "// Aster generated HLSL\n";
  case ShaderBackend::SoftwareReference:
    return "// Aster software-reference shader source\n";
  }
  return {};
}

} // namespace

std::string_view shaderBackendName(const ShaderBackend backend) {
  switch (backend) {
  case ShaderBackend::MetalMSL:
    return "metal-msl";
  case ShaderBackend::D3D12HLSL:
    return "d3d12-hlsl";
  case ShaderBackend::SoftwareReference:
    return "software-reference";
  }
  return "software-reference";
}

std::uint32_t shaderBackendPermutationValue(const ShaderBackend backend) {
  switch (backend) {
  case ShaderBackend::SoftwareReference:
    return 0u;
  case ShaderBackend::MetalMSL:
    return 1u;
  case ShaderBackend::D3D12HLSL:
    return 2u;
  }
  return 0u;
}

ShaderCompileResult compileShaderVariant(const ShaderLibrary &library,
                                         const ShaderCompileRequest &request) {
  ShaderCompileResult result;
  result.entry_point = request.entry_point;
  result.reflection = reflectShaderVariantBindings(request.variant);
  result.stable_key = request.stable_key.hash != 0u
                          ? request.stable_key
                          : stableShaderKeyForVariant(
                                request.variant, shaderBackendName(request.backend),
                                shaderBackendPermutationValue(request.backend),
                                request.quality_level);

  std::vector<ShaderPermutationDefine> defines = result.stable_key.defines;
  defines.push_back({.name = "ASTER_FEATURE_MASK",
                     .value = std::to_string(request.variant.feature_mask) + "ull"});
  defines.push_back({.name = "ASTER_STABLE_SHADER_KEY",
                     .value = shaderStableHex(result.stable_key.hash) + "ull"});
  for (const ShaderPermutationDefine &define : request.permutation_defines) {
    const auto found =
        std::find_if(defines.begin(), defines.end(),
                     [&define](const ShaderPermutationDefine &candidate) {
                       return candidate.name == define.name;
                     });
    if (found == defines.end()) {
      defines.push_back(define);
    } else {
      found->value = define.value;
    }
  }

  ShaderPreprocessRequest preprocess_request;
  preprocess_request.source_name = "compile-root";
  preprocess_request.source = request.source;
  preprocess_request.modules = request.modules;
  preprocess_request.include_roots = request.include_roots;
  preprocess_request.in_memory_sources = request.in_memory_sources;
  preprocess_request.defines = std::move(defines);
  preprocess_request.backend_name = std::string(shaderBackendName(request.backend));
  const ShaderPreprocessResult preprocessed =
      preprocessShaderSource(library, preprocess_request);

  result.source += backendPreamble(request.backend);
  result.source += "// backend: ";
  result.source += shaderBackendName(request.backend);
  result.source += "\n// variant: ";
  result.source += request.variant.tag;
  result.source += "\n// stable-key: ";
  result.source += shaderStableHex(result.stable_key.hash);
  result.source += "\n";
  result.source += preprocessed.lowered_source;
  result.dependencies = preprocessed.dependencies;
  result.preprocessed_source_hash = preprocessed.source_hash;
  for (const ShaderPreprocessDiagnostic &diagnostic : preprocessed.diagnostics) {
    result.diagnostics.push_back(std::string(shaderPreprocessDiagnosticKindName(diagnostic.kind)) +
                                 ": " + diagnostic.message);
  }
  if (result.source.find(request.entry_point) == std::string::npos) {
    result.diagnostics.push_back("entry point '" + request.entry_point + "' was not found");
  }
  result.success = result.diagnostics.empty();
  return result;
}

} // namespace aster
