# Lumen Run As Sample

Lumen Run is a sample game built on Aster. It exists to exercise engine systems:
input, movement, camera, inventory, interaction, mining, generated cave gates,
lighting, particles, UI, scene rebuilding, world/frame reports, and capture
paths.
Cave illumination keeps the interior deliberately dark: low ambient, short fog,
and red industrial wall fixtures near `{1.0, 0.16, 0.08}` as the primary local
light sources. Warm beige lamp-wash decals, green cave shelf patches, and
void-blocker boxes are not part of the contract; attachments such as webs, wet
streaks, contact shadows, and lamp overlays use renderer depth policy instead of
overlapping geometry.

It is not the engine kernel and should not define reusable engine contracts by
itself. Reusable behavior belongs in `include/aster`, `src`, `crates`, or docs
only when it has a general API or conformance story.

Lumen Run should grow by consuming world, renderer, and asset contracts, not by
forcing new gameplay assumptions into the engine. Before new sample-game
features become engine work, generated-region validity, backend presentation,
GPU timings, feature parity, asset compiler diagnostics, and cooked-content
workflows must stay visible and testable.

## World Gate Route

The cave route is a world gate before it is a renderer acceptance route. The
first proof asks whether the player can be spawned, move through the generated
region, reach intended resource/encounter affordances, and receive enough
perceptual signal for light/fog/wetness to affect decisions. That proof now
runs through the World Perception Ledger: each cave cell must carry material
memory, contact history, lighting exposure, atmosphere membership, occlusion
role, gameplay affordance, wear continuity, semantic LOD, and audio/visual cue
budget before it is publishable. The ledger remains deterministic evidence; the
source-level `PerceptualWorldRuntime` is the continuous player-facing regime
above it. Runtime cave updates accumulate exposure, material memory,
interaction residue, traversal pressure, lighting believability, occlusion
trust, ecology signal, player-readable cause, and continuity debt before the
frame is extracted. `aster_assetc cook` emits the ledger block, the compatibility
perceptual-continuity aggregate, and the authored perceptual-runtime contract
for cave inputs, and runtime streaming validates candidate generated chunks
before they are published.

After that gate passes, each stress station binds a visible artifact to
frame-forensics evidence: authored scale cues and camera framing, contact shadow
receivers, surface attribute and surface-occlusion captures, shadow atlas stress
with dense casters and receiver bias checks, volumetric fog stress with
low-resolution injection and final sampling proof, wet material stress for
roughness/normal/height/wetness response, physical texel-density proof,
reflection-probe stress for local influence radius and backend diff, overdraw
stress for translucent cave detail, and streaming stress for cooked
graph/material/texture residency.

Every visual glitch should resolve into a renderer or asset bug report, not a
sample-only workaround. Every world glitch should resolve into a world-gate,
perception-ledger, or authoring bug report, not a prettier frame. The report
needs the region id, probe trace hash, nav/resource/encounter/perceptual
verdicts, ledger hash, ledger cell count, world transition hash, capture, image
diff status, pass cost map, resource transitions, material binding trace,
backend feature proof, object perception traces, and asset provenance that
explain why the player-visible result failed.
For long-horizon cave reports, it also needs the perceptual runtime state hash,
semantic budget hash, continuity debt, and accepted/rejected runtime verdict so
authors can see whether the space still carries the player's prior behavior.

## Classic Gauntlet

The deep cave includes a Classic Gauntlet route built from Aster-native systems
ported from the FarukAlpay source family: deterministic command/replay support,
classic actor state transitions, switch-driven doors/lifts, automap markers,
HUD alerts, and melt-wipe presentation. The route is deliberately content-owned
by Lumen Run; reusable behavior stays in `include/aster` and `src`.

Useful commands:

```bash
./build/aster_lumen_run --validate-cave
./build/aster_lumen_run --smoke-test --no-vsync
./build/aster_lumen_run --frame-report --run-frames 240 --window-width 1280 --window-height 720
./build/aster_lumen_run --screenshot /tmp/lumen_run.ppm --screenshot-frame 8 --capture-hud
./build/aster_lumen_run --screenshot /tmp/lumen_classic.ppm --capture-route classic-gauntlet --screenshot-frame 160 --capture-hud --msaa 0 --window-width 1280 --window-height 720
```
