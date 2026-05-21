# Entanglement Porting Ledger

Aster may reuse source ideas from `/Users/farukalpay/Documents/GitHub/Entanglement`
because both projects are owned by Faruk Alpay. Porting still has to preserve
Aster ownership boundaries and avoid replacing newer Aster renderer/RHI work
with older source-port code.

## Policy

- Port only the pieces that strengthen Aster's agent, audit, review, or native
  migration workflows.
- Rename public identifiers to Aster naming before landing them here.
- Keep Entanglement's language/compiler/proof stack out of Aster unless a future
  task explicitly asks for a language integration.
- Prefer Aster's current renderer, RHI, asset compiler, and Game SDK contracts
  when a source-port file overlaps with newer Aster code.
- Record copied or adapted modules in this file when a port lands.

## Initial Port

The first pass ports concepts rather than whole crates:

| Entanglement source | Aster target | Status |
| --- | --- | --- |
| `ent-inspect` workspace/source reporting | `aster_assetc agent-audit` | Adapted as dependency-light Aster workspace inspection |
| `ent-native-audit` native inventory | `aster_assetc agent-native-audit` | Adapted as C/C++/Rust source surface reporting |
| `ent-runtime-audit` runtime ledger | `aster_assetc agent-runtime-audit` | Adapted as Aster agent/tooling architecture reporting |
| `ent-review` review packet shape | `aster_assetc agent-review` | Adapted as plan/repo review packet |

No Entanglement `native/ent-gfx/source_port` renderer files are copied in this
pass. Aster's renderer/RHI code is newer and remains authoritative.
