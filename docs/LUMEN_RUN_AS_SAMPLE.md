# Lumen Run As Sample

Lumen Run is a sample game built on Aster. It exists to exercise engine systems:
input, movement, camera, inventory, interaction, mining, lighting, particles,
UI, scene rebuilding, frame reports, and capture paths.
Cave illumination keeps the interior deliberately dark: low ambient, short fog,
and red industrial wall fixtures near `{1.0, 0.16, 0.08}` as the primary local
light sources. Warm beige lamp-wash decals and void-blocker boxes are not part of
the contract; attachments such as webs, wet streaks, moss, and lamp overlays use
renderer depth policy instead of overlapping geometry.

It is not the engine kernel and should not define reusable engine contracts by
itself. Reusable behavior belongs in `include/aster`, `src`, `crates`, or docs
only when it has a general API or conformance story.

Useful commands:

```bash
./build/aster_lumen_run --smoke-test --no-vsync
./build/aster_lumen_run --frame-report --run-frames 240 --window-width 1280 --window-height 720
./build/aster_lumen_run --screenshot /tmp/lumen_run.ppm --screenshot-frame 8 --capture-hud
```
