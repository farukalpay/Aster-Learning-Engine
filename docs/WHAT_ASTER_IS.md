# What Aster Is

Aster Learning Engine is a small inspectable engine kernel with a draw-first
path and diagnostic contracts for explaining player-visible results. Its first
job is not to cover every feature expected from a commercial engine; it is to
keep the public runtime boundary narrow, testable, and understandable while the
repository proves renderer, asset, authoring, and sample workflows around it.

What it is:

- A real-time engine kernel with a stable C ABI in `include/aster/kernel`.
- A public source SDK in `include/aster/game_sdk` for game-authoring documents.
- A first-contact C++ facade in `include/aster/aster.hpp` for drawing before
  inspecting.
- A repository of internal renderer, RHI, scene, systems, asset, and sample
  modules that exercise the public contracts.
- A validation/debugging stack where forensics, backend conformance, material
  diagnostics, and frame reports explain visible behavior.

What it is not yet:

- A finished Unity/Unreal-style engine with broad editor, marketplace, and
  platform expectations.
- A finished commercial RHI with full platform parity.
- A bindless texture/material editor stack.
- A completed Windows swapchain scene renderer.
- A native HDR/MSAA renderer with GPU timestamp coverage on every backend.
- A D3D12 parity backend for shadow atlas, volumetric fog, and reflection probes.
- A gameplay feature backlog that can grow before world, backend, and asset
  contracts are proven.

## Maturity Map

| Area | Status | Notes |
| --- | --- | --- |
| Stable public contract | Product boundary | `include/aster/kernel` is the binary C ABI; `include/aster/game_sdk` is the public source SDK. |
| First-contact API | Convenience surface | `include/aster/aster.hpp` is for quick draw/capture examples before renderer inspection. |
| Internal engine modules | Repository source contracts | Renderer, RHI, scene, systems, asset runtime, geometry, UI, and sample support can evolve with the engine. |
| Validation/debug surfaces | Diagnostic product surfaces | `FrameForensics`, `WorldForensics`, GraphicsCore7, frame reports, backend conformance, and material diagnostics explain visible results. |
| Research/internal experiments | Not productized APIs | World Perception Ledger, belief/perceptual ABI work, and advanced continuity models are active research unless explicitly promoted. |
| Showcases | Examples and regression content | Lumen Run, Material Lab, Pipe Lab, and Primate Lab demonstrate and stress contracts; they do not define the engine boundary. |

The current proof stack is valuable because it protects the first user value:
draw something, inspect what happened, and trace visible behavior back to the
public contract when needed. The frozen public surface remains
`include/aster/kernel` for the C ABI and `include/aster/game_sdk` for
source-level game-authoring documents. Other headers under `include/aster` are
repository-internal source contracts until they are deliberately promoted.
