# Material Lab

Material Lab is the renderer-facing material contract set used by README captures
and backend conformance. It is intentionally capability-focused, not Lumen Run
content.

Scenes:

- `material-lab`: wet rock, weathered metal pipe, brushed metal, and translucent glass.
- Primary checks: procedural normals, roughness/metallic response, alpha blend sorting,
  contact shadows, fog/tone map stability, and capture readback.

Preview:

```bash
./build/aster_preview --scene material-lab --output /tmp/aster_material_lab.ppm --width 1280 --height 720 --samples 2
```
