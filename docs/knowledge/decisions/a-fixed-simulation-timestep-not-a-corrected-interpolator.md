---
type: Decision
title: A fixed simulation timestep, not a corrected interpolator
description: Frame-rate-dependent movement is fixed by simulating in fixed 16 ms steps, zero, one or several per rendered frame, so every user sits at the 60 fps reference the keyframes were authored for, rather than by making the keyframe step carry a remainder, which would diverge from the ASM oracle, or by capping the frame rate, which helps nothing below 60; it pins the cadence and is not a collision fix.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
owner: /subsystems/movement.md
constrains:
  - /porting/anim.md
relates_to:
  - /quirks/the-keyframe-step-carries-no-remainder.md
  - /decisions/sub-steps-hold-the-frames-input.md
  - /subsystems/timing.md
sources:
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent, "Fix" and "Implementation plan"
  - id: pr-throttle
    resource: https://github.com/LBALab/lba2-classic-community/pull/388
    title: fix, frame-rate-independent movement via a fixed-timestep sim throttle
  - id: pr-substep
    resource: https://github.com/LBALab/lba2-classic-community/pull/389
    title: fix, sub-step the simulation at low frame rates
  - id: global
    resource: ../../../SOURCES/GLOBAL.CPP
    title: GLOBAL.CPP, the default and the persistence
  - id: step-test
    resource: ../../../tests/timer/test_fixed_step.cpp
    title: The step count's invariance
---

# Context

Reduced and inconsistent movement was reported first on Android and then on every build: an Apple laptop at the classic resolution, any display above 60 Hz, anywhere vsync was off. The cause was not the platform and not a recent change. The keyframe step that moves every walker only tracks game time near 60 fps, rounding away sub-unit deltas above it and discarding overshoot below it, and the movement code had not changed in the port. What changed was the exposure: vsync as the only frame cap, a corrected game clock that no longer hid the coupling behind uniform slowness, and a higher default render cost that dropped weak hardware below the rate.[^movement-doc]

Three fixes were on the table. Make `ObjectSetInterDep` carry its remainder, the most surgical, and a divergence from the ASM oracle that the equivalence tests and the projection golden would report. Cap the frame rate at 60, a band-aid that leaves everything below 60 as slow as before. Or drive the simulation at a constant step and let the render run at any rate.[^movement-doc]

# Decision

The constant step. `FixedTimestep`, 16 ms by default, persisted in lba2.cfg and settable live; zero is the historical path byte for byte. Each rendered frame runs `elapsed / FixedTimestep` steps of the simulation block, where elapsed is the game time banked since the last step: none above the rate, so the frame re-presents the settled scene and the time carries; one at the rate, which is the historical path; several below it, each advancing the clock by one step, capped with the backlog dropped so a machine below about eight frames a second slows rather than spirals. The step count over any span of wall time is the span divided by the step whatever the chunking, which the host test asserts. Shipped in two halves: the throttle first, default on, because it fixed every desktop report and made the demo reel's cadence host-independent; the sub-stepping second, for the low rate the throttle had left exactly as it was.[^pr-throttle][^pr-substep][^step-test]

Under `--fixed-dt 16` elapsed is one step exactly, so the throttle neither skips nor sub-steps and the harness's byte identity holds; the interpolator's arithmetic runs at the same step in production that it always ran at under the harness, and the ASM oracle is untouched.[^movement-doc]

# Non-goals

- **Correcting the interpolator.** The keyframe step keeps its 1997 arithmetic; the reference rate is pinned around it, not designed out of it.
- **A frame cap.** Vsync stays the only cap and the fix does not depend on it behaving alike on every backend.
- **Collision.** Pinning the cadence made the demo reel's bat scene host-independent and stopped its timing-related stuck hero; the bat still collides with the hero, so the collision issue is related and not resolved by this, and its watchdog stays.[^pr-throttle]
- **Smoothness above 60 fps.** The throttle re-presents the settled pose on skipped frames; interpolating the render between steps is a separate, parked piece of work.
- **A ticker of its own.** The step is a loop around one block of the render's iteration; the modal loops, the renderer calls from game code and the world writes in the render are untouched.

[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "Why it looks like a recent regression", "Fix: fixed simulation timestep" and "Implementation plan".
[^pr-throttle]: Pull request 388 on origin, merged as 55428323, and its "Related: #255" section.
[^pr-substep]: Pull request 389 on origin, merged as 5f8df73d.
[^global]: [SOURCES/GLOBAL.CPP](../../../SOURCES/GLOBAL.CPP), `FixedTimestep` and the comment above it.
[^step-test]: [tests/timer/test_fixed_step.cpp](../../../tests/timer/test_fixed_step.cpp), the file comment.
