# Learning Contract

Aster's learning contract is code-first. Lessons are source Game SDK assets with
machine-readable objectives, evidence references, learner-state hypotheses,
misconceptions, scaffold rules, safety checks, and workflow stages.

The canonical V1 path is Lumen Run:

```bash
cargo run -p aster_assetc --bin aster_assetc -- lesson-inspect \
  --project projects/lumen_run/lumen_run.asterproj \
  --lesson lesson.lumen_mining \
  --output-schema

cargo run -p aster_assetc --bin aster_assetc -- learning-proof-run \
  --project projects/lumen_run/lumen_run.asterproj \
  --lesson lesson.lumen_mining \
  --trace projects/lumen_run/lessons/lumen_mining.trace.jsonl \
  --output /tmp/aster_lumen_learning_proof \
  --output-schema
```

A learning proof passes only when the trace covers declared objectives, covers
the diagnose -> design -> teach -> evaluate workflow, grounds scaffold decisions
in declared evidence and learner-state hypotheses, and passes pedagogical safety
checks such as false-mastery rejection and no gameplay-forcing scaffolds.

The stable kernel C ABI is unchanged. The new public surface is the Game SDK
lesson/trace contract and the `aster_assetc` lesson proof commands.
