// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/material/material_asset.hpp"

#include <string>
#include <string_view>
#include <map>
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

struct MaterialAuthoringNode {
  std::string id;
  std::string label;
  std::string operation;
  std::string role;
  std::map<std::string, std::string> params;
  MaterialGraphOperation op = MaterialGraphOperation::Unknown;
  MaterialGraphValueType value_type = MaterialGraphValueType::Unknown;
  std::string capability_status;
  bool editable = true;
  bool persisted = true;
};

struct MaterialAuthoringEdge {
  std::string from;
  std::string to;
  std::string role;
};

struct MaterialAuthoringGraph {
  std::string source_id;
  std::string source_kind;
  std::vector<MaterialAuthoringNode> nodes;
  std::vector<MaterialAuthoringEdge> edges;
};

[[nodiscard]] std::string_view materialGraphOperationName(MaterialGraphOperation operation);
[[nodiscard]] std::string_view materialGraphValueTypeName(MaterialGraphValueType value_type);
[[nodiscard]] MaterialGraph materialGraphForAsset(const MaterialAsset &asset);
[[nodiscard]] MaterialAuthoringGraph materialAuthoringGraphForAsset(const MaterialAsset &asset);

} // namespace aster
