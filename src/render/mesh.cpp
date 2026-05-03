// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace aster {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float ridgeNoise(const float a, const float b, const float c) {
  return std::sin(a * 2.31f + b * 1.73f) * 0.11f + std::sin(a * 5.17f - c * 3.11f) * 0.07f +
         std::sin((a + b + c) * 7.09f) * 0.045f;
}

Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return normalize(cross(b - a, c - a));
}

void appendTriangle(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec2 uv_a,
                    const Vec2 uv_b, const Vec2 uv_c) {
  const Vec3 normal = faceNormal(a, b, c);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, uv_a});
  mesh.vertices.push_back({b, normal, uv_b});
  mesh.vertices.push_back({c, normal, uv_c});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u});
}

void appendQuad(CpuMesh &mesh, const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d,
                const Vec2 uv_a = {0.0f, 0.0f}, const Vec2 uv_b = {1.0f, 0.0f},
                const Vec2 uv_c = {1.0f, 1.0f}, const Vec2 uv_d = {0.0f, 1.0f}) {
  appendTriangle(mesh, a, b, c, uv_a, uv_b, uv_c);
  appendTriangle(mesh, a, c, d, uv_a, uv_c, uv_d);
}

Vec3 rockPoint(const int segment, const int ring, const int segments, const int rings,
               const float radius) {
  const float u = static_cast<float>(segment) / static_cast<float>(segments);
  const float v = static_cast<float>(ring) / static_cast<float>(rings);
  const float theta = u * kPi * 2.0f;
  const float phi = v * kPi;
  const float horizontal = std::sin(phi);
  const float vertical = std::cos(phi);
  const float radius_offset = 1.0f + ridgeNoise(theta, phi, static_cast<float>(segment + ring));
  const float vertical_scale = 0.78f + 0.08f * std::sin(theta * 2.0f) * horizontal;

  return {
      std::cos(theta) * horizontal * radius * radius_offset * 1.06f,
      vertical * radius * vertical_scale,
      std::sin(theta) * horizontal * radius * (0.94f + radius_offset * 0.09f),
  };
}

} // namespace

CpuMesh makeUvSphere(const int segments, const int rings, const float radius) {
  if (segments < 3 || rings < 2 || radius <= 0.0f) {
    throw std::invalid_argument(
        "Sphere generation requires segments >= 3, rings >= 2, radius > 0.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * (rings + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(segments * rings * 6));

  for (int y = 0; y <= rings; ++y) {
    const float v = static_cast<float>(y) / static_cast<float>(rings);
    const float phi = v * kPi;
    for (int x = 0; x <= segments; ++x) {
      const float u = static_cast<float>(x) / static_cast<float>(segments);
      const float theta = u * kPi * 2.0f;
      const Vec3 normal{
          std::sin(phi) * std::cos(theta),
          std::cos(phi),
          std::sin(phi) * std::sin(theta),
      };
      mesh.vertices.push_back({normal * radius, normal, {u, v}});
    }
  }

  for (int y = 0; y < rings; ++y) {
    for (int x = 0; x < segments; ++x) {
      const std::uint32_t a = static_cast<std::uint32_t>(y * (segments + 1) + x);
      const std::uint32_t b = static_cast<std::uint32_t>((y + 1) * (segments + 1) + x);
      const std::uint32_t c = static_cast<std::uint32_t>((y + 1) * (segments + 1) + x + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(y * (segments + 1) + x + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }

  return mesh;
}

CpuMesh makeBox() {
  CpuMesh mesh;
  mesh.vertices.reserve(24);
  mesh.indices.reserve(36);

  constexpr float h = 0.5f;
  appendQuad(mesh, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
  appendQuad(mesh, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});
  appendQuad(mesh, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});
  appendQuad(mesh, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});
  appendQuad(mesh, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});
  appendQuad(mesh, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});
  return mesh;
}

CpuMesh makePlane(const float size) {
  if (size <= 0.0f) {
    throw std::invalid_argument("Plane generation requires size > 0.");
  }
  const float half = size * 0.5f;
  CpuMesh mesh;
  mesh.vertices = {
      {{-half, 0.0f, -half}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{half, 0.0f, -half}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{half, 0.0f, half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-half, 0.0f, half}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
  };
  mesh.indices = {0, 2, 1, 0, 3, 2};
  return mesh;
}

CpuMesh makeRock(const int segments, const int rings, const float radius) {
  if (segments < 5 || rings < 3 || radius <= 0.0f) {
    throw std::invalid_argument("Rock generation requires segments >= 5, rings >= 3, radius > 0.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * (rings + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(segments * rings * 6));

  for (int y = 0; y <= rings; ++y) {
    for (int x = 0; x <= segments; ++x) {
      const Vec3 point = rockPoint(x % segments, y, segments, rings, radius);
      const Vec3 normal = normalize({point.x * 0.92f, point.y * 1.28f, point.z * 0.94f});
      mesh.vertices.push_back({point,
                               normal,
                               {static_cast<float>(x) / static_cast<float>(segments),
                                static_cast<float>(y) / static_cast<float>(rings)}});
    }
  }

  for (int y = 0; y < rings; ++y) {
    for (int x = 0; x < segments; ++x) {
      const std::uint32_t a = static_cast<std::uint32_t>(y * (segments + 1) + x);
      const std::uint32_t b = static_cast<std::uint32_t>((y + 1) * (segments + 1) + x);
      const std::uint32_t c = static_cast<std::uint32_t>((y + 1) * (segments + 1) + x + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(y * (segments + 1) + x + 1);
      mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
    }
  }

  return mesh;
}

CpuMesh makeCrystal(const int sides, const float radius, const float height) {
  if (sides < 3 || radius <= 0.0f || height <= 0.0f) {
    throw std::invalid_argument("Crystal generation requires sides >= 3, radius > 0, height > 0.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(sides * 12));
  mesh.indices.reserve(static_cast<std::size_t>(sides * 12));

  const Vec3 top{0.0f, height * 0.5f, 0.0f};
  const Vec3 shoulder_center{0.0f, height * 0.18f, 0.0f};
  const Vec3 waist_center{0.0f, -height * 0.22f, 0.0f};
  const Vec3 bottom{0.0f, -height * 0.5f, 0.0f};

  for (int i = 0; i < sides; ++i) {
    const float a0 = (static_cast<float>(i) / static_cast<float>(sides)) * kPi * 2.0f;
    const float a1 = (static_cast<float>(i + 1) / static_cast<float>(sides)) * kPi * 2.0f;
    const Vec3 shoulder_a =
        shoulder_center + Vec3{std::cos(a0) * radius, 0.0f, std::sin(a0) * radius * 0.88f};
    const Vec3 shoulder_b =
        shoulder_center + Vec3{std::cos(a1) * radius, 0.0f, std::sin(a1) * radius * 0.88f};
    const Vec3 waist_a =
        waist_center + Vec3{std::cos(a0) * radius * 0.78f, 0.0f, std::sin(a0) * radius * 0.72f};
    const Vec3 waist_b =
        waist_center + Vec3{std::cos(a1) * radius * 0.78f, 0.0f, std::sin(a1) * radius * 0.72f};
    appendTriangle(mesh, top, shoulder_a, shoulder_b, {0.5f, 1.0f}, {0.0f, 0.54f}, {1.0f, 0.54f});
    appendQuad(mesh, shoulder_a, waist_a, waist_b, shoulder_b);
    appendTriangle(mesh, waist_a, bottom, waist_b, {0.0f, 0.46f}, {0.5f, 0.0f}, {1.0f, 0.46f});
  }

  return mesh;
}

CpuMesh makeRuinBlock() {
  CpuMesh mesh;
  mesh.vertices.reserve(36);
  mesh.indices.reserve(36);

  const Vec3 p000{-0.54f, -0.47f, -0.50f};
  const Vec3 p100{0.52f, -0.51f, -0.48f};
  const Vec3 p110{0.48f, 0.48f, -0.54f};
  const Vec3 p010{-0.50f, 0.52f, -0.46f};
  const Vec3 p001{-0.49f, -0.53f, 0.52f};
  const Vec3 p101{0.55f, -0.46f, 0.47f};
  const Vec3 p111{0.50f, 0.50f, 0.54f};
  const Vec3 p011{-0.55f, 0.46f, 0.48f};

  appendQuad(mesh, p100, p000, p010, p110);
  appendQuad(mesh, p001, p101, p111, p011);
  appendQuad(mesh, p000, p001, p011, p010);
  appendQuad(mesh, p101, p100, p110, p111);
  appendQuad(mesh, p010, p011, p111, p110);
  appendQuad(mesh, p001, p000, p100, p101);
  return mesh;
}

CpuMesh makePillar(const int sides, const float radius, const float height) {
  if (sides < 5 || radius <= 0.0f || height <= 0.0f) {
    throw std::invalid_argument("Pillar generation requires sides >= 5, radius > 0, height > 0.");
  }

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(sides * 24));
  mesh.indices.reserve(static_cast<std::size_t>(sides * 24));

  const float half_height = height * 0.5f;
  const float cap_y = height * 0.38f;
  for (int i = 0; i < sides; ++i) {
    const float a0 = (static_cast<float>(i) / static_cast<float>(sides)) * kPi * 2.0f;
    const float a1 = (static_cast<float>(i + 1) / static_cast<float>(sides)) * kPi * 2.0f;
    const float chip0 = 1.0f + 0.05f * std::sin(static_cast<float>(i) * 2.7f);
    const float chip1 = 1.0f + 0.05f * std::sin(static_cast<float>(i + 1) * 2.7f);
    const Vec3 b0{std::cos(a0) * radius * 1.18f * chip0, -half_height,
                  std::sin(a0) * radius * 1.18f * chip0};
    const Vec3 b1{std::cos(a1) * radius * 1.18f * chip1, -half_height,
                  std::sin(a1) * radius * 1.18f * chip1};
    const Vec3 m0{std::cos(a0) * radius * chip0, -cap_y, std::sin(a0) * radius * chip0};
    const Vec3 m1{std::cos(a1) * radius * chip1, -cap_y, std::sin(a1) * radius * chip1};
    const Vec3 t0{std::cos(a0) * radius * 0.98f * chip0, cap_y,
                  std::sin(a0) * radius * 0.98f * chip0};
    const Vec3 t1{std::cos(a1) * radius * 0.98f * chip1, cap_y,
                  std::sin(a1) * radius * 0.98f * chip1};
    const Vec3 c0{std::cos(a0) * radius * 1.14f * chip0, half_height,
                  std::sin(a0) * radius * 1.14f * chip0};
    const Vec3 c1{std::cos(a1) * radius * 1.14f * chip1, half_height,
                  std::sin(a1) * radius * 1.14f * chip1};

    appendQuad(mesh, b0, b1, m1, m0);
    appendQuad(mesh, m0, m1, t1, t0);
    appendQuad(mesh, t0, t1, c1, c0);
    appendTriangle(mesh, {0.0f, half_height, 0.0f}, c0, c1, {0.5f, 0.5f}, {0.0f, 0.0f},
                   {1.0f, 0.0f});
    appendTriangle(mesh, {0.0f, -half_height, 0.0f}, b1, b0, {0.5f, 0.5f}, {1.0f, 0.0f},
                   {0.0f, 0.0f});
  }

  return mesh;
}

GpuMesh::~GpuMesh() {
  release();
}

GpuMesh::GpuMesh(GpuMesh &&other) noexcept
    : vertex_array_(other.vertex_array_), vertex_buffer_(other.vertex_buffer_),
      index_buffer_(other.index_buffer_), index_count_(other.index_count_) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
  other.index_buffer_ = 0;
  other.index_count_ = 0;
}

GpuMesh &GpuMesh::operator=(GpuMesh &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  release();
  vertex_array_ = other.vertex_array_;
  vertex_buffer_ = other.vertex_buffer_;
  index_buffer_ = other.index_buffer_;
  index_count_ = other.index_count_;
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
  other.index_buffer_ = 0;
  other.index_count_ = 0;
  return *this;
}

void GpuMesh::upload(const CpuMesh &mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    throw std::invalid_argument("Cannot stage an empty mesh.");
  }
  index_count_ = static_cast<std::uint32_t>(mesh.indices.size());
}

void GpuMesh::draw() const {}

void GpuMesh::release() {
  vertex_array_ = 0;
  vertex_buffer_ = 0;
  index_buffer_ = 0;
  index_count_ = 0;
}

} // namespace aster
