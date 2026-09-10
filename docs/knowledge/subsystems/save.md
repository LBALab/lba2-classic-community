---
type: Subsystem
title: Savegames
description: Three save files with different writers and different staleness, one envelope writer and one field-by-field reader, a load that is a cube change with the fade left out, and a clock that travels in the file and is installed on the way in.
status: draft
subsystem: save
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /decisions/harness-load-bracket-closes-at-arming.md
  - /quirks/savetimer-counts-up-at-any-depth.md
  - /subsystems/transitions.md
  - /subsystems/timing.md
  - /subsystems/recording.md
  - /subsystems/control.md
sources:
  - id: savegame-doc
    resource: ../../SAVEGAME.md
    title: Savegame system (docs/SAVEGAME.md)
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, the writers, the loaders and LoadContexte
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the FlagLoadGame block in ChangeCube and the autosave in AffScene
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, Control_WriteSnapshot, Control_RequestLoad and the --no-autosave comment
  - id: gamemenu-cpp
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the load list, the CurrentSaveGame calls and the name gate
  - id: corpus-readme
    resource: ../../../tests/savegame/corpus/README.md
    title: The corpus harness README, "Requires retail game data"
---

Distilled from [docs/SAVEGAME.md](../../SAVEGAME.md), which stays the reference and owns the field map, the lifecycle tables and the hardening table; this concept holds the contracts, the seams, and what the doc does not say. The file itself is [the .lba Format](/formats/lba-save.md). The principles are structural; the seams and the gates are read at `as_of`.

# Principles

- **The file is the retail file.** Format 36 as the 1997 engine wrote it, byte for byte on every host. A 64-bit build converts its three pointer-bearing structs through fixed 32-bit wire mirrors instead of bumping the version, which is [a decision](/decisions/the-save-version-stays-36.md) with a consequence for the reader.[^savegame-doc]
- **The reader trusts no count and no stride.** Every count is range-checked against its compile-time maximum, every read is bounded by a read limit set at the file's end, the per-object stride is discovered by trial rather than read, and a failure comes back as a context error rather than a crash.[^savegame-cpp]
- **A load is a cube change.** Nothing loads a game in one call. A loader sets `FlagLoadGame` and reads the cube out of the file; the next `NewCube != -1` at the top of `MainLoop` runs `ChangeCube`, which calls `LoadGame` in place of the scene's own initialisation. So a load has every property of a cube change, except that it fades nothing and opens no bracket; [transitions](/subsystems/transitions.md) owns the silence.[^object-cpp]
- **The save carries a clock and the load installs it.** `savetimerrefhr` is written from `TimerRefHR` and put back with `SetTimerHR`, so game time goes backwards across a load; [timing](/subsystems/timing.md) owns what follows from that.[^savegame-cpp]

# Contracts

| Contract | Statement |
|---|---|
| Three files, three writers | `current.lba`, player name `CURRENT`, written by `CurrentSaveGame` after a start or a load lands and when the in-game menu opens, while saving is enabled; it is the main menu's Resume entry. `autosave.lba`, player name `AUTOSAVE`, written by `AutoSaveGame` at the tail of `AffScene` after a genuine cube change that is not the phantom cube; it is the AUTOSAVE slot in the load list. A named save is the menu's Save. The load list filters both automatic files out of the directory scan and re-adds only the autosave.[^gamemenu-cpp] |
| Compression is per writer | The two automatic writers set the version byte to `NUM_VERSION` alone and put the player's value back afterwards, so those two files are uncompressed. The menu's save keeps the high bit that `CompressSave` in lba2.cfg seeded at startup, and the harness snapshot sets it. One session writes both shapes.[^savegame-cpp] |
| The load is two-phase | `LoadGameNumCube` reads the header, decompresses, skips the thumbnail and restores only `ListVarGame` and `NewCube`. `LoadGame`, from inside `ChangeCube`, reads the whole file through `LoadContexte` and returns `flagload`: zero for a full restore, which `InitLoadedGame` completes; a context error, which skips both `InitLoadedGame` and `CheckProtoPack` because the object list may be partial; and a positive value, the checksum short path or the debug-only old-version loader, which runs `LoadFile3dObjects` alone.[^object-cpp] |
| The checksum is a scenario gate | A stored checksum that differs from the build's forces `SceneStartX` to -1, and the reader takes the short path: hero and globals repaired, the extended blob left unread. The engine does not refuse the file; it starts the hero at the scene start.[^savegame-cpp] |
| Who may save | `SavingEnable` is cleared and set from `MainLoop` around the sequences where a save would be wrong; the automatic writers return early in the attract reel and the ending; `--no-autosave` disables `AutoSaveGame` only, and an explicit save still works. A harness run without it rewrites `autosave.lba` on the real install when the hero walks through a door.[^control-cpp] |
| The name gate | The save menu offers typing only when the last input was a keyboard and the device is not a TV; otherwise it generates a name from the island and the date. That flag is a device fact the game reads, in no digest, which is why the recorder carries it in its header and marks each change; the generated shape is reserved so a typed name cannot collide with it.[^gamemenu-cpp] |
| The trailing bracket | `LoadGame` ends on a `SaveTimer` nothing in it closes. The menu's load path restores right after `ChangeCube` and a harness load closes it at the first armed tick, which is [a decision](/decisions/harness-load-bracket-closes-at-arming.md) manifesting [the counter's asymmetry](/quirks/savetimer-counts-up-at-any-depth.md). `SaveGame` reads the clock to save directly when no bracket is open, for the same reason.[^savegame-cpp] |
| Older layouts | `LoadContexte` branches on the version for the per-object fields added at 35 and 36, and below 34 only in debug, test and editor builds. A release build reading a different layout revision misaligns; the old-version loader that restores the early context alone exists only in debug and test builds.[^savegame-doc] |

# Seams

- **The harness snapshot and load.** `Control_WriteSnapshot` borrows the save context, names the player `rec`, sets compression and calls `SaveGame`; `Control_RequestLoad` sets `FlagLoadGame` and `LoadGameNumCube` and lets `MainLoop`'s next tick land it. The recorder's two chunks are files of this Format, staged beside the recording and inlined.[^control-cpp]
- **`--save-load-test <path>`** boots to the menu, replicates the load path and prints `SAVE_LOAD_TEST: stage=` lines with `flagload` and the object count, then exits. `LBA2_SAVE_LOAD_ABI` forces the reader onto the wire or the native stride for an audit.[^savegame-doc]
- **The bug slots**: `savebug`, `loadbug` and `listbugs` in the console, the debug build's keys, under `save/bugs/` through `GetBugPath`; the same writer and reader with a different directory and compression on.[^savegame-doc]
- **`GetSavePath`** under the user directory, which `--user-dir` and `--profile` move.
- **The offline reader**, `scripts/save_probe.py`, an independent parser that forward-simulates `LoadContexte` under both stride hypotheses, with `save_decompress` as its decompressor; the corpus driver checks its prediction against the engine's outcome per save.
- **The corpus**, fifty retail saves in the tree with a driver that needs retail data and runs locally, which is [a decision](/decisions/the-committed-corpus-is-the-oracle.md).[^corpus-readme]

# What it does not tell you

- **That `current.lba` is the latest state.** It is written when the menu is touched and when a load lands, so during free roam it is as old as the last menu visit; the autosave is as old as the last genuine cube change; and a console warp or a load writes neither.
- **That a load is read-only.** A `--load --tick` run through a door rewrites the autosave, and the in-game menu rewrites the current save, on whatever user directory the run points at.
- **Which build wrote the file.** The version byte is not an ABI tag: the same first byte came from the retail 32-bit engine and from 64-bit builds before the wire writer, and the reader tells them apart by trial, not by reading.
- **That `flagload` zero means the load was right.** Zero is "no context error". A stride that validated by luck reaches `InitLoadedGame` and crashes there, on the full `--load` path and not in `--save-load-test`, so a load is crash-tested with ticks after it.[^savegame-doc]
- **The file's name.** The player name in the header is what the menu shows and derives a filename from; the automatic files carry their fixed names and the harness snapshot carries `rec` whatever the recording is called. Nothing in the file says where it was saved.

[^savegame-doc]: [docs/SAVEGAME.md](../../SAVEGAME.md), "Lifecycle", "Version compatibility", "32-bit vs 64-bit and pointers" and "Tooling".
[^savegame-cpp]: SAVEGAME.CPP: `AutoSaveGame`, `CurrentSaveGame`, the bracket comment and version byte in `SaveGame`, `LoadGame` from `LoadContexte` to its `SaveTimer`, and the checksum test and stride trial in `LoadContexte`.
[^object-cpp]: OBJECT.CPP, `ChangeCube`, the `if (FlagLoadGame)` block and its `flagload` branches; `AffScene`, the `if (FlagChgCube)` block.
[^control-cpp]: CONTROL.CPP, `Control_WriteSnapshot`, `Control_RequestLoad`, and the comment at `--no-autosave`.
[^gamemenu-cpp]: GAMEMENU.CPP, the load-list scan, the `CurrentSaveGame` calls, and the `LastInputWasKeyboard` test in the save menu.
[^corpus-readme]: [tests/savegame/corpus/README.md](../../../tests/savegame/corpus/README.md), "Requires retail game data".
