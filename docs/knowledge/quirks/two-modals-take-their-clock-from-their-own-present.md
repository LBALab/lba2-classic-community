---
type: Quirk
title: Two modals take their clock from their own present
description: OpenInventory's box-opening loop and the end-credits scroll each end on the game clock, and their own BoxUpdate is the only thing in the body that moves it; under a pinned step, presents that do not mint leave both unable to end, while nine other modals exit on a keypress and never notice.
status: draft
scope: "under a pinned step; on a host clock the wall clock moves regardless"
equivalence: untested
asm_origin: "SOURCES/INVENT.CPP:OpenInventory"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T21:00:00Z }
owner: /subsystems/timing.md
manifests: /decisions/presents-mint-the-pinned-step.md
relates_to:
  - /decisions/presents-mint-the-pinned-step.md
sources:
  - id: invent-cpp
    resource: ../../../SOURCES/INVENT.CPP
    title: INVENT.CPP, the box-opening loop in OpenInventory
  - id: credits-cpp
    resource: ../../../SOURCES/CREDITS.CPP
    title: CREDITS.CPP, the scroll loop in GamePlayCredits
  - id: survey
    resource: ../../plan/ENGINE_TICK_POLICY_SURVEY.md
    title: Can the four mint policies collapse into one? A survey, "The counterexample"
  - id: timer-tests
    resource: ../../../tests/timer/test_fixed_step.cpp
    title: The fixed-step host tests
---

# Scope

Under a pinned step. On the host clock the wall clock moves whether or not anything presents, so both loops end as written. The trap opens when a present stops minting.

# Evidence

`untested` for the two loops themselves. The class is pinned in miniature by the fixed-step test whose name says a modal loop can have no clock but its present.[^timer-tests] The two loops were measured by an arm and a control in one binary, gated on an environment variable that the control had to leave unset rather than empty: with presents not minting, the inventory and the end credits exit 124 on the timeout while nine other surfaces exit 0, and with presents minting all eleven exit 0.[^survey]

# Behaviour

`OpenInventory` grows the box with `while (x1 != INV_END_X)`, where the position is interpolated over 300 ms of `TimerRefHR` since the loop began. `BoxUpdate` is the only thing in the body that moves that clock; there is no input poll and no pump. Take the mint away and the condition can never become false.[^invent-cpp]

The credits scroll in `GamePlayCredits` positions each line at `ModeDesiredY` minus the elapsed `TimerRefHR` over 20, and the loop ends when the scroll reaches the terminator line. It polls, but the poll cannot end it; the clock does, and `BoxUpdate` is the clock.[^credits-cpp]

# Why it is load bearing

- "One entry point advances the game clock and presents never do" reads as a consolidation and is a deadlock. Nine of eleven modal surfaces survive it because they exit on a keypress, so a survey of the nine says the change is safe. These two are why `Timer_FixedDtPresent` cannot be deleted before every clock-terminated loop owns a step of its own.[^survey]
- Adding a pump to them is not the tidy-up either. A loop that pumps and presents on the same iteration mints two steps for one, which is the fade shape the survey measured at 32 ms an iteration.
- The credits loop reads as safe because it polls. A poll is not a clock source; only what the loop's exit condition reads decides that.

# What it does not tell you

- Whether a third such loop exists. Eleven surfaces were driven, and the survey's scan counts a present anywhere in a body as a clock source, which a present under a condition is not. The scan cannot find the next one.
- What the loops cost a player under a recording. The mint decision carries the measured figures.

[^invent-cpp]: SOURCES/INVENT.CPP, the loop over `INV_END_X` in `OpenInventory`.
[^credits-cpp]: SOURCES/CREDITS.CPP, the scroll loop in `GamePlayCredits`.
[^survey]: docs/plan/ENGINE_TICK_POLICY_SURVEY.md, "The counterexample" and "The control has to be unset, not empty".
[^timer-tests]: tests/timer/test_fixed_step.cpp, `test_a_modal_loop_can_have_no_clock_but_its_present`.
