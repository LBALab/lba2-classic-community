# Camera blending design

Status: exploratory. Phase A (in-scene handoff blending) has a prototype behind
the `cam_blend` cvar; Phase B (free dynamic camera) is not started.

## Problem

LBA2's exterior camera is driven by several independent controllers that each
grab the camera by hard-writing its globals. There is no interpolation between
them, so every handoff is a jump cut:

- Entering a scripted camera zone (a scenic view of an area) cuts instantly to
  the scripted framing.
- Leaving the zone cuts instantly back to the dynamic follow camera.
- Manually re-centering (Center camera) snaps instantly.

The goal is to make these handoffs gradual, and separately to make the dynamic
camera hold its angle (a free third-person camera) instead of auto-recentering.

## The controllers

Everything that moves the exterior camera, and how it does it today:

| Controller | Applies via | Trigger |
| --- | --- | --- |
| Dynamic follow (`FollowCamera`) | per-frame `CameraCenter(3)` / `SetFollowCamera` in `UpdateFollowCameraExt` | default in exterior |
| Scripted zone camera | `SetZoneCamera` forces `AlphaCam`/`BetaCam`/`GammaCam`/`VueDistance` + `CameraZone=TRUE` + `AffScene(AFF_ALL_FLIP)` | hero enters a camera zone (`OBJECT.CPP`), or the `LM_SET_CAMERA` script op |
| Cutscene | scripted | `CinemaMode` |
| Manual recenter | `CameraCenter(1)` snaps behind the hero (zeroes `AddBetaCam`) | Enter / gamepad B / mouse right-click |
| Scene transition | `CameraCenter(1)` (full, `FlagChgCube` 0/2) or `CameraCenter(0)` (seamless, `FlagChgCube` 1) | `ChangeCube` |
| Off-screen recenter | `CameraCenter(0)` | already suppressed under `FollowCamera` |
| Load | `CameraCenter(1)`/`(0)` | `SAVEGAME` |

The camera's interpolatable state is seven scalars: `VueOffsetX/Y/Z` (look-at
target), `AlphaCam`/`BetaCam`/`GammaCam` (angles, mod 4096), and `VueDistance`
(boom). `SetFollowCamera(...)` builds the matrix from exactly these. The
editor-only `FlyCamera` already lerps all seven with `RegleTrois` over a
duration; it is the reference for the blend, just blocking and editor-gated.

## The blend layer

Insert one step between "a controller decided where the camera should be" and
"the matrix is committed for rendering":

```
controllers -> [target params in globals] -> BLEND -> SetFollowCamera(presented)
```

Each frame while a blend is active:

```
w = ease(elapsed / duration)             // 0 -> 1
presented = lerp(blendStart, target, w)  // per param
SetFollowCamera(presented...)
```

`blendStart` is a frozen snapshot of the presented camera at the moment of the
handoff; `target` is whatever the active controller wrote this frame. `lerp` is
`RegleTrois` for the positional and distance params and a shortest-arc variant
for the three angles (4000 to 100 must travel +192, not -3900).

### Blending to a moving target

Blending into a zone is a fixed endpoint. Blending back out to the dynamic
follow is not: the follow target moves every frame as it tracks the hero. The
robust form is a weight blend rather than an endpoint blend:

```
presented = lerp(frozenStart, liveTargetThisFrame, w)   // w rises 0 -> 1
```

Only the outgoing state is frozen. Each frame the presented camera is pulled a
little further toward whatever the incoming controller currently wants, so if
the hero keeps walking during the blend the camera converges onto the live
follow instead of a stale point. The same code path handles static (zone) and
moving (follow) targets.

This composes with the existing recompose / occlusion / manual-fade work: those
all produce the dynamic target, so they feed the incoming side of the blend.

## Selective blending

Not every cut should become a blend:

- Cutscene (`CinemaMode`) cuts are usually intentional. Exclude.
- Scene transitions and load teleport the camera across the world. Blending
  across that is a long fly through nothing. Cut.
- Interior to exterior is the one transition that should blend (Phase B), and
  in-scene zone and recenter handoffs are the ones this design targets.

So the blend is opt-in per handoff. The prototype approximates this cheaply (see
below); a production version would instrument the handoff sites directly.

## Phase A prototype (divergence-triggered)

Rather than instrument every controller, the prototype watches the committed
camera params once per frame just before `AffScene` and reacts to jumps:

- Track the presented params each frame.
- If they diverge beyond a threshold (a handoff) and we are not already
  blending, in an exterior cube, not in `CinemaMode`, and the cube did not just
  change, start a blend from the previous presented state toward the new target.
- While blending, present the interpolated params via `SetFollowCamera` and
  force a redraw; when `elapsed >= duration`, hand back to the controller.

Scene transitions are cut by resetting the tracker when `NumCube` changes.
Cutscenes are cut by the `CinemaMode` guard. This catches the in-scene zone-in,
zone-out, and manual-recenter handoffs (the reported jump cuts) with a single
integration point and no changes to the zone or transition code.

Tunables (cvars, persisted): `cam_blend` (enable), `cam_blend_dur` (frames).

Known rough edges of the prototype, to revisit:

- Divergence detection is a heuristic; a very fast legitimate move could trip
  it, and a small zone offset could miss it. Instrumenting `SetZoneCamera` /
  zone-exit / `CameraCenter` directly removes the guesswork.
- The blend presents the parametric camera without the per-frame occlusion lift,
  so the lift pops in at blend end (usually small; zones have none).
- A handoff during a blend continues toward the new target rather than
  re-snapshotting from the current presented state.
- Interpolation is linear; ease-in-out is a feel refinement.

## Phase B: free dynamic camera

Independent of blending: make the dynamic camera hold its angle and stop
grabbing back. `cam_hold_angle` already stops the pan drift. The remaining
involuntary recenters are the `CameraCenter(1)` calls; gate the non-interior-to-
exterior ones so only interior-to-exterior transitions and manual Center camera
re-center. With the blend layer, even those become gradual.

## Risks

- Cutscene integrity: a blend leaking into `CinemaMode` could desync scripted
  timing. The exclusion must be airtight.
- Gameplay-critical framing: some zones frame a puzzle or jump that the player
  needs to see now; a slow blend can hurt readability. Per-zone metadata to opt
  out is not currently addable without scene-data editing.
- Save/load mid-blend: snapshot the target, or force-complete on save.
- Determinism harness: the blend is frame-counted and deterministic, but adds
  per-frame camera state the projrec/dump baselines should account for.
