# Lumen Run As Sample

Lumen Run is a sample game built on Aster. It exists to exercise engine systems:
input, movement, camera, inventory, interaction, mining, lighting, particles,
UI, scene rebuilding, frame reports, and capture paths.
Cave illumination keeps the interior deliberately dark: low ambient, short fog,
and red industrial wall fixtures near `{1.0, 0.16, 0.08}` as the primary local
light sources. Warm beige lamp-wash decals, green cave shelf patches, and
void-blocker boxes are not part of the contract; attachments such as webs, wet
streaks, contact shadows, and lamp overlays use renderer depth policy instead of
overlapping geometry.

It is not the engine kernel and should not define reusable engine contracts by
itself. Reusable behavior belongs in `include/aster`, `src`, `crates`, or docs
only when it has a general API or conformance story.

Lumen Run should grow by consuming renderer and asset contracts, not by forcing
new gameplay assumptions into the engine. Before new sample-game features become
engine work, backend presentation, GPU timings, feature parity, asset compiler
diagnostics, and cooked-content workflows must stay visible and testable.

## Renderer Acceptance Route

The cave route is a renderer acceptance test before it is a game level. Each
stress station should bind a visible artifact to frame-forensics evidence:
authored scale cues and camera framing, contact shadow receivers, surface
attribute and surface-occlusion captures, shadow atlas stress with dense casters
and receiver bias checks, volumetric fog stress with low-resolution injection
and final sampling proof, wet material stress for
roughness/normal/height/wetness response, physical texel-density proof,
reflection-probe stress for local influence radius and backend diff, overdraw
stress for translucent cave detail, and streaming stress for cooked
graph/material/texture residency.

Every visual glitch should resolve into a renderer or asset bug report, not a
sample-only workaround. The report needs the capture, image diff status, pass
cost map, resource transitions, material binding trace, backend feature proof,
and asset provenance that explain why the frame failed. New gameplay routes wait
behind D3D12 presentation proof, GPU timing proof, and D3D12 shadow/fog/probe
parity.

## Classic Gauntlet

The deep cave includes a Classic Gauntlet route built from Aster-native systems
ported from the FarukAlpay source family: deterministic command/replay support,
classic actor state transitions, switch-driven doors/lifts, automap markers,
HUD alerts, and melt-wipe presentation. The route is deliberately content-owned
by Lumen Run; reusable behavior stays in `include/aster` and `src`.

Useful commands:

```bash
./build/aster_lumen_run --smoke-test --no-vsync
./build/aster_lumen_run --frame-report --run-frames 240 --window-width 1280 --window-height 720
./build/aster_lumen_run --screenshot /tmp/lumen_run.ppm --screenshot-frame 8 --capture-hud
./build/aster_lumen_run --screenshot /tmp/lumen_classic.ppm --capture-route classic-gauntlet --screenshot-frame 160 --capture-hud --msaa 0 --window-width 1280 --window-height 720
```
