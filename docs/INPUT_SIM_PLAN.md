# Input simulation and the throttle-drop class: plan

Goal: a headless way to drive the hero with sustained, combinable key input and assert on the
game's response, so an input-handling regression can be caught in CI instead of on a real pad.
The counterpart engine goal: no input edge is ever lost, whatever the frame/sim cadence.

This is the build plan that follows the findings in
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md) (there was no recorded-input subsystem to
reuse; the seam is `MyGetInput`/`Control_TickHook`). Scope agreed with Ivan: phases 0 to 2
(harness simulator plus the engine latch that retires the force-step whitelist). The render/sim
interpolation half of issue #412 (which also cures the movement stutter) is out of scope here.

## Why framestep still gobbles input

Two facts in the current code, plus a measured harness gap.

1. Edge detection lives in the render domain, but is consumed in the sim domain.
   `LastInput = Input` runs every rendered frame (PERSO.CPP MainLoop, ~L950), but the hero object
   loop that reads edges (`LastInput & I_THROW`, `LastInput & I_ACTION_M` in OBJECT.CPP MOVE_MANUAL)
   only runs on stepped frames. A press or release straddling a skipped frame is never observed.

2. The current fix is a whitelist, not a cure. `Timer_ForceStepIfPending` (PERSO.CPP ~L1968) forces
   a sim step only when a named one-frame signal is pending:
   `(InventoryAction >= 0) || (FirstTime == AFF_ALL_FLIP) || ((Input ^ LastInput) & I_ACTION_EDGE)`.
   That OR grew one term per bug: #358 item-use, #411 scene-flip, #407 input edges
   (`I_ACTION_EDGE`, INPUT.H). Any edge-consuming bit not in the whitelist still drops.

   Scope check (done for this plan): the bits outside `I_ACTION_EDGE` that could matter (weapon
   select `I_WEAPON_*`, behaviour select `I_NORMAL/SPORTIF/AGRESSIF/DISCRET`, `I_HOLOMAP`,
   `I_PAUSE`, `I_MENUS`) are all read at PERSO.CPP:1221-1842, which is *before* the throttle gate,
   so they run every rendered frame and the throttle cannot drop them. The only post-gate `Input`
   edge consumers are the object-loop combat/action edges, and those are exactly the set
   `I_ACTION_EDGE` covers. So the engine's live drop exposure today is narrow and mostly patched.
   The problem the whitelist leaves open is structural: it is a hand-maintained list, so the next
   code that reads a new input edge post-gate drops silently until someone adds a term.

3. The harness injector has the same failure it is meant to test, and a metering bug on top.
   The `input` console command ORs `DbgInjectInput` into `Input` after `MyGetInput` (PERSO.CPP
   ~L956), every *rendered* frame, counting down `DbgInjectInputFrames` in *rendered* frames. So a
   "hold" is metered in render iterations, not sim ticks, and injected input rides the render
   domain (subject to the same drop as real input). This makes the simulator unusable as a
   regression net across throttle settings.

## Phase 0 findings (measured)

Save `Desert Island.LBA` (steam_classic_2023 corpus), `--fixed-dt 8`, `--tick 60`, hero position
from `--dump-state`. Baseline with no input is `(13646, 1360, 15311)` at every throttle setting
(deterministic, so the scene is not a confound).

Same command `input up 30`, varying only the throttle:

| `--fixed-timestep` | hero end position    | distance walked |
|--------------------|----------------------|-----------------|
| 0 (no throttle)    | (13679, 1360, 15436) | ~129 units      |
| 16                 | (13676, 1360, 15430) | ~123 units      |
| 100                | (13646, 1360, 15311) | 0 units         |

At fixed-timestep 100 the 30-render-frame hold expired before any sim step consumed it, so the
hero did not move at all. It is a metering artifact, not terrain: `input up 300` at fixed-timestep
100 does walk the hero (to `(13715, 1360, 15548)` in 60 ticks, and far further over 300 ticks,
comparably to no-throttle). So the simulator cannot express a sustained hold that means the same
thing across cadences, which is precisely the stated requirement ("hold keys and combinations for
a sustained time").

Note this case is movement (`I_JOY`), which is held state the sim throttle is correct to skip; the
bug here is the *harness* metering, independent of the engine. The engine-side edge drop is the
separate, narrow, whitelist-managed case in fact 2.

## Design: two layers that prove each other

### Layer A: engine input latch (the cure)

Invariant: between two consecutive sim steps, no input edge is lost.

Split the edge baseline out of the render domain. Keep render-domain `Input`/`LastInput` for
view-only reads (camera, mouse-look) as now. Add a sim-domain accumulator: every rendered frame
after `MyGetInput`, `SimInputAccum |= Input`; the edge baseline `SimLastInput` advances only when
a sim step actually runs. The object loop reads edges against `SimInputAccum`/`SimLastInput`, not
the render-frame snapshot. On a skipped frame nothing is consumed, so the latch carries the edge
to the next natural step.

This is the general form of the `I_ACTION_EDGE` whitelist: instead of forcing a step so the live
edge is seen now, latch the edge so the next step sees it, for any bit. It retires the input term
from the force-step OR (the item-use and scene-flip terms stay: those are engine signals, not
`Input` bits). Byte-exact at `--fixed-dt 16`: one step per frame means `SimInputAccum == Input`
and `SimLastInput == LastInput` every frame, so behaviour differs only on skip frames, which is
the buggy path (same argument that cleared #407/#411/#358).

Core implementation risk: reconciling level reads (`Input & I_UP`) with edge reads
(`Input ^ LastInput`) under a union accumulator. Proposed semantics: on a step the sim sees
`SimInputAccum` (union of held bits since last step, so a brief press is not missed) against
baseline `SimLastInput`; after the step `SimLastInput = SimInputAccum`, the accumulator clears and
re-primes from the current render `Input`. The byte-exactness proof concentrates here.

### Layer B: input simulator (the tool) -- implemented (Phase 1)

Generalised `DbgInjectInput` into a sim-tick-metered, scriptable timeline (`Control_Input*` in
`CONTROL.CPP`, driven from the console `input` command). Command surface:

- `input <flags> [ticks]` holds the OR of the flags for N sim ticks (default 1). Combine flags
  with `+` or `,` (e.g. `input up+throw 30`), or give a raw `0xNNN` mask.
- `input seq <t:flags:dur/...>` schedules a timeline of events at absolute start ticks, e.g.
  `input seq 0:up:60/60:up+throw:30`; a one-tick event is a press, a bit dropped between events is
  a release. Chained through the single `--exec` buffer with `;` (so events use `/`, not `;`).
- `input fseq <f:flags:dur/...>` is the same but measured in RENDERED frames, not sim ticks, on a
  separate frame clock that advances every rendered frame (skipped ones included). This is the tool
  for reproducing the throttle edge-drop: `input fseq 3:action:4` places a press on rendered frame 3,
  which at a high refresh rate is a frame the sim never steps, so the object loop never sees it.
- `input trace <0|1>` logs the injected mask + hero `gen_anim`/position each sim tick (the "watch
  the hero to confirm" half), and `input off` clears.

Metering unit: SIM TICKS, not rendered frames. The hold's countdown advances only on frames the
throttle actually steps (`Control_InputAdvance(nSimSteps)` before the sim runs; a skipped frame
`goto`s past it and consumes nothing), while the mask is OR'd into `Input` every rendered frame so
whatever step lands sees it. This is the gobble-proof unit: it guarantees N consumptions whatever
the frame/step cadence. It is a no-op at the common `--fixed-dt 16` cadence (one step per frame, so
tick == frame), which is why the existing `input` tests are unchanged; only `test_blowgun_release_
throttle` needed a larger `--tick` budget, because 6 fire ticks at `--fixed-timestep 100` span ~75
rendered frames.

Why sim ticks and not game-time. A held input's distance-per-tick scales with the timestep (a
step at `--fixed-timestep 100` advances 100 ms of sim, vs 16 ms at 16), so "30 ticks" walks farther
at a coarser throttle. That is the throttle's real coarseness, not a metering error, and it is not
the property a test asserts. The property that IS invariant, and the one the tool guarantees, is
that a sustained hold is consumed on every step: hold forward for the same GAME-TIME (pin it with
`--fixed-dt` and bound the run by `--tick`) and the hero floats the same distance at any throttle.
Measured on `V Jump.LBA` (a high-altitude glide), 30 s of forward hold floats 8612 units at
`--fixed-timestep 16` and 8515 at 100 (1.1% apart) where the old render-frame count collapsed to
zero. `test_input_hold_throttle.sh` is that regression.

Two behaviours to know: a hold needs a few warm-up ticks before movement registers (the walk anim
ramps up after load), and at a high `--fixed-timestep` a hold needs a correspondingly large
`--tick` budget for its ticks to elapse.

Constraint that keeps the harness honest: it must not force a sim step just because it injected.
It injects and lets the throttle do its worst; Layer A's latch is what will save a dropped edge.
If the harness force-stepped, a test would pass while real players still dropped edges.

### How they prove each other

Author a timeline that presses/releases on skip frames at `--fixed-dt 100`, paired with the hero
probe. The test fails today (edge dropped) and passes with the latch. It is the reusable
regression net for every future input-drop report, replacing the per-signal whitelist entry plus
bespoke corpus-save hunt.

## Reproducing the gobble, and simulating a high refresh rate

The throttle only drops input when rendered frames outpace sim steps, i.e. on a high-refresh
display with vsync off. The harness reproduces that deterministically: `--fixed-dt N` sets the
game-time each rendered frame advances (a stand-in for frame pacing), so a high refresh rate is a
SMALL `--fixed-dt`. The throttle skips whenever `--fixed-dt < --fixed-timestep`, and the skip ratio
(rendered frames per sim step) is `FixedTimestep / fixed-dt`. Model a real display with
`--fixed-dt = 1000/refreshHz` and the real `--fixed-timestep 16` (144 Hz -> `--fixed-dt 7`, ~2.3
frames/step; 240 Hz -> `--fixed-dt 4`); or exaggerate for a near-certain skip with
`--fixed-dt 8 --fixed-timestep 100` (~12.5 frames/step). Verified: at `--fixed-dt 8
--fixed-timestep 100`, 120 rendered frames run only 10 sim steps.

That the harness genuinely exercises the drop was proven by temporarily neutering the #407 edge
force-step (a stand-in for the broken engine): `input throw 6` then leaves the blowgun stuck firing
(`gen_anim` 35) at `--fixed-timestep 100` while `--fixed-timestep 0` settles to 0 -- the release
edge landed on a skipped frame and was gobbled. Restoring the force-step makes both settle to 0.

Level vs edge, and why `fseq` exists. Weapon fire *start* is LEVEL-triggered (`Input & I_THROW`),
so holding fire through a skip frame still fires at the next step -- a sim-tick hold cannot gobble
it. The genuine press-EDGE cases are melee (`I_ACTION_M` press edge, gated by an anti-repeat on
`LastInput`) and the fire *release* edge. A sim-tick hold always lands a press on a step, so it can
never reproduce a press-edge drop; that is what `fseq` is for -- place the press on a chosen skip
frame. The melee press-edge drop is captured by `tests/automation/test_melee_throttle.sh`: a
walking, aggressive hero (`input fseq 0:0x10000:6/0:up:120/<f>:up+action:6`) must fire a
GEN_ANIM_COUP (17/18/19, seen in the input trace) whether the action press lands on a stepped frame
(30) or a skipped one (31). At `--fixed-dt 8 --fixed-timestep 24` the current engine fires both
(COUP=2/2); neuter the `I_ACTION_EDGE` term and the skip-frame press drops to COUP=0. So the test
is a real guard, and it confirms #407's melee fix that was never auto-tested.

## Phased plan

- Phase 0, done: falsify first. The reproducible gap is the harness metering (table above); the
  engine's live post-gate exposure is narrow and whitelist-managed. This reprioritises Phase 2
  from "fix a rampant drop" to "retire the fragile whitelist and let injected input test the real
  path".
- Phase 1, DONE, branch `feat/input-sim-timeline` (harness-only): injection metered in sim ticks;
  `input <flags> [ticks]` / `input seq` / `input fseq` (frame-precise) / `input trace` / `input
  off` with `+`/`,` combos; per-tick hero-trace probe. No engine-behaviour change (no-op at
  `--fixed-dt 16`, so existing tests unchanged bar the blowgun `--tick` budget). Delivers the
  sustained-hold-and-combo requirement (`test_input_hold_throttle.sh`) and the frame-precise
  edge-placement (`fseq`) that Phase 2's red test needs.
- Phase 2, RESCOPED after measuring -- latch NOT built. The measure-first pass falsified the
  premise that there is a live edge to fix: the `#413` whitelist (`I_ACTION_EDGE`) turns out to be
  complete for every post-gate edge that can be found and tested. Verified on the current engine at
  `--fixed-timestep 24/100`: the melee press edge fires on a throttle-skipped frame (the force-step
  catches it), and the blowgun release edge stops firing; both drop only when the whitelist term is
  neutered. So the latch would be a pure refactor (retire the hand-maintained whitelist) carrying
  byte-exact risk for zero live bugs. Decision (match test weight to fix weight): skip it. Instead
  lock in `tests/automation/test_melee_throttle.sh` -- built with `fseq` + a GEN_ANIM_COUP trace, it
  confirms #407's melee fix, which until now was "covered by construction" and never auto-tested
  (it fails with the whitelist term neutered, passes with it). The latch stays a documented option
  the day a future post-gate edge consumer lands outside the whitelist.

## Risks and open questions

- Spell edges (`I_PINGOUIN`/`I_JETPACK`/`I_PROTECTION`/`I_FOUDRE`, the `Input2` word) are read via
  `CheckKey`/`SpellKeyDown`, not the `Input` bitfield, so they are outside both the whitelist and
  the latch. Confirm whether they need their own handling or are genuinely out of scope.
- Modal frames (menus, dialogue) sample input differently; a timeline that crosses a modal needs
  care, and `skipmodals` may be the right pairing.
- The latch delays an edge by up to one sim period (it is seen at the next step, not forced
  immediately). That is the fixed-timestep-correct behaviour and a no-op at `--fixed-dt 16`;
  confirm it does not regress feel on a real high-refresh device.
- The coarse sim at a high `--fixed-timestep` can diverge on physics for reasons unrelated to input
  delivery: at `--fixed-timestep 100` a 10 Hz step can tunnel collision. Measured on `Zeelich.LBA`,
  the same 30 s forward hold floats 5001 units at 16 but 22792 at 100 (the hero clips terrain a fine
  step would have stopped at). So a sustained-hold regression must use an open-air fixture, and this
  divergence is issue #412 territory (interpolation / smaller effective step), not an input bug.
- Record/replay (log latched sim-input per tick, replay at the same seam) and the render/sim
  interpolation that cures the stutter are the issue #412 follow-ons, deliberately out of scope.
