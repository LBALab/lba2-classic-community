---
type: Subsystem
title: Boot and exit
description: main brings the engine up in a fixed order in which each facility exists only from a known step, the log before the game data and the exit report only after the platform is up, and every ending goes through exit() and its handlers except a fatal signal, which skips them all.
status: draft
subsystem: boot
as_of: e8ae05fc
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T20:30:00Z }
relates_to:
  - /subsystems/memory.md
  - /subsystems/control.md
  - /subsystems/save.md
sources:
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, main from the first argument check to the dispatch, and InitProgram
  - id: initadel
    resource: ../../../SOURCES/INITADEL.C
    title: INITADEL.C, InitAdeline, the platform bring-up
  - id: boot-exit-h
    resource: ../../../SOURCES/BOOT_EXIT.H
    title: BOOT_EXIT.H, the exit entry points
  - id: boot-exit-cpp
    resource: ../../../SOURCES/BOOT_EXIT.CPP
    title: BOOT_EXIT.CPP, TheEnd, TheEndCheckFile, BootFatal and TheEndInfo
  - id: boot-end-h
    resource: ../../../SOURCES/BOOT_END.H
    title: BOOT_END.H, which exit codes are failures and what a player is told
  - id: common-h
    resource: ../../../SOURCES/COMMON.H
    title: COMMON.H, the exit codes
  - id: preflight
    resource: ../../../SOURCES/ASSET_PREFLIGHT.CPP
    title: ASSET_PREFLIGHT.CPP, the required and optional asset check
  - id: android-cpp
    resource: ../../../LIB386/SYSTEM/ANDROID.CPP
    title: ANDROID.CPP, the storage permission wait
  - id: window-cpp
    resource: ../../../LIB386/SYSTEM/WINDOW.CPP
    title: WINDOW.CPP, the quit event
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the recorder's exit flush
  - id: test-boot-end
    resource: ../../../tests/boot_end/test_boot_end.cpp
    title: test_boot_end, the exit report's decisions
  - id: test-preflight
    resource: ../../../tests/asset_preflight/test_asset_preflight.cpp
    title: test_asset_preflight, required against optional assets
  - id: init-research
    resource: ../../plan/INIT_RESEARCH.md
    title: Initialisation research, the earlier trace of the same path
---

`main` in PERSO.CPP is the boot: about nine hundred lines that run once, in an order that is mostly Adeline's with the port's seams inserted. This concept holds that order as a map of what exists when, and every way the process ends. What happens after the dispatch, a new game or a load, belongs to [savegames](/subsystems/save.md) and [transitions](/subsystems/transitions.md). The contracts are read at `as_of`; the one measurement below was taken on a macOS Release build at that commit.

# Principles

- **A facility exists from a known step.** The log is up before the game data is found, SDL's filesystem before the user folder, the main buffer before the window, the console before the resource banks. Code that runs earlier than a facility must not use it, and several early steps record what they need to say and let a later step print it.
- **Fail before the heavy work.** Arguments are validated before SDL starts, missing required assets stop the boot before any bank is loaded, and a failure says what is wrong in the player's terms.[^perso-cpp] [^preflight]
- **Every orderly end is `exit()`.** No path unwinds back through `main`; an ending records its code and calls `exit`, and the handlers registered along the boot do the reporting and the saving.[^boot-exit-cpp]
- **The harness takes the same path.** `--headless` and the control harness boot through `main` like a player, with presentation skipped at named points rather than a separate entry.[^perso-cpp]

# Contracts

## The order

| Step | What runs | What exists afterwards |
|---|---|---|
| 1. Arguments | `--version`, `--help`, then `Cli_ValidateArgs`, which rejects an unknown flag with exit 2. Harness and recording flags are parsed out of `argv`. | stdout and stderr only |
| 2. SDL core | `SDL_Init(0)`; failure exits 1 through `LogPrintf`, which has no sink yet. | SDL filesystem calls |
| 3. Android storage | `Android_EnsureExternalStoragePermission` opens Settings when All Files Access is missing and waits up to 15 seconds, or until the process is seen to thaw. A no-op elsewhere. | the permission answer |
| 4. User folder | `--user-dir` and `--profile` are read from `argv`, then `LBA2_PROFILE`, then `GetDefaultUserDir` resolves and caches the folder. See [discovery](/subsystems/discovery.md). | where the run writes |
| 5. Log | `CreateLog`, which keeps the previous run's log as `adeline.prev.log`, `Log_Init`, the level, the file and terminal sinks, `atexit(Log_Shutdown)`, then `Crash_Install` and, on Android, `Android_SavePreviousCrashReport`. See [logging](/subsystems/logging.md). | `adeline.log`, stderr and the crash report |
| 6. Game data | `--disc`, `ResolveGameDataDir`, the picker or an exit, profile binding, `InitDirectories`, `DiscImage_Mount`. See [discovery](/subsystems/discovery.md). | `GetResPath` and the disc image |
| 7. Main buffer | The config path, the video subsystem so the display can be measured, `Res_LoadBootDimensions`, `Mem_ConfigureScreenBuffers`, `InitMainBuffer`. See [memory](/subsystems/memory.md). | the fixed regions |
| 8. Platform | `InitAdeline`: the console sink and the boot banner, events, joystick, window, a default config written when none exists anywhere, audio, video, screen and graphics mode, keyboard, mouse, timer, and the layered config buffer. | a window and a timer |
| 9. Hooks | The console's event filter and pre-present callback, the touch overlay, perftrace, `atexit(TheEndInfo)`, `register_crash_state`. | the exit report, and game state in a crash block |
| 10. Program | `InitProgram`, which reads the config, `InitMemory`, the Display line, `AssetPreflight`. | settings and small buffers |
| 11. Banks | Samples, language, the Release line and the `Ready` banner, then dialogue buffers, palettes, font, the video player, the logos, the 3D extension and the resource banks. | a fully resourced engine |
| 12. Dispatch | In order: a cube number in a debug build, `--save-load-test`, the control harness through `Control_Begin` and `MainGameMenu(0, TRUE)`, and otherwise `MainGameMenu`, which loads a save path given in `argv`. Each branch ends in `TheEnd`. | a game |

Sources for the table: `main` and `InitProgram`[^perso-cpp], `InitAdeline`[^initadel], `Android_EnsureExternalStoragePermission`[^android-cpp], `AssetPreflight`[^preflight].

## Endings

| Contract | Statement |
|---|---|
| `TheEnd` | Records the code and a detail string and calls `exit`: 0 for `PROGRAM_OK`, 1 for any other code. It never returns.[^boot-exit-cpp] [^boot-exit-h] |
| `TheEndCheckFile` | A required file failed to load. Records `NOT_ENOUGH_MEM` when the file has a size, so it is there and unusable, and `ERROR_NOT_FOUND_FILE` when it does not, then exits 1.[^boot-exit-cpp] |
| `BootFatal` | For a platform failure inside `InitAdeline`: logs the reason, raises a native dialog when stderr is not a terminal, the harness is not active and video can come up, and exits 1. It tears nothing down itself.[^boot-exit-cpp] [^initadel] |
| `TheEndInfo` | Registered at step 9 and run by `exit`. Logs the failure line for a failure code, `OK.` for `PROGRAM_OK`, raises the dialog for a failure, then writes the config and clears the 3D extension. What counts as a failure and what the dialog says is `BOOT_END.H`, under a host test.[^boot-exit-cpp] [^boot-end-h] [^test-boot-end] |
| Handler order | `exit` runs handlers last registered first. The recorder's flush, registered when a recording starts, runs before `TheEndInfo`, which runs before `Log_Shutdown`, so every report line reaches the file.[^perso-cpp] [^record-cpp] |
| Before step 9 | An exit from steps 1 to 8, a rejected argument, no game data, a cancelled picker, an invalid folder in `InitDirectories` or a `BootFatal`, runs no `TheEndInfo`: no report line, no config write. |
| Closing the window | The quit event calls `exit(0)` directly. The code is still unset, so `TheEndInfo` logs no outcome, and it still writes the config.[^window-cpp] [^boot-exit-cpp] |
| A fatal signal | Ends the process without `exit`, so no handler runs: no report, no config write, no recorder flush. From step 5 the crash handler appends a `CRASH` block after the log's last flushed line and hands the signal back, so the process still dies of it; on Windows an unhandled exception and `abort` do the same. See [logging](/subsystems/logging.md).[^perso-cpp] |

# Seams

| Seam | Direction | Contract |
|---|---|---|
| [Logging](/subsystems/logging.md) | lifecycle | Brought up at step 5 and shut down by the first handler registered, which is the last to run. |
| [Discovery](/subsystems/discovery.md) | call | Steps 4 and 6. Both read `argv` before `GetCmdLine` sees it, and discovery strips `--game-dir` out of it. |
| [Memory](/subsystems/memory.md) | call | Step 7 sizes the screen regions from the resolution the display and config give, and `InitAdeline` re-reads the same resolution for the window; the two must agree or the buffers and the window disagree.[^initadel] |
| [Control harness](/subsystems/control.md) | branch | The harness skips the logos and the CD screen, refuses the folder picker, and dispatches through `Control_Begin`. A setup or replay failure ends in `TheEnd(PROGRAM_FAIL, ...)`.[^perso-cpp] |
| The boot banner | write | Written in `InitAdeline`; names the build, the assets and the probe that found them, the disc, the user folders and any `Note:` recorded before the log existed.[^initadel] |
| `AssetPreflight` | gate | Required banks and `video.hqr` in a retail build stop the boot through `TheEnd`; holomap, scene, screen, background, music and voices only warn. Under `test_asset_preflight`.[^preflight] [^test-preflight] |

# What it does not tell you

- **That a `return` after `TheEnd` sets the exit code.** `TheEnd` does not return. Measured: `--save-load-test` on a missing file prints `status=missing`, logs `OK.` and exits 0, although the code after the call says `return 2`.[^perso-cpp]
- **That a failure's detail is reported.** `TheEndInfo` prints the detail string for `PROGRAM_OK` and for the three codes `BOOT_END.H` calls failures. `PROGRAM_FAIL` is none of those, so the string passed with it, `control: setup failed` among them, is never printed; the exit status is 1 and the caller has to have logged its own reason.[^boot-exit-cpp] [^boot-end-h] [^common-h]
- **What `Ready in` measures.** The time to the end of `InitAdeline`, printed later, after the preflight and the language. The banks and the logos are not in it.[^perso-cpp]
- **That INIT_RESEARCH.md is the current order.** It predates the log moving into `main`, still says each `InitAdeline` failure exits 1 with a graceful-exit TODO where `BootFatal` now reports it, and dispatches the harness straight to `MainLoop`. It is kept for its later phases, the per-game init and the cube load.[^init-research]
- **That `BootFatal`'s pointer is followed.** Its comment sends the reader to PERSO.H for why it cannot go through `TheEnd`; nothing there says. The reason is the order above: it runs before `TheEndInfo` is registered.[^boot-exit-cpp]

[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main` and `InitProgram`.
[^initadel]: [SOURCES/INITADEL.C](../../../SOURCES/INITADEL.C), `InitAdeline`, `BootFatal` at each platform step, and `LOG_PLATFORM_NAME`.
[^boot-exit-h]: [SOURCES/BOOT_EXIT.H](../../../SOURCES/BOOT_EXIT.H), `TheEnd`, `TheEndCheckFile` and `BootFatal`.
[^boot-exit-cpp]: [SOURCES/BOOT_EXIT.CPP](../../../SOURCES/BOOT_EXIT.CPP), `TheEnd`, `TheEndCheckFile`, `BootFatal`, `ShowFatalErrorDialog` and `TheEndInfo`.
[^boot-end-h]: [SOURCES/BOOT_END.H](../../../SOURCES/BOOT_END.H), `BootEnd_IsFailure` and `BootEnd_FormatMessage`.
[^common-h]: [SOURCES/COMMON.H](../../../SOURCES/COMMON.H), `PROGRAM_OK` and `PROGRAM_FAIL`.
[^preflight]: [SOURCES/ASSET_PREFLIGHT.CPP](../../../SOURCES/ASSET_PREFLIGHT.CPP), `AssetPreflight`.
[^android-cpp]: [LIB386/SYSTEM/ANDROID.CPP](../../../LIB386/SYSTEM/ANDROID.CPP), `Android_EnsureExternalStoragePermission`.
[^window-cpp]: [LIB386/SYSTEM/WINDOW.CPP](../../../LIB386/SYSTEM/WINDOW.CPP), `SDL_EVENT_QUIT`.
[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), `record_arm_atexit` and `record_flush_at_exit`.
[^test-boot-end]: [tests/boot_end/test_boot_end.cpp](../../../tests/boot_end/test_boot_end.cpp).
[^test-preflight]: [tests/asset_preflight/test_asset_preflight.cpp](../../../tests/asset_preflight/test_asset_preflight.cpp).
[^init-research]: [docs/plan/INIT_RESEARCH.md](../../plan/INIT_RESEARCH.md), "Phase A" and "Phase C".
