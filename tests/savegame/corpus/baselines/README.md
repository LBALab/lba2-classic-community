# Render/state baselines (Layer 4)

Golden engine-state snapshots for each save in the corpus, captured through the CLI
control harness (see [docs/CONTROL.md](../../../../docs/CONTROL.md)). Where `run_harness.py`
checks that a save *loads* (Layer 3), this checks that it *renders to the expected state*.

Driven by `../baseline_harness.py`.

## What's here

- `<save>.json` — committed golden: a normalized `--dump-state` snapshot taken after
  `--load <save> --tick 8`. The `fps`, `timer_ref_hr`, and `log` fields are dropped (they
  are wall-clock or run-specific).
- `screenshots/` — **gitignored**. Full-frame PNGs regenerated on each run for human
  before/after review. They are not committed (large binaries; and a legitimate widescreen
  / resolution change alters every pixel, so they are for eyeballing, not auto-assert).

## What it protects

The dump is **world-space**, so a projection or resolution change leaves it unchanged.
That makes it the guardrail for the widescreen work: *"I changed rendering and did not
accidentally perturb the simulation."* The screenshots are the visual the human reviews.

## Comparison rules

Two tiers, from the measured determinism of the engine (see
[docs/AUTOMATION_PLAN.md](../../../../docs/AUTOMATION_PLAN.md)):

- **Structural (fails the baseline):** scene, `vars`, hero discrete fields, inventory,
  per-actor `index`/`body`/`life`/`move`/`flags`, and all counts. These are timing-invariant
  and must match exactly — this is the widescreen guardrail.
- **Kinematic (reported, non-fatal):** moving-actor `x`/`y`/`z`/`beta` (±64 tolerance) and
  `anim`/`last_frame`. These are inherently timing-dependent until `--fixed-dt` lands,
  because per-tick wall-clock variance moves/animates actors slightly differently between
  runs. The harness prints a `(+N kinematic)` note but does not fail. When `--fixed-dt`
  arrives these become structural too.
- `fps`, `timer_ref_hr`, `log` are dropped entirely.

## Usage

```bash
# Check current build against the goldens:
tests/savegame/corpus/baseline_harness.py --game-dir /path/to/LBA2

# Regenerate the goldens (after an intended, reviewed state change):
tests/savegame/corpus/baseline_harness.py --update --game-dir /path/to/LBA2
```

Local-only: needs the retail HQR (to render) and a display. Not part of host-only
`make test`, same as the rest of the corpus harness. A save that fails to load or loads
into a modal cutscene (blocking the first tick) is reported as skipped, never hangs the run.
