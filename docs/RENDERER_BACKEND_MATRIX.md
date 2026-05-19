# Renderer Backend Matrix

This table is the public renderer/RHI truth source. It should stay conservative:
only list a feature as supported when a backend runs it through the shared scene,
material, render graph, capture, and diagnostic contracts.

| Area | Software Reference | Metal | D3D12 | Linux Presentation |
| --- | --- | --- | --- | --- |
| Role | Deterministic reference, fallback, preview, capture | Native macOS scene renderer | Native offscreen raster/readback under conformance | Wayland/X11 software presentation |
| Presentation | Software framebuffer | CAMetalLayer | None yet; readback/capture only | wl_shm or raw X11 from software framebuffer |
| Scene contract | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Uses software renderer output |
| Render graph passes | Declarative registry plus compiled scheduler trace; software executes scene/lighting/contact/transparent/post/capture and reference shadow/fog/probe producers | Same registry and trace; native scene/UI/capture work plus cave-conformance shadow/fog/probe producers and consumers | Same registry and trace; native offscreen/capture work; no UI composite, swapchain presentation, or shadow/fog/probe resource support yet | Presents software output |
| Clustered forward lighting | CPU reference contract, deterministic cluster lists, frame-debug membership trace | CPU reference contract; GPU buffer consumption not yet wired | CPU reference contract in offscreen conformance; GPU buffer consumption not yet wired | Software reference |
| Shadow atlas | Yes: reference cascaded directional atlas, PCF shadow sampling when shadows are enabled, RGBA debug capture | Yes: native depth atlas pass with cascade viewports, caster filtering, receiver bias, PCF sampling, and GPU/readback debug capture | Not supported yet: pass may exist in the graph, but `ShadowAtlas` stays out of the D3D12 resource mask until the Windows offscreen path writes and samples it | Software contract trace |
| Volumetric fog injection | Yes: low-resolution integrated fog resource and RGBA debug capture; final image still uses the legacy software fog equation for golden stability | Yes: native low-resolution fog target written before final shading, sampled by the scene shader, and captured from GPU/readback data | Not supported yet: `VolumetricFog` remains out of the D3D12 resource mask until the native pass writes and feeds final shading | Software contract trace |
| Reflection probes | Yes: static local probe atlas from `Scene::reflectionProbes`, sampled by software reflections when enabled | Yes: native static local probe atlas target from `Scene::reflectionProbes`, sampled by influence radius for wet/specular response, with GPU/readback debug capture | Not supported yet: `ReflectionProbes` remains out of the D3D12 resource mask until the native atlas path lands | Software contract trace |
| Shader model | Software reference | Metal MSL scene shaders | D3D12 HLSL scene shaders | Software reference |
| Shader materials | Yes, procedural/material variants | Yes, procedural/material variants | Yes, conservative procedural/material variants | Software reference |
| Material texture sampling | Yes: runtime role table and CPU sampler for albedo/normal/ORM/roughness/metallic/AO/height/emissive/wetness/opacity | Yes: role table, Metal texture/sampler binding, shader sampling, fallback trace | Yes: role table, D3D12 SRV table/sampler binding, shader sampling, fallback trace | Software output |
| Instancing | No hardware instancing | Yes | Yes | Software output |
| Color formats advertised | BGRA8, RGBA8 | RGBA8, BGRA8 | BGRA8 | BGRA8/RGBA8 via software |
| Depth formats advertised | Depth32Float | Depth32Float | Depth32Float | Depth32Float via software |
| MSAA advertised | 1x only; `msaa=false` | 1x only; `msaa=false` | 1x only; `msaa=false` | 1x only |
| Storage buffers / texture arrays | Unsupported | Advertised for renderer contract work | Advertised for offscreen renderer contract work | Software output |
| HDR render targets | Software reference advertises RGBA16F scene-color contract | Unsupported until a native HDR color target is wired | Unsupported until a native HDR color target is wired | Software output |
| Blend modes advertised | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend via software |
| GPU timestamps | No | No | No | No |
| Golden conformance | Exact baseline | Tolerance diff vs software | Tolerance diff vs software on Windows | Exact software baseline |

The frame-debugger truth layer is available through `FrameForensics`: pass stats,
resource transition traces, descriptor layout hashes, pipeline cache keys, queue
submit traces, material binding traces, and debug-capture declarations. Software
captures include RGBA payloads and content hashes for final color, shadow atlas,
volumetric fog, and reflection probe resources. Metal cave conformance captures
are populated from native GPU/readback payloads for those same resources. Object
visibility and object-to-cluster membership traces are recorded for
frame-debugger queries.

Resource capability reporting is intentionally strict: if a render graph pass
declares an output that is not in a backend's `graph_resource_mask`,
`FrameForensics` emits a `CapabilityMismatch` event even when the pass itself is
present in the compiled graph. That keeps declared contracts separate from
native behavior.

Capability details are available at runtime through
`aster_kernel_renderer_get_backend_capability_table`. The older
`aster_kernel_renderer_get_capabilities` flags are compatibility summaries for
current ABI consumers and editor UI.

D3D12 note: the backend is a native offscreen rasterizer/readback path for scene
contract work. It is not yet the Windows presentation renderer. Windows desktop
presentation remains the Win32/GDI software path today.
