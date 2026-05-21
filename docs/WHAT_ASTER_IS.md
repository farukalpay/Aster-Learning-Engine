# What Aster Is

Aster Learning Engine is a player-observable world transition contract engine.
Its purpose is to make input intent, simulation epochs, world deltas,
animation/sensory consequences, visibility, render extraction, frame submission,
and authoring pipelines inspectable and testable through explicit contracts.

What it is:

- A small real-time engine with a stable C kernel ABI and C++ source modules.
- A world-root kernel where `AsterWorld` is the public ABI root and `Scene` is a
  renderable projection, not the source of truth.
- A renderer laboratory with Metal, D3D12 offscreen/readback, and deterministic
  software-reference paths connected to world-transition evidence.
- A sample-game host: Lumen Run is content built on Aster, not the engine itself.
- A product path where world contracts feed renderer and asset contracts, asset
  compilers feed Aster Studio, and Studio-authored content feeds samples.

What it is not yet:

- A finished commercial RHI with full platform parity.
- A bindless texture/material editor stack.
- A completed Windows swapchain scene renderer.
- A native HDR/MSAA renderer with GPU timestamp coverage on every backend.
- A D3D12 parity backend for shadow atlas, volumetric fog, and reflection probes.
- A gameplay feature backlog that can grow before world, backend, and asset
  contracts are proven.

The current spine is proven by contracts: world transition hashes,
generated-region gates, backend capability tables, render graph passes, golden
software captures, native diff reports, and lab scenes. `FrameForensics` remains
the renderer truth layer, but `WorldForensics` is the higher proof surface for
player-observable causality.

The frozen public surface is `include/aster/kernel` for the C ABI and
`include/aster/game_sdk` for source-level game-authoring documents. Other
headers under `include/aster` are repository-internal source contracts until
they are deliberately promoted.
