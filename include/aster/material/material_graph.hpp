// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class MaterialGraphValueType {
  Unknown,
  Float,
  Float2,
  Float3,
  Texture2D,
  MaterialLayer,
};

enum class MaterialGraphOperation {
  Unknown,
  TextureSample,
  TriplanarSample,
  HeightBlend,
  NormalMap,
  OrmUnpack,
  Wetness,
  Parallax,
  Output,
};

struct MaterialGraphNode {
  std::string id;
  std::string operation;
  std::vector<std::string> inputs;
  MaterialGraphOperation op = MaterialGraphOperation::Unknown;
  MaterialGraphValueType value_type = MaterialGraphValueType::Unknown;
};

struct MaterialGraph {
  std::vector<MaterialGraphNode> nodes;
};

[[nodiscard]] std::string_view materialGraphOperationName(MaterialGraphOperation operation);
[[nodiscard]] std::string_view materialGraphValueTypeName(MaterialGraphValueType value_type);
[[nodiscard]] MaterialGraph materialGraphForAsset(const MaterialAsset &asset);

} // namespace aster
