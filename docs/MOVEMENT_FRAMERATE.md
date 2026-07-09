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

## Implementation plan (the MainLoop split)

The scheduler core landed first as a pure, unit-tested function (`Timer_FixedStepCount`,
[LIB386/SYSTEM/TIMER.CPP](../LIB386/SYSTEM/TIMER.CPP)): fold real elapsed into a carry,
return how many fixed dt-steps are due, clamp runaway backlog. It has no caller yet. The
rest is wiring it into `MainLoop` ([SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP):785).

**The loop today**, per `while(TRUE)` iteration:

| lines | role | cadence under the fix |
|---|---|---|
| 918 `Control_TickHook` | harness seam | once per frame |
| 944-945 `MyGetInput(); ManageTime()` | input poll + game-clock advance | input once per frame; clock becomes the step driver |
| 947-1836 | UI, menus, pause, demo watchdog, camera setup | once per frame (input sampled once, applied to every sub-step) |
| ~1877-2139 | simulation: global movers, `GereAmbiance`/`CheckNextMusic`, `GereExtras`, `AnimAllFlow`, the per-object physics/AI/collision/anim loop, camera centering | run 0..N times per frame |
| 2145 `AffScene` | render | once per frame |

**Why the sim block must loop.** Advancing the game clock by `N*dt` and running the sim
once would just feed `ObjectSetInterDep` one big dt, which is the overshoot bug again. The
sim block has to run `N` times, each after a single `dt` advance, so every animation step
sees `dt=16`. So the block from ~1877 to ~2139 runs inside
`for (step = 0; step < nSteps; step++)`, with the render and the frame-level UI outside it.
`nSteps == 0` (render faster than 60 fps) skips the sim and re-presents the settled frame.

**The catch: input re-entrancy (found during wiring).** The sim block's per-object loop
mixes animation advancement (which we want to run `N` times) with `DoLife`, which processes
hero input. Hero actions are level-triggered off the held input: e.g. the jump path at
[OBJECT.CPP:4056-4090](../SOURCES/OBJECT.CPP) falls into `InitAnim(GEN_ANIM_SAUTE)` whenever
it runs with the action button held. Running the object loop `N` times per frame re-fires
the jump (and combat, throw) `N` times. So `N`-step sub-stepping is only safe once `DoLife`
input/action processing is made once-per-frame (split from the per-object anim/physics
advance, or the momentary-action input bits are masked after the first step). That is real
surgery and its own correctness risk.

**Phased delivery.**

- **Phase 1 (throttle, safe, fixes the high-fps cases), implemented.** Run the sim block at
  most once per ~16 ms of game-time: skip the sim on frames where `TimerRefHR` has advanced
  less than one step since the last sim run (still render). The sim then sees a >= 16 ms delta
  instead of a tiny one, so the high-frame-rate rounding loss is gone. The sim runs **0 or 1
  times per frame**, so there is no input re-entrancy and no object-loop restructure.
  Low-frame-rate (arm64) is left exactly as today (a >= 16 ms frame runs every frame,
  unchanged), so it is neither fixed nor regressed. This covers every reported desktop case
  (vsync-off, high-refresh, M1 at Classic res), and it also stops the demo-reel cube 201
  (Desert Island bat) derail from #255 by making the sim cadence host-independent. Default on
  (16 ms), persisted to `lba2.cfg` (`FixedTimestep` key), toggleable live via the
  `fixedtimestep on|off|toggle|<ms>` console verb (the `FixedTimestep` global,
  [GLOBAL.CPP](../SOURCES/GLOBAL.CPP); the gate is in `MainLoop`,
  [PERSO.CPP](../SOURCES/PERSO.CPP)). Off (0) is byte-for-byte the historical per-frame path;
  the throttle is a no-op under `--fixed-dt 16` (16 >= 16 never skips), so the projection golden
  and the ASM-equivalence tests are unchanged, verified identical. At `--fixed-dt 8` (~125 fps)
  the throttle reproduces the true dt=16 displacement.

- **Phase 2 (sub-stepping, fixes low-fps too), implemented.** The throttle above became a
  unified fixed-step loop: each frame runs `elapsed / FixedTimestep` simulation sub-steps, where
  `elapsed` is the game-time banked since the last simulated sub-step (`LastSimRefHR`). That is
  0 steps at high frame rate (the throttle), 1 step at ~60 fps (the historical path), and N steps
  below ~60 fps (the arm64 low-frame-rate overshoot fix). Each sub-step drives `TimerRefHR` to
  its own value and the sub-16 ms remainder carries via `LastSimRefHR`, so the sim advances at
  the correct average rate at any frame rate; this also removes phase 1's just-above-60 rough
  edge. A `FIXED_TIMESTEP_MAX_STEPS` clamp (8) bounds catch-up below ~8 fps, and a large-jump /
  first-frame / clock-rewind guard re-anchors the step grid.

  The implementation is much smaller than the clock-decoupling plan below anticipated: it reuses
  `ManageTime` as-is, sets `TimerRefHR` per sub-step inside a `goto` loop, and restores it
  afterward, so modals and the `--fixed-dt` harness are untouched and `--fixed-dt 16` stays
  byte-identical (elapsed == 16 -> exactly 1 step, verified). The input re-entrancy hazard is
  handled by masking `Input` down to movement bits (`I_JOY`) after the first sub-step so held
  actions (jump, throw) do not re-fire; `InitAnim` is idempotent
  ([OBJECT.CPP](../SOURCES/OBJECT.CPP)), so keeping the direction bits does not reset the walk
  anim. Verified by `tests/automation/test_move_substep.sh` (dt16 vs dt64 travel now match).

**One-frame signals on a skipped frame -- the invariant.** A skip runs no simulation but still
presents a frame, so anything that must be *read/observed by the sim on the exact frame it changes*
is lost when that frame skips. The rule that keeps this class from recurring: **the throttle may
skip a frame only when it is a true no-op for the sim** -- nothing discrete happened on it.
`Timer_ForceStepIfPending` enforces that by promoting a would-skip frame to a single step when a
discrete signal is pending, keeping the producer and consumer on the same frame. The pending
conditions:

- **A one-frame inventory-use** (`InventoryAction`, set by `MenuInventory`, read by
  `LF_USE_INVENTORY` in `DoLife`). Without the promotion a quest item-use is wiped before any
  simulated frame reads it (#358, softlocks the Peddler). Covered by
  `tests/automation/test_item_use_throttle.sh`.
- **A full-scene flip** (`FirstTime == AFF_ALL_FLIP`, set on scene load / `LM_CAM_FOLLOW`).
  `DoLife` reads `FirstTime` this frame; `AffScene(FirstTime)` then consumes it and the loop tail
  resets it to `AFF_OBJETS_FLIP`. If the flip frame skips, `AffScene` still resets it, so a
  `DoLife` deferred to the next frame reads the stale value. The visible symptom was the attract
  demo's behaviour menu (`LM_COMPORTEMENT_HERO`, suppressed while `FirstTime == AFF_ALL_FLIP`)
  popping over scene setup once the throttle defaulted on. Covered by
  `tests/automation/test_demo_behaviour_menu.sh`.
- **A hero action/fire input edge** (`(Input ^ LastInput) & I_ACTION_EDGE`). The object loop that
  drives the hero is edge-triggered on these bits -- a melee/weapon attack starts on the *press*
  edge, weapon fire stops on the *release* edge -- but it runs only on simulated frames, while
  `LastInput` advances every rendered frame. A press/release landing on a skipped frame is never
  observed, so the blowgun keeps firing on its own and melee never starts while moving (#407,
  same class, reported on keyboard and pad alike). Promoting the edge frame keeps the transition on
  a simulated frame. Movement (`I_JOY`) is deliberately *excluded* from `I_ACTION_EDGE`: it is held
  state the throttle is correct to skip, and forcing a step on every walk change would defeat the
  frame-rate fix. Covered by `tests/automation/test_blowgun_release_throttle.sh`.

All are no-ops under `--fixed-dt 16` (that path never skips), so the goldens are untouched.

> A more robust structure -- running the sim at a fixed 60 Hz and *interpolating* skipped render
> frames, with input latched across the gap (canonical "fix your timestep") -- would end this
> dropped-signal class outright and remove the high-refresh stutter too, but it is not byte-exact
> and needs its own re-baselining. Tracked in #412; the force-step promotion is the low-risk,
> byte-exact containment.

**Clock model (the sensitive part; see [TIMING.md](TIMING.md)).** Separate the game-clock
source from `TimerSystemHR`:

- `TimerSystemHR` stays real (`SDL_GetTicks`) so the FPS counter, audio/sample fades
  ([AMBIANCE.CPP](../SOURCES/AMBIANCE.CPP)), texture animation
  ([ANIMTEX.CPP](../SOURCES/ANIMTEX.CPP)), and the HQR LRU keep real-time behaviour.
- `TimerRefHR` (the game clock the simulation reads) advances only by explicit `dt` bumps:
  once per sim-step in the main step-loop, and once per iteration in the modal subloops that
  already pump the clock for the harness (`Timer_FixedDtPresent`/`Timer_FixedDtPump`, the
  `SaveTimer`/`RestoreTimer` modals in `MESSAGE.CPP`/`INVENT.CPP`/`AMBIANCE.CPP`).

This generalizes the existing fixed-dt overlay ([FIXED_DT_PLAN.md](FIXED_DT_PLAN.md)): the
harness case is this same model with `TimerSystemHR` *also* virtual for determinism. Because
the harness path is unchanged, `--fixed-dt 16` stays bit-identical and the projection golden
and ASM-equivalence tests are untouched. The refactor is contained to `TIMER.CPP` (a
game-clock source distinct from `TimerSystemHR`, banked in `ManageTime`) plus the step-loop
in `PERSO.CPP`.

**Backstops and rollout.**

- Clamp catch-up steps per frame (`Timer_FixedStepCount`'s `maxSteps`) so weak/arm hardware
  that renders slower than the sim rate cannot spiral; the dropped backlog is bounded slow-mo
  under overload, not a freeze.
- Add a real frame-rate limiter (sleep toward 16 ms when vsync does not block) so we do not
  busy-render thousands of duplicate frames at very high fps, and so correctness never depends
  on SDL vsync behaving across Metal/GL/Android.
- Land behind a config flag (default off first), so it can be A/B'd on real hardware at 60 /
  144 / vsync-off before becoming the default.

**Open decision:** whether to enable by default in the first release or ship it opt-in for one
cycle of field testing. This touches the game clock, which is the most bug-prone surface in
the port (see the `ManageTime` history in [TIMING.md](TIMING.md)), so a review of the clock
model is warranted before the `TIMER.CPP`/`PERSO.CPP` change.

## The end-credits loop (#403)

The end credits run their own loop (`GamePlayCredits`,
[SOURCES/CREDITS.CPP](../SOURCES/CREDITS.CPP)) outside `MainLoop`, so the same
`ObjectSetInterDep` coupling survived the fix above: the credits walkers lost distance on
high-refresh displays while the rest of the game was already frame-rate independent. Both
the demo/attract credits (`Credits(FALSE)`) and the final "happy end" credits
(`Credits(TRUE)`) share this loop; `mode` only branches scene setup, not the walk.

The fix is a lighter variant of Phase 1: the credits object loop advances off a game clock
quantized to whole `FixedTimestep` steps (via the same `Timer_FixedStepCount` core), while
input, music, and the text scroll keep the real clock. On a held-clock frame
`ObjectSetInterDep` is a no-op (its `TimerRefHR <= obj->Time` guards), so a high-refresh
frame simply re-presents the settled walkers instead of advancing them a sub-step. No
sub-stepping loop is needed: the credits scene never renders below 60 fps, so the
low-frame-rate catch-up the main loop handles does not arise. Off (`FixedTimestep == 0`) is
the historical per-frame path.

One subtlety specific to this loop: every walker heads to the same central "hello" spot, so
their on-screen spacing comes only from the stagger between spawns. The spawn is therefore
gated on the same step count as the travel -- spawning per render frame while moving per
step collapses the stagger at high fps and the walkers bunch up. Note the credits have no
collision avoidance at all, so walkers that draw near-equal spawn angles already overlap in
the unmodified engine at 60 fps; that is pre-existing behaviour, not a frame-rate artefact,
and is tracked as a separate enhancement.

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
- **Scheduler core (CI, `tests/timer/test_fixed_step.cpp`).** Drives the pure
  `Timer_FixedStepCount` and asserts the step count over a fixed span of wall-time is
  identical across steady / fine / coarse / jittery frame pacing (exactly 1000 steps over
  16000 ms at dt=16), plus the clamp and guards. GREEN. This is the clean CI gate for the
  fix's core logic.
- **To come with the wiring.** Re-point the end-to-end test at a render-rate knob (vary
  render fps with the sim pinned) so it verifies movement invariance after the MainLoop
  split, rather than the `--fixed-dt` sweep (which drives the sim step directly).

## Related

- [TIMING.md](TIMING.md): the two clocks, `ManageTime`, and the fixed-dt primitives this
  fix promotes to a gameplay mode.
- [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md): the harness-only virtual clock the fix builds on.
- [CONTROL.md](CONTROL.md): `--fixed-dt`, `--tick`, `--dump-state`.
