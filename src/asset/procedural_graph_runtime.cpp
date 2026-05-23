// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/procedural_graph_runtime.hpp"

#include "aster/asset/asset_modifier_stack.hpp"
#include "aster/geometry/geometry_operations.hpp"
#include "aster/geometry/mesh_authoring.hpp"
#include "aster/geometry/mesh_modeling.hpp"
#include "aster/geometry/procedural_modeling.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <string>
#include <sstream>
#include <utility>

namespace aster {
namespace {

void appendHash(std::uint64_t &hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
}

[[nodiscard]] std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << "aster-graph-0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

[[nodiscard]] float nodeParamF32(const ProceduralAssetGraphNode &node,
                                 const std::string_view key, const float fallback) {
  const auto found = node.params.find(std::string(key));
  if (found == node.params.end()) {
    return fallback;
  }
  try {
    return std::stof(found->second);
  } catch (...) {
    return fallback;
  }
}

[[nodiscard]] std::uint32_t nodeParamU32(const ProceduralAssetGraphNode &node,
                                         const std::string_view key,
                                         const std::uint32_t fallback) {
  return static_cast<std::uint32_t>(std::max(0.0f, std::round(nodeParamF32(node, key, fallback))));
}

[[nodiscard]] float nodeParamF32Any(const ProceduralAssetGraphNode &node,
                                    const std::initializer_list<std::string_view> keys,
                                    const float fallback) {
  for (const std::string_view key : keys) {
    const auto found = node.params.find(std::string(key));
    if (found == node.params.end()) {
      continue;
    }
    try {
      return std::stof(found->second);
    } catch (...) {
      return fallback;
    }
  }
  return fallback;
}

[[nodiscard]] std::uint32_t nodeParamU32Any(const ProceduralAssetGraphNode &node,
                                            const std::initializer_list<std::string_view> keys,
                                            const std::uint32_t fallback) {
  return static_cast<std::uint32_t>(
      std::max(0.0f, std::round(nodeParamF32Any(node, keys, fallback))));
}

[[nodiscard]] std::string nodeParamString(const ProceduralAssetGraphNode &node,
                                          const std::string_view key,
                                          std::string fallback = {}) {
  const auto found = node.params.find(std::string(key));
  return found == node.params.end() ? std::move(fallback) : found->second;
}

[[nodiscard]] bool nodeParamBool(const ProceduralAssetGraphNode &node,
                                 const std::string_view key, const bool fallback) {
  const auto found = node.params.find(std::string(key));
  if (found == node.params.end()) {
    return fallback;
  }
  return found->second == "true" || found->second == "yes" || found->second == "1";
}

[[nodiscard]] Vec3 nodeParamVec3(const ProceduralAssetGraphNode &node,
                                 const std::string_view prefix, const Vec3 fallback) {
  return {nodeParamF32(node, std::string(prefix) + "_x", fallback.x),
          nodeParamF32(node, std::string(prefix) + "_y", fallback.y),
          nodeParamF32(node, std::string(prefix) + "_z", fallback.z)};
}

[[nodiscard]] std::string normalizedKind(std::string value) {
  std::replace(value.begin(), value.end(), '-', '_');
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

[[nodiscard]] bool capabilityIsDescriptorOnly(const std::string &status) {
  return status == "descriptor-only-reference" || status == "cook-reference";
}

[[nodiscard]] AssetModifierKind modifierKindForNode(const std::string &kind) {
  if (kind == "transform_geometry") {
    return AssetModifierKind::Transform;
  }
  if (kind == "merge_by_distance") {
    return AssetModifierKind::Weld;
  }
  if (kind == "triangulate") {
    return AssetModifierKind::Triangulate;
  }
  if (kind == "extrude_mesh") {
    return AssetModifierKind::Extrude;
  }
  if (kind == "bevel_modifier" || kind == "bevel") {
    return AssetModifierKind::Bevel;
  }
  if (kind == "seam_inset") {
    return AssetModifierKind::Inset;
  }
  if (kind == "contact_skirt") {
    return AssetModifierKind::Extrude;
  }
  if (kind == "array_modifier") {
    return AssetModifierKind::Array;
  }
  if (kind == "solidify_modifier") {
    return AssetModifierKind::Solidify;
  }
  if (kind == "displace_modifier") {
    return AssetModifierKind::Displace;
  }
  if (kind == "weighted_normal" || kind == "soft_rim_normals") {
    return AssetModifierKind::WeightedNormal;
  }
  if (kind == "smooth_modifier") {
    return AssetModifierKind::Smooth;
  }
  if (kind == "decimate_modifier") {
    return AssetModifierKind::Decimate;
  }
  return AssetModifierKind::Transform;
}

[[nodiscard]] bool isRuntimeModifierNode(const std::string &kind) {
  return kind == "transform_geometry" || kind == "merge_by_distance" || kind == "triangulate" ||
         kind == "extrude_mesh" || kind == "bevel_modifier" || kind == "bevel" ||
         kind == "seam_inset" || kind == "contact_skirt" ||
         kind == "array_modifier" || kind == "solidify_modifier" ||
         kind == "displace_modifier" || kind == "weighted_normal" ||
         kind == "soft_rim_normals" || kind == "smooth_modifier" ||
         kind == "decimate_modifier";
}

[[nodiscard]] AssetModifierDesc modifierFromNode(const ProceduralAssetGraphNode &node) {
  AssetModifierDesc desc;
  desc.id = node.id;
  desc.kind = modifierKindForNode(node.kind);
  desc.amount = nodeParamF32(node, "amount",
                             nodeParamF32(node, "width",
                                          desc.kind == AssetModifierKind::Decimate ? 0.72f
                                                                                  : 0.02f));
  desc.epsilon = nodeParamF32(node, "epsilon", nodeParamF32(node, "distance", 0.0001f));
  desc.seed = nodeParamU32(node, "seed", 0xA57E2026u);
  desc.count = std::max(1u, nodeParamU32(node, "count", nodeParamU32(node, "copies", 1u)));
  desc.transform.position = nodeParamVec3(node, "translate", {});
  desc.transform.scale = nodeParamVec3(node, "scale", {1.0f, 1.0f, 1.0f});
  desc.enabled = nodeParamBool(node, "enabled", true);
  if (!node.role.empty()) {
    desc.creative_variant_tags.push_back("graph-role:" + node.role);
  }
  return desc;
}

[[nodiscard]] bool isRuntimePrimitiveNode(const std::string &kind) {
  return kind == "mesh_primitive" || kind == "mesh_primitive_cube" ||
         kind == "mesh_primitive_grid" || kind == "mesh_primitive_uv_sphere" ||
         kind == "mesh_primitive_cylinder" || kind == "mesh_primitive_cone" ||
         kind == "grid_primitive" || kind == "uv_sphere" || kind == "cone_primitive" ||
         kind == "cylinder_cone" || kind == "curve_primitive_line" ||
         kind == "mesh_primitive_line";
}

[[nodiscard]] CpuMesh scaledBox(const Vec3 size) {
  return transformCpuMesh(makeBox(), {.scale = {std::max(size.x, 0.0001f),
                                                std::max(size.y, 0.0001f),
                                                std::max(size.z, 0.0001f)}});
}

[[nodiscard]] CpuMesh lineMeshFromNode(const ProceduralAssetGraphNode &node) {
  const Vec3 start = nodeParamVec3(node, "start", {});
  Vec3 end = nodeParamVec3(node, "end", {0.0f, 0.0f, 1.0f});
  const std::string mode = normalizedKind(nodeParamString(node, "mode", "points"));
  if (mode == "direction") {
    const Vec3 direction = normalizeOr(nodeParamVec3(node, "direction", {0.0f, 0.0f, 1.0f}),
                                       {0.0f, 0.0f, 1.0f});
    end = start + direction * nodeParamF32(node, "length", 1.0f);
  }
  const float half_width = std::max(0.0005f, nodeParamF32Any(node, {"half_width", "radius", "width"},
                                                            0.025f));
  return makeRibbonStrip({.centerline = {start, end},
                          .half_widths = {half_width, half_width},
                          .up = nodeParamVec3(node, "up", {0.0f, 1.0f, 0.0f}),
                          .thickness = nodeParamF32(node, "thickness", 0.0f)});
}

[[nodiscard]] CpuMesh primitiveMeshFromNode(const ProceduralAssetGraphNode &node) {
  const std::string kind = normalizedKind(node.kind);
  const std::string primitive = normalizedKind(nodeParamString(node, "primitive", kind));
  if (kind == "mesh_primitive_cube" || primitive == "cube" || primitive == "box") {
    const float size = nodeParamF32(node, "size", 1.0f);
    return scaledBox(nodeParamVec3(node, "size", {size, size, size}));
  }
  if (kind == "mesh_primitive_grid" || kind == "grid_primitive" || primitive == "grid" ||
      primitive == "grid_plane" || primitive == "plane") {
    return makeGridMesh({.width = nodeParamF32Any(node, {"width", "size_x", "size"}, 2.0f),
                         .depth = nodeParamF32Any(node, {"depth", "size_y", "size"}, 2.0f),
                         .columns = static_cast<int>(
                             std::max(1u, nodeParamU32Any(node, {"columns", "vertices_x"}, 4u))),
                         .rows = static_cast<int>(
                             std::max(1u, nodeParamU32Any(node, {"rows", "vertices_y"}, 4u)))});
  }
  if (kind == "mesh_primitive_uv_sphere" || kind == "uv_sphere" || primitive == "uv_sphere" ||
      primitive == "sphere") {
    return makeUvSphere(static_cast<int>(
                            std::max(3u, nodeParamU32Any(node, {"segments", "vertices"}, 32u))),
                        static_cast<int>(std::max(2u, nodeParamU32(node, "rings", 16u))),
                        nodeParamF32(node, "radius", 1.0f));
  }
  if (kind == "mesh_primitive_cylinder" || primitive == "cylinder") {
    const float radius = nodeParamF32(node, "radius", 0.5f);
    return makeCylinderConeMesh(
        {.radius_top = radius,
         .radius_bottom = radius,
         .depth = nodeParamF32Any(node, {"depth", "height"}, 1.0f),
         .radial_segments = static_cast<int>(
             std::max(3u, nodeParamU32Any(node, {"radial_segments", "vertices"}, 32u))),
         .side_segments = static_cast<int>(std::max(1u, nodeParamU32(node, "side_segments", 1u))),
         .fill_caps = nodeParamBool(node, "fill_caps", true)});
  }
  if (kind == "mesh_primitive_cone" || kind == "cone_primitive" || kind == "cylinder_cone" ||
      primitive == "cone" || primitive == "cylinder_cone") {
    const float radius = nodeParamF32(node, "radius", 0.5f);
    return makeCylinderConeMesh(
        {.radius_top = nodeParamF32(node, "radius_top", primitive == "cone" ? 0.0f : radius),
         .radius_bottom = nodeParamF32(node, "radius_bottom", radius),
         .depth = nodeParamF32Any(node, {"depth", "height"}, 1.0f),
         .radial_segments = static_cast<int>(
             std::max(3u, nodeParamU32Any(node, {"radial_segments", "vertices"}, 32u))),
         .side_segments = static_cast<int>(std::max(1u, nodeParamU32(node, "side_segments", 1u))),
         .fill_caps = nodeParamBool(node, "fill_caps", true)});
  }
  if (kind == "curve_primitive_line" || kind == "mesh_primitive_line" || primitive == "line") {
    return lineMeshFromNode(node);
  }
  return makeRock(18, 10, 1.0f);
}

[[nodiscard]] CpuMesh boundingBoxMesh(const CpuMesh &mesh) {
  const MeshBounds bounds = calculateMeshBounds(mesh);
  if (!bounds.valid) {
    return {};
  }
  const Vec3 size = {std::max(bounds.max.x - bounds.min.x, 0.0001f),
                     std::max(bounds.max.y - bounds.min.y, 0.0001f),
                     std::max(bounds.max.z - bounds.min.z, 0.0001f)};
  CpuMesh box = scaledBox(size);
  return transformCpuMesh(box, {.position = meshBoundsCenter(bounds)});
}

void flipCpuMeshFaces(CpuMesh &mesh) {
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    std::swap(mesh.indices[i + 1u], mesh.indices[i + 2u]);
  }
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = vertex.normal * -1.0f;
    vertex.tangent.w *= -1.0f;
  }
}

void appendRuntimeDiagnostic(ProceduralGraphEvaluationResult &result,
                             ProceduralGraphNodeExecutionReport &report,
                             std::string message) {
  report.diagnostics.push_back(message);
  result.diagnostics.push_back({"warning", report.node_id, std::move(message)});
}

void applyRuntimeGraphNodes(ProceduralGraphEvaluationResult &result,
                            const ProceduralAssetGraphPackage &package,
                            const ProceduralNodeRegistry &registry) {
  std::vector<GeometryPoint> scattered_points;

  for (const ProceduralAssetGraphNode &node : package.nodes) {
    const std::string kind = normalizedKind(node.kind);
    const ProceduralNodeDescriptor *descriptor = registry.find(kind);
    ProceduralGraphNodeExecutionReport report;
    report.node_id = node.id;
    report.kind = kind;
    report.status = descriptor == nullptr ? "unregistered" : "registered-reference";
    report.input_vertices = result.mesh.vertices.size();
    report.input_indices = result.mesh.indices.size();
    report.input_points = scattered_points.size();

    const auto finish_report = [&]() {
      report.output_vertices = result.mesh.vertices.size();
      report.output_indices = result.mesh.indices.size();
      report.output_points = scattered_points.size();
      result.execution_reports.push_back(std::move(report));
    };

    if (!nodeParamBool(node, "enabled", true)) {
      report.status = "disabled";
      finish_report();
      continue;
    }
    if (descriptor != nullptr && capabilityIsDescriptorOnly(descriptor->capability_status)) {
      report.status = "descriptor-only";
      finish_report();
      continue;
    }
    if (isRuntimePrimitiveNode(kind)) {
      result.mesh = primitiveMeshFromNode(node);
      report.status = kind == "curve_primitive_line" ? "executed-mesh-approximation"
                                                     : "executed";
      finish_report();
      continue;
    }
    if (isRuntimeModifierNode(kind)) {
      AssetModifierStack stack;
      stack.asset_id = package.id;
      stack.source_provenance_id = result.stable_provenance_id;
      stack.modifiers.push_back(modifierFromNode({.id = node.id,
                                                  .kind = kind,
                                                  .role = node.role,
                                                  .label = node.label,
                                                  .params = node.params,
                                                  .capability_status =
                                                      node.capability_status}));
      const AssetModifierStackResult modified = applyAssetModifierStack(result.mesh, stack);
      result.mesh = modified.mesh;
      result.quality_score = std::min(result.quality_score, modified.report.quality_score);
      report.status = "executed";
      report.diagnostics.insert(report.diagnostics.end(), modified.report.diagnostics.begin(),
                                modified.report.diagnostics.end());
      for (const std::string &diagnostic : modified.report.diagnostics) {
        result.diagnostics.push_back({"warning", node.id, diagnostic});
      }
      finish_report();
      continue;
    }
    if (kind == "mesh_to_points") {
      scattered_points = meshToPoints(result.mesh);
      report.status = "executed";
      finish_report();
      continue;
    }
    if (kind == "scatter_points" || kind == "distribute_points_on_faces") {
      scattered_points = scatterPointsOnMesh(
          result.mesh, {.count = nodeParamU32(node, "count", 16u),
                        .seed = nodeParamU32(node, "seed", 1u),
                        .radius = nodeParamF32(node, "radius", 1.0f)});
      report.status = "executed";
      finish_report();
      continue;
    }
    if (kind == "instance_on_points") {
      if (!scattered_points.empty()) {
        GeometrySet instances = instanceMeshOnPoints(result.mesh, scattered_points, node.id);
        result.mesh = joinGeometryMeshes(instances);
        report.status = "executed";
      } else {
        report.status = "skipped";
        appendRuntimeDiagnostic(result, report, "instance_on_points had no point input");
      }
      finish_report();
      continue;
    }
    if (kind == "join_geometry") {
      const std::uint32_t copies = std::max(1u, nodeParamU32(node, "copies", 1u));
      if (copies > 1u) {
        GeometrySet set;
        set.meshes.reserve(copies);
        for (std::uint32_t copy = 0u; copy < copies; ++copy) {
          GeometryMeshPart part;
          part.id = node.id + "." + std::to_string(copy);
          part.mesh = result.mesh;
          part.transform.position = {nodeParamF32(node, "spacing", 1.0f) *
                                         static_cast<float>(copy),
                                     0.0f, 0.0f};
          set.meshes.push_back(std::move(part));
        }
        result.mesh = joinGeometryMeshes(set);
      }
      report.status = "executed";
      finish_report();
      continue;
    }
    if (kind == "realize_instances") {
      report.status = "executed";
      finish_report();
      continue;
    }
    if (kind == "separate_geometry") {
      const std::vector<CpuMesh> islands = separateDisconnectedMeshIslands(result.mesh);
      GeometrySet set;
      set.meshes.reserve(islands.size());
      for (std::size_t i = 0u; i < islands.size(); ++i) {
        set.meshes.push_back({.id = node.id + "." + std::to_string(i),
                              .mesh = islands[i],
                              .source_index = static_cast<std::uint32_t>(i)});
      }
      if (!set.meshes.empty()) {
        result.mesh = joinGeometryMeshes(set);
      }
      report.status = "executed";
      report.diagnostics.push_back("islands=" + std::to_string(islands.size()));
      finish_report();
      continue;
    }
    if (kind == "bounding_box") {
      CpuMesh bounds = boundingBoxMesh(result.mesh);
      if (bounds.vertices.empty()) {
        report.status = "skipped";
        appendRuntimeDiagnostic(result, report, "bounding_box could not compute valid bounds");
      } else {
        result.mesh = std::move(bounds);
        report.status = "executed";
      }
      finish_report();
      continue;
    }
    if (kind == "flip_faces") {
      flipCpuMeshFaces(result.mesh);
      report.status = "executed";
      finish_report();
      continue;
    }
    if (kind == "uv_pack" || kind == "uv_pack_islands") {
      MeshAuthoringReport op_report;
      EditableMesh editable = packEditableMeshUvIslands(
          editableMeshFromCpuMesh(result.mesh, result.stable_provenance_id),
          {.padding = nodeParamF32(node, "padding", 0.02f),
           .normalize_to_unit_square = nodeParamBool(node, "normalize", true),
           .preserve_aspect = nodeParamBool(node, "preserve_aspect", true),
           .channel = nodeParamString(node, "channel", "uv0")},
          &op_report);
      result.mesh = cpuMeshFromEditableMesh(editable);
      result.quality_score = std::min(result.quality_score, op_report.quality_score);
      report.status = "executed";
      for (const MeshAuthoringIssue &issue : op_report.issues) {
        report.diagnostics.push_back(issue.severity + ":" + issue.operation + ":" +
                                     issue.message);
      }
      finish_report();
      continue;
    }
    if (kind == "set_material" || kind == "set_material_index" ||
        kind == "material_assignment") {
      const std::string material_id =
          nodeParamString(node, "material", nodeParamString(node, "material_id"));
      if (!material_id.empty()) {
        result.material.asset_id = material_id;
      }
      report.status = "metadata";
      finish_report();
      continue;
    }
    if (descriptor != nullptr) {
      finish_report();
    }
  }
}

void registerRuntimeGeometryNodeAliases(ProceduralNodeRegistry &registry) {
  registry.registerNode({"mesh_primitive_cube", "mesh", "runtime-reference", {}});
  registry.registerNode({"mesh_primitive_grid", "mesh", "runtime-reference", {}});
  registry.registerNode({"mesh_primitive_uv_sphere", "mesh", "runtime-reference", {}});
  registry.registerNode({"mesh_primitive_cylinder", "mesh", "runtime-reference", {}});
  registry.registerNode({"mesh_primitive_cone", "mesh", "runtime-reference", {}});
  registry.registerNode({"mesh_primitive_line", "mesh", "runtime-reference", {}});
  registry.registerNode({"curve_primitive_line", "mesh", "runtime-reference", {}});
  registry.registerNode({"bounding_box", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"mesh_to_points", "point-cloud", "runtime-reference", {}});
  registry.registerNode({"distribute_points_on_faces", "point-cloud", "runtime-reference", {}});
  registry.registerNode({"flip_faces", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"uv_pack_islands", "mesh-policy", "runtime-reference", {}});
  registry.registerNode({"set_material", "material", "runtime-procedural-reference", {}});
  registry.registerNode({"set_material_index", "material", "runtime-procedural-reference", {}});
}

void registerDescriptorOnlyGeometryNodes(ProceduralNodeRegistry &registry) {
  const std::vector<std::string> descriptor_only{
      "attribute_statistic",
      "attribute_capture",
      "evaluate_on_domain",
      "evaluate_at_index",
      "field_average",
      "field_min_and_max",
      "field_variance",
      "accumulate_field",
      "field_to_list",
      "field_to_grid",
      "grid_curl",
      "grid_gradient",
      "grid_laplacian",
      "grid_mean",
      "grid_median",
      "grid_dilate_erode",
      "grid_advect",
      "grid_clip",
      "grid_prune",
      "grid_info",
      "grid_to_mesh",
      "grid_to_points",
      "grid_voxelize",
      "sdf_grid_boolean",
      "sdf_grid_offset",
      "sdf_grid_fillet",
      "sdf_grid_laplacian",
      "sdf_grid_mean",
      "sdf_grid_median",
      "sdf_grid_mean_curvature",
      "mesh_to_volume",
      "volume_to_mesh",
      "volume_cube",
      "points_to_volume",
      "points_to_sdf_grid",
      "mesh_to_sdf_grid",
      "mesh_to_density_grid",
      "raycast",
      "sample_nearest",
      "sample_nearest_surface",
      "sample_index",
      "sample_uv_surface",
      "sample_grid",
      "sample_grid_index",
      "subdivision_surface",
      "convex_hull",
      "boolean",
      "carve",
      "fracture",
      "mesh_boolean",
      "dual_mesh",
      "mesh_subdivide",
      "mesh_to_curve",
      "curve_to_mesh",
      "curve_to_points",
      "points_to_curves",
      "curve_fill",
      "curve_sample",
      "curve_trim",
      "curve_length",
      "curve_reverse",
      "curve_resample",
      "curve_subdivide",
      "curve_fillet",
      "curve_primitive_circle",
      "curve_primitive_arc",
      "curve_primitive_spiral",
      "curve_primitive_star",
      "curve_primitive_quadrilateral",
      "curve_primitive_bezier_segment",
      "curve_primitive_quadratic_bezier",
      "string_to_curves",
      "gizmo_transform",
      "gizmo_linear",
      "gizmo_dial",
      "viewer",
      "tool_selection",
      "tool_set_selection",
      "tool_face_set",
      "tool_set_face_set",
      "tool_3d_cursor",
      "tool_active_element",
      "object_info",
      "collection_info",
      "collection_children",
      "self_object",
      "camera_info",
      "viewport_transform",
      "is_viewport",
      "import_obj",
      "import_ply",
      "import_stl",
      "import_vdb",
      "import_csv",
      "import_text",
      "image",
      "image_texture",
      "image_info",
      "sample_sound_frequencies",
      "xpbd_solver"};
  for (const std::string &kind : descriptor_only) {
    registry.registerNode({kind, "geometry-descriptor", "descriptor-only-reference", {}});
  }
}

} // namespace

void ProceduralNodeRegistry::registerNode(ProceduralNodeDescriptor descriptor) {
  const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                  [&](const ProceduralNodeDescriptor &node) {
                                    return node.kind == descriptor.kind;
                                  });
  if (found != nodes_.end()) {
    *found = std::move(descriptor);
    return;
  }
  nodes_.push_back(std::move(descriptor));
}

const ProceduralNodeDescriptor *ProceduralNodeRegistry::find(
    const std::string_view kind) const noexcept {
  const auto found = std::find_if(nodes_.begin(), nodes_.end(),
                                  [&](const ProceduralNodeDescriptor &node) {
                                    return node.kind == kind;
                                  });
  return found == nodes_.end() ? nullptr : &*found;
}

const std::vector<ProceduralNodeDescriptor> &ProceduralNodeRegistry::nodes() const noexcept {
  return nodes_;
}

ProceduralNodeRegistry makeDefaultProceduralNodeRegistry() {
  ProceduralNodeRegistry registry;
  registry.registerNode({"mesh_primitive", "mesh", "runtime-reference", {"primitive"}});
  registry.registerNode({"grid_primitive", "mesh", "runtime-reference", {}});
  registry.registerNode({"uv_sphere", "mesh", "runtime-reference", {}});
  registry.registerNode({"cone_primitive", "mesh", "runtime-reference", {}});
  registry.registerNode({"cylinder_cone", "mesh", "runtime-reference", {}});
  registry.registerNode({"pipe_body", "mesh", "runtime-procedural-reference", {"primitive"}});
  registry.registerNode({"factory_recipe", "asset-foundry", "runtime-procedural-reference", {"target"}});
  registry.registerNode({"foundry_recipe", "asset-foundry", "runtime-procedural-reference", {"target"}});
  registry.registerNode({"factory_stage", "asset-foundry-stage", "runtime-procedural-reference", {"kind"}});
  registry.registerNode({"foundry_stage", "asset-foundry-stage", "runtime-procedural-reference", {"kind"}});
  registry.registerNode({"surface_contract", "asset-foundry-surface", "runtime-procedural-reference", {}});
  registry.registerNode({"foundry_surface_contract", "asset-foundry-surface", "runtime-procedural-reference", {}});
  registry.registerNode({"physics_proxy", "asset-foundry-physics", "runtime-procedural-reference", {"shape"}});
  registry.registerNode({"foundry_physics_proxy", "asset-foundry-physics", "runtime-procedural-reference", {"shape"}});
  registry.registerNode({"lod_recipe", "asset-foundry-lod", "runtime-procedural-reference", {"levels"}});
  registry.registerNode({"foundry_lod_recipe", "asset-foundry-lod", "runtime-procedural-reference", {"levels"}});
  registry.registerNode({"quality_signal", "asset-foundry-quality", "runtime-procedural-reference", {"signal"}});
  registry.registerNode(
      {"visual_brief_claim", "asset-foundry-quality", "runtime-procedural-reference", {"signal"}});
  registry.registerNode({"visual_brief_rejection", "asset-foundry-quality",
                         "runtime-procedural-reference", {"signal"}});
  registry.registerNode({"transform_geometry", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"join_geometry", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"separate_geometry", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"realize_instances", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"scatter_points", "point-cloud", "runtime-reference", {}});
  registry.registerNode({"instance_on_points", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"merge_by_distance", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"triangulate", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"extrude_mesh", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"bevel_modifier", "mesh-operator", "runtime-procedural-reference", {}});
  registry.registerNode({"bevel", "mesh-operator", "descriptor-only-reference", {}});
  registry.registerNode({"seam_inset", "mesh-operator", "runtime-procedural-reference", {}});
  registry.registerNode({"contact_skirt", "mesh-operator", "runtime-procedural-reference", {}});
  registry.registerNode({"array_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"solidify_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"displace_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"weighted_normal", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"smooth_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"decimate_modifier", "mesh-operator", "runtime-reference", {}});
  registry.registerNode({"material", "material", "runtime-reference", {}});
  registry.registerNode({"material_assignment", "material", "runtime-procedural-reference", {}});
  registry.registerNode({"layered_corrosion", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"texture_sample", "material-layer", "runtime-reference", {"role"}});
  registry.registerNode({"triplanar", "material-layer", "runtime-reference", {}});
  registry.registerNode({"height_blend", "material-layer", "runtime-reference", {}});
  registry.registerNode({"normal_map", "material-layer", "runtime-reference", {}});
  registry.registerNode({"normal_height", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"rust_mask", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"wetness_flow", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"edge_wear", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"cavity_dirt", "material-layer", "runtime-procedural-reference", {}});
  registry.registerNode({"weld_seam", "mesh-detail", "runtime-procedural-reference", {}});
  registry.registerNode({"depth_bias_policy", "render-policy", "runtime-procedural-reference", {}});
  registry.registerNode({"baked_mask_preview", "quality", "runtime-procedural-reference", {}});
  registry.registerNode({"wetness", "material-layer", "runtime-reference", {}});
  registry.registerNode({"uv_policy", "mesh-policy", "cook-reference", {}});
  registry.registerNode({"uv_pack", "mesh-policy", "runtime-reference", {}});
  registry.registerNode({"attribute_transfer", "mesh-policy", "descriptor-only-reference", {}});
  registry.registerNode({"tangent_validation", "mesh-policy", "cook-reference", {}});
  registry.registerNode({"lod_policy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"lod_generator", "runtime-policy", "runtime-procedural-reference", {}});
  registry.registerNode({"collision_proxy", "runtime-policy", "cook-reference", {}});
  registry.registerNode({"mesh_boolean", "mesh-operator", "descriptor-only-reference", {}});
  registry.registerNode({"creative_variant", "variant", "runtime-reference", {}});
  registry.registerNode({"prefab_variant", "variant", "runtime-procedural-reference", {}});
  registry.registerNode({"cook_export", "package", "runtime-procedural-reference", {}});
  registry.registerNode({"diagnostic", "quality", "runtime-procedural-reference", {}});
  registry.registerNode({"provenance", "quality", "runtime-reference", {}});
  registry.registerNode({"anatomy_landmark", "biology", "runtime-procedural-reference", {}});
  registry.registerNode({"follicle_distribution", "biology", "runtime-procedural-reference", {}});
  registry.registerNode({"epidermal_strata", "biology", "runtime-procedural-reference", {}});
  registerRuntimeGeometryNodeAliases(registry);
  registerDescriptorOnlyGeometryNodes(registry);
  return registry;
}

std::string stableProceduralGraphProvenanceId(const ProceduralAssetGraphPackage &package) {
  std::uint64_t hash = 1469598103934665603ull;
  appendHash(hash, package.asset_guid);
  appendHash(hash, package.id);
  appendHash(hash, package.runtime_model);
  appendHash(hash, package.shader_variant_tag);
  appendHash(hash, package.pipeline_tag);
  for (const ProceduralAssetGraphNode &node : package.nodes) {
    appendHash(hash, node.id);
    appendHash(hash, node.kind);
    appendHash(hash, node.role);
  }
  for (const ProceduralAssetGraphEdge &edge : package.edges) {
    appendHash(hash, edge.from);
    appendHash(hash, edge.to);
    appendHash(hash, edge.role);
  }
  return hex64(hash);
}

ProceduralGraphEvaluationResult evaluateProceduralAssetGraph(
    const ProceduralAssetGraphPackage &package, const ProceduralNodeRegistry &registry) {
  ProceduralGraphEvaluationResult result;
  result.package_id = package.id;
  result.stable_provenance_id = stableProceduralGraphProvenanceId(package);
  result.quality_score = package.quality.score;
  result.production_ready = package.quality.production_ready;

  for (const ProceduralAssetGraphNode &node : package.nodes) {
    const ProceduralNodeDescriptor *descriptor = registry.find(node.kind);
    if (descriptor == nullptr) {
      result.diagnostics.push_back({"warning", node.id,
                                    "node kind is not registered in Aster runtime"});
      result.quality_score = std::min(result.quality_score, 72u);
      continue;
    }
    for (const std::string &param : descriptor->required_params) {
      if (node.params.find(param) == node.params.end()) {
        result.diagnostics.push_back({"error", node.id,
                                      "required node parameter is missing: " + param});
        result.production_ready = false;
        result.quality_score = 0u;
      }
    }
  }
  for (const ProceduralAssetGraphQualityIssue &issue : package.quality.issues) {
    result.diagnostics.push_back({issue.severity, issue.node, issue.message});
  }

  result.material = proceduralAssetGraphMaterial(package);
  result.material_graph = materialAuthoringGraphForPackage(package);
  result.mesh = proceduralAssetGraphMesh(package);
  applyRuntimeGraphNodes(result, package, registry);
  return result;
}

} // namespace aster
