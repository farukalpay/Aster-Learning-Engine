// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/geometry.hpp"
#include "aster/math/mat4.hpp"

namespace aster {

struct TangentFrame {
  Normal normal{0.0f, 1.0f, 0.0f};
  Direction tangent{1.0f, 0.0f, 0.0f};
  Direction bitangent{0.0f, 0.0f, 1.0f};
  float handedness = 1.0f;
};

[[nodiscard]] inline TangentFrame makeTangentFrame(const Vec3 normal,
                                                   const Vec3 tangent_hint = {1.0f, 0.0f, 0.0f}) {
  TangentFrame frame;
  frame.normal = Normal{normalizeOr(normal, {0.0f, 1.0f, 0.0f})};
  Vec3 tangent = tangent_hint - frame.normal.value * dot(frame.normal.value, tangent_hint);
  if (lengthSquared(tangent) <= 0.000001f) {
    const Vec3 reference =
        std::abs(frame.normal.y) > 0.80f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
    tangent = cross(reference, frame.normal.value);
  }
  frame.tangent = Direction{normalizeOr(tangent, {1.0f, 0.0f, 0.0f})};
  frame.bitangent = Direction{normalizeOr(cross(frame.normal.value, frame.tangent.value),
                                          {0.0f, 0.0f, 1.0f})};
  frame.handedness =
      dot(cross(frame.normal.value, frame.tangent.value), frame.bitangent.value) < 0.0f ? -1.0f
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
  frame.handedness =
      dot(cross(frame.normal.value, frame.tangent.value), bitangent) < 0.0f ? -1.0f : 1.0f;
  frame.bitangent = Direction{cross(frame.normal.value, frame.tangent.value) * frame.handedness};
  return frame;
}

[[nodiscard]] inline MathResult<TangentFrame> transformTangentFrame(
    const TangentFrame frame, const WorldFromLocal world_from_local,
    const MathPolicy policy = defaultMathPolicy()) {
  const MathResult<WorldNormalFromLocal> normal_from_local =
      worldNormalFromLocal(world_from_local, policy);
  if (!normal_from_local) {
    return MathResult<TangentFrame>::failure(normal_from_local.diagnostics.error,
                                             normal_from_local.diagnostics.message);
  }

  TangentFrame out;
  out.normal = transformNormal(frame.normal, normal_from_local.value);
  Vec3 tangent = transformVector(world_from_local.value, frame.tangent.value);
  tangent = tangent - out.normal.value * dot(out.normal.value, tangent);
  if (length(tangent) <= 0.000001f) {
    const Vec3 reference =
        std::abs(out.normal.y) > 0.80f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
    tangent = cross(reference, out.normal.value);
  }
  out.tangent = Direction{normalizeOr(tangent, {1.0f, 0.0f, 0.0f})};
  const float scale_handedness =
      determinant(upperLeftMat3(world_from_local.value)) < 0.0f ? -1.0f : 1.0f;
  out.handedness = frame.handedness * scale_handedness;
  out.bitangent = Direction{cross(out.normal.value, out.tangent.value) * out.handedness};
  return MathResult<TangentFrame>::success(out);
}

} // namespace aster
