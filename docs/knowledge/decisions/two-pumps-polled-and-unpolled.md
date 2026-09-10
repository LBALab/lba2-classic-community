---
type: Decision
title: Two pumps, for a wait that polls and one that does not
description: Timer_FixedDtPump mints a step and drives the recorder's wait hook, for a wait with no input poll; Timer_FixedDtPumpPolled mints only, for a wait that polls, because a polling loop already gets a fresh reading per iteration and the hook would mint a second step and sleep it out.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T21:00:00Z }
owner: /subsystems/timing.md
constrains:
  - /subsystems/timing.md
relates_to:
  - /subsystems/recording.md
sources:
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, the two pumps and the comment between them
  - id: survey
    resource: ../../plan/ENGINE_TICK_POLICY_SURVEY.md
    title: Can the four mint policies collapse into one? A survey, "The pump a polling wait needs"
---

# Context

The recorder's wait hook is reached from exactly one place, the pump. It exists because a loop that never polls gets no new reading from the clock hook, there being no poll to take one at, so the recorder mints one at the pump instead. Its unstated precondition was that the loop takes no input poll. A loop that polls already gets a fresh reading per iteration; the hook then mints a second step the loop did not need and sleeps out the real time for it, in loops that run thousands of iterations a second on a host clock.[^survey]

Measured on a loose-clock recording that crosses the logo wait, 534,570 polls over 258 ticks, replayed three ways: clean in 8 seconds before any pump; with the unpolled pump in the polling waits, no first progress line after 60 seconds, where progress prints every 20,000 polls; with the polled pump, clean in 8 seconds with the same numbers as the unpumped replay.[^survey]

# Decision

Two functions, chosen by whether the loop polls. `Timer_FixedDtPump` mints and drives the wait hook, for a wait that does not poll: the slate's page flips and the fade loops. `Timer_FixedDtPumpPolled` mints only, for a wait that does: the six polled waits in the menus. The comment between the two in the timer says which is which and why.[^timer-cpp] Outside the pinned clock the polled form is inert; the unpolled one is inert only while nothing is recording.

# Non-goals

- **One pump.** The survey's collapse of all four policies into one call per iteration would retire the distinction, and it is step B of the ladder, not something the timer can do alone.
- **Detecting a poll from inside the hook.** The loop knows whether it polls; the hook does not, and guessing from call counts is what the measurement above rules out.
- **Pumping the boot-only logos.** They run before the clock is armed and never meet either pump.

[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP, `Timer_FixedDtPumpPolled` and `Timer_FixedDtPump`.
[^survey]: docs/plan/ENGINE_TICK_POLICY_SURVEY.md, "The pump a polling wait needs is not the pump a silent one needs".
