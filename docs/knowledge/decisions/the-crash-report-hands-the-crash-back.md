---
type: Decision
title: The crash report hands the crash back
description: The crash handler writes its block and then gives the signal or exception back to whatever was installed before it, so the process dies of the original crash with the same exit status and the same platform report as if the handler were not there; how it hands the crash back differs per platform because each one was measured to break the simpler ways.
status: draft
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T20:30:00Z }
owner: /subsystems/logging.md
constrains:
  - /subsystems/boot.md
sources:
  - id: crash-posix
    resource: ../../../LIB386/SYSTEM/CRASH_POSIX.CPP
    title: CRASH_POSIX.CPP, installing without SA_NODEFER and restoring the previous actions
  - id: crash-linux
    resource: ../../../LIB386/SYSTEM/CRASH_LINUX.CPP
    title: CRASH_LINUX.CPP, re-queueing the original siginfo
  - id: crash-macos
    resource: ../../../LIB386/SYSTEM/CRASH_MACOS.CPP
    title: CRASH_MACOS.CPP, confirming a fault by re-entry
  - id: crash-win
    resource: ../../../LIB386/SYSTEM/CRASH_WIN.CPP
    title: CRASH_WIN.CPP, returning the previous filter's answer and the SIGABRT handler returning
  - id: test-crash-report
    resource: ../../../tests/crash/test_crash_report.cpp
    title: test_crash_report, each way of dying against a run without the handler
---

# Context

A crash report exists so a player can send what a crash left behind, and a player's crash is also seen by the shell or launcher that ran the game, the operating system's crash reporter, a sanitizer in a developer's build and a debugger. The harness reads exit status 139, a negative return code and signal names, and reads 124 as a hang. A handler that ends the process its own way changes some of those: a reset to the default action loses a sanitizer's report, `_exit` breaks the negative return codes, returning survives a signal that was sent rather than raised by a fault, and a fresh `raise` puts the handler on the crashing stack and turns the code into `SI_TKILL`.[^crash-linux]

# Decision

- **Hand the crash back, never end the process.** After the block, the handler restores what was installed before it and lets that run. The process dies of the original signal or exception, with the exit status, the core or tombstone or crash report, and any sanitizer report it would have had.[^crash-posix] [^crash-win]
- **Linux and Android re-queue the original siginfo.** `rt_tgsigqueueinfo` to the crashing thread, with the signal blocked in the handler, so it is delivered when the handler returns: a fault before its instruction runs again, a sent signal with its code and sender.[^crash-linux]
- **macOS confirms a fault by its return.** It has no `rt_tgsigqueueinfo` and synthesizes `si_code` for sent signals, so the handler returns with itself still installed; a real fault comes straight back and goes to the previous action. SIGABRT, SIGFPE and a signal delivered after `svc` are raised at once, and a signal that does not come back within 200 ms is raised from a `SIGALRM` timer the handler owns.[^crash-macos]
- **Windows returns the previous filter's answer.** MinGW's filter hands the exception on, and no CRT handler for a fault signal is installed, since that filter would call one and end the process with exit code 3. The SIGABRT handler returns, so `abort` still ends in `__fastfail`.[^crash-win]
- **The block is finished before the crash is handed back.** A kernel that delivers the signal at once, as qemu-user did, still leaves a whole block.[^crash-posix]
- **One block per process.** Handing a crash back restores every previous action, so a sanitizer's own abort after its report writes no second block.[^crash-posix]
- **Tested against a control.** Each way of dying runs once without the handler and once with it, and the two must end alike.[^test-crash-report]

# Non-goals

- **A notice or a dialog.** The block is written silently; nothing tells the player on the next launch.
- **An off switch.** The handler exists so players can report crashes, and a sanitizer build keeps it, since handing back keeps the sanitizer's report.
- **Keeping the process alive.** The handler never recovers from a crash; recovery exists only inside the handler, for a fault in its own stack walk.[^crash-posix]
- **A second phase after the block.** The `dumpstate` writer uses stdio and walks the object list, which can deadlock or fault in a crashed process; the block carries its scalar fields instead.

[^crash-posix]: [LIB386/SYSTEM/CRASH_POSIX.CPP](../../../LIB386/SYSTEM/CRASH_POSIX.CPP), `crash_handler`, `CrashPosix_RestorePrevious` and `Crash_Install`.
[^crash-linux]: [LIB386/SYSTEM/CRASH_LINUX.CPP](../../../LIB386/SYSTEM/CRASH_LINUX.CPP), `CrashOs_HandOn`.
[^crash-macos]: [LIB386/SYSTEM/CRASH_MACOS.CPP](../../../LIB386/SYSTEM/CRASH_MACOS.CPP), `CrashOs_Reentered`, `CrashOs_HandOn` and `on_confirm_timeout`.
[^crash-win]: [LIB386/SYSTEM/CRASH_WIN.CPP](../../../LIB386/SYSTEM/CRASH_WIN.CPP), `on_unhandled_exception` and `on_abort_signal`.
[^test-crash-report]: [tests/crash/test_crash_report.cpp](../../../tests/crash/test_crash_report.cpp), `check_kind`.
