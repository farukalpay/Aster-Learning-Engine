# Research Notes

This file records what influenced the current architecture and what is only a roadmap item. It deliberately avoids claiming that unimplemented research ideas are engine features.

## Project References

- [The Forge](https://github.com/ConfettiFX/The-Forge) is a useful reference for cross-platform rendering framework boundaries, explicit platform support, resource loading, and tooling organization.
- [Filament](https://github.com/google/filament) was used as a rendering and material-system reference, especially for PBR vocabulary, tone-management direction, and sample presentation discipline. Aster does not vendor Filament code or assets.
- Desktop window/context creation is handled by Aster's runtime windowing bridge.

## Rendering Research Direction

- [Neural Visibility Cache for Real-Time Light Sampling](https://arxiv.org/abs/2506.05930) describes an online-trained visibility cache for many-light direct illumination and notes compatibility with reservoir sampling approaches.
- [Neural Two-Level Monte Carlo Real-Time Rendering](https://arxiv.org/abs/2412.04634) frames neural incident radiance caches as an online component that can reduce noise while preserving a residual unbiased estimator.
- [Area ReSTIR](https://graphics.cs.utah.edu/research/projects/area-restir/) extends reservoir reuse into pixel and lens domains for antialiasing and depth-of-field quality.

## What Is Implemented Now

- Analytic procedural geometry
- PBR-style direct lighting
- ACES tone mapping
- Inspectable render settings
- Aster-native control schemes
- Engine-owned immediate UI canvas for studio and game HUD
- Sampled scene coherence reports for visual/collision/navigation/material/fluid/visibility consistency
- Symbolic scene trace validation for bounded-horizon camera, path, collision, water, and affordance defects
- Lumen Run gameplay loop
- Native framebuffer screenshots
- Headless preview rendering

## What Is Not Implemented Yet

- ReSTIR
- Neural visibility or radiance caches
- Temporal denoising
- GPU-driven culling
- Metal/Vulkan/D3D backends
- Full FO([<]) quantifier-rank solving. The current trace validator exposes an
  engine-facing rank proxy over its rule DSL; it is intended for QA
  classification, not as a theorem prover.

Those should enter behind explicit renderer capability contracts rather than being mixed directly into app or UI code.
