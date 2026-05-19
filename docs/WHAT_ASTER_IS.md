# What Aster Is

Aster Learning Engine is a renderer contract engine. Its purpose is to make
graphics, scenes, materials, meshes, platform adapters, and authoring pipelines
inspectable and testable through explicit contracts.

What it is:

- A small real-time engine with a stable C kernel ABI and C++ source modules.
- A renderer laboratory with Metal, D3D12 offscreen/readback, and deterministic
  software-reference paths.
- A sample-game host: Lumen Run is content built on Aster, not the engine itself.
- A product path where renderer contracts feed asset compilers, asset compilers
  feed Aster Studio, and Studio-authored content feeds samples.

What it is not yet:

- A finished commercial RHI with full platform parity.
- A bindless texture/material editor stack.
- A completed Windows swapchain scene renderer.
- A gameplay feature backlog that can grow before backend and asset contracts
  are proven.

The v1 spine is proven by contracts: backend capability tables, render graph
passes, golden software captures, native diff reports, and lab scenes.

The frozen public surface is `include/aster/kernel` for the C ABI and
`include/aster/game_sdk` for source-level game-authoring documents. Other
headers under `include/aster` are repository-internal source contracts until
they are deliberately promoted.
