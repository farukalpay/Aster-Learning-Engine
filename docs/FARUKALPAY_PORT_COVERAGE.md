# FarukAlpay Native Port Coverage

This pass ports ideas from `/Users/farukalpay/Downloads/FarukAlpay-master`
into Aster-owned C++ systems. The old C source is not vendored and its public
names do not leak into Aster APIs; the behavior is translated into reusable
engine contracts and a visible Lumen Run sample route.

## Coverage Map

| FarukAlpay source family | Aster-owned destination | Port status |
| --- | --- | --- |
| `m_random.c`, `d_ticcmd.h`, `d_net.c` tick command flow | `DeterministicRandomStream`, `SimCommand`, `CommandReplay` in `include/aster/core/deterministic_sim.hpp` | Implemented as seedable streams, tick commands, replay lookup, and stable checksums. |
| `w_wad.c`, `w_wad.h`, `sndserv/wadread.*` lump indexing | `LegacyLumpArchive` in `include/aster/asset/legacy_lump_archive.hpp` | Implemented for WAD/PWAD and single-file lumps with canonical names, reloadable caches, and profile rows. |
| `p_enemy.c`, `p_mobj.c`, `p_sight.c`, `info.c` actor state ideas | `ClassicActorRuntime` in `include/aster/systems/classic_actor_runtime.hpp` | Implemented as stateful Aster actors with idle, alert, chase, strike, flinch, and death transitions. |
| `p_switch.c`, `p_doors.c`, `p_plats.c`, `p_floor.c`, `p_lights.c`, `p_spec.c` world specials | `WorldMechanismSystem` in `include/aster/systems/world_mechanism.hpp` | Implemented doors, lifts, timed lights, material-cycle timing, triggers, and deterministic progress. |
| `am_map.c`, `hu_*`, `st_*`, `f_wipe.c` presentation ideas | `AutomapModel`, `ClassicHudSignals`, `TransitionWipe` in `include/aster/ui` | Implemented as UI data models and deterministic melt-column progression rendered through `HudLayer`. |
| `d_net.c`, `i_net.c`, `ipx/*`, `sersrc/*` network transport ideas | `LockstepCommandChannel` in `include/aster/net/lockstep_command_channel.hpp` | Implemented packet checksums, command windows, resend hints, and decode validation over Aster command data. |

## Intentionally Not Ported Verbatim

- IPX and serial drivers from `ipx/` and `sersrc/` are platform-obsolete for
  Aster. Their checksum, window, and retransmit concepts were retained in the
  lockstep channel instead of carrying DOS-era transport code.
- Renderer internals such as BSP columns, visplanes, patch columns, palette
  colormaps, and direct framebuffer drawing were not copied. Aster already has
  renderer/RHI contracts; classic presentation is modeled as data and HUD
  overlays.
- Zone allocator and savegame binary layouts were not ported. Aster keeps RAII
  containers, typed records, and explicit cache/profile diagnostics.
- Old public prefixes and structs remain source-reference concepts only. Aster
  exposes `ClassicActorRuntime`, `WorldMechanismSystem`, `LegacyLumpArchive`,
  and lockstep command APIs instead of legacy names.

## Lumen Run Integration

Lumen Run consumes the new systems through the Classic Gauntlet route in the
deep cave. The route adds a pressure glyph, prism bulkhead doors, a moving lift,
classic sentinel encounters, an automap overlay, HUD alert signals, and the melt
transition model. The capture CLI accepts:

```bash
./build/aster_lumen_run --capture-route classic-gauntlet --screenshot /tmp/lumen_classic.ppm --screenshot-frame 96 --capture-hud --msaa 0 --window-width 1280 --window-height 720
```

The sample remains content-owned: the reusable behavior lives under
`include/aster` and `src`, while the cave placement and route composition stay
in `src/samples/lumen_run_*.cpp`.

## Test Coverage

- `aster_systems_tests` covers deterministic replay/lump archive behavior,
  actor transitions, mechanisms, automap projection, HUD signals, and wipe
  progression.
- `aster_network_tests` covers lockstep packet checksum/decode and resend
  behavior.
- `aster_sample_tests` covers the Lumen Classic Gauntlet scene objects,
  activation, automap visibility, bulkhead/lift behavior, and HUD/wipe exposure.
