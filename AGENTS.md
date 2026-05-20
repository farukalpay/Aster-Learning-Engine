# Aster Agent Guide

Aster is an engine contract repo. When an agent edits it, the first job is to
preserve ownership boundaries: public kernel ABI, source Game SDK, internal
engine modules, sample content, and asset compiler code each have different
blast radii.

## First Pass

1. Check `git status --short` before editing. Other agents may be working in
   this tree.
2. Read `docs/START_HERE.md`, then `docs/AGENT_AUTHORING.md` for agent-facing
   batch rules.
3. For game/content work, generate a project plan:

```bash
cargo run -p aster_assetc --bin aster_assetc -- agent-plan --project projects/lumen_run/lumen_run.asterproj --output-schema
```

4. For visual asset work, generate an asset brief with every reference image
   before editing:

```bash
cargo run -p aster_assetc --bin aster_assetc -- asset-brief --project showcases/pipe_lab/pipe_lab.asterproj --asset asset_graph.pipe_lab.rusted_pipe --reference path/to/reference.png --output-schema
```

5. Prefer `.asterproj`, `.scene`, `.prefab`, `.material`, `.item`, `.action_graph`,
   and `.astergraph` edits before adding sample-specific runtime shortcuts.
6. Do not add third-party notice files as part of an Aster agent batch.

## Ownership

- `include/aster/kernel` is the stable C ABI. Do not change it unless the task
  explicitly asks for a kernel contract update.
- `include/aster/game_sdk` is the source-level authoring surface for projects,
  scenes, prefabs, materials, items, action graphs, and agent authoring plans.
- `src/*` engine modules own reusable behavior. Keep sample content out of
  engine defaults.
- `projects/lumen_run` is sample content. It may prove features, but it should
  not define engine architecture by accident.
- `crates/aster_assetc` and `crates/aster_content` own asset/compiler reports.
  Prefer CLI reports when an agent needs machine-readable project context.

## Validation

Use the smallest relevant proof first:

```bash
cmake --build build --target aster_game_sdk_public_consumer
ctest --test-dir build --output-on-failure -R aster_game_sdk_public_consumer
cargo test -p aster_assetc
```

For renderer, material, or asset pipeline changes, add the targeted conformance
or compiler test named in the touched module.

For visual assets, a build passing is not enough. The asset iteration report must
claim the required visual signals from the brief, reject forbidden signals, and
list the preview artifact used for review.

For cylindrical surface attachments such as industrial pipe welds, seams, rims,
and grime bands, agents must treat z-fight, floating rings, and knife-edge rims
as geometry/contact failures. Use inset geometry, small normal/depth bias, contact
skirts, and rounded rim normals before tuning color.
