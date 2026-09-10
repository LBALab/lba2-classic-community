---
type: Quirk
title: SaveTimer counts up at any depth
description: SaveTimer increments the bracket depth on every call while RestoreTimer no-ops at zero, so an unmatched save raises the floor for the life of the process and every later restore goes inert, with nothing reported in either direction.
status: draft
scope: "unconditional; every build and every clock mode"
equivalence: untested
asm_origin: "LIB386/SYSTEM/TIMERWIN.CPP:SaveTimer"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T18:00:00Z }
owner: /subsystems/timing.md
relates_to:
  - /quirks/restoretimer-restores-one-of-a-pair.md
sources:
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, SaveTimer and RestoreTimer
  - id: timerwin-cpp
    resource: ../../../LIB386/SYSTEM/TIMERWIN.CPP
    title: TIMERWIN.CPP, the same two functions in the original
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, the depth-zero read in SaveGame
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the deferred close in Control_TickHook
---

# Scope

Unconditional. The counter is a plain global and the two functions are four lines each, identical in the original Windows timer and in the SDL port.

# Evidence

`untested`, read from the code. The original is C++, so there is no disassembly, and no test pins the asymmetry itself: `tests/timer` pins the lock window, not the save depth. The three manifestations below were each found by a replay comparing two runs of the clock, which is the only instrument that has noticed so far.

# Behaviour

`SaveTimer` does `if (!CmptMemoTimerRef++)`: the increment happens whatever the depth, and only the transition from zero takes the snapshot. `RestoreTimer` does `if (CmptMemoTimerRef)` and then `if (!--CmptMemoTimerRef)`: at depth zero it does nothing, and only the transition to zero puts the snapshot back.[^timer-cpp] So an unmatched restore is free and an unmatched save is permanent. The depth never returns to zero, and since only the outermost restore rewinds the clock, every bracket opened afterwards is inert for the rest of the process: modals, cinematics and scene changes stop giving back the time they take. Neither direction logs, asserts or returns anything.[^timerwin-cpp]

# Why it is load bearing

The mistake makes no sound where it is made and surfaces as a clock that stopped rewinding somewhere else entirely. Three sites have met it, and they are three presentations of this one fact:

| Manifestation | Where | State at `as_of` |
|---|---|---|
| `LoadGame` ends on an unmatched `SaveTimer`, so a harness `--load` returns with the depth at one and nothing left to close it. | [SOURCES/SAVEGAME.CPP](../../../SOURCES/SAVEGAME.CPP) | Closed from the harness by [the bracket decision](/decisions/harness-load-bracket-closes-at-arming.md); the engine's own call is untouched. |
| `SaveGame` read the clock to save by closing the bracket, reading, and reopening. At depth zero the close is a no-op and the reopen raises the floor: net plus one. | [SOURCES/SAVEGAME.CPP](../../../SOURCES/SAVEGAME.CPP), `SaveGame` | Fixed: at depth zero it reads `TimerRefHR` directly, which is the value the restore would have produced.[^savegame-cpp] |
| `ChangeCube` takes neither half of its own bracket on a load. Its save is gated on `FlagChgCube`, clear on a fresh boot, and its restore sits in the branch taken when `FlagLoadGame` is clear. A played game is balanced because the menu's load path restores right after `ChangeCube`. | [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), [SOURCES/GAMEMENU.CPP](../../../SOURCES/GAMEMENU.CPP) | Untouched. The harness owns that restore since the decision above.[^control-cpp] |

- Making `SaveTimer` guard like `RestoreTimer` is not a fix. It changes what every nested bracket does on every path in the game, and the pairs that are balanced today rely on the count nesting. The repair for a manifestation is at the unmatched call, never in the counter.
- An inert bracket looks exactly like a working one: the code runs, nothing errors, game time is simply not given back. A reader who sees a `SaveTimer` and `RestoreTimer` pair and a clock that did not rewind should ask what the depth was on entry before suspecting the pair.

# What it does not tell you

- The depth at any moment. `CmptMemoTimerRef` is reported by neither the harness nor the recorder; the inertness has been inferred from time the game kept, not read off the counter.
- Whether a given open bracket is a defect. A harness run that never arms a step keeps the load's bracket open and is not repaired, because it has no replay to disagree with.[^control-cpp]

[^timer-cpp]: [LIB386/SYSTEM/TIMER.CPP](../../../LIB386/SYSTEM/TIMER.CPP), `SaveTimer` and `RestoreTimer`.
[^timerwin-cpp]: [LIB386/SYSTEM/TIMERWIN.CPP](../../../LIB386/SYSTEM/TIMERWIN.CPP), the same two functions in the original.
[^savegame-cpp]: [SOURCES/SAVEGAME.CPP](../../../SOURCES/SAVEGAME.CPP), the comment above the depth-zero read in `SaveGame`.
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), the comment above the deferred `RestoreTimer` in `Control_TickHook`.
