// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/material/material_graph.hpp"

#include <utility>

namespace aster {
namespace {

MaterialGraphOperation operationForName(const std::string &name) {
  if (name == "sample" || name == "texture_sample") {
    return MaterialGraphOperation::TextureSample;
  }
  if (name == "triplanar") {
    return MaterialGraphOperation::TriplanarSample;
  }
  if (name == "height_blend") {
    return MaterialGraphOperation::HeightBlend;
  }
  if (name == "normal_map") {
    return MaterialGraphOperation::NormalMap;
  }
  if (name == "orm" || name == "orm_unpack") {
    return MaterialGraphOperation::OrmUnpack;
  }
  if (name == "wetness") {
    return MaterialGraphOperation::Wetness;
  }
  if (name == "parallax") {
    return MaterialGraphOperation::Parallax;
  }
  if (name == "output") {
    return MaterialGraphOperation::Output;
  }
  return MaterialGraphOperation::Unknown;
}

MaterialGraphValueType valueTypeForOperation(const MaterialGraphOperation operation) {
  switch (operation) {
  case MaterialGraphOperation::TextureSample:
  case MaterialGraphOperation::TriplanarSample:
  case MaterialGraphOperation::HeightBlend:
  case MaterialGraphOperation::NormalMap:
  case MaterialGraphOperation::Wetness:
  case MaterialGraphOperation::Parallax:
  case MaterialGraphOperation::Output:
    return MaterialGraphValueType::MaterialLayer;
  case MaterialGraphOperation::OrmUnpack:
    return MaterialGraphValueType::Float3;
  case MaterialGraphOperation::Unknown:
  default:
    return MaterialGraphValueType::Unknown;
  }
}

} // namespace

std::string_view materialGraphOperationName(const MaterialGraphOperation operation) {
  switch (operation) {
  case MaterialGraphOperation::TextureSample:
    return "texture-sample";
  case MaterialGraphOperation::TriplanarSample:
    return "triplanar-sample";
  case MaterialGraphOperation::HeightBlend:
    return "height-blend";
  case MaterialGraphOperation::NormalMap:
    return "normal-map";
  case MaterialGraphOperation::OrmUnpack:
    return "orm-unpack";
  case MaterialGraphOperation::Wetness:
    return "wetness";
  case MaterialGraphOperation::Parallax:
    return "parallax";
  case MaterialGraphOperation::Output:
    return "output";
  case MaterialGraphOperation::Unknown:
  default:
    return "unknown";
  }
}

std::string_view materialGraphValueTypeName(const MaterialGraphValueType value_type) {
  switch (value_type) {
  case MaterialGraphValueType::Float:
    return "float";
  case MaterialGraphValueType::Float2:
    return "float2";
  case MaterialGraphValueType::Float3:
    return "float3";
  case MaterialGraphValueType::Texture2D:
    return "texture2d";
  case MaterialGraphValueType::MaterialLayer:
    return "material-layer";
  case MaterialGraphValueType::Unknown:
  default:
    return "unknown";
  }
}

MaterialGraph materialGraphForAsset(const MaterialAsset &asset) {
  MaterialGraph graph;
  graph.nodes.reserve(asset.layers.size());
  for (const MaterialLayerExpression &layer : asset.layers) {
    const MaterialGraphOperation operation = operationForName(layer.operation);
    graph.nodes.push_back({.id = layer.name,
                           .operation = layer.operation,
                           .inputs = layer.arguments,
                           .op = operation,
                           .value_type = valueTypeForOperation(operation)});
  }
  return graph;
}

MaterialAuthoringGraph materialAuthoringGraphForAsset(const MaterialAsset &asset) {
  MaterialAuthoringGraph graph;
  graph.source_id = asset.id;
  graph.source_kind = "astermat";
  graph.nodes.reserve(asset.layers.size() + asset.textures.size() + 1u);
  for (const auto &[role, texture] : asset.textures) {
    (void)texture;
    graph.nodes.push_back({.id = "texture." + role,
                           .label = role,
                           .operation = "texture",
                           .role = role,
                           .op = MaterialGraphOperation::TextureSample,
                           .value_type = MaterialGraphValueType::Texture2D,
                           .capability_status = "source-texture"});
  }
  for (const MaterialLayerExpression &layer : asset.layers) {
    const MaterialGraphOperation operation = operationForName(layer.operation);
    MaterialAuthoringNode node{.id = "layer." + layer.name,
                               .label = layer.name,
                               .operation = layer.operation,
                               .role = "material-layer",
                               .op = operation,
                               .value_type = valueTypeForOperation(operation),
                               .capability_status = operation == MaterialGraphOperation::Unknown
                                                        ? "unsupported"
                                                        : "runtime-reference"};
    node.params["raw"] = layer.raw;
    for (std::size_t i = 0u; i < layer.arguments.size(); ++i) {
      node.params["arg" + std::to_string(i)] = layer.arguments[i];
      if (asset.textures.find(layer.arguments[i]) != asset.textures.end()) {
        graph.edges.push_back(
            {.from = "texture." + layer.arguments[i], .to = node.id, .role = "input"});
      }
    }
    graph.nodes.push_back(std::move(node));
  }
  if (!asset.layers.empty()) {
    graph.nodes.push_back({.id = "output.surface",
                           .label = "surface",
                           .operation = "output",
                           .role = "surface",
                           .op = MaterialGraphOperation::Output,
                           .value_type = MaterialGraphValueType::MaterialLayer,
                           .capability_status = "runtime-reference"});
    graph.edges.push_back(
        {.from = "layer." + asset.layers.back().name, .to = "output.surface", .role = "surface"});
  }
  return graph;
}

} // namespace aster
