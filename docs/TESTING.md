# Testing

This repository uses one testing strategy throughout: compare the original ASM
implementation against the C/C++ port and treat the ASM result as the source of
truth. The tests are designed to catch behavioral drift during the ongoing
porting work in `LIB386/`.

## Core rule

All equivalence tests must compare ASM and CPP results exactly.

- Use `ASSERT_ASM_CPP_EQ_INT` for scalar values.
- Use `ASSERT_ASM_CPP_MEM_EQ` for buffers, framebuffers, tables, and structs.
- Do not add "no crash" tests.
- Do not weaken comparisons with tolerances when the goal is ASM equivalence.
- If a single byte differs, fix the CPP implementation or isolate the mismatch
  with a smaller test.

For current coverage and status, see [`ASM_VALIDATION_PROGRESS.md`](ASM_VALIDATION_PROGRESS.md).

## CI (GitHub Actions)

Two CI workflows exercise the test suite: the host build workflows
(`linux.yml` / `macos.yml` / `windows.yml`) run `ctest -L host_quick`,
and `test.yml` runs the full Docker ASM↔C++ equivalence suite. Host jobs
need neither retail game files nor Docker; the Docker job does not run
the host discovery tests.

For the full workflow map, triggers, path filtering, and branch
protection, see [CI.md](CI.md).

## What is in scope

The repo currently uses three complementary testing workflows.

### 1. Unit and equivalence tests

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

### 2. Polygon recording replay (`.lba2polyrec`)

For rendering bugs that are hard to reproduce with small synthetic fixtures, the
game can record real polygon draw calls into `.lba2polyrec` files. Replay tools
then run those exact recorded calls through both ASM and CPP renderers and
compare the resulting framebuffers byte-for-byte.

This is the main workflow for investigating polygon filler mismatches.

### 3. Focused numerical / precision tests

Some mismatches come from x87 behavior, intermediate rounding, or calling
convention details rather than high-level logic. The `tests/fpu_precision`
directory contains targeted ASM-vs-CPP tests for those low-level cases.

### 4. Host tests — game data discovery (`tests/discovery`)

`tests/discovery/test_res_discovery.cpp` exercises `ResolveGameDataDir` (`LBA2_GAME_DIR`, `--game-dir` stripping), parent-directory sibling discovery (retail folder or `CommonClassic` next to a fake `repo_clone` via `chdir`), and embedded default `lba2.cfg` writing. These run on the host (no Docker, no 32-bit ASM). Configure with `-DLBA2_BUILD_TESTS=ON` and `-DLBA2_BUILD_ASM_EQUIV_TESTS=OFF` if you only need host tests (no `objcopy`; used by `make test` and PR host jobs).

### 5. Host tests — console (`tests/console`)

`tests/console/test_console_state.cpp` links the `console` library and exercises `SOURCES/CONSOLE/CONSOLE_STATE.CPP`: availability strings (same contract as `console_avail_in_game_scene` in `CONSOLE_CMD.CPP`), legacy video/menu/game labels from `Console_ModeString_FromState` (for tests), and `Console_FormatStatusIslandLine_FromState` (first line of the `status` command: island, cube, chapter; raw values like `cmd_status`, not give-style scene masking). No retail `LBA2.HQR` or full engine init.

`tests/console/test_console_commands.cpp` covers parser/output integration in `SOURCES/CONSOLE/CONSOLE.CPP` using host-only hooks: built-in `help` and `cmdlist`, unknown-command diagnostics, cvar set/get (`fps`), and a registered `status` command path that prints the same status headline format.

### 6. Host tests — disc-image source (`tests/iso9660`, `tests/cue`, `tests/disc_image`)

These cover the disc-image resource source ([DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md)), along with `tests/cdtracks`; all are SDL-free and asset-free, run against synthetic fixtures built in-test (no retail image on disk).

`tests/iso9660/test_iso9660.cpp` builds a minimal ISO9660 image in memory (PVD at LBA 16, a root directory with files), packs it both as 2352-byte raw (Mode-1) and 2048-byte cooked sectors, and proves the reader auto-detects the layout, reads a file by path (case-insensitively, ignoring the `;1` version), streams cross-sector ranges and clamps at EOF (`iso_stat` / `iso_pread`), enumerates the tree (`iso_walk`), and rejects a non-ISO file. Two hardening cases assert a malformed image (a directory record whose name runs past its extent) and a truncated one (a root extent pointing past EOF) stop cleanly instead of reading out of bounds; run them under the `linux_sanitize` preset to catch a bounds regression.

`tests/cue/test_cue.cpp` exercises the cue reader (`LIB386/SYSTEM/CUE.CPP`): the first external (non-BINARY) audio track wins, a data-only cue has no external audio, and AUDIO tracks nested under a BINARY file (in-image CD-DA) are skipped. It also covers the full track table: numbers are taken from the sheet and may have holes (a rip that lost a track must still address the survivors by their true numbers), `INDEX 01` is the start rather than the `INDEX 00` pregap, tracks past the caller's capacity do not let their `INDEX` corrupt the last one kept, and `Cue_ImageForSheet` resolves the image a sheet names against the sheet's own directory while reporting "not a cue" for anything else.

`tests/cdtracks/test_cdtracks.cpp` pins the CD-track map (`LIB386/SYSTEM/CDTRACKS.CPP`), which decides which sectors a music request plays and so fails silently when wrong: the lookup both ways, its bounds, path and case handling, and two checks that read `SOURCES/MUSIC.CPP` rather than linking it, so the table cannot drift from `ListJingle`, and so `TrackCD`/`TrackCDUS` keep the identical music numbers that let `PlayMusic` route both releases through one path.

`tests/disc_image/test_disc_image.cpp` links `DISCIMG.CPP` against a synthetic nested image and checks mount + banner info, asset-root detection, path resolution under the mount base, file/dir existence, misses, and unmount. Two more: the marker probe discovery uses before anything is mounted, and that a raw image beats a cooked one regardless of name or directory order.

### 6b. Retail distribution sweep (`scripts/dev/dist_check.sh`, local, opt-in)

Needs real installs, so it cannot run in CI. It exists because the retail releases are not interchangeable: each states its own `Version` in the `lba2.cfg` in its install folder, that config is read underneath the profile's own on every boot, and the value picks the music track table and two logo sprites. A change can be right on one distribution and wrong on another.

```bash
scripts/dev/dist_check.sh [outdir]          # defaults to ../LBA2/Common, ../LBA2-GOG, ../TWINSEN
LBA2_DIST_LIST="name:/path" scripts/dev/dist_check.sh
```

Each install gets a profile of its own inside one throwaway user directory, so what is measured is the new-user path rather than the developer's own settings, and every run goes through profile path composition. One row per install: release identity, language, whether an image mounted, the asset preflight, how each of three music requests was actually served, and pixel hashes (`scripts/dev/png_hash.py`) of six captures: an interior scene, an exterior one, three UI modals, and a demo-mode frame.

It is a check, not only a report, and exits non-zero when any of six assertions fails:

| | |
|---|---|
| `IDENTITY` | the release the engine reports matches the `Version` the install's own config declares, read off the file rather than out of the engine. `-` for a disc image, whose config is inside it |
| `WROTE` | nothing under the game directory changes. An install is input; a config read out of it must not be written back, which also has to hold on read-only media |
| `BOUND` | naming the profile and nothing else finds the folder that profile was given |
| `ISOLATION` | the folder the engine writes to when nobody tells it otherwise is untouched across the whole sweep, which is what proves `--user-dir` took |
| `REBIND` | a profile seeded from one release, pointed at another, reports the second |
| `MASTER` | the banks do not contradict the declaration. A row that fails this is an install assembled from two sources, drawing one publisher's sprites over another's data |

`WROTE` earned its place immediately: it caught the CD voice cache stamping mtimes on installed game data, in code nothing was reading.

### 6c. Platform contract and SDL conformance (`tests/platform`)

These are host-only tests for community platform infrastructure with no ASM original. `test_platform_contract` links real engine facades and input policy against a fixture table without SDL headers or libraries. It pins input-hook order, device/focus/timer coupling, fixed-dt and clock wrap, gamepad/touch policy, palette snapshots, and nested presentation/overlay order.

`test_platform_sdl3` and `test_platform_sdl3_staging` exercise the selected SDL3 implementation, including byte-exact event payloads, virtual-gamepad sampling, worker-thread dialog/error ownership, and full-frame pixels read from the actual SDL renderer. The second target forces the persistent-buffer/upload mechanism used on Android; it does not replace on-device JNI/window/MTE checks. Both use dummy drivers and need no retail assets. See [PLATFORM_ABSTRACTION.md](PLATFORM_ABSTRACTION.md).

Select desktop SDL2 with `-DPLATFORM_BACKEND=sdl2 -DSOUND_BACKEND=sdl -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON`. `test_platform_sdl2` checks exact renderer output, letterboxing, untranslated native pointer coordinates, event payloads and text lifetime, controller instance IDs, filesystem services, message-box dispatch, and fullscreen rollback. `test_sdl_audio` checks the selected SDL transport; its SDL2 build additionally compares full converter buffers and shared-device mixing. `test_sdl_audio_device` exercises SDL2's production dummy-device consumption without deterministic test hooks. All require no retail assets; real hardware and native dialog UI still need manual coverage. Use genuine SDL2 for independence checks: `sdl2-compat` implements the SDL2 API using SDL3.

```bash
cmake -S . -B build-sdl2 -DPLATFORM_BACKEND=sdl2 -DSOUND_BACKEND=sdl \
  -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON \
  -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
cmake --build build-sdl2 --target lba2cc host_tests
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir build-sdl2 -L host_quick --output-on-failure
```

With `-DPLATFORM_BACKEND=null -DSOUND_BACKEND=null -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON`, the whole game and host suite link without SDL. The null conformance test compares complete offscreen frames and overlays; `test_platform_null_game` runs the actual executable's help and missing-data boot with an isolated writable directory. Linux CI configures this independently of its SDL installation job and rejects SDL dynamic linkage. The keyboard-layout test may skip because a null backend has no physical keymap; the shared contract fixture still pins injected-key policy.

```bash
cmake --build build --target host_tests
ctest --test-dir build -R '^test_platform_' --output-on-failure
```

Demo mode is in there for a reason. It is the only surface where `DistribVersion` visibly differs: the logo swapped at `OBJECT.CPP`'s "incrust logo demo" is drawn only when `DemoSlide` is set, so every other capture matches across releases even though the releases are identified differently. In a good run the demo hash agrees for the two releases that map to the same sprite and differs for the third.

**Pass `--fixed-dt` to anything that captures a UI screen.** The menus animate a plasma strip on the clock, so without a pinned clock the same screen hashes differently on every run, and a comparison built on it reports regressions that are not there. This is worth knowing beyond this script: it applies to any UI capture in the engine. With `--fixed-dt` the captures are exactly reproducible and need no masking at all, which is why `png_hash.py` is a plain pixel hash.

Hashes are a per-install baseline to diff against a later run (`diff a/summary.txt b/summary.txt`). Do not compare them across installs: US and EU legitimately differ in language, in volume defaults inherited from each install's config, and in that demo logo.

### 6c. Release detection (`tests/automation/test_release_detection.sh`, local)

[tests/distrib](../tests/distrib/test_distrib_resolve.cpp) pins the rules between a declaration and a measurement and needs no assets, so it runs in CI. This is the other half, against a real install: that the measurement happens, that it reaches the banks wherever they live including inside a mounted disc image, and that the two are compared rather than one standing in for the other.

```bash
LBA2_GAME_DIR=/path/to/data bash tests/automation/test_release_detection.sh
```

The declaration side is driven from the profile, never from the install. Writing a config beside the game data to set up a test would be the defect `WROTE` exists to catch, and would not work at all on an install mounted from an image. A tree the size table has no entry for skips rather than fails, which is the same stance the table itself takes.

### 6d. Config upgrade path (`tests/automation/test_config_upgrade.sh`, local)

The sweep above measures the new-user path: every profile in it is one the engine just created. The other direction is the player who already has an `lba2.cfg` and updates the engine under it, and it is the one the layered read changed the mechanics of, so it gets a test of its own.

```bash
LBA2_GAME_DIR=/path/to/data bash tests/automation/test_config_upgrade.sh
```

It writes a config by hand with values that all differ from the compiled defaults, runs, and reads the file back. Four assertions:

1. The keys the player owns come back out unchanged, and none are dropped.
2. A key the game data owns is answered on every boot and is never copied into the profile, so it cannot outlive the install it came from. Checked twice: the key must be absent from the written file, and the same install must report the same release to an upgrading player as to a new one.
3. A config naming a `Version` itself still beats the one under it.
4. An empty config is a config to fill in, not a fatal.

`Shadow` is the interesting exception and is asserted separately: `SetDetailLevel` re-derives it from `DetailLevel` at boot, so it is a key the engine writes but does not keep. Without that assertion the next reader finds a value that changed by itself and reads it as a config bug.

The invariant is not "matches an older build". That would need a second binary and could only be measured once, whereas what actually decays is the layering, and a player's settings resolving differently after an update is not something anyone reports as a bug.

PR host jobs and `make test` build the `host_tests` aggregate target, then:

```bash
cd build && ctest -L host_quick --output-on-failure
```

The `host_quick` label is set by `register_host_test()` in
`tests/CMakeLists.txt`; each test's own CMakeLists opts in with one
line. Adding a new host test requires no workflow or Makefile edits.

To run only the console suite: `ctest -R test_console_ --output-on-failure`.

### 6. Host tests — savegame (`tests/savegame`)

`tests/savegame/test_load_bounds.cpp` (issue #62) links [SOURCES/SAVEGAME_LOAD_BOUNDS.CPP](../SOURCES/SAVEGAME_LOAD_BOUNDS.CPP) and checks `SaveLoadValidateCompressedStaging` / `SaveLoadGuessObjectWireStride` on synthetic buffers — pure parser bounds, no retail game data. Built and run as part of `make test` / PR host jobs.

`tests/savegame/corpus/` holds the Layer-3 driver scripts (`run_harness.py`, `build_manifest.py`) that exercise the real `lba2cc --save-load-test` flag against a directory of `.lba` saves; requires retail game data so it runs locally only. See `tests/savegame/corpus/README.md`.

### 7. Host tests — user directories (`tests/user_dirs`)

`tests/user_dirs/test_user_dirs.cpp` links [SOURCES/DIRECTORIES.CPP](../SOURCES/DIRECTORIES.CPP) and exercises `AddDirIfNot`, which every user-writable folder is made through: a nested path with a trailing separator, a second call on a folder already there, and a name taken by a file.

The last case is why it is a host test rather than a fixture. It moves the process to the root of the volume the folder is going on and creates from there, which on Windows makes the first component of an absolute path (`C:`) resolve to a directory that cannot be created. That is the shape that silently created nothing for years, and `tests/automation/test_user_dirs.sh` can only reach it on a Windows machine somebody runs it on by hand. This one runs in `ctest -L host_quick`, including the Windows PR job.

### 8. Camera fixtures (`tests/automation`, local)

The camera is guarded in two places. CI runs `tests/camera/test_followcam_math.cpp`, which covers the angle and ease arithmetic in `FOLLOWCAM_MATH.H` over its whole domain and needs no data. The behavioural fixtures below drive the real engine, so they need retail data and a display and run in no CI workflow: a camera change CI passes has had its arithmetic checked and its behaviour not. Run every `test_*cam*.sh` on both data sets, the default and the US disc. How the camera works is in [CAMERA_INTERNALS.md](CAMERA_INTERNALS.md).

`camlib.sh` turns a run's `camtrace` into a per-frame table, so each fixture asserts on the shape of a motion rather than one end state. The trace fields are listed under `camtrace` in [CONSOLE.md](CONSOLE.md); `camnudge` drives the analog camera without a device, `--dump-state` carries a `camera` block and `zonelist` turns "get into a camera zone" into a `cube` plus `teleport`.

| fixture | what it pins |
| --- | --- |
| `autocam_orbit_snap` | a 1-unit touch moves the camera 1 unit, with a turn's worth of rotation pending |
| `followcam_hold` | the angle survives the hero turning underneath it |
| `followcam_release` | letting go eases down instead of halting |
| `followcam_tracking` | the camera orbits at the speed asked for |
| `followcam_recenter` | the classic camera drifts back while walking, not while standing |
| `followcam_interrupt` | an interrupted gesture ends instead of resuming |
| `followcam_obstruct` | with `cam_obstruct`, the tilt over a building is an extra that comes and goes with the building, the player's pitch is never rewritten, and the hero is never hidden or recentred |
| `followcam_ease` | with `cam_ease_ms`, the turn behind the hero takes the time the setting says at either update length and lands on its target; with `cam_target_ms`, the look-at point trails the hero, closes after he stops, and keeps its trail across the seam between two scenes |
| `followcam_scene_carry` | with `cam_scene_carry`, the camera arrives behind the hero with the pitch left alone: the pan kept across an ordered change between two outdoor scenes, and sent home after a visit to a building |
| `followcam_world_anchor` | with `cam_world_anchor`, the target holds still while the hero turns on the spot and while walking, an orbit still moves it, with `cam_hold_angle 0` it comes back behind him by the ground he walks (not during the grace, a turn on the spot, standing or backing up), and a holding shot gives back the world direction the camera had before it |
| `camzone_model` | the two shapes a camera zone comes in |
| `camzone_skip` | with `cam_zone_skip`, a latching zone leaves a steering player's view alone under the Auto camera; forced zones and the classic camera are unaffected |
| `camzone_hold` | both cameras leave an authored shot in the same place |
| `camzone_return` | with `cam_zone_return` or `cam_zone_anchor` on, a shot's yaw and pitch come back on leaving its box, the player's own stay, and a latching shot holds its world angle |
| `camzone_blend` | with `cam_shot_blend_ms`, the frame blends into a shot and out of it over the time the setting says, starting on the view on screen and ending on the camera's own, with the camera's own state as it is with the setting at 0; Center camera in the middle of one is still a cut |

Most start from `CAM_PLAIN`, which is `settings_before cam_`: every Auto camera setting that switches on a later behaviour is set to the value that switches it off, and the fixture switches on what it measures. A new setting of that kind declares that value on its row in `FOLLOWCAM.CPP` (`SETTING_BEFORE`), and `make settings-before-check` refuses a `cam_` row that says neither `SETTING_BEFORE(v)` nor `SETTING_BEFORE_NONE`.

Things that have caught camera work out:

- **Run a new fixture against a build without the thing it guards** and confirm it fails there. Several passed at first because their setup never happened.
- **Assert the cause when the symptom is unobservable.** The orbit realign runs in the input pass, before the camera update logs anything, so on a working engine the divergence it corrects leaves no trace.
- **A `camtrace` row is a simulation step, not a rendered frame.** Under `--fixed-dt` above 16 one tick is several rows, so a motion's time is its rows times the step.
- **Check at 480 and at a tall height.** The HD recompose is a no-op at 480, so a change measured at one height has not met the other's framing.
- **A render-only change can still reach state**: the hidden-hero recovery reads what was drawn. Compare every trace field the change does not show against the run with it off, as `test_camzone_blend.sh` does.
- **Count steps from `$LBA2_USER_DIR/adeline.log`**, not the stdout capture, where lines collide with the console echo.

## Control-harness tiers and timing

`tests/automation/run.sh` times every fixture, and each fixture declares what it costs, so a change can be checked in seconds and the whole suite kept for when it matters.

```bash
tests/automation/run.sh --fast -j 8        # the small tier: about 10 s on an Apple Silicon Mac
tests/automation/run.sh -j 8               # everything: about 4 minutes there
tests/automation/run.sh camzone followcam  # fixtures whose name has one of these words
tests/automation/run.sh --tier medium      # one tier (or a list: small,medium)
tests/automation/run.sh --list             # each fixture's tier and last time
tests/automation/run.sh --profile ring     # also count engine runs and engine seconds
```

**Tiers.** A `# Tier:` line just before `TESTNAME=` puts a fixture in one, by what it costs on its own on an Apple Silicon Mac: **small** within 3 s, **medium** within 15 s, **big** beyond. `make automation-index` refuses a fixture without one. A serial run (`-j 1`) reports any fixture past its tier's budget; that is a prompt to look, not a failure, and when the fixture really is that expensive, move it up a tier.

**Telemetry.** Every run appends one line per fixture to `tests/automation/.runs/history.tsv` (untracked): date, commit, host, `-j`, fixture, tier, milliseconds, verdict. `-j` starts the slowest fixtures first from it. `--profile` puts `engine-profile.sh` in front of the binary and adds each fixture's engine runs and engine seconds to its line; a fixture whose wall time is well above its engine time is waiting on something.

**Running several at once.** Each fixture runs in a folder of its own, used as its working directory and `TMPDIR` and removed afterwards, so fixtures do not meet in a shared path and what they leak goes with the folder. `-j` needs bash 4.3 or newer (macOS's `/bin/bash` is 3.2; Homebrew's is fine). The two ring fixtures start their own engine runs, one per processor unless `RING_JOBS` says otherwise.

**What a fixture costs.** A headless run is not tied to the clock, so a fixture costs what its engine runs cost. Measured on an Apple Silicon Mac at 640x480:

- about 0.3 s to boot, so a fixture that makes 40 runs pays 12 s before it simulates anything;
- about 2 ms a tick, twice that at 1920x1080, and eight times as much under `--fixed-dt 2`, which draws eight frames a step;
- a recording (`--record`, `rec start`) is held to real time for the player making it, 16 ms a tick. The `lib.sh` helpers pass `--no-pace`, which drops the wait and records the same thing, so a fixture that calls the binary itself should too. A fixture that measures the pacing uses `ctl_paced`.

Writing a fixture that stays cheap:

- **Go through the helpers** (`ctl`, `ctl_headless`, `ring_engine`): they isolate the user folder and pass `--no-pace`.
- **Never wait out a timeout.** A run that ends in a dialogue that waits for a key never exits; `wait_for_line` stops it as soon as the line the fixture wants is in the log, instead of a fixed `timeout -s KILL`.
- **Run independent engine runs side by side** (`&` and `wait`), as `test_demo_behaviour_menu.sh` does with its two.
- **Keep to POSIX and bash.** `mktemp --suffix`, `date +%N`, `sed '\?'` and `wc`'s unpadded output are GNU-only; on a Mac they fail quietly and the fixture can pass without testing anything. `lib.sh` has `mktemp_ext` for a temp file with an extension, and falls back to Homebrew's `gtimeout` when there is no `timeout`.

## Test harness

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

## How ASM-vs-CPP tests are built

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

## Running tests

### Preferred entry point

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

On Apple Silicon, Docker Desktop runs the 32-bit test binaries under
`qemu-i386` on an arm64 kernel, which differs from CI in two ways:

- The kernel treats the emulated program as a 64-bit caller and returns 64-bit
  directory offsets, which a 32-bit `readdir` cannot hold: it fails with
  `EOVERFLOW` and lists nothing. The `linux_test` preset builds with
  `-D_FILE_OFFSET_BITS=64` so directory listings work there too.
- `test_crash_report` fails its stack-overflow cases: GCC's unwinder cannot
  step through `qemu-i386`'s signal frame, so the crash block stops after its
  first frame. A minimal program that unwinds from a signal handler fails the
  same way under `qemu-i386`, so this is the emulator, not the crash handler.
  CI runs on x86-64 and is the check for that test.

### Common commands

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

### `run_tests_docker.sh` parameters

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

## Writing or updating unit tests

When adding or modifying an ASM equivalence test:

1. Add the test source under the appropriate `tests/<module>/` directory.
2. Register it in that directory's `CMakeLists.txt` with `add_asm_cpp_test(...)`.
3. Include `tests/test_harness.h` and the relevant LIB386 header.
4. Declare ASM-side entry points as renamed `asm_*` symbols where needed.
5. Cover normal inputs, edge cases, and a deterministic random stress pass.
6. If the function writes global state, compare that global state after both
   paths run.
7. Update [`ASM_VALIDATION_PROGRESS.md`](ASM_VALIDATION_PROGRESS.md) so coverage status stays current.

For randomized stress tests, use a deterministic seed so failures are
reproducible.

## Debugging mismatches

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

## Polyrec workflow

For the harness internals — the `.lba2polyrec` format, the capture and replay
pipelines, what the byte-for-byte comparison actually checks, current scope, and
extension points — see [POLYREC.md](POLYREC.md). The rest of this section is the
operator-facing how-to.

### What polyrec captures

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

### Capturing a recording from the game

The game-side recording hook is enabled by configuring with `-DENABLE_POLY_RECORDING=ON` (the option defaults OFF; see `CMakeLists.txt`).

In a recording-enabled build:

1. Press `Alt+F9` during gameplay.
2. The game sets `SnapshotRequested`.
3. At the start of the next `AffScene()`, recording begins before rendering.
4. The engine forces a full redraw so terrain and objects are both captured.
5. At the end of that scene render, `polyrec_stop()` writes
   `polyrec_XXXX.lba2polyrec`.

New fixtures should be moved into `tests/SNAPSHOT/fixtures/` if they are meant
to become part of the regression corpus.

### Automated polyrec tests

`tests/SNAPSHOT/CMakeLists.txt` registers one CTest per
`tests/SNAPSHOT/fixtures/*.lba2polyrec` file.

Each test runs `tests/SNAPSHOT/compare_polyrec.sh`, which:

1. replays the recording through `replay_polyrec_asm`
2. replays the same recording through `replay_polyrec_cpp`
3. compares the raw framebuffers with `cmp`

If the buffers differ, the script reports size information and the first byte
differences.

### Replay utilities

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

## Related files

- [`ASM_VALIDATION_PROGRESS.md`](ASM_VALIDATION_PROGRESS.md) tracks which ASM/CPP pairs are already covered.
- `tests/test_harness.h` defines the assertion and test runner macros.
- `tests/cmake/asm_test_helpers.cmake` defines `add_asm_cpp_test(...)`.
- `run_tests_docker.sh` is the supported entry point for the suite.
- `tests/SNAPSHOT/` contains the polyrec replay and comparison tooling.
- [`POLYREC.md`](POLYREC.md) documents the polyrec harness internals: format, pipelines, scope, and extension points.