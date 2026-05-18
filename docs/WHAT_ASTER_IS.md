# What Aster Is

Aster Learning Engine is a renderer-first educational C++20 engine laboratory.
Its purpose is to make graphics, scenes, materials, meshes, platform adapters,
and authoring pipelines inspectable.

What it is:

- A small real-time engine with a stable C kernel ABI and C++ source modules.
- A renderer laboratory with Metal, D3D12 offscreen/readback, and deterministic
  software-reference paths.
- A sample-game host: Lumen Run is content built on Aster, not the engine itself.

What it is not yet:

- A finished commercial RHI with full platform parity.
- A bindless texture/material editor stack.
- A completed Windows swapchain scene renderer.

The v1 spine is proven by contracts: backend capability tables, render graph
passes, golden software captures, native diff reports, and lab scenes.
