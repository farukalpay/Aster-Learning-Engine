// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/render/material_compiler.hpp"
#include "aster/shader/shader_reflection.hpp"

#include <string>
#include <vector>

namespace aster {

struct MaterialBinding {
  std::string name;
  ShaderResourceKind kind = ShaderResourceKind::UniformBuffer;
  std::uint32_t binding = 0u;
};

struct MaterialBindingLayout {
  std::vector<MaterialBinding> bindings;
};

struct CompiledMaterialAsset {
  MaterialAsset asset;
  CompiledMaterial fallback_material;
  ShaderVariantKey variant;
  MaterialGraph graph;
  MaterialBindingLayout binding_layout;
  std::vector<MaterialDiagnostic> diagnostics;
};

[[nodiscard]] MaterialBindingLayout bindingLayoutForMaterial(const MaterialAsset &asset);
[[nodiscard]] CompiledMaterialAsset compileMaterialAssetForRendering(const MaterialAsset &asset);

} // namespace aster
