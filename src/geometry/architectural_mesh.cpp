// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/architectural_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aster {
namespace {

BrushLevelMeshOptions architecturalOptions(const BrushLevelMeshOptions &options) {
  BrushLevelMeshOptions out = options;
  if (out.uv_scale <= 0.0f) {
    out.uv_scale = 0.42f;
  }
  return out;
}

void validatePositive(const float value, const char *name) {
  if (value <= 0.0f) {
    throw std::invalid_argument(name);
  }
}

void addMass(ArchitecturalMeshSpec &spec, const Vec3 center, const Vec3 half_extents,
             const float chamfer = 0.0f, const Vec3 rotation = {}) {
  spec.masses.push_back({center, half_extents, rotation, chamfer});
}

void addOpening(ArchitecturalMeshSpec &spec, const Vec3 center, const Vec3 half_extents,
                const float chamfer = 0.0f, const Vec3 rotation = {}) {
  spec.openings.push_back({center, half_extents, rotation, chamfer});
}

void addParapet(ArchitecturalMeshSpec &spec, const float y, const float z_half, const float x_min,
                const float x_max, const int count) {
  const int block_count = std::max(1, count);
  const float span = x_max - x_min;
  const float step = span / static_cast<float>(block_count);
  const float half_width = step * 0.28f;
  for (int i = 0; i < block_count; ++i) {
    const float x = x_min + (static_cast<float>(i) + 0.5f) * step;
    addMass(spec, {x, y, 0.0f}, {half_width, 0.24f, z_half}, 0.025f);
  }
}

BrushLevelMeshOptions cleanMasonryOptions(const float uv_scale, const int seed) {
  BrushLevelMeshOptions options;
  options.uv_scale = uv_scale;
  options.edge_softening = 0.026f;
  options.noise_frequency = 0.76f;
  options.noise_strength = 0.006f;
  options.noise_seed = seed;
  return options;
}

} // namespace

CpuMesh buildArchitecturalMesh(const ArchitecturalMeshSpec &spec) {
  if (spec.masses.empty()) {
    throw std::invalid_argument("Architectural mesh requires at least one mass.");
  }

  std::vector<BrushBox> brushes;
  brushes.reserve(spec.masses.size() + spec.openings.size());
  for (const ArchitecturalMass &mass : spec.masses) {
    brushes.push_back(
        {mass.center, mass.half_extents, mass.rotation, BrushVolume::Solid, 0, mass.chamfer});
  }
  for (const ArchitecturalOpening &opening : spec.openings) {
    brushes.push_back({opening.center, opening.half_extents, opening.rotation, BrushVolume::Air, 1,
                       opening.chamfer});
  }

  return buildBrushLevelMesh(brushes, architecturalOptions(spec.mesh_options));
}

CpuMesh makeGatehouseMesh(const GatehouseMeshSpec spec) {
  validatePositive(spec.wall_half_width, "Gatehouse wall width must be positive.");
  validatePositive(spec.wall_height, "Gatehouse wall height must be positive.");
  validatePositive(spec.tower_half_width, "Gatehouse tower width must be positive.");
  validatePositive(spec.tower_height, "Gatehouse tower height must be positive.");
  validatePositive(spec.depth, "Gatehouse depth must be positive.");
  validatePositive(spec.door_half_width, "Gatehouse door width must be positive.");
  validatePositive(spec.door_height, "Gatehouse door height must be positive.");

  ArchitecturalMeshSpec mesh;
  mesh.mesh_options = cleanMasonryOptions(0.38f, 211);

  const float z_wall = spec.depth * 0.5f;
  const float z_tower = spec.depth * 0.62f;
  const float tower_x = spec.wall_half_width + spec.tower_half_width * 0.96f;
  addMass(mesh, {0.0f, spec.wall_height * 0.5f, 0.0f},
          {spec.wall_half_width, spec.wall_height * 0.5f, z_wall}, 0.035f);
  addMass(mesh, {0.0f, spec.wall_height + 0.18f, 0.0f},
          {spec.wall_half_width + 0.18f, 0.18f, z_wall * 1.08f}, 0.03f);
  addMass(mesh, {-tower_x, spec.tower_height * 0.5f, 0.0f},
          {spec.tower_half_width, spec.tower_height * 0.5f, z_tower}, 0.045f);
  addMass(mesh, {tower_x, spec.tower_height * 0.5f, 0.0f},
          {spec.tower_half_width, spec.tower_height * 0.5f, z_tower}, 0.045f);
  addMass(mesh, {-tower_x, spec.tower_height + 0.10f, 0.0f},
          {spec.tower_half_width * 1.20f, 0.16f, z_tower * 1.08f}, 0.035f);
  addMass(mesh, {tower_x, spec.tower_height + 0.10f, 0.0f},
          {spec.tower_half_width * 1.20f, 0.16f, z_tower * 1.08f}, 0.035f);

  addParapet(mesh, spec.wall_height + 0.55f, z_wall * 1.02f, -spec.wall_half_width,
             spec.wall_half_width, spec.parapet_blocks);
  addParapet(mesh, spec.tower_height + 0.46f, z_tower, -tower_x - spec.tower_half_width * 0.92f,
             -tower_x + spec.tower_half_width * 0.92f, 2);
  addParapet(mesh, spec.tower_height + 0.46f, z_tower, tower_x - spec.tower_half_width * 0.92f,
             tower_x + spec.tower_half_width * 0.92f, 2);

  const float door_depth = spec.depth * 0.82f;
  addOpening(mesh, {0.0f, spec.door_height * 0.43f, 0.0f},
             {spec.door_half_width, spec.door_height * 0.43f, door_depth}, 0.0f);
  addOpening(mesh, {0.0f, spec.door_height * 0.82f, 0.0f},
             {spec.door_half_width * 0.76f, spec.door_height * 0.20f, door_depth}, 0.0f);
  addOpening(mesh, {0.0f, spec.door_height * 1.02f, 0.0f},
             {spec.door_half_width * 0.44f, spec.door_height * 0.12f, door_depth}, 0.0f);

  addOpening(mesh, {-tower_x, spec.tower_height * 0.44f, 0.0f},
             {spec.tower_half_width * 0.22f, 0.28f, door_depth}, 0.0f);
  addOpening(mesh, {-tower_x, spec.tower_height * 0.68f, 0.0f},
             {spec.tower_half_width * 0.18f, 0.24f, door_depth}, 0.0f);
  addOpening(mesh, {tower_x, spec.tower_height * 0.44f, 0.0f},
             {spec.tower_half_width * 0.22f, 0.28f, door_depth}, 0.0f);
  addOpening(mesh, {tower_x, spec.tower_height * 0.68f, 0.0f},
             {spec.tower_half_width * 0.18f, 0.24f, door_depth}, 0.0f);

  return buildArchitecturalMesh(mesh);
}

CpuMesh makeFacadeMesh(const FacadeMeshSpec spec) {
  validatePositive(spec.half_width, "Facade width must be positive.");
  validatePositive(spec.height, "Facade height must be positive.");
  validatePositive(spec.depth, "Facade depth must be positive.");

  ArchitecturalMeshSpec mesh;
  mesh.mesh_options = cleanMasonryOptions(0.42f, 307);

  const float z_half = spec.depth * 0.5f;
  addMass(mesh, {0.0f, spec.height * 0.5f, 0.0f}, {spec.half_width, spec.height * 0.5f, z_half},
          0.028f);
  addMass(mesh, {0.0f, spec.height + 0.12f, 0.0f}, {spec.half_width * 1.05f, 0.12f, z_half},
          0.024f);
  if (spec.crenellated) {
    addParapet(mesh, spec.height + 0.40f, z_half, -spec.half_width, spec.half_width,
               std::max(2, spec.window_columns + 2));
  }

  const float opening_depth = spec.depth * 0.72f;
  if (spec.doorway) {
    addOpening(mesh, {0.0f, 0.48f, 0.0f}, {spec.half_width * 0.25f, 0.48f, opening_depth});
  }

  const int columns = std::max(1, spec.window_columns);
  const int rows = std::max(1, spec.window_rows);
  const float min_y = spec.doorway ? 1.18f : 0.72f;
  const float max_y = std::max(min_y + 0.1f, spec.height - 0.48f);
  for (int row = 0; row < rows; ++row) {
    const float row_fill =
        rows == 1 ? 0.5f : static_cast<float>(row) / static_cast<float>(rows - 1);
    const float y = min_y + row_fill * (max_y - min_y);
    for (int column = 0; column < columns; ++column) {
      const float column_fill =
          columns == 1 ? 0.5f : static_cast<float>(column) / static_cast<float>(columns - 1);
      const float x = -spec.half_width * 0.52f + column_fill * spec.half_width * 1.04f;
      addOpening(mesh, {x, y, 0.0f}, {spec.half_width * 0.12f, 0.18f, opening_depth});
    }
  }

  return buildArchitecturalMesh(mesh);
}

CpuMesh makeCourtyardHouseMesh(const CourtyardHouseMeshSpec spec) {
  validatePositive(spec.half_width, "House width must be positive.");
  validatePositive(spec.body_height, "House body height must be positive.");
  validatePositive(spec.depth, "House depth must be positive.");

  ArchitecturalMeshSpec mesh;
  mesh.mesh_options = cleanMasonryOptions(0.40f, 419);

  const float z_half = spec.depth * 0.5f;
  addMass(mesh, {0.0f, spec.body_height * 0.5f, 0.0f},
          {spec.half_width, spec.body_height * 0.5f, z_half}, 0.030f);
  addMass(mesh, {-spec.half_width * 0.28f, spec.body_height + 0.26f, 0.0f},
          {spec.half_width * 0.66f, 0.14f, z_half * 1.12f}, 0.020f, {0.0f, 0.0f, spec.roof_pitch});
  addMass(mesh, {spec.half_width * 0.28f, spec.body_height + 0.26f, 0.0f},
          {spec.half_width * 0.66f, 0.14f, z_half * 1.12f}, 0.020f, {0.0f, 0.0f, -spec.roof_pitch});
  addMass(mesh, {spec.half_width * 0.34f, spec.body_height + 0.78f, 0.0f},
          {0.12f, 0.32f, z_half * 0.36f}, 0.018f);

  addOpening(mesh, {0.0f, 0.44f, 0.0f}, {spec.half_width * 0.22f, 0.44f, spec.depth * 0.72f});
  addOpening(mesh, {-spec.half_width * 0.48f, 1.02f, 0.0f},
             {spec.half_width * 0.14f, 0.18f, spec.depth * 0.72f});
  addOpening(mesh, {spec.half_width * 0.48f, 1.02f, 0.0f},
             {spec.half_width * 0.14f, 0.18f, spec.depth * 0.72f});

  return buildArchitecturalMesh(mesh);
}

CpuMesh makeRuinedCastleCourseMesh(const RuinedCastleCourseMeshSpec spec) {
  validatePositive(spec.half_width, "Ruined castle width must be positive.");
  validatePositive(spec.wall_height, "Ruined castle wall height must be positive.");
  validatePositive(spec.tower_half_width, "Ruined castle tower width must be positive.");
  validatePositive(spec.tower_height, "Ruined castle tower height must be positive.");
  validatePositive(spec.depth, "Ruined castle depth must be positive.");
  validatePositive(spec.gate_half_width, "Ruined castle gate width must be positive.");
  validatePositive(spec.gate_height, "Ruined castle gate height must be positive.");

  ArchitecturalMeshSpec mesh;
  mesh.mesh_options = cleanMasonryOptions(0.32f, 733);
  mesh.mesh_options.edge_softening = 0.040f;
  mesh.mesh_options.noise_frequency = 0.92f;
  mesh.mesh_options.noise_strength = 0.014f;

  const float ruin = std::clamp(spec.ruin_amount, 0.0f, 1.0f);
  const float z_wall = spec.depth * 0.5f;
  const float z_tower = spec.depth * 0.66f;
  const float tower_x = spec.half_width + spec.tower_half_width * 0.78f;
  addMass(mesh, {0.0f, 0.18f, 0.0f}, {spec.half_width + 0.62f, 0.18f, z_wall * 1.18f}, 0.045f);
  addMass(mesh, {0.0f, spec.wall_height * 0.48f, 0.0f},
          {spec.half_width, spec.wall_height * 0.48f, z_wall}, 0.055f);
  addMass(mesh, {-tower_x, spec.tower_height * 0.50f, 0.0f},
          {spec.tower_half_width, spec.tower_height * 0.50f, z_tower}, 0.060f);
  addMass(mesh, {tower_x, spec.tower_height * 0.46f, 0.0f},
          {spec.tower_half_width, spec.tower_height * 0.46f, z_tower}, 0.060f);
  addMass(mesh, {-tower_x, spec.tower_height + 0.08f, 0.0f},
          {spec.tower_half_width * 1.12f, 0.15f, z_tower * 1.05f}, 0.035f);
  addMass(mesh, {tower_x, spec.tower_height * (0.92f + ruin * 0.05f), 0.0f},
          {spec.tower_half_width * 1.10f, 0.14f, z_tower * 1.05f}, 0.035f);

  const int parapets = std::max(3, spec.parapet_blocks);
  const float span = spec.half_width * 2.0f;
  const float step = span / static_cast<float>(parapets);
  for (int i = 0; i < parapets; ++i) {
    const float fill = static_cast<float>(i) / static_cast<float>(std::max(1, parapets - 1));
    const bool gap = fill > 0.38f && fill < 0.54f;
    const float broken = std::sin(static_cast<float>(i) * 1.73f) * 0.5f + 0.5f;
    if (gap && ruin > 0.18f) {
      continue;
    }
    const float x = -spec.half_width + (static_cast<float>(i) + 0.5f) * step;
    const float height = 0.18f + (1.0f - ruin * broken) * 0.17f;
    addMass(mesh, {x, spec.wall_height + height * 0.58f, 0.0f},
            {step * 0.28f, height * 0.58f, z_wall * 1.02f}, 0.030f);
  }

  for (const float side : {-1.0f, 1.0f}) {
    const float x = side * tower_x;
    addMass(mesh, {x, spec.tower_height + 0.40f, 0.0f},
            {spec.tower_half_width * 0.30f, 0.24f, z_tower}, 0.028f);
    addMass(mesh, {x + side * spec.tower_half_width * 0.46f, spec.tower_height + 0.30f, 0.0f},
            {spec.tower_half_width * 0.18f, 0.18f, z_tower * 0.92f}, 0.026f);
  }

  addMass(mesh, {-1.34f, spec.wall_height + 0.18f, -0.10f}, {0.74f, 0.16f, 0.45f}, 0.036f);
  addMass(mesh, {0.18f, spec.wall_height + 0.43f, -0.10f}, {0.66f, 0.14f, 0.40f}, 0.034f);
  addMass(mesh, {1.42f, spec.wall_height + 0.74f, -0.08f}, {0.54f, 0.13f, 0.38f}, 0.032f);
  addMass(mesh, {0.88f, spec.wall_height + 1.06f, -0.05f}, {0.72f, 0.13f, 0.36f}, 0.032f);

  addOpening(mesh, {0.0f, spec.gate_height * 0.44f, 0.0f},
             {spec.gate_half_width, spec.gate_height * 0.44f, spec.depth * 0.78f}, 0.0f);
  addOpening(mesh, {0.0f, spec.gate_height * 0.84f, 0.0f},
             {spec.gate_half_width * 0.74f, spec.gate_height * 0.20f, spec.depth * 0.78f}, 0.0f);
  addOpening(mesh, {-1.36f, spec.wall_height * 0.68f, 0.0f}, {0.18f, 0.28f, spec.depth * 0.72f},
             0.0f);
  addOpening(mesh, {1.30f, spec.wall_height * 0.62f, 0.0f}, {0.16f, 0.24f, spec.depth * 0.72f},
             0.0f);
  addOpening(mesh, {-tower_x, spec.tower_height * 0.48f, 0.0f},
             {spec.tower_half_width * 0.18f, 0.28f, spec.depth * 0.72f}, 0.0f);
  addOpening(mesh, {tower_x, spec.tower_height * 0.42f, 0.0f},
             {spec.tower_half_width * 0.18f, 0.26f, spec.depth * 0.72f}, 0.0f);

  return buildArchitecturalMesh(mesh);
}

CpuMesh makeGothicUnderpassMesh(const GothicUnderpassMeshSpec spec) {
  validatePositive(spec.half_width, "Gothic underpass width must be positive.");
  validatePositive(spec.height, "Gothic underpass height must be positive.");
  validatePositive(spec.depth, "Gothic underpass depth must be positive.");
  validatePositive(spec.passage_half_width, "Gothic underpass passage width must be positive.");
  validatePositive(spec.passage_height, "Gothic underpass passage height must be positive.");
  validatePositive(spec.buttress_width, "Gothic underpass buttress width must be positive.");
  validatePositive(spec.trim_depth, "Gothic underpass trim depth must be positive.");
  if (spec.passage_half_width >= spec.half_width || spec.passage_height >= spec.height ||
      spec.arch_steps < 1) {
    throw std::invalid_argument("Gothic underpass opening must fit inside the wall mass.");
  }

  ArchitecturalMeshSpec mesh;
  mesh.mesh_options = cleanMasonryOptions(0.34f, 971);
  mesh.mesh_options.edge_softening = 0.032f;
  mesh.mesh_options.noise_frequency = 0.88f;
  mesh.mesh_options.noise_strength = 0.010f;

  const float z_half = spec.depth * 0.5f;
  const float front_z = z_half + spec.trim_depth * 0.45f;
  const float back_z = -front_z;
  addMass(mesh, {0.0f, spec.height * 0.5f, 0.0f}, {spec.half_width, spec.height * 0.5f, z_half},
          0.040f);
  addMass(mesh, {0.0f, spec.height + 0.10f, 0.0f}, {spec.half_width * 1.10f, 0.10f, z_half * 1.08f},
          0.028f);

  for (const float side : {-1.0f, 1.0f}) {
    const float x = side * (spec.passage_half_width + spec.buttress_width * 0.74f);
    addMass(mesh, {x, spec.passage_height * 0.48f, front_z},
            {spec.buttress_width, spec.passage_height * 0.48f, spec.trim_depth}, 0.024f);
    addMass(mesh, {x, spec.passage_height * 0.48f, back_z},
            {spec.buttress_width, spec.passage_height * 0.48f, spec.trim_depth}, 0.024f);
    addMass(mesh,
            {side * (spec.half_width + spec.buttress_width * 0.35f), spec.height * 0.44f, 0.0f},
            {spec.buttress_width * 0.70f, spec.height * 0.44f, z_half * 1.06f}, 0.030f);
  }

  const float rib_y = spec.passage_height + 0.09f;
  addMass(mesh, {0.0f, rib_y, front_z}, {spec.passage_half_width * 0.82f, 0.075f, spec.trim_depth},
          0.020f);
  addMass(mesh, {0.0f, rib_y, back_z}, {spec.passage_half_width * 0.82f, 0.075f, spec.trim_depth},
          0.020f);
  addParapet(mesh, spec.height + 0.36f, z_half * 0.96f, -spec.half_width * 0.92f,
             spec.half_width * 0.92f, spec.parapet_blocks);

  const float opening_depth = spec.depth * 0.82f;
  addOpening(mesh, {0.0f, spec.passage_height * 0.35f, 0.0f},
             {spec.passage_half_width, spec.passage_height * 0.35f, opening_depth}, 0.0f);
  const float arch_base = spec.passage_height * 0.56f;
  const float arch_top = spec.passage_height * 1.03f;
  const float arch_step_height =
      std::max((arch_top - arch_base) / static_cast<float>(spec.arch_steps), 0.025f);
  for (int step = 0; step < spec.arch_steps; ++step) {
    const float fill =
        static_cast<float>(step) / static_cast<float>(std::max(1, spec.arch_steps - 1));
    const float y = arch_base + (static_cast<float>(step) + 0.5f) * arch_step_height;
    const float half_width =
        spec.passage_half_width * (1.0f - fill * 0.68f) + spec.buttress_width * 0.10f;
    addOpening(mesh, {0.0f, y, 0.0f}, {half_width, arch_step_height * 0.64f, opening_depth}, 0.0f);
  }

  return buildArchitecturalMesh(mesh);
}

std::vector<ArchitecturalCollisionBox>
makeGothicUnderpassCollisionBoxes(const GothicUnderpassMeshSpec spec) {
  validatePositive(spec.half_width, "Gothic underpass width must be positive.");
  validatePositive(spec.height, "Gothic underpass height must be positive.");
  validatePositive(spec.depth, "Gothic underpass depth must be positive.");
  validatePositive(spec.passage_half_width, "Gothic underpass passage width must be positive.");
  validatePositive(spec.passage_height, "Gothic underpass passage height must be positive.");
  validatePositive(spec.buttress_width, "Gothic underpass buttress width must be positive.");
  if (spec.passage_half_width >= spec.half_width || spec.passage_height >= spec.height) {
    throw std::invalid_argument("Gothic underpass passage must fit inside the masonry mass.");
  }

  const float depth_half = spec.depth * 0.5f;
  const float inner_face = std::min(spec.half_width - spec.buttress_width * 0.25f,
                                    spec.passage_half_width + spec.buttress_width * 0.55f);
  const float outer_face = spec.half_width + spec.buttress_width * 0.35f;
  const float side_half_x = std::max((outer_face - inner_face) * 0.5f, spec.buttress_width * 0.45f);
  const float side_center_x = inner_face + side_half_x;
  const float side_height = std::max(spec.passage_height * 0.96f, 0.10f);
  const float lintel_bottom = std::min(spec.passage_height + spec.buttress_width * 0.35f,
                                       spec.height - spec.buttress_width * 0.25f);
  const float lintel_top = spec.height + 0.18f;
  const float lintel_half_y = std::max((lintel_top - lintel_bottom) * 0.5f, 0.05f);

  return {{{-side_center_x, side_height * 0.5f, 0.0f},
           {side_half_x, side_height * 0.5f, depth_half * 0.96f}},
          {{side_center_x, side_height * 0.5f, 0.0f},
           {side_half_x, side_height * 0.5f, depth_half * 0.96f}},
          {{0.0f, lintel_bottom + lintel_half_y, 0.0f},
           {spec.half_width * 0.92f, lintel_half_y, depth_half * 0.96f}}};
}

} // namespace aster
