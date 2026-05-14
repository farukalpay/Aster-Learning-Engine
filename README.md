# Aster Learning Engine

Aster Learning Engine is an educational C++20 game engine by Faruk Alpay. The
core engine, native renderer, platform adapters, networking, profiling, sample
game, editor UI, generated media, and tests live in one inspectable source tree.

Aster v1 keeps reusable code in `include/aster` and `src`, keeps executable code
thin, and limits outside boundaries to standard C++ plus direct operating-system
APIs inside isolated platform files.

## Captures

The images below are generated from the current build.

![Lumen Run gameplay](assets/screenshots/lumen_run.png)

![Lumen Run cave route](assets/screenshots/lumen_cave.png)

![Lumen Run cave interior](assets/screenshots/lumen_cave_interior.png)

![Lumen Run cave entry](assets/screenshots/lumen_cave_entry.gif)

![Lumen Run inventory](assets/screenshots/lumen_inventory.png)

![Aster Learning Studio](assets/screenshots/learning_studio.png)

![Aster preview renderer](assets/screenshots/aster_preview.png)

## What Is Included

- A macOS native Metal renderer with depth, translucent sorting, procedural
  material shading, contact shadows, fog, tonemapping, frame pacing, and UI
  composition.
- A deterministic software renderer used for fallback, capture, preview, and
  renderer diagnostics.
- Lumen Run, a playable sample that exercises terrain, castle geometry, cave
  traversal, sealed cave portals, streaming cave continuation, fixture-driven
  cave lighting, mineral formations, water, vegetation, avatar animation,
  inventory, HUD, interaction, physics, particles, and capture automation.
- Engine-owned math, mesh preparation, brush level mesh generation, procedural
  noise, scene import, scenery assembly, TCP loopback transport, and lightweight
  CPU profiling.
- Native platform adapters behind `aster::Window`; product code consumes only
  window size and input snapshots.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `apps/` | Thin executables for Lumen Run, Studio, preview, and network probe |
| `include/aster/` | Public engine headers |
| `src/` | Engine, renderer, platform, game, UI, asset, geometry, net, and core code |
| `tests/` | Unit, regression, and structure tests |
| `assets/` | Generated screenshots and README media |
| `docs/` | Architecture and research notes |

## Changelog

- Added a continuous procedural cave-mouth formation, cached cave fixture state,
  and a deterministic cave-entry capture route.
- Added sealed cave portal/throat geometry, engine-native voxel cave streaming,
  and industrial wall-light placement for darker cave traversal.
- Expanded analytic procedural material controls across terrain, grass, rock,
  cave stone, water, and avatar surfaces.
- Tightened Studio UI text fitting, framebuffer origin regression coverage, and
  README media refresh commands.

## Build

Prerequisites:

- CMake 3.24+
- A C++20 compiler
- macOS with Cocoa and Metal, or Linux with a local X server

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build directories are disposable. If CMake reports that a cache was generated
from a different source path, configure a fresh directory such as `build-release`
or remove the stale local build tree.

Run the game:

```bash
./build/aster_lumen_run
```

The desktop executables default to frame-paced interactive presentation. Pass
`--unlocked` only when measuring raw throughput or debugging the render loop.

Run the studio:

```bash
./build/aster_studio
```

Run smoke checks:

```bash
./build/aster_lumen_run --smoke-test
./build/aster_studio --smoke-test
./build/aster_net_probe
```

Check frame timing:

```bash
./build/aster_lumen_run --frame-report --frame-report-warmup 30 --run-frames 240 --lag-budget-ms 16.7 --window-width 1280 --window-height 720 --msaa 0
```

Set `ASTER_FORCE_SOFTWARE_RENDERER=1` to run the deterministic software fallback
on macOS.

## Refresh Screenshots

```bash
mkdir -p assets/screenshots /tmp/aster_learning_shots
mkdir -p /tmp/aster_learning_shots/cave_entry_frames
find /tmp/aster_learning_shots -type f \( -name '*.ppm' -o -name '*.png' -o -name '*.gif' \) -delete

./build/aster_lumen_run --screenshot /tmp/aster_learning_shots/lumen_run.ppm --screenshot-frame 8 --capture-hud --msaa 0 --window-width 1280 --window-height 720
./build/aster_lumen_run --screenshot /tmp/aster_learning_shots/lumen_cave.ppm --screenshot-frame 8 --msaa 0 --window-width 1280 --window-height 720 --camera-target-x 31.0 --camera-target-y 4.8 --camera-target-z -58.5 --camera-yaw-deg 0 --camera-pitch-deg 31 --camera-radius 18.8 --camera-fov-deg 32
./build/aster_lumen_run --screenshot /tmp/aster_learning_shots/lumen_cave_interior.ppm --screenshot-frame 60 --capture-route cave-entry --msaa 0 --window-width 1280 --window-height 720
./build/aster_lumen_run --screenshot /tmp/aster_learning_shots/lumen_inventory.ppm --screenshot-frame 2 --open-inventory --player-at-supply-crate --capture-hud --msaa 0 --window-width 1280 --window-height 720
./build/aster_studio --screenshot /tmp/aster_learning_shots/studio.ppm --window-width 1280 --window-height 720
./build/aster_preview --output /tmp/aster_learning_shots/preview.ppm --width 960 --height 540
./build/aster_lumen_run --capture-sequence /tmp/aster_learning_shots/cave_entry_frames --capture-frames 96 --capture-route cave-entry --msaa 0 --window-width 960 --window-height 540

sips -s format png /tmp/aster_learning_shots/lumen_run.ppm --out assets/screenshots/lumen_run.png
sips -s format png /tmp/aster_learning_shots/lumen_cave.ppm --out assets/screenshots/lumen_cave.png
sips -s format png /tmp/aster_learning_shots/lumen_cave_interior.ppm --out assets/screenshots/lumen_cave_interior.png
sips -s format png /tmp/aster_learning_shots/lumen_inventory.ppm --out assets/screenshots/lumen_inventory.png
sips -s format png /tmp/aster_learning_shots/studio.ppm --out assets/screenshots/learning_studio.png
sips -s format png /tmp/aster_learning_shots/preview.ppm --out assets/screenshots/aster_preview.png
ffmpeg -y -framerate 24 -i /tmp/aster_learning_shots/cave_entry_frames/frame_%04d.ppm -vf "fps=18,scale=720:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3" assets/screenshots/lumen_cave_entry.gif
```

On non-macOS hosts, use an equivalent PPM-to-PNG encoder for the media refresh
commands and any GIF encoder that accepts numbered PPM frames.

## Platform Targets

Aster v1 targets macOS and Linux.

- macOS uses a native Cocoa adapter for windows, events, cursor state, and
  Metal presentation.
- Linux uses a raw X11 protocol adapter implemented over POSIX sockets; no
  desktop client library is linked.
- Windows and Wayland are not v1 targets.

## Authorship

Engine-owned source files include:

```text
Author: Faruk Alpay
Do not remove this notice.
```

## License

Original Aster Learning Engine code and assets are available for educational,
non-commercial, and nonprofit use with attribution. See [LICENSE](LICENSE).
