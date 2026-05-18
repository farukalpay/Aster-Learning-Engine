// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/physics_world.hpp"

#include "aster/core/profiler.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace aster {
namespace {

constexpr float kEpsilon = 0.000001f;

struct Aabb {
  Vec3 min{};
  Vec3 max{};
};

struct ContactCandidate {
  bool hit = false;
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float penetration = 0.0f;
};

struct SegmentTriangleClosest {
  Vec3 segment_point{};
  Vec3 triangle_point{};
  float distance_sq = std::numeric_limits<float>::infinity();
};

struct BroadphaseEntry {
  std::size_t index = 0u;
  Aabb bounds{};
};

float safeMass(const float mass) {
  return mass > kEpsilon ? mass : 1.0f;
}

Vec3 absVec(const Vec3 value) {
  return {std::abs(value.x), std::abs(value.y), std::abs(value.z)};
}

Vec3 clampVec(const Vec3 value, const Vec3 low, const Vec3 high) {
  return {clamp(value.x, low.x, high.x), clamp(value.y, low.y, high.y),
          clamp(value.z, low.z, high.z)};
}

Vec3 transformPhysicsPoint(const Transform &transform, const Vec3 point) {
  return rotate(transform.rotation, point * transform.scale) + transform.position;
}

float component(const Vec3 value, const int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

Vec3 axisNormal(const int axis, const float sign) {
  if (axis == 0) {
    return {sign, 0.0f, 0.0f};
  }
  if (axis == 1) {
    return {0.0f, sign, 0.0f};
  }
  return {0.0f, 0.0f, sign};
}

float bodyInverseMass(const PhysicsBody &body) {
  return body.type == PhysicsBodyType::Dynamic ? body.inverse_mass : 0.0f;
}

float supportExtentY(const PhysicsBody &body) {
  if (body.shape == PhysicsShapeType::Sphere) {
    return body.radius;
  }
  if (body.shape == PhysicsShapeType::Capsule) {
    return body.half_extents.y + body.radius;
  }
  return body.half_extents.y;
}

Aabb bodyAabb(const PhysicsBody &body) {
  if (body.shape == PhysicsShapeType::Sphere) {
    const Vec3 extents{body.radius, body.radius, body.radius};
    return {body.position - extents, body.position + extents};
  }
  if (body.shape == PhysicsShapeType::Capsule) {
    const Vec3 extents{body.radius, body.half_extents.y + body.radius, body.radius};
    return {body.position - extents, body.position + extents};
  }
  if (body.shape == PhysicsShapeType::TriangleMesh) {
    return {body.mesh_bounds_min, body.mesh_bounds_max};
  }
  return {body.position - body.half_extents, body.position + body.half_extents};
}

Aabb expandAabb(const Aabb box, const float amount) {
  const Vec3 expansion{amount, amount, amount};
  return {box.min - expansion, box.max + expansion};
}

Aabb triangleAabb(const PhysicsMeshTriangle &triangle) {
  return {{std::min({triangle.a.x, triangle.b.x, triangle.c.x}),
           std::min({triangle.a.y, triangle.b.y, triangle.c.y}),
           std::min({triangle.a.z, triangle.b.z, triangle.c.z})},
          {std::max({triangle.a.x, triangle.b.x, triangle.c.x}),
           std::max({triangle.a.y, triangle.b.y, triangle.c.y}),
           std::max({triangle.a.z, triangle.b.z, triangle.c.z})}};
}

Aabb segmentAabb(const Vec3 a, const Vec3 b) {
  return {{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)},
          {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}};
}

bool contains(const Aabb box, const Vec3 point) {
  return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
         point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

bool sphereOverlapsAabb(const Vec3 center, const float radius, const Aabb box) {
  const Vec3 closest = clampVec(center, box.min, box.max);
  return dot(center - closest, center - closest) <= radius * radius;
}

void expandAabbByPoint(Aabb &box, const Vec3 point) {
  box.min.x = std::min(box.min.x, point.x);
  box.min.y = std::min(box.min.y, point.y);
  box.min.z = std::min(box.min.z, point.z);
  box.max.x = std::max(box.max.x, point.x);
  box.max.y = std::max(box.max.y, point.y);
  box.max.z = std::max(box.max.z, point.z);
}

std::vector<PhysicsMeshTriangle> prepareMeshTriangles(const CpuMesh &mesh,
                                                      const Transform &transform, Aabb &bounds) {
  std::vector<PhysicsMeshTriangle> triangles;
  triangles.reserve(mesh.indices.size() / 3u);
  bounds = {{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::infinity()},
            {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}};

  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }

    PhysicsMeshTriangle triangle;
    triangle.a = transformPhysicsPoint(transform, mesh.vertices[ia].position);
    triangle.b = transformPhysicsPoint(transform, mesh.vertices[ib].position);
    triangle.c = transformPhysicsPoint(transform, mesh.vertices[ic].position);
    triangle.normal = normalize(cross(triangle.b - triangle.a, triangle.c - triangle.a));
    const float area_scale = length(cross(triangle.b - triangle.a, triangle.c - triangle.a));
    if (area_scale <= kEpsilon) {
      continue;
    }

    triangles.push_back(triangle);
    expandAabbByPoint(bounds, triangle.a);
    expandAabbByPoint(bounds, triangle.b);
    expandAabbByPoint(bounds, triangle.c);
  }

  if (triangles.empty()) {
    bounds = {};
  }
  return triangles;
}

bool overlaps(const Aabb lhs, const Aabb rhs) {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y &&
         lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

bool filtersAllowCollision(const PhysicsBody &lhs, const PhysicsBody &rhs) {
  return (lhs.filter.collides_with & rhs.filter.layer_bits) != 0u &&
         (rhs.filter.collides_with & lhs.filter.layer_bits) != 0u;
}

bool filtersAllowQuery(const PhysicsQueryFilter &filter, const PhysicsBodyHandle handle,
                       const PhysicsBody &body) {
  if (!body.active || !body.filter.query_enabled) {
    return false;
  }
  if (samePhysicsHandle(filter.ignore_body, handle)) {
    return false;
  }
  if (body.filter.sensor && !filter.include_sensors) {
    return false;
  }
  return (filter.collides_with & body.filter.layer_bits) != 0u;
}

PhysicsMaterial combineMaterial(const PhysicsBody &lhs, const PhysicsBody &rhs) {
  return {(lhs.material.friction + rhs.material.friction) * 0.5f,
          std::max(lhs.material.restitution, rhs.material.restitution)};
}

Vec3 axisNormalForInsidePoint(const Vec3 offset, const Vec3 half_extents) {
  const Vec3 remaining = half_extents - absVec(offset);
  if (remaining.x <= remaining.y && remaining.x <= remaining.z) {
    return {offset.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
  }
  if (remaining.y <= remaining.z) {
    return {0.0f, offset.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
  }
  return {0.0f, 0.0f, offset.z >= 0.0f ? 1.0f : -1.0f};
}

float axisComponent(const Vec3 value, const Vec3 normal) {
  return std::abs(normal.x) > 0.0f ? value.x : (std::abs(normal.y) > 0.0f ? value.y : value.z);
}

void wake(PhysicsBody &body) {
  if (body.type == PhysicsBodyType::Dynamic) {
    body.sleeping = false;
    body.sleep_timer = 0.0f;
  }
}

Vec3 closestPointOnTriangle(const Vec3 point, const Vec3 a, const Vec3 b, const Vec3 c) {
  const Vec3 ab = b - a;
  const Vec3 ac = c - a;
  const Vec3 ap = point - a;
  const float d1 = dot(ab, ap);
  const float d2 = dot(ac, ap);
  if (d1 <= 0.0f && d2 <= 0.0f) {
    return a;
  }

  const Vec3 bp = point - b;
  const float d3 = dot(ab, bp);
  const float d4 = dot(ac, bp);
  if (d3 >= 0.0f && d4 <= d3) {
    return b;
  }

  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
    const float v = d1 / (d1 - d3);
    return a + ab * v;
  }

  const Vec3 cp = point - c;
  const float d5 = dot(ab, cp);
  const float d6 = dot(ac, cp);
  if (d6 >= 0.0f && d5 <= d6) {
    return c;
  }

  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
    const float w = d2 / (d2 - d6);
    return a + ac * w;
  }

  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
    const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return b + (c - b) * w;
  }

  const float denom = 1.0f / (va + vb + vc);
  const float v = vb * denom;
  const float w = vc * denom;
  return a + ab * v + ac * w;
}

void closestPointsOnSegments(const Vec3 p1, const Vec3 q1, const Vec3 p2, const Vec3 q2,
                             Vec3 &closest1, Vec3 &closest2) {
  const Vec3 d1 = q1 - p1;
  const Vec3 d2 = q2 - p2;
  const Vec3 r = p1 - p2;
  const float a = dot(d1, d1);
  const float e = dot(d2, d2);
  const float f = dot(d2, r);

  float s = 0.0f;
  float t = 0.0f;
  if (a <= kEpsilon && e <= kEpsilon) {
    closest1 = p1;
    closest2 = p2;
    return;
  }
  if (a <= kEpsilon) {
    t = clamp(f / e, 0.0f, 1.0f);
  } else {
    const float c = dot(d1, r);
    if (e <= kEpsilon) {
      s = clamp(-c / a, 0.0f, 1.0f);
    } else {
      const float b = dot(d1, d2);
      const float denom = a * e - b * b;
      if (denom != 0.0f) {
        s = clamp((b * f - c * e) / denom, 0.0f, 1.0f);
      }
      t = (b * s + f) / e;
      if (t < 0.0f) {
        t = 0.0f;
        s = clamp(-c / a, 0.0f, 1.0f);
      } else if (t > 1.0f) {
        t = 1.0f;
        s = clamp((b - c) / a, 0.0f, 1.0f);
      }
    }
  }

  closest1 = p1 + d1 * s;
  closest2 = p2 + d2 * t;
}

bool segmentIntersectsTriangle(const Vec3 p, const Vec3 q, const PhysicsMeshTriangle &triangle,
                               Vec3 &point) {
  const Vec3 direction = q - p;
  const float denom = dot(triangle.normal, direction);
  if (std::abs(denom) <= kEpsilon) {
    return false;
  }
  const float t = dot(triangle.a - p, triangle.normal) / denom;
  if (t < 0.0f || t > 1.0f) {
    return false;
  }
  point = p + direction * t;
  return length(closestPointOnTriangle(point, triangle.a, triangle.b, triangle.c) - point) <=
         0.0005f;
}

SegmentTriangleClosest closestSegmentTriangle(const Vec3 segment_a, const Vec3 segment_b,
                                              const PhysicsMeshTriangle &triangle) {
  SegmentTriangleClosest best;
  Vec3 intersection{};
  if (segmentIntersectsTriangle(segment_a, segment_b, triangle, intersection)) {
    return {intersection, intersection, 0.0f};
  }

  const auto consider = [&best](const Vec3 segment_point, const Vec3 triangle_point) {
    const float distance_sq = dot(segment_point - triangle_point, segment_point - triangle_point);
    if (distance_sq < best.distance_sq) {
      best = {segment_point, triangle_point, distance_sq};
    }
  };

  const Vec3 tri_from_a = closestPointOnTriangle(segment_a, triangle.a, triangle.b, triangle.c);
  consider(segment_a, tri_from_a);
  const Vec3 tri_from_b = closestPointOnTriangle(segment_b, triangle.a, triangle.b, triangle.c);
  consider(segment_b, tri_from_b);

  const Vec3 edges[][2] = {
      {triangle.a, triangle.b}, {triangle.b, triangle.c}, {triangle.c, triangle.a}};
  for (const auto &edge : edges) {
    Vec3 capsule_point{};
    Vec3 edge_point{};
    closestPointsOnSegments(segment_a, segment_b, edge[0], edge[1], capsule_point, edge_point);
    consider(capsule_point, edge_point);
  }

  return best;
}

ContactCandidate sphereSphereContact(const PhysicsBody &a, const PhysicsBody &b) {
  Vec3 delta = a.position - b.position;
  const float distance = length(delta);
  const float radius_sum = a.radius + b.radius;
  if (distance >= radius_sum) {
    return {};
  }

  const Vec3 normal = distance > kEpsilon ? delta / distance : Vec3{0.0f, 1.0f, 0.0f};
  return {true, b.position + normal * b.radius, normal, radius_sum - distance};
}

ContactCandidate sphereBoxContact(const PhysicsBody &sphere, const PhysicsBody &box) {
  const Vec3 box_min = box.position - box.half_extents;
  const Vec3 box_max = box.position + box.half_extents;
  const Vec3 closest = clampVec(sphere.position, box_min, box_max);
  Vec3 delta = sphere.position - closest;
  const float distance = length(delta);

  Vec3 normal{};
  float penetration = 0.0f;
  if (distance <= kEpsilon) {
    normal = axisNormalForInsidePoint(sphere.position - box.position, box.half_extents);
    const float remaining =
        axisComponent(box.half_extents - absVec(sphere.position - box.position), normal);
    penetration = sphere.radius + std::max(remaining, 0.0f);
  } else if (distance < sphere.radius) {
    normal = delta / distance;
    penetration = sphere.radius - distance;
  } else {
    return {};
  }

  return {true, closest, normal, penetration};
}

ContactCandidate capsuleBoxContact(const PhysicsBody &capsule, const PhysicsBody &box) {
  const Vec3 box_min = box.position - box.half_extents;
  const Vec3 box_max = box.position + box.half_extents;
  const float segment_min_y = capsule.position.y - capsule.half_extents.y;
  const float segment_max_y = capsule.position.y + capsule.half_extents.y;
  const Vec3 segment_point{capsule.position.x, clamp(box.position.y, segment_min_y, segment_max_y),
                           capsule.position.z};
  const Vec3 closest = clampVec(segment_point, box_min, box_max);
  const Vec3 delta = segment_point - closest;
  const float distance = length(delta);

  Vec3 normal{};
  float penetration = 0.0f;
  if (distance <= kEpsilon) {
    normal = axisNormalForInsidePoint(segment_point - box.position, box.half_extents);
    const float remaining =
        axisComponent(box.half_extents - absVec(segment_point - box.position), normal);
    penetration = capsule.radius + std::max(remaining, 0.0f);
  } else if (distance < capsule.radius) {
    normal = delta / distance;
    penetration = capsule.radius - distance;
  } else {
    return {};
  }

  return {true, closest, normal, penetration};
}

ContactCandidate sphereTriangleMeshContact(const PhysicsBody &sphere, const PhysicsBody &mesh) {
  ContactCandidate best;
  float best_penetration = 0.0f;
  if (!sphereOverlapsAabb(sphere.position, sphere.radius, bodyAabb(mesh))) {
    return best;
  }
  for (const PhysicsMeshTriangle &triangle : mesh.mesh_triangles) {
    if (!sphereOverlapsAabb(sphere.position, sphere.radius, triangleAabb(triangle))) {
      continue;
    }
    const Vec3 triangle_point =
        closestPointOnTriangle(sphere.position, triangle.a, triangle.b, triangle.c);
    Vec3 delta = sphere.position - triangle_point;
    const float distance = length(delta);
    if (distance >= sphere.radius) {
      continue;
    }

    Vec3 normal = distance > kEpsilon ? delta / distance : triangle.normal;
    if (distance <= kEpsilon &&
        dot(normal, sphere.position - ((triangle.a + triangle.b + triangle.c) / 3.0f)) < 0.0f) {
      normal = normal * -1.0f;
    }
    const float penetration = sphere.radius - distance;
    if (!best.hit || penetration > best_penetration) {
      best = {true, triangle_point, normal, penetration};
      best_penetration = penetration;
    }
  }
  return best;
}

ContactCandidate capsuleTriangleMeshContact(const PhysicsBody &capsule, const PhysicsBody &mesh) {
  ContactCandidate best;
  float best_penetration = 0.0f;
  const Vec3 segment_a{capsule.position.x, capsule.position.y - capsule.half_extents.y,
                       capsule.position.z};
  const Vec3 segment_b{capsule.position.x, capsule.position.y + capsule.half_extents.y,
                       capsule.position.z};
  const Vec3 segment_mid = (segment_a + segment_b) * 0.5f;
  const Aabb capsule_bounds = expandAabb(segmentAabb(segment_a, segment_b), capsule.radius);
  if (!overlaps(capsule_bounds, bodyAabb(mesh))) {
    return best;
  }

  for (const PhysicsMeshTriangle &triangle : mesh.mesh_triangles) {
    if (!overlaps(capsule_bounds, triangleAabb(triangle))) {
      continue;
    }
    const SegmentTriangleClosest closest = closestSegmentTriangle(segment_a, segment_b, triangle);
    const float radius_sq = capsule.radius * capsule.radius;
    if (closest.distance_sq >= radius_sq) {
      continue;
    }

    const float distance = std::sqrt(std::max(closest.distance_sq, 0.0f));
    Vec3 normal = distance > kEpsilon ? (closest.segment_point - closest.triangle_point) / distance
                                      : triangle.normal;
    if (distance <= kEpsilon &&
        dot(normal, segment_mid - ((triangle.a + triangle.b + triangle.c) / 3.0f)) < 0.0f) {
      normal = normal * -1.0f;
    }
    const float penetration = capsule.radius - distance;
    if (!best.hit || penetration > best_penetration) {
      best = {true, closest.triangle_point, normal, penetration};
      best_penetration = penetration;
    }
  }

  return best;
}

ContactCandidate boxBoxContact(const PhysicsBody &a, const PhysicsBody &b) {
  const Vec3 delta = a.position - b.position;
  const Vec3 overlap = a.half_extents + b.half_extents - absVec(delta);
  if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
    return {};
  }

  Vec3 normal{delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
  float penetration = overlap.x;
  if (overlap.y < penetration) {
    normal = {0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
    penetration = overlap.y;
  }
  if (overlap.z < penetration) {
    normal = {0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f};
    penetration = overlap.z;
  }

  return {true, (a.position + b.position) * 0.5f, normal, penetration};
}

ContactCandidate contactFor(const PhysicsBody &a, const PhysicsBody &b) {
  if (a.shape == PhysicsShapeType::Sphere && b.shape == PhysicsShapeType::Sphere) {
    return sphereSphereContact(a, b);
  }
  if (a.shape == PhysicsShapeType::Sphere && b.shape == PhysicsShapeType::Box) {
    return sphereBoxContact(a, b);
  }
  if (a.shape == PhysicsShapeType::Box && b.shape == PhysicsShapeType::Sphere) {
    ContactCandidate contact = sphereBoxContact(b, a);
    contact.normal = contact.normal * -1.0f;
    return contact;
  }
  if (a.shape == PhysicsShapeType::Capsule && b.shape == PhysicsShapeType::Box) {
    return capsuleBoxContact(a, b);
  }
  if (a.shape == PhysicsShapeType::Box && b.shape == PhysicsShapeType::Capsule) {
    ContactCandidate contact = capsuleBoxContact(b, a);
    contact.normal = contact.normal * -1.0f;
    return contact;
  }
  if (a.shape == PhysicsShapeType::Sphere && b.shape == PhysicsShapeType::TriangleMesh) {
    return sphereTriangleMeshContact(a, b);
  }
  if (a.shape == PhysicsShapeType::TriangleMesh && b.shape == PhysicsShapeType::Sphere) {
    ContactCandidate contact = sphereTriangleMeshContact(b, a);
    contact.normal = contact.normal * -1.0f;
    return contact;
  }
  if (a.shape == PhysicsShapeType::Capsule && b.shape == PhysicsShapeType::TriangleMesh) {
    return capsuleTriangleMeshContact(a, b);
  }
  if (a.shape == PhysicsShapeType::TriangleMesh && b.shape == PhysicsShapeType::Capsule) {
    ContactCandidate contact = capsuleTriangleMeshContact(b, a);
    contact.normal = contact.normal * -1.0f;
    return contact;
  }
  if (a.shape == PhysicsShapeType::Box && b.shape == PhysicsShapeType::Box) {
    return boxBoxContact(a, b);
  }
  return {};
}

bool raycastAabb(const Vec3 origin, const Vec3 direction, const float max_distance, const Aabb box,
                 float &distance, Vec3 &normal) {
  float t_min = 0.0f;
  float t_max = max_distance;
  Vec3 enter_normal = direction * -1.0f;

  for (int axis = 0; axis < 3; ++axis) {
    const float origin_axis = component(origin, axis);
    const float direction_axis = component(direction, axis);
    const float min_axis = component(box.min, axis);
    const float max_axis = component(box.max, axis);

    if (std::abs(direction_axis) <= kEpsilon) {
      if (origin_axis < min_axis || origin_axis > max_axis) {
        return false;
      }
      continue;
    }

    const float inverse_direction = 1.0f / direction_axis;
    float t1 = (min_axis - origin_axis) * inverse_direction;
    float t2 = (max_axis - origin_axis) * inverse_direction;
    Vec3 n1 = axisNormal(axis, direction_axis > 0.0f ? -1.0f : 1.0f);
    Vec3 n2 = n1 * -1.0f;
    if (t1 > t2) {
      std::swap(t1, t2);
      std::swap(n1, n2);
    }

    if (t1 > t_min) {
      t_min = t1;
      enter_normal = n1;
    }
    t_max = std::min(t_max, t2);
    if (t_min > t_max) {
      return false;
    }
  }

  if (t_min < 0.0f || t_min > max_distance) {
    return false;
  }
  distance = t_min;
  normal = normalizeOr(enter_normal, {0.0f, 1.0f, 0.0f});
  return true;
}

bool rayTouchesAabb(const Vec3 origin, const Vec3 direction, const float max_distance,
                    const Aabb box) {
  if (contains(box, origin)) {
    return true;
  }
  float distance = 0.0f;
  Vec3 normal{};
  return raycastAabb(origin, direction, max_distance, box, distance, normal);
}

bool raycastSphere(const Vec3 origin, const Vec3 direction, const float max_distance,
                   const Vec3 center, const float radius, float &distance, Vec3 &normal) {
  const Vec3 m = origin - center;
  const float b = dot(m, direction);
  const float c = dot(m, m) - radius * radius;
  if (c > 0.0f && b > 0.0f) {
    return false;
  }

  const float discriminant = b * b - c;
  if (discriminant < 0.0f) {
    return false;
  }

  float t = -b - std::sqrt(discriminant);
  t = std::max(t, 0.0f);
  if (t > max_distance) {
    return false;
  }

  const Vec3 point = origin + direction * t;
  Vec3 n = point - center;
  if (length(n) <= kEpsilon) {
    n = direction * -1.0f;
  }
  distance = t;
  normal = normalizeOr(n, {0.0f, 1.0f, 0.0f});
  return true;
}

bool raycastTriangle(const Vec3 origin, const Vec3 direction, const float max_distance,
                     const PhysicsMeshTriangle &triangle, float &distance, Vec3 &normal) {
  const Vec3 edge1 = triangle.b - triangle.a;
  const Vec3 edge2 = triangle.c - triangle.a;
  const Vec3 pvec = cross(direction, edge2);
  const float det = dot(edge1, pvec);
  if (std::abs(det) <= kEpsilon) {
    return false;
  }

  const float inverse_det = 1.0f / det;
  const Vec3 tvec = origin - triangle.a;
  const float u = dot(tvec, pvec) * inverse_det;
  if (u < 0.0f || u > 1.0f) {
    return false;
  }

  const Vec3 qvec = cross(tvec, edge1);
  const float v = dot(direction, qvec) * inverse_det;
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }

  const float t = dot(edge2, qvec) * inverse_det;
  if (t < 0.0f || t > max_distance) {
    return false;
  }

  distance = t;
  normal = dot(triangle.normal, direction) > 0.0f ? triangle.normal * -1.0f : triangle.normal;
  return true;
}

bool raycastTriangleMesh(const Vec3 origin, const Vec3 direction, const float max_distance,
                         const PhysicsBody &mesh, float &distance, Vec3 &normal) {
  if (!rayTouchesAabb(origin, direction, max_distance, bodyAabb(mesh))) {
    return false;
  }
  bool found = false;
  float closest = max_distance;
  Vec3 closest_normal{};
  for (const PhysicsMeshTriangle &triangle : mesh.mesh_triangles) {
    if (!rayTouchesAabb(origin, direction, closest, triangleAabb(triangle))) {
      continue;
    }
    float triangle_distance = 0.0f;
    Vec3 triangle_normal{};
    if (!raycastTriangle(origin, direction, closest, triangle, triangle_distance,
                         triangle_normal)) {
      continue;
    }
    found = true;
    closest = triangle_distance;
    closest_normal = triangle_normal;
  }
  if (found) {
    distance = closest;
    normal = closest_normal;
  }
  return found;
}

bool sphereCastTriangle(const Vec3 origin, const Vec3 direction, const float max_distance,
                        const float radius, const PhysicsMeshTriangle &triangle, float &distance,
                        Vec3 &normal) {
  const auto sphere_triangle_distance_sq = [&](const Vec3 center) {
    const Vec3 triangle_point = closestPointOnTriangle(center, triangle.a, triangle.b, triangle.c);
    const Vec3 delta = center - triangle_point;
    return dot(delta, delta);
  };

  const float radius_sq = radius * radius;
  if (sphere_triangle_distance_sq(origin) <= radius_sq) {
    distance = 0.0f;
    Vec3 triangle_point = closestPointOnTriangle(origin, triangle.a, triangle.b, triangle.c);
    Vec3 delta = origin - triangle_point;
    normal =
        length(delta) > kEpsilon
            ? normalize(delta)
            : (dot(triangle.normal, direction) > 0.0f ? triangle.normal * -1.0f : triangle.normal);
    return true;
  }

  const Vec3 cast_end = origin + direction * max_distance;
  const SegmentTriangleClosest closest = closestSegmentTriangle(origin, cast_end, triangle);
  if (closest.distance_sq > radius_sq) {
    return false;
  }

  const float closest_along =
      std::clamp(dot(closest.segment_point - origin, direction), 0.0f, max_distance);
  if (closest_along <= kEpsilon) {
    return false;
  }

  float lo = 0.0f;
  float hi = closest_along;
  for (int step = 0; step < 18; ++step) {
    const float mid = (lo + hi) * 0.5f;
    if (sphere_triangle_distance_sq(origin + direction * mid) <= radius_sq) {
      hi = mid;
    } else {
      lo = mid;
    }
  }

  const Vec3 center = origin + direction * hi;
  const Vec3 triangle_point = closestPointOnTriangle(center, triangle.a, triangle.b, triangle.c);
  Vec3 delta = center - triangle_point;
  normal =
      length(delta) > kEpsilon
          ? normalize(delta)
          : (dot(triangle.normal, direction) > 0.0f ? triangle.normal * -1.0f : triangle.normal);
  distance = hi;
  return distance <= max_distance;
}

bool sphereCastTriangleMesh(const Vec3 origin, const Vec3 direction, const float max_distance,
                            const float radius, const PhysicsBody &mesh, float &distance,
                            Vec3 &normal) {
  const Aabb swept_bounds = expandAabb(segmentAabb(origin, origin + direction * max_distance),
                                      radius);
  if (!overlaps(swept_bounds, bodyAabb(mesh))) {
    return false;
  }
  bool found = false;
  float closest = max_distance;
  Vec3 closest_normal{};
  for (const PhysicsMeshTriangle &triangle : mesh.mesh_triangles) {
    if (!overlaps(swept_bounds, expandAabb(triangleAabb(triangle), radius))) {
      continue;
    }
    float triangle_distance = 0.0f;
    Vec3 triangle_normal{};
    if (!sphereCastTriangle(origin, direction, closest, radius, triangle, triangle_distance,
                            triangle_normal)) {
      continue;
    }
    found = true;
    closest = triangle_distance;
    closest_normal = triangle_normal;
  }
  if (found) {
    distance = closest;
    normal = closest_normal;
  }
  return found;
}

bool sphereOverlapsBody(const Vec3 center, const float radius, const PhysicsBody &body) {
  if (body.shape == PhysicsShapeType::Sphere) {
    return length(center - body.position) <= radius + body.radius;
  }
  if (body.shape == PhysicsShapeType::TriangleMesh) {
    if (!sphereOverlapsAabb(center, radius, bodyAabb(body))) {
      return false;
    }
    PhysicsBody sphere;
    sphere.shape = PhysicsShapeType::Sphere;
    sphere.position = center;
    sphere.radius = radius;
    return sphereTriangleMeshContact(sphere, body).hit;
  }

  const Aabb box = bodyAabb(body);
  const Vec3 closest = clampVec(center, box.min, box.max);
  return length(center - closest) <= radius;
}

} // namespace

bool samePhysicsHandle(const PhysicsBodyHandle lhs, const PhysicsBodyHandle rhs) {
  return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

void PhysicsWorld::clear() {
  bodies_.clear();
  constraints_.clear();
  fluid_volumes_.clear();
  contacts_.clear();
  broadphase_pairs_.clear();
}

void PhysicsWorld::setSettings(const PhysicsSettings settings) {
  settings_ = settings;
  settings_.solver_iterations = std::max(1, settings_.solver_iterations);
  settings_.max_step = std::max(settings_.max_step, 0.001f);
  settings_.sleep_linear_threshold = std::max(0.0f, settings_.sleep_linear_threshold);
  settings_.sleep_time_threshold = std::max(0.0f, settings_.sleep_time_threshold);
}

PhysicsBodyHandle PhysicsWorld::addBody(const PhysicsBodyDesc &desc) {
  if ((desc.shape == PhysicsShapeType::Sphere || desc.shape == PhysicsShapeType::Capsule) &&
      desc.radius <= 0.0f) {
    throw std::invalid_argument("Sphere and capsule physics bodies require radius > 0.");
  }
  if ((desc.shape == PhysicsShapeType::Box) &&
      (desc.half_extents.x <= 0.0f || desc.half_extents.y <= 0.0f || desc.half_extents.z <= 0.0f)) {
    throw std::invalid_argument("Box physics body requires positive half extents.");
  }
  if (desc.shape == PhysicsShapeType::Capsule && desc.half_extents.y < 0.0f) {
    throw std::invalid_argument("Capsule physics body requires non-negative half height.");
  }
  if (desc.shape == PhysicsShapeType::TriangleMesh) {
    if (desc.type != PhysicsBodyType::Static) {
      throw std::invalid_argument("Triangle mesh physics bodies must be static.");
    }
    if (desc.mesh == nullptr || desc.mesh->vertices.empty() || desc.mesh->indices.size() < 3u) {
      throw std::invalid_argument("Triangle mesh physics bodies require indexed mesh geometry.");
    }
  }

  PhysicsBody body;
  body.type = desc.type;
  body.shape = desc.shape;
  body.position = desc.position;
  body.previous_position = desc.position;
  body.half_extents = desc.half_extents;
  body.radius = desc.radius;
  body.inverse_mass = desc.type == PhysicsBodyType::Dynamic ? 1.0f / safeMass(desc.mass) : 0.0f;
  body.material = desc.material;
  body.linear_damping = clamp(desc.linear_damping, 0.0f, 1.0f);
  body.filter = desc.filter;
  body.allow_sleep = desc.allow_sleep;
  body.mesh_double_sided = desc.mesh_double_sided;

  if (desc.shape == PhysicsShapeType::TriangleMesh) {
    Aabb bounds;
    body.mesh_triangles = prepareMeshTriangles(*desc.mesh, desc.mesh_transform, bounds);
    if (body.mesh_triangles.empty()) {
      throw std::invalid_argument("Triangle mesh physics body contains no valid triangles.");
    }
    body.mesh_bounds_min = bounds.min;
    body.mesh_bounds_max = bounds.max;
  }

  bodies_.push_back(body);
  return {static_cast<std::uint32_t>(bodies_.size() - 1u), body.generation};
}

PhysicsConstraintHandle PhysicsWorld::addDistanceConstraint(const DistanceConstraintDesc &desc) {
  if (!valid(desc.body)) {
    throw std::invalid_argument("Distance constraint body handle is invalid.");
  }
  if (desc.distance <= 0.0f) {
    throw std::invalid_argument("Distance constraint requires distance > 0.");
  }

  DistanceConstraint constraint;
  constraint.desc = desc;
  constraint.desc.stiffness = clamp(desc.stiffness, 0.0f, 1.0f);
  constraint.desc.damping = clamp(desc.damping, 0.0f, 1.0f);
  constraints_.push_back(constraint);
  wakeBody(desc.body);
  return {static_cast<std::uint32_t>(constraints_.size() - 1u), constraint.generation};
}

PhysicsFluidHandle PhysicsWorld::addFluidVolume(const PhysicsFluidVolumeDesc &desc) {
  if (desc.half_extents.x <= 0.0f || desc.half_extents.y <= 0.0f || desc.half_extents.z <= 0.0f ||
      desc.density < 0.0f || desc.linear_drag < 0.0f) {
    throw std::invalid_argument("Fluid volume requires positive extents and non-negative forces.");
  }

  FluidVolume volume;
  volume.desc = desc;
  fluid_volumes_.push_back(volume);
  return {static_cast<std::uint32_t>(fluid_volumes_.size() - 1u), volume.generation};
}

bool PhysicsWorld::removeBody(const PhysicsBodyHandle handle) {
  if (!valid(handle)) {
    return false;
  }
  PhysicsBody &target = bodies_[handle.index];
  target.active = false;
  target.sleeping = true;
  target.velocity = {};
  target.force = {};
  ++target.generation;
  contacts_.clear();
  broadphase_pairs_.clear();
  return true;
}

bool PhysicsWorld::removeConstraint(const PhysicsConstraintHandle handle) {
  if (!valid(handle)) {
    return false;
  }
  DistanceConstraint &constraint = constraints_[handle.index];
  constraint.active = false;
  ++constraint.generation;
  return true;
}

bool PhysicsWorld::valid(const PhysicsBodyHandle handle) const {
  return handle.index < bodies_.size() && bodies_[handle.index].active &&
         bodies_[handle.index].generation == handle.generation;
}

bool PhysicsWorld::valid(const PhysicsConstraintHandle handle) const {
  return handle.index < constraints_.size() && constraints_[handle.index].active &&
         constraints_[handle.index].generation == handle.generation;
}

bool PhysicsWorld::valid(const PhysicsFluidHandle handle) const {
  return handle.index < fluid_volumes_.size() && fluid_volumes_[handle.index].active &&
         fluid_volumes_[handle.index].generation == handle.generation;
}

PhysicsBody &PhysicsWorld::body(const PhysicsBodyHandle handle) {
  if (!valid(handle)) {
    throw std::invalid_argument("Physics body handle is invalid.");
  }
  return bodies_[handle.index];
}

const PhysicsBody &PhysicsWorld::body(const PhysicsBodyHandle handle) const {
  if (!valid(handle)) {
    throw std::invalid_argument("Physics body handle is invalid.");
  }
  return bodies_[handle.index];
}

void PhysicsWorld::applyForce(const PhysicsBodyHandle handle, const Vec3 force) {
  PhysicsBody &target = body(handle);
  if (target.type == PhysicsBodyType::Dynamic) {
    wake(target);
    target.force = target.force + force;
  }
}

void PhysicsWorld::applyImpulse(const PhysicsBodyHandle handle, const Vec3 impulse) {
  PhysicsBody &target = body(handle);
  if (target.type == PhysicsBodyType::Dynamic) {
    wake(target);
    target.velocity = target.velocity + impulse * target.inverse_mass;
  }
}

void PhysicsWorld::setVelocity(const PhysicsBodyHandle handle, const Vec3 velocity) {
  PhysicsBody &target = body(handle);
  if (target.type == PhysicsBodyType::Dynamic) {
    target.velocity = velocity;
    wake(target);
  }
}

void PhysicsWorld::setPosition(const PhysicsBodyHandle handle, const Vec3 position) {
  PhysicsBody &target = body(handle);
  target.position = position;
  target.previous_position = position;
  wake(target);
}

void PhysicsWorld::wakeBody(const PhysicsBodyHandle handle) {
  if (valid(handle)) {
    wake(bodies_[handle.index]);
  }
}

void PhysicsWorld::step(const float dt) {
  if (dt <= 0.0f) {
    return;
  }

  const int substeps = std::max(1, static_cast<int>(std::ceil(dt / settings_.max_step)));
  const float h = dt / static_cast<float>(substeps);
  for (int i = 0; i < substeps; ++i) {
    applyFluidForces();
    integrate(h);
    for (int iteration = 0; iteration < settings_.solver_iterations; ++iteration) {
      solveConstraints(h);
      contacts_.clear();
      solveCollisions();
    }
    updateSleeping(h);
  }
}

bool PhysicsWorld::raycast(const PhysicsRay &ray, PhysicsRayHit &hit) const {
  ASTER_PROFILE_SCOPE("PhysicsWorld::raycast");
  if (ray.max_distance <= 0.0f) {
    return false;
  }

  const MathResult<Vec3> direction_result = safeNormalize(ray.direction);
  if (!direction_result) {
    return false;
  }
  const Vec3 direction = direction_result.value;

  bool found = false;
  float closest_distance = ray.max_distance;
  PhysicsRayHit closest_hit{};
  for (std::size_t index = 0; index < bodies_.size(); ++index) {
    const PhysicsBody &body = bodies_[index];
    const PhysicsBodyHandle handle{static_cast<std::uint32_t>(index), body.generation};
    if (!filtersAllowQuery(ray.filter, handle, body)) {
      continue;
    }

    float distance = std::numeric_limits<float>::max();
    Vec3 normal{};
    bool shape_hit = false;
    if (body.shape == PhysicsShapeType::Sphere) {
      shape_hit = raycastSphere(ray.origin, direction, closest_distance, body.position, body.radius,
                                distance, normal);
    } else if (body.shape == PhysicsShapeType::TriangleMesh) {
      shape_hit =
          raycastTriangleMesh(ray.origin, direction, closest_distance, body, distance, normal);
    } else {
      shape_hit =
          raycastAabb(ray.origin, direction, closest_distance, bodyAabb(body), distance, normal);
    }

    if (!shape_hit || distance > closest_distance) {
      continue;
    }

    found = true;
    closest_distance = distance;
    closest_hit.body = handle;
    closest_hit.distance = distance;
    closest_hit.fraction = distance / ray.max_distance;
    closest_hit.normal = normal;
    closest_hit.point = ray.origin + direction * distance;
  }

  if (found) {
    hit = closest_hit;
  }
  return found;
}

bool PhysicsWorld::castSphere(const PhysicsSphereCast &cast, PhysicsShapeCastHit &hit) const {
  ASTER_PROFILE_SCOPE("PhysicsWorld::castSphere");
  if (cast.radius <= 0.0f) {
    throw std::invalid_argument("Sphere cast requires radius > 0.");
  }

  const float cast_distance = length(cast.displacement);
  if (cast_distance <= kEpsilon) {
    return false;
  }

  const Vec3 direction = cast.displacement / cast_distance;
  bool found = false;
  float closest_distance = cast_distance;
  PhysicsShapeCastHit closest_hit{};
  for (std::size_t index = 0; index < bodies_.size(); ++index) {
    const PhysicsBody &body = bodies_[index];
    const PhysicsBodyHandle handle{static_cast<std::uint32_t>(index), body.generation};
    if (!filtersAllowQuery(cast.filter, handle, body)) {
      continue;
    }

    float distance = std::numeric_limits<float>::max();
    Vec3 normal{};
    bool shape_hit = false;
    if (body.shape == PhysicsShapeType::Sphere) {
      shape_hit = raycastSphere(cast.origin, direction, closest_distance, body.position,
                                cast.radius + body.radius, distance, normal);
    } else if (body.shape == PhysicsShapeType::TriangleMesh) {
      shape_hit = sphereCastTriangleMesh(cast.origin, direction, closest_distance, cast.radius,
                                         body, distance, normal);
    } else {
      shape_hit = raycastAabb(cast.origin, direction, closest_distance,
                              expandAabb(bodyAabb(body), cast.radius), distance, normal);
    }

    if (!shape_hit || distance > closest_distance) {
      continue;
    }

    found = true;
    closest_distance = distance;
    closest_hit.body = handle;
    closest_hit.distance = distance;
    closest_hit.fraction = distance / cast_distance;
    closest_hit.normal = normal;
    closest_hit.point = cast.origin + direction * distance - normal * cast.radius;
  }

  if (found) {
    hit = closest_hit;
  }
  return found;
}

std::vector<PhysicsOverlapHit> PhysicsWorld::overlapSphere(const Vec3 center, const float radius,
                                                           const PhysicsQueryFilter filter) const {
  ASTER_PROFILE_SCOPE("PhysicsWorld::overlapSphere");
  if (radius <= 0.0f) {
    throw std::invalid_argument("Sphere overlap requires radius > 0.");
  }

  std::vector<PhysicsOverlapHit> hits;
  for (std::size_t index = 0; index < bodies_.size(); ++index) {
    const PhysicsBody &body = bodies_[index];
    const PhysicsBodyHandle handle{static_cast<std::uint32_t>(index), body.generation};
    if (!filtersAllowQuery(filter, handle, body)) {
      continue;
    }
    if (sphereOverlapsBody(center, radius, body)) {
      hits.push_back({handle});
    }
  }
  return hits;
}

PhysicsFluidSample PhysicsWorld::sampleFluid(const PhysicsBodyHandle handle) const {
  if (!valid(handle)) {
    return {};
  }

  const PhysicsBody &target = bodies_[handle.index];
  PhysicsFluidSample sample;
  const Aabb target_box = bodyAabb(target);
  const float target_height = std::max(target_box.max.y - target_box.min.y, kEpsilon);
  for (const FluidVolume &volume : fluid_volumes_) {
    if (!volume.active || (volume.desc.affects_layers & target.filter.layer_bits) == 0u) {
      continue;
    }

    const Aabb fluid_box{volume.desc.center - volume.desc.half_extents,
                         volume.desc.center + volume.desc.half_extents};
    if (!overlaps(target_box, fluid_box)) {
      continue;
    }

    const float surface = std::min(volume.desc.surface_y, fluid_box.max.y);
    const float submerged_height = clamp(surface - target_box.min.y, 0.0f, target_height);
    const float submersion = submerged_height / target_height;
    if (submersion <= 0.0f) {
      continue;
    }

    sample.submerged = true;
    sample.submersion = std::max(sample.submersion, submersion);
    if (submersion >= sample.submersion) {
      sample.surface_y = surface;
      sample.depth_below_surface = std::max(surface - target.position.y, 0.0f);
    }
    sample.flow = sample.flow + volume.desc.flow * submersion;
  }
  return sample;
}

CharacterMoveResult PhysicsWorld::moveCharacter(const PhysicsBodyHandle handle,
                                                const CharacterMoveInput &input,
                                                const CharacterControllerSettings &settings,
                                                const float dt) {
  if (!valid(handle)) {
    throw std::invalid_argument("Character body handle is invalid.");
  }
  PhysicsBody &character = bodies_[handle.index];
  if (character.type != PhysicsBodyType::Dynamic) {
    throw std::invalid_argument("Character controller requires a dynamic body.");
  }

  const float support = supportExtentY(character);
  PhysicsRayHit ground_hit;
  PhysicsRay ground_ray;
  ground_ray.origin = character.position;
  ground_ray.direction = {0.0f, -1.0f, 0.0f};
  ground_ray.max_distance = support + std::max(settings.ground_probe_distance, 0.0f);
  ground_ray.filter.collides_with = character.filter.collides_with;
  ground_ray.filter.ignore_body = handle;

  const float slope_limit = std::cos(std::max(settings.max_slope_angle, 0.0f));
  const bool has_ground = raycast(ground_ray, ground_hit);
  const float ground_distance =
      has_ground ? ground_hit.distance - support : std::numeric_limits<float>::max();
  const bool grounded = has_ground && ground_hit.normal.y >= slope_limit &&
                        ground_distance <= std::max(settings.ground_probe_distance, 0.0f);

  if (grounded && ground_distance > 0.0f) {
    character.position.y -= ground_distance;
  }

  const Vec3 desired_horizontal{input.desired_velocity.x, 0.0f, input.desired_velocity.z};
  Vec3 horizontal{character.velocity.x, 0.0f, character.velocity.z};
  const bool wants_motion = length(desired_horizontal) > kEpsilon;
  const float response_rate = wants_motion
                                  ? (grounded ? settings.acceleration : settings.air_acceleration)
                                  : settings.braking;
  const float response = 1.0f - std::exp(-std::max(response_rate, 0.0f) * std::max(dt, 0.0f));
  horizontal = horizontal + (desired_horizontal - horizontal) * response;
  character.velocity.x = horizontal.x;
  character.velocity.z = horizontal.z;

  if (grounded && character.velocity.y < 0.0f) {
    character.velocity.y = 0.0f;
  }
  if (grounded && input.jump_requested && settings.jump_speed > 0.0f) {
    character.velocity.y = settings.jump_speed;
  }

  wake(character);
  return {grounded, has_ground ? ground_hit.normal : Vec3{0.0f, 1.0f, 0.0f}, character.velocity,
          ground_distance};
}

void PhysicsWorld::updateSleeping(const float dt) {
  for (PhysicsBody &body : bodies_) {
    if (!body.active || body.type != PhysicsBodyType::Dynamic || !body.allow_sleep) {
      continue;
    }

    if (length(body.velocity) <= settings_.sleep_linear_threshold &&
        length(body.force) <= kEpsilon) {
      body.sleep_timer += dt;
      body.sleeping = body.sleep_timer >= settings_.sleep_time_threshold;
    } else {
      body.sleep_timer = 0.0f;
      body.sleeping = false;
    }
  }
}

void PhysicsWorld::applyFluidForces() {
  if (fluid_volumes_.empty()) {
    return;
  }

  for (std::size_t index = 0; index < bodies_.size(); ++index) {
    PhysicsBody &body = bodies_[index];
    if (!body.active || body.type != PhysicsBodyType::Dynamic) {
      continue;
    }

    const PhysicsBodyHandle handle{static_cast<std::uint32_t>(index), body.generation};
    const Aabb target_box = bodyAabb(body);
    const float target_height = std::max(target_box.max.y - target_box.min.y, kEpsilon);
    for (const FluidVolume &volume : fluid_volumes_) {
      if (!volume.active || (volume.desc.affects_layers & body.filter.layer_bits) == 0u) {
        continue;
      }

      const Aabb fluid_box{volume.desc.center - volume.desc.half_extents,
                           volume.desc.center + volume.desc.half_extents};
      if (!overlaps(target_box, fluid_box)) {
        continue;
      }

      const float surface = std::min(volume.desc.surface_y, fluid_box.max.y);
      const float submerged_height = clamp(surface - target_box.min.y, 0.0f, target_height);
      const float submersion = submerged_height / target_height;
      if (submersion <= 0.0f) {
        continue;
      }

      wakeBody(handle);
      const Vec3 buoyancy =
          settings_.gravity * (-volume.desc.density * submersion / body.inverse_mass);
      const Vec3 drag = (volume.desc.flow - body.velocity) * (volume.desc.linear_drag * submersion);
      body.force = body.force + buoyancy + drag;
    }
  }
}

void PhysicsWorld::integrate(const float dt) {
  for (PhysicsBody &body : bodies_) {
    if (!body.active || body.type != PhysicsBodyType::Dynamic || body.sleeping) {
      continue;
    }

    body.previous_position = body.position;
    const Vec3 acceleration = settings_.gravity + body.force * body.inverse_mass;
    body.velocity = body.velocity + acceleration * dt;
    body.velocity = body.velocity * std::max(0.0f, 1.0f - body.linear_damping * dt);
    body.position = body.position + body.velocity * dt;
    body.force = {};
  }
}

void PhysicsWorld::buildBroadphasePairs() {
  ASTER_PROFILE_SCOPE("PhysicsWorld::broadphase");
  broadphase_pairs_.clear();
  std::vector<BroadphaseEntry> entries;
  entries.reserve(bodies_.size());
  for (std::size_t index = 0; index < bodies_.size(); ++index) {
    const PhysicsBody &body = bodies_[index];
    if (body.active) {
      entries.push_back({index, bodyAabb(body)});
    }
  }
  std::sort(entries.begin(), entries.end(),
            [](const BroadphaseEntry &lhs, const BroadphaseEntry &rhs) {
              if (lhs.bounds.min.x != rhs.bounds.min.x) {
                return lhs.bounds.min.x < rhs.bounds.min.x;
              }
              return lhs.index < rhs.index;
            });

  for (std::size_t entry_a = 0; entry_a < entries.size(); ++entry_a) {
    const BroadphaseEntry &a = entries[entry_a];
    const PhysicsBody &body_a = bodies_[a.index];
    for (std::size_t entry_b = entry_a + 1u; entry_b < entries.size(); ++entry_b) {
      const BroadphaseEntry &b = entries[entry_b];
      if (b.bounds.min.x > a.bounds.max.x) {
        break;
      }
      const PhysicsBody &body_b = bodies_[b.index];
      if (body_a.type == PhysicsBodyType::Static && body_b.type == PhysicsBodyType::Static) {
        continue;
      }
      if (!filtersAllowCollision(body_a, body_b) || !overlaps(a.bounds, b.bounds)) {
        continue;
      }

      const std::size_t first = std::min(a.index, b.index);
      const std::size_t second = std::max(a.index, b.index);
      broadphase_pairs_.push_back({{static_cast<std::uint32_t>(first), bodies_[first].generation},
                                   {static_cast<std::uint32_t>(second),
                                    bodies_[second].generation}});
    }
  }
  std::sort(broadphase_pairs_.begin(), broadphase_pairs_.end(),
            [](const PhysicsBroadphasePair &lhs, const PhysicsBroadphasePair &rhs) {
              if (lhs.body_a.index != rhs.body_a.index) {
                return lhs.body_a.index < rhs.body_a.index;
              }
              return lhs.body_b.index < rhs.body_b.index;
            });
}

void PhysicsWorld::solveConstraints(const float dt) {
  for (const DistanceConstraint &constraint : constraints_) {
    if (!constraint.active || !valid(constraint.desc.body)) {
      continue;
    }

    PhysicsBody &body = bodies_[constraint.desc.body.index];
    if (body.type != PhysicsBodyType::Dynamic) {
      continue;
    }

    const Vec3 offset = body.position - constraint.desc.world_anchor;
    const float current_distance = length(offset);
    if (current_distance <= kEpsilon) {
      continue;
    }

    const bool needs_projection = constraint.desc.mode == DistanceConstraintMode::Fixed ||
                                  current_distance > constraint.desc.distance;
    if (!needs_projection) {
      continue;
    }

    wake(body);
    const Vec3 normal = offset / current_distance;
    const float error = current_distance - constraint.desc.distance;
    const Vec3 correction = normal * (error * constraint.desc.stiffness);
    body.position = body.position - correction;

    const float normal_speed = dot(body.velocity, normal);
    if (normal_speed > 0.0f || constraint.desc.mode == DistanceConstraintMode::Fixed) {
      body.velocity = body.velocity - normal * normal_speed * (1.0f + constraint.desc.damping);
    }

    if (dt > kEpsilon) {
      body.velocity = body.velocity - correction * (constraint.desc.damping / dt);
    }
  }
}

void PhysicsWorld::solveCollisions() {
  ASTER_PROFILE_SCOPE("PhysicsWorld::narrowphase");
  buildBroadphasePairs();
  for (const PhysicsBroadphasePair &pair : broadphase_pairs_) {
    if (!valid(pair.body_a) || !valid(pair.body_b)) {
      continue;
    }
    solvePair(pair.body_a, bodies_[pair.body_a.index], pair.body_b, bodies_[pair.body_b.index]);
  }
}

void PhysicsWorld::solvePair(const PhysicsBodyHandle handle_a, PhysicsBody &body_a,
                             const PhysicsBodyHandle handle_b, PhysicsBody &body_b) {
  ContactCandidate contact = contactFor(body_a, body_b);
  if (!contact.hit || contact.penetration <= 0.0f) {
    return;
  }

  contacts_.push_back({handle_a, handle_b, contact.point, contact.normal, contact.penetration});
  if (body_a.filter.sensor || body_b.filter.sensor) {
    return;
  }

  const float inverse_mass_a = bodyInverseMass(body_a);
  const float inverse_mass_b = bodyInverseMass(body_b);
  const float inverse_mass_sum = inverse_mass_a + inverse_mass_b;
  if (inverse_mass_sum <= kEpsilon) {
    return;
  }

  const float correction_depth = std::max(contact.penetration - 0.0005f, 0.0f);
  const Vec3 correction = contact.normal * (correction_depth / inverse_mass_sum);
  if (inverse_mass_a > 0.0f) {
    body_a.position = body_a.position + correction * inverse_mass_a;
    wake(body_a);
  }
  if (inverse_mass_b > 0.0f) {
    body_b.position = body_b.position - correction * inverse_mass_b;
    wake(body_b);
  }

  Vec3 relative_velocity = body_a.velocity - body_b.velocity;
  const float normal_speed = dot(relative_velocity, contact.normal);
  if (normal_speed >= 0.0f) {
    return;
  }

  const PhysicsMaterial material = combineMaterial(body_a, body_b);
  const float restitution = clamp(material.restitution, 0.0f, 1.0f);
  const float normal_impulse_size = -(1.0f + restitution) * normal_speed / inverse_mass_sum;
  const Vec3 normal_impulse = contact.normal * normal_impulse_size;
  if (inverse_mass_a > 0.0f) {
    body_a.velocity = body_a.velocity + normal_impulse * inverse_mass_a;
  }
  if (inverse_mass_b > 0.0f) {
    body_b.velocity = body_b.velocity - normal_impulse * inverse_mass_b;
  }

  relative_velocity = body_a.velocity - body_b.velocity;
  Vec3 tangent = relative_velocity - contact.normal * dot(relative_velocity, contact.normal);
  const float tangent_length = length(tangent);
  if (tangent_length <= kEpsilon) {
    return;
  }

  tangent = tangent / tangent_length;
  float tangent_impulse_size = -dot(relative_velocity, tangent) / inverse_mass_sum;
  const float friction_limit = normal_impulse_size * clamp(material.friction, 0.0f, 1.0f);
  tangent_impulse_size = std::clamp(tangent_impulse_size, -friction_limit, friction_limit);
  const Vec3 tangent_impulse = tangent * tangent_impulse_size;
  if (inverse_mass_a > 0.0f) {
    body_a.velocity = body_a.velocity + tangent_impulse * inverse_mass_a;
  }
  if (inverse_mass_b > 0.0f) {
    body_b.velocity = body_b.velocity - tangent_impulse * inverse_mass_b;
  }
}

} // namespace aster
