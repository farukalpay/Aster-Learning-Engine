# Learning Contract

Aster's learning contract is code-first. Lessons are source Game SDK assets with
machine-readable objectives, evidence references, learner-state hypotheses,
misconceptions, scaffold rules, safety checks, and workflow stages.

The canonical V1 path is Lumen Run:

```bash
./build/aster_lumen_run --learning-proof-run \
  --learning-out /tmp/aster_lumen_live_learning

cargo run -p aster_assetc --bin aster_assetc -- lesson-inspect \
  --project projects/lumen_run/lumen_run.asterproj \
  --lesson lesson.lumen_mining \
  --output-schema

cargo run -p aster_assetc --bin aster_assetc -- learning-proof-run \
  --project projects/lumen_run/lumen_run.asterproj \
  --lesson lesson.lumen_mining \
  --trace /tmp/aster_lumen_live_learning/trace.jsonl \
  --output /tmp/aster_lumen_learning_proof \
  --output-schema
```

`LearningSession` is the runtime bridge between source Game SDK lesson assets
and gameplay. Games emit lesson-neutral signals containing an event, asset id,
and evidence channels. The session accumulates the declared event/channel
requirements, diagnoses misconceptions, proposes only grounded scaffolds,
rejects premature mastery, and writes replayable `trace.jsonl` plus `proof.json`
artifacts. Its proof passes only when the Game SDK evaluator and the internal
learning runtime agree on coverage and verdict.

Lumen Run emits these signals from actual focus, inventory, equipment, and
mining behavior. Normal interactive runs show learning coverage and the active
scaffold in the HUD debug log. Pass `--learning-out <directory>` to persist that
live session when the app exits. `--learning-proof-run` executes the same mining
loop headlessly, without opening a render window.

A learning proof passes only when the trace covers declared objectives, covers
the diagnose -> design -> teach -> evaluate workflow, grounds scaffold decisions
in declared evidence and learner-state hypotheses, and passes pedagogical safety
checks such as false-mastery rejection and no gameplay-forcing scaffolds.

Durable memory proof extends that lesson contract with typed traces, a real
SQLite graph/memory store, and provider-backed controller decisions:

```bash
cargo run -p aster_assetc --bin aster_assetc -- memory-proof-run \
  --project projects/lumen_run/lumen_run.asterproj \
  --policy memory.policy.lumen_mining \
  --trace projects/lumen_run/lessons/lumen_mining.trace.jsonl \
  --store /tmp/aster_lumen_memory.sqlite \
  --output /tmp/aster_lumen_memory_proof \
  --output-schema

cargo run -p aster_assetc --bin aster_assetc -- memory-bench-run \
  --project projects/lumen_run/lumen_run.asterproj \
  --suite memory.bench.lumen_mining \
  --store /tmp/aster_lumen_memory.sqlite \
  --output /tmp/aster_lumen_memory_bench \
  --output-schema
```

`memory-bench-run` requires a real Generic JSON HTTP provider through
`ASTER_MEMORY_PROVIDER_URL` or `--provider-url`. Auth and model headers are
configuration/env only; secrets are not written into trace or benchmark
artifacts. If no provider is available, the benchmark is blocked rather than
replaced by a local fake server.

ABI 8.0 promotes the learning runtime into the kernel boundary. Public
consumers can append/query typed trace events on `AsterSystemWorldHandle`, create
and step an `AsterMemoryControllerHandle`, query SQLite-backed graph results,
inspect controller decisions, and export replayable benchmark summaries. The
Game SDK adds memory policy and memory benchmark asset kinds; Lumen Run carries
sample policy/suite assets without defining engine defaults.
