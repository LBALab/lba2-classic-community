---
type: Quirk
title: RestoreTimer restores one of a coupled pair
description: The snapshot SaveTimer takes is TimerRefHR alone and RestoreTimer puts that one variable back; LastTime is whatever the ManageTime call inside the restore left it, so the rewind discards the interval only when the timer is unlocked at that moment.
status: draft
scope: "unconditional; the consequence differs locked against unlocked"
equivalence: untested
asm_origin: "LIB386/SYSTEM/TIMERWIN.CPP:RestoreTimer"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T18:00:00Z }
relates_to:
  - /subsystems/timing.md
  - /quirks/savetimer-counts-up-at-any-depth.md
sources:
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, ManageTime and RestoreTimer
  - id: timerwin-cpp
    resource: ../../../LIB386/SYSTEM/TIMERWIN.CPP
    title: TIMERWIN.CPP, the original
---

# Scope

Unconditional: the restore always writes one variable. What follows depends on `TimerLock` at the moment of the restore.

# Evidence

`untested`, read from the code. No test pins it; the lock-window test in `tests/timer` covers `LastTime` across a lock, not across a restore. The original has the same shape.[^timerwin-cpp]

# Behaviour

`ManageTime` advances `TimerRefHR` by `TimerSystemHR - LastTime` and then sets `LastTime` to the sample, both only while unlocked. `SaveTimer` snapshots `TimerRefHR`. `RestoreTimer` calls `ManageTime` and then assigns `TimerRefHR` back; `LastTime` is not in the snapshot.[^timer-cpp]

Unlocked, the `ManageTime` inside the restore has just set `LastTime` to the current sample, so the interval is discarded cleanly and game time resumes from the snapshot. Locked, that call leaves `LastTime` alone, and the next unlocked `ManageTime` banks everything since the last unlocked one on top of the rewound value: the rewind discards nothing.

# Why it is load bearing

- A `SaveTimer` and `RestoreTimer` pair inside a `LockTimer` window is not a rewind, and nothing at the call site says so. Reasoning that "the modal gave the time back" needs the lock depth as well as the bracket.
- The partial snapshot is what makes the unlocked case correct, not an oversight. Restoring `LastTime` as well would hand the next `ManageTime` the whole interval to bank, and the modal would cost double.
- The restore is an install, not an advance, so the recorder's clock hook never sees it; see [engine timing](/subsystems/timing.md). A replay reproduces the restores by performing them, not by reading them back.

# What it does not tell you

- What the rewind discarded. No record of the interval is kept anywhere.
- Whether a given modal is locked when it restores. That is a property of its callers, not of the bracket.

[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP, `ManageTime` and `RestoreTimer`.
[^timerwin-cpp]: LIB386/SYSTEM/TIMERWIN.CPP, `ManageTime` and `RestoreTimer`.
