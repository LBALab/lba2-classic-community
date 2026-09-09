---
type: Decision
title: Under the pinned step a present is a tick, and the four mint policies stay four
description: The harness clock mints a step of simulation time from four policies through one funnel; the funnel is done, the policies cannot collapse inside the timer because only the loop bodies know where an iteration ends, and until they do the minimum stable set is four.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T21:00:00Z }
constrains:
  - /subsystems/timing.md
relates_to:
  - /quirks/two-modals-take-their-clock-from-their-own-present.md
  - /quirks/a-clock-wait-mints-its-own-step.md
  - /decisions/two-pumps-polled-and-unpolled.md
  - /subsystems/recording.md
sources:
  - id: survey
    resource: ../../plan/ENGINE_TICK_POLICY_SURVEY.md
    title: Can the four mint policies collapse into one? A survey
  - id: ladder
    resource: ../../plan/ENGINE_RENDER_SPLIT_RESEARCH.md
    title: Separating the renderer from the simulation, research
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, FixedDtStep and the four policies
  - id: timer-tests
    resource: ../../../tests/timer/test_fixed_step.cpp
    title: The fixed-step host tests
---

# Context

The engine has no simulation tick of its own. A modal loop blocks `MainLoop` and takes its time from whatever it happens to draw, so the harness reached for the present as a tick source: `BoxBlit`, the only present path, opens with `Timer_FixedDtPresent`, and under the pinned clock putting a frame on screen is what moves game time. Every present site carries that policy, 142 of them at 9f3750f5, and exactly one has ever asked for one: the console, whose per-frame redraw doubled the game clock until its present was marked an overlay.[^survey]

Measured inside one main-loop tick: one mint idle, 15 across a scene change, 61 to 64 across a menu or the inventory, 83 in the holomap, 7158 in the end credits. The inventory's 64 decompose into 20 for the box-opening animation, which is timed on the clock and legitimately costs what it mints, and 35 for one pass of `DrawInventoryScreen`, which presents once per slot of a static grid and mints 560 ms of simulation time for a draw. A recording paces, so a player pays that in wall time: about a second to open the inventory, matched by the mint count to within 2%.[^survey]

# Decision

- **One funnel for the cost, four policies for the decision.** Every step the pinned clock hands out goes through `FixedDtStep`, which is where pacing spends wall time. The decision to hand one out is made by `Timer_FixedDtAdvance` (the loop iterated), `Timer_FixedDtPresent` (a frame reached the screen), `Timer_FixedDtPump` (a wait iterated without drawing) and `Timer_FixedDtOverlayPresent` (the next present is not a frame).[^timer-cpp]
- **The four collapse into one, and not in the timer.** One call per iteration by every loop that iterates retires the present policy, the overlay policy and the free-present flag together. Nothing except `MainLoop` knows where an iteration boundary is, so the call has to be added to the loop bodies, which is step B of the ladder. The deletion half of step A is step B, not something that precedes it.[^survey]
- **Until then the minimum stable set is four.** Drop the overlay policy and the console double-clock returns. Drop the present policy and the game deadlocks in two places; see [the two modals](/quirks/two-modals-take-their-clock-from-their-own-present.md).
- **The heuristic is right at every poll-driven modal and wrong at exactly two shapes**, which can be searched for where a range cannot: a draw loop that presents per drawn element, and a wait loop that pumps and presents on the same iteration, which is every palette fade in AMBIANCE.CPP at 32 ms an iteration instead of 16.[^survey]
- **A step of clock is not a simulation tick.** The recorder's tick number and digest are indexed on `MainLoop` iterations, so a collapse that makes every modal iteration a tick moves every recording's numbering at once. Any collapse keeps the two counts apart.[^survey]
- **`FixedDtTicking` survives any collapse.** It gates presents before the first tick so boot cannot move the clock, and it is shared state rather than a policy.[^timer-cpp]

`tests/timer` pins the shapes in miniature: the tick's first present is free and later ones step, an overlay present does not move the clock, a fade iteration costs two steps, a modal loop can have no clock but its present, a clock wait with no source cannot advance, and arming clears a pending overlay claim.[^timer-tests]

# Non-goals

- **Step A as the ladder first stated it**, one entry point and presents never mint. Built that way the game hangs in two of eleven surfaces, so it is neither behaviour-neutral nor a consolidation; the ladder carries the correction.[^ladder]
- **Fixing the two wrong shapes from inside the timer.** A present under a condition, a per-slot draw, a fade that pumps and presents: each is a loop body's shape, and the fix is the loop owning its step.
- **Pacing a replay.** Only a recording paces. A replay runs flat out, which is right for verification and wrong for playback, and is left open.
- **The rest of the ladder**, one pump for the modal loops onward. Priced there, not here.

[^survey]: docs/plan/ENGINE_TICK_POLICY_SURVEY.md, "The inventory", "What a present actually costs", "The verdict" and "One constraint on the collapse".
[^ladder]: docs/plan/ENGINE_RENDER_SPLIT_RESEARCH.md, "What each step would take".
[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP, `FixedDtStep`, `Timer_FixedDtAdvance`, `Timer_FixedDtPresent`, `Timer_FixedDtPump`, `Timer_FixedDtOverlayPresent` and `FixedDtTicking`.
[^timer-tests]: tests/timer/test_fixed_step.cpp.
