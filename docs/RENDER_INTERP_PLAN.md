# Render interpolation: smooth motion above the sim rate (#412)

Plan of record for the render-interpolation half of #412. Companion to
[MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md) (the #358 fixed-timestep sim this builds on),
[TIMING.md](TIMING.md) (the clocks), and [CONTROL.md](CONTROL.md) (the `--fixed-dt` harness that
measures it). The plan is phased so each step is independently validated and ships behind an
off-by-default flag, so no phase can regress the game.

> **Status: parked (2026-07-16).** The #358 fixed-timestep throttle already fixed the *felt*
> high-refresh problem (movement speeding up / slow NPCs freezing at high fps -- the original M1 and
> vsync-off reports). The residual *render staircase* this plan targets was not perceptible on any
> tested hardware: WSL at ~300 fps, an M1/ProMotion Mac, and a **144 Hz Windows** display, all vsync
> off. It only bites on a non-multiple-of-60 refresh with vsync genuinely off, and even there the
> reporter could not see it. Phases 0-2 are implemented and proven in the harness on branch
> `feat/render-interp` (the probe shows the staircase drop from ~77% to ~11% zero-delta frames, and
> the sim `--dump-state` is byte-identical to `main` when the flag is off), kept as resumable WIP.
> Revisit if a real, perceptible high-refresh case surfaces. Tracked in #412.

## The problem, measured

The #358 fix pins the simulation to a fixed ~60 Hz step. Correct, but it leaves a presentation cost:
a display faster than 60 Hz renders frames that have no new sim state, so they re-present the frozen
pose. Held movement staircases. Measured on the V Jump protopack glide (`input up`, `--fixed-dt 4` =
240 Hz emulation, one sim step every 4th frame):

| render frame | per-frame hero move |
|---|---|
| step frame | ~19 units |
| next 3 frames | 0, 0, 0 (re-presented) |
| … | repeats |

**75% of rendered frames repeat the previous position.** Uncapped and headless the same scene renders
at ~658 fps -- 11 duplicate frames per sim step. The total distance is frame-rate independent
(`--fixed-dt 4 --tick 800` and `--fixed-dt 16 --tick 200` land byte-identical), so the sim is right;
only the *presentation* judders.

## The target already exists in the engine

The behaviour menu ([COMPORTE.CPP](../SOURCES/COMPORTE.CPP) `MenuComportement`) renders the hero
perfectly smoothly at any refresh, because its modal loop does two things every rendered frame:

- `ObjectSetInterAnim` -- advances the animation pose off the real clock (keyframe interpolation).
- `InitMove(&RealRot, …)` + `DrawObjComportement(beta = -1)` -- rotation rides the `MOVE` accumulator
  (`velocity × dt`), which carries its sub-unit remainder and is frame-rate independent.

It renders **fresh, frame-rate-independent motion every frame**. That is exactly what throttled
gameplay refuses to do on skip frames. The menu is the visual oracle for this whole campaign.

## Why interpolation (and not the alternatives)

- **Frame cap** (built, then reverted). Holds the render rate down to the sim rate so there are no
  surplus frames. It closes the gap from the wrong side: it removes the staircase but delivers only
  60 distinct poses/sec, beats against non-multiple refresh rates (144/60 = 2.4 → 2:3 judder), and
  fights vsync. With vsync off it just trades a 658 fps staircase for a 60 fps one. Kept in mind only
  as a possible *busy-render backstop* to pair with interpolation later, never as the smoothness fix.
- **Frame-rate-independent locomotion** (`velocity × dt` walking, like the menu's rotation). The
  green-field-correct fix, but walk/run/jump translation is baked into the animation keyframes, so
  running it per-frame reintroduces the #358 high-fps rounding drift. It rewrites the anim-to-world
  pipeline and is not byte-exact -- wrong for a preservation port (already rejected in
  MOVEMENT_FRAMERATE.md).
- **Render interpolation** (chosen). Keep the byte-exact 60 Hz sim untouched; each rendered frame
  *draws* every object at `lerp(previous sim state → current sim state, alpha)`. New position every
  frame (menu-smooth) while the sim, physics, and the anim math stay exactly as they are at 60 Hz.

## The core mechanism

Classic "fix your timestep" interpolation. Per rendered frame:

    alpha      = (TimerRefHR - LastSimRefHR) / FixedTimestep        // banked fraction, [0,1)
    renderPos  = lerp(prevStepPos, curStepPos, alpha)               // per interpolated entity

- `prevStepPos` = position after the previous sim step, `curStepPos` = position after the latest.
  Both already persist on the object across skip frames: `OldPos{X,Y,Z}` (set at each sim-step start
  = post previous step) and `Obj.{X,Y,Z}` (post latest step), [PERSO.CPP:2065](../SOURCES/PERSO.CPP).
  Whether to reuse those or snapshot a dedicated pair is a Phase 2 decision (see risks).
- `alpha` comes from the throttle's own carry: on a skip frame `LastSimRefHR` is frozen while
  `TimerRefHR` advances, so `alpha` sweeps 0→1 across the gap and resets when the next step runs
  ([TIMER.CPP](../LIB386/SYSTEM/TIMER.CPP) `Timer_PlanSimSteps`).
- **Render-only**: overwrite the drawn position before `AffScene`, restore the true sim position
  after. The sim never sees the interpolated value.
- Inherent **~1 sim-step (16 ms) of visual latency** -- interpolation always shows the last
  *completed* step interval. Acceptable for LBA2 (not a twitch game); to be confirmed by feel.

## Validated assumptions (Phase -1 spike, 2026-07-15)

Four parallel read-only code investigations ran before committing to the ladder, to falsify the
load-bearing assumptions first. All four held. Two produced new hard requirements, one shrank the
scope, and the architecture question came back sound.

1. **Render-only is sound (the architecture question).** `overwrite Obj position → AffScene →
   restore` is clean. AffScene rebuilds the sort tree, dirty boxes, projected coords, shadows, and
   sound pan fresh every frame from the source arrays, and `T_OBJ_3D` caches no screen coords, so
   nothing position-derived survives the restore -- *except* two narrow writes, both handled by
   **excluding a small object class** from interpolation (no change inside AffScene):
   - `OBJ_BACKGROUND` bake into the `Screen` plate ([OBJECT.CPP:4966](../SOURCES/OBJECT.CPP), the
     #424/#432 path). Background objects are static and only bake on `AFF_ALL_FLIP` frames → skip
     interpolation for `OBJ_BACKGROUND` objects and on full-flip frames. (The driveable buggy is not
     background -- verified.)
   - `WAIT_COORD` extra-origin write-back ([OBJECT.CPP:4994](../SOURCES/OBJECT.CPP)) writes sim state
     from the drawn position → skip interpolation for `WAIT_COORD`-pending objects (one frame).
   No deeper injection point is needed.

2. **The clock mechanism is correct.** `alpha = (TimerRefHR - LastSimRefHR) / FixedTimestep` sweeps a
   clean sawtooth in [0,1): on a skip frame `LastSimRefHR` is frozen
   ([TIMER.CPP:200](../LIB386/SYSTEM/TIMER.CPP) returns 0 without writing it) while `TimerRefHR`
   advances (`ManageTime` runs every frame). Gate on `FixedTimestep > 0`.

3. **Endpoints need a dedicated snapshot + a teleport guard (new hard requirement).** `OldPos` is a
   *start-of-step* snapshot, and direct position sets (Life `SetPos`, `ChangeCube`, respawns) happen
   after it, so `lerp(OldPos, Obj)` would streak objects across the map on every teleport -- and LBA
   teleports constantly. Required instead:
   - per-object `RenderPrev/RenderCur` (+ `Beta`), updated once per completed sim step
     (`RenderPrev = RenderCur; RenderCur = Obj`), naturally frozen across skip frames;
   - a per-object **snap flag** set on teleport / cube change / `InitAnim` reposition, so that frame
     renders `RenderCur` directly (alpha ignored). No save serialization; reinit on load/cube-enter.

4. **The whole render path is one seam.** No out-of-loop call-site blits of moving objects exist;
   every mover renders through AffScene's rebuilt sort tree, seeded from source arrays. So the pattern
   -- snapshot, lerp the source arrays in place, AffScene, restore -- applies uniformly to `ListObjet`
   plus exactly three side arrays: `ListExtra` (magic ball, dropped pickups, lightning, thrown
   weapons, particles), `ListPartFlow` (flows), `ListDart` (darts). **Doors come free** (SPRITE_3D and
   ANIM_3DS doors are `ListObjet` objects). `ListExtra` trajectories are closed-form in `TimerRefHR`
   (`org + V*t`), so extras can be evaluated directly at the interpolated render clock instead of lerped.

5. **Camera is per-step, frozen on skip frames.** The default *classic* camera is static between
   discrete events, so interpolating the hero against it is correct -- Phase 2 stands alone there,
   which is the common held-movement-judder case. The opt-in *Auto/follow* camera tracks the hero
   every step, so it needs camera interpolation (7 view globals: `VueOffsetX/Y/Z`, `BetaCam`
   shortest-arc, `AlphaCam`, `VueDistance`, `GammaCam`; re-issue `SetFollowCamera`, never lerp the
   matrix) or the hero rubber-bands. The iso-interior edge-scroll has the same per-step camera move.

6. **Pose interpolation is cheap -- and position-only introduces a new artifact.** Today position and
   pose freeze *together* on skip frames (rigid stutter, no sliding feet), so position-only
   interpolation would move the body while the walk pose holds -- introducing slight sliding feet that
   does not exist today. The fix is cheap: the render already tweens the pose (`ObjectSetInterFrame`,
   [OBJECT.CPP:4905](../SOURCES/OBJECT.CPP)) from `obj->Interpolator`, which the sim freezes on skip
   frames. Overriding `Interpolator` with its value at the interpolated render clock before that call
   tweens the pose to match, with no translation double-count (InterFrame writes only limb angles,
   never `Obj.X`). Driving the position lerp and the pose `Interpolator` from the **same alpha** makes
   them exactly consistent (both linear within a keyframe interval) -- feet plant. Pose is a cheap
   fast-follow, not a deferred deep rewrite.

## Non-breaking invariants (every phase must hold)

1. **No-op under `--fixed-dt 16`.** That path never skips (elapsed == 16 → exactly 1 step, alpha
   collapses), so the projection golden, ASM-equivalence tests, and harness determinism stay
   byte-identical. Re-verified after every phase, not assumed.
2. **Off by default** behind a `RenderInterp` flag until the whole campaign is validated on real
   hardware. Flag off = today's behaviour exactly. So any phase is safe to merge.
3. **Sim state fully restored** before the next sim step. A single missed restore feeds an
   interpolated position back into physics/collision and diverges the sim -- the highest risk here.
4. **Every phase has a RED→GREEN oracle** through the render-position probe (Phase 0), plus the
   `--fixed-dt 16` byte-identity guard from invariant 1.

## Phase 0 -- observe the render position

The judder is a *per-rendered-frame* artifact, but `--dump-state` reports the *sim* position, which
stays a staircase by design even after interpolation lands (the sim is untouched). So the first thing
built is the instrument: capture the position actually used for projection, per rendered frame.

- **Instrument.** A render-position trace toggled from the console (like `input trace` / lifetrace):
  log `frame, hero_render_x/y/z` at `AffScene` on every rendered frame, including skip frames. One
  process yields a whole frame sequence -- far cheaper to iterate on than the N-process `--tick`
  sweep, which stays as the cross-check.
- **Metric.** Fraction of *moving* frames whose render-position delta is zero (staircase treads), and
  the per-frame delta coefficient of variation. RED baseline: ~75% zero-delta at `--fixed-dt 4`.
  GREEN target after Phase 2: ~0% zero-delta, near-uniform deltas.
- **Deliverable.** The probe + a RED characterization test + committed baseline numbers. No gameplay
  behaviour change. This phase merges on its own.

## Phase 1 -- the interpolation core (pure, CI)

- Pure functions in TIMER/anim math: `Render_LerpCoord(prev, cur, alphaNum, alphaDen)` and the alpha
  derivation from `(TimerRefHR, LastSimRefHR, FixedTimestep)`.
- Unit tests (no game data, deterministic): endpoints byte-exact (`alpha 0 → prev`, `alpha 1 → cur`),
  midpoint, monotonic, no overshoot, and the byte-exact-off guarantee (`FixedTimestep == 0` or the
  collapsed-alpha case returns `cur` unchanged). Written RED-first.
- No wiring. Merges on its own.

## Phase 2 -- hero position (first end-to-end slice)

- Wire the lerp for the hero (`NumObjFollow`) only: before `AffScene`, set the drawn hero position to
  `lerp(prev, cur, alpha)`; restore after. Behind `RenderInterp` (default off).
- **Oracle.** With the flag on, the Phase 0 probe shows near-uniform per-frame hero deltas (no
  treads) on the protopack glide; with it off, the identical staircase; the sim `--dump-state` is
  byte-identical either way. Guard: `--fixed-dt 16` byte-identical with the flag on.
- **Endpoints (decided by the spike):** a dedicated per-object `RenderPrev/RenderCur` snapshot updated
  once per completed sim step, plus the teleport **snap flag** (validated assumption 3). Not `OldPos`.
- **Exclusions (from the spike):** skip interpolation for `OBJ_BACKGROUND` objects and on
  `AFF_ALL_FLIP` frames, and for `WAIT_COORD`-pending objects (validated assumption 1).
- Validated to **stand alone for the default classic camera** (assumption 5); Auto-cam users need
  Phase 5 with it. Because position-only introduces slight sliding feet (assumption 6), pair the pose
  hook (Phase 3) in the same feel-test, or at least drive both from the one alpha from the start.
- This is the first frame where gameplay should *feel* like the behaviour menu. Stop and feel it.

## Phase 3 -- hero animation pose (fixes the slide Phase 2 introduces)

Position-only interpolation moves the body while the walk pose holds (slight sliding feet), because
today position and pose freeze together. The spike (assumption 6) found this cheap to fix and *not* a
rewrite: the render already tweens the pose via `ObjectSetInterFrame` ([OBJECT.CPP:4905](../SOURCES/OBJECT.CPP))
from `obj->Interpolator`, which the sim freezes on skip frames. Override `Interpolator` with its value
at the interpolated render clock (recompute from `obj->LastTimer`/`NextTimer`) before that call, driven
by the **same alpha** as the position lerp -- both are linear within a keyframe interval, so feet plant
exactly. No translation double-count (InterFrame writes only limb angles). Edge case: when a keyframe
boundary falls between the two bracketing steps, hold the last pose for that one sub-16 ms sub-step
(visually negligible). Oracle: the hero glides *and* strides on the protopack walk with the flag on.

## Phase 4 -- all actors, side arrays, and rotation

The same seam (assumption 4) extends to every mover: all `ListObjet` actors (NPCs, both door types,
bodies -- shadows follow free) plus the three side arrays `ListExtra` (magic ball, dropped pickups,
lightning, thrown weapons, particles), `ListPartFlow` (flows), and `ListDart` (darts). Snapshot/lerp
each source array in place before `AffScene`, restore after -- one uniform pattern. Add facing (`Beta`)
with shortest-arc angle lerp. `ListExtra` may instead be evaluated directly at the interpolated render
clock (its trajectories are closed-form `org + V*t`). Oracle: probe several actors + a thrown magic
ball at once.

## Phase 5 -- camera (Auto/follow and iso edge-scroll)

Only needed where the camera tracks the hero *per step* (assumption 5): the opt-in Auto/follow camera
and the iso-interior edge-scroll. The default classic camera is static between events, so Phases 2-4
already look right there. Interpolate the seven view-parameter globals (`VueOffsetX/Y/Z`, `BetaCam`
shortest-arc, `AlphaCam`, `VueDistance`, `GammaCam`) at the step boundary and re-issue
`SetFollowCamera` per rendered frame to rebuild `MatriceWorld` + `CameraXr/Yr/Zr` consistently -- do
not lerp the derived matrix. Suppress interpolation on frames where the discrete `FlagCameraForcee` /
`SearchCameraPos` override fires. Oracle: probe the projected screen position of a fixed world point
across rendered frames → uniform in Auto cam.

## Phase 6 -- rollout

- `RenderInterp` default decision (on for release, or opt-in one cycle), persisted to `lba2.cfg`,
  live via a console verb.
- Optionally re-introduce the reverted frame cap as a high-value busy-render limiter (native refresh
  or a ceiling like 240) so vsync-off does not burn 658 fps -- now paired with interpolation, so
  every frame still carries fresh interpolated content.
- Re-baselining review. Expected: none, because every golden runs under `--fixed-dt` (no skip frames,
  interpolation inert). Confirm explicitly before shipping.

## Risks and open questions

Resolved by the Phase -1 spike (kept here for the record): `OldPos` is unsafe as an endpoint → use the
dedicated snapshot + teleport snap flag (assumption 3); there are no out-of-loop sprite blits → one
seam over `ListObjet` + three side arrays (assumption 4); `AffScene` is render-only-clean given two
exclusions (assumption 1); pose interpolation is tractable and shares the position alpha (assumption 6).

Still open:

- **State restore (top remaining risk).** Interpolation mutates drawn positions; a missed restore
  corrupts the sim. Prefer one explicit apply/restore pass around `AffScene` over scattered
  save/restore. Test with a long real-clock run diffed against a flag-off run's sim trace, and re-run
  the `--fixed-dt 16` byte-identity guard every phase.
- **Interior vs exterior.** Different render classes and projection-scale sites; validate both.
- **Latency.** 16 ms is one step; measure whether it hurts input feel (throw/melee aim especially). If
  it does, extrapolation (predict forward) is the alternative, at the cost of overshoot on direction
  change -- interpolation is the safe default.
- **`ListExtra` design choice.** Closed-form render-clock evaluation (`org + V*t`) vs prev/cur lerp;
  decide in Phase 4 (closed-form is exact and needs no snapshot, but only extras have it).
- **Modals untouched.** Menus already render per-frame and are smooth; interpolation applies only to
  the in-game render path.

## Test strategy (weight matched to fix)

- Pure core (Phase 1): CI unit tests.
- Each slice (Phases 2-4): local harness test via the render-position probe (RED→GREEN) plus the
  `--fixed-dt 16` byte-identity guard. Local-only (needs retail data), skips cleanly in CI.
- Feel: manual hardware A/B against the behaviour menu as the reference. The probe cannot measure
  "feels smooth"; it measures "renders new positions every frame," which is the necessary condition.
