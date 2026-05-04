# Testing

This repository uses one testing strategy throughout: compare the original ASM
implementation against the C/C++ port and treat the ASM result as the source of
truth. The tests are designed to catch behavioral drift during the ongoing
porting work in `LIB386/`.

## Core Rule

All equivalence tests must compare ASM and CPP results exactly.

- Use `ASSERT_ASM_CPP_EQ_INT` for scalar values.
- Use `ASSERT_ASM_CPP_MEM_EQ` for buffers, framebuffers, tables, and structs.
- Do not add "no crash" tests.
- Do not weaken comparisons with tolerances when the goal is ASM equivalence.
- If a single byte differs, fix the CPP implementation or isolate the mismatch
  with a smaller test.

For current coverage and status, see `ASM_VALIDATION_PROGRESS.md` in the repo
root.

## CI (GitHub Actions)

Workflows under `.github/workflows/`:

| Workflow | What it does |
|----------|----------------|
| `linux.yml` | Configure with preset `linux`, build `lba2` + `test_res_discovery` + `test_console_state` + `test_savegame_load_bounds`, run `ctest -R 'test_(res_discovery|console_state|savegame_load_bounds)'` |
| `macos.yml` | Same host tests on `macos-latest` (preset `macos_arm64`) |
| `windows.yml` | Same host tests on Windows MSYS2 UCRT64 (preset `windows_ucrt64`) |
| `format.yml` | `scripts/ci/check-format.sh` (clang-format) |
| `test.yml` | **Docker:** `./run_tests_docker.sh` — full ASM↔C++ equivalence suite (Linux only; slow) |

Host jobs do **not** need retail game files or Docker. The Docker job does not run discovery tests; it focuses on LIB386 equivalence.

## What Is In Scope

The repo currently uses three complementary testing workflows.

### 1. Unit and Equivalence Tests

Most tests live under `tests/` and are grouped by subsystem:

- `tests/3D`
- `tests/SYSTEM`
- `tests/MENU`
- `tests/ANIM`
- `tests/SVGA`
- `tests/pol_work`
- `tests/OBJECT`
- `tests/fpu_precision`
- `tests/VIDEO_AUDIO` (CPP-only; ResampleAndAddToMix, GetEffectiveTrackRate)
- `tests/SNAPSHOT`
- `tests/console` (host-only; `CONSOLE_STATE` helpers — see §5; plain `main` + asserts, no `test_harness.h`)

These tests are registered through CTest and typically use the custom harness in
`tests/test_harness.h` (ASM↔CPP and subsystem fixtures above; not `tests/console`).

### 2. Polygon Recording Replay (`.lba2polyrec`)

For rendering bugs that are hard to reproduce with small synthetic fixtures, the
game can record real polygon draw calls into `.lba2polyrec` files. Replay tools
then run those exact recorded calls through both ASM and CPP renderers and
compare the resulting framebuffers byte-for-byte.

This is the main workflow for investigating polygon filler mismatches.

### 3. Focused Numerical / Precision Tests

Some mismatches come from x87 behavior, intermediate rounding, or calling
convention details rather than high-level logic. The `tests/fpu_precision`
directory contains targeted ASM-vs-CPP tests for those low-level cases.

### 4. Host tests — game data discovery (`tests/discovery`)

`tests/discovery/test_res_discovery.cpp` exercises **`ResolveGameDataDir`** (`LBA2_GAME_DIR`, `--game-dir` stripping), **parent-directory sibling discovery** (retail folder or `CommonClassic` next to a fake `repo_clone` via `chdir`), and **embedded default `lba2.cfg`** writing. These run **on the host** (no Docker, no 32-bit ASM). Configure with `-DLBA2_BUILD_TESTS=ON` and **`-DLBA2_BUILD_ASM_EQUIV_TESTS=OFF`** if you only need discovery tests (no `objcopy`; used by `make test-host` and PR host jobs).

### 6. Host tests — savegame (`tests/savegame`)

`tests/savegame/test_load_bounds.cpp` (issue #62) links [SOURCES/SAVEGAME_LOAD_BOUNDS.CPP](../SOURCES/SAVEGAME_LOAD_BOUNDS.CPP) and checks **`SaveLoadValidateCompressedStaging`** / **`SaveLoadGuessObjectWireStride`** on synthetic buffers — pure parser bounds, no retail game data. Built and run as part of `make test-host` / PR host jobs.

`tests/savegame/corpus/` holds the Layer-3 driver scripts (`run_harness.py`, `build_manifest.py`) that exercise the real `lba2 --save-load-test` flag against a directory of `.lba` saves; **requires retail game data** so it runs locally only. See `tests/savegame/corpus/README.md`.

### 5. Host tests — console (`tests/console`)

`tests/console/test_console_state.cpp` links the `console` library and exercises **`SOURCES/CONSOLE/CONSOLE_STATE.CPP`**: availability strings (same contract as `console_avail_in_game_scene` in `CONSOLE_CMD.CPP`), legacy **video/menu/game** labels from **`Console_ModeString_FromState`** (for tests), and **`Console_FormatStatusIslandLine_FromState`** (first line of the **`status`** command: island, cube, chapter; raw values like `cmd_status`, not give-style scene masking). No retail `lba2.hqr` or full engine init.

`tests/console/test_console_commands.cpp` covers parser/output integration in **`SOURCES/CONSOLE/CONSOLE.CPP`** using host-only hooks: built-in `help` and `cmdlist`, unknown-command diagnostics, cvar set/get (`fps`), and a registered **`status`** command path that prints the same status headline format.

PR host jobs and **`make test`** / **`make test-host`** build `test_res_discovery`, `test_console_state`, `test_console_commands`, and `test_savegame_load_bounds`, then:

```bash
cd build && ctest -R 'test_(res_discovery|console_state|console_commands|savegame_load_bounds)' --output-on-failure
```

To run only the console suite: `ctest -R test_console_ --output-on-failure`.

## Test Harness

`tests/test_harness.h` provides a minimal TAP-style harness with no external
framework dependency.

Useful macros:

- `RUN_TEST(fn)`
- `TEST_SUMMARY()`
- `ASSERT_EQ_INT(...)`
- `ASSERT_MEM_EQ(...)`
- `ASSERT_ASM_CPP_EQ_INT(...)`
- `ASSERT_ASM_CPP_MEM_EQ(...)`

The harness prints relative source paths and keeps the tests simple enough to
compile in the same environment as the engine code.

## How ASM-vs-CPP Tests Are Built

The helper in `tests/cmake/asm_test_helpers.cmake` implements
`add_asm_cpp_test(...)`.

That helper does the following:

1. Builds the C++ test executable.
2. Patches `.model SMALL` ASM files to `.model FLAT` so they assemble into
   valid 32-bit ELF objects without changing instruction encoding.
3. Assembles the original ASM with UASM.
4. Uses `objcopy --redefine-sym` to rename exported ASM symbols to `asm_*` so
   ASM and CPP implementations can coexist in one binary.
5. Links the renamed ASM object together with the CPP libraries and the test.

Important caveats:

- ASM equivalence binaries are linked with `-no-pie` because text relocations
  in generated 32-bit objects do not work reliably with PIE.
- Some original ASM procedures use Watcom register calling convention rather
  than normal C stack parameters. Those cases may need wrapper code or extra ASM
  dependency objects to test safely.
- If an ASM file exports function-pointer globals or depends on ASM data tables,
  those symbols often need to be renamed consistently in both the main object
  and its dependencies.

## Running Tests

### Preferred Entry Point

Run tests through:

```bash
./run_tests_docker.sh
```

This is the supported path for the full ASM-vs-CPP suite. The script builds a
Linux `amd64` Docker image, installs UASM inside the container at runtime,
configures the `linux_test` CMake preset, builds the test binaries, and runs
CTest.

This is especially important on macOS ARM64, where the Docker-based workflow
avoids local 32-bit toolchain and UASM compatibility issues.

### Common Commands

Run the entire suite:

```bash
./run_tests_docker.sh
```

Build the Docker image only:

```bash
./run_tests_docker.sh --build-only
```

Force a fresh Docker rebuild before running:

```bash
./run_tests_docker.sh --rebuild
```

Run only specific tests by CTest name regex components:

```bash
./run_tests_docker.sh test_getang2d test_lirot3df
./run_tests_docker.sh test_polyrec_polyrec_0002
```

Logs are written automatically to `build_logs/`.

### `run_tests_docker.sh` Parameters

The script accepts a small set of flags plus positional test-name arguments.

General form:

```bash
./run_tests_docker.sh [options] [test-name ...]
```

Parsing rules:

- Any positional argument that is not one of the recognized flags is treated as
  a test-name filter.
- Multiple positional test names are joined into a single CTest regex with `|`.
- `--start-after` and `--stop-after` are forwarded to the polyrec replay tools,
  not to CTest itself.
- `--polyrec`, `--start-after`, and `--stop-after` are only meaningful in
  replay-oriented workflows.

Accepted parameters:

| Parameter | Argument | Meaning | Notes |
|---|---|---|---|
| `--build-only` | none | Build the Docker image and exit without running tests | Useful after `docker/Dockerfile.test` changes or for warming the cache |
| `--rebuild` | none | Force a Docker image rebuild even if `lba2-test` already exists | Still proceeds to the selected run mode unless combined with `--build-only` |
| `--render` | none | Build the replay executables and render `.lba2polyrec` recordings instead of running CTest | Copies generated `.raw` and `.ppm` files back into `tests/SNAPSHOT/fixtures/` |
| `--render-individually` | none | With `--render`, fan out a replay range into one render per draw call | Requires `--stop-after`; `--start-after 123 --stop-after 126` renders calls 124, 125, and 126 independently |
| `--bisect` | none | Build the replay executables and binary-search for the first divergent polygon draw call | Stops on the first recording that shows a mismatch |
| `--polyrec` | recording name or path | Restrict `--render` to one polygon recording | Accepts a bare fixture stem like `polyrec_0002`, a fixture filename like `polyrec_0002.lba2polyrec`, or an absolute path |
| `--start-after` | integer | Start replay after draw call `N` | Forwarded to `replay_polyrec_{asm,cpp}`; mainly useful with `--render` |
| `--stop-after` | integer | Stop replay after draw call `N` | Forwarded to `replay_polyrec_{asm,cpp}`; used heavily during bisect follow-up |
| `<test-name ...>` | one or more strings | Filter CTest to matching test names | In normal mode, `test_getang2d test_lirot3df` becomes `ctest -R 'test_getang2d|test_lirot3df'` |

Mode behavior:

- With no replay flags, the script configures and builds the `linux_test`
  preset, then runs CTest.
- With `--render`, the script skips CTest and instead renders recordings with
  `render_polyrec.sh`.
- With `--bisect`, the script skips CTest and instead runs `bisect_polyrec.sh`
  across the available recordings.
- If both `--render` and `--bisect` are passed, `--render` wins because that
  branch is checked first in the script.
- Positional test names only affect the normal CTest mode. They do not filter
  recordings during `--render` or `--bisect`.

Examples:

```bash
./run_tests_docker.sh --render
./run_tests_docker.sh --render --polyrec polyrec_0002 --stop-after 3226
./run_tests_docker.sh --render --render-individually --polyrec polyrec_0002 --start-after 123 --stop-after 126
```

In individual render mode, outputs are suffixed per draw call, for example
`polyrec_0002_draw_0124_asm.ppm` and `polyrec_0002_draw_0124_cpp.ppm`.

```bash
./run_tests_docker.sh --bisect
./run_tests_docker.sh test_getang2d test_lirot3df
```

During `--render`, generated `.raw` and `.ppm` files are copied back into
`tests/SNAPSHOT/fixtures/`. If ImageMagick is available in the environment used
by `render_polyrec.sh`, PNGs and a visual diff image are produced as well.

The Docker image definition used by the script lives at
`docker/Dockerfile.test`.

## Writing or Updating Unit Tests

When adding or modifying an ASM equivalence test:

1. Add the test source under the appropriate `tests/<module>/` directory.
2. Register it in that directory's `CMakeLists.txt` with `add_asm_cpp_test(...)`.
3. Include `tests/test_harness.h` and the relevant LIB386 header.
4. Declare ASM-side entry points as renamed `asm_*` symbols where needed.
5. Cover normal inputs, edge cases, and a deterministic random stress pass.
6. If the function writes global state, compare that global state after both
   paths run.
7. Update `ASM_VALIDATION_PROGRESS.md` so coverage status stays current.

For randomized stress tests, use a deterministic seed so failures are
reproducible.

## Debugging Mismatches

When ASM and CPP diverge, the expected workflow is:

1. Reproduce the mismatch with a real test or replay fixture.
2. Reduce the problem to the smallest helper or intermediate computation you can
   isolate.
3. Add debug traces to both implementations if needed.
4. Extract the concrete failing values into a focused regression test.
5. Fix the CPP implementation to match ASM.
6. Re-run the failing test and then the broader suite.

Avoid changing the test to make a mismatch "acceptable". The point of these
tests is to preserve exact behavior while the port progresses.

## Polyrec Workflow

### What Polyrec Captures

`LIB386/SNAPSHOT/POLY_RECORDING.CPP` records polygon rendering activity during a
real game frame, including:

- filler switches
- fog state
- CLUT changes
- texture selection and `RepMask`
- clip rectangle changes
- polygon, sphere, and line draw calls
- the live game's reference framebuffer for that capture

Each recording is written as a `.lba2polyrec` fixture.

### Capturing a Recording From the Game

The game-side recording hook is enabled by a CMake preset that defines
`ENABLE_POLY_RECORDING`.

In a recording-enabled build:

1. Press `Alt+F9` during gameplay.
2. The game sets `SnapshotRequested`.
3. At the start of the next `AffScene()`, recording begins before rendering.
4. The engine forces a full redraw so terrain and objects are both captured.
5. At the end of that scene render, `polyrec_stop()` writes
   `polyrec_XXXX.lba2polyrec`.

New fixtures should be moved into `tests/SNAPSHOT/fixtures/` if they are meant
to become part of the regression corpus.

### Automated Polyrec Tests

`tests/SNAPSHOT/CMakeLists.txt` registers one CTest per
`tests/SNAPSHOT/fixtures/*.lba2polyrec` file.

Each test runs `tests/SNAPSHOT/compare_polyrec.sh`, which:

1. replays the recording through `replay_polyrec_asm`
2. replays the same recording through `replay_polyrec_cpp`
3. compares the raw framebuffers with `cmp`

If the buffers differ, the script reports size information and the first byte
differences.

### Replay Utilities

The snapshot test directory contains several useful utilities:

- `compare_polyrec.sh` compares ASM and CPP replay results.
- `render_polyrec.sh` renders ASM, CPP, and reference outputs to raw and PPM
  files for visual inspection, and can optionally render one draw call per
  output set.
- `bisect_polyrec.sh` binary-searches for the first divergent draw call.
- `replay_polyrec_main.cpp` supports replay slicing with `--start-after` and
  `--stop-after`, optional framebuffer dumps with `--ppm`, reference dumps with
  `--ref-ppm`, z-buffer dumps with `--zbuf`, and event inspection with
  `--dump`.

Typical investigation flow:

```bash
./run_tests_docker.sh --bisect
./run_tests_docker.sh --render --polyrec polyrec_0002 --stop-after 3226
```

Once the first divergent draw call is known, convert that concrete failure into
the smallest possible unit regression in `tests/pol_work/` or another relevant
test directory.

## Related Files

- `ASM_VALIDATION_PROGRESS.md` tracks which ASM/CPP pairs are already covered.
- `tests/test_harness.h` defines the assertion and test runner macros.
- `tests/cmake/asm_test_helpers.cmake` defines `add_asm_cpp_test(...)`.
- `run_tests_docker.sh` is the supported entry point for the suite.
- `tests/SNAPSHOT/` contains the polyrec replay and comparison tooling.