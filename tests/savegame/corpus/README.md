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
cmake --build build --target lba2 save_decompress

# 2. Probe every save into NDJSON (uses scripts/save_probe.py)
export LBA2_SAVE_TEST_DIR=$HOME/.local/share/Twinsen/LBA2/save
LBA2_SAVE_DECOMPRESS=$PWD/build/tools/save_decompress \
    python3 scripts/save_probe.py --recursive --json-lines \
    "$LBA2_SAVE_TEST_DIR" \
    > tests/savegame/corpus/probe.ndjson

# 3. Build classification manifest
python3 tests/savegame/corpus/build_manifest.py

# 4. Drive lba2 --save-load-test on each save (auto + abi=32 by default)
python3 tests/savegame/corpus/run_harness.py --timeout 15 --game-dir ../LBA2
```

`run_harness.py` writes the per-save outcome (`ok_init`, `ok_loaded`,
`ctxerr`, `signal_11`, `timeout`) into `manifest.json` for diffing across
engine builds.

## Engine flag

The harness uses `lba2 --save-load-test <path>` — present whether or not this
test dir is available. It boots the engine to the menu and replicates the
"Load Game" path on the given save, prints `SAVE_LOAD_TEST: stage=… …` lines
to stdout, and exits. Useful as a developer tool for any save-load
investigation, not just this corpus.

## Files

- `build_manifest.py` — classifies each save by stride sniff (32-wire / native /
  ambiguous) and records expected-load slots; reads `LBA2_SAVE_TEST_DIR`.
- `run_harness.py` — drives `lba2 --save-load-test`, parses `SAVE_LOAD_TEST` lines,
  writes outcomes back to `manifest.json`.
- `probe.ndjson`, `manifest.json` — regenerated locally; gitignored because
  they reference user paths.
