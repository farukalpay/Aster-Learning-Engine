// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/brush_level_mesh.hpp"

#include "aster/math/noise.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aster {
namespace {

constexpr float kEpsilon = 0.0001f;

struct BrushFrame {
  BrushBox brush{};
  std::array<Vec3, 3> axis{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f},
                           Vec3{0.0f, 0.0f, 1.0f}};
};

float component(const Vec3 value, const int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

Vec3 withComponent(Vec3 value, const int axis, const float component_value) {
  if (axis == 0) {
    value.x = component_value;
  } else if (axis == 1) {
    value.y = component_value;
  } else {
    value.z = component_value;
  }
  return value;
}

Vec3 rotateX(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

Vec3 rotateY(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

Vec3 rotateZ(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

Vec3 rotateEuler(const Vec3 value, const Vec3 rotation) {
  return rotateZ(rotateY(rotateX(value, rotation.x), rotation.y), rotation.z);
}

BrushFrame makeFrame(const BrushBox &brush) {
  BrushFrame frame;
  frame.brush = brush;
  frame.axis[0] = normalize(rotateEuler({1.0f, 0.0f, 0.0f}, brush.rotation));
  frame.axis[1] = normalize(rotateEuler({0.0f, 1.0f, 0.0f}, brush.rotation));
  frame.axis[2] = normalize(rotateEuler({0.0f, 0.0f, 1.0f}, brush.rotation));
  return frame;
}

Vec3 toLocal(const BrushFrame &frame, const Vec3 point) {
  const Vec3 d = point - frame.brush.center;
  return {dot(d, frame.axis[0]), dot(d, frame.axis[1]), dot(d, frame.axis[2])};
}

Vec3 toWorld(const BrushFrame &frame, const Vec3 local) {
  return frame.brush.center + frame.axis[0] * local.x + frame.axis[1] * local.y +
         frame.axis[2] * local.z;
}

bool containsLocal(const Vec3 local, const Vec3 half_extents, const float tolerance = kEpsilon) {
  return std::abs(local.x) <= half_extents.x + tolerance &&
         std::abs(local.y) <= half_extents.y + tolerance &&
         std::abs(local.z) <= half_extents.z + tolerance;
}

bool contains(const BrushFrame &frame, const Vec3 point, const float tolerance = kEpsilon) {
  return containsLocal(toLocal(frame, point), frame.brush.half_extents, tolerance);
}

std::array<Vec3, 8> corners(const BrushFrame &frame) {
  std::array<Vec3, 8> out{};
  int index = 0;
  for (const float sx : {-1.0f, 1.0f}) {
    for (const float sy : {-1.0f, 1.0f}) {
      for (const float sz : {-1.0f, 1.0f}) {
        out[static_cast<std::size_t>(index++)] =
            toWorld(frame, frame.brush.half_extents * Vec3{sx, sy, sz});
      }
    }
  }
  return out;
}

struct LocalBounds {
  Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
           std::numeric_limits<float>::max()};
  Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
           std::numeric_limits<float>::lowest()};
};

LocalBounds boundsOfInFrame(const BrushFrame &target, const BrushFrame &source) {
  LocalBounds bounds;
  for (const Vec3 point : corners(source)) {
    const Vec3 local = toLocal(target, point);
    bounds.min.x = std::min(bounds.min.x, local.x);
    bounds.min.y = std::min(bounds.min.y, local.y);
    bounds.min.z = std::min(bounds.min.z, local.z);
    bounds.max.x = std::max(bounds.max.x, local.x);
    bounds.max.y = std::max(bounds.max.y, local.y);
    bounds.max.z = std::max(bounds.max.z, local.z);
  }
  return bounds;
}

void addSplit(std::vector<float> &values, const float value, const float min_value,
              const float max_value) {
  if (value > min_value + kEpsilon && value < max_value - kEpsilon) {
    values.push_back(value);
  }
}

void normalizeSplits(std::vector<float> &values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end(),
                           [](const float a, const float b) {
                             return std::abs(a - b) <= kEpsilon;
                           }),
               values.end());
}

Vec2 projectUv(const Vec3 position, const Vec3 normal, const float uv_scale) {
  const float scale = uv_scale <= 0.0001f ? 1.0f : uv_scale;
  const Vec3 abs_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z) {
    return {position.x / scale, position.z / scale};
  }
  if (abs_normal.x >= abs_normal.z) {
    return {position.z / scale, position.y / scale};
  }
  return {position.x / scale, position.y / scale};
}

void appendQuad(CpuMesh &mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal,
                const BrushLevelMeshOptions &options) {
  normal = normalize(normal);
  if (dot(cross(b - a, c - a), normal) < 0.0f) {
    std::swap(b, d);
  }

  const auto displace = [&](Vec3 point) {
    if (options.noise_strength <= 0.0001f) {
      return point;
    }
    const float frequency = std::max(options.noise_frequency, 0.001f);
    const float sample =
        ridgedNoise(point * frequency, static_cast<std::uint32_t>(options.noise_seed), 4) * 2.0f -
        1.0f;
    return point + normal * (sample * options.noise_strength);
  };

  a = displace(a);
  b = displace(b);
  c = displace(c);
  d = displace(d);

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, projectUv(a, normal, options.uv_scale)});
  mesh.vertices.push_back({b, normal, projectUv(b, normal, options.uv_scale)});
  mesh.vertices.push_back({c, normal, projectUv(c, normal, options.uv_scale)});
  mesh.vertices.push_back({d, normal, projectUv(d, normal, options.uv_scale)});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u,
                                           base + 3u});
}

bool pointInsideAny(const std::vector<BrushFrame> &frames, const Vec3 point) {
  return std::any_of(frames.begin(), frames.end(),
                     [&](const BrushFrame &frame) { return contains(frame, point, -kEpsilon); });
}

void appendSolidFaces(CpuMesh &mesh, const BrushFrame &solid,
                      const std::vector<BrushFrame> &solids,
                      const std::vector<BrushFrame> &airs,
                      const BrushLevelMeshOptions &options) {
  for (int normal_axis = 0; normal_axis < 3; ++normal_axis) {
    const int u_axis = (normal_axis + 1) % 3;
    const int v_axis = (normal_axis + 2) % 3;
    for (const float sign : {-1.0f, 1.0f}) {
      const float fixed = component(solid.brush.half_extents, normal_axis) * sign;
      std::vector<float> us{-component(solid.brush.half_extents, u_axis),
                            component(solid.brush.half_extents, u_axis)};
      std::vector<float> vs{-component(solid.brush.half_extents, v_axis),
                            component(solid.brush.half_extents, v_axis)};

      for (const BrushFrame &air : airs) {
        const LocalBounds air_bounds = boundsOfInFrame(solid, air);
        if (fixed < component(air_bounds.min, normal_axis) - kEpsilon ||
            fixed > component(air_bounds.max, normal_axis) + kEpsilon) {
          continue;
        }
        addSplit(us, std::clamp(component(air_bounds.min, u_axis), us.front(), us.back()),
                 us.front(), us.back());
        addSplit(us, std::clamp(component(air_bounds.max, u_axis), us.front(), us.back()),
                 us.front(), us.back());
        addSplit(vs, std::clamp(component(air_bounds.min, v_axis), vs.front(), vs.back()),
                 vs.front(), vs.back());
        addSplit(vs, std::clamp(component(air_bounds.max, v_axis), vs.front(), vs.back()),
                 vs.front(), vs.back());
      }

      normalizeSplits(us);
      normalizeSplits(vs);
      for (std::size_t ui = 0; ui + 1u < us.size(); ++ui) {
        for (std::size_t vi = 0; vi + 1u < vs.size(); ++vi) {
          const float u0 = us[ui];
          const float u1 = us[ui + 1u];
          const float v0 = vs[vi];
          const float v1 = vs[vi + 1u];
          const Vec3 center_local =
              withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis,
                                          (u0 + u1) * 0.5f),
                            v_axis, (v0 + v1) * 0.5f);
          const Vec3 center = toWorld(solid, center_local);
          if (pointInsideAny(airs, center)) {
            continue;
          }

          bool hidden_by_solid = false;
          for (const BrushFrame &other : solids) {
            if (&other == &solid) {
              continue;
            }
            if (contains(other, center + solid.axis[normal_axis] * (sign * kEpsilon * 4.0f),
                         -kEpsilon)) {
              hidden_by_solid = true;
              break;
            }
          }
          if (hidden_by_solid) {
            continue;
          }

          const Vec3 p00 = toWorld(
              solid, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis, u0),
                                   v_axis, v0));
          const Vec3 p10 = toWorld(
              solid, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis, u1),
                                   v_axis, v0));
          const Vec3 p11 = toWorld(
              solid, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis, u1),
                                   v_axis, v1));
          const Vec3 p01 = toWorld(
              solid, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis, u0),
                                   v_axis, v1));
          appendQuad(mesh, p00, p10, p11, p01, solid.axis[normal_axis] * sign, options);
        }
      }
    }
  }
}

void appendAirBoundaryFaces(CpuMesh &mesh, const BrushFrame &air,
                            const std::vector<BrushFrame> &solids,
                            const BrushLevelMeshOptions &options) {
  for (int normal_axis = 0; normal_axis < 3; ++normal_axis) {
    const int u_axis = (normal_axis + 1) % 3;
    const int v_axis = (normal_axis + 2) % 3;
    for (const float sign : {-1.0f, 1.0f}) {
      const float fixed = component(air.brush.half_extents, normal_axis) * sign;
      const Vec3 center_local =
          withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis, 0.0f),
                        v_axis, 0.0f);
      const Vec3 center = toWorld(air, center_local);
      if (!pointInsideAny(solids, center - air.axis[normal_axis] * (sign * kEpsilon * 4.0f))) {
        continue;
      }

      const float u0 = -component(air.brush.half_extents, u_axis);
      const float u1 = component(air.brush.half_extents, u_axis);
      const float v0 = -component(air.brush.half_extents, v_axis);
      const float v1 = component(air.brush.half_extents, v_axis);
      const Vec3 p00 =
          toWorld(air, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis,
                                                   u0),
                                     v_axis, v0));
      const Vec3 p10 =
          toWorld(air, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis,
                                                   u1),
                                     v_axis, v0));
      const Vec3 p11 =
          toWorld(air, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis,
                                                   u1),
                                     v_axis, v1));
      const Vec3 p01 =
          toWorld(air, withComponent(withComponent(withComponent({}, normal_axis, fixed), u_axis,
                                                   u0),
                                     v_axis, v1));
      appendQuad(mesh, p00, p10, p11, p01, air.axis[normal_axis] * -sign, options);
    }
  }
}

} // namespace

CpuMesh buildBrushLevelMesh(const std::vector<BrushBox> &brushes,
                            const BrushLevelMeshOptions options) {
  if (brushes.empty()) {
    throw std::invalid_argument("Brush mesh build requires at least one brush.");
  }

  std::vector<BrushFrame> solids;
  std::vector<BrushFrame> airs;
  solids.reserve(brushes.size());
  airs.reserve(brushes.size());
  for (const BrushBox &brush : brushes) {
    if (brush.half_extents.x <= 0.0f || brush.half_extents.y <= 0.0f ||
        brush.half_extents.z <= 0.0f) {
      throw std::invalid_argument("Brush half extents must be positive.");
    }
    if (brush.volume == BrushVolume::Solid) {
      solids.push_back(makeFrame(brush));
    } else {
      airs.push_back(makeFrame(brush));
    }
  }
  if (solids.empty()) {
    throw std::invalid_argument("Brush mesh build requires at least one solid brush.");
  }

  CpuMesh mesh;
  for (const BrushFrame &solid : solids) {
    appendSolidFaces(mesh, solid, solids, airs, options);
  }
  for (const BrushFrame &air : airs) {
    appendAirBoundaryFaces(mesh, air, solids, options);
  }

  if (mesh.indices.empty()) {
    throw std::runtime_error("Brush world produced no renderable geometry.");
  }
  return prepareMeshForRendering(std::move(mesh), options.mesh_options);
}

} // namespace aster
