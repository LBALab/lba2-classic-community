---
type: Subsystem
title: Scene and video transitions
description: Black brackets the load. The fade-out runs inside the scene load, the new scene is composited and presented under a black palette, and the next render's tail ramps the palette up; every regression in the family is a present that broke that order.
status: draft
subsystem: transitions
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /quirks/sample-fades-end-on-the-wall-clock.md
  - /decisions/presents-mint-the-pinned-step.md
  - /subsystems/timing.md
  - /subsystems/save.md
  - /subsystems/recording.md
sources:
  - id: transitions-doc
    resource: ../../TRANSITIONS.md
    title: Scene and FMV transitions (docs/TRANSITIONS.md)
  - id: ambiance-cpp
    resource: ../../../SOURCES/AMBIANCE.CPP
    title: AMBIANCE.CPP, the fade primitives
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, ChangeCube, AffScene, FixeCinemaMode and ResetCinemaMode
  - id: diskfunc-cpp
    resource: ../../../SOURCES/DISKFUNC.CPP
    title: DISKFUNC.CPP, the fade gate in LoadScene
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the cube-change block and the mid-frame guard in MainLoop
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the table of the five globals no savegame carries
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the replay outcomes and the poll and tick counters
---

Distilled from [docs/TRANSITIONS.md](../../TRANSITIONS.md), which stays the walk-through contributors read and owns the mechanism descriptions and the regression history; this concept holds the contracts, the seams, and what the doc does not say. The principles are structural. The call sites and the open defects are read at `as_of`.

# Principles

- **Black brackets the load.** The screen fades to black, the old scene is torn down and the new one composited while the palette is black, and the palette ramps up on the composited frame. The reveal is a palette fade, not a content fade, so the new scene has to be on screen, under a black palette, before the ramp starts.[^transitions-doc]
- **The fade-out is inside the load and the fade-in is the next render's tail.** `LoadScene` fades before it reads the new cube; `AffScene` reveals after it has presented. Nothing between the two presents at a lit palette on purpose, and every regression class in the family is something that did.[^diskfunc-cpp]
- **A fade is timed, not stepped.** A ramp lasts 200 ms of `TimerSystemHR` whatever the frame rate, and only the number of presents scales with it. A fade timed on the wall clock counts the synchronous load and collapses to one step. Which clock that is, and what advances it, belongs to [timing](/subsystems/timing.md).[^ambiance-cpp]
- **Under SDL a palette write shows nothing until something presents.** The original tinted the live framebuffer with the palette write alone, so a fade could ramp without touching the frame. The port has to re-present the composited frame at every step, and has to do it with `BoxBlit` rather than `BoxUpdate`, whose clean restores the actor-less backdrop over the actor boxes. This is a port contract and not an original behaviour; it has been broken in both directions, by a fade that cleaned and by a fade that did not present.[^ambiance-cpp]

# Contracts

| Contract | Statement |
|---|---|
| A genuine transition is marked | `NewCube` is set by a zone crossing, a Life script `LM_CHANGE_CUBE`, the phantom teleport, a cutscene ending on a destination cube (`ResetCinemaMode`), the console `cube` verb, and a load, which reads it from the file. Only the first three set `FlagChgCube`, 1, 2 and 1. `MainLoop` reads `NewCube != -1` at the top of its loop and calls `ChangeCube`.[^object-cpp] |
| What `FlagChgCube` gates | The timer bracket `ChangeCube` opens, the autosave at the tail of `AffScene` (not for the phantom cube), `FlagReinit`, and which camera centring runs. A console warp, a load and a cutscene's destination cube are cube changes that are not genuine transitions: no autosave, no bracket.[^object-cpp] |
| The load path is silent | With `FlagLoadGame` set, `LoadScene` skips the fade-out and the clear, `ChangeCube` arms neither `FlagFade` nor `FlagPal`, and the tail of `AffScene` syncs the palette instantly. The menu reveals a load its own way, through the save-thumbnail zoom, and that instant sync is original. A harness `--load` therefore never fades, and a screenshot of its first frames is lit.[^object-cpp] |
| The reveal order | `ChangeCube` arms `FlagFade` and sets `FirstTime = AFF_ALL_FLIP`. The next `AffScene(FirstTime)` draws into `Log`, presents through its flip switch, and only then runs its tail: `changepal`, then `FlagPal` as an instant sync, then `FlagFade` as `FadeToPalAndSamples`, else `FlagLoadGame` as an instant sync. The palette has to be black at that present, which is [a quirk of its own](/quirks/affscene-presents-before-it-reveals.md).[^object-cpp] |
| The video contract | `PlayAcf` returns with the screen black and `FlagBlackPal` set, and the caller owes the reveal. Seven game call sites and two console verbs at `as_of`; the in-game ones arm `FlagFade`, the win screen and the console preview fade in for themselves. [The PlayAcf quirk](/quirks/playacf-leaves-the-screen-black.md) owns it, and which callers keep the video's clock belongs to [the mint decision](/decisions/presents-mint-the-pinned-step.md).[^transitions-doc] |
| The two flags | `FlagBlackPal` says the palette is black: set by `SetBlackPal` and by the end of every fade to black, cleared by the end of every fade to a palette, and cleared by hand in three menu paths and the credits where they present lit without a fade. `FlagFade` says a reveal is pending and is consumed once, at the tail of `AffScene`. `PtrPal` is a pointer `ChoicePalette` repoints and syncs nothing by itself.[^ambiance-cpp] |
| No savegame carries the flags | `FlagFade` and `FlagBlackPal` decide whether a fade runs at all, and a fade pumps the clock under a pinned step, so a replay that starts on the wrong value diverges on the clock with nothing having diverged. Seven of nine contributed recordings differed on `FlagBlackPal` at tick 0. The recorder carries both as `state.` header lines and compares them as carried, with the three dialogue globals that share the problem.[^control-cpp] |
| The letterbox is a clip window | The cutscene bars are `ClipWindowYMin` and `ClipWindowYMax`, moved by `FixeCinemaMode` on `TimerRefHR` against `TimerCinema`. Same subsystem, a different mechanism from the palette, and its own [dispatch quirk](/quirks/the-letterbox-slides-only-on-an-objects-flip.md).[^object-cpp] |
| A cube change's fade is one digest record, and a script's is none | Inside one `MainLoop` iteration the order is fixed. The `NewCube` block at the top runs `ChangeCube`, which arms `FlagFade` on the fading branch of the same statement that gives [the exterior Quirk](/quirks/two-exterior-cubes-change-without-a-fade.md) its exception, and whose `LoadScene` fade-out sets `FlagBlackPal`; `Control_TickHook` then samples the digest; the script runners and the rest of the frame follow; `AffScene` at the end clears `FlagFade` in its tail, and the reveal it runs clears `FlagBlackPal`. So a cube change's fade reads as each flag at 1 for exactly one tick, whatever the frame rate and however many presents the ramp took. A fade that begins after the sample reads as nothing: a Life or Track script's video transition arms `FlagFade` and blackens the palette inside `DoLife` or `DoTrack`, after the hook, and the same iteration's `AffScene` clears both before the next sample, so the digest never sees it; the demo build's own cube-change block at the top of the loop runs `ChangeCube` and an `AffScene` before the hook, with the same result. The oracle therefore reads one way, and it covers one shape. A flag at 1 across more than one tick in a replay is a candidate defect signal, a reveal that did not run or a flag left set across a boundary; a fade the digest did not see is not evidence that none ran. A fade that hangs is the other shape and this oracle cannot see it: control never leaves the script runner to reach the next sample, so there is no further digest record at all, and the signature is ticks frozen while polls keep advancing, which is the replay's stall outcome, waiting on input the recording does not contain. The two instruments cover different failures and neither subsumes the other. Corroborated by a peer session on five contributed recordings with per-tick telemetry, four carrying fades and one carrying none over 918 ticks: 124 fades, every one a single tick, read by a reader that took the tail from each file's declared digest version and was checked against the shape of the whole modal block before it was trusted. All five declare digest version 2, the files are outside the tree, and the converse, that a real hang shows as a multi-tick fade, is not demonstrated. Keyframes cannot see any of this: at one tick per fade a 3% sample misses nearly all of them, and a reading of one of those files from keyframes alone had said two transitions in ninety thousand ticks.[^perso-cpp][^ambiance-cpp][^record-cpp] |
| The invariant | Fingerprint every present as content and brightness. Correct is bright old, ramp down, black, swap, black new, ramp up, bright new. Pop-out: content changes at a lit palette before the fade-out covered it. Pop-in: the new content is presented lit before the ramp. Reveal before swap: the swap is bracketed but the ramp reveals the old scene, because a `NewCube` set outside the object loop misses the mid-frame `goto startloop` guard, and the frame renders and reveals before `ChangeCube` runs; that third shape is open as #425 and [docs/TRANSITIONS.md](../../TRANSITIONS.md) names only the first two.[^perso-cpp] |

# Seams

- **The fade gate in `LoadScene`**, `!FlagLoadGame AND (!FlagDrawHorizon OR CubeMode + LastCubeMode != 2)`, repeated in `ChangeCube` for arming `FlagFade`. The load half is the silent-load contract; the horizon half is [the exterior quirk](/quirks/two-exterior-cubes-change-without-a-fade.md).[^diskfunc-cpp]
- **The tail of `AffScene`** is the only place a reveal happens, and the autosave sits in it between the present and the reveal.[^object-cpp]
- **`FadePal`** is the one present step every black fade funnels through, `FadeToBlack`, `FadeToPal` and both `AndSamples` variants; a fix to how the family presents lands there. `FadePalToPal`, the lit-to-lit crossfade behind the `LM_FADE_TO_PAL` opcode, has its own loop and needed its own fix.[^ambiance-cpp]
- **The entry and tail of `PlayAcf`**: black, reset, clear and present on the way in; `FlagBlackPal` cleared once the first video palette lands; fade to black, clear, present and black palette on the way out. Its loop freezes while the console is open or the window has lost focus, and a never-focused run keeps playing.
- **The uncarried table** in CONTROL.CPP, where `state.fade=` and `state.blackpal=` are written and installed, one row each.[^control-cpp]
- **The mid-frame guard** in `MainLoop`, `if (NewCube != -1) goto startloop`, inside the per-object loop, so it catches a cube change a Life script makes and nothing a later stage makes.[^perso-cpp]
- **No instrument in the tree.** The present trace, the grader and their fixtures that found the pop-in live on a branch [docs/TRANSITIONS.md](../../TRANSITIONS.md) names and the tree does not carry; `tests/transition` does not exist. The only oracle checked in is a screenshot, which shows an end state and cannot see a missing middle.

# What it does not tell you

- **Whether a fade ran.** A fade to black on a black palette returns before its bracket, and every caller is written for that. The flag says the palette is black, not how it got there, and a replay started on a black palette takes the same short path as the recording did only if the header carried the flag.
- **That a cube change fades.** Between two exterior cubes with the horizon drawn there is no fade and no clear, and the new cube is presented over the old one at a lit palette on purpose. The principle has an exception the reference doc does not name.
- **Whether the frame at the swap was black.** `FlagBlackPal` is engine state; the hardware palette is what the viewer saw, and the two were apart for one frame under every video caller for two years without a test noticing. A screenshot taken under a black palette is a black image whatever `Log` holds, and one taken after the ramp shows the end state; neither sees the frame.
- **How long a transition takes in ticks.** A fade is 200 ms of game clock spread over as many presents as the frame rate allows, inside one `MainLoop` iteration, and under a pinned step each of those presents mints clock. A tick count across a transition is not comparable between a pinned and a loose run, while a cube change's fade is one digest record and a script's fade is none on every run, which is the contract above.

[^transitions-doc]: [docs/TRANSITIONS.md](../../TRANSITIONS.md), "The core principle", "The fade primitives", "FMV transitions" and "Regression classes seen so far".
[^ambiance-cpp]: [SOURCES/AMBIANCE.CPP](../../../SOURCES/AMBIANCE.CPP), `FadePal` and the comment above its `BoxBlit`, `FadeToBlack`, `FadeToBlackAndSamples`, `FadeToPalAndSamples`, `SetBlackPal` and `FadePalToPal`.
[^object-cpp]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `ChangeCube` from the reseed to `NewCube = -1`, the flip switch and tail of `AffScene`, `ResetCinemaMode` and `FixeCinemaMode`.
[^diskfunc-cpp]: [SOURCES/DISKFUNC.CPP](../../../SOURCES/DISKFUNC.CPP), the gate around `FadeToBlackAndSamples` in `LoadScene` and the comment above `BoxReset`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`: the `NewCube != -1` block at the top with its `#ifdef DEMO` half, the one `Control_TickHook()` call after it, the `DoTrack` and `DoLife` calls in the per-object loop, the `AffScene(FirstTime)` call at the end of the iteration, and the `goto startloop` inside the per-object loop.
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), the comment above `s_uncarried` and the `DIGEST_CARRIED` lines in `Control_StateDigest`.
[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), `REPLAY_END_STALL` in the `T_ReplayEnd` enum and its comment, and the `s_polls` and `s_ticks` counters.
