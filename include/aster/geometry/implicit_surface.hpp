// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/mesh_modeling.hpp"
#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace aster {

struct SurfaceGridCoord {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct SurfaceExtractionSettings {
  float normal_probe_scale = 0.35f;
  float uv_scale = 0.12f;
  bool emit_collision_mesh = true;
  bool emit_channel_meshes = true;
  bool capture_faces = false;
  bool emit_partial_faces = true;
};

struct SurfaceVertex {
  Vec3 position{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  SurfaceGridCoord grid_coord{};
  std::uint64_t key = 0u;
};

struct SurfaceChannelSample {
  std::uint32_t channel = 0u;
  float strength = 0.0f;
};

struct SurfaceFace {
  std::array<SurfaceVertex, 4> corners{};
  std::array<std::uint64_t, 4> corner_keys{};
  int corner_count = 0;
  Vec3 preferred_normal{0.0f, 1.0f, 0.0f};
  Vec3 centroid{};
  SurfaceChannelSample channel{};
};

struct SurfaceMeshIndexCache {
  std::unordered_map<std::uint64_t, std::uint32_t> vertices;
};

struct SurfaceMeshBatch {
  std::uint32_t channel = 0u;
  CpuMesh mesh{};
};

struct SurfaceBatchStats {
  std::uint32_t channel = 0u;
  std::uint32_t vertices = 0u;
  std::uint32_t triangles = 0u;
};

struct SurfaceBuildStats {
  std::uint32_t active_cells = 0u;
  std::uint32_t faces = 0u;
  std::uint32_t partial_faces = 0u;
  std::uint32_t rejected_faces = 0u;
  std::uint32_t degenerate_faces = 0u;
  std::uint32_t surface_vertices = 0u;
  std::uint32_t surface_triangles = 0u;
  std::uint32_t material_batches = 0u;
  MeshBounds bounds{};
  MeshTopologyReport topology{};
  std::vector<SurfaceBatchStats> batches;
};

struct SurfaceExtractionResult {
  CpuMesh collision_mesh{};
  std::vector<SurfaceMeshBatch> mesh_batches;
  std::vector<SurfaceFace> faces;
  SurfaceBuildStats stats{};
};

enum class SurfaceBoundaryAxis {
  X,
  Y,
  Z,
};

struct SurfaceBoundarySignature {
  SurfaceBoundaryAxis axis = SurfaceBoundaryAxis::X;
  float value = 0.0f;
  float tolerance = 0.001f;
  std::vector<Vec3> vertices;
  std::size_t triangles_touching = 0u;
  MeshBounds bounds{};
};

struct SurfaceBoundaryCompatibilityReport {
  std::size_t lhs_vertices = 0u;
  std::size_t rhs_vertices = 0u;
  std::size_t unmatched_lhs_vertices = 0u;
  std::size_t unmatched_rhs_vertices = 0u;
  float max_delta = 0.0f;

  [[nodiscard]] bool compatible() const;
};

using SurfaceDensityFn = std::function<float(Vec3)>;
using SurfaceChannelFn = std::function<SurfaceChannelSample(const SurfaceFace &, float)>;
using SurfaceRejectFn = std::function<bool(const SurfaceFace &)>;

struct SurfaceExtractionSpec {
  Vec3 origin{};
  float cell_size = 1.0f;
  SurfaceGridCoord cell_count{1, 1, 1};
  SurfaceExtractionSettings settings{};
  SurfaceDensityFn density;
  SurfaceChannelFn channel;
  SurfaceRejectFn reject_face;
};

enum class ImplicitBooleanOperation {
  Union,
  Intersection,
  Difference,
};

class ImplicitSurfaceField {
public:
  using Sampler = std::function<float(Vec3)>;

  ImplicitSurfaceField() = default;
  explicit ImplicitSurfaceField(Sampler sampler);

  [[nodiscard]] float sample(Vec3 point) const;
  [[nodiscard]] explicit operator bool() const;

private:
  Sampler sampler_;
};

[[nodiscard]] ImplicitSurfaceField makeCapsuleField(Vec3 from, Vec3 to, float radius);
[[nodiscard]] ImplicitSurfaceField makeEllipsoidField(Vec3 center, Vec3 radii);
[[nodiscard]] ImplicitSurfaceField makePointRadiusField(Vec3 center, float radius);
[[nodiscard]] ImplicitSurfaceField makeOffsetField(ImplicitSurfaceField field, float offset);
[[nodiscard]] ImplicitSurfaceField
combineImplicitFields(std::vector<ImplicitSurfaceField> fields, ImplicitBooleanOperation operation);

[[nodiscard]] Vertex surfaceVertexForMesh(SurfaceVertex vertex, float uv_scale);
void appendSurfaceFace(CpuMesh &mesh, const SurfaceFace &face, float uv_scale,
                       SurfaceMeshIndexCache *cache = nullptr);
[[nodiscard]] SurfaceBoundarySignature
collectSurfaceBoundary(const CpuMesh &mesh, SurfaceBoundaryAxis axis, float value,
                       float tolerance = 0.001f);
[[nodiscard]] SurfaceBoundaryCompatibilityReport
compareSurfaceBoundaries(const SurfaceBoundarySignature &lhs, const SurfaceBoundarySignature &rhs,
                         float tolerance = 0.001f);
[[nodiscard]] SurfaceExtractionResult extractImplicitSurface(const SurfaceExtractionSpec &spec);

} // namespace aster
