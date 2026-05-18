// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_graph.hpp"

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

} // namespace aster
