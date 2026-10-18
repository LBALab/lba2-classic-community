---
type: Quirk
title: The game counts what is on screen
description: The renderer marks each actor it draws, and the original game reads that mark as game state, in a life-script condition and in the lightning spell, which strikes only the enemies drawn on the last frame; anything that changes what is on screen, a resolution or a camera, can therefore change what happens.
status: draft
scope: "every scene; measured on interiors and exteriors with the classic camera at 640x480 and 1920x1080"
equivalence: untested
asm_origin: "SOURCES/EXTRA.CPP:InvoqueFoudre"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-18T18:00:00Z }
owner: /subsystems/camera.md
relates_to:
  - /decisions/a-camera-setting-is-watched-not-played.md
sources:
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, AffScene clearing and setting WAS_DRAWN, AffOneObject setting it on draw
  - id: extra
    resource: ../../../SOURCES/EXTRA.CPP
    title: EXTRA.CPP, InvoqueFoudre and its test of WAS_DRAWN
  - id: gerelife
    resource: ../../../SOURCES/GERELIFE.CPP
    title: GERELIFE.CPP, LF_OBJECT_DISPLAYED
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, where every reader of WAS_DRAWN named here already is
  - id: control
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, Control_StateDigest and where it puts the hero's drawn bits
---

# Scope

Every scene. `AffScene` clears `WAS_DRAWN` on every actor and sets it again on the ones it draws, or, for an actor baked into the background, on the ones whose origin projects inside the view.[^object] So the mark answers "was this actor on screen last frame", and the screen is whatever the resolution and the camera make it.

# Evidence

`untested` in the schema's sense: no regression test pins it. The readers were checked against the initial import, where all of them already are.[^original] The lightning spell was measured on the port, 2026-10-18, on the corpus saves that carry the ring and enough magic to cast it: the classic camera, `--fixed-dt 16`, the spell key pressed at tick 20, every actor's life point recorded each tick. Of the twelve casts that fired, five killed more actors at 1920x1080 than at 640x480:

| Save | Killed at 640x480 | Killed at 1920x1080 |
|---|---|---|
| Bu | none | actor 12 |
| Esmer Secret Base | none | actors 10, 20 |
| ICX | 5, 12 | 4, 5, 12 |
| Island CX 4 Real | none | actor 14 |
| Sword | 13 | 12, 13, 14 |

No actor lost life in the same runs without the cast, a second 640x480 run killed the same actors, and every kill fell on ticks 187 to 205, when the strike lands. In Sword the extra two are guards standing outside the 640x480 frame and inside the 1920x1080 one. The Auto camera at 640x480 killed the same actors as the classic camera in all twelve, but only three of the casts were outdoors, where it acts, so that says little about it.

A scan of `SCENE.HQR` (GOG data, every one of its 223 scenes parsed) found 24 tests of `LF_OBJECT_DISPLAYED` in 11 scenes. Twenty-one are on island 2, cubes 62 to 72, each an actor testing whether it is itself off screen. What those scripts then do was not decoded.

# Behaviour

- **The lightning spell** (`InvoqueFoudre`) strikes every actor other than its caster that was drawn on the last frame, has a body, is not invisible and has an armour below 51; one weak enough for the caster's magic level is killed outright.[^extra]
- **`LF_OBJECT_DISPLAYED`** gives a life script the mark of any actor it names.[^gerelife]
- **`LM_BACKGROUND` and `TM_BACKGROUND`** read it to decide whether switching an actor in or out of the background needs a full redraw, and **the shadow search** in `BEZIER.CPP` reads it to pick the decor an actor stands on. Both are drawing, not game state.
- **`WAIT_COORD`** is a related path: an animation that throws an object waits until the thrower has been drawn to read the point it throws from. Read, not measured.

# Why it is load bearing

- **It is the one way the screen reaches the game.** A camera or a resolution that changes which actors are drawn can change who the spell kills and which branch a script takes. The rings Decision counts this as the original's rule applied to a different screen, not as a setting leaking into the game.[^control]
- **The replay digest has to say which side the mark is on.** The hero's `WAS_DRAWN`, and the draw list's `DRAW_SHADOW` beside it, are hashed on the view side from recording format 16: a camera that frames the hero differently moves them with nothing else changed, and when the game does act on the mark, the fields it changes are on the world side.
- **Deciding "on screen" from the 1997 frame** would make the spell the same at every resolution. It would also leave enemies a player can see at 1920x1080 untouched, and an actor waiting to be off screen could act in plain view. Neither is chosen; this page is where that choice starts.

# What it does not tell you

- **That a recording replays the same at another resolution.** It does not, whenever the spell is cast or one of those scripts runs: the resolution is shown in a recording's header as how the run was displayed, and the replay does not take it on.
- **What the island 2 scripts do once they see themselves off screen.** Found by a byte scan that matches the instruction's whole shape, not by a decoder.
- **Anything about other actors' marks in a replay.** Only the hero's is hashed; another actor's shows up only through what follows from it.

[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `AffScene` and `AffOneObject`.
[^extra]: [SOURCES/EXTRA.CPP](../../../SOURCES/EXTRA.CPP), `InvoqueFoudre`.
[^gerelife]: [SOURCES/GERELIFE.CPP](../../../SOURCES/GERELIFE.CPP), `DoFuncLife`, case `LF_OBJECT_DISPLAYED`.
[^original]: Commit 333929ab, the initial import: `EXTRA.CPP`, `GERELIFE.CPP`, `GERETRAK.CPP`, `BEZIER.CPP` and `OBJECT.CPP` all read or set `WAS_DRAWN` as they do now.
[^control]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), `Control_StateDigest`, the hero's `WorkFlags`.
