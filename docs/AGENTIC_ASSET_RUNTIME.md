# Agentic Asset Runtime

Aster's first product identity is an Agentic Asset Runtime: agent-authored asset
changes become runtime content only after they carry machine-readable proof.
The proof path ties a visual brief, source asset graph, package output, cooked
asset database, preview artifact, and final verdict into one reviewable bundle.

This does not make Aster a broad asset marketplace, a finished editor, or a
renderer-first platform. Renderer, Game SDK, and sample systems consume asset
proof; they are not the center of this identity.

## Pipe Lab Proof Run

Pipe Lab is the canonical V1 proof cartridge. It starts from
`showcases/pipe_lab/rusted_pipe.astergraph` and proves the rusted pipe through:

1. `asset-brief`: required and forbidden visual signals from the reference.
2. `graph-inspect`: graph claims, rejected signals, material params, shader keys,
   factory stages, collision proxy, LOD, and perceptual template.
3. `graph-package`: runtime `assetgraphbin` package and graph report.
4. `cook`: project asset database, manifest, diagnostics, and world-ready fate.
5. `asset-proof-run`: top-level pass/fail report over the whole bundle.

```bash
cargo run -p aster_assetc --bin aster_assetc -- asset-proof-run \
  --project showcases/pipe_lab/pipe_lab.asterproj \
  --asset asset_graph.pipe_lab.rusted_pipe \
  --reference assets/screenshots/industrial_pipe.png \
  --preview-artifact assets/screenshots/industrial_pipe.png \
  --output /tmp/aster_pipe_lab_proof \
  --output-schema
```

The command writes `brief.json`, `graph-inspect.json`, packaged graph output,
cooked asset DB output, and `proof-run.json`. A proof run passes only when the
graph claims every required signal, rejects every forbidden signal, declares the
surface stack, points to an existing preview artifact, packages successfully,
and cooks with zero errors.

## V1 Boundary

V1 validates structured proof and artifact existence. It does not perform
computer-vision scoring of the preview image. Preview generation stays explicit:
`asset-proof-run` consumes `--preview-artifact` and never launches a renderer.

The stable kernel ABI is unchanged. New public surface for this phase is the
`aster_assetc asset-proof-run` CLI report and the source Game SDK's asset
iteration review fields for presentation quality and surface stack data.
