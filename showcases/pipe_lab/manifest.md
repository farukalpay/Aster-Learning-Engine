# Pipe Lab

`rusted_pipe.astergraph` is the first Agentic Asset Runtime proof cartridge for
Aster. It describes a rusted hollow pipe as an Aster-owned asset contract: pipe
body, bevel modifier, weld seams, rust and wetness masks, UV islands, LOD chain,
collision proxy, prefab variant, visual signal claims, forbidden-signal
rejections, and cook report.

The graph cooks to `assetgraphbin` and the same runtime asset is used by the
`industrial-pipe` preview scene and a small Lumen Run placement.

The canonical proof run is:

```bash
cargo run -p aster_assetc --bin aster_assetc -- asset-proof-run --project showcases/pipe_lab/pipe_lab.asterproj --asset asset_graph.pipe_lab.rusted_pipe --reference assets/screenshots/industrial_pipe.png --preview-artifact assets/screenshots/industrial_pipe.png --output /tmp/aster_pipe_lab_proof --output-schema
```

The preview remains a quality gate, not a declaration that the pipe is final
production art. Asset iterations should keep proving wall thickness, hollow rim
darkening, weld contact, pitting, axial wear, and grounded scene lighting before
using the capture as gallery material.
