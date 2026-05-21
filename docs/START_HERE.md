# Start Here

Aster is a player-observable world transition contract engine. Start with the
public `AsterWorld` contract, then follow how a world transition becomes render
extraction, frame submission, and renderer proof. Lumen Run is sample content
that proves the contract; it is not the contract root.

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

- The public root is `InputEvent -> PlayerIntent -> SimulationEpoch ->
  WorldDelta -> AnimationPose -> SensoryEvent -> VisibilitySet ->
  RenderExtraction -> FrameSubmission`.
- `AsterWorld` owns the causal substrate. `Scene`, `Material`, mesh, camera,
  settings, render graph, and frame diagnostics are render projections of that
  substrate, with direct scene-render paths kept for lab and compatibility use.
- `WorldForensics` is the primary proof surface. `FrameForensics` remains
  required, but it is linked to world transition evidence when a frame comes
  from a world extraction.
- `tests/golden/render/*.ppm` are deterministic software-reference baselines.
- `aster_render_backend_conformance_tests` compares native captures against the
  software reference and writes diff artifacts on mismatch.
- `aster_materialc`, `aster_texturec`, and `aster_assetc` expose material,
  texture, bundle, and generated-region gate contracts separately.
