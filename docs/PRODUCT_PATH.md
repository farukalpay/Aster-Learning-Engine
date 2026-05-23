# Aster Product Path

Aster grows from the inspectable kernel outward. The product promise is not a
finished all-purpose engine; it is a narrow public runtime contract, a draw-first
entry point, and enough diagnostics to explain why a visible result happened.

The roadmap is staged so public API, internal engine work, diagnostics,
research, and samples do not blur into one claim.

## Current V1 Product

- Stable public runtime boundary: `include/aster/kernel` C ABI with opaque
  handles, fixed-layout descriptors, validation events, world/frame diagnostics,
  and install-tree consumer checks.
- Public source SDK: `include/aster/game_sdk` for project, scene, prefab,
  material, item, action graph, and agent-authoring documents.
- First-contact draw path: `include/aster/aster.hpp` and `aster_quickstart` for
  loading simple assets, drawing, capturing, and then opting into inspection.
- Renderer proof base: deterministic software reference, native Metal scene
  rendering/presentation, D3D12 offscreen/readback conformance paths, golden
  captures, frame reports, and backend capability tables.
- Asset compiler base: `aster_materialc`, `aster_texturec`, and `aster_assetc`
  for material packages, texture packages, graph/package inspection, project
  cooking, reports, and diagnostics.

## Product Boundary

The public engine contract is intentionally small:

```text
Stable ABI + Game SDK
  -> First Draw
  -> Renderer/Material/Asset Diagnostics
  -> Authoring Tooling
  -> Samples and Showcases
  -> Research Tracks
```

`AsterWorld`, `Scene`, materials, meshes, render graph output, and frame reports
are important because they let the engine explain visible behavior. External
consumers should rely on the kernel ABI, Game SDK, runtime capability tables,
frame/world diagnostic accessors, and documented compiler commands. Internal
renderer/RHI/framegraph/source headers can evolve until deliberately promoted.

## Renderer Roadmap

The renderer is a diagnostic product surface and an internal engine system, not
yet a complete commercial RHI. Backend support must stay conservative and match
[RENDERER_BACKEND_MATRIX.md](RENDERER_BACKEND_MATRIX.md).

Current priority gaps:

1. D3D12 parity for shadow atlas, volumetric fog, and reflection probes.
2. GPU timestamp queries in Metal and D3D12 for real pass timings.
3. Native HDR/MSAA proof paths.
4. Full D3D12/Windows presentation proof, including explicit successful
   swapchain present evidence.
5. Backend conformance gates for every advertised native feature.

GraphicsCore7, `FrameForensics`, resource provenance, pass cost maps, debug
captures, and backend feature proofs should remain framed as diagnostic
surfaces. They are product value when they explain a visible frame; they should
not make the README sound like every backend feature is already complete.

## Asset And Authoring Roadmap

The asset pipeline is useful today when it keeps compiler contracts visible:

- `aster_assetc graph-inspect` and `graph-package` keep `.astergraph` identity,
  dependency, procedural material IR, mesh/collision/LOD, shader key, quality,
  and diagnostic data reproducible.
- `aster_materialc` owns legacy/import `.astermat` package diagnostics and
  shader/material binding reports.
- `aster_texturec` owns role, color-space, mip/compression, and byte-cost
  diagnostics for texture packages.
- `aster_assetc cook` orchestrates projects, scenes, graphs, materials,
  textures, cooked artifacts, failure reports, and asset databases without
  hiding single-domain compiler failures.

Authoring Studio remains a product path, not a finished claim. The missing
workflow is still: open a project, browse assets, edit scene hierarchy, inspect
materials and procedural graphs, cook content, inspect dependency/errors, and
run the cooked result. Node editing, viewport authoring, prefab variants,
cooked-content UX, and diagnostics-to-preview feedback are authoring gaps until
that loop is complete.

## Samples And Showcases

Lumen Run, Material Lab, Pipe Lab, Primate Lab, screenshot galleries, and lab
scenes demonstrate or stress engine contracts. They should not define engine
architecture by accident.

Samples may prove:

- first draw and capture workflows
- material and mesh diagnostics
- generated-region and world/frame reports
- backend conformance evidence
- cooked asset placement and regression captures

Samples should not claim production-art completeness or imply that sample
gameplay rules are reusable engine APIs.

## Research Track

World transition proof is Aster's distinctive diagnostic philosophy, but some of
the current vocabulary is research/internal unless promoted through the public
contract.

Research/internal surfaces include:

- World Perception Ledger and perceptual continuity/runtime experiments
- belief/perceptual ABI work
- advanced world-causality and long-horizon sensory memory models
- player-readable frame verdict research beyond the stable diagnostic API

These systems can guide renderer, asset, and sample work, but they should be
presented as research or diagnostics until they have stable public interfaces,
clear user workflows, and acceptance tests.
