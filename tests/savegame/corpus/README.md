# Save-game corpus harness (issue #62)

Layer-3 regression harness — runs the real `lba2` binary on a directory of
`.lba` save files and records the load outcome per save.

**Requires retail game data (`lba2.hqr`)**, so it does not run in public CI.
For pure parser regressions that need no retail, see
`tests/savegame/test_load_bounds.cpp`.

## Bundled reference corpus

`saves/steam_classic_2023/` ships with the repo: 50 anonymized saves
captured from the Steam re-release (`TLBA2C.exe`, app 397330). Provenance,
license, and the anonymization process are documented in
`saves/steam_classic_2023/README.md`. Use this corpus as the regression
baseline when changing `SOURCES/SAVEGAME.CPP` — expected outcome is
50/50 `ok_init` under both `auto` and forced `LBA2_SAVE_LOAD_ABI=32`.

## Quick start (one command)

```bash
# Make target (wrapper around scripts/dev/run-savegame-corpus.sh)
make savegame-corpus

# Explicit retail path override (portable across local layouts)
make savegame-corpus GAME_DIR=/path/to/LBA2/Common

# Optional harness knobs
make savegame-corpus GAME_DIR=/path/to/LBA2/Common TIMEOUT=20 ABIS=auto,32

# Direct script usage (same behavior as make target)
./scripts/dev/run-savegame-corpus.sh --game-dir /path/to/LBA2/Common
```

The wrapper resolves game data in this order:

1. `--game-dir` / `GAME_DIR=...` override
2. `LBA2_GAME_DIR`
3. local candidates: `./data`, `../LBA2`, `../game` (from repo root), if they
   contain `lba2.hqr`/`LBA2.HQR`

## Workflow

```bash
# 1. Build required binaries (any preset)
cmake --build build --target lba2cc save_decompress

# 2. Probe every save into NDJSON (uses scripts/save_probe.py)
export LBA2_SAVE_TEST_DIR=$HOME/.local/share/Twinsen/LBA2/save
LBA2_SAVE_DECOMPRESS=$PWD/build/tools/save_decompress \
    python3 scripts/save_probe.py --recursive --json-lines \
    "$LBA2_SAVE_TEST_DIR" \
    > tests/savegame/corpus/probe.ndjson

# 3. Build classification manifest
python3 tests/savegame/corpus/build_manifest.py

# 4. Drive lba2cc --save-load-test on each save (auto + abi=32 by default)
python3 tests/savegame/corpus/run_harness.py --timeout 15 --game-dir ../LBA2
```

`run_harness.py` writes the per-save outcome (`ok_init`, `ok_loaded`,
`ctxerr`, `signal_11`, `timeout`) into `manifest.json` for diffing across
engine builds.

## Engine flag

The harness uses `lba2cc --save-load-test <path>` — present whether or not this
test dir is available. It boots the engine to the menu and replicates the
"Load Game" path on the given save, prints `SAVE_LOAD_TEST: stage=… …` lines
to stdout, and exits. Useful as a developer tool for any save-load
investigation, not just this corpus.

## Baseline harness (Layer 4)

`baseline_harness.py` is the render/state regression net. Where `run_harness.py` asks
"does the save load?", this loads each save through the CLI control harness, advances a
fixed number of ticks, and captures a normalized `--dump-state` snapshot as a committed
golden under `baselines/`. The world-space dump is the guardrail that rendering /
projection changes do not perturb the simulation; screenshots (gitignored) are captured
alongside for human review. See `docs/CONTROL.md` and `docs/FIXED_DT_PLAN.md`.

```bash
# Compare current run against committed goldens (exits non-zero on drift)
LBA2_GAME_DIR=/path/to/LBA2/Common \
    python3 tests/savegame/corpus/baseline_harness.py --lba2 build-nullsnd/SOURCES/lba2cc

# Regenerate goldens
... --update --repeats 5
```

Determinism requirements (both matter — see `docs/CONTROL.md`):

- **Build with `-DSOUND_BACKEND=null`.** `--fixed-dt` pins the clock, but the SDL audio
  backend services exterior ambient samples on a wall-clock thread that branches the
  simulation; only the null backend removes that. `SDL_AUDIODRIVER=dummy` alone is not
  enough. The harness passes `--fixed-dt 16` (the canonical step) by default.
- **Goldens are per platform/build.** `long double` precision differs across Linux x86_64
  (80-bit) vs Windows/macOS-ARM (64-bit), so the committed goldens are the Linux x86_64
  canonical set. Validate on that platform (a Linux container is the portable route).

**Self-calibrating, no skip-list.** Each save is run `--repeats` times; only saves that
reproduce themselves byte-for-byte are compared against (or written as) a golden. A save
that cannot reproduce itself this session is reported `FLAKY` (non-fatal) and never stored
or asserted, so it can neither mask a regression nor fail spuriously. ~7 of the 48 saves
are currently FLAKY from a residual non-dt nondeterminism source (e.g. a dying-enemy spin
angle, an uninitialized bodiless-extra position) that fixed-dt does not pin — a known
follow-up to investigate, tracked separately from this regression net.

## Files

- `build_manifest.py` — classifies each save by stride sniff (32-wire / native /
  ambiguous) and records expected-load slots; reads `LBA2_SAVE_TEST_DIR`.
- `run_harness.py` — drives `lba2cc --save-load-test`, parses `SAVE_LOAD_TEST` lines,
  writes outcomes back to `manifest.json`.
- `baseline_harness.py` — Layer-4 render/state goldens (above); writes `baselines/*.json`.
- `probe.ndjson`, `manifest.json` — regenerated locally; gitignored because
  they reference user paths.
