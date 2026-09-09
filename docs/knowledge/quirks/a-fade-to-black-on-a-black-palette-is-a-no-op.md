---
type: Quirk
title: A fade to black on a black palette is a no-op
description: FadeToBlack returns before its bracket when FlagBlackPal is set and FadeToBlackAndSamples skips its ramp, while the two fades to a palette have no guard and always start from black; callers, the video player and the recorder are each written around that asymmetry.
status: draft
scope: "unconditional; every build and every clock mode"
equivalence: untested
asm_origin: "SOURCES/AMBIANCE.CPP:FadeToBlack"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /subsystems/transitions.md
  - /quirks/playacf-leaves-the-screen-black.md
  - /subsystems/recording.md
  - /decisions/digest-membership.md
sources:
  - id: ambiance-cpp
    resource: ../../../SOURCES/AMBIANCE.CPP
    title: AMBIANCE.CPP, the four fades and SetBlackPal
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/AMBIANCE.CPP, the same guard at the top of FadeToBlack
  - id: playacf-cpp
    resource: ../../../SOURCES/PLAYACF.CPP
    title: PLAYACF.CPP, the comment above the mid-play clear of FlagBlackPal
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the comment above the uncarried table
---

# Scope

Unconditional. The guard is a plain global test at the top of two functions, identical in the initial import and at `as_of`.[^original]

# Evidence

`untested`, read against the original source and the code; no test pins the guard. The measurement that made it matter is the recorder's: seven of nine contributed recordings differed on the flag at tick 0, which is the count of sessions whose first fade-out would have taken the other branch.[^control-cpp]

# Behaviour

`FadeToBlack` tests `FlagBlackPal` and returns before it opens its timer bracket. `FadeToBlackAndSamples` opens the bracket first, skips the ramp when the flag is set, and still stops the rain sample, pauses the samples and sets the flag. Neither `FadeToPal` nor `FadeToPalAndSamples` has a guard: both start at a delta of zero, which is an all-black palette, and ramp up to the target, so a fade-in on a palette that is already lit dips to black first. `SetBlackPal` writes the black palette and sets the flag without a ramp. The flag is written by those five functions, by the video player once mid-play, and by hand in three menu paths and the credits; `PaletteSync` does not touch it.[^ambiance-cpp]

# Why it is load bearing

- The callers are written for it. The script opcodes fade out with `if (!FlagFade)`, the deluge with `if (!FlagBlackPal)`, and several with no test at all; all three shapes are correct only because a second fade to black costs nothing.
- The video player has to clear the flag once its own palette is on screen, or its tail's `FadeToBlack` returns at once and the video cuts instead of fading out. That clear looks like a stray write and is the contract working.[^playacf-cpp]
- A replay has to start on the recording's value. The flag decides whether every fade-out for the rest of the session ramps or returns, and a ramp pumps the clock under a pinned step while a return does not, so two runs that disagree on it at tick 0 diverge on the clock with nothing having diverged. No savegame carries it, which is why the recorder carries it as a header line and compares it as carried.[^control-cpp]
- Making the pair symmetric in either direction changes the game. A guard on the fades to a palette would skip the reveal after a black present, leaving a scene that never lights. Removing the guard from the fades to black would ramp an already-black palette for 200 ms of bracketed clock at every call site that relies on the return, the video player's entry among them.

# What it does not tell you

- **What the palette is.** The flag tracks the fades, not the hardware. An instant sync through `FlagPal` or `changepal` in the tail of `AffScene` moves the palette and leaves the flag where it was, so the flag can say black over a lit screen until the next fade corrects it, and the next fade to black will then return without ramping.
- **Whether a fade pumped the clock.** A skipped fade-out banks no steps and opens no bracket in `FadeToBlack`, and opens one in `FadeToBlackAndSamples`; the two functions are not interchangeable under a pinned step even when neither ramps.

[^ambiance-cpp]: SOURCES/AMBIANCE.CPP, `FadeToBlack`, `FadeToBlackAndSamples`, `FadeToPal`, `FadeToPalAndSamples` and `SetBlackPal`.
[^original]: Commit 333929ab, the initial import, SOURCES/AMBIANCE.CPP, the first statement of `FadeToBlack`.
[^playacf-cpp]: SOURCES/PLAYACF.CPP, `PlayAcf`, the comment above `FlagBlackPal = FALSE`.
[^control-cpp]: SOURCES/CONTROL.CPP, the comment above `s_uncarried`, "Measured over the nine contributed recordings".
