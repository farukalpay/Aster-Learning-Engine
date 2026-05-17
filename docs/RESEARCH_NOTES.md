# Research Notes

This file records what influenced the current architecture and separates implemented
engine contracts from papers that were useful as background context.

## Rendering Research References

- [Terrain Diffusion: A Diffusion-Based Successor to Perlin Noise in Infinite, Real-Time Terrain Generation](https://arxiv.org/abs/2512.08309)
  is relevant to the cave generator because it preserves the procedural-world
  contract that large spaces must remain seed-addressable and locally
  queryable. Aster applies that as a deterministic hierarchical path field, not
  as a runtime neural dependency.
- [WorldGen: From Text to Traversable and Interactive 3D Worlds](https://arxiv.org/abs/2511.16825)
  supports keeping layout, traversability, and editable object state as separate
  concerns. The cave update follows that by keeping cave path sampling in
  geometry, chunk publication in the voxel engine, and encounter state in Lumen
  Run.
- [MultiGen: Level-Design for Editable Multiplayer Worlds in Diffusion Game Engines](https://arxiv.org/abs/2603.06679)
  is background for external world memory: player edits should mutate explicit
  state that future generation/streaming queries can observe. Aster applies that
  through voxel edits, chunk generations, and encounter teardown state rather
  than implicit visual-only changes.
- [From Visual Synthesis to Interactive Worlds](https://arxiv.org/abs/2604.23629)
  reinforces that game-ready generated content needs topology, physics, layout,
  and material contracts. The cave pass therefore changes collision streaming,
  path frames, visual publication, and creature/web behavior together.
- [A LoD of Gaussians](https://arxiv.org/abs/2507.01110)
  informs the transition from chunk partitions toward view-scheduled LOD and
  temporal coherence. Aster keeps chunk partitions, but now cross-fades retiring
  coarse proxies while full voxel meshes publish.
- [Voxel-visible Objects](https://arxiv.org/abs/2510.09081)
  is relevant to cave streaming because it treats voxel visibility as
  preprocessable state. Aster implements a lighter runtime version through
  deterministic coarse/full chunk lifecycle and predictive streaming rather than
  a full offline visibility database.
- [Occlusion Culling and LOD for Real-Time Rendering](https://arxiv.org/abs/2511.19202)
  informs the renderer direction for explicit visibility and level-of-detail
  contracts. Aster now exposes cave chunk lifecycle and render-batch kinds so
  streaming visibility can be inspected instead of hidden in product code.
- [LucidRaster: GPU Software Rasterizer for Exact Order-Independent Transparency](https://arxiv.org/abs/2405.13364)
  is relevant to future transparency work because it frames exact ordering as an
  explicit raster contract rather than a post-process approximation.
- [Software Rasterization of 2 Billion Points in Real Time](https://arxiv.org/abs/2204.01287)
  informs the direction for high-throughput software paths: batching,
  batch-level culling, and precision selection. Aster applies that at the current
  engine scale through Rust draw-key grouping, object culling, dynamic mesh
  resource cache diagnostics, and explicit LOD policy fields.
- [Infinite Photorealistic Worlds using Procedural Generation](https://arxiv.org/abs/2306.09310)
  supports the engine direction for authored procedural worlds: geometry,
  placement, and material variation should compose from deterministic generators
  rather than fixed imported scene assets.
- [Real-Time Neural Materials using Block-Compressed Features](https://arxiv.org/abs/2311.16121)
  is relevant to material contracts because it treats material appearance as a
  compact shader-decodable representation. Aster uses analytic procedural
  parameters rather than learned feature textures.
- [Procedural Generation and Rendering of Realistic, Navigable Forest Environments](https://arxiv.org/abs/2208.01471)
  supports the current direction for navigable generated scenery: natural detail
  comes from composable generation and real-time material shading, not fixed
  screenshot assets. Aster applies that principle through reusable grass and
  ground-detail scatter contracts instead of scene-specific placement lists.
- [Real-time Neural Radiance Caching for Path Tracing](https://arxiv.org/abs/2106.12372)
  is background context for making lighting caches explicit capability contracts;
  this cave pass keeps lighting deterministic and direct rather than introducing
  a learned cache.
- [Optimisations for Real-Time Volumetric Cloudscapes](https://arxiv.org/abs/1609.05344)
  is background context for bounded sample budgets. This cave pass uses explicit
  fog/atmosphere parameters rather than volumetric cloud rendering.

## Control And Scheduling References

- [Feedback Optimization for Dynamical Systems](https://arxiv.org/abs/2508.03503)
  informs the closed-loop frame-work controller: work budgets react to measured
  frame pressure, backlog, and observed work cost instead of a fixed per-frame
  rule.
- [Adaptive Real-Time Scheduling](https://arxiv.org/abs/2512.21364)
  supports the work-queue contract that separates class budgets, starvation
  handling, and telemetry from product gameplay code.
- The uploaded control/dynamics blueprint
  (`/Users/farukalpay/Downloads/cs_logic_control_dynamics_norepeat_blueprint.pdf`)
  is used as a validation guide for explicit lifecycle states and no-repeat
  publication invariants; it is not treated as a performance algorithm.

## What Is Implemented Now

- Analytic procedural geometry
- PBR-style direct lighting
- ACES tone mapping
- Inspectable render settings
- Engine-level material alpha, depth, render-queue, procedural surface, and
  camera-occlusion contracts
- Shared `SurfacePattern` procedural material behavior across the software
  renderer and native Metal backend
- Object-level camera culling, sorted translucent worksets, and frame-local
  object-uniform streaming in the native renderer
- Required Rust frame planning for renderer-facing packets, including frustum
  culling, draw-key grouping, instance grouping, translucent ordering, and
  renderer diagnostics
- Default interactive frame pacing with explicit unlocked runs
- macOS native Metal backend with depth, translucent sorting, procedural
  material shading, fog, contact shadows, and UI overlay composition
- Deterministic software renderer fallback and headless preview rendering
- Reusable generated-scenery assembly for grouped scene objects, sockets, and
  collision proxies
- Terrain-fitted cave entrance generation, continuous portal formation geometry,
  sealed portal/throat geometry, procedural cave formations, authored deep-cave
  sections, fixture-driven cave lighting, and mineral-accent
  placements driven by the cave tunnel profile
- Closed-loop work budgeting with measured class costs, backlog telemetry, and
  starvation-aware selection for interactive streaming work
- Voxel cave chunk lifecycle with deterministic render-only coarse proxies,
  full mesh/collider publication, and inspectable coarse/full snapshot stats
- Dynamic custom-mesh synchronization before every render frame, plus native
  Metal buffer eviction for retired prepared meshes
- Stable dynamic mesh resource ids/generations, renderer visibility classes,
  LOD policy fields, and frame-report counters for LOD culls, visibility hints,
  dynamic mesh usage, and dynamic mesh cache size
- Static cave section sampling, lighting probes, collision meshes, and support
  surfaces share the authored tunnel profile so traversal does not depend on
  runtime chunk publication order
- Geometry-evidence interaction targets with explicit hit distance and occlusion
  state, used by Lumen Run to avoid stale proximity targets behind cave webs
- HUD visibility policy for gameplay, pause, inventory, and defeat overlays
- Deterministic grass and ground-detail scatter for natural route edges, cave
  mouths, pebbles, leaf litter, and twig detail
- Aster-native control schemes
- Engine-owned immediate UI canvas with measured row heights, clipping, wrapped
  control labels, and studio panel scrolling
- Sampled scene coherence reports for visual/collision/navigation/material/fluid/visibility consistency
- Symbolic scene trace validation for bounded-horizon camera, path, collision, water, and affordance defects
- Exact FO([<]) model checking and quantifier-rank separation for finite symbolic
  traces. The solver uses the finite Ehrenfeucht-Fraisse game, so its
  exponential rank/trace-length cost is explicit theorem-prover behavior rather
  than a low-cost QA heuristic.
- Lumen Run gameplay loop
- Native framebuffer screenshots and deterministic cave-entry sequence capture
- Scripted frame reports for default gameplay, cave entry, deep cave,
  deep-cave stress, and Studio UI
