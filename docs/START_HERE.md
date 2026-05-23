# Start Here

Aster is a small inspectable engine kernel with a draw-first path. Start by
drawing and capturing a frame, then move outward into the public ABI, renderer
inspection, asset tools, samples, and research-heavy proof surfaces.

## Recommended Path

1. Draw something:
   run `./build/aster_quickstart --capture /tmp/aster_quickstart.ppm`, then read
   the README `First Draw` section and [SIMPLE_API.md](SIMPLE_API.md).
2. Understand the public contract:
   read [WHAT_ASTER_IS.md](WHAT_ASTER_IS.md), then
   [ENGINE_INTERNALS/ENGINE_KERNEL.md](ENGINE_INTERNALS/ENGINE_KERNEL.md) for
   the installed C ABI boundary.
3. Inspect renderer and material behavior:
   render `./build/aster_preview --scene material-lab`, then read
   [RENDERING_PIPELINE.md](RENDERING_PIPELINE.md),
   [MATERIALS_AND_SHADERS.md](MATERIALS_AND_SHADERS.md), and
   [RENDERER_BACKEND_MATRIX.md](RENDERER_BACKEND_MATRIX.md).
4. Explore assets and samples:
   read [SCENE_AND_MESH_PIPELINE.md](SCENE_AND_MESH_PIPELINE.md),
   [PRODUCT_PATH.md](PRODUCT_PATH.md), and
   [LUMEN_RUN_AS_SAMPLE.md](LUMEN_RUN_AS_SAMPLE.md). Treat Lumen Run, Material
   Lab, Pipe Lab, and Primate Lab as showcase/regression content, not as the
   engine boundary.
5. Read proof-heavy and research surfaces last:
   `FrameForensics`, `WorldForensics`, GraphicsCore7, perception-ledger,
   belief/perceptual ABI, and world-continuity docs explain diagnostics and
   experiments after the first draw and public API boundaries are clear.

## Public API Boundaries

- Binary-stable public ABI: `include/aster/kernel`.
- Public source SDK: `include/aster/game_sdk`.
- First-contact draw facade: `include/aster/aster.hpp`.
- Internal source contracts: other `include/aster/*` modules unless a doc or
  build/export rule explicitly promotes them.

## Diagnostic And Lab Surfaces

- `FrameForensics`, `WorldForensics`, GraphicsCore7, and backend conformance
  reports are diagnostic surfaces for explaining visible behavior.
- `tests/golden/render/*.ppm` are deterministic software-reference baselines.
- `aster_materialc`, `aster_texturec`, and `aster_assetc` expose material,
  texture, bundle, project, and generated-region checks separately.
- For agent-led content work, read [AGENT_AUTHORING.md](AGENT_AUTHORING.md) and
  generate an `aster_assetc agent-plan` report before editing broad project
  content.
