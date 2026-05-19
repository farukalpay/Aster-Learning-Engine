# Aster Product Path

Aster grows as a renderer contract engine first. Gameplay and sample features do
not define the engine boundary; they consume it. The product stack is:

```text
Aster Renderer Contract
  -> Aster Asset Compilers
  -> Aster Authoring Studio
  -> Lumen Run and sample games
```

## Renderer Contract

The renderer spine is:

```text
Scene / Material / Mesh Input
  -> Frame Intent
  -> RenderGraph Compiler
  -> Resource Lifetime + Barriers + Descriptors + Pipeline Compatibility
  -> Backend Execution
  -> Frame Forensics + GPU Timings + Image Diff
```

The public runtime surface remains frozen around the kernel ABI and source Game
SDK. Internal renderer/RHI/framegraph headers can evolve, but external consumers
should learn the contract through `include/aster/kernel`, `include/aster/game_sdk`,
runtime capability tables, and frame-forensics accessors.

Backend feature support is proof-gated. A feature is supported only when the
backend supplies native work, captures or samples when required, resource
transition evidence, and conformance results. Declared graph passes without
native proof remain unsupported.

Priority order:

1. D3D12 presentation.
2. GPU timestamp queries in Metal and D3D12 for real frame timings.
3. Native HDR and MSAA proof paths.
4. D3D12 shadow atlas, volumetric fog, and reflection probe parity.
5. Backend conformance gates for every advertised feature.

## Asset Compilers

The content pipeline has three explicit compiler roles:

- `aster_materialc`: `.astermat` or material graph input to material package,
  shader variants, reflection, binding layout, preview, and diagnostics.
- `aster_texturec`: source image/KTX2 input to cooked texture, mip/compression
  profile, role/color-space validation, report, and byte-cost metadata.
- `aster_assetc`: project, scene, mesh, material, and image bundles to stable
  GUIDs, dependency graph, cooked artifacts, failure reports, and asset database.

`aster_assetc` may orchestrate material and texture compilation, but it should
not hide their contracts. Single-domain compiler failures must remain visible
and reproducible from the command line.

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

## Lumen Run

Lumen Run is the showcase. It should demonstrate that renderer and asset
contracts catch real production bugs. Cave lighting, material binding, fog,
shadow, probe, and readback issues should be investigated through frame
forensics, image diffs, resource transitions, and material binding traces rather
than by sample-specific guessing.
