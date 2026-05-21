# Material Lab

Material Lab is the renderer-facing material contract set used by README
captures and backend conformance. It is intentionally capability-focused, not
Lumen Run content or a production environment-art claim.

Scenes:

- `material-lab`: wet rock, weathered metal pipe, brushed metal, and translucent glass.
- `.astergraph` material assets: procedural wet rock, weathered metal/rust, cave
  moss, and wet decal/soot layering.
- Primary checks: procedural normals, roughness/metallic response, alpha blend sorting,
  contact shadows, fog/tone map stability, quality diagnostics, graph
  provenance, and capture readback.

The lab rig is allowed to be sterile when it is testing parameters. A material
does not become production-ready until the same response survives scene scale,
ground contact, non-grid lighting, and backend-diff review.

Preview:

```bash
./build/aster_preview --scene material-lab --output /tmp/aster_material_lab.ppm --width 1280 --height 720 --samples 2
aster_assetc cook --project showcases/material_lab/material_lab.asterproj --platform desktop --output /tmp/aster_material_lab_cooked
./build/aster_material_lab --graph /tmp/aster_material_lab_cooked/asset_graphs/asset_graph.material_lab.wet_rock.assetgraphbin --output /tmp/wet_rock_graph.ppm
```
