// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_device.hpp"

#include "../platform/frame_presenter.hpp"
#include "aster/asset/mesh_pipeline.hpp"
#include "aster/core/profiler.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/scene.hpp"

#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kSphereSegments = 48;
constexpr int kSphereRings = 24;
constexpr int kRockSegments = 28;
constexpr int kRockRings = 16;
constexpr int kCrystalSides = 6;
constexpr int kPillarSides = 10;
constexpr float kPlaneSize = 12.0f;
constexpr float kContactShadowPlaneSize = 2.0f;

struct LocalBounds {
  aster::Vec3 min{};
  aster::Vec3 max{};
};

struct TransparentDrawItem {
  std::size_t object_index = 0;
  bool occlusion_fade = false;
  float distance_sq = 0.0f;
};

struct MetalLightUniform {
  float position[4]{};
  float color[4]{};
};

struct MetalSceneUniforms {
  float camera_position[4]{};
  float sun_direction[4]{};
  float sun_color[4]{};
  float sky_ambient_color[4]{};
  float ground_ambient_color[4]{};
  float fog_color[4]{};
  float shadow_tint[4]{};
  float highlight_tint[4]{};
  float render_params[4]{};
  float atmosphere_params[4]{};
  float visual_params[4]{};
  float grounding_params[4]{};
  float grounding_tuning[4]{};
  MetalLightUniform lights[aster::kRenderLightCount]{};
};

struct MetalMaterialUniforms {
  float base_color_roughness[4]{};
  float emission_color_metallic[4]{};
  float detail_params[4]{};
  float pattern_params[4]{};
  float pattern_scale_opacity[4]{};
  float material_params[4]{};
};

struct MetalObjectUniforms {
  float model[16]{};
  float model_view_projection[16]{};
};

float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

float maxComponent(const aster::Vec3 value) {
  return std::max(value.x, std::max(value.y, value.z));
}

aster::MeshProcessOptions renderMeshOptions() {
  aster::MeshProcessOptions options;
  options.generate_tangents = false;
  options.optimize_vertex_cache = false;
  options.optimize_vertex_fetch = false;
  return options;
}

bool isContactShadowUtility(const aster::RenderObject &object) {
  return object.material.surface_pattern == aster::SurfacePattern::ContactShadow;
}

bool containsViewerCullVolume(const aster::ViewerCullVolume &volume, const aster::Vec3 point) {
  if (!volume.enabled || volume.half_extents.x <= 0.0f || volume.half_extents.y <= 0.0f ||
      volume.half_extents.z <= 0.0f) {
    return false;
  }
  const aster::Vec3 delta = point - volume.center;
  return std::abs(delta.x) <= volume.half_extents.x && std::abs(delta.y) <= volume.half_extents.y &&
         std::abs(delta.z) <= volume.half_extents.z;
}

aster::FaceCullMode objectCullMode(const aster::RenderObject &object,
                                   const aster::Vec3 camera_position,
                                   const bool pipeline_back_face_culling) {
  if (!pipeline_back_face_culling || aster::isDoubleSidedMaterial(object.material)) {
    return aster::FaceCullMode::None;
  }
  if (containsViewerCullVolume(object.viewer_cull_volume, camera_position)) {
    return object.viewer_cull_volume.inside;
  }
  if (object.viewer_cull_volume.enabled) {
    return object.viewer_cull_volume.outside;
  }
  return object.material.cull_mode;
}

float primitiveLocalRadius(const aster::MeshPrimitive primitive) {
  switch (primitive) {
  case aster::MeshPrimitive::Box:
    return 0.8661f;
  case aster::MeshPrimitive::Sphere:
  case aster::MeshPrimitive::Rock:
  case aster::MeshPrimitive::Pillar:
    return 1.0f;
  case aster::MeshPrimitive::Crystal:
    return 1.8f;
  case aster::MeshPrimitive::RuinBlock:
    return 0.95f;
  case aster::MeshPrimitive::Plane:
    return 8.5f;
  }
  return 1.0f;
}

LocalBounds primitiveLocalBounds(const aster::MeshPrimitive primitive) {
  switch (primitive) {
  case aster::MeshPrimitive::Box:
    return {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  case aster::MeshPrimitive::Sphere:
  case aster::MeshPrimitive::Rock:
    return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
  case aster::MeshPrimitive::Crystal:
    return {{-1.0f, -0.9f, -0.9f}, {1.0f, 0.9f, 0.9f}};
  case aster::MeshPrimitive::RuinBlock:
    return {{-0.56f, -0.54f, -0.55f}, {0.56f, 0.54f, 0.55f}};
  case aster::MeshPrimitive::Pillar:
    return {{-1.18f, -0.5f, -1.18f}, {1.18f, 0.5f, 1.18f}};
  case aster::MeshPrimitive::Plane:
    return {{-6.0f, 0.0f, -6.0f}, {6.0f, 0.0f, 6.0f}};
  }
  return {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
}

LocalBounds customMeshLocalBounds(const aster::CpuMesh &mesh) {
  if (mesh.vertices.empty()) {
    return {};
  }
  LocalBounds bounds{mesh.vertices.front().position, mesh.vertices.front().position};
  for (const aster::Vertex &vertex : mesh.vertices) {
    bounds.min.x = std::min(bounds.min.x, vertex.position.x);
    bounds.min.y = std::min(bounds.min.y, vertex.position.y);
    bounds.min.z = std::min(bounds.min.z, vertex.position.z);
    bounds.max.x = std::max(bounds.max.x, vertex.position.x);
    bounds.max.y = std::max(bounds.max.y, vertex.position.y);
    bounds.max.z = std::max(bounds.max.z, vertex.position.z);
  }
  return bounds;
}

LocalBounds objectLocalBounds(const aster::RenderObject &object) {
  return object.custom_mesh != nullptr ? customMeshLocalBounds(*object.custom_mesh)
                                       : primitiveLocalBounds(object.primitive);
}

float localMaxAbs(const float min_value, const float max_value) {
  return std::max(std::abs(min_value), std::abs(max_value));
}

float objectFootY(const aster::RenderObject &object, const LocalBounds &bounds) {
  return object.transform.position.y + bounds.min.y * object.transform.scale.y;
}

aster::Vec2 objectContactHalfExtents(const aster::RenderObject &object, const LocalBounds &bounds) {
  return {localMaxAbs(bounds.min.x, bounds.max.x) * std::abs(object.transform.scale.x),
          localMaxAbs(bounds.min.z, bounds.max.z) * std::abs(object.transform.scale.z)};
}

float customMeshLocalRadius(const aster::CpuMesh &mesh) {
  float radius = 0.0f;
  for (const aster::Vertex &vertex : mesh.vertices) {
    radius = std::max(radius, aster::length(vertex.position));
  }
  return radius;
}

float objectBoundingRadius(const aster::RenderObject &object) {
  const float local_radius = object.custom_mesh != nullptr
                                 ? customMeshLocalRadius(*object.custom_mesh)
                                 : primitiveLocalRadius(object.primitive);
  return local_radius * std::max(maxComponent(object.transform.scale), 0.001f);
}

float objectSortDistanceSq(const aster::RenderObject &object, const aster::Vec3 camera_position) {
  const aster::Vec3 delta = object.transform.position - camera_position;
  return aster::dot(delta, delta);
}

float distancePointSegment(const aster::Vec3 point, const aster::Vec3 a, const aster::Vec3 b,
                           float &along) {
  const aster::Vec3 segment = b - a;
  const float length_sq = aster::dot(segment, segment);
  if (length_sq <= 0.000001f) {
    along = 0.0f;
    return aster::length(point - a);
  }
  const float t = std::clamp(aster::dot(point - a, segment) / length_sq, 0.0f, 1.0f);
  along = std::sqrt(length_sq) * t;
  return aster::length(point - (a + segment * t));
}

bool intersectsLineOfSightFade(const aster::RenderObject &object,
                               const aster::LineOfSightFadeSettings &fade) {
  if (!fade.enabled || !object.camera_occlusion_fade) {
    return false;
  }
  if (object.custom_mesh == nullptr && object.primitive == aster::MeshPrimitive::Plane) {
    return false;
  }
  const float object_radius = objectBoundingRadius(object);
  if (fade.max_object_radius > 0.0f && object_radius > fade.max_object_radius) {
    return false;
  }

  const aster::Vec3 segment = fade.target_position - fade.camera_position;
  const float segment_length = aster::length(segment);
  if (segment_length <= 0.001f) {
    return false;
  }

  float along = 0.0f;
  const float distance = distancePointSegment(object.transform.position, fade.camera_position,
                                              fade.target_position, along);
  if (along <= std::max(fade.camera_clearance, 0.0f) ||
      along >= segment_length - std::max(fade.target_clearance, 0.0f)) {
    return false;
  }
  return distance <= object_radius + std::max(fade.radius, 0.0f);
}

bool canCastContactShadow(const aster::RenderObject &object,
                          const aster::GroundingSettings &grounding) {
  const bool requested =
      object.casts_contact_shadow || (grounding.auto_contact_shadows && object.auto_contact_shadow);
  if (!grounding.enabled || !grounding.contact_shadows || !requested) {
    return false;
  }
  if (object.material.render_role == aster::MaterialRenderRole::SupportSurface) {
    return false;
  }
  if (aster::isMaterialTranslucent(object.material) || isContactShadowUtility(object)) {
    return false;
  }
  if (object.material.surface_pattern == aster::SurfacePattern::WaterSurface) {
    return false;
  }
  if (object.custom_mesh == nullptr && object.primitive == aster::MeshPrimitive::Plane) {
    return false;
  }

  const LocalBounds bounds = objectLocalBounds(object);
  const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  if (raw_radius < min_radius || raw_radius > max_radius) {
    return false;
  }

  const float receiver_delta = std::abs(objectFootY(object, bounds) - grounding.reference_y);
  return receiver_delta <= std::max(grounding.contact_shadow_receiver_height, 0.0f);
}

aster::RenderObject contactShadowObjectFor(const aster::RenderObject &object,
                                           const aster::GroundingSettings &grounding) {
  const LocalBounds bounds = objectLocalBounds(object);
  const aster::Vec2 half_extents = objectContactHalfExtents(object, bounds);
  const float min_radius = std::max(grounding.contact_shadow_min_radius, 0.0f);
  const float max_radius = std::max(grounding.contact_shadow_max_radius, min_radius);
  const float raw_radius = std::max(half_extents.x, half_extents.y) *
                           grounding.contact_shadow_radius_scale *
                           object.contact_shadow_radius_scale;
  const float radius = std::clamp(raw_radius, min_radius, max_radius);
  const float footprint_x = std::clamp(half_extents.x * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float footprint_z = std::clamp(half_extents.y * grounding.contact_shadow_radius_scale *
                                           object.contact_shadow_radius_scale,
                                       min_radius, radius);
  const float foot_y = objectFootY(object, bounds);
  const float receiver_delta = std::abs(foot_y - grounding.reference_y);
  const float fade = 1.0f - smoothstep(grounding.contact_shadow_receiver_height * 0.72f,
                                       grounding.contact_shadow_receiver_height, receiver_delta);

  aster::RenderObject shadow;
  shadow.name = "Procedural contact shadow";
  shadow.primitive = aster::MeshPrimitive::Plane;
  shadow.transform.position = {object.transform.position.x,
                               foot_y + grounding.contact_shadow_receiver_bias,
                               object.transform.position.z};
  shadow.transform.rotation = {0.0f, object.transform.rotation.y, 0.0f};
  shadow.transform.scale = {footprint_x, 1.0f, footprint_z};
  shadow.material.base_color = {0.0f, 0.0f, 0.0f};
  shadow.material.roughness = 1.0f;
  shadow.material.opacity = std::clamp(
      grounding.contact_shadow_strength * object.contact_shadow_strength * fade, 0.0f, 0.85f);
  shadow.material.surface_pattern = aster::SurfacePattern::ContactShadow;
  shadow.material.double_sided = true;
  shadow.casts_contact_shadow = false;
  shadow.camera_occlusion_fade = false;
  return shadow;
}

void copyMat4(float (&out)[16], const aster::Mat4 &matrix) {
  std::memcpy(out, matrix.data(), sizeof(out));
}

void setVec4(float (&out)[4], const aster::Vec3 value, const float w = 0.0f) {
  out[0] = value.x;
  out[1] = value.y;
  out[2] = value.z;
  out[3] = w;
}

int toneMapperUniform(const aster::RendererSettings &settings) {
  if (settings.use_aces_tonemap) {
    return 0;
  }
  switch (settings.pipeline.tone_mapper) {
  case aster::ToneMapper::FilmicAces:
    return 0;
  case aster::ToneMapper::PbrNeutral:
    return 1;
  case aster::ToneMapper::Reinhard:
    return 2;
  }
  return 1;
}

MetalSceneUniforms makeSceneUniforms(const aster::OrbitCamera &camera,
                                     const aster::RendererSettings &settings,
                                     const double frame_seconds) {
  MetalSceneUniforms uniforms;
  setVec4(uniforms.camera_position, camera.position(), 1.0f);
  setVec4(uniforms.sun_direction, settings.sun_light.direction_to_light,
          settings.sun_light.enabled ? 1.0f : 0.0f);
  setVec4(uniforms.sun_color, settings.sun_light.color, settings.sun_light.intensity);
  setVec4(uniforms.sky_ambient_color, settings.sky_ambient_color, 0.0f);
  setVec4(uniforms.ground_ambient_color, settings.ground_ambient_color, 0.0f);
  setVec4(uniforms.fog_color, settings.atmosphere.fog_color, 0.0f);
  setVec4(uniforms.shadow_tint, settings.atmosphere.shadow_tint,
          settings.atmosphere.shadow_tint_strength);
  setVec4(uniforms.highlight_tint, settings.atmosphere.highlight_tint,
          settings.atmosphere.highlight_tint_strength);
  uniforms.render_params[0] = settings.exposure;
  uniforms.render_params[1] = settings.ambient_strength;
  uniforms.render_params[2] = settings.ambient_floor;
  uniforms.render_params[3] = static_cast<float>(frame_seconds);
  uniforms.atmosphere_params[0] = settings.atmosphere.enabled ? 1.0f : 0.0f;
  uniforms.atmosphere_params[1] = settings.atmosphere.fog_start;
  uniforms.atmosphere_params[2] = settings.atmosphere.fog_end;
  uniforms.atmosphere_params[3] = settings.atmosphere.fog_strength;
  uniforms.visual_params[0] = static_cast<float>(toneMapperUniform(settings));
  uniforms.visual_params[1] = settings.atmosphere.saturation;
  uniforms.visual_params[2] = settings.atmosphere.contrast;
  uniforms.visual_params[3] = settings.procedural_surface_normals ? 1.0f : 0.0f;
  uniforms.grounding_params[0] = settings.grounding.enabled ? 1.0f : 0.0f;
  uniforms.grounding_params[1] = settings.grounding.surface_occlusion_strength;
  uniforms.grounding_params[2] = settings.grounding.surface_occlusion_height;
  uniforms.grounding_params[3] = settings.grounding.reference_y;
  uniforms.grounding_tuning[0] = settings.grounding.surface_occlusion_mix;
  uniforms.grounding_tuning[1] = settings.grounding.surface_occlusion_min;
  for (std::size_t i = 0; i < settings.light_rig.size(); ++i) {
    setVec4(uniforms.lights[i].position, settings.light_rig[i].position,
            settings.light_rig[i].source_radius);
    setVec4(uniforms.lights[i].color, settings.light_rig[i].color, settings.light_rig[i].intensity);
  }
  return uniforms;
}

MetalMaterialUniforms makeMaterialUniforms(const aster::Material &material, const float opacity) {
  MetalMaterialUniforms uniforms;
  setVec4(uniforms.base_color_roughness, material.base_color, material.roughness);
  setVec4(uniforms.emission_color_metallic, material.emission_color, material.metallic);
  uniforms.detail_params[0] = material.emission_strength;
  uniforms.detail_params[1] = material.detail_strength;
  uniforms.detail_params[2] = material.detail_scale;
  uniforms.detail_params[3] = material.ambient_occlusion;
  uniforms.pattern_params[0] = static_cast<float>(static_cast<int>(material.surface_pattern));
  uniforms.pattern_params[1] = material.pattern_depth;
  uniforms.pattern_params[2] = material.pattern_contrast;
  uniforms.pattern_params[3] = material.pattern_mortar;
  uniforms.pattern_scale_opacity[0] = material.pattern_scale.x;
  uniforms.pattern_scale_opacity[1] = material.pattern_scale.y;
  uniforms.pattern_scale_opacity[2] = std::clamp(opacity, 0.0f, 1.0f);
  uniforms.pattern_scale_opacity[3] = aster::isDoubleSidedMaterial(material) ? 1.0f : 0.0f;
  uniforms.material_params[0] = material.edge_wear;
  return uniforms;
}

MetalObjectUniforms makeObjectUniforms(const aster::RenderObject &object,
                                       const aster::OrbitCamera &camera,
                                       const aster::RendererSettings &settings, const int width,
                                       const int height, const double frame_seconds) {
  const aster::Mat4 model =
      settings.animate_scene && object.spin_rate != 0.0f
          ? object.transform.matrix() *
                aster::rotation_y(static_cast<float>(frame_seconds) * object.spin_rate)
          : object.transform.matrix();
  const float aspect_ratio =
      static_cast<float>(std::max(width, 1)) / static_cast<float>(std::max(height, 1));
  const aster::Mat4 mvp = camera.projectionMatrix(aspect_ratio) * camera.viewMatrix() * model;

  MetalObjectUniforms uniforms;
  copyMat4(uniforms.model, model);
  copyMat4(uniforms.model_view_projection, mvp);
  return uniforms;
}

MTLCullMode metalCullMode(const aster::FaceCullMode mode) {
  switch (mode) {
  case aster::FaceCullMode::None:
    return MTLCullModeNone;
  case aster::FaceCullMode::Back:
    return MTLCullModeBack;
  case aster::FaceCullMode::Front:
    return MTLCullModeFront;
  }
  return MTLCullModeNone;
}

void releaseObject(void *&object) {
  if (object != nullptr) {
    [(id)object release];
    object = nullptr;
  }
}

NSString *shaderSource() {
  return @R"MSL(
#include <metal_stdlib>
using namespace metal;

constant int PatternNone = 0;
constant int PatternCourseCells = 1;
constant int PatternFiberStrands = 2;
constant int PatternGrassSoil = 3;
constant int PatternWaterSurface = 4;
constant int PatternSoilPath = 5;
constant int PatternTerrainBlend = 6;
constant int PatternFurFibers = 7;
constant int PatternLayeredTerrain = 8;
constant int PatternWeatheredStone = 9;
constant int PatternPaintedWood = 10;
constant int PatternFoliage = 11;
constant int PatternIridescentScales = 12;
constant int PatternAmberResin = 13;
constant int PatternFeatherVanes = 14;
constant int PatternTwigNest = 15;
constant int PatternReptileScales = 16;
constant int PatternCaveRock = 17;
constant int PatternCoalVein = 18;
constant int PatternContactShadow = 19;

struct VertexIn {
  packed_float3 position;
  packed_float3 normal;
  packed_float2 uv;
  packed_float4 tangent;
  float ambient_occlusion;
};

struct ObjectUniforms {
  float4x4 model;
  float4x4 model_view_projection;
};

struct LightUniform {
  float4 position;
  float4 color;
};

struct SceneUniforms {
  float4 camera_position;
  float4 sun_direction;
  float4 sun_color;
  float4 sky_ambient_color;
  float4 ground_ambient_color;
  float4 fog_color;
  float4 shadow_tint;
  float4 highlight_tint;
  float4 render_params;
  float4 atmosphere_params;
  float4 visual_params;
  float4 grounding_params;
  float4 grounding_tuning;
  LightUniform lights[4];
};

struct MaterialUniforms {
  float4 base_color_roughness;
  float4 emission_color_metallic;
  float4 detail_params;
  float4 pattern_params;
  float4 pattern_scale_opacity;
  float4 material_params;
};

struct VSOut {
  float4 position [[position]];
  float3 world_position;
  float3 normal;
  float2 uv;
  float ambient_occlusion;
};

float saturate_value(float value) {
  return clamp(value, 0.0f, 1.0f);
}

float3 mix_vec(float3 a, float3 b, float t) {
  return mix(a, b, saturate_value(t));
}

float fract_value(float value) {
  return fract(value);
}

float hash31(float3 p) {
  p = fract(float3(p.x * 0.1031f, p.y * 0.11369f, p.z * 0.13787f));
  float d = dot(p, p.yzx + 19.19f);
  p += d;
  return fract((p.x + p.y) * p.z);
}

float value_noise(float3 p) {
  float3 i = floor(p);
  float3 f = fract(p);
  f = f * f * (3.0f - 2.0f * f);
  float n000 = hash31(i + float3(0.0f, 0.0f, 0.0f));
  float n100 = hash31(i + float3(1.0f, 0.0f, 0.0f));
  float n010 = hash31(i + float3(0.0f, 1.0f, 0.0f));
  float n110 = hash31(i + float3(1.0f, 1.0f, 0.0f));
  float n001 = hash31(i + float3(0.0f, 0.0f, 1.0f));
  float n101 = hash31(i + float3(1.0f, 0.0f, 1.0f));
  float n011 = hash31(i + float3(0.0f, 1.0f, 1.0f));
  float n111 = hash31(i + float3(1.0f, 1.0f, 1.0f));
  float nx00 = mix(n000, n100, f.x);
  float nx10 = mix(n010, n110, f.x);
  float nx01 = mix(n001, n101, f.x);
  float nx11 = mix(n011, n111, f.x);
  return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}

float hash21(float2 p) {
  return fract(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453123f);
}

float aa_width(float value) {
  return max(fwidth(value), 0.0015f);
}

float filtered_stripe(float value, float width) {
  float line = abs(fract(value) - 0.5f);
  return 1.0f - smoothstep(width, width + aa_width(value), line);
}

float cell_mortar(float2 uv, float mortar_width) {
  float2 grid = abs(fract(uv) - 0.5f);
  float edge = min(grid.x, grid.y);
  float filter = max(fwidth(edge), 0.002f);
  return 1.0f - smoothstep(mortar_width, mortar_width + filter, edge);
}

bool is_terrain_pattern(int pattern) {
  return pattern == PatternGrassSoil || pattern == PatternTerrainBlend ||
         pattern == PatternLayeredTerrain;
}

float projected_noise(float3 world_position, float3 normal, float scale, float salt) {
  normal = normalize(normal);
  float3 weights = pow(abs(normal), float3(4.0f));
  weights /= max(dot(weights, float3(1.0f)), 0.0001f);
  float xy = value_noise(float3(world_position.x * scale, world_position.y * scale, salt));
  float xz =
      value_noise(float3(world_position.x * scale, world_position.z * scale, salt + 11.7f));
  float zy =
      value_noise(float3(world_position.z * scale, world_position.y * scale, salt + 23.4f));
  return xy * weights.z + xz * weights.y + zy * weights.x;
}

float terrain_fbm(float3 world_position, float3 normal, float scale, float salt) {
  float sum = 0.0f;
  float amplitude = 0.56f;
  float amplitude_sum = 0.0f;
  float frequency = scale;
  for (int octave = 0; octave < 4; ++octave) {
    sum += projected_noise(world_position, normal, frequency,
                           salt + float(octave) * 17.0f) *
           amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.08f;
    amplitude *= 0.52f;
  }
  return amplitude_sum > 0.0f ? sum / amplitude_sum : 0.0f;
}

float3 terrain_layered_albedo(float3 base, float3 world_position, float3 normal,
                              constant MaterialUniforms& material) {
  float pattern_id = material.pattern_params.x;
  float detail_scale = max(material.detail_params.z, 0.001f);
  float macro_scale = 0.018f + sqrt(detail_scale) * 0.011f;
  float mid_scale = 0.055f + detail_scale * 0.018f;
  float fine_scale = 0.38f + detail_scale * 0.055f;
  float macro = terrain_fbm(world_position, normal, macro_scale, pattern_id * 3.7f);
  float mid = terrain_fbm(world_position, normal, mid_scale, pattern_id * 5.1f + 9.0f);
  float fine = terrain_fbm(world_position, normal, fine_scale, pattern_id * 7.9f + 2.0f);
  float ridge = 1.0f - abs(terrain_fbm(world_position, normal, mid_scale * 1.85f,
                                       pattern_id * 2.4f + 31.0f) *
                               2.0f -
                           1.0f);
  float up = saturate_value(normalize(normal).y);
  float slope = 1.0f - smoothstep(0.54f, 0.92f, up);
  float altitude = smoothstep(-0.20f, 2.80f, world_position.y + (macro - 0.5f) * 0.65f);
  float soil_weight = saturate_value(slope * 0.46f + (0.54f - mid) * 0.46f + ridge * 0.10f);
  float rock_weight =
      saturate_value(smoothstep(0.70f, 1.08f, slope + ridge * 0.14f + altitude * 0.04f)) *
      0.52f;
  float grass_weight =
      saturate_value(0.70f + (macro - 0.45f) * 0.28f + (fine - 0.50f) * 0.12f -
                     soil_weight * 0.36f - rock_weight * 0.58f +
                     material.pattern_params.y * 1.10f);

  float3 grass = mix_vec(base * float3(0.88f, 1.16f, 0.68f), float3(0.24f, 0.34f, 0.16f),
                         0.18f);
  float3 dry_grass =
      mix_vec(base * float3(1.06f, 0.98f, 0.74f), float3(0.38f, 0.34f, 0.20f), 0.22f);
  float3 soil = mix_vec(base * float3(1.02f, 0.78f, 0.54f), float3(0.30f, 0.23f, 0.16f),
                        0.25f);
  float3 rock = mix_vec(base * float3(0.94f, 0.94f, 0.86f), float3(0.44f, 0.42f, 0.36f),
                        0.32f);
  float3 albedo = mix_vec(soil, dry_grass, saturate_value(grass_weight * 0.30f + macro * 0.18f));
  albedo = mix_vec(albedo, grass, grass_weight);
  albedo = mix_vec(albedo, rock, rock_weight);

  float fiber = projected_noise(world_position + float3(fine * 1.7f, 0.0f, macro), normal,
                                fine_scale * 2.35f, pattern_id + 43.0f);
  float pebble = projected_noise(world_position, normal, fine_scale * 3.8f, pattern_id + 67.0f);
  albedo *= 0.92f + fine * 0.10f + fiber * grass_weight * 0.05f;
  albedo = mix_vec(albedo, albedo * 0.82f,
                   saturate_value(pebble * soil_weight * 0.08f + slope * 0.05f));
  return clamp(albedo, float3(0.0f), float3(4.0f));
}

float pattern_height_value(float3 world_position, float3 normal, float2 uv,
                           constant MaterialUniforms& material, constant SceneUniforms& scene) {
  int pattern = int(material.pattern_params.x + 0.5f);
  if (pattern == PatternNone || pattern == PatternContactShadow) {
    return 0.0f;
  }

  float2 pattern_scale = max(abs(material.pattern_scale_opacity.xy), float2(0.001f));
  float detail_scale = max(material.detail_params.z, 0.001f);
  float contrast = material.pattern_params.z;
  float mortar = clamp(material.pattern_params.w, 0.005f, 0.45f);
  float time = scene.render_params.w;
  float3 sample_position = world_position * detail_scale + normal * 0.73f;
  float broad = value_noise(sample_position * 0.37f);

  if (pattern == PatternCourseCells) {
    float row = floor(world_position.y * 1.65f);
    float2 cell_uv = uv * pattern_scale + float2(row * 0.37f, 0.0f);
    float joints = cell_mortar(cell_uv, mortar);
    float chip = filtered_stripe(cell_uv.x + cell_uv.y * 0.31f, 0.08f);
    return chip * 0.24f - joints * (0.62f + contrast * 0.18f);
  }
  if (pattern == PatternWeatheredStone || pattern == PatternCaveRock) {
    float rock = value_noise(sample_position * 1.45f);
    float strata = filtered_stripe(world_position.y * pattern_scale.y * 0.22f + broad * 0.52f,
                                   0.20f);
    float cracks = filtered_stripe((world_position.x + world_position.z) * pattern_scale.x * 0.065f +
                                       rock * 1.80f,
                                   0.026f);
    return rock * 0.30f + strata * 0.24f - cracks * (0.58f + contrast * 0.18f);
  }
  if (pattern == PatternCoalVein) {
    float vein = filtered_stripe(world_position.x * pattern_scale.x * 0.12f +
                                     world_position.y * 0.75f + broad * 1.2f,
                                 0.09f);
    return vein * 0.56f + broad * 0.18f;
  }
  if (is_terrain_pattern(pattern)) {
    float detail_scale = max(material.detail_params.z, 0.001f);
    float fine_scale = 0.38f + detail_scale * 0.055f;
    float fine = terrain_fbm(world_position, normal, fine_scale, material.pattern_params.x * 7.9f);
    float ridge = 1.0f - abs(terrain_fbm(world_position, normal, fine_scale * 0.72f,
                                         material.pattern_params.x * 2.4f + 31.0f) *
                                 2.0f -
                             1.0f);
    float slope = 1.0f - smoothstep(0.54f, 0.92f, normalize(normal).y);
    return (fine - 0.5f) * 0.10f + ridge * 0.04f - slope * 0.025f;
  }
  if (pattern == PatternWaterSurface) {
    return sin((uv.x * pattern_scale.x * 0.42f + uv.y * pattern_scale.y * 0.38f +
                time * 0.24f + broad) *
               6.2831853f) *
           0.42f;
  }
  if (pattern == PatternFurFibers) {
    float fiber = filtered_stripe(uv.y * pattern_scale.y * 0.42f + broad * 0.74f, 0.105f);
    float nap = value_noise(float3(uv.x * pattern_scale.x * 0.62f,
                                   uv.y * pattern_scale.y * 0.26f,
                                   world_position.y * 2.0f));
    return fiber * 0.28f + nap * 0.18f;
  }
  if (pattern == PatternPaintedWood || pattern == PatternFiberStrands ||
      pattern == PatternFeatherVanes || pattern == PatternTwigNest) {
    float grain = filtered_stripe(uv.x * pattern_scale.x * 0.28f + broad * 0.9f, 0.13f);
    float lengthwise = value_noise(float3(uv.y * pattern_scale.y * 0.35f, world_position.y * 1.2f, broad));
    return grain * 0.36f + lengthwise * 0.18f;
  }
  if (pattern == PatternFoliage) {
    return filtered_stripe(uv.x * pattern_scale.x * 0.42f + abs(uv.y - 0.5f) * 1.6f,
                           0.055f) *
           0.42f;
  }
  if (pattern == PatternIridescentScales || pattern == PatternReptileScales) {
    return cell_mortar(uv * pattern_scale, 0.22f) * 0.48f;
  }
  if (pattern == PatternAmberResin) {
    return sin((world_position.x + world_position.z + broad) * pattern_scale.x * 3.2f) * 0.22f +
           broad * 0.16f;
  }
  if (pattern == PatternSoilPath) {
    return value_noise(sample_position * 2.4f) * 0.34f;
  }
  return broad * 0.20f;
}

float3 apply_pattern_normal(float3 world_position, float3 normal, float2 uv,
                            constant MaterialUniforms& material, constant SceneUniforms& scene) {
  if (scene.visual_params.w < 0.5f) {
    return normal;
  }
  int pattern = int(material.pattern_params.x + 0.5f);
  if (pattern == PatternNone || pattern == PatternContactShadow) {
    return normal;
  }

  float depth = clamp(material.pattern_params.y, 0.0f, 0.30f);
  float detail_strength = clamp(material.detail_params.y, 0.0f, 1.5f);
  float strength = clamp(depth * 0.95f + detail_strength * 0.018f, 0.0f, 0.14f);
  if (strength <= 0.0001f) {
    return normal;
  }

  float height = pattern_height_value(world_position, normal, uv, material, scene);
  float dhdx = dfdx(height);
  float dhdy = dfdy(height);
  float3 dpdx = dfdx(world_position);
  float3 dpdy = dfdy(world_position);
  float3 r1 = cross(dpdy, normal);
  float3 r2 = cross(normal, dpdx);
  float det = max(abs(dot(dpdx, r1)), 0.0001f);
  float3 gradient = (r1 * dhdx + r2 * dhdy) / det;
  float gradient_length = length(gradient);
  if (gradient_length > 1.35f) {
    gradient *= 1.35f / gradient_length;
  }
  return normalize(normal - gradient * strength);
}

float3 pattern_albedo(float3 base, float3 world_position, float3 normal, float2 uv,
                      constant MaterialUniforms& material, constant SceneUniforms& scene) {
  int pattern = int(material.pattern_params.x + 0.5f);
  if (pattern == PatternContactShadow) {
    return float3(0.0f);
  }

  float2 pattern_scale = max(abs(material.pattern_scale_opacity.xy), float2(0.001f));
  float detail_strength = clamp(material.detail_params.y, 0.0f, 1.5f);
  float detail_scale = max(material.detail_params.z, 0.001f);
  float depth = material.pattern_params.y;
  float contrast = material.pattern_params.z;
  float mortar = clamp(material.pattern_params.w, 0.005f, 0.45f);
  float time = scene.render_params.w;
  float3 sample_position = world_position * detail_scale + normal * 0.73f;
  float broad = value_noise(sample_position * 0.37f);
  float material_variation = 0.94f + (broad - 0.5f) * 0.18f;
  float3 albedo = base * mix(1.0f, material_variation, clamp(detail_strength, 0.0f, 1.0f));

  if (pattern == PatternCourseCells) {
    float row = floor(world_position.y * 1.65f);
    float2 cell_uv = uv * pattern_scale + float2(row * 0.37f, 0.0f);
    float2 cell_id = floor(cell_uv);
    float joints = cell_mortar(cell_uv, mortar);
    float chipped = hash21(cell_id);
    float face = 0.88f + (chipped - 0.5f) * 0.18f + contrast * 0.10f;
    float worn_edge = smoothstep(0.55f, 0.96f, material.material_params.x) *
                      (filtered_stripe(cell_uv.x + cell_uv.y * 0.31f, 0.08f) * 0.08f);
    albedo = mix(albedo * (face + worn_edge), albedo * (0.76f + depth * 0.16f),
                 joints * (0.24f + contrast * 0.05f));
  } else if (pattern == PatternWeatheredStone || pattern == PatternCaveRock) {
    float rock = value_noise(sample_position * 1.45f);
    float strata = filtered_stripe(world_position.y * pattern_scale.y * 0.22f + broad * 0.52f,
                                   0.20f);
    float cracks = filtered_stripe((world_position.x + world_position.z) * pattern_scale.x * 0.065f +
                                       rock * 1.80f,
                                   0.026f);
    albedo *= 0.89f + rock * 0.11f + strata * 0.035f - cracks * (0.020f + contrast * 0.025f);
  } else if (pattern == PatternCoalVein) {
    float vein = filtered_stripe(world_position.x * pattern_scale.x * 0.12f +
                                     world_position.y * 0.75f + broad * 1.2f,
                                 0.09f);
    albedo = mix(albedo * 0.58f, float3(0.02f, 0.023f, 0.026f), 0.42f + vein * 0.24f);
  } else if (is_terrain_pattern(pattern)) {
    albedo = terrain_layered_albedo(base, world_position, normal, material);
  } else if (pattern == PatternWaterSurface) {
    float wave = sin((uv.x * pattern_scale.x * 0.42f + uv.y * pattern_scale.y * 0.38f +
                      time * 0.24f + broad) *
                     6.2831853f);
    float glint = pow(saturate_value(dot(normalize(normal), normalize(float3(-0.35f, 0.85f, 0.20f))) * 0.5f + 0.5f), 8.0f);
    albedo *= 0.86f + wave * 0.055f + glint * 0.10f;
  } else if (pattern == PatternFurFibers) {
    float fiber = filtered_stripe(uv.y * pattern_scale.y * 0.42f + broad * 0.74f, 0.105f);
    float nap = value_noise(float3(uv.x * pattern_scale.x * 0.62f,
                                   uv.y * pattern_scale.y * 0.26f,
                                   world_position.y * 2.0f));
    float mottling = value_noise(sample_position * 1.82f);
    float3 undercoat = albedo * float3(0.78f, 0.70f, 0.62f);
    float3 guard_hair = albedo * float3(1.10f, 0.96f, 0.78f);
    albedo = mix(undercoat, guard_hair, saturate_value(fiber * 0.42f + mottling * 0.28f));
    albedo *= 0.84f + nap * 0.18f;
  } else if (pattern == PatternPaintedWood || pattern == PatternFiberStrands ||
             pattern == PatternFeatherVanes || pattern == PatternTwigNest) {
    float grain = filtered_stripe(uv.x * pattern_scale.x * 0.28f + broad * 0.9f, 0.13f);
    float lengthwise = value_noise(float3(uv.y * pattern_scale.y * 0.35f, world_position.y * 1.2f, broad));
    albedo *= 0.78f + grain * (0.16f + contrast * 0.12f) + lengthwise * 0.10f;
  } else if (pattern == PatternFoliage) {
    float veins = filtered_stripe(uv.x * pattern_scale.x * 0.42f + abs(uv.y - 0.5f) * 1.6f,
                                  0.055f);
    albedo *= float3(0.80f, 1.14f, 0.72f) * (0.90f + veins * 0.13f + broad * 0.08f);
  } else if (pattern == PatternIridescentScales || pattern == PatternReptileScales) {
    float2 scale_uv = uv * pattern_scale;
    float cells = cell_mortar(scale_uv, 0.22f);
    float shimmer = dot(normalize(normal), normalize(float3(0.4f, 0.8f, 0.3f))) * 0.5f + 0.5f;
    albedo *= mix(0.78f + shimmer * 0.22f, 1.08f + contrast * 0.10f, cells * 0.82f);
  } else if (pattern == PatternAmberResin) {
    float swirl = sin((world_position.x + world_position.z + broad) * pattern_scale.x * 3.2f);
    albedo *= float3(1.22f, 0.92f, 0.48f) * (0.86f + swirl * 0.08f + broad * 0.14f);
  } else if (pattern == PatternSoilPath) {
    float grit = value_noise(sample_position * 2.4f);
    albedo *= 0.82f + grit * 0.16f;
  } else {
    albedo *= material_variation;
  }

  float pattern_id = material.pattern_params.x;
  float3 macro_position = world_position * 0.22f + float3(pattern_id * 0.41f, 0.0f,
                                                          pattern_id * 0.29f);
  float macro_patch = value_noise(macro_position);
  float macro_stain = value_noise(macro_position * 0.43f + float3(2.7f, 0.0f, -1.9f));
  float upward_surface = smoothstep(0.50f, 0.92f, normalize(normal).y);
  float macro_weight = saturate_value((detail_strength * 0.20f + contrast * 0.12f) *
                                      (0.45f + upward_surface * 0.55f));
  if (is_terrain_pattern(pattern)) {
    macro_weight *= 0.20f;
  }
  albedo *= 1.0f + (macro_patch * 0.12f - macro_stain * 0.06f) * macro_weight;
  return clamp(albedo, float3(0.0f), float3(4.0f));
}

float3 aces_tonemap(float3 color) {
  constexpr float a = 2.51f;
  constexpr float b = 0.03f;
  constexpr float c = 2.43f;
  constexpr float d = 0.59f;
  constexpr float e = 0.14f;
  return clamp((color * (a * color + b)) / (color * (c * color + d) + e), float3(0.0f), float3(1.0f));
}

float3 pbr_neutral_tonemap(float3 color) {
  constexpr float start_compression = 0.76f;
  constexpr float desaturation = 0.15f;
  float x = min(color.x, min(color.y, color.z));
  float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
  color -= float3(offset);
  float peak = max(color.x, max(color.y, color.z));
  if (peak < start_compression) {
    return clamp(color, float3(0.0f), float3(1.0f));
  }
  constexpr float d = 1.0f - start_compression;
  float new_peak = 1.0f - d * d / (peak + d - start_compression);
  color *= new_peak / max(peak, 0.0001f);
  float g = 1.0f - 1.0f / (desaturation * (peak - new_peak) + 1.0f);
  return clamp(mix(color, float3(new_peak), g), float3(0.0f), float3(1.0f));
}

float3 reinhard_tonemap(float3 color) {
  return color / (color + float3(1.0f));
}

float luminance(float3 color) {
  return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 tone_map(float3 color, constant SceneUniforms& scene) {
  if (scene.visual_params.x < 0.5f) {
    return aces_tonemap(color);
  }
  if (scene.visual_params.x > 1.5f) {
    return reinhard_tonemap(color);
  }
  return pbr_neutral_tonemap(color);
}

float3 gamma_encode(float3 color) {
  return pow(clamp(color, float3(0.0f), float3(1.0f)), float3(1.0f / 2.2f));
}

float3 evaluate_light(float3 albedo, float metallic, float alpha2, float k, float3 f0,
                      float3 normal, float3 view, float n_dot_v, float3 light_dir,
                      float3 radiance) {
  float3 half_vector = normalize(light_dir + view);
  float n_dot_l = max(dot(normal, light_dir), 0.0f);
  float n_dot_h = max(dot(normal, half_vector), 0.0f);
  float h_dot_v = max(dot(half_vector, view), 0.0f);
  float distribution_denominator = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;
  float distribution =
      alpha2 / max(3.14159265f * distribution_denominator * distribution_denominator, 0.001f);
  float geometry_l = n_dot_l / max(n_dot_l * (1.0f - k) + k, 0.001f);
  float geometry_v = n_dot_v / max(n_dot_v * (1.0f - k) + k, 0.001f);
  float3 fresnel = f0 + (float3(1.0f) - f0) * pow(1.0f - h_dot_v, 5.0f);
  float3 specular = distribution * geometry_l * geometry_v * fresnel;
  float3 diffuse = albedo * (1.0f - clamp(metallic, 0.0f, 1.0f)) * 0.82f;
  return (diffuse + specular) * radiance * n_dot_l;
}

float3 shade(float3 world_position, float3 normal, float2 uv,
             float vertex_ao, constant MaterialUniforms& material, constant SceneUniforms& scene) {
  normal = normalize(normal);
  if (length(normal) <= 0.0001f) {
    normal = float3(0.0f, 1.0f, 0.0f);
  }

  float3 base = material.base_color_roughness.xyz;
  float roughness = clamp(material.base_color_roughness.w, 0.04f, 1.0f);
  float metallic = material.emission_color_metallic.w;
  float ambient_occlusion = clamp(material.detail_params.w * clamp(vertex_ao, 0.0f, 1.0f),
                                  0.0f, 1.0f);
  normal = apply_pattern_normal(world_position, normal, uv, material, scene);
  float3 albedo = pattern_albedo(base, world_position, normal, uv, material, scene);
  float sky_factor = saturate_value(normal.y * 0.5f + 0.5f);
  float3 ambient_color = mix(scene.ground_ambient_color.xyz, scene.sky_ambient_color.xyz, sky_factor);
  float ambient_level = max(scene.render_params.y * ambient_occlusion, scene.render_params.z);
  float3 color = ambient_color * albedo * ambient_level + albedo * 0.05f;
  float3 view = normalize(scene.camera_position.xyz - world_position);
  float n_dot_v = max(dot(normal, view), 0.001f);
  float3 f0 = mix(float3(0.04f), albedo, clamp(metallic, 0.0f, 1.0f));
  float perceptual_roughness = clamp(roughness, 0.045f, 1.0f);
  float alpha = perceptual_roughness * perceptual_roughness;
  float alpha2 = max(alpha * alpha, 0.0005f);
  float k = ((perceptual_roughness + 1.0f) * (perceptual_roughness + 1.0f)) * 0.125f;

  if (scene.sun_direction.w > 0.5f && scene.sun_color.w > 0.0f) {
    float sun_length = length(scene.sun_direction.xyz);
    if (sun_length > 0.0001f) {
      float3 sun_dir = scene.sun_direction.xyz / sun_length;
      color += evaluate_light(albedo, metallic, alpha2, k, f0, normal, view, n_dot_v, sun_dir,
                              scene.sun_color.xyz * scene.sun_color.w);
    }
  }

  for (uint i = 0; i < 4; ++i) {
    float3 light_vector = scene.lights[i].position.xyz - world_position;
    float distance_sq = max(dot(light_vector, light_vector), 0.0001f);
    float3 light_dir = normalize(light_vector);
    float source_radius = scene.lights[i].position.w;
    float softened_distance = max(distance_sq, source_radius * source_radius + 0.0001f);
    float3 radiance = scene.lights[i].color.xyz * (scene.lights[i].color.w / softened_distance);
    color += evaluate_light(albedo, metallic, alpha2, k, f0, normal, view, n_dot_v, light_dir,
                            radiance);
  }

  if (scene.grounding_params.x > 0.5f && scene.grounding_params.y > 0.0f) {
    float height = max(scene.grounding_params.z, 0.001f);
    float above_reference = max(world_position.y - scene.grounding_params.w, 0.0f);
    float proximity = 1.0f - smoothstep(0.0f, height, above_reference);
    float vertical_receiver = (1.0f - smoothstep(0.48f, 0.92f, normal.y)) * 0.35f;
    float raw = 1.0f - clamp(proximity * vertical_receiver * scene.grounding_params.y, 0.0f, 0.70f);
    color *= mix(float3(1.0f), float3(max(raw, clamp(scene.grounding_tuning.y, 0.0f, 1.0f))),
                 clamp(scene.grounding_tuning.x, 0.0f, 1.0f));
  }

  if (scene.atmosphere_params.x > 0.5f) {
    float fog_range = max(scene.atmosphere_params.z - scene.atmosphere_params.y, 0.001f);
    float fog = smoothstep(0.0f, 1.0f,
                           (length(scene.camera_position.xyz - world_position) - scene.atmosphere_params.y) /
                               fog_range) *
                clamp(scene.atmosphere_params.w, 0.0f, 1.0f);
    color = mix(color, scene.fog_color.xyz, fog);
  }

  float pre_grade_luma = luminance(color);
  color = mix(color, color * scene.shadow_tint.xyz,
              clamp(scene.shadow_tint.w, 0.0f, 1.0f) *
                  (1.0f - smoothstep(0.05f, 0.48f, pre_grade_luma)));
  color = mix(color, color * scene.highlight_tint.xyz,
              clamp(scene.highlight_tint.w, 0.0f, 1.0f) *
                  smoothstep(0.52f, 1.45f, pre_grade_luma));

  color += material.emission_color_metallic.xyz * material.detail_params.x;
  color *= scene.render_params.x;
  color = tone_map(color, scene);
  float post_luma = luminance(color);
  color = mix(float3(post_luma), color, clamp(scene.visual_params.y, 0.0f, 2.0f));
  color = (color - float3(0.5f)) * max(scene.visual_params.z, 0.0f) + float3(0.5f);
  return gamma_encode(clamp(color, float3(0.0f), float3(1.0f)));
}

vertex VSOut vs_main(const device VertexIn* vertices [[buffer(0)]],
                     constant ObjectUniforms& object [[buffer(1)]],
                     uint id [[vertex_id]]) {
  VertexIn vtx = vertices[id];
  float4 world = object.model * float4(float3(vtx.position), 1.0f);
  float4 clip = object.model_view_projection * float4(float3(vtx.position), 1.0f);
  clip.z = (clip.z + clip.w) * 0.5f;
  VSOut out;
  out.position = clip;
  out.world_position = world.xyz;
  out.normal = normalize((object.model * float4(float3(vtx.normal), 0.0f)).xyz);
  out.uv = float2(vtx.uv);
  out.ambient_occlusion = vtx.ambient_occlusion;
  return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        constant MaterialUniforms& material [[buffer(0)]],
                        constant SceneUniforms& scene [[buffer(1)]],
                        bool front_facing [[front_facing]]) {
  float opacity = clamp(material.pattern_scale_opacity.z, 0.0f, 1.0f);
  int pattern = int(material.pattern_params.x + 0.5f);
  if (pattern == PatternContactShadow) {
    return float4(0.0f, 0.0f, 0.0f, opacity);
  }
  float3 normal = in.normal;
  if (material.pattern_scale_opacity.w > 0.5f && !front_facing) {
    normal = -normal;
  }
  return float4(shade(in.world_position, normal, in.uv, in.ambient_occlusion, material, scene),
                opacity);
}
)MSL";
}

} // namespace

namespace aster {

RenderDevice::RenderDevice() = default;

RenderDevice::~RenderDevice() {
  for (auto &[_, buffers] : native_mesh_cache_) {
    releaseNativeMeshBuffers(buffers);
  }
  native_mesh_cache_.clear();
  releaseObject(native_depth_texture_);
  releaseObject(native_color_texture_);
  releaseObject(native_depth_read_state_);
  releaseObject(native_depth_write_state_);
  releaseObject(native_pipeline_);
  releaseObject(native_library_);
  releaseObject(native_queue_);
  releaseObject(native_device_);
}

void RenderDevice::releaseNativeMeshBuffers(NativeMeshBuffers &buffers) {
  releaseObject(buffers.vertex_buffer);
  releaseObject(buffers.index_buffer);
  buffers.index_count = 0;
}

const CpuMesh &RenderDevice::meshForPrimitive(const MeshPrimitive primitive) const {
  switch (primitive) {
  case MeshPrimitive::Box:
    return box_;
  case MeshPrimitive::Sphere:
    return sphere_;
  case MeshPrimitive::Plane:
    return plane_;
  case MeshPrimitive::Rock:
    return rock_;
  case MeshPrimitive::Crystal:
    return crystal_;
  case MeshPrimitive::RuinBlock:
    return ruin_block_;
  case MeshPrimitive::Pillar:
    return pillar_;
  }
  return sphere_;
}

const CpuMesh &RenderDevice::meshForObject(const RenderObject &object) {
  if (object.custom_mesh == nullptr) {
    return meshForPrimitive(object.primitive);
  }

  const CpuMesh *source = object.custom_mesh.get();
  auto it = custom_mesh_cache_.find(source);
  if (it == custom_mesh_cache_.end()) {
    it = custom_mesh_cache_.emplace(source, prepareMeshForRendering(*source, renderMeshOptions()))
             .first;
  }
  return it->second;
}

RenderDevice::NativeMeshBuffers &RenderDevice::nativeBuffersForMesh(const CpuMesh &mesh) {
  auto it = native_mesh_cache_.find(&mesh);
  if (it != native_mesh_cache_.end()) {
    return it->second;
  }

  RenderDevice::NativeMeshBuffers buffers;
  buffers.index_count = static_cast<std::uint32_t>(mesh.indices.size());
  id<MTLDevice> metal_device = (id<MTLDevice>)native_device_;
  if (!mesh.vertices.empty()) {
    buffers.vertex_buffer = [metal_device newBufferWithBytes:mesh.vertices.data()
                                                      length:mesh.vertices.size() * sizeof(Vertex)
                                                     options:MTLResourceStorageModeShared];
  }
  if (!mesh.indices.empty()) {
    buffers.index_buffer =
        [metal_device newBufferWithBytes:mesh.indices.data()
                                  length:mesh.indices.size() * sizeof(std::uint32_t)
                                 options:MTLResourceStorageModeShared];
  }
  it = native_mesh_cache_.emplace(&mesh, buffers).first;
  return it->second;
}

void RenderDevice::initialize() {
  ASTER_PROFILE_SCOPE("RenderDevice::initialize");
  const MeshProcessOptions mesh_options = renderMeshOptions();
  box_ = prepareMeshForRendering(makeBox(), mesh_options);
  sphere_ =
      prepareMeshForRendering(makeUvSphere(kSphereSegments, kSphereRings, 1.0f), mesh_options);
  plane_ = prepareMeshForRendering(makePlane(kPlaneSize), mesh_options);
  contact_shadow_plane_ = prepareMeshForRendering(makePlane(kContactShadowPlaneSize), mesh_options);
  rock_ = prepareMeshForRendering(makeRock(kRockSegments, kRockRings, 1.0f), mesh_options);
  crystal_ = prepareMeshForRendering(makeCrystal(kCrystalSides, 1.0f, 1.8f), mesh_options);
  ruin_block_ = prepareMeshForRendering(makeRuinBlock(), mesh_options);
  pillar_ = prepareMeshForRendering(makePillar(kPillarSides, 1.0f, 1.0f), mesh_options);

  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      throw std::runtime_error("Native GPU device is unavailable.");
    }
    native_device_ = device;

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      throw std::runtime_error("Could not create native GPU command queue.");
    }
    native_queue_ = queue;

    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource() options:nil error:&error];
    if (library == nil) {
      const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
      throw std::runtime_error(std::string("Could not compile native renderer shaders: ") +
                               message);
    }
    native_library_ = library;

    id<MTLFunction> vertex = [library newFunctionWithName:@"vs_main"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"fs_main"];
    MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
    pipeline_desc.vertexFunction = vertex;
    pipeline_desc.fragmentFunction = fragment;
    pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pipeline_desc.colorAttachments[0].blendingEnabled = YES;
    pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipeline_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipeline_desc.colorAttachments[0].destinationAlphaBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;
    pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipeline_desc
                                                                                 error:&error];
    [pipeline_desc release];
    [vertex release];
    [fragment release];
    if (pipeline == nil) {
      const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
      throw std::runtime_error(std::string("Could not create native renderer pipeline: ") +
                               message);
    }
    native_pipeline_ = pipeline;

    MTLDepthStencilDescriptor *depth_write_desc = [[MTLDepthStencilDescriptor alloc] init];
    depth_write_desc.depthCompareFunction = MTLCompareFunctionLess;
    depth_write_desc.depthWriteEnabled = YES;
    native_depth_write_state_ = [device newDepthStencilStateWithDescriptor:depth_write_desc];
    [depth_write_desc release];

    MTLDepthStencilDescriptor *depth_read_desc = [[MTLDepthStencilDescriptor alloc] init];
    depth_read_desc.depthCompareFunction = MTLCompareFunctionLessEqual;
    depth_read_desc.depthWriteEnabled = NO;
    native_depth_read_state_ = [device newDepthStencilStateWithDescriptor:depth_read_desc];
    [depth_read_desc release];
  }
}

void RenderDevice::prepareScene(const Scene &scene) {
  ASTER_PROFILE_SCOPE("RenderDevice::prepareScene");
  std::unordered_set<const CpuMesh *> live_meshes;
  live_meshes.reserve(scene.objects().size());
  for (const RenderObject &object : scene.objects()) {
    if (object.custom_mesh != nullptr) {
      live_meshes.insert(object.custom_mesh.get());
    }
  }

  for (auto it = custom_mesh_cache_.begin(); it != custom_mesh_cache_.end();) {
    if (live_meshes.contains(it->first)) {
      ++it;
      continue;
    }
    const CpuMesh *prepared_mesh = &it->second;
    auto buffers = native_mesh_cache_.find(prepared_mesh);
    if (buffers != native_mesh_cache_.end()) {
      releaseNativeMeshBuffers(buffers->second);
      native_mesh_cache_.erase(buffers);
    }
    it = custom_mesh_cache_.erase(it);
  }

  for (const CpuMesh *mesh : live_meshes) {
    if (!custom_mesh_cache_.contains(mesh)) {
      custom_mesh_cache_.emplace(mesh, prepareMeshForRendering(*mesh, renderMeshOptions()));
    }
  }
}

void RenderDevice::ensureNativeRenderTargets(const int width, const int height) {
  if (native_width_ == width && native_height_ == height && native_color_texture_ != nullptr &&
      native_depth_texture_ != nullptr) {
    return;
  }

  releaseObject(native_depth_texture_);
  releaseObject(native_color_texture_);
  native_width_ = width;
  native_height_ = height;

  id<MTLDevice> metal_device = (id<MTLDevice>)native_device_;
  MTLTextureDescriptor *color_desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:static_cast<NSUInteger>(width)
                                                        height:static_cast<NSUInteger>(height)
                                                     mipmapped:NO];
  color_desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  color_desc.storageMode = MTLStorageModePrivate;
  native_color_texture_ = [metal_device newTextureWithDescriptor:color_desc];

  MTLTextureDescriptor *depth_desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                         width:static_cast<NSUInteger>(width)
                                                        height:static_cast<NSUInteger>(height)
                                                     mipmapped:NO];
  depth_desc.usage = MTLTextureUsageRenderTarget;
  depth_desc.storageMode = MTLStorageModePrivate;
  native_depth_texture_ = [metal_device newTextureWithDescriptor:depth_desc];
}

FrameStats RenderDevice::render(const Scene &scene, const OrbitCamera &camera,
                                const RendererSettings &settings, const int framebuffer_width,
                                const int framebuffer_height, const double frame_seconds) {
  ASTER_PROFILE_SCOPE("RenderDevice::render");
  FrameStats stats;
  stats.frame_seconds = frame_seconds;
  stats.framebuffer_width = framebuffer_width;
  stats.framebuffer_height = framebuffer_height;

  if (framebuffer_width <= 0 || framebuffer_height <= 0) {
    return stats;
  }

  @autoreleasepool {
    ensureNativeRenderTargets(framebuffer_width, framebuffer_height);
    id<MTLTexture> color_texture = (id<MTLTexture>)native_color_texture_;
    id<MTLTexture> depth_texture = (id<MTLTexture>)native_depth_texture_;
    id<MTLCommandQueue> queue = (id<MTLCommandQueue>)native_queue_;
    if (color_texture == nil || depth_texture == nil || queue == nil) {
      publishNativeFrameTexture(nullptr, nullptr, 0, 0);
      activeFrameBuffer().resize(framebuffer_width, framebuffer_height);
      activeFrameBuffer().clear(settings.pipeline.clear_color);
      return stats;
    }

    const Vec3 camera_position = camera.position();
    std::vector<bool> occlusion_fade_candidates;
    occlusion_fade_candidates.reserve(scene.objects().size());
    std::vector<TransparentDrawItem> transparent_draw_items;
    transparent_draw_items.reserve(scene.objects().size());
    for (std::size_t i = 0; i < scene.objects().size(); ++i) {
      const RenderObject &object = scene.objects()[i];
      const bool fade_candidate = intersectsLineOfSightFade(object, settings.line_of_sight_fade);
      occlusion_fade_candidates.push_back(fade_candidate);
      if (aster::isMaterialTranslucent(object.material) || fade_candidate) {
        transparent_draw_items.push_back(
            {i, fade_candidate, objectSortDistanceSq(object, camera_position)});
      }
    }
    std::sort(transparent_draw_items.begin(), transparent_draw_items.end(),
              [](const TransparentDrawItem lhs, const TransparentDrawItem rhs) {
                return lhs.distance_sq > rhs.distance_sq;
              });

    id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = color_texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(settings.pipeline.clear_color.x, settings.pipeline.clear_color.y,
                          settings.pipeline.clear_color.z, 1.0);
    pass.depthAttachment.texture = depth_texture;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = 1.0;

    id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)native_pipeline_];
    const MTLViewport viewport{
        0.0, 0.0, static_cast<double>(framebuffer_width), static_cast<double>(framebuffer_height),
        0.0, 1.0};
    [encoder setViewport:viewport];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];

    const MetalSceneUniforms scene_uniforms = makeSceneUniforms(camera, settings, frame_seconds);
    const auto draw_object = [&](const RenderObject &object, const float opacity) {
      const CpuMesh &mesh = isContactShadowUtility(object) && object.custom_mesh == nullptr
                                ? contact_shadow_plane_
                                : meshForObject(object);
      NativeMeshBuffers &buffers = nativeBuffersForMesh(mesh);
      if (buffers.vertex_buffer == nullptr || buffers.index_buffer == nullptr ||
          buffers.index_count == 0u) {
        return;
      }

      const MetalObjectUniforms object_uniforms = makeObjectUniforms(
          object, camera, settings, framebuffer_width, framebuffer_height, frame_seconds);
      const MetalMaterialUniforms material_uniforms =
          makeMaterialUniforms(object.material, opacity);
      const bool alpha_blend = opacity < 0.999f || aster::isMaterialTranslucent(object.material);
      [encoder setDepthStencilState:alpha_blend
                                        ? (id<MTLDepthStencilState>)native_depth_read_state_
                                        : (id<MTLDepthStencilState>)native_depth_write_state_];
      [encoder setCullMode:metalCullMode(objectCullMode(object, camera_position,
                                                        settings.pipeline.back_face_culling))];
      [encoder setVertexBuffer:(id<MTLBuffer>)buffers.vertex_buffer offset:0 atIndex:0];
      [encoder setVertexBytes:&object_uniforms length:sizeof(object_uniforms) atIndex:1];
      [encoder setFragmentBytes:&material_uniforms length:sizeof(material_uniforms) atIndex:0];
      [encoder setFragmentBytes:&scene_uniforms length:sizeof(scene_uniforms) atIndex:1];
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:buffers.index_count
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:(id<MTLBuffer>)buffers.index_buffer
                   indexBufferOffset:0];
      ++stats.draw_calls;
    };

    for (std::size_t i = 0; i < scene.objects().size(); ++i) {
      const RenderObject &object = scene.objects()[i];
      if (!aster::isMaterialTranslucent(object.material) && !occlusion_fade_candidates[i]) {
        draw_object(object, object.material.opacity);
      }
    }

    for (const RenderObject &object : scene.objects()) {
      if (canCastContactShadow(object, settings.grounding)) {
        const RenderObject shadow = contactShadowObjectFor(object, settings.grounding);
        draw_object(shadow, shadow.material.opacity);
      }
    }

    for (const TransparentDrawItem item : transparent_draw_items) {
      const RenderObject &object = scene.objects()[item.object_index];
      const float opacity = item.occlusion_fade ? std::min(object.material.opacity,
                                                           settings.line_of_sight_fade.min_opacity)
                                                : object.material.opacity;
      draw_object(object, opacity);
    }

    [encoder endEncoding];

    [command_buffer commit];
    publishNativeFrameTexture(color_texture, queue, framebuffer_width, framebuffer_height);
    activeFrameBuffer().resize(framebuffer_width, framebuffer_height);
    activeFrameBuffer().clearTransparentColor();
  }

  return stats;
}

const char *RenderDevice::backendName() const {
  return "Aster Learning Native Renderer";
}

} // namespace aster
