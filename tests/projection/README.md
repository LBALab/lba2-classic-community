# Projection baselines

Tiny committed fingerprints of the engine's projection pipeline, used by
[`tests/automation/test_projection_baseline.sh`](../automation/test_projection_baseline.sh)
to detect drift. Phase 1 of the [widescreen plan](../../docs/WIDESCREEN.md).

## What's in a baseline

One line per scenario:

```
# projrec-hash v1
SCENE events=10778 fnv1a=f624dd8a3c79d364
```

The hash is FNV-1a 64-bit over the textual event stream that
`--capture-projection` would have written for the same run. Two runs of the
same harness invocation against the same retail data produce the same hash
iff the projection pipeline produced bit-identical output.

The full text stream isn't committed (hundreds of MB for a `--demo` replay)
— only the fingerprint. When a hash differs and you need to diagnose what
changed, regenerate the full capture locally:

```bash
LBA2_BIN=build/SOURCES/lba2cc \
LBA2_GAME_DIR=/path/to/retail \
LBA2_TEST_SAVE=/path/to/Downtown.LBA \
SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
$LBA2_BIN --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 5 \
    --capture-projection /tmp/current.projrec --exit

# repeat against the prior build, then:
diff /tmp/old.projrec /tmp/current.projrec
```

## Regenerating a baseline

Use when the change is intentional (e.g. Phase 2's projection-origin fix
which must still match at 640×480 — if it doesn't, the change is wrong;
if it does, the hash stays the same and no regen is needed):

```bash
LBA2_BIN=build/SOURCES/lba2cc \
LBA2_GAME_DIR=/path/to/retail \
LBA2_TEST_SAVE=/path/to/Downtown.LBA \
LBA2_PROJECTION_REGEN=1 \
bash tests/automation/test_projection_baseline.sh
```

Commit the updated `*.projrec.hash` file alongside the engine change.

## Current baselines

| File | Scenario | Notes |
|---|---|---|
| `baselines/downtown_640x480.projrec.hash` | `--load Downtown.LBA --fixed-dt 16 --tick 5` at 640×480 | The starter baseline. Exterior scene with mixed `SETPROJ` / `PROJ` / `PRLI` coverage. |

Wider-resolution baselines (e.g. `downtown_853x480.projrec.hash`) land
alongside the engine work that actually renders wider frames — Phase 3
of [WIDESCREEN.md](../../docs/WIDESCREEN.md).

## When a hash differs

1. If the change was intentional and you've validated the new behavior
   is correct, regenerate with `LBA2_PROJECTION_REGEN=1`.
2. If the change was unintentional, regenerate the full capture (above)
   and diff against the prior build's capture to find the first event
   that diverged. The event's sequence number narrows it to a specific
   call into the projection pipeline.
3. If the divergence is non-deterministic (different on different runs
   of the same build), the determinism guarantee is broken — investigate
   `--fixed-dt`, the RNG seed, or any new global state added since the
   baseline was generated.
