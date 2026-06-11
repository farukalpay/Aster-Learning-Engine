# Start Here

Aster is an Agentic Asset Runtime built around a small inspectable engine
kernel. Start by proving an asset or drawing a frame, then move outward into the
public ABI, renderer inspection, asset tools, samples, and research-heavy proof
surfaces.

## Recommended Path

1. Prove the hero asset:
   run `cargo run -p aster_assetc --bin aster_assetc -- asset-proof-run --project showcases/pipe_lab/pipe_lab.asterproj --asset asset_graph.pipe_lab.rusted_pipe --reference assets/screenshots/industrial_pipe.png --preview-artifact assets/screenshots/industrial_pipe.png --output /tmp/aster_pipe_lab_proof --output-schema`,
   then read [AGENTIC_ASSET_RUNTIME.md](AGENTIC_ASSET_RUNTIME.md).
2. Draw something:
   run `./build/aster_quickstart --capture /tmp/aster_quickstart.ppm`, then read
   the README `First Draw` section and [SIMPLE_API.md](SIMPLE_API.md).
3. Understand the public contract:
   read [WHAT_ASTER_IS.md](WHAT_ASTER_IS.md), then
   [ENGINE_INTERNALS/ENGINE_KERNEL.md](ENGINE_INTERNALS/ENGINE_KERNEL.md) for
   the installed C ABI boundary.
4. Inspect renderer and material behavior:
   render `./build/aster_preview --scene material-lab`, then read
   [RENDERING_PIPELINE.md](RENDERING_PIPELINE.md),
   [MATERIALS_AND_SHADERS.md](MATERIALS_AND_SHADERS.md), and
   [RENDERER_BACKEND_MATRIX.md](RENDERER_BACKEND_MATRIX.md).
5. Explore assets and samples:
   read [SCENE_AND_MESH_PIPELINE.md](SCENE_AND_MESH_PIPELINE.md),
   [PRODUCT_PATH.md](PRODUCT_PATH.md), and
   [LUMEN_RUN_AS_SAMPLE.md](LUMEN_RUN_AS_SAMPLE.md). Treat Lumen Run, Material
   Lab, Pipe Lab, and Primate Lab as showcase/regression content, not as the
   engine boundary.
6. Inspect the learning contract:
   run `./build/aster_lumen_run --learning-proof-run --learning-out /tmp/aster_lumen_live_learning`,
   then independently validate its gameplay-produced trace with
   `cargo run -p aster_assetc --bin aster_assetc -- learning-proof-run --project projects/lumen_run/lumen_run.asterproj --lesson lesson.lumen_mining --trace /tmp/aster_lumen_live_learning/trace.jsonl --output /tmp/aster_lumen_learning_proof --output-schema`,
   and read [LEARNING_CONTRACT.md](LEARNING_CONTRACT.md).
7. Inspect durable memory:
   run `cargo run -p aster_assetc --bin aster_assetc -- memory-proof-run --project projects/lumen_run/lumen_run.asterproj --policy memory.policy.lumen_mining --trace projects/lumen_run/lessons/lumen_mining.trace.jsonl --store /tmp/aster_lumen_memory.sqlite --output /tmp/aster_lumen_memory_proof --output-schema`.
   For provider-backed benchmark validation, set `ASTER_MEMORY_PROVIDER_URL`
   and auth headers, then run `memory-bench-run`; without a real provider the
   benchmark reports `blocked` and does not use a fake endpoint.
8. Read proof-heavy and research surfaces last:
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
- `aster_lumen_run --learning-proof-run`, `lesson-inspect`, `learning-proof-run`, `memory-proof-run`,
  `memory-bench-run`, and `memory-bench-compare` expose objective,
  learner-state, scaffold, intervention, safety, workflow, typed trace, SQLite
  memory graph, provider artifact, ablation, and regression replay proof for
  learning content.
- For agent-led content work, read [AGENT_AUTHORING.md](AGENT_AUTHORING.md) and
  generate an `aster_assetc agent-plan` report before editing broad project
  content.
