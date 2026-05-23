// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/asset/procedural_asset_graph.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct ProceduralNodeDescriptor {
  std::string kind;
  std::string output_role;
  std::string capability_status = "runtime-reference";
  std::vector<std::string> required_params;
};

class ProceduralNodeRegistry {
public:
  void registerNode(ProceduralNodeDescriptor descriptor);
  [[nodiscard]] const ProceduralNodeDescriptor *find(std::string_view kind) const noexcept;
  [[nodiscard]] const std::vector<ProceduralNodeDescriptor> &nodes() const noexcept;

private:
  std::vector<ProceduralNodeDescriptor> nodes_;
};

struct ProceduralGraphRuntimeDiagnostic {
  std::string severity;
  std::string node_id;
  std::string message;
};

struct ProceduralGraphNodeExecutionReport {
  std::string node_id;
  std::string kind;
  std::string status;
  std::size_t input_vertices = 0u;
  std::size_t output_vertices = 0u;
  std::size_t input_indices = 0u;
  std::size_t output_indices = 0u;
  std::size_t input_points = 0u;
  std::size_t output_points = 0u;
  std::vector<std::string> diagnostics;
};

struct ProceduralGraphEvaluationResult {
  std::string package_id;
  std::string stable_provenance_id;
  CpuMesh mesh;
  Material material;
  MaterialAuthoringGraph material_graph;
  std::uint32_t quality_score = 0u;
  bool production_ready = false;
  std::vector<ProceduralGraphNodeExecutionReport> execution_reports;
  std::vector<ProceduralGraphRuntimeDiagnostic> diagnostics;
};

[[nodiscard]] ProceduralNodeRegistry makeDefaultProceduralNodeRegistry();
[[nodiscard]] std::string stableProceduralGraphProvenanceId(
    const ProceduralAssetGraphPackage &package);
[[nodiscard]] ProceduralGraphEvaluationResult evaluateProceduralAssetGraph(
    const ProceduralAssetGraphPackage &package,
    const ProceduralNodeRegistry &registry = makeDefaultProceduralNodeRegistry());

} // namespace aster
