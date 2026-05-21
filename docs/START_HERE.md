# Start Here

Aster is a renderer contract engine. Start with the public contract and renderer
proof path, then inspect Lumen Run as one sample built on that surface.

Recommended path:

1. Run `aster_quickstart --capture /tmp/aster_quickstart.ppm` after building.
2. Read the README `First Draw` section and `SIMPLE_API.md`.
3. Read `WHAT_ASTER_IS.md`.
4. Read `PRODUCT_PATH.md`.
5. Render a lab scene with `aster_preview --scene material-lab`.
6. Read `RENDERING_PIPELINE.md` and `MATERIALS_AND_SHADERS.md`.
7. Inspect `SCENE_AND_MESH_PIPELINE.md`.
8. For agent-led content work, read `AGENT_AUTHORING.md` and generate an
   `aster_assetc agent-plan` report before editing.
9. Use `LUMEN_RUN_AS_SAMPLE.md` only after the engine loop is clear.

Core proof points:

- The same `Scene`, `Material`, mesh, camera, settings, render graph, and frame
  diagnostics feed software, Metal, and D3D12 render paths.
- `tests/golden/render/*.ppm` are deterministic software-reference baselines.
- `aster_render_backend_conformance_tests` compares native captures against the
  software reference and writes diff artifacts on mismatch.
- `aster_materialc`, `aster_texturec`, and `aster_assetc` expose material,
  texture, and bundle compiler contracts separately.
