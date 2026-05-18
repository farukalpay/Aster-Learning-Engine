# Start Here

Aster is a renderer-first educational C++20 engine laboratory. Start with the
engine surface, then inspect Lumen Run as one sample built on that surface.

Recommended path:

1. Read `WHAT_ASTER_IS.md`.
2. Render a lab scene with `aster_preview --scene material-lab`.
3. Read `RENDERING_PIPELINE.md` and `MATERIALS_AND_SHADERS.md`.
4. Inspect `SCENE_AND_MESH_PIPELINE.md`.
5. Use `LUMEN_RUN_AS_SAMPLE.md` only after the engine loop is clear.

Core proof points:

- The same `Scene`, `Material`, mesh, camera, settings, render graph, and frame
  diagnostics feed software, Metal, and D3D12 render paths.
- `tests/golden/render/*.ppm` are deterministic software-reference baselines.
- `aster_render_backend_conformance_tests` compares native captures against the
  software reference and writes diff artifacts on mismatch.
