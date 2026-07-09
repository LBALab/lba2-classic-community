# Documentation

Index of documentation in this repository.

**New to the fork?** Start with the root [README](../README.md) (First clone + prerequisites), then [GAME_DATA.md](GAME_DATA.md) for retail files and discovery (you are not required to use a fixed layout — see overrides there).

## Engine reference

| Doc | Description |
|-----|-------------|
| [GLOSSARY.md](GLOSSARY.md) | Domain terms (Cube, Zone, T_OBJET, scripts, hero, collision, enums) with code locations. |
| [LIFECYCLES.md](LIFECYCLES.md) | Main loop order, scene load, object/hero/animation lifecycles and where they live in code. |
| [INIT_RESEARCH.md](INIT_RESEARCH.md) | Initialisation path from `main` to a running scene: boot phases, new-game vs load-save, timing/speed model, cleanup candidates, verbatim TODO inventory. |
| [SCENES.md](SCENES.md) | All 223 cubes by island with location names; interior/exterior; object and zone counts. |
| [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md) | Effects subsystem: IMPACT bytecode + FLOW particle emitters + POF wireframe shapes — runtimes (`DoImpact`, `CreateParticleFlow`, `PofDisplay3DExt`), the on-disk formats, the shipped data, and decoder/compiler tools (`scripts/dev/impact_disasm.py`, `flow_dump.py`, `pof_dump.py`). |
| [MENU.md](MENU.md) | Game menu flow, layout, localization, submenus, and entry points. |
| [CONFIG.md](CONFIG.md) | lba2.cfg lifecycle, keys, and what each does (original vs community). |
| [SAVEGAME.md](SAVEGAME.md) | .lba save format: lifecycle, binary layout, version compatibility, save editors, LBALab tools. |
| [CAMERA.md](CAMERA.md) | Camera system: interior (iso) vs exterior (perspective), CameraCenter, Auto camera (`FollowCamera`, community addition). |
| [TIMING.md](TIMING.md) | Engine timing: TimerSystemHR vs TimerRefHR, LockTimer vs SaveTimer semantics, ManageTime call sites, fixed-dt overlay, and the 1997 `ManageTime` bug history. |
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
| [LBA1_PORT_PLAN.md](LBA1_PORT_PLAN.md) | Plan (awaiting go/no-go): how to bring LBA1 to this groundwork. Costs three paths (native port / host on lba2cc / hybrid repo), recommends hosting LBA1 content on the lba2cc engine in-repo behind a game-id, with an agnostic-menu/shell design, twin-e as oracle, and a feasibility-spike ladder. |

## Build & debug

| Doc | Description |
|-----|-------------|
| [WINDOWS.md](WINDOWS.md) | Building on Windows with MSYS2; game files, toolchain. |
| [ANDROID.md](ANDROID.md) | Building, packaging, and running on Android (arm64-v8a / armeabi-v7a): NDK + SDL3 cross-build, APK bundler, 16 KB pages, game-data placement, touch overlay. |
| [GAME_DATA.md](GAME_DATA.md) | Retail game files: `LBA2_GAME_DIR`, `--game-dir`, discovery order, dev layouts. |
| [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md) | Reading retail assets straight from a raw ISO/BIN disc image (GOG `LBA2.GOG`): ISO9660 reader, mount + `OpenRead` fallback, in-image music. |
| [DEBUG.md](DEBUG.md) | Original Adeline debug tools (DEBUG_TOOLS=ON): overlay, F9 screenshot, bug save/load, cheats, scene selection. |
| [CONSOLE.md](CONSOLE.md) | Quake-style debug console (always available): backtick/F12, commands and cvars. |
| [CONTROL.md](CONTROL.md) | CLI control harness: drive the engine non-interactively (`--load`/`--exec`/`--tick`/`--dump-state`/`--screenshot`/`--exit`) for automation and regression. |
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
| [ASCII_ART.md](ASCII_ART.md) | Catalog of ASCII art banners in the original source files. |

## Porting & technical

| Doc | Description |
|-----|-------------|
| [BIT_EXACTNESS.md](BIT_EXACTNESS.md) | What "bit exact" / "byte identical" actually means here: the three kinds (format contract, ASM-parity oracle, regression tripwire), when byte-identity is the goal vs a proxy, and the rule for when a byte diff is acceptable. |
| [CONTROLLER.md](CONTROLLER.md) | Manual camera (orbit, elevation, zoom) and the input sources that drive it: keyboard, mouse, gamepad. |
| [FEATURE_WORKFLOW.md](FEATURE_WORKFLOW.md) | Reasoning and docs for big features: console commands, headless mode, menu changes, camera. |
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
| [PLATFORM_PAL_PLAN.md](PLATFORM_PAL_PLAN.md) | Plan (awaiting go/no-go): in-place Platform Abstraction Layer decoupling the engine from direct SDL3. SDL-surface audit, RFC #120 contract reconciliation, and a PR-sequenced, behavior-preserving extraction plan with a headless backend. |

## External resources

| Resource | Description |
|----------|-------------|
| [LBA Classic Doc](https://lba-classic-doc.readthedocs.io/) | Read the Docs: engine documentation. |
| [README Links](../README.md#links) | Official site, Discord, GOG/Steam. |
