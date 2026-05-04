# Save-game corpus harness (issue #62)

Layer-3 regression harness — runs the real `lba2` binary on a directory of
`.lba` save files and records the load outcome per save.

**Requires retail game data (`lba2.hqr`)**, so it does not run in public CI.
For pure parser regressions that need no retail, see
`tests/savegame/test_load_bounds.cpp`.

## Workflow

```bash
# 1. Build the engine (any preset)
cmake --build build --target lba2

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
