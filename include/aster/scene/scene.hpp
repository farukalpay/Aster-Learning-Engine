// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/transform.hpp"
#include "aster/math/vec.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aster {

struct CpuMesh;

enum class MeshPrimitive {
  Box,
  Sphere,
  Plane,
  Rock,
  Crystal,
  RuinBlock,
  Pillar,
};

enum class SurfacePattern {
  None,
  CourseCells,
  FiberStrands,
  GrassSoil,
  WaterSurface,
  SoilPath,
  TerrainBlend,
  FurFibers,
  LayeredTerrain,
  WeatheredStone,
  PaintedWood,
  Foliage,
  IridescentScales,
  AmberResin,
  FeatherVanes,
  TwigNest,
  ReptileScales,
  CaveRock,
  CoalVein,
  ContactShadow,
};

enum class FaceCullMode {
  None,
  Back,
  Front,
};

enum class MaterialRenderRole {
  Surface,
  SupportSurface,
};

struct ViewerCullVolume {
  bool enabled = false;
  Vec3 center{};
  Vec3 half_extents{};
  FaceCullMode outside = FaceCullMode::Back;
  FaceCullMode inside = FaceCullMode::Front;
};

struct Material {
  Vec3 base_color{1.0f, 1.0f, 1.0f};
  Vec3 emission_color{0.0f, 0.0f, 0.0f};
  float roughness = 0.55f;
  float metallic = 0.0f;
  float emission_strength = 0.0f;
  float detail_strength = 0.0f;
  float detail_scale = 1.0f;
  float edge_wear = 0.0f;
  float ambient_occlusion = 1.0f;
  SurfacePattern surface_pattern = SurfacePattern::None;
  Vec2 pattern_scale{1.0f, 1.0f};
  float pattern_depth = 0.0f;
  float pattern_contrast = 0.0f;
  float pattern_mortar = 0.08f;
  float opacity = 1.0f;
  bool double_sided = false;
  FaceCullMode cull_mode = FaceCullMode::Back;
  MaterialRenderRole render_role = MaterialRenderRole::Surface;
};

[[nodiscard]] Material makeSupportSurfaceMaterial(Material material);
[[nodiscard]] bool isMaterialTranslucent(const Material &material);
[[nodiscard]] bool isDoubleSidedMaterial(const Material &material);

struct RenderObject {
  std::string name;
  MeshPrimitive primitive = MeshPrimitive::Sphere;
  Transform transform{};
  Material material{};
  float spin_rate = 0.0f;
  std::shared_ptr<const CpuMesh> custom_mesh{};
  bool camera_occlusion_fade = true;
  bool auto_contact_shadow = true;
  bool casts_contact_shadow = false;
  float contact_shadow_strength = 1.0f;
  float contact_shadow_radius_scale = 1.0f;
  ViewerCullVolume viewer_cull_volume{};
};

class Scene {
public:
  static Scene makeShowcase();

  [[nodiscard]] const std::vector<RenderObject> &objects() const {
    return objects_;
  }

  [[nodiscard]] std::vector<RenderObject> &objects() {
    return objects_;
  }

private:
  std::vector<RenderObject> objects_;
};

} // namespace aster
