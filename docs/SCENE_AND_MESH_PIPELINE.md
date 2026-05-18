# Scene And Mesh Pipeline

Scene data is deliberately plain:

- `RenderObject` carries transform, material, primitive/custom mesh, visibility
  hints, contact-shadow policy, and optional dynamic mesh identity.
- Built-in primitives cover boxes, spheres, planes, rocks, crystals, ruin blocks,
  and pillars.
- Custom mesh helpers build terrain, caves, tubes, cables, fractured pieces,
  water, architectural pieces, vegetation, projected meshes, and generated
  scenery.

The showcase labs are the first learning path:

- `material-lab`: material variety under one render contract.
- `mesh-lab`: procedural/custom mesh variety.
- `lighting-lab`: direct light, contact shadows, emissive, fog, and tone map.
- `scene-lab`: multi-object instancing/visibility diagnostic scene.

Run them with `./build/aster_preview --scene <name> --output /tmp/<name>.ppm`.
