# Engine timing

Clocks, locks, and the game-clock vs wall-clock distinction. Read this before touching
`LIB386/SYSTEM/TIMER.CPP` or adding a `LockTimer`/`SaveTimer` call site. Companion to
[LIFECYCLES.md](LIFECYCLES.md) (which gives the main-loop call order) and
[FIXED_DT_PLAN.md](FIXED_DT_PLAN.md) (which is the harness-only deterministic overlay).

Truth hierarchy: code > this document > external sources.

## The two clocks

The engine maintains two parallel `U32` millisecond clocks (`LIB386/SYSTEM/TIMER.CPP`):

| Variable | Meaning | Who reads it |
|---|---|---|
| `TimerSystemHR` | Wall clock. Snapshot of `SDL_GetTicks()` taken inside `ManageTime()`. | FPS calc, present pacing, anywhere "how long has the host been running" matters. |
| `TimerRefHR` | Game clock. Advances by wall-clock delta only when *unlocked*. | Animation interpolation (`Obj.Time`), `DoTrack` script delays, music fades, zone timers, RNG seeding at `ChangeCube`. |

The distinction matters: any pause/render/modal subloop that should freeze game time pauses
`TimerRefHR` but lets `TimerSystemHR` keep ticking. The fixed-dt deterministic mode
([FIXED_DT_PLAN.md](FIXED_DT_PLAN.md)) replaces `SDL_GetTicks()` with a virtual clock that
only advances on explicit tick events, so both `TimerSystemHR` and `TimerRefHR` become
reproducible byte-for-byte.

## Internal state

| Variable | Role |
|---|---|
| `LastTime` | The most recent `TimerSystemHR` an *unlocked* `ManageTime()` saw. The next unlocked call computes `TimerRefHR += TimerSystemHR - LastTime`, then updates `LastTime` to the new `TimerSystemHR`. While locked, both `TimerRefHR` and `LastTime` are held frozen. |
| `MemoTimerRefHR` | Snapshot of `TimerRefHR` taken by the outermost `SaveTimer()`. The outermost `RestoreTimer()` *assigns* `TimerRefHR` back to this snapshot (see below). |
| `TimerLock` | Lock depth counter. Incremented by `LockTimer()`, decremented by `UnlockTimer()`. `ManageTime()` skips the `TimerRefHR`/`LastTime` updates while `TimerLock > 0`. |
| `CmptMemoTimerRef` | `SaveTimer/RestoreTimer` nesting counter. Only the outermost pair runs the snapshot/restore. |
| `LastEvaluate` | Per-second window anchor used by the FPS calc inside `ManageTime`. Independent of `LastTime`. |

`FixedDt*` state (off by default; see [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md)) overlays a
virtual clock source on `TimerSystemHR` for harness determinism.

## Lock vs Save — they are *not* the same

This is the most-common rake to step on. Both look like "pause and resume," only one is.

| Pattern | Outermost-entry behaviour | Outermost-exit behaviour | Net effect on game clock |
|---|---|---|---|
| `LockTimer` / `UnlockTimer` | Snapshot `LastTime`. | Resume `LastTime` from where it was. Next `ManageTime` credits the full elapsed wall time to `TimerRefHR`. | **Transparent**: the locked interval is invisible to game logic but the game clock catches up afterwards. |
| `SaveTimer` / `RestoreTimer` | Snapshot `TimerRefHR` into `MemoTimerRefHR`. | `TimerRefHR = MemoTimerRefHR`. Elapsed wall time is **discarded**. | **Rewind**: the locked interval is invisible *and* the game clock acts as if it never happened. |

Choose by intent:

- **Wrapping a synchronous render or a brief CPU window where game time should pause and
  then resume correctly**: `LockTimer/UnlockTimer`. Example: the `AffScene` full-redraw
  path at `SOURCES/OBJECT.CPP:5400`.
- **Wrapping a modal subloop (menu, dialogue, paused message) that should leave the game
  clock exactly where it found it**: `SaveTimer/RestoreTimer`. Example:
  `SOURCES/PERSO.CPP:354` around the pause/clover dialog.

`SaveTimer/RestoreTimer` discarding time is intentional, not a bug. `LockTimer/UnlockTimer`
*used* to discard time too — see the history section below.

## `ManageTime` and the call-site population

`ManageTime()` is the only function that mutates `TimerSystemHR`, `TimerRefHR`, and
`LastTime`. The body is short (`LIB386/SYSTEM/TIMER.CPP:134`):

```c
void ManageTime() {
    TimerSystemHR = FixedDtActive ? FixedDtNow : SDL_GetTicks();

    if (!TimerLock) {
        TimerRefHR += TimerSystemHR - LastTime;
        LastTime = TimerSystemHR;
    }

    // FPS calc against LastEvaluate ...
}
```

There are ~100 `ManageTime()` call sites across the engine. Most are inside modal/wait
loops that pump the clock manually because the main loop's `ManageSystem()` macro
(`LIB386/H/SYSTEM/TIMER.H:57`) isn't reached during a modal. Examples:

| File | Use case |
|---|---|
| `SOURCES/PERSO.CPP` (`MainLoop`) | Once per tick via `ManageSystem()` at the top of the loop body. |
| `SOURCES/AMBIANCE.CPP`, `SOURCES/MESSAGE.CPP`, `SOURCES/INVENT.CPP` | Modal subloops pump the clock while waiting for input or for an animation to finish. |
| `SOURCES/MUSIC.CPP` | Music fade-out loops; under fixed-dt these use `Timer_FixedDtPump` to step the virtual clock without a present. |
| `LIB386/SVGA/SDL.CPP`, `LIB386/SYSTEM/TIMER.CPP` | Internal callers from `LockTimer`/`SaveTimer`/`SetTimerHR`. |

If you add a new modal/wait loop, pump `ManageTime` (and call `Timer_FixedDtPump` if you
also want the loop to terminate under fixed-dt) — see `SOURCES/MUSIC.CPP:312` for the
canonical fade pattern.

## Fixed-dt mode (harness only)

Inert in default builds. When `Timer_EnableFixedDt(dt_ms)` is called (from
`Control_Begin` when the harness sees `--fixed-dt`):

- `TimerSystemHR` reads `FixedDtNow` instead of `SDL_GetTicks()`.
- `Timer_FixedDtAdvance()` is called once per main-loop tick from the control hook,
  incrementing `FixedDtNow` by `dt`.
- `Timer_FixedDtPresent()` is called from `BoxBlit`; the first present after each
  `Timer_FixedDtAdvance()` is "free" (the main-loop render already paid for it). Every
  additional present in the same tick advances `FixedDtNow` by `dt` — that's how modal
  subloops avoid deadlocking on a `TimerRefHR` deadline.
- `Timer_FixedDtPump()` advances `FixedDtNow` without a present, for non-presenting
  busy-waits.

Result: `--dump-state` is byte-identical across repeated harness runs. The host wall
clock has zero influence. See [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md) for the design rationale
and [CONTROL.md](CONTROL.md) for the CLI surface.

## History — the 1997 `ManageTime` bug

A latent bug lived in `ManageTime` from the 1997 retail release until 2026-05.

The original Adeline source (`LIB386/SYSTEM/TIMERWIN.CPP` in the initial public
commit `333929ab`, 2021-10-27) updated `LastTime` *unconditionally* — outside the
`if (!TimerLock)` guard:

```c
if (!TimerLock) {
    TimerRefHR += TimerSystemHR - LastTime;
}
LastTime = TimerSystemHR;  // unconditional
```

Effect: any `ManageTime()` call inside a Lock or Save window (audio, input, animation,
modal pumps — ~100 sites) bumped `LastTime` to the current wall clock while skipping the
`TimerRefHR` increment. When the lock released, the next `ManageTime()` saw
`TimerSystemHR - LastTime ≈ 0` and the whole locked interval was discarded from `TimerRefHR`.

**Why retail never noticed.** On bare-metal DOS at 640×480, the `LockTimer` window at
`OBJECT.CPP:5400` (around the `AffScene` full-redraw path) contained sub-millisecond CPU
work between the lock and the direct-to-VGA blit. The amount of wall time "lost" per frame
was rounding noise.

**Why the SDL port resurrected it.** The SDL port (PR #3, commit `e29b9bd7`, 2023-01-24)
created `LIB386/SYSTEM/TIMER.CPP` as a byte-for-byte clone of the original logic, with
`timeGetTime()` replaced by `SDL_GetTicks()`. The same window now contained
`BoxBlit` → `UnlockVideoSurface` → `SDL_RenderPresent`, which under SDL3 + GL backbuffer
blocks on vsync for a full refresh period (~16.67 ms at 60 Hz). At native resolution where
vsync becomes the dominant per-frame cost, ~16-17 ms of game-clock advance was discarded
*every frame*. Animations and sim ran at 25-50% real speed; held movement keys felt
unresponsive.

**Local patch first.** PR #50 (`8af40faa7`, 2026-04-08) noticed the symptom on
`FollowCamTerrainFrame` exterior frames and patched it locally
(`SOURCES/PERSO.CPP:1916`) with a `SaveTimer`/`RestoreTimer` + wall-clock re-add. The
comment there names the mechanism exactly:

> "save the timer before the render+vsync and restore it after, then add back the real
> wall-clock time elapsed ... This makes the full render+vsync transparent to the game
> clock so hero, NPCs, and rain run at correct speed."

The patch fixed the FollowCam exterior path. Every other Lock/Save site in the engine
still discarded its window's wall time.

**General fix in 2026-05.** Diagnosis with a `perftrace`-style ring-buffer instrumentation
narrowed the leak to one source line (`OBJECT.CPP:5400` `LockTimer()`, called every frame
in the user-facing scene). The fix is one line moved: `LastTime = TimerSystemHR;` moves
*inside* the `if (!TimerLock)` block, so `LastTime` is held frozen across the locked
window and the next unlocked `ManageTime` correctly credits the full wall-clock delta to
`TimerRefHR`. The `FollowCamTerrainFrame` workaround in `PERSO.CPP:1916` becomes
redundant; left in place for now, removable in a follow-up.

Self-determinism (`test_demo` at `--fixed-dt 16`, byte-identical reruns) is preserved.
The 5-tick projection-corpus replay drifts on 12/50 saves with active animations in the
window — the drift is the fix working, since animation interpolation now lands at the
correct game-clock time. The corpus golden was regenerated alongside the fix.

## Practical guidance

When adding code that touches the timer, in rough order of how often you'll need each:

1. **Don't wrap rendering in `SaveTimer`.** Use `LockTimer/UnlockTimer`. `Save/Restore`
   *rewinds* the game clock; you almost never want that.
2. **Wrapping a modal subloop (menu, dialogue, paused message)?** Use
   `SaveTimer/RestoreTimer` — the rewind is the point.
3. **Adding `ManageTime()` inside a Lock/Save window** (e.g. you have a busy-wait that
   pumps animation): pre-2026-05 fix, this silently discarded the wall time. Post-fix it
   doesn't, but the standing rule is the same — only pump `ManageTime` inside loops that
   need the game clock to actually move, and call `Timer_FixedDtPump`/`Timer_FixedDtPresent`
   alongside if the loop should also terminate under fixed-dt.
4. **Adding a new modal/wait loop?** Mirror `SOURCES/MUSIC.CPP:312` (`PauseMusic`,
   fade-out): `SaveTimer()`, loop with `ManageTime()` + `Timer_FixedDtPump()`,
   `RestoreTimer()`.
5. **Touching `ManageTime` itself?** Re-run the projection-corpus regression after any
   change to `TimerRefHR`/`LastTime` semantics — the 5-tick golden catches one-frame
   timing drift across 50 diverse saves and is the strongest fixed-dt invariant the
   suite carries today.

## Related

- [LIFECYCLES.md](LIFECYCLES.md) — main loop step order; where `ManageTime` lives in the
  per-tick sequence.
- [CONTROL.md](CONTROL.md) — the CLI control harness; `--fixed-dt`, `--tick`,
  `--dump-state`.
- [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md) — the deterministic-clock design that overlays the
  virtual time source on `TimerSystemHR`.
- [FIXED_DT_RESEARCH.md](FIXED_DT_RESEARCH.md) — Phase 1 research mapping loop classes
  and inventorying every site that reads `TimerRefHR`/`TimerSystemHR`.
- `LIB386/SYSTEM/TIMER.CPP`, `LIB386/H/SYSTEM/TIMER.H` — source of truth for
  everything above.
