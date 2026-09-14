---
type: Decision
title: The log starts before game-data discovery
description: The log file and the terminal sink are created in main as soon as the user folder is known and before the game data is looked for, so the run that cannot find its data is the run whose log says why; the console sink still joins later, and what happens before the user folder is known is recorded and printed in the banner instead.
status: draft
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T15:00:00Z }
owner: /subsystems/boot.md
constrains:
  - /subsystems/logging.md
  - /subsystems/discovery.md
sources:
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, main, the comment above CreateLog
  - id: initadel
    resource: ../../../SOURCES/INITADEL.C
    title: INITADEL.C, InitAdeline adding only the console sink, and the Note lines
  - id: directories-cpp
    resource: ../../../SOURCES/DIRECTORIES.CPP
    title: DIRECTORIES.CPP, GetLogPathInDir and GetLogPath
  - id: commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/72d3a4a2
    title: "72d3a4a2, boot log: start the log before game-data discovery"
  - id: plan
    resource: ../../plan/BOOT_LOG_PLAN.md
    title: Boot log plan, the history of the sinks, superseded on severity
---

# Context

The log used to be created inside `InitAdeline`, which runs after game-data discovery, the folder picker and `InitDirectories`. Every message from that window went to stderr through the pre-sink fallback and never reached `adeline.log`. The case the picker exists for, a player whose game data cannot be found, was therefore the one case the log file could not describe.[^commit]

# Decision

- **Create the log in `main`, between the user folder and the game data.** `CreateLog`, `Log_Init`, the level, the file sink, the terminal sink and `atexit(Log_Shutdown)` run right after `GetDefaultUserDir` and before `ResolveGameDataDir`.[^perso-cpp]
- **Derive the log path from the user folder alone.** `GetLogPath` needs `InitDirectories`, which needs the game folder. `GetLogPathInDir` takes the user folder directly, and `GetLogPath` delegates to it, so both name the same file.[^directories-cpp]
- **Add the console sink later.** `InitAdeline` adds only the console sink, since there is no console to draw until then, and writes the banner as its first records.[^initadel]
- **Record what happens before the user folder is known.** A user folder that could not be written, saves copied up from an older folder and a second set of saves are stored when they are found and printed as `Note:` lines in the banner.[^initadel]

# Non-goals

- **Logging to the file from the very first line.** Argument validation, `SDL_Init`, the Android storage permission wait and the user folder's own resolution still run with no file, and what they print goes to stderr. Moving the log earlier would need a log path before the folder that holds it is chosen.
- **A separate early-boot log.** One file, reused by every sink and by the legacy `LogPrintf` sites.[^plan]
- **Changing teardown.** `Log_Shutdown` is still registered before `TheEndInfo`, so it still runs after it.[^commit]

[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main`, `CreateLog` and `Log_Init`.
[^initadel]: [SOURCES/INITADEL.C](../../../SOURCES/INITADEL.C), `InitAdeline` and `Log_MakeConsoleBufferSink`.
[^directories-cpp]: [SOURCES/DIRECTORIES.CPP](../../../SOURCES/DIRECTORIES.CPP), `GetLogPathInDir` and `GetLogPath`.
[^commit]: [72d3a4a2](https://github.com/LBALab/lba2-classic-community/commit/72d3a4a2), boot log: start the log before game-data discovery.
[^plan]: [docs/plan/BOOT_LOG_PLAN.md](../../plan/BOOT_LOG_PLAN.md), "Load-bearing design decisions".
