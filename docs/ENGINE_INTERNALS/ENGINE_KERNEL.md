# Aster Engine Kernel

Aster's stable public surface is frozen at `include/aster/kernel`. The rest of
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

ABI 6 promotes `AsterWorldHandle` as the public root. The kernel now centers the
player-observable proof chain:

```text
InputEvent
  -> PlayerIntent
  -> SimulationEpoch
  -> WorldDelta
  -> AnimationPose
  -> SensoryEvent
  -> VisibilitySet
  -> RenderExtraction
  -> FrameSubmission
```

`AsterWorld` uses the same causal substrate introduced for system world state:
generational entity identity, monotonic simulation ticks, append-only
transactions, declared component read/write access, world trace events,
snapshot migration reports, replay reports, and hashes that connect rendered
frames back to world transitions. `AsterSystemWorldHandle` remains a
compatibility route for that older trace surface. ABI 6 also keeps the spatial
math and explicit GPU resource lifecycle contracts: textures, render targets,
buffers, descriptor heaps/sets, pipeline caches, and frame schedules are opaque
kernel handles with fixed-layout descriptors and queryable validation events.
This is not a gameplay framework; behavior composition, quest logic, UI policy,
and sample rules stay above the kernel. The installed contract is proven by
`external_app_minimal/`, which is configured only with
`find_package(AsterKernel CONFIG REQUIRED)` from an install prefix.

ABI 4 promoted spatial math through typed fixed-layout structs:
`AsterWorldPoint`, `AsterScreenPoint`, `AsterWorldRay`, `AsterViewport`, and
`AsterProjectionConvention`. The old ambiguous math projection calls were
replaced by `aster_kernel_math_world_to_screen`,
`aster_kernel_math_screen_to_world`, and
`aster_kernel_math_screen_to_world_ray` so public consumers cross the same
world/clip/NDC/screen boundary as the C++ camera and renderer code.
Math contract failures remain C++-first, but the frame-forensics ABI now has
diagnostic kinds for math contracts, non-finite world matrices, singular normal
matrices, negative-scale tangent flips, projection/backend convention drift, and
robust predicate uncertainty.

ABI 7.1 keeps that binary boundary and expands proof access without renumbering
existing enums or handles. Public consumers can now query fixed-layout rows for
the frame debugger timeline, material binding traces, asset frame traces,
resource provenance, regression gallery entries, pipeline signature evidence,
and material residency evidence. `AsterFrameForensicsDetailCounts` appends the
matching row counts behind `size` checks, so older consumers continue to see the
ABI 7.0 prefix while newer consumers can walk the full renderer proof model.
The C++ wrapper in `aster/kernel/api.hpp` mirrors those accessors as convenience
methods only; ownership, allocation, and native backend objects remain internal.

## Ownership And Lifetime

Kernel resources are opaque handles. A handle returned by a kernel creation
function is owned by the caller until it is passed to the matching destroy
function. ABI 6 makes the world root, renderer, and explicit RHI lifecycle
constructible and inspectable through the public kernel: engine, world, window,
scene, mesh, material, texture, render target, buffer, descriptor heap/set,
pipeline cache, renderer, shader artifact, render pipeline, and frame schedule
handles are created and destroyed through fixed-layout C descriptors, with
world-forensics, frame-forensics, schedule, validation, and backend capability
queries for the last rendered frame:

- `AsterEngineHandle` -> `aster_kernel_engine_destroy`
- `AsterWorldHandle` -> `aster_kernel_world_destroy`
- `AsterWindowHandle` -> `aster_kernel_window_destroy`
- `AsterSceneHandle` -> `aster_kernel_scene_destroy`
- `AsterRendererHandle` -> `aster_kernel_renderer_destroy`
- `AsterMeshHandle` -> `aster_kernel_mesh_destroy`
- `AsterMaterialHandle` -> `aster_kernel_material_destroy`
- `AsterTextureHandle` -> `aster_kernel_texture_destroy`
- `AsterRenderTargetHandle` -> `aster_kernel_render_target_destroy`
- `AsterBufferHandle` -> `aster_kernel_buffer_destroy`
- `AsterDescriptorHeapHandle` -> `aster_kernel_descriptor_heap_destroy`
- `AsterDescriptorSetHandle` -> `aster_kernel_descriptor_set_destroy`
- `AsterPipelineCacheHandle` -> `aster_kernel_pipeline_cache_destroy`
- `AsterShaderArtifactHandle` -> `aster_kernel_shader_destroy`
- `AsterRenderPipelineHandle` -> `aster_kernel_render_pipeline_destroy`
- `AsterFrameScheduleHandle` -> `aster_kernel_frame_schedule_destroy`
- `AsterSystemWorldHandle` -> `aster_kernel_system_world_destroy`

The remaining declared subsystem families reject uncreated handles with
`ASTER_STATUS_UNSUPPORTED` until their matching create APIs land:

- `AsterPhysicsWorldHandle` -> `aster_kernel_physics_world_destroy`
- `AsterSampleAppHandle` -> `aster_kernel_sample_app_destroy`

The public C++ wrappers encode those rules with move-only RAII types. Handles do
not expose native platform objects, renderer backend objects, STL containers, or
sample-owned state.

## Failure Model

Kernel ABI functions return `AsterStatus`. Exceptions never cross the public ABI
boundary. Internal exceptions are caught at the boundary and converted to
`AsterStatusCode` values. Assertions remain for tests and unreachable internal
invariants, not for recoverable public input failures.

ABI 6 makes strict validation the default for public misuse. Non-finite or
zero-scale transforms, invalid custom mesh spans, bad texture role/color-space
declarations, DirectX normal convention in LitPBR bindings, missing
required albedo/normal/ORM roles, destroyed public handles, render-target
format/sample mismatches, unsupported backend resources, and capture before a
rendered frame return `ASTER_STATUS_VALIDATION_ERROR`,
`ASTER_STATUS_CAPABILITY_MISMATCH`, or `ASTER_STATUS_LIFETIME_ERROR`. The caller
can inspect structured `AsterValidationEvent` records through engine, renderer,
or frame-schedule accessors after the failed call.

C++ wrappers return `aster::kernel::Status` or `aster::kernel::Result<T>`.
Callers inspect status explicitly; no kernel wrapper throws as part of normal
failure reporting.

## Dependency Graph

The architectural dependency direction is:

```text
kernel ABI
  -> platform handles
  -> input snapshots and core timing
  -> world state, transactions, replay trace, and generated-region gates
  -> scene/resource render projections
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
status values, fixed-layout descriptors, shader compiler artifacts, validation
events, world forensics, render targets/captures, frame stats, frame schedules,
and ABI 7.1 renderer proof rows. The
install-tree smoke test builds `external_app_minimal/` from the installed
`aster::kernel` target and verifies private implementation header directories
are not installed. Future subsystem work should either stay internal, be
re-exposed through the source SDK as authoring/runtime data contracts, or be
promoted through versioned opaque handles and fixed-layout kernel contracts.
World-root promotion follows that same rule: the kernel owns causal identity,
time, validation, trace, generated-region gate evidence, and render extraction
linkage, while game production semantics remain in Game SDK documents, systems
modules, editor tooling, and product code.
