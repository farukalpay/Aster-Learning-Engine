# Aster Product Path

Aster grows as a world transition contract engine first. Gameplay and sample
features do not define the engine boundary; they consume it. Renderer proof is
still mandatory, but it sits under player-observable world proof. The product
stack is:

```text
Aster World Transition Contract
  -> Aster Renderer Contract
  -> Aster Procedural Asset Graph
  -> Aster Asset Compilers
  -> Aster Authoring Studio
  -> Lumen Run and sample games
```

## World Transition Contract

The public runtime spine is:

```text
InputEvent
  -> PlayerIntent
  -> SimulationEpoch
  -> WorldDelta
  -> AnimationPose
  -> SensoryEvent
  -> VisibilitySet
  -> RenderExtraction
  -> FrameSubmission
```

`AsterWorld` is the public ABI root for this spine. It uses the existing
world-state identity, tick, transaction, snapshot, and replay substrate for
causal evidence, while `AsterSystemWorldHandle` remains a compatibility path for
the older system-world trace surface. `Scene` is a render projection extracted
from world state; direct scene rendering remains useful for labs, backend
conformance, and compatibility, but it must declare compatibility provenance
when it is not linked to a world transition.

`WorldForensics` answers whether a region can be published to the player before
the frame is judged. It carries the world transition hash, epoch/tick evidence,
actor delta summary, generated-region gate verdict, navigation validity,
encounter/resource probe results, perceptual budget, streaming region identity,
and render extraction linkage.

Generated cave regions are proof-gated twice:

1. Cook-time `aster_assetc cook` emits a machine-readable world gate report for
   cave assets: seed, region identity, deterministic probe trace hash,
   pass/fail reasons, and navigation/resource/encounter/perceptual verdicts.
2. Runtime streaming validates candidate cave chunks before publish. Valid
   chunks become visible; failed chunks are quarantined and emit world-forensics
   validation evidence.

## Renderer Contract

The renderer spine is:

```text
Scene / Material / Mesh Input
  -> Frame Intent
  -> RenderGraph Compiler
  -> Resource Lifetime + Barriers + Descriptors + Pipeline Compatibility
  -> Backend Execution
  -> Frame Forensics Timeline + Resource Provenance Graph + Regression Lab
```

This renderer contract is now a subordinate proof surface. Every rendered frame
should carry world linkage when it came from `AsterWorld`: world transition hash,
actor-state delta hash, encounter budget/result, navigation validity, streaming
region id, and perceptual salience. Frames submitted from direct scene/lab paths
remain valid, but their provenance is compatibility scene extraction rather than
world transition extraction.

The public runtime surface remains frozen around the kernel ABI and source Game
SDK. Internal renderer/RHI/framegraph headers can evolve, but external consumers
should learn the contract through `include/aster/kernel`, `include/aster/game_sdk`,
runtime capability tables, world-forensics accessors, and frame-forensics
accessors.

Backend feature support is proof-gated. A feature is supported only when the
backend supplies native work, captures or samples when required, resource
transition evidence, and conformance results. Declared graph passes without
native proof remain unsupported.

The current renderer is still a contract-first spine. Frame forensics, resource
provenance, and RHI reports are valuable only when attached to backend work that
applies real GPU pressure. D3D12 presentation, native HDR/MSAA, GPU timestamp
queries, GPU consumption of clustered-light buffers, and D3D12 shadow/fog/probe
parity remain product gaps until the backend proves them.

The debugger is a required product surface, not a bonus overlay. Each frame must
explain visibility, material binding, light clusters, shadow, fog, probe, pass
outputs, overdraw, and fallback reasons on one timeline. Pass entries must read
as a CPU/GPU cost map: CPU build time, GPU time when sampled, bandwidth estimate,
render target size, draw count, material variant count, descriptor heap pressure,
and pipeline cache hit/miss totals. Render graph inspection must answer
provenance questions such as which producer node created a texture, which
material graph and cook report fed it, which backend fallback touched it, and
which asset hash and shader variant key identify it. Screenshot galleries are
regression labs: visuals are useful only when paired with image diff status,
backend difference, pass timing, asset hash, and shader variant evidence.

Priority order:

1. D3D12 swapchain presentation with real presentation proof; offscreen
   readback remains capture evidence only.
2. GPU timestamp queries in Metal and D3D12 for real frame timings.
3. Native HDR and MSAA proof paths.
4. D3D12 shadow atlas, volumetric fog, and reflection probe parity.
5. Backend conformance gates for every advertised feature.

## Asset Compilers

The content pipeline has three explicit compiler roles:

- `aster_assetc graph-inspect` and `graph-package`: `.astergraph` input to
  `assetgraphbin`, stable graph GUID/node IDs, dependency edges, procedural
  material IR, mesh/collision/LOD descriptors, shader and pipeline keys, quality
  score, diagnostics, and world/frame provenance.
- `aster_materialc`: legacy `.astermat` input to material package,
  shader variants, reflection, binding layout, preview, and diagnostics.
- `aster_texturec`: source image/KTX2 input to cooked texture, mip/compression
  profile, role/color-space validation, report, and byte-cost metadata.
- `aster_assetc cook`: project, scene, mesh, graph, material, and image bundles
  to stable GUIDs, dependency graph, cooked artifacts, cave world-gate reports,
  failure reports, and asset database.

`aster_assetc` may orchestrate graph, material, and texture compilation, but it
should not hide their contracts. Single-domain compiler failures must remain
visible and reproducible from the command line. `.astergraph` is the canonical
full asset graph format; `.astermat` remains the legacy/import material path.

The V1 authoring kernel is deliberately bounded but end to end. `pipe_lab`
is the first runtime asset-contract proof: a rusted pipe graph must produce
mesh parts, material masks, UV islands, LODs, collision proxy metadata, cook
diagnostics, and a live Lumen Run placement from the same Aster-owned asset
contract.

```text
Procedural Graph
  -> Mesh Generator / Descriptor
  -> UV / Tangent / Lightmap Policy
  -> Material Graph
  -> Collision / Gameplay Tags
  -> LOD / Impostor / Proxy
  -> Prefab Variant
  -> Cooked Runtime Asset
  -> Frame Forensics
```

Material Lab is the acceptance gate for V1 contracts. It can prove texture
roles, procedural material IR, debug views, and backend traces, but a sterile
lab rig is not the same thing as production art in a lived scene. Complex mesh
operators can start as deterministic descriptors plus diagnostics, but
graph-authored materials must execute through renderer-facing procedural IR and
trace back to graph GUID, node ID, shader variant, pipeline key, backend
capability, and fallback/degradation reason.

## Authoring Studio

Aster Studio is a production authoring shell, not a sample-game editor. The
first complete workflow is:

1. Open a project and browse assets.
2. Author a cave scene with hierarchy and inspector views.
3. Add a pipe mesh/prefab and assign a rust/wet material.
4. Edit material nodes and inspect compiler diagnostics.
5. Generate collision and prefab variants.
6. Cook the project and inspect dependency/error reports.
7. Run the cooked result in Lumen Run.

Required Studio surfaces are asset browser, outliner, inspector, viewport gizmo,
material node editor, procedural mesh graph, prefab authoring, cook button,
dependency viewer, and error panel.

Studio adoption comes after the graph package, diagnostics, and renderer traces
are stable. Node edits should update both preview output and frame forensics so
authors can see draw count, overdraw, normal aliasing, probe coverage, backend
fallback, and quality-score changes from the same graph.

## Lumen Run

Lumen Run is the showcase. It should demonstrate that renderer and asset
contracts are downstream of world proof. A generated cave region must pass
cook/runtime world gates before its lighting, material binding, fog, shadow,
probe, and readback issues are investigated through frame forensics, image
diffs, resource transitions, and material binding traces. Sample-specific
guessing is still a bug smell; the new first question is whether the world
transition was valid for the player before the frame was captured.
