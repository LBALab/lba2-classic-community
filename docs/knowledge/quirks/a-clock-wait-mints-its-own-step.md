---
type: Quirk
title: A wait on the game clock mints its own step
description: Eight wait loops that end on the game clock without presenting carry a one-line pump call that looks like a no-op; under a pinned step it is the loop's only clock source, and removing it leaves the loop unable to end.
status: draft
scope: "under a pinned step; outside it the polled pump is two branch tests, and the unpolled one is inert unless a recording is running"
equivalence: untested
asm_origin: "SOURCES/GAMEMENU.CPP:ShowLogo"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T21:00:00Z }
relates_to:
  - /decisions/presents-mint-the-pinned-step.md
  - /decisions/two-pumps-polled-and-unpolled.md
  - /quirks/sample-fades-end-on-the-wall-clock.md
  - /subsystems/timing.md
sources:
  - id: gamemenu-cpp
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the six pumped waits
  - id: invent-cpp
    resource: ../../../SOURCES/INVENT.CPP
    title: INVENT.CPP, the two page flips in GereArdoise
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, ManageTime, FixedDtStep and the pumps
  - id: survey
    resource: ../../plan/ENGINE_TICK_POLICY_SURVEY.md
    title: Can the four mint policies collapse into one? A survey, "A wait with no clock at all"
  - id: timer-tests
    resource: ../../../tests/timer/test_fixed_step.cpp
    title: The fixed-step host tests
---

# Scope

Under a pinned step, which every recording arms. Outside it the polled pump compiles to two branch tests and the unpolled one does nothing unless a recording is holding the clock, so a shipping run is unaffected either way.

# Evidence

`untested` for the loops; the mechanism is an argument with no gap rather than a measurement. `ManageTime` reads `FixedDtNow`; `FixedDtNow` moves only in `FixedDtStep`; `FixedDtStep` is called only from the mint policies; and none of them is reachable from a body whose statements are `ManageTime` and an input poll.[^timer-cpp] One of the eight is drivable and was measured before its pump: the `slide` console verb's wait exits 0 on the host clock and 124 on the timeout under a pinned step.[^survey] The class is pinned by the fixed-step test that says a clock wait with no source cannot advance.[^timer-tests]

# Behaviour

Eight loops wait on the game clock and neither present nor delay, so each carries a pump call as its clock source. Six poll input every iteration and take the polled form: the save confirmation, the game-over screen twice (its zoom loop, whose only present sits under a clip test and so is not a source, and its wait), the end-of-game slide show, the logo the `slide` verb shows, and the demo build's logo, which is compiled only there but called from `MainLoop`.[^gamemenu-cpp] Two do not poll and take the unpolled form: the slate's page flips, left and right.[^invent-cpp] The boot-only logos run before the clock is armed and carry none.

# Why it is load bearing

- The call looks like a no-op, and outside the pinned clock it is one. A tidy-up that removes it changes nothing a shipping run can see and makes the loop unable to end under every recording.
- The rule these loops broke postdates them. TIMING.md says to pump alongside `ManageTime` in any loop that should end under the fixed step; the loops were written in 1997 and the rule in 2026.
- A present under a condition is not a clock source. The game-over zoom loop had one under a clip test, so a clipped body never presented and the deadline never came; the survey's scan, which counts a present anywhere in a body, cleared it.[^survey]
- Which pump matters, and the wrong one is worse than none for a loop that polls; see [two pumps](/decisions/two-pumps-polled-and-unpolled.md).

# What it does not tell you

- Whether the six undrivable sites are reachable by any driver. The game-over screen needs the hero dead, the confirmation a save driven through the menu, the page flips a second slate, and the demo logo the demo build; they hold by the argument, not by a run.
- Whether another loop of this shape exists behind a conditional present. The scan cannot find it; only reading the exit condition can.

[^gamemenu-cpp]: SOURCES/GAMEMENU.CPP, `ShowSaveConfirmation`, `GameOver`, `SlideShow`, `ShowLogo` and `DemoLogo`.
[^invent-cpp]: SOURCES/INVENT.CPP, `GereArdoise`.
[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP, `ManageTime`, `FixedDtStep`, `Timer_FixedDtPump` and `Timer_FixedDtPumpPolled`.
[^survey]: docs/plan/ENGINE_TICK_POLICY_SURVEY.md, "A wait with no clock at all" and "What the scan counts as a clock source, and what it should".
[^timer-tests]: tests/timer/test_fixed_step.cpp, `test_a_clock_wait_with_no_source_cannot_advance`.
