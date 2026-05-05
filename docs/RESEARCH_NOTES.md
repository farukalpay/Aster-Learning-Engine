# Research Notes

This file records what influenced the current architecture and what is only a roadmap item. It deliberately avoids claiming that unimplemented research ideas are engine features.

## Rendering Research References

- [LucidRaster: GPU Software Rasterizer for Exact Order-Independent Transparency](https://arxiv.org/abs/2405.13364)
  is relevant to future transparency work because it frames exact ordering as an
  explicit raster contract rather than a post-process approximation.
- [Software Rasterization of 2 Billion Points in Real Time](https://arxiv.org/abs/2204.01287)
  informs the direction for high-throughput software paths: batching,
  batch-level culling, and precision selection. Aster v1 has object-level camera
  culling; point-cloud batching and precision selection remain roadmap work.
- [Real-Time Neural Materials using Block-Compressed Features](https://arxiv.org/abs/2311.16121)
  is relevant to material contracts because it treats material appearance as a
  compact shader-decodable representation. Aster uses analytic procedural
  parameters rather than learned feature textures.
- [Procedural Generation and Rendering of Realistic, Navigable Forest Environments](https://arxiv.org/abs/2208.01471)
  supports the current direction for navigable generated scenery: natural detail
  comes from composable generation and real-time material shading, not fixed
  screenshot assets.
- [Real-time Neural Radiance Caching for Path Tracing](https://arxiv.org/abs/2106.12372)
  remains a roadmap reference for dynamic indirect-light caches; no neural cache
  is implemented in Aster v1.
- [Optimisations for Real-Time Volumetric Cloudscapes](https://arxiv.org/abs/1609.05344)
  is a roadmap reference for lower-sample volumetric atmosphere work.

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
- Default interactive frame pacing with explicit unlocked runs
- macOS native Metal backend with depth, translucent sorting, procedural
  material shading, fog, contact shadows, and UI overlay composition
- Deterministic software renderer fallback and headless preview rendering
- Reusable generated-scenery assembly for grouped scene objects, sockets, and
  collision proxies
- Terrain-fitted cave entrance generation, portal blend geometry, procedural
  cave formations, and mineral-accent placements driven by the cave tunnel
  profile
- Aster-native control schemes
- Engine-owned immediate UI canvas with clipping and studio panel scrolling
- Sampled scene coherence reports for visual/collision/navigation/material/fluid/visibility consistency
- Symbolic scene trace validation for bounded-horizon camera, path, collision, water, and affordance defects
- Lumen Run gameplay loop
- Native framebuffer screenshots and deterministic cave-entry sequence capture

## What Is Not Implemented Yet

- ReSTIR
- Neural visibility or radiance caches
- Temporal denoising
- GPU-driven culling
- Linux GPU shader rendering
- Volumetric clouds
- Full FO([<]) quantifier-rank solving. The current trace validator exposes an
  engine-facing rank proxy over its rule DSL; it is intended for QA
  classification, not as a theorem prover.

Those should enter behind explicit renderer capability contracts rather than being mixed directly into app or UI code.
