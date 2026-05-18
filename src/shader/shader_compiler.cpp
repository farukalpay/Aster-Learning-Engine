// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_compiler.hpp"

#include <string>

namespace aster {
namespace {

void replaceAll(std::string &text, const std::string &from, const std::string &to) {
  if (from.empty()) {
    return;
  }
  std::size_t cursor = 0u;
  while ((cursor = text.find(from, cursor)) != std::string::npos) {
    text.replace(cursor, from.size(), to);
    cursor += to.size();
  }
}

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

std::string lowerAstslForBackend(std::string source, const ShaderBackend backend) {
  if (backend == ShaderBackend::D3D12HLSL) {
    replaceAll(source, "mix(", "lerp(");
  }
  return source;
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

ShaderCompileResult compileShaderVariant(const ShaderLibrary &library,
                                         const ShaderCompileRequest &request) {
  ShaderCompileResult result;
  result.entry_point = request.entry_point;
  result.reflection = reflectShaderVariantBindings(request.variant);
  result.source += backendPreamble(request.backend);
  result.source += "// backend: ";
  result.source += shaderBackendName(request.backend);
  result.source += "\n// variant: ";
  result.source += request.variant.tag;
  result.source += "\n#define ASTER_FEATURE_MASK ";
  result.source += std::to_string(request.variant.feature_mask);
  result.source += "ull\n";
  result.source += lowerAstslForBackend(library.compose(request.modules), request.backend);
  if (result.source.find(request.entry_point) == std::string::npos) {
    result.diagnostics.push_back("entry point '" + request.entry_point + "' was not found");
  }
  result.success = result.diagnostics.empty();
  return result;
}

} // namespace aster
