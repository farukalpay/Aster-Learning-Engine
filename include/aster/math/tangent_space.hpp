// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/geometry.hpp"

namespace aster {

struct TangentFrame {
  Vec3 normal{0.0f, 1.0f, 0.0f};
  Vec3 tangent{1.0f, 0.0f, 0.0f};
  Vec3 bitangent{0.0f, 0.0f, 1.0f};
  float handedness = 1.0f;
};

[[nodiscard]] inline TangentFrame makeTangentFrame(const Vec3 normal,
                                                   const Vec3 tangent_hint = {1.0f, 0.0f, 0.0f}) {
  TangentFrame frame;
  frame.normal = normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  Vec3 tangent = tangent_hint - frame.normal * dot(frame.normal, tangent_hint);
  if (lengthSquared(tangent) <= 0.000001f) {
    const Vec3 reference =
        std::abs(frame.normal.y) > 0.80f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
    tangent = cross(reference, frame.normal);
  }
  frame.tangent = normalizeOr(tangent, {1.0f, 0.0f, 0.0f});
  frame.bitangent = normalizeOr(cross(frame.normal, frame.tangent), {0.0f, 0.0f, 1.0f});
  frame.handedness = dot(cross(frame.normal, frame.tangent), frame.bitangent) < 0.0f ? -1.0f
                                                                                     : 1.0f;
  return frame;
}

[[nodiscard]] inline TangentFrame tangentFrameFromTriangle(const Vec3 a, const Vec3 b,
                                                           const Vec3 c, const Vec2 uv_a,
                                                           const Vec2 uv_b, const Vec2 uv_c) {
  const Vec3 edge_ab = b - a;
  const Vec3 edge_ac = c - a;
  const Vec2 uv_ab = uv_b - uv_a;
  const Vec2 uv_ac = uv_c - uv_a;
  const Vec3 normal = normalizeOr(cross(edge_ab, edge_ac), {0.0f, 1.0f, 0.0f});
  const float det = uv_ab.x * uv_ac.y - uv_ac.x * uv_ab.y;
  if (std::abs(det) <= 0.000001f) {
    return makeTangentFrame(normal);
  }
  const float inv = 1.0f / det;
  const Vec3 tangent = (edge_ab * uv_ac.y - edge_ac * uv_ab.y) * inv;
  const Vec3 bitangent = (edge_ac * uv_ab.x - edge_ab * uv_ac.x) * inv;
  TangentFrame frame = makeTangentFrame(normal, tangent);
  frame.handedness = dot(cross(frame.normal, frame.tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
  frame.bitangent = cross(frame.normal, frame.tangent) * frame.handedness;
  return frame;
}

} // namespace aster
