# Aster Engine Kernel

Aster's stable public surface is `include/aster/kernel`. The rest of
`include/aster` is implementation-facing engine source used by the repository's
apps, samples, and tests; it is not the binary-stable engine API.

## Public And Internal API

The kernel API has two layers:

- `aster/kernel/abi.h` is the cross-compiler ABI substrate. It is plain C,
  fixed-layout, and contains no STL types, templates, exceptions, public C++
  class layout, or native platform handles.
- `aster/kernel/api.hpp` is a header-only C++ convenience layer compiled by the
  consumer. It may provide RAII and `Result<T>` wrappers, but it does not define
  exported binary C++ class ABI.

Internal headers under `include/aster/{ai,asset,core,geometry,input,math,net,
physics,platform,render,samples,scene,systems,ui}` are source contracts for the
current repository build. They can change when the engine implementation changes
and are deliberately not installed by the `aster_kernel` target.

`include/aster/game_sdk` is a separate public source SDK, not part of the binary
kernel ABI. It is installed as `aster::game_sdk` and owns game-authoring
documents, entity/component data, prefab data, item/material definitions, and
action graph contracts. It may evolve under source-compatibility rules without
expanding the C ABI promise described here.

## ABI Promise

The ABI promise is the exported `aster_kernel_*` symbol set plus the fixed-layout
types in `aster/kernel/abi.h`.

- `AsterAbiVersion` reports the ABI version implemented by the loaded kernel.
- ABI structs that may evolve carry `size` and `version` fields.
- Status values use fixed-width integer storage.
- New kernel capabilities must add new versioned functions or append compatible
  struct fields behind `size` and `version` checks.
- Removing or changing an exported `aster_kernel_*` symbol requires an ABI
  version change and an update to `abi/aster_kernel.symbols`.

The kernel does not promise binary stability for the existing rich C++ engine
headers. Cross-compiler stability is obtained by keeping the binary boundary in
plain C and compiling C++ wrappers in the consuming toolchain.

## Ownership And Lifetime

Kernel resources are opaque handles. A handle returned by a kernel creation
function is owned by the caller until it is passed to the matching destroy
function. ABI 2.2 makes the renderer path constructible and inspectable through
the public kernel: engine, window, scene, mesh, material, renderer, shader
artifact, and render pipeline handles can be created and destroyed through
fixed-layout C descriptors, with additive frame-forensics and backend capability
table queries for the last rendered frame:

- `AsterEngineHandle` -> `aster_kernel_engine_destroy`
- `AsterWindowHandle` -> `aster_kernel_window_destroy`
- `AsterSceneHandle` -> `aster_kernel_scene_destroy`
- `AsterRendererHandle` -> `aster_kernel_renderer_destroy`
- `AsterMeshHandle` -> `aster_kernel_mesh_destroy`
- `AsterMaterialHandle` -> `aster_kernel_material_destroy`
- `AsterShaderArtifactHandle` -> `aster_kernel_shader_destroy`
- `AsterRenderPipelineHandle` -> `aster_kernel_render_pipeline_destroy`

The remaining declared subsystem families reject uncreated handles with
`ASTER_STATUS_UNSUPPORTED` until their matching create APIs land:

- `AsterPhysicsWorldHandle` -> `aster_kernel_physics_world_destroy`
- `AsterSystemWorldHandle` -> `aster_kernel_system_world_destroy`
- `AsterSampleAppHandle` -> `aster_kernel_sample_app_destroy`

The public C++ wrappers encode those rules with move-only RAII types. Handles do
not expose native platform objects, renderer backend objects, STL containers, or
sample-owned state.

## Failure Model

Kernel ABI functions return `AsterStatus`. Exceptions never cross the public ABI
boundary. Internal exceptions are caught at the boundary and converted to
`AsterStatusCode` values. Assertions remain for tests and unreachable internal
invariants, not for recoverable public input failures.

C++ wrappers return `aster::kernel::Status` or `aster::kernel::Result<T>`.
Callers inspect status explicitly; no kernel wrapper throws as part of normal
failure reporting.

## Dependency Graph

The architectural dependency direction is:

```text
kernel ABI
  -> platform handles
  -> input snapshots and core timing
  -> scene/resource descriptions
  -> render planning and render devices
  -> geometry, physics, systems, and UI extension layers
  -> samples and apps
```

Dependencies flow downward through contracts. Platform code owns native handles.
Scene descriptions do not own platform resources. Render code consumes
renderer-facing scene packets. Geometry, physics, systems, and UI must not
depend on sample modules. Samples may be content-specific but cannot define
engine defaults by leaking state back into the kernel.

## Current Transitional State

The repository still carries broad implementation headers under `include/aster`
because the existing apps and subsystem tests compile against them. The boundary
is enforced at the build/export level first: `aster_kernel` installs only
`include/aster/kernel`, while `aster_game_sdk` installs only
`include/aster/game_sdk`. `aster_kernel` links the shared renderer/window
implementation internally, while public consumers still see only opaque handles,
status values, fixed-layout descriptors, shader compiler artifacts, and frame
stats. Future subsystem work should either stay internal, be re-exposed through
the source SDK as authoring/runtime data contracts, or be promoted through
versioned opaque handles and fixed-layout kernel contracts.
