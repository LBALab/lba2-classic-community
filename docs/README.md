# Documentation

Index of documentation in this repository.

**New to the fork?** Start with the root [README](../README.md) (First clone + prerequisites), then [GAME_DATA.md](GAME_DATA.md) for retail files and discovery (you are not required to use a fixed layout — see overrides there).

## Engine reference

| Doc | Description |
|-----|-------------|
| [GLOSSARY.md](GLOSSARY.md) | Domain terms (Cube, Zone, T_OBJET, scripts, hero, collision, enums) with code locations. |
| [LIFECYCLES.md](LIFECYCLES.md) | Main loop order, scene load, object/hero/animation lifecycles and where they live in code. |
| [SCENES.md](SCENES.md) | All 223 cubes by island with location names; interior/exterior; object and zone counts. |
| [ZONES.md](ZONES.md) | The scene trigger layer: what each of the ten zone types reads from its eight opaque `Info` slots, the `Info7` flag word, camera zones and the authored shot they carry (and which parts of it the Auto camera honours), and inspecting zones with `zonelist`. |
| [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md) | Effects subsystem: IMPACT bytecode + FLOW particle emitters + POF wireframe shapes — runtimes (`DoImpact`, `CreateParticleFlow`, `PofDisplay3DExt`), the on-disk formats, the shipped data, and decoder/compiler tools (`scripts/dev/impact_disasm.py`, `flow_dump.py`, `pof_dump.py`). |
| [MENU.md](MENU.md) | Game menu flow, layout, localization, submenus, and entry points. |
| [TEXT.md](TEXT.md) | Text and localization: the `TEXT.HQR` format (language x bank entry pairs, order/text banks, the attribute byte), id resolution, the two fonts and their codepages, the dialogue engine, and why community strings must live in source. |
| [CONFIG.md](CONFIG.md) | lba2.cfg lifecycle, keys, and what each does (original vs community). |
| [VERSIONS.md](VERSIONS.md) | Every "version" field in the engine and what it does: `DistribVersion` (the `Version` config key) and its six branch sites, the dead `Version_US`, installer-only keys, save-layout `NUM_VERSION`, and what the pressed discs actually declare. |
| [SAVEGAME.md](SAVEGAME.md) | .lba save format: lifecycle, binary layout, version compatibility, save editors, LBALab tools. |
| [CAMERA.md](CAMERA.md) | Camera system: interior (iso) vs exterior (perspective), CameraCenter, Auto camera (`FollowCamera`, community addition). |
| [TIMING.md](TIMING.md) | Engine timing: TimerSystemHR vs TimerRefHR, LockTimer vs SaveTimer semantics, ManageTime call sites, fixed-dt overlay, and the 1997 `ManageTime` bug history. |
| [TRANSITIONS.md](TRANSITIONS.md) | Scene and FMV transitions: the fade-out / load / fade-in reveal, why the palette must be black when a scene is composited, `PlayAcf`'s leave-black contract, the `FlagFade`/`FlagBlackPal` state, and the pop regression classes (#353/#404). |
| [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md) | Why movement speed is frame-rate dependent (#358): animation-baked locomotion, the two couplings in `ObjectSetInterDep`, evidence, and the fixed-simulation-timestep fix. |
| [SPRITES.md](SPRITES.md) | Sprite system: UI / world-extra / 3D-anim lanes, `ScaleSprite` vs `ScaleSpriteTransp`, perspective scale (`CalculeScaleFactorSprite`), sort-tree integration. Magic-ball case study. |
| [LBA_EDITOR.md](LBA_EDITOR.md) | What `LBA_EDITOR`/`PERSO` paths still do: editor capabilities, runtime hooks, and likely missing pieces. |

## Architecture

The engine mapped as a whole: layers, the engine/game membrane, and the on-disk data contract. Start with [ARCHITECTURE.md](ARCHITECTURE.md).

| Doc | Description |
|-----|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Overview + roadmap: the three axes (structure / layer / time), the engine/game membrane, the north-star (one engine, two games). |
| [ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md) | Structure axis: which of the ~200 globals each domain owns; the shared-state bus. |
| [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md) | Layer axis: engine vs game vs platform, with every module pinned (live/dormant) and the dual-path format seams. |
| [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md) | The engine↔game membrane: the Life/Track per-object script VM. |
| [ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md) | Data contract: the Adeline on-disk formats (HQR, LZ, body, anim, sprite, samples, XMIDI, XCF), each with its cross-title version timeline. |
| [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md) | Per-subsystem cost of hosting LBA1 on this engine; verified against LBA1 retail data. |

## Build & debug

| Doc | Description |
|-----|-------------|
| [TOOLING.md](TOOLING.md) | Every external tool the repo expects, tiered by what breaks without it: build & run, pass review, per lane, nice to have. Version floors are cited, never restated. `scripts/dev/check-tooling.sh` probes the lot. |
| [WINDOWS.md](WINDOWS.md) | Building on Windows with MSYS2; game files, toolchain. |
| [ANDROID.md](ANDROID.md) | Building, packaging, and running on Android (arm64-v8a / armeabi-v7a): NDK + SDL3 cross-build, APK bundler, 16 KB pages, game-data placement, touch overlay. |
| [GAME_DATA.md](GAME_DATA.md) | Retail game files: `LBA2_GAME_DIR`, `--game-dir`, discovery order, dev layouts. |
| [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md) | Reading retail assets straight from a raw ISO/BIN disc image (GOG `LBA2.GOG`): ISO9660 reader, mount + `OpenRead` fallback, in-image music. Plus the retail CD assessment (US "Twinsen's Odyssey" rip) and the plan for CD-DA music, other containers and multi-track cues. |
| [DEBUG.md](DEBUG.md) | Original Adeline debug tools (DEBUG_TOOLS=ON): overlay, F9 screenshot, bug save/load, cheats, scene selection. |
| [CONSOLE.md](CONSOLE.md) | Quake-style debug console (always available): backtick/F12, commands and cvars. |
| [CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md) | Runbook for a crash you already have: ASan builds (preload or static link), inspecting state at the fault, reading ASan output, pointer-arithmetic traps, and pinning the fix with a regression test. |
| [BUG_HUNTING.md](BUG_HUNTING.md) | Runbook for finding defects nobody has reported: driving the control socket through player-shaped sequences under ASan/UBSan, un-merging the memory arena so spills are visible at all, and the oracle discipline that decides whether a green result means anything. |
| [CONTROL.md](CONTROL.md) | CLI control harness: drive the engine non-interactively (`--load`/`--exec`/`--tick`/`--dump-state`/`--screenshot`/`--exit`) for automation and regression. |
| [RECORDING.md](RECORDING.md) | Session recording: capture a played session and replay it into the same simulation, with a per-tick state digest naming the first tick that stops matching. |
| [RELEASING.md](RELEASING.md) | Maintainer recipe for cutting a release: versioning, the `1.0` bar, `git-cliff`, engine version vs `NUM_VERSION`. |
| [CI.md](CI.md) | GitHub Actions workflows: validation vs release tiers, triggers, path filtering, the docs-only gate, branch protection. |

## Testing

| Doc | Description |
|-----|-------------|
| [TESTING.md](TESTING.md) | Test suite architecture, Docker ASM equivalence, host discovery tests, and CI workflow summary. |
| [POLYREC.md](POLYREC.md) | Polygon record/replay harness: `.lba2polyrec` format, capture and replay pipelines, what the byte-for-byte comparison checks, scope, and extension points. |
| [PERFTRACE.md](PERFTRACE.md) | Per-frame timing capture: console-driven ring buffer for diagnosing high-res / platform-specific frame-pacing without rebuilding. |

## Preservation & culture

| Doc | Description |
|-----|-------------|
| [FRENCH_COMMENTS.md](FRENCH_COMMENTS.md) | Curated French comments from the codebase with English translations. |
| [SPEEDRUN_MECHANICS.md](SPEEDRUN_MECHANICS.md) | Why the movement techniques used in runs work, read from the engine: the behaviour-switch animation rewind, the take-off foot chosen by animation keyframe events, hold versus re-press, simultaneous-opposite resolution, and what preserving the original behaviour alongside fixes would take. |
| [ASCII_ART.md](ASCII_ART.md) | Catalog of ASCII art banners in the original source files. |

## Porting & technical

| Doc | Description |
|-----|-------------|
| [BIT_EXACTNESS.md](BIT_EXACTNESS.md) | What "bit exact" / "byte identical" actually means here: the three kinds (format contract, ASM-parity oracle, regression tripwire), when byte-identity is the goal vs a proxy, and the rule for when a byte diff is acceptable. |
| [CONTROLLER.md](CONTROLLER.md) | Manual camera (orbit, elevation, zoom) and the input sources that drive it: keyboard, mouse, gamepad. |
| [FEATURE_WORKFLOW.md](FEATURE_WORKFLOW.md) | Reasoning and docs for big features: console commands, headless mode, menu changes, camera. Plus the order to run a refactor in, and what a refactor has to promise. |
| [AUDIO.md](AUDIO.md) | Audio system: AIL contract, SDL backend, sound scripting patterns, known issues. |
| [MUSIC.md](MUSIC.md) | Music state machine: track routing, the `PlayMusic` decision + `NextMusic` deferred-switch queue, the two-layer pause/park model + `STREAM_PARK.H` seam, WAV vs OGG decode/cache, and the host test coverage. |
| [ASM_TO_CPP_REFERENCE.md](ASM_TO_CPP_REFERENCE.md) | Which modules are ported from ASM to C++ in this fork. |
| [ASM_VALIDATION_PROGRESS.md](ASM_VALIDATION_PROGRESS.md) | Per-pair equivalence-test status across LIB386 ASM/CPP pairs. |
| [ASM_TEST_COVERAGE_AUDIT.md](ASM_TEST_COVERAGE_AUDIT.md) | Rubric and progress for strengthening existing equivalence-test coverage (branches, side effects, edge inputs). |
| [COMPILER_NOTES.md](COMPILER_NOTES.md) | Calling conventions and compiler-related notes. |
| [GFX_OPTIONS.md](GFX_OPTIONS.md) | Variables and locations for graphical quality options. |
| [WIDESCREEN.md](WIDESCREEN.md) | Widescreen / higher-resolution plan: render-vs-UI coordinate spaces, phased roadmap, current status. |
| [WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md) | Projection 4:3 audit: where projection hardcodes the screen centre and 640, culling/preclip sites, and what PR #134 did and did not route. |
| [ABI.md](ABI.md) | Rule for reading 32-bit DOS-era data on 64-bit hosts; catalogue of fat types; compile-time guards. |
| [PLATFORM.md](PLATFORM.md) | High-level map of host assumptions (pointer ABI, endianness, FP precision, ASM, OS boundary) with status badges and next-step pointers. |

## Plans & research

Design trails, in [plan/](plan/). A plan is written before the work and kept afterwards as
the record of why it was built that way; the reference docs above describe what the engine
does today, and are the ones to read first. **Implemented** means the plan landed and the
doc is now history: where it disagrees with the code, the code wins.

| Doc | Status | Description |
|-----|--------|-------------|
| [AUTOMATION_RESEARCH.md](plan/AUTOMATION_RESEARCH.md) | Implemented | Phase 1 findings behind the CLI harness: what could be driven from outside the game loop, and at what cost. |
| [AUTOMATION_PLAN.md](plan/AUTOMATION_PLAN.md) | Implemented | Design trail for the `CONTROL` module (`--load`, `--exec`, `--tick`, `--dump-state`), with as-built notes on where the implementation diverged. See [CONTROL.md](CONTROL.md) for usage. |
| [FIXED_DT_RESEARCH.md](plan/FIXED_DT_RESEARCH.md) | Implemented | Phase 1 mapping of the engine's loop classes and which of them wall-clock timing makes irreproducible. |
| [FIXED_DT_PLAN.md](plan/FIXED_DT_PLAN.md) | Implemented | Design for `--fixed-dt <ms>`, the harness-only virtual clock that makes `--dump-state` byte-reproducible. See [TIMING.md](TIMING.md) for the clocks as they stand. |
| [INPUT_SIM_PLAN.md](plan/INPUT_SIM_PLAN.md) | Implemented | Design for driving the hero headlessly with sustained, combinable input (`input seq` / `fseq`), and the throttle-drop regression class it exists to catch. |
| [SAVE_WIRE_PLAN.md](plan/SAVE_WIRE_PLAN.md) | Implemented | Derivation of the bit-exact 32-bit save wire format (276-byte stride) and the four layout decisions behind `SAVEGAME_WIRE`. |
| [PORTABILITY_PLAN.md](plan/PORTABILITY_PLAN.md) | Implemented | Keeping several installs out of each other's saves and settings: `--user-dir`, `--profile`, the `portable.txt` marker, and the config read as a chain rather than a copy. |
| [RELEASE_DETECTION_PLAN.md](plan/RELEASE_DETECTION_PLAN.md) | Implemented | Why the release is read off the data as well as the config: what `DistribVersion` decides, and separating the publisher splash from the release identity. See [VERSIONS.md](VERSIONS.md). |
| [BOOT_LOG_PLAN.md](plan/BOOT_LOG_PLAN.md) | Implemented, partly superseded | Boot log and exit screen. Its per-sink severity model was reworked into the single global log level described in [LOGGING_UNIFICATION.md](LOGGING_UNIFICATION.md). |
| [INIT_RESEARCH.md](plan/INIT_RESEARCH.md) | Research | Initialisation path from `main` to a running scene: boot phases, new-game vs load-save, timing/speed model, cleanup candidates, verbatim TODO inventory. |
| [INPUT_REPLAY_RESEARCH.md](plan/INPUT_REPLAY_RESEARCH.md) | Research | Capturing input per tick and replaying a session as a gameplay-regression net, the counterpart to the draw-call [polyrec](POLYREC.md). No implementation. |
| [INPUT_RESEARCH.md](plan/INPUT_RESEARCH.md) | Research | The input system as a whole: the two funnels and which devices depend on the unbindable one, what each device can and cannot express, the open issues clustered, and nine gaps no issue records. |
| [INPUT_DOOM3_RESEARCH.md](plan/INPUT_DOOM3_RESEARCH.md) | Research | Doom 3's input path read from the GPL release, and which of its decisions apply here: fixed sample rate, impulses off level bits, per-subsystem inhibit, and command demos with a consistency hash. |
| [RECORDING_RESEARCH.md](plan/RECORDING_RESEARCH.md) | Research | Where a Doom 3 style recording system would attach: the four layers it could sit at, the one function wide enough to carry the whole digital funnel and the three analog values that sit outside it, why the index is input polls rather than ticks, and what a recording carrying its own state hash buys for fixtures, attract demos and bug reports. Tested with an uncommitted prototype: a keyboard-driven session replays bit-for-bit, an existing fixture's trajectory replays four times faster than the fixture runs, and the clock, not the input or the oracle, is what a real session costs. |
| [DIGEST_MEMBERSHIP.md](plan/DIGEST_MEMBERSHIP.md) | Implemented | A membership rule for the state digest, so a field is compared only when a replay can be made to start from it. Four classes, declared where each field is hashed. The five globals no savegame carries are carried in the recording and installed by the replay rather than dropped from the hash, because all five reach the simulation; measured on nine contributed recordings, seven mismatch at tick 0 with nothing having diverged. Prices grouped per-tick hashes and drops them: a plain tick is 19.8 bytes and five group hashes are 40. Companion to [RECORDING.md](RECORDING.md). |
| [ENGINE_RENDER_SPLIT_RESEARCH.md](plan/ENGINE_RENDER_SPLIT_RESEARCH.md) | Research | What it would take to give the simulation a clock of its own the way Doom does. Half of it shipped with #358; the rest is five couplings, measured: 21 modal loops that own the clock, 46 renderer calls from game code, the world writes in `AffScene`, a present that is also the tick and the event pump, and input sampled per frame rather than per tick. Companion to [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md) and [TIMING.md](TIMING.md). |
| [ENGINE_TICK_POLICY_SURVEY.md](plan/ENGINE_TICK_POLICY_SURVEY.md) | Research | Step A of the ladder in [ENGINE_RENDER_SPLIT_RESEARCH.md](plan/ENGINE_RENDER_SPLIT_RESEARCH.md), surveyed and measured: whether the four policies that decide when to mint virtual time can become one, what a present costs at each modal surface, the two loops that deadlock if presents stop minting, and the waits that have no clock source at all under a pinned clock. |
| [INPUT_PLAN.md](plan/INPUT_PLAN.md) | Proposed | Awaiting go/no-go: seven increments making input a subsystem, ordered so each de-risks the next. Scope excludes a new control scheme, with the reason measured rather than preferred. Companion to [SPEEDRUN_MECHANICS.md](SPEEDRUN_MECHANICS.md). |
| [LBA1_PORT_PLAN.md](plan/LBA1_PORT_PLAN.md) | Proposed | Awaiting go/no-go: how to bring LBA1 to this groundwork. Costs three paths, recommends hosting LBA1 content on the lba2cc engine behind a game-id, with a feasibility-spike ladder. Companion to [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md). |
| [PLATFORM_PAL_PLAN.md](plan/PLATFORM_PAL_PLAN.md) | Proposed | Awaiting go/no-go: in-place Platform Abstraction Layer decoupling the engine from direct SDL3. SDL-surface audit, RFC #120 reconciliation, PR-sequenced extraction with a headless backend. |
| [REFACTOR_ROADMAP.md](plan/REFACTOR_ROADMAP.md) | Living | Which areas are worth restructuring and what each one buys, ordered by what tests cover them rather than by how untidy they are. Measures what CI can actually see: 5% of `SOURCES/`, and no 1997 game logic at all. Companion to [FEATURE_WORKFLOW.md](FEATURE_WORKFLOW.md) Example 5. |
| [ARCH_RULES_PLAN.md](plan/ARCH_RULES_PLAN.md) | Implemented | Checking the boundaries the docs already state: seven rules from CODESTYLE and AGENTS.md, each measured against the tree, plus the four candidates deliberately left out. Companion to [REFACTOR_ROADMAP.md](plan/REFACTOR_ROADMAP.md). |
| [RENDER_INTERP_PLAN.md](plan/RENDER_INTERP_PLAN.md) | Proposed | Smooth motion above the sim rate (#412), building on the fixed-timestep sim in [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md). Nothing landed. |

## External resources

| Resource | Description |
|----------|-------------|
| [LBA Classic Doc](https://lba-classic-doc.readthedocs.io/) | Read the Docs: engine documentation. |
| [README Links](../README.md#links) | Official site, Discord, GOG/Steam. |
