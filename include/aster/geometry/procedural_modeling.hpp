// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aster {

struct MeshAssemblyPart {
  std::string name;
  std::uint32_t first_vertex = 0u;
  std::uint32_t vertex_count = 0u;
  std::uint32_t first_index = 0u;
  std::uint32_t index_count = 0u;
};

class AsterMeshAssembly {
public:
  [[nodiscard]] const CpuMesh &mesh() const {
    return mesh_;
  }

  [[nodiscard]] CpuMesh &mesh() {
    return mesh_;
  }

  [[nodiscard]] const std::vector<MeshAssemblyPart> &parts() const {
    return parts_;
  }

  [[nodiscard]] std::uint32_t beginPart(std::string name);
  void finishPart(std::uint32_t part_index);
  void appendPart(std::string name, const CpuMesh &mesh, Vec3 translation = {},
                  Vec3 scale = {1.0f, 1.0f, 1.0f});
  [[nodiscard]] CpuMesh releaseMesh();

private:
  CpuMesh mesh_;
  std::vector<MeshAssemblyPart> parts_;
};

struct SweepPathPoint {
  Vec3 position{};
  Vec3 up{0.0f, 1.0f, 0.0f};
  float radius_scale = 1.0f;
  Vec2 uv{};
};

struct SweepPath {
  std::vector<SweepPathPoint> points;
  bool closed = false;
};

struct SweepProfilePoint {
  Vec2 offset{};
  Vec2 uv{};
};

struct SweepProfile {
  std::vector<SweepProfilePoint> points;
  bool closed = true;
};

struct LatheSurfaceSpec {
  std::vector<Vec2> profile;
  int radial_segments = 32;
  float start_angle = 0.0f;
  float end_angle = pi() * 2.0f;
  Vec3 center{};
  Vec2 uv_scale{1.0f, 1.0f};
};

struct EllipsoidSectionSpec {
  Vec3 center{};
  Vec3 radius{1.0f, 1.0f, 1.0f};
  int segments = 32;
  int rings = 16;
  float theta_min = 0.0f;
  float theta_max = pi() * 2.0f;
  float phi_min = 0.0f;
  float phi_max = pi();
  Vec2 uv_origin{};
  Vec2 uv_scale{1.0f, 1.0f};
};

struct SweptTubeSpec {
  SweepPath path;
  SweepProfile profile;
  float radius = 1.0f;
  bool rebuild_normals = true;
};

struct ExtrudedRidgeSpec {
  std::vector<Vec3> spine;
  Vec3 up{0.0f, 1.0f, 0.0f};
  float width = 0.10f;
  float height = 0.05f;
  bool cap_ends = true;
};

struct RibbonStripSpec {
  std::vector<Vec3> centerline;
  std::vector<float> half_widths;
  Vec3 up{0.0f, 1.0f, 0.0f};
  float thickness = 0.0f;
  bool cap_ends = true;
  Vec2 uv_scale{1.0f, 1.0f};
};

struct CapsuleSpec {
  Vec3 start{};
  Vec3 end{0.0f, 1.0f, 0.0f};
  float radius = 0.1f;
  float end_radius_scale = 1.0f;
  int segments = 16;
  int rings = 8;
  float profile_vertical_scale = 1.0f;
};

struct SurfaceDisplacementSpec {
  float amplitude = 0.0f;
  float frequency = 1.0f;
  float ridge_strength = 0.0f;
  float cavity_ao_strength = 0.0f;
  std::uint32_t seed = 0u;
  Vec3 directional_bias{};
  bool rebuild_normals = true;
};

[[nodiscard]] SweepProfile makeCircularSweepProfile(int segments, float radius = 1.0f,
                                                    float vertical_scale = 1.0f);

void mergeMesh(CpuMesh &target, const CpuMesh &source, Vec3 translation = {},
               Vec3 scale = {1.0f, 1.0f, 1.0f});

void appendLathedSurface(CpuMesh &mesh, const LatheSurfaceSpec &spec);
void appendEllipsoidSection(CpuMesh &mesh, const EllipsoidSectionSpec &spec);
void appendSweptTube(CpuMesh &mesh, const SweptTubeSpec &spec);
void appendExtrudedRidge(CpuMesh &mesh, const ExtrudedRidgeSpec &spec);
void appendRibbonStrip(CpuMesh &mesh, const RibbonStripSpec &spec);
void appendCapsule(CpuMesh &mesh, const CapsuleSpec &spec);
void applyDeterministicSurfaceDetail(CpuMesh &mesh, const SurfaceDisplacementSpec &spec);

[[nodiscard]] CpuMesh makeLathedSurface(const LatheSurfaceSpec &spec);
[[nodiscard]] CpuMesh makeEllipsoidSection(const EllipsoidSectionSpec &spec);
[[nodiscard]] CpuMesh makeSweptTube(const SweptTubeSpec &spec);
[[nodiscard]] CpuMesh makeExtrudedRidge(const ExtrudedRidgeSpec &spec);
[[nodiscard]] CpuMesh makeRibbonStrip(const RibbonStripSpec &spec);
[[nodiscard]] CpuMesh makeCapsule(const CapsuleSpec &spec);

} // namespace aster
