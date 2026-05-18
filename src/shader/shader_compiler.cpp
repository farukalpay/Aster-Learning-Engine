// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_compiler.hpp"

namespace aster {

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
  result.source += "// backend: ";
  result.source += shaderBackendName(request.backend);
  result.source += "\n// variant: ";
  result.source += request.variant.tag;
  result.source += "\n#define ASTER_FEATURE_MASK ";
  result.source += std::to_string(request.variant.feature_mask);
  result.source += "ull\n";
  result.source += library.compose(request.modules);
  if (result.source.find(request.entry_point) == std::string::npos) {
    result.diagnostics.push_back("entry point '" + request.entry_point + "' was not found");
  }
  result.success = result.diagnostics.empty();
  return result;
}

} // namespace aster
