// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/voxel_structure.hpp"

#include <cstdint>
#include <vector>

namespace aster {

enum class CastleCourseChannel : std::uint32_t {
  Foundation = 0,
  WarmStone = 1,
  CoolStone = 2,
  TrimStone = 3,
  Wood = 4,
  Iron = 5,
  WarmLight = 6,
};

struct CastleCourseWallStyle {
  CastleCourseChannel body = CastleCourseChannel::WarmStone;
  CastleCourseChannel cap = CastleCourseChannel::TrimStone;
  int body_height = 14;
  int thickness = 3;
  int cap_height = 3;
  int merlon_height = 2;
  int merlon_period = 3;
};

struct CastleCourseBoxElement {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  CastleCourseChannel channel = CastleCourseChannel::WarmStone;
  bool collidable = false;
  bool support = false;
};

struct CastleCourseGroundZone {
  Vec2 center{};
  Vec2 half_extents{0.5f, 0.5f};
};

struct CastleCourseSpec {
  Vec3 cell_size{0.36f, 0.30f, 0.36f};
  float uv_scale = 0.34f;
  CastleCourseWallStyle perimeter_wall{};
  CastleCourseChannel tower_channel = CastleCourseChannel::CoolStone;
  int tower_height = 17;
};

struct CastleCourse {
  VoxelStructure structure{};
  std::vector<CastleCourseBoxElement> box_elements;
  std::vector<CastleCourseGroundZone> ground_zones;
};

[[nodiscard]] CastleCourse buildCastleCourse(CastleCourseSpec spec = {});

} // namespace aster
