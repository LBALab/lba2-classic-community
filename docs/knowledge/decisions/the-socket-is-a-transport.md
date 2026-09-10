---
type: Decision
title: The socket is a transport, not an arming
description: --listen puts a line server in front of the console bus so a driver can steer a running engine, and does nothing else; it does not arm the harness, does not pin the clock, and gives up reproducibility on purpose, so CI stays on --exec-at.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
owner: /subsystems/control.md
constrains:
  - /subsystems/control.md
  - /formats/control-socket-protocol.md
sources:
  - id: server-h
    resource: ../../../SOURCES/CONTROL_SERVER.H
    title: CONTROL_SERVER.H, what it is for and what it is not for
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the argument parser
  - id: control-doc
    resource: ../../CONTROL.md
    title: CLI control harness (docs/CONTROL.md), "Driving a running engine"
---

# Context

`--exec` and `--exec-at` need the whole plan before boot. A probe loop whose next step depends on what the last one showed cannot be written that way, so every observation costs a boot, a load and a walk back to the scene. Searching the camera cvars for a value that fixes a behaviour is that shape. Measured on one fixture: 0.6 to 1.2 ms per command over the socket against 282 ms for a boot per probe, and 1 to 2 ms for a mid-session `load` to reset the scene.[^control-doc]

The tick hook has one call site inside `MainLoop`, and a modal inner loop never reaches it, while every mode presents. So a channel that has to answer during a menu or a cinematic cannot hang off the hook.[^server-h]

# Decision

- **A line server in front of the console bus, and nothing more.** The verbs and cvars already there answer a driver outside the process while the run proceeds. No new commands exist for the socket.[^server-h]
- **Polled from the present path, on the game thread.** Every mode presents, so the socket is serviced inside menus and cinematics where the tick hook is dark. No threading: reads of engine globals are as safe as in the hook.[^server-h]
- **It does not arm the harness.** Parsing `--listen` records the port and sets nothing else, so a bare `--listen` boots the game normally and answers from the main menu. Arming would bypass the menu and leave a live driver able to reach everything except the menus.[^control-cpp]
- **Not deterministic, by construction.** Commands land at whatever tick the driver sent them, so a session does not replay. That is the trade for reacting to what you see. When a run needs to be reproducible, `--exec-at` is still the tool, and CI stays on it.[^control-doc]
- **Kept out of shipped binaries.** Built only with `LBA2_CONTROL_SERVER`, live only with `--listen`, bound to loopback. It runs arbitrary console commands for whoever connects.[^server-h]
- **A slow driver loses lines, never frames.** Sends are bounded, a client that will not drain is dropped, and pushed events queue in a ring that discards oldest-first and says how many it dropped.[^control-doc]

# Non-goals

- **Replayable socket sessions.** A command's tick is not recorded against the session; the recorder captures what the console ran and where, which is a different instrument.
- **A persistent batch process** that boots once and loads many saves. It would erase the per-process boot cost, and it reuses global engine state across loads, which is exactly what a fresh process per run exists to rule out. Not done until batch-loaded dumps are shown equal to fresh-process dumps across the corpus.
- **Windows and macOS parity as an afterthought.** Berkeley sockets and Winsock sit behind one seam, because the automation this serves runs on Windows too and a diagnostic channel missing on the platform a bug came from is the wrong half to have.[^server-h]

[^server-h]: [SOURCES/CONTROL_SERVER.H](../../../SOURCES/CONTROL_SERVER.H), the header comment.
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), the `--listen` case in `Control_ParseArgs`, which records the port and leaves the active flag alone.
[^control-doc]: [docs/CONTROL.md](../../CONTROL.md), "Driving a running engine" and "Notes and limits" beneath it.
