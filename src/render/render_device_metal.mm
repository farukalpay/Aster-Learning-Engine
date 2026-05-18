// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#include "aster/core/profiler.hpp"
#include "aster/math/mat4.hpp"
#include "aster/render/render_graph_executor.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/scene.hpp"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct MetalFrameState {
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  id<MTLTexture> scene_texture = nil;
  id<MTLCommandBuffer> scene_command_buffer = nil;
  int scene_width = 0;
  int scene_height = 0;
  bool has_scene = false;

  id<MTLLibrary> present_library = nil;
  id<MTLRenderPipelineState> present_pipeline = nil;
  id<MTLTexture> overlay_texture = nil;
  int overlay_width = 0;
  int overlay_height = 0;
};

aster::rhi::DeviceCapabilities metalCapabilityTable() {
  aster::rhi::DeviceCapabilities table;
  table.backend = aster::rhi::BackendKind::Metal;
  table.shader_materials = true;
  table.texture_sampling = false;
  table.instancing = true;
  table.capture = true;
  table.ui_composite = true;
  table.gpu_timestamps = false;
  table.color_format_mask =
      aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Rgba8Unorm) |
      aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Bgra8Unorm);
  table.depth_format_mask =
      aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Depth32Float);
  table.sample_count_mask = aster::rhi::sampleCountCapabilityBit(1u);
  table.sampler_filter_mask =
      aster::rhi::filterModeCapabilityBit(aster::rhi::FilterMode::Linear);
  table.sampler_address_mode_mask =
      aster::rhi::addressModeCapabilityBit(aster::rhi::AddressMode::ClampToEdge);
  table.blend_mode_mask = aster::rhi::blendModeCapabilityBit(aster::rhi::BlendMode::Opaque) |
                          aster::rhi::blendModeCapabilityBit(aster::rhi::BlendMode::AlphaBlend);
  table.shader_model = aster::rhi::ShaderModel::MetalMSL23;
  table.presentation = aster::rhi::PresentationMode::MetalLayer;
  table.limits.max_color_attachments = 1u;
  table.limits.max_uniform_buffers_per_stage = 2u;
  table.limits.max_bind_groups = 3u;
  table.limits.max_vertex_attributes = 4u;
  table.limits.max_texture_dimension_2d = 16384u;
  table.limits.max_dynamic_uniform_bytes = 256u * 1024u;
  return table;
}

MetalFrameState &metalFrameState() {
  static MetalFrameState state;
  return state;
}

id<MTLDevice> sharedMetalDevice() {
  MetalFrameState &state = metalFrameState();
  if (state.device == nil) {
    state.device = MTLCreateSystemDefaultDevice();
    [state.device retain];
  }
  return state.device;
}

id<MTLCommandQueue> sharedMetalQueue() {
  MetalFrameState &state = metalFrameState();
  id<MTLDevice> device = sharedMetalDevice();
  if (device != nil && state.queue == nil) {
    state.queue = [device newCommandQueue];
  }
  return state.queue;
}

void publishSceneTexture(id<MTLTexture> texture, id<MTLCommandBuffer> command_buffer,
                         const int width, const int height) {
  MetalFrameState &state = metalFrameState();
  [state.scene_texture release];
  [state.scene_command_buffer release];
  state.scene_texture = texture;
  state.scene_command_buffer = command_buffer;
  [state.scene_texture retain];
  [state.scene_command_buffer retain];
  state.scene_width = width;
  state.scene_height = height;
  state.has_scene = texture != nil && width > 0 && height > 0;
}

struct MetalVertex {
  float position[4]{};
  float normal[4]{};
  float uv_ao[4]{};
};

struct MetalLightUniform {
  float position_radius[4]{};
  float color_intensity[4]{};
};

struct MetalSceneUniforms {
  float camera_time[4]{};
  float sun_direction_enabled[4]{};
  float sun_color_intensity[4]{};
  float sky_ambient[4]{};
  float ground_ambient[4]{};
  float exposure_ambient[4]{};
  float fog_color_strength[4]{};
  float fog_params[4]{};
  float shadow_tint_strength[4]{};
  float highlight_tint_strength[4]{};
  MetalLightUniform lights[aster::kRenderLightCount]{};
};

struct MetalObjectUniforms {
  float model[16]{};
  float model_view_projection[16]{};
  float base_color_opacity[4]{};
  float emission_strength[4]{};
  float material_params[4]{};
  float pattern_params[4]{};
  float pattern_params2[4]{};
  float procedural_params[4]{};
  float procedural_params2[4]{};
  float material_flags[4]{};
};

struct MetalMeshBuffers {
  id<MTLBuffer> vertices = nil;
  id<MTLBuffer> indices = nil;
  NSUInteger index_count = 0;
};

constexpr std::size_t kObjectUniformStride = 256u;

struct LocalBounds {
  aster::Vec3 min{};
  aster::Vec3 max{};
};

float saturate(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(const float edge0, const float edge1, const float value) {
  const float range = std::max(edge1 - edge0, 0.0001f);
  const float t = saturate((value - edge0) / range);
  return t * t * (3.0f - 2.0f * t);
}

bool isTerrainLayerPattern(const aster::SurfacePattern pattern) {
  return pattern == aster::SurfacePattern::GrassSoil ||
         pattern == aster::SurfacePattern::SoilPath ||
         pattern == aster::SurfacePattern::TerrainBlend ||
         pattern == aster::SurfacePattern::LayeredTerrain;
}

bool isStructuredSurfacePattern(const aster::SurfacePattern pattern) {
  return pattern == aster::SurfacePattern::CourseCells ||
         pattern == aster::SurfacePattern::FiberStrands ||
         pattern == aster::SurfacePattern::FurFibers ||
         pattern == aster::SurfacePattern::WeatheredStone ||
         pattern == aster::SurfacePattern::PaintedWood ||
         pattern == aster::SurfacePattern::Foliage ||
         pattern == aster::SurfacePattern::IridescentScales ||
         pattern == aster::SurfacePattern::AmberResin ||
         pattern == aster::SurfacePattern::FeatherVanes ||
         pattern == aster::SurfacePattern::TwigNest ||
         pattern == aster::SurfacePattern::ReptileScales ||
         pattern == aster::SurfacePattern::CaveRock || pattern == aster::SurfacePattern::CoalVein ||
         pattern == aster::SurfacePattern::CaveWeb ||
         pattern == aster::SurfacePattern::CaveSkitterChitin ||
         pattern == aster::SurfacePattern::CaveSkitterEye ||
         pattern == aster::SurfacePattern::WeatheredMetal ||
         pattern == aster::SurfacePattern::WeldBead;
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
  return std::abs(delta.x) <= volume.half_extents.x &&
         std::abs(delta.y) <= volume.half_extents.y &&
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

MTLCullMode metalCullMode(const aster::FaceCullMode mode) {
  switch (mode) {
  case aster::FaceCullMode::Back:
    return MTLCullModeBack;
  case aster::FaceCullMode::Front:
    return MTLCullModeFront;
  case aster::FaceCullMode::None:
    return MTLCullModeNone;
  }
  return MTLCullModeNone;
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

bool canCastContactShadow(const aster::RenderObject &object,
                          const aster::GroundingSettings &grounding) {
  const bool requested =
      object.casts_contact_shadow || (grounding.auto_contact_shadows && object.auto_contact_shadow);
  if (!grounding.enabled || !grounding.contact_shadows || !requested) {
    return false;
  }
  if (object.material.render_role == aster::MaterialRenderRole::SupportSurface ||
      aster::classifyMaterialRenderQueue(object.material) != aster::MaterialRenderQueue::Opaque ||
      isContactShadowUtility(object) ||
      (object.custom_mesh == nullptr && object.primitive == aster::MeshPrimitive::Plane)) {
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
  return std::abs(objectFootY(object, bounds) - grounding.reference_y) <=
         std::max(grounding.contact_shadow_receiver_height, 0.0f);
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
  constexpr float kContactShadowOpacityCeiling = 0.42f;
  shadow.material.opacity =
      std::clamp(grounding.contact_shadow_strength * object.contact_shadow_strength * fade, 0.0f,
                 kContactShadowOpacityCeiling);
  shadow.material.surface_pattern = aster::SurfacePattern::ContactShadow;
  shadow.material.alpha_mode = aster::MaterialAlphaMode::Blend;
  shadow.material.depth_write = aster::MaterialDepthWrite::Disabled;
  shadow.material.camera_occlusion = aster::CameraOcclusionPolicy::Solid;
  shadow.material.double_sided = true;
  shadow.casts_contact_shadow = false;
  shadow.camera_occlusion_fade = false;
  return shadow;
}

const aster::CpuMesh *meshForObject(const aster::RenderObject &object,
                                    const aster::PreparedRenderMeshes &meshes) {
  if (isContactShadowUtility(object) && object.custom_mesh == nullptr) {
    return meshes.contact_shadow_plane;
  }
  if (object.custom_mesh != nullptr) {
    if (object.dynamic_mesh.valid()) {
      if (meshes.custom_mesh_resources == nullptr) {
        return nullptr;
      }
      const auto it = meshes.custom_mesh_resources->find(object.dynamic_mesh);
      return it == meshes.custom_mesh_resources->end() ? nullptr : &it->second;
    }
    if (meshes.custom_meshes == nullptr) {
      return nullptr;
    }
    const auto it = meshes.custom_meshes->find(object.custom_mesh.get());
    return it == meshes.custom_meshes->end() ? nullptr : &it->second;
  }
  switch (object.primitive) {
  case aster::MeshPrimitive::Box:
    return meshes.box;
  case aster::MeshPrimitive::Sphere:
    return meshes.sphere;
  case aster::MeshPrimitive::Plane:
    return meshes.plane;
  case aster::MeshPrimitive::Rock:
    return meshes.rock;
  case aster::MeshPrimitive::Crystal:
    return meshes.crystal;
  case aster::MeshPrimitive::RuinBlock:
    return meshes.ruin_block;
  case aster::MeshPrimitive::Pillar:
    return meshes.pillar;
  }
  return meshes.sphere;
}

bool ensurePresenterPipeline() {
  MetalFrameState &state = metalFrameState();
  id<MTLDevice> device = sharedMetalDevice();
  if (device == nil) {
    return false;
  }
  if (state.present_pipeline != nil) {
    return true;
  }

  NSString *source =
      @"#include <metal_stdlib>\n"
       "using namespace metal;\n"
       "struct VSOut { float4 position [[position]]; float2 uv; };\n"
       "vertex VSOut present_vs(uint id [[vertex_id]]) {\n"
       "  float2 pos[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };\n"
       "  float2 uv[3] = { float2(0.0, 0.0), float2(2.0, 0.0), float2(0.0, 2.0) };\n"
       "  VSOut out; out.position = float4(pos[id], 0.0, 1.0); out.uv = uv[id]; return out;\n"
       "}\n"
       "fragment float4 present_fs(VSOut in [[stage_in]], texture2d<float> scene [[texture(0)]], "
       "texture2d<float> overlay [[texture(1)]], constant uint &mode [[buffer(0)]]) {\n"
       "  constexpr sampler s(address::clamp_to_edge, filter::linear);\n"
       "  float2 uv = float2(in.uv.x, 1.0 - in.uv.y);\n"
       "  float4 base = ((mode & 1u) != 0u) ? scene.sample(s, uv) : float4(0.0, 0.0, 0.0, "
       "1.0);\n"
       "  float4 over = ((mode & 2u) != 0u) ? overlay.sample(s, uv) : float4(0.0);\n"
       "  return float4(base.rgb * (1.0 - over.a) + over.rgb * over.a, 1.0);\n"
       "}\n";

  NSError *error = nil;
  state.present_library = [device newLibraryWithSource:source options:nil error:&error];
  if (state.present_library == nil) {
    return false;
  }
  id<MTLFunction> vertex = [state.present_library newFunctionWithName:@"present_vs"];
  id<MTLFunction> fragment = [state.present_library newFunctionWithName:@"present_fs"];
  MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
  desc.vertexFunction = vertex;
  desc.fragmentFunction = fragment;
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
  state.present_pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
  [desc release];
  [vertex release];
  [fragment release];
  return state.present_pipeline != nil;
}

bool uploadOverlayTexture() {
  MetalFrameState &state = metalFrameState();
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  if (framebuffer.empty() || framebuffer.width() != state.scene_width ||
      framebuffer.height() != state.scene_height) {
    return false;
  }
  id<MTLDevice> device = sharedMetalDevice();
  if (device == nil) {
    return false;
  }
  if (state.overlay_texture == nil || state.overlay_width != framebuffer.width() ||
      state.overlay_height != framebuffer.height()) {
    [state.overlay_texture release];
    MTLTextureDescriptor *desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:framebuffer.width()
                                                          height:framebuffer.height()
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    state.overlay_texture = [device newTextureWithDescriptor:desc];
    state.overlay_width = framebuffer.width();
    state.overlay_height = framebuffer.height();
  }
  const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
  MTLRegion region = MTLRegionMake2D(0, 0, framebuffer.width(), framebuffer.height());
  [state.overlay_texture replaceRegion:region
                           mipmapLevel:0
                             withBytes:rgba.data()
                           bytesPerRow:static_cast<NSUInteger>(framebuffer.width()) * 4u];
  return true;
}

class MetalNativeRenderBackend final : public aster::NativeRenderBackend {
public:
  ~MetalNativeRenderBackend() override {
    for (auto &entry : mesh_buffers_) {
      [entry.second.vertices release];
      [entry.second.indices release];
    }
    [library_ release];
    [opaque_pipeline_ release];
    [transparent_pipeline_ release];
    [opaque_depth_ release];
    [read_only_depth_ release];
    [transparent_depth_ release];
    [disabled_depth_ release];
    [scene_texture_ release];
    [depth_texture_ release];
    [object_uniform_buffer_ release];
  }

  bool initialize() override {
    @autoreleasepool {
      device_ = sharedMetalDevice();
      queue_ = sharedMetalQueue();
      if (device_ == nil || queue_ == nil) {
        return false;
      }

      NSString *source =
          @"#include <metal_stdlib>\n"
           "using namespace metal;\n"
           "struct Vertex { float4 position; float4 normal; float4 uv_ao; };\n"
           "struct Light { float4 position_radius; float4 color_intensity; };\n"
           "struct Scene { float4 camera_time; float4 sun_direction_enabled; float4 "
           "sun_color_intensity; "
           "float4 sky_ambient; float4 ground_ambient; float4 exposure_ambient; float4 "
           "fog_color_strength; "
           "float4 fog_params; float4 shadow_tint_strength; float4 highlight_tint_strength; Light "
           "lights[4]; };\n"
           "struct Object { float4x4 model; float4x4 mvp; float4 base_color_opacity; "
           "float4 emission_strength; float4 material_params; float4 pattern_params; "
           "float4 pattern_params2; float4 procedural_params; float4 procedural_params2; "
           "float4 material_flags; };\n"
           "struct VSOut { float4 position [[position]]; float3 world; float3 normal; float2 uv; "
           "float ao; uint object_index; };\n"
           "float saturate1(float v) { return clamp(v, 0.0, 1.0); }\n"
           "float smooth1(float a, float b, float v) { float t = saturate1((v - a) / max(b - a, "
           "0.0001)); return t * t * (3.0 - 2.0 * t); }\n"
           "float fract1(float v) { return v - floor(v); }\n"
           "float hash31(float3 p) { p = fract(p * float3(0.1031, 0.11369, 0.13787)); float d = "
           "dot(p, p.yzx + 19.19); p += d; return fract1((p.x + p.y) * p.z); }\n"
           "float noise3(float3 p) { float3 i = floor(p); float3 f = fract(p); f = f * f * (3.0 - "
           "2.0 * f); "
           "float n000 = hash31(i + float3(0,0,0)); float n100 = hash31(i + float3(1,0,0)); "
           "float n010 = hash31(i + float3(0,1,0)); float n110 = hash31(i + float3(1,1,0)); "
           "float n001 = hash31(i + float3(0,0,1)); float n101 = hash31(i + float3(1,0,1)); "
           "float n011 = hash31(i + float3(0,1,1)); float n111 = hash31(i + float3(1,1,1)); "
           "return mix(mix(mix(n000,n100,f.x), mix(n010,n110,f.x), f.y), mix(mix(n001,n101,f.x), "
           "mix(n011,n111,f.x), f.y), f.z); }\n"
           "float3 gamma_encode(float3 c) { return pow(clamp(c, 0.0, 1.0), float3(1.0 / 2.2)); }\n"
           "float3 tonemap(float3 c) { return c / (float3(1.0) + max(c, float3(0.0))); }\n"
           "float grid_line(float2 p, float mortar) { float2 f = fract(p); float2 edge = min(f, "
           "1.0 - f); float d = min(edge.x, edge.y); return 1.0 - smooth1(mortar, mortar + 0.035, "
           "d); }\n"
           "float is_pattern(float pattern, float id) { return abs(pattern - id) < 0.5 ? 1.0 : "
           "0.0; }\n"
           "float ridge1(float v) { return 1.0 - abs(v * 2.0 - 1.0); }\n"
           "float projected_noise(float3 world, float3 normal, float scale, float salt) { "
           "float3 n = normalize(normal); if (length(n) < 0.001) n = float3(0.0, 1.0, 0.0); "
           "float3 w = pow(abs(n), float3(4.0)); w /= max(w.x + w.y + w.z, 0.0001); "
           "float xy = noise3(float3(world.x * scale, world.y * scale, salt)); "
           "float xz = noise3(float3(world.x * scale, world.z * scale, salt + 11.7)); "
           "float zy = noise3(float3(world.z * scale, world.y * scale, salt + 23.4)); "
           "return xy * w.z + xz * w.y + zy * w.x; }\n"
           "float projected_fbm(float3 world, float3 normal, float scale, float salt) { "
           "float sum = 0.0; float amp = 0.56; float norm = 0.0; float f = max(scale, 0.0001); "
           "for (int i = 0; i < 4; ++i) { sum += projected_noise(world, normal, f, salt + float(i) "
           "* 17.0) * amp; norm += amp; f *= 2.08; amp *= 0.52; } "
           "return norm > 0.0 ? sum / norm : 0.0; }\n"
           "float3 structured_stone(float3 base, constant Object &object, float3 world, float3 "
           "normal) { "
           "float2 structural_uv = abs(normal.y) > 0.55 ? world.xz : world.xy; "
           "float line = grid_line(structural_uv * max(object.pattern_params.yz, float2(0.001)) * "
           "0.55, max(object.pattern_params2.y, 0.005)); "
           "float broad = projected_fbm(world, normal, object.material_params.w * 0.11, 19.0); "
           "float fine = projected_fbm(world, normal, object.material_params.w * 0.84, 29.0); "
           "float3 block = base * (0.82 + broad * 0.22 + fine * 0.12); "
           "block = mix(block, block * float3(1.10, 1.04, 0.92), object.material_params.z * "
           "ridge1(fine) * 0.18); "
           "return mix(block, base * 0.36, line * 0.70); }\n"
           "float3 cave_rock(float3 base, constant Object &object, float3 world, float3 normal) { "
           "float strata = 0.5 + 0.5 * sin((world.y * object.pattern_params.z + world.z * 0.33 + "
           "world.x * 0.18) * 1.75); "
           "float broad = projected_fbm(world, normal, object.material_params.w * 0.10, 41.0); "
           "float fine = projected_fbm(world, normal, object.material_params.w * 0.88, 53.0); "
           "float crack = ridge1(projected_fbm(world, normal, object.material_params.w * 0.42, "
           "67.0)); "
           "float3 damp = base * float3(0.72, 0.70, 0.62); float3 mineral = base * float3(1.24, "
           "1.13, 0.92); "
           "float3 albedo = mix(damp, base, broad * 0.74); "
           "albedo = mix(albedo, mineral, saturate1(strata * 0.18 + ridge1(fine) * 0.12) * "
           "saturate1(object.pattern_params2.x)); "
           "albedo = mix(albedo, albedo * 0.50, smooth1(0.68, 0.96, crack) * (0.35 + "
           "object.pattern_params.w * 1.8)); "
           "return clamp(albedo * (0.82 + fine * 0.22), float3(0.0), float3(4.0)); }\n"
           "float3 coal_vein(float3 base, constant Object &object, float3 world, float3 normal) { "
           "float vein_a = ridge1(projected_fbm(world, normal, object.material_params.w * 0.32, "
           "71.0)); "
           "float vein_b = ridge1(projected_fbm(world + normal * 0.17, normal, "
           "object.material_params.w * 0.76, 89.0)); "
           "float vein = smooth1(0.72, 0.96, vein_a * vein_b + object.pattern_params.w * 0.52); "
           "float sheen = smooth1(0.50, 0.95, projected_fbm(world, normal, "
           "object.material_params.w * 1.55, 97.0)); "
           "float3 coal = base * (0.54 + sheen * 0.32); float3 warm = mix(float3(0.22, 0.15, "
           "0.075), object.emission_strength.rgb + base, 0.35); "
           "return mix(coal, warm, vein * 0.78); }\n"
           "float3 organic_fiber(float3 base, constant Object &object, float3 world, float3 "
           "normal, float2 uv, float salt) { "
           "float flow = world.y * object.pattern_params.z + (world.x + world.z * 0.38) * "
           "object.pattern_params.y * 0.18; "
           "float n = projected_fbm(world, normal, object.material_params.w * 0.70, salt); "
           "float strand = 0.5 + 0.5 * sin(flow * 0.46 + n * 5.3 + uv.x * 6.2831853); "
           "float mask = smooth1(0.28, 0.92, strand); return mix(base * float3(0.64, 0.58, 0.50), "
           "base * float3(1.22, 1.14, 0.96), mask * (0.56 + object.pattern_params2.x * 0.28)); }\n"
           "float3 cave_web(float3 base, constant Object &object, float3 world, float3 normal, "
           "float2 uv) { "
           "float along = uv.x * max(object.pattern_params.z, 0.001); "
           "float across = abs(uv.y - 0.5) * 2.0; "
           "float n = projected_fbm(world, normal, object.material_params.w * 0.88, 211.0); "
           "float fiber = ridge1(0.5 + 0.5 * sin(along * 0.72 + n * 4.6)); "
           "float core = 1.0 - smooth1(0.18, 1.0, across); "
           "float dust = projected_fbm(world, normal, object.material_params.w * 1.42, 227.0); "
           "float glint = smooth1(0.58, 0.98, fiber * core); "
           "float3 shadow = base * float3(0.62, 0.66, 0.66); "
           "float3 silk = base * float3(1.22, 1.24, 1.16); "
           "float3 pearl = base + float3(0.08, 0.09, 0.075); "
           "return clamp(mix(mix(shadow, silk, core * 0.72 + dust * 0.16), pearl, glint), "
           "float3(0.0), float3(4.0)); }\n"
           "float3 cave_skitter_eye(float3 base, constant Object &object, float3 world, float3 "
           "normal);\n"
           "float3 cave_skitter_chitin(float3 base, constant Object &object, float3 world, "
           "float3 normal, float2 uv) { if (uv.x > 0.90 && uv.y < 0.28) return "
           "cave_skitter_eye(base, object, world, normal); "
           "float tx = floor(uv.x * max(object.pattern_params.y, 1.0)); "
           "float ty = floor(uv.y * max(object.pattern_params.z, 1.0)); "
           "float texel = fract(sin(tx * 12.9898 + ty * 78.233) * 43758.5453); "
           "float ridge_a = ridge1(projected_fbm(world, normal, object.material_params.w * 1.35, "
           "241.0)); "
           "float band = 0.5 + 0.5 * sin((ty * 0.92 + floor(tx * 0.25)) * 1.73); "
           "float shell = smooth1(0.20, 0.92, ridge_a * 0.56 + band * 0.30 + texel * 0.14); "
           "float center_spot = (1.0 - smooth1(0.04, 0.18, abs(uv.x - 0.50))) * "
           "smooth1(0.10, 0.54, uv.y) * (1.0 - smooth1(0.78, 0.98, uv.y)); "
           "float leg_band = smooth1(0.62, 0.96, ridge1(0.5 + 0.5 * sin(ty * 2.45))); "
           "float oil = projected_fbm(world + normal * 0.05, normal, object.material_params.w * "
           "0.78, 263.0); "
           "float3 under = base * float3(0.44, 0.34, 0.30); float3 lacquer = base * "
           "float3(1.54, 1.04, 0.78); float3 warm_mark = base + float3(0.070, 0.020, 0.010); "
           "float3 dark_band = base * float3(0.22, 0.18, 0.18); "
           "float3 sheen = base + float3(0.042, 0.050, 0.060); "
           "float3 albedo = mix(under, lacquer, shell); "
           "albedo = mix(albedo, warm_mark, center_spot * 0.58); "
           "albedo = mix(albedo, dark_band, leg_band * (0.18 + texel * 0.12)); "
           "return clamp(mix(albedo, sheen, oil * 0.26), float3(0.0), float3(4.0)); }\n"
           "float3 cave_skitter_eye(float3 base, constant Object &object, float3 world, float3 "
           "normal) { "
           "float wet = smooth1(0.45, 0.96, projected_fbm(world, normal, object.material_params.w "
           "* 1.9, 283.0)); "
           "return clamp(base * (0.62 + wet * 0.44) + object.emission_strength.rgb * (0.28 + wet "
           "* 0.34), float3(0.0), float3(4.0)); }\n"
           "float3 weathered_metal(float3 base, constant Object &object, float3 world, float3 "
           "normal) { "
           "float detail = max(object.material_params.w, 0.001); "
           "float broad = projected_fbm(world, normal, detail * 0.16, 307.0); "
           "float fine = projected_fbm(world + normal * 0.05, normal, detail * 0.74, 331.0); "
           "float pitting = ridge1(projected_fbm(world, normal, detail * 1.38, 359.0)); "
           "float upward = smooth1(0.14, 0.80, normal.y); "
           "float oxide = smooth1(0.42, 0.95, broad * 0.48 + pitting * 0.34 + upward * "
           "object.pattern_params.w * 0.42); "
           "float3 cool = base * float3(0.82, 0.88, 0.92) * (0.60 + fine * 0.22); "
           "float3 exposed = base * float3(1.18, 1.16, 1.06) * (0.66 + pitting * 0.16); "
           "float3 rust = mix(float3(0.085, 0.060, 0.043), float3(0.42, 0.15, 0.045), fine); "
           "float3 color = mix(cool, rust, oxide); "
           "return clamp(mix(color, exposed, object.material_params.z * smooth1(0.72, 0.98, "
           "pitting)), float3(0.0), float3(4.0)); }\n"
           "float3 weld_bead(float3 base, constant Object &object, float3 world, float3 normal, "
           "float2 uv) { "
           "float ripple = 0.5 + 0.5 * sin(uv.y * max(object.pattern_params.z, 0.001) * "
           "6.2831853 + projected_fbm(world, normal, object.material_params.w, 401.0) * 3.8); "
           "float heat = smooth1(0.05, 0.58, ridge1(uv.x)); "
           "float3 color = base * (0.76 + ripple * 0.28); "
           "color = mix(color, float3(0.72, 0.38, 0.12), heat * 0.24); "
           "return clamp(mix(color, float3(0.11, 0.16, 0.30), heat * (1.0 - ripple) * 0.16), "
           "float3(0.0), float3(4.0)); }\n"
           "float3 foliage(float3 base, constant Object &object, float3 world, float3 normal, "
           "float2 uv) { "
           "float blade_height = saturate1(uv.y); float root_weight = 1.0 - smooth1(0.04, "
           "0.36, blade_height); "
           "float tip_weight = smooth1(0.48, 1.0, blade_height); float vein = 1.0 - "
           "smooth1(0.025, 0.22, abs(uv.x - 0.5)); "
           "float strand = 0.5 + 0.5 * sin((uv.y * object.pattern_params.z + uv.x * 2.0) * "
           "6.2831853); "
           "float mottling = projected_fbm(world, normal, object.material_params.w * 0.64, 109.0); "
           "float fiber = ridge1(projected_fbm(world, normal, object.material_params.w * 1.22, "
           "113.0)); "
           "float3 root = base * float3(0.50, 0.66, 0.38); float3 mid = base * float3(0.82, "
           "1.08, 0.58); float3 tip = base * float3(1.18, 1.30, 0.72); "
           "float3 blade = mix(root, mid, smooth1(0.02, 0.72, blade_height)); "
           "blade = mix(blade, tip, tip_weight * (0.42 + mottling * 0.28)); "
           "blade *= 0.88 + mottling * 0.18 + fiber * 0.08 + strand * 0.05; "
           "blade = mix(blade, blade * float3(1.26, 1.34, 0.88), vein * 0.18); "
           "return clamp(mix(blade, blade * 0.72, root_weight * 0.34), float3(0.0), "
           "float3(4.0)); }\n"
           "float3 terrain_layered(float3 base, constant Object &object, float3 world, float3 "
           "normal) { "
           "float pattern = object.pattern_params.x; float detail = max(object.material_params.w, "
           "0.001); "
           "float macro_scale = 0.018 + sqrt(detail) * 0.011; float mid_scale = 0.055 + detail * "
           "0.018; float fine_scale = 0.38 + detail * 0.055; "
           "float macro = projected_fbm(world, normal, macro_scale, pattern * 3.7); "
           "float mid = projected_fbm(world, normal, mid_scale, pattern * 5.1 + 9.0); "
           "float fine = projected_fbm(world, normal, fine_scale, pattern * 7.9 + 2.0); "
           "float ridge = ridge1(projected_fbm(world, normal, mid_scale * 1.85, pattern * 2.4 + "
           "31.0)); "
           "float up = saturate1(normalize(normal).y); float slope = 1.0 - smooth1(0.54, 0.92, "
           "up); "
           "float altitude = smooth1(-0.20, 2.80, world.y + (macro - 0.5) * 0.65); "
           "float soil_weight = saturate1(slope * 0.46 + (0.54 - mid) * 0.46 + ridge * 0.10); "
           "float rock_weight = saturate1(smooth1(0.70, 1.08, slope + ridge * 0.14 + altitude * "
           "0.04)) * 0.52; "
           "float grass_weight = saturate1(0.70 + (macro - 0.45) * 0.28 + (fine - 0.50) * 0.12 - "
           "soil_weight * 0.36 - rock_weight * 0.58 + object.pattern_params2.x * 1.10); "
           "float3 grass = mix(base * float3(0.78, 1.04, 0.58), float3(0.18, 0.28, 0.12), 0.24); "
           "float3 dry_grass = mix(base * float3(0.98, 0.90, 0.62), float3(0.34, 0.30, 0.18), "
           "0.26); "
           "float3 soil = mix(base * float3(1.02, 0.76, 0.52), float3(0.30, 0.23, 0.16), 0.30); "
           "float3 rock = mix(base * float3(0.94, 0.94, 0.86), float3(0.44, 0.42, 0.36), 0.32); "
           "float3 albedo = mix(soil, dry_grass, saturate1(grass_weight * 0.30 + macro * 0.18)); "
           "albedo = mix(albedo, grass, grass_weight); albedo = mix(albedo, rock, rock_weight); "
           "float fiber = projected_noise(world + float3(fine * 1.7, 0.0, macro), normal, "
           "fine_scale * 2.35, pattern + 43.0); "
           "float pebble = projected_noise(world, normal, fine_scale * 3.8, pattern + 67.0); "
           "albedo *= 0.92 + fine * 0.10 + fiber * grass_weight * 0.05; "
           "albedo = mix(albedo, albedo * 0.82, saturate1(pebble * soil_weight * 0.08 + slope * "
           "0.05)); "
           "return clamp(albedo, float3(0.0), float3(4.0)); }\n"
           "float3 procedural_normal(float3 normal, float3 world, constant Object &object) { float "
           "detail = max(object.material_params.w, 0.001); "
           "float inferred = object.material_params.z * (object.material_flags.y > 0.5 ? 0.44 : "
           "0.28); "
           "float strength = saturate1(max(inferred, object.procedural_params.y)); "
           "float macro_gain = 1.0 + max(object.procedural_params.x, 0.0) * 0.34; "
           "float3 p = world * (0.55 * detail * macro_gain) + float3(object.pattern_params.x * "
           "0.23, 0.0, object.pattern_params.x * 0.41); "
           "float3 bump = float3(noise3(p + float3(11.1, 2.3, 0.7)) - 0.5, (noise3(p + float3(5.7, "
           "13.1, 3.2)) - 0.5) * 0.35, noise3(p + float3(2.9, 7.4, 17.6)) - 0.5); "
           "return normalize(normal + bump * strength); }\n"
           "float3 apply_procedural_layer(float3 color, constant Object &object, float3 world, "
           "float3 normal) { "
           "float macro_strength = max(object.procedural_params.x, 0.0); float wetness = "
           "saturate1(object.procedural_params.w); float height_shading = "
           "max(object.procedural_params2.x, 0.0); "
           "if (macro_strength <= 0.0001 && wetness <= 0.0001 && height_shading <= 0.0001) return "
           "color; "
           "float detail = max(object.material_params.w, 0.001); float broad = noise3(world * "
           "(0.10 + detail * 0.014) + float3(23.7, 0.0, -11.3)); "
           "float fine = noise3(world * (0.48 + detail * 0.036) + normal * 0.31); float lift = "
           "smooth1(0.18, 0.86, normal.y); "
           "float variation = 1.0 + (broad - 0.5) * macro_strength * 0.28 + (fine - 0.5) * "
           "macro_strength * 0.12 + lift * height_shading * 0.045; "
           "float3 damp = mix(color * 0.70, color * float3(0.80, 0.86, 0.92), lift * 0.45); "
           "return clamp(mix(color * variation, damp, wetness * (0.18 + fine * 0.18)), "
           "float3(0.0), float3(4.0)); }\n"
           "float3 material_albedo(constant Object &object, float3 world, float3 normal, float2 "
           "uv, float time) { "
           "float pattern = object.pattern_params.x; float detail = max(object.material_params.w, "
           "0.001); "
           "float macro = noise3(world * (0.13 * detail) + float3(pattern * 0.7, 0.0, pattern * "
           "0.31)); "
           "float fine = noise3(world * (0.72 * detail) + normal * 0.23 + float3(0.0, pattern * "
           "0.19, time * 0.01)); "
           "float3 base = object.base_color_opacity.rgb; "
           "if (object.material_flags.y > 0.5) { if (is_pattern(pattern, 5.0) > 0.5) { "
           "float slope = 1.0 - smooth1(0.50, 0.92, normalize(normal).y); "
           "float path_grain = smooth1(0.20, 0.72, noise3(float3(world.x * 1.2, world.y * 0.2, "
           "world.z * 2.1) + pattern)); "
           "float3 soil = base * float3(1.16, 0.88, 0.62); float3 compacted = base * "
           "float3(0.82, 0.70, 0.52); float3 stone = base * float3(0.90, 0.88, 0.78); "
           "return apply_procedural_layer(mix(mix(compacted, soil, macro * 0.42 + path_grain * "
           "0.24), stone, saturate1(slope * 0.24 + fine * 0.08)), object, world, normal); } "
           "return apply_procedural_layer(terrain_layered(base, object, world, normal), object, "
           "world, normal); } "
           "if (object.material_flags.z > 0.5) { float wave = 0.5 + 0.5 * sin((uv.x * "
           "object.pattern_params.y + uv.y * object.pattern_params.z) * 7.5 + time * 1.7); "
           "return apply_procedural_layer(mix(base, float3(0.07, 0.58, 0.70), 0.28 + 0.18 * wave) "
           "+ float3(0.02, 0.04, 0.05) * fine, object, world, normal); } "
           "if (is_pattern(pattern, 1.0) > 0.5 || is_pattern(pattern, 9.0) > 0.5) return "
           "apply_procedural_layer(structured_stone(base, object, world, normal), object, world, "
           "normal); "
           "if (is_pattern(pattern, 17.0) > 0.5) return apply_procedural_layer(cave_rock(base, "
           "object, world, normal), object, world, normal); "
           "if (is_pattern(pattern, 18.0) > 0.5) return apply_procedural_layer(coal_vein(base, "
           "object, world, normal), object, world, normal); "
           "if (is_pattern(pattern, 2.0) > 0.5 || is_pattern(pattern, 7.0) > 0.5) return "
           "organic_fiber(base, object, world, normal, uv, pattern + 151.0); "
           "if (is_pattern(pattern, 20.0) > 0.5) return apply_procedural_layer(cave_web(base, "
           "object, world, normal, uv), object, world, normal); "
           "if (is_pattern(pattern, 21.0) > 0.5) return apply_procedural_layer("
           "cave_skitter_chitin(base, object, world, normal, uv), object, world, normal); "
           "if (is_pattern(pattern, 22.0) > 0.5) return cave_skitter_eye(base, object, world, "
           "normal); "
           "if (is_pattern(pattern, 23.0) > 0.5) return apply_procedural_layer("
           "weathered_metal(base, object, world, normal), object, world, normal); "
           "if (is_pattern(pattern, 24.0) > 0.5) return apply_procedural_layer("
           "weld_bead(base, object, world, normal, uv), object, world, normal); "
           "if (is_pattern(pattern, 11.0) > 0.5) return apply_procedural_layer(foliage(base, "
           "object, world, normal, uv), object, world, normal); "
           "if (is_pattern(pattern, 13.0) > 0.5) { float streak = ridge1(projected_fbm(world, "
           "normal, detail * 0.44, 127.0)); float cloud = projected_fbm(world, normal, detail * "
           "1.10, 131.0); "
           "return mix(base * float3(0.62, 0.42, 0.28), base * float3(1.30, 0.96, 0.56), "
           "smooth1(0.25, 0.92, cloud)) + object.emission_strength.rgb * (0.08 + smooth1(0.70, "
           "0.98, streak) * 0.16); } "
           "if (is_pattern(pattern, 10.0) > 0.5) { float grain = ridge1(projected_fbm(world, "
           "normal, detail * 0.52, 167.0)); float rings = 0.5 + 0.5 * sin((world.y * "
           "object.pattern_params.z + world.x * 0.28 + world.z * 0.19) * 2.2 + grain * 3.4); "
           "return mix(base * float3(0.62, 0.48, 0.34), base * float3(1.18, 0.96, 0.68), "
           "smooth1(0.18, 0.92, rings)); } "
           "if (is_pattern(pattern, 14.0) > 0.5) { float central = 1.0 - smooth1(0.025, 0.18, "
           "abs(uv.x - 0.5)); float barb = 0.5 + 0.5 * sin((uv.y * object.pattern_params.z + uv.x "
           "* 3.0) * 6.2831853); "
           "float3 feather = mix(base * 0.72, base * 1.22, smooth1(0.26, 0.88, barb)); return "
           "mix(feather, feather * 1.35, central * 0.34); } "
           "if (is_pattern(pattern, 12.0) > 0.5 || is_pattern(pattern, 16.0) > 0.5) { float2 cell "
           "= fract(uv * max(object.pattern_params.yz, float2(0.001))); float shell = "
           "smooth1(0.18, 0.50, 1.0 - length(cell - float2(0.5))); "
           "float hue = projected_fbm(world, normal, detail * 1.10, 181.0); float3 scale_color = "
           "mix(base * float3(0.66, 0.82, 0.76), base * float3(1.22, 1.02, 0.68), hue); return "
           "mix(base * 0.62, scale_color, shell); } "
           "if (is_pattern(pattern, 15.0) > 0.5) return organic_fiber(base, object, world, normal, "
           "uv, 193.0) * float3(0.90, 0.78, 0.58); "
           "return apply_procedural_layer(base * (0.86 + (macro * 0.18 + fine * 0.08) * "
           "saturate1(object.material_params.z + object.pattern_params2.x * 0.35 + "
           "object.procedural_params.x * 0.24)), object, world, normal); }\n"
           "vertex VSOut vs_main(uint id [[vertex_id]], uint instance_id [[instance_id]], "
           "device const Vertex *vertices [[buffer(0)]], constant Scene &scene [[buffer(1)]], "
           "constant Object *objects [[buffer(2)]]) {\n"
           "  constant Object &object = objects[instance_id]; Vertex v = vertices[id]; "
           "float4 local = float4(v.position.xyz, 1.0); VSOut out; "
           "out.position = object.mvp * local; "
           "  out.position.z = out.position.z * 0.5 + out.position.w * 0.5; out.world = "
           "(object.model * local).xyz; "
           "  out.normal = normalize((object.model * float4(v.normal.xyz, 0.0)).xyz); out.uv = "
           "v.uv_ao.xy; out.ao = v.uv_ao.z; out.object_index = instance_id; return out;\n"
           "}\n"
           "fragment float4 fs_main(VSOut in [[stage_in]], constant Scene &scene [[buffer(1)]], "
           "constant Object *objects [[buffer(2)]]) { constant Object &object = "
           "objects[in.object_index]; "
           "float opacity = object.base_color_opacity.w; if (object.material_flags.x > 0.5) { "
           "float2 centered = in.uv * 2.0 - 1.0; float soft = 1.0 - smooth1(0.12, 1.0, "
           "dot(centered, centered)); "
           "return float4(0.018, 0.015, 0.012, opacity * soft * soft * 0.70); } "
           "float3 normal = normalize(in.normal); if (length(normal) < 0.001) normal = float3(0.0, "
           "1.0, 0.0); "
           "if (scene.exposure_ambient.w > 0.5 && object.material_flags.x < 0.5 && "
           "object.material_flags.z < 0.5) normal = procedural_normal(normal, in.world, object); "
           "float3 albedo = material_albedo(object, in.world, normal, in.uv, scene.camera_time.w); "
           "float sky = saturate1(normal.y * 0.5 + 0.5); float ao = clamp(object.pattern_params2.z "
           "* in.ao, 0.0, 1.0); "
           "float ambient = max(scene.exposure_ambient.y * ao, scene.exposure_ambient.z); "
           "float3 color = mix(scene.ground_ambient.rgb, scene.sky_ambient.rgb, sky) * albedo * "
           "ambient + albedo * 0.035; "
           "float3 view = normalize(scene.camera_time.xyz - in.world); "
           "float rim = pow(1.0 - saturate1(dot(normal, view)), 2.0); color += albedo * "
           "scene.sky_ambient.rgb * rim * 0.10; "
           "float layer_roughness = clamp(object.material_params.x + (noise3(in.world * 0.37 + "
           "float3(3.1, 7.2, 1.9)) - 0.5) * object.procedural_params.z * 0.18, 0.0, 1.0); "
           "auto add_light = [&](float3 light_dir, float3 radiance) { float ndotl = "
           "max(dot(normal, light_dir), 0.0); float wrapped = ndotl * 0.86 + 0.14; float3 h = "
           "normalize(light_dir + view); "
           "float spec_power = mix(64.0, 8.0, layer_roughness); float spec = pow(max(dot(normal, "
           "h), 0.0), spec_power) * (1.0 - layer_roughness) * 0.34; "
           "color += (albedo * 0.78 * wrapped + spec) * radiance; }; "
           "if (scene.sun_direction_enabled.w > 0.5) "
           "add_light(normalize(scene.sun_direction_enabled.xyz), scene.sun_color_intensity.rgb * "
           "scene.sun_color_intensity.w); "
           "for (uint i = 0; i < 4; ++i) { float3 lv = scene.lights[i].position_radius.xyz - "
           "in.world; float d2 = max(dot(lv, lv), 0.0001); "
           "float soft = max(d2, scene.lights[i].position_radius.w * "
           "scene.lights[i].position_radius.w + 0.0001); add_light(normalize(lv), "
           "scene.lights[i].color_intensity.rgb * (scene.lights[i].color_intensity.w / soft)); } "
           "float fog = smooth1(0.0, 1.0, (distance(scene.camera_time.xyz, in.world) - "
           "scene.fog_params.x) / max(scene.fog_params.y - scene.fog_params.x, 0.001)) * "
           "scene.fog_color_strength.w; "
           "color = mix(color, scene.fog_color_strength.rgb, clamp(fog, 0.0, 1.0)); float luma = "
           "dot(color, float3(0.2126, 0.7152, 0.0722)); "
           "color = mix(float3(luma), color, clamp(scene.fog_params.z, 0.0, 2.0)); color = (color "
           "- 0.5) * max(scene.fog_params.w, 0.0) + 0.5; "
           "float floor_lift = 0.030 + 0.018 * max(object.material_flags.y, "
           "object.material_flags.w); color = max(color, albedo * floor_lift); "
           "color += object.emission_strength.rgb * object.emission_strength.w; color = "
           "tonemap(color * scene.exposure_ambient.x); return float4(gamma_encode(color), "
           "opacity); }\n";

      NSError *error = nil;
      library_ = [device_ newLibraryWithSource:source options:nil error:&error];
      if (library_ == nil) {
        return false;
      }

      id<MTLFunction> vertex = [library_ newFunctionWithName:@"vs_main"];
      id<MTLFunction> fragment = [library_ newFunctionWithName:@"fs_main"];

      MTLRenderPipelineDescriptor *opaque_desc = [[MTLRenderPipelineDescriptor alloc] init];
      opaque_desc.vertexFunction = vertex;
      opaque_desc.fragmentFunction = fragment;
      opaque_desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
      opaque_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
      opaque_pipeline_ = [device_ newRenderPipelineStateWithDescriptor:opaque_desc error:&error];

      MTLRenderPipelineDescriptor *transparent_desc = [opaque_desc copy];
      transparent_desc.colorAttachments[0].blendingEnabled = YES;
      transparent_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
      transparent_desc.colorAttachments[0].destinationRGBBlendFactor =
          MTLBlendFactorOneMinusSourceAlpha;
      transparent_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
      transparent_desc.colorAttachments[0].destinationAlphaBlendFactor =
          MTLBlendFactorOneMinusSourceAlpha;
      transparent_pipeline_ = [device_ newRenderPipelineStateWithDescriptor:transparent_desc
                                                                      error:&error];

      MTLDepthStencilDescriptor *opaque_depth_desc = [[MTLDepthStencilDescriptor alloc] init];
      opaque_depth_desc.depthCompareFunction = MTLCompareFunctionLess;
      opaque_depth_desc.depthWriteEnabled = YES;
      opaque_depth_ = [device_ newDepthStencilStateWithDescriptor:opaque_depth_desc];

      MTLDepthStencilDescriptor *transparent_depth_desc = [[MTLDepthStencilDescriptor alloc] init];
      transparent_depth_desc.depthCompareFunction = MTLCompareFunctionLessEqual;
      transparent_depth_desc.depthWriteEnabled = NO;
      transparent_depth_ = [device_ newDepthStencilStateWithDescriptor:transparent_depth_desc];

      MTLDepthStencilDescriptor *read_only_depth_desc = [[MTLDepthStencilDescriptor alloc] init];
      read_only_depth_desc.depthCompareFunction = MTLCompareFunctionLessEqual;
      read_only_depth_desc.depthWriteEnabled = NO;
      read_only_depth_ = [device_ newDepthStencilStateWithDescriptor:read_only_depth_desc];

      MTLDepthStencilDescriptor *disabled_depth_desc = [[MTLDepthStencilDescriptor alloc] init];
      disabled_depth_desc.depthCompareFunction = MTLCompareFunctionAlways;
      disabled_depth_desc.depthWriteEnabled = NO;
      disabled_depth_ = [device_ newDepthStencilStateWithDescriptor:disabled_depth_desc];

      [opaque_depth_desc release];
      [transparent_depth_desc release];
      [read_only_depth_desc release];
      [disabled_depth_desc release];
      [opaque_desc release];
      [transparent_desc release];
      [vertex release];
      [fragment release];
      return opaque_pipeline_ != nil && transparent_pipeline_ != nil && opaque_depth_ != nil &&
             transparent_depth_ != nil && read_only_depth_ != nil && disabled_depth_ != nil;
    }
  }

  aster::FrameStats render(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                           const aster::OrbitCamera &camera,
                           const aster::RendererSettings &settings,
                           const aster::FixedRenderGraph &graph,
                           const aster::PreparedRenderMeshes &meshes, const int framebuffer_width,
                           const int framebuffer_height, const double frame_seconds,
                           aster::FrameForensics *forensics) override {
    ASTER_PROFILE_SCOPE("MetalNativeRenderBackend::render");
    aster::FrameStats stats;
    stats.frame_seconds = frame_seconds;
    stats.framebuffer_width = framebuffer_width;
    stats.framebuffer_height = framebuffer_height;
    stats.graph_passes = graph.passes.size();
    stats.graph_resources = graph.resources.size();
    stats.graph_barriers = graph.barriers.size();
    stats.graph_transient_resources = graph.transient_resource_count;
    stats.backend_feature_mask = capabilities().graph_resource_mask;
    stats.backend_kind_value = static_cast<std::uint32_t>(capabilities().kind);
    if (device_ == nil || queue_ == nil || framebuffer_width <= 0 || framebuffer_height <= 0) {
      return stats;
    }

    ensureTargets(framebuffer_width, framebuffer_height);
    if (scene_texture_ == nil || depth_texture_ == nil) {
      return stats;
    }
    evictRetiredMeshBuffers(meshes);
    metalFrameState().scene_width = framebuffer_width;
    metalFrameState().scene_height = framebuffer_height;

    const auto encode_start = std::chrono::steady_clock::now();
    id<MTLCommandBuffer> command_buffer = [queue_ commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = scene_texture_;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(settings.pipeline.clear_color.x, settings.pipeline.clear_color.y,
                          settings.pipeline.clear_color.z, 1.0);
    pass.depthAttachment.texture = depth_texture_;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = 1.0;

    id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
    const MetalSceneUniforms scene_uniforms = makeSceneUniforms(camera, settings, frame_seconds);
    [encoder setVertexBytes:&scene_uniforms length:sizeof(scene_uniforms) atIndex:1];
    [encoder setFragmentBytes:&scene_uniforms length:sizeof(scene_uniforms) atIndex:1];
    const std::size_t uniform_capacity =
        std::max<std::size_t>(plan.instances.size() + scene.objects().size(), 1u);
    if (!ensureObjectUniformCapacity(uniform_capacity)) {
      [encoder endEncoding];
      [command_buffer commit];
      publishSceneTexture(scene_texture_, command_buffer, framebuffer_width, framebuffer_height);
      return stats;
    }
    object_uniform_cursor_ = 0;
    const aster::Vec3 camera_position = camera.position();

    const auto write_object_uniform = [&](const aster::RenderObject &object, const float opacity) {
      if (object_uniform_cursor_ >= object_uniform_capacity_) {
        return std::optional<std::size_t>{};
      }
      const std::size_t uniform_index = object_uniform_cursor_++;
      auto *uniform_bytes = static_cast<std::uint8_t *>([object_uniform_buffer_ contents]);
      const MetalObjectUniforms object_uniforms =
          makeObjectUniforms(object, camera, settings, frame_seconds, opacity);
      std::memcpy(uniform_bytes + uniform_index * kObjectUniformStride, &object_uniforms,
                  sizeof(object_uniforms));
      return std::optional<std::size_t>{uniform_index};
    };

    const auto encode_object = [&](const aster::RenderObject &object, const float opacity) {
      const aster::CpuMesh *mesh = meshForObject(object, meshes);
      if (mesh == nullptr || mesh->indices.empty()) {
        return;
      }
      MetalMeshBuffers *buffers = buffersForMesh(*mesh);
      if (buffers == nullptr || buffers->vertices == nil || buffers->indices == nil ||
          buffers->index_count == 0) {
        return;
      }
      const bool transparent =
          opacity < 0.999f || aster::classifyMaterialRenderQueue(object.material) ==
                                  aster::MaterialRenderQueue::Translucent;
      const bool writes_depth = !transparent && aster::materialWritesDepth(object.material);
      const std::optional<std::size_t> uniform_index = write_object_uniform(object, opacity);
      if (!uniform_index.has_value()) {
        return;
      }
      const NSUInteger object_uniform_offset =
          static_cast<NSUInteger>(*uniform_index * kObjectUniformStride);
      [encoder setRenderPipelineState:transparent ? transparent_pipeline_ : opaque_pipeline_];
      [encoder setDepthStencilState:!settings.pipeline.depth_test
                                        ? disabled_depth_
                                        : (transparent ? transparent_depth_
                                                       : (writes_depth ? opaque_depth_
                                                                       : read_only_depth_))];
      [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
      [encoder setCullMode:metalCullMode(objectCullMode(
                               object, camera_position, settings.pipeline.back_face_culling))];
      [encoder setVertexBuffer:buffers->vertices offset:0 atIndex:0];
      [encoder setVertexBuffer:object_uniform_buffer_ offset:object_uniform_offset atIndex:2];
      [encoder setFragmentBuffer:object_uniform_buffer_ offset:object_uniform_offset atIndex:2];
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:buffers->index_count
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:buffers->indices
                   indexBufferOffset:0];
      ++stats.draw_calls;
    };

    const auto encode_group = [&](const aster::FrameRenderDrawGroup &group) {
      if (group.instance_count == 0 || group.first_instance >= plan.instances.size()) {
        return;
      }
      const aster::FrameRenderInstance &first_instance = plan.instances[group.first_instance];
      if (first_instance.object_index >= scene.objects().size()) {
        return;
      }
      const aster::RenderObject &first_object = scene.objects()[first_instance.object_index];
      const aster::CpuMesh *mesh = meshForObject(first_object, meshes);
      if (mesh == nullptr || mesh->indices.empty()) {
        return;
      }
      const aster::FaceCullMode first_cull_mode =
          objectCullMode(first_object, camera_position, settings.pipeline.back_face_culling);
      bool shared_cull_mode = true;
      for (std::size_t i = 1; i < group.instance_count; ++i) {
        const std::size_t plan_index = group.first_instance + i;
        if (plan_index >= plan.instances.size()) {
          break;
        }
        const aster::FrameRenderInstance &instance = plan.instances[plan_index];
        if (instance.object_index >= scene.objects().size()) {
          break;
        }
        if (objectCullMode(scene.objects()[instance.object_index], camera_position,
                           settings.pipeline.back_face_culling) != first_cull_mode) {
          shared_cull_mode = false;
          break;
        }
      }
      if (!shared_cull_mode) {
        for (std::size_t i = 0; i < group.instance_count; ++i) {
          const std::size_t plan_index = group.first_instance + i;
          if (plan_index >= plan.instances.size()) {
            break;
          }
          const aster::FrameRenderInstance &instance = plan.instances[plan_index];
          if (instance.object_index >= scene.objects().size()) {
            break;
          }
          encode_object(scene.objects()[instance.object_index], instance.opacity);
        }
        return;
      }
      MetalMeshBuffers *buffers = buffersForMesh(*mesh);
      if (buffers == nullptr || buffers->vertices == nil || buffers->indices == nil ||
          buffers->index_count == 0) {
        return;
      }
      const std::size_t first_uniform = object_uniform_cursor_;
      std::size_t written = 0;
      for (std::size_t i = 0; i < group.instance_count; ++i) {
        const std::size_t plan_index = group.first_instance + i;
        if (plan_index >= plan.instances.size()) {
          break;
        }
        const aster::FrameRenderInstance &instance = plan.instances[plan_index];
        if (instance.object_index >= scene.objects().size()) {
          break;
        }
        if (!write_object_uniform(scene.objects()[instance.object_index], instance.opacity)
                 .has_value()) {
          break;
        }
        ++written;
      }
      if (written == 0) {
        return;
      }
      const bool transparent = group.pass == aster::FrameRenderPass::Transparent;
      const bool writes_depth = !transparent && aster::materialWritesDepth(first_object.material);
      const NSUInteger object_uniform_offset =
          static_cast<NSUInteger>(first_uniform * kObjectUniformStride);
      [encoder setRenderPipelineState:transparent ? transparent_pipeline_ : opaque_pipeline_];
      [encoder setDepthStencilState:!settings.pipeline.depth_test
                                        ? disabled_depth_
                                        : (transparent ? transparent_depth_
                                                       : (writes_depth ? opaque_depth_
                                                                       : read_only_depth_))];
      [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
      [encoder setCullMode:metalCullMode(first_cull_mode)];
      [encoder setVertexBuffer:buffers->vertices offset:0 atIndex:0];
      [encoder setVertexBuffer:object_uniform_buffer_ offset:object_uniform_offset atIndex:2];
      [encoder setFragmentBuffer:object_uniform_buffer_ offset:object_uniform_offset atIndex:2];
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:buffers->index_count
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:buffers->indices
                   indexBufferOffset:0
                       instanceCount:static_cast<NSUInteger>(written)];
      ++stats.draw_calls;
    };

    const auto encode_contact_shadows = [&] {
      for (const aster::FrameRenderInstance &instance : plan.instances) {
        if (instance.object_index >= scene.objects().size()) {
          continue;
        }
        const aster::RenderObject &object = scene.objects()[instance.object_index];
        if (canCastContactShadow(object, settings.grounding)) {
          const aster::RenderObject shadow = contactShadowObjectFor(object, settings.grounding);
          encode_object(shadow, shadow.material.opacity);
        }
      }
    };

    (void)aster::executeFixedRenderGraph(
        graph, [&](const aster::RenderGraphPassInvocation &invocation) {
          const auto pass_start = std::chrono::steady_clock::now();
          const std::size_t draws_before = stats.draw_calls;
          switch (invocation.semantic) {
          case aster::RenderGraphPass::Opaque:
            for (const aster::FrameRenderDrawGroup &group : plan.groups) {
              if (group.pass == aster::FrameRenderPass::Opaque) {
                encode_group(group);
              }
            }
            break;
          case aster::RenderGraphPass::ContactShadow:
            encode_contact_shadows();
            break;
          case aster::RenderGraphPass::Transparent:
            for (const aster::FrameRenderDrawGroup &group : plan.groups) {
              if (group.pass == aster::FrameRenderPass::Transparent) {
                encode_group(group);
              }
            }
            break;
          case aster::RenderGraphPass::SceneColorDepth:
          case aster::RenderGraphPass::UiComposite:
          case aster::RenderGraphPass::Capture:
            break;
          }
          const auto pass_end = std::chrono::steady_clock::now();
          if (forensics != nullptr) {
            forensics->passes.push_back(
                {.pass = invocation.semantic,
                 .name = invocation.pass == nullptr
                             ? std::string(aster::renderGraphPassName(invocation.semantic))
                             : invocation.pass->name,
                 .draw_calls = stats.draw_calls - draws_before,
                 .encode_seconds =
                     std::chrono::duration<double>(pass_end - pass_start).count()});
          }
        });
    [encoder endEncoding];
    const auto encode_end = std::chrono::steady_clock::now();
    stats.render_encode_seconds =
        std::chrono::duration<double>(encode_end - encode_start).count();
    [command_buffer commit];

    publishSceneTexture(scene_texture_, command_buffer, framebuffer_width, framebuffer_height);
    return stats;
  }

  const char *backendName() const override {
    return "Aster Native Metal Rasterizer";
  }

  aster::RenderBackendCapabilities capabilities() const override {
    const std::uint32_t graph_resources =
        aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor) |
        aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth) |
        aster::renderGraphResourceBit(aster::RenderGraphResource::UiOverlay) |
        aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
    return {.kind = aster::RenderBackendKind::Metal,
            .name = "Aster Native Metal Rasterizer",
            .gpu = true,
            .supports_shader_materials = true,
            .supports_texture_sampling = false,
            .supports_instancing = true,
            .supports_capture = true,
            .supports_ui_composite = true,
            .supports_gpu_timestamps = false,
            .graph_resource_mask = graph_resources,
            .capability_table = metalCapabilityTable()};
  }

private:
  void ensureTargets(const int width, const int height) {
    if (scene_texture_ != nil && width == width_ && height == height_) {
      return;
    }
    [scene_texture_ release];
    [depth_texture_ release];
    width_ = width;
    height_ = height;

    MTLTextureDescriptor *color_desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    color_desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    color_desc.storageMode = MTLStorageModeShared;
    scene_texture_ = [device_ newTextureWithDescriptor:color_desc];

    MTLTextureDescriptor *depth_desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    depth_desc.usage = MTLTextureUsageRenderTarget;
    depth_desc.storageMode = MTLStorageModePrivate;
    depth_texture_ = [device_ newTextureWithDescriptor:depth_desc];
  }

  MetalSceneUniforms makeSceneUniforms(const aster::OrbitCamera &camera,
                                       const aster::RendererSettings &settings,
                                       const double frame_seconds) const {
    const aster::Vec3 camera_position = camera.position();
    MetalSceneUniforms out;
    out.camera_time[0] = camera_position.x;
    out.camera_time[1] = camera_position.y;
    out.camera_time[2] = camera_position.z;
    out.camera_time[3] = static_cast<float>(frame_seconds);
    const aster::Vec3 sun = aster::normalize(settings.sun_light.direction_to_light);
    out.sun_direction_enabled[0] = sun.x;
    out.sun_direction_enabled[1] = sun.y;
    out.sun_direction_enabled[2] = sun.z;
    out.sun_direction_enabled[3] = settings.sun_light.enabled ? 1.0f : 0.0f;
    out.sun_color_intensity[0] = settings.sun_light.color.x;
    out.sun_color_intensity[1] = settings.sun_light.color.y;
    out.sun_color_intensity[2] = settings.sun_light.color.z;
    out.sun_color_intensity[3] = settings.sun_light.intensity;
    out.sky_ambient[0] = settings.sky_ambient_color.x;
    out.sky_ambient[1] = settings.sky_ambient_color.y;
    out.sky_ambient[2] = settings.sky_ambient_color.z;
    out.ground_ambient[0] = settings.ground_ambient_color.x;
    out.ground_ambient[1] = settings.ground_ambient_color.y;
    out.ground_ambient[2] = settings.ground_ambient_color.z;
    out.exposure_ambient[0] = settings.exposure;
    out.exposure_ambient[1] = settings.ambient_strength;
    out.exposure_ambient[2] = settings.ambient_floor;
    out.exposure_ambient[3] = settings.procedural_surface_normals ? 1.0f : 0.0f;
    out.fog_color_strength[0] = settings.atmosphere.fog_color.x;
    out.fog_color_strength[1] = settings.atmosphere.fog_color.y;
    out.fog_color_strength[2] = settings.atmosphere.fog_color.z;
    out.fog_color_strength[3] =
        settings.atmosphere.enabled ? settings.atmosphere.fog_strength : 0.0f;
    out.fog_params[0] = settings.atmosphere.fog_start;
    out.fog_params[1] = settings.atmosphere.fog_end;
    out.fog_params[2] = settings.atmosphere.saturation;
    out.fog_params[3] = settings.atmosphere.contrast;
    out.shadow_tint_strength[0] = settings.atmosphere.shadow_tint.x;
    out.shadow_tint_strength[1] = settings.atmosphere.shadow_tint.y;
    out.shadow_tint_strength[2] = settings.atmosphere.shadow_tint.z;
    out.shadow_tint_strength[3] = settings.atmosphere.shadow_tint_strength;
    out.highlight_tint_strength[0] = settings.atmosphere.highlight_tint.x;
    out.highlight_tint_strength[1] = settings.atmosphere.highlight_tint.y;
    out.highlight_tint_strength[2] = settings.atmosphere.highlight_tint.z;
    out.highlight_tint_strength[3] = settings.atmosphere.highlight_tint_strength;
    for (std::size_t i = 0; i < settings.light_rig.size(); ++i) {
      const aster::Light &light = settings.light_rig[i];
      out.lights[i].position_radius[0] = light.position.x;
      out.lights[i].position_radius[1] = light.position.y;
      out.lights[i].position_radius[2] = light.position.z;
      out.lights[i].position_radius[3] = light.source_radius;
      out.lights[i].color_intensity[0] = light.color.x;
      out.lights[i].color_intensity[1] = light.color.y;
      out.lights[i].color_intensity[2] = light.color.z;
      out.lights[i].color_intensity[3] = light.intensity;
    }
    return out;
  }

  bool ensureObjectUniformCapacity(const std::size_t capacity) {
    if (object_uniform_buffer_ != nil && object_uniform_capacity_ >= capacity) {
      return true;
    }
    [object_uniform_buffer_ release];
    object_uniform_buffer_ = [device_ newBufferWithLength:capacity * kObjectUniformStride
                                                  options:MTLResourceStorageModeShared];
    object_uniform_capacity_ = object_uniform_buffer_ == nil ? 0u : capacity;
    return object_uniform_buffer_ != nil;
  }

  MetalObjectUniforms makeObjectUniforms(const aster::RenderObject &object,
                                         const aster::OrbitCamera &camera,
                                         const aster::RendererSettings &settings,
                                         const double frame_seconds, const float opacity) const {
    const aster::Mat4 model =
        settings.animate_scene && object.spin_rate != 0.0f
            ? object.transform.matrix() *
                  aster::rotation_y(static_cast<float>(frame_seconds) * object.spin_rate)
            : object.transform.matrix();
    const float aspect_ratio =
        static_cast<float>(std::max(width_, 1)) / static_cast<float>(std::max(height_, 1));
    const aster::Mat4 mvp = camera.projectionMatrix(aspect_ratio) * camera.viewMatrix() * model;
    MetalObjectUniforms out;
    std::memcpy(out.model, model.m.data(), sizeof(out.model));
    std::memcpy(out.model_view_projection, mvp.m.data(), sizeof(out.model_view_projection));
    out.base_color_opacity[0] = object.material.base_color.x;
    out.base_color_opacity[1] = object.material.base_color.y;
    out.base_color_opacity[2] = object.material.base_color.z;
    out.base_color_opacity[3] = opacity;
    out.emission_strength[0] = object.material.emission_color.x;
    out.emission_strength[1] = object.material.emission_color.y;
    out.emission_strength[2] = object.material.emission_color.z;
    out.emission_strength[3] = object.material.emission_strength;
    out.material_params[0] = object.material.roughness;
    out.material_params[1] = object.material.metallic;
    out.material_params[2] = object.material.detail_strength;
    out.material_params[3] = object.material.detail_scale;
    out.pattern_params[0] = static_cast<float>(static_cast<int>(object.material.surface_pattern));
    out.pattern_params[1] = object.material.pattern_scale.x;
    out.pattern_params[2] = object.material.pattern_scale.y;
    out.pattern_params[3] = object.material.pattern_depth;
    out.pattern_params2[0] = object.material.pattern_contrast;
    out.pattern_params2[1] = object.material.pattern_mortar;
    out.pattern_params2[2] = object.material.ambient_occlusion;
    out.pattern_params2[3] = object.material.double_sided ? 1.0f : 0.0f;
    out.procedural_params[0] = object.material.procedural.macro_variation;
    out.procedural_params[1] = object.material.procedural.micro_normal_strength;
    out.procedural_params[2] = object.material.procedural.roughness_variation;
    out.procedural_params[3] = object.material.procedural.wetness;
    out.procedural_params2[0] = object.material.procedural.height_shading;
    out.material_flags[0] =
        object.material.surface_pattern == aster::SurfacePattern::ContactShadow ? 1.0f : 0.0f;
    out.material_flags[1] = isTerrainLayerPattern(object.material.surface_pattern) ? 1.0f : 0.0f;
    out.material_flags[2] =
        object.material.surface_pattern == aster::SurfacePattern::WaterSurface ? 1.0f : 0.0f;
    out.material_flags[3] =
        isStructuredSurfacePattern(object.material.surface_pattern) ? 1.0f : 0.0f;
    return out;
  }

  MetalMeshBuffers *buffersForMesh(const aster::CpuMesh &mesh) {
    auto it = mesh_buffers_.find(&mesh);
    if (it != mesh_buffers_.end()) {
      return &it->second;
    }

    std::vector<MetalVertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const aster::Vertex &vertex : mesh.vertices) {
      vertices.push_back({{vertex.position.x, vertex.position.y, vertex.position.z, 1.0f},
                          {vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f},
                          {vertex.uv.x, vertex.uv.y, vertex.ambient_occlusion, 0.0f}});
    }

    MetalMeshBuffers buffers;
    if (!vertices.empty()) {
      buffers.vertices = [device_ newBufferWithBytes:vertices.data()
                                              length:vertices.size() * sizeof(MetalVertex)
                                             options:MTLResourceStorageModeShared];
    }
    if (!mesh.indices.empty()) {
      buffers.indices = [device_ newBufferWithBytes:mesh.indices.data()
                                             length:mesh.indices.size() * sizeof(std::uint32_t)
                                            options:MTLResourceStorageModeShared];
      buffers.index_count = static_cast<NSUInteger>(mesh.indices.size());
    }
    auto inserted = mesh_buffers_.emplace(&mesh, buffers);
    return &inserted.first->second;
  }

  void evictRetiredMeshBuffers(const aster::PreparedRenderMeshes &meshes) {
    std::unordered_set<const aster::CpuMesh *> live;
    live.reserve(8u + (meshes.custom_meshes != nullptr ? meshes.custom_meshes->size() : 0u) +
                 (meshes.custom_mesh_resources != nullptr
                      ? meshes.custom_mesh_resources->size()
                      : 0u));
    const auto add = [&](const aster::CpuMesh *mesh) {
      if (mesh != nullptr) {
        live.insert(mesh);
      }
    };
    add(meshes.box);
    add(meshes.sphere);
    add(meshes.plane);
    add(meshes.contact_shadow_plane);
    add(meshes.rock);
    add(meshes.crystal);
    add(meshes.ruin_block);
    add(meshes.pillar);
    if (meshes.custom_meshes != nullptr) {
      for (const auto &entry : *meshes.custom_meshes) {
        add(&entry.second);
      }
    }
    if (meshes.custom_mesh_resources != nullptr) {
      for (const auto &entry : *meshes.custom_mesh_resources) {
        add(&entry.second);
      }
    }

    for (auto it = mesh_buffers_.begin(); it != mesh_buffers_.end();) {
      if (live.contains(it->first)) {
        ++it;
        continue;
      }
      [it->second.vertices release];
      [it->second.indices release];
      it = mesh_buffers_.erase(it);
    }
  }

  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  id<MTLLibrary> library_ = nil;
  id<MTLRenderPipelineState> opaque_pipeline_ = nil;
  id<MTLRenderPipelineState> transparent_pipeline_ = nil;
  id<MTLDepthStencilState> opaque_depth_ = nil;
  id<MTLDepthStencilState> read_only_depth_ = nil;
  id<MTLDepthStencilState> transparent_depth_ = nil;
  id<MTLDepthStencilState> disabled_depth_ = nil;
  id<MTLTexture> scene_texture_ = nil;
  id<MTLTexture> depth_texture_ = nil;
  id<MTLBuffer> object_uniform_buffer_ = nil;
  int width_ = 0;
  int height_ = 0;
  std::size_t object_uniform_capacity_ = 0;
  std::size_t object_uniform_cursor_ = 0;
  std::unordered_map<const aster::CpuMesh *, MetalMeshBuffers> mesh_buffers_;
};

} // namespace

namespace aster {

std::unique_ptr<NativeRenderBackend> createNativeRenderBackend() {
  return std::make_unique<MetalNativeRenderBackend>();
}

void clearNativeFrame() {
  MetalFrameState &state = metalFrameState();
  state.has_scene = false;
}

bool captureNativeFrameToActiveFramebuffer() {
  MetalFrameState &state = metalFrameState();
  if (!state.has_scene || state.scene_texture == nil || state.scene_width <= 0 ||
      state.scene_height <= 0) {
    return false;
  }
  [state.scene_command_buffer waitUntilCompleted];

  std::vector<std::uint8_t> scene_pixels(static_cast<std::size_t>(state.scene_width) *
                                         static_cast<std::size_t>(state.scene_height) * 4u);
  MTLRegion region = MTLRegionMake2D(0, 0, state.scene_width, state.scene_height);
  [state.scene_texture getBytes:scene_pixels.data()
                    bytesPerRow:static_cast<NSUInteger>(state.scene_width) * 4u
                     fromRegion:region
                    mipmapLevel:0];

  const SoftwareFrameBuffer &overlay = activeFrameBuffer();
  if (!overlay.empty() && overlay.width() == state.scene_width &&
      overlay.height() == state.scene_height) {
    const std::span<const std::uint8_t> rgba = overlay.rgba8();
    for (std::size_t i = 0; i + 3u < scene_pixels.size(); i += 4u) {
      const float alpha = static_cast<float>(rgba[i + 3u]) / 255.0f;
      scene_pixels[i + 0u] =
          static_cast<std::uint8_t>(static_cast<float>(scene_pixels[i + 0u]) * (1.0f - alpha) +
                                    static_cast<float>(rgba[i + 0u]) * alpha + 0.5f);
      scene_pixels[i + 1u] =
          static_cast<std::uint8_t>(static_cast<float>(scene_pixels[i + 1u]) * (1.0f - alpha) +
                                    static_cast<float>(rgba[i + 1u]) * alpha + 0.5f);
      scene_pixels[i + 2u] =
          static_cast<std::uint8_t>(static_cast<float>(scene_pixels[i + 2u]) * (1.0f - alpha) +
                                    static_cast<float>(rgba[i + 2u]) * alpha + 0.5f);
      scene_pixels[i + 3u] = 255u;
    }
  }
  activeFrameBuffer().replaceRgba8(state.scene_width, state.scene_height, scene_pixels);
  return true;
}

bool presentNativeFrameInView(void *view_pointer, const bool vsync) {
  @autoreleasepool {
    MetalFrameState &state = metalFrameState();
    if (!state.has_scene || state.scene_texture == nil || view_pointer == nullptr ||
        !ensurePresenterPipeline()) {
      return false;
    }

    NSView *view = static_cast<NSView *>(view_pointer);
    CAMetalLayer *layer = nil;
    if ([[view layer] isKindOfClass:[CAMetalLayer class]]) {
      layer = static_cast<CAMetalLayer *>([view layer]);
    } else {
      layer = [CAMetalLayer layer];
      layer.device = sharedMetalDevice();
      layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
      layer.framebufferOnly = YES;
      layer.opaque = YES;
      [view setWantsLayer:YES];
      [view setLayer:layer];
    }
    const CGFloat scale = [[view window] backingScaleFactor] > 0.0
                              ? [[view window] backingScaleFactor]
                              : [[NSScreen mainScreen] backingScaleFactor];
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(state.scene_width, state.scene_height);
    if ([layer respondsToSelector:@selector(setMaximumDrawableCount:)]) {
      layer.maximumDrawableCount = 3;
    }
    if ([layer respondsToSelector:@selector(setDisplaySyncEnabled:)]) {
      layer.displaySyncEnabled = vsync ? YES : NO;
    }

    const bool has_overlay = uploadOverlayTexture();
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (drawable == nil) {
      return true;
    }

    id<MTLCommandBuffer> command_buffer = [sharedMetalQueue() commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:state.present_pipeline];
    const std::uint32_t mode = 1u | (has_overlay ? 2u : 0u);
    [encoder setFragmentTexture:state.scene_texture atIndex:0];
    [encoder setFragmentTexture:has_overlay ? state.overlay_texture : state.scene_texture
                        atIndex:1];
    [encoder setFragmentBytes:&mode length:sizeof(mode) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [command_buffer presentDrawable:drawable];
    [command_buffer commit];
    return true;
  }
}

} // namespace aster
