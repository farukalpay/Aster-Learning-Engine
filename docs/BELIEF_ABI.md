# Belief ABI

Status: stable in kernel ABI 6.4.0.

Belief ABI exposes Aster's player-believable world proof as typed C data. It
stabilizes the internal Belief Extraction V1 report, the frame falseness report,
and the perceptual world scheduler summary without requiring C ABI consumers to
include C++ engine headers.

## Contract

The stable belief surface is append-only:

- `AsterBeliefFindingKind` names the canonical falseness categories.
- `AsterBeliefFindingInfo` describes one failed belief check.
- `AsterBeliefReportInfo` describes the accepted/score/hash summary and finding
  count.
- `AsterPerceptualWorldScheduleInfo` describes the runtime scheduler evidence
  that influenced memory residue, threat signal, material age, interaction debt,
  perceptual priority, streaming budget, belief stability, and frame cost.
- `AsterWorldPerceptualPrimitiveInfo` describes one extracted primitive truth
  record for world and renderer inspection.

All structs carry `size` and `version`. Callers initialize them with
`sizeof(type)` and `ASTER_KERNEL_STRUCT_VERSION_1`. Future fields must be added
only at the tail and must be guarded by `size` checks in the kernel.

Strings are borrowed `AsterStringView` values owned by the queried world or
renderer. They remain valid until the next mutating call on that object or until
the object is destroyed. Hashes are deterministic evidence identifiers, not
cryptographic security boundaries.

## Accessors

World reports are recorded through the tail fields on
`AsterWorldRegionGateReport` and queried with:

```c
AsterStatus aster_kernel_world_belief_report(
    AsterWorldHandle world,
    AsterBeliefReportInfo *out_report);

AsterStatus aster_kernel_world_belief_finding(
    AsterWorldHandle world,
    uint32_t index,
    AsterBeliefFindingInfo *out_finding);

AsterStatus aster_kernel_world_perceptual_primitive(
    AsterWorldHandle world,
    uint32_t index,
    AsterWorldPerceptualPrimitiveInfo *out_primitive);
```

Renderer reports are recorded through the tail fields on `AsterRendererSettings`
and queried from the last frame with:

```c
AsterStatus aster_kernel_renderer_frame_falseness_report(
    AsterRendererHandle renderer,
    AsterBeliefReportInfo *out_report);

AsterStatus aster_kernel_renderer_frame_falseness_finding(
    AsterRendererHandle renderer,
    uint32_t index,
    AsterBeliefFindingInfo *out_finding);

AsterStatus aster_kernel_renderer_frame_perceptual_world_schedule(
    AsterRendererHandle renderer,
    AsterPerceptualWorldScheduleInfo *out_schedule);

AsterStatus aster_kernel_renderer_frame_perceptual_primitive(
    AsterRendererHandle renderer,
    uint32_t index,
    AsterWorldPerceptualPrimitiveInfo *out_primitive);
```

The report accessors return `ASTER_STATUS_OK` for an object with no recorded
belief report; in that case the output report is zero-valued with
`finding_count == 0`. Finding accessors return `ASTER_STATUS_INVALID_ARGUMENT`
when `index >= finding_count`.

## Input Shape

`AsterWorldRegionGateReport` appends:

- `perceptual_world_schedule`
- `belief_report`
- `belief_findings`
- `perceptual_world_truth`
- `perceptual_primitives`

`AsterRendererSettings` appends:

- `perceptual_world_schedule`
- `belief_falseness_report`
- `belief_falseness_findings`

The finding spans use `AsterSpan::size` as element count and `stride` as the byte
distance between `AsterBeliefFindingInfo` entries. If a report declares findings,
the matching span must be present and have a stride at least
`sizeof(AsterBeliefFindingInfo)`.

The primitive span uses `AsterWorldPerceptualPrimitiveInfo` entries. When a new
caller supplies a non-zero `perceptual_world_truth.primitive_count`, the matching
primitive span must resolve to the same count. Old-size callers can omit the span
and continue to submit summary-only evidence.

## Diagnostics Fallback

The existing `AsterFrameDiagnosticEvent` stream remains supported. A renderer
that receives a belief report still emits diagnostic events with
`pass == "belief-extraction"` and labels such as
`belief.backend_visual_truth_gap`. Typed accessors are now the preferred surface
for stable consumers; diagnostics remain the compatibility route and frame
debugger proof trail.

`ASTER_BELIEF_FINDING_BACKEND_VISUAL_TRUTH_GAP` maps to capability mismatch
diagnostics. It means the backend proof is incomplete for visual truth
equivalence. It does not claim D3D12 swapchain, HDR, MSAA, GPU timestamp, fog,
probe, or shadow parity has been implemented.

Perceptual primitive findings are stable categories:

- `ASTER_BELIEF_FINDING_MISSING_PERCEPTUAL_PRIMITIVE`
- `ASTER_BELIEF_FINDING_UNRESOLVED_PERCEPTUAL_BINDING`
- `ASTER_BELIEF_FINDING_PERCEPTUAL_EXTRACTION_DESYNCHRONIZATION`
- `ASTER_BELIEF_FINDING_BACKEND_PERCEPTUAL_TRUTH_GAP`

The matching validation/falseness categories are also exposed for strict
renderable rejection, unresolved authoring bindings, primitive/extraction hash
drift, and backend frames that do not prove native primitive consumption.

## Compatibility

- ABI version is `6.4.0`.
- Existing ABI 6.3 callers remain source-compatible because new inputs are tail
  fields behind `size` checks.
- Existing `AsterFrameDiagnosticEvent` reporting remains available.
- The scheduler bridge is report-only. It mirrors frame/runtime evidence and
  never mutates world state through the ABI.
- Stable C consumers should use `include/aster/kernel/abi.h` or the source C++
  wrappers in `include/aster/kernel/api.hpp`; they must not depend on internal
  renderer or core C++ headers for this contract.
