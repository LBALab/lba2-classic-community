---
type: Quirk
title: The letterbox slides only on an objects-only flip
description: The cutscene bars are a clip window that FixeCinemaMode animates on the game clock, and the original AffScene animates it only in the objects-only flip mode while a full flip redraws it in place; the port's exterior auto-camera forces a full flip every frame, so the bars stuck after a cutscene and then snapped, and the port now animates the retract in both modes.
status: draft
scope: "a cutscene ending with the bars up; the stick was visible only with the port's exterior auto-camera moving"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:FixeCinemaMode"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
owner: /subsystems/transitions.md
relates_to:
  - /quirks/affscene-presents-before-it-reveals.md
  - /subsystems/timing.md
sources:
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, FixeCinemaMode and the flip switch of AffScene
  - id: gerelife-cpp
    resource: ../../../SOURCES/GERELIFE.CPP
    title: GERELIFE.CPP, the LM_CINEMA_MODE opcode that arms the ramp
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/OBJECT.CPP, the flip switch with TRUE in one case and FALSE in the other
  - id: transitions-doc
    resource: ../../TRANSITIONS.md
    title: Scene and FMV transitions (docs/TRANSITIONS.md), "Letterbox timer"
---

# Scope

The end of a cutscene, when `CinemaMode` drops to zero with `ClipWindowYMin` still above it. In normal play the clip window's top is zero at every resolution, because the aspect fit insets the sides and not the top, so a non-zero top is a clean signal that the bars are up.[^object-cpp]

# Evidence

`untested`, read against the original source. The initial import passes `TRUE` to `FixeCinemaMode` in the objects-only flip case and `FALSE` in the full flip case.[^original] The stick and the snap were reproduced and fixed from a per-frame trace of the flip mode, the cinema mode and the clip top; no test pins either.

# Behaviour

The bars are `ClipWindowYMin` and `ClipWindowYMax`. `FixeCinemaMode` moves them by a rule of three on `TimerRefHR - TimerCinema` over `DureeCycleCinema`, inward while `CinemaMode` is set and outward when it is clear, but only when called with `TRUE`; with `FALSE` it redraws the bars where they are, which is the redraw a camera change inside a cutscene wants. The original `AffScene` calls it with `TRUE` in the objects-only flip and `FALSE` in the full flip. The ramp's start and duration are armed by the `LM_CINEMA_MODE` opcode alone; `ResetCinemaMode` and the other paths that clear the mode do not re-arm them.[^object-cpp][^gerelife-cpp]

At `as_of` the port's full flip animates the retract too, when the mode is clear and the bars are up, and leaves the active-cutscene redraw at `FALSE`; and `FixeCinemaMode` re-arms the ramp itself from the current position when it sees the mode drop to zero with the bars up, tracked through a previous-frame copy of the mode.[^object-cpp]

# Why it is load bearing

- The original had no exterior auto-camera, so after a cutscene it stayed in the objects-only flip and the bars slid out. The port's recentring at the cube edge and its follow-camera lerp force a full flip every frame the camera moves, and the hero reads as near the edge against the shrunken clip window, so the retract was starved until the camera settled and then ran on a ramp timer that had already expired: stuck, then snapped. Neither half is a bug in `FixeCinemaMode`; both are the original dispatch meeting a port feature.[^transitions-doc]
- Passing `TRUE` in every case is not the fix. The `FALSE` branch is what redraws the bars at a camera change during a cutscene, and the retract is the only thing the full flip should animate. Removing the port's added branch as redundant brings the stick back for anyone whose camera moves after a cutscene, which on an exterior is everyone.
- The ramp runs on `TimerRefHR`, so a load or a scene change that installs the clock while the bars are moving jumps the ramp; [timing](/subsystems/timing.md) owns that the clock is not monotonic across either.

# What it does not tell you

- **Why the bars are up.** The clip window says they are, not whether a cutscene or a stale clear left them there. The re-arm keys on the mode dropping, not on who dropped it.
- **That the retract will finish.** It advances only on frames that reach a flip mode that animates; the no-flip render the load path runs after `ChangeCube` leaves the bars where they are.

[^object-cpp]: SOURCES/OBJECT.CPP, `FixeCinemaMode`, including the re-arm block and the `PrevCinemaMode` static above it, and the `case AFF_OBJETS_FLIP` and `case AFF_ALL_FLIP` of `AffScene`.
[^gerelife-cpp]: SOURCES/GERELIFE.CPP, `case LM_CINEMA_MODE`, the assignments to `DebCycleCinema`, `TimerCinema` and `DureeCycleCinema`.
[^original]: Commit 333929ab, the initial import, SOURCES/OBJECT.CPP, the `switch` at the end of `AffScene`.
[^transitions-doc]: docs/TRANSITIONS.md, "Regression classes seen so far", the letterbox timer entry.
