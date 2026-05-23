# Agent Authoring

Aster now has an agent-facing authoring contract in the Game SDK and the asset
compiler. The goal is to let an agent grow a game project in explicit batches
without losing the connection between project files, scene documents, runtime
systems, renderer proof, and validation.

## CLI Plan

Generate a machine-readable plan before broad content work:

```bash
cargo run -p aster_assetc --bin aster_assetc -- agent-plan --project projects/lumen_run/lumen_run.asterproj --output-schema
```

The report includes:

- project identity, root, startup scene, and asset rows
- writable content scopes grouped by Aster domain
- repository license/header policy for new source files
- current dirty-worktree status and ownership-boundary reminders
- recommended batches for foundation, content surface, gameplay systems, and
  proof/handoff
- validation commands that keep work tied to Game SDK and asset compiler proof
- a JSON schema for batch reports

Agent maintenance commands:

```bash
cargo run -p aster_assetc --bin aster_assetc -- agent-audit --repo . --json
cargo run -p aster_assetc --bin aster_assetc -- agent-fix-headers --repo . --check
cargo run -p aster_assetc --bin aster_assetc -- agent-native-audit --repo . --markdown --output /tmp/aster-native-audit.md
cargo run -p aster_assetc --bin aster_assetc -- agent-runtime-audit --repo . --markdown
```

When an agent creates engine source, it must add the SPDX/copyright header for
the core license. Content files under projects, showcases, screenshots, and
branded sample assets stay under the Aster content license unless a file says
otherwise.

For visual asset work, generate a stricter asset brief before implementation:

```bash
cargo run -p aster_assetc --bin aster_assetc -- asset-brief --project showcases/pipe_lab/pipe_lab.asterproj --asset asset_graph.pipe_lab.rusted_pipe --reference path/to/industrial_pipe.png --output-schema
```

That report treats reference images as visual contracts. It names required
signals, forbidden signals, a minimum visual score, candidate artifacts, and the
structured iteration report that the agent must return. A successful build does
not pass the asset if the brief says the candidate is missing required visual
signals.

For the canonical Pipe Lab proof cartridge, run the full proof bundle after the
brief and asset edit:

```bash
cargo run -p aster_assetc --bin aster_assetc -- asset-proof-run --project showcases/pipe_lab/pipe_lab.asterproj --asset asset_graph.pipe_lab.rusted_pipe --reference assets/screenshots/industrial_pipe.png --preview-artifact assets/screenshots/industrial_pipe.png --output /tmp/aster_pipe_lab_proof --output-schema
```

The proof run writes `brief.json`, `graph-inspect.json`, packaged graph output,
cooked asset DB output, and `proof-run.json`. It consumes an explicit preview
artifact and does not launch a renderer.

## C++ SDK Surface

The same contract is available through `include/aster/game_sdk`:

```cpp
const auto project = aster::sdk::loadProjectDocument("projects/lumen_run/lumen_run.asterproj");

aster::sdk::AsterAgentWorkspaceOptions options;
options.objective = "Extend the game in schema-first batches.";
options.project_file = "projects/lumen_run/lumen_run.asterproj";

const auto profile =
    aster::sdk::createAsterAgentWorkspaceProfile(project.value, "projects/lumen_run", options);
const auto audit = aster::sdk::auditAsterAgentWorkspace(project.value, profile);
const auto board = aster::sdk::planAsterAgentAuthoringBatches(project.value, profile);
const std::string prompt = aster::sdk::makeAsterAgentPrompt(profile, project.value, board);
```

Core types:

- `AsterAgentWorkspaceProfile` records objective, project root, scopes,
  validation commands, output contracts, and metadata.
- `AsterAgentWorkspaceAudit` checks duplicate ids, startup-scene ownership,
  schema version, unsafe asset paths, and missing validation.
- `AsterAgentTaskBoard` tracks tasks, dependencies, batch policy, ready/blocked
  work, markdown summaries, and deterministic contract stamps.
- `AsterAgentHandoff` keeps continuation state small enough for another agent to
  resume without rereading the entire repo.
- `AsterAgentAssetBrief` and `AsterAgentAssetReview` gate visual asset iterations
  against references, required signals, forbidden signals, and preview artifacts.

## Batch Shape

Default batches are intentionally conservative:

1. `batch.foundation` maps the project manifest and normalizes authoring scopes.
2. `batch.content_surface` handles scene, cave, prefab, material, texture, mesh,
   and procedural graph surfaces.
3. `batch.gameplay_systems` binds items, action graphs, input maps, and UI.
4. `batch.proof` runs targeted validation and emits a handoff.

Agents should mark tasks complete only after the related files and validation
notes exist. If another agent has unrelated changes, keep those changes intact
and move around them.

## Visual Asset Gate

Use an asset brief whenever the output is judged by a screenshot, preview, mesh
silhouette, material read, or reference image.

The default industrial pipe brief requires:

- orange-brown corrosion rather than a smooth black pipe
- dark oxide in cavities, rims, underside, and weld shadows
- raised weld rings and a hollow worn rim
- uneven pitting, axial scratches, and the original pipe silhouette

It forbids:

- mostly smooth black material
- extra bolts or decorative flanges when the reference does not ask for them
- clean plastic-like material response
- monochrome rust with no oxide/wear/depth variation
- hidden or missing weld bands
