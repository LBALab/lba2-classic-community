---
type: Subsystem
title: Engine timing
description: Two millisecond clocks, one function that advances them, two bracket pairs with different intents, and a harness-only virtual clock source laid over the wall clock.
status: draft
subsystem: timing
as_of: d9cf303d
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T18:00:00Z }
relates_to:
  - /quirks/savetimer-counts-up-at-any-depth.md
  - /quirks/restoretimer-restores-one-of-a-pair.md
  - /quirks/sample-fades-end-on-the-wall-clock.md
  - /subsystems/recording.md
sources:
  - id: timing-doc
    resource: ../../TIMING.md
    title: Engine timing (docs/TIMING.md)
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, the SDL port of the timer
  - id: timerwin-cpp
    resource: ../../../LIB386/SYSTEM/TIMERWIN.CPP
    title: TIMERWIN.CPP, the original Adeline timer
  - id: review
    resource: ../../plan/RECORDER_OBSERVER_REVIEW.md
    title: The recorder as an observer, "Three clock facts worth keeping separate"
---

Distilled from docs/TIMING.md, which stays the reference contributors read; this concept holds the contracts and the seams, and what the reference gets wrong is logged rather than repeated. The principles are structural. The seams are read at `as_of`.

# Principles

- **Two clocks, one direction of dependence.** `TimerSystemHR` is the wall clock as last sampled. `TimerRefHR` is game time, and it advances by the wall clock's delta only while unlocked. Everything a player perceives as the game's pace reads the second: animation, script delays, fades, zone timers, the RNG seed at a cube change.[^timing-doc]
- **Advance and install are different operations.** `ManageTime` is the only place either clock advances. Three functions install a value instead: `RestoreTimer` puts a snapshot back, `SetTimerHR` writes a load's clock, `Timer_EnableFixedDt` zeroes game time at the moment the step is armed. An install is not a delta, and nothing downstream can tell which it got.[^timer-cpp]
- **A bracket's intent is the thing to choose, not its shape.** `LockTimer` and `UnlockTimer` make an interval invisible and then credit it: transparent. `SaveTimer` and `RestoreTimer` make it invisible and discard it: a rewind. Rendering wants the first, a modal that should leave the clock where it found it wants the second, and the two look identical at a call site.[^timing-doc]
- **The fixed step is an overlay on the clock source, not a second clock.** Under `--fixed-dt` the one sample `ManageTime` takes comes from `FixedDtNow` instead of the host, and `FixedDtNow` moves only where something mints a step. Every wait loop therefore has to say where its time comes from, or it does not end.[^timer-cpp]

# Contracts

| Contract | Statement |
|---|---|
| Who advances | `ManageTime` banks `TimerSystemHR - LastTime` into `TimerRefHR` and refreshes `LastTime`, both only while `TimerLock` is zero. About a hundred call sites, most of them modal loops pumping the clock because the main loop's per-tick call is not reached from inside a modal.[^timer-cpp] |
| Lock | Only the outermost `LockTimer` and `UnlockTimer` matter; the count nests. The locked interval is credited on the next unlocked call, so the clock catches up. |
| Save | Only the outermost `SaveTimer` takes the snapshot and only the outermost `RestoreTimer` puts it back; the interval is discarded. The pair is unbalanced in one direction, see [the SaveTimer quirk](/quirks/savetimer-counts-up-at-any-depth.md), and the restore puts back one variable of a coupled pair, see [the RestoreTimer quirk](/quirks/restoretimer-restores-one-of-a-pair.md). |
| Where the fixed step mints | `Timer_FixedDtAdvance` once per tick from the control hook; `Timer_FixedDtPresent` on every present past the tick's first, unless `Timer_FixedDtOverlayPresent` marked it as an overlay; `Timer_FixedDtPumpPolled` for a wait that polls input; `Timer_FixedDtPump` for a wait that does not, which also drives the recorder's wait hook. A polling wait given the unpolled form mints a step on top of the one its poll already took.[^timer-cpp] |
| Game time is not monotonic | A load installs the save's `TimerRefHR`, and a scene change installs a clock rather than advancing one, so across a load or a scene change `TimerRefHR` can go backwards by minutes. It is not a valid rate numerator across a transition.[^review] |
| Focus | Losing window focus locks the timer and regaining it unlocks, unless the harness asked for focus to be ignored; a batch run that paused on focus would be nondeterministic by what else the desktop was doing.[^timer-cpp] |
| Lineage | The original is `TIMERWIN.CPP`, C++ on `timeGetTime`. The SDL port reproduces its logic with the host call swapped, plus the 2026 correction that holds `LastTime` across a lock. `tests/timer` pins the lock window and the fixed-step arithmetic on the host.[^timerwin-cpp] |

# Seams

- **`Record_ClockHook`**, called by `ManageTime` with the sample it is about to bank, before either clock is touched. It sees the sample and never `TimerRefHR`, so an install that bypasses `ManageTime` is invisible to it: `RestoreTimer` calls `ManageTime` and then assigns `TimerRefHR` after the hook has been and gone. A replay therefore performs its own restores rather than reproducing recorded ones. The recorder Subsystem lists this as its third seam class.[^timer-cpp]
- **`Record_WaitHook`**, driven from `Timer_FixedDtPump`: a wait that ends on the clock and does not poll gets no new reading from the clock hook, so the recorder mints one here.
- **Arming.** `Control_Begin` arms the step from `--fixed-dt` before the loop starts, and the recorder's reload path arms it part-way through a run for a console `rec start`. Both reach `Timer_EnableFixedDt`, which zeroes `TimerRefHR` and re-anchors `LastTime`; the tick it happened on is carried in the recording header.
- **The focus edge**, through `HandleEventsTimer`.
- **The call sites.** Every modal or wait loop that pumps `ManageTime` is a place that also has to choose a pump under the fixed step.

# What it does not tell you

- Whether a clock value was advanced or installed. `TimerRefHR` after a load, a scene change, a restore or an arming is a number with no provenance, which is why the recorder carries the baseline and the arming tick as header lines rather than reading them back.
- What the reference doc says about the `ManageTime` body. docs/TIMING.md's listing predates the clock hook and calls `ManageTime` the only mutator of `TimerRefHR`; the three installs above are in the same file. Logged as a doc fix for a separate change.
- How long a session took. A recording's clock stream summed over a session with loads in it comes to a fraction of the elapsed time, because every load reinstalls the save's baseline.[^review]

[^timing-doc]: docs/TIMING.md, "The two clocks", "Lock vs Save" and "Practical guidance".
[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP: `ManageTime`, `SaveTimer`, `RestoreTimer`, `Timer_EnableFixedDt`, the pump functions and `HandleEventsTimer`.
[^timerwin-cpp]: LIB386/SYSTEM/TIMERWIN.CPP, `SaveTimer`, `RestoreTimer` and `ManageTime`.
[^review]: docs/plan/RECORDER_OBSERVER_REVIEW.md, "Three clock facts worth keeping separate".
