# Perceptual World ABI

This surface is stable and append-only. The compact
`AsterPerceptualWorldTruthSummary` remains available for older consumers:
primitive count, active subrecord counts, deterministic truth hash, averaged V1
signals, and an accepted bit.

Detailed `WorldPerceptualPrimitive` truth is now promoted as immutable read-only
ABI evidence. Mutation remains owned by runtime world state, authoring, and asset
compiler layers; C scene descriptors do not author primitive truth.

The detailed inspection surface is:

- `AsterWorldPerceptualPrimitiveInfo` with primitive id, owner hash, cause hash,
  template hash, cell hash, residency, exposure age, semantic LOD, material
  half-life, wetness/history decay, residue, contact-normal history, light,
  acoustic and visual occlusion trust, traversal affordance, streaming cost,
  decision impact, player-readable cause, and truth hash.
- Four span-backed subrecord views for `CellAnchor`, `SurfacePatch`,
  `ContactZone`, and `ResidueChannel`.
- `aster_kernel_world_perceptual_primitive`, which returns the primitive records
  stored with the last accepted world evidence.
- `aster_kernel_renderer_frame_perceptual_primitive`, which returns the
  primitive traces consumed by the last rendered frame.

Strict renderables must provide accepted primitive truth before frame planning.
The frozen `AsterSceneObjectDesc` path remains a compatibility exception: frames
from direct C scene input can render, but they are reported as non-truth-equivalent
instead of silently satisfying strict perceptual proof.

Acceptance requirements for future tail fields:

- Old-size structs must continue to pass with zeroed perceptual summary and no
  primitive spans.
- New-size structs must round-trip the compact summary and detailed primitive
  records without exposing mutable runtime storage.
- Deterministic replay must keep primitive truth hashes aligned with simulation,
  render extraction, belief extraction, and FrameForensics audit hashes.
