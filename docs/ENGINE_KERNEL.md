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
function:

- `AsterEngineHandle` -> `aster_kernel_engine_destroy`
- `AsterWindowHandle` -> `aster_kernel_window_destroy`
- `AsterSceneHandle` -> `aster_kernel_scene_destroy`
- `AsterRendererHandle` -> `aster_kernel_renderer_destroy`
- `AsterMeshHandle` -> `aster_kernel_mesh_destroy`
- `AsterMaterialHandle` -> `aster_kernel_material_destroy`
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
is enforced at the build/export level first: only `aster_kernel` is installed and
only `include/aster/kernel` is treated as the stable public surface. Future
subsystem work should either stay internal or be re-exposed through versioned
opaque handles and fixed-layout kernel contracts.
