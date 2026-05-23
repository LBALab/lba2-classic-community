# Fixed-dt deterministic mode — research

Phase 1 research for a harness-only `--fixed-dt <ms>` flag that advances the simulation by
a constant time step per tick instead of by wall-clock, so `--dump-state` becomes fully
reproducible and the baseline harness's kinematic tier can be promoted from
reported-but-non-fatal to exact. This is findings only — no design decision, no
implementation. It builds on the CLI control harness (`SOURCES/CONTROL.{H,CPP}`,
`docs/CONTROL.md`) and on the determinism measurement recorded there.

Hard constraint carried through every section: the mode must be opt-in and must not change
default gameplay timing. It is only ever active when the harness drives it.

Line numbers were verified against the working tree at the time of writing. Treat them as
"look here," re-grep before relying on an exact number. Where I am unsure I say so rather
than guessing.

---

## 1. Timer mechanics

The game clock lives in `LIB386/SYSTEM/TIMER.CPP`. The relevant globals
(`TIMER.CPP:15-23`):

- `TimerSystemHR` — the raw `SDL_GetTicks()` reading from the last `ManageTime()` call.
- `TimerRefHR` — the master game clock in ms. This is what the whole simulation reads. It
  is persisted in saves and restored by `--load` (see `docs/CONTROL.md`), so it starts at
  the save's accumulated play-time, not zero.
- `TimerLock` — when non-zero, `ManageTime()` stops advancing `TimerRefHR` (the clock is
  frozen).
- `CmptMemoTimerRef` / `MemoTimerRefHR` — the save/restore nesting counter and the saved
  clock value.
- `LastTime` — the previous `TimerSystemHR`, used to compute the per-call wall-clock delta.

How the clock advances (`ManageTime()`, `TIMER.CPP:72-88`):

```c
TimerSystemHR = SDL_GetTicks();
if (!TimerLock)
    TimerRefHR += TimerSystemHR - LastTime;   // wall-clock delta since last call
LastTime = TimerSystemHR;
// ...FPS accounting...
```

So each `ManageTime()` call adds the real wall-clock milliseconds elapsed since the
previous call — unless `TimerLock > 0`, in which case the delta is dropped entirely (it is
not deferred or accumulated; the wall time spent locked simply vanishes from the game
clock). This is the variable-dt mechanism the determinism finding identified.

Helper roles (`TIMER.CPP:38-70`):

- `LockTimer()` — calls `ManageTime()` (banking the elapsed time up to now), then `++TimerLock`.
- `UnlockTimer()` — if `TimerLock > 0`, calls `ManageTime()` then `--TimerLock`. The
  `ManageTime()` while still locked refreshes `LastTime` to "now," so the locked interval
  does not leak into `TimerRefHR` when the lock drops to zero.
- `SetTimerHR(time)` — `LockTimer(); TimerRefHR = time; UnlockTimer();`. Sets the clock to
  an absolute value with the lock held so the surrounding `ManageTime()` calls don't perturb
  it. This is exactly the seam the console `timer` command uses (`SetTimerHR(TimerRefHR + ms)`).
- `SaveTimer()` — on the *first* nesting level (`!CmptMemoTimerRef++`), calls `ManageTime()`
  and stashes `MemoTimerRefHR = TimerRefHR`. Re-entrant via the counter.
- `RestoreTimer()` — on the *last* unwind (`!--CmptMemoTimerRef`), calls `ManageTime()` then
  `TimerRefHR = MemoTimerRefHR`. So a `SaveTimer()`/`RestoreTimer()` pair makes everything in
  between transparent to the game clock: real time passes but `TimerRefHR` is rewound to its
  pre-`SaveTimer` value. `DEBUG_TOOLS` builds even print a warning if the pairing is
  unbalanced (`PERSO.CPP:557-561`).

Per `MainLoop` iteration the clock advances at exactly one place in the steady state:
`ManageTime()` at `PERSO.CPP:578`, right after `MyGetInput()` at `:577`. That single call
adds the frame's wall-clock duration. Everything downstream in the same iteration
(movement, animation, timed script logic) reads the resulting `TimerRefHR`. The other
`ManageTime()` calls in the loop are inside `Lock`/`Save`/`Restore` helpers and busy-wait
spots (e.g. the `DEBUG_TOOLS` 20-loop benchmark at `PERSO.CPP:645-703`) and do not add a
second steady-state increment.

One subtlety: `Control_TickHook()` is at `PERSO.CPP:551`, which is *before*
`MyGetInput()`/`ManageTime()` in the loop body (`:577-578`) and *after* the `ChangeCube`
block (`:495-547`). So at the moment the hook runs on iteration N, `TimerRefHR` still holds
the value from iteration N-1's `ManageTime()` (or the loaded save's value on the first
iteration). This ordering matters for any "pin the clock from the hook" approach (§3).

---

## 2. dt consumers — the surface that must observe a constant dt

Everything that turns elapsed `TimerRefHR` into simulation state. If the clock advances by a
constant step each tick, all of these become reproducible.

**Movement / speed integration — `LIB386/3D/MOVE.CPP`.** The core is `GetDeltaMove(MOVE *)`
(`MOVE.CPP:64-78`): `delta = TimerRefHR - pmove->LastTimer`, accumulate `Acc += delta * Speed`,
advance `pmove->LastTimer = TimerRefHR`, and return integer steps via `GetDeltaAccMove`
(`:167-175`, divides accumulator by 1000). Each `MOVE` carries its own `LastTimer`
(`LIB386/H/3D/MOVE.H:13-15`), so the dt a mover sees is the gap between successive reads of
the clock, not an absolute. `ChangeSpeedMove` (`:12-25`) does the same banking when speed
changes. Consumers in the loop:

- `StepFalling = GetDeltaMove(&RealFalling)` and `StepShifting = GetDeltaMove(&RealShifting)`
  at `PERSO.CPP:1471-1473` — global per-frame fall/conveyor deltas applied to all objects.
- `pansample = GetDeltaMove(&SampleAlwaysMove)` at `PERSO.CPP:1476` — sample panning rate.
- `RealFalling`/`RealShifting`/`SampleAlwaysMove` are (re)initialised at `PERSO.CPP:459-464`
  with `InitMove`, which resets `Acc` and stamps `LastTimer = TimerRefHR`
  (`MOVE.H:44-48`). The grep surface for other movers is `GetDeltaMove` / `GetBoundMove` /
  `GetBoundAngleMove` callers across `SOURCES/` (script-driven actor motion in
  `GERETRAK.CPP`, door/lift bounds, etc.) — all route through the same `TimerRefHR - LastTimer`
  read, so pinning the clock pins them uniformly.

**Animation interpolation — `LIB386/ANIM/FRAME.CPP`, `INTERDEP.CPP`.** `SetInterAnim`
(`FRAME.CPP:42-43`) stamps `obj->LastTimer = obj->Time = TimerRefHR` at anim start.
`ObjectSetInterDep` (`INTERDEP.CPP:14-153`) is the per-frame interpolator: it compares
`TimerRefHR` to `obj->Time`/`obj->NextTimer`, computes an `interpolator` fraction
(`:51-53`) from `(Time - LastTimer)` over `(NextTimer - LastTimer)`, and advances the frame
when `Time >= NextTimer`. The interpolation fraction — and therefore the per-tick pose,
which feeds `hero.last_frame`/`anim` and the actors' positions — is a direct function of how
much `TimerRefHR` advanced this tick. This is precisely the kinematic tier
(`x/y/z/beta/anim/last_frame`) the baseline harness currently treats as soft.

**Timed script / object logic — `SOURCES/OBJECT.CPP` and others.** Many sites read
`TimerRefHR` as an absolute timestamp for timeouts and animation phases, e.g. rotating
platforms (`OBJECT.CPP:4598-4626`, `angle = ((TimerRefHR - Info2) * 320) / SRot + Info1`),
extras lifetimes (`:5564-5613`), cinema cycles (`:4684-4722`), proto timers
(`:4120-4158`), flash effects (`:5476`, `(TimerRefHR & 0xFF)`). These are deterministic
under a pinned clock because `TimerRefHR` itself becomes a deterministic sequence — they read
the same global. (`TimerProto`, `TimerCinema`, `PersoFlashTimer`, the per-object `Info`/
`Info2` stamps are all derived from `TimerRefHR`.)

**FPS counter — `TIMER.CPP:82-87`.** `NbFramePerSecond` is wall-clock derived and will stay
nondeterministic; the dump already drops it (`baseline_harness.py:34`). Not a simulation
consumer.

Net: the entire moving-actor surface flows from one global, `TimerRefHR`, read at three
kinds of sites (per-mover `GetDeltaMove`, per-anim `ObjectSetInterDep`, and direct absolute
reads). Make `TimerRefHR` advance by a constant per tick and all three become reproducible.
I am confident about the movement and animation paths; I have *not* exhaustively audited
every absolute-`TimerRefHR` read in `GERELIFE.CPP`/`GERETRAK.CPP` for branch-on-time logic,
so a residual "is there any time read that isn't a pure function of `TimerRefHR`" check
belongs in Phase 2 validation (§6).

---

## 3. Injection point — two candidate approaches

The goal is: on a fixed-dt tick, `TimerRefHR` advances by a constant `DT` rather than by the
wall-clock delta. Two ways to achieve that.

### (a) Teach `ManageTime()` to add a fixed increment under a mode flag

Add a flag (set only by the harness) that makes the `!TimerLock` branch of `ManageTime()`
(`TIMER.CPP:75-77`) add a constant `DT` instead of `TimerSystemHR - LastTime`.

- **Pro:** there is exactly one steady-state increment site (§1), so one guarded branch
  covers the whole simulation. Every `GetDeltaMove`/`ObjectSetInterDep`/absolute read sees a
  clean constant step with no special-casing. It naturally composes with `Lock`/`Save`/
  `Restore` — those still freeze/rewind around their regions; the only change is the *size*
  of the increment when the clock is allowed to move.
- **Con:** it edits a `LIB386` file (`TIMER.CPP`) that is otherwise pure C-linkage engine
  code, to add behaviour driven by a `SOURCES`-level harness. The flag would need to live
  somewhere both can reach (a `TIMER.CPP` global with a setter, set from `CONTROL.CPP`),
  which is a small but real coupling. It also still calls `SDL_GetTicks()` and updates
  `LastTime`/the FPS counter, so FPS stays wall-clock (fine — it's dropped).
- **Subtlety:** `ManageTime()` is called many times per loop (inside every `Lock`/`Save`/
  `Restore`), but only the ones with `TimerLock == 0` advance the clock. A naive "add DT on
  every `ManageTime`" would over-count badly. The increment must be gated so it fires once
  per *tick*, not once per `ManageTime`. The cleanest framing is: the harness arms a
  "pending DT" before the tick's main `ManageTime` and the function consumes it once. This
  needs care precisely because of the many `Save`/`Restore` `ManageTime` calls.

### (b) `LockTimer()` for the tick, then one `SetTimerHR(TimerRefHR + DT)` from the hook

Drive it entirely from `Control_TickHook()` (`CONTROL.CPP:359`): hold the clock locked for
the whole tick so wall-clock never leaks in, and bump it by `DT` exactly once per tick.

- **Pro:** zero `LIB386` changes — it reuses the existing public `SetTimerHR`/`LockTimer`
  seams (the same ones the console `timer` command and `docs/CONTROL.md` already cite). The
  whole feature stays inside the harness TU, matching the harness's stated design ("reuses
  existing engine seams rather than changing game logic," `CONTROL.H:11-13`). Strongly
  preferred on the surgical-changes / opt-in principles.
- **Con — this is the load-bearing risk:** the loop is *saturated* with `SaveTimer`/
  `RestoreTimer` and `LockTimer`/`UnlockTimer` pairs (dozens of sites; grep `PERSO.CPP` for
  `SaveTimer`/`RestoreTimer` returns ~50 hits across menus, the load path, benchmarks, and
  notably the FollowCam compensation). If the harness leaves `TimerLock` raised across the
  tick, it nests with all of those. `SaveTimer`/`RestoreTimer` are counter-based and
  re-entrant so they tolerate nesting, but the *semantics* interact: while the harness lock
  is held, every internal `ManageTime()` drops its delta, so the internal `Save`/`Restore`
  pairs become no-ops on the clock value (they rewind to a value that never moved) — which is
  arguably what we want, but needs verifying tick by tick. The cleaner variant is *not* to
  hold a lock across the tick at all, but to let the normal `ManageTime()` run and then, once
  per tick from the hook, overwrite the clock to a deterministic value: `SetTimerHR(base + n*DT)`
  where `base` is the loaded clock and `n` the tick index. That makes the per-tick clock a
  pure function of the tick count regardless of how much real time `ManageTime` added, and
  `SetTimerHR` already brackets itself in `LockTimer`/`UnlockTimer`.
- **Subtlety — hook timing (§1):** the hook runs *before* this tick's `ManageTime()`
  (`:551` vs `:578`). So a "set the clock from the hook" approach sets the value that the
  *upcoming* tick will read after `MyGetInput`. If instead we want the increment applied
  after `ManageTime` has run, the hook is the wrong seam and a post-`ManageTime` call site
  (or approach (a)) is needed. This ordering is the single most important thing to pin down
  in Phase 2; I am not certain which side of `ManageTime` the pin must sit on to get the
  movers to see exactly `DT`, because `GetDeltaMove` reads `TimerRefHR` at `:1471` (after
  `ManageTime`) while `ObjectSetInterDep` reads it later still. A `SetTimerHR` from the hook
  (pre-`ManageTime`) followed by the normal `ManageTime` would add the *real* delta on top of
  the pinned value, defeating the purpose — so a from-the-hook overwrite likely has to also
  neutralise the subsequent `ManageTime` (e.g. by holding the lock through `:578`, i.e.
  variant (b)-with-lock). This tension is the reason approach (a) is structurally cleaner
  even though it touches `LIB386`.

### FollowCam timer compensation interaction (both approaches)

`PERSO.CPP:1877-1889` saves the timer before `AffScene` and, after it, does
`RestoreTimer(); TimerRefHR += TimerSystemHR - FollowCamTimerBefore;` — it deliberately adds
back the *real* wall-clock render+vsync duration so the follow-camera frames don't stall the
game clock. Under fixed-dt this hand-rolled re-add reintroduces a wall-clock term
(`TimerSystemHR - FollowCamTimerBefore`) *after* our pin, so it would corrupt determinism on
exterior follow-cam frames. Same shape, smaller, at `PERSO.CPP:645-703` (the `DEBUG_TOOLS`
benchmark) and any other site that manually adds a `TimerSystemHR` delta. A fixed-dt mode has
to either (i) pin the clock *after* this block, or (ii) make this re-add a no-op when the
mode is active. This is a concrete correctness hazard, not a theoretical one — flag it
prominently for Phase 2. Note `WaitVideoSync()` is a no-op (`LIB386/SVGA/SDL.CPP:113-114`)
and vsync is delegated to `SDL_SetRenderVSync` (`WINDOW.CPP:159`), so the render stall the
FollowCam block compensates for is real present latency, not an explicit busy-wait.

**Tentative lean (not a decision):** approach (a) is structurally cleaner — one increment
site, no nesting against the `Save`/`Restore` thicket, naturally once-per-tick if gated
right — at the cost of a small, well-contained `LIB386` edit behind a harness-set flag.
Approach (b) keeps everything in the harness TU but has to reason about the
`Lock`/`Save`/`Restore` nesting and the hook-vs-`ManageTime` ordering, and still needs the
FollowCam re-add neutralised. Either way the FollowCam re-add must be handled. The decision
belongs in Phase 2.

---

## 4. RNG

There is exactly one RNG seed in the game: `srand(TimerRefHR)` in `ChangeCube()`
(`OBJECT.CPP:1171`) — confirmed by grepping `srand` across `SOURCES/` and `LIB386/`
(only that one hit). `Rnd(n)` is `rand() % n` and `MyRnd` wraps it (per the prior automation
research; not re-verified here).

Does pinning the clock *before* `ChangeCube` already determinise the seed? **Partly, and the
detail matters.** `ChangeCube()` runs inside the `if (NewCube != -1)` block at
`PERSO.CPP:540`, which is *above* `Control_TickHook()` at `:551`. So on the load path
(`Control_Begin` calls `ChangeCube()` directly at `CONTROL.CPP:163`, and the first loop
iteration may call it again), the seed is taken from whatever `TimerRefHR` holds at that
moment. On `--load`, `TimerRefHR` is the *restored save value* (a fixed number for a given
save), so `srand(TimerRefHR)` is in fact already deterministic across runs for a loaded
save — which is consistent with the measured finding that "the `srand(TimerRefHR)` reseed
produced no observable divergence." The seed isn't the drift source; the per-tick dt is.

So the spec's measured conclusion holds up under inspection: for the `--load X --tick N`
workflow, the seed is already pinned by the restored clock, and no explicit reseed is needed
for the *currently observed* determinism. **Caveat I cannot rule out without testing:** if a
fixed-dt run ever triggers a *mid-run* `ChangeCube` (a scene transition during the ticks,
which the harness over-counts per `docs/CONTROL.md`), the seed at that transition is
`TimerRefHR` at that tick — which is deterministic only if the clock was already pinned by
then. With a working fixed-dt that is true. So: an explicit fixed reseed is likely
*unnecessary* given a working clock pin, but a fixed-dt mode that also wants to be robust to
mid-run cube changes should confirm the clock is pinned before the `:540` `ChangeCube` (it
is, if the pin is applied each tick before that block — note the hook is *after* it, so a
hook-only pin does not cover the first `ChangeCube`; `Control_Begin`'s direct `ChangeCube`
already runs with the restored clock, which is fine). I would verify this empirically rather
than assert it.

---

## 5. Hazards — what still varies under a pinned clock

- **The FollowCam wall-clock re-add (§3).** `PERSO.CPP:1887` adds real elapsed ms back into
  `TimerRefHR` on exterior follow-cam frames. This is the highest-priority hazard: it
  silently reintroduces nondeterminism after any clock pin. Must be neutralised in fixed-dt
  mode.
- **Other `SDL_GetTicks()` readers.** Grep across `LIB386/`+`SOURCES/` (excluding ASM):
  `TIMER.CPP:29,73` (the clock itself), `MOUSE.CPP:41,74` (pointer-idle hide timer — cosmetic,
  not simulation), `AIL/SDL/SAMPLE.CPP:432,461` (audio voice timing — audio thread, see
  below), `PLAYACF.CPP:509-539` and `RES_PICKER.CPP:191` (video/resource-picker pacing —
  modal paths the harness already avoids per `docs/CONTROL.md`). None of these feed the
  world-state dump under the null audio backend, but they are the exhaustive set of raw
  wall-clock reads to be aware of.
- **Audio thread.** The SDL audio backend services voices on a callback thread
  (`AIL/SDL/SAMPLE.CPP`, `LockVoices`/`SDL_LockAudioStream` around `:248-510`), and some
  script logic gates on `IsSamplePlaying(...)` (`PERSO.CPP:1566`). With real audio, sample
  completion timing is wall-clock and threaded, so it can branch script logic
  nondeterministically. The baseline harness already runs with `SDL_AUDIODRIVER=dummy`
  (`baseline_harness.py:56`) / null backend, which sidesteps both. Fixed-dt does *not* fix
  audio-timed branches; it relies on the null backend, same as today. Worth stating
  explicitly so nobody expects fixed-dt alone to determinise a real-audio run.
- **`FirstLoop` asymmetry.** The first post-load tick differs from later ticks
  (`PERSO.CPP:543`, gating at `:1549`-ish per the automation research). Not nondeterministic,
  but means tick 1 ≠ tick 2 for the same state — the dump must keep recording the tick count
  (it does: `CONTROL.CPP:257-261`).
- **Platform precision (long double).** Projection/rotation use `long double` + `lrintl()`,
  which is 80-bit on Linux x86_64 but 64-bit on Windows/macOS-ARM (see `CLAUDE.md`,
  `docs/PLATFORM.md`). Fixed-dt removes *temporal* nondeterminism but **does not** remove
  *cross-platform* coordinate differences: two platforms running the identical fixed-dt
  sequence can still land on slightly different `x/y/z` because the math rounds differently.
  Implication for §6: a fixed-dt golden is reproducible *on the same platform/build*; the
  baseline harness should keep goldens per-platform or keep a position tolerance for
  cross-platform comparisons even after the kinematic tier goes exact same-platform. This is
  the one place I'd push back on "promote kinematics to fully exact" — exact within a
  platform, yes; exact across platforms, not guaranteed by fixed-dt alone.
- **Integer vs sub-step truncation.** `GetDeltaAccMove` divides the accumulator by 1000 and
  keeps the remainder in `Acc` (`MOVE.CPP:167-175`). A constant `DT` makes the accumulator
  sequence deterministic, but the *choice* of `DT` changes the truncation pattern, so two
  different `--fixed-dt` values produce different (each internally reproducible) trajectories.
  The harness should pick and document one canonical `DT` for goldens (a value matching the
  game's nominal frame time — verify what the engine targets; I did not find a hardcoded
  target-frame-ms constant in this pass).

---

## 6. Validation plan

How to *prove* fixed-dt works, using tooling that already exists:

1. **Reproducibility across runs (the core claim).** Run a fixed-dt `--load X --tick N
   --dump-state` 3× on the same build/platform with the null audio backend and diff the
   dumps. Success = byte-identical including the kinematic fields (`x/y/z/beta/anim/
   last_frame`). Today those fields drift ~0.1% on busy exterior scenes (`docs/CONTROL.md`
   determinism section); fixed-dt should drop that to zero. `fps`/`timer_ref_hr` are expected
   to differ run-to-run only if the clock pin is imperfect — under a correct pin
   `timer_ref_hr` becomes a deterministic function of the tick count, so it can move from the
   `VOLATILE` set to checked.
2. **Baseline harness kinematic tier → zero soft notes.** Re-run
   `tests/savegame/corpus/baseline_harness.py` with the binary built/run in fixed-dt mode.
   Expected outcome: the `(+N kinematic)` annotations (`baseline_harness.py:156,163`) drop to
   zero across the corpus. Concretely that means `SOFT_POS`/`SOFT_OTHER`
   (`baseline_harness.py:39-40`) no longer fire.
3. **Promote kinematics to structural.** Once (2) holds, move `x/y/z/beta` and
   `anim/last_frame` out of the soft tiers so a mismatch becomes a hard failure — i.e. delete
   the `SOFT_POS`/`SOFT_OTHER` special-cases in `diff()` (`baseline_harness.py:105-110`) and
   regenerate goldens with `--update`. Caveat from §5: this exactness is *per platform/build*;
   if goldens are shared across platforms, keep a tolerance or store per-platform goldens,
   because long-double precision still differs.
4. **Regression guard for the constraint.** Prove the mode is opt-in: a default (non-harness)
   run, and a harness run *without* `--fixed-dt`, must behave exactly as today (variable dt).
   The cleanest check is that `ManageTime()`/the loop is byte-for-byte unchanged in behaviour
   when the flag is off — e.g. a host test or an A/B dump showing no difference vs the current
   binary on a non-fixed-dt run.
5. **Sanity that simulation still advances.** Keep the spirit of `tests/automation/test_tick.sh`
   (1 vs 60 ticks must advance the clock and the hero's idle-anim frame): under fixed-dt the
   clock advances by exactly `N*DT` and the anim frame advances deterministically, which is a
   stronger assertion than the current "advanced at all."

---

## 7. Post-merge finding: modal and fade loops need their own clock advance

§6's validation covered the merged fixed-dt mechanism on settled-state baselines. It did
not surface a class of loops that wait on `TimerRefHR` from inside a single main-loop tick:
under fixed-dt the virtual clock only steps in `Timer_FixedDtAdvance()` at the tick hook
(`PERSO.CPP:551`), so any loop that runs *between* hook calls and spins on a clock deadline
freezes. The baseline corpus dodged this because settled saves don't enter such loops in
their 8-tick window; the `--demo` attract path hits all three loop shapes.

**Three loop shapes, three fixes** (`TIMER.CPP`).

- **Modal render loops** — `DoFoundObj` (`INVENT.CPP:1541`), `Dial` SpeakAnimation loops
  (`MESSAGE.CPP:1967, 2005`), `MenuInventory`, anything that presents a frame each iteration
  via `BoxUpdate`→`BoxBlit`. The first present after each tick advance is the main loop's
  own render (already paid for by the hook); every *additional* present in the same tick is
  a modal iteration. `Timer_FixedDtPresent()` (called from `BoxBlit` in `DIRTYBOX.CPP:395`)
  steps the clock with a skip-first guard so a non-modal tick stays exactly `+dt` and modal
  loops advance per iteration.
- **Music volume fades** — `FadeOutVolumeMusic`/`FadeInVolumeMusic` (`MUSIC.CPP`). A
  `do { ManageTime(); ... } while(volume != target)` spin with no present.
- **Palette fades** — `FadeToBlack`/`WhiteFade`/`FadeWhiteToPal`/`FadeToPal`/`FadePalToPal`
  (`AMBIANCE.CPP`, 5 sites). Same `do { ManageTime(); delta = TimerSystemHR - start; ... }`
  shape, no present.

Both fade classes wrap in `SaveTimer`/`RestoreTimer`, so the pumped time is discarded
when the loop returns — they don't perturb the game clock, they just need the loop to
*exit*. `Timer_FixedDtPump()` (no skip-first; one direct step per call) sits next to the
inner `ManageTime()` in each.

**Why baselines stay byte-identical.** Non-modal ticks call `BoxBlit` exactly once (the
main render), which consumes the skip; the fades aren't entered during settled-state
corpus saves. Re-verified `39 ok, 0 drift` after each fix.

**Demo determinism map** (cube N played under `--demo --fixed-dt 16` for 800 ticks,
gameplay-state compared including `timer_ref_hr`):

All 14 reel-position cubes surveyed (193, 194, 195, 196, 200, 207, 208, 210, 213, 214, 218,
219, 220, 221) are byte-deterministic. No HANG remains and no run-to-run divergence
remains. An earlier revision of this section listed seven cubes as a "plays-through but
moving-NPC FP caveat" class; that was a misdiagnosis — the divergence was an RNG seed
leak, not floating-point. `srand(TimerRefHR)` at `ChangeCube` (`OBJECT.CPP:1171`) was
reading whatever wall-clock interval had elapsed between `InitTimer()` at engine boot and
the harness arming fixed-dt, and that residual leaked into the seed only on the fresh-
start path (`--demo` without `--load`; the `--load` path overrides `TimerRefHR` from the
saved game's clock after enable). Closed in `Timer_EnableFixedDt()` by resetting
`TimerRefHR` alongside `FixedDtNow`/`LastTime`. The §5 platform-precision hazard
(`long double` 80- vs 64-bit) is a separate cross-platform concern; same-platform
exactness is now real for moving actors too.

---

## Summary

- The whole moving-actor surface is driven by one global, `TimerRefHR`, advanced once per
  steady-state tick in `ManageTime()` (`TIMER.CPP:72-88`, called at `PERSO.CPP:578`). dt
  consumers are `GetDeltaMove` (`MOVE.CPP:64-78`; movement/fall/shift at
  `PERSO.CPP:1471-1476`), the animation interpolator `ObjectSetInterDep`
  (`INTERDEP.CPP:14-153`), and many absolute-`TimerRefHR` reads in `OBJECT.CPP`. Pin the
  clock and they all reproduce.
- Two injection approaches: (a) a guarded fixed increment inside `ManageTime()` — structurally
  cleaner, one site, small `LIB386` edit; (b) drive `SetTimerHR`/`LockTimer` from
  `Control_TickHook` — no `LIB386` change but must reason about the dense `Save`/`Restore`
  nesting and the hook-runs-before-`ManageTime` ordering (`:551` vs `:578`). No decision made.
- RNG is already effectively pinned for the `--load --tick` workflow: the only seed is
  `srand(TimerRefHR)` at `OBJECT.CPP:1171`, and on `--load` `TimerRefHR` is the restored save
  value, so the seed is already deterministic — matching the measured "reseed produced no
  divergence." No explicit reseed appears necessary given a working clock pin; verify rather
  than assume for mid-run cube changes.
- Main open questions / hazards: (1) the FollowCam wall-clock re-add at `PERSO.CPP:1887`
  reintroduces real time after any pin and must be neutralised; (2) which side of
  `ManageTime` the pin must sit on for movers to see exactly `DT` (hook ordering); (3)
  cross-platform long-double precision means fixed-dt gives same-platform exactness, not
  cross-platform — the baseline promotion in §6 should account for that; (4) the canonical
  `DT` value to standardise goldens on (no hardcoded target-frame-ms constant found this
  pass); (5) a residual audit of branch-on-`TimerRefHR` script logic in
  `GERELIFE.CPP`/`GERETRAK.CPP`.
