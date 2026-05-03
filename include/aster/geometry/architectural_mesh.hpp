// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/brush_level_mesh.hpp"

#include <vector>

namespace aster {

struct ArchitecturalMass {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  Vec3 rotation{};
  float chamfer = 0.0f;
};

struct ArchitecturalOpening {
  Vec3 center{};
  Vec3 half_extents{0.25f, 0.5f, 0.25f};
  Vec3 rotation{};
  float chamfer = 0.0f;
};

struct ArchitecturalMeshSpec {
  std::vector<ArchitecturalMass> masses;
  std::vector<ArchitecturalOpening> openings;
  BrushLevelMeshOptions mesh_options{};
};

struct GatehouseMeshSpec {
  float wall_half_width = 1.72f;
  float wall_height = 2.35f;
  float tower_half_width = 0.46f;
  float tower_height = 3.16f;
  float depth = 0.78f;
  float door_half_width = 0.52f;
  float door_height = 1.52f;
  int parapet_blocks = 5;
};

struct FacadeMeshSpec {
  float half_width = 0.72f;
  float height = 2.42f;
  float depth = 0.56f;
  int window_columns = 1;
  int window_rows = 3;
  bool doorway = false;
  bool crenellated = false;
};

struct CourtyardHouseMeshSpec {
  float half_width = 0.86f;
  float body_height = 1.55f;
  float depth = 0.92f;
  float roof_pitch = 0.42f;
};

struct RuinedCastleCourseMeshSpec {
  float half_width = 3.05f;
  float wall_height = 2.18f;
  float tower_half_width = 0.58f;
  float tower_height = 3.18f;
  float depth = 0.92f;
  float gate_half_width = 0.58f;
  float gate_height = 1.48f;
  float ruin_amount = 0.42f;
  int parapet_blocks = 9;
};

struct GothicUnderpassMeshSpec {
  float half_width = 1.12f;
  float height = 1.86f;
  float depth = 1.28f;
  float passage_half_width = 0.46f;
  float passage_height = 1.22f;
  float buttress_width = 0.16f;
  float trim_depth = 0.12f;
  int arch_steps = 5;
  int parapet_blocks = 5;
};

struct ArchitecturalCollisionBox {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
};

[[nodiscard]] CpuMesh buildArchitecturalMesh(const ArchitecturalMeshSpec &spec);
[[nodiscard]] CpuMesh makeGatehouseMesh(GatehouseMeshSpec spec = {});
[[nodiscard]] CpuMesh makeFacadeMesh(FacadeMeshSpec spec = {});
[[nodiscard]] CpuMesh makeCourtyardHouseMesh(CourtyardHouseMeshSpec spec = {});
[[nodiscard]] CpuMesh makeRuinedCastleCourseMesh(RuinedCastleCourseMeshSpec spec = {});
[[nodiscard]] CpuMesh makeGothicUnderpassMesh(GothicUnderpassMeshSpec spec = {});
[[nodiscard]] std::vector<ArchitecturalCollisionBox>
makeGothicUnderpassCollisionBoxes(GothicUnderpassMeshSpec spec = {});

} // namespace aster
