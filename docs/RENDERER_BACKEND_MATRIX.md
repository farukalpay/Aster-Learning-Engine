# Renderer Backend Matrix

This table is the public renderer/RHI truth source. It should stay conservative:
only list a feature as supported when a backend runs it through the shared scene,
material, render graph, capture, and diagnostic contracts.

| Area | Software Reference | Metal | D3D12 | Linux Presentation |
| --- | --- | --- | --- | --- |
| Role | Deterministic reference, fallback, preview, capture | Native macOS scene renderer | Native offscreen raster/readback under conformance | Wayland/X11 software presentation |
| Presentation | Software framebuffer | CAMetalLayer | None yet; readback/capture only | wl_shm or raw X11 from software framebuffer |
| Scene contract | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Shared `Scene` and `FrameRenderPlan` | Uses software renderer output |
| Render graph passes | Scene, opaque, contact shadow, transparent, UI, capture | Scene, opaque, contact shadow, transparent, UI, capture | Scene, opaque, contact shadow, transparent, capture readback; no UI composite yet | Presents software output |
| Shader model | Software reference | Metal MSL scene shaders | D3D12 HLSL scene shaders | Software reference |
| Shader materials | Yes, procedural/material variants | Yes, procedural/material variants | Yes, conservative procedural/material variants | Software reference |
| Material texture sampling | No scene material texture binding yet | No scene material texture binding yet | No scene material texture binding yet | No scene material texture binding yet |
| Instancing | No hardware instancing | Yes | Yes | Software output |
| Color formats advertised | BGRA8, RGBA8 | RGBA8, BGRA8 | BGRA8 | BGRA8/RGBA8 via software |
| Depth formats advertised | Depth32Float | Depth32Float | Depth32Float | Depth32Float via software |
| MSAA advertised | 1x only | 1x only | 1x only | 1x only |
| Blend modes advertised | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend | Opaque, alpha blend via software |
| GPU timestamps | No | No | No | No |
| Golden conformance | Exact baseline | Tolerance diff vs software | Tolerance diff vs software on Windows | Exact software baseline |

Capability details are available at runtime through
`aster_kernel_renderer_get_backend_capability_table`. The older
`aster_kernel_renderer_get_capabilities` flags are compatibility summaries for
current ABI consumers and editor UI.

D3D12 note: the backend is a native offscreen rasterizer/readback path for scene
contract work. It is not yet the Windows presentation renderer. Windows desktop
presentation remains the Win32/GDI software path today.
