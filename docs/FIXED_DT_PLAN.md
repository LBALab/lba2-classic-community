# Fixed-dt deterministic mode — plan

Phase 2 design for a harness-only `--fixed-dt <ms>` flag that advances the simulation by a
constant time step per tick instead of by wall-clock, making `--dump-state` fully
reproducible and unblocking faithful input record/replay. Builds on
[FIXED_DT_RESEARCH.md](FIXED_DT_RESEARCH.md) (Phase 1) and the existing CLI control harness
([CONTROL.md](CONTROL.md), [../SOURCES/CONTROL.CPP](../SOURCES/CONTROL.CPP)).

Hard constraint carried throughout: the mode is opt-in and must not change default-build or
normal gameplay timing. It is only ever active when the harness drives it. Line numbers were
re-verified against the working tree; treat them as "look here" and re-grep before relying on
an exact number.

---

## 1. Decision: virtual time source in `ManageTime()`, advanced once per tick from the hook

Research §3 framed two candidates: (a) a guarded fixed increment inside `ManageTime()`, and
(b) a hook-driven `LockTimer` + `SetTimerHR(TimerRefHR + DT)`. Both have the problems the
research flagged — (a) must gate the increment so it fires once per *tick* and not once per
*`ManageTime` call* (the loop calls `ManageTime` many times via `Lock`/`Save`/`Restore`), and
(b) must reason about the hook running *before* this tick's `ManageTime` (`:551` vs `:578`)
and still neutralise the FollowCam re-add.

The chosen design is a refinement of (a) that dissolves both problems: **replace the time
*source* `ManageTime()` reads, rather than special-casing the increment.**

In fixed-dt mode, `ManageTime()` reads a virtual clock `FixedDtNow` instead of
`SDL_GetTicks()`. The harness advances `FixedDtNow` by exactly `DT` once per tick from
`Control_TickHook()`. Everything else in `TIMER.CPP` — `LockTimer`/`UnlockTimer`,
`SaveTimer`/`RestoreTimer`, `SetTimerHR`, the `!TimerLock` accumulate branch — runs
unchanged, because they all derive from the same "now."

Why this dominates raw (a) and (b):

- **Over-counting solves itself.** `FixedDtNow` only changes at the hook, so within a single
  tick every `ManageTime()` call (including the dozens inside `Save`/`Restore`/`Lock` pairs)
  sees the *same* value. Each such call computes `delta = FixedDtNow - LastTime`, which is `0`
  after the first — so the internal calls contribute nothing to `TimerRefHR`. The single `DT`
  step advanced at the hook is banked exactly once, by the first unlocked `ManageTime()` after
  the hook (the steady-state call at [`PERSO.CPP:578`](../SOURCES/PERSO.CPP)). No per-call
  gating, no "which `ManageTime` is the real one" bookkeeping.
- **The hook-ordering question disappears.** Advancing `FixedDtNow` at the hook (`:551`,
  *before* `ManageTime()` at `:578`) is exactly right: the hook arms the step and the very next
  unlocked `ManageTime()` banks it into `TimerRefHR`. The movers at
  [`PERSO.CPP:1471-1476`](../SOURCES/PERSO.CPP) (`GetDeltaMove`) and the animation interpolator
  `ObjectSetInterDep` both read `TimerRefHR` *after* `:578`, so they observe exactly `+DT`
  versus the previous tick. (Confirmed: `MyGetInput()` at `:577` does not call `ManageTime` —
  grep of `SOURCES/INPUT.CPP`/`CONFIG/INPUT.CPP` shows no timer touch — so `:578` is the first
  and only steady-state bank site.)
- **The FollowCam re-add neutralises itself** — see §2.

Cost: a small, well-contained edit to one `LIB386` file (`TIMER.CPP`), behind a flag that is
only set by `SOURCES/CONTROL.CPP`. The flag state lives in `TIMER.CPP` with C-linkage setters,
matching how `SetTimerHR`/`LockTimer` are already reused by the harness and the console `timer`
command. When the flag is off, `ManageTime()` takes the identical `SDL_GetTicks()` path it does
today — byte-for-byte unchanged behaviour for default builds and non-fixed-dt harness runs.

Sketch of the `ManageTime()` change (final form decided in implementation):

```c
void ManageTime() {
    TimerSystemHR = FixedDtActive ? FixedDtNow : SDL_GetTicks();
    if (!TimerLock)
        TimerRefHR += TimerSystemHR - LastTime;
    LastTime = TimerSystemHR;
    // ...FPS accounting unchanged (wall-clock-ish; dropped from the dump)...
}
```

---

> **Implementation correction (post-build).** The "neutralised for free" reasoning below was
> wrong in practice: an exterior follow-cam save still leaked wall-clock under the virtual
> clock, so the re-add was explicitly guarded off in fixed-dt mode (research option (ii)) via a
> `Timer_FixedDtActive()` accessor — a one-line `PERSO.CPP` edit, opt-in. Two further leaks
> surfaced and were fixed: the first-tick step (force `LastTime = FixedDtNow` in
> `Timer_EnableFixedDt`) and boot-residual contamination of the base clock (enable fixed-dt at
> the *top* of `Control_Begin`, before the load, so boot `ManageTime` calls freeze). With all
> three closed, a busy exterior save is byte-identical across 10 runs **on the null sound
> backend**; the SDL backend retains a residual audio-thread wobble (see CONTROL.md). The
> theory below is kept for the record.

## 2. FollowCam timer re-add — neutralised for free

The research's top hazard ([`PERSO.CPP:1877-1889`](../SOURCES/PERSO.CPP)):

```c
U32 FollowCamTimerBefore = 0;
if (FollowCamTerrainFrame) { SaveTimer(); FollowCamTimerBefore = TimerSystemHR; }
AffScene(FirstTime);
if (FollowCamTerrainFrame) {
    RestoreTimer();
    TimerRefHR += TimerSystemHR - FollowCamTimerBefore;   // re-adds real render+vsync ms
    FollowCamTerrainFrame = FALSE;
}
```

Under the virtual clock this requires **no edit**. `FollowCamTimerBefore` is captured from
`TimerSystemHR` at `:1880`; the re-add at `:1887` uses `TimerSystemHR` as refreshed by
`RestoreTimer()`'s internal `ManageTime()` at `:1886`. Both reads happen *within the same
tick*, during which `FixedDtNow` does not change (it only advances at the hook, `:551`).
Therefore `TimerSystemHR` at `:1886` equals `FixedDtTimerBefore` captured at `:1880`, and the
re-add evaluates to `TimerRefHR += 0`. The wall-clock term it injects in normal mode is
identically zero in fixed-dt mode.

This is the decisive advantage of replacing the time *source* rather than gating the
increment: the same property that freezes the clock within a tick makes every "add back a
`TimerSystemHR` delta" site (the FollowCam block, and the `DEBUG_TOOLS` benchmark at
`:645-703`, which is off the harness path anyway) collapse to a no-op automatically. We will
still assert this empirically on an exterior follow-cam save in validation (§ Validation),
since it is the research's flagged correctness hazard.

---

## 3. DT semantics, canonical value, Save/Restore interaction

- **Units.** `--fixed-dt <ms>` takes an integer count of milliseconds — the same unit as
  `TimerRefHR` and the `delta` in `GetDeltaMove` ([`MOVE.CPP:64-78`](../LIB386/3D/MOVE.CPP)).
  One tick advances `TimerRefHR` by exactly that many ms.
- **Canonical value: 16 ms — and no engine default.** No hardcoded target-frame-ms constant
  exists in the engine (re-confirmed by grep of `TIMER.CPP`/`PERSO.CPP`), so the canonical value
  is a fixed *choice*, not a recovered constant. 16 ms (≈ the 60 fps the dump reports, and SDL's
  vsync cadence) is what the baseline harness passes and what goldens are generated against. The
  precise number is arbitrary so long as it is fixed: §5 of the research notes `GetDeltaAccMove`
  truncates the accumulator by 1000, so a *different* `DT` yields a different (each internally
  reproducible) trajectory.
- **Where the 16 lives — exactly one named place, never the engine.** `Timer_EnableFixedDt`
  takes `dt_ms` as a parameter, so the engine carries no default and no policy. `--fixed-dt`
  *requires* an explicit value (like `--tick`), so there is no bare literal in `CONTROL.CPP`
  either. The canonical 16 appears once, as a named constant in the baseline harness
  (`CANONICAL_DT_MS = 16`, with a rationale comment), and is documented once in `CONTROL.md` as
  the source of truth — so nobody regenerates goldens with a different step, and there is no
  magic number scattered across C, the flag, and the test.
- **Interaction with `SaveTimer`/`RestoreTimer`.** A `Save`/`Restore` pair rewinds `TimerRefHR`
  to its pre-`Save` value, making the bracketed region transparent to the game clock. Under the
  virtual clock that value did not move during the region (no hook fired inside it), so the pair
  is transparent in exactly the way it is designed to be — it rewinds to a value that never
  changed. The harness advances `FixedDtNow` at the hook (`:551`), which is outside every
  `Save`/`Restore`/`Lock` region in the loop body (the FollowCam `Save`/`Restore` is at
  `:1879-1886`, well after the hook), so the `DT` step is never swallowed by a rewind.
- **Lock edge case.** If `TimerLock > 0` at the moment the hook advances `FixedDtNow` (e.g. a
  focus-lost `LockTimer` via `HandleEventsTimer`), the step is not lost — it is banked by the
  next unlocked `ManageTime()`, identical to how wall-clock time is handled today. A `--exit`
  harness run is normally focused, so this is a robustness note, not an expected path.

---

## 4. RNG — clock pin is sufficient, verify don't reseed

The only seed in the game is `srand(TimerRefHR)` in `ChangeCube()`
([`OBJECT.CPP:1171`](../SOURCES/OBJECT.CPP)) — confirmed single hit in research §4. For the
`--load X --tick N` workflow, `TimerRefHR` on the load path is the restored save value (a fixed
number for a given save), so the seed is already deterministic across runs — consistent with the
measured "the `srand(TimerRefHR)` reseed produced no observable divergence" (CONTROL.md
determinism section).

`ChangeCube()` at [`PERSO.CPP:540`](../SOURCES/PERSO.CPP) runs *before* the hook (`:551`). For a
mid-run cube change on tick N, the seed is `TimerRefHR` as banked by tick N-1, i.e.
`base + (N-1)*DT` — deterministic under a working clock pin. **Decision: no explicit reseed.**
The clock pin determinises the seed for both the load path and mid-run transitions. This is a
claim to *verify* in validation (a 3× repeat that crosses a cube boundary), not to assert — if a
counterexample surfaces, the fallback is a fixed reseed in `Control_Begin`, but the research and
the existing measurement both indicate it is unnecessary.

> **As-built (PR #175):** the counterexample did surface — on the fresh-start path (`--demo`
> without `--load`), `TimerRefHR` at engine boot already carries the wall-clock interval
> banked by `InitTimer()`'s first `ManageTime()` since `SDL_GetTicks()`'s epoch. That residual
> reached `srand(TimerRefHR)` and varied the seed run-to-run by however many ms elapsed
> before the harness armed fixed-dt (measured 125-470 ms). The fix is functionally the
> fallback this section anticipated: `Timer_EnableFixedDt()` now resets `TimerRefHR = 0`
> alongside its `FixedDtNow`/`LastTime` seeding, so the seed is canonical on the fresh-start
> path. The `--load` path is unaffected (the save's clock overrides `TimerRefHR` after the
> reset). See PR #175 commit and `docs/FIXED_DT_RESEARCH.md` §7. The misdiagnosis trail is
> recorded in `docs/INPUT_REPLAY_RESEARCH.md` §2d.

---

## 5. Files, signatures, pin sites

**`LIB386/H/SYSTEM/TIMER.H`** — declare the two C-linkage entry points (in the existing
`extern "C"` block, near `SetTimerHR`):

```c
/* Harness-only deterministic clock. When enabled, ManageTime() advances from an
   internal virtual clock stepped by Timer_FixedDtAdvance() instead of SDL_GetTicks(),
   making the per-tick dt constant. Off by default; default builds are unaffected. */
void Timer_EnableFixedDt(U32 dt_ms);   /* arm fixed-dt; seeds the virtual clock to now */
void Timer_FixedDtAdvance(void);       /* advance the virtual clock by dt_ms (once per tick) */
```

**`LIB386/SYSTEM/TIMER.CPP`** — add file-scope state and the two functions, modify
`ManageTime()`:

- New state (alongside the existing private state, `:20-25`):
  `static S32 FixedDtActive = 0; static U32 FixedDt = 0; static U32 FixedDtNow = 0;`
- `Timer_EnableFixedDt(U32 dt_ms)`: `FixedDt = dt_ms; FixedDtNow = TimerSystemHR;
  FixedDtActive = 1;` — seeding `FixedDtNow` to the current `TimerSystemHR` keeps the first
  `delta` (`FixedDtNow - LastTime`) at ~0 and keeps the FPS accumulator sane across the switch.
- `Timer_FixedDtAdvance()`: `if (FixedDtActive) FixedDtNow += FixedDt;`
- `ManageTime()`: change only the source line to
  `TimerSystemHR = FixedDtActive ? FixedDtNow : SDL_GetTicks();` (`:73`). Everything else
  unchanged. No other `LIB386` file is touched.

**`SOURCES/CONTROL.CPP`** — flag parse + wiring (no new engine seam; reuses the existing three
call sites):

- Parse `--fixed-dt <ms>` in `Control_ParseArgs` (`:57-122`), alongside `--tick`: store into
  `static int s_have_fixed_dt; static long s_fixed_dt;` with the same `strtol` validation and
  `s_active = 1`. The value is **required** (mirroring `--tick`); an absent/invalid value is a
  parse error, not a silent default — so no canonical literal is baked into `CONTROL.CPP`. The
  canonical 16 lives only in the baseline harness (§3, §6).
- In `Control_Begin` (`:150-180`), after the load/fresh-start branch and before arming:
  `if (s_have_fixed_dt) Timer_EnableFixedDt((U32)s_fixed_dt);`
- In `Control_TickHook` (`:359-375`), advance once per tick *before* the finalize check:
  `if (s_have_fixed_dt) Timer_FixedDtAdvance();` placed right after `s_hook_calls++` (`:362`).
  The hook runs exactly once per loop-body iteration, which is the per-tick guarantee — cube-
  change "ticks" (the documented `--tick` over-count) also advance one `DT`, keeping
  `timer_ref_hr` a pure function of loop iterations.
- Add `#include <SYSTEM/TIMER.H>` — already present (`:13`).

**`SOURCES/CONTROL.H`** — extend the usage comment block (`:6-15`) with `--fixed-dt`.

One `PERSO.CPP` edit was needed after all (see §2 correction): the FollowCam re-add at
`:1892` is guarded behind `Timer_FixedDtActive()` so it is skipped only in fixed-dt mode —
default behaviour unchanged. The LIB386 footprint is the one `ManageTime` source line plus
three small functions (`Timer_EnableFixedDt`, `Timer_FixedDtAdvance`, `Timer_FixedDtActive`),
all dead unless the harness arms them.

---

## 6. Baseline promotion + cross-platform

Per research §5/§6, the promotion is **same-platform exact**, not cross-platform exact — `long
double` + `lrintl()` projection rounds differently on Linux x86_64 (80-bit) vs Windows/macOS-ARM
(64-bit), and fixed-dt removes *temporal* nondeterminism only, not that *spatial* difference.

Plan for [`tests/savegame/corpus/baseline_harness.py`](../tests/savegame/corpus/baseline_harness.py):

1. **Pass the flag.** Add `--fixed-dt $CANONICAL_DT_MS` to the `cmd` list in `run_save`
   (`:59-60`), where `CANONICAL_DT_MS = 16` is a module constant (the single named definition,
   §3), exposed as an overridable `--fixed-dt` CLI arg for experiments.
2. **Verify soft notes drop to zero first.** Re-run without `--update`; expect every
   `(+N kinematic)` annotation (`:156,163`) to vanish across the corpus. This is the gate — do
   not promote until it holds.
3. **Promote kinematics to structural.** Delete the `SOFT_POS`/`SOFT_OTHER` special-cases in
   `diff()` (`:105-110`) and the constants (`:39-41`) so `x/y/z/beta/anim/last_frame` mismatches
   become hard failures, then regenerate goldens with `--update`. Update the module docstring
   (`:14-17`) and the final-line note (`:171`).
4. **Cross-platform: single canonical platform, not per-platform subdirectories.** Because
   exactness is per-platform, goldens generated on one OS will *structurally* fail on another for
   the kinematic fields. Rather than build a `baselines/<platform>/` matrix now, define the
   goldens on **one canonical platform** — the Linux x86_64 build the maintainer/CI already uses
   — and document that validating elsewhere means matching that platform. This keeps the
   guardrail strict and the layout flat; per-platform subdirectories can be added later only if a
   second platform actually needs committed goldens.

   - **Why Linux-only goldens, even though Windows/macOS hardware is available.** As a regression
     net, extra platforms add little: a simulation-*logic* regression breaks all three and is
     already caught on Linux; the only Win/macOS-specific delta is the known, constant
     `long double` precision drift, not a regression. And committed per-platform goldens are a
     maintenance trap — CI cannot regenerate them (no retail data in CI), so every
     simulation-touching PR would owe three hand-regenerated sets.
   - **Do run all three once, as a measurement, not as committed goldens.** Research §5 flags
     cross-platform drift but never *quantified* it. A one-time fixed-dt corpus run on
     Linux/Windows/macOS, diffed, answers the open question: is the drift a few units and bounded,
     or does 80-bit-vs-64-bit rounding amplify over N ticks? Record the magnitude in
     `FIXED_DT_RESEARCH.md`. This also surfaces any genuinely structural per-platform difference
     (struct packing, the `T_PTR_NUM` 32/64 union) without standing up continuous goldens. The
     trigger to add per-platform committed goldens is "this measurement showed drift we must
     guard," not "the machines exist."
   - **Docker as the canonical-platform mechanism (follow-up, not this PR).** A non-Linux
     contributor can reproduce the canonical platform in a Linux x86_64 container (SDL dummy
     video, null audio, their own retail data mounted) instead of trusting a native run. Note
     this is *not* the existing equiv-test container — that is 32-bit x86 + UASM, runs
     pure-function byte-equality in CI, and needs no assets or display; the baseline harness
     drives the full game binary and needs retail data + a display, so it stays local-only and
     gets its own image. The container makes the *golden* portable; it does not make a native
     macOS/Windows run match (80-bit vs 64-bit `long double` still differs there).

   This is the one place the plan pushes back on "promote to fully exact": exact within a
   platform/build, yes; exact across platforms, not guaranteed by fixed-dt alone.

`timer_ref_hr` also becomes deterministic under the pin (`base + N*DT`), so it can move out of
`VOLATILE` (`:34`) and be asserted — a useful secondary check that the pin is intact. `fps` and
`log` stay volatile.

---

## Validation

Stated as concrete, runnable checks (research §6):

1. **Reproducibility across runs.** Run `--load X --tick N --fixed-dt 16 --dump-state` 3× on
   the same build/platform with the null audio backend; diff the dumps. Success = byte-identical
   *including* `x/y/z/beta/anim/last_frame` and `timer_ref_hr`. Today those drift ~0.1% on busy
   exterior scenes; fixed-dt should drop that to zero.
2. **FollowCam hazard specifically.** Item (1) on an exterior follow-cam save (a terrain frame
   where `FollowCamTerrainFrame` fires) — proves the re-add (§2) is genuinely a no-op under the
   pin, not just in theory.
3. **RNG / cube-boundary.** Item (1) on a `--tick N` that crosses a cube change; byte-identical
   dumps confirm `srand(TimerRefHR)` is pinned without an explicit reseed (§4).
4. **Baseline kinematic tier → zero soft notes**, then promote (§6).
5. **Opt-in regression guard.** A default run and a harness run *without* `--fixed-dt` must be
   byte-for-byte unchanged vs the current binary (A/B dump on a non-fixed-dt run). Proves the
   `FixedDtActive ? … : SDL_GetTicks()` branch is inert when off.
6. **Simulation still advances.** Extend the spirit of
   [`tests/automation/test_tick.sh`](../tests/automation/test_tick.sh): under fixed-dt the clock
   advances by exactly `N*DT` and the hero idle-anim frame advances deterministically — a
   stronger assertion than today's "advanced at all."
7. **Residual time-read audit.** Grep `GERELIFE.CPP`/`GERETRAK.CPP` for branch-on-`TimerRefHR`
   script logic (research §2 left this not-exhaustively-audited); confirm all reads are pure
   functions of the now-deterministic `TimerRefHR`.

---

## Commit / PR breakdown

Focused, reviewable, dependency-ordered:

1. **`feat(timer): virtual fixed-dt clock source behind a harness flag`** — `TIMER.H` +
   `TIMER.CPP` (state, two functions, the one `ManageTime` source line). Inert until armed; the
   diff is self-contained and reviewable against the "default builds unchanged" constraint.
2. **`feat(control): --fixed-dt deterministic mode`** — `CONTROL.CPP` parse + wiring,
   `CONTROL.H` + `CONTROL.md` docs (flag, canonical 16 ms, determinism section update,
   roadmap item 2 → done). Validation items 1-3, 5, 6.
3. **`test(savegame): promote kinematic baseline tier to structural under fixed-dt`** —
   `baseline_harness.py` flag wiring + soft-tier removal + goldens regenerated on the canonical
   platform. Validation item 4. Separated because it regenerates committed golden data and is the
   step most likely to need maintainer input on the canonical-platform choice.

Could be one PR if the maintainer prefers, but the timer-core change is worth isolating so the
"does this touch default gameplay timing?" review is trivial.

## What I'd push back on in this scope

- **"Fully reproducible" is same-platform.** fixed-dt does not erase the `long double`
  cross-platform coordinate difference (§6). Promising cross-platform byte-identical dumps would
  overstate it; the plan defines goldens on one canonical platform (Docker as the later
  portability mechanism) rather than claiming native cross-platform identity.
- **fixed-dt is not a real-audio determiniser.** Audio-timed script branches
  (`IsSamplePlaying`, research §5) stay nondeterministic under real audio; the harness relies on
  the null backend, same as today. Worth stating so nobody expects fixed-dt to cover it.
- **No explicit RNG reseed unless validation demands it.** Adding a reseed "to be safe" is
  speculative complexity the measurement does not justify (§4); gate it on item 3 failing.
