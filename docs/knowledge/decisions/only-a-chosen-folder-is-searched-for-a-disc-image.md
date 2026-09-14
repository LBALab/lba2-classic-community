---
type: Decision
title: Only a chosen folder is searched for a disc image
description: Accepting a folder whose game data exists only inside a disc image means opening every file in it, so that check runs on folders somebody chose or was told to use, and the speculative candidates discovery sweeps through stay on the cheap test for an extracted marker file.
status: draft
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T15:00:00Z }
owner: /subsystems/discovery.md
sources:
  - id: directories-cpp
    resource: ../../../SOURCES/DIRECTORIES.CPP
    title: DIRECTORIES.CPP, IsValidResourceDir and IsValidResourceDirOrImage
  - id: discovery-cpp
    resource: ../../../SOURCES/RES_DISCOVERY.CPP
    title: RES_DISCOVERY.CPP, the allowImage argument at each probe
  - id: picker-cpp
    resource: ../../../SOURCES/RES_PICKER.CPP
    title: RES_PICKER.CPP, the picker's validation
  - id: commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/953a7e16
    title: "953a7e16, fix(disc): keep the image probe off the speculative discovery sweep"
  - id: android-commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/1beede02
    title: "1beede02, fix(android): discover disc images in app data folder"
  - id: android-doc
    resource: ../../ANDROID.md
    title: Android (docs/ANDROID.md), the discovery order
---

# Context

A retail CD rip is a `.bin` and a `.cue` with no `LBA2.HQR` beside them. Recognising such a folder means identifying an image among its files, which opens each one. When that probe was folded into `IsValidResourceDir`, every candidate in the discovery sweep paid for it, up to 160 of them including the parents of the working directory and the siblings of its parent, and a check that had been a few stats became a directory walk repeated across the sweep.[^commit] The CI host tests hanging at the time is the suspected consequence, never reproduced.[^commit]

# Decision

- **Two checks.** `IsValidResourceDir` looks for the marker file only. `IsValidResourceDirOrImage` adds the image probe, and nothing folds the second into the first.[^directories-cpp]
- **The image-aware check runs on a folder somebody chose:** `--game-dir` and `LBA2_GAME_DIR` with their `Common/`, the remembered `last_game_dir.txt`, the folder an AppImage was launched from and the binary's folder, the working directory, a folder picked in the dialog, and `InitDirectories` re-checking the folder already chosen.[^discovery-cpp] [^picker-cpp] [^directories-cpp]
- **And on the one folder a platform's docs tell players to use.** On Android that is `/sdcard/lba2cc/`, under both of its spellings, added because a rip placed there as ANDROID.md says was not found.[^android-commit] [^android-doc]
- **Everything else stays cheap:** the `Common/`, `data/` and `game/` subfolders of the install roots and of the working directory, the Android app-specific folder and the bare storage roots, the parent walk, the relative paths and the sibling scan.[^discovery-cpp]

# Non-goals

- **Finding a rip wherever discovery looks.** A rip in a sibling folder or a parent is not found. Naming it with `--game-dir` or `--disc` is the way in.
- **Searching shared storage.** The bare `/sdcard/` roots are whole shared-storage directories, the expensive case this decision exists to avoid.[^android-doc]
- **Deciding per machine.** The cost on a fast disk is not the argument: a guess about a folder nobody named should not be reading its files.[^commit]

[^directories-cpp]: [SOURCES/DIRECTORIES.CPP](../../../SOURCES/DIRECTORIES.CPP), `IsValidResourceDir`, `IsValidResourceDirOrImage` and `InitDirectories`.
[^discovery-cpp]: [SOURCES/RES_DISCOVERY.CPP](../../../SOURCES/RES_DISCOVERY.CPP), `NormalizeAndTry`, `TryInstallRoot` and `ResolveGameDataDir`.
[^picker-cpp]: [SOURCES/RES_PICKER.CPP](../../../SOURCES/RES_PICKER.CPP), `PromptForResDir`.
[^commit]: [953a7e16](https://github.com/LBALab/lba2-classic-community/commit/953a7e16), fix(disc): keep the image probe off the speculative discovery sweep.
[^android-commit]: [1beede02](https://github.com/LBALab/lba2-classic-community/commit/1beede02), fix(android): discover disc images in app data folder.
[^android-doc]: [docs/ANDROID.md](../../ANDROID.md), "Game data on Android".
