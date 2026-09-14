---
type: Decision
title: A named folder is never replaced by a guess
description: A game folder given with --game-dir or LBA2_GAME_DIR that holds no data is refused rather than passed over for whatever the probes find, a user folder given with --user-dir or LBA2_USER_DIR that cannot be made stops the boot, and a disc named with --disc is that disc or an error, because a run against something nobody asked for still looks right and exits 0.
status: draft
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T15:00:00Z }
owner: /subsystems/discovery.md
sources:
  - id: discovery-cpp
    resource: ../../../SOURCES/RES_DISCOVERY.CPP
    title: RES_DISCOVERY.CPP, the two refusals in ResolveGameDataDir
  - id: directories-cpp
    resource: ../../../SOURCES/DIRECTORIES.CPP
    title: DIRECTORIES.CPP, GetDefaultUserDir keeping a named folder
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, main, the headless exit, the profile name check and profile binding
  - id: discimg-cpp
    resource: ../../../LIB386/SYSTEM/DISCIMG.CPP
    title: DISCIMG.CPP, PickImage with an override
  - id: commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/8d21c52e
    title: "8d21c52e, fix(discovery): don't boot a different install than the one asked for"
  - id: game-data-doc
    resource: ../../GAME_DATA.md
    title: Game data (docs/GAME_DATA.md), "What to point the engine at"
---

# Context

A `--game-dir` that did not hold the game data used to be dropped silently. Discovery carried on to the environment, the remembered folder and the probes, booted a different install, and said so only in the banner's `Assets:` line. Every artefact of the run came from assets nobody asked for, and it exited 0, so an A/B between two trees compared neither.[^commit] An `LBA2_GAME_DIR` exported in a shell profile and forgotten does the same thing without any flag on the command line.[^discovery-cpp]

# Decision

- **A named game folder is tried twice and then refused.** `--game-dir`, `--data-dir` or `LBA2_GAME_DIR`: the path, then a `Common/` inside it, because people point these at the install root. If neither holds the data, discovery logs the path and returns false without trying another probe.[^discovery-cpp]
- **The refusal is loud and recoverable.** On a desktop the picker opens; under the harness or `--headless` the run exits with the flag to use in the message, because the picker would open on the dummy video driver with nobody to answer it.[^perso-cpp] [^commit]
- **A named user folder is kept even when it cannot be made.** `GetDefaultUserDir` does not fall back to the default folder for a `--user-dir` or `LBA2_USER_DIR` it cannot use, so `InitDirectories` stops the boot naming it. An unusable profile name stops the boot before the user folder is resolved.[^directories-cpp] [^perso-cpp]
- **A named disc is that disc.** With `--disc`, the image is opened as given and never ranked against, or replaced by, other images in the folder; an image that cannot be read is logged as an error.[^discimg-cpp]

# Non-goals

- **Refusing a guess.** The sibling scan boots a neighbouring install on purpose, for a clone sitting beside a retail copy. It warns and names the install instead.[^discovery-cpp] [^game-data-doc]
- **Treating the remembered folder as named.** A stale `last_game_dir.txt` means the install moved, which is not a mistake on this run's command line, so it warns and falls through to the probes.[^discovery-cpp]
- **Binding a guess to a profile.** A named profile binds to a folder from the flag or the environment, never to a probe's answer. A folder picked in the dialog is remembered in the user folder the same way on any run, profile or not.[^perso-cpp]

[^discovery-cpp]: [SOURCES/RES_DISCOVERY.CPP](../../../SOURCES/RES_DISCOVERY.CPP), `ResolveGameDataDir` and `TryParentSiblingScan`.
[^directories-cpp]: [SOURCES/DIRECTORIES.CPP](../../../SOURCES/DIRECTORIES.CPP), `GetDefaultUserDir`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main`, `ProfileNameOrExit` and `Res_ResolvedFromUserInput`.
[^discimg-cpp]: [LIB386/SYSTEM/DISCIMG.CPP](../../../LIB386/SYSTEM/DISCIMG.CPP), `PickImage` and `DiscImage_SetOverride`.
[^commit]: [8d21c52e](https://github.com/LBALab/lba2-classic-community/commit/8d21c52e), fix(discovery): don't boot a different install than the one asked for, and never hang on the picker.
[^game-data-doc]: [docs/GAME_DATA.md](../../GAME_DATA.md), "What to point the engine at".
