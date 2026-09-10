---
type: Decision
title: The harness closes the load's timer bracket at the first armed tick
description: A harness --load returns with the timer bracket one level open, so the harness closes it itself, at the first tick the deterministic step is armed rather than at the load, so that both ends of a recording close it in the same clock regime.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T18:00:00Z }
owner: /subsystems/control.md
manifests: /quirks/savetimer-counts-up-at-any-depth.md
relates_to:
  - /subsystems/recording.md
sources:
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the comments at the pending flag and the deferred close
  - id: commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/e44bb58b
    title: fix(control), close the timer bracket the harness load opens
  - id: savegame-commit
    resource: https://github.com/LBALab/lba2-classic-community/commit/fef7af79
    title: fix(save), keep SaveGame's timer bracket balanced at depth 0
---

# Context

`LoadGame` ends on an unmatched `SaveTimer`, and on a load `ChangeCube` takes neither half of its own bracket, so both harness entry points return from the load with the depth at one. Measured headless at a fixed 16 ms step: `--load` one save and no restores, final depth one; `--save-load-test` the same; a run with no `--load` two saves and three restores, two real and one inert, final depth zero. Left open, every nested bracket for the rest of the run is inert. Driven into a found-object cinematic on a load run, two brackets opened and both restores were inert: the run kept 432 ms that a played game gives back.[^commit]

The menu does not have this problem. Its load path restores immediately after `ChangeCube`, which is what balances a load for a played game. Driving the load from outside the menu means owning that step.[^commit]

# Decision

Close it from the harness, in the tick hook at the first tick the step is armed, not where the bracket was opened. The close calls `ManageTime`, which samples the clock: with the step armed the sample is the frozen virtual one and the call banks nothing; without it the sample is the host clock and the call banks real elapsed time the rewind then discards. A recording started mid-session from the console is unarmed at the load while its replay is armed there, so closing at the load puts the two runs in different clock regimes and the run diverges hundreds of ticks later, intermittently. Deferring to the first armed tick puts both ends in the same regime. They do not close on the same tick and do not need to; what has to match is that neither banks host time.[^control-cpp]

`SaveGame`'s depth-zero read had to land first. The harness load had been masking it: with a bracket always open, `SaveGame` ran at depth one and its close-read-reopen balanced. Closing the bracket without that fix would have left a recording run one level above the replay it is compared against.[^savegame-commit]

# Non-goals

- **Repairing the engine's own halves.** `LoadGame`'s trailing save and `ChangeCube`'s gates are untouched. `Control_Begin` has one caller, gated on the harness being active, and the save-load test mode is set from its own flag; neither is reachable in a played game, so this is harness only.
- **A run that never arms a step.** It keeps the bracket open and keeps the time a played game gives back. It has no replay to disagree with and no oracle that would notice, and it is not repaired here; the defect was described as every `--load` harness run, and this covers the armed ones.
- **Observing the save-load test path's depth.** That path exits immediately after the load and its depth is unobservable today. It carries the same close so that the next step added between the load and the exit does not trip on an open bracket.

[^control-cpp]: SOURCES/CONTROL.CPP, the comment above the pending flag in `Control_Begin` and the deferred `RestoreTimer` in `Control_TickHook`.
[^commit]: Commit e44bb58b on origin, "fix(control): close the timer bracket the harness load opens".
[^savegame-commit]: Commit fef7af79 on origin, "fix(save): keep SaveGame's timer bracket balanced at depth 0".
