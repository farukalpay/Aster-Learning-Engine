# Contributing

Aster values changes that improve the engine's long-term shape.

## Expectations

- Keep reusable engine logic in library layers.
- Keep app files thin and explicit.
- Prefer named data contracts over hidden global state.
- Add platform input through snapshots and control schemes instead of direct key polling in product logic.
- Add tests when changing math, scene contracts, mesh generation, or backend-visible behavior.
- Do not introduce topic-specific hardcoded rules where a general contract would fit.

## Local Checks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/aster_studio --smoke-test
./build/aster_lumen_run --smoke-test
```

## Dependency Policy

Dependencies should be:

- C or C++ oriented
- Actively maintained
- Compatible with open-source redistribution
- Isolated behind engine contracts where practical
