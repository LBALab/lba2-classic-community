# Projection baselines

Tiny committed fingerprints of the engine's projection pipeline. Phase 1
of the [widescreen plan](../../docs/WIDESCREEN.md) — Phase 2's
projection-origin / brick-clip / `Map2Screen` changes must keep these
hashes stable at 640×480.

## What's committed

| File | Scope | Tests it |
|---|---|---|
| `baselines/corpus_640x480.projrec.hash` | One line per save across the 50 committed saves under `tests/savegame/corpus/saves/steam_classic_2023/` | [`tests/automation/test_projection_corpus.sh`](../automation/test_projection_corpus.sh) |
| `baselines/demo_640x480.projrec.hash` | 30000-tick attract-mode snapshot — cross-scene transitions saves can't reach | [`tests/automation/test_projection_demo.sh`](../automation/test_projection_demo.sh), opt-in via `PROJREC_RUN_DEMO=1` |

Both fixtures are tiny — the corpus is ~3.8 KB (50 lines), the demo is a
single line. What's hashed is the FNV-1a 64-bit digest over the full
text event stream `--capture-projection` would have written for the same
run; the full stream isn't committed (~hundreds of MB per save).

## Coverage audit

Run `--projection-hash` against every committed save:

| Path | Function(s) | Saves exercising it | Total events across corpus |
|---|---|---|---|
| Exterior 3D perspective | `SetProjection`, `LongProjectPoint3D`, `ProjectList3DF` | **22 of 50** | 22 `SETPROJ`, 237K `PROJ`, 2K `PRLI` |
| Interior orthographic | `SetIsoProjection`, `LongProjectPointIso`, `ProjectListIso` | **50 of 50** (all touch `SETISO` via menu pre-arm) | 79 `SETISO`, 12K `PROJISO`, 394 `PRLIISO` |
| Isometric brick world→screen | `Map2Screen` (`GRILLE.CPP`) | **28 of 50** | 102K `M2S` |

That maps directly onto Phase 2's three change sites:

| Phase 2 site | Independent witness saves in corpus |
|---|---|
| `EXTFUNC.CPP:93` `SetProjection(320,...)` → `SetProjection(ModeDesiredX/2,...)` | 22 saves |
| `GRILLE.CPP` brick clips → render-width-derived | 22 saves (same — exterior rendering invokes the brick path) |
| `Map2Screen` `+288 / +226` → render-width / render-height-derived | 28 saves |

If any of those changes drift at 640×480, multiple corpus hashes will
flip and the failing save names point at which path regressed.

## Regenerating

```bash
LBA2_BIN=build/SOURCES/lba2cc LBA2_GAME_DIR=/path/to/data \
  bash scripts/dev/regen_projrec_baselines.sh
```

Runs all 50 saves (~7 seconds) + the demo (~35 seconds). Set
`PROJREC_SKIP_DEMO=1` to skip the demo for a quick regen.

Per-test in-place regen (when only one drifts):

```bash
LBA2_PROJECTION_REGEN=1 bash tests/automation/test_projection_corpus.sh
PROJREC_RUN_DEMO=1 LBA2_PROJECTION_REGEN=1 bash tests/automation/test_projection_demo.sh
```

Commit the updated `*.projrec.hash` file alongside whatever engine
change made it drift.

## Diagnosing a hash divergence

The committed baselines are just hashes. When one differs, regenerate the
full text capture locally and diff against the prior build's:

```bash
# Current build:
lba2cc --load <save> --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/current.projrec --exit

# Switch to the prior build (last known good), then repeat:
lba2cc --load <save> --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/prior.projrec --exit

diff /tmp/prior.projrec /tmp/current.projrec | head
```

The sequence number on the first divergent line tells you which
projection call changed — `SETPROJ` for the setup, `PROJ`/`PROJISO` for
individual vertices, `M2S` for the isometric brick origin. Cross-
reference with [`docs/CONTROL.md`'s Projection capture section](../../docs/CONTROL.md#projection-capture).

If the divergence is non-deterministic (same build produces different
hashes), the problem is upstream of projection — `--fixed-dt`, RNG
seed, or new global state leaking timing information.

## Wide-resolution baselines

Phase 3 of the widescreen plan lands `corpus_768x480.projrec.hash` and
`demo_768x480.projrec.hash` — a parallel regression net for the wide
render path. `RESOLUTION_X` must be a multiple of 8 (compile-time check
in `LIB386/H/SYSTEM/RESOLUTION.H`), so 768 was picked over a literal
16:9 (853 rounds down to 856, 768 is the cleanest near-16:9 multiple of
8 from the doc's existing example).

Build wide + regenerate:

```bash
cmake -B build-wide -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-DRESOLUTION_X=768" \
    -DCMAKE_CXX_FLAGS="-DRESOLUTION_X=768"
cmake --build build-wide

LBA2_BIN=build-wide/SOURCES/lba2cc \
  LBA2_GAME_DIR=/path/to/data \
  LBA2_PROJREC_WIDTH=768 \
  bash scripts/dev/regen_projrec_baselines.sh
```

Run wide tests against the wide build:

```bash
LBA2_BIN=build-wide/SOURCES/lba2cc \
  LBA2_GAME_DIR=/path/to/data \
  LBA2_PROJREC_WIDTH=768 \
  bash tests/automation/test_projection_corpus.sh
```

`LBA2_PROJREC_WIDTH` selects which committed golden to compare against;
it must match the binary's compiled `RESOLUTION_X`.

The wide hashes are necessarily different from the 640×480 ones
(different `ModeDesiredX` → different `SETPROJ`, different `M2S`, etc.).
What they prove is that the wide path is deterministic across runs, so
any future drift in the wide build gets caught.
