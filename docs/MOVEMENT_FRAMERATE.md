# Movement is frame-rate dependent (#358)

Reduced and inconsistent movement speed (NPC walk/patrol, hero walk/sneak, jump
length, scripted crawl-throughs) reported first on Android arm64, then reproduced on
desktop: an Apple M1 at Classic 640×480, and anywhere vsync is off or the display runs
above 60 Hz. This documents the root cause, the evidence, and the fix.

Companion to [TIMING.md](TIMING.md) (the clocks) and [CONTROL.md](CONTROL.md) (the
`--fixed-dt` harness used to measure this).

## The short version

Hero/NPC horizontal movement is not `velocity × dt`. Pressing a direction just selects
a walk/run/jump animation; the actual displacement is baked into the animation's
keyframe translations and applied by `ObjectSetInterDep`
([LIB386/ANIM/INTERDEP.CPP](../LIB386/ANIM/INTERDEP.CPP)), which the main loop calls
once per rendered frame per object (`GereObjAnim`,
[SOURCES/OBJECT.CPP](../SOURCES/OBJECT.CPP), no catch-up loop). So how far an actor
walks in a given span of *game-time* depends on the frame rate. It is only correct in a
narrow band around 60 fps; both slower and faster frame rates lose distance, and the
slowest-moving NPCs are hit hardest. The engine has no frame-rate limiter other than
vsync, so movement quietly rides on vsync actually capping at 60 Hz or below.

By contrast, everything that *is* `velocity × dt` (gravity/falling via `StepFalling`,
conveyor belts via `StepShifting`, turning via `Beta`, projectiles, vehicles) goes
through the `MOVE` accumulator ([LIB386/3D/MOVE.CPP](../LIB386/3D/MOVE.CPP)), which
carries its sub-unit remainder and is frame-rate independent. Those are not affected.

## Evidence

Measured deterministically with `--fixed-dt N`, which pins the per-tick game-clock step
to N ms (so it emulates ~1000/N fps). Same "Desert Island" save, same 3200 ms of
game-time, per-actor displacement of a scripted mover and a slow mover, as a percentage
of their 60-fps distance:

| ~FPS | 1000 | 500 | 250 | 125 | **62** | 31 | 16 |
|---|---|---|---|---|---|---|---|
| scripted mover | 47% | 80% | 93% | 94% | **100%** | 92% | 88% |
| slow mover | **0%** | 26% | 80% | 104% | **100%** | 86% | 71% |

A sweet spot at ~60 fps, falling off both ways. At 1000 fps the slow mover is frozen
(0%); at 16 fps everything is 12 to 29% short. It reproduces on x86_64 (WSL) via
`--fixed-dt` alone, so it is not ARM `long double` arithmetic: it is frame timing. This
matches every field observation. "Vsync off is worse, on is better" (off is uncapped
high fps). "Slower on a high-refresh monitor" (above 60 Hz). "Happens on the M1 at
Classic res" (a fast machine exceeding 60 fps). "Slowest NPCs most affected."

The unit test `tests/movement/test_move_framerate.cpp` reproduces the same shape on a
synthetic 4-keyframe walk with no game data: 100% of ideal at high fps, plateauing at
78% from ~60 fps down.

## Mechanism

Two frame-rate couplings, both in `ObjectSetInterDep`, pulling opposite ways.

1. **Low frame rate: overshoot discard.** When a keyframe completes, the next
   keyframe's window is re-anchored to the current clock
   (`obj->LastTimer = obj->Time; obj->NextTimer = obj->Time + dur` in INTERDEP.CPP). Any
   game-time by which `obj->Time` overshot the keyframe's intended end is thrown away,
   so the animation (and the locomotion baked into it) falls behind real game-time. The
   bigger the frame (lower fps), the more is discarded per boundary. The existing
   `test_cpp_frame_advance` in `tests/ANIM/test_interdep.cpp` even asserts this discard
   as correct, because the original ASM does the same: it is inherent, not a port bug.

2. **High frame rate: per-frame rounding.** The per-frame local delta is rotated to
   world space and `fistp`-rounded to integer coordinates with no fractional remainder
   carried (`obj->X += X0` in INTERDEP.CPP, via `LongRotatePointF`). At high fps each
   per-frame delta is sub-unit; rounding a stream of them drifts, toward zero for a slow
   mover, which is how a slow NPC freezes at 1000 fps. The pre-rotation telescoping
   (`LastAnimStep*`) only cancels the *local*-space truncation; the post-rotation
   rounding is not carried.

Between them the animation clock only tracks real game-time near ~60 fps, which is
where period hardware ran the in-scene loop.

## Why it looks like a recent regression

It is inherent to the 1997 engine, invisible on DOS at a stable ~60 fps. The movement
math (`INTERDEP.CPP`, `MOVE.CPP`, `ANIM.CPP`, `FRAME.CPP`) has had no behavioural change
in the port, only reformatting and warning fixes, and `--fixed-dt 16` displacement is
bit-identical across recent commits. What changed is the *exposure*:

- The game's only frame cap is vsync (`SDL_SetRenderVSync`,
  [LIB386/SYSTEM/WINDOW.CPP](../LIB386/SYSTEM/WINDOW.CPP)); `DisplayVSync` defaults on
  ([SOURCES/GLOBAL.CPP](../SOURCES/GLOBAL.CPP)). Turning it off, or a backend where SDL
  vsync does not hard-block, runs uncapped into the high-fps loss.
- The `ManageTime` fix (`ed0e7a64`, see [TIMING.md](TIMING.md)) made `TimerRefHR` track
  true wall-time honestly; before it, a lagging game clock masked the coupling behind
  uniform slowness.
- Higher default render cost (HD-resolution auto-detect) drops weak or arm hardware into
  the low-fps loss; high-refresh displays sit above 60 Hz with vsync on.

## Fix: fixed simulation timestep

Chosen 2026-07-07. Drive the simulation at a constant 16 ms step regardless of render
rate, so every user sits at the ~60 fps behaviour the animator assumed, and
`ObjectSetInterDep`'s per-step math runs exactly as it does under `--fixed-dt 16`
today. That keeps the ASM-equivalence tests and the projection-corpus determinism
golden green: this pins the function to a constant dt in production rather than changing
its arithmetic (which would diverge from the ASM oracle).

Shape (the standard "fix your timestep"):

- Accumulate real elapsed wall-time each loop iteration.
- Step the simulation in fixed 16 ms quanta (each advancing the game clock by 16),
  reusing the existing `Timer_EnableFixedDt` / `Timer_FixedDtAdvance` primitives
  ([LIB386/SYSTEM/TIMER.CPP](../LIB386/SYSTEM/TIMER.CPP)), and render once per iteration.
- Clamp the catch-up steps per render (a spiral-of-death guard for weak or arm
  hardware): if the accumulator is many steps behind, run a bounded number and drop the
  rest.
- Poll input once per rendered frame.

Add a real frame-rate limiter as a backstop (sleep to 16 ms when vsync does not block),
so correctness never depends on SDL vsync behaving identically across Metal, GL, and
Android. Both belong in the same change, behind a config flag initially so it can be
A/B'd against current behaviour.

The delicate part is not the anim math. It is separating the simulation block from the
render (`AffScene`) and the input poll in `MainLoop`
([SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP)) so the sim can run 0 to N times per rendered
frame. That split gets its own design note before implementation.

Trade-off (accepted): pinning to 16 ms locks in the ~60 fps reference behaviour, giving
consistency and the intended feel at any render rate, not a theoretical keyframe-ideal.
Rejected alternatives: (a) making `ObjectSetInterDep` itself frame-rate independent,
which is the most surgical fix but diverges from the ASM oracle and breaks the
equivalence tests plus the golden; (b) a 60 fps frame cap only, a band-aid that still
leaves sub-60-fps (arm64) slow and does nothing below 60.

## Tests

Test level must match the fix level. The fix is in the main loop, so:

- **Characterization (CI, `tests/movement/test_move_framerate.cpp`).** Drives the real
  `ObjectSetInterDep` over a fixed game-time span at several dt and asserts the walked
  distance matches. CPP-only, deterministic, no game data. RED today; pins the coupling
  as an executable fact. It is not the merge gate (a main-loop fix does not change this
  function's per-call dt sensitivity), it is the root-cause proof.
- **End-to-end (local, `tests/automation/test_move_framerate.sh`).** Loads a save and
  compares how far the scene's scripted movers travel over the same game-time at ~60 fps
  vs ~16 fps. RED today (~12% gap on Desert Island). Needs retail data, so it is
  local-only, not CI.
- **To come with the fix.** A unit test of the fixed-timestep scheduler (given a
  sequence of real frame times, assert the number of 16 ms sim steps and the clamp),
  which is the clean CI GREEN gate; and re-pointing the end-to-end test at a render-rate
  knob (varying render fps with the sim pinned) so it verifies movement invariance.

## Related

- [TIMING.md](TIMING.md): the two clocks, `ManageTime`, and the fixed-dt primitives this
  fix promotes to a gameplay mode.
- [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md): the harness-only virtual clock the fix builds on.
- [CONTROL.md](CONTROL.md): `--fixed-dt`, `--tick`, `--dump-state`.
