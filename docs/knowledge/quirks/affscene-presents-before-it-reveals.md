---
type: Quirk
title: AffScene presents the frame before it consumes the reveal
description: The render's flip switch presents the composited frame under whatever palette is loaded, and only the tail that follows syncs or fades the palette, so a clean reveal depends on the palette being black at the present; moving the reveal before the present, or skipping the present, is the refactor the regression history tempts.
status: draft
scope: "every render that ends with a reveal pending; unconditional"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:AffScene"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
owner: /subsystems/transitions.md
relates_to:
  - /quirks/playacf-leaves-the-screen-black.md
  - /subsystems/save.md
sources:
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the flip switch and the tail of AffScene
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the mid-frame guard inside the per-object loop of MainLoop
  - id: transitions-doc
    resource: ../../TRANSITIONS.md
    title: Scene and FMV transitions (docs/TRANSITIONS.md), "The reveal, in detail"
---

# Scope

Every render. The order is original and the tail runs on every `AffScene` call whatever the flip mode; the flip modes without a present reach the same tail.

# Evidence

`untested`, read from the code. The order is visible in the function and unchanged from the initial import; the consequences below were each found by a defect rather than by a test.

# Behaviour

`AffScene` draws the scene into `Log`, then runs its flip switch: the two flip modes present through `BoxBlit`, the two no-flip modes do not. After the switch come the autosave for a genuine cube change, then `changepal`, then `FlagPal` as an instant palette sync, then `FlagFade` as `FadeToPalAndSamples`, else `FlagLoadGame` as an instant sync. `FadeToPalAndSamples` starts from black whatever the palette was, so the frame it ramps up is the one just presented, and the viewer saw that frame under whatever palette was loaded at the present.[^object-cpp]

# Why it is load bearing

- The whole family's correctness rests on the palette being black at that one present. It is why `LoadScene` fades before it reads the cube, why the video player leaves the screen black, and why the intro's caller fades to black before it lets the opening scene be rendered once.[^transitions-doc]
- The obvious fixes for the video pop-in were to run the reveal before the present, or to skip the present when `FlagFade` is set. Both were considered and neither was taken: the order is Adeline's and the regression was in the video player's tail. A reader who meets the pop-in again should look at what lit the palette, not at this order.[^transitions-doc]
- The autosave sits between the present and the reveal, so it runs while the screen is black and its thumbnail is read from a `Log` that already holds the composited new scene. The thumbnail is palette indices, so a black palette does not darken it.[^object-cpp]
- The mid-frame guard that keeps a Life script's cube change from being rendered sits inside the per-object loop, so a `NewCube` set by anything later in the frame is rendered and revealed before `ChangeCube` runs on the next iteration; the ramp then reveals the old scene and a second fade cycle follows. That is the open reveal-before-swap shape of #425, and it is a property of where the guard is, not of the fade primitives.[^perso-cpp]

# What it does not tell you

- **Whether the presented frame was black.** `FlagBlackPal` is the engine's belief; the present is what the viewer saw. Only a present trace separates them, and none is in the tree.
- **What the next flip mode will be.** `FirstTime` is set to a full flip by a transition and by camera work on every frame it moves, and the letterbox animates on one mode and not the other; see [the letterbox quirk](/quirks/the-letterbox-slides-only-on-an-objects-flip.md).
- **That the frame was complete.** The menu's version of the same rule is that a page is flipped after its last draw, not after each; a flip between the backdrop and the rows showed the bare backdrop for a frame. The rule is one rule in two places, and the tail of `AffScene` is the engine's.

[^object-cpp]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `AffScene`, the `switch (flagflip)` and everything after it to the `FlagRestoreCD` switch.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the `if (NewCube != -1) goto startloop` at the end of the per-object loop body.
[^transitions-doc]: [docs/TRANSITIONS.md](../../TRANSITIONS.md), "The reveal, in detail (AffScene tail)" and "The #404 regression".
