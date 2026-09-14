---
type: Subsystem
title: Logging
description: One call formats a record once and fans it out to a file, stderr and the console through SDL's log output, behind a single master level that structural lines bypass; the previous run's file is kept beside it at launch and the new one is flushed per line, and a fatal signal or unhandled exception appends a CRASH block before the process dies of it as it would have.
status: draft
subsystem: logging
as_of: e8ae05fc
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T20:30:00Z }
relates_to:
  - /subsystems/console.md
  - /subsystems/control.md
sources:
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
  - id: crash-h
    resource: ../../../LIB386/H/SYSTEM/CRASH.H
    title: CRASH.H, Crash_Install, Crash_PrepareThread and Crash_AddState
  - id: crash-cpp
    resource: ../../../LIB386/SYSTEM/CRASH.CPP
    title: CRASH.CPP, the block's lines, the module table, the frame cap and the state fields
  - id: crash-posix
    resource: ../../../LIB386/SYSTEM/CRASH_POSIX.CPP
    title: CRASH_POSIX.CPP, the signal handler, the recovery guard and the alternate stacks
  - id: crash-linux
    resource: ../../../LIB386/SYSTEM/CRASH_LINUX.CPP
    title: CRASH_LINUX.CPP, re-queueing, the unwinder on Linux and the frame-record walk on Android
  - id: crash-macos
    resource: ../../../LIB386/SYSTEM/CRASH_MACOS.CPP
    title: CRASH_MACOS.CPP, confirming a fault by re-entry and the frame-record walk
  - id: crash-win
    resource: ../../../LIB386/SYSTEM/CRASH_WIN.CPP
    title: CRASH_WIN.CPP, the exception filter, the vectored stack check and the SIGABRT handler
  - id: crash-state
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the game fields the block reports
  - id: crash-helper
    resource: ../../../packaging/android/java/org/lbalab/lba2cc/CrashHelper.java
    title: CrashHelper.java, the Android tombstone kept beside the log
  - id: test-crash-core
    resource: ../../../tests/crash/test_crash_core.cpp
    title: test_crash_core, formatting, modules, the frame cap and the state fields
  - id: test-crash-report
    resource: ../../../tests/crash/test_crash_report.cpp
    title: test_crash_report, a child crashing each way, against a run without the handler
  - id: split-symbols
    resource: ../../../scripts/packaging/split-symbols.sh
    title: split-symbols.sh, the release binary's debug info moved into a symbol archive
  - id: symbolize-crash
    resource: ../../../scripts/dev/symbolize_crash.py
    title: symbolize_crash.py, a block's frames matched to symbol files by identity
  - id: releasing
    resource: ../../RELEASING.md
    title: RELEASING.md, "Crash report symbols", where the archives are published
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

## Crash reports

| Contract | Statement |
|---|---|
| Install | `Crash_Install` runs in `main` right after `atexit(Log_Shutdown)`, with the log's path and the build's version and commit. It opens its own append descriptor on the log, caches the loaded modules, gives the main thread room to report a stack overflow, and installs the handler.[^perso-cpp] [^crash-h] |
| The block | Lines starting `CRASH `, appended after the last line the file sink flushed: `==== fatal signal ====`, the `signal` or `exception` line, `build`, `reg`, the `state` lines, the engine's `module` line, the `frame` lines as module+offset, the other modules the frames used, `chain` naming how the crash was handed on, and `==== end ====`. A block cut short still names the signal, the build and the engine's identity. Leading words are stable and values are appended, as for the console.[^crash-cpp] |
| Module identity | `LC_UUID` on macOS, the GNU build ID on Linux and Android, and the link timestamp with the image size on Windows. Frame 0 is the faulting pc and the others are return addresses, so a symbolizer looks those up one byte back. When frame 0 is in no module, as after a call through a null pointer, the next frame is the return address the call left. The symbol table names the functions; release binaries keep it.[^crash-cpp] [^crash-linux] [^crash-macos] [^crash-win] |
| Symbols | Release builds compile with `-g` and move the debug info into a `-symbols.tar.xz` published beside each artifact. `symbolize_crash.py` matches a block's modules to its files by identity and prints each frame's function, file, line and inlined callers. On Windows the stripped image is smaller than the one its debug file describes, and the script derives the shipped size from the debug file's sections.[^split-symbols] [^symbolize-crash] [^releasing] |
| Deep recursion | The first 32 frames and the last 16 are written, with a line counting the frames between. A walk stops when the stack stops climbing or after 2^20 frames, so the tail reaches the thread's start.[^crash-cpp] [^test-crash-report] |
| State | `register_crash_state` adds the scene, the chapter, the behaviour, cinema mode, the fade flag, fps, the game clock and the hero's position, angles, life, body and animation, next to `atexit(TheEndInfo)`. They are read by address when the block is written, and no pointer is followed. A crash before that point writes the block without them.[^crash-state] [^crash-cpp] |
| Handing the crash back | The process dies of the original signal or exception with the platform's report unchanged; see [the decision](/decisions/the-crash-report-hands-the-crash-back.md). One block per process: the first thread to crash writes it, a thread that crashes meanwhile waits for it, and once it is handed on every previous action is restored.[^crash-posix] [^test-crash-report] |
| Nothing in the handler may fault | No stdio, no allocation, no lock. Every walk that reads stack memory runs under a guard that turns a fault inside it into a `nested ... recovered` line, since a fault left to repeat would put the walk in the platform's report instead of the crash. Android reads through `process_vm_readv` instead, which fails rather than faulting.[^crash-posix] [^crash-linux] [^crash-win] |
| Threads | `Crash_PrepareThread` gives a thread an alternate signal stack, or on Windows a stack guarantee. The music decoder thread calls it on entry and the SDL audio callback on each call; bionic gives every Android thread one already.[^crash-h] [^crash-posix] |
| Android's own report | On the next launch, `Android_SavePreviousCrashReport` copies the tombstone of the newest native crash not saved before to `crash-<time>.pb` beside the log, replacing the one saved before, and the banner says so on a `Note:` line.[^crash-helper] [^initadel] |
| Tests | `test_crash_core` checks the block's formatting in memory. `test_crash_report` runs a child that crashes each way, once without the handler and once with it, and compares how it ended, on macOS, Linux and Windows.[^test-crash-core] [^test-crash-report] |

# Seams

| Seam | Direction | Contract |
|---|---|---|
| [Boot](/subsystems/boot.md) | lifecycle | The log comes up after the user folder is resolved and before game-data discovery, which is what puts discovery's errors in the file. See [the decision](/decisions/the-log-starts-before-game-data-discovery.md). The boot banner is written in `InitAdeline` as the first records after the console sink joins.[^perso-cpp] [^initadel] |
| [Console](/subsystems/console.md) | write | `Log_MakeConsoleBufferSink` with an adapter, `ConsoleLogLine`, that maps severity and line kind to a console colour, which keeps the log core in LIB386 free of any SOURCES dependency.[^initadel] |
| [Control harness](/subsystems/control.md) | write | A callback sink streams records to a connected socket client. It is registered once and gated by a flag, because removing a sink does not return its pool slot.[^control-server] |
| `LogPrintf` and `LogPuts` | write | About a hundred legacy call sites, routed through the fan-out as raw lines split on newlines. New code uses `Log_*`.[^logprint-cpp] [^agents] |
| The host tests | read | `test_log` drives the fan-out, the level, the pool limit and the shim without SDL; `test_log_spine` sends the same records through SDL's output, including a bare `SDL_Log`.[^test-log] [^test-spine] |

# What it does not tell you

- **That a crash before step 5 is recorded.** Arguments, `SDL_Init`, the Android permission wait and the user folder run before `Crash_Install`, and a crash there writes nothing.[^perso-cpp]
- **That a block names functions.** Frames are offsets; naming them needs the matching build's symbols. A build with no symbol table, such as an AppImage before its packaging kept one, gives offsets only. Files and lines need the build's symbol archive, and a rolling build's expires with its workflow artifacts after 90 days.[^crash-cpp] [^releasing]
- **That every thread's stack overflow is reported.** A thread neither the engine nor its C library gave an alternate stack, SDL's and the system's own, reports nothing. Measured on macOS: with any handler installed, such an overflow dies of SIGILL instead of SIGBUS, because the kernel cannot build the signal frame; the crash report keeps the exception type and the frames.[^crash-posix]
- **That macOS reports a sent signal the same way.** macOS synthesizes `si_code` for a signal another process sends, so the handler confirms a fault by its return instead. Measured: a signal that does not come back is raised again from a 200 ms `SIGALRM` timer, so its report shows the timer; one raised at once, abort's included, keeps its frames but leaves the report's termination record empty.[^crash-macos]
- **That Windows reports a smashed stack the same way.** On a stack Windows' own handler search cannot walk, the process dies without the unhandled-exception filter. A vectored handler repeats the search's frame check, as far as the first frame with a handler of its own, and writes the block then. Read from the code, not measured: a frame rejected below such a handler, as a C++ frame with cleanups has, is left to the search and writes no block. Measured: the exit code and the Application Error event's code and module match a run without the handler, and the fault offset inside ntdll varied between runs once any handler was installed.[^crash-win]
- **That Android's module list is complete.** It is taken at install; a library loaded later appears in frames as a bare address.[^crash-linux]
- **That `adeline.log` is the run that just failed.** Launching again moves it to `adeline.prev.log`, and a second launch replaces that too. A player who crashes and restarts twice before sending the files has lost the run that crashed.[^logprint-cpp]
- **That every boot message reaches the file.** Everything before `CreateLog` goes to stderr only: an unusable profile name, `SDL_Init` failing, and on Android the storage permission wait and its outcome, which run before the user folder is chosen. The user-folder decisions made in that window are recorded and replayed as `Note:` lines in the banner instead. Whether an Android app's stderr is visible anywhere was not checked; nothing in the tree routes the log to logcat.[^perso-cpp] [^initadel]
- **That a failed log file is named.** Measured: with a user folder that cannot be created, the warning reads `Unable to create log file at ''`. `CreateLog` clears the path before printing it.[^logprint-cpp]
- **That a record arrives whole.** A formatted record is cut to fit a 1024-byte buffer, and a `LogPrintf` line likewise.[^log-cpp] [^logprint-cpp]
- **That the console shows what the file shows.** Records from other threads are missing from the console only.[^log-cpp]
- **That removing a sink frees it.** `Log_RemoveSink` takes a sink off the dispatch list and leaves its slot in use. The pool holds eight and boot takes three, so a sink added and removed per event exhausts it in a few cycles and the next sink silently fails to register.[^log-cpp] [^control-server]

[^log-cpp]: [LIB386/SYSTEM/LOG.CPP](../../../LIB386/SYSTEM/LOG.CPP), `log_emit`, `is_structural`, `emit_formatted`, `log_fallback_write`, `file_sink_write`, `term_sink_write`, `console_sink_write`, `alloc_sink`, `Log_Init` and `Log_RemoveSink`.
[^logprint-cpp]: [LIB386/SYSTEM/LOGPRINT.CPP](../../../LIB386/SYSTEM/LOGPRINT.CPP), `CreateLog`, `RouteOrFallback` and `EmitLine`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `main`, from `GetDefaultUserDir` to `atexit`, and `ProfileNameOrExit`.
[^initadel]: [SOURCES/INITADEL.C](../../../SOURCES/INITADEL.C), `InitAdeline` and `ConsoleLogLine`.
[^control-server]: [SOURCES/CONTROL_SERVER.CPP](../../../SOURCES/CONTROL_SERVER.CPP), `s_log_sink` and the comment above it.
[^console-cmd]: [SOURCES/CONSOLE/CONSOLE_CMD.CPP](../../../SOURCES/CONSOLE/CONSOLE_CMD.CPP), `Log_SetLevel` in the loglevel command.
[^agents]: [AGENTS.md](../../../AGENTS.md), "Logging".
[^test-log]: [tests/logging/test_log.cpp](../../../tests/logging/test_log.cpp), `test_structural_bypasses_level`, `test_sink_pool_limit` and `test_logprintf_early_boot_fallback`.
[^test-spine]: [tests/logging/test_log_spine.cpp](../../../tests/logging/test_log_spine.cpp), `test_spine_routes_bare_sdl_log`.
[^crash-h]: [LIB386/H/SYSTEM/CRASH.H](../../../LIB386/H/SYSTEM/CRASH.H), `Crash_Install`, `Crash_PrepareThread` and `Crash_AddState`.
[^crash-cpp]: [LIB386/SYSTEM/CRASH.CPP](../../../LIB386/SYSTEM/CRASH.CPP), `CrashBlock_Begin`, `CrashBlock_EngineModule`, `CrashBlock_Frame`, `CrashBlock_EndFrames` and `CrashBlock_State`.
[^crash-posix]: [LIB386/SYSTEM/CRASH_POSIX.CPP](../../../LIB386/SYSTEM/CRASH_POSIX.CPP), `crash_handler`, `CrashPosix_Guarded`, `CrashPosix_RestorePrevious` and `Crash_PrepareThread`.
[^crash-linux]: [LIB386/SYSTEM/CRASH_LINUX.CPP](../../../LIB386/SYSTEM/CRASH_LINUX.CPP), `CrashOs_WriteFrames`, `CrashOs_HandOn` and `CrashOs_Prepare`.
[^crash-macos]: [LIB386/SYSTEM/CRASH_MACOS.CPP](../../../LIB386/SYSTEM/CRASH_MACOS.CPP), `CrashOs_Reentered`, `CrashOs_HandOn` and `on_confirm_timeout`.
[^crash-win]: [LIB386/SYSTEM/CRASH_WIN.CPP](../../../LIB386/SYSTEM/CRASH_WIN.CPP), `walk_unwind`, `on_vectored_exception`, `walk_dispatch_check`, `claim_report`, `on_unhandled_exception` and `on_abort_signal`.
[^crash-state]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `register_crash_state`.
[^crash-helper]: [packaging/android/java/org/lbalab/lba2cc/CrashHelper.java](../../../packaging/android/java/org/lbalab/lba2cc/CrashHelper.java), `saveLastNativeCrash`.
[^test-crash-core]: [tests/crash/test_crash_core.cpp](../../../tests/crash/test_crash_core.cpp), `test_frame_cap_keeps_both_ends` and `test_state_fields`.
[^test-crash-report]: [tests/crash/test_crash_report.cpp](../../../tests/crash/test_crash_report.cpp), `check_kind` and `check_tail_leaves_recursion`.
