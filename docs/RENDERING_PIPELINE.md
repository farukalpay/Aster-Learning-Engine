# Rendering Pipeline

Aster builds a frame from engine data, not sample-specific code:

1. `Scene` owns renderable objects, materials, transforms, visibility hints, and
   optional custom meshes.
2. `RenderScene` and the Rust planner produce visible instances, draw groups,
   translucent ordering, and diagnostics.
3. `FixedRenderGraph` executes semantic passes: scene color/depth, opaque,
   contact shadow, transparent, UI composite, and capture.
4. Backends consume the same plan and publish `FrameStats` plus `FrameForensics`.

Backend roles today:

- Software reference: deterministic fallback, Linux presentation source, preview,
  capture, and exact golden baseline.
- Metal: native macOS scene renderer and presentation path.
- D3D12: native offscreen raster/readback path under conformance; Windows
  presentation still uses the production software path.

Use `docs/RENDERER_BACKEND_MATRIX.md` for current feature support.
