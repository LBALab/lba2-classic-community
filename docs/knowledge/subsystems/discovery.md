---
type: Subsystem
title: Game data and user folder discovery
description: Two folders are settled before the engine starts, where the run writes and where the retail data is; both follow flag over environment over remembered choice over guess, a folder someone named is never replaced by a guess, only a chosen folder is searched for a disc image, and the banner names the probe that won.
status: draft
subsystem: discovery
as_of: f3e13ac0
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T15:00:00Z }
relates_to:
  - /subsystems/boot.md
  - /subsystems/logging.md
sources:
  - id: discovery-h
    resource: ../../../SOURCES/RES_DISCOVERY.H
    title: RES_DISCOVERY.H, ResolveGameDataDir and the probe label
  - id: discovery-cpp
    resource: ../../../SOURCES/RES_DISCOVERY.CPP
    title: RES_DISCOVERY.CPP, the probe chain and the remembered folder
  - id: directories-cpp
    resource: ../../../SOURCES/DIRECTORIES.CPP
    title: DIRECTORIES.CPP, the marker, the image-aware check, the user folder and its reconciliation
  - id: picker-cpp
    resource: ../../../SOURCES/RES_PICKER.CPP
    title: RES_PICKER.CPP, the first-launch folder picker and the Android message
  - id: discimg-h
    resource: ../../../LIB386/H/SYSTEM/DISCIMG.H
    title: DISCIMG.H, mounting a disc image and asking whether a folder holds one
  - id: bundle-h
    resource: ../../../LIB386/H/SYSTEM/BUNDLE.H
    title: BUNDLE.H, the folder an AppImage was launched from
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, main calling discovery, the picker, --disc and profile binding
  - id: initadel
    resource: ../../../SOURCES/INITADEL.C
    title: INITADEL.C, the banner's Assets, Disc, Writes and Note lines
  - id: game-data-doc
    resource: ../../GAME_DATA.md
    title: Game data (docs/GAME_DATA.md), the player-facing account
  - id: android-doc
    resource: ../../ANDROID.md
    title: Android (docs/ANDROID.md), "Game data on Android"
  - id: test-discovery
    resource: ../../../tests/discovery/test_res_discovery.cpp
    title: test_res_discovery, the probe order, the remembered folder, AppImages and the user folder policy
  - id: test-picker
    resource: ../../../tests/picker/test_res_picker.cpp
    title: test_res_picker, the picker's messages and platform gate
---

[GAME_DATA.md](../../GAME_DATA.md) is the player-facing account of where to put the data and which flag moves what, and this concept does not repeat its tables.[^game-data-doc] It holds the rules those tables follow and the places they are enforced. The contracts are read at `as_of`, after the change that lets the Android storage folder hold a disc image.

# Principles

- **Two folders, in a fixed order.** The user folder is settled first, because the log lives in it; the game data second, with the log already recording what discovery tried.[^perso-cpp]
- **What someone typed outranks what the engine guesses.** Both folders take a flag first, the environment second, a remembered or marked choice third, and a probe last.[^discovery-cpp] [^directories-cpp]
- **A folder someone named is never replaced by a guess.** A named game folder that holds no data goes to the picker or exits, and a named user folder that cannot be made stops the boot. See [the decision](/decisions/a-named-folder-is-never-replaced-by-a-guess.md).
- **Only a chosen folder pays for the disc image check.** Recognising a folder whose data exists only inside an image means opening every file in it, so the check runs on folders somebody chose and never across the speculative sweep. See [the decision](/decisions/only-a-chosen-folder-is-searched-for-a-disc-image.md).
- **Say which probe won.** The probes run silently, so the banner names the winner next to the path. A forgotten environment variable and a broken probe otherwise look the same in a bug report.[^discovery-h] [^initadel]

# Contracts

## The game data

| Contract | Statement |
|---|---|
| The marker | A folder is game data when it holds `LBA2.HQR`, or `RESS.HQR` in a demo build, matched as written, upper, lower and title case. `IsValidResourceDirOrImage` also accepts a folder holding a disc image whose filesystem contains the marker.[^directories-cpp] |
| The chain | `ResolveGameDataDir`, first match wins. Each root that names an install also tries a `Common/` inside it.[^discovery-cpp] |
| Refusals | `--game-dir` or `--data-dir`, then `LBA2_GAME_DIR`: the path, then its `Common/`, both image-aware. Neither matching logs the path and returns false without trying anything else.[^discovery-cpp] |
| Remembered | `--pick-game-dir` skips everything below and goes to the picker. Otherwise `last_game_dir.txt` in the user folder, image-aware; a stale entry logs a warning and falls through.[^discovery-cpp] |
| Install roots | The folder an AppImage was launched from, then the folder the binary is in. Each tries itself, image-aware, then `Common/`, `data/` and `game/`.[^discovery-cpp] [^bundle-h] |
| Working directory | Itself, image-aware, then its `Common/`.[^discovery-cpp] |
| Android | The app-specific external files folder, then `/sdcard/lba2cc/` and `/storage/emulated/0/lba2cc/`, both image-aware, then `/sdcard/` and `/storage/emulated/0/` themselves. The paths are spelled without a platform conditional and name nothing elsewhere.[^discovery-cpp] |
| Guesses | Up to eight parents of the working directory, then its `data/`, `../LBA2/` and `../game/`, then the sibling scan: up to 24 directories beside the working directory, each tried bare and with `CommonClassic`, `common`, `Common` and `Classic`. A sibling match logs a warning naming the neighbour it booted.[^discovery-cpp] |
| Failure | Logs the marker it wanted and every path tried, with hints, and returns false. `main` then exits with an error under the harness or `--headless`; on Android the picker shows where to copy the data and the run exits; on a desktop the picker opens.[^discovery-cpp] [^perso-cpp] [^picker-cpp] |
| The picker | A native folder dialog, validated with the image-aware check, reopened on an invalid pick, exiting on cancel. A valid pick is written to `last_game_dir.txt` and labelled `first-launch picker`.[^picker-cpp] [^perso-cpp] |
| The label | Each probe sets its label before it tries, so the label left when a probe succeeds is that probe's. `Res_ResolvedFromUserInput` is true only for labels beginning `--game-dir` or `LBA2_GAME_DIR`.[^discovery-cpp] |
| A disc named with `--disc` | Set before discovery, and from then on the image-aware check opens that image whatever folder it is asked about, so the first image-aware candidate passes. That is why, without an explicit game folder, `main` replaces the result with the image's own folder. The image is mounted after `InitDirectories`, and a file not on disk is then read from it.[^perso-cpp] [^discimg-h] |
| Profile binding | With a named profile and a folder from user input, the folder is written to that profile's `last_game_dir.txt` if nothing is bound yet. A different folder later is used for that run only, and logged as such, unless `--bind-game-dir` moves the binding. `--bind-game-dir` with nothing to bind warns.[^perso-cpp] |

## The user folder

| Contract | Statement |
|---|---|
| Order | `--user-dir`, then `LBA2_USER_DIR`, then a `portable.txt` beside the binary, which makes `User/LBA2/` there the folder, then the platform's roots. Resolved once and cached for the process.[^directories-cpp] |
| Roots | The first root a probe file can be written to. On a desktop that is the one SDL preference path. On Android it is `/sdcard/lba2cc/user/LBA2`, then the same folder under `/storage/emulated/0`, then the app-specific external folder, then SDL's app-private folder.[^directories-cpp] |
| A named folder | Created if missing. One that cannot be made is kept as named rather than replaced, and `InitDirectories` stops the boot naming it.[^directories-cpp] |
| Profiles | `--profile` or `LBA2_PROFILE`, or a `profile:` line in the portable marker, adds `profiles/<name>/` inside the folder. A name that is not a plain folder name stops the boot.[^directories-cpp] [^perso-cpp] |
| Reconciliation | Every root less preferred than the chosen one is checked for saves. If the chosen folder has none, the most preferred root holding some is copied up without overwriting, and marked as migrated when the copy is complete. If both hold saves, neither is touched and the other is reported.[^directories-cpp] [^test-discovery] |
| Reported later | The unwritable preferred root, a migration and a rival save folder are recorded when they happen, before the log exists, and printed as `Note:` lines in the banner.[^initadel] |

# Seams

| Seam | Direction | Contract |
|---|---|---|
| [Boot](/subsystems/boot.md) | call | Steps 4 and 6 of the boot order, before `InitAdeline`. Discovery strips `--game-dir` and `--pick-game-dir` out of `argv` before the engine's own command line reads it. |
| [Logging](/subsystems/logging.md) | write | Everything game-data discovery logs reaches `adeline.log`. User-folder resolution runs before the log and records instead. |
| The disc image | read | `DiscImage_DirHoldsMarker` answers the image-aware check without mounting; `DiscImage_Mount` mounts later; `FILES.CPP` falls back to the image for a file not on disk.[^discimg-h] |
| The picker | UI | `PromptForResDir`. Android has no usable folder dialog, since SDL's returns document URIs rather than paths, so it shows a message instead.[^picker-cpp] |
| Android storage | precondition | The All Files Access wait runs before the user folder is chosen, and its answer decides whether `/sdcard/lba2cc` can be written to and read from. |
| The host tests | read | `test_res_discovery` runs the chain against temporary folders for the flag, the environment, the remembered folder, AppImages, the working directory, siblings and the user folder policy. `test_res_picker` pins the picker's messages and its Android gate.[^test-discovery] [^test-picker] |

# What it does not tell you

- **The probe order, from the header.** `ResolveGameDataDir`'s comment in RES_DISCOVERY.H lists the flag, the environment, the remembered folder, the binary, the working directory, the parents, the relative paths and the siblings. It leaves out `--pick-game-dir`, the AppImage folder and the Android storage probes. ANDROID.md's list ends with a folder picker, which Android replaces with a message. The code is the order.[^discovery-h] [^discovery-cpp] [^android-doc]
- **What a stale remembered folder does.** The warning says it is re-prompting through the picker, but the probes run first and can find another install without a picker. GAME_DATA.md's table calls the fall-through silent; it logs.[^discovery-cpp] [^game-data-doc]
- **What the picker validates with.** GAME_DATA.md says `IsValidResourceDir`; the picker uses the image-aware check, which is what lets a player browse to a rip.[^picker-cpp] [^game-data-doc]
- **That the Android probes are tested.** The host tests run the user folder policy with the roots a test hands in; nothing runs the `/sdcard` probes of the game-data chain, so the image-aware Android probe added at `as_of` is not covered by CI.[^test-discovery]
- **That a found folder is the install that was meant.** The sibling scan boots whatever install sits next to the working directory and only warns.[^discovery-cpp]
- **That every candidate is tried.** The tried list holds 160 paths; a candidate past that is skipped without a message. The chain as written stays under it, but a longer sibling list would not.[^discovery-cpp]
- **That discovery's messages about the user folder are in the log.** A partial migration's per-file warnings are raised before the log exists and go to stderr; only the summary `Note:` reaches the file.[^directories-cpp] [^initadel]

[^discovery-h]: [SOURCES/RES_DISCOVERY.H](../../../SOURCES/RES_DISCOVERY.H), `ResolveGameDataDir` and `Res_GetDiscoverySource`.
[^discovery-cpp]: [SOURCES/RES_DISCOVERY.CPP](../../../SOURCES/RES_DISCOVERY.CPP), `ResolveGameDataDir`, `NormalizeAndTry`, `TryInstallRoot`, `TryParentSiblingScan`, `Res_SetDiscoverySource`, `LogFailure` and `kMaxTried`.
[^directories-cpp]: [SOURCES/DIRECTORIES.CPP](../../../SOURCES/DIRECTORIES.CPP), `FILE_VALID_RES_DIR`, `IsValidResourceDirOrImage`, `GetDefaultUserDir`, `ResolveDefaultUserDirRoot`, `Directories_SelectWritableRoot`, `Directories_ReconcileUserDir` and `Directories_PortableUserDir`.
[^picker-cpp]: [SOURCES/RES_PICKER.CPP](../../../SOURCES/RES_PICKER.CPP), `PromptForResDir`.
[^discimg-h]: [LIB386/H/SYSTEM/DISCIMG.H](../../../LIB386/H/SYSTEM/DISCIMG.H), `DiscImage_DirHoldsMarker`, `DiscImage_SetOverride` and `DiscImage_Mount`.
[^bundle-h]: [LIB386/H/SYSTEM/BUNDLE.H](../../../LIB386/H/SYSTEM/BUNDLE.H), `Bundle_GetLauncherDir`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main`, from `GetDefaultUserDir` to `DiscImage_Mount`.
[^initadel]: [SOURCES/INITADEL.C](../../../SOURCES/INITADEL.C), `InitAdeline`, the banner block.
[^game-data-doc]: [docs/GAME_DATA.md](../../GAME_DATA.md), "First-launch folder picker".
[^android-doc]: [docs/ANDROID.md](../../ANDROID.md), "Game data on Android".
[^test-discovery]: [tests/discovery/test_res_discovery.cpp](../../../tests/discovery/test_res_discovery.cpp), `test_persisted_last_game_dir`, `test_appimage_dir_disc_image` and `test_userdir_reconcile_reports_a_second_save_set`.
[^test-picker]: [tests/picker/test_res_picker.cpp](../../../tests/picker/test_res_picker.cpp).
