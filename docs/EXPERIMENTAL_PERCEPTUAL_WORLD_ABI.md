# Experimental Perceptual World ABI Shadow

This batch keeps the stable kernel ABI append-only. The accepted C surface exposes
only `AsterPerceptualWorldTruthSummary`: primitive count, active subrecord counts,
the deterministic truth hash, averaged V1 signals, and an accepted bit. Detailed
`WorldPerceptualPrimitive` records remain internal runtime and SDK data for V1.

The shadow proposal for a future ABI version is a read-only inspection surface:

- `AsterWorldPerceptualPrimitiveInfo` with primitive id, owner hash, cause hash,
  residency, semantic LOD, material memory, residue, contact, light, acoustic,
  ecology, threat, traversal, decision impact, and truth hash.
- Four span-backed subrecord views for `CellAnchor`, `SurfacePatch`,
  `ContactZone`, and `ResidueChannel`.
- A renderer query that returns primitive traces for the last frame without
  changing replay semantics or resource ownership.
- A world query that returns the same primitive truth model used by Studio
  overlays, FrameForensics, AI visibility, streaming, and render extraction.

Acceptance requirements before promotion:

- Old-size structs must continue to pass with zeroed perceptual summary fields.
- New-size structs must round-trip the compact summary without exposing mutable
  runtime storage.
- Deterministic replay must prove parity for simulation, render extraction, AI,
  audio cue budget, streaming budget, belief extraction, and FrameForensics audit
  hashes before detailed primitive inspection becomes stable ABI.
