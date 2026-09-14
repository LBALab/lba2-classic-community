---
type: Subsystem
title: Logging
description: One call formats a record once and fans it out to a file, stderr and the console through SDL's log output, behind a single master level that structural lines bypass; the previous run's file is kept beside it at launch and the new one is flushed per line, and nothing is written when the process dies on a signal.
status: draft
subsystem: logging
as_of: f3e13ac0
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T15:00:00Z }
relates_to:
  - /subsystems/console.md
  - /subsystems/control.md
sources:
  - id: log-h
    resource: ../../../LIB386/H/SYSTEM/LOG.H
    title: LOG.H, the API, the sink constructors and the level
  - id: log-cpp
    resource: ../../../LIB386/SYSTEM/LOG.CPP
    title: LOG.CPP, the SDL spine, the fan-out, the sinks and the pre-sink fallback
  - id: logprint-cpp
    resource: ../../../LIB386/SYSTEM/LOGPRINT.CPP
    title: LOGPRINT.CPP, CreateLog and the legacy LogPrintf shim
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, main bringing the log up before game-data discovery
  - id: initadel
    resource: ../../../SOURCES/INITADEL.C
    title: INITADEL.C, the console sink and the boot banner
  - id: control-server
    resource: ../../../SOURCES/CONTROL_SERVER.CPP
    title: CONTROL_SERVER.CPP, the socket's record stream and the sink pool
  - id: console-cmd
    resource: ../../../SOURCES/CONSOLE/CONSOLE_CMD.CPP
    title: CONSOLE_CMD.CPP, the loglevel command
  - id: agents
    resource: ../../../AGENTS.md
    title: AGENTS.md, "Logging", the how-to for choosing a severity
  - id: test-log
    resource: ../../../tests/logging/test_log.cpp
    title: test_log, the fan-out, the level, the pool and the LogPrintf shim
  - id: test-spine
    resource: ../../../tests/logging/test_log_spine.cpp
    title: test_log_spine, the same records through SDL's log output
---

The how-to for writing a log line, which severity to pick and what belongs in `Log_Debug`, is in AGENTS.md and stays there.[^agents] This concept holds the mechanism and what it guarantees. The contracts are read at `as_of`; the one measurement below was taken on a macOS Release build at that commit.

# Principles

- **One pipe.** Every `Log_*` call formats once and reaches every sink. There are no categories or channels: filtering by area is a text prefix and a grep.[^agents]
- **SDL's log output is the spine.** A record is handed to `SDL_LogMessage` under one of five custom categories, and the output function the log installs routes it back to the sinks. That same function receives SDL's own messages and any bare `SDL_Log` call in the engine, so they reach `adeline.log` too.[^log-cpp] [^test-spine]
- **One master level.** A single threshold drops a record for every sink before any sink's own floor is consulted. Framing lines bypass it, so a pasted log still says what it is when the level is raised.[^log-cpp] [^test-log]
- **Logging never aborts.** A full sink pool, a file that will not open and a message too long for the buffer all degrade silently.[^log-cpp]

# Contracts

| Contract | Statement |
|---|---|
| The sinks | A file sink on `adeline.log`, a terminal sink on stderr, both added in `main`, and a console sink added in `InitAdeline` once there is a console to print to. The socket adds a callback sink for its record stream, made once for the process.[^perso-cpp] [^initadel] [^control-server] |
| Levels | `LOG_DEBUG` < `LOG_INFO` < `LOG_WARN` < `LOG_ERROR`. `Log_Init` seeds the level permissive; `main` sets it from `--log-level`, then `LBA2_LOG_LEVEL`, then `INFO`, before any sink exists. The `loglevel` console command moves it at runtime.[^perso-cpp] [^console-cmd] |
| What bypasses the level | `Log_Banner`, `Log_BeginSection` and `Log_EndSection`. `Log_Raw` does not, and neither does `LogPrintf`, which emits raw lines. A sink's own floor can only raise the bar above the master level, never lower it.[^log-cpp] [^test-log] |
| Record kinds | Normal records carry a severity tag in the file (`[WARN] ...`) and a colour on a terminal and in the console. Raw and banner lines are verbatim. A section begins as `==== Title ====` in the file and has no closing line there.[^log-cpp] |
| The file | `CreateLog` renames an existing `adeline.log` to `adeline.prev.log`, replacing the one kept before, then opens `adeline.log` with `"wb"`; the file sink then reopens it for append. Exactly one previous run is kept, and a launch that finds no `adeline.log` leaves `adeline.prev.log` alone. Every record is written and `fflush`ed, so a line is in the operating system's hands before the call returns.[^logprint-cpp] [^log-cpp] |
| stderr | The terminal sink always writes. ANSI colour only on a TTY with `NO_COLOR` unset; redirected, it writes the file sink's plain tagged lines, so a harness or CI run shows the log inline. stdout is a data channel and never carries log lines.[^log-cpp] [^agents] |
| Before the sinks | With no sink registered, `Log_*` writes to stderr in the file sink's format, honouring the level. `LogPrintf` falls back to stderr and to the log file if `CreateLog` has already named one.[^log-cpp] [^logprint-cpp] [^test-log] |
| Threads | The console sink drops records raised off the thread that called `Log_Init`, because the console ring is not thread-safe; the file and terminal still get them. The audio callback is the thread this is for.[^log-cpp] |
| Shutdown | `atexit(Log_Shutdown)` is registered in `main` right after the first two sinks, so every exit handler registered later, `TheEndInfo` among them, runs before it and still logs to the file.[^perso-cpp] |

# Seams

| Seam | Direction | Contract |
|---|---|---|
| [Boot](/subsystems/boot.md) | lifecycle | The log comes up after the user folder is resolved and before game-data discovery, which is what puts discovery's errors in the file. See [the decision](/decisions/the-log-starts-before-game-data-discovery.md). The boot banner is written in `InitAdeline` as the first records after the console sink joins.[^perso-cpp] [^initadel] |
| [Console](/subsystems/console.md) | write | `Log_MakeConsoleBufferSink` with an adapter, `ConsoleLogLine`, that maps severity and line kind to a console colour, which keeps the log core in LIB386 free of any SOURCES dependency.[^initadel] |
| [Control harness](/subsystems/control.md) | write | A callback sink streams records to a connected socket client. It is registered once and gated by a flag, because removing a sink does not return its pool slot.[^control-server] |
| `LogPrintf` and `LogPuts` | write | About a hundred legacy call sites, routed through the fan-out as raw lines split on newlines. New code uses `Log_*`.[^logprint-cpp] [^agents] |
| The host tests | read | `test_log` drives the fan-out, the level, the pool limit and the shim without SDL; `test_log_spine` sends the same records through SDL's output, including a bare `SDL_Log`.[^test-log] [^test-spine] |

# What it does not tell you

- **That a crash is recorded.** Nothing catches a fatal signal, so a segfault adds no line. Everything written before it is on disk, because of the per-line flush, and that is all there is.
- **That `adeline.log` is the run that just failed.** Launching again moves it to `adeline.prev.log`, and a second launch replaces that too. A player who crashes and restarts twice before sending the files has lost the run that crashed.[^logprint-cpp]
- **That every boot message reaches the file.** Everything before `CreateLog` goes to stderr only: an unusable profile name, `SDL_Init` failing, and on Android the storage permission wait and its outcome, which run before the user folder is chosen. The user-folder decisions made in that window are recorded and replayed as `Note:` lines in the banner instead. Whether an Android app's stderr is visible anywhere was not checked; nothing in the tree routes the log to logcat.[^perso-cpp] [^initadel]
- **That a failed log file is named.** Measured: with a user folder that cannot be created, the warning reads `Unable to create log file at ''`. `CreateLog` clears the path before printing it.[^logprint-cpp]
- **That a record arrives whole.** A formatted record is cut to fit a 1024-byte buffer, and a `LogPrintf` line likewise.[^log-cpp] [^logprint-cpp]
- **That the console shows what the file shows.** Records from other threads are missing from the console only.[^log-cpp]
- **That removing a sink frees it.** `Log_RemoveSink` takes a sink off the dispatch list and leaves its slot in use. The pool holds eight and boot takes three, so a sink added and removed per event exhausts it in a few cycles and the next sink silently fails to register.[^log-cpp] [^control-server]
- **That LOG.H's summary is current.** Its opening comment says the boot path wires all three sinks in `InitAdeline`; the file and terminal sinks have been added in `main` since the log moved ahead of discovery.[^log-h] [^perso-cpp]

[^log-h]: [LIB386/H/SYSTEM/LOG.H](../../../LIB386/H/SYSTEM/LOG.H), the header comment and `Log_MakeFileSink`.
[^log-cpp]: [LIB386/SYSTEM/LOG.CPP](../../../LIB386/SYSTEM/LOG.CPP), `log_emit`, `is_structural`, `emit_formatted`, `log_fallback_write`, `file_sink_write`, `term_sink_write`, `console_sink_write`, `alloc_sink`, `Log_Init` and `Log_RemoveSink`.
[^logprint-cpp]: [LIB386/SYSTEM/LOGPRINT.CPP](../../../LIB386/SYSTEM/LOGPRINT.CPP), `CreateLog`, `RouteOrFallback` and `EmitLine`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main`, from `GetDefaultUserDir` to `atexit`, and `ProfileNameOrExit`.
[^initadel]: [SOURCES/INITADEL.C](../../../SOURCES/INITADEL.C), `InitAdeline` and `ConsoleLogLine`.
[^control-server]: [SOURCES/CONTROL_SERVER.CPP](../../../SOURCES/CONTROL_SERVER.CPP), `s_log_sink` and the comment above it.
[^console-cmd]: [SOURCES/CONSOLE/CONSOLE_CMD.CPP](../../../SOURCES/CONSOLE/CONSOLE_CMD.CPP), `Log_SetLevel` in the loglevel command.
[^agents]: [AGENTS.md](../../../AGENTS.md), "Logging".
[^test-log]: [tests/logging/test_log.cpp](../../../tests/logging/test_log.cpp), `test_structural_bypasses_level`, `test_sink_pool_limit` and `test_logprintf_early_boot_fallback`.
[^test-spine]: [tests/logging/test_log_spine.cpp](../../../tests/logging/test_log_spine.cpp), `test_spine_routes_bare_sdl_log`.
