# Rendering Pipeline

Aster builds a frame from engine data, not sample-specific code:

1. `Scene` owns renderable objects, materials, transforms, visibility hints, and
   optional custom meshes.
2. `RenderScene` and the Rust planner produce visible instances, draw groups,
   translucent ordering, and diagnostics.
3. `FixedRenderGraph` executes semantic passes: scene color/depth, light cull,
   shadow atlas, opaque, contact shadow, scene lighting, volumetric fog,
   reflection probe, transparent, UI composite, and capture.
4. Backends consume the same plan and publish `FrameStats` plus `FrameForensics`.

Clustered forward lighting v1 is a shared CPU-reference contract. It builds
deterministic cluster lists from the camera and `LightRig` so software, Metal,
and D3D12 can report the same visible-light budget before native GPU buffer
consumption is wired.

Camera and CPU projection code use the semantic spatial pipeline:
`WorldPoint -> ClipPoint -> NdcPoint -> ScreenPoint`, with `Viewport` carrying
screen-origin convention. Kernel ABI projection entrypoints mirror that contract
as `world_to_screen`, `screen_to_world`, and `screen_to_world_ray`.

Backend roles today:

- Software reference: deterministic fallback, Linux presentation source, preview,
  capture, and exact golden baseline.
- Metal: native macOS scene renderer and presentation path.
- D3D12: native offscreen raster/readback path under conformance; Windows
  presentation still uses the production software path.

Use `docs/RENDERER_BACKEND_MATRIX.md` for current feature support.
