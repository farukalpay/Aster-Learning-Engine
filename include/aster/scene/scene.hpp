// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/transform.hpp"
#include "aster/math/vec.hpp"

#include <cstddef>
#include <cstdint>
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
  CaveWeb,
  CaveSkitterChitin,
  CaveSkitterEye,
  WeatheredMetal,
  WeldBead,
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

enum class MaterialAlphaMode {
  Opaque,
  Masked,
  DitheredCoverage,
  Blend,
};

enum class MaterialDepthWrite {
  Auto,
  Enabled,
  Disabled,
};

enum class CameraOcclusionPolicy {
  Fade,
  Solid,
};

enum class MaterialRenderQueue {
  Opaque,
  Masked,
  Translucent,
};

struct ProceduralSurfaceLayer {
  float macro_variation = 0.0f;
  float micro_normal_strength = 0.0f;
  float roughness_variation = 0.0f;
  float wetness = 0.0f;
  float height_shading = 0.0f;
};

struct ViewerCullVolume {
  bool enabled = false;
  Vec3 center{};
  Vec3 half_extents{};
  FaceCullMode outside = FaceCullMode::Back;
  FaceCullMode inside = FaceCullMode::Front;
};

enum class RenderVisibilityClass : std::uint32_t {
  General,
  CaveCell,
  CavePortal,
  DynamicVoxel,
  Creature,
};

struct RenderVisibilityHint {
  RenderVisibilityClass visibility_class = RenderVisibilityClass::General;
  Vec3 cell{};
  float portal_depth = 0.0f;
};

struct RenderLodPolicy {
  float max_distance = 0.0f;
  float min_projected_radius = 0.0f;
};

struct DynamicMeshResourceKey {
  std::uint64_t id = 0u;
  std::uint64_t generation = 0u;

  [[nodiscard]] bool valid() const noexcept {
    return id != 0u;
  }

  [[nodiscard]] friend bool operator==(const DynamicMeshResourceKey lhs,
                                       const DynamicMeshResourceKey rhs) noexcept {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
};

struct DynamicMeshResourceKeyHash {
  [[nodiscard]] std::size_t operator()(const DynamicMeshResourceKey key) const noexcept {
    std::uint64_t hash = key.id ^ 0x9e3779b97f4a7c15ull;
    hash ^= key.generation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33u;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33u;
    return static_cast<std::size_t>(hash);
  }
};

struct Material {
  std::string asset_id;
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
  MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
  MaterialDepthWrite depth_write = MaterialDepthWrite::Auto;
  CameraOcclusionPolicy camera_occlusion = CameraOcclusionPolicy::Fade;
  ProceduralSurfaceLayer procedural{};
  std::uint64_t compiled_permutation_key = 0u;
  std::uint32_t compiled_permutation_flags = 0u;
  std::uint64_t shader_variant_key = 0u;
};

struct MaterialDesc {
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
  MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
  MaterialDepthWrite depth_write = MaterialDepthWrite::Auto;
  CameraOcclusionPolicy camera_occlusion = CameraOcclusionPolicy::Fade;
  ProceduralSurfaceLayer procedural{};
};

[[nodiscard]] Material makeMaterial(const MaterialDesc &desc);
[[nodiscard]] Material makeSupportSurfaceMaterial(Material material);
[[nodiscard]] MaterialRenderQueue classifyMaterialRenderQueue(const Material &material);
[[nodiscard]] bool materialWritesDepth(const Material &material);
[[nodiscard]] bool allowsCameraOcclusionFade(const Material &material);
[[nodiscard]] bool isMaterialTranslucent(const Material &material);
[[nodiscard]] bool isDoubleSidedMaterial(const Material &material);

struct RenderObject {
  std::string name;
  std::string material_asset_id;
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
  RenderVisibilityHint visibility_hint{};
  RenderLodPolicy lod{};
  DynamicMeshResourceKey dynamic_mesh{};
};

class Scene {
public:
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
