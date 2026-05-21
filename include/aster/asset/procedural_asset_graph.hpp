// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace aster {

struct ProceduralAssetGraphNode {
  std::string id;
  std::string kind;
  std::string role;
  std::string label;
  std::map<std::string, std::string> params;
  std::string capability_status;
};

struct ProceduralAssetGraphEdge {
  std::string from;
  std::string to;
  std::string role;
};

struct ProceduralAssetGraphMeshDescriptor {
  std::string primitive;
  std::string uv_policy;
  std::string tangent_policy;
  std::string collision_proxy;
  std::string lod_policy;
};

struct ProceduralAssetGraphQualityIssue {
  std::string severity;
  std::string category;
  std::string node;
  std::string message;
};

struct ProceduralAssetGraphQualityReport {
  std::uint32_t score = 0u;
  bool production_ready = false;
  std::vector<ProceduralAssetGraphQualityIssue> issues;
};

struct ProceduralAssetGraphProductionSession {
  std::string session_id;
  std::string graph_hash;
  std::string preview_artifact_hash;
  std::string quality_gate;
  std::vector<std::string> cook_steps;
};

struct ProceduralAssetGraphFactoryStageReport {
  std::string id;
  std::string kind;
  std::string status;
  std::vector<std::string> diagnostics;
};

struct ProceduralAssetGraphFactorySignalCoverage {
  std::string signal;
  float average = 0.0f;
  float coverage = 0.0f;
  std::string status;
};

struct ProceduralAssetGraphFactoryReport {
  std::string stable_recipe_hash;
  std::vector<ProceduralAssetGraphFactoryStageReport> stage_diagnostics;
  std::vector<ProceduralAssetGraphFactorySignalCoverage> surface_signal_coverage;
  std::map<std::string, std::string> collision_proxy_summary;
  std::vector<std::string> visual_brief_claims;
  std::vector<std::string> visual_brief_rejections;
};

struct ProceduralAssetGraphPackage {
  std::filesystem::path package_path;
  std::string asset_guid;
  std::string id;
  std::string name;
  std::filesystem::path source_path;
  std::string runtime_model;
  std::string shader_variant_tag;
  std::string pipeline_tag;
  std::uint64_t feature_mask = 0u;
  std::uint64_t shader_variant_key = 0u;
  std::uint64_t pipeline_key = 0u;
  MaterialAsset material;
  ProceduralAssetGraphMeshDescriptor mesh;
  std::vector<ProceduralAssetGraphNode> nodes;
  std::vector<ProceduralAssetGraphEdge> edges;
  ProceduralAssetGraphProductionSession production_session;
  ProceduralAssetGraphFactoryReport factory_report;
  ProceduralAssetGraphQualityReport quality;
  std::vector<MaterialDiagnostic> diagnostics;
};

[[nodiscard]] ProceduralAssetGraphPackage
loadProceduralAssetGraphPackage(const std::filesystem::path &path);

[[nodiscard]] Material proceduralAssetGraphMaterial(const ProceduralAssetGraphPackage &package);
[[nodiscard]] CpuMesh proceduralAssetGraphMesh(const ProceduralAssetGraphPackage &package);
[[nodiscard]] MaterialAuthoringGraph
materialAuthoringGraphForPackage(const ProceduralAssetGraphPackage &package);

} // namespace aster
