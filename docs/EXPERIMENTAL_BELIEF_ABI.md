# Experimental Belief ABI

Status: proposal only. This batch does not change `include/aster/kernel`, does
not edit `abi/aster_kernel.symbols`, and does not add exported C symbols.

## Goal

Belief Extraction V1 lives behind internal C++ and asset compiler contracts. It
connects world proof, extraction proof, and frame diagnostics with a typed
report:

- `accepted`
- `score`
- `minimum_score`
- `belief_contract_hash`
- `readability_audit_hash`
- canonical falseness findings

Stable ABI consumers currently observe these findings through existing
`AsterFrameDiagnosticEvent` entries. The precise falseness kind is encoded in
the event label and message so ABI 6.3 consumers do not need new structs.

## Future Append-Only Shape

The first stable exposure should be append-only and size/version gated, matching
the current kernel ABI style.

```c
typedef enum AsterBeliefFindingKind {
  ASTER_BELIEF_FINDING_MATERIAL_FAMILY_COLLAPSE = 1,
  ASTER_BELIEF_FINDING_CONTEXTUAL_GROUNDING_FAILURE = 2,
  ASTER_BELIEF_FINDING_CONTACT_SHADOW_CREDIBILITY_FAILURE = 3,
  ASTER_BELIEF_FINDING_VOLUMETRIC_SCENE_COUPLING_FAILURE = 4,
  ASTER_BELIEF_FINDING_MATERIAL_RESPONSE_INSTABILITY = 5,
  ASTER_BELIEF_FINDING_LOD_TRANSITION_VISIBILITY = 6,
  ASTER_BELIEF_FINDING_ASSET_SCALE_INCOHERENCE = 7,
  ASTER_BELIEF_FINDING_ENVIRONMENTAL_ENTROPY_DEFICIT = 8
} AsterBeliefFindingKind;

typedef struct AsterBeliefFindingInfo {
  size_t size;
  uint32_t version;
  AsterBeliefFindingKind kind;
  AsterDiagnosticSeverity severity;
  AsterStringView subject;
  float score;
  float threshold;
  uint64_t evidence_hash;
  AsterStringView source;
  AsterStringView message;
} AsterBeliefFindingInfo;

typedef struct AsterBeliefReportInfo {
  size_t size;
  uint32_t version;
  uint32_t accepted;
  float score;
  float minimum_score;
  uint64_t world_transition_hash;
  uint64_t extraction_hash;
  uint64_t belief_contract_hash;
  uint64_t readability_audit_hash;
  uint32_t finding_count;
} AsterBeliefReportInfo;
```

Candidate accessors:

```c
AsterStatus aster_kernel_world_belief_report(
    AsterWorldHandle world,
    AsterBeliefReportInfo *out_report);

AsterStatus aster_kernel_world_belief_finding(
    AsterWorldHandle world,
    uint32_t index,
    AsterBeliefFindingInfo *out_finding);

AsterStatus aster_kernel_renderer_frame_falseness_report(
    AsterRendererHandle renderer,
    AsterBeliefReportInfo *out_report);

AsterStatus aster_kernel_renderer_frame_falseness_finding(
    AsterRendererHandle renderer,
    uint32_t index,
    AsterBeliefFindingInfo *out_finding);
```

## Compatibility Rules

- New structs must append fields only after existing fields once stabilized.
- Callers must pass `size` and `version`; the kernel must zero-fill unknown
  trailing fields for smaller caller sizes.
- Strings remain borrowed views owned by the queried world or renderer object.
- Hash values are evidence identifiers, not cryptographic security boundaries.
- Existing `AsterFrameDiagnosticEvent` reporting should remain available after
  typed belief accessors ship.

## Not In This Batch

- No ABI major/minor bump.
- No new public kernel headers or symbols.
- No D3D12 swapchain, HDR, MSAA, shadow, fog, or probe parity changes.
- No Studio UI surface for belief authoring.
