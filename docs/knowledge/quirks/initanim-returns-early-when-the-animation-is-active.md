---
type: Quirk
title: InitAnim returns early when the animation is already active
description: Starting an animation an object is already playing is a no-op that reports success, so the hero's walk does not restart on every frame a direction is held; the port's sub-stepping re-presents a frame's held input on every step and rests on that no-op to keep a held action from re-firing.
status: draft
scope: "unconditional; every object with a loaded animation"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:InitAnim"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
owner: /subsystems/movement.md
relates_to:
  - /decisions/sub-steps-hold-the-frames-input.md
sources:
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, InitAnim and the manual move that calls it every frame
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/OBJECT.CPP, the same early return
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the sub-step comment that names the no-op as what it relies on
---

# Scope

Every object with an animation loaded. The check is on the generic animation id and on the object having an animation at all, so it is not a hero rule.

# Evidence

`untested`, read against the original source. The early return is in the initial import with the same two conditions, and no test pins it by itself; the sub-step fixture exercises it indirectly, since a held action across sub-steps would double-fire without it.[^original]

# Behaviour

`InitAnim` returns true without doing anything when the requested generic animation is the one the object already has and the object's animation slot is not empty. The manual move calls it on every frame a direction is held, `GEN_ANIM_MARCHE` for forward, so a held walk is a stream of requests of which only the first starts anything.[^object]

# Why it is load bearing

- The hero's walk is a per-frame request, not a state transition. Remove the early return, or make it reset the frame, and every held direction restarts the walk cycle each frame, which is the hero stuck on the first frame of the animation and never moving.
- The port's sub-stepping holds a frame's input across the frame's steps and re-presents it on each, so a held direction reaches `InitAnim` once per step rather than once per frame. That is safe only because the second and later requests are no-ops, which is why [the sub-step decision](/decisions/sub-steps-hold-the-frames-input.md) names it. Projectile spawns are gated on an animation frame for the same reason.[^perso]
- A request for a different animation is not a no-op, so the check is on identity and not on "busy": a held action that switches animations mid-walk still switches.

# What it does not tell you

- **Whether the animation is still on its first frame.** A no-op leaves the cycle where it was; a caller that wants a restart has to ask for something else first.
- **That the return value means anything ran.** True comes back from the no-op and from a real start alike; the callers do not distinguish them.

[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `InitAnim`, the test on `GenAnim` and `Obj.Anim.Num` before the switch on the flag; `DoDir`, the `MOVE_MANUAL` case that calls it for `GEN_ANIM_MARCHE`.
[^original]: Commit 333929ab, the initial import, SOURCES/OBJECT.CPP, `InitAnim`, the same early return.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the sub-step comment: "InitAnim is idempotent and projectile spawns are animation-frame-gated".
