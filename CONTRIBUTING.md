# Contributing

Aster values changes that improve the engine's long-term shape.

## Expectations

- Keep reusable engine logic in library layers.
- Keep app files thin and explicit.
- Preserve public contracts unless a contract change is the point of the work.
- Keep native platform details inside platform adapters.
- Pass viewport and input snapshots through UI/product code instead of native
  handles.
- Keep renderer decisions inspectable through material, queue, depth, and
  capability contracts instead of pattern-name branches.
- Keep sample-owned content in sample implementation files. Do not promote
  authored Lumen Run placement, route, material, or encounter data into engine
  modules unless the change defines a reusable contract.
- Put tests in the subsystem target that owns the contract. Shared test helpers
  stay under `tests/` and are not engine API.
- Keep interactive loops frame-paced by default; use explicit flags for
  unbounded profiling runs.
- Add tests when changing math, scene contracts, mesh generation, networking,
  profiling, renderer behavior, or platform-visible behavior.
- Remove dead compatibility code when a target is no longer supported.
- Avoid topic-specific hardcoded rules where a general contract fits.

## Dependency Policy

Aster v1 is standard C++ plus direct OS APIs in isolated platform files. Do not
add engine library dependencies. If a future change truly needs an
outside boundary, put it behind an engine contract, make the boundary explicit,
and document why the code cannot stay engine-owned.

## Local Checks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cargo test --workspace
./build/aster_studio --smoke-test
./build/aster_lumen_run --smoke-test
./build/aster_net_probe
```

## Local Cleanup

Build trees, Cargo targets, CTest output, captures, and OS metadata are local
artifacts. Inspect before deleting:

```bash
git status --short --ignored
git clean -ndX
```

Use `git clean -fdX` only for ignored generated state. Remove empty local
scratch directories with `rmdir` rather than broad untracked cleanup when source
experiments may exist.

## Screenshot Checks

Use the commands in `README.md` to regenerate screenshots after renderer, UI, or
sample-scene changes. Captures should have the expected dimensions and visible
color variance before replacing the checked-in PNGs.
