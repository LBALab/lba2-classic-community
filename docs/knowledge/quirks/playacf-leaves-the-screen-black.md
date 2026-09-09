---
type: Quirk
title: PlayAcf leaves the screen black
description: The video player fades its last frame to black, clears, presents and forces the palette black before it returns, and every caller is written to reveal the next screen itself; a player that hands back the previous screen lit makes the next scene flash for a frame under every in-game caller.
status: draft
scope: "every caller and every clock mode; which reveal the caller owes differs by caller"
equivalence: untested
asm_origin: "SOURCES/PLAYACF.CPP:PlayAcf"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /subsystems/transitions.md
  - /quirks/affscene-presents-before-it-reveals.md
  - /quirks/a-fade-to-black-on-a-black-palette-is-a-no-op.md
  - /decisions/presents-mint-the-pinned-step.md
sources:
  - id: playacf-cpp
    resource: ../../../SOURCES/PLAYACF.CPP
    title: PLAYACF.CPP, the entry, the palette clear and the tail of PlayAcf
  - id: playacf-h
    resource: ../../../SOURCES/PLAYACF.H
    title: PLAYACF.H, the contract at the declaration
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/PLAYACF.CPP, the same four lines at the end of PlayAcf
  - id: gerelife-cpp
    resource: ../../../SOURCES/GERELIFE.CPP
    title: GERELIFE.CPP, the LM_PLAY_ACF caller
  - id: gamemenu-cpp
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the BABY, DELUGE and INTRO callers
  - id: console-cmd-cpp
    resource: ../../../SOURCES/CONSOLE/CONSOLE_CMD.CPP
    title: CONSOLE_CMD.CPP, the playvideo preview and the ui video capture
  - id: transitions-doc
    resource: ../../TRANSITIONS.md
    title: Scene and FMV transitions (docs/TRANSITIONS.md), "The PlayAcf contract" and "The #404 regression"
---

# Scope

Every caller. At `as_of` there are seven in the game, the Life and Track script opcodes, the main loop's own call, the visionneuse item, and the three menu videos, plus two console verbs. The contract is the same for all of them; what differs is how each one reveals the screen afterwards.[^playacf-cpp]

# Evidence

`untested`, read against the original source. The initial import ends `PlayAcf` with the same four lines, `FadeToBlack`, `Cls`, `BoxStaticFullflip`, `SetBlackPal`, so the contract is Adeline's and the two-year departure from it was the port's.[^original] No test in the tree pins it: the present trace that caught the regression stays on an unshipped branch, and a screenshot cannot see one frame.

# Behaviour

On entry the player forces the palette black, resets the dirty boxes, clears and presents, so the scene the caller faded out is gone before the first video frame. When the first video palette lands it clears `FlagBlackPal` by hand, because the palette on screen is the video's, and because the tail's `FadeToBlack` would otherwise return at once and the video would cut instead of fading. On exit it fades the last frame's palette to black, clears, presents the clear frame and forces the palette black again, with the flag set.[^playacf-cpp]

The in-game callers share one shape: open a timer bracket, fade out unless a fade is already pending, arm `FlagFade`, play, reload the grid for an interior, reset the particle flows, repoint `PtrPal` at the scene palette, arm `FlagFade` again, set `FirstTime` to a full flip, close the bracket. The reveal is the next `AffScene` consuming `FlagFade`.[^gerelife-cpp] Two callers reveal for themselves: the win screen repoints `PtrPal` and calls `FadeToPal`, and the console preview snapshots `Log` before the video, puts it back after, and fades it in.[^gamemenu-cpp][^console-cmd-cpp]

# Why it is load bearing

- The console PR replaced the tail with a restore of the pre-video screen, lit, "so we don't leave a black screen". Under every caller that arms `FlagFade` the next `AffScene` then presented the new scene at a lit palette for one frame before `FadeToPalAndSamples` forced it black: the pop-in of #404. It is a race against the Smacker decoder's own clock, so it was invisible at 30 fps and a clear flash at high frame rates, and it was intermittent even under a pinned step. The fix was to put the four original lines back and give the one caller the band-aid had served, the bare preview, its own snapshot and fade.[^transitions-doc]
- A caller that neither arms `FlagFade` nor fades in for itself sits on black. That is the contract working, not a defect in the player, and the repair is at that caller. The console capture verb leaves the screen black on purpose, because a headless capture has no screen to reveal.[^console-cmd-cpp]
- The clear of `FlagBlackPal` mid-play is part of the contract, not a stray write: remove it and the fade-out at the end of every video becomes a cut, because [a fade to black on a black palette is a no-op](/quirks/a-fade-to-black-on-a-black-palette-is-a-no-op.md).

# What it does not tell you

- **Which reveal a caller owes.** The opcode callers arm `FlagFade` and wait for the loop. The intro's caller fades to black on the logo palette, runs one `AffScene` with `FlagFade` cleared so the opening scene is presented black, and only then arms `FlagFade` for the loop's first render. The deluge caller arms `FlagFade` and calls `AffScene` itself, so the reveal happens inside the menu code. The win screen and the preview fade in on their own.[^gamemenu-cpp]
- **Whether the video played.** `PlayAcf` returns 0 for a missing video and 0 for a video that ran to its end; only an interruption returns the key that caused it. A caller cannot tell absence from completion by the return value.[^playacf-cpp]
- **Whether the frame at the reveal was black.** The tail sets the flag and the palette together, and the flag is what the engine reads afterwards; what the viewer saw at the next present is only observable with a present trace, and there is none in the tree.

[^playacf-cpp]: SOURCES/PLAYACF.CPP, `PlayAcf`: the `SetBlackPal` and `Cls` after the video is opened, the `FlagBlackPal = FALSE` where the first palette is synced, the four lines above `HQ_ResumeSamples`, and the three early `return 0`.
[^playacf-h]: SOURCES/PLAYACF.H, the contract comment above the declaration of `PlayAcf`.
[^original]: Commit 333929ab, the initial import, SOURCES/PLAYACF.CPP, the end of `PlayAcf`.
[^gerelife-cpp]: SOURCES/GERELIFE.CPP, `case LM_PLAY_ACF`; SOURCES/GERETRAK.CPP and SOURCES/PERSO.CPP carry the same shape.
[^gamemenu-cpp]: SOURCES/GAMEMENU.CPP, the `PlayAcf("BABY")` case and the comment after it, the `PlayAcf("DELUGE")` block, and the lines after the `Introduction()` call.
[^console-cmd-cpp]: SOURCES/CONSOLE/CONSOLE_CMD.CPP, the comment above the snapshot in the `playvideo` verb and the comment above `Video_RequestCapture`.
[^transitions-doc]: docs/TRANSITIONS.md, "The PlayAcf contract (critical)" and "The #404 regression".
