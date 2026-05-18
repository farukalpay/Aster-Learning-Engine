// Author: Faruk Alpay
// Do not remove this notice.

#include "native_render_backend.hpp"

#ifdef _WIN32

#include "aster/core/profiler.hpp"
#include "aster/render/render_graph_executor.hpp"
#include "aster/render/software_framebuffer.hpp"
#include "aster/scene/scene.hpp"
#include "render_backend_common.hpp"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

constexpr std::size_t kConstantBufferAlignment = 256u;

struct D3D12Vertex {
  float position[4]{};
  float normal[4]{};
  float uv_ao[4]{};
};

struct D3D12LightUniform {
  float position_radius[4]{};
  float color_intensity[4]{};
};

struct D3D12SceneUniforms {
  float camera_time[4]{};
  float sun_direction_enabled[4]{};
  float sun_color_intensity[4]{};
  float sky_ambient[4]{};
  float ground_ambient[4]{};
  float exposure_ambient[4]{};
  float fog_color_strength[4]{};
  float fog_params[4]{};
  float style_params[4]{};
  float style_params2[4]{};
  float lighting_params[4]{};
  D3D12LightUniform lights[aster::kRenderLightUniformCapacity]{};
};

struct D3D12ObjectUniforms {
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

struct D3D12MeshBuffers {
  ComPtr<ID3D12Resource> vertices;
  ComPtr<ID3D12Resource> indices;
  D3D12_VERTEX_BUFFER_VIEW vertex_view{};
  D3D12_INDEX_BUFFER_VIEW index_view{};
  UINT index_count = 0u;
};

std::size_t alignTo(const std::size_t value, const std::size_t alignment) {
  return (value + alignment - 1u) & ~(alignment - 1u);
}

bool depthPolicyUsesBias(const aster::RenderDepthPolicy policy) {
  return policy.layer != aster::RenderDepthLayer::BaseSurface ||
         std::abs(policy.constant_bias) > 0.0f || std::abs(policy.slope_bias) > 0.0f ||
         std::abs(policy.normal_offset) > 0.0f;
}

D3D12_HEAP_PROPERTIES heapProperties(const D3D12_HEAP_TYPE type) {
  D3D12_HEAP_PROPERTIES props{};
  props.Type = type;
  props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  props.CreationNodeMask = 1u;
  props.VisibleNodeMask = 1u;
  return props;
}

D3D12_RESOURCE_DESC bufferDesc(const std::uint64_t byte_size) {
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment = 0u;
  desc.Width = byte_size;
  desc.Height = 1u;
  desc.DepthOrArraySize = 1u;
  desc.MipLevels = 1u;
  desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count = 1u;
  desc.SampleDesc.Quality = 0u;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  return desc;
}

bool isTerrainLayerProfile(const aster::MaterialSurfaceProfile profile) {
  return profile == aster::MaterialSurfaceProfile::TerrainLayer;
}

bool isStructuredSurfaceProfile(const aster::MaterialSurfaceProfile profile) {
  return profile != aster::MaterialSurfaceProfile::Plain &&
         profile != aster::MaterialSurfaceProfile::TerrainLayer &&
         profile != aster::MaterialSurfaceProfile::Liquid &&
         profile != aster::MaterialSurfaceProfile::ContactShadow;
}

aster::rhi::DeviceCapabilities d3d12CapabilityTable() {
  aster::rhi::DeviceCapabilities table;
  table.backend = aster::rhi::BackendKind::D3D12;
  table.shader_materials = true;
  table.texture_sampling = false;
  table.instancing = true;
  table.capture = true;
  table.ui_composite = false;
  table.gpu_timestamps = false;
  table.storage_buffers = true;
  table.texture_arrays = true;
  table.shadow_maps = false;
  table.debug_markers = false;
  table.hdr_render_targets = false;
  table.msaa = false;
  table.color_format_mask =
      aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Bgra8Unorm);
  table.depth_format_mask =
      aster::rhi::imageFormatCapabilityBit(aster::rhi::ImageFormat::Depth32Float);
  table.sample_count_mask = aster::rhi::sampleCountCapabilityBit(1u);
  table.blend_mode_mask = aster::rhi::blendModeCapabilityBit(aster::rhi::BlendMode::Opaque) |
                          aster::rhi::blendModeCapabilityBit(aster::rhi::BlendMode::AlphaBlend);
  table.shader_model = aster::rhi::ShaderModel::D3D12ShaderModel51;
  table.presentation = aster::rhi::PresentationMode::D3D12OffscreenReadback;
  table.limits.max_color_attachments = 1u;
  table.limits.max_uniform_buffers_per_stage = 1u;
  table.limits.max_storage_buffers_per_stage = 1u;
  table.limits.max_bind_groups = 1u;
  table.limits.max_vertex_attributes = 4u;
  table.limits.max_texture_dimension_2d = 16384u;
  table.limits.max_dynamic_uniform_bytes = 256u * 1024u;
  return table;
}

aster::RenderBackendCapabilities d3d12Capabilities() {
  const std::uint32_t graph_resources =
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneColor) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::SceneDepth) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::LightClusters) |
      aster::renderGraphResourceBit(aster::RenderGraphResource::CaptureReadback);
  return {.kind = aster::RenderBackendKind::D3D12,
          .name = "Aster Native D3D12 Rasterizer",
          .gpu = true,
          .supports_shader_materials = true,
          .supports_texture_sampling = false,
          .supports_instancing = true,
          .supports_capture = true,
          .supports_ui_composite = false,
          .supports_gpu_timestamps = false,
          .graph_resource_mask = graph_resources,
          .capability_table = d3d12CapabilityTable()};
}

const char *d3dShaderSource() {
  return R"hlsl(
struct Light { float4 position_radius; float4 color_intensity; };
cbuffer SceneBuffer : register(b0) {
  float4 scene_camera_time;
  float4 scene_sun_direction_enabled;
  float4 scene_sun_color_intensity;
  float4 scene_sky_ambient;
  float4 scene_ground_ambient;
  float4 scene_exposure_ambient;
  float4 scene_fog_color_strength;
  float4 scene_fog_params;
  float4 scene_style_params;
  float4 scene_style_params2;
  float4 scene_lighting_params;
  Light scene_lights[64];
};
struct Object {
  float4x4 model;
  float4x4 mvp;
  float4 base_color_opacity;
  float4 emission_strength;
  float4 material_params;
  float4 pattern_params;
  float4 pattern_params2;
  float4 procedural_params;
  float4 procedural_params2;
  float4 material_flags;
};
StructuredBuffer<Object> objects : register(t0);
struct VSIn { float4 position : POSITION; float4 normal : NORMAL; float4 uv_ao : TEXCOORD0; };
struct VSOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  float ao : TEXCOORD3;
  uint object_index : TEXCOORD4;
};
float saturate1(float v) { return saturate(v); }
float smooth1(float a, float b, float v) {
  float t = saturate((v - a) / max(b - a, 0.0001));
  return t * t * (3.0 - 2.0 * t);
}
float fract1(float v) { return v - floor(v); }
float hash31(float3 p) {
  p = frac(p * float3(0.1031, 0.11369, 0.13787));
  float d = dot(p, p.yzx + 19.19);
  p += d;
  return fract1((p.x + p.y) * p.z);
}
float noise3(float3 p) {
  float3 i = floor(p);
  float3 f = frac(p);
  f = f * f * (3.0 - 2.0 * f);
  float n000 = hash31(i + float3(0,0,0));
  float n100 = hash31(i + float3(1,0,0));
  float n010 = hash31(i + float3(0,1,0));
  float n110 = hash31(i + float3(1,1,0));
  float n001 = hash31(i + float3(0,0,1));
  float n101 = hash31(i + float3(1,0,1));
  float n011 = hash31(i + float3(0,1,1));
  float n111 = hash31(i + float3(1,1,1));
  return lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
              lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
}
float ridge1(float v) { return 1.0 - abs(v * 2.0 - 1.0); }
float3 gamma_encode(float3 c) { return pow(saturate(c), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)); }
float3 tonemap(float3 c) { return c / (float3(1.0, 1.0, 1.0) + max(c, float3(0.0, 0.0, 0.0))); }
float3 style_sample_world(float3 world) {
  float snap = max(scene_style_params2.x, 0.0);
  if (snap <= 0.0001) return world;
  return floor(world / snap + float3(0.5, 0.5, 0.5)) * snap;
}
float style_fog(float distance_to_camera) {
  float range = max(scene_fog_params.y - scene_fog_params.x, 0.001);
  float normalized = max((distance_to_camera - scene_fog_params.x) / range, 0.0);
  float power = max(scene_style_params2.z, 0.001);
  float curve = smooth1(0.0, 1.0, normalized);
  if (scene_style_params2.y > 1.5) {
    curve = pow(saturate(normalized), power);
  } else if (scene_style_params2.y > 0.5) {
    curve = 1.0 - exp(-normalized * power);
  }
  return saturate(curve * scene_fog_color_strength.w);
}
float3 style_post(float3 color) {
  float crush = saturate(scene_style_params.z);
  if (crush > 0.0001) {
    float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
    float dark = 1.0 - smooth1(0.10, 0.58, luma);
    color = lerp(color, color * (0.58 + luma * 0.42), dark * crush);
  }
  float steps = floor(max(scene_style_params.w, 0.0));
  if (steps > 1.0) {
    color = floor(saturate(color) * steps + float3(0.5, 0.5, 0.5)) / steps;
  }
  return color;
}
float projected_noise(float3 world, float3 normal, float scale, float salt) {
  float3 n = normalize(normal);
  if (length(n) < 0.001) n = float3(0.0, 1.0, 0.0);
  float3 w = pow(abs(n), float3(4.0, 4.0, 4.0));
  w /= max(w.x + w.y + w.z, 0.0001);
  float xy = noise3(float3(world.x * scale, world.y * scale, salt));
  float xz = noise3(float3(world.x * scale, world.z * scale, salt + 11.7));
  float zy = noise3(float3(world.z * scale, world.y * scale, salt + 23.4));
  return xy * w.z + xz * w.y + zy * w.x;
}
float projected_fbm(float3 world, float3 normal, float scale, float salt) {
  float sum = 0.0;
  float amp = 0.56;
  float norm = 0.0;
  float f = max(scale, 0.001);
  [unroll] for (int i = 0; i < 4; ++i) {
    sum += projected_noise(world, normal, f, salt + float(i) * 17.0) * amp;
    norm += amp;
    f *= 2.08;
    amp *= 0.52;
  }
  return norm > 0.0 ? sum / norm : 0.0;
}
float is_pattern(float pattern, float id) { return abs(pattern - id) < 0.5 ? 1.0 : 0.0; }
float3 stratified_rock(float3 base, Object object, float3 world, float3 normal) {
  float strata = 0.5 + 0.5 * sin((world.y * object.pattern_params.z + world.z * 0.33 + world.x * 0.18) * 1.75);
  float broad = projected_fbm(world, normal, object.material_params.w * 0.10, 41.0);
  float fine = projected_fbm(world, normal, object.material_params.w * 0.88, 53.0);
  float crack = ridge1(projected_fbm(world, normal, object.material_params.w * 0.42, 67.0));
  float3 damp = base * float3(0.72, 0.70, 0.62);
  float3 mineral = base * float3(1.24, 1.13, 0.92);
  float3 albedo = lerp(damp, base, broad * 0.74);
  albedo = lerp(albedo, mineral, saturate(strata * 0.18 + ridge1(fine) * 0.12) * saturate(object.pattern_params2.x));
  albedo = lerp(albedo, albedo * 0.50, smooth1(0.68, 0.96, crack) * (0.35 + object.pattern_params.w * 1.8));
  return clamp(albedo * (0.82 + fine * 0.22), 0.0, 4.0);
}
float3 material_albedo(Object object, float3 world, float3 normal, float2 uv) {
  float pattern = object.pattern_params.x;
  float detail = max(object.material_params.w, 0.001);
  float3 base = object.base_color_opacity.rgb;
  if (object.material_flags.x > 0.5) return float3(0.018, 0.015, 0.012);
  if (is_pattern(pattern, 11.0) > 0.5) return stratified_rock(base, object, world, normal);
  if (is_pattern(pattern, 12.0) > 0.5) {
    float vein = smooth1(0.70, 0.96, ridge1(projected_fbm(world, normal, detail * 0.44, 71.0)));
    return lerp(base * 0.52, base + object.emission_strength.rgb * 0.35, vein);
  }
  if (is_pattern(pattern, 7.0) > 0.5) {
    float cloud = projected_fbm(world, normal, detail * 1.10, 131.0);
    return lerp(base * float3(0.62, 0.42, 0.28), base * float3(1.30, 0.96, 0.56), smooth1(0.25, 0.92, cloud));
  }
  if (object.material_flags.y > 0.5) {
    float n = projected_fbm(world, normal, detail * 0.18, pattern * 3.7);
    float slope = 1.0 - smooth1(0.54, 0.92, normalize(normal).y);
    return base * (0.78 + n * 0.22 + slope * 0.10);
  }
  float macro = noise3(world * (0.16 * detail) + float3(pattern * 0.7, 0.0, pattern * 0.31));
  return base * (0.86 + macro * 0.22 + object.pattern_params2.x * 0.08);
}
VSOut vs_main(VSIn input, uint instance_id : SV_InstanceID) {
  Object object = objects[instance_id];
  float4 local = float4(input.position.xyz, 1.0);
  VSOut outp;
  outp.position = mul(object.mvp, local);
  outp.world = mul(object.model, local).xyz;
  outp.normal = normalize(mul(object.model, float4(input.normal.xyz, 0.0)).xyz);
  outp.uv = input.uv_ao.xy;
  outp.ao = input.uv_ao.z;
  outp.object_index = instance_id;
  return outp;
}
float4 fs_main(VSOut input) : SV_Target0 {
  Object object = objects[input.object_index];
  float opacity = object.base_color_opacity.w;
  if (object.material_flags.x > 0.5) {
    float2 centered = input.uv * 2.0 - 1.0;
    float soft = 1.0 - smooth1(0.12, 1.0, dot(centered, centered));
    return float4(0.018, 0.015, 0.012, opacity * soft * soft * 0.70);
  }
  float3 normal = normalize(input.normal);
  if (length(normal) < 0.001) normal = float3(0.0, 1.0, 0.0);
  float3 sample_world = style_sample_world(input.world);
  float3 albedo = material_albedo(object, sample_world, normal, input.uv);
  float sky = saturate(normal.y * 0.5 + 0.5);
  float ambient = max(scene_exposure_ambient.y * saturate(object.pattern_params2.z * input.ao), scene_exposure_ambient.z);
  float3 color = lerp(scene_ground_ambient.rgb, scene_sky_ambient.rgb, sky) * albedo * ambient + albedo * 0.035;
  float3 view = normalize(scene_camera_time.xyz - input.world);
  float layer_roughness = saturate(object.material_params.x + (noise3(sample_world * 0.37) - 0.5) * object.procedural_params.z * 0.18);
  if (scene_sun_direction_enabled.w > 0.5) {
    float3 light_dir = normalize(scene_sun_direction_enabled.xyz);
    float ndotl = max(dot(normal, light_dir), 0.0);
    float3 h = normalize(light_dir + view);
    float spec = pow(max(dot(normal, h), 0.0), lerp(64.0, 8.0, layer_roughness)) * (1.0 - layer_roughness) * 0.28;
    color += (albedo * 0.78 * (ndotl * 0.86 + 0.14) + spec) * scene_sun_color_intensity.rgb * scene_sun_color_intensity.w;
  }
  for (uint i = 0; i < uint(scene_lighting_params.x); ++i) {
    float3 lv = scene_lights[i].position_radius.xyz - input.world;
    float d2 = max(dot(lv, lv), 0.0001);
    float soft = max(d2, scene_lights[i].position_radius.w * scene_lights[i].position_radius.w + 0.0001);
    float ndotl = max(dot(normal, normalize(lv)), 0.0);
    color += albedo * (ndotl * 0.84 + 0.12) * scene_lights[i].color_intensity.rgb * (scene_lights[i].color_intensity.w / soft);
  }
  color = lerp(color, albedo * max(scene_exposure_ambient.y + scene_exposure_ambient.z + 0.14, 0.18), saturate(scene_style_params.x));
  float fog = style_fog(distance(scene_camera_time.xyz, input.world));
  color = lerp(color, scene_fog_color_strength.rgb, saturate(fog));
  float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
  color = lerp(float3(luma, luma, luma), color, saturate(scene_fog_params.z));
  color = (color - 0.5) * max(scene_fog_params.w, 0.0) + 0.5;
  color += object.emission_strength.rgb * object.emission_strength.w * max(scene_style_params.y, 0.0);
  color = tonemap(color * scene_exposure_ambient.x);
  color = style_post(color);
  return float4(gamma_encode(color), opacity);
}
)hlsl";
}

class D3D12NativeRenderBackend final : public aster::NativeRenderBackend {
public:
  ~D3D12NativeRenderBackend() override {
    waitForGpu();
    if (fence_event_ != nullptr) {
      CloseHandle(fence_event_);
      fence_event_ = nullptr;
    }
  }

  bool initialize() override {
    UINT factory_flags = 0u;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
      debug->EnableDebugLayer();
      factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_)))) {
      return false;
    }
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapter_index = 0u;
         factory_->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapter_index) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u) {
        adapter.Reset();
        continue;
      }
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                      IID_PPV_ARGS(&device_)))) {
        break;
      }
      adapter.Reset();
    }
    if (device_ == nullptr &&
        FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
      return false;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_))) ||
        FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&allocator_))) ||
        FAILED(device_->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(),
                                          nullptr, IID_PPV_ARGS(&command_list_)))) {
      return false;
    }
    command_list_->Close();
    if (FAILED(device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
      return false;
    }
    fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return fence_event_ != nullptr && createRootSignature() && createPipelines();
  }

  aster::FrameStats render(const aster::Scene &scene, const aster::FrameRenderPlan &plan,
                           const aster::OrbitCamera &camera,
                           const aster::RendererSettings &settings,
                           const aster::FixedRenderGraph &graph,
                           const aster::PreparedRenderMeshes &meshes,
                           const int framebuffer_width, const int framebuffer_height,
                           const double frame_seconds,
                           aster::FrameForensics *forensics) override {
    ASTER_PROFILE_SCOPE("D3D12NativeRenderBackend::render");
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
    if (device_ == nullptr || framebuffer_width <= 0 || framebuffer_height <= 0 ||
        !ensureTargets(framebuffer_width, framebuffer_height) ||
        !ensureSceneUpload() || !ensureObjectUpload(std::max<std::size_t>(
                                     plan.instances.size() + scene.objects().size(), 1u))) {
      return stats;
    }

    evictRetiredMeshBuffers(meshes);
    width_ = framebuffer_width;
    height_ = framebuffer_height;
    object_cursor_ = 0u;
    const auto encode_start = std::chrono::steady_clock::now();
    if (!beginCommandList()) {
      return stats;
    }

    transitionColor(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(framebuffer_width),
                            static_cast<float>(framebuffer_height), 0.0f, 1.0f};
    D3D12_RECT scissor{0, 0, framebuffer_width, framebuffer_height};
    command_list_->RSSetViewports(1u, &viewport);
    command_list_->RSSetScissorRects(1u, &scissor);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    command_list_->OMSetRenderTargets(1u, &rtv, FALSE, &dsv);
    const float clear[4] = {settings.pipeline.clear_color.x, settings.pipeline.clear_color.y,
                            settings.pipeline.clear_color.z, 1.0f};
    command_list_->ClearRenderTargetView(rtv, clear, 0u, nullptr);
    command_list_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0u, 0u, nullptr);
    command_list_->SetGraphicsRootSignature(root_signature_.Get());
    uploadSceneUniforms(camera, settings, frame_seconds);
    command_list_->SetGraphicsRootConstantBufferView(
        0u, scene_upload_->GetGPUVirtualAddress());
    command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto draw_object = [&](const aster::RenderObject &object, const float opacity) {
      const aster::CpuMesh *mesh = aster::render_backend::meshForObject(object, meshes);
      if (mesh == nullptr || mesh->indices.empty()) {
        return;
      }
      D3D12MeshBuffers *buffers = buffersForMesh(*mesh);
      if (buffers == nullptr || buffers->index_count == 0u) {
        return;
      }
      const std::optional<std::size_t> object_index =
          writeObjectUniform(object, camera, settings, frame_seconds, opacity);
      if (!object_index.has_value()) {
        return;
      }
      const bool transparent =
          opacity < 0.999f || aster::classifyMaterialRenderQueue(object.material) ==
                                  aster::MaterialRenderQueue::Translucent;
      const bool biased = depthPolicyUsesBias(object.material.depth_policy);
      command_list_->SetPipelineState(
          transparent ? (biased ? transparent_biased_pso_.Get() : transparent_pso_.Get())
                      : (biased ? opaque_biased_pso_.Get() : opaque_pso_.Get()));
      command_list_->SetGraphicsRootShaderResourceView(
          1u, object_upload_->GetGPUVirtualAddress() +
                  static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(*object_index * sizeof(D3D12ObjectUniforms)));
      command_list_->IASetVertexBuffers(0u, 1u, &buffers->vertex_view);
      command_list_->IASetIndexBuffer(&buffers->index_view);
      command_list_->DrawIndexedInstanced(buffers->index_count, 1u, 0u, 0, 0u);
      ++stats.draw_calls;
    };

    const auto draw_group = [&](const aster::FrameRenderDrawGroup &group) {
      if (group.instance_count == 0u || group.first_instance >= plan.instances.size()) {
        return;
      }
      const aster::FrameRenderInstance &first_instance = plan.instances[group.first_instance];
      if (first_instance.object_index >= scene.objects().size()) {
        return;
      }
      const aster::RenderObject &first_object = scene.objects()[first_instance.object_index];
      const aster::CpuMesh *mesh = aster::render_backend::meshForObject(first_object, meshes);
      if (mesh == nullptr || mesh->indices.empty()) {
        return;
      }
      D3D12MeshBuffers *buffers = buffersForMesh(*mesh);
      if (buffers == nullptr || buffers->index_count == 0u) {
        return;
      }
      const std::size_t first_uniform = object_cursor_;
      std::size_t written = 0u;
      for (std::size_t i = 0; i < group.instance_count; ++i) {
        const std::size_t plan_index = group.first_instance + i;
        if (plan_index >= plan.instances.size()) {
          break;
        }
        const aster::FrameRenderInstance &instance = plan.instances[plan_index];
        if (instance.object_index >= scene.objects().size()) {
          continue;
        }
        if (!writeObjectUniform(scene.objects()[instance.object_index], camera, settings,
                                frame_seconds, instance.opacity)
                 .has_value()) {
          break;
        }
        ++written;
      }
      if (written == 0u) {
        return;
      }
      const bool biased = depthPolicyUsesBias(first_object.material.depth_policy);
      command_list_->SetPipelineState(
          group.pass == aster::FrameRenderPass::Transparent
              ? (biased ? transparent_biased_pso_.Get() : transparent_pso_.Get())
              : (biased ? opaque_biased_pso_.Get() : opaque_pso_.Get()));
      command_list_->SetGraphicsRootShaderResourceView(
          1u, object_upload_->GetGPUVirtualAddress() +
                  static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(first_uniform * sizeof(D3D12ObjectUniforms)));
      command_list_->IASetVertexBuffers(0u, 1u, &buffers->vertex_view);
      command_list_->IASetIndexBuffer(&buffers->index_view);
      command_list_->DrawIndexedInstanced(buffers->index_count, static_cast<UINT>(written), 0u, 0,
                                          0u);
      ++stats.draw_calls;
    };

    const auto draw_contact_shadows = [&] {
      for (const aster::FrameRenderInstance &instance : plan.instances) {
        if (instance.object_index >= scene.objects().size()) {
          continue;
        }
        const aster::RenderObject &object = scene.objects()[instance.object_index];
        if (aster::render_backend::canCastContactShadow(object, settings.grounding)) {
          const aster::RenderObject shadow =
              aster::render_backend::contactShadowObjectFor(object, settings.grounding);
          draw_object(shadow, shadow.material.opacity);
        }
      }
    };

    (void)aster::executeFixedRenderGraph(
        graph, [&](const aster::RenderGraphPassInvocation &invocation) {
          const auto pass_start = std::chrono::steady_clock::now();
          const std::size_t draws_before = stats.draw_calls;
          switch (invocation.semantic) {
          case aster::RenderGraphPass::LightCull:
          case aster::RenderGraphPass::ShadowAtlas:
          case aster::RenderGraphPass::SceneLighting:
          case aster::RenderGraphPass::VolumetricFog:
          case aster::RenderGraphPass::ReflectionProbe:
            break;
          case aster::RenderGraphPass::Opaque:
            for (const aster::FrameRenderDrawGroup &group : plan.groups) {
              if (group.pass == aster::FrameRenderPass::Opaque) {
                draw_group(group);
              }
            }
            break;
          case aster::RenderGraphPass::ContactShadow:
            draw_contact_shadows();
            break;
          case aster::RenderGraphPass::Transparent:
            for (const aster::FrameRenderDrawGroup &group : plan.groups) {
              if (group.pass == aster::FrameRenderPass::Transparent) {
                draw_group(group);
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

    transitionColor(D3D12_RESOURCE_STATE_COPY_SOURCE);
    copyColorToReadback();
    const auto encode_end = std::chrono::steady_clock::now();
    stats.render_encode_seconds =
        std::chrono::duration<double>(encode_end - encode_start).count();
    if (!submitAndReadback()) {
      return stats;
    }
    return stats;
  }

  const char *backendName() const override {
    return "Aster Native D3D12 Rasterizer";
  }

  aster::RenderBackendCapabilities capabilities() const override {
    return d3d12Capabilities();
  }

private:
  bool createRootSignature() {
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0u;
    params[0].Descriptor.RegisterSpace = 0u;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[1].Descriptor.ShaderRegister = 0u;
    params[1].Descriptor.RegisterSpace = 0u;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2u;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &signature, &error))) {
      return false;
    }
    return SUCCEEDED(device_->CreateRootSignature(0u, signature->GetBufferPointer(),
                                                  signature->GetBufferSize(),
                                                  IID_PPV_ARGS(&root_signature_)));
  }

  bool createPipelines() {
    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    ComPtr<ID3DBlob> error;
    constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (FAILED(D3DCompile(d3dShaderSource(), std::strlen(d3dShaderSource()), "aster_d3d12",
                          nullptr, nullptr, "vs_main", "vs_5_0", flags, 0u, &vs, &error)) ||
        FAILED(D3DCompile(d3dShaderSource(), std::strlen(d3dShaderSource()), "aster_d3d12",
                          nullptr, nullptr, "fs_main", "ps_5_0", flags, 0u, &ps, &error))) {
      return false;
    }

    D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 0u,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
        {"NORMAL", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 16u,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
        {"TEXCOORD", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 32u,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = {input, static_cast<UINT>(std::size(input))};
    pso.pRootSignature = root_signature_.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1u;
    pso.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1u;
    if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&opaque_pso_)))) {
      return false;
    }

    pso.RasterizerState.DepthBias = 1;
    pso.RasterizerState.SlopeScaledDepthBias = 1.0f;
    if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&opaque_biased_pso_)))) {
      return false;
    }

    pso.RasterizerState.DepthBias = 0;
    pso.RasterizerState.SlopeScaledDepthBias = 0.0f;
    pso.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&transparent_pso_)))) {
      return false;
    }
    pso.RasterizerState.DepthBias = 1;
    pso.RasterizerState.SlopeScaledDepthBias = 1.0f;
    return SUCCEEDED(
        device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&transparent_biased_pso_)));
  }

  bool beginCommandList() {
    if (FAILED(allocator_->Reset())) {
      return false;
    }
    return SUCCEEDED(command_list_->Reset(allocator_.Get(), nullptr));
  }

  bool ensureUpload(ComPtr<ID3D12Resource> &resource, const std::size_t byte_size) {
    if (resource != nullptr && resource->GetDesc().Width >= byte_size) {
      return true;
    }
    const D3D12_HEAP_PROPERTIES heap = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC desc = bufferDesc(byte_size);
    return SUCCEEDED(device_->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&resource)));
  }

  bool ensureSceneUpload() {
    return ensureUpload(scene_upload_, alignTo(sizeof(D3D12SceneUniforms), kConstantBufferAlignment));
  }

  bool ensureObjectUpload(const std::size_t object_capacity) {
    object_capacity_ = object_capacity;
    return ensureUpload(object_upload_, object_capacity * sizeof(D3D12ObjectUniforms));
  }

  bool ensureTargets(const int width, const int height) {
    if (color_ != nullptr && width == width_ && height == height_) {
      return true;
    }
    waitForGpu();
    color_.Reset();
    depth_.Reset();
    readback_.Reset();
    width_ = width;
    height_ = height;
    color_state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_RESOURCE_DESC color_desc{};
    color_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    color_desc.Width = static_cast<UINT64>(width);
    color_desc.Height = static_cast<UINT>(height);
    color_desc.DepthOrArraySize = 1u;
    color_desc.MipLevels = 1u;
    color_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    color_desc.SampleDesc.Count = 1u;
    color_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE color_clear{};
    color_clear.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    color_clear.Color[3] = 1.0f;
    D3D12_HEAP_PROPERTIES default_heap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &color_desc,
                                                color_state_, &color_clear,
                                                IID_PPV_ARGS(&color_)))) {
      return false;
    }

    D3D12_RESOURCE_DESC depth_desc = color_desc;
    depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
    depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE depth_clear{};
    depth_clear.Format = DXGI_FORMAT_D32_FLOAT;
    depth_clear.DepthStencil.Depth = 0.0f;
    if (FAILED(device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &depth_desc,
                                                D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_clear,
                                                IID_PPV_ARGS(&depth_)))) {
      return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors = 1u;
    D3D12_DESCRIPTOR_HEAP_DESC dsv_desc{};
    dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_desc.NumDescriptors = 1u;
    if ((rtv_heap_ == nullptr &&
         FAILED(device_->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap_)))) ||
        (dsv_heap_ == nullptr &&
         FAILED(device_->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&dsv_heap_))))) {
      return false;
    }
    device_->CreateRenderTargetView(color_.Get(), nullptr, rtv_heap_->GetCPUDescriptorHandleForHeapStart());
    device_->CreateDepthStencilView(depth_.Get(), nullptr, dsv_heap_->GetCPUDescriptorHandleForHeapStart());
    return ensureReadback();
  }

  bool ensureReadback() {
    D3D12_RESOURCE_DESC desc = color_->GetDesc();
    UINT64 total_bytes = 0u;
    device_->GetCopyableFootprints(&desc, 0u, 1u, 0u, &readback_footprint_, &readback_rows_,
                                   &readback_row_bytes_, &total_bytes);
    const D3D12_HEAP_PROPERTIES heap = heapProperties(D3D12_HEAP_TYPE_READBACK);
    const D3D12_RESOURCE_DESC readback_desc = bufferDesc(total_bytes);
    return SUCCEEDED(device_->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &readback_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&readback_)));
  }

  void transitionColor(const D3D12_RESOURCE_STATES after) {
    if (color_state_ == after || color_ == nullptr) {
      return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = color_.Get();
    barrier.Transition.StateBefore = color_state_;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list_->ResourceBarrier(1u, &barrier);
    color_state_ = after;
  }

  void uploadSceneUniforms(const aster::OrbitCamera &camera,
                           const aster::RendererSettings &settings,
                           const double frame_seconds) {
    D3D12SceneUniforms uniforms;
    const aster::Vec3 camera_position = camera.position();
    uniforms.camera_time[0] = camera_position.x;
    uniforms.camera_time[1] = camera_position.y;
    uniforms.camera_time[2] = camera_position.z;
    uniforms.camera_time[3] = static_cast<float>(frame_seconds);
    const aster::Vec3 sun = aster::normalize(settings.sun_light.direction_to_light);
    uniforms.sun_direction_enabled[0] = sun.x;
    uniforms.sun_direction_enabled[1] = sun.y;
    uniforms.sun_direction_enabled[2] = sun.z;
    uniforms.sun_direction_enabled[3] = settings.sun_light.enabled ? 1.0f : 0.0f;
    uniforms.sun_color_intensity[0] = settings.sun_light.color.x;
    uniforms.sun_color_intensity[1] = settings.sun_light.color.y;
    uniforms.sun_color_intensity[2] = settings.sun_light.color.z;
    uniforms.sun_color_intensity[3] = settings.sun_light.intensity;
    uniforms.sky_ambient[0] = settings.sky_ambient_color.x;
    uniforms.sky_ambient[1] = settings.sky_ambient_color.y;
    uniforms.sky_ambient[2] = settings.sky_ambient_color.z;
    uniforms.ground_ambient[0] = settings.ground_ambient_color.x;
    uniforms.ground_ambient[1] = settings.ground_ambient_color.y;
    uniforms.ground_ambient[2] = settings.ground_ambient_color.z;
    uniforms.exposure_ambient[0] = settings.exposure;
    uniforms.exposure_ambient[1] = settings.ambient_strength;
    uniforms.exposure_ambient[2] = settings.ambient_floor;
    uniforms.exposure_ambient[3] = settings.procedural_surface_normals ? 1.0f : 0.0f;
    uniforms.fog_color_strength[0] = settings.atmosphere.fog_color.x;
    uniforms.fog_color_strength[1] = settings.atmosphere.fog_color.y;
    uniforms.fog_color_strength[2] = settings.atmosphere.fog_color.z;
    uniforms.fog_color_strength[3] =
        settings.atmosphere.enabled ? settings.atmosphere.fog_strength : 0.0f;
    uniforms.fog_params[0] = settings.atmosphere.fog_start;
    uniforms.fog_params[1] = settings.atmosphere.fog_end;
    uniforms.fog_params[2] = settings.atmosphere.saturation;
    uniforms.fog_params[3] = settings.atmosphere.contrast;
    uniforms.style_params[0] = settings.style.unlit_mix;
    uniforms.style_params[1] = settings.style.emissive_gain;
    uniforms.style_params[2] = settings.style.luma_crush;
    uniforms.style_params[3] = settings.style.color_quantization_steps;
    uniforms.style_params2[0] = settings.style.procedural_sample_snap;
    uniforms.style_params2[1] = static_cast<float>(settings.atmosphere.fog_falloff);
    uniforms.style_params2[2] = settings.atmosphere.fog_power;
    const std::vector<aster::Light> selected_lights =
        aster::selectRenderLights(settings.light_rig, camera.target, settings.light_policy);
    uniforms.lighting_params[0] = static_cast<float>(selected_lights.size());
    for (std::size_t i = 0; i < selected_lights.size(); ++i) {
      const aster::Light &light = selected_lights[i];
      uniforms.lights[i].position_radius[0] = light.position.x;
      uniforms.lights[i].position_radius[1] = light.position.y;
      uniforms.lights[i].position_radius[2] = light.position.z;
      uniforms.lights[i].position_radius[3] = light.source_radius;
      uniforms.lights[i].color_intensity[0] = light.color.x;
      uniforms.lights[i].color_intensity[1] = light.color.y;
      uniforms.lights[i].color_intensity[2] = light.color.z;
      uniforms.lights[i].color_intensity[3] = light.intensity;
    }
    void *mapped = nullptr;
    if (SUCCEEDED(scene_upload_->Map(0u, nullptr, &mapped))) {
      std::memcpy(mapped, &uniforms, sizeof(uniforms));
      scene_upload_->Unmap(0u, nullptr);
    }
  }

  std::optional<std::size_t> writeObjectUniform(
      const aster::RenderObject &object, const aster::OrbitCamera &camera,
      const aster::RendererSettings &settings, const double frame_seconds, const float opacity) {
    if (object_cursor_ >= object_capacity_) {
      return std::nullopt;
    }
    const std::size_t index = object_cursor_++;
    const aster::Mat4 model =
        settings.animate_scene && object.spin_rate != 0.0f
            ? object.transform.matrix() *
                  aster::rotation_y(static_cast<float>(frame_seconds) * object.spin_rate)
            : object.transform.matrix();
    const float aspect_ratio =
        static_cast<float>(std::max(width_, 1)) / static_cast<float>(std::max(height_, 1));
    const aster::Mat4 mvp =
        camera.projectionMatrix(aspect_ratio).value * camera.viewMatrix().value * model;
    D3D12ObjectUniforms uniforms;
    std::memcpy(uniforms.model, model.m.data(), sizeof(uniforms.model));
    std::memcpy(uniforms.model_view_projection, mvp.m.data(),
                sizeof(uniforms.model_view_projection));
    uniforms.base_color_opacity[0] = object.material.base_color.x;
    uniforms.base_color_opacity[1] = object.material.base_color.y;
    uniforms.base_color_opacity[2] = object.material.base_color.z;
    uniforms.base_color_opacity[3] = opacity;
    uniforms.emission_strength[0] = object.material.emission_color.x;
    uniforms.emission_strength[1] = object.material.emission_color.y;
    uniforms.emission_strength[2] = object.material.emission_color.z;
    uniforms.emission_strength[3] = object.material.emission_strength;
    uniforms.material_params[0] = object.material.roughness;
    uniforms.material_params[1] = object.material.metallic;
    uniforms.material_params[2] = object.material.detail_strength;
    uniforms.material_params[3] = object.material.detail_scale;
    const aster::MaterialSurfaceProfile surface_profile =
        aster::resolveMaterialSurfaceProfile(object.material);
    uniforms.pattern_params[0] =
        static_cast<float>(aster::materialSurfaceProfileId(surface_profile));
    uniforms.pattern_params[1] = object.material.pattern_scale.x;
    uniforms.pattern_params[2] = object.material.pattern_scale.y;
    uniforms.pattern_params[3] = object.material.pattern_depth;
    uniforms.pattern_params2[0] = object.material.pattern_contrast;
    uniforms.pattern_params2[1] = object.material.pattern_mortar;
    uniforms.pattern_params2[2] = object.material.ambient_occlusion;
    uniforms.pattern_params2[3] = object.material.double_sided ? 1.0f : 0.0f;
    uniforms.procedural_params[0] = object.material.procedural.macro_variation;
    uniforms.procedural_params[1] = object.material.procedural.micro_normal_strength;
    uniforms.procedural_params[2] = object.material.procedural.roughness_variation;
    uniforms.procedural_params[3] = object.material.procedural.wetness;
    uniforms.procedural_params2[0] = object.material.procedural.height_shading;
    uniforms.material_flags[0] =
        surface_profile == aster::MaterialSurfaceProfile::ContactShadow ? 1.0f : 0.0f;
    uniforms.material_flags[1] = isTerrainLayerProfile(surface_profile) ? 1.0f : 0.0f;
    uniforms.material_flags[2] =
        surface_profile == aster::MaterialSurfaceProfile::Liquid ? 1.0f : 0.0f;
    uniforms.material_flags[3] = isStructuredSurfaceProfile(surface_profile) ? 1.0f : 0.0f;

    void *mapped = nullptr;
    if (FAILED(object_upload_->Map(0u, nullptr, &mapped))) {
      return std::nullopt;
    }
    std::memcpy(static_cast<std::uint8_t *>(mapped) + index * sizeof(D3D12ObjectUniforms),
                &uniforms, sizeof(uniforms));
    object_upload_->Unmap(0u, nullptr);
    return index;
  }

  D3D12MeshBuffers *buffersForMesh(const aster::CpuMesh &mesh) {
    auto found = mesh_buffers_.find(&mesh);
    if (found != mesh_buffers_.end()) {
      return &found->second;
    }

    std::vector<D3D12Vertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const aster::Vertex &vertex : mesh.vertices) {
      vertices.push_back({{vertex.position.x, vertex.position.y, vertex.position.z, 1.0f},
                          {vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f},
                          {vertex.uv.x, vertex.uv.y, vertex.ambient_occlusion, 0.0f}});
    }
    D3D12MeshBuffers buffers;
    if (!vertices.empty()) {
      const std::size_t byte_size = vertices.size() * sizeof(D3D12Vertex);
      if (!createUploadBuffer(vertices.data(), byte_size, buffers.vertices)) {
        return nullptr;
      }
      buffers.vertex_view.BufferLocation = buffers.vertices->GetGPUVirtualAddress();
      buffers.vertex_view.SizeInBytes = static_cast<UINT>(byte_size);
      buffers.vertex_view.StrideInBytes = sizeof(D3D12Vertex);
    }
    if (!mesh.indices.empty()) {
      const std::size_t byte_size = mesh.indices.size() * sizeof(std::uint32_t);
      if (!createUploadBuffer(mesh.indices.data(), byte_size, buffers.indices)) {
        return nullptr;
      }
      buffers.index_view.BufferLocation = buffers.indices->GetGPUVirtualAddress();
      buffers.index_view.SizeInBytes = static_cast<UINT>(byte_size);
      buffers.index_view.Format = DXGI_FORMAT_R32_UINT;
      buffers.index_count = static_cast<UINT>(mesh.indices.size());
    }
    auto inserted = mesh_buffers_.emplace(&mesh, std::move(buffers));
    return &inserted.first->second;
  }

  bool createUploadBuffer(const void *source, const std::size_t byte_size,
                          ComPtr<ID3D12Resource> &resource) {
    const D3D12_HEAP_PROPERTIES heap = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC desc = bufferDesc(byte_size);
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&resource)))) {
      return false;
    }
    void *mapped = nullptr;
    if (FAILED(resource->Map(0u, nullptr, &mapped))) {
      return false;
    }
    std::memcpy(mapped, source, byte_size);
    resource->Unmap(0u, nullptr);
    return true;
  }

  void evictRetiredMeshBuffers(const aster::PreparedRenderMeshes &meshes) {
    std::unordered_set<const aster::CpuMesh *> live;
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
      } else {
        it = mesh_buffers_.erase(it);
      }
    }
  }

  void copyColorToReadback() {
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = readback_.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = readback_footprint_;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = color_.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0u;
    command_list_->CopyTextureRegion(&dst, 0u, 0u, 0u, &src, nullptr);
  }

  bool submitAndReadback() {
    if (FAILED(command_list_->Close())) {
      return false;
    }
    ID3D12CommandList *lists[] = {command_list_.Get()};
    queue_->ExecuteCommandLists(1u, lists);
    waitForGpu();

    void *mapped = nullptr;
    if (FAILED(readback_->Map(0u, nullptr, &mapped))) {
      return false;
    }
    const auto *bytes = static_cast<const std::uint8_t *>(mapped);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width_) *
                                   static_cast<std::size_t>(height_) * 4u);
    for (int y = 0; y < height_; ++y) {
      const std::uint8_t *src_row =
          bytes + readback_footprint_.Offset +
          static_cast<std::size_t>(y) * readback_footprint_.Footprint.RowPitch;
      for (int x = 0; x < width_; ++x) {
        const std::size_t src = static_cast<std::size_t>(x) * 4u;
        const std::size_t dst =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
             static_cast<std::size_t>(x)) *
            4u;
        rgba[dst + 0u] = src_row[src + 2u];
        rgba[dst + 1u] = src_row[src + 1u];
        rgba[dst + 2u] = src_row[src + 0u];
        rgba[dst + 3u] = src_row[src + 3u];
      }
    }
    readback_->Unmap(0u, nullptr);
    aster::activeFrameBuffer().replaceRgba8(width_, height_, rgba);
    return true;
  }

  void waitForGpu() {
    if (queue_ == nullptr || fence_ == nullptr || fence_event_ == nullptr) {
      return;
    }
    const UINT64 value = ++fence_value_;
    if (FAILED(queue_->Signal(fence_.Get(), value))) {
      return;
    }
    if (fence_->GetCompletedValue() < value) {
      if (SUCCEEDED(fence_->SetEventOnCompletion(value, fence_event_))) {
        WaitForSingleObject(fence_event_, INFINITE);
      }
    }
  }

  ComPtr<IDXGIFactory6> factory_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12CommandAllocator> allocator_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  ComPtr<ID3D12Fence> fence_;
  HANDLE fence_event_ = nullptr;
  UINT64 fence_value_ = 0u;

  ComPtr<ID3D12RootSignature> root_signature_;
  ComPtr<ID3D12PipelineState> opaque_pso_;
  ComPtr<ID3D12PipelineState> opaque_biased_pso_;
  ComPtr<ID3D12PipelineState> transparent_pso_;
  ComPtr<ID3D12PipelineState> transparent_biased_pso_;
  ComPtr<ID3D12DescriptorHeap> rtv_heap_;
  ComPtr<ID3D12DescriptorHeap> dsv_heap_;
  ComPtr<ID3D12Resource> color_;
  ComPtr<ID3D12Resource> depth_;
  ComPtr<ID3D12Resource> readback_;
  ComPtr<ID3D12Resource> scene_upload_;
  ComPtr<ID3D12Resource> object_upload_;
  D3D12_RESOURCE_STATES color_state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT readback_footprint_{};
  UINT readback_rows_ = 0u;
  UINT64 readback_row_bytes_ = 0u;
  int width_ = 0;
  int height_ = 0;
  std::size_t object_capacity_ = 0u;
  std::size_t object_cursor_ = 0u;
  std::unordered_map<const aster::CpuMesh *, D3D12MeshBuffers> mesh_buffers_;
};

} // namespace

namespace aster {

std::unique_ptr<NativeRenderBackend> createNativeRenderBackend() {
  return std::make_unique<D3D12NativeRenderBackend>();
}

bool captureNativeFrameToActiveFramebuffer() {
  return !activeFrameBuffer().empty();
}

void clearNativeFrame() {}

} // namespace aster

#endif
