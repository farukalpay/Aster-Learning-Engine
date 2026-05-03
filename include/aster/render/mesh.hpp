// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct Vertex {
  Vec3 position{};
  Vec3 normal{};
  Vec2 uv{};
  Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  float ambient_occlusion = 1.0f;
};

struct CpuMesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

CpuMesh makeUvSphere(int segments, int rings, float radius);
CpuMesh makeBox();
CpuMesh makePlane(float size);
CpuMesh makeRock(int segments, int rings, float radius);
CpuMesh makeCrystal(int sides, float radius, float height);
CpuMesh makeRuinBlock();
CpuMesh makePillar(int sides, float radius, float height);

class GpuMesh {
public:
  GpuMesh() = default;
  ~GpuMesh();

  GpuMesh(const GpuMesh &) = delete;
  GpuMesh &operator=(const GpuMesh &) = delete;

  GpuMesh(GpuMesh &&other) noexcept;
  GpuMesh &operator=(GpuMesh &&other) noexcept;

  void upload(const CpuMesh &mesh);
  void draw() const;
  void release();

  [[nodiscard]] std::uint32_t indexCount() const {
    return index_count_;
  }

private:
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
  unsigned int index_buffer_ = 0;
  std::uint32_t index_count_ = 0;
};

} // namespace aster
