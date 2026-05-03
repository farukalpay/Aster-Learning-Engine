// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/castle_course.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace aster {
namespace {

struct VoxelCoordHash {
  std::size_t operator()(const VoxelCoord &coord) const {
    std::size_t hash = 0;
    const auto mix = [&](const int value) {
      hash ^= std::hash<int>{}(value) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    };
    mix(coord.x);
    mix(coord.y);
    mix(coord.z);
    return hash;
  }
};

class CastleCellWriter {
public:
  void set(const VoxelCoord coord, const CastleCourseChannel channel,
           const bool collidable = true) {
    cells_[coord] = {coord, static_cast<std::uint32_t>(channel), true, collidable};
  }

  void box(const VoxelCoord min, const VoxelCoord extent, const CastleCourseChannel channel,
           const bool collidable = true) {
    if (extent.x <= 0 || extent.y <= 0 || extent.z <= 0) {
      throw std::invalid_argument("Castle voxel extent must be positive.");
    }
    for (int y = 0; y < extent.y; ++y) {
      for (int z = 0; z < extent.z; ++z) {
        for (int x = 0; x < extent.x; ++x) {
          set({min.x + x, min.y + y, min.z + z}, channel, collidable);
        }
      }
    }
  }

  void clearBox(const VoxelCoord min, const VoxelCoord extent) {
    if (extent.x <= 0 || extent.y <= 0 || extent.z <= 0) {
      throw std::invalid_argument("Castle voxel clearance extent must be positive.");
    }
    for (int y = 0; y < extent.y; ++y) {
      for (int z = 0; z < extent.z; ++z) {
        for (int x = 0; x < extent.x; ++x) {
          cells_.erase({min.x + x, min.y + y, min.z + z});
        }
      }
    }
  }

  [[nodiscard]] std::vector<VoxelCell> cells() const {
    std::vector<VoxelCell> out;
    out.reserve(cells_.size());
    for (const auto &[coord, cell] : cells_) {
      out.push_back(cell);
    }
    std::sort(out.begin(), out.end(),
              [](const VoxelCell &lhs, const VoxelCell &rhs) { return lhs.coord < rhs.coord; });
    return out;
  }

private:
  std::unordered_map<VoxelCoord, VoxelCell, VoxelCoordHash> cells_;
};

constexpr int kWallMinX = -58;
constexpr int kWallMaxX = 58;
constexpr int kLeftWallX = -61;
constexpr int kRightWallX = 59;
constexpr int kRearWallZ = -64;
constexpr int kFrontWallZ = 38;
constexpr int kFoundationMinX = -64;
constexpr int kFoundationMinZ = -66;
constexpr int kFoundationWidth = 129;
constexpr int kFoundationDepth = 110;
constexpr int kGateHalfWidth = 8;
constexpr int kGateClearHeight = 9;

enum class WallAxis {
  AlongX,
  AlongZ,
};

struct WallSpan {
  VoxelCoord min{};
  int length = 1;
  WallAxis axis = WallAxis::AlongX;
};

VoxelCoord wallExtent(const WallSpan &span, const CastleCourseWallStyle &style, const int height) {
  if (span.axis == WallAxis::AlongX) {
    return {span.length, height, style.thickness};
  }
  return {style.thickness, height, span.length};
}

VoxelCoord wallStep(const WallAxis axis, const int offset) {
  if (axis == WallAxis::AlongX) {
    return {offset, 0, 0};
  }
  return {0, 0, offset};
}

bool merlonAt(const int offset, const CastleCourseWallStyle &style) {
  return (offset % style.merlon_period) != 1;
}

void addWallSpan(CastleCellWriter &writer, const WallSpan span,
                 const CastleCourseWallStyle &style) {
  const int cap_start = std::max(0, style.body_height - style.cap_height);
  for (int y = 0; y < style.body_height; ++y) {
    const CastleCourseChannel channel = y >= cap_start ? style.cap : style.body;
    writer.box({span.min.x, span.min.y + y, span.min.z}, wallExtent(span, style, 1), channel);
  }

  for (int offset = 1; offset < span.length - 1; ++offset) {
    if (!merlonAt(offset, style)) {
      continue;
    }
    const VoxelCoord step = wallStep(span.axis, offset);
    writer.box({span.min.x + step.x, style.body_height, span.min.z + step.z},
               wallExtent({span.min, 1, span.axis}, style, style.merlon_height), style.cap);
  }
}

void addFrontFacadeButtresses(CastleCellWriter &writer, const CastleCourseWallStyle &style) {
  const int wall_width = kWallMaxX - kWallMinX + 1;
  const int center_x = (kWallMinX + kWallMaxX) / 2;
  const int buttress_offset = std::max(style.thickness * 3, wall_width / 6);
  writer.box({center_x - buttress_offset, 0, kFrontWallZ}, {1, style.body_height, style.thickness},
             style.cap);
  writer.box({center_x + buttress_offset, 0, kFrontWallZ}, {1, style.body_height, style.thickness},
             style.cap);
}

void addFoundationDeck(CastleCellWriter &writer) {
  constexpr int border = 14;
  constexpr int rear_depth = 18;
  constexpr int gate_walkway_width = kGateHalfWidth * 2 + 1;
  constexpr int gate_walkway_depth = 18;
  const int right_strip_x = kFoundationMinX + kFoundationWidth - border;

  writer.box({kFoundationMinX, -1, kFoundationMinZ}, {kFoundationWidth, 1, rear_depth},
             CastleCourseChannel::Foundation);
  writer.box({kFoundationMinX, -1, kFoundationMinZ + rear_depth},
             {border, 1, kFoundationDepth - rear_depth}, CastleCourseChannel::Foundation);
  writer.box({right_strip_x, -1, kFoundationMinZ + rear_depth},
             {border, 1, kFoundationDepth - rear_depth}, CastleCourseChannel::Foundation);
  writer.box({-kGateHalfWidth, -1, kFrontWallZ - gate_walkway_depth},
             {gate_walkway_width, 1, gate_walkway_depth + 6}, CastleCourseChannel::Foundation);
}

void addFrontWallWithGate(CastleCellWriter &writer, const CastleCourseWallStyle &style) {
  const int gate_min_x = -kGateHalfWidth;
  const int gate_max_x = kGateHalfWidth;
  const int left_length = gate_min_x - kWallMinX;
  const int right_start_x = gate_max_x + 1;
  const int right_length = kWallMaxX - right_start_x + 1;

  if (left_length > 0) {
    addWallSpan(writer, {{kWallMinX, 0, kFrontWallZ}, left_length, WallAxis::AlongX}, style);
  }
  if (right_length > 0) {
    addWallSpan(writer, {{right_start_x, 0, kFrontWallZ}, right_length, WallAxis::AlongX}, style);
  }

  const int arch_height = std::clamp(kGateClearHeight, 1, std::max(1, style.body_height - 1));
  writer.box({gate_min_x - 1, 0, kFrontWallZ}, {1, style.body_height, style.thickness}, style.cap);
  writer.box({gate_max_x + 1, 0, kFrontWallZ}, {1, style.body_height, style.thickness}, style.cap);
  writer.box({gate_min_x, arch_height, kFrontWallZ},
             {gate_max_x - gate_min_x + 1, style.body_height - arch_height, style.thickness},
             style.cap);
  writer.box({gate_min_x - 3, arch_height - 1, kFrontWallZ}, {3, 1, style.thickness}, style.cap);
  writer.box({gate_max_x + 1, arch_height - 1, kFrontWallZ}, {3, 1, style.thickness}, style.cap);
  addFrontFacadeButtresses(writer, style);
}

void addPerimeterWalls(CastleCellWriter &writer, const CastleCourseWallStyle &style) {
  const int wall_width = kWallMaxX - kWallMinX + 1;
  const int side_depth = kFrontWallZ - kRearWallZ + style.thickness;
  addFrontWallWithGate(writer, style);
  addWallSpan(writer, {{kWallMinX, 0, kRearWallZ}, wall_width, WallAxis::AlongX}, style);
  addWallSpan(writer, {{kLeftWallX, 0, kRearWallZ}, side_depth, WallAxis::AlongZ}, style);
  addWallSpan(writer, {{kRightWallX, 0, kRearWallZ}, side_depth, WallAxis::AlongZ}, style);
}

void addOctTower(CastleCellWriter &writer, const int center_x, const int center_z, const int radius,
                 const int height, const CastleCourseChannel channel,
                 const CastleCourseChannel cap_channel) {
  for (int y = 0; y < height; ++y) {
    for (int z = center_z - radius; z <= center_z + radius; ++z) {
      for (int x = center_x - radius; x <= center_x + radius; ++x) {
        const int dx = std::abs(x - center_x);
        const int dz = std::abs(z - center_z);
        if (std::max(dx, dz) > radius || dx + dz > radius * 2 - 1) {
          continue;
        }
        writer.set({x, y, z}, y >= height - 2 ? cap_channel : channel);
      }
    }
  }

  const int y = height;
  for (int z = center_z - radius; z <= center_z + radius; ++z) {
    for (int x = center_x - radius; x <= center_x + radius; ++x) {
      const int dx = std::abs(x - center_x);
      const int dz = std::abs(z - center_z);
      const bool perimeter = std::max(dx, dz) == radius || dx + dz >= radius * 2 - 2;
      if (perimeter && ((x + z) & 1) == 0) {
        writer.box({x, y, z}, {1, 2, 1}, cap_channel);
      }
    }
  }
}

void addClosedPlatformModule(CastleCellWriter &writer, const VoxelCoord min,
                             const VoxelCoord extent, const CastleCourseChannel deck,
                             const CastleCourseChannel core) {
  const int slab_depth = std::clamp(min.y, 1, 3);
  writer.box({min.x, min.y - slab_depth, min.z}, {extent.x, extent.y + slab_depth, extent.z}, deck);

  if (extent.x > 2 && extent.z > 2 && slab_depth > 1) {
    writer.box({min.x + 1, min.y - slab_depth - 1, min.z + 1}, {extent.x - 2, 1, extent.z - 2},
               core);
  }

  const int lip_y = min.y + extent.y;
  writer.box({min.x, lip_y, min.z}, {extent.x, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({min.x, lip_y, min.z + extent.z - 1}, {extent.x, 1, 1},
             CastleCourseChannel::TrimStone);
  writer.box({min.x, lip_y, min.z}, {1, 1, extent.z}, CastleCourseChannel::TrimStone);
  writer.box({min.x + extent.x - 1, lip_y, min.z}, {1, 1, extent.z},
             CastleCourseChannel::TrimStone);

  const int apron_y = std::max(min.y - slab_depth, 0);
  const int apron_height = min.y + extent.y - apron_y;
  writer.box({min.x, apron_y, min.z}, {1, apron_height, extent.z}, core);
  writer.box({min.x + extent.x - 1, apron_y, min.z}, {1, apron_height, extent.z}, core);
  writer.box({min.x, apron_y, min.z}, {extent.x, apron_height, 1}, core);
  writer.box({min.x, apron_y, min.z + extent.z - 1}, {extent.x, apron_height, 1}, core);
}

void addClosedStepBridge(CastleCellWriter &writer, const VoxelCoord min, const VoxelCoord extent,
                         const CastleCourseChannel channel) {
  const int depth = std::clamp(min.y, 1, 2);
  writer.box({min.x, min.y - depth, min.z}, {extent.x, extent.y + depth, extent.z}, channel);
  if (extent.x > 2 && extent.z > 2) {
    writer.box({min.x + 1, std::max(min.y - depth - 1, 0), min.z + 1},
               {extent.x - 2, 1, extent.z - 2}, CastleCourseChannel::CoolStone);
  }
}

void addParkourUnderpassClearance(CastleCellWriter &writer) {
  writer.clearBox({-3, 0, -9}, {7, 4, 18});
}

void addParkourRun(CastleCellWriter &writer) {
  struct Platform {
    VoxelCoord min;
    VoxelCoord extent;
    CastleCourseChannel deck;
  };
  const Platform platforms[] = {
      {{-18, 2, 11}, {6, 1, 4}, CastleCourseChannel::CoolStone},
      {{-15, 3, 9}, {6, 1, 4}, CastleCourseChannel::TrimStone},
      {{-11, 4, 8}, {6, 1, 4}, CastleCourseChannel::WarmStone},
      {{5, 2, 8}, {6, 1, 4}, CastleCourseChannel::WarmStone},
      {{2, 3, 6}, {6, 1, 4}, CastleCourseChannel::TrimStone},
      {{-2, 4, 5}, {6, 1, 4}, CastleCourseChannel::CoolStone},
      {{-6, 5, 3}, {6, 1, 4}, CastleCourseChannel::TrimStone},
      {{-10, 6, 1}, {5, 1, 4}, CastleCourseChannel::CoolStone},
      {{-9, 7, -1}, {7, 1, 4}, CastleCourseChannel::WarmStone},
      {{-5, 8, -2}, {9, 1, 4}, CastleCourseChannel::TrimStone},
      {{-2, 9, -4}, {7, 1, 4}, CastleCourseChannel::WarmStone},
      {{4, 10, -7}, {9, 1, 4}, CastleCourseChannel::CoolStone},
      {{10, 11, -10}, {7, 1, 4}, CastleCourseChannel::TrimStone},
  };

  for (const Platform &platform : platforms) {
    addClosedPlatformModule(writer, platform.min, platform.extent, platform.deck,
                            CastleCourseChannel::CoolStone);
  }

  addClosedStepBridge(writer, {4, 3, 8}, {4, 1, 3}, CastleCourseChannel::TrimStone);
  addClosedStepBridge(writer, {1, 4, 6}, {4, 1, 3}, CastleCourseChannel::CoolStone);
  addClosedStepBridge(writer, {-3, 5, 5}, {4, 1, 3}, CastleCourseChannel::TrimStone);
  addClosedStepBridge(writer, {-7, 6, 3}, {4, 1, 3}, CastleCourseChannel::CoolStone);
  addClosedStepBridge(writer, {-9, 7, 1}, {5, 1, 3}, CastleCourseChannel::WarmStone);
  addClosedStepBridge(writer, {-7, 8, -1}, {6, 1, 3}, CastleCourseChannel::TrimStone);
  addClosedStepBridge(writer, {-4, 9, -3}, {6, 1, 3}, CastleCourseChannel::WarmStone);
  addClosedStepBridge(writer, {3, 10, -6}, {6, 1, 3}, CastleCourseChannel::CoolStone);
  addClosedStepBridge(writer, {8, 11, -9}, {5, 1, 3}, CastleCourseChannel::TrimStone);

  writer.box({-18, 0, 11}, {1, 2, 1}, CastleCourseChannel::CoolStone);
  writer.box({-13, 0, 14}, {1, 2, 1}, CastleCourseChannel::CoolStone);
  writer.box({-15, 0, 9}, {1, 3, 1}, CastleCourseChannel::CoolStone);
  writer.box({-10, 0, 11}, {1, 4, 1}, CastleCourseChannel::CoolStone);
  writer.box({5, 0, 8}, {1, 2, 1}, CastleCourseChannel::CoolStone);
  writer.box({10, 0, 11}, {1, 2, 1}, CastleCourseChannel::CoolStone);
  writer.box({2, 0, 6}, {1, 3, 1}, CastleCourseChannel::CoolStone);
  writer.box({7, 0, 9}, {1, 3, 1}, CastleCourseChannel::CoolStone);
  writer.box({-6, 0, 3}, {1, 5, 1}, CastleCourseChannel::CoolStone);
  writer.box({-1, 0, 6}, {1, 5, 1}, CastleCourseChannel::CoolStone);
  writer.box({-9, 0, -1}, {1, 7, 1}, CastleCourseChannel::CoolStone);
  writer.box({3, 0, -2}, {1, 8, 1}, CastleCourseChannel::CoolStone);

  writer.box({4, 3, 12}, {7, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({1, 4, 10}, {7, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({-3, 5, 9}, {7, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({-7, 6, 7}, {7, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({4, 10, -3}, {9, 1, 1}, CastleCourseChannel::TrimStone);
  writer.box({10, 11, -6}, {7, 1, 1}, CastleCourseChannel::TrimStone);

  addParkourUnderpassClearance(writer);
}

Vec3 cellCenter(const CastleCourseSpec &spec, const VoxelCoord min, const VoxelCoord extent) {
  const Vec3 origin{-spec.cell_size.x * 0.5f, 0.0f, -spec.cell_size.z * 0.5f};
  return {origin.x +
              (static_cast<float>(min.x) + static_cast<float>(extent.x) * 0.5f) * spec.cell_size.x,
          origin.y +
              (static_cast<float>(min.y) + static_cast<float>(extent.y) * 0.5f) * spec.cell_size.y,
          origin.z +
              (static_cast<float>(min.z) + static_cast<float>(extent.z) * 0.5f) * spec.cell_size.z};
}

Vec3 cellHalfExtents(const CastleCourseSpec &spec, const VoxelCoord extent) {
  return {static_cast<float>(extent.x) * spec.cell_size.x * 0.5f,
          static_cast<float>(extent.y) * spec.cell_size.y * 0.5f,
          static_cast<float>(extent.z) * spec.cell_size.z * 0.5f};
}

float cellTopY(const CastleCourseSpec &spec, const VoxelCoord min, const VoxelCoord extent) {
  return static_cast<float>(min.y + extent.y) * spec.cell_size.y;
}

CastleCourseGroundZone groundZone(const CastleCourseSpec &spec, const VoxelCoord min,
                                  const VoxelCoord extent) {
  const Vec3 center = cellCenter(spec, min, extent);
  const Vec3 half_extents = cellHalfExtents(spec, extent);
  return {{center.x, center.z}, {half_extents.x, half_extents.z}};
}

std::vector<CastleCourseBoxElement> buildBoxElements(const CastleCourseSpec &spec) {
  std::vector<CastleCourseBoxElement> elements;
  constexpr float sign_board_center_y = 3.42f;
  constexpr float sign_board_half_height = 0.38f;
  constexpr float sign_board_half_width = 1.12f;
  constexpr float sign_board_half_depth = 0.065f;
  constexpr float sign_post_x = 0.76f;
  const float sign_post_bottom_y = cellTopY(spec, {-6, 5, 3}, {6, 1, 4}) + 0.035f;
  constexpr float sign_post_top_y = sign_board_center_y + sign_board_half_height * 0.28f;
  const float sign_post_center_y = (sign_post_bottom_y + sign_post_top_y) * 0.5f;
  const float sign_post_half_height = (sign_post_top_y - sign_post_bottom_y) * 0.5f;
  constexpr float sign_post_half_width = 0.055f;
  constexpr float sign_post_half_depth = 0.055f;
  constexpr float sign_post_z = 3.36f;
  constexpr float sign_beam_center_y = 2.96f;
  constexpr float sign_beam_half_width = 0.96f;
  constexpr float sign_beam_half_height = 0.045f;
  constexpr float sign_beam_half_depth = 0.045f;

  elements.push_back({{0.0f, sign_board_center_y, 3.34f},
                      {sign_board_half_width, sign_board_half_height, sign_board_half_depth},
                      CastleCourseChannel::Wood,
                      false,
                      false});
  elements.push_back({{-sign_post_x, sign_post_center_y, sign_post_z},
                      {sign_post_half_width, sign_post_half_height, sign_post_half_depth},
                      CastleCourseChannel::Wood,
                      false,
                      false});
  elements.push_back({{sign_post_x, sign_post_center_y, sign_post_z},
                      {sign_post_half_width, sign_post_half_height, sign_post_half_depth},
                      CastleCourseChannel::Wood,
                      false,
                      false});
  elements.push_back({{0.0f, sign_beam_center_y, 3.35f},
                      {sign_beam_half_width, sign_beam_half_height, sign_beam_half_depth},
                      CastleCourseChannel::Wood,
                      false,
                      false});
  return elements;
}

std::vector<CastleCourseGroundZone> buildGroundZones(const CastleCourseSpec &spec) {
  std::vector<CastleCourseGroundZone> zones;
  const int min_x = kLeftWallX + spec.perimeter_wall.thickness;
  const int max_x = kRightWallX;
  const int min_z = kRearWallZ + spec.perimeter_wall.thickness;
  const int max_z = kFrontWallZ;
  zones.push_back(groundZone(spec, {min_x, 0, min_z}, {max_x - min_x, 1, max_z - min_z}));
  return zones;
}

} // namespace

CastleCourse buildCastleCourse(CastleCourseSpec spec) {
  if (spec.cell_size.x <= 0.0f || spec.cell_size.y <= 0.0f || spec.cell_size.z <= 0.0f) {
    throw std::invalid_argument("Castle course cell size must be positive.");
  }
  if (spec.perimeter_wall.body_height <= 0 || spec.perimeter_wall.thickness <= 0 ||
      spec.perimeter_wall.cap_height < 0 ||
      spec.perimeter_wall.cap_height > spec.perimeter_wall.body_height ||
      spec.perimeter_wall.merlon_height <= 0 || spec.perimeter_wall.merlon_period < 2 ||
      spec.tower_height <= 0) {
    throw std::invalid_argument("Castle course wall style dimensions must be positive.");
  }

  CastleCellWriter writer;
  addFoundationDeck(writer);
  addPerimeterWalls(writer, spec.perimeter_wall);
  addOctTower(writer, kLeftWallX + 1, kFrontWallZ + 1, 5, spec.tower_height, spec.tower_channel,
              spec.perimeter_wall.cap);
  addOctTower(writer, kRightWallX + 1, kFrontWallZ + 1, 5, spec.tower_height, spec.tower_channel,
              spec.perimeter_wall.cap);
  addOctTower(writer, kLeftWallX + 1, kRearWallZ + 1, 5, spec.tower_height, spec.tower_channel,
              spec.perimeter_wall.cap);
  addOctTower(writer, kRightWallX + 1, kRearWallZ + 1, 5, spec.tower_height, spec.tower_channel,
              spec.perimeter_wall.cap);
  addParkourRun(writer);

  CastleCourse course;
  course.structure = buildVoxelStructure(
      writer.cells(), {.cell_size = spec.cell_size,
                       .origin = {-spec.cell_size.x * 0.5f, 0.0f, -spec.cell_size.z * 0.5f},
                       .uv_scale = spec.uv_scale,
                       .merge_collision_boxes = true});
  course.box_elements = buildBoxElements(spec);
  course.ground_zones = buildGroundZones(spec);
  return course;
}

} // namespace aster
