# Renderer Backend Matrix

This table is the public renderer/RHI truth source. It should stay conservative:
only list a feature as supported when a backend runs it through the shared scene,
material, render graph, capture, and diagnostic contracts.

| Area | Software Reference | Metal | D3D12 | Linux Presentation |
| --- | --- | --- | --- | --- |
| Role | Deterministic reference, fallback, preview, capture | Native macOS scene renderer | Native Windows scene renderer with offscreen/readback and swapchain paths | Wayland/X11 software presentation |
| Presentation | Software framebuffer | CAMetalLayer | D3D12 swapchain when a Win32 window is bound; offscreen readback otherwise | wl_shm or raw X11 from software framebuffer |
| Scene contract | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Uses software renderer output |
| Render graph passes | Declarative registry plus compiled scheduler trace; software executes scene/lighting/contact/surface-occlusion/transparent/post/capture and reference surface-attribute/shadow/fog/probe producers | Same registry and trace; native scene/UI/capture work plus cave-conformance surface/shadow/fog/probe producers and consumers | Same registry and trace; native scene/capture work plus surface-attribute and surface-occlusion proof resources; UI composite and shadow/fog/probe resource support remain unsupported | Presents software output |
| Clustered forward lighting | CPU reference contract, deterministic cluster lists, frame-debug membership trace | CPU reference contract; GPU buffer consumption not yet wired | Cluster-selected lights are uploaded through the D3D12 scene uniform path and consumed by HLSL shading; full per-tile GPU indirection remains future work | Software reference |
| Shadow atlas | Yes: reference cascaded directional atlas, PCF shadow sampling when shadows are enabled, RGBA debug capture | Yes: native depth atlas pass with cascade viewports, caster filtering, receiver bias, PCF sampling, and GPU/readback debug capture | Not supported yet: pass may exist in the graph, but `ShadowAtlas` stays out of the D3D12 resource mask until the Windows offscreen path writes and samples it | Software contract trace |
| Surface attributes | Yes: packed normal/roughness/AO presentation resource written from the opaque surface stack and captured for proof | Yes: native proof capture and resource mask support for cave/material conformance | Yes: native offscreen proof capture and resource mask support | Software contract trace |
| Surface occlusion | Yes: deterministic horizon/cavity/contact occlusion resource with final-frame software sampling and RGBA debug capture | Yes: native proof capture, resource mask support, and conformance evidence before advertisement | Yes: native offscreen proof capture and resource mask support; shadow/fog/probe remain separate unsupported resources | Software contract trace |
| Volumetric fog injection | Yes: low-resolution integrated fog resource and RGBA debug capture; final image still uses the legacy software fog equation for golden stability | Yes: native low-resolution fog target written before final shading, sampled by the scene shader, and captured from GPU/readback data | Not supported yet: `VolumetricFog` remains out of the D3D12 resource mask until the native pass writes and feeds final shading | Software contract trace |
| Reflection probes | Yes: static local probe atlas from `Scene::reflectionProbes`, sampled by software reflections when enabled | Yes: native static local probe atlas target from `Scene::reflectionProbes`, sampled by influence radius for wet/specular response, with GPU/readback debug capture | Not supported yet: `ReflectionProbes` remains out of the D3D12 resource mask until the native atlas path lands | Software contract trace |
| Shader model | Software reference | Metal MSL scene shaders | D3D12 HLSL scene shaders | Software reference |
| Shader materials | Yes, procedural/material variants, perceptual material modulation | Yes, procedural/material variants, perceptual material modulation | Yes, conservative procedural/material variants plus perceptual material modulation in object uniforms | Software reference |
| Material texture sampling | Yes: runtime role table and CPU sampler for albedo/normal/ORM/roughness/metallic/AO/height/emissive/wetness/opacity | Yes: role table, Metal texture/sampler binding, shader sampling, fallback trace | Yes: role table, D3D12 SRV table/sampler binding, shader sampling, fallback trace | Software output |
| Instancing | No hardware instancing | Yes | Yes | Software output |
| Color formats advertised | BGRA8, RGBA8 | RGBA8, BGRA8 | BGRA8 | BGRA8/RGBA8 via software |
| Depth formats advertised | Depth32Float | Depth32Float | Depth32Float | Depth32Float via software |
| MSAA advertised | 1x only; `msaa=false` | 1x only; `msaa=false` | 1x only; `msaa=false` | 1x only |
| Storage buffers / texture arrays | Unsupported | Advertised for renderer contract work | Advertised for offscreen renderer contract work | Software output |
| HDR render targets | Software reference advertises RGBA16F scene-color contract | Unsupported until a native HDR color target is wired | Unsupported until a native HDR color target is wired | Software output |
| Blend modes advertised | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend via software |
| GPU timestamps | No native queries; pass cost map still reports CPU build time, target size, estimated bandwidth, descriptor pressure, and pipeline cache pressure | No native queries yet; same pass cost map is emitted, GPU time stays unavailable until Metal timestamp sampling lands | No native queries yet; same pass cost map is emitted, GPU time stays unavailable until D3D12 timestamp sampling lands | No native queries; software cost map only |
| GraphicsCore7 role | Reference-mode player-readable truth can be accepted without claiming native GPU timestamps or presentation | Native visual-truth producer for advertised shadow/fog/probe/surface resources; timestamp signal remains unsupported until native queries land | Native offscreen/readback diagnostic producer for advertised surface resources; shadow/fog/probe/timestamp gaps reject strict GC7 truth until native proof lands | Presents software reference truth |
| Golden conformance | Exact baseline | Tolerance diff vs software | Tolerance diff vs software on Windows | Exact software baseline |

The frame-debugger truth layer is available through `FrameForensics`: pass cost
maps, resource transition traces, descriptor layout hashes, pipeline cache keys,
queue submit traces, material binding traces, debug-capture declarations, RHI
validation events, pass artifacts, timestamp samples, and backend feature
proofs. Each pass stat carries CPU build time, GPU execution time when native
timestamps exist, estimated bandwidth, render target size, draw count, material
variant count, descriptor heap pressure, and pipeline cache hit/miss totals.
Software captures include RGBA payloads and content hashes for final color,
surface attributes, surface occlusion, shadow atlas, volumetric fog, and
reflection probe resources. Metal cave conformance captures are populated from
native GPU/readback payloads for those same resources. D3D12 conformance may
advertise surface-attribute and surface-occlusion resources only after it writes
native proof captures; shadow, fog, and reflection probes stay unsupported until
their own native pass data exists. Object visibility, object-to-cluster
membership traces, and last-frame perceptual primitive traces are recorded for
frame-debugger queries.

Backend feature support is certification-gated. GraphicsCore7 first preflights
the compiled graph against the backend resource mask, material/shader evidence,
light clusters, shadow/probe ownership, and native-backend requirements before
encode. `BackendFeatureProof` then records state whether graph resources,
capture, texture sampling, instancing, GPU timestamps, HDR, MSAA, and
presentation were proven, not exercised, unsupported, or missing proof in the
current frame. GraphicsCore7 folds those proofs with surface captures, light
tables, shadow continuity, reflection residency, material-frequency evidence,
temporal evidence, and backend visual delta into a `PlayerReadableFrameVerdict`.
Presentation support requires a real
window/swapchain surface: software framebuffer, CAMetalLayer, or a bound D3D12
swapchain. D3D12 offscreen readback is useful capture evidence, but it does not
count as presentation proof. A bound D3D12 swapchain is marked not exercised
until `RenderDevice::present` records explicit swapchain-present evidence. The
conformance tests write per-pass certification artifacts next to image/diff
artifacts under the temporary conformance artifact directory. If a backend
advertises surface occlusion, shadow, fog, or reflection-probe graph resources in
the cave proof scene, it must produce native pass evidence, resource transitions,
debug captures, final sampling proof, and the matching GraphicsCore7 signal.
Unsupported GPU timestamps, MSAA, and native HDR stay unsupported until native
proof data exists.

Resource capability reporting is intentionally strict: if a render graph pass
declares an output that is not in a backend's `graph_resource_mask`,
`FrameForensics` emits a `CapabilityMismatch` event even when the pass itself is
present in the compiled graph. That keeps declared contracts separate from
native behavior.

RHI resource lifetime validation now runs as part of frame certification. It
reports read-before-write, missing barriers, queue ownership mismatches,
descriptor/resource mismatches, missing resources, retired resource use, and
invalid graph state through the forensics/ABI detail accessors.

Capability details are available at runtime through
`aster_kernel_renderer_get_backend_capability_table`. The older
`aster_kernel_renderer_get_capabilities` flags are compatibility summaries for
current ABI consumers and editor UI.

D3D12 note: the backend has both a native offscreen rasterizer/readback path and
a Win32 swapchain path. Only an explicit successful swapchain present upgrades
presentation proof to proven. Linux presentation remains explicitly software
backed for this v1 contract.
