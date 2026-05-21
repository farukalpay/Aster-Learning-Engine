# Simple API

The simple API is Aster's draw-first surface. It is meant for the first hour:
open Aster, load a mesh, load a material, draw a light, render a frame. It does
not replace the renderer contracts; it keeps them out of the way until debugging,
profiling, conformance, or asset-pipeline work needs them.

```cpp
#include "aster/aster.hpp"

int main() {
  using namespace aster;

  InitAster(1280, 720, "Aster");
  Camera3D cam = MakeOrbitCamera({0.0f, 0.58f, 0.0f}, 5.0f, 64.0f, 15.0f);
  Material rust = LoadMaterial("showcases/material_lab/weathered_metal.astermat");
  Mesh pipe = LoadMesh("showcases/pipe_lab/rusted_pipe.astergraph");

  while (Frame()) {
    BeginScene(cam);
    DrawMesh(pipe, rust);
    DrawLight({-3.6f, 3.2f, 2.4f}, {8.0f, 6.4f, 4.8f}, 1.0f, 0.8f);
    EndScene();
  }

  CloseAster();
}
```

For a headless one-frame capture, use the checked-in quickstart:

```bash
./build/aster_quickstart --capture /tmp/aster_quickstart.ppm
```

## What It Hides

- `InitAster` owns the window or headless render context, render device, default
  renderer settings, and material resource library.
- `Frame`, `BeginScene`, `DrawMesh`, `DrawLight`, and `EndScene` build a normal
  `Scene` and submit it to `RenderDevice`.
- `LoadMaterial` accepts `.astermat` files and simple names such as `rust`.
- `LoadMesh` accepts simple primitive names and `.astergraph` procedural assets
  such as the rusted pipe.
- `LastFrameStats` and `LastFrameForensics` are the escape hatch when a first
  draw becomes a renderer investigation.

Use `include/aster/render`, `include/aster/scene`, `docs/RENDERING_PIPELINE.md`,
and `docs/MATERIALS_AND_SHADERS.md` once you want pass timing, backend proof,
resource traces, material binding diagnostics, or conformance captures.
