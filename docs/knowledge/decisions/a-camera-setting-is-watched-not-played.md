---
type: Decision
title: A camera setting changes how the game is seen, not what happens in it
description: The rings, Original inside Modern inside Experimental, held for the Auto camera's settings as two fixtures rather than as prose; with the camera off its settings change nothing at all, with it on they leave the world hash alone, and the one door between the two is the original game's own reading of what is on screen.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-18T18:00:00Z }
owner: /subsystems/recording.md
relates_to:
  - /decisions/one-auto-camera-each-behaviour-a-switch.md
  - /decisions/digest-membership.md
  - /quirks/the-game-counts-what-is-on-screen.md
sources:
  - id: pr-739
    resource: https://github.com/LBALab/lba2-classic-community/pull/739
    title: The state digest split into a world hash and a view hash, and the Auto camera measured with it
  - id: pr-717
    resource: https://github.com/LBALab/lba2-classic-community/pull/717
    title: The camera handed back to a 3D icon is the one the frame was drawn with
  - id: fixture-watch
    resource: ../../../tests/automation/test_ring_watch_not_play.sh
    title: The Modern contract, flipped setting by setting
  - id: fixture-off
    resource: ../../../tests/automation/test_ring_off_is_gone.sh
    title: The Original contract, flipped setting by setting
  - id: ringlib
    resource: ../../../tests/automation/ringlib.sh
    title: The flip list, the scenes and the replay reader both fixtures share
  - id: control
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, Control_StateDigest
---

# Context

The project keeps the original game knowable while play grows around it. It does that in three rings, each inside the next: **Original**, the game as a 1997 player met it; **Modern**, settings that change how it is shown or handled; **Experimental**, anything else, removable without ceremony. A player who never touches a setting plays Original, and a purist loses nothing to the outer rings as long as they never reach into it.

Written as prose, that promise could only be believed. Splitting the replay digest into a world hash and a view hash made it measurable: the same inputs giving the same world hash is the same game, however it was shown.[^pr-739] The Auto camera was measured with it over 315 replays and the world hash never moved. Turning that measurement into fixtures found two things the prose had wrong:

- **The view hash does not see everything the camera does.** It hashes the camera's globals. A shot blend and the obstruction tilt are applied to the view that is drawn and never written back: switching the blend off left the view hash matching while the drawn view differed. The bug #717 fixed lived in that gap.[^pr-717]
- **The original game reads what is on screen.** A camera framing the hero differently changed the world hash through one bit the renderer sets, and the lightning spell, which strikes only drawn enemies, kills more at 1920x1080 than at 640x480. The Quirk has the measurement.

# Decision

Two contracts for the Auto camera's settings, each checked by a fixture that flips every setting in turn on recorded scenes.[^ringlib]

| Ring | Contract | Checked by |
|---|---|---|
| Original | With the Auto camera off, no setting of it changes the world hash, the view hash, the view drawn on any step, or the last frame, here or with a 3D icon up | `test_ring_off_is_gone.sh`[^fixture-off] |
| Modern | With it on, flipping any setting of it leaves the world hash the same on every tick; the camera may move | `test_ring_watch_not_play.sh`[^fixture-watch] |

**What is on screen is part of the game where the original reads it.** A setting that changes what is drawn, a camera or a resolution, may change the game through `LF_OBJECT_DISPLAYED` or the lightning spell. That is the 1997 rule applied to a different screen, not an outer ring reaching in, and the spell killing what the player can see is its nature. The digest is arranged to say so: the two bits the renderer writes on the hero, `WAS_DRAWN` and `DRAW_SHADOW`, are hashed on the view side from recording format 16, with the combined hash unchanged, so the world hash reports what the game did with the mark and not the mark itself.[^control]

**Every setting is accounted for.** The fixtures list each Auto camera setting with a value to flip it to and check the list against the binary's own `varlist cam_`, so a new setting fails both until it is added. A setting must move the camera somewhere in the scenes; one that cannot is listed with the reason. Four are listed today, all acting on the two adjustments that never reach the frame (the Quirk on `AffGrilleExt`).

**Four instruments, each covering what the last misses:** the world hash is what the game holds; the view hash, the camera globals; the camera trace, the view drawn on each step; the frame hash, the picture.

# Non-goals

- **Making the game independent of the screen.** Deciding "on screen" from the 1997 frame would give the lightning spell the same strength at every resolution, and would leave enemies a player can see untouched. It is possible later, for the spell alone; a setting for it now would be a commitment made before anyone has asked for it, so the Quirk records it instead.
- **Every other setting.** The contracts cover the Auto camera's. Resolution changes the game through the same door and is not yet carried by a recording; `DetailLevel` changes the rain's draws from the shared random stream, which is original. A ring tag on each settings row, and presets built from them, are later.
- **What the routes do not walk.** Cutscenes, camera writes from scripts, and other actors' drawn marks, which are not hashed, are outside both fixtures.

[^pr-739]: Pull request 739 on origin, its description and the measurement linked from it.
[^pr-717]: Pull request 717 on origin; `FollowCamApplyShownView` in [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP).
[^fixture-watch]: [tests/automation/test_ring_watch_not_play.sh](../../../tests/automation/test_ring_watch_not_play.sh).
[^fixture-off]: [tests/automation/test_ring_off_is_gone.sh](../../../tests/automation/test_ring_off_is_gone.sh).
[^ringlib]: [tests/automation/ringlib.sh](../../../tests/automation/ringlib.sh), `RING_FLIPS` and `ring_scene`.
[^control]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), `Control_StateDigest`, the hero's `WorkFlags`.
