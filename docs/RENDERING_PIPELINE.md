# Rendering Pipeline

Aster builds a frame from engine data, not sample-specific code:

1. `Scene` owns renderable objects, materials, transforms, visibility hints, and
   optional custom meshes.
2. `RenderScene` and the Rust planner produce visible instances, draw groups,
   translucent ordering, and diagnostics.
3. `FrameIntent` records the frame extent, backend resource mask, required
   resources, UI/capture intent, and unsupported-pass policy.
4. `RenderPassRegistry` declares semantic passes, resource inputs/outputs,
   load/store/clear policy, viewport/scissor policy, queue, debug capture policy,
   and backend executor key.
5. `RenderGraphCompiler` turns the intent and pass registry into a
   `FixedRenderGraph` compiled artifact. The compiler report summarizes resource
   lifetimes, alias groups, expanded RHI barriers, descriptor requirements,
   queue ownership transfers, culled passes, and render-pass compatibility
   metadata used by pipeline cache keys.
6. Backends consume the same plan and publish `FrameStats` plus
   `FrameForensics`.
7. The certification layer validates the compiled RHI contract and records
   backend feature proofs before the frame is considered conformant.

The frame debugger is contract-first: every frame records pass stats, resource
transition traces, queue submit traces, descriptor layout hashes, pipeline cache
keys, material binding/fallback state, debug-capture declarations, resource
lifetime validation events, pass artifacts, timestamp samples, and backend
feature proofs. The
software reference path now produces checksummed RGBA captures for final color,
shadow atlas, volumetric fog, and reflection probes. Native backends that
advertise those proof resources must populate matching capture and sampling
evidence; otherwise certification records missing proof. Per-object visibility
traces and object-to-cluster membership traces are recorded alongside the
pass/resource data.

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
  capture, exact golden baseline, and reference shadow/fog/probe resource
  producers.
- Metal: native macOS scene renderer and presentation path.
- D3D12: native offscreen raster/readback path under conformance; Windows
  presentation still uses the production software path.

Use `docs/RENDERER_BACKEND_MATRIX.md` for current feature support.
