// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/scene.hpp"

namespace aster {

Material makeMaterial(const MaterialDesc &desc) {
  Material material;
  material.base_color = desc.base_color;
  material.emission_color = desc.emission_color;
  material.roughness = desc.roughness;
  material.metallic = desc.metallic;
  material.emission_strength = desc.emission_strength;
  material.detail_strength = desc.detail_strength;
  material.detail_scale = desc.detail_scale;
  material.edge_wear = desc.edge_wear;
  material.ambient_occlusion = desc.ambient_occlusion;
  material.surface_pattern = desc.surface_pattern;
  material.pattern_scale = desc.pattern_scale;
  material.pattern_depth = desc.pattern_depth;
  material.pattern_contrast = desc.pattern_contrast;
  material.pattern_mortar = desc.pattern_mortar;
  material.opacity = desc.opacity;
  material.double_sided = desc.double_sided;
  material.cull_mode = desc.cull_mode;
  material.render_role = desc.render_role;
  material.alpha_mode = desc.alpha_mode;
  material.depth_write = desc.depth_write;
  material.camera_occlusion = desc.camera_occlusion;
  material.procedural = desc.procedural;
  return material;
}

Material makeSupportSurfaceMaterial(Material material) {
  material.opacity = 1.0f;
  material.double_sided = true;
  material.render_role = MaterialRenderRole::SupportSurface;
  material.alpha_mode = MaterialAlphaMode::Opaque;
  material.depth_write = MaterialDepthWrite::Enabled;
  material.camera_occlusion = CameraOcclusionPolicy::Solid;
  return material;
}

MaterialRenderQueue classifyMaterialRenderQueue(const Material &material) {
  if (material.render_role == MaterialRenderRole::SupportSurface) {
    return MaterialRenderQueue::Opaque;
  }
  if (material.alpha_mode == MaterialAlphaMode::Blend || material.opacity < 0.999f) {
    return MaterialRenderQueue::Translucent;
  }
  if (material.alpha_mode == MaterialAlphaMode::Masked ||
      material.alpha_mode == MaterialAlphaMode::DitheredCoverage) {
    return MaterialRenderQueue::Masked;
  }
  return MaterialRenderQueue::Opaque;
}

bool materialWritesDepth(const Material &material) {
  if (material.depth_write == MaterialDepthWrite::Enabled) {
    return true;
  }
  if (material.depth_write == MaterialDepthWrite::Disabled) {
    return false;
  }
  return classifyMaterialRenderQueue(material) != MaterialRenderQueue::Translucent;
}

bool allowsCameraOcclusionFade(const Material &material) {
  return material.render_role == MaterialRenderRole::Surface &&
         material.camera_occlusion == CameraOcclusionPolicy::Fade &&
         classifyMaterialRenderQueue(material) != MaterialRenderQueue::Translucent;
}

bool isMaterialTranslucent(const Material &material) {
  return classifyMaterialRenderQueue(material) == MaterialRenderQueue::Translucent;
}

bool isDoubleSidedMaterial(const Material &material) {
  return material.double_sided || material.render_role == MaterialRenderRole::SupportSurface;
}

} // namespace aster
